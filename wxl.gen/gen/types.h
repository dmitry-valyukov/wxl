#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>

#include "md.h"

// How a WinRT type crosses wxl's public boundary.
//
// A generated member has two faces: the declaration in <Namespace>.h, which
// may name only wxl's own types, and the body in <Namespace>.cpp, which
// talks to the real cppwinrt projection. This maps one metadata type
// signature onto both, plus the two conversion expressions that join them.
//
// A type this does not understand is not an error: `supported` comes back
// false with a reason, and the member naming it is skipped and counted. That
// is what keeps the generated tree compiling while the set of representable
// types grows.

namespace gen {

// Where a generated type can be found: its flat wxl name and the public
// header declaring it. Built by the class analysis, which is where classes
// are grouped into files.
struct type_index {
    // What wxl owns itself: a projected type maps onto wxl's equivalent, not
    // onto a wrapper, whatever the names below say.
    type_map const& types;

    std::map<md::TypeDef, std::string> names;
    std::map<md::TypeDef, std::string> headers;

    // Wrapped classes that declare no default interface: there is nothing to
    // hand such an object over to a call as, so a member naming one is
    // skipped and counted rather than emitted as a call that cannot compile.
    std::set<md::TypeDef> without_default_interface;
};

struct type_use {
    bool supported = false;
    std::string reason;  // why not, when unsupported

    std::string value_type;  // as returned:  "Object", "double", "hstring"
    std::string param_type;  // as taken in:  "Object const&", "double", "hstring_param const&"
    std::string winrt_type;  // the projection's own name, for the .cpp side

    // A wrapped class -- wxl::Object or a generated wrapper -- as opposed to
    // a primitive, an enum, a struct or a projected type. Only these can be
    // the element of a wxl::Collection, which reaches into the element's own
    // Impl for the WinRT type it stands for.
    bool is_wrapper = false;

    // wxl::Collection<E>, the one wrapper every vector-shaped WinRT
    // collection maps onto. The DSL writer gives such a property a subscript
    // tag instead of an assignable one.
    bool is_collection = false;

    // E of a Collection<E> or a VectorView<E>, when E is a wrapped class: the
    // name a declaration announces instead of including its header, and the
    // type a collection tag takes. Empty for any other type.
    std::string element_type;

    // The specializations of wxl's own templates the type names --
    // "Collection<UIElement>", "VectorView<hstring>". Their bodies are not in
    // a public header, so each one is stated (`extern template class`) by the
    // header handing it out and defined (`template class`) by exactly one
    // .cpp.
    std::set<std::string> instantiations;

    // A type a string literal turns into by itself (see type_map::projection
    // in profile.h). Such a type claims no unnamed-argument route: a bare
    // literal already means the class's own text.
    bool from_string = false;

    // The type can be taken in but not handed back: a delegate is built from
    // whatever a caller passes, while turning one that a call *returned* into
    // a wxl callable is a wrapper of its own that nothing needs yet.
    bool parameter_only = false;

    // The opposite: an asynchronous operation (wxl::Operation) is what a call hands back and
    // awaits; there is nothing a caller could give in its place.
    bool result_only = false;

    // Conversion expressions with '$' standing for the value being
    // converted; see substitute() below.
    std::string to_winrt;
    std::string from_winrt;

    std::set<std::string> public_includes;  // needed by <Namespace>.h
    std::set<std::string> impl_includes;    // needed by <Namespace>.cpp
};

// Replaces every '$' in `expression` with `argument`.
std::string substitute(std::string_view expression, std::string_view argument);

// The C++ type a primitive ElementType is on both sides of the boundary --
// "int32_t", "char16_t", "double" -- or nullptr for anything else: an enum, a
// struct or a class is a TypeDefOrRef in the signature, not an ElementType.
char const* primitive_name(md::ElementType element);

// The mapping for one signature. `index` decides which types are wrapped
// at all: anything it doesn't name and that isn't primitive, a string, an
// object or a projected type comes back unsupported.
type_use map_type(md::TypeSig const& sig, type_index const& index);

// The same for a type named directly rather than reached through a
// signature -- which is how a profile's synthetic member names the type of
// its value, there being no signature in the metadata to read it from.
type_use map_type(md::TypeDef const& type, type_index const& index);

// A type metadata carries as a signature element rather than as a TypeDef of
// its own, named the way the metadata spells it: "String". A synthetic member
// is the only thing that has to name one, since nothing resolves a TypeDef
// for it.
type_use map_element_type(std::string_view metadata_name);

// Whether `type` is a class wxl represents as a wxl::Collection rather than
// as a wrapper of its own -- UIElementCollection, ItemCollection and their
// kind, which exist in metadata only to name an IVector<T>. The class
// analysis asks this to leave such a class out of the generated hierarchy
// entirely.
bool is_collection_class(md::TypeDef const& type);

// Whether a WinRT struct is the ABI struct field for field, and can
// therefore be generated at all: wxl writes its own copy from the same
// metadata and crosses with a bit_cast. A struct carrying a String is not --
// the projection holds an owning winrt::hstring there -- and is dropped
// instead, along with every member naming it.
bool mirrors_abi(md::TypeDef const& type, type_map const& types);

}  // namespace gen
