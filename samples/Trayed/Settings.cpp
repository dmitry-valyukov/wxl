// Reading and writing the settings -- the registry and the model -- and the
// tables the index-valued settings stand for: which font families are offered,
// and which themes.

// NOMINMAX so the min/max macros do not wreck the std::min/max the wxl headers
// (reached through Settings.h -> core.h) use.
#define NOMINMAX
#include <windows.h>

#include "Settings.h"

namespace {

wchar_t const kKey[] = L"Software\\wxl\\Trayed";

DWORD readDword(HKEY key, wchar_t const* name, DWORD fallback) {
    DWORD value = 0;
    DWORD size = sizeof value;
    DWORD type = 0;
    if (::RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) ==
            ERROR_SUCCESS &&
        type == REG_DWORD) {
        return value;
    }
    return fallback;
}

void writeDword(HKEY key, wchar_t const* name, DWORD value) {
    ::RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<BYTE const*>(&value), sizeof value);
}

std::wstring readString(HKEY key, wchar_t const* name, std::wstring const& fallback) {
    DWORD size = 0;
    if (::RegQueryValueExW(key, name, nullptr, nullptr, nullptr, &size) != ERROR_SUCCESS ||
        size < 2) {
        return fallback;
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (::RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<BYTE*>(value.data()),
                           &size) != ERROR_SUCCESS) {
        return fallback;
    }

    // The stored size counts the terminating null; drop it (and anything written
    // past the text) so the string is exactly its characters.
    value.resize(::lstrlenW(value.c_str()));
    return value;
}

void writeString(HKEY key, wchar_t const* name, std::wstring const& value) {
    ::RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<BYTE const*>(value.c_str()),
                     static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

/// A stored index kept inside its table: a table that has since shrunk must not
/// leave a setting pointing past its end.
int clampIndex(int value, std::size_t count) {
    if (value < 0) return 0;
    if (count > 0 && static_cast<std::size_t>(value) >= count) return static_cast<int>(count) - 1;
    return value;
}

}  // namespace

std::vector<std::wstring> const& fontFamilies() {
    // Curated, not enumerated: the faces a console is actually readable in.
    // Cascadia Mono ships with modern Windows; the rest are the old reliables,
    // present since long before it.
    static std::vector<std::wstring> const families = {
        L"Cascadia Mono",
        L"Consolas",
        L"Lucida Console",
        L"Courier New",
    };
    return families;
}

std::vector<std::wstring> const& themeNames() {
    static std::vector<std::wstring> const names = {
        L"Классическая",
        L"Campbell",
        L"Solarized Dark",
        L"Светлая",
    };
    return names;
}

namespace {

/// A console colour theme: the sixteen palette entries a cell index maps to.
/// Index 0 is the background and index 7 the foreground -- the pair ordinary
/// console text is written with -- so every theme stays legible for text that
/// was there before it was chosen; the other fourteen are what a program using
/// colour draws in.
struct Theme {
    COLORREF table[16];
};

std::vector<Theme> const& themes() {
    static std::vector<Theme> const list = {
        // Классическая -- the legacy Windows console palette.
        {{RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x80), RGB(0x00, 0x80, 0x00),
          RGB(0x00, 0x80, 0x80), RGB(0x80, 0x00, 0x00), RGB(0x80, 0x00, 0x80),
          RGB(0x80, 0x80, 0x00), RGB(0xC0, 0xC0, 0xC0), RGB(0x80, 0x80, 0x80),
          RGB(0x00, 0x00, 0xFF), RGB(0x00, 0xFF, 0x00), RGB(0x00, 0xFF, 0xFF),
          RGB(0xFF, 0x00, 0x00), RGB(0xFF, 0x00, 0xFF), RGB(0xFF, 0xFF, 0x00),
          RGB(0xFF, 0xFF, 0xFF)}},
        // Campbell -- the modern Windows default.
        {{RGB(0x0C, 0x0C, 0x0C), RGB(0x00, 0x37, 0xDA), RGB(0x13, 0xA1, 0x0E),
          RGB(0x3A, 0x96, 0xDD), RGB(0xC5, 0x0F, 0x1F), RGB(0x88, 0x17, 0x98),
          RGB(0xC1, 0x9C, 0x00), RGB(0xCC, 0xCC, 0xCC), RGB(0x76, 0x76, 0x76),
          RGB(0x3B, 0x78, 0xFF), RGB(0x16, 0xC6, 0x0C), RGB(0x61, 0xD6, 0xD6),
          RGB(0xE7, 0x48, 0x56), RGB(0xB4, 0x00, 0x9E), RGB(0xF9, 0xF1, 0xA5),
          RGB(0xF2, 0xF2, 0xF2)}},
        // Solarized Dark -- index 0 base03 (background), index 7 base0 (text).
        {{RGB(0x00, 0x2B, 0x36), RGB(0xDC, 0x32, 0x2F), RGB(0x85, 0x99, 0x00),
          RGB(0xB5, 0x89, 0x00), RGB(0x26, 0x8B, 0xD2), RGB(0xD3, 0x36, 0x82),
          RGB(0x2A, 0xA1, 0x98), RGB(0x83, 0x94, 0x96), RGB(0x07, 0x36, 0x42),
          RGB(0xCB, 0x4B, 0x16), RGB(0x58, 0x6E, 0x75), RGB(0x65, 0x7B, 0x83),
          RGB(0xEE, 0xE8, 0xD5), RGB(0x6C, 0x71, 0xC4), RGB(0x93, 0xA1, 0xA1),
          RGB(0xFD, 0xF6, 0xE3)}},
        // Светлая -- dark text (index 7) on a warm off-white (index 0).
        {{RGB(0xFD, 0xF6, 0xE3), RGB(0xC5, 0x0F, 0x1F), RGB(0x13, 0xA1, 0x0E),
          RGB(0xB5, 0x89, 0x00), RGB(0x00, 0x37, 0xDA), RGB(0x88, 0x17, 0x98),
          RGB(0x2A, 0xA1, 0x98), RGB(0x38, 0x3A, 0x42), RGB(0xAF, 0xAF, 0xAF),
          RGB(0xE7, 0x48, 0x56), RGB(0x16, 0xC6, 0x0C), RGB(0xC1, 0x9C, 0x00),
          RGB(0x3B, 0x78, 0xFF), RGB(0xB4, 0x00, 0x9E), RGB(0x2A, 0xA1, 0x98),
          RGB(0x1C, 0x1C, 0x1C)}},
    };
    return list;
}

// The default attributes every theme keeps: foreground index 7 on background
// index 0. Ordinary console text is written with exactly this pair, so leaving
// it fixed and moving only what 0 and 7 mean in the table is what recolours the
// text already on screen instead of stranding it in the previous theme.
constexpr WORD kDefaultAttributes = 7;

}  // namespace

void applyConsoleFont(int familyIndex, int sizePx) {
    HANDLE const out = ::GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_FONT_INFOEX font{};
    font.cbSize = sizeof font;
    // Height in pixels; width left 0, so the console keeps the face's own
    // aspect rather than being stretched.
    font.dwFontSize.Y = static_cast<SHORT>(sizePx);
    font.FontFamily = FF_DONTCARE;
    font.FontWeight = FW_NORMAL;

    std::vector<std::wstring> const& families = fontFamilies();
    std::wstring const& face = families[clampIndex(familyIndex, families.size())];
    std::size_t const fits = std::min(face.size(), std::size(font.FaceName) - 1);
    face.copy(font.FaceName, fits);
    font.FaceName[fits] = L'\0';

    ::SetCurrentConsoleFontEx(out, FALSE, &font);
}

void applyConsoleTheme(int themeIndex) {
    HANDLE const out = ::GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFOEX info{};
    info.cbSize = sizeof info;
    if (!::GetConsoleScreenBufferInfoEx(out, &info)) return;

    Theme const& theme = themes()[clampIndex(themeIndex, themes().size())];
    for (int i = 0; i < 16; ++i) info.ColorTable[i] = theme.table[i];
    info.wAttributes = kDefaultAttributes;

    // The documented quirk of this call: the srWindow it read back is inclusive
    // on the right and bottom, and handing it straight back shrinks the window
    // by a column and a row every time. Grow it by one to hold the size still.
    info.srWindow.Right += 1;
    info.srWindow.Bottom += 1;
    ::SetConsoleScreenBufferInfoEx(out, &info);
}

void loadSettings(Settings& settings) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;  // first run: the defaults
    }

    settings.minimizeOnClose.set(
        readDword(key, L"MinimizeOnClose", settings.minimizeOnClose.get()) != 0);
    settings.fontFamily.set(clampIndex(
        static_cast<int>(readDword(key, L"FontFamily", settings.fontFamily.get())),
        fontFamilies().size()));
    settings.fontSize.set(static_cast<int>(readDword(key, L"FontSize", settings.fontSize.get())));
    settings.theme.set(clampIndex(
        static_cast<int>(readDword(key, L"Theme", settings.theme.get())), themeNames().size()));
    // The stored title is text from outside -- repaired, not trusted: an odd
    // 16-bit unit in the registry becomes U+FFFD rather than breaking the value.
    settings.windowTitle.set(wxl::core::unicode::repaired(
        readString(key, L"WindowTitle", std::wstring{settings.windowTitle.get().wchars()})));

    ::RegCloseKey(key);
}

void saveSettings(Settings const& settings) {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                          nullptr) != ERROR_SUCCESS) {
        return;
    }

    writeDword(key, L"MinimizeOnClose", settings.minimizeOnClose.get() ? 1 : 0);
    writeDword(key, L"FontFamily", static_cast<DWORD>(settings.fontFamily.get()));
    writeDword(key, L"FontSize", static_cast<DWORD>(settings.fontSize.get()));
    writeDword(key, L"Theme", static_cast<DWORD>(settings.theme.get()));
    writeString(key, L"WindowTitle", std::wstring{settings.windowTitle.get().wchars()});

    ::RegCloseKey(key);
}
