#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "crawl.h"
#include "xml_input.h"

// The documentation of the metadata, as the generator and the profile editor
// both read it: a member by its .NET documentation ID, out of the file that
// documents the type declaring it.

// The ID a .NET documentation file names a type by: "T:Ns.Type".
std::string documentation_id(winmd::reader::TypeDef const& type);

// The ID of a member of `owner`: "P:Ns.Type.Name", "E:", "F:", and for a method
// "M:Ns.Type.Name(System.String,Ns.Other@)" -- types by their full names, the
// built-in ones by their System names, a generic's arguments in braces, a
// parameter passed by reference (an out one) with @. `owner` is the type the
// file keys the member on: a class for the members of its interfaces, its
// statics and its factories alike.
std::string documentation_id(winmd::reader::TypeDef const& owner, member_declaration const& declaration,
                             std::string_view name);

// The ID of the constructor a factory method of `owner` stands for:
// "M:Ns.Type.#ctor(...)", with the method's parameters but the last `dropped`
// -- the pair a composable factory adds for composition.
std::string constructor_documentation_id(winmd::reader::TypeDef const& owner,
                                         winmd::reader::MethodDef const& method, std::size_t dropped);

// A language of the Windows SDK's reference: the name of its folder, which is
// what --doc-language takes, and the name Doxygen gives the same language in
// OUTPUT_LANGUAGE, which the documentation site is built in. The one list of
// them: the generator refuses a code missing here before it starts, its --help
// names these, and the site takes the Doxygen name from what the generator
// writes, so no second list can disagree with this one.
struct documentation_language {
    std::string_view code;
    std::string_view doxygen;
};

// In the order of the folders' names.
inline constexpr std::array<documentation_language, 10> documentation_languages {{
    {"de", "German"},
    {"en", "English"},
    {"es", "Spanish"},
    {"fr", "French"},
    {"it", "Italian"},
    {"jp", "Japanese"},
    {"ko", "Korean"},
    {"ru", "Russian"},
    {"zh-hans", "Chinese"},
    {"zh-hant", "Chinese-Traditional"},
}};

// The entry of documentation_languages with this code, or null.
documentation_language const* find_documentation_language(std::string_view code) noexcept;

// Where the documentation of a type is, read once per file and kept. The file
// is the .xml beside the type's .winmd, as Windows App SDK ships it, in
// English only; the metadata of Windows has nothing beside it, and its
// documentation is the Windows SDK's reference for the type's contract,
// References\<SDK version>\<contract>\<version>\<language>\<contract>.xml, in
// the first of the languages that has one. A language is the name of that
// folder, one of documentation_languages.
class documentation_source {
public:
    // `windows_sdk` is the version of the Windows SDK whose reference is read --
    // the one the build projects and reads the metadata of Windows from
    // (restored_packages::windows_sdk_version); a reference of another version
    // installed beside it is not read. `references` is the Windows SDK's
    // References directory; empty, it is found under the SDK's installed root
    // when first needed. A version that is empty or not UTF-8 throws
    // std::invalid_argument.
    documentation_source(std::vector<std::wstring> languages, std::string_view windows_sdk,
                         std::filesystem::path references = {});
    documentation_source(documentation_source const&) = delete;
    documentation_source& operator=(documentation_source const&) = delete;
    ~documentation_source();

    // The member of `type`'s documentation by its ID, or null. Good for as long
    // as the file it came from: until languages() closes a reference file.
    //
    // A file that does not read, or a member of it that does not parse,
    // throws std::runtime_error naming the file; the error stays with the
    // file, and every later find() in it -- or one in a file whose error
    // parse_some() met -- throws it again.
    member_documentation const* find(winmd::reader::TypeDef const& type, std::string_view id);

    // The reference files open now are closed, and the next need opens one in
    // these languages. What lies beside the .winmd files stays.
    void languages(std::vector<std::wstring> languages);

    // Parses members of the open files not parsed yet until the deadline
    // passes; whether any are left. A member that does not parse ends its
    // file's parsing, and the error waits for the next find() in that file.
    bool parse_some(std::chrono::steady_clock::time_point deadline);

    // Whether an open file still has members not parsed.
    bool parsing() const noexcept { return !unfinished_.empty(); }

    // The metadata files, by path, whose types were documented in a language
    // other than the first -- the reference had no file in it for their
    // contract -- and those with types no file documents at all.
    std::set<std::string> const& fell_back() const noexcept { return fell_back_; }
    std::set<std::string> const& undocumented() const noexcept { return undocumented_; }

private:
    // One documentation file, or the error that stops it being read.
    struct source {
        std::unique_ptr<documentation_file> file;
        std::exception_ptr error;
    };

    struct reference {
        source found;
        bool first_language = true;
    };

    source* source_of(winmd::reader::TypeDef const& type);
    // `image_root` is what the images of the file's descriptions are relative to.
    static source open(std::filesystem::path const& file, std::string_view image_root);
    reference open_reference(std::string const& contract);
    void started(source& opened);  // a file with members to parse joins unfinished_

    std::vector<std::wstring> languages_;
    std::filesystem::path windows_sdk_;  // the folder of the version under references_
    std::filesystem::path references_;
    std::map<winmd::reader::database const*, source> beside_;
    std::map<std::string, reference> contracts_;
    // The reference of each metadata file and contract, settled once: that is
    // when a fall back or a missing file is noted.
    std::map<std::pair<winmd::reader::database const*, std::string>, source*> resolved_;
    std::vector<source*> unfinished_;  // in the order they were opened
    std::set<std::string> fell_back_;
    std::set<std::string> undocumented_;
};
