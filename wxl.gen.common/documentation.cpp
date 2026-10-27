#include <format>

#include "documentation.h"

import std;
import wxl.core;

namespace md = winmd::reader;

namespace {

// The ID as it is spelled, one part at a time.
class id_writer {
public:
    void head(char kind, md::TypeDef const& owner, std::string_view name) {
        text_ = std::format("{}:{}.{}.{}", kind, owner.TypeNamespace(), owner.TypeName(), name);
    }

    // The parameter list, of all but the last `dropped`; none at all is no parentheses.
    void parameters(md::MethodDef const& method, std::size_t dropped) {
        auto const signature = method.Signature();
        std::size_t count = 0;
        for (auto&& param : signature.Params()) {
            static_cast<void>(param);
            ++count;
        }
        count = count > dropped ? count - dropped : 0;
        std::size_t written = 0;
        for (auto&& param : signature.Params()) {
            if (written == count) {
                break;
            }
            text_ += written++ == 0 ? '(' : ',';
            type(param.Type());
            if (param.ByRef()) {
                text_ += '@';
            }
        }
        if (written > 0) {
            text_ += ')';
        }
    }

    std::string text() && { return std::move(text_); }

private:
    void type(md::TypeSig const& signature) {
        std::visit([this](auto const& value) { element(value); }, signature.Type());
        if (signature.is_szarray()) {
            text_ += "[]";
        }
    }

    void element(md::coded_index<md::TypeDefOrRef> const& ref) {
        if (ref.type() == md::TypeDefOrRef::TypeSpec) {
            element(ref.TypeSpec().Signature().GenericTypeInst());
            return;
        }
        auto const [ns, name] = md::get_type_namespace_and_name(ref);
        text_ += std::format("{}.{}", ns, name);
    }

    void element(md::GenericTypeInstSig const& instance) {
        auto const [ns, name] = md::get_type_namespace_and_name(instance.GenericType());
        text_ += std::format("{}.{}{{", ns, name.substr(0, name.find('`')));
        bool first = true;
        for (auto&& argument : instance.GenericArgs()) {
            text_ += first ? "" : ",";
            first = false;
            type(argument);
        }
        text_ += '}';
    }

    void element(md::GenericTypeIndex index) { text_ += std::format("`{}", index.index); }
    void element(md::GenericMethodTypeIndex index) { text_ += std::format("``{}", index.index); }

    void element(md::ElementType element) {
        switch (element) {
            case md::ElementType::Boolean: text_ += "System.Boolean"; break;
            case md::ElementType::Char: text_ += "System.Char"; break;
            case md::ElementType::I1: text_ += "System.SByte"; break;
            case md::ElementType::U1: text_ += "System.Byte"; break;
            case md::ElementType::I2: text_ += "System.Int16"; break;
            case md::ElementType::U2: text_ += "System.UInt16"; break;
            case md::ElementType::I4: text_ += "System.Int32"; break;
            case md::ElementType::U4: text_ += "System.UInt32"; break;
            case md::ElementType::I8: text_ += "System.Int64"; break;
            case md::ElementType::U8: text_ += "System.UInt64"; break;
            case md::ElementType::R4: text_ += "System.Single"; break;
            case md::ElementType::R8: text_ += "System.Double"; break;
            case md::ElementType::String: text_ += "System.String"; break;
            case md::ElementType::Object: text_ += "System.Object"; break;
            default: break;
        }
    }

    std::string text_;
};

// The Windows SDK's references: References\<SDK version>\<contract>\<version>\<language>.
std::filesystem::path sdk_references() {
    wchar_t root[MAX_PATH] {};
    DWORD size = sizeof(root);
    if (::RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", L"KitsRoot10",
                       RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, nullptr, root, &size) != ERROR_SUCCESS) {
        return {};
    }
    return std::filesystem::path {root} / L"References";
}

// The newest of the version folders: names like 10.0.26100.0 and 19.0.0.0. A directory that
// is not there has none; one the file system will not walk throws, as stepping through it
// and asking what an item is do on their own.
std::filesystem::path newest_version(std::filesystem::path const& directory) {
    std::filesystem::path newest;
    std::vector<int> best;
    std::error_code error;
    std::filesystem::directory_iterator items {directory, error};
    if (error == std::errc::no_such_file_or_directory) {
        return {};
    }
    if (error) {
        throw std::filesystem::filesystem_error("directory_iterator", directory, error);
    }
    for (auto&& item : items) {
        if (!item.is_directory()) {
            continue;
        }
        std::vector<int> parts;
        for (auto&& part : std::views::split(item.path().filename().wstring(), L'.')) {
            parts.push_back(std::wcstol(std::wstring {part.begin(), part.end()}.c_str(), nullptr, 10));
        }
        if (parts > best) {
            best = std::move(parts);
            newest = item.path();
        }
    }
    return newest;
}

// The metadata file a database was read from. The reader keeps the name it was opened by
// in UTF-8, its contract, and a name that is not UTF-8 is said rather than taken as a path.
std::filesystem::path metadata_file(md::database const& database) {
    auto const name = wxl::core::unicode::checked(database.path());
    if (!name) {
        throw std::runtime_error(
            std::format("metadata opened by a name that is not UTF-8: {}",
                        wxl::core::unicode::repaired(database.path()).chars()));
    }
    return wxl::core::unicode::to_path(*name);
}

// The contract a Windows type belongs to: its ContractVersionAttribute.
std::string contract_of(md::TypeDef const& type) {
    for (auto&& attribute : type.CustomAttribute()) {
        if (!is_attribute(attribute, metadata_namespace, "ContractVersionAttribute")) {
            continue;
        }
        for (auto&& argument : attribute.Value().FixedArgs()) {
            if (auto const* element = std::get_if<md::ElemSig>(&argument.value)) {
                if (auto const* contract = std::get_if<md::ElemSig::SystemType>(&element->value)) {
                    return std::string {contract->name};
                }
            }
        }
    }
    return {};
}

}  // namespace

std::string documentation_id(md::TypeDef const& type) {
    return std::format("T:{}.{}", type.TypeNamespace(), type.TypeName());
}

std::string documentation_id(md::TypeDef const& owner, member_declaration const& declaration, std::string_view name) {
    id_writer id;
    std::visit(
        [&]<typename Definition>(Definition const& definition) {
            if constexpr (std::is_same_v<Definition, md::Property>) {
                id.head('P', owner, name);
            } else if constexpr (std::is_same_v<Definition, md::Event>) {
                id.head('E', owner, name);
            } else if constexpr (std::is_same_v<Definition, md::Field>) {
                id.head('F', owner, name);
            } else {
                id.head('M', owner, name);
                id.parameters(definition, 0);
            }
        },
        declaration.definition);
    return std::move(id).text();
}

std::string constructor_documentation_id(md::TypeDef const& owner, md::MethodDef const& method, std::size_t dropped) {
    id_writer id;
    id.head('M', owner, "#ctor");
    id.parameters(method, dropped);
    return std::move(id).text();
}

documentation_language const* find_documentation_language(std::string_view code) noexcept {
    for (auto&& language : documentation_languages) {
        if (language.code == code) {
            return &language;
        }
    }
    return nullptr;
}

documentation_source::documentation_source(std::vector<std::wstring> languages, std::filesystem::path references)
    : languages_(std::move(languages)), references_(std::move(references)) {}

documentation_source::~documentation_source() = default;

member_documentation const* documentation_source::find(md::TypeDef const& type, std::string_view id) {
    source& found = *source_of(type);
    if (found.error) {
        std::rethrow_exception(found.error);
    }
    if (!found.file) {
        return nullptr;
    }
    try {
        return found.file->find(id);
    } catch (...) {
        found.error = std::current_exception();
        throw;
    }
}

void documentation_source::languages(std::vector<std::wstring> languages) {
    if (languages == languages_) {
        return;
    }
    languages_ = std::move(languages);
    std::erase_if(unfinished_, [this](source const* open) {
        return std::ranges::any_of(contracts_, [open](auto const& each) { return &each.second.found == open; });
    });
    contracts_.clear();
    resolved_.clear();
}

bool documentation_source::parse_some(std::chrono::steady_clock::time_point deadline) {
    while (!unfinished_.empty()) {
        source& next = *unfinished_.front();
        try {
            if (next.file->parse_some(deadline)) {
                return true;
            }
        } catch (...) {
            next.error = std::current_exception();
        }
        unfinished_.erase(unfinished_.begin());
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }
    return !unfinished_.empty();
}

// A file is found by the metadata file, and for the metadata of Windows by the
// contract; each is looked for and read once. An entry is kept only once it is
// settled -- the file, the error that stops it, or that there is none -- so a
// lookup that throws halfway leaves nothing behind to pass for an answer.
documentation_source::source* documentation_source::source_of(md::TypeDef const& type) {
    auto const& database = type.get_database();
    auto beside = beside_.find(&database);
    if (beside == beside_.end()) {
        source opened = open(metadata_file(database).replace_extension(L".xml"), app_sdk_image_root);
        beside = beside_.emplace(&database, std::move(opened)).first;
        started(beside->second);
    }
    if (beside->second.file || beside->second.error) {
        return &beside->second;
    }

    std::pair const key {&database, contract_of(type)};
    if (auto const settled = resolved_.find(key); settled != resolved_.end()) {
        return settled->second;
    }
    auto contract = contracts_.find(key.second);
    if (contract == contracts_.end()) {
        // What the search of the reference throws stays with the contract,
        // as the error of a file does.
        reference opened;
        try {
            opened = open_reference(key.second);
        } catch (std::filesystem::filesystem_error const& error) {
            opened.found.error = std::make_exception_ptr(std::runtime_error(
                std::string {wxl::core::environment::error_message(error).chars()}));
        } catch (...) {
            opened.found.error = std::current_exception();
        }
        contract = contracts_.emplace(key.second, std::move(opened)).first;
        started(contract->second.found);
    }
    reference const& entry = contract->second;
    if (!entry.found.file && !entry.found.error) {
        undocumented_.insert(std::string {database.path()});
    } else if (!entry.first_language) {
        fell_back_.insert(std::string {database.path()});
    }
    resolved_.emplace(key, &contract->second.found);
    return &contract->second.found;
}

// Whether the file is there is asked inside the try too: exists() throws for
// any answer but "not found" -- access denied, a network path gone -- and that
// is an error about this file, kept with it like one met reading it.
documentation_source::source documentation_source::open(std::filesystem::path const& file,
                                                         std::string_view image_root) {
    if (file.empty()) {
        return {};
    }
    source opened;
    try {
        if (!std::filesystem::exists(file)) {
            return {};
        }
        opened.file = std::make_unique<documentation_file>(file, std::string {image_root});
    } catch (std::filesystem::filesystem_error const& error) {
        opened.error = std::make_exception_ptr(std::runtime_error(
            std::string {wxl::core::environment::error_message(error).chars()}));
    } catch (...) {
        // The file names itself in what it throws, as its reader does, so the
        // error is kept as thrown.
        opened.error = std::current_exception();
    }
    return opened;
}

void documentation_source::started(source& opened) {
    if (opened.file && opened.file->size() > 0) {
        unfinished_.push_back(&opened);
    }
}

documentation_source::reference documentation_source::open_reference(std::string const& contract) {
    if (contract.empty()) {
        return {};
    }
    if (references_.empty()) {
        references_ = sdk_references();
    }
    // No SDK, or no version of it, or no folder of the contract: no reference, and nothing
    // wrong -- that is the type undocumented.
    if (references_.empty()) {
        return {};
    }
    std::filesystem::path const sdk = newest_version(references_);
    if (sdk.empty()) {
        return {};
    }
    std::filesystem::path const versions = newest_version(sdk / metadata_path(contract));
    if (versions.empty()) {
        return {};
    }
    for (std::wstring const& language : languages_) {
        source found = open(versions / language / metadata_path(contract + ".xml"), windows_sdk_image_root);
        if (found.file || found.error) {
            return {std::move(found), &language == &languages_.front()};
        }
    }
    return {};
}
