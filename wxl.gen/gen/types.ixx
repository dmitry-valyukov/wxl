module;

#include <map>
#include <set>
#include <string>
#include <string_view>

// How a WinRT type crosses wxl's public boundary.
//
// A generated member has two faces: the declaration in <Namespace>.h, which
// may name only wxl's own types, and the body in <Namespace>.cpp, which
// talks to the real cppwinrt projection. This maps one metadata type
// signature onto both, plus the two conversion expressions that join them.
//
// AI note: a type this doesn't understand yet is not an error -- `supported`
// comes back false with a reason, and the member naming it is skipped and
// counted. That is what keeps the generated tree compiling while the set of
// representable types grows (collections, delegates and generics are the
// ones still missing).

export module wxl.gen:types;

import :md;
export namespace gen {

// Where a generated type can be found: its flat wxl name and the public
// header declaring it. Built by the class writer, which is the only place
// that knows how classes are grouped into files.
struct TypeIndex {
    std::map<md::TypeDef, std::string> names;
    std::map<md::TypeDef, std::string> headers;
};

struct TypeUse {
    bool supported = false;
    std::string reason;  // why not, when unsupported

    std::string value_type;  // as returned:  "Object", "double", "wstring"
    std::string param_type;  // as taken in:  "Object const&", "double", "std::wstring_view"
    std::string winrt_type;  // the projection's own name, for the .cpp side

    // A wrapped class -- wxl::Object or a generated wrapper -- as opposed to
    // a primitive, an enum, a struct or a projected type. Only these can be
    // the element of a wxl::Collection, which reaches into the element's own
    // Impl for the WinRT type it stands for.
    bool is_wrapper = false;

    // wxl::Collection<E>, the one wrapper every vector-shaped WinRT
    // collection maps onto. `element_type` is E's public wxl name; the class
    // writer needs it to state the explicit instantiation, and the DSL writer
    // to give the property a subscript tag instead of an assignable one.
    bool is_collection = false;
    std::string element_type;

    // A type a string literal turns into by itself (see TypeMap::Projection
    // in profile.h). Such a type claims no unnamed-argument route: a bare
    // literal already means the class's own text.
    bool from_string = false;

    // The type can be taken in but not handed back: a delegate is built from
    // whatever a caller passes, while turning one that a call *returned* into
    // a wxl callable is a wrapper of its own that nothing needs yet.
    bool parameter_only = false;

    // Conversion expressions with '$' standing for the value being
    // converted; see substitute() below.
    std::string to_winrt;
    std::string from_winrt;

    std::set<std::string> public_includes;  // needed by <Namespace>.h
    std::set<std::string> impl_includes;    // needed by <Namespace>.cpp
};

// Replaces every '$' in `expression` with `argument`.
std::string substitute(std::string_view expression, std::string_view argument);

// The mapping for one signature. `index` decides which types are wrapped
// at all: anything it doesn't name and that isn't primitive, a string, an
// object or a projected type comes back unsupported.
TypeUse map_type(md::TypeSig const& sig, TypeIndex const& index);

// The same for a type named directly rather than reached through a
// signature -- which is how a profile's synthetic member names the type of
// its value, there being no signature in the metadata to read it from.
TypeUse map_type(md::TypeDef const& type, TypeIndex const& index);

// A type metadata carries as a signature element rather than as a TypeDef of
// its own, named the way the metadata spells it: "String". A synthetic member
// is the only thing that has to name one, since nothing resolves a TypeDef
// for it.
TypeUse map_element_type(std::string_view metadata_name);

// Whether `type` is a class wxl represents as a wxl::Collection rather than
// as a wrapper of its own -- UIElementCollection, ItemCollection and their
// kind, which exist in metadata only to name an IVector<T>. The class writer
// asks this to leave such a class out of the generated hierarchy entirely.
bool is_collection_class(md::TypeDef const& type);

// Whether a WinRT struct is the ABI struct field for field, and can
// therefore be generated at all: wxl writes its own copy from the same
// metadata and crosses with a bit_cast. A struct carrying a String is not --
// the projection holds an owning winrt::hstring there -- and is dropped
// instead, along with every member naming it.
bool mirrors_abi(md::TypeDef const& type);

}  // namespace gen
