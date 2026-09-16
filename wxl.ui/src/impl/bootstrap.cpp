#include "bootstrap.h"

#include "hresult.h"

#include <mutex>

#include <windows.h>

#include <MddBootstrap.h>

namespace wxl::impl {

void ensure_windows_app_runtime_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        // majorMinorVersion: for release 2.0+, only the major version is
        // consulted (minor is ignored -- see MddBootstrap.h) -- 0x00020000
        // selects "release 2", matching the 2.3.2 WinUI metadata wxl
        // currently targets. No version tag (this is a
        // stable release, not a preview build), no minimum version floor.
        check_hresult(::MddBootstrapInitialize(0x00020000, nullptr, {}));
    });
}

} // namespace wxl::impl
