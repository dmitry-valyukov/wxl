#pragma once

// The folder the running executable lives in: where a relative path such as
// "Assets/data.json" points, the same way an Image's source and a window's
// background picture resolve it. For an application that reads its own data
// files, which the build copies there.

// The folder is core::environment::application_folder(): nothing about it
// is a window's, and a library below the window -- or a test of one -- asks
// for it too. This is its name in the application's vocabulary.

#include "core.h"

namespace wxl {

inline std::filesystem::path applicationFolder() {
    return core::environment::application_folder();
}

}  // namespace wxl
