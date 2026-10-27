#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "documentation.h"

// After documentation.h, whose platform.h brings <windows.h> ahead of it.
#include <sddl.h>

import wxl.core;

namespace md = winmd::reader;

namespace {

// u8, so that the path reads the directory as UTF-8 and not in the ANSI code page.
std::filesystem::path const profiles_dir{u8"" WXL_GEN_PROFILES_DIR};

// A path as the errors spell it, and as the metadata reader takes it.
std::string utf8(std::filesystem::path const& path) {
    return std::string{wxl::core::unicode::to_utf8(path).chars()};
}

std::vector<std::filesystem::path> const& rich_metadata_files() {
    static std::vector<std::filesystem::path> const files =
        resolve_profiles({profiles_dir / "rich.json"}, default_nuget_root()).metadata;
    return files;
}

std::vector<std::string> const& rich_metadata() {
    static std::vector<std::string> const files = [] {
        std::vector<std::string> paths;
        for (auto&& file : rich_metadata_files()) {
            paths.push_back(utf8(file));
        }
        return paths;
    }();
    return files;
}

md::cache const& metadata() {
    static md::cache const db{rich_metadata()};
    return db;
}

md::TypeDef type(std::string_view name) {
    return metadata().find_required(name);
}

template <typename List>
auto named(List const& list, std::string_view name) {
    for (auto&& member : list) {
        if (member.Name() == name) {
            return member;
        }
    }
    throw std::runtime_error(std::string{name} + " is not declared there");
}

// The ID a member of `iface` gets as a member of `owner`, which is how the
// generator keys what a class's interfaces contribute.
std::string method_id(std::string_view owner, std::string_view iface, std::string_view method) {
    return documentation_id(type(owner), {type(owner), named(type(iface).MethodList(), method)}, method);
}

std::filesystem::path scratch(std::wstring_view name) {
    auto const directory = std::filesystem::temp_directory_path() / L"wxl.gen.common.documentation" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

void write_documentation(std::filesystem::path const& file, std::string_view id, std::string_view summary) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream{file} << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<doc><members>\n<member name=\"" << id
                        << "\"><summary>" << summary << "</summary></member>\n</members></doc>\n";
}

}  // namespace

// The codes --doc-language takes, each with the name Doxygen builds the site in,
// and no row besides them.
TEST(documentation_languages, names_doxygens_language_for_each_code) {
    documentation_language const expected[] {
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
    };
    EXPECT_EQ(documentation_languages.size(), std::size(expected));
    for (auto&& [code, doxygen] : expected) {
        SCOPED_TRACE(code);
        auto const language = find_documentation_language(code);
        ASSERT_NE(language, nullptr);
        EXPECT_EQ(language->doxygen, doxygen);
    }
}

// What the generator's arguments refuse: a code is the reference's folder,
// spelled as the folder is.
TEST(documentation_languages, knows_no_other_code) {
    EXPECT_EQ(find_documentation_language("pt"), nullptr);
    EXPECT_EQ(find_documentation_language("RU"), nullptr);
    EXPECT_EQ(find_documentation_language(""), nullptr);
}

TEST(documentation_id, names_a_type) {
    EXPECT_EQ(documentation_id(type("Microsoft.UI.Xaml.Controls.Button")), "T:Microsoft.UI.Xaml.Controls.Button");
}

// A member an interface declares is keyed on the class that implements it.
TEST(documentation_id, keys_a_property_and_an_event_on_their_class) {
    auto const button = type("Microsoft.UI.Xaml.Controls.Button");
    auto const flyout = named(type("Microsoft.UI.Xaml.Controls.IButton").PropertyList(), "Flyout");
    EXPECT_EQ(documentation_id(button, {button, flyout}, "Flyout"), "P:Microsoft.UI.Xaml.Controls.Button.Flyout");

    auto const base = type("Microsoft.UI.Xaml.Controls.Primitives.ButtonBase");
    auto const click = named(type("Microsoft.UI.Xaml.Controls.Primitives.IButtonBase").EventList(), "Click");
    EXPECT_EQ(documentation_id(base, {base, click}, "Click"), "E:Microsoft.UI.Xaml.Controls.Primitives.ButtonBase.Click");
}

TEST(documentation_id, spells_the_parameters_of_a_method) {
    // An out parameter, with @.
    EXPECT_EQ(method_id("Microsoft.UI.Xaml.Automation.Provider.ITextProvider2",
                        "Microsoft.UI.Xaml.Automation.Provider.ITextProvider2", "GetCaretRange"),
              "M:Microsoft.UI.Xaml.Automation.Provider.ITextProvider2.GetCaretRange(System.Boolean@)");
    // A static one, keyed on the class and not on its statics interface.
    EXPECT_EQ(method_id("Microsoft.UI.Xaml.Controls.Grid", "Microsoft.UI.Xaml.Controls.IGridStatics", "SetRow"),
              "M:Microsoft.UI.Xaml.Controls.Grid.SetRow(Microsoft.UI.Xaml.FrameworkElement,System.Int32)");
    // A generic argument, in braces.
    auto const peer = type("Microsoft.UI.Xaml.Automation.Peers.AutomationPeer");
    auto const raise = named(peer.MethodList(), "RaiseTextEditTextChangedEvent");
    EXPECT_EQ(documentation_id(peer, {peer, raise}, "RaiseTextEditTextChangedEvent"),
              "M:Microsoft.UI.Xaml.Automation.Peers.AutomationPeer.RaiseTextEditTextChangedEvent("
              "Microsoft.UI.Xaml.Automation.AutomationTextEditChangeType,"
              "Windows.Foundation.Collections.IVectorView{System.String})");
}

// A composable factory's method takes the pair composition adds after its own
// parameters; the constructor it stands for does not.
TEST(documentation_id, names_a_constructor_by_its_own_parameters) {
    auto const peer = type("Microsoft.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer");
    auto const create =
        named(type("Microsoft.UI.Xaml.Automation.Peers.IFrameworkElementAutomationPeerFactory").MethodList(),
              "CreateInstanceWithOwner");
    EXPECT_EQ(constructor_documentation_id(peer, create, 2),
              "M:Microsoft.UI.Xaml.Automation.Peers.FrameworkElementAutomationPeer.#ctor(Microsoft.UI.Xaml.FrameworkElement)");
}

// The IDs are the ones the documentation shipped with the metadata uses.
TEST(documentation_source, finds_what_beside_the_metadata_documents) {
    documentation_source docs{{L"en"}};
    auto const button = type("Microsoft.UI.Xaml.Controls.Button");
    auto const flyout = named(type("Microsoft.UI.Xaml.Controls.IButton").PropertyList(), "Flyout");

    auto const found = docs.find(button, documentation_id(button, {button, flyout}, "Flyout"));
    ASSERT_TRUE(found);
    EXPECT_FALSE(found->summary.empty());

    auto const grid = type("Microsoft.UI.Xaml.Controls.Grid");
    auto const set_row = docs.find(grid, method_id("Microsoft.UI.Xaml.Controls.Grid",
                                                   "Microsoft.UI.Xaml.Controls.IGridStatics", "SetRow"));
    ASSERT_TRUE(set_row);
    ASSERT_EQ(set_row->params.size(), 2u);
    EXPECT_EQ(set_row->params[0].first, "element");
}

// The .xml beside a .winmd is the type's documentation whatever the language:
// Windows App SDK ships it in English alone, and nothing falls back.
TEST(documentation_source, reads_beside_the_metadata_in_any_language) {
    auto const directory = scratch(L"beside");
    std::filesystem::path text_winmd;
    for (auto&& file : rich_metadata_files()) {
        if (file.filename() == L"Microsoft.UI.Text.winmd") {
            text_winmd = file;
        }
    }
    ASSERT_FALSE(text_winmd.empty());
    std::filesystem::copy_file(text_winmd, directory / "Microsoft.UI.Text.winmd");
    write_documentation(directory / "Microsoft.UI.Text.xml", "T:Microsoft.UI.Text.CaretType", "Stub.");

    md::cache const db{std::vector<std::string>{utf8(directory / "Microsoft.UI.Text.winmd")}};
    documentation_source docs{{L"ru", L"en"}, directory / "no-references"};
    auto const caret = db.find_required("Microsoft.UI.Text.CaretType");
    auto const found = docs.find(caret, "T:Microsoft.UI.Text.CaretType");
    ASSERT_TRUE(found);
    EXPECT_EQ(found->summary, "Stub.");
    EXPECT_TRUE(docs.fell_back().empty());
}

// A Windows type is documented by the reference of its contract, in the first
// language that has a file, and a later one is said.
TEST(documentation_source, reads_the_reference_in_the_first_language_it_has) {
    auto const references = scratch(L"references");
    auto const contract = references / "10.0.1.0" / "Windows.Foundation.UniversalApiContract" / "1.0.0.0";
    write_documentation(contract / "en" / "Windows.Foundation.UniversalApiContract.xml", "T:Windows.UI.Color",
                        "A color.");
    write_documentation(contract / "ru" / "Windows.Foundation.UniversalApiContract.xml", "T:Windows.UI.Color",
                        "Цвет.");

    std::filesystem::path const root{wxl::core::environment::variable(L"SystemRoot")};
    ASSERT_FALSE(root.empty());
    auto const winmd = root / "System32" / "WinMetadata" / "Windows.UI.winmd";
    md::cache const db{std::vector<std::string>{utf8(winmd)}};
    auto const color = db.find_required("Windows.UI.Color");

    documentation_source russian{{L"ru", L"en"}, references};
    EXPECT_EQ(russian.find(color, "T:Windows.UI.Color")->summary, "Цвет.");
    EXPECT_TRUE(russian.fell_back().empty());

    documentation_source german{{L"de", L"en"}, references};
    EXPECT_EQ(german.find(color, "T:Windows.UI.Color")->summary, "A color.");
    EXPECT_EQ(german.fell_back(), std::set<std::string>{utf8(winmd)});

    // Another language closes the reference and opens it again in that one.
    german.languages({L"ru", L"en"});
    EXPECT_EQ(german.find(color, "T:Windows.UI.Color")->summary, "Цвет.");

    // No file in any language: said of the metadata file, not passed over.
    documentation_source none{{L"en"}, references / "empty"};
    EXPECT_FALSE(none.find(color, "T:Windows.UI.Color"));
    EXPECT_EQ(none.undocumented(), std::set<std::string>{utf8(winmd)});
    EXPECT_TRUE(none.fell_back().empty());
}

// A file that does not parse stops the documentation of its types: the error
// names the file, and every later find() in it throws it again -- whether
// find() met it first or parse_some() did.
TEST(documentation_source, keeps_the_error_of_a_file) {
    std::filesystem::path const root{wxl::core::environment::variable(L"SystemRoot")};
    ASSERT_FALSE(root.empty());
    auto const winmd = root / "System32" / "WinMetadata" / "Windows.UI.winmd";
    md::cache const db{std::vector<std::string>{utf8(winmd)}};
    auto const color = db.find_required("Windows.UI.Color");

    auto const references = scratch(L"broken");
    auto const file = references / "10.0.1.0" / "Windows.Foundation.UniversalApiContract" / "1.0.0.0" / "en" /
                      "Windows.Foundation.UniversalApiContract.xml";
    std::filesystem::create_directories(file.parent_path());
    std::ofstream{file} << "<doc><members><member name=\"T:Windows.UI.Color\"><summary>Open</member>"
                           "<member name=\"T:Windows.UI.Colors\"><summary>Fine.</summary></member></members></doc>";

    auto const throws_naming_the_file = [&](documentation_source& docs, std::string_view id) {
        try {
            docs.find(color, id);
            ADD_FAILURE() << "no error for " << id;
        } catch (std::runtime_error const& error) {
            EXPECT_TRUE(std::string_view{error.what()}.contains(utf8(file))) << error.what();
        }
    };

    documentation_source asked{{L"en"}, references};
    throws_naming_the_file(asked, "T:Windows.UI.Color");
    throws_naming_the_file(asked, "T:Windows.UI.Colors");

    documentation_source parsed{{L"en"}, references};
    EXPECT_EQ(parsed.find(color, "T:Windows.UI.Colors")->summary, "Fine.");
    EXPECT_TRUE(parsed.parsing());
    while (parsed.parse_some(std::chrono::steady_clock::now() + std::chrono::seconds{1})) {
    }
    EXPECT_FALSE(parsed.parsing());
    throws_naming_the_file(parsed, "T:Windows.UI.Colors");
}

// A file that cannot be opened -- here one truncated mid-member -- stops the
// documentation of its types as well, with the file's own error: the file
// named once, and the offset of the member that broke it.
TEST(documentation_source, keeps_the_error_of_a_file_truncated) {
    std::filesystem::path const root{wxl::core::environment::variable(L"SystemRoot")};
    ASSERT_FALSE(root.empty());
    auto const winmd = root / "System32" / "WinMetadata" / "Windows.UI.winmd";
    md::cache const db{std::vector<std::string>{utf8(winmd)}};
    auto const color = db.find_required("Windows.UI.Color");

    auto const references = scratch(L"cut");
    auto const file = references / "10.0.1.0" / "Windows.Foundation.UniversalApiContract" / "1.0.0.0" / "en" /
                      "Windows.Foundation.UniversalApiContract.xml";
    std::filesystem::create_directories(file.parent_path());
    std::ofstream{file} << "<doc><members><member name=\"T:Windows.UI.Color\"><summary>Cut";

    documentation_source docs{{L"en"}, references};
    for (int ask = 0; ask < 2; ++ask) {
        try {
            docs.find(color, "T:Windows.UI.Color");
            ADD_FAILURE() << "no error";
        } catch (std::runtime_error const& error) {
            std::string_view const what = error.what();
            std::string const name = utf8(file);
            EXPECT_TRUE(what.starts_with(name)) << what;
            EXPECT_EQ(what.find(name), what.rfind(name)) << what;
            EXPECT_TRUE(what.contains("documentation member at offset 14 is truncated")) << what;
        }
    }
    EXPECT_FALSE(docs.parsing());
}

namespace {

// Every access to the paths refused while it lives -- an empty DACL, which the
// owner may still replace -- and each one's own DACL back after, last refused
// first. Get/SetFileSecurityW rather than SetNamedSecurityInfoW, which would
// walk the children of a directory it has just made unreadable. A file in a
// directory that refuses it too is one the file system will not say is there
// or not, and exists() throws for it, where a missing file, or a path through
// a regular file, is only "not found".
class refused {
public:
    explicit refused(std::vector<std::filesystem::path> const& paths) {
        PSECURITY_DESCRIPTOR nobody = nullptr;
        if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P", SDDL_REVISION_1, &nobody, nullptr)) {
            ADD_FAILURE() << "no descriptor: " << ::GetLastError();
            return;
        }
        for (auto const& path : paths) {
            DWORD size = 0;
            ::GetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &size);
            std::vector<std::byte> own(size);
            if (!::GetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION, own.data(), size, &size) ||
                !::SetFileSecurityW(path.c_str(), DACL_SECURITY_INFORMATION, nobody)) {
                ADD_FAILURE() << "cannot refuse " << utf8(path) << ": " << ::GetLastError();
                break;
            }
            kept_.push_back({path, std::move(own)});
        }
        ::LocalFree(nobody);
    }
    refused(refused const&) = delete;
    refused& operator=(refused const&) = delete;
    ~refused() {
        for (auto each = kept_.rbegin(); each != kept_.rend(); ++each) {
            EXPECT_TRUE(::SetFileSecurityW(each->path.c_str(), DACL_SECURITY_INFORMATION, each->own.data()))
                << "cannot give " << utf8(each->path) << " its permissions back: " << ::GetLastError();
        }
    }

private:
    struct kept {
        std::filesystem::path path;
        std::vector<std::byte> own;  // the path's own security descriptor, its DACL in it
    };
    std::vector<kept> kept_;
};

// find() throws an error that starts with the file's name, and asked again
// throws the same one.
void expect_the_same_error_twice(documentation_source& docs, md::TypeDef const& type,
                                 std::filesystem::path const& file) {
    std::string first;
    for (int ask = 0; ask < 2; ++ask) {
        try {
            docs.find(type, "T:Windows.UI.Color");
            ADD_FAILURE() << "no error when asked " << ask + 1 << " times";
        } catch (std::runtime_error const& error) {
            std::string_view const what = error.what();
            EXPECT_TRUE(what.starts_with(utf8(file))) << what;
            if (ask == 0) {
                first = what;
            } else {
                EXPECT_EQ(what, first);
            }
        }
    }
}

}  // namespace

// A file the file system will not even say is there stops the documentation of
// its types as one that does not read does: the error names the file, stays
// with it, and the next find() throws it again -- beside a .winmd, where it
// must not fall through to the reference, and as the reference of a contract.
TEST(documentation_source, keeps_the_error_of_a_file_refused_beside_the_metadata) {
    std::filesystem::path const root{wxl::core::environment::variable(L"SystemRoot")};
    ASSERT_FALSE(root.empty());
    auto const directory = scratch(L"refused_beside") / "metadata";
    std::filesystem::create_directories(directory);
    auto const winmd = directory / "Windows.UI.winmd";
    std::filesystem::copy_file(root / "System32" / "WinMetadata" / "Windows.UI.winmd", winmd);
    md::cache const db{std::vector<std::string>{utf8(winmd)}};
    auto const color = db.find_required("Windows.UI.Color");

    auto const file = directory / "Windows.UI.xml";
    write_documentation(file, "T:Windows.UI.Color", "A color.");
    refused const denied{{file, directory}};
    documentation_source docs{{L"en"}, scratch(L"refused_beside_references")};
    expect_the_same_error_twice(docs, color, file);
}

TEST(documentation_source, keeps_the_error_of_a_reference_refused) {
    std::filesystem::path const root{wxl::core::environment::variable(L"SystemRoot")};
    ASSERT_FALSE(root.empty());
    auto const winmd = root / "System32" / "WinMetadata" / "Windows.UI.winmd";
    md::cache const db{std::vector<std::string>{utf8(winmd)}};
    auto const color = db.find_required("Windows.UI.Color");

    auto const references = scratch(L"refused");
    auto const language = references / "10.0.1.0" / "Windows.Foundation.UniversalApiContract" / "1.0.0.0" / "en";
    auto const file = language / "Windows.Foundation.UniversalApiContract.xml";
    write_documentation(file, "T:Windows.UI.Color", "A color.");
    refused const denied{{file, language}};
    documentation_source docs{{L"en"}, references};
    expect_the_same_error_twice(docs, color, file);
}

// A folder of the references the file system will not walk is the error of the contract's
// reference, naming the folder -- not the types quietly undocumented.
TEST(documentation_source, keeps_the_error_of_a_reference_folder_refused) {
    std::filesystem::path const root{wxl::core::environment::variable(L"SystemRoot")};
    ASSERT_FALSE(root.empty());
    auto const winmd = root / "System32" / "WinMetadata" / "Windows.UI.winmd";
    md::cache const db{std::vector<std::string>{utf8(winmd)}};
    auto const color = db.find_required("Windows.UI.Color");

    auto const references = scratch(L"refused_folder");
    auto const contract = references / "10.0.1.0" / "Windows.Foundation.UniversalApiContract";
    write_documentation(contract / "1.0.0.0" / "en" / "Windows.Foundation.UniversalApiContract.xml",
                        "T:Windows.UI.Color", "A color.");
    refused const denied{{contract}};
    documentation_source docs{{L"en"}, references};
    expect_the_same_error_twice(docs, color, contract);
    EXPECT_TRUE(docs.undocumented().empty());
}
