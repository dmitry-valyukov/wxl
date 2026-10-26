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
 * Per-node link used by `intrusive_slist`: just a pointer to the next node,
 * or null past the tail. A type inherits this (directly, or via a CRTP
 * `Base` like `wxl::core::function`'s) to become storable in an
 * `intrusive_slist<Node>` without any separate node allocation.
 */
template <class Node>
struct intrusive_slist_node : public noncopyable {
    /// The next node in the list, or null past the tail.
    Node* next_{};
};

/// The same link for a node that lives in the STA pool: allocated and freed there, on the
/// pool's thread and inside its life. What `event` links its callbacks with.
template <class Node>
struct sta_intrusive_slist_node : public intrusive_slist_node<Node> {
    /// Allocates \p size bytes from the STA memory pool.
    inline static void* operator new(std::size_t size) {
        return sta_memory_pool::alloc(size);
    }

    /// Returns \p mem, of \p size bytes, to the STA memory pool. The operator is sized because
    /// the pool returns memory to the size class it was allocated from. The size is that of the
    /// complete object: the callback node derived from this type has a virtual destructor, and
    /// the deleting destructor that the compiler generates knows which object it frees.
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
    /// Constructs an iterator that refers to \p node; a null \p node is the end iterator.
    intrusive_slist_iterator(Node* node) : node_(node) {}

    /// The type of the node that the iterator refers to.
    using value_type = Node;
    /// The type of a reference to the node.
    using reference = Node&;
    /// The type of a pointer to the node.
    using pointer = Node*;
    /// The type that represents the distance between two iterators.
    using difference_type = std::ptrdiff_t;
    /// The iterator category: forward.
    using iterator_category = std::forward_iterator_tag;

    /// Returns a reference to the node that the iterator refers to.
    reference operator*() const { return *node_; }
    /// Returns a pointer to the node that the iterator refers to.
    pointer operator->() const { return node_; }

    /// Moves the iterator to the next node and returns a reference to the iterator.
    intrusive_slist_iterator& operator++() {
        node_ = node_->next_;
        return *this;
    }

    /// Moves the iterator to the next node and returns its previous value.
    intrusive_slist_iterator operator++(int) {
        intrusive_slist_iterator tmp = *this;
        node_ = node_->next_;
        return tmp;
    }

    /// Tests whether two iterators refer to the same node. The compiler synthesizes
    /// `operator!=` from this operator.
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
    /// The type that iterates over the nodes of the list.
    using iterator = intrusive_slist_iterator<Node>;
    /// The type that iterates over the nodes of the list as `const`.
    using const_iterator = intrusive_slist_iterator<const Node>;

    /// Links `node` in as the new head. `node` must not already be linked.
    void push_front(not_null<Node> node) {
        assert(!node->next_);
        node->next_ = this->next_;
        this->next_ = node.get();

        if (!last_) last_ = node.get();
    }

    /// Links `node` in as the new tail. `node` must not already be linked.
    void push_back(not_null<Node> node) {
        assert(!node->next_);

        if (last_)
            last_->next_ = node.get();
        else
            this->next_ = node.get();

        last_ = node.get();
    }

    /// Unlinks and returns the node at `node`, or null if it is not in this list. Only the
    /// address is compared: `node` is not read through until it is found here, so it may name
    /// a node that is gone.
    Node* remove(Node const* node) {
        intrusive_slist_node<Node>* prev = this;
        Node* cur = prev->next_;

        while (cur) {
            if (cur == node) {
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
    /// they hang from -- so a node's address now names a node of `other`.
    void swap(intrusive_slist & other) noexcept {
        std::swap(this->next_, other.next_);
        std::swap(last_, other.last_);
    }

    /// Tests whether the list contains no nodes.
    bool empty() const { return this->next_ == nullptr; }

    /// Returns the first node, or null when the list is empty.
    Node* front() const { return this->next_; }

    /// Returns the last node, or null when the list is empty.
    Node* back() const { return last_; }

    /// Returns an iterator to the first node.
    iterator begin() { return iterator{this->next_}; }

    /// Returns a `const` iterator to the first node.
    const_iterator begin() const { return const_iterator{this->next_}; }

    /// Returns an iterator past the last node; it holds null.
    iterator end() { return iterator{nullptr}; }

    /// Returns a `const` iterator past the last node; it holds null.
    const_iterator end() const { return const_iterator{nullptr}; }

private:
    // The tail, kept so that appending is a pointer write rather than a walk
    // from the head: filling a list of n nodes one push_back at a time would
    // otherwise cost n²/2 steps.
    Node* last_ = nullptr;
};

}  // namespace wxl::core
