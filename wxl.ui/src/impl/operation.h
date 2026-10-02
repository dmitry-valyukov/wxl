#pragma once

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>

#include "../Operation.h"

// The producer side of wxl::Operation, for the generated wrappers: a WinRT
// IAsyncOperation<X> or IAsyncAction goes in, the Operation comes out.
//
// The call has already started the operation. The completion handler runs on a thread of the
// runtime's choice; all it does there is post to the dispatcher of the thread that made the call,
// and the result is read, converted to wxl's types and handed to the coroutine on that thread.
// If the awaiter has been dropped in the meantime, the post finds the state abandoned and ends.
//
// Private: this header names winrt types, so only wxl's own sources include it.

namespace wxl::impl {

namespace operation_detail {

// Finishes on the interface thread: the result into the state, the coroutine resumed.
template <class State, class Read>
void finish(std::shared_ptr<State> const& state, Read&& read) {
    if (state->abandoned) return;
    try {
        read(*state);
    } catch (...) {
        state->error = std::current_exception();
    }
    state->done = true;
    if (auto const waiter = std::exchange(state->waiter, {})) waiter.resume();
}

}  // namespace operation_detail

/// An asynchronous operation with a result: `convert` turns what it returns (a projection value)
/// into `R`, on the interface thread.
template <class R, class Op, class Convert>
Operation<R> start_operation(Op const& operation, Convert convert) {
    auto const state = std::make_shared<OperationState<R>>();
    state->cancel = [operation] { operation.Cancel(); };
    auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    operation.Completed([state, queue, convert](auto const& finished, winrt::Windows::Foundation::AsyncStatus status) {
        queue.TryEnqueue([state, finished, status, convert] {
            operation_detail::finish(state, [&](OperationState<R>& target) {
                if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
                    target.emplace(convert(finished.GetResults()));
                } else {
                    finished.GetResults();  // throws what the operation failed with
                    throw winrt::hresult_canceled();
                }
            });
        });
    });
    return Operation<R>(state);
}

/// An asynchronous operation without a result (IAsyncAction).
template <class Op>
Operation<void> start_action(Op const& operation) {
    auto const state = std::make_shared<OperationState<void>>();
    state->cancel = [operation] { operation.Cancel(); };
    auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    operation.Completed([state, queue](auto const& finished, winrt::Windows::Foundation::AsyncStatus status) {
        queue.TryEnqueue([state, finished, status] {
            operation_detail::finish(state, [&](OperationState<void>&) {
                finished.GetResults();  // throws what the operation failed with
                if (status != winrt::Windows::Foundation::AsyncStatus::Completed) throw winrt::hresult_canceled();
            });
        });
    });
    return Operation<void>(state);
}

}  // namespace wxl::impl
