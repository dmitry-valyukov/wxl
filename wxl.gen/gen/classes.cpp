#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;

// Class wrappers, grouped per source WinRT namespace into three files:
//
//   <Namespace>.h        public wrappers -- name no winrt:: type at all,
//                        each declaring its `Impl` without defining it
//   <Namespace>.impl.h   private -- the matching `Impl` chain, one field
//                        per surviving interface
//   <Namespace>.cpp      out-of-line bodies (constructors today, member
//                        forwarding once members are generated)
//
// Grouping per namespace rather than per class is what the closure's
// dependency ordering buys: inside one file the classes are already
// emitted base-before-derived, so a base class is a complete type by the
// time the derived one names it, with no per-class include graph at all.
// Across files only the namespaces a file's base classes live in are
// included.
//
// The `Impl` definitions have to live in a *header*: the derived level's
// `Impl` inherits the base's, so ButtonBase::Impl must be visible while
// compiling anything that defines Button::Impl.
//
// The chain bottoms out in the hand-written, given-from-above pair
// wxl.ui/src/Object.h + Object.impl.h -- wxl::Object holds the only data
// member in the whole hierarchy (an intrusive_ptr to the Impl chain), and
// Object::Impl is core::refcounted, allocated from the STA pool.
//
// Members are forwarded one interface at a time: a class's instance members
// are declared by the interfaces it implements, so each generated member
// reaches its interface through get<&Impl::field>() -- one real
// QueryInterface on first use, a cached pointer read afterwards. Every type
// a member names crosses the public boundary through gen/types.h, and a
// member naming a type that has no wxl equivalent yet is skipped and
// counted rather than emitted broken.
//
// AI note: events and static members (Grid.SetRow and friends) are still
// missing. Statics are where wxl::impl::Statics<I> gets wired in; like the
// runtime class name below, its specialization belongs in the .cpp, never
// in a header -- it is used by exactly one translation unit.

using namespace md;

namespace gen {
namespace {

CustomAttribute find_attribute(TypeDef const& type, std::string_view name) {
    for (auto&& attribute : type.CustomAttribute()) {
        auto const [ns, attribute_name] = attribute.TypeNamespaceAndName();
        if (ns == "Windows.Foundation.Metadata" && attribute_name == name) {
            return attribute;
        }
    }
    return {};
}

// The property unnamed children belong to, as the class itself declares it:
// XAML marks every markup-facing class with ContentPropertyAttribute, and
// that is exactly the question the builder syntax asks -- Panel says
// Children, ItemsControl and MenuFlyout say Items, TabView says TabItems.
// Reading it off the metadata means a profile aimed at another control
// library gets the same treatment with nothing hand-written; the namespace
// is either markup one, since WinUI3 kept the older spelling on the types
// that came from it.
std::string content_property_of(TypeDef const& type) {
    for (auto&& attribute : type.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (name != "ContentPropertyAttribute" ||
            (ns != "Microsoft.UI.Xaml.Markup" && ns != "Windows.UI.Xaml.Markup")) {
            continue;
        }
        for (auto&& argument : attribute.Value().NamedArgs()) {
            if (argument.name != "Name") {
                continue;
            }
            auto const* element = std::get_if<ElemSig>(&argument.value.value);
            if (!element) {
                continue;
            }
            if (auto const* text = std::get_if<std::string_view>(&element->value)) {
                return std::string{*text};
            }
        }
    }
    return {};
}

// How the real WinRT class is created, which is also how its constructor is
// declared -- the C# view of it (dotPeek) is `public extern Button()` against
// `protected extern Control()`, and wxl mirrors that access exactly.
//
//   ActivatableAttribute, bare      -> public, plain activation
//   ActivatableAttribute(factory)   -> public, but no default constructor:
//                                      the class's constructors are the
//                                      methods of the factory interface it
//                                      names, one each. A class can carry
//                                      both spellings, and then it has a
//                                      default constructor as well
//   ComposableAttribute(.., Public) -> public, through the factory interface
//                                      the attribute names: an instance with
//                                      no outer object, which is what a class
//                                      nobody derives from is
//   ComposableAttribute(.., Protected) -> protected, and no factory at all:
//                                      nobody can create one directly, so
//                                      there is nothing to activate
//   neither                         -> no default constructor
//
// The composable path is not an optimisation to skip: a composable class is
// free to leave IActivationFactory::ActivateInstance unimplemented, and
// RadialGradientBrush does exactly that (E_NOTIMPL). Going through the
// factory interface is what the projection itself does for every such class.
enum class Construction { None, PublicActivation, PublicFactory, PublicComposition, ProtectedOnly };

struct Creation {
    Construction kind = Construction::None;
    std::string factory;  // the composable factory interface, in metadata form
};

Creation construction_of(TypeDef const& type) {
    // ActivatableAttribute is written once per constructor shape: bare for
    // the default one, naming a factory interface for every other. Reading
    // all of them is what tells a class with a default constructor from one
    // that has only parameterised ones -- CanvasCommandList is made from a
    // resource creator and from nothing else, and activating it plainly
    // fails at run time.
    bool default_activation = false;
    bool factory_activation = false;
    for (auto&& attribute : type.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (ns != "Windows.Foundation.Metadata" || name != "ActivatableAttribute") {
            continue;
        }
        bool names_factory = false;
        for (auto&& argument : attribute.Value().FixedArgs()) {
            auto const* element = std::get_if<ElemSig>(&argument.value);
            names_factory = names_factory ||
                            (element && std::get_if<ElemSig::SystemType>(&element->value));
        }
        (names_factory ? factory_activation : default_activation) = true;
    }

    if (default_activation) {
        return {Construction::PublicActivation, {}};
    }
    if (factory_activation) {
        return {Construction::PublicFactory, {}};
    }

    auto const composable = find_attribute(type, "ComposableAttribute");
    if (!composable) {
        return {};
    }

    // ComposableAttribute(factory interface, CompositionType, version): the
    // first argument is the interface an instance comes from, the second is
    // what separates `public Button()` from `protected Control()`.
    std::string factory;
    for (auto&& arg : composable.Value().FixedArgs()) {
        auto const* elem = std::get_if<ElemSig>(&arg.value);
        if (!elem) {
            continue;
        }
        if (auto const* named = std::get_if<ElemSig::SystemType>(&elem->value)) {
            factory = named->name;
            continue;
        }
        auto const* enum_value = std::get_if<ElemSig::EnumValue>(&elem->value);
        if (!enum_value) {
            continue;
        }
        if (enum_value->equals_enumerator("Public")) {
            return {Construction::PublicComposition, std::move(factory)};
        }
        if (enum_value->equals_enumerator("Protected")) {
            return {Construction::ProtectedOnly, {}};
        }
    }
    return {};
}

// wxl's own hand-written types, given from above (see crawl.h). Object and
// DependencyObject share one header pair.
std::string given_header(std::string_view name, bool impl_side) {
    // Statics stands beside Object in the same hand-written header: both are
    // levels a generated class derives from without being generated itself.
    std::string_view const file =
        (name == "Object" || name == "DependencyObject" || name == "Statics") ? "Object" : name;
    return std::format("../{}{}.h", file, impl_side ? ".impl" : "");
}

// One unnamed-argument route: what it takes, and what it does with it. A
// property assigns, the content collection appends -- the statement is
// carried rather than the property name, so the two read the same here.
struct positional_route {
    std::string param_type;
    std::string statement;  // uses `value`, the parameter's name
};

// One constructor of a class that has parameterised ones: the factory
// interface it comes from, as the projection names it, and the method on
// that interface. The method carries its own parameters, so the declaration
// and the call are written from it exactly like an ordinary member's.
struct factory_ctor {
    std::string statics_interface;
    std::string include;  // the projection header declaring that interface
    member_info method;
};

struct class_info {
    TypeDef type;
    std::string name;
    std::string base_name;
    std::string base_namespace;  // empty when the base is given from above
    Construction construction = Construction::None;
    // The factory interface a composable class is created through, in
    // metadata form; empty for every other class.
    std::string composable_factory;
    std::vector<TypeDef> interfaces;  // survived the filter; one Impl field each
    std::vector<TypeDef> statics;     // the interfaces its static members live on
    std::vector<member_info> members;

    // The constructors that take arguments, one per method of every factory
    // interface ActivatableAttribute named. Empty for almost every class:
    // WinRT declares constructors this way only where there are some.
    std::vector<factory_ctor> constructors;

    // A class that is nothing but its static members -- WinRT's way of
    // naming a group of free functions. Metadata says so in three parts at
    // once: nothing constructs it, it implements no instance interface, and
    // its statics are not empty. It gets no base, no Impl and no
    // constructor, because an instance of it cannot exist: a wrapper for
    // one would be a smart pointer that is always null, and its Impl field
    // a projection object that is never filled.
    bool statics_only = false;

    // The interface this level *is* on the ABI, and the field holding it.
    // That field is declared as the projection class, so handing the object
    // to a WinRT call that wants the class costs nothing; every other field
    // stays the interface it stands for.
    TypeDef default_interface;
    std::string primary_field;

    // Unnamed constructor arguments this level claims. An argument is routed
    // by its type alone, which is what lets the DSL leave the property name
    // out where the type already says it: `hAlign.center`, `L"..."`,
    // `Margin{20}`, and a child element inside its parent's braces.
    std::vector<positional_route> positional;
    bool base_has_positional = false;

    // Element types of the wxl::Collection specializations this level's
    // members name. The template's bodies are not in its public header, so
    // every specialization has to be stated: `extern template` wherever it is
    // used, one `template class` definition across the whole library.
    std::set<std::string> collection_elements;
};


// A wxl class name and every level above it, itself included, up to the
// hand-written level the walk stops at.
std::vector<std::string> ancestry(std::string name,
                                  std::map<std::string, std::string> const& base_of) {
    std::vector<std::string> chain;
    for (;;) {
        chain.push_back(name);
        auto const found = base_of.find(name);
        if (found == base_of.end()) {
            return chain;
        }
        name = found->second;
    }
}

// Whether a property's type disqualifies it from an unnamed argument.
//
// An unnamed argument is matched by its type alone, so a route accepting the
// class itself, an ancestor of it, or anything descended from it would take
// over the one-argument case from the constructors that own it: a lone
// argument of such a type binds to the setter-pack constructor exactly,
// while the copy constructor needs a conversion, and `X x2(x1)` would
// quietly become a property assignment on a freshly activated X. Such a
// property keeps its named form -- `basedOn = other` -- and claims no
// positional route.
//
// No property in the current profiles reaches this: a route is claimed only
// where the property is named after its own type, and that coincidence has
// yet to meet a type on a class related to it. The check is what keeps the
// rule from depending on that continuing to be true, and the run reports
// every route it withholds.
bool positional_conflicts(std::string_view value_type, std::string_view class_name,
                          std::map<std::string, std::string> const& base_of) {
    std::string const name{value_type};

    // The two hand-written levels are above everything and are named by no
    // base_of entry, so the walks below cannot reach them.
    if (name == "Object" || name == "DependencyObject") {
        return true;
    }

    for (auto&& level : ancestry(std::string(class_name), base_of)) {
        if (level == name) {
            return true;
        }
    }
    for (auto&& level : ancestry(name, base_of)) {
        if (level == class_name) {
            return true;
        }
    }
    return false;
}

// "Microsoft.UI.Xaml.Media.IRadialGradientBrushFactory" as the projection
// spells it. Metadata writes a type in one dotted string; C++ wants the
// namespace and the name apart.
std::string winrt_type_name(std::string_view metadata_name) {
    auto const dot = metadata_name.rfind('.');
    if (dot == std::string_view::npos) {
        return std::string{metadata_name};
    }
    return std::format("{}::{}", winrt_namespace(metadata_name.substr(0, dot)),
                       metadata_name.substr(dot + 1));
}

// The types every wrapper is, and which therefore say nothing about which
// property a value belongs to. A route needs a type that identifies one.
bool universal_type(std::string_view value_type) {
    return value_type == "Object" || value_type == "DependencyObject";
}

// The public wxl name of a wrapped type. For a class it is the class's own
// name; a wrapped *interface* drops WinRT's I prefix -- the I is metadata
// convention, not meaning, and wxl names its types the way its classes are
// named (getRange hands out a TextRange, nobody spells ITextRange).
std::string wxl_class_name(TypeDef const& type) {
    std::string_view name = type.TypeName();
    if (get_category(type) == category::interface_type && name.size() > 1 && name[0] == 'I' &&
        name[1] >= 'A' && name[1] <= 'Z') {
        name.remove_prefix(1);
    }
    return std::string{name};
}

class_info analyze_class(TypeDef const& type, std::set<TypeDef> const& generated,
                         Model const& model) {
    class_info info{type, wxl_class_name(type)};

    // No Extends(), an unresolved one, or a base outside the closure all
    // mean the same thing here: this level sits directly on wxl::Object.
    TypeDef base;
    if (auto const extends = type.Extends();
        extends && extends.type() != TypeDefOrRef::TypeSpec) {
        base = md::find(extends);
    }

    if (base && is_event_args_class(base)) {
        // An EventArgs wrapper has no Impl chain of its own (see
        // gen/event_args.cpp), so a class wrapper cannot derive from one.
        // Nothing in the current profiles hits this; report it rather than
        // emit a class that silently loses its real base.
        std::print(stderr,
                   "warning: {} derives from the EventArgs wrapper {} -- rooted at Object "
                   "instead\n",
                   full_name(type), base.TypeName());
        base = {};
    }

    if (base && !is_given_from_above(base) && generated.count(base)) {
        info.base_name = std::string{base.TypeName()};
        info.base_namespace = std::string{base.TypeNamespace()};
    } else if (base && is_given_from_above(base)) {
        info.base_name = std::string{base.TypeName()};
    } else {
        info.base_name = "Object";
    }

    auto const creation = construction_of(type);
    info.construction = creation.kind;
    info.composable_factory = creation.factory;

    if (auto const it = model.interfaces_of.find(type); it != model.interfaces_of.end()) {
        info.interfaces = it->second;
    }
    if (auto const it = model.statics_of.find(type); it != model.statics_of.end()) {
        info.statics = it->second;
    }

    // A wrapped interface is its own default interface: the object *is* the
    // interface, so the primary field holds it directly, and there is
    // nothing to construct -- an instance only ever arrives out of a call.
    // Its required interfaces, if any survived, keep fields beside it like
    // a class's do.
    if (get_category(type) == category::interface_type) {
        info.interfaces.insert(info.interfaces.begin(), type);
        info.default_interface = type;
        info.primary_field = interface_field_name(type.TypeName());
        return info;
    }

    // Nothing constructs it, it implements no instance interface, and its
    // statics are not empty: metadata's description of a class that is only
    // a place to hang free functions. It gets none of what follows -- no
    // base, no default interface, no Impl -- because there is nothing for
    // any of it to point at.
    info.statics_only = info.construction == Construction::None && info.interfaces.empty() &&
                        !info.statics.empty();
    if (info.statics_only) {
        info.base_name.clear();
        info.base_namespace.clear();
        return info;
    }

    // The primary field exists whether or not any member survived the
    // filter: it is what a *call* hands this object over as, so a class with
    // no members of its own at all (Style, say) still needs it. Every other
    // surviving interface gets a field of its own beside it.
    auto const& facts = facts_of(type);
    info.default_interface = facts.default_interface;
    info.primary_field = facts.primary_field;
    if (!info.default_interface) {
        std::print(stderr,
                   "warning: {} has no resolvable default interface -- nothing can hand this "
                   "object over to a call\n",
                   full_name(type));
    }
    return info;
}

// Namespaces routinely derive from each other -- Controls.Button extends
// Primitives.ButtonBase while Primitives.Selector extends
// Controls.ItemsControl -- so "one file per namespace" only works if
// mutually dependent namespaces share a file. This computes those groups
// (the strongly connected components of the namespace graph, by closure
// over a graph small enough that the naive algorithm is the right one) and
// names each after its shortest member, which is the parent namespace in
// every case seen so far.
std::map<std::string, std::string> group_namespaces(
    std::map<std::string, std::set<std::string>> edges) {
    std::set<std::string> nodes;
    for (auto&& [from, to_set] : edges) {
        nodes.insert(from);
        nodes.insert(to_set.begin(), to_set.end());
    }

    // Transitive closure.
    auto reach = edges;
    for (auto&& via : nodes) {
        for (auto&& from : nodes) {
            if (!reach[from].count(via)) {
                continue;
            }
            auto const& onward = reach[via];
            reach[from].insert(onward.begin(), onward.end());
        }
    }

    std::map<std::string, std::string> group_of;
    for (auto&& ns : nodes) {
        std::string name = ns;
        for (auto&& other : nodes) {
            if (other == ns || !reach[ns].count(other) || !reach[other].count(ns)) {
                continue;  // not mutually dependent
            }
            if (other.size() < name.size() || (other.size() == name.size() && other < name)) {
                name = other;
            }
        }
        group_of[ns] = name;
    }
    return group_of;
}

// The files a group has to include: where its base classes live. A base in
// the same group needs nothing (dependency order put it earlier in the same
// file); a given-from-above base pulls in the hand-written header instead.
std::set<std::string> file_includes(std::string_view group,
                                    std::vector<class_info> const& classes,
                                    std::map<std::string, std::string> const& group_of,
                                    bool impl_side) {
    std::set<std::string> includes;
    for (auto&& info : classes) {
        if (info.statics_only) {
            // wxl::Statics on the public side; on the private one the same
            // header pair's impl half, because the member bodies wrap what a
            // static call returned and that goes through Object::Impl.
            includes.insert(given_header("Statics", impl_side));
            continue;
        }
        if (info.base_namespace.empty()) {
            includes.insert(given_header(info.base_name, impl_side));
            continue;
        }
        auto const it = group_of.find(info.base_namespace);
        auto const base_group = it != group_of.end() ? it->second : info.base_namespace;
        if (base_group != group) {
            includes.insert(std::format("{}{}.h", base_group, impl_side ? ".impl" : ""));
        }
    }
    return includes;
}

void write_includes(std::ostream& out, std::set<std::string> const& includes) {
    for (auto&& include : includes) {
        if (include.starts_with('<')) {
            std::print(out, "#include {}\n", include);
        } else {
            std::print(out, "#include \"{}\"\n", include);
        }
    }
}

void write_public_header(std::filesystem::path const& path, std::string_view ns,
                         std::vector<class_info> const& classes,
                         std::map<std::string, std::string> const& group_of,
                         std::set<std::string> const& collection_elements) {
    auto out = open_output(path);

    auto includes = file_includes(ns, classes, group_of, /*impl_side=*/false);
    // The classes a narrowed setter names are announced rather than included.
    // Such a class is routinely built on this very group -- Window.titleBar
    // takes a TitleBar, whose header includes this one for FrameworkElement --
    // and a declaration taking a reference needs the name alone.
    std::set<std::string> announced;
    for (auto&& info : classes) {
        for (auto&& member : info.members) {
            if (!member.returns_void) {
                includes.insert(member.result.public_includes.begin(),
                                member.result.public_includes.end());
            }
            if (member.braced) {
                announced.insert(member.params.front().type.value_type);
                continue;
            }
            for (auto&& param : member.params) {
                includes.insert(param.type.public_includes.begin(),
                                param.type.public_includes.end());
            }
        }
        // A constructor taking arguments names its parameter types in the
        // declaration exactly like a member does.
        for (auto&& ctor : info.constructors) {
            for (auto&& param : ctor.method.params) {
                includes.insert(param.type.public_includes.begin(),
                                param.type.public_includes.end());
            }
        }
    }
    includes.erase(std::string(ns) + ".h");  // a member of this very file's own group

    // The variadic constructors below are the DSL's entry point, and they
    // apply whatever the braces contained. A group of nothing but
    // statics-only classes has no such constructor and does not need it --
    // the only file that reaches this state today is Hosting, whose whole
    // content is ElementCompositionPreview.
    if (std::any_of(classes.begin(), classes.end(),
                    [](class_info const& info) { return !info.statics_only; })) {
        includes.insert("../impl/member.h");
    }

    std::print(out, R"({}// Public wrappers for {} -- in inheritance order, so every base class
// is already complete where the class deriving from it appears.
#pragma once

)",
               banner, ns);
    write_includes(out, includes);

    std::print(out, "\nnamespace wxl {{\n");

    // Every class in the group, announced before any of them is defined.
    // Inheritance is what fixes the order below -- a base has to be
    // complete -- and that order cannot also satisfy the members, because
    // WinRT metadata genuinely contains cycles that run through a base:
    // CompositionObject.startAnimation takes a CompositionAnimation, which
    // derives from CompositionObject. A member only ever needs the name,
    // though: these are declarations, and a declaration may return an
    // incomplete type. So the names come first and the order stops being
    // a constraint on anything but bases.
    if (classes.size() > 1) {
        std::print(out, "\n");
        for (auto&& info : classes) {
            std::print(out, "class {};\n", info.name);
        }
    }
    for (auto&& name : announced) {
        std::print(out, "class {};\n", name);
    }

    for (auto&& info : classes) {
        // A class that is only its statics is only its statics: no base, no
        // Impl, no constructor. WinRT gives such a class System.Object as
        // its base like every other, but there is nothing to inherit for --
        // an instance cannot be made, so a wrapper for one would be a smart
        // pointer that is always null over an Impl field that is never
        // filled. The static members reach their factory through
        // impl::Statics<>, keyed on the interface rather than on any object,
        // and touch none of it.
        //
        // What it derives from instead is wxl::Statics, hand-written beside
        // Object: a base with nothing in it but deleted members. A deleted
        // default constructor on the class itself would say only that the
        // class cannot be built; the base says what it *is*, at a glance and
        // in one word, which is the thing a reader wants. `final` on top of
        // it, because deriving from a set of functions is meaningless.
        if (info.statics_only) {
            std::print(out, R"(
// {0}
class {1} final : public Statics
{{
public:
)",
                       full_name(info.type), info.name);
            for (auto&& member : info.members) {
                std::print(out, "    static {} {}({});\n", result_type(member), member.name,
                           parameter_list(member));
            }
            std::print(out, "}};\n");
            continue;
        }

        // `Impl` is public but only ever *declared* here, so publishing the
        // name gives a consumer nothing: the pointer-to-member every use of
        // it goes through cannot be formed without the definition, and that
        // lives in the private impl header. What it does give is the one
        // thing the wrappers need from each other -- a member of one class
        // naming the Impl field of another to hand it to WinRT without a
        // QueryInterface.
        std::print(out, R"(
// {}
class {} : public {}
{{
    using base_t = {};

public:
    // Defined in {}.impl.h -- this header deliberately never names a
    // winrt:: type, so including it stays cheap.
    class Impl;
)",
                   full_name(info.type), info.name, info.base_name, info.base_name, ns);

        // Activated or composed, the class is created the same way from
        // outside: a public default constructor. Which of the two it is
        // shows only in the body.
        bool const public_constructor = info.construction == Construction::PublicActivation ||
                                        info.construction == Construction::PublicComposition;
        if (public_constructor) {
            std::print(out, "\n");
        }
        if (public_constructor) {
            std::print(out, "    {}();\n\n", info.name);

            // Construction is the DSL. Each argument is either a deferred
            // op produced by `Name = value` or a bare value routed by its
            // type; the pack applies them, in order, to the object the
            // default constructor just activated.
            //
            // The condition is stated once, in impl/member.h: the
            // constructor exists for arguments this object can actually
            // take, and never for the single argument that belongs to the
            // copy constructor or to the protected one from an Impl.
            //
            // Explicit, and deliberately so: an implicit constructor from
            // anything at all would make every wrapper a candidate
            // conversion in unrelated overload resolutions -- a string
            // argument would be as good a FontFamily as it is a string.
            std::print(out, R"(    template <typename... Setters>
        requires impl::setter_pack<{0}, Setters...>
    explicit {0}(Setters&&... setters) : {0}() {{
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }}

)",
                       info.name);
        }

        // The constructors that take arguments, which is what WinRT means
        // by a factory interface. Explicit for the same reason the pack
        // constructor above is: a wrapper is never a silent conversion.
        if (!info.constructors.empty()) {
            if (!public_constructor) {
                std::print(out, "\n");
            }
            for (auto&& ctor : info.constructors) {
                std::print(out, "    explicit {}({});\n", info.name, parameter_list(ctor.method));
            }
            std::print(out, "\n");
        }

        // Every member is const-qualified: a wrapper is a smart pointer to
        // its Impl chain, so its own constness says nothing about the
        // object behind it -- and an event handler is handed a const
        // Object&, from which a non-const member would be unreachable.
        for (auto&& member : info.members) {
            // A static member belongs to the class, so it is neither called
            // on an object nor const-qualified.
            std::print(out, "    {}{} {}({}){};\n", member.is_static ? "static " : "",
                       result_type(member), member.name, parameter_list(member),
                       member.is_static ? "" : " const");
        }

        if (!info.positional.empty()) {
            std::print(out, "\n    // Unnamed constructor arguments, routed by type alone.\n");
            if (info.base_has_positional) {
                // Otherwise these would hide the routes the base levels
                // claim, and `Button { hAlign.center }` would stop
                // compiling the moment Button claimed a route of its own.
                std::print(out, "    using base_t::setPositional;\n");
            }
            for (auto&& route : info.positional) {
                std::print(out, "    void setPositional({} value) const {{ {}; }}\n",
                           route.param_type, route.statement);
            }
        }
        std::print(out, "\nprotected:\n");
        if (info.construction == Construction::ProtectedOnly) {
            // Mirrors `protected extern X()` in the real class: only a type
            // deriving from it can construct one, so there is no activation
            // to do here.
            std::print(out, "    {}();\n\n", info.name);
        } else if (info.construction == Construction::PublicFactory) {
            std::print(out,
                       "    // No default constructor: every constructor the real {}\n"
                       "    // declares takes arguments, and they are above.\n\n",
                       info.type.TypeName());
        } else if (info.construction == Construction::None) {
            std::print(out,
                       "    // No default constructor: the real {} declares none,\n"
                       "    // instances only ever arrive from somewhere else.\n\n",
                       info.type.TypeName());
        }

        std::print(out, R"(    explicit {}(Impl* impl) noexcept;

    friend class Object::Impl;
}};
)",
                   info.name);
    }

    if (!collection_elements.empty()) {
        // Stated after the classes, where every element type above is a
        // complete type. Without it a consumer would instantiate the whole
        // template itself -- exactly the per-translation-unit cost the
        // library exists to avoid; with it, the one definition compiled
        // inside wxl's own build is what every use links against.
        std::print(out, "\n// The collection specializations this namespace hands out. Their\n"
                        "// bodies are compiled once, inside wxl, never here.\n");
        for (auto&& element : collection_elements) {
            std::print(out, "extern template class Collection<{}>;\n", element);
        }
    }

    // Recovering a wrapper from a bare Object, which is what a handler naming
    // its own sender type compiles into. try_as reads T::Impl::winrt_t and
    // builds the wrapper around it -- both on wxl's private side, so a
    // consumer that instantiated the template itself would have nothing to
    // link against. These point every use at the one body compiled inside
    // wxl, the same bargain Collection<T> above makes.
    {
        bool announced = false;
        for (auto&& info : classes) {
            if (info.statics_only) continue;
            if (!announced) {
                std::print(out, "\n// Reading one of these out of an event's sender. The bodies\n"
                                "// are compiled once, inside wxl, never here.\n");
                announced = true;
            }
            std::print(out, "extern template {0} Object::try_as<{0}>() const;\n", info.name);
        }
    }

    std::print(out, "\n}}  // namespace wxl\n");
}

void write_impl_header(std::filesystem::path const& path, std::string_view ns,
                       std::vector<class_info> const& classes,
                       std::map<std::string, std::string> const& group_of) {
    auto out = open_output(path);

    auto includes = file_includes(ns, classes, group_of, /*impl_side=*/true);
    includes.insert(std::string(ns) + ".h");
    for (auto&& info : classes) {
        includes.insert(winrt_include(info.type.TypeNamespace()));
        for (auto&& iface : info.interfaces) {
            includes.insert(winrt_include(iface.TypeNamespace()));
        }
    }

    std::print(out, R"({}// Private side of {} -- never included by consuming code.
//
// One field per interface a level implements *directly* and that survived
// the profile filter: an interface whose every member was filtered out
// needs no field, no lazy QueryInterface slot, no code. The base classes'
// interfaces belong to their own levels.
//
// The exception is the level's own default interface. Its field is declared
// as the projection *class*, and it exists whether or not any member
// survived, because that is what a call hands the object over as: a
// parameter typed as the class then binds to it directly, with no
// QueryInterface and no conversion. Calling through it is equally free --
// the class derives from that interface, so a method the interface declares
// is reached without touching the held pointer. Methods of any *other*
// interface reached through the class would not be: cppwinrt gets to those
// through a conversion operator that queries every time, which is exactly
// why every other interface keeps a field of its own.
#pragma once

)",
               banner, ns);
    write_includes(out, includes);

    std::print(out, "\nnamespace wxl {{\n");

    for (auto&& info : classes) {
        if (info.statics_only) {
            continue;  // no instance, no wrapper, nothing private to say
        }

        std::string const winrt_class =
            std::format("{}::{}", winrt_namespace(info.type.TypeNamespace()),
                        info.type.TypeName());

        // `winrt_t` is what the wrapper stands for, stated once per level.
        // Anything that needs the real WinRT type of a wxl type reads it
        // here rather than keeping a table of its own -- a second table
        // would be a second thing to get wrong, and this one cannot drift.
        std::print(out,
                   "\nclass {}::Impl : public base_t::Impl\n{{\npublic:\n    using "
                   "base_t::Impl::Impl;\n\n    using winrt_t = {};\n",
                   info.name, winrt_class);

        if (!info.primary_field.empty()) {
            // Explicitly empty: a projection *class* has no default
            // constructor, only the one from nullptr -- an interface would
            // default to empty on its own.
            std::print(out, "\n    {} {}{{nullptr}};\n", winrt_class, info.primary_field);

            // Wrapping an object a call returned fills both slots at once:
            // the two hold the same pointer (every WinRT interface is an
            // IInspectable at the ABI), so the base copy is one AddRef and
            // the move below takes the caller's reference rather than adding
            // a second.
            //
            // The conversion operator is how this object reaches a call that
            // wants that interface: the caller passes the wrapper's Impl and
            // names no field at all, and the lazy cache behind get<> pays the
            // one QueryInterface per object.
            std::print(out, R"(
    explicit Impl({0}&& object) noexcept
        : base_t::Impl(object), {1}(std::move(object)) {{}}

    operator {0} const&() {{ return get<&Impl::{1}>(); }}
)",
                       winrt_class, info.primary_field);
        }

        for (auto&& iface : info.interfaces) {
            if (iface == info.default_interface) {
                continue;  // the class-typed field above already stands for it
            }
            auto const iface_type =
                std::format("{}::{}", winrt_namespace(iface.TypeNamespace()), iface.TypeName());
            auto const field = interface_field_name(iface.TypeName());
            std::print(out, "\n    {0} {1};\n    operator {0} const&() {{ return get<&Impl::{1}>(); }}\n",
                       iface_type, field);
        }
        std::print(out, "}};\n");
    }

    std::print(out, "\n}}  // namespace wxl\n");
}

void write_source(std::filesystem::path const& path, std::string_view ns,
                  std::vector<class_info> const& classes,
                  std::set<std::string> const& collection_elements) {
    auto out = open_output(path);

    bool const any_activation =
        std::any_of(classes.begin(), classes.end(), [](class_info const& info) {
            return info.construction == Construction::PublicActivation ||
                   info.construction == Construction::PublicComposition;
        });

    bool const any_statics = std::any_of(classes.begin(), classes.end(),
                                         [](class_info const& info) { return !info.statics.empty(); });

    // A constructor taking arguments reaches its factory interface through
    // the same proxy a static member does.
    bool const any_constructors =
        std::any_of(classes.begin(), classes.end(),
                    [](class_info const& info) { return !info.constructors.empty(); });

    std::set<std::string> includes;
    if (any_activation) {
        includes.insert("../impl/activation_factory.h");
    }
    if (any_statics || any_constructors) {
        includes.insert("../impl/statics.h");
    }
    for (auto&& info : classes) {
        for (auto&& iface : info.statics) {
            includes.insert(winrt_include(iface.TypeNamespace()));
        }
        for (auto&& ctor : info.constructors) {
            includes.insert(ctor.include);
        }
        // The factory interface a composable class is created through is
        // named in the body, so its namespace's projection header is needed
        // even when nothing else in this file names that namespace.
        if (!info.composable_factory.empty()) {
            auto const dot = info.composable_factory.rfind('.');
            includes.insert(winrt_include(std::string_view{info.composable_factory}.substr(0, dot)));
        }
        for (auto&& member : info.members) {
            if (!member.returns_void) {
                includes.insert(member.result.impl_includes.begin(),
                                member.result.impl_includes.end());
            }
            for (auto&& param : member.params) {
                includes.insert(param.type.impl_includes.begin(), param.type.impl_includes.end());
            }
        }
    }

    std::print(out, "{}#include \"{}.impl.h\"\n", banner, ns);
    if (!includes.empty()) {
        std::print(out, "\n");
        write_includes(out, includes);
    }

    std::print(out, "\nnamespace wxl {{\n");

    if (any_activation || any_statics || any_constructors) {
        // Deliberately here and not in a header: each name is used by
        // exactly one translation unit -- this one -- and a header would
        // make every including TU parse it.
        std::print(out, "\nnamespace impl {{\n// Runtime class names, keyed on by the cached "
                        "activation factory.\n");
        for (auto&& info : classes) {
            if (info.construction == Construction::PublicActivation ||
                info.construction == Construction::PublicComposition) {
                std::print(out, R"(
template <>
struct runtime_class_name_of<{}> {{
    static constexpr wchar_t value[] = L"{}";
}};
)",
                           info.name, full_name(info.type));
            }
            // A factory interface is keyed on the same way a statics one is
            // -- it too is reached through the class's activation factory,
            // and it too names exactly one class. Once per interface, not
            // once per constructor: a factory routinely declares several.
            std::set<std::string> keyed;
            for (auto&& ctor : info.constructors) {
                if (!keyed.insert(ctor.statics_interface).second) {
                    continue;
                }
                std::print(out, R"(
template <>
struct runtime_class_name_of<{}> {{
    static constexpr wchar_t value[] = L"{}";
}};
)",
                           ctor.statics_interface, full_name(info.type));
            }
            // A statics interface is keyed on directly: it already names
            // exactly one class, so it needs no marker type of its own.
            for (auto&& iface : info.statics) {
                std::print(out, R"(
template <>
struct runtime_class_name_of<{}::{}> {{
    static constexpr wchar_t value[] = L"{}";
}};
)",
                           winrt_namespace(iface.TypeNamespace()), iface.TypeName(),
                           full_name(info.type));
            }
        }
        std::print(out, "}}  // namespace impl\n");
    }

    for (auto&& info : classes) {
        // A statics-only class has no Impl to be constructed from -- only
        // the member bodies below, which reach their factory through
        // impl::Statics<> and never touch an object.
        if (!info.statics_only) {
            std::print(out, "\n{}::{}(Impl* impl) noexcept : base_t(impl) {{}}\n", info.name,
                       info.name);
        }

        switch (info.construction) {
            case Construction::PublicActivation:
                // Activated straight into the Impl's own field: the cached
                // factory writes the raw pointer into the slot cppwinrt
                // stores it in, so there is no temporary smart pointer and
                // no extra AddRef/Release on the way.
                std::print(out, R"(
{}::{}() : base_t(new Impl{{}}) {{
    impl::ActivationFactory<{}>::activate(put_abi());
}}
)",
                           info.name, info.name, info.name);
                break;
            case Construction::PublicComposition:
                // A composable class, which may or may not answer plain
                // activation; the factory below finds out once and keeps the
                // path that worked. The factory interface is named because
                // its IID is what the composing path asks the factory object
                // for.
                std::print(out, R"(
{}::{}() : base_t(new Impl{{}}) {{
    impl::ComposableFactory<{}, {}>::activate(put_abi());
}}
)",
                           info.name, info.name, info.name,
                           winrt_type_name(info.composable_factory));
                break;
            case Construction::ProtectedOnly:
                // No activation: the real class has no public constructor,
                // so an instance can only come from a derived type.
                std::print(out, "\n{}::{}() : base_t(new Impl{{}}) {{}}\n", info.name, info.name);
                break;
            case Construction::PublicFactory:
            case Construction::None:
                break;
        }

        // A constructor that takes arguments goes through the class's own
        // factory interface, reached by the same cached proxy a static
        // member is: the factory object answers both. What it hands back is
        // the finished object, so it is moved into a fresh Impl rather than
        // activated into an empty one.
        for (auto&& ctor : info.constructors) {
            std::string arguments;
            for (auto&& param : ctor.method.params) {
                if (!arguments.empty()) {
                    arguments += ", ";
                }
                arguments += substitute(param.type.to_winrt, param.name);
            }

            std::print(out, "\n{}::{}({})\n    : base_t(new Impl{{impl::Statics<{}>->{}({})}}) {{}}\n",
                       info.name, info.name, parameter_list(ctor.method), ctor.statics_interface,
                       ctor.method.winrt_name, arguments);
        }

        // Each member is one hop: fetch the interface declaring it from the
        // lazy cache, hand the arguments over converted, convert the result
        // back. get<> does a real QueryInterface only the first time the
        // object is asked for that interface.
        for (auto&& member : info.members) {
            auto const signature =
                std::format("{} {}::{}({}){}", result_type(member), info.name, member.name,
                            parameter_list(member), member.is_static ? "" : " const");

            if (member.kind == member_info::Kind::EventAdd) {
                // The delegate is built from a braced initialiser, so its
                // type never has to be named here -- the projection's own
                // parameter type is what constructs it. That is also what
                // makes the generic TypedEventHandler<S, A> events work
                // without spelling their instantiation out.
                //
                // The handler is copied into the delegate: it outlives this
                // call by definition, and the sender it is given is a fresh
                // wrapper around the object the runtime passed.
                //
                // A static event is reached the same way every other static
                // member is -- through the statics proxy. There is no object
                // to hop through and no field to hop by: a statics-only class
                // has neither, and an Impl field name would come out empty.
                auto const raise = member.is_static
                                       ? std::format("impl::Statics<{}>->{}",
                                                     member.statics_interface, member.winrt_name)
                                       : std::format("get<&Impl::{}>().{}", member.field,
                                                     member.winrt_name);

                std::print(out, R"(
{} {{
    return impl::from_winrt({}({{
        [handler](auto const& sender, auto const& args) {{
            auto view = {};
            handler(Object::Impl::wrap<Object>(sender), view);
        }}}}));
}}
)",
                           signature, raise,
                           member.args_are_wrapper
                               // An owning wrapper, copied from what the
                               // runtime passed rather than taking its
                               // reference: the caller keeps ownership.
                               ? std::format("Object::Impl::wrap<{}>(args)", member.args_type)
                               : std::format("Object::Impl::make_args<{}>(winrt::get_abi(args))",
                                             member.args_type));
                continue;
            }

            if (!member.synthetic_call.empty()) {
                // The property WinRT does not have, implemented by a
                // hand-written function: it is handed this level's own field
                // -- the object it needs, in the type it needs -- and the
                // value.
                std::print(out, "\n{} {{\n    {}(get<&Impl::{}>(), value);\n}}\n", signature,
                           member.synthetic_call, info.primary_field);
                continue;
            }

            std::string arguments;
            for (auto&& param : member.params) {
                if (!arguments.empty()) {
                    arguments += ", ";
                }
                switch (member.kind) {
                    case member_info::Kind::EventRemove:
                        arguments += std::format("impl::to_winrt({})", param.name);
                        break;
                    case member_info::Kind::BoxedString:
                        arguments += std::format("impl::box_text({})", param.name);
                        break;
                    default:
                        arguments += substitute(param.type.to_winrt, param.name);
                        break;
                        break;
                }
            }

            // A static member is reached through the statics proxy, which
            // resolves the class's activation factory once and then answers
            // from a cached pointer; an instance member goes through the
            // object's own lazily cached interface field.
            auto const call =
                member.is_static
                    ? std::format("impl::Statics<{}>->{}({})", member.statics_interface,
                                  member.winrt_name, arguments)
                    : std::format("get<&Impl::{}>().{}({})", member.field,
                                  member.method_call.empty() ? member.winrt_name
                                                             : member.method_call,
                                  arguments);

            std::print(out, "\n{} {{\n    {}{};\n}}\n", signature,
                       member.returns_void ? "" : "return ",
                       member.returns_void ? call
                                           : substitute(member.result.from_winrt, call));
        }
    }

    if (!collection_elements.empty()) {
        // The one definition of each specialization in the whole library.
        // Which translation unit gets it is arbitrary but has to be decided
        // somewhere: it is the first file group that names the element, and
        // that file already includes everything the instantiation needs --
        // the element's own Impl, which is where Collection reads the WinRT
        // type it stands for.
        std::print(out, "\n// Collection specializations defined here, once for the library.\n");
        for (auto&& element : collection_elements) {
            std::print(out, "template class Collection<{}>;\n", element);
        }
    }

    // The one body of each try_as the header promised. This file has what the
    // instantiation needs and a consumer has not: the class's own Impl, where
    // try_as reads the WinRT type the wrapper stands for.
    {
        bool announced = false;
        for (auto&& info : classes) {
            if (info.statics_only) continue;
            if (!announced) {
                std::print(out, "\n// try_as defined here, once for the library.\n");
                announced = true;
            }
            std::print(out, "template {0} Object::try_as<{0}>() const;\n", info.name);
        }
    }

    std::print(out, "\n}}  // namespace wxl\n");
}

// Where every wrapped type can be found, once the class grouping is known:
// the flat wxl name and the public header declaring it. Enums and structs
// sit in per-namespace files of their own; a class lives in its group's
// file.
TypeIndex build_type_index(Model const& model, std::set<TypeDef> const& generated_classes,
                           std::map<std::string, std::string> const& group_of) {
    TypeIndex index;

    auto const add = [&index](TypeDef const& type, std::string header) {
        index.names.emplace(type, wxl_class_name(type));
        index.headers.emplace(type, std::move(header));
    };

    for (auto&& type : model.enums) {
        add(type, std::format("{}.Enums.h", type.TypeNamespace()));
    }
    for (auto&& type : model.structs) {
        // A struct that does not mirror the ABI field for field is not
        // generated (see mirrors_abi), so nothing may name it either.
        if (!project_type(type) && mirrors_abi(type)) {
            add(type, std::format("{}.Structs.h", type.TypeNamespace()));
        }
    }
    for (auto&& type : generated_classes) {
        std::string const ns{type.TypeNamespace()};
        auto const it = group_of.find(ns);
        add(type, std::format("{}.h", it != group_of.end() ? it->second : ns));
    }
    // Args views live in their own per-namespace files. They are in the
    // index so an event can name the one it hands its handler; map_type
    // still refuses them anywhere else.
    for (auto&& type : model.classes) {
        if (is_event_args_class(type)) {
            add(type, std::format("{}.EventArgs.h", type.TypeNamespace()));
        }
    }
    return index;
}

}  // namespace

void write_classes(Output const& out, Model const& model, Emitted& emitted, ClassOutput& produced) {
    std::set<TypeDef> generated;
    for (auto&& type : model.classes) {
        // A collection class is not wrapped either: it exists in metadata
        // only to name an IVector<T>, and wxl::Collection<T> already is that.
        if (!is_event_args_class(type) && !project_type(type) && !is_collection_class(type)) {
            generated.insert(type);
        }
    }

    // An interface a profile names directly is wrapped like a class. It
    // goes *after* the classes: it roots directly at Object, so no base
    // needs it earlier, and members naming it across the file are served
    // by the group's forward declarations like any other member edge.
    std::vector<TypeDef> wrapped{model.classes};
    for (auto&& type : model.listed_interfaces) {
        generated.insert(type);
        wrapped.push_back(type);
    }

    std::vector<class_info> infos_in_order;
    std::set<std::string> written_names;
    std::map<std::string, std::set<std::string>> namespace_deps;
    for (auto&& type : wrapped) {
        if (!generated.count(type)) {
            continue;  // EventArgs wrappers and projected types are emitted elsewhere
        }
        auto info = analyze_class(type, generated, model);
        if (!written_names.insert(info.name).second) {
            std::print(stderr, "warning: two classes map to the same flat wxl name '{}' ({})\n",
                       info.name, full_name(type));
            continue;
        }
        std::string const ns{type.TypeNamespace()};
        if (!info.base_namespace.empty() && info.base_namespace != ns) {
            namespace_deps[ns].insert(info.base_namespace);
        }
        namespace_deps.try_emplace(ns);
        infos_in_order.push_back(std::move(info));
    }

    // Namespaces that derive from each other share a file; everything else
    // gets its own. Grouping preserves the closure's dependency order
    // within each file, which is what lets a base class simply appear
    // earlier instead of needing an include.
    auto const group_of = group_namespaces(namespace_deps);

    // Members come second, because naming a type in a signature means
    // knowing which file declares it, which is only settled once the
    // grouping above is.
    auto const index = build_type_index(model, generated, group_of);
    produced.index = index;

    size_t emitted_members = 0;
    std::vector<std::string> skipped_report;
    std::vector<std::string> withheld_positional;
    std::vector<std::string> content_collections;
    Dsl dsl;
    Schema schema;
    // An attached property is written on the child and applied by the
    // parent's statics, so it belongs to the schema of the class its setter
    // takes -- which is a different class from the one being visited, and
    // may be visited later. Held here by that target's name and merged once
    // every class has been seen.
    std::vector<std::pair<std::string, Schema::Member>> attached_elsewhere;
    std::map<std::string, bool> chain_has_positional;  // by wxl class name

    // The whole hierarchy up front: a positional route has to be checked
    // against levels *below* the class as well as above it, and those are
    // not yet visited in this dependency-ordered walk.
    std::map<std::string, std::string> base_of;
    for (auto&& info : infos_in_order) {
        base_of.emplace(info.name, info.base_name);
    }


    for (auto&& info : infos_in_order) {
        std::vector<skipped_member> skipped;
        for (auto&& iface : info.interfaces) {
            auto const members = model.members.find(iface);
            if (members == model.members.end()) {
                continue;
            }
            collect_interface_members(iface, members->second, index, info.members, skipped);
        }

        // Static members are collected the same way and then marked: the
        // only difference is where the call goes -- through the cached
        // statics proxy rather than through the object's own Impl.
        for (auto&& iface : info.statics) {
            auto const members = model.members.find(iface);
            if (members == model.members.end()) {
                continue;
            }
            std::vector<member_info> collected;
            collect_interface_members(iface, members->second, index, collected, skipped);
            for (auto&& member : collected) {
                member.is_static = true;
                member.field.clear();
                member.statics_interface = std::format(
                    "{}::{}", winrt_namespace(iface.TypeNamespace()), iface.TypeName());
                info.members.push_back(std::move(member));
            }
        }

        // The constructors that take arguments. A factory interface's
        // methods are exactly those constructors, so they are collected the
        // way members are and then checked for the one thing that makes a
        // method a constructor here: it hands back an instance of this very
        // class.
        for (auto&& iface : [&]() -> std::vector<TypeDef> const& {
                 static std::vector<TypeDef> const none;
                 auto const it = model.factories_of.find(info.type);
                 return it == model.factories_of.end() ? none : it->second;
             }()) {
            auto const members = model.members.find(iface);
            if (members == model.members.end()) {
                continue;
            }
            std::vector<member_info> collected;
            collect_interface_members(iface, members->second, index, collected, skipped);
            for (auto&& member : collected) {
                if (member.returns_void || member.result.value_type != info.name) {
                    skipped.push_back({member.name, std::format("constructor: {} hands back {}, "
                                                                "not an instance of the class",
                                                                iface.TypeName(),
                                                                result_type(member))});
                    continue;
                }
                info.constructors.push_back(
                    {std::format("{}::{}", winrt_namespace(iface.TypeNamespace()),
                                 iface.TypeName()),
                     winrt_include(iface.TypeNamespace()), std::move(member)});
            }
        }

        // The properties a profile added to this class. They are ordinary
        // members from here on -- key, tag, declaration, unnamed-argument
        // route -- and differ only in the body.
        if (auto const added = model.synthetic_of.find(info.type);
            added != model.synthetic_of.end()) {
            for (auto&& [declaration, value_type] : added->second) {
                // A value wxl owns has no metadata to map: the profile named
                // the C++ type outright, and it is taken by value.
                auto use = !declaration.cpp_type.empty()
                               ? TypeUse{.supported = true,
                                         .value_type = declaration.cpp_type,
                                         .param_type = declaration.cpp_type,
                                         .public_includes = {declaration.cpp_include}}
                           : value_type ? map_type(value_type, index)
                                        : map_element_type(declaration.type);
                if (!use.supported) {
                    skipped.push_back(
                        {declaration.name, std::format("synthetic member: {}", use.reason)});
                    continue;
                }
                // The hand-written function behind it is what the body
                // calls, so its header belongs to the .cpp side.
                use.impl_includes = {declaration.include};

                member_info member{member_info::Kind::Forward, member_name(declaration.name),
                                   declaration.name, {}, {},
                                   /*returns_void=*/true, {{"value", std::move(use)}}};
                member.is_property_setter = true;
                member.synthetic_call = declaration.function;
                info.members.push_back(std::move(member));
            }
        }

        // The methods a profile writes as tags. Each becomes a property setter
        // beside the method it calls, and only a method that is a setter in
        // all but name qualifies: one argument, nothing handed back, an
        // instance to call it on.
        if (auto const setters = model.setter_methods_of.find(info.type);
            setters != model.setter_methods_of.end()) {
            for (auto&& [declaration, value_type] : setters->second) {
                auto const method =
                    std::find_if(info.members.begin(), info.members.end(), [&](auto&& member) {
                        return member.kind == member_info::Kind::Forward &&
                               member.winrt_name == declaration.method;
                    });
                if (method == info.members.end()) {
                    skipped.push_back({declaration.method,
                                       "setter method: the class declares no such method, or "
                                       "the profile filtered it out"});
                    continue;
                }
                if (method->is_static || !method->returns_void || method->params.size() != 1 ||
                    method->is_property_setter) {
                    skipped.push_back({declaration.method,
                                       "setter method: not a method taking one argument and "
                                       "returning nothing"});
                    continue;
                }

                member_info setter = *method;
                setter.winrt_name = declaration.method.substr(3);
                setter.name = member_name(setter.winrt_name);
                setter.method_call = declaration.method;
                setter.is_property_setter = true;
                if (!declaration.type.empty()) {
                    auto use = value_type ? map_type(value_type, index)
                                          : TypeUse{.reason = std::format(
                                                        "no type {} in the metadata",
                                                        declaration.type)};
                    if (!use.supported || !use.is_wrapper) {
                        skipped.push_back(
                            {declaration.method,
                             std::format("setter method: {}",
                                         use.supported ? declaration.type + " is not a class"
                                                       : use.reason)});
                        continue;
                    }
                    setter.params.front().type = std::move(use);
                    setter.braced = true;
                }
                info.members.push_back(std::move(setter));
            }
        }

        // Two interfaces of one class can declare the same name (a revision
        // interface restating a member of the one it supersedes); C++ would
        // see a redeclaration, so the first one wins.
        // The key is the signature, not just the name and arity: one name
        // legitimately carries several one-argument overloads (an event's
        // add and remove, a property's own type and the string form that
        // boxes for it).
        std::set<std::string> seen;
        std::erase_if(info.members, [&seen](member_info const& member) {
            std::string signature = member.is_static ? "static " + member.name : member.name;
            for (auto&& param : member.params) {
                signature += '|';
                signature += param.type.param_type;
            }
            return !seen.insert(std::move(signature)).second;
        });

        emitted_members += info.members.size();
        for (auto&& drop : skipped) {
            skipped_report.push_back(std::format("{}.{}: {}", info.name, drop.name, drop.reason));
        }

        // The DSL vocabulary, and the type routes this level claims. Only a
        // setter contributes: a read-only property has nothing to assign to.
        std::string text_route;
        std::string content_route;
        std::string const content_property = content_property_of(info.type);
        auto const& attached_here = [&]() -> std::vector<std::string> const& {
            static std::vector<std::string> const none;
            auto const it = model.attached_of.find(info.type);
            return it == model.attached_of.end() ? none : it->second;
        }();

        Schema::Class klass{info.name, info.base_name, {}, {}};
        if (auto const header = index.headers.find(info.type); header != index.headers.end()) {
            klass.header = header->second;
        }

        for (auto&& member : info.members) {
            if (member.is_static) {
                // One kind of static does get a tag, and it is written on
                // somebody else: an attached property's setter takes the
                // element it applies to, so `row = 1` inside a Button reaches
                // Grid::setRow(button, 1).
                if (member.winrt_name.starts_with("Set") && member.params.size() == 2 &&
                    std::find(attached_here.begin(), attached_here.end(),
                              member.winrt_name.substr(3)) != attached_here.end()) {
                    dsl.attached.emplace(member.winrt_name.substr(3),
                                         Dsl::Attached{info.name, member.name,
                                                       member.params[1].type.value_type});
                    attached_elsewhere.emplace_back(
                        member.params.front().type.value_type,
                        Schema::Member{Schema::Member::Kind::Property,
                                       member.winrt_name.substr(3),
                                       member_name(member.winrt_name.substr(3)),
                                       member.params[1].type.value_type, /*attached=*/true});
                    dsl.includes.insert(member.params[1].type.public_includes.begin(),
                                        member.params[1].type.public_includes.end());
                    if (auto const header = index.headers.find(info.type);
                        header != index.headers.end()) {
                        dsl.includes.insert(header->second);
                    }
                }
                // Everything else static is a call, not a tag:
                // `OverlappedPresenter::create()` is not assigned to.
                continue;
            }
            if (!member.returns_void && member.result.is_collection) {
                info.collection_elements.insert(member.result.element_type);
            }
            for (auto&& param : member.params) {
                if (param.type.is_collection) {
                    info.collection_elements.insert(param.type.element_type);
                }
            }

            if (member.kind == member_info::Kind::EventAdd) {
                dsl.events.insert(member.winrt_name);
                klass.members.push_back({Schema::Member::Kind::Event, member.winrt_name,
                                         "on" + member.winrt_name});
                continue;
            }

            // A collection-valued property is written `Children[a, b, c]`,
            // not assigned to, so its tag is a subscript rather than an
            // assignment -- and it is the *getter* that reveals it, since a
            // collection property is read-only in metadata.
            if (member.kind == member_info::Kind::Forward && !member.returns_void &&
                member.params.empty() && member.result.is_collection) {
                dsl.collection_element.emplace(member.winrt_name, member.result.element_type);
                dsl.includes.insert(member.result.public_includes.begin(),
                                    member.result.public_includes.end());
                klass.members.push_back({Schema::Member::Kind::Collection, member.winrt_name,
                                         member.name, member.result.element_type});

                // The class's content property: an element written straight
                // inside its parent's braces is appended to it, so
                // `StackPanel { TextBlock {...}, Button {...} }` says what
                // `children[...]` says and reads as the tree it builds.
                //
                // The inheritance check the property routes go through is
                // deliberately not made here: an element type is nearly
                // always an ancestor of the class holding the collection
                // (Panel is a UIElement), and the one case that check exists
                // for -- a lone argument of the class's own kind, which
                // belongs to the copy constructor -- is already excluded by
                // the setter-pack constructor's own condition.
                if (member.winrt_name == content_property) {
                    info.positional.emplace_back(
                        std::format("{} const&", member.result.element_type),
                        std::format("{}().append(value)", member.name));
                    content_collections.push_back(
                        std::format("{}.{}: {}", info.name, member.name,
                                    member.result.element_type));
                }
                continue;
            }

            if (!member.is_property_setter) {
                if (member.kind == member_info::Kind::BoxedString &&
                    member.winrt_name == "Content") {
                    content_route = member.name;
                }
                continue;
            }

            auto const& value = member.params.front().type;
            auto [it, inserted] =
                dsl.property_value_type.emplace(member.winrt_name, value.value_type);
            if (!inserted && it->second != value.value_type) {
                it->second.clear();  // no single type, so no braced form
            }
            dsl.includes.insert(value.public_includes.begin(), value.public_includes.end());

            // The schema keeps the type this class declares it with, which is
            // the disagreement above resolved rather than given up on -- for
            // the disagreement between *classes*. One class saying it twice
            // is a different thing (SymbolIcon takes both Symbol and
            // FluentSymbol) and lands where the flat tag lands: no single
            // type, so no braced form, and one member rather than two.
            auto const declared = std::find_if(
                klass.members.begin(), klass.members.end(), [&member](auto&& already) {
                    return already.kind == Schema::Member::Kind::Property
                           && already.key == member.winrt_name;
                });
            if (declared != klass.members.end()) {
                if (declared->type != value.value_type) {
                    declared->single_type = false;
                }
            } else {
                klass.members.push_back({Schema::Member::Kind::Property, member.winrt_name,
                                         member.name, value.value_type});
                klass.members.back().braced = member.braced;
            }
            if (member.braced) {
                dsl.braced.insert(member.winrt_name);
            }

            // A property whose type belongs to it alone is unambiguous as an
            // unnamed argument: writing `hAlign.center` can only mean
            // HorizontalAlignment, and a Margin can only be the Margin, so
            // the property name adds nothing. Type and property carry the same
            // name here because the conventions keep them apart -- the type is
            // Margin, the member is margin.
            // The class's content property, where it holds one element
            // rather than a collection: a Border's Child, a Flyout's
            // Content. Same rule as the collection form -- what is written
            // inside the braces is what the parent is for -- and the
            // inheritance check is relaxed for the same reason, the element
            // type being an ancestor of nearly every class that has one.
            bool const is_content_element = member.winrt_name == content_property && value.is_wrapper;

            if (value.from_string) {
                // A string literal is already the class's own text, and a
                // type it also turns into by itself would make the same
                // literal mean two things. The property keeps its name.
                withheld_positional.push_back(
                    std::format("{}.{}: a string literal is already the text route", info.name,
                                member.name));
            } else if (value.value_type == member.winrt_name || is_content_element) {
                // A route has to say something. `Object` says nothing --
                // every wrapper is one -- so a property typed that way keeps
                // its name whatever else it is.
                bool const refused = is_content_element
                                         ? universal_type(value.value_type)
                                         : positional_conflicts(value.value_type, info.name, base_of);
                if (refused) {
                    withheld_positional.push_back(std::format(
                        "{}.{}: {} is related to the class by inheritance", info.name, member.name,
                        value.value_type));
                } else {
                    info.positional.emplace_back(value.param_type,
                                                 std::format("{}(value)", member.name));
                }
            } else if (member.winrt_name == "Text" && value.param_type == "string_param") {
                text_route = member.name;
            }
        }

        // The default string property, so `TextBlock { L"..." }` needs no
        // property name. A rule over the metadata rather than a table of
        // names: point a profile at another control library and its
        // controls get the same treatment with nothing hand-written.
        if (auto const& route = text_route.empty() ? content_route : text_route; !route.empty()) {
            info.positional.emplace_back("string_param", std::format("{}(value)", route));
        }

        bool const base_has = info.base_name != "Object" && info.base_name != "DependencyObject" &&
                              chain_has_positional[info.base_name];
        info.base_has_positional = base_has;
        chain_has_positional[info.name] = base_has || !info.positional.empty();

        schema.classes.push_back(std::move(klass));
    }

    if (!skipped_report.empty()) {
        std::print("\nmembers skipped -- no wxl type for a signature yet ({}):\n",
                   skipped_report.size());
        for (auto&& line : skipped_report) {
            std::print("  {}\n", line);
        }
        std::print("\n");
    }

    if (!content_collections.empty()) {
        std::print("\ncontent collections ({}) -- an unnamed element of that type is\n"
                   "appended to this property:\n",
                   content_collections.size());
        for (auto&& line : content_collections) {
            std::print("  {}\n", line);
        }
        std::print("\n");
    }

    if (!withheld_positional.empty()) {
        std::print("\nunnamed-argument routes withheld ({}) -- these properties keep their\n"
                   "named form only:\n",
                   withheld_positional.size());
        for (auto&& line : withheld_positional) {
            std::print("  {}\n", line);
        }
        std::print("\n");
    }

    std::map<std::string, std::vector<class_info>> by_group;
    for (auto&& info : infos_in_order) {
        std::string const ns{info.type.TypeNamespace()};
        auto const it = group_of.find(ns);
        by_group[it != group_of.end() ? it->second : ns].push_back(info);
    }

    // Which collection specializations each file group needs stated, and
    // which of them it is the one to define. An explicit instantiation
    // definition may exist in exactly one translation unit, so the first
    // group naming an element -- in the same order the files are written --
    // takes it.
    std::map<std::string, std::set<std::string>> used_by_group;
    std::map<std::string, std::set<std::string>> defined_by_group;
    std::set<std::string> already_defined;
    for (auto&& [ns, infos] : by_group) {
        for (auto&& info : infos) {
            for (auto&& element : info.collection_elements) {
                used_by_group[ns].insert(element);
                if (already_defined.insert(element).second) {
                    defined_by_group[ns].insert(element);
                }
            }
        }
    }
    produced.collection_elements = already_defined;

    size_t classes = 0;
    size_t activatable = 0;
    size_t constructors = 0;
    for (auto&& [ns, infos] : by_group) {
        auto const header = out.dir / (ns + ".h");
        auto const impl_header = out.dir / (ns + ".impl.h");
        auto const source = out.dir / (ns + ".cpp");

        write_public_header(header, ns, infos, group_of, used_by_group[ns]);
        write_impl_header(impl_header, ns, infos, group_of);
        write_source(source, ns, infos, defined_by_group[ns]);
        emitted.add(header);
        emitted.add(impl_header);
        emitted.add(source);

        classes += infos.size();
        for (auto&& info : infos) {
            activatable += info.construction == Construction::PublicActivation ? 1 : 0;
            constructors += info.constructors.size();
        }
    }

    std::print("wrote {} classes in {} file group(s), {} publicly activatable, {} constructors "
               "taking arguments, {} members\n",
               classes, by_group.size(), activatable, constructors, emitted_members);

    // The values every enum-typed tag carries. Only the enums some property
    // actually has, since a tag is the only thing that surfaces them.
    std::set<std::string> const enum_types{[&] {
        std::set<std::string> types;
        for (auto&& [name, value_type] : dsl.property_value_type) {
            types.insert(value_type);
        }
        return types;
    }()};
    for (auto&& type : model.enums) {
        auto const name = std::string{type.TypeName()};
        if (enum_types.count(name)) {
            dsl.enumerators.emplace(name, enum_members(type, model.members));
        }
    }

    // The vocabulary of the classes wxl writes by hand (types.json): no
    // metadata declares it, and the dispatch behind a tag is a template on the
    // object, so a key and a tag are all such a member needs. A generated class
    // that declares the same name with another type leaves no definite one.
    for (auto&& property : type_map().hand_written_properties) {
        auto [it, inserted] =
            dsl.property_value_type.emplace(property.name, property.value_type);
        if (!inserted && it->second != property.value_type) {
            it->second.clear();
        }
        if (!property.include.empty()) {
            dsl.includes.insert(property.include);
        }
    }
    dsl.events.insert(type_map().hand_written_events.begin(),
                      type_map().hand_written_events.end());

    // A bound-only member needs the same key and tag; the setter the tag
    // dispatches to is constrained on the member's existence, so on a class
    // that has none the assignment is refused rather than compiled.
    for (auto&& member : type_map().bound_members) {
        auto [it, inserted] = dsl.property_value_type.emplace(member.name, member.value_type);
        if (!inserted && it->second != member.value_type) {
            it->second.clear();
        }
        if (!member.include.empty()) {
            dsl.includes.insert(member.include);
        }
    }

    write_dsl(out, dsl, emitted);

    // The schema, last, because it is the same vocabulary said per class and
    // wants the enumerators the tags carry, which are only just settled.
    //
    // Two passes over what the walk collected. An attached property joins the
    // class its setter takes rather than the class that declares the setter:
    // `row = 1` is written on the Button. And a class with nothing of its own
    // and nothing to inherit gets no struct at all -- a statics-only class
    // like ElementCompositionPreview would otherwise offer an empty list to
    // whoever typed its name -- which means a class below it has to inherit
    // from the nearest ancestor that does have one.
    for (auto&& [target, member] : attached_elsewhere) {
        auto const owner = std::find_if(schema.classes.begin(), schema.classes.end(),
                                        [&target](auto&& klass) { return klass.name == target; });
        if (owner != schema.classes.end()) {
            owner->members.push_back(std::move(member));
        }
    }

    // A bound-only member joins the class it is written on (types.json). The
    // class has to be in the closure: a member on a class nobody generates
    // would anchor to nothing, and is reported rather than dropped.
    for (auto&& bound : type_map().bound_members) {
        std::string const name = bound.class_name.substr(bound.class_name.rfind('.') + 1);
        auto const owner = std::find_if(schema.classes.begin(), schema.classes.end(),
                                        [&name](auto&& klass) { return klass.name == name; });
        if (owner == schema.classes.end()) {
            std::print("warning: bound member {}.{} names a class the profile does not generate\n",
                       bound.class_name, bound.name);
            continue;
        }
        Schema::Member member{Schema::Member::Kind::Bound, bound.name, member_name(bound.name),
                              bound.value_type};
        member.direction = bound.direction;
        owner->members.push_back(std::move(member));
    }

    std::map<std::string, std::string> schema_base;  // every class -> its base, before pruning
    for (auto&& klass : schema.classes) {
        schema_base.emplace(klass.name, klass.base);
    }

    std::set<std::string> kept;
    for (auto&& klass : schema.classes) {  // base before derived, so a base is decided already
        std::string base = klass.base;
        while (!base.empty() && !kept.count(base)) {
            auto const above = schema_base.find(base);
            base = above == schema_base.end() ? std::string{} : above->second;
        }
        klass.base = base;

        if (!klass.members.empty() || !klass.base.empty()) {
            kept.insert(klass.name);
        }
    }
    std::erase_if(schema.classes,
                  [&kept](auto&& klass) { return kept.count(klass.name) == 0; });

    write_schema(out, schema, dsl, emitted);
}

}  // namespace gen
