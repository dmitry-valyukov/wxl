module;
#include "pch.h"

module wxl.async;
import std;

namespace wxl::async {

// Объяснение системы дописывает сама std::system_error: её категория зовёт FormatMessage, как
// звали бы и мы, а код, которого система не знает, называет «unknown error». Наше -- что
// делали и какой код, чтобы по нему искать.
last_error::last_error(std::string_view msg, int err_code)
    : std::system_error(err_code, std::system_category(),
                        std::format("{} (system error code = {})", msg, err_code)) {}

}  // namespace wxl::async
