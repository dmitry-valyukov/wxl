#pragma once

// wxl::Operation<T> -- what a WinRT asynchronous operation (IAsyncOperation<T>,
// IAsyncAction) is on this side: a value a coroutine awaits.
//
//     auto text = co_await package.getTextAsync();
//
// Like every other wrapper, the projection's own type does not cross: what is handed
// over is the result in wxl's terms (a text, a wrapper, a number), and the operation
// itself is not for the application to hold. It is made by the call, runs from the
// call, and is awaited once.
//
// What makes it safe, and what a bare IAsyncOperation is not:
//
//  * The coroutine resumes on the thread the operation was started on -- the
//    interface thread -- and nowhere else. The completion of the operation arrives on
//    whatever thread the runtime picks; it is posted from there to the dispatcher of
//    the thread that started the call, and the result is read and turned into wxl's
//    types there, where the wrappers' allocator lives.
//  * Dropping the awaiter while the coroutine is suspended on it cancels the
//    operation and unhooks the coroutine: when the completion comes, it finds the
//    state abandoned and does nothing. A coroutine stopped from outside (its holder
//    destroyed the frame) is therefore never resumed, and nothing writes into it after
//    it is gone. This is the contract wxl's event waits keep too.
//  * A failure -- the HRESULT of the operation, or its cancellation -- is an
//    exception at the co_await, where the coroutine catches it as its own.
//
// The state is shared between the awaiter and the completion that runs on another
// thread, so it is a std::shared_ptr: the one place where wxl takes an atomic count on
// purpose, for an object that is made once per user action and never in a loop.

#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace wxl {

namespace impl {

// What the awaiter and the completion share, apart from the result. Touched on the
// interface thread only; the pointer to it itself crosses threads.
struct OperationStateBase {
    std::coroutine_handle<> waiter;  // the coroutine suspended on it, if any
    bool done = false;               // the result, or the error, has arrived
    bool abandoned = false;          // the awaiter is gone; the completion does nothing
    std::exception_ptr error;        // what the operation failed with
    std::function<void()> cancel;    // asks the runtime to stop it; set by the producer

    virtual ~OperationStateBase() = default;
};

template <class T>
struct OperationState final : OperationStateBase {
    alignas(T) unsigned char storage[sizeof(T)];
    bool has_value = false;

    template <class... Arguments>
    void emplace(Arguments&&... arguments) {
        ::new (static_cast<void*>(storage)) T(std::forward<Arguments>(arguments)...);
        has_value = true;
    }

    T take() {
        T* const value = std::launder(reinterpret_cast<T*>(storage));
        T result = std::move(*value);
        value->~T();
        has_value = false;
        return result;
    }

    ~OperationState() override {
        if (has_value) std::launder(reinterpret_cast<T*>(storage))->~T();
    }
};

template <>
struct OperationState<void> final : OperationStateBase {};

}  // namespace impl

template <class T>
class Operation {
public:
    explicit Operation(std::shared_ptr<impl::OperationState<T>> state) noexcept : state_(std::move(state)) {}

    Operation(Operation&&) noexcept = default;
    Operation& operator=(Operation&& other) noexcept {
        if (this != &other) {
            abandon();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    Operation(Operation const&) = delete;
    Operation& operator=(Operation const&) = delete;

    ~Operation() { abandon(); }

    /// Whether the result is already here: the call has finished before it was awaited.
    bool await_ready() const noexcept { return state_->done; }

    void await_suspend(std::coroutine_handle<> waiter) noexcept { state_->waiter = waiter; }

    /// The result, or the failure of the operation as an exception.
    T await_resume() {
        auto const state = std::move(state_);
        if (state->error) std::rethrow_exception(state->error);
        if constexpr (!std::is_void_v<T>) {
            return state->take();
        }
    }

private:
    // An awaiter let go before the end of the operation stops it. After the end there is
    // nothing to stop, and `done` says so.
    void abandon() noexcept {
        if (state_ && !state_->done) {
            state_->abandoned = true;
            state_->waiter = nullptr;
            if (state_->cancel) state_->cancel();
        }
    }

    std::shared_ptr<impl::OperationState<T>> state_;
};

}  // namespace wxl
