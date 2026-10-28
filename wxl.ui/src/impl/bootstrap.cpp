#include "platform.h"

#include <mutex>

#include <MddBootstrap.h>
// After MddBootstrap.h: with the macros of this header defined first,
// MddBootstrap.h also declares its C++ wrappers, which nothing here calls.
#include <WindowsAppSDK-VersionInfo.h>

#include "winrt_helpers.h"

namespace wxl::impl {

void ensure_windows_app_runtime_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        // The release is the one of the Microsoft.WindowsAppSDK.Runtime package
        // the build restored. Its WindowsAppSDK-VersionInfo.h gives the major and
        // minor version in the form MddBootstrapInitialize takes (0xMMMMNNNN; for
        // release 2.0 and later the minor version is not consulted -- see
        // MddBootstrap.h) and the version tag, which with the major version names
        // the release's framework package, Microsoft.WindowsAppRuntime.<major>-<tag>:
        // the one that has the interfaces the release's metadata declares. A
        // stable release has an empty tag. No minimum version floor.
        constexpr wchar_t const* tag = WINDOWSAPPSDK_RELEASE_VERSION_TAG_W;
        check_hresult(::MddBootstrapInitialize(WINDOWSAPPSDK_RELEASE_MAJORMINOR,
                                               tag[0] ? tag : nullptr, {}));
    });
}

}  // namespace wxl::impl
