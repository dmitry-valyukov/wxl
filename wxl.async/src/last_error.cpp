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

std::wstring last_error::explanation() const {
    // Язык 0 -- порядок поиска самой системы: язык потока, пользователя, системы, английский.
    // Вставки в тексте (%1) нечем заполнить, и без IGNORE_INSERTS такая фраза не находится
    // вовсе. HRESULT система объясняет тем же путём.
    wchar_t* buffer = nullptr;
    DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(code().value()), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    std::wstring text;

    if (length && buffer) {
        // Система заканчивает фразу переводом строки (а иные -- и пробелом перед ним);
        // человеку показывают фразу.
        while (length && (buffer[length - 1] == L'\n' || buffer[length - 1] == L'\r' ||
                          buffer[length - 1] == L' ')) {
            --length;
        }
        text.assign(buffer, length);
    }

    if (buffer) ::LocalFree(buffer);
    return text;
}

}  // namespace wxl::async
