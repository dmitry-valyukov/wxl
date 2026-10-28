#include <format>

#include "profile.h"

import std;
import wxl.core;
import wxl.json;

using wxl::json::value;

namespace {

[[noreturn]] void fail(std::filesystem::path const& source, std::string_view message) {
    throw std::runtime_error(
        std::format("{}: {}", wxl::core::unicode::to_utf8(source).chars(), message));
}

// A name a profile or a package spells, as a path: a file, a folder, a version. Each
// is a copy of a string wxl.json or wxl.xml checked with the whole document, a piece
// of one cut at an ASCII character, or a literal; the std::string it is kept in no
// longer says so, and assume_valid() records it here.
std::filesystem::path path_of(std::string_view name) {
    return wxl::core::unicode::to_path(wxl::core::unicode::assume_valid(name));
}


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
        throw std::runtime_error(
            std::format("cannot open: {}", wxl::core::unicode::to_utf8(path).chars()));
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

member_filter read_member_filter(value const& entry, std::filesystem::path const& source,
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
        return member_filter::allow(read_string_set(*allow, source, "members"));
    }
    if (deny) {
        return member_filter::deny(read_string_set(*deny, source, "excludeMembers"));
    }
    return member_filter::all();
}

// The properties wxl adds to a type although WinRT declares none. Each is
// written under the type it belongs to, and says the three things the
// generator cannot work out on its own: what the value is, what function
// takes it, and where that function is declared.
std::vector<synthetic_member> read_synthetic_members(value const& entry,
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

    std::vector<synthetic_member> members;
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

        synthetic_member member{name, {}, field("function"), field("include")};
        member.positional = bool_or(definition, source, "positional", false);
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
    std::ranges::sort(members, {}, &synthetic_member::name);
    return members;
}

// The methods a profile writes as tags. Keyed by the method's own name, each
// an object that may narrow the value to a more specific class of the
// metadata. Whether the method exists, takes one argument and returns nothing
// is the class writer's to check: only it has read the signatures.
std::vector<setter_method> read_setter_methods(value const& entry,
                                               std::filesystem::path const& source,
                                               std::string_view type_name) {
    value const* const listed = entry.find("setterMethods");
    if (!listed) {
        return {};
    }
    if (!listed->is_object()) {
        fail(source, std::format("type '{}': 'setterMethods' must be an object keyed by method "
                                 "name",
                                 type_name));
    }

    std::vector<setter_method> methods;
    for (value const& definition : listed->members()) {
        std::string const name{definition.name().chars()};
        if (!definition.is_object()) {
            fail(source, std::format("type '{}': setter method '{}' must be an object", type_name,
                                     name));
        }
        if (!name.starts_with("Set") || name.size() == 3) {
            fail(source, std::format("type '{}': setter method '{}' has no name after 'Set' to "
                                     "give its tag",
                                     type_name, name));
        }
        setter_method method{name, {}};
        if (value const* const type = definition.find("type")) {
            if (!type->is_string()) {
                fail(source, std::format("type '{}': setter method '{}': 'type' must be a string",
                                         type_name, name));
            }
            method.type = text(*type);
        }
        methods.push_back(std::move(method));
    }
    std::ranges::sort(methods, {}, &setter_method::method);
    return methods;
}

// The methods whose call wxl replaces with a function of its own, keyed by the
// method's name. Whether the class declares the method and the profile lets it
// through is the class writer's to check: only it has read the members.
std::map<std::string, replaced_call> read_replaced_calls(value const& entry,
                                                        std::filesystem::path const& source,
                                                        std::string_view type_name) {
    value const* const listed = entry.find("replacedCalls");
    if (!listed) {
        return {};
    }
    if (!listed->is_object()) {
        fail(source, std::format("type '{}': 'replacedCalls' must be an object keyed by method "
                                 "name",
                                 type_name));
    }

    std::map<std::string, replaced_call> calls;
    for (value const& definition : listed->members()) {
        std::string const name{definition.name().chars()};
        if (!definition.is_object() || !definition.find("function") ||
            !definition.find("include") || !definition["function"].is_string() ||
            !definition["include"].is_string()) {
            fail(source, std::format("type '{}': replaced call '{}' must be an object with a "
                                     "string 'function' and a string 'include'",
                                     type_name, name));
        }
        calls[name] = {text(definition["function"]), text(definition["include"])};
    }
    return calls;
}

// A minimum version a profile names, checked as it is read: a requirement that
// does not parse -- an empty one among them -- could be compared with nothing.
// Empty when the key is not there. `where` starts the message.
std::string minimum_or_empty(value const& entry, std::filesystem::path const& source,
                             std::string_view key, std::string_view where) {
    if (!entry.find(key)) {
        return {};
    }
    std::string minimum = string_or(entry, source, key, {});
    if (!nuget_version::parse(minimum)) {
        fail(source, std::format("{}'{}' is \"{}\", which is not a NuGet version", where, key,
                                 minimum));
    }
    return minimum;
}

package_ref read_package(value const& entry, std::filesystem::path const& source) {
    if (!entry.is_object() || !entry.find("id")) {
        fail(source, "every 'packages' entry must be an object with an 'id'");
    }
    package_ref package;
    package.id = required_string(entry, source, "id", "package");
    // A profile says what to project; the version read is the one the build's
    // restore resolved, so a key that names one is a mistake to report.
    if (entry.find("version")) {
        fail(source, std::format("package '{}': 'version' is not a key of a package entry: the "
                                 "package is read at the version the build's NuGet restore "
                                 "resolves, and a profile whose types need a certain version "
                                 "names the lowest one they need as 'minimumVersion'",
                                 package.id));
    }
    if (entry.find("overrideVersion")) {
        fail(source, std::format("package '{}': 'overrideVersion' is not a key of a package "
                                 "entry: the build's NuGet restore resolves the version of every "
                                 "package, and a profile whose types need a newer one names the "
                                 "lowest one they need as 'minimumVersion' of the package, or as "
                                 "'minimumWindowsAppSdk' for the whole Windows App SDK release",
                                 package.id));
    }
    package.minimum_version = minimum_or_empty(entry, source, "minimumVersion",
                                               std::format("package '{}': ", package.id));
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

// What the restore of the build is, in a message: the list it is read from.
std::string restore_of(restored_packages const& restored) {
    return std::format("the restore of the build ({})",
                       wxl::core::unicode::to_utf8(restored.source).chars());
}

// The version of a text that has been checked to be one, as it was read.
nuget_version version_of(std::string_view text) { return nuget_version::parse(text).value(); }

// <package directory>/<metadata dir>/<file>.winmd. The directory is "metadata"
// for the WindowsAppSDK packages, and whatever a package from outside the SDK
// ships its .winmd in -- Win2D puts it under lib/uap10.0.
std::vector<std::filesystem::path> resolve_package(package_ref const& package,
                                                   restored_package const& restored,
                                                   std::filesystem::path const& source) {
    auto const& version = restored.version;
    auto const dir = restored.directory /
                     path_of(package.metadata_dir.empty() ? "metadata" : package.metadata_dir);
    if (!std::filesystem::is_directory(dir)) {
        fail(source, std::format("package '{}' {}: no metadata directory at {}", package.id,
                                 version, wxl::core::unicode::to_utf8(dir).chars()));
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
            fail(source, std::format("package '{}' {}: no .winmd files in {}", package.id, version,
                                     wxl::core::unicode::to_utf8(dir).chars()));
        }
        return files;
    }

    for (auto&& name : package.metadata) {
        auto path = dir / path_of(name);
        if (!std::filesystem::is_regular_file(path)) {
            fail(source, std::format("package '{}' {}: no such metadata file: {}", package.id,
                                     version, wxl::core::unicode::to_utf8(path).chars()));
        }
        files.push_back(std::move(path));
    }
    return files;
}

// A package's non-metadata files, named relative to its root rather than to
// the metadata directory: a resource dictionary lives wherever the package
// ships it, not beside the .winmd.
std::vector<std::filesystem::path> resolve_resources(package_ref const& package,
                                                     restored_package const& restored,
                                                     std::filesystem::path const& source) {
    auto const& version = restored.version;
    auto const& root = restored.directory;

    std::vector<std::filesystem::path> files;
    for (auto&& name : package.resources) {
        auto path = root / path_of(name);
        if (!std::filesystem::is_regular_file(path)) {
            fail(source, std::format("package '{}' {}: no such resource file: {}", package.id,
                                     version, wxl::core::unicode::to_utf8(path).chars()));
        }
        files.push_back(std::move(path));
    }
    return files;
}

// A numeric identifier, or a numeric part of a version: digits, and at least
// one of them.
bool is_numeric(std::string_view text) {
    return !text.empty() &&
           std::ranges::all_of(text, [](char c) { return wxl::core::is_ascii_digit(c); });
}

// An identifier of a prerelease label or of build metadata: [0-9A-Za-z-], and
// at least one of them.
bool is_identifier(std::string_view text) {
    return !text.empty() && std::ranges::all_of(text, [](char c) {
               return wxl::core::is_ascii_digit(c) || (c >= 'A' && c <= 'Z') ||
                      (c >= 'a' && c <= 'z') || c == '-';
           });
}

// The whole of `text` as an Int32, or nothing: what Int32.TryParse takes of
// the characters a version is written in -- digits, after a '-' for a negative
// number, within the range of Int32.
std::optional<std::int32_t> int32_of(std::string_view text) {
    std::int32_t number = 0;
    auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return number;
}

// Two identifiers of prerelease labels, as NuGet's VersionComparer orders them
// (CompareRelease): one that Int32.TryParse takes is a number, and two numbers
// are compared as numbers; a number is below any text; text is compared
// character by character in upper case, which is how OrdinalIgnoreCase orders
// ASCII.
std::weak_ordering compare_identifiers(std::string_view a, std::string_view b) {
    auto const x_number = int32_of(a);
    auto const y_number = int32_of(b);
    if (x_number && y_number) {
        return *x_number <=> *y_number;
    }
    if (x_number || y_number) {
        return x_number ? std::weak_ordering::less : std::weak_ordering::greater;
    }
    for (std::size_t at = 0; at != std::min(a.size(), b.size()); ++at) {
        auto const x = static_cast<unsigned char>(wxl::core::to_ascii_upper(a[at]));
        auto const y = static_cast<unsigned char>(wxl::core::to_ascii_upper(b[at]));
        if (x != y) {
            return x <=> y;
        }
    }
    return a.size() <=> b.size();
}

}  // namespace

std::optional<nuget_version> nuget_version::parse(std::string_view text) {
    if (auto const plus = text.find('+'); plus != std::string_view::npos) {
        for (std::string_view const identifier : wxl::core::split(text.substr(plus + 1), '.')) {
            if (!is_identifier(identifier)) {
                return std::nullopt;
            }
        }
        text = text.substr(0, plus);
    }

    // The numeric parts are what System.Version parses: Int32 each, a leading
    // zero allowed.
    nuget_version version;
    auto const dash = text.find('-');
    std::size_t count = 0;
    for (std::string_view const part : wxl::core::split(text.substr(0, dash), '.')) {
        if (count == version.numbers_.size() || !is_numeric(part)) {
            return std::nullopt;
        }
        auto const number = int32_of(part);
        if (!number) {
            return std::nullopt;
        }
        version.numbers_[count++] = *number;
    }
    // A numeric identifier of the label has no leading zero: 0 is one, 00 and 01
    // are not (SemanticVersionFactory.IsValidPart).
    if (dash != std::string_view::npos) {
        for (std::string_view const identifier : wxl::core::split(text.substr(dash + 1), '.')) {
            if (!is_identifier(identifier) ||
                (identifier.size() > 1 && identifier.front() == '0' && is_numeric(identifier))) {
                return std::nullopt;
            }
            version.prerelease_.emplace_back(identifier);
        }
    }
    return version;
}

std::weak_ordering operator<=>(nuget_version const& a, nuget_version const& b) {
    if (auto const order = a.numbers_ <=> b.numbers_; order != 0) {
        return order;
    }
    if (a.prerelease_.empty() != b.prerelease_.empty()) {
        return a.prerelease_.empty() ? std::weak_ordering::greater : std::weak_ordering::less;
    }
    for (std::size_t at = 0; at != std::min(a.prerelease_.size(), b.prerelease_.size()); ++at) {
        if (auto const order = compare_identifiers(a.prerelease_[at], b.prerelease_[at]);
            order != 0) {
            return order;
        }
    }
    return a.prerelease_.size() <=> b.prerelease_.size();
}

std::filesystem::path resolve_reference(std::string_view reference,
                                        std::filesystem::path const& referrer) {
    auto path = path_of(reference);
    if (path.has_extension() || path.has_parent_path()) {
        return path.is_absolute() ? path : referrer.parent_path() / path;
    }
    path += L".winxl-profile";
    return referrer.parent_path() / path;
}

bool member_filter::allows(std::string_view name) const {
    switch (kind) {
        case kind_t::All:
            return true;
        case kind_t::None:
            return false;
        case kind_t::Allow:
            return names.count(std::string{name}) != 0;
        case kind_t::Deny:
            return names.count(std::string{name}) == 0;
    }
    return false;
}

void member_filter::merge(member_filter const& other) {
    if (kind == kind_t::All || other.kind == kind_t::None) {
        return;
    }
    if (other.kind == kind_t::All || kind == kind_t::None) {
        *this = other;
        return;
    }
    if (kind == kind_t::Allow && other.kind == kind_t::Allow) {
        names.insert(other.names.begin(), other.names.end());
        return;
    }
    if (kind == kind_t::Deny && other.kind == kind_t::Deny) {
        // Denied by both, i.e. allowed by neither.
        std::set<std::string> both;
        std::set_intersection(names.begin(), names.end(), other.names.begin(), other.names.end(),
                              std::inserter(both, both.end()));
        names = std::move(both);
        return;
    }

    // Allow(A) + Deny(B): everything except B, plus A -- that is,
    // Deny(B \ A).
    auto const& allow = kind == kind_t::Allow ? names : other.names;
    auto const& deny = kind == kind_t::Deny ? names : other.names;
    std::set<std::string> rest;
    std::set_difference(deny.begin(), deny.end(), allow.begin(), allow.end(),
                        std::inserter(rest, rest.end()));
    kind = kind_t::Deny;
    names = std::move(rest);
}

profile load_profile(std::filesystem::path const& path) {
    wxl::json::document file;
    auto const& document = read_json(file, path, "a profile must be a JSON object");

    profile prof;
    prof.source = path;
    prof.name = string_or(document, path, "name", wxl::core::unicode::to_utf8(path.stem()).chars());
    prof.description = string_or(document, path, "description", {});

    if (value const* const extends = document.find("extends")) {
        auto const names = read_string_set(*extends, path, "extends");
        prof.extends.assign(names.begin(), names.end());
    }

    // The release read is the one the build's restore resolved, so a key that
    // names one is a mistake to report.
    if (document.find("windowsAppSdk")) {
        fail(path, "'windowsAppSdk' is not a key of a profile: the Windows App SDK release read "
                   "is the one the build's NuGet restore resolves, and a profile whose types need "
                   "a certain release names the lowest one they need as 'minimumWindowsAppSdk'");
    }
    prof.minimum_windows_app_sdk = minimum_or_empty(document, path, "minimumWindowsAppSdk", {});

    if (value const* const packages = document.find("packages")) {
        if (!packages->is_array()) {
            fail(path, "'packages' must be an array");
        }
        for (value const& entry : packages->elements()) {
            prof.packages.push_back(read_package(entry, path));
        }
    }

    if (value const* const types = document.find("types")) {
        if (!types->is_object()) {
            fail(path, "'types' must be an object keyed by metadata type name");
        }
        for (value const& entry : types->members()) {
            std::string const name{entry.name().chars()};
            prof.types.emplace(name, read_member_filter(entry, path, name));
            value const* const styles = entry.find("styles");
            prof.styles.emplace(
                name, styles ? member_filter::allow(read_string_set(*styles, path, "styles"))
                             : member_filter::all());
            if (auto added = read_synthetic_members(entry, path, name); !added.empty()) {
                prof.synthetic.emplace(name, std::move(added));
            }
            if (auto setters = read_setter_methods(entry, path, name); !setters.empty()) {
                prof.setter_methods.emplace(name, std::move(setters));
            }
            if (auto replaced = read_replaced_calls(entry, path, name); !replaced.empty()) {
                prof.replaced_calls.emplace(name, std::move(replaced));
            }
            if (value const* const converted = entry.find("fromText")) {
                if (!converted->is_object() || !converted->find("function") || !converted->find("include")) {
                    fail(path, std::format("type '{}': 'fromText' must be an object with 'function' "
                                           "and 'include'",
                                           name));
                }
                prof.from_text[name] = {text(*converted->find("function")),
                                           text(*converted->find("include"))};
            }
        }
    }

    // Types the walk discovers on its own (through base classes and member
    // signatures) rather than finding listed: "none" generates the type
    // but none of its members, "all" generates everything it declares.
    if (auto const discovered = string_or(document, path, "discoveredTypes", "none");
        discovered == "all") {
        prof.discovered = member_filter::all();
    } else if (discovered == "none") {
        prof.discovered = member_filter::none();
    } else {
        fail(path, std::format("'discoveredTypes' must be \"none\" or \"all\", got \"{}\"",
                               discovered));
    }

    if (value const* const brushes = document.find("brushes")) {
        prof.brushes = member_filter::allow(read_string_set(*brushes, path, "brushes"));
    }

    prof.windows_metadata = bool_or(document, path, "windowsMetadata", false);
    return prof;
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
struct profile_loader {
    profile_set& result;
    std::set<std::filesystem::path>& loaded;   // canonical paths, load-once
    std::set<std::filesystem::path>& loading;  // cycle detection
    std::vector<profile>& in_load_order;
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
            throw std::runtime_error(std::format("profile reference cycle at {}",
                                                 wxl::core::unicode::to_utf8(key).chars()));
        }

        auto const prof = load_profile(path);
        for (auto&& reference : prof.extends) {
            load(resolve_reference(reference, prof.source));
        }

        // Versions and packages wait for the second pass: the release the set
        // needs is the highest minimum of all its profiles, and the check of
        // the release comes before the packages of the release are read.
        in_load_order.push_back(prof);

        for (auto&& [name, filter] : prof.types) {
            auto [it, inserted] = result.types.try_emplace(name, filter);
            if (!inserted) {
                it->second.merge(filter);
            }
            result.listed_by.try_emplace(name, prof.source);
        }
        // Synthetic members compose the way the rest of a profile does: an
        // extending profile adds to what it extends, and a name declared
        // twice is the same property, kept once.
        for (auto&& [name, added] : prof.synthetic) {
            auto& known = result.synthetic[name];
            for (auto&& member : added) {
                if (std::none_of(known.begin(), known.end(), [&](synthetic_member const& existing) {
                        return existing.name == member.name;
                    })) {
                    known.push_back(member);
                }
            }
        }
        for (auto&& [name, converted] : prof.from_text) {
            result.from_text[name] = converted;
        }
        // Replaced calls compose by the method's name, the extending profile winning.
        for (auto&& [name, calls] : prof.replaced_calls) {
            for (auto&& [method, call] : calls) {
                result.replaced_calls[name][method] = call;
            }
        }
        // Setter methods compose the same way, by the method's name.
        for (auto&& [name, setters] : prof.setter_methods) {
            auto& known = result.setter_methods[name];
            for (auto&& setter : setters) {
                if (std::none_of(known.begin(), known.end(), [&](setter_method const& existing) {
                        return existing.method == setter.method;
                    })) {
                    known.push_back(setter);
                }
            }
        }
        for (auto&& [name, filter] : prof.styles) {
            auto [it, inserted] = result.styles.try_emplace(name, filter);
            if (!inserted) {
                it->second.merge(filter);
            }
        }
        result.brushes.merge(prof.brushes);
        result.discovered.merge(prof.discovered);
        windows_metadata = windows_metadata || prof.windows_metadata;

        loading.erase(key);
        loaded.insert(key);
        result.loaded.push_back(prof.name);
    }
};

profile_set resolve_profiles(std::vector<std::filesystem::path> const& paths,
                             restored_packages const& restored) {
    profile_set result;

    std::set<std::filesystem::path> loaded;   // canonical paths, load-once
    std::set<std::filesystem::path> loading;  // cycle detection
    std::set<std::filesystem::path> metadata;
    std::set<std::filesystem::path> resources;
    std::vector<profile> in_load_order;
    bool windows_metadata = false;

    profile_loader loader{result, loaded, loading, in_load_order, windows_metadata};
    for (auto&& path : paths) {
        loader.load(path);
    }
    result.restored_from = restored.source;
    result.sdk = restored.sdk;
    result.windows_metadata = windows_metadata;
    if (restored_package const* const release = restored.find(windows_app_sdk_id)) {
        result.windows_app_sdk = release->version;
    }

    // The release the set needs is the highest of its minimums, named by the
    // first profile in load order that asks for it.
    profile const* needs_release = nullptr;
    for (auto&& prof : in_load_order) {
        if (!prof.minimum_windows_app_sdk.empty() &&
            (!needs_release || version_of(needs_release->minimum_windows_app_sdk) <
                                   version_of(prof.minimum_windows_app_sdk))) {
            needs_release = &prof;
        }
    }
    if (needs_release) {
        auto const& minimum = needs_release->minimum_windows_app_sdk;
        restored_package const* const release = restored.find(windows_app_sdk_id);
        if (!release) {
            fail(needs_release->source,
                 std::format("'minimumWindowsAppSdk' asks for {} {} or newer, and {} has no {}: "
                             "add a reference to {} {} or newer to the restore of the build (a "
                             "minimum in tools/packages.json) and run tools\\build.ps1 "
                             "-UpdatePackages in the wxl tree",
                             windows_app_sdk_id, minimum, restore_of(restored), windows_app_sdk_id,
                             windows_app_sdk_id, minimum));
        }
        if (version_of(release->version) < version_of(minimum)) {
            fail(needs_release->source,
                 std::format("'minimumWindowsAppSdk' asks for {} {} or newer, and {} resolved {} "
                             "{}: raise the minimum of {} in tools/packages.json and run "
                             "tools\\build.ps1 -UpdatePackages in the wxl tree",
                             windows_app_sdk_id, minimum, restore_of(restored), release->id,
                             release->version, windows_app_sdk_id));
        }
    }

    for (auto&& prof : in_load_order) {
        for (auto&& package : prof.packages) {
            restored_package const* const found = restored.find(package.id);
            if (!found) {
                auto const wanted = package.minimum_version.empty()
                                        ? package.id
                                        : std::format("{} {} or newer", package.id,
                                                      package.minimum_version);
                fail(prof.source,
                     std::format("package '{}' is not in {}: add a reference to {} to the restore "
                                 "of the build (a minimum in tools/packages.json) and run "
                                 "tools\\build.ps1 -UpdatePackages in the wxl tree",
                                 package.id, restore_of(restored), wanted));
            }
            if (!package.minimum_version.empty() &&
                version_of(found->version) < version_of(package.minimum_version)) {
                fail(prof.source,
                     std::format("package '{}': 'minimumVersion' asks for {} or newer, and {} "
                                 "resolved {}: raise the minimum in tools/packages.json and run "
                                 "tools\\build.ps1 -UpdatePackages in the wxl tree",
                                 package.id, package.minimum_version, restore_of(restored),
                                 found->version));
            }
            for (auto&& file : resolve_package(package, *found, prof.source)) {
                metadata.insert(file);
            }
            for (auto&& file : resolve_resources(package, *found, prof.source)) {
                resources.insert(file);
            }
        }
    }

    // The projected Windows platform metadata (Windows.Foundation.IReference,
    // IVector<T>, EventHandler<T>, ...): WinUI's own .winmd only *references*
    // those, and without them the walk stops at every such edge. The files are
    // those the build's projection of the platform was made from, so a
    // wrapper and the projection it calls see the same Windows SDK.
    if (windows_metadata) {
        for (auto&& file : restored.sdk.metadata) {
            if (!std::filesystem::is_regular_file(file)) {
                fail(restored.source,
                     std::format("Windows SDK {}: no metadata file {}; configure the build again",
                                 restored.sdk.version,
                                 wxl::core::unicode::to_utf8(file).chars()));
            }
            metadata.insert(file);
        }
    }

    result.metadata.assign(metadata.begin(), metadata.end());
    result.resources.assign(resources.begin(), resources.end());
    return result;
}

restored_package const* restored_packages::find(std::string_view id) const {
    for (auto&& package : packages) {
        if (wxl::core::equal_ignore_ascii_case(std::string_view{package.id}, id)) {
            return &package;
        }
    }
    return nullptr;
}

restored_packages load_restored_packages(std::filesystem::path const& path) {
    // Asked here rather than left to read_json: what is missing is a step of the
    // build, and the message says which.
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error(std::format(
            "no list of the restored packages at {}: the configure of a build writes it, as "
            "<build>/wxl.packages/restored-packages.json, from the result of its NuGet restore; "
            "configure the build first",
            wxl::core::unicode::to_utf8(path).chars()));
    }
    wxl::json::document file;
    auto const& document =
        read_json(file, path, "a list of the restored packages must be a JSON object");

    restored_packages restored;
    restored.source = path;

    value const& packages = document["packages"];
    if (!packages.is_array()) {
        fail(path, "'packages' must be an array");
    }
    for (value const& entry : packages.elements()) {
        if (!entry.is_object()) {
            fail(path, "every 'packages' entry must be an object");
        }
        restored_package package{required_string(entry, path, "id", "package"),
                                 required_string(entry, path, "version", "package"),
                                 path_of(required_string(entry, path, "directory", "package"))};
        if (!nuget_version::parse(package.version)) {
            fail(path, std::format("package '{}': \"{}\" is not a NuGet version", package.id,
                                   package.version));
        }
        if (restored.find(package.id)) {
            fail(path, std::format("package '{}' is listed twice", package.id));
        }
        restored.packages.push_back(std::move(package));
    }

    value const& sdk = document["windowsSdk"];
    if (!sdk.is_object() || !sdk["version"].is_string() || !sdk["metadata"].is_array() ||
        sdk["metadata"].elements().empty()) {
        fail(path, "'windowsSdk' must be an object with a string 'version' and an array "
                   "'metadata' of the .winmd files the projection of the platform is made from");
    }
    restored.sdk.version = text(sdk["version"]);
    // The name of a folder of the SDK's references as well (References\<version>),
    // so it is held to the form the configure writes it in: four numbers
    // separated by dots, 10.0.26100.0.
    std::size_t parts = 0;
    bool numeric = true;
    for (std::string_view const part :
         wxl::core::split(std::string_view{restored.sdk.version}, '.')) {
        ++parts;
        numeric = numeric && is_numeric(part);
    }
    if (parts != 4 || !numeric) {
        fail(path, std::format("'windowsSdk.version' is \"{}\", which is not a Windows SDK "
                               "version of four numbers separated by dots",
                               restored.sdk.version));
    }
    for (value const& entry : sdk["metadata"].elements()) {
        if (!entry.is_string()) {
            fail(path, "'windowsSdk.metadata' must contain strings only");
        }
        restored.sdk.metadata.push_back(path_of(text(entry)));
    }
    return restored;
}

type_map load_type_map(std::filesystem::path const& path) {
    wxl::json::document file;
    auto const& document = read_json(file, path, "a type map must be a JSON object");

    type_map map;
    if (value const* const listed = document.find("predefined")) {
        map.predefined = read_string_set(*listed, path, "predefined");
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

    if (value const* const members = document.find("handWrittenMembers")) {
        if (!members->is_object()) {
            fail(path, "'handWrittenMembers' must be an object");
        }
        if (value const* const array = members->find("properties")) {
            if (!array->is_array()) {
                fail(path, "'handWrittenMembers.properties' must be an array");
            }
            for (value const& entry : array->elements()) {
                if (!entry.is_object()) {
                    fail(path, "every hand-written property must be an object");
                }
                map.hand_written_properties.push_back(
                    {required_string(entry, path, "property", "hand-written property"),
                     required_string(entry, path, "value", "hand-written property"),
                     string_or(entry, path, "include", {})});
            }
        }
        if (value const* const events = members->find("events")) {
            map.hand_written_events = read_string_set(*events, path, "handWrittenMembers.events");
        }
        if (value const* const array = members->find("boundMembers")) {
            if (!array->is_array()) {
                fail(path, "'handWrittenMembers.boundMembers' must be an array");
            }
            for (value const& entry : array->elements()) {
                if (!entry.is_object()) {
                    fail(path, "every bound member must be an object");
                }
                std::string direction = required_string(entry, path, "direction", "bound member");
                if (direction != "input" && direction != "output" && direction != "both") {
                    fail(path, "a bound member's 'direction' is \"input\", \"output\" or \"both\"");
                }
                map.bound_members.push_back(
                    {required_string(entry, path, "class", "bound member"),
                     required_string(entry, path, "property", "bound member"),
                     required_string(entry, path, "value", "bound member"), std::move(direction),
                     string_or(entry, path, "include", {})});
            }
        }
    }
    return map;
}

std::vector<symbol> load_symbol_names(std::filesystem::path const& path) {
    wxl::json::document file;
    auto const& document = read_json(file, path, "a symbol table must be a JSON object");

    value const* const symbols = document.find("symbols");
    if (!symbols || !symbols->is_object()) {
        fail(path, "'symbols' must be an object of name -> code point");
    }

    std::vector<symbol> names;
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
    std::ranges::sort(names, {}, &symbol::code);
    return names;
}
