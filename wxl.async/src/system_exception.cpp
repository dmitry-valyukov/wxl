module;
#include "pch.h"

module wxl.async;
import std;

namespace wxl::async {

namespace {

/// Текст, которым система объясняет свой код ошибки.
///
/// Пусто, если объяснения нет: FormatMessage знает не про все коды, и
/// сетевые ищутся отдельно, в netmsg.dll, — она грузится без разрешения
/// импортов, потому что нужны из неё только строки.
std::string describe(DWORD err_code) {
    const DWORD langid = LANGIDFROMLCID(::GetThreadLocale());

    HLOCAL buffer = nullptr;
    DWORD length =
        ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr,
                         err_code, langid, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

    if (!length) {
        if (HMODULE netmsg = ::LoadLibraryExW(L"netmsg.dll", nullptr,
                                              DONT_RESOLVE_DLL_REFERENCES)) {
            length = ::FormatMessageA(
                FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_ALLOCATE_BUFFER,
                netmsg, err_code, langid, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
            ::FreeLibrary(netmsg);
        }
    }

    std::string text;

    if (length && buffer) {
        const char* chars = static_cast<const char*>(static_cast<void*>(buffer));

        // FormatMessage заканчивает строку переводом строки; в сообщении
        // исключения он ни к чему.
        if (length > 2 && chars[length - 2] == '\r') length -= 2;

        text.assign(chars, length);
    }

    ::LocalFree(buffer);
    return text;
}

/// Собирает сообщение целиком: сперва наше, потом системное, потом код.
///
/// Отдельной функцией, потому что std::runtime_error принимает текст в
/// конструкторе и дописать к нему потом уже нельзя.
std::string compose(std::string_view msg, int err_code) {
    const std::string reason = describe(static_cast<DWORD>(err_code));

    std::string text(msg);

    if (!reason.empty()) {
        if (!text.empty()) text += ". ";
        text += reason;
    }

    return text + std::format(" System error code = {}", err_code);
}

}  // namespace

system_exception::system_exception(std::string_view msg, int err_code)
    : std::runtime_error(compose(msg, err_code)), err_code_(err_code) {}

}  // namespace wxl::async
