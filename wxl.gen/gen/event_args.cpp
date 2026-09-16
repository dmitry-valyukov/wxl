module;

#include <format>
#include <ostream>
#include <print>

module wxl.gen;

import std;

// EventArgs wrappers. WinRT has no formal "EventArgs" category -- these
// are ordinary classes that merely follow a naming convention.
//
// They are deliberately *not* shaped like the class wrappers: an args
// object belongs to the runtime and lives only for the duration of the
// callback, so its wrapper is a non-owning view of the ABI pointer
// (wxl::EventArgsBase) rather than a reference-counted Impl chain. That
// keeps an event that fires on every mouse move free of an allocation and
// a reference count it would gain nothing from, and non-copyability is what
// makes "valid only inside the handler" a compiler rule.
//
// Members are forwarded the way a class wrapper's are -- same collection,
// same type mapping (gen/members.h) -- with one difference that follows from
// there being no Impl: a body reaches its interface by querying it off abi_
// (impl::args_as) instead of reading a lazily cached field. They are also
// not const-qualified, because a view is handed to a handler mutably: an
// args object is there to be written to as well as read.
//
// An args header names a wrapper type only as a forward declaration. A class
// header already includes the args headers of the events it declares, so
// including it back would close a cycle; a declaration is all a signature
// needs, and the .cpp beside it includes the real thing.
//
// AI note: this is why gen/classes.cpp skips *EventArgs classes -- they
// are wrapped here, in their own shape, and would otherwise be emitted
// twice under the same flat wxl name.

using namespace md;

namespace gen {

bool is_event_args_class(TypeDef const& type) {
    return get_category(type) == category::class_type && type.TypeName().ends_with("EventArgs");
}

namespace {

std::vector<TypeDef> collect_event_args(std::vector<TypeDef> const& classes) {
    std::vector<TypeDef> result;
    for (auto&& type : classes) {
        if (is_event_args_class(type)) {
            result.push_back(type);
        }
    }
    return result;
}

// One member of an args view, together with the interface its body has to
// query off abi_ -- which a class wrapper reads from a field instead, so
// member_info has nowhere to carry it.
struct args_member {
    member_info info;
    std::string winrt_interface;  // "winrt::Microsoft::UI::Xaml::IRoutedEventArgs"
    std::string winrt_header;     // the projection header declaring it
};

struct event_args_info {
    TypeDef type;
    TypeDef base;  // empty if this is a root -- derives from EventArgsBase
    std::vector<args_member> members;
};

// `candidates` is keyed by TypeDef so a resolved base can be checked
// for membership in the same EventArgs set this generator is emitting
// wrappers for -- a base outside that set (typically just IInspectable
// by way of no Extends() at all) makes this class a root.
event_args_info analyze_event_args(TypeDef const& type, std::set<TypeDef> const& candidates) {
    event_args_info info{type, {}};
    if (auto const base = type.Extends()) {
        if (auto const resolved = md::find(base);
            resolved && candidates.count(resolved)) {
            info.base = resolved;
        }
    }
    return info;
}

// Same shape as the struct sort in gen/enums_structs.cpp, but ordering a
// single-inheritance chain instead of multiple field dependencies -- a
// base class must be a complete type before it's used, same requirement,
// different edge kind.
std::vector<event_args_info> topological_sort(std::vector<event_args_info> classes) {
    std::map<TypeDef, event_args_info const*> by_type;
    for (auto&& c : classes) {
        by_type[c.type] = &c;
    }

    std::set<TypeDef> done;
    std::set<TypeDef> visiting;
    std::vector<event_args_info> order;
    order.reserve(classes.size());

    std::function<void(event_args_info const&)> visit = [&](event_args_info const& c) {
        if (done.count(c.type) || !visiting.insert(c.type).second) {
            return;
        }
        if (c.base) {
            if (auto const it = by_type.find(c.base); it != by_type.end()) {
                visit(*it->second);
            }
        }
        done.insert(c.type);
        order.push_back(c);
    };

    for (auto&& c : classes) {
        visit(c);
    }
    return order;
}


// What an args view's own signatures need declared, and how. A wrapper type
// is forward-declared rather than included; everything else -- enums,
// structs, collections.h, <optional> -- comes in as the include the type
// mapping named.
struct header_needs {
    std::set<std::string> includes;
    std::set<std::string> forwards;     // wrapper types a signature names
    std::set<std::string> collections;  // Collection<E> specializations used
};

void note_type(TypeUse const& use, std::set<std::string> const& class_headers,
               header_needs& needs) {
    bool declared_elsewhere = false;
    for (auto&& include : use.public_includes) {
        if (class_headers.count(include)) {
            declared_elsewhere = true;
        } else {
            needs.includes.insert(include);
        }
    }

    // Only a type whose header was left out needs declaring: Object and the
    // hand-written wrappers come in as themselves.
    if (declared_elsewhere) {
        needs.forwards.insert(use.is_collection ? use.element_type : use.value_type);
    }
    if (use.is_collection) {
        needs.collections.insert(use.element_type);
    }
}

// Where an args view keeps its const-ness, and it is the one hierarchy in
// wxl that does.
//
// A wrapper of the Object family is a smart pointer: its own const-ness says
// nothing about the object behind it, so every member of one is const and
// the qualifier carries no information. An args view is not a pointer -- it
// *is* the object, sitting on the stack for the duration of the call -- so
// const means what it says. Reading a property does not change the args;
// setting one (Handled) does, and so may a method. Hence: getters const,
// everything else not.
//
// Which is also why the view arrives by non-const reference: a handler that
// only reads could take it const, and one that answers -- Handled, a
// cancellable request refused -- could not.
std::string declaration(member_info const& member) {
    return std::format("{} {}({}){};", result_type(member), member.name, parameter_list(member),
                       member.is_property_getter ? " const" : "");
}

size_t member_count(std::vector<event_args_info> const& classes) {
    size_t total = 0;
    for (auto&& c : classes) {
        total += c.members.size();
    }
    return total;
}

void write_event_args_file(std::filesystem::path const& path,
                           std::vector<event_args_info> const& classes,
                           header_needs const& needs) {
    auto out = open_output(path);

    std::print(out, "{}#pragma once\n\n#include \"../events.h\"\n", banner);
    for (auto&& include : needs.includes) {
        if (include.starts_with('<')) {
            std::print(out, "#include {}\n", include);
        } else {
            std::print(out, "#include \"{}\"\n", include);
        }
    }
    std::print(out, "\nnamespace wxl {{\n");

    if (!needs.forwards.empty()) {
        std::print(out, "\n");
        for (auto&& name : needs.forwards) {
            std::print(out, "class {};\n", name);
        }
    }
    for (auto&& element : needs.collections) {
        std::print(out, "\nextern template class Collection<{}>;\n", element);
    }
    std::print(out, "\n");

    for (auto&& c : classes) {
        auto const base = c.base ? std::string{c.base.TypeName()} : std::string{"EventArgsBase"};

        std::print(out, "class {} : public {}\n{{\n", c.type.TypeName(), base);
        if (!c.members.empty()) {
            std::print(out, "public:\n");
            for (auto&& member : c.members) {
                std::print(out, "    {}\n", declaration(member.info));
            }
            std::print(out, "\n");
        }
        // The constructor stays inherited all the way down: every level is
        // the same non-owning view over the same ABI pointer, so no level
        // adds anything to construct.
        std::print(out, "protected:\n    using {}::{};\n\n    friend class Object::Impl;\n}};\n\n",
                   base, base);
    }

    std::print(out, "}} // namespace wxl\n");
}

void write_event_args_source(std::filesystem::path const& path, std::string_view header,
                             std::vector<event_args_info> const& classes,
                             std::set<std::string> const& includes,
                             std::set<std::string> const& collection_definitions) {
    auto out = open_output(path);

    // The private headers come first and the args header last, which is the
    // opposite of the usual order and has to be: every wxl public header
    // imports wxl.core, and MSVC refuses a standard header first seen after
    // that import. A private header includes its winrt ones ahead of any wxl
    // header for the same reason, so leading with them is what gets the
    // standard headers winrt/base.h needs in ahead of the import.
    std::print(out, "{}", banner);
    for (auto&& include : includes) {
        if (include.starts_with('<')) {
            std::print(out, "#include {}\n", include);
        } else {
            std::print(out, "#include \"{}\"\n", include);
        }
    }
    std::print(out, "\n#include \"{}\"\n\nnamespace wxl {{\n", header);

    for (auto&& c : classes) {
        for (auto&& member : c.members) {
            auto const& info = member.info;

            std::string arguments;
            for (auto&& param : info.params) {
                if (!arguments.empty()) {
                    arguments += ", ";
                }
                arguments += info.kind == member_info::Kind::BoxedString
                                 ? std::format("impl::box_text({})", param.name)
                                 : substitute(param.type.to_winrt, param.name);
            }

            // The one difference from a class wrapper's body: the interface
            // is asked for, not read out of a cached field.
            auto const call = std::format("impl::args_as<{}>(abi_).{}({})", member.winrt_interface,
                                          info.winrt_name, arguments);

            std::print(out, "\n{} {}::{}({}){} {{\n    {}{};\n}}\n", result_type(info),
                       c.type.TypeName(), info.name, parameter_list(info),
                       info.is_property_getter ? " const" : "",
                       info.returns_void ? "" : "return ",
                       info.returns_void ? call : substitute(info.result.from_winrt, call));
        }
    }

    if (!collection_definitions.empty()) {
        std::print(out, "\n// Collection specializations no class file claimed.\n");
        for (auto&& element : collection_definitions) {
            std::print(out, "template class Collection<{}>;\n", element);
        }
    }

    std::print(out, "\n}}  // namespace wxl\n");
}

void write_umbrella_file(std::filesystem::path const& path, std::vector<std::string> const& files) {
    auto out = open_output(path);

    std::print(out, "{}#pragma once\n\n", banner);
    for (auto&& file : files) {
        std::print(out, "#include \"{}\"\n", file);
    }
}

// The headers declaring a wrapped class, which an args header must not
// include: a class header already includes the args headers of the events it
// declares, so including it back would close a cycle.
std::set<std::string> class_headers_of(Model const& model, TypeIndex const& index) {
    std::set<std::string> headers;
    for (auto&& type : model.classes) {
        if (is_event_args_class(type)) {
            continue;
        }
        if (auto const found = index.headers.find(type); found != index.headers.end()) {
            headers.insert(found->second);
        }
    }
    return headers;
}

// The members an args class contributes, read off the interfaces it
// implements directly -- the same walk a class wrapper's members come from,
// with the interface remembered rather than turned into an Impl field.
std::vector<args_member> collect_args_members(TypeDef const& type, Model const& model,
                                              TypeIndex const& index,
                                              std::vector<std::string>& skipped_report) {
    std::vector<args_member> result;

    auto const interfaces = model.interfaces_of.find(type);
    if (interfaces == model.interfaces_of.end()) {
        return result;
    }

    for (auto&& iface : interfaces->second) {
        auto const allowed = model.members.find(iface);
        if (allowed == model.members.end()) {
            continue;
        }

        std::vector<member_info> members;
        std::vector<skipped_member> skipped;
        collect_interface_members(iface, allowed->second, index, members, skipped);

        auto const winrt_interface =
            std::format("{}::{}", winrt_namespace(iface.TypeNamespace()), iface.TypeName());
        auto const winrt_header = winrt_include(iface.TypeNamespace());

        for (auto&& member : members) {
            // An args object declaring an event of its own has no answer
            // here: a subscription outlives the callback the view is valid
            // for, and the view has no object to hand the token back to.
            if (member.kind == member_info::Kind::EventAdd
                || member.kind == member_info::Kind::EventRemove) {
                skipped.push_back({member.name, "an args view cannot carry an event"});
                continue;
            }
            result.push_back({std::move(member), winrt_interface, winrt_header});
        }

        for (auto&& drop : skipped) {
            skipped_report.push_back(
                std::format("  {}.{}: {}", type.TypeName(), drop.name, drop.reason));
        }
    }

    return result;
}

}  // namespace

void write_event_args(Output const& out, Model const& model, ClassOutput const& classes,
                      Emitted& emitted) {
    auto const all_event_args = collect_event_args(model.classes);
    std::set<TypeDef> const event_args_set{all_event_args.begin(), all_event_args.end()};
    auto const class_headers = class_headers_of(model, classes.index);

    std::vector<std::string> skipped_report;
    std::vector<event_args_info> event_args_infos;
    event_args_infos.reserve(all_event_args.size());
    for (auto&& type : all_event_args) {
        auto info = analyze_event_args(type, event_args_set);
        info.members = collect_args_members(type, model, classes.index, skipped_report);
        event_args_infos.push_back(std::move(info));
    }
    auto const sorted_event_args = topological_sort(std::move(event_args_infos));

    std::map<std::string, std::vector<event_args_info>> event_args_by_namespace;
    for (auto&& c : sorted_event_args) {
        event_args_by_namespace[std::string(c.type.TypeNamespace())].push_back(c);
    }

    // Which Collection specializations an args file is the one to define: the
    // class writer already defined every element it named itself, so only an
    // element nothing but an args member reaches is left over.
    std::set<std::string> already_defined = classes.collection_elements;

    std::vector<std::string> event_args_files;
    size_t members = 0;
    for (auto&& [ns, infos] : event_args_by_namespace) {
        header_needs needs;
        std::set<std::string> source_includes{"../Object.impl.h", "../impl/conversions.h",
                                              "../impl/event_args.h"};
        std::set<std::string> collection_definitions;

        for (auto&& c : infos) {
            if (c.base) {
                if (std::string const base_ns{c.base.TypeNamespace()}; base_ns != ns) {
                    needs.includes.insert(base_ns + ".EventArgs.h");
                }
            }
            for (auto&& member : c.members) {
                source_includes.insert(member.winrt_header);

                if (!member.info.returns_void) {
                    note_type(member.info.result, class_headers, needs);
                    source_includes.insert(member.info.result.impl_includes.begin(),
                                           member.info.result.impl_includes.end());
                }
                for (auto&& param : member.info.params) {
                    note_type(param.type, class_headers, needs);
                    source_includes.insert(param.type.impl_includes.begin(),
                                           param.type.impl_includes.end());
                }
            }
        }

        for (auto&& element : needs.collections) {
            if (already_defined.insert(element).second) {
                collection_definitions.insert(element);
            }
        }

        std::string const filename = ns + ".EventArgs.h";
        auto const path = out.dir / filename;
        write_event_args_file(path, infos, needs);
        emitted.add(path);
        event_args_files.push_back(filename);

        size_t const emitted_members = member_count(infos);
        members += emitted_members;

        if (emitted_members != 0) {
            auto const source = out.dir / (ns + ".EventArgs.cpp");
            write_event_args_source(source, filename, infos, source_includes,
                                    collection_definitions);
            emitted.add(source);
        }

        std::print("wrote {} ({} EventArgs wrappers, {} members)\n", path.string(), infos.size(),
                   emitted_members);
    }

    if (!skipped_report.empty()) {
        std::print("EventArgs members skipped:\n");
        for (auto&& line : skipped_report) {
            std::print("{}\n", line);
        }
    }

    auto const umbrella = out.dir / "EventArgs.h";
    write_umbrella_file(umbrella, event_args_files);
    emitted.add(umbrella);
    std::print("wrote {} ({} members in all)\n", umbrella.string(), members);
}

}  // namespace gen
