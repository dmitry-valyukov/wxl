module;

#include "abi.h"

export module wxl.core:intrusive_slist;

import :checks;
import :not_null;
import :noncopyable;
import :sta_allocator;
import std;

export namespace wxl::core {

/**
 * Opaque handle identifying a node linked into an `intrusive_slist`. Returned
 * by `push_front`/`push_back` and consumed by `remove` to unlink that same
 * node again; carries no meaning beyond node identity.
 */
using cookie_t = not_null<const void>;

/**
 * Per-node link used by `intrusive_slist`: just a pointer to the next node,
 * or null past the tail. A type inherits this (directly, or via a CRTP
 * `Base` like `wxl::core::function`'s) to become storable in an
 * `intrusive_slist<Node>` without any separate node allocation.
 */
template <class Node>
struct intrusive_slist_node : public noncopyable {
    Node* next_{};
};

/// The same link for a node that lives in the STA pool: allocated and freed there, on the
/// pool's thread and inside its life. What `event` links its callbacks with.
template <class Node>
struct sta_intrusive_slist_node : public intrusive_slist_node<Node> {
    inline static void* operator new(std::size_t size) {
        return sta_memory_pool::alloc(size);
    }

    // Sized, and it has to be: the pool gives back to the size class it took
    // from. The size is the complete object's, because the destructor is
    // virtual and the deleting one the compiler writes knows which object it
    // is freeing.
    inline static void operator delete(void* mem, std::size_t size) noexcept {
        sta_memory_pool::free(mem, size);
    }
};

/**
 * Satisfied by any `Node` that derives from `intrusive_slist_node<Node>`
 * (ignoring const) — i.e. any type usable as the element of an
 * `intrusive_slist<Node>`.
 *
 * Deliberately not used to constrain the list or its iterator below: a
 * concept that asks whether `Node` derives from something has to have `Node`
 * complete, which rules out the very case an intrusive list is best at —
 * a node holding a list of its own kind, declared inside its own definition.
 * A `Node` that does not carry the link fails to compile on `next_` anyway,
 * one line further in.
 */
template <class Node>
concept node_like =
    std::derived_from<std::remove_const_t<Node>, intrusive_slist_node<std::remove_const_t<Node>>>;

/**
 * Forward iterator over an `intrusive_slist<Node>`, walking `next_` links.
 * Single-pass, forward-only, matching the list it iterates.
 */
template <class Node>
class intrusive_slist_iterator
{
    Node* node_ = nullptr;

public:
    intrusive_slist_iterator() = default;
    intrusive_slist_iterator(Node* node) : node_(node) {}

    using value_type = Node;
    using reference = Node&;
    using pointer = Node*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    reference operator*() const { return *node_; }
    pointer operator->() const { return node_; }

    intrusive_slist_iterator& operator++() {
        node_ = node_->next_;
        return *this;
    }

    intrusive_slist_iterator operator++(int) {
        intrusive_slist_iterator tmp = *this;
        node_ = node_->next_;
        return tmp;
    }

    // operator!= is synthesized from this by the compiler.
    friend bool operator==(intrusive_slist_iterator, intrusive_slist_iterator) = default;
};

/**
 * A singly-linked, forward-only intrusive list: nodes carry their own
 * `next_` link (via `intrusive_slist_node<Node>`) instead of the list owning
 * separate node storage, so linking/unlinking a node never allocates.
 * The list does not own its nodes — the caller keeps responsibility for
 * their lifetime; `remove` only unlinks a node and hands it back.
 */
template <class Node>
class intrusive_slist : intrusive_slist_node<Node>
{
public:
    using iterator = intrusive_slist_iterator<Node>;
    using const_iterator = intrusive_slist_iterator<const Node>;

    /// Links `node` in as the new head. `node` must not already be linked.
    cookie_t push_front(not_null<Node> node) {
        assert(!node->next_);
        node->next_ = this->next_;
        this->next_ = node.get();

        if (!last_) last_ = node.get();

        return cookie_t{node};
    }

    /// Links `node` in as the new tail. `node` must not already be linked.
    cookie_t push_back(not_null<Node> node) {
        assert(!node->next_);

        if (last_)
            last_->next_ = node.get();
        else
            this->next_ = node.get();

        last_ = node.get();
        return cookie_t{node};
    }

    /// Unlinks and returns the node identified by `cookie`, or null if it's not in this list.
    Node* remove(cookie_t cookie) {
        intrusive_slist_node<Node>* prev = this;
        Node* cur = prev->next_;

        while (cur) {
            if (cookie_t{cur} == cookie) {
                prev->next_ = cur->next_;

                // The node just unlinked was the tail, so the one before it
                // is -- and the list itself, when there is nothing before it.
                if (cur == last_)
                    last_ = prev == static_cast<intrusive_slist_node<Node>*>(this) ? nullptr
                                                                                 : static_cast<Node*>(prev);

                return cur;
            }

            prev = cur;
            cur = cur->next_;
        }

        return nullptr;
    }

    /// Exchanges the contents of two lists. The nodes are not touched -- only which list
    /// they hang from -- so a cookie stays valid and now names a node of `other`.
    void swap(intrusive_slist & other) noexcept {
        std::swap(this->next_, other.next_);
        std::swap(last_, other.last_);
    }

    bool empty() const { return this->next_ == nullptr; }

    Node* front() const { return this->next_; }

    Node* back() const { return last_; }

    iterator begin() { return iterator{this->next_}; }

    const_iterator begin() const { return const_iterator{this->next_}; }

    iterator end() { return iterator{nullptr}; }

    const_iterator end() const { return const_iterator{nullptr}; }

private:
    // The tail, kept so that appending is a pointer write rather than a walk
    // from the head: filling a list of n nodes one push_back at a time would
    // otherwise cost n²/2 steps.
    Node* last_ = nullptr;
};

}  // namespace wxl::core
