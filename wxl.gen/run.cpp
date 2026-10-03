#include <print>

#include "wxl.gen.h"

import std;
import wxl.core;

using namespace md;

namespace {

char const* category_name(category cat) {
    switch (cat) {
        case category::class_type:
            return "class";
        case category::interface_type:
            return "interface";
        case category::struct_type:
            return "struct";
        case category::enum_type:
            return "enum";
        case category::delegate_type:
            return "delegate";
    }
    return "?";
}

void report(profile_set const& profiles, closure const& cl) {
    std::print("profiles: {}\n", wxl::core::join(profiles.loaded, ", "));
    for (auto&& file : profiles.metadata) {
        std::print("  metadata: {}\n", file.string());
    }
    std::print("  roots: {}\n", profiles.types.size());

    if (!profiles.missing_windows_metadata.empty()) {
        std::print(stderr,
                   "warning: no Windows metadata in {} -- the walk stops at every Windows type\n",
                   profiles.missing_windows_metadata.string());
    }

    for (auto&& name : cl.missing_types) {
        std::print(stderr, "warning: profile type not found in metadata: {}\n", name);
    }
    for (auto&& name : cl.unknown_members) {
        std::print(stderr, "warning: profile names a member the type doesn't declare: {}\n", name);
    }

    if (!cl.deprecated.empty()) {
        std::print("\nmembers dropped -- WinRT marks them deprecated ({}):\n",
                   cl.deprecated.size());
        for (auto&& name : cl.deprecated) {
            std::print("  {}\n", name);
        }
    }

    // In dependency order, the way the closure came out: sources of
    // dependencies first, the types built on top of them after. For a
    // class, the interfaces listed under it are the ones that survived
    // the filter -- those are what the wrapper actually has to hold (one
    // `Impl` field each); interfaces whose every member was filtered out
    // are reported as dropped, so the trimming stays visible.
    std::print("\ntype closure, in dependency order ({} types):\n", cl.ordered.size());
    for (auto&& type : cl.ordered) {
        std::print("  {:9} {}.{}\n", category_name(get_category(type)), type.TypeNamespace(),
                   type.TypeName());

        if (auto const it = cl.held_interfaces.find(type); it != cl.held_interfaces.end()) {
            for (auto&& iface : it->second) {
                std::print("      implements {}.{}\n", iface.TypeNamespace(), iface.TypeName());
            }
        }
        if (auto const it = cl.dropped_interfaces.find(type);
            it != cl.dropped_interfaces.end()) {
            for (auto&& iface : it->second) {
                std::print("      dropped    {}.{}\n", iface.TypeNamespace(), iface.TypeName());
            }
        }
        if (auto const it = cl.members.find(type); it != cl.members.end()) {
            std::print("      members    {}\n", wxl::core::join(it->second, ", "));
        }
    }

    if (!cl.boundary.empty()) {
        std::print("\nstopped at given-from-above types ({}):\n", cl.boundary.size());
        for (auto&& type : cl.boundary) {
            std::print("  {}.{}\n", type.TypeNamespace(), type.TypeName());
        }
    }

    std::print("\nproperty keys: {}\n", cl.property_names.size());
    std::print("event keys: {}\n", cl.event_names.size());
}

}  // namespace

void run(profile_set const& profiles, type_map const& types, std::vector<symbol> symbols,
         output const& out) {
    std::vector<std::string> files;
    files.reserve(profiles.metadata.size());
    for (auto&& file : profiles.metadata) {
        files.push_back(file.string());
    }

    cache const db{files};
    auto const cl = crawl(profiles, types, db);
    report(profiles, cl);

    write_all(out, analyze(cl, profiles, types, std::move(symbols)));
}
