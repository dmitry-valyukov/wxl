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
enum class TeardownReason {
    Closed,  // the application closed normally
    Error,   // it is going down because of a failure
};

// What wxl_launched hands back: the handler wxl calls on the way down, or
// nothing when the application has nothing to do there.
//
// What the handler returns is the process's exit code. One that returns
// nothing leaves it at zero; one that hosts another program hands on what that
// program said. The code is a std::optional<int> rather than a nullable<int>,
// which spends INT_MIN on its empty state, because an exit code may be any int.
using Teardown = core::nullable<core::function<std::optional<int>(TeardownReason)>>;

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
//
//     wxl::Teardown wxl_launched() {
//         auto window = wxl::Window{ /* ... */ };
//         window.activate();
//         return {};
//     }
//
// A handler that returns an int names the process's exit code -- how a
// program hosting another one hands on what that one said:
//
//     return [app](wxl::TeardownReason) { app->stop(); return app->exitCode(); };
wxl::Teardown wxl_launched();
