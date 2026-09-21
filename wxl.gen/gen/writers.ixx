module;

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// In the global module fragment on purpose: the XML reader is an ordinary
// header shared with non-module code, so the type must stay a global-module
// entity -- a second declaration inside the module would be a different
// entity of the same name. Callers include it the same way.
#include "../xml_input.h"

// One entry point per kind of generated artefact; each lives in its own
// gen/*.cpp so the file you open matches the output you're changing.
//
// AI note: adding a new artefact = new gen/<thing>.cpp + one declaration
// here + one call in write_all() (generate.cpp). A writer takes where to
// write, what to write, and the record of what has been written (which the
// CMakeLists writer consumes) -- and, where one writer's output is another's
// input, that too, in between.

export module wxl.gen:writers;

import :emit;
import :generate;
import :types;
import :md;
export namespace gen {

// PropertyKey.h / EventKey.h -- the flat key enums the builder syntax
// uses for named-argument-style assignment.
void write_key_enums(Output const& out, Model const& model, Emitted& emitted);

// One <Namespace>.Enums.h / <Namespace>.Structs.h per source WinRT
// namespace, plus the Enums.h / Structs.h umbrella headers.
void write_enums_and_structs(Output const& out, Model const& model, Emitted& emitted);

// Structs.impl.h -- the private to_winrt/from_winrt overloads for every
// generated struct, added to the same overload sets impl/conversions.h
// opens. Written by gen/enums_structs.cpp alongside the structs themselves.

// What the class writer worked out and the writers after it need: where
// every generated type is declared, and which Collection specializations it
// has already defined. Both are settled only once classes are grouped into
// files, which is the class writer's own job.
struct ClassOutput {
    TypeIndex index;
    std::set<std::string> collection_elements;
};

// One <Namespace>.EventArgs.h per source namespace -- plus a .cpp beside it
// wherever an args class has members -- and the EventArgs.h umbrella header.
// Runs after the class writer, whose index says what an args member's
// signature may name.
void write_event_args(Output const& out, Model const& model, ClassOutput const& classes,
                      Emitted& emitted);

// Interfaces.h -- reference list only, no C++ declarations yet.
void write_interfaces(Output const& out, Model const& model, Emitted& emitted);

// <Class>.h / <Class>.impl.h / <Class>.cpp for every wrapped class: the
// public wrapper hierarchy and the matching `Impl` chain.
void write_classes(Output const& out, Model const& model, Emitted& emitted,
                   ClassOutput& produced);

// collections.h -- the STA-allocator-backed container aliases generated
// code uses in place of the std ones.
void write_collections(Output const& out, Emitted& emitted);

// styles.h / styles.cpp -- the framework's named styles as a path,
// `styles.textBlock.title`. Read out of the XAML resource dictionaries the
// profile names, not out of metadata: a resource has a key and a target
// type, and no type of its own for the walk to find.
void write_styles(Output const& out, Model const& model,
                  std::vector<DictionaryResource> const& resources, Emitted& emitted);

// brushes.h / brush_names.h -- the framework's named brushes as a path,
// `brushes.Card.BackgroundFillColorDefault`. From the same dictionaries as
// the styles, and theme-resolved at lookup, which is what a hard-coded
// colour in application code can never be. The model comes in because a brush
// key names no target of its own: which control a key belongs to is guessed,
// and a type the profile generated is the stronger of the two guesses.
void write_brushes(Output const& out, Model const& model,
                   std::vector<DictionaryResource> const& resources, Emitted& emitted);

// FluentSymbol.h -- the named glyphs of the icon font as an enumeration,
// from profiles/fluent-symbols.json. Not from metadata and not from the
// dictionaries: the font carries no glyph names, so the list is data typed
// in once and shared by every profile, the way types.json is.
void write_symbols(Output const& out, Emitted& emitted);

// aliases.txt -- a reference sheet, not code: every StaticResource alias of
// the theme dictionaries and where it forwards, per theme where the themes
// disagree. How a developer learns which base resource dresses which part
// of which control, without reading a 3 MB dictionary.
void write_alias_index(Output const& out, std::vector<DictionaryResource> const& resources,
                       Emitted& emitted);

// What the builder syntax needs to know about the members that were
// generated. Collected by the class writer, which is the only place that
// has read the signatures.
struct Dsl {
    // Property name -> the wxl type of its value, empty when different
    // classes declare that name with different types (the braced form
    // `Margin = {20}` needs one definite type; the deduced form does not).
    std::map<std::string, std::string> property_value_type;
    std::set<std::string> events;

    // Properties whose value is a class the tag can also build from braces:
    // `titleBar = { leftHeader = ..., content = ... }`. A setter method with a
    // narrowed type is the one kind so far (see profile.h).
    std::set<std::string> braced;

    // Collection-valued property name -> the wxl type of its elements. These
    // get a subscript tag (`Children[a, b, c]`) rather than an assignable
    // one, and a CollectionSetter that receives the whole run at once.
    std::map<std::string, std::string> collection_element;

    // Attached properties, by the name the *child* writes: `row = 1` on a
    // Button reaches Grid::setRow(button, 1). The tag is a property tag like
    // any other; only the setter behind it belongs to a different class than
    // the object it is written on.
    struct Attached {
        std::string owner;       // "Grid"
        std::string setter;      // "setRow"
        std::string value_type;  // "int32_t", for the braced form
    };
    std::map<std::string, Attached> attached;

    // Enum type (spelled as the value types above spell it, `wxl::Orientation`)
    // -> its enumerators, each as the member name a tag surfaces it under and
    // the enumerator's own name. A tag of such a type carries the values it
    // accepts, so the DSL writes `orientation.horizontal` where the enum type
    // would otherwise be named in full.
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> enumerators;

    std::set<std::string> includes;  // public headers the value types live in
};

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
struct Schema {
    struct Member {
        enum class Kind { Property, Collection, Event };

        Kind kind = Kind::Property;
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

        // The tag builds its value from braces too (see Dsl::braced).
        bool braced = false;
    };

    struct Class {
        std::string name;  // "Button"
        // The class it derives from, or empty where that base has no schema
        // of its own (Object and DependencyObject are hand-written).
        std::string base;
        // The generated header declaring the wrapper. The schema names every
        // class it covers -- each anchor carries its owner -- so it needs
        // them all, which is more than Members.h needs: that one names only
        // the types properties are *valued* with.
        std::string header;
        std::vector<Member> members;
    };

    // Base before derived, the order the class writer emits in, so a schema
    // struct can simply name its base.
    std::vector<Class> classes;
};

// Tags.h -- the tag types an unnamed argument uses for properties whose own
// type cannot identify them: `Margin{20}`, where the property itself still
// takes the plain Thickness the metadata declares.
void write_tags(Output const& out, Model const& model, Emitted& emitted);

// Members.h -- the property and event tags the DSL writes (`Content`,
// `OnClick`) and the per-key dispatch behind them. One entry per member,
// none of it per class: the dispatch is a template on the object.
void write_dsl(Output const& out, Dsl const& dsl, Emitted& emitted);

// schema.h -- the per-class vocabulary described above -- and, beside it,
// schema_surface.cpp: one compile-only line per element of it, which is what
// keeps the schema honest. Every new type and every new member lands in both
// files by the same run, so coverage cannot drift from the surface. The Dsl
// comes in for the enumerators a tag of enum type carries; everything else
// the schema needs is in the Schema itself.
void write_schema(Output const& out, Schema const& schema, Dsl const& dsl, Emitted& emitted);

// CMakeLists.txt adding everything above to the consuming target.
void write_cmake_lists(Output const& out, Emitted const& emitted);

// An enum's values: the member name each is surfaced under and the
// enumerator's own name. Defined in gen/enums_structs.cpp, which is where
// what counts as a value of an enum is decided.
std::vector<std::pair<std::string, std::string>> enum_members(md::TypeDef const& type);

// WinRT has no formal "EventArgs" category -- it's a naming convention.
// Defined in gen/event_args.cpp; gen/classes.cpp uses it to leave those
// classes to the EventArgs writer.
bool is_event_args_class(md::TypeDef const& type);

}  // namespace gen
