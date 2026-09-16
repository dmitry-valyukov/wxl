module;

#include "abi.h"

export module wxl.core:intrusive_list;

import :not_null;
import :noncopyable;
import std;

export namespace wxl::core {

/**
 * Per-node link used by `intrusive_list`: both neighbours, so a node can be
 * taken out knowing nothing but itself. A type inherits this to become
 * storable in an `intrusive_list<Node>` without any separate node
 * allocation.
 *
 * The links point at the link, not at the node, because the list is one of
 * these too and serves as the ring's sentinel -- and the list is not a
 * `Node`. An unlinked node points at itself, which is what makes `linked()`
 * a comparison rather than a flag to keep in step.
 */
template <class Node>
struct intrusive_list_node : public noncopyable {
    intrusive_list_node* next_ = this;
    intrusive_list_node* prev_ = this;

    /// `true` while this node is in some list.
    bool linked() const noexcept { return next_ != this; }
};

/**
 * Satisfied by any `Node` that derives from `intrusive_list_node<Node>`
 * (ignoring const) -- i.e. any type usable as the element of an
 * `intrusive_list<Node>`.
 *
 * Deliberately not used to constrain the list or its iterator below, for the
 * reason `intrusive_slist` gives: a concept that asks whether `Node` derives
 * from something needs `Node` complete, which rules out the very case an
 * intrusive list is best at -- a node holding a list of its own kind,
 * declared inside its own definition.
 */
template <class Node>
concept list_node_like =
    std::derived_from<std::remove_const_t<Node>, intrusive_list_node<std::remove_const_t<Node>>>;

/**
 * Bidirectional iterator over an `intrusive_list<Node>`. It walks links
 * rather than nodes, and the one link that is not a node -- the list's own,
 * which closes the ring -- is what `end()` names.
 */
template <class Node>
class intrusive_list_iterator
{
    using link_t = intrusive_list_node<std::remove_const_t<Node>>;

    link_t* link_ = nullptr;

public:
    intrusive_list_iterator() = default;
    explicit intrusive_list_iterator(link_t* link) noexcept : link_(link) {}

    using value_type = Node;
    using reference = Node&;
    using pointer = Node*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    reference operator*() const { return *static_cast<Node*>(link_); }
    pointer operator->() const { return static_cast<Node*>(link_); }

    intrusive_list_iterator& operator++() {
        link_ = link_->next_;
        return *this;
    }

    intrusive_list_iterator operator++(int) {
        intrusive_list_iterator tmp = *this;
        link_ = link_->next_;
        return tmp;
    }

    intrusive_list_iterator& operator--() {
        link_ = link_->prev_;
        return *this;
    }

    intrusive_list_iterator operator--(int) {
        intrusive_list_iterator tmp = *this;
        link_ = link_->prev_;
        return tmp;
    }

    // operator!= is synthesized from this by the compiler.
    friend bool operator==(intrusive_list_iterator, intrusive_list_iterator) = default;
};

/**
 * A doubly-linked intrusive list: nodes carry both their links (via
 * `intrusive_list_node<Node>`), so linking and unlinking never allocate and
 * **taking a node out costs four pointer writes whatever the list holds**.
 * That last is the whole reason this exists beside `intrusive_slist`, whose
 * `remove` has to walk to find the predecessor.
 *
 * Built as a ring closed through the list itself, which is why nothing here
 * branches on a null: an empty list is one whose own link points at itself,
 * and the first and last nodes point back at it rather than at nothing.
 *
 * The list does not own its nodes. It is the natural shape for a registry
 * that objects put themselves into and take themselves out of -- the node
 * lives inside the object, and leaving is something the object can do on its
 * own, without asking the list or remembering a cookie.
 */
template <class Node>
class intrusive_list : intrusive_list_node<Node>
{
    using link_t = intrusive_list_node<Node>;

public:
    using iterator = intrusive_list_iterator<Node>;
    using const_iterator = intrusive_list_iterator<const Node>;

    /// Links `node` in at the head. `node` must not already be linked.
    void push_front(not_null<Node> node) noexcept { link_after(this, node.get()); }

    /// Links `node` in at the tail. `node` must not already be linked.
    void push_back(not_null<Node> node) noexcept { link_after(this->prev_, node.get()); }

    /**
     * Takes `node` out of whatever list it is in, and does nothing if it is
     * in none.
     *
     * Deliberately does not check that the node is in *this* list: it cannot
     * be done in constant time, and being able to leave without consulting
     * the list is the property this container is chosen for.
     */
    static void remove(not_null<Node> node) noexcept {
        link_t* const link = node.get();

        link->prev_->next_ = link->next_;
        link->next_->prev_ = link->prev_;
        link->next_ = link;
        link->prev_ = link;
    }

    /// Takes every node out, leaving each of them unlinked.
    void clear() noexcept {
        while (!empty()) {
            remove(not_null<Node>{front()});
        }
    }

    bool empty() const noexcept { return this->next_ == this; }

    /// The first node, or null when there is none.
    Node* front() const noexcept { return empty() ? nullptr : as_node(this->next_); }

    /// The last node, or null when there is none.
    Node* back() const noexcept { return empty() ? nullptr : as_node(this->prev_); }

    iterator begin() noexcept { return iterator{this->next_}; }

    const_iterator begin() const noexcept { return const_iterator{mutable_self()->next_}; }

    iterator end() noexcept { return iterator{this}; }

    const_iterator end() const noexcept { return const_iterator{mutable_self()}; }

private:
    static void link_after(link_t* at, link_t* node) noexcept {
        assert(!node->linked() && "wxl: this node is already in a list");

        node->prev_ = at;
        node->next_ = at->next_;
        at->next_->prev_ = node;
        at->next_ = node;
    }

    static Node* as_node(link_t* link) noexcept { return static_cast<Node*>(link); }

    // The ring is walked the same way whether the walk may write or not, and
    // the constness the caller asked for is put back on by the iterator's own
    // element type.
    link_t* mutable_self() const noexcept { return const_cast<intrusive_list*>(this); }
};

}  // namespace wxl::core
