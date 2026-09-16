module;

#include "abi.h"

export module wxl.async:drain_stack;

import wxl.core;
import std;

export namespace wxl::async {

/// Intrusive stack that is emptied whole rather than element by element: any number of
/// threads push, and whoever drains takes the entire chain in a single exchange. The
/// missing single-element pop is the point -- it is what makes a reader pay one atomic
/// operation per batch instead of one per element, and what keeps the pushing CAS clear
/// of ABA.
template <core::node_like Node>
class drain_stack : public core::noncopyable
{
public:
    drain_stack() noexcept : top_(nullptr) {}

    ~drain_stack() { assert(top_.load(std::memory_order_relaxed) == nullptr); }

    /// Push an element to the top of stack.
    /// Safe to call from many threads.
    ///
    /// `order` is the success order of the publishing CAS. Release is the default and is
    /// what handing the element to the reader needs: every later push is itself an RMW on
    /// the top, so it extends the release sequence, and pop_all()'s single acquire
    /// synchronizes with every pusher rather than only the last. Pass seq_cst when the
    /// push is one half of a store/load exchange -- when the caller reads *another* atomic
    /// straight afterwards and needs the two ordered, the way mpsc_channel's writer reads
    /// its wait trigger. Release does not order a store against a later load.
    void push(Node* element, std::memory_order order = std::memory_order_release) noexcept {
        Node* top = top_.load(std::memory_order_relaxed);

        do {
            element->next_ = top;
        } while (!top_.compare_exchange_weak(top, element, order, std::memory_order_relaxed));
    }

    /// Removes all items from stack.
    /// Safe to call from many threads.
    Node* pop_all() noexcept {
        if (top_.load(std::memory_order_relaxed) == nullptr) [[likely]]
            return nullptr;

        return top_.exchange(nullptr, std::memory_order_acquire);
    }

    bool empty() const noexcept { return top_.load(std::memory_order_relaxed) == nullptr; }

private:
    std::atomic<Node*> top_;
};

/// Gives a type the link a stack node needs without touching the type itself: inherits
/// both, so the payload keeps its own constructors.
template <typename Base>
class as_node : public Base, public core::intrusive_slist_node<as_node<Base>>
{
    using base = Base;

public:
    using base::base;
};

}  // namespace wxl::async
