#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"

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
// (impl::args_as) instead of reading a lazily cached field. And a view's
// const-ness means what it says, unlike a wrapper's: getters are const,
// everything else -- a setter such as Handled, a method -- is not (see
// declaration()).
//
// An args header names a wrapper type only as a forward declaration. A class
// header already includes the args headers of the events it declares, so
// including it back would close a cycle; a declaration is all a signature
// needs, and the .cpp beside it includes the real thing.
//
// gen/classes.cpp leaves *EventArgs classes to this writer: wrapped there as
// well, they would be emitted twice under the same flat wxl name.

using namespace md;

namespace gen {

bool is_event_args_class(TypeDef const& type) {
    return get_category(type) == category::class_type && type.TypeName().ends_with("EventArgs");
}

namespace {

// An args class as analysis sees it: its TypeDef, its base among the args
// classes (empty for a root, which derives from EventArgsBase), and the
// event_args_info a writer gets.
struct args_source {
    TypeDef type;
    TypeDef base;
    event_args_info info;
};

// `candidates` is keyed by TypeDef so a resolved base can be checked
// for membership in the same EventArgs set this generator is emitting
// wrappers for -- a base outside that set (typically just IInspectable
// by way of no Extends() at all) makes this class a root.
args_source read_event_args(TypeDef const& type, std::set<TypeDef> const& candidates) {
    args_source source{type, {}, {std::string{type.TypeName()}, "EventArgsBase", {}}};
    if (auto const base = type.Extends()) {
        if (auto const resolved = md::find(base);
            resolved && candidates.count(resolved)) {
            source.base = resolved;
            source.info.base = std::string{resolved.TypeName()};
        }
    }
    return source;
}

// What an args view's own signatures need declared, and how. A wrapper type
// is forward-declared rather than included; everything else -- enums,
// structs, the standard headers a projection names -- comes in as the include
// the type mapping named.
void note_type(type_use const& use, std::set<std::string> const& class_headers,
               event_args_file& file) {
    bool declared_elsewhere = false;
    for (auto&& include : use.public_includes) {
        if (class_headers.count(include)) {
            declared_elsewhere = true;
        } else {
            file.includes.insert(include);
        }
    }

    // Only a type whose header was left out needs declaring: Object and the
    // hand-written wrappers come in as themselves.
    if (declared_elsewhere) {
        file.forwards.insert(use.is_collection ? use.element_type : use.value_type);
    }
    if (use.is_collection) {
        file.collections.insert(use.element_type);
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

void write_event_args_file(std::filesystem::path const& path, event_args_file const& file) {
    auto out = open_output(path);

    std::print(out, "{}#pragma once\n\n#include \"../events.h\"\n", banner);
    write_includes(out, file.includes);
    std::print(out, "\nnamespace wxl {{\n");

    if (!file.forwards.empty()) {
        std::print(out, "\n");
        for (auto&& name : file.forwards) {
            std::print(out, "class {};\n", name);
        }
    }
    for (auto&& element : file.collections) {
        std::print(out, "\nextern template class Collection<{}>;\n", element);
    }
    std::print(out, "\n");

    for (auto&& c : file.classes) {
        std::print(out, "class {} : public {}\n{{\n", c.name, c.base);
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
                   c.base, c.base);
    }

    std::print(out, "}} // namespace wxl\n");
}

void write_event_args_source(std::filesystem::path const& path, std::string_view header,
                             event_args_file const& file) {
    auto out = open_output(path);

    // The private headers come first and the args header last, which is the
    // opposite of the usual order and has to be: every wxl public header
    // imports wxl.core, and MSVC refuses a standard header first seen after
    // that import. A private header includes its winrt ones ahead of any wxl
    // header for the same reason, so leading with them is what gets the
    // standard headers winrt/base.h needs in ahead of the import.
    std::print(out, "{}", banner);
    write_includes(out, file.source_includes);
    std::print(out, "\n#include \"{}\"\n\nnamespace wxl {{\n", header);

    for (auto&& c : file.classes) {
        for (auto&& member : c.members) {
            auto const& info = member.info;

            std::string arguments;
            for (auto&& param : info.params) {
                if (!arguments.empty()) {
                    arguments += ", ";
                }
                arguments += info.kind == member_info::kind_t::BoxedString
                                 ? std::format("impl::box_text({})", param.name)
                                 : substitute(param.type.to_winrt, param.name);
            }

            // The one difference from a class wrapper's body: the interface
            // is asked for, not read out of a cached field.
            auto const call = std::format("impl::args_as<{}>(abi_).{}({})", member.winrt_interface,
                                          info.winrt_name, arguments);

            std::print(out, "\n{} {}::{}({}){} {{\n    {}{};\n}}\n", result_type(info), c.name,
                       info.name, parameter_list(info), info.is_property_getter ? " const" : "",
                       info.returns_void ? "" : "return ",
                       info.returns_void ? call : substitute(info.result.from_winrt, call));
        }
    }

    if (!file.collection_definitions.empty()) {
        std::print(out, "\n");
        for (auto&& element : file.collection_definitions) {
            std::print(out, "template class Collection<{}>;\n", element);
        }
    }

    std::print(out, "\n}}  // namespace wxl\n");
}

// The headers declaring a wrapped class, which an args header must not
// include: a class header already includes the args headers of the events it
// declares, so including it back would close a cycle.
std::set<std::string> class_headers_of(std::vector<TypeDef> const& classes, type_index const& index) {
    std::set<std::string> headers;
    for (auto&& type : classes) {
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
std::vector<args_member> collect_args_members(TypeDef const& type, closure const& cl,
                                              type_index const& index,
                                              std::vector<std::string>& skipped_report) {
    std::vector<args_member> result;

    auto const interfaces = cl.held_interfaces.find(type);
    if (interfaces == cl.held_interfaces.end()) {
        return result;
    }

    for (auto&& iface : interfaces->second) {
        auto const allowed = cl.members.find(iface);
        if (allowed == cl.members.end()) {
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
            if (member.kind == member_info::kind_t::EventAdd
                || member.kind == member_info::kind_t::EventRemove) {
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

void analyze_event_args(type_kinds const& kinds, closure const& cl, type_index const& index,
                        model& m) {
    std::vector<TypeDef> all_event_args;
    for (auto&& type : kinds.classes) {
        if (is_event_args_class(type)) {
            all_event_args.push_back(type);
        }
    }
    std::set<TypeDef> const event_args_set{all_event_args.begin(), all_event_args.end()};
    auto const class_headers = class_headers_of(kinds.classes, index);

    std::vector<args_source> sources;
    sources.reserve(all_event_args.size());
    for (auto&& type : all_event_args) {
        auto source = read_event_args(type, event_args_set);
        source.info.members = collect_args_members(type, cl, index, m.skipped_args_members);
        sources.push_back(std::move(source));
    }
    // A base args class has to be complete before the class deriving from it.
    auto const sorted = topological_sort(std::move(sources), [](args_source const& c, auto&& follow) {
        if (c.base) {
            follow(c.base);
        }
    });

    // Which Collection specializations an args file is the one to define: the
    // class files already define every element they name, so only an element
    // nothing but an args member reaches is left over.
    std::set<std::string> already_defined;
    for (auto&& group : m.class_groups) {
        already_defined.insert(group.collections_defined.begin(), group.collections_defined.end());
    }

    std::map<std::string, event_args_file> files;
    for (auto&& c : sorted) {
        std::string const ns{c.type.TypeNamespace()};
        auto& file = files[ns];
        file.ns = ns;
        if (file.source_includes.empty()) {
            file.source_includes = {"../Object.impl.h", "../impl/conversions.h",
                                    "../impl/event_args.h"};
        }

        if (c.base) {
            if (std::string const base_ns{c.base.TypeNamespace()}; base_ns != ns) {
                file.includes.insert(base_ns + ".EventArgs.h");
            }
        }
        for (auto&& member : c.info.members) {
            file.source_includes.insert(member.winrt_header);

            if (!member.info.returns_void) {
                note_type(member.info.result, class_headers, file);
                file.source_includes.insert(member.info.result.impl_includes.begin(),
                                            member.info.result.impl_includes.end());
            }
            for (auto&& param : member.info.params) {
                note_type(param.type, class_headers, file);
                file.source_includes.insert(param.type.impl_includes.begin(),
                                            param.type.impl_includes.end());
            }
        }
        file.classes.push_back(c.info);
    }

    for (auto&& [ns, file] : files) {
        for (auto&& element : file.collections) {
            if (already_defined.insert(element).second) {
                file.collection_definitions.insert(element);
            }
        }
        m.event_args_files.push_back(std::move(file));
    }
}

void write_event_args(output const& out, model const& m, emitted& em) {
    std::vector<std::string> event_args_files;
    size_t members = 0;
    for (auto&& file : m.event_args_files) {
        std::string const filename = file.ns + ".EventArgs.h";
        auto const path = out.dir / filename;
        write_event_args_file(path, file);
        em.add(path);
        event_args_files.push_back(filename);

        size_t const emitted_members = member_count(file.classes);
        members += emitted_members;

        if (emitted_members != 0) {
            auto const source = out.dir / (file.ns + ".EventArgs.cpp");
            write_event_args_source(source, filename, file);
            em.add(source);
        }

        std::print("generated {} ({} EventArgs wrappers, {} members)\n", path.string(),
                   file.classes.size(), emitted_members);
    }

    if (!m.skipped_args_members.empty()) {
        std::print("EventArgs members skipped:\n");
        for (auto&& line : m.skipped_args_members) {
            std::print("{}\n", line);
        }
    }

    auto const umbrella = out.dir / "EventArgs.h";
    write_umbrella_file(umbrella, event_args_files);
    em.add(umbrella);
    std::print("generated {} ({} members in all)\n", umbrella.string(), members);
}

}  // namespace gen
