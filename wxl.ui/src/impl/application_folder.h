#pragma once

#include <filesystem>

// Where a relative path an application writes -- "Assets/logo.png" -- points:
// beside its executable. The build copies an application's assets there, so a
// picture named in an Image's source and one named as a window's background
// are found the same way.

namespace wxl::impl {

/// The folder the executable was started from.
std::filesystem::path application_folder();

/// The path as it is when absolute, otherwise the same path in the application
/// folder.
std::filesystem::path beside_application(std::filesystem::path const& path);

}  // namespace wxl::impl
