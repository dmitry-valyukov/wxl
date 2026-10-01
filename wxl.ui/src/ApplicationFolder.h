#pragma once

// The folder the running executable lives in: where a relative path such as
// "Assets/data.json" points, the same way an Image's source and a window's
// background picture resolve it. For an application that reads its own data
// files, which the build copies there.

#include "impl/application_folder.h"

namespace wxl {

inline std::filesystem::path applicationFolder() {
    return impl::application_folder();
}

}  // namespace wxl
