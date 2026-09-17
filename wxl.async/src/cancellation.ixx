export module wxl.async:cancellation;

import wxl.core;
import std;

export namespace wxl::async {

/// The operation was cancelled.
///
/// Lives here rather than with the component model that used to own it: an
/// asynchronous operation may be cancelled without any component being
/// involved, and the layer that hands out the right to cancel is the one that
/// has to name the outcome.
class operation_canceled_exception : public std::runtime_error
{
    using base = std::runtime_error;

public:
    inline operation_canceled_exception() : base("Operation was canceled") {}
    inline explicit operation_canceled_exception(const char* message) : base(message) {}
    inline explicit operation_canceled_exception(std::string_view message)
        : base(std::string(message)) {}
};

namespace cancellation_detail {

/// The flag both ends share, kept alive by whichever of them outlives the
/// other. The count sits inside the object rather than in a control block
/// beside it, and it is the multi-threaded base because the token travels to
/// whatever thread runs the operation while cancelling usually happens on the
/// thread that asked for it.
class cancellation_state : public core::refcounted_mt
{
public:
    inline bool canceled() const noexcept { return canceled_.load(std::memory_order_acquire); }

    inline void cancel() noexcept { canceled_.store(true, std::memory_order_release); }

private:
    std::atomic<bool> canceled_{false};
};

using cancellation_state_ptr = core::intrusive_ptr<cancellation_state>;

}  // namespace cancellation_detail

/// The right to cancel an operation, handed to whoever asked for it.
///
/// A token is passed into an asynchronous operation and read by it at the
/// points where stopping is possible. What a cancelled operation does with the
/// answer is its own business, but the shape is always the same: it does not
/// start the work, or it stops between steps, and it reports
/// operation_canceled_exception to whoever was waiting -- to the same place
/// that a value or an error would have gone, because cancellation is an
/// outcome like the other two and travels the same way.
///
/// What a token does NOT do is interrupt work already under way. A blocking
/// call that has started will finish; there is nothing to interrupt it with.
/// So the grain of cancellation is one operation, and a cancelled loop stops
/// at its next step rather than in the middle of the current one.
///
/// A default-constructed token is never cancelled and costs nothing: it has no
/// state, and asking is a comparison against a null pointer.
class cancellation_token
{
public:
    cancellation_token() noexcept = default;

    inline bool is_canceled() const noexcept { return state_ && state_->canceled(); }

    /// \throw operation_canceled_exception if cancellation has been requested.
    inline void throw_if_canceled() const {
        if (is_canceled()) throw operation_canceled_exception();
    }

private:
    friend class cancellation_source;

    inline explicit cancellation_token(cancellation_detail::cancellation_state* state) noexcept
        : state_(state) {}

    cancellation_detail::cancellation_state_ptr state_;
};

/// The other end of a token: the one that cancels.
///
/// The state is shared and lives as long as the last holder, so an operation
/// may outlive the source that cancelled it, and the source may outlive the
/// operation.
class cancellation_source
{
public:
    /// The reference the state is born with is taken over rather than added
    /// to: objects deriving from refcounted_mt start at a count of one.
    inline cancellation_source() : state_(new cancellation_detail::cancellation_state, false) {}

    inline cancellation_token token() const noexcept { return cancellation_token(state_.get()); }

    inline void cancel() noexcept { state_->cancel(); }

    inline bool is_canceled() const noexcept { return state_->canceled(); }

private:
    cancellation_detail::cancellation_state_ptr state_;
};

}  // export namespace wxl::async
