export module wxl.async:cancellation_scope;

import :cancellation;
import :coroutine_checks;
import wxl.core;
import std;

// The implicit token, as an experiment beside the explicit one: off unless the library is
// built with WXL_AMBIENT_CANCELLATION, and then every task and detached_task takes part.
#ifdef WXL_AMBIENT_CANCELLATION

export namespace wxl::async {

namespace cancellation_scope_detail {

/// A coroutine body as the implicit token sees it -- or the code around all of them,
/// which is one too: the token its waits stand under and the tasks it starts inherit,
/// and the frame that was running when it was last resumed, to hand the thread back to.
struct frame {
    cancellation_detail::cancellation_state_ptr scope;
    frame* resumer = nullptr;
};

/// The code that is no coroutine's: the bottom of the stack.
inline frame outermost;

/// The frame whose body is on top of the stack. One thread -- the one coroutines run
/// on -- so a plain variable: set where a body is resumed, put back where it suspends.
inline frame* running = &outermost;

/// What a task's promise is: born under the token of the frame that calls it, and
/// running from its birth to its first suspension.
struct inheriting_frame : frame {
    inline inheriting_frame() noexcept {
        scope = running->scope;
        resumer = running;
        running = this;
    }
};

/// What a detached_task's promise is: an orphan is nobody's, and inherits nothing. Its
/// body can stand under a token all the same, by naming one (`cancellation_scope`).
struct orphan_frame : frame {
    inline orphan_frame() noexcept {
        resumer = running;
        running = this;
    }
};

/// The operand of a co_await that is another coroutine -- a task, awaited or joined.
template <class Awaitable>
concept coroutine_object = requires { typename std::remove_cvref_t<Awaitable>::promise_type; };

/// Every co_await in a body, as the implicit token compiles it: the running frame handed
/// back where the body suspends and taken again where it is resumed, and the wait under
/// the frame's token -- told through the awaiter's cancel() if it can be, answered with
/// operation_canceled_exception once the token is cancelled. A wait for another task is
/// not answered: that task stands under the same token and answers for itself.
template <class Awaiter, bool joins>
class scoped_wait
    : cancellation_detail::standing<!joins && cancellable_awaiter<std::remove_reference_t<Awaiter>>>
{
    using awaiter_value_t = std::remove_reference_t<Awaiter>;

    static constexpr bool can_be_told = !joins && cancellable_awaiter<awaiter_value_t>;

    using standing = cancellation_detail::standing<can_be_told>;

public:
    /// The awaiter is made in place, by `make`: some awaiters do not move.
    template <class Make>
    inline scoped_wait(frame& self, Make&& make)
        : standing(&tell_this), self_(self), scope_(self.scope.get()), awaiter_(make()) {}

    scoped_wait(const scoped_wait&) = delete;
    scoped_wait& operator=(const scoped_wait&) = delete;

    inline ~scoped_wait() { this->leave(); }

    inline bool await_ready(coro_detail::site where = coro_detail::site::current()) {
        if constexpr (!joins) {
            if (scope_ && scope_->canceled()) [[unlikely]] {
                if constexpr (can_be_told) {
                    awaiter_.cancel();
                    return ready(where);
                } else {
                    return true;
                }
            }
        }

        return ready(where);
    }

    template <class Promise>
    inline decltype(auto) await_suspend(std::coroutine_handle<Promise> awaiting,
                                        [[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        if constexpr (can_be_told)
            if (scope_ && !scope_->canceled()) scope_->enter(*this);

        // Handed back before the awaiter is: past it, this frame may be running again
        // elsewhere, or gone.
        running = self_.resumer;

        if constexpr (requires { awaiter_.await_suspend(awaiting, where); }) {
            // One of this module's, which do not throw.
            return awaiter_.await_suspend(awaiting, where);
        } else if constexpr (noexcept(awaiter_.await_suspend(awaiting))) {
            return awaiter_.await_suspend(awaiting);
        } else {
            // An await_suspend that throws resumes the body with the exception, past
            // await_resume: the body is running again here.
            try {
                return awaiter_.await_suspend(awaiting);
            } catch (...) {
                running = &self_;
                throw;
            }
        }
    }

    inline decltype(auto) await_resume([[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        // Not suspended at all -- ready at once, or an await_suspend that declined --
        // leaves the frame running where it was.
        if (running != &self_) {
            self_.resumer = running;
            running = &self_;
        }

        this->leave();

        if constexpr (!joins)
            if (scope_ && scope_->canceled()) [[unlikely]]
                throw operation_canceled_exception();

        if constexpr (requires { awaiter_.await_resume(where); })
            return awaiter_.await_resume(where);
        else
            return awaiter_.await_resume();
    }

private:
    inline bool ready([[maybe_unused]] coro_detail::site where) {
        if constexpr (requires { awaiter_.await_ready(where); })
            return awaiter_.await_ready(where);
        else
            return awaiter_.await_ready();
    }

    static void tell_this(cancellation_detail::wait& told) noexcept {
        if constexpr (can_be_told) static_cast<scoped_wait&>(told).awaiter_.cancel();
    }

    frame& self_;

    /// Read once: a scope set inside the body changes it between waits, never during one.
    cancellation_detail::cancellation_state* scope_;

    Awaiter awaiter_;
};

}  // namespace cancellation_scope_detail

/// The token the running coroutine stands under, from here to the end of the block: its
/// waits, and the tasks it starts, which inherit it at birth. Without a token it is the
/// shield -- cleanup that must not be cut short, under no token at all.
///
/// In code that is no coroutine's, it is what a chain is started under:
/// `cancellation_scope under(stop.token()); owned.push_back(flow());`.
class cancellation_scope
{
public:
    inline explicit cancellation_scope(const cancellation_token& token) noexcept
        : frame_(*cancellation_scope_detail::running),
          previous_(std::exchange(frame_.scope, cancellation_detail::cancellation_state_ptr(
                                                    cancellation_detail::state_of(token)))) {}

    inline cancellation_scope() noexcept
        : frame_(*cancellation_scope_detail::running),
          previous_(std::exchange(frame_.scope, nullptr)) {}

    cancellation_scope(const cancellation_scope&) = delete;
    cancellation_scope& operator=(const cancellation_scope&) = delete;

    inline ~cancellation_scope() { frame_.scope = std::move(previous_); }

private:
    cancellation_scope_detail::frame& frame_;
    cancellation_detail::cancellation_state_ptr previous_;
};

}  // export namespace wxl::async

#endif
