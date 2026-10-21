#pragma once

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

// Where the documentation of a type is, read once per file and kept. The file
// is the .xml beside the type's .winmd, as Windows App SDK ships it, in
// English only; the metadata of Windows has nothing beside it, and its
// documentation is the Windows SDK's reference for the type's contract,
// References\<SDK version>\<contract>\<version>\<language>\<contract>.xml, in
// the first of the languages that has one. A language is the name of that
// folder: en, ru, de, jp, zh-hans, zh-hant.
class documentation_source {
public:
    // `references` is the Windows SDK's References directory; empty, it is
    // found under the SDK's installed root when first needed.
    explicit documentation_source(std::vector<std::wstring> languages, std::filesystem::path references = {});
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
    static source open(std::filesystem::path const& file);
    reference open_reference(std::string const& contract);
    void started(source& opened);  // a file with members to parse joins unfinished_

    std::vector<std::wstring> languages_;
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
