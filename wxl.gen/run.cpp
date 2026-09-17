module;

#include <format>
#include <print>

module wxl.gen;

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

void report(ProfileSet const& profiles, Closure const& closure) {
    std::print("profiles: {}\n", wxl::core::join(profiles.loaded, ", "));
    for (auto&& file : profiles.metadata) {
        std::print("  metadata: {}\n", file.string());
    }
    std::print("  roots: {}\n", profiles.types.size());

    for (auto&& name : closure.missing_types) {
        std::print(stderr, "warning: profile type not found in metadata: {}\n", name);
    }
    for (auto&& name : closure.unknown_members) {
        std::print(stderr, "warning: profile names a member the type doesn't declare: {}\n", name);
    }

    if (!closure.deprecated.empty()) {
        std::print("\nmembers dropped -- WinRT marks them deprecated ({}):\n",
                   closure.deprecated.size());
        for (auto&& name : closure.deprecated) {
            std::print("  {}\n", name);
        }
    }

    // In dependency order, the way the closure came out: sources of
    // dependencies first, the types built on top of them after. For a
    // class, the interfaces listed under it are the ones that survived
    // the filter -- those are what the wrapper actually has to hold (one
    // `Impl` field each); interfaces whose every member was filtered out
    // are reported as dropped, so the trimming stays visible.
    std::print("\ntype closure, in dependency order ({} types):\n", closure.ordered.size());
    for (auto&& type : closure.ordered) {
        std::print("  {:9} {}.{}\n", category_name(get_category(type)), type.TypeNamespace(),
                   type.TypeName());

        if (auto const it = closure.interfaces.find(type); it != closure.interfaces.end()) {
            for (auto&& iface : it->second) {
                std::print("      implements {}.{}\n", iface.TypeNamespace(), iface.TypeName());
            }
        }
        if (auto const it = closure.dropped_interfaces.find(type);
            it != closure.dropped_interfaces.end()) {
            for (auto&& iface : it->second) {
                std::print("      dropped    {}.{}\n", iface.TypeNamespace(), iface.TypeName());
            }
        }
        if (auto const it = closure.members.find(type); it != closure.members.end()) {
            std::print("      members    {}\n", wxl::core::join(it->second, ", "));
        }
    }

    if (!closure.boundary.empty()) {
        std::print("\nstopped at given-from-above types ({}):\n", closure.boundary.size());
        for (auto&& type : closure.boundary) {
            std::print("  {}.{}\n", type.TypeNamespace(), type.TypeName());
        }
    }

    std::print("\nproperty keys: {}\n", closure.property_names.size());
    std::print("event keys: {}\n", closure.event_names.size());
}

// The Model's vectors keep the closure's dependency order (see
// generate.h) -- category grouping only splits that one ordering into
// per-category slices, it never re-sorts.
Model build_model(Closure const& closure) {
    Model model;
    for (auto&& type : closure.ordered) {
        switch (get_category(type)) {
            case category::enum_type:
                model.enums.push_back(type);
                break;
            case category::struct_type:
                model.structs.push_back(type);
                break;
            case category::class_type:
                model.classes.push_back(type);
                break;
            case category::interface_type:
                model.interfaces.push_back(type);
                if (closure.listed_interfaces.contains(type)) {
                    model.listed_interfaces.push_back(type);
                }
                break;
            case category::delegate_type:
                break;  // no output of their own yet
        }
    }
    model.interfaces_of = closure.interfaces;
    model.statics_of = closure.statics;
    model.factories_of = closure.factories;
    model.synthetic_of = closure.synthetic;
    model.setter_methods_of = closure.setter_methods;
    model.attached_of = closure.attached;
    model.members = closure.members;
    model.property_names = closure.property_names;
    model.event_names = closure.event_names;
    return model;
}

}  // namespace

void run(ProfileSet const& profiles, Output const& out) {
    std::vector<std::string> files;
    files.reserve(profiles.metadata.size());
    for (auto&& file : profiles.metadata) {
        files.push_back(file.string());
    }

    cache const db{files};
    auto const closure = crawl(profiles, db);
    report(profiles, closure);

    write_all(out, build_model(closure), profiles.resources);
}
