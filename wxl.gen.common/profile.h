#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Input side of winui-srcgen: profiles say *what* to generate -- which
// NuGet packages' metadata to read, which types are the roots of the
// dependency walk, and which of their members take part in it. See
// the profile documentation. A profile is a file <name>.winxl-profile, JSON
// with comments, read by wxl::json; nothing here exposes the reader, so only
// profile.cpp imports it.
//
// Which version of a package is read, and where it is, a profile does not
// say: the NuGet restore of the build resolves it once, and the configure
// writes the result into a list (restored_packages below) that every run
// takes as its input.

// Which members of a type participate in generation -- and therefore in
// the dependency walk, since a member that isn't generated can't drag its
// parameter/return types into the closure.
struct member_filter {
    enum class kind_t {
        All,    // profile listed the type without member constraints
        None,   // type never listed; only reached through the walk
        Allow,  // "members": [...]
        Deny,   // "excludeMembers": [...]
    };

    kind_t kind = kind_t::None;
    std::set<std::string> names;

    inline static member_filter all() { return {kind_t::All, {}}; }
    inline static member_filter none() { return {kind_t::None, {}}; }
    inline static member_filter allow(std::set<std::string> names) {
        return {kind_t::Allow, std::move(names)};
    }
    inline static member_filter deny(std::set<std::string> names) {
        return {kind_t::Deny, std::move(names)};
    }

    bool allows(std::string_view name) const;

    // Union of two member surfaces. The same type is routinely reached
    // from several profiles (or from a profile and the one it extends) --
    // it's still generated once, with everything any of them asked for.
    void merge(member_filter const& other);

    friend bool operator==(member_filter const&, member_filter const&) = default;
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
struct synthetic_member {
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

    // The value is also written unnamed in the class's braces. A property claims that route
    // by itself only when its type carries its own name (Margin); a wxl type that cannot --
    // RowLengths behind RowDefinitions, whose name is taken by the value written --
    // says so here.
    bool positional = false;
};

// A method WinRT declares that is a setter in all but name: one argument, no
// result -- Window.SetTitleBar(UIElement). Only a property gets a tag, so such a
// method can be called and never written in braces; a profile that names it
// here gives it one, under the name without "Set" (`titleBar = ...`), whose
// setter calls the method. The method itself stays as it is.
//
// `type` narrows the value to a class of the metadata more specific than the
// parameter -- TitleBar where the method takes any UIElement -- and with a
// definite class to build, the tag also takes the braces that build one:
// `titleBar = { leftHeader = ..., content = ... }`. Empty keeps the parameter's
// own type and the ordinary assignment.
struct setter_method {
    std::string method;  // "SetTitleBar", spelled as metadata spells it
    std::string type;    // "Microsoft.UI.Xaml.Controls.TitleBar", or empty
};

// A wrapped class that a string builds: the abstract Geometry, which is made from
// the path language of XAML by a converter and by nothing else. The class gets an
// implicit constructor from text, defined with the function named here -- one that
// takes the text and returns the projection's own object.
struct from_text_t {
    std::string function;  // "impl::geometry_from_text"
    std::string include;   // header declaring it, as the generated source writes it
};

// A method of the platform whose call wxl replaces with its own: the class keeps the
// method, with the signature the metadata gives it, and the body calls the function
// named here instead of the projection. The function takes the arguments exactly as
// the platform call would have been handed them -- already converted to the
// projection's types -- and returns what the platform call returns, so it is a drop-in
// for it. For a platform that refuses an argument the DSL has every reason to pass:
// RandomAccessStreamReference.CreateFromUri takes an address and cannot read a file one.
struct replaced_call {
    std::string function;  // "impl::stream_reference_from_uri", under namespace wxl
    std::string include;   // header declaring it, as the generated source writes it
};

// One NuGet package a profile draws metadata from. `metadata` names the
// .winmd files to read out of `metadata_dir` inside the package; when it's
// empty, every .winmd found there is read. The package is read at the version
// and from the directory the restore of the build resolved
// (restored_packages).
struct package_ref {
    std::string id;
    std::vector<std::string> metadata;

    // Files to read out of the package that are not metadata, named
    // relative to the package root -- the XAML resource dictionaries the
    // framework's named styles are declared in. Metadata says nothing about
    // those: a resource has a name and a target type, not a type of its own.
    std::vector<std::string> resources;

    // The lowest version of the package whose metadata declares what the
    // profile asks for ("minimumVersion"), or empty when any version does: a
    // restored version below it stops the run. For a package outside the
    // Windows App SDK release -- Win2D -- this is the one requirement there
    // is; a package of the release is usually covered by the release's
    // minimum (profile::minimum_windows_app_sdk).
    std::string minimum_version;
    std::string metadata_dir;  // empty means "metadata", the layout the SDK uses
};

// The umbrella package of a Windows App SDK release: its dependencies are the
// packages of the release, so its version is the release's.
inline constexpr std::string_view windows_app_sdk_id = "Microsoft.WindowsAppSDK";

// One profile file, as loaded -- `extends` is not yet resolved here.
struct profile {
    std::string name;
    std::string description;
    std::filesystem::path source;
    std::vector<std::string> extends;

    // The lowest Windows App SDK release (the umbrella package
    // Microsoft.WindowsAppSDK) whose packages declare the types this profile
    // projects ("minimumWindowsAppSdk"), or empty when the profile does not
    // constrain the release. For a set of profiles the highest of these is
    // the requirement, so a profile that extends another can raise it and
    // never lower it.
    std::string minimum_windows_app_sdk;

    std::vector<package_ref> packages;
    std::map<std::string, member_filter> types;  // "Microsoft.UI.Xaml.Controls.Button" -> filter
    std::map<std::string, std::vector<synthetic_member>> synthetic;  // by the same type name
    std::map<std::string, std::vector<setter_method>> setter_methods;  // by the same type name
    std::map<std::string, from_text_t> from_text;                      // by the same type name
    std::map<std::string, std::map<std::string, replaced_call>> replaced_calls;  // type name -> method name

    // The named styles of a listed type, by dictionary key: "styles": [...]
    // under the type's entry narrows them to those; a type listed without it
    // keeps all of its own (All). A style belongs with its type because
    // without the type it is not generated anyway -- there is nothing to
    // apply it to. A type the profile does not list has no entry here.
    std::map<std::string, member_filter> styles;  // by the same type name

    // The named brushes, by dictionary key: "brushes": [...] at the top of
    // the profile narrows them to those; without it, All. A brush belongs to
    // no type, so it is listed on its own.
    member_filter brushes = member_filter::all();

    member_filter discovered = member_filter::none();
    bool windows_metadata = false;
};

// What wxl implements on its own side of the boundary: the base types it
// supplies by hand, the roots every run gets, the WinRT types it already has
// something better than, and the properties that carry a tag of their own.
//
// This is not a profile. A profile says what to generate; this says what wxl
// itself is, which is the same for every profile -- so it lives in one shared
// file (profiles/types.json) that a run reads once.
struct type_map {
    // A WinRT type wxl does not wrap because it already owns an equivalent.
    struct projection {
        std::string metadata_name;  // "Windows.Foundation.Size"
        std::string cpp_name;       // "Size", under namespace wxl
        std::string include;        // header defining it, as the output writes it

        // A type a string literal turns into by itself -- a Uri, a font, an
        // image source. Such a type claims no unnamed-argument route: a bare
        // literal already means the class's own text.
        bool from_string = false;
    };

    // A property whose own type cannot identify it, and the tag that can.
    struct tag {
        std::string property_name;  // "Margin", as the metadata spells it
        std::string name;           // "Margin", the tag type under namespace wxl
        std::string value_type;     // "Thickness" -- what the tag is a tag over
        std::string include;        // header defining that value type
    };

    // A property of a class wxl writes by hand -- CompositionWindow's zoom --
    // which no metadata declares and which the builder syntax is still to
    // write in braces. The dispatch behind a tag is a template on the object,
    // so the key and the tag are all it takes; the class declares the member.
    struct hand_written_property {
        std::string name;        // "Zoom", as metadata would spell it
        std::string value_type;  // "double" -- the braced form's type, under namespace wxl
        std::string include;     // header defining that type, or empty
    };

    // A property a generated class has as a binding target alone -- NumberBox's
    // IntermediateValue, the number as it is being typed: no metadata declares
    // it and no setter writes it, a pair in impl/binding.h reads it off the
    // control. It gets the key, the tag, the class's schema anchor and a test
    // line binding it in the direction the pair takes.
    struct bound_member {
        std::string class_name;  // "Microsoft.UI.Xaml.Controls.NumberBox"
        std::string name;        // "IntermediateValue", as metadata would spell it
        std::string value_type;  // "double" -- the observable's type, under namespace wxl
        std::string direction;   // "input", "output" or "both"
        std::string include;     // header defining that type, or empty
    };

    std::set<std::string> predefined;
    std::set<std::string> implicit_roots;
    std::vector<projection> projections;
    std::vector<tag> tags;

    // The same for events -- CompositionWindow's ClientSizeChanged: a key, an
    // `on...` tag, and an event_adder calling the class's add_on.../remove_on....
    std::vector<hand_written_property> hand_written_properties;
    std::set<std::string> hand_written_events;
    std::vector<bound_member> bound_members;
};

// Reads profiles/types.json. Throws std::runtime_error naming the file on
// malformed JSON or an entry missing a required key.
type_map load_type_map(std::filesystem::path const& path);

// The Windows SDK a build projects: its version, which also names the folder of
// its reference (References\<version>), and the .winmd files the build's C++/WinRT
// projection of the platform was made from.
struct windows_sdk {
    std::string version;                          // "10.0.26100.0"
    std::vector<std::filesystem::path> metadata;  // the platform's .winmd files
};

// Several profiles (plus everything they reference, transitively) merged
// into the single input the crawler actually walks.
struct profile_set {
    std::vector<std::string> loaded;               // profile names, in load order
    std::vector<std::filesystem::path> metadata;   // .winmd files to open
    std::vector<std::filesystem::path> resources;  // XAML dictionaries to read
    std::map<std::string, member_filter> types;     // roots of the walk
    std::map<std::string, std::vector<synthetic_member>> synthetic;  // properties wxl adds
    std::map<std::string, std::vector<setter_method>> setter_methods;  // methods written as tags
    std::map<std::string, from_text_t> from_text;                      // classes a string builds
    std::map<std::string, std::map<std::string, replaced_call>> replaced_calls;  // type name -> method name

    // Merged the way members are, as a union: a profile that lists a type
    // without "styles", or has no "brushes", asks for all of them, and so
    // the merge does. A type no profile lists keeps all of its styles.
    std::map<std::string, member_filter> styles;
    member_filter brushes = member_filter::none();  // None until the first profile merges in

    member_filter discovered = member_filter::none();

    // The profile that lists each type of `types`, the first in load order: a
    // type the metadata does not declare is reported against it.
    std::map<std::string, std::filesystem::path> listed_by;

    // What the set was resolved from, for the steps after the walk: the list of
    // the restore, the Windows App SDK release it resolved (the version of
    // Microsoft.WindowsAppSDK, empty when it has none), and the Windows SDK of the
    // build, whose reference documents its metadata -- read into `metadata` when
    // a profile of the set asks for it ("windowsMetadata", `windows_metadata`).
    std::filesystem::path restored_from;
    std::string windows_app_sdk;
    windows_sdk sdk;
    bool windows_metadata = false;
};

// Reads a single profile file. Throws std::runtime_error with the file
// name on malformed JSON, on an unknown key shape, on a type that
// carries both "members" and "excludeMembers" (mutually exclusive by
// design -- a member named in both would be a straight contradiction),
// on a minimum that is not a NuGet version, and on the keys that name a
// version to read -- "windowsAppSdk", and "version" or "overrideVersion" of
// a package -- with the key that says what a profile can say instead.
profile load_profile(std::filesystem::path const& path);

// Where a profile named in `extends` lives. A reference is either a path (has
// a directory part or an extension), relative to the referring profile's
// directory, or a bare profile name, the file <name>.winxl-profile next to the
// profile that named it. The reference is a profile's own text, checked UTF-8,
// as load_profile() puts it in `extends`.
std::filesystem::path resolve_reference(std::string_view reference,
                                        std::filesystem::path const& referrer);

// A package version, parsed and compared the way NuGet's NuGetVersion is:
// SemVer 2.0 with a fourth numeric part. The numeric parts are compared as
// numbers, a part not written being 0; a release is above its prereleases;
// prerelease labels are compared identifier by identifier -- an identifier
// that is an Int32 as a number, a number below any text, text ignoring ASCII
// case -- and of two labels that agree as far as the shorter goes, the
// shorter is the lower; build metadata takes no part. The order is weak:
// 1.0.0-A and 1.0.0-a are equivalent and spelled differently.
class nuget_version {
public:
    // The version `text` spells, or nothing when it spells none: one to four
    // numeric parts separated by dots, each an Int32, then optionally '-' and a
    // label of dot-separated identifiers of [0-9A-Za-z-], none of them a number
    // with a leading zero, then optionally '+' and metadata of identifiers of
    // the same characters. Whitespace is refused.
    static std::optional<nuget_version> parse(std::string_view text);

    friend std::weak_ordering operator<=>(nuget_version const& a, nuget_version const& b);
    friend bool operator==(nuget_version const& a, nuget_version const& b) {
        return (a <=> b) == 0;
    }

private:
    std::array<std::int32_t, 4> numbers_{};
    std::vector<std::string> prerelease_;
};

// One package the NuGet restore of a build resolved: its id as the restore
// spells it, the version it resolved, and the directory the package is in.
struct restored_package {
    std::string id;
    std::string version;
    std::filesystem::path directory;
};

// What the NuGet restore of a build resolved, and the Windows SDK the build
// projects: the input every run reads its packages from. The configure of the
// build writes it into <build>/wxl.packages/restored-packages.json
// (cmake/wxl_packages.cmake). Nothing here is worked out by the generator:
// a package is read at the version the restore resolved, and the Windows
// metadata is the set of .winmd files the build's own C++/WinRT projection of
// the platform was made from.
struct restored_packages {
    std::filesystem::path source;  // the file, for the messages that name it
    std::vector<restored_package> packages;
    windows_sdk sdk;

    // The package with this id, matched the way NuGet matches ids, ignoring
    // ASCII case; null when the restore has none.
    restored_package const* find(std::string_view id) const;
};

// Reads the list of the restore. Throws std::runtime_error naming the file
// when it is not there -- with how to get it -- on malformed JSON, on an entry
// missing a key, on a version that is not a NuGet version, on a package
// listed twice, on a Windows SDK version that is not four numbers separated by
// dots, and on a Windows SDK without metadata.
restored_packages load_restored_packages(std::filesystem::path const& path);

// Loads every named profile plus, transitively, the profiles they
// `extends`, and merges them. A profile reference is either a path or a
// bare name resolved as `<dir of the referencing profile>/<name>.winxl-profile`.
// Diamonds are loaded once; cycles are an error.
//
// Every package a profile names is read from `restored`, at the version the
// restore resolved. A package the restore does not have, a version below the
// package's minimumVersion, and a Microsoft.WindowsAppSDK below the highest
// minimumWindowsAppSdk of the set (or absent while a profile names one) are
// errors: std::runtime_error naming the profile, the package and both
// versions, or what the restore has to add.
profile_set resolve_profiles(std::vector<std::filesystem::path> const& paths,
                             restored_packages const& restored);

// One named glyph of the icon font: the name a `FluentSymbol` is written
// under, and the code point behind it.
struct symbol {
    std::string name;
    uint32_t code = 0;
};

// Reads profiles/fluent-symbols.json, sorted by code point. Not a profile
// and not metadata: the font carries no glyph names of its own, so these are
// typed in from the documentation once and read by every run -- the same
// standing as types.json. Throws std::runtime_error naming the file on
// malformed JSON, on a code point that is not four hex digits, and on a name
// or a code point used twice.
std::vector<symbol> load_symbol_names(std::filesystem::path const& path);
