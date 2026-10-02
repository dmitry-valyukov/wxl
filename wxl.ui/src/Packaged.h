#pragma once

// wxl::is_packaged -- whether the process runs with the identity of an MSIX package.
//
// Part of the Windows App SDK works only for an application the shell knows by a package
// identity: the badge on a taskbar button (BadgeNotificationManager), the packaged form of
// app notifications, the registration of background tasks. An unpackaged application, which is
// what wxl applications are as they come out of CMake, has none of it, and a call to such an API
// fails; an application that offers the feature asks first.

namespace wxl {

/// True when the process has a package identity.
bool is_packaged() noexcept;

}  // namespace wxl
