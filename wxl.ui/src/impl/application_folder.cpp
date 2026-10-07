#include "application_folder.h"

// Imports last, after every plain header.
import wxl.core;

namespace wxl::impl {

std::filesystem::path beside_application(std::filesystem::path const& path) {
    return path.is_absolute() ? path : core::environment::application_folder() / path;
}

}  // namespace wxl::impl
