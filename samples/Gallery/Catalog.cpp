#include "Catalog.h"

#include "ApplicationFolder.h"

#include <string>

import std;
import wxl.core;
import wxl.json;

using wxl::json::value;

namespace gallery {

namespace {

std::wstring text(value const& field) {
    std::wstring wide = field.as_string().to_utf16();
    return wide;
}

std::vector<std::wstring> texts(value const& array) {
    std::vector<std::wstring> result;
    for (auto const& element : array.elements()) {
        result.push_back(text(element));
    }
    return result;
}

ControlInfo readItem(value const& node) {
    ControlInfo item;
    item.uniqueId = text(node["UniqueId"]);
    item.title = text(node["Title"]);
    item.subtitle = text(node["Subtitle"]);
    item.description = text(node["Description"]);
    item.imagePath = text(node["ImagePath"]);
    item.apiNamespace = text(node["ApiNamespace"]);
    item.sourcePath = text(node["SourcePath"]);
    item.tags = texts(node["Tags"]);
    item.baseClasses = texts(node["BaseClasses"]);
    item.relatedControls = texts(node["RelatedControls"]);
    for (auto const& doc : node["Docs"].elements()) {
        item.docs.push_back({text(doc["Title"]), text(doc["Uri"])});
    }
    item.isNew = node["IsNew"].as_bool();
    item.isUpdated = node["IsUpdated"].as_bool();
    item.isExperimental = node["IsExperimental"].as_bool();
    return item;
}

Catalog load() {
    Catalog result;
    try {
        wxl::json::document document;
        auto const& root = document.load_file(wxl::applicationFolder() / L"Assets" / L"Data" /
                                              L"ControlInfoData.json");
        for (auto const& node : root["Groups"].elements()) {
            ControlGroup group;
            group.uniqueId = text(node["UniqueId"]);
            group.title = text(node["Title"]);
            group.iconGlyph = text(node["IconGlyph"]);
            group.isSpecialSection = node["IsSpecialSection"].as_bool();
            for (auto const& item : node["Items"].elements()) {
                group.items.push_back(readItem(item));
            }
            result.groups.push_back(std::move(group));
        }
    } catch (std::exception const&) {
        result.groups.clear();
    }
    return result;
}

}  // namespace

ControlInfo const* Catalog::find(std::wstring_view id) const {
    for (auto const& group : groups) {
        for (auto const& item : group.items) {
            if (item.uniqueId == id) {
                return &item;
            }
        }
    }
    return nullptr;
}

ControlGroup const* Catalog::groupOfItem(std::wstring_view id) const {
    for (auto const& group : groups) {
        for (auto const& item : group.items) {
            if (item.uniqueId == id) {
                return &group;
            }
        }
    }
    return nullptr;
}

ControlGroup const* Catalog::group(std::wstring_view id) const {
    for (auto const& group : groups) {
        if (group.uniqueId == id) {
            return &group;
        }
    }
    return nullptr;
}

Catalog const& catalog() {
    static Catalog const loaded = load();
    return loaded;
}

}  // namespace gallery
