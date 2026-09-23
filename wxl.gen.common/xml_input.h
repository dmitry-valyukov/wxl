#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// The generator's XML side. The reader is wxl::xml, imported by xml_input.cpp
// alone; the profiles and the writers see only the plain declarations below.
//
// Nothing here interprets what it reads: a caller gets the document's own
// strings and decides what they mean.

// The <dependency> elements of a nuspec, as id/version pairs in document
// order -- flat or grouped by target framework, which the caller need not
// care about. Versions come back exactly as written, NuGet range brackets
// included.
std::vector<std::pair<std::string, std::string>> nuspec_dependencies(
    std::filesystem::path const& nuspec);

// One keyed resource of a XAML dictionary, as declared. The reader hands out
// everything it finds and interprets none of it: what a writer makes of a
// Thickness, a Storyboard or a converter is that writer's business, and the
// walk over a 3 MB document happens once instead of once per interest.
struct DictionaryResource {
    std::string key;
    std::string type;         // the element's local name: SolidColorBrush, Style, Double...
    std::string target_type;  // styles: the TargetType, still carrying its prefix, if any
    std::string alias_for;    // StaticResource aliases: the ResourceKey they forward to
    std::string theme;        // the enclosing theme dictionary's key (Default, Light,
                              // HighContrast); empty outside ThemeDictionaries
    bool scoped = false;      // declared inside some element's own Resources (a template's
                              // local override) -- unreachable from Application.Resources
};

// Every resource in the dictionary that carries a key, in document order.
std::vector<DictionaryResource> dictionary_resources(std::filesystem::path const& dictionary);
