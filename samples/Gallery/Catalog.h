#pragma once

// Каталог контролов — ControlInfoData.json оригинала, прочитанный один раз.
// Модель та же, что у оригинала (Models/ControlInfoData.cs): группы, в них
// контролы; какой контрол перенесён, каталог не знает — это вопрос
// `pageFor` (Pages.h).

#include <string>
#include <string_view>
#include <vector>

namespace gallery {

struct DocLink {
    std::wstring title;
    std::wstring uri;
};

struct ControlInfo {
    std::wstring uniqueId;
    std::wstring title;
    std::wstring subtitle;
    std::wstring description;
    std::wstring imagePath;
    std::wstring apiNamespace;
    std::wstring sourcePath;
    std::vector<std::wstring> tags;
    std::vector<std::wstring> baseClasses;
    std::vector<std::wstring> relatedControls;
    std::vector<DocLink> docs;
    bool isNew = false;
    bool isUpdated = false;
    bool isExperimental = false;
};

struct ControlGroup {
    std::wstring uniqueId;
    std::wstring title;
    std::wstring iconGlyph;
    bool isSpecialSection = false;
    std::vector<ControlInfo> items;
};

struct Catalog {
    std::vector<ControlGroup> groups;

    // Контрол по идентификатору или nullptr.
    ControlInfo const* find(std::wstring_view id) const;

    // Группа, в которой лежит контрол, или nullptr.
    ControlGroup const* groupOfItem(std::wstring_view id) const;

    // Группа по идентификатору или nullptr.
    ControlGroup const* group(std::wstring_view id) const;
};

// Каталог из Assets/Data/ControlInfoData.json рядом с exe; при поломке файла
// пустой.
Catalog const& catalog();

}  // namespace gallery
