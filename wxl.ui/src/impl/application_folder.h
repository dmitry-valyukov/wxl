#pragma once

#include <filesystem>

// A relative path an application writes -- "Assets/logo.png" -- points beside
// its executable, where the build copies its assets. The folder itself is
// core::environment::application_folder(); this is the one step wxl.ui adds
// on top, so that a picture named as a window's background and one named in
// an Image's source are found the same way.

namespace wxl::impl {

/// The path as it is when absolute, otherwise the same path in the application
/// folder.
std::filesystem::path beside_application(std::filesystem::path const& path);

}  // namespace wxl::impl
