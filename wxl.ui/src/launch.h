#pragma once

// How an application built on wxl starts and stops.
//
// wxl owns the entry point. It brings up everything the library needs -- the
// Windows App Runtime, the STA apartment and its memory pool, the XAML
// application object and the framework's own control resources -- and only
// then calls the application. So an application writes no wWinMain, no
// bootstrap call, no Application subclass: one function, and the declarative
// UI inside it.

#include "core.h"

namespace wxl {

// Why the application is going down. It is the one argument the Teardown
// handler is given, because it is the one thing that changes what such a
// handler does: state worth saving on a normal close is usually not worth
// writing over good state on the way out of a failure.
enum class Reason {
    Closed,  // the application closed normally
    Error,   // it is going down because of a failure
};

// What wxl_launched hands back: the handler wxl calls on the way down.
//
// A callable rather than an opaque pointer handed to a separate exit
// function: on a single STA thread the closure captures whatever state it
// needs directly, so the round trip through a void* buys nothing. What the
// handler returns is what the process returns. Most applications have nothing
// to say there and return nothing, which is zero; one that hosts another
// program hands on what that program said.
//
// The callback is erased into wxl's own core::function -- one allocation with
// the capture laid out inside it -- and held by refcount, the way Preset and
// FormattedBlock hold theirs. std::function is not used, and neither is a
// std::unique_ptr around the core::function: a std smart pointer here would
// pull the textual std library into a header that reaches the module std
// through import, and the two do not share a translation unit that also
// includes cppwinrt (which launch.cpp does).
class Teardown {
    // A preset's setters and this handler alike are immutable once made, so
    // copies of either share the one callback by reference count -- which is
    // what core::function is, held by value.
    using handler_t = core::function<int(Reason)>;

public:
    Teardown() noexcept = default;

    /// From any callable taking a Reason. One returning nothing means an exit
    /// code of zero; one returning an int names the code -- how a program
    /// hosting another hands on what that one said.
    template <typename F>
        requires(!std::same_as<std::decay_t<F>, Teardown>) &&
                (core::invocable<F, int(Reason)> || core::invocable<F, void(Reason)>)
    Teardown(F&& handler) : handler_{make(std::forward<F>(handler))} {}

    explicit operator bool() const noexcept { return static_cast<bool>(handler_); }

    /// Runs the handler; the result is the process's exit code, zero when
    /// there is no handler.
    int operator()(Reason reason) const { return handler_ ? (*handler_)(reason) : 0; }

private:
    template <typename F>
    static handler_t make(F&& handler) {
        if constexpr (core::invocable<F, int(Reason)>) {
            return std::forward<F>(handler);
        } else {
            // A void handler names no code, which is a code of zero.
            return [inner = std::forward<F>(handler)](Reason reason) mutable {
                inner(reason);
                return 0;
            };
        }
    }

    // Nullable, because an application with nothing to do on the way down is
    // the ordinary case and a core::function is never empty. The empty state
    // costs nothing here: it is the null body the function already has room
    // for.
    core::nullable<handler_t> handler_;
};

}  // namespace wxl

// The application's only entry point, implemented by the application and
// called by wxl.
//
// Named after the moment WinUI itself calls "launched", which is exactly when
// it runs -- not "activated", because that word already means a window
// gaining focus.
//
// It runs once everything is up, so the declarative UI goes straight into it,
// and what it returns is what wxl calls on the way down (it may be empty).
// The returned handler is also what keeps the window alive: capture it there,
// and it lives exactly as long as the application does.
//
//     wxl::Teardown wxl_launched() {
//         auto window = wxl::Window{ /* ... */ };
//         window.activate();
//         return [window](wxl::Reason) {};
//     }
//
// A handler that returns an int names the process's exit code -- how a
// program hosting another one hands on what that one said:
//
//     return [app](wxl::Reason) { app->stop(); return app->exitCode(); };
wxl::Teardown wxl_launched();
