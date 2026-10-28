module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::async {

// Объяснение системы дописывает сама std::system_error: её категория зовёт FormatMessage, как
// звали бы и мы, а код, которого система не знает, называет «unknown error». Наше -- что
// делали и какой код, чтобы по нему искать.
last_error::last_error(std::string_view msg, int err_code)
    : std::system_error(err_code, std::system_category(),
                        std::format("{} (system error code = {})", msg, err_code)) {}

std::wstring last_error::explanation() const {
    // Слова системы даёт environment ядра -- они нужны всякому, кто сообщает о сбое; здесь
    // лишь обратно в UTF-16, в котором их показывает интерфейс.
    return std::wstring{
        core::environment::error_explanation(static_cast<std::uint32_t>(code().value()))
            .to_utf16()
            .wchars()};
}

}  // namespace wxl::async
