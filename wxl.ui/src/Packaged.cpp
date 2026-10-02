#include "platform.h"

#include "Packaged.h"

#include <appmodel.h>

namespace wxl {

bool is_packaged() noexcept {
    UINT32 length = 0;
    return ::GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}

}  // namespace wxl
