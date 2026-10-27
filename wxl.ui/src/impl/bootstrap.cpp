#include "platform.h"

#include <mutex>

#include <MddBootstrap.h>

#include "winrt_helpers.h"

namespace wxl::impl {

void ensure_windows_app_runtime_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        // majorMinorVersion: for release 2.0+, only the major version is
        // consulted (minor is ignored -- see MddBootstrap.h) -- 0x00020000
        // selects "release 2". The version tag is that of the Windows App SDK
        // release wxl.ui is built against, set by hand to match it (see
        // WXL_WINDOWSAPPSDK_VERSION_TAG in CMakeLists.txt); with the major
        // version it names the release's framework package ("2-experimentalF"),
        // the one that has the interfaces the release's metadata declares. A
        // stable release has an empty tag. No minimum version floor.
        constexpr wchar_t const* tag = WXL_WINDOWSAPPSDK_VERSION_TAG;
        check_hresult(::MddBootstrapInitialize(0x00020000, tag[0] ? tag : nullptr, {}));
    });
}

} // namespace wxl::impl
