#pragma once

namespace wxl::impl {

// Ensures the Windows App Runtime framework package -- the redistributable
// backing WinUI3/Microsoft.UI.Xaml at runtime, separate from the .winmd
// metadata used at generator time (see the WinUI3 .winmd
// metadata") -- is resolved and available to this process, via the
// WindowsAppSDK bootstrapper (MddBootstrapInitialize). Idempotent: safe to
// call from every place that's about to touch a real WinUI3 API (real
// composable activation, once built); the actual
// initialization happens once, the first time any caller reaches it.
//
// This exists so that *using* wxl never requires an application author to
// write any WindowsAppSDK bootstrap boilerplate themselves -- wxl owns this
// responsibility internally, once, rather than pushing it onto every
// consuming app.
//
// Throws wxl::hresult_error if the framework package can't be resolved
// (e.g. the Windows App Runtime isn't installed on the machine, or the
// process isn't otherwise eligible to use it).
void ensure_windows_app_runtime_initialized();

} // namespace wxl::impl
