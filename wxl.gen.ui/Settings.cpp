#include "platform.h"

#include "Settings.h"

#include <algorithm>

namespace editor {

namespace {

constexpr wchar_t key[] = L"Software\\wxl\\wxl.gen.ui";

DWORD readDword(HKEY root, wchar_t const* name, DWORD fallback) {
    DWORD value = 0;
    DWORD size = sizeof value;
    DWORD type = 0;
    if (::RegQueryValueExW(root, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS &&
        type == REG_DWORD) {
        return value;
    }
    return fallback;
}

void writeDword(HKEY root, wchar_t const* name, DWORD value) {
    ::RegSetValueExW(root, name, 0, REG_DWORD, reinterpret_cast<BYTE const*>(&value), sizeof value);
}

}  // namespace

void loadSettings(Settings& settings) {
    HKEY root = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &root) != ERROR_SUCCESS) {
        return;
    }
    settings.hideDependencyProperties.set(
        readDword(root, L"HideDependencyProperties", settings.hideDependencyProperties.get()) != 0);
    settings.showSignatures.set(readDword(root, L"ShowSignatures", settings.showSignatures.get()) != 0);
    settings.englishDocumentation.set(
        readDword(root, L"EnglishDocumentation", settings.englishDocumentation.get()) != 0);
    // Кегль из реестра — в пределах, которые даёт попап.
    auto const size = static_cast<int>(readDword(root, L"DetailsFontSize", settings.detailsFontSize.get()));
    settings.detailsFontSize.set(std::clamp(size, 12, 24));
    ::RegCloseKey(root);
}

void saveSettings(Settings const& settings) {
    HKEY root = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, nullptr, 0, KEY_WRITE, nullptr, &root, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }
    writeDword(root, L"HideDependencyProperties", settings.hideDependencyProperties.get() ? 1 : 0);
    writeDword(root, L"ShowSignatures", settings.showSignatures.get() ? 1 : 0);
    writeDword(root, L"EnglishDocumentation", settings.englishDocumentation.get() ? 1 : 0);
    writeDword(root, L"DetailsFontSize", static_cast<DWORD>(settings.detailsFontSize.get()));
    ::RegCloseKey(root);
}

}  // namespace editor
