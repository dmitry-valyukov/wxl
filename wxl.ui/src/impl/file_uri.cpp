#include "file_uri.h"

// Imports last, after every plain header.
import wxl.core;

namespace wxl::impl {
namespace {

bool is_separator(char16_t c) noexcept { return c == u'/' || c == u'\\'; }

bool is_letter(char16_t c) noexcept { return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'); }

bool is_drive(std::u16string_view text) noexcept { return text.size() >= 2 && is_letter(text[0]) && text[1] == u':'; }

// The names of the scheme and of the local host are ASCII, and compared as such.
bool equals_ignoring_case(std::u16string_view text, std::u16string_view lower) noexcept {
    if (text.size() != lower.size()) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char16_t const c = text[i] >= u'A' && text[i] <= u'Z' ? static_cast<char16_t>(text[i] + (u'a' - u'A')) : text[i];
        if (c != lower[i]) return false;
    }
    return true;
}

int hex_value(char16_t c) noexcept {
    if (c >= u'0' && c <= u'9') return c - u'0';
    if (c >= u'a' && c <= u'f') return c - u'a' + 10;
    if (c >= u'A' && c <= u'F') return c - u'A' + 10;
    return -1;
}

// Escapes are bytes of UTF-8, and a character spans several of them, so a run of escapes is
// decoded as a whole; what is not escaped is kept as it was written. A '%' that does not
// open an escape is an ordinary character.
bool try_unescape(std::u16string_view text, std::u16string& result) {
    result.clear();
    result.reserve(text.size());
    std::string bytes;
    auto const flush = [&] {
        if (bytes.empty()) return true;
        auto const decoded = core::unicode::checked(std::string_view {bytes});
        if (!decoded) return false;
        result += decoded->to_utf16().plain();
        bytes.clear();
        return true;
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'%' && i + 2 < text.size() && hex_value(text[i + 1]) >= 0 && hex_value(text[i + 2]) >= 0) {
            bytes.push_back(static_cast<char>(hex_value(text[i + 1]) * 16 + hex_value(text[i + 2])));
            i += 2;
            continue;
        }
        if (!flush()) return false;
        result.push_back(text[i]);
    }
    return flush();
}

}  // namespace

bool try_path_of_file_uri(std::u16string_view address, std::u16string& path) {
    constexpr std::u16string_view scheme = u"file:";
    if (address.size() < scheme.size() || !equals_ignoring_case(address.substr(0, scheme.size() - 1), u"file") ||
        address[scheme.size() - 1] != u':') {
        return false;
    }
    std::u16string_view rest = address.substr(scheme.size());
    rest = rest.substr(0, rest.find_first_of(u"?#"));

    // The authority follows two separators: empty for file:///M:/a, "localhost" for the same
    // file said with a host, a drive for file://M:/a written without the third slash, and a
    // server for a network path.
    std::u16string_view authority;
    if (rest.size() >= 2 && is_separator(rest[0]) && is_separator(rest[1])) {
        rest.remove_prefix(2);
        auto const end = rest.find_first_of(u"/\\");
        authority = rest.substr(0, end);
        rest = end == std::u16string_view::npos ? std::u16string_view {} : rest.substr(end);
    }

    std::u16string text;
    path.clear();
    if (is_drive(authority)) {
        path = authority;
    } else if (!authority.empty() && !equals_ignoring_case(authority, u"localhost")) {
        if (!try_unescape(authority, text)) return false;
        path = u"\\\\" + text;
    } else if (rest.size() >= 3 && is_separator(rest[0]) && is_drive(rest.substr(1))) {
        rest.remove_prefix(1);  // "/M:/a": the slash belongs to the address, not to the path
    }

    if (!try_unescape(rest, text)) return false;
    path += text;
    for (auto& c : path) {
        if (c == u'/') c = u'\\';
    }
    return true;
}

}  // namespace wxl::impl
