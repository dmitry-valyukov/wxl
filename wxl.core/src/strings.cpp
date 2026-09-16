module wxl.core;

import std;

namespace wxl::core {
namespace {

constexpr std::uint64_t high_bits = 0x8080808080808080ull;

/// The escaped form of every character that cannot stand as itself, and an
/// empty view for the ones that can. A 256-entry table rather than a switch:
/// the scan below asks the question once per candidate byte, and a lookup has
/// nothing to mispredict.
constexpr std::array<std::string_view, 256> entities = [] {
    std::array<std::string_view, 256> table{};
    table[static_cast<unsigned char>('&')] = "&amp;";
    table[static_cast<unsigned char>('<')] = "&lt;";
    table[static_cast<unsigned char>('>')] = "&gt;";
    table[static_cast<unsigned char>('"')] = "&quot;";
    return table;
}();

/// The text with every character passed through `map`, in a string of its own.
/// The callable is a template parameter so it inlines: a pointer to
/// to_ascii_lower would turn a shift-and-add into an indirect call per
/// character.
template <typename CharT, typename Map>
std::basic_string<CharT> mapped(std::basic_string_view<CharT> text, Map map) {
    std::basic_string<CharT> result;

    result.resize_and_overwrite(text.size(), [text, map](CharT* buffer, std::size_t size) {
        for (std::size_t at = 0; at != size; ++at) buffer[at] = map(text[at]);
        return size;
    });

    return result;
}

}  // namespace

bool is_ascii(const std::string_view text) noexcept {
    const char* at = text.data();
    const char* const end = at + text.size();

    while (static_cast<std::size_t>(end - at) >= sizeof(std::uint64_t)) {
        std::uint64_t word = 0;
        std::memcpy(&word, at, sizeof word);

        if ((word & high_bits) != 0) return false;

        at += sizeof(std::uint64_t);
    }

    while (at != end)
        if (static_cast<unsigned char>(*at++) >= 0x80) return false;

    return true;
}

std::string ascii_lower(const std::string_view text) {
    return mapped<char>(text, [](char character) noexcept { return to_ascii_lower(character); });
}

std::string ascii_upper(const std::string_view text) {
    return mapped<char>(text, [](char character) noexcept { return to_ascii_upper(character); });
}

std::wstring ascii_lower(const std::wstring_view text) {
    return mapped<wchar_t>(text,
                           [](wchar_t character) noexcept { return to_ascii_lower(character); });
}

std::wstring ascii_upper(const std::wstring_view text) {
    return mapped<wchar_t>(text,
                           [](wchar_t character) noexcept { return to_ascii_upper(character); });
}

void append_xml_escaped(std::string& out, const std::string_view text) {
    std::size_t run = 0;

    for (std::size_t at = 0; at != text.size(); ++at) {
        const std::string_view entity = entities[static_cast<unsigned char>(text[at])];

        if (entity.empty()) continue;

        // Whole runs between the escapes, so ordinary text -- which is nearly
        // all of it -- moves by memcpy rather than a byte at a time.
        out.append(text.substr(run, at - run));
        out.append(entity);
        run = at + 1;
    }

    out.append(text.substr(run));
}

std::string xml_escaped(const std::string_view text) {
    std::string result;
    result.reserve(text.size());
    append_xml_escaped(result, text);
    return result;
}

}  // namespace wxl::core
