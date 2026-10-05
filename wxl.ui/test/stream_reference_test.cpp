// RandomAccessStreamReference::CreateFromUri с адресом file:// -- impl::stream_reference_from_uri,
// на настоящей платформе (Windows.Storage), без WinUI.
//
// Обычная программа, а не gtest, по той же причине, что teardown_test: исходники, которые она
// собирает, доходят до wxl.core через core.h (include, затем import), а стандартные заголовки
// gtest с этим import в одной единице трансляции не уживаются. Итог -- в коде выхода.
//
// Что проверяется: ссылка, сделанная из file-адреса любого написания (три слеша, один, два
// с диском, localhost, обратные слеши, пробел как есть и как %20, кириллица как есть и UTF-8
// escape-ами), открывается и отдаёт байты файла, тип содержимого -- по расширению; адрес
// относительно базы читается так же; несуществующий файл -- ошибка при создании ссылки;
// escape не из UTF-8 -- ошибка неверного аргумента; адрес с другой схемой остаётся за
// платформой: ссылка делается и не читается заранее.
//
// В платформе без правки не читался ни один из этих адресов: OpenReadAsync отвечал E_NOTIMPL
// ("этот API поддерживает только схемы URI http, https, ms-appx и ms-appdata").

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "impl/file_stream_reference.h"

namespace {

namespace wf = winrt::Windows::Foundation;
namespace ss = winrt::Windows::Storage::Streams;

int failures = 0;

void check(bool ok, std::string const& what) {
    if (!ok) {
        std::fprintf(stderr, "stream_reference_test: FAILED -- %s\n", what.c_str());
        ++failures;
    }
}

std::string narrow(std::wstring_view text) {
    return winrt::to_string(text);
}

std::vector<uint8_t> make_bytes() {
    std::vector<uint8_t> bytes(5000);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>((i * 31 + i / 7) & 0xFF);
    return bytes;
}

// Читает ссылку целиком; false -- если она не открылась, причина -- в `error`.
bool read_all(ss::RandomAccessStreamReference const& reference, std::vector<uint8_t>& bytes, std::wstring& type, std::wstring& error) {
    try {
        auto const stream = reference.OpenReadAsync().get();
        type = stream.ContentType();
        ss::DataReader reader {stream.GetInputStreamAt(0)};
        auto const loaded = reader.LoadAsync(static_cast<uint32_t>(stream.Size())).get();
        bytes.resize(loaded);
        reader.ReadBytes(bytes);
        return true;
    } catch (winrt::hresult_error const& e) {
        error = std::wstring {e.message()};
        return false;
    }
}

// UTF-8 escapes для всего, кроме ASCII-букв, цифр и знаков пути.
std::wstring escaped(std::wstring_view text) {
    std::string const bytes = winrt::to_string(text);
    std::wstring out;
    for (unsigned char c : bytes) {
        if (c < 0x80 && (std::iswalnum(c) || c == '/' || c == ':' || c == '.' || c == '-' || c == '_')) {
            out += static_cast<wchar_t>(c);
        } else {
            wchar_t hex[4];
            std::swprintf(hex, 4, L"%%%02X", c);
            out += hex;
        }
    }
    return out;
}

void check_reads(std::wstring const& address, std::vector<uint8_t> const& expected, std::string const& what) {
    try {
        std::vector<uint8_t> bytes;
        std::wstring type, error;
        auto const reference = wxl::impl::stream_reference_from_uri(wf::Uri {address});
        check(read_all(reference, bytes, type, error), what + ": does not open: " + narrow(error) + " (" + narrow(address) + ")");
        check(bytes == expected, what + ": other bytes (" + narrow(address) + ")");
        check(type == L"image/jpeg", what + ": content type is " + narrow(type));
    } catch (winrt::hresult_error const& e) {
        check(false, what + ": " + narrow(e.message()) + " (" + narrow(address) + ")");
    }
}

}  // namespace

int main() {
    winrt::init_apartment();

    auto const root = std::filesystem::temp_directory_path() / L"wxl-stream-reference-test";
    std::filesystem::remove_all(root);
    auto const folder = root / L"папка с пробелом";
    std::filesystem::create_directories(folder);
    auto const bytes = make_bytes();
    std::vector<std::filesystem::path> const files {folder / L"plain.jpg", folder / L"with space.jpg", folder / L"Кириллица.jpg"};
    for (auto const& file : files) {
        std::ofstream out {file, std::ios::binary};
        out.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    for (auto const& file : files) {
        std::wstring const native = file.wstring();
        std::wstring forward = native;
        for (auto& c : forward) {
            if (c == L'\\') c = L'/';
        }
        std::string const name = file.filename().string();
        check_reads(L"file:///" + forward, bytes, name + " file:///");
        check_reads(L"file:///" + escaped(forward), bytes, name + " file:/// with escapes");
        check_reads(L"file:///" + native, bytes, name + " file:/// with backslashes");
        check_reads(L"file://localhost/" + forward, bytes, name + " file://localhost/");
        check_reads(L"file://" + forward, bytes, name + " file:// with the drive");
        check_reads(L"file:/" + forward, bytes, name + " file:/");
    }

    {
        std::wstring base = folder.wstring() + L"\\";
        for (auto& c : base) {
            if (c == L'\\') c = L'/';
        }
        try {
            std::vector<uint8_t> read;
            std::wstring type, error;
            auto const reference = wxl::impl::stream_reference_from_uri(wf::Uri {L"file:///" + base, L"with space.jpg"});
            check(read_all(reference, read, type, error) && read == bytes, "an address relative to a base: " + narrow(error));
        } catch (winrt::hresult_error const& e) {
            check(false, "an address relative to a base: " + narrow(e.message()));
        }
    }

    try {
        (void)wxl::impl::stream_reference_from_uri(wf::Uri {L"file:///" + std::filesystem::temp_directory_path().wstring() + L"/there-is-no-such-file.jpg"});
        check(false, "a file that is not there is an error");
    } catch (winrt::hresult_error const&) {
    }

    try {
        (void)wxl::impl::stream_reference_from_uri(wf::Uri {L"file:///M:/%FF.jpg"});
        check(false, "an escape that is not UTF-8 is an error");
    } catch (winrt::hresult_invalid_argument const&) {
    } catch (winrt::hresult_error const& e) {
        check(false, "an escape that is not UTF-8 is an invalid argument, not " + narrow(e.message()));
    }

    // Не файл -- платформа: ссылка делается, и ничто не читается раньше времени.
    for (wchar_t const* address : {L"https://example.invalid/a.png", L"ms-appx:///a.png", L"ms-appdata:///local/a.png"}) {
        try {
            check(static_cast<bool>(wxl::impl::stream_reference_from_uri(wf::Uri {address})), std::string {"a reference is made for "} + narrow(address));
        } catch (winrt::hresult_error const& e) {
            check(false, std::string {"a reference is made for "} + narrow(address) + ": " + narrow(e.message()));
        }
    }

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return failures == 0 ? 0 : 1;
}
