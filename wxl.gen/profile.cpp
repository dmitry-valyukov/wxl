module;

#include <format>
#include <print>

// The XML reader lives outside this module; see xml_input.h.
#include "xml_input.h"

module wxl.gen;

import std;
import wxl.core;
import wxl.json;

using wxl::json::value;

namespace {

std::string env(char const* name) {
    // getenv rather than _dupenv_s: this is a build-time tool, the value is
    // read once at startup and never written back.
    char const* value = std::getenv(name);
    return value ? std::string{value} : std::string{};
}

[[noreturn]] void fail(std::filesystem::path const& source, std::string_view message) {
    throw std::runtime_error(std::format("{}: {}", source.string(), message));
}


// The type map, installed once by main() and read from every stage. A
// process-wide fact of the run: which types wxl already has is not something
// one profile can disagree with another about.
TypeMap loaded_type_map;

// A field's text, copied out of the tree. The reader answers with views into
// the buffer the document owns, and that document is gone at the end of the
// load, while what is built out of it lives for the rest of the run.
std::string text(value const& field) { return std::string{field.as_string().chars()}; }

std::string required_string(value const& entry, std::filesystem::path const& source,
                            std::string_view key, std::string_view what) {
    value const& field = entry[key];
    if (!field.is_string()) {
        fail(source, std::format("every {} needs a string '{}'", what, key));
    }
    return text(field);
}

// The string a key carries, or `fallback` when the file leaves the key out. A
// key that is there but is not a string is a mistake rather than a default:
// `"name": 42` would otherwise read exactly like no name at all.
std::string string_or(value const& entry, std::filesystem::path const& source,
                      std::string_view key, std::string_view fallback) {
    value const* const field = entry.find(key);
    if (!field) {
        return std::string{fallback};
    }
    if (!field->is_string()) {
        fail(source, std::format("'{}' must be a string", key));
    }
    return text(*field);
}

bool bool_or(value const& entry, std::filesystem::path const& source, std::string_view key,
             bool fallback) {
    value const* const field = entry.find(key);
    if (!field) {
        return fallback;
    }
    if (!field->is_boolean()) {
        fail(source, std::format("'{}' must be true or false", key));
    }
    return field->as_bool();
}

// Every file this stage reads is JSON with comments allowed: the tables in
// them are decisions, and a decision that cannot carry its reason beside it
// loses the reason. wxl.json reads comments always, with no switch to set --
// these files are half of why it does.
//
// The document object belongs to the caller. The tree is views into the
// buffer it holds, so it has to outlive every string read out of it, and a
// reader that can be neither copied nor moved says so plainly.
value const& read_json(wxl::json::document& file, std::filesystem::path const& path,
                       std::string_view what_object) {
    // Asked here rather than left to the reader: a file that is not there is
    // the one failure this tool can word in its own voice, and "cannot open"
    // is what the rest of it says.
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error(std::format("cannot open: {}", path.string()));
    }

    try {
        file.load_file(path);
    } catch (wxl::json::exception const& broken) {
        // What is left, the reader knows better: its message already names
        // the file and the place in it -- `file(line,column): reason`.
        throw std::runtime_error(broken.what());
    }

    value const& document = *file.root();
    if (!document.is_object()) {
        fail(path, what_object);
    }
    return document;
}

std::set<std::string> read_string_set(value const& array, std::filesystem::path const& source,
                                      std::string_view what) {
    if (!array.is_array()) {
        fail(source, std::format("'{}' must be an array of strings", what));
    }
    std::set<std::string> names;
    for (auto&& entry : array.elements()) {
        if (!entry.is_string()) {
            fail(source, std::format("'{}' must contain strings only", what));
        }
        names.insert(text(entry));
    }
    return names;
}

MemberFilter read_member_filter(value const& entry, std::filesystem::path const& source,
                                std::string_view type_name) {
    if (!entry.is_object()) {
        fail(source, std::format("type '{}' must map to an object", type_name));
    }

    value const* const allow = entry.find("members");
    value const* const deny = entry.find("excludeMembers");

    if (allow && deny) {
        // Deliberately an error rather than a precedence rule: a member
        // named in both lists is a contradiction, not something to
        // silently resolve.
        fail(source, std::format("type '{}' has both 'members' and 'excludeMembers'; "
                                 "they are mutually exclusive",
                                 type_name));
    }
    if (allow) {
        // An empty array is meaningful and distinct from an absent key:
        // it means "none of this type's own declarations", which is how a
        // type that only exists to be a walk root (CheckBox, ListView) is
        // spelled.
        return MemberFilter::allow(read_string_set(*allow, source, "members"));
    }
    if (deny) {
        return MemberFilter::deny(read_string_set(*deny, source, "excludeMembers"));
    }
    return MemberFilter::all();
}

// The properties wxl adds to a type although WinRT declares none. Each is
// written under the type it belongs to, and says the three things the
// generator cannot work out on its own: what the value is, what function
// takes it, and where that function is declared.
std::vector<SyntheticMember> read_synthetic_members(value const& entry,
                                                    std::filesystem::path const& source,
                                                    std::string_view type_name) {
    value const* const added = entry.find("syntheticMembers");
    if (!added) {
        return {};
    }
    if (!added->is_object()) {
        fail(source, std::format("type '{}': 'syntheticMembers' must be an object keyed by "
                                 "property name",
                                 type_name));
    }

    std::vector<SyntheticMember> members;
    for (value const& definition : added->members()) {
        std::string const name{definition.name().chars()};
        if (!definition.is_object()) {
            fail(source, std::format("type '{}': synthetic member '{}' must be an object",
                                     type_name, name));
        }

        // Every one of these is text, and a key that is there but is not text
        // is the same mistake as a key that is absent -- so both are told the
        // same way.
        auto const field = [&](std::string_view key) {
            value const& found = definition[key];
            if (!found.is_string()) {
                fail(source, std::format("type '{}': synthetic member '{}' needs a string '{}'",
                                         type_name, name, key));
            }
            return text(found);
        };

        // Where the value comes from: metadata names it, or wxl owns it.
        // Both would be two answers to one question, neither none at all.
        bool const from_metadata = definition.find("type") != nullptr;
        bool const from_wxl = definition.find("cppType") != nullptr;
        if (from_metadata == from_wxl) {
            fail(source, std::format("type '{}': synthetic member '{}' needs exactly one of "
                                     "'type' and 'cppType'",
                                     type_name, name));
        }
        if (from_wxl && !definition.find("cppInclude")) {
            fail(source, std::format("type '{}': synthetic member '{}' has 'cppType' and so "
                                     "needs 'cppInclude'",
                                     type_name, name));
        }

        SyntheticMember member{name, {}, field("function"), field("include")};
        if (from_metadata) {
            member.type = field("type");
        } else {
            member.cpp_type = field("cppType");
            member.cpp_include = field("cppInclude");
        }
        members.push_back(std::move(member));
    }

    // By name, because the order these reach the output should not be the
    // order someone happened to type them into the profile. The map-backed
    // reader this stage used before sorted them as a side effect of how it
    // stored an object; wxl.json keeps the document's own order, so the
    // sorting is said out loud here instead.
    std::ranges::sort(members, {}, &SyntheticMember::name);
    return members;
}

PackageRef read_package(value const& entry, std::filesystem::path const& source) {
    if (!entry.is_object() || !entry.find("id")) {
        fail(source, "every 'packages' entry must be an object with an 'id'");
    }
    PackageRef package;
    package.id = required_string(entry, source, "id", "package");
    // Checked against the SDK release below, where it is known whether this
    // package is one the release ships.
    package.version = string_or(entry, source, "version", {});
    package.metadata_dir = string_or(entry, source, "metadataDir", {});
    if (value const* const listed = entry.find("metadata")) {
        auto const names = read_string_set(*listed, source, "metadata");
        package.metadata.assign(names.begin(), names.end());
    }
    if (value const* const listed = entry.find("resources")) {
        auto const names = read_string_set(*listed, source, "resources");
        package.resources.assign(names.begin(), names.end());
    }
    return package;
}

constexpr std::string_view windows_app_sdk_id = "Microsoft.WindowsAppSDK";

// A nuspec dependency version is either plain (2.3.6) or a NuGet range
// pinning one release ([2.4.0]); both mean the same thing here.
std::string trim_version_range(std::string_view text) {
    auto const first = text.find_first_not_of(" \t[(");
    if (first == std::string_view::npos) {
        return {};
    }
    auto const rest = text.substr(first);
    auto const last = rest.find_first_of(" \t,])");
    return std::string{last == std::string_view::npos ? rest : rest.substr(0, last)};
}

// Every package version an SDK release ships, read out of its own nuspec:
// the umbrella package declares one <dependency> per sub-package, which is
// what makes a single version number in a profile enough.
std::map<std::string, std::string> read_sdk_dependencies(std::filesystem::path const& nuspec) {
    std::map<std::string, std::string> versions;
    for (auto&& [id, version] : nuspec_dependencies(nuspec)) {
        versions.emplace(wxl::core::ascii_lower(id), trim_version_range(version));
    }
    return versions;
}

// <nuget root>/<id, lowercased>/<version>/<metadata dir>/<file>.winmd. The
// directory is "metadata" for the WindowsAppSDK packages, and whatever a
// package from outside the SDK ships its .winmd in -- Win2D puts it under
// lib/uap10.0.
std::vector<std::filesystem::path> resolve_package(PackageRef const& package,
                                                   std::string_view version,
                                                   std::filesystem::path const& nuget_root,
                                                   std::filesystem::path const& source) {
    auto const dir = nuget_root / wxl::core::ascii_lower(package.id) / version /
                     (package.metadata_dir.empty() ? "metadata" : package.metadata_dir);
    if (!std::filesystem::is_directory(dir)) {
        fail(source, std::format("package '{}' {}: no metadata directory at {}", package.id,
                                 version, dir.string()));
    }

    std::vector<std::filesystem::path> files;
    if (package.metadata.empty()) {
        for (auto&& item : std::filesystem::directory_iterator{dir}) {
            if (item.is_regular_file() && item.path().extension() == ".winmd") {
                files.push_back(item.path());
            }
        }
        std::sort(files.begin(), files.end());
        if (files.empty()) {
            fail(source, std::format("package '{}' {}: no .winmd files in {}", package.id,
                                     version, dir.string()));
        }
        return files;
    }

    for (auto&& name : package.metadata) {
        auto path = dir / name;
        if (!std::filesystem::is_regular_file(path)) {
            fail(source, std::format("package '{}' {}: no such metadata file: {}", package.id,
                                     version, path.string()));
        }
        files.push_back(std::move(path));
    }
    return files;
}

// A package's non-metadata files, named relative to its root rather than to
// the metadata directory: a resource dictionary lives wherever the package
// ships it, not beside the .winmd.
std::vector<std::filesystem::path> resolve_resources(PackageRef const& package,
                                                     std::string_view version,
                                                     std::filesystem::path const& nuget_root,
                                                     std::filesystem::path const& source) {
    auto const root = nuget_root / wxl::core::ascii_lower(package.id) / version;

    std::vector<std::filesystem::path> files;
    for (auto&& name : package.resources) {
        auto path = root / name;
        if (!std::filesystem::is_regular_file(path)) {
            fail(source, std::format("package '{}' {}: no such resource file: {}", package.id,
                                     version, path.string()));
        }
        files.push_back(std::move(path));
    }
    return files;
}

// The projected Windows platform metadata (Windows.Foundation.IReference,
// IVector<T>, EventHandler<T>, ...). WinUI's own .winmd only *references*
// those; without this the walk silently stops at every such edge.
std::vector<std::filesystem::path> windows_metadata_files() {
    auto const dir = std::filesystem::path{env("SystemRoot").empty() ? "C:/Windows"
                                                                    : env("SystemRoot")} /
                     "System32" / "WinMetadata";
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::is_directory(dir)) {
        return files;
    }
    for (auto&& item : std::filesystem::directory_iterator{dir}) {
        if (item.is_regular_file() && item.path().extension() == ".winmd") {
            files.push_back(item.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// A reference is either a path (has a directory part or a .json suffix)
// or a bare profile name sitting next to the profile that named it.
std::filesystem::path resolve_reference(std::string_view reference,
                                        std::filesystem::path const& referrer) {
    std::filesystem::path path{reference};
    if (path.has_extension() || path.has_parent_path()) {
        return path.is_absolute() ? path : referrer.parent_path() / path;
    }
    return referrer.parent_path() / (std::string(reference) + ".json");
}

}  // namespace

bool MemberFilter::allows(std::string_view name) const {
    switch (kind) {
        case Kind::All:
            return true;
        case Kind::None:
            return false;
        case Kind::Allow:
            return names.count(std::string{name}) != 0;
        case Kind::Deny:
            return names.count(std::string{name}) == 0;
    }
    return false;
}

void MemberFilter::merge(MemberFilter const& other) {
    if (kind == Kind::All || other.kind == Kind::None) {
        return;
    }
    if (other.kind == Kind::All || kind == Kind::None) {
        *this = other;
        return;
    }
    if (kind == Kind::Allow && other.kind == Kind::Allow) {
        names.insert(other.names.begin(), other.names.end());
        return;
    }
    if (kind == Kind::Deny && other.kind == Kind::Deny) {
        // Denied by both, i.e. allowed by neither.
        std::set<std::string> both;
        std::set_intersection(names.begin(), names.end(), other.names.begin(), other.names.end(),
                              std::inserter(both, both.end()));
        names = std::move(both);
        return;
    }

    // Allow(A) + Deny(B): everything except B, plus A -- that is,
    // Deny(B \ A).
    auto const& allow = kind == Kind::Allow ? names : other.names;
    auto const& deny = kind == Kind::Deny ? names : other.names;
    std::set<std::string> rest;
    std::set_difference(deny.begin(), deny.end(), allow.begin(), allow.end(),
                        std::inserter(rest, rest.end()));
    kind = Kind::Deny;
    names = std::move(rest);
}

Profile load_profile(std::filesystem::path const& path) {
    wxl::json::document file;
    auto const& document = read_json(file, path, "a profile must be a JSON object");

    Profile profile;
    profile.source = path;
    profile.name = string_or(document, path, "name", path.stem().string());
    profile.description = string_or(document, path, "description", {});

    if (value const* const extends = document.find("extends")) {
        auto const names = read_string_set(*extends, path, "extends");
        profile.extends.assign(names.begin(), names.end());
    }

    profile.windows_app_sdk = string_or(document, path, "windowsAppSdk", {});

    if (value const* const packages = document.find("packages")) {
        if (!packages->is_array()) {
            fail(path, "'packages' must be an array");
        }
        for (value const& entry : packages->elements()) {
            profile.packages.push_back(read_package(entry, path));
        }
    }

    if (value const* const types = document.find("types")) {
        if (!types->is_object()) {
            fail(path, "'types' must be an object keyed by metadata type name");
        }
        for (value const& entry : types->members()) {
            std::string const name{entry.name().chars()};
            profile.types.emplace(name, read_member_filter(entry, path, name));
            if (auto added = read_synthetic_members(entry, path, name); !added.empty()) {
                profile.synthetic.emplace(name, std::move(added));
            }
        }
    }

    // Types the walk discovers on its own (through base classes and member
    // signatures) rather than finding listed: "none" generates the type
    // but none of its members, "all" generates everything it declares.
    if (auto const discovered = string_or(document, path, "discoveredTypes", "none");
        discovered == "all") {
        profile.discovered = MemberFilter::all();
    } else if (discovered == "none") {
        profile.discovered = MemberFilter::none();
    } else {
        fail(path, std::format("'discoveredTypes' must be \"none\" or \"all\", got \"{}\"",
                               discovered));
    }

    profile.windows_metadata = bool_or(document, path, "windowsMetadata", false);
    return profile;
}

std::filesystem::path canonical_path(std::filesystem::path const& path) {
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(path, ec);
    return ec ? path : resolved;
}

// The depth-first load, as a type rather than as a lambda that takes itself.
// Recursion is what it needs and what a lambda cannot say without becoming a
// template over its own closure -- which MSVC 14.51 crashes on in a module
// implementation unit (C1001 in msc1.cpp). A named function has no such
// construct to trip over, and the state the walk carries reads better as
// fields than as a capture list.
struct ProfileLoader {
    ProfileSet& result;
    std::set<std::filesystem::path>& loaded;   // canonical paths, load-once
    std::set<std::filesystem::path>& loading;  // cycle detection
    std::vector<Profile>& with_packages;
    bool& windows_metadata;

    // Depth-first so that a referenced profile ("base") is merged before
    // the profile that extends it -- merging is a union, so the order
    // doesn't change the outcome, but it keeps `loaded` readable.
    void load(std::filesystem::path const& path) {
        auto const key = canonical_path(path);
        if (loaded.count(key)) {
            return;
        }
        if (!loading.insert(key).second) {
            throw std::runtime_error(
                std::format("profile reference cycle at {}", key.string()));
        }

        auto const profile = load_profile(path);
        for (auto&& reference : profile.extends) {
            load(resolve_reference(reference, profile.source));
        }

        // Packages wait for the second pass: the SDK release they are taken
        // from may be named by a profile further up the chain than this one.
        with_packages.push_back(profile);

        for (auto&& [name, filter] : profile.types) {
            auto [it, inserted] = result.types.try_emplace(name, filter);
            if (!inserted) {
                it->second.merge(filter);
            }
        }
        // Synthetic members compose the way the rest of a profile does: an
        // extending profile adds to what it extends, and a name declared
        // twice is the same property, kept once.
        for (auto&& [name, added] : profile.synthetic) {
            auto& known = result.synthetic[name];
            for (auto&& member : added) {
                if (std::none_of(known.begin(), known.end(), [&](SyntheticMember const& existing) {
                        return existing.name == member.name;
                    })) {
                    known.push_back(member);
                }
            }
        }
        result.discovered.merge(profile.discovered);
        windows_metadata = windows_metadata || profile.windows_metadata;

        loading.erase(key);
        loaded.insert(key);
        result.loaded.push_back(profile.name);
    }
};

ProfileSet resolve_profiles(std::vector<std::filesystem::path> const& paths,
                            std::filesystem::path const& nuget_root) {
    ProfileSet result;

    std::set<std::filesystem::path> loaded;   // canonical paths, load-once
    std::set<std::filesystem::path> loading;  // cycle detection
    std::set<std::filesystem::path> metadata;
    std::set<std::filesystem::path> resources;
    std::vector<Profile> with_packages;  // resolved once the SDK release is known
    bool windows_metadata = false;

    ProfileLoader loader{result, loaded, loading, with_packages, windows_metadata};
    for (auto&& path : paths) {
        loader.load(path);
    }

    // One SDK release across the whole set. Profiles that name none inherit
    // it; two that name different ones are asking for a mixture nobody
    // ships, which is the thing this key exists to prevent.
    Profile const* chose_sdk = nullptr;
    for (auto&& profile : with_packages) {
        if (profile.windows_app_sdk.empty()) {
            continue;
        }
        if (chose_sdk && chose_sdk->windows_app_sdk != profile.windows_app_sdk) {
            fail(profile.source,
                 std::format("asks for {} {}, while {} asks for {}", windows_app_sdk_id,
                             profile.windows_app_sdk, chose_sdk->source.string(),
                             chose_sdk->windows_app_sdk));
        }
        chose_sdk = &profile;
    }

    std::map<std::string, std::string> package_versions;
    if (chose_sdk) {
        auto const dir = nuget_root / wxl::core::ascii_lower(windows_app_sdk_id) / chose_sdk->windows_app_sdk;
        auto const nuspec = dir / std::format("{}.nuspec", wxl::core::ascii_lower(windows_app_sdk_id));
        if (!std::filesystem::is_regular_file(nuspec)) {
            fail(chose_sdk->source, std::format("{} {} is not in the package root: no {}",
                                                windows_app_sdk_id, chose_sdk->windows_app_sdk,
                                                nuspec.string()));
        }
        package_versions = read_sdk_dependencies(nuspec);
        package_versions.emplace(wxl::core::ascii_lower(windows_app_sdk_id), chose_sdk->windows_app_sdk);
    }

    for (auto&& profile : with_packages) {
        for (auto&& package : profile.packages) {
            auto const shipped = package_versions.find(wxl::core::ascii_lower(package.id));

            // A package the release ships takes its version from the release
            // and may not name one: a hand-picked version there is the
            // mixture nobody ships, which is what naming the release once
            // exists to prevent. A package from outside the release has no
            // such declaration to read, so its own version is the only
            // answer -- and the only place the two rules meet is here.
            if (shipped != package_versions.end() && !package.version.empty()) {
                fail(profile.source,
                     std::format("package '{}' carries a 'version', but {} ships it and declares "
                                 "{}; the SDK release is named once, as 'windowsAppSdk', and its "
                                 "packages take their versions from it",
                                 package.id, windows_app_sdk_id, shipped->second));
            }
            if (shipped == package_versions.end() && package.version.empty()) {
                fail(profile.source,
                     chose_sdk ? std::format("package '{}' is not one {} {} ships, so it has to "
                                             "name its own 'version'",
                                             package.id, windows_app_sdk_id,
                                             chose_sdk->windows_app_sdk)
                               : std::format("package '{}' needs a version: either name one "
                                             "here, or name an SDK release as 'windowsAppSdk' "
                                             "for it to come from",
                                             package.id));
            }

            std::string version = package.version;
            if (version.empty()) {
                version = shipped->second;
            }

            for (auto&& file : resolve_package(package, version, nuget_root, profile.source)) {
                metadata.insert(file);
            }
            for (auto&& file : resolve_resources(package, version, nuget_root, profile.source)) {
                resources.insert(file);
            }
        }
    }

    if (windows_metadata) {
        for (auto&& file : windows_metadata_files()) {
            metadata.insert(file);
        }
    }

    result.metadata.assign(metadata.begin(), metadata.end());
    result.resources.assign(resources.begin(), resources.end());
    return result;
}

// NuGet's own answer to "where is the global packages folder", read out of
// its configuration: `<add key="globalPackagesFolder" value="..."/>`.
//
// Asking the tool rather than guessing from the environment is what makes
// this work on a machine that moved the cache: the conventional
// %USERPROFILE%\.nuget\packages usually still exists there, holding an older
// partial copy, so guessing does not fail loudly -- it silently reads the
// wrong tree and reports packages as missing that are installed.
//
// A targeted scan rather than a parse: one key, one attribute, and the
// alternative is linking the whole XML reader into the generator for it.
std::filesystem::path nuget_config_packages_folder() {
    auto const appdata = env("APPDATA");
    if (appdata.empty()) {
        return {};
    }

    std::ifstream file{std::filesystem::path{appdata} / "NuGet" / "NuGet.Config"};
    if (!file) {
        return {};
    }
    std::string const text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    auto const key = text.find("globalPackagesFolder");
    if (key == std::string::npos) {
        return {};
    }
    auto const value = text.find("value=", key);
    if (value == std::string::npos) {
        return {};
    }
    auto const open = text.find('"', value);
    auto const close = open == std::string::npos ? std::string::npos : text.find('"', open + 1);
    if (close == std::string::npos) {
        return {};
    }
    return std::filesystem::path{text.substr(open + 1, close - open - 1)};
}


TypeMap load_type_map(std::filesystem::path const& path) {
    wxl::json::document file;
    auto const& document = read_json(file, path, "a type map must be a JSON object");

    TypeMap map;
    if (value const* const given = document.find("givenFromAbove")) {
        map.given_from_above = read_string_set(*given, path, "givenFromAbove");
    }
    if (value const* const roots = document.find("implicitRoots")) {
        map.implicit_roots = read_string_set(*roots, path, "implicitRoots");
    }

    if (value const* const array = document.find("projectedTypes")) {
        if (!array->is_array()) {
            fail(path, "'projectedTypes' must be an array");
        }
        for (value const& entry : array->elements()) {
            if (!entry.is_object()) {
                fail(path, "every projected type must be an object");
            }
            map.projections.push_back({required_string(entry, path, "type", "projected type"),
                                       required_string(entry, path, "wxl", "projected type"),
                                       required_string(entry, path, "include", "projected type"),
                                       bool_or(entry, path, "fromString", false)});
        }
    }

    if (value const* const array = document.find("taggedProperties")) {
        if (!array->is_array()) {
            fail(path, "'taggedProperties' must be an array");
        }
        for (value const& entry : array->elements()) {
            if (!entry.is_object()) {
                fail(path, "every tagged property must be an object");
            }
            // The include is optional: a tag over a built-in -- an int, a
            // double -- has no header to name, and Tags.h already carries
            // everything such a value needs.
            map.tags.push_back({required_string(entry, path, "property", "tagged property"),
                                required_string(entry, path, "wxl", "tagged property"),
                                required_string(entry, path, "value", "tagged property"),
                                string_or(entry, path, "include", {})});
        }
    }
    return map;
}

void use_type_map(TypeMap map) { loaded_type_map = std::move(map); }

TypeMap const& type_map() { return loaded_type_map; }

std::filesystem::path default_nuget_root() {
    // The environment first: it is the documented override, and CI sets it.
    if (auto const packages = env("NUGET_PACKAGES"); !packages.empty()) {
        return packages;
    }
    if (auto const configured = nuget_config_packages_folder();
        !configured.empty() && std::filesystem::is_directory(configured)) {
        return configured;
    }
    return std::filesystem::path{env("USERPROFILE")} / ".nuget" / "packages";
}

std::vector<Symbol> load_symbol_names(std::filesystem::path const& path) {
    wxl::json::document file;
    auto const& document = read_json(file, path, "a symbol table must be a JSON object");

    value const* const symbols = document.find("symbols");
    if (!symbols || !symbols->is_object()) {
        fail(path, "'symbols' must be an object of name -> code point");
    }

    std::vector<Symbol> names;
    std::set<uint32_t> seen_codes;
    std::set<std::string> seen_names;
    for (value const& entry : symbols->members()) {
        std::string name{entry.name().chars()};
        if (!entry.is_string()) {
            fail(path, std::format("'{}': a code point must be a string", name));
        }
        // The digits stay a view into the document: from_chars reads them
        // where they lie, and nothing built here outlives the load.
        auto const digits = entry.as_string().chars();
        uint32_t code = 0;
        auto const [end, error] =
            std::from_chars(digits.data(), digits.data() + digits.size(), code, 16);
        if (error != std::errc{} || end != digits.data() + digits.size() || digits.size() != 4) {
            fail(path, std::format("'{}': '{}' is not four hex digits", name, digits));
        }
        if (!seen_names.insert(name).second) {
            fail(path, std::format("'{}' is named twice", name));
        }
        if (!seen_codes.insert(code).second) {
            fail(path, std::format("'{}': code point {} is used twice", name, digits));
        }
        names.push_back({std::move(name), code});
    }

    // By code point, which is the order the font is documented in and the
    // only order the reader of the generated enum can predict.
    std::ranges::sort(names, {}, &Symbol::code);
    return names;
}

namespace {
std::vector<Symbol> loaded_symbol_names;
}

void use_symbol_names(std::vector<Symbol> names) { loaded_symbol_names = std::move(names); }

std::vector<Symbol> const& symbol_names() { return loaded_symbol_names; }
