export module wxl.async:mpsc_queue;

import :drain_stack;
import wxl.core;
import std;

export namespace wxl::async {

/// The order in which a claimed batch is handed out. FIFO costs one walk of the batch to
/// reverse it; LIFO hands the chain over as the stack built it, which is what a free list
/// wants -- the warmest element is the one released last.
enum class drain_order { fifo, lifo };

/// Queue of intrusive nodes for many writing threads and exactly one reading thread. A
/// write is a single push onto the underlying stack; a read claims the whole accumulated
/// chain at once and then hands it out from a reader-owned list, without atomics. So the
/// cost per element falls as the load rises: the busier the queue, the larger the batch a
/// claim brings back.
///
/// The single reader is the price. `reader_node_` is a plain field, and everything cheap
/// here follows from nobody else being allowed to touch it.
template <core::node_like Node, drain_order order = drain_order::fifo>
class mpsc_queue : public core::noncopyable
{
public:
    mpsc_queue() noexcept : pops_since_claim_(), reader_node_() {}

    /// Safe to call from reader thread only
    bool empty() const noexcept { return (reader_node_ == nullptr) && stack_.empty(); }

    void push_one(Node* element) noexcept { stack_.push(element); }

    /// Publishes the element with seq_cst instead of release, for a writer that reads
    /// another atomic straight after the push and needs the two ordered -- mpsc_channel's
    /// arm/disarm exchange is the one caller. See drain_stack::push.
    void push_one_ordered(Node* element) noexcept {
        stack_.push(element, std::memory_order_seq_cst);
    }

    /// Removes next element from the queue.
    /// @return Next element or NULL if both the reader-owned batch and the stack are empty
    Node* pop_one() noexcept;

    /// Does not remove next element from the queue.
    /// @return Next element or NULL if both the reader-owned batch and the stack are empty
    Node* peek() noexcept;

    /// How many elements the reader has taken since it last claimed a batch -- its position
    /// inside that batch, not the number of elements queued. `pool` reads it to notice that
    /// it is holding more free objects than it wants to keep.
    size_t pops_since_claim() const noexcept { return pops_since_claim_; }

private:
    alignas(std::hardware_destructive_interference_size) drain_stack<Node> stack_;
    alignas(std::hardware_destructive_interference_size) size_t
        pops_since_claim_;  // prevent false sharing
    Node* reader_node_;
};

/// The same structure handing its batches back newest first -- what a pool of free objects
/// wants, and the reason `order` exists at all.
template <core::node_like Node>
using mpsc_stack = mpsc_queue<Node, drain_order::lifo>;

// -------------------------------------------------------------
// utility functions

namespace details {

template <core::node_like Node>
Node* reverse_linked_list(Node* first_node) noexcept {
    Node* next_node = first_node->next_;
    first_node->next_ = nullptr;

    while (next_node) {
        Node* const next_next_node = next_node->next_;
        next_node->next_ = first_node;
        first_node = next_node;
        next_node = next_next_node;
    }

    return first_node;
}

}  // namespace details

// -------------------------------------------------------------
// mpsc_queue methods

template <core::node_like Node, drain_order order>
inline Node* mpsc_queue<Node, order>::peek() noexcept {
    if (Node* reader_top = reader_node_) return reader_top;

    if (Node* element = stack_.pop_all()) {
        if constexpr (order == drain_order::fifo) element = details::reverse_linked_list(element);

        pops_since_claim_ = 0;
        reader_node_ = element;
        return element;
    }

    return nullptr;
}

template <core::node_like Node, drain_order order>
inline Node* mpsc_queue<Node, order>::pop_one() noexcept {
    if (Node* node = reader_node_) {
        reader_node_ = node->next_;
        ++pops_since_claim_;
        return node;
    }

    if (Node* element = stack_.pop_all()) {
        if constexpr (order == drain_order::fifo) element = details::reverse_linked_list(element);

        pops_since_claim_ = 1;
        reader_node_ = element->next_;
        return element;
    }

    return nullptr;
}

}  // namespace wxl::async
