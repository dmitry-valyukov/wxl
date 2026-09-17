module;

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Input side of winui-srcgen: profiles say *what* to generate -- which
// NuGet packages' metadata to read, which types are the roots of the
// dependency walk, and which of their members take part in it. See
// the profile documentation. Profiles are plain JSON with comments,
// read by wxl.json; nothing here exposes the reader, so only
// profile.cpp imports it.

export module wxl.gen:profile;

export {

// Which members of a type participate in generation -- and therefore in
// the dependency walk, since a member that isn't generated can't drag its
// parameter/return types into the closure.
struct MemberFilter {
    enum class Kind {
        All,    // profile listed the type without member constraints
        None,   // type never listed; only reached through the walk
        Allow,  // "members": [...]
        Deny,   // "excludeMembers": [...]
    };

    Kind kind = Kind::None;
    std::set<std::string> names;

    inline static MemberFilter all() { return {Kind::All, {}}; }
    inline static MemberFilter none() { return {Kind::None, {}}; }
    inline static MemberFilter allow(std::set<std::string> names) {
        return {Kind::Allow, std::move(names)};
    }
    inline static MemberFilter deny(std::set<std::string> names) {
        return {Kind::Deny, std::move(names)};
    }

    bool allows(std::string_view name) const;

    // Union of two member surfaces. The same type is routinely reached
    // from several profiles (or from a profile and the one it extends) --
    // it's still generated once, with everything any of them asked for.
    void merge(MemberFilter const& other);

    friend bool operator==(MemberFilter const&, MemberFilter const&) = default;
};

// A property wxl gives a type although WinRT has none: the minimum size of
// a window, which the framework keeps on the window's presenter and nowhere
// a metadata walk would find it. The generator emits the member and its tag
// exactly like a real one; only the body differs, being a call to a
// hand-written function in wxl.ui.
//
// Four things say it, and the type it belongs to is the profile entry it is
// written under: the property's name, the type of its value, the function
// behind it, and the header declaring that function.
struct SyntheticMember {
    std::string name;      // "MinSize", spelled as metadata would
    std::string type;      // the value's type in metadata form, "Windows.Graphics.SizeInt32"
    std::string function;  // "impl::set_minimum_size", under namespace wxl
    std::string include;   // where it is declared, as the generated .cpp writes it

    // The alternative to `type`, for a value wxl owns rather than metadata.
    // A synthetic member is wxl's own invention, so its value may be too --
    // FluentSymbol, the enumeration of icon-font glyph names, is not a WinRT
    // type and there is nothing in the metadata to resolve. Spelled the way
    // C++ spells it under namespace wxl, with the public header declaring
    // it; taken by value, so this is for enums and other small types.
    //
    // Exactly one of `type` and `cpp_type` is given.
    std::string cpp_type;     // "FluentSymbol"
    std::string cpp_include;  // "FluentSymbol.h"
};

// One NuGet package a profile draws metadata from. `metadata` names the
// .winmd files to read out of `metadata_dir` inside the package; when it's
// empty, every .winmd found there is read.
//
// A package the SDK release ships names no version: the version comes from
// that release, which declares one for every package in it -- see
// `windows_app_sdk` below. A package from outside the SDK has no such
// declaration to read, so it names its own version and must.
struct PackageRef {
    std::string id;
    std::vector<std::string> metadata;

    // Files to read out of the package that are not metadata, named
    // relative to the package root -- the XAML resource dictionaries the
    // framework's named styles are declared in. Metadata says nothing about
    // those: a resource has a name and a target type, not a type of its own.
    std::vector<std::string> resources;

    std::string version;       // empty for a package the SDK release ships
    std::string metadata_dir;  // empty means "metadata", the layout the SDK uses
};

// One profile file, as loaded -- `extends` is not yet resolved here.
struct Profile {
    std::string name;
    std::string description;
    std::filesystem::path source;
    std::vector<std::string> extends;

    // The Microsoft.WindowsAppSDK release every package below is taken
    // from. That package's nuspec declares the exact version of each
    // sub-package shipped with it, so naming the release once is what
    // keeps the set consistent -- a hand-picked version per package drifts
    // into combinations nobody ships. Empty when a profile leaves the
    // choice to the one it extends.
    std::string windows_app_sdk;

    std::vector<PackageRef> packages;
    std::map<std::string, MemberFilter> types;  // "Microsoft.UI.Xaml.Controls.Button" -> filter
    std::map<std::string, std::vector<SyntheticMember>> synthetic;  // by the same type name
    MemberFilter discovered = MemberFilter::none();
    bool windows_metadata = false;
};

// What wxl implements on its own side of the boundary: the base types it
// supplies by hand, the roots every run gets, the WinRT types it already has
// something better than, and the properties that carry a tag of their own.
//
// This is not a profile. A profile says what to generate; this says what wxl
// itself is, which is the same for every profile -- so it lives in one shared
// file (profiles/types.json) that a run reads once.
struct TypeMap {
    // A WinRT type wxl does not wrap because it already owns an equivalent.
    struct Projection {
        std::string metadata_name;  // "Windows.Foundation.Size"
        std::string cpp_name;       // "Size", under namespace wxl
        std::string include;        // header defining it, as the output writes it

        // A type a string literal turns into by itself -- a Uri, a font, an
        // image source. Such a type claims no unnamed-argument route: a bare
        // literal already means the class's own text.
        bool from_string = false;
    };

    // A property whose own type cannot identify it, and the tag that can.
    struct Tag {
        std::string property_name;  // "Margin", as the metadata spells it
        std::string name;           // "Margin", the tag type under namespace wxl
        std::string value_type;     // "Thickness" -- what the tag is a tag over
        std::string include;        // header defining that value type
    };

    std::set<std::string> given_from_above;
    std::set<std::string> implicit_roots;
    std::vector<Projection> projections;
    std::vector<Tag> tags;
};

// Reads profiles/types.json. Throws std::runtime_error naming the file on
// malformed JSON or an entry missing a required key.
TypeMap load_type_map(std::filesystem::path const& path);

// Installs the map for the run. Everything that asks about a type -- the
// walk's boundary, the projections, the tags -- reads it back through
// type_map(), so it is set once in main() rather than threaded through
// every call.
void use_type_map(TypeMap map);
TypeMap const& type_map();

// Several profiles (plus everything they reference, transitively) merged
// into the single input the crawler actually walks.
struct ProfileSet {
    std::vector<std::string> loaded;               // profile names, in load order
    std::vector<std::filesystem::path> metadata;   // .winmd files to open
    std::vector<std::filesystem::path> resources;  // XAML dictionaries to read
    std::map<std::string, MemberFilter> types;     // roots of the walk
    std::map<std::string, std::vector<SyntheticMember>> synthetic;  // properties wxl adds
    MemberFilter discovered = MemberFilter::none();
};

// Reads a single profile file. Throws std::runtime_error with the file
// name on malformed JSON, on an unknown key shape, or on a type that
// carries both "members" and "excludeMembers" (mutually exclusive by
// design -- a member named in both would be a straight contradiction).
Profile load_profile(std::filesystem::path const& path);

// Loads every named profile plus, transitively, the profiles they
// `extends`, and merges them. A profile reference is either a path or a
// bare name resolved as `<dir of the referencing profile>/<name>.json`.
// Diamonds are loaded once; cycles are an error.
ProfileSet resolve_profiles(std::vector<std::filesystem::path> const& paths,
                            std::filesystem::path const& nuget_root);

// %NUGET_PACKAGES%, or %USERPROFILE%\.nuget\packages when unset.
std::filesystem::path default_nuget_root();

// One named glyph of the icon font: the name a `FluentSymbol` is written
// under, and the code point behind it.
struct Symbol {
    std::string name;
    uint32_t code = 0;
};

// Reads profiles/fluent-symbols.json, sorted by code point. Not a profile
// and not metadata: the font carries no glyph names of its own, so these are
// typed in from the documentation once and read by every run -- the same
// standing as types.json. Throws std::runtime_error naming the file on
// malformed JSON, on a code point that is not four hex digits, and on a name
// or a code point used twice.
std::vector<Symbol> load_symbol_names(std::filesystem::path const& path);

// Installs the names for the run, the way use_type_map() installs the map.
void use_symbol_names(std::vector<Symbol> names);
std::vector<Symbol> const& symbol_names();

}  // export
