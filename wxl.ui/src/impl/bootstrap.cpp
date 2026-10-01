#include "platform.h"

#include "bootstrap.h"

#include "hresult.h"

#include <mutex>

#include <MddBootstrap.h>

namespace wxl::impl {

void ensure_windows_app_runtime_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        // majorMinorVersion: for release 2.0+, only the major version is
        // consulted (minor is ignored -- see MddBootstrap.h) -- 0x00020000
        // selects "release 2". The version tag names the channel: the metadata
        // wxl.ui is generated from is the experimental one, and that framework
        // package ("2-experimentalF") is what has the interfaces it declares;
        // empty is the stable channel (see WXL_WINDOWSAPPSDK_VERSION_TAG in
        // CMakeLists.txt). No minimum version floor.
        constexpr wchar_t const* tag = WXL_WINDOWSAPPSDK_VERSION_TAG;
        check_hresult(::MddBootstrapInitialize(0x00020000, tag[0] ? tag : nullptr, {}));
    });
}

} // namespace wxl::impl
