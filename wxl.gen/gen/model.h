#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gen/members.h"
#include "gen/types.h"
#include "md.h"
#include "xml_input.h"

// What the generator writes, worked out before anything is written.
//
// The walk (wxl.gen.common) hands over a `closure`: which types and members take
// part. Turning that into what each file says -- which file a class lives in,
// what its members are as C++, which struct depends on which, what the builder
// syntax's vocabulary is -- is analysis, and it happens once, in analyze(),
// into the `model` below. The writers in gen/*.cpp then read their part of the
// model and nothing else: no metadata, no profile, and none of each other's
// results. Everything here is text the output is made of.
//
// Analysis prints nothing. What a run reports -- members skipped, routes
// withheld, structs dropped -- is collected here too and printed by the
// writer of the file it concerns, so the report keeps the order the files are
// written in.

namespace gen {

// ---- Tags.h ----

// One tag alias: `using Margin = TaggedValue<Thickness, PropertyKey::Margin>;`.
struct tag_alias {
    std::string name;        // "Margin"
    std::string value_type;  // "Thickness"
    std::string property;    // "Margin"
};

// ---- <Namespace>.Enums.h ----

struct enum_value {
    std::string name;   // as metadata spells it: "Horizontal"
    std::string value;  // as it is written: "1"
};

struct enum_info {
    std::string name;
    std::string underlying;  // "int32_t", or "uint32_t" for a flags enum
    // The enumerators the profiles kept, each with its value: with some of
    // them left out, the rest must not move.
    std::vector<enum_value> values;
};

struct enum_file {
    std::string ns;
    std::vector<enum_info> enums;
};

// ---- <Namespace>.Structs.h and Structs.impl.h ----

struct struct_field {
    std::string name;      // camelCase: a field is a member
    std::string cpp_type;  // "int32_t", "Point", "Visibility"
};

struct struct_info {
    std::string name;               // "GridLength"
    std::string winrt_name;         // "winrt::Microsoft::UI::Xaml::GridLength"
    std::string projection_header;  // "<winrt/Microsoft.UI.Xaml.h>"
    std::vector<struct_field> fields;

    // What proves the struct is the ABI struct, which is what lets it cross
    // whole as a bit_cast. One distinct value per field, so that the assert
    // catches a reordering and not merely a resize: the WinRT struct is built
    // from `probe_init`, bit-cast across and checked by `probe_check` under
    // the wxl names. A field that is neither a primitive nor an enum has no
    // such literal, and then `probe_complete` is false and the assert falls
    // back to size and alignment alone.
    bool probe_complete = true;
    std::string probe_init;
    std::string probe_check;
};

struct struct_file {
    std::string ns;
    std::vector<size_t> structs;  // into model::structs, in that order
    // Where the fields' types live: an enum always in its namespace's
    // Enums.h, a struct of another namespace in its Structs.h, a projected
    // type in the header wxl keeps it in. A struct of the same namespace
    // needs nothing -- it is earlier in the same file.
    std::set<std::string> includes;
};

// ---- <Namespace>.h / .impl.h / .cpp ----

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
enum class construction_t
{
    None,
    PublicActivation,
    PublicFactory,
    PublicComposition,
    ProtectedOnly
};

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

// An interface a level holds a field for, beside its default one.
struct interface_field {
    std::string winrt_type;  // "winrt::Microsoft::UI::Xaml::Controls::IControl2"
    std::string name;        // "control2_"
};

// An interface the static members of a class live on.
struct statics_interface {
    std::string winrt_type;         // "winrt::Microsoft::UI::Xaml::Controls::IGridStatics"
    std::string projection_header;  // "<winrt/Microsoft.UI.Xaml.Controls.h>"
};

struct class_info {
    std::string name;           // the flat wxl name: "Button", "TextRange"
    std::string metadata_name;  // "Microsoft.UI.Xaml.Controls.Button"
    std::string winrt_name;     // "winrt::Microsoft::UI::Xaml::Controls::Button"
    std::string base_name;
    std::string base_namespace;  // empty when the base is given from above
    construction_t construction = construction_t::None;
    // The factory interface a composable class is created through, in
    // metadata form; empty for every other class.
    std::string composable_factory;

    // The projection headers the private side needs for this level: the
    // class's own namespace and that of every interface it holds.
    std::set<std::string> projection_headers;

    // The interfaces that survived the filter, other than the default one,
    // one Impl field each.
    std::vector<interface_field> fields;
    std::vector<statics_interface> statics;
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

    // The Impl field holding the interface this level *is* on the ABI. That
    // field is declared as the projection class, so handing the object to a
    // WinRT call that wants the class costs nothing; every other field stays
    // the interface it stands for.
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

// One file group: the classes of one namespace, or of several that derive
// from each other, written base before derived.
struct class_group {
    std::string name;  // "Microsoft.UI.Xaml.Controls"
    std::vector<class_info> classes;
    // The Collection specializations the group's classes name, and those of
    // them it is the one to define: an explicit instantiation definition may
    // exist in exactly one translation unit, so the first group naming an
    // element -- in the order the files are written -- takes it.
    std::set<std::string> collections_used;
    std::set<std::string> collections_defined;
};

// ---- Members.h ----

// What the builder syntax needs to know about the members that were
// generated, and about the members of the classes wxl writes by hand.
struct dsl {
    // Property name -> the wxl type of its value, empty when different
    // classes declare that name with different types (the braced form
    // `margin = {20}` needs one definite type; the deduced form does not).
    std::map<std::string, std::string> property_value_type;
    std::set<std::string> events;

    // Properties whose value is a class the tag can also build from braces:
    // `titleBar = { leftHeader = ..., content = ... }`. A setter method with a
    // narrowed type is the one kind so far (see profile.h).
    std::set<std::string> braced;

    // Collection-valued property name -> the wxl type of its elements. These
    // get a subscript tag (`children[a, b, c]`) rather than an assignable
    // one, and a CollectionSetter that receives the whole run at once.
    std::map<std::string, std::string> collection_element;

    // Attached properties, by the name the *child* writes: `row = 1` on a
    // Button reaches Grid::setRow(button, 1). The tag is a property tag like
    // any other; only the setter behind it belongs to a different class than
    // the object it is written on.
    struct attached_t {
        std::string owner;       // "Grid"
        std::string setter;      // "setRow"
        std::string value_type;  // "int32_t", for the braced form
    };
    std::map<std::string, attached_t> attached;

    // Enum type (spelled as the value types above spell it, `Orientation`)
    // -> its enumerators, each as the member name the syntax surfaces it under
    // and the enumerator's own name. A property over such a type carries the
    // values it accepts (impl::enum_values), so the DSL writes
    // `orientation.horizontal` where the enum type would otherwise be named in
    // full.
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> enumerators;

    std::set<std::string> includes;  // public headers the value types live in
};

// ---- schema.h ----

// The same vocabulary as Members.h, but reached through the class that
// declares it -- `schema::Button::content` beside the bare `dsl::content`.
//
// It exists for one reason, and the reason is not the compiler: typing
// `schema::` offers the classes and `schema::Button::` offers exactly what a
// Button takes, which is how a name is *found* rather than remembered. Two
// things follow from being generated per class rather than per member: the
// anchor knows its owner, so writing one class's member on another is
// refused by name instead of deep inside a template, and it knows the type
// that class declares the property with -- which the flat vocabulary has to
// give up wherever two classes disagree.
struct schema {
    struct member {
        // Bound: a property that exists on this class as a binding target
        // alone -- no setter, a hand-written pair in impl/binding.h reads it
        // off the control -- so its anchor takes Bind forms and nothing else.
        enum class kind_t { Property, Collection, Event, Bound };

        kind_t kind = kind_t::Property;
        std::string key;   // the PropertyKey / EventKey enumerator
        std::string name;  // as the DSL spells it: camelCase
        // A property's value type, a collection's element type; empty for an
        // event and for a property no single type can be named.
        std::string type;
        // Set for an attached property, whose setter belongs to another
        // class: the member is written on this one, and `type` is what it
        // takes. Nothing else needs it -- the owner is the class itself.
        bool attached = false;

        // False where the class declares the same property with more than
        // one type -- SymbolIcon takes both a Symbol and a FluentSymbol. The
        // anchor then carries no type, exactly as the flat tag does, and
        // keeps only the deduced assignment; `type` stays the first of them,
        // because the test still has to hand it something it accepts.
        bool single_type = true;

        // The tag builds its value from braces too (see dsl::braced).
        bool braced = false;

        // Bound only: "input", "output" or "both" -- which Bind form the
        // pair takes, and so which one the test line writes.
        std::string direction;
    };

    struct class_t {
        std::string name;  // "Button"
        // The class it derives from, or empty where that base has no schema
        // of its own (Object and DependencyObject are hand-written).
        std::string base;
        // The generated header declaring the wrapper. The schema names every
        // class it covers -- each anchor carries its owner -- so it needs
        // them all, which is more than Members.h needs: that one names only
        // the types properties are *valued* with.
        std::string header;
        std::vector<member> members;
    };

    // Base before derived, the order the classes are written in, so a schema
    // struct can simply name its base.
    std::vector<class_t> classes;
};

// ---- <Namespace>.EventArgs.h / .cpp ----

// One member of an args view, together with the interface its body has to
// query off abi_ -- which a class wrapper reads from a field instead, so
// member_info has nowhere to carry it.
struct args_member {
    member_info info;
    std::string winrt_interface;  // "winrt::Microsoft::UI::Xaml::IRoutedEventArgs"
    std::string winrt_header;     // the projection header declaring it
};

struct event_args_info {
    std::string name;
    std::string base;  // "EventArgsBase" for a root
    std::vector<args_member> members;
};

struct event_args_file {
    std::string ns;
    std::vector<event_args_info> classes;  // base before derived

    // What the header's signatures need: an include for every type the
    // mapping named a header for, a forward declaration for every wrapper
    // whose header the args header must not include (a class header already
    // includes the args headers of its events), and an extern template for
    // every Collection specialization.
    std::set<std::string> includes;
    std::set<std::string> forwards;
    std::set<std::string> collections;

    std::set<std::string> source_includes;
    // The Collection specializations no class file defines and this one is
    // the first to name.
    std::set<std::string> collection_definitions;
};

}  // namespace gen

// Everything the writers write, in the order write_all() writes it.
struct model {
    // PropertyKey.h / EventKey.h
    std::set<std::string> property_names;
    std::set<std::string> event_names;

    // Tags.h
    std::vector<gen::tag_alias> tags;
    std::set<std::string> tag_includes;

    // Enums and structs. `structs` is in dependency order -- a struct after
    // the structs its fields hold -- which is the order Structs.impl.h takes
    // them in; each Structs.h takes its own namespace's in the same order.
    std::vector<gen::enum_file> enum_files;
    std::vector<gen::struct_info> structs;
    std::vector<gen::struct_file> struct_files;
    std::vector<std::string> dropped_structs;  // "Ns.Name", reported

    // Class wrappers: the file groups in the order they are written, the
    // group of every namespace, and what the analysis reports.
    std::vector<gen::class_group> class_groups;
    std::map<std::string, std::string> group_of;
    std::vector<std::string> skipped_members;
    std::vector<std::string> content_collections;
    std::vector<std::string> withheld_routes;
    std::vector<std::string> class_warnings;  // to stderr, ahead of the rest

    // Members.h and schema.h
    gen::dsl dsl;
    gen::schema schema;
    std::vector<std::string> unplaced_bound_members;  // "Ns.Class.Member", reported

    // EventArgs
    std::vector<gen::event_args_file> event_args_files;
    std::vector<std::string> skipped_args_members;

    // styles.h, brushes.h, aliases.txt: the resources of the dictionaries the
    // profiles name, the bare names of the classes the run knows, the style
    // filter of every one of them the profiles named, and the brush filter.
    std::vector<dictionary_resource> resources;
    std::set<std::string> class_names;
    std::map<std::string, member_filter> style_filters;
    member_filter brushes = member_filter::all();

    // FluentSymbol.h, by code point; empty when the run has no symbol table.
    std::vector<symbol> symbols;
};

// Works out the model from what the walk found, what the profiles name and
// what wxl owns itself (the type map, the icon font's names). Reads the
// metadata the closure points into and the profiles' resource dictionaries;
// writes nothing and prints nothing.
model analyze(closure const& cl, profile_set const& profiles, type_map const& types,
              std::vector<symbol> symbols);

namespace gen {

// The closure's types by category, each in the closure's own dependency
// order -- sources of dependencies first, the types built on them after --
// so analysis keeps that order rather than re-sorting by name.
struct type_kinds {
    std::vector<md::TypeDef> enums;
    std::vector<md::TypeDef> structs;
    std::vector<md::TypeDef> classes;
    // The interfaces a profile names directly. They are wrapped like classes
    // -- an interface listed by name is one some member hands back, and a
    // wrapper is the only way to hold what came back.
    std::vector<md::TypeDef> listed_interfaces;
};

// The analysis of each part, defined beside the writer of that part, called
// by analyze() in this order: what a later one needs, an earlier one settled.
void analyze_tags(type_map const& types, model& m);
void analyze_enums_and_structs(type_kinds const& kinds, closure const& cl, type_map const& types,
                               model& m);
// Classes, and with them the builder syntax's vocabulary and the schema --
// only the class analysis has read the signatures. Hands back where every
// wrapped type is declared, which the args views' signatures need.
type_index analyze_classes(type_kinds const& kinds, closure const& cl, type_map const& types,
                           model& m);
void analyze_event_args(type_kinds const& kinds, closure const& cl, type_index const& index,
                        model& m);

}  // namespace gen
