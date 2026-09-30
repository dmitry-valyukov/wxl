// Метаданные и профиль — только здесь: crawl.h приносит winmd_reader.h вместе с
// <windows.h> в нужном порядке, а окно видит одни модели деревьев.
#include "crawl.h"
#include "profile.h"
#include "xml_input.h"

#include "Editor.h"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {

using wxl::core::intrusive_ptr;
namespace md = winmd::reader;

namespace {

// Значки строк: знаки размера 16 шрифта Fluent UI System Icons
// (Assets/FluentSystemIcons-Regular.ttf), коды — из его
// FluentSystemIcons-Regular.json. Заливка —
// тёмный край, светлая середина.
using wxl::rgb;
constexpr RowIcon libraryIcon {0xE761, rgb(136, 14, 79), rgb(248, 187, 208)};    // library: бордово-розовый
constexpr RowIcon namespaceIcon {0xE058, rgb(13, 71, 161), rgb(128, 222, 234)};  // app_folder: сине-голубой
constexpr RowIcon classIcon {0xF132, rgb(109, 55, 16), rgb(255, 167, 38)};       // apps: коричнево-оранжевый
constexpr RowIcon structIcon {0xE202, rgb(27, 94, 32), rgb(255, 204, 170)};      // broad_activity_feed: зелёно-персиковый
constexpr RowIcon enumIcon {0xE779, rgb(74, 20, 140), rgb(186, 104, 200)};       // list: фиолетовый
constexpr RowIcon propertyIcon {0xEE85, rgb(66, 66, 66), rgb(189, 189, 189)};    // wrench: серый
constexpr RowIcon methodIcon {0xF334, rgb(49, 27, 146), rgb(209, 196, 233)};     // cube: тёмно-светло-фиолетовый
constexpr RowIcon eventIcon {0xE617, rgb(230, 81, 0), rgb(255, 235, 59)};        // flash: оранжево-жёлтый
constexpr RowIcon constantIcon {0xF0544, rgb(74, 20, 140), rgb(186, 104, 200)};  // storage: как у перечисления
constexpr RowIcon styleIcon {0xF355, rgb(0, 96, 100), rgb(178, 235, 242)};       // design_ideas: тёмно-светло-бирюзовый
constexpr RowIcon brushIcon {0xF591, rgb(183, 28, 28), rgb(255, 171, 145)};      // paint_brush: красно-персиковый

// Текст метаданных в разметку HtmlBlock: знаки разметки — сущностями.
void append_text(std::wstring& html, std::string_view utf8) {
    std::wstring wide;
    if (auto const text = wxl::core::unicode::checked(utf8)) {
        wxl::core::unicode::append_utf16(wide, *text);
    }
    for (wchar_t const c : wide) {
        switch (c) {
            case L'<': html += L"&lt;"; break;
            case L'>': html += L"&gt;"; break;
            case L'&': html += L"&amp;"; break;
            default: html += c; break;
        }
    }
}

void append_bold(std::wstring& html, std::string_view utf8) {
    html += L"<b>";
    append_text(html, utf8);
    html += L"</b>";
}

// Что профиль говорит о членах типа.
void append_filter(std::wstring& html, MemberFilter const* filter) {
    html += L"<p>Profile: ";
    if (!filter || filter->kind == MemberFilter::Kind::None) {
        html += L"not listed</p>";
        return;
    }
    if (filter->kind == MemberFilter::Kind::All) {
        html += L"all members</p>";
        return;
    }
    html += filter->kind == MemberFilter::Kind::Allow ? L"only " : L"all members except ";
    bool first = true;
    for (std::string const& name : filter->names) {
        html += first ? L"" : L", ";
        append_text(html, name);
        first = false;
    }
    if (first) {
        html += L"no members";
    }
    html += L"</p>";
}

std::u16string to_u16(std::string_view utf8) {
    std::wstring wide;
    if (auto const text = wxl::core::unicode::checked(utf8)) {
        wxl::core::unicode::append_utf16(wide, *text);
    }
    return {wide.begin(), wide.end()};
}

// Отметка члена в фильтре типа. Тип, которого в профиле нет, входит в него со
// списком из одного этого члена — кроме перечисления: не названное профилем, оно
// выходит со всеми значениями, и снятая отметка вносит его запрещающим списком.
// У «всех членов» снятая отметка делает список запрещающим; разрешающий и
// запрещающий списки просто пополняются и худеют. Разрешающий, опустев, уходит
// из профиля вместе с типом, если стилей тип не генерирует: тип без членов и
// без стилей не генерирует ничего. Стили типа из профиля без своей записи —
// все его стили, и styled говорит, есть ли они у него в словарях.
void mark_member(Profile& profile, std::string const& type, std::string_view member, bool on,
                 bool unlistedKeepsAll, bool styled) {
    auto& types = profile.types;
    std::string const name {member};
    auto const found = types.find(type);
    if (found == types.end()) {
        if (on != unlistedKeepsAll) {
            types.emplace(type, on ? MemberFilter::allow({name}) : MemberFilter::deny({name}));
        }
        return;
    }

    MemberFilter& filter = found->second;
    switch (filter.kind) {
        case MemberFilter::Kind::All:
        case MemberFilter::Kind::None:
            filter = on ? MemberFilter::all() : MemberFilter::deny({name});
            break;
        case MemberFilter::Kind::Allow:
            if (on) {
                filter.names.insert(name);
            } else {
                filter.names.erase(name);
                auto const styles = profile.styles.find(type);
                bool const generatesStyles = styles == profile.styles.end()
                                                 ? styled
                                                 : styles->second.kind != MemberFilter::Kind::Allow ||
                                                       !styles->second.names.empty();
                if (filter.names.empty() && !generatesStyles) {
                    types.erase(found);
                    if (styles != profile.styles.end()) {
                        profile.styles.erase(styles);
                    }
                }
            }
            break;
        case MemberFilter::Kind::Deny:
            if (on) {
                filter.names.erase(name);
                if (filter.names.empty()) {
                    filter = MemberFilter::all();
                }
            } else {
                filter.names.insert(name);
            }
            break;
    }
}

}  // namespace

namespace {

// Язык интерфейса — именем папки справочника Windows SDK: ru, de, jp,
// zh-hans; справочник на английском есть всегда.
std::vector<std::wstring> documentation_languages() {
    std::vector<std::wstring> languages;
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] {};
    if (::LCIDToLocaleName(::GetUserDefaultUILanguage(), locale, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        std::wstring name {locale};
        std::ranges::transform(name, name.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        std::wstring_view const language = std::wstring_view {name}.substr(0, name.find(L'-'));
        if (language == L"zh") {
            bool const traditional = name == L"zh-tw" || name == L"zh-hk" || name == L"zh-mo" || name.contains(L"hant");
            languages.emplace_back(traditional ? L"zh-hant" : L"zh-hans");
        } else if (language == L"ja") {
            languages.emplace_back(L"jp");
        } else {
            languages.emplace_back(name);
            languages.emplace_back(language);
        }
    }
    languages.emplace_back(L"en");
    return languages;
}

// Справочники Windows SDK: References\<версия SDK>\<контракт>\<версия>\<язык>.
std::filesystem::path sdk_references() {
    wchar_t root[MAX_PATH] {};
    DWORD size = sizeof(root);
    if (::RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", L"KitsRoot10",
                       RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, nullptr, root, &size) != ERROR_SUCCESS) {
        return {};
    }
    return std::filesystem::path {root} / L"References";
}

// Самая новая из версий-папок: имена вида 10.0.26100.0 и 19.0.0.0.
std::filesystem::path newest_version(std::filesystem::path const& directory) {
    std::filesystem::path newest;
    std::vector<int> best;
    std::error_code error;
    for (auto&& item : std::filesystem::directory_iterator {directory, error}) {
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

// Контракт, которому принадлежит тип Windows: ContractVersionAttribute.
std::string contract_of(md::TypeDef const& type) {
    for (auto&& attribute : type.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (ns != "Windows.Foundation.Metadata" || name != "ContractVersionAttribute") {
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

// Документация метаданных, по файлу при первой нужде. Файл — .xml рядом с
// .winmd, как её кладёт Windows App SDK; у метаданных Windows рядом ничего нет,
// и их документация — справочник Windows SDK по контракту типа, на языке
// интерфейса, а без него на английском.
class Documentation {
public:
    std::optional<MemberDocumentation> find(md::TypeDef const& type, std::string const& id) {
        DocumentationFile const* const file = fileOf(type);
        return file ? file->find(id) : std::nullopt;
    }

private:
    using File = std::unique_ptr<DocumentationFile>;

    // Файл находится по файлу метаданных, а у метаданных Windows — по
    // контракту; и тот, и другой ищется и читается один раз.
    DocumentationFile const* fileOf(md::TypeDef const& type) {
        auto const& database = type.get_database();
        auto [beside, fresh] = beside_.try_emplace(&database);
        if (fresh) {
            beside->second = open(std::filesystem::path {database.path()}.replace_extension(L".xml"));
        }
        if (beside->second) {
            return beside->second.get();
        }

        auto [contract, added] = contracts_.try_emplace(contract_of(type));
        if (added) {
            contract->second = open(sdkFile(contract->first));
        }
        return contract->second.get();
    }

    static File open(std::filesystem::path const& file) {
        if (file.empty() || !std::filesystem::exists(file)) {
            return {};
        }
        try {
            return std::make_unique<DocumentationFile>(file);
        } catch (std::exception const&) {
            // Без документации сведения остаются сигнатурой.
            return {};
        }
    }

    std::filesystem::path sdkFile(std::string const& contract) {
        if (contract.empty()) {
            return {};
        }
        if (references_.empty()) {
            references_ = sdk_references();
        }
        std::filesystem::path const versions = newest_version(newest_version(references_) / contract);
        if (versions.empty()) {
            return {};
        }
        for (std::wstring const& language : languages_) {
            auto const file = versions / language / (contract + ".xml");
            if (std::filesystem::exists(file)) {
                return file;
            }
        }
        return {};
    }

    std::vector<std::wstring> const languages_ = documentation_languages();
    std::filesystem::path references_;
    std::map<md::database const*, File> beside_;
    std::map<std::string, File> contracts_;
};

}  // namespace

struct Editor::Data {
    Data(Profile own, std::vector<std::string> const& files)
        : profile(std::move(own)), db(files) {}

    // Отметки — то, что говорит сам файл, без профилей, которые он продолжает.
    Profile profile;
    md::cache db;

    // Ресурсы словарей XAML с ключами, как их объявил документ; модель ресурсов
    // смотрит в их строки.
    std::vector<DictionaryResource> resources;

    // Полные имена типов, у которых в словарях есть стили.
    std::set<std::string, std::less<>> styled;

    Documentation documentation;
};

// Тип в левом дереве; адрес постоянен, пока жив редактор.
struct TypeEntry {
    md::TypeDef def;
    RowIcon const* icon = nullptr;
    bool listed = false;
};

// Левое дерево: файлы winmd профиля, в них — пространства имён, в тех — типы.
class TypesModel final : public TreeModel {
public:
    explicit TypesModel(Editor& editor) : editor_(editor) {
        auto const& db = editor_.data_->db;
        for (md::database const& source : db.databases()) {
            // Имя файла без .winmd: в дереве библиотека зовётся по своему имени.
            std::string_view const path = source.path();
            std::string_view const file = path.substr(path.find_last_of("\\/") + 1);
            libraries_.push_back({.source = &source, .name = file.substr(0, file.rfind('.'))});
        }
        std::ranges::sort(libraries_, {}, &Library::name);

        // Тип находит свой файл по адресу базы, из которой прочитан.
        std::vector<std::pair<md::database const*, Library*>> bySource;
        bySource.reserve(libraries_.size());
        for (Library& library : libraries_) {
            bySource.emplace_back(library.source, &library);
        }
        std::ranges::sort(bySource, {}, &std::pair<md::database const*, Library*>::first);

        for (auto&& [name, members] : db.namespaces()) {
            if (name.empty()) {
                continue;
            }
            // Показываются только классы, структуры и перечисления; пространство,
            // где их нет, не показывается вовсе. Списки кэша, а не категория типа:
            // атрибуты — тоже классы, а контракты — структуры, и кэш держит их
            // отдельно.
            for (auto&& [kind, icon] : {std::pair {&members.classes, &classIcon},
                                        std::pair {&members.structs, &structIcon},
                                        std::pair {&members.enums, &enumIcon}}) {
                for (auto&& def : *kind) {
                    Library& library = *std::ranges::lower_bound(
                        bySource, &def.get_database(), {}, &std::pair<md::database const*, Library*>::first)->second;
                    // Пространства идут по имени, поэтому своё у файла — последнее.
                    if (library.namespaces.empty() || library.namespaces.back().name != name) {
                        library.namespaces.push_back({.name = name});
                    }
                    library.namespaces.back().types.push_back({def, icon});
                }
            }
        }

        std::erase_if(libraries_, [](Library const& library) { return library.namespaces.empty(); });
        for (Library& library : libraries_) {
            for (Namespace& space : library.namespaces) {
                std::ranges::sort(space.types, {}, [](TypeEntry const& entry) { return entry.def.TypeName(); });
            }
        }

        for (auto&& [type, filter] : editor_.data_->profile.types) {
            if (TypeEntry* entry = find(type)) {
                entry->listed = true;
            }
        }
        recount();
    }

    uint32_t size() const override { return size_; }

    TreeRow row(uint32_t index) const override {
        auto const [library, space, type] = locate(index);
        if (!space) {
            return {.text = library->name,
                    .icon = &libraryIcon,
                    .expander = library->expanded ? Expander::Expanded : Expander::Collapsed,
                    .selected = library == selected_.library && !selected_.space};
        }
        if (!type) {
            return {.text = space->name,
                    .depth = 1,
                    .icon = &namespaceIcon,
                    .expander = space->expanded ? Expander::Expanded : Expander::Collapsed,
                    .selected = space == selected_.space && !selected_.type};
        }

        return {.text = type->def.TypeName(),
                .depth = 2,
                .icon = type->icon,
                .check = type->listed ? Check::Checked : Check::Unchecked,
                .selected = type == selected_.type};
    }

    void toggleExpanded(uint32_t index) override {
        auto const [library, space, type] = locate(index);
        if (!space) {
            library->expanded = !library->expanded;
        } else if (!type) {
            space->expanded = !space->expanded;
        } else {
            return;
        }
        recount();
    }

    void toggleChecked(uint32_t index) override {
        TypeEntry* const entry = locate(index).type;
        if (!entry) {
            return;
        }

        Profile& profile = editor_.data_->profile;
        std::string const name = full_name(entry->def);
        if (entry->listed) {
            profile.types.erase(name);
            profile.styles.erase(name);
        } else {
            profile.types.emplace(name, MemberFilter::all());
        }
        entry->listed = !entry->listed;
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override;
    std::wstring describe() const override;

    // Тип внесли в профиль или вынесли из него не отсюда — отметкой стиля.
    void relist(std::string_view full) {
        if (TypeEntry* entry = find(full)) {
            entry->listed = editor_.data_->profile.types.contains(std::string {full});
        }
    }

    // Есть ли тип в дереве: только на такой ведёт ссылка из сведений.
    bool shows(std::string_view full) const { return find(full) != nullptr; }

    // Раскрывает файл и пространство типа и выбирает его; номер его строки
    // среди видимых.
    wxl::core::nullable<uint32_t> reveal(std::string_view full) {
        TypeEntry* const entry = find(full);
        if (!entry) {
            return {};
        }
        for (Library& library : libraries_) {
            for (Namespace& space : library.namespaces) {
                if (entry >= space.types.data() && entry < space.types.data() + space.types.size()) {
                    library.expanded = true;
                    space.expanded = true;
                    recount();
                    uint32_t const index = space.start + 1 + static_cast<uint32_t>(entry - space.types.data());
                    invoke(index);
                    return index;
                }
            }
        }
        return {};
    }

private:
    struct Namespace {
        std::string_view name;
        std::vector<TypeEntry> types;
        bool expanded = false;
        uint32_t start = 0;  // номер строки самого пространства среди видимых
    };

    struct Library {
        md::database const* source = nullptr;
        std::string_view name;  // имя файла winmd
        std::vector<Namespace> namespaces;
        bool expanded = false;
        uint32_t start = 0;  // номер строки самого файла среди видимых
    };

    // Строка дерева: файл, а под ним — пространство и тип, если строка их.
    struct Location {
        Library* library;
        Namespace* space;
        TypeEntry* type;
    };

    static std::string full_name(md::TypeDef const& type) {
        return std::format("{}.{}", type.TypeNamespace(), type.TypeName());
    }

    // Номера строк у свёрнутого не пересчитываются: к ним не спускаются.
    void recount() {
        uint32_t start = 0;
        for (Library& library : libraries_) {
            library.start = start++;
            if (!library.expanded) {
                continue;
            }
            for (Namespace& space : library.namespaces) {
                space.start = start++;
                start += space.expanded ? static_cast<uint32_t>(space.types.size()) : 0;
            }
        }
        size_ = start;
    }

    Location locate(uint32_t index) const {
        auto& library = const_cast<Library&>(*std::prev(std::ranges::upper_bound(libraries_, index, {}, &Library::start)));
        if (index == library.start) {
            return {&library, nullptr, nullptr};
        }
        auto& space = *std::prev(std::ranges::upper_bound(library.namespaces, index, {}, &Namespace::start));
        if (index == space.start) {
            return {&library, &space, nullptr};
        }
        return {&library, &space, &space.types[index - space.start - 1]};
    }

    TypeEntry* find(std::string_view full) { return const_cast<TypeEntry*>(std::as_const(*this).find(full)); }

    TypeEntry const* find(std::string_view full) const {
        md::TypeDef const def = editor_.data_->db.find(full);
        if (!def) {
            return nullptr;
        }
        auto const library = std::ranges::find(libraries_, &def.get_database(), &Library::source);
        if (library == libraries_.end()) {
            return nullptr;
        }
        auto const space = std::ranges::lower_bound(library->namespaces, def.TypeNamespace(), {}, &Namespace::name);
        if (space == library->namespaces.end() || space->name != def.TypeNamespace()) {
            return nullptr;
        }
        auto const type = std::ranges::lower_bound(
            space->types, def.TypeName(), {}, [](TypeEntry const& entry) { return entry.def.TypeName(); });
        if (type == space->types.end() || type->def != def) {
            return nullptr;
        }
        return &*type;
    }

    static std::wstring libraryInfo(Library const& library) {
        size_t types = 0;
        for (Namespace const& space : library.namespaces) {
            types += space.types.size();
        }
        std::wstring html = L"library ";
        append_bold(html, library.name);
        html += L"<br>";
        append_text(html, library.source->path());
        html += std::format(L"<p>{} namespaces, {} classes, structs and enums</p>", library.namespaces.size(), types);
        return html;
    }

    static std::wstring namespaceInfo(Library const& library, Namespace const& space) {
        size_t classes = 0;
        size_t structs = 0;
        size_t enums = 0;
        for (TypeEntry const& type : space.types) {
            (type.icon == &classIcon ? classes : type.icon == &structIcon ? structs : enums) += 1;
        }
        std::wstring html = L"namespace ";
        append_bold(html, space.name);
        html += L"<br>Library ";
        append_text(html, library.name);
        html += std::format(L"<p>{} classes, {} structs, {} enums</p>", classes, structs, enums);
        return html;
    }

    std::wstring typeInfo(Namespace const& space, TypeEntry const& type) const;

    Editor& editor_;
    std::vector<Library> libraries_;
    uint32_t size_ = 0;
    Location selected_ {};  // выбранная строка: файл, а под ним — пространство и тип
};

namespace {

// Ключ документации .NET, которым файлы документации называют члены:
// "M:Ns.Type.Method(System.String,Ns.Other@)". Типы — полными именами,
// встроенные — именами System, аргументы обобщённого — в фигурных скобках,
// параметр по ссылке (выходной) — с @.
class DocumentationId {
public:
    static std::string of(md::TypeDef const& type) { return std::format("T:{}.{}", type.TypeNamespace(), type.TypeName()); }

    static std::string of(md::TypeDef const& owner, MemberDeclaration const& declaration, std::string_view name) {
        DocumentationId id;
        std::visit([&](auto const& definition) { id.member(owner, definition, name); }, declaration.definition);
        return std::move(id.text_);
    }

private:
    void head(char kind, md::TypeDef const& owner, std::string_view name) {
        text_ = std::format("{}:{}.{}.{}", kind, owner.TypeNamespace(), owner.TypeName(), name);
    }

    void member(md::TypeDef const& owner, md::Property const&, std::string_view name) { head('P', owner, name); }
    void member(md::TypeDef const& owner, md::Event const&, std::string_view name) { head('E', owner, name); }
    void member(md::TypeDef const& owner, md::Field const&, std::string_view name) { head('F', owner, name); }

    void member(md::TypeDef const& owner, md::MethodDef const& method, std::string_view name) {
        head('M', owner, name);
        auto const signature = method.Signature();
        bool first = true;
        for (auto&& param : signature.Params()) {
            text_ += first ? '(' : ',';
            first = false;
            type(param.Type());
            if (param.ByRef()) {
                text_ += '@';
            }
        }
        if (!first) {
            text_ += ')';
        }
    }

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

// Документация члена или типа под сведениями о нём.
void append_documentation(std::wstring& html, MemberDocumentation const& documentation) {
    if (!documentation.deprecated.empty()) {
        html += L"<p><b>Deprecated:</b> ";
        append_text(html, documentation.deprecated);
        html += L"</p>";
    }
    if (!documentation.summary.empty()) {
        html += L"<p><b>Summary:</b><br>";
        append_text(html, documentation.summary);
        html += L"</p>";
    }
    // Параметр без описания — такие в файлах есть — не показывается.
    auto const described = [](auto const& param) { return !param.second.empty(); };
    if (std::ranges::any_of(documentation.params, described)) {
        html += L"<p><b>Parameters:</b>";
        for (auto const& [name, text] : documentation.params | std::views::filter(described)) {
            html += L"<br><i>";
            append_text(html, name);
            html += L"</i> — ";
            append_text(html, text);
        }
        html += L"</p>";
    }
    if (!documentation.returns.empty()) {
        html += L"<p><b>Returns:</b><br>";
        append_text(html, documentation.returns);
        html += L"</p>";
    }
}

// Сигнатура члена в разметку сведений. Типы — полными именами; тип, который
// есть в левом дереве, — ссылкой на него.
class SignatureWriter {
public:
    SignatureWriter(std::wstring& html, TypesModel const& types, md::TypeDef const& source)
        : html_(html), types_(types), source_(source) {}

    void write(MemberDeclaration const& declaration, std::string_view name) {
        std::visit([this, name](auto const& definition) { member(definition, name); }, declaration.definition);
    }

    void type(md::coded_index<md::TypeDefOrRef> const& ref) {
        if (ref.type() == md::TypeDefOrRef::TypeSpec) {
            generic(ref.TypeSpec().Signature().GenericTypeInst());
            return;
        }
        auto const [ns, name] = md::get_type_namespace_and_name(ref);
        // У обобщённого имени в метаданных число параметров после `.
        std::string const full = std::format("{}.{}", ns, name.substr(0, name.find('`')));
        if (types_.shows(full)) {
            html_ += L"<a href=\"";
            append_text(html_, full);
            html_ += L"\">";
            append_text(html_, full);
            html_ += L"</a>";
        } else {
            append_text(html_, full);
        }
    }

private:
    // Доступ и виртуальность есть только у объявления в самом классе: у
    // интерфейса все члены открытые и абстрактные. У свойства и события они —
    // у метода доступа.
    void modifiers(md::MethodDef const& method) {
        if (md::get_category(source_) != md::category::class_type) {
            return;
        }
        auto const flags = method.Flags();
        html_ += flags.Access() == md::MemberAccess::Family ? L"protected " : L"public ";
        if (flags.Static()) {
            html_ += L"static ";
        } else if (flags.Virtual() && !flags.Final()) {
            html_ += L"overridable ";
        }
    }

    void member(md::Property const& property, std::string_view name) {
        bool setter = false;
        for (auto&& semantic : property.MethodSemantic()) {
            setter = setter || semantic.Semantic().Setter();
            if (semantic.Semantic().Getter()) {
                modifiers(semantic.Method());
            }
        }
        type(property.Type().Type());
        html_ += L' ';
        append_bold(html_, name);
        html_ += setter ? L" { get; set; }" : L" { get; }";
    }

    void member(md::MethodDef const& method, std::string_view name) {
        modifiers(method);
        auto const signature = method.Signature();
        if (signature.ReturnType()) {
            type(signature.ReturnType().Type());
        } else {
            html_ += L"void";
        }
        html_ += L' ';
        append_bold(html_, name);
        html_ += L'(';
        uint16_t sequence = 0;
        for (auto&& param : signature.Params()) {
            ++sequence;
            html_ += sequence > 1 ? L", " : L"";
            // Имя и направление параметра — в таблице Param, по номеру.
            md::Param definition;
            for (auto&& each : method.ParamList()) {
                if (each.Sequence() == sequence) {
                    definition = each;
                }
            }
            if (definition && definition.Flags().Out()) {
                html_ += L"out ";
            }
            type(param.Type());
            if (definition) {
                html_ += L" <i>";
                append_text(html_, definition.Name());
                html_ += L"</i>";
            }
        }
        html_ += L')';
    }

    void member(md::Event const& event, std::string_view name) {
        for (auto&& semantic : event.MethodSemantic()) {
            if (semantic.Semantic().AddOn()) {
                modifiers(semantic.Method());
            }
        }
        html_ += L"event ";
        type(event.EventType());
        html_ += L' ';
        append_bold(html_, name);
    }

    void member(md::Field const& field, std::string_view name) {
        append_bold(html_, name);
        if (auto const constant = field.Constant()) {
            std::visit(
                [this](auto const& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char16_t>) {
                        html_ += std::format(L" = {}", value);
                    }
                },
                constant.Value());
        }
    }

    void type(md::TypeSig const& signature) {
        std::visit([this](auto const& value) { element(value); }, signature.Type());
        if (signature.is_szarray()) {
            html_ += L"[]";
        }
    }

    void generic(md::GenericTypeInstSig const& instance) {
        type(instance.GenericType());
        html_ += L"&lt;";
        bool first = true;
        for (auto&& argument : instance.GenericArgs()) {
            html_ += first ? L"" : L", ";
            first = false;
            type(argument);
        }
        html_ += L"&gt;";
    }

    void element(md::coded_index<md::TypeDefOrRef> const& ref) { type(ref); }
    void element(md::GenericTypeInstSig const& instance) { generic(instance); }

    // Параметр обобщённого интерфейса, где член объявлен, — по его имени.
    void element(md::GenericTypeIndex index) {
        uint32_t number = 0;
        for (auto&& param : source_.GenericParam()) {
            if (number++ == index.index) {
                append_text(html_, param.Name());
                return;
            }
        }
        html_ += L"T";
    }

    void element(md::GenericMethodTypeIndex) { html_ += L"T"; }

    void element(md::ElementType element) {
        switch (element) {
            case md::ElementType::Boolean: html_ += L"Boolean"; break;
            case md::ElementType::Char: html_ += L"Char16"; break;
            case md::ElementType::I1: html_ += L"Int8"; break;
            case md::ElementType::U1: html_ += L"UInt8"; break;
            case md::ElementType::I2: html_ += L"Int16"; break;
            case md::ElementType::U2: html_ += L"UInt16"; break;
            case md::ElementType::I4: html_ += L"Int32"; break;
            case md::ElementType::U4: html_ += L"UInt32"; break;
            case md::ElementType::I8: html_ += L"Int64"; break;
            case md::ElementType::U8: html_ += L"UInt64"; break;
            case md::ElementType::R4: html_ += L"Single"; break;
            case md::ElementType::R8: html_ += L"Double"; break;
            case md::ElementType::String: html_ += L"String"; break;
            case md::ElementType::Object: html_ += L"Object"; break;
            default: html_ += L"?"; break;
        }
    }

    std::wstring& html_;
    TypesModel const& types_;
    md::TypeDef const& source_;
};

}  // namespace

std::wstring TypesModel::typeInfo(Namespace const& space, TypeEntry const& type) const {
    std::wstring html;
    if (type.icon == &classIcon) {
        html += type.def.Flags().Sealed() ? L"sealed class " : L"class ";
    } else {
        html += type.icon == &structIcon ? L"struct " : L"enum ";
    }
    append_bold(html, type.def.TypeName());

    // Классы WinRT наследуют System.Object; базу называем, только если она
    // своя.
    if (type.icon == &classIcon) {
        if (auto const base = type.def.Extends(); base && base.type() != md::TypeDefOrRef::TypeSpec) {
            auto const [ns, name] = md::get_type_namespace_and_name(base);
            if (ns != "System" || name != "Object") {
                html += L" : ";
                SignatureWriter {html, *this, type.def}.type(base);
            }
        }
    }

    html += L"<br>Member of ";
    append_bold(html, space.name);

    auto const& types = editor_.data_->profile.types;
    auto const found = types.find(full_name(type.def));
    append_filter(html, found == types.end() ? nullptr : &found->second);
    if (auto const documentation = editor_.data_->documentation.find(type.def, DocumentationId::of(type.def))) {
        append_documentation(html, *documentation);
    }
    return html;
}

class MembersModel final : public TreeModel {
public:
    MembersModel(Editor& editor, TypeEntry& entry)
        : editor_(editor),
          entry_(entry),
          name_(std::format("{}.{}", entry.def.TypeNamespace(), entry.def.TypeName())),
          enum_(md::get_category(entry.def) == md::category::enum_type) {
        auto declared = declared_members_of(entry.def);
        if (enum_) {
            groups_.push_back({"Values", MemberKind::Constant, &constantIcon, std::move(declared.constants)});
        } else {
            groups_.push_back({"Properties", MemberKind::Property, &propertyIcon, std::move(declared.properties)});
            groups_.push_back({"Methods", MemberKind::Method, &methodIcon, std::move(declared.methods)});
            groups_.push_back({"Events", MemberKind::Event, &eventIcon, std::move(declared.events)});
        }
        // Пустая группа не показывается.
        std::erase_if(groups_, [](Group const& group) { return group.names.empty(); });
    }

    uint32_t size() const override {
        uint32_t size = 0;
        for (Group const& group : groups_) {
            size += 1 + group.shown();
        }
        return size;
    }

    TreeRow row(uint32_t index) const override {
        auto const [group, member] = locate(index);
        if (member == npos) {
            return {.text = group->title,
                    .icon = group->icon,
                    .expander = group->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = groupCheck(*group)};
        }

        std::string_view const name = group->names[member];
        return {.text = name,
                .depth = 1,
                .icon = group->icon,
                .check = allows(name) ? Check::Checked : Check::Unchecked,
                .selected = group == selected_.first && member == selected_.second};
    }

    void toggleExpanded(uint32_t index) override {
        auto const [group, member] = locate(index);
        if (member == npos) {
            group->expanded = !group->expanded;
        }
    }

    void toggleChecked(uint32_t index) override {
        auto const [group, member] = locate(index);
        Profile& profile = editor_.data_->profile;
        bool const styled = editor_.data_->styled.contains(name_);
        if (member == npos) {
            // Отмеченная целиком группа снимается, иначе отмечается целиком.
            bool const on = groupCheck(*group) != Check::Checked;
            for (std::string_view const name : group->names) {
                if (allows(name) != on) {
                    mark_member(profile, name_, name, on, enum_, styled);
                }
            }
        } else {
            std::string_view const name = group->names[member];
            mark_member(profile, name_, name, !allows(name), enum_, styled);
        }
        entry_.listed = profile.types.contains(name_);
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override {
        auto const [group, member] = locate(index);
        if (member == npos) {
            return;
        }
        selected_ = {group, member};
        editor_.selection.set({intrusive_ptr<TreeModel> {this}, &group->names[member]});
    }

    std::wstring describe() const override {
        auto const [group, member] = selected_;
        if (!group) {
            return {};
        }

        // Метод — каждой перегрузкой, строкой на каждую. Класс объявляет член и
        // сам, и в своём интерфейсе; своё объявление полнее — с доступом и
        // виртуальностью, — и интерфейсное тогда не показывается.
        std::string_view const name = group->names[member];
        auto declarations = declarations_of(entry_.def, group->kind, name);
        if (std::ranges::any_of(declarations, [this](MemberDeclaration const& each) { return each.source == entry_.def; })) {
            std::erase_if(declarations, [this](MemberDeclaration const& each) { return each.source != entry_.def; });
        }
        std::vector<std::pair<std::wstring, std::optional<MemberDocumentation>>> lines;
        for (MemberDeclaration const& declaration : declarations) {
            std::wstring line;
            SignatureWriter {line, *editor_.types_, declaration.source}.write(declaration, name);
            if (std::ranges::find(lines, line, &decltype(lines)::value_type::first) == lines.end()) {
                auto const id = DocumentationId::of(declaration.source, declaration, name);
                lines.emplace_back(std::move(line), editor_.data_->documentation.find(declaration.source, id));
            }
        }
        std::wstring html;
        for (auto const& [line, documentation] : lines) {
            html += line;
            html += L"<br>";
        }
        html += L"Member of ";
        SignatureWriter {html, *editor_.types_, entry_.def}.type(entry_.def.coded_index<md::TypeDefOrRef>());
        html += allows(name) ? L"<p>Profile: generated</p>" : L"<p>Profile: not generated</p>";

        // У перегрузок документация у каждой своя — под её сигнатурой.
        for (auto const& [line, documentation] : lines) {
            if (documentation) {
                if (lines.size() > 1) {
                    html += L"<p>" + line + L"</p>";
                }
                append_documentation(html, *documentation);
            }
        }
        return html;
    }

private:
    static constexpr uint32_t npos = UINT32_MAX;

    struct Group {
        std::string_view title;
        MemberKind kind;
        RowIcon const* icon = nullptr;
        std::vector<std::string_view> names;
        bool expanded = true;

        uint32_t shown() const { return expanded ? static_cast<uint32_t>(names.size()) : 0; }
    };

    // Перечисление, которого профиль не называет, генератор выпускает целиком.
    bool allows(std::string_view member) const {
        auto const& types = editor_.data_->profile.types;
        auto const found = types.find(name_);
        return found == types.end() ? enum_ : found->second.allows(member);
    }

    Check groupCheck(Group const& group) const {
        auto const chosen = std::ranges::count_if(group.names, [this](std::string_view name) { return allows(name); });
        if (chosen == 0) {
            return Check::Unchecked;
        }
        return std::cmp_equal(chosen, group.names.size()) ? Check::Checked : Check::Indeterminate;
    }

    std::pair<Group*, uint32_t> locate(uint32_t index) const {
        for (Group const& group : groups_) {
            if (index == 0) {
                return {const_cast<Group*>(&group), npos};
            }
            --index;
            if (index < group.shown()) {
                return {const_cast<Group*>(&group), index};
            }
            index -= group.shown();
        }
        return {const_cast<Group*>(&groups_.back()), npos};
    }

    Editor& editor_;
    TypeEntry& entry_;
    std::string const name_;
    bool const enum_;
    std::vector<Group> groups_;
    std::pair<Group const*, uint32_t> selected_ {nullptr, npos};
};

// Левое дерево на вкладке Resources: именованные ресурсы словарей XAML, из
// которых генератор пишет styles.h и brushes.h. Стили — по типу, к которому они
// относятся: отметка стиля пишется при этом типе. Кисти — двумя списками, как
// их пишет генератор (brushes и themeBrushes), а отметка — одним списком
// профиля. Верхнего уровня, объявленные вне шаблонов, — только они доступны
// поиску ресурсов по имени.
class ResourcesModel final : public TreeModel {
public:
    ResourcesModel(Editor& editor, TypesModel& types) : editor_(editor), types_(types) {
        std::set<std::string_view> seen;  // словарь объявляет ключ по разу на тему
        std::map<std::string_view, std::vector<std::string_view>> styles;  // цель -> ключи
        std::vector<std::string_view> plain;
        std::vector<std::string_view> theme;
        for (DictionaryResource const& declared : editor_.data_->resources) {
            if (declared.scoped || !seen.insert(declared.key).second) {
                continue;
            }
            if (declared.type == "Style" && !declared.target_type.empty()) {
                // Префикс пространства XAML (controls:InfoBadge) не разбирается:
                // тип называется голым именем.
                std::string_view target = declared.target_type;
                target.remove_prefix(target.rfind(':') + 1);
                styles[target].push_back(declared.key);
            } else if (declared.type.ends_with("Brush")) {
                (declared.key.ends_with("ThemeBrush") ? theme : plain).push_back(declared.key);
            }
        }

        groups_.reserve(styles.size());
        for (auto&& [target, keys] : styles) {
            std::ranges::sort(keys);
            groups_.push_back({.target = target, .type = resolve(target), .keys = std::move(keys)});
            if (!groups_.back().type.empty()) {
                editor_.data_->styled.insert(groups_.back().type);
            }
        }

        std::ranges::sort(plain);
        std::ranges::sort(theme);
        themeFrom_ = static_cast<uint32_t>(plain.size());
        brushes_ = std::move(plain);
        brushes_.insert(brushes_.end(), theme.begin(), theme.end());
        recount();
    }

    uint32_t size() const override { return size_; }

    TreeRow row(uint32_t index) const override {
        Location const at = locate(index);
        if (at.group) {
            if (at.key == npos) {
                return {.text = at.group->target,
                        .depth = 1,
                        .icon = &classIcon,
                        .expander = at.group->expanded ? Expander::Expanded : Expander::Collapsed};
            }
            std::string_view const key = at.group->keys[at.key];
            return {.text = key, .depth = 2, .icon = &styleIcon, .check = styleCheck(*at.group, key)};
        }
        if (at.key == npos) {
            return {.text = at.section->title,
                    .icon = at.section->icon,
                    .expander = at.section->expanded ? Expander::Expanded : Expander::Collapsed};
        }
        bool const chosen = editor_.data_->profile.brushes.allows(brushes_[at.key]);
        return {.text = brushes_[at.key],
                .depth = 1,
                .icon = &brushIcon,
                .check = chosen ? Check::Checked : Check::Unchecked};
    }

    void toggleExpanded(uint32_t index) override {
        Location const at = locate(index);
        if (at.key != npos) {
            return;
        }
        bool& expanded = at.group ? at.group->expanded : at.section->expanded;
        expanded = !expanded;
        recount();
    }

    void toggleChecked(uint32_t index) override {
        Location const at = locate(index);
        if (at.key == npos) {
            return;
        }

        Profile& profile = editor_.data_->profile;
        if (!at.group) {
            toggle(profile.brushes, brushes_[at.key], brushes_);
        } else {
            StyleGroup const& group = *at.group;
            std::string_view const key = group.keys[at.key];
            if (group.type.empty()) {
                return;
            }
            if (auto const type = profile.types.find(group.type); type != profile.types.end()) {
                auto const styles = profile.styles.try_emplace(group.type, MemberFilter::all()).first;
                toggle(styles->second, key, group.keys);
                // Тип без членов и без стилей не генерирует ничего — уходит из
                // профиля вместе с пустым списком стилей.
                auto const empty = [](MemberFilter const& filter) {
                    return filter.kind == MemberFilter::Kind::Allow && filter.names.empty();
                };
                if (empty(styles->second) && empty(type->second)) {
                    profile.styles.erase(styles);
                    profile.types.erase(type);
                    types_.relist(group.type);
                }
            } else {
                // Без своего типа стиль не генерируется: отметка вносит тип
                // корнем обхода без собственных членов и с одним этим стилем.
                profile.types.emplace(group.type, MemberFilter::allow({}));
                profile.styles.insert_or_assign(group.type, MemberFilter::allow({std::string {key}}));
                types_.relist(group.type);
            }
        }
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t) override {}

private:
    static constexpr uint32_t npos = UINT32_MAX;

    struct Section {
        std::string_view title;
        RowIcon const* icon = nullptr;
        bool expanded = false;
        uint32_t start = 0;  // номер строки самого раздела среди видимых
    };

    struct StyleGroup {
        std::string_view target;  // TextBlock — как тип называет словарь
        std::string type;         // Microsoft.UI.Xaml.Controls.TextBlock; пусто — в метаданных нет
        std::vector<std::string_view> keys;
        bool expanded = false;
        uint32_t start = 0;
    };

    // Строка дерева: раздел, а в разделе стилей — группа; key — номер ключа в
    // группе или в списке кистей, npos — строка самого раздела или группы.
    struct Location {
        Section* section;
        StyleGroup* group = nullptr;
        uint32_t key = npos;
    };

    // Полное имя типа цели: словарь пишет голое, а типы WinUI живут в
    // пространствах Microsoft.UI.Xaml — берётся первое, где такой тип есть.
    std::string resolve(std::string_view target) const {
        constexpr std::string_view root = "Microsoft.UI.Xaml";
        auto const& spaces = editor_.data_->db.namespaces();
        for (auto space = spaces.lower_bound(root); space != spaces.end() && space->first.starts_with(root);
             ++space) {
            if (space->second.types.contains(target)) {
                return std::format("{}.{}", space->first, target);
            }
        }
        return {};
    }

    // Стиль выбран, если его тип в профиле и фильтр стилей типа его пропускает;
    // тип без "styles" пропускает все свои.
    Check styleCheck(StyleGroup const& group, std::string_view key) const {
        if (group.type.empty()) {
            return Check::None;
        }
        Profile const& profile = editor_.data_->profile;
        if (!profile.types.contains(group.type)) {
            return Check::Unchecked;
        }
        auto const filter = profile.styles.find(group.type);
        return filter == profile.styles.end() || filter->second.allows(key) ? Check::Checked : Check::Unchecked;
    }

    // Отметка в списке, который без записи значит «все»: снятая превращает
    // «все» в список остальных, а список, где выбрано всё, снова становится «все».
    static void toggle(MemberFilter& filter, std::string_view key, std::vector<std::string_view> const& all) {
        if (filter.kind == MemberFilter::Kind::Allow) {
            std::string const name {key};
            if (!filter.names.erase(name)) {
                filter.names.insert(name);
            }
        } else {
            bool const on = !filter.allows(key);
            std::set<std::string> names;
            for (std::string_view const each : all) {
                if (each == key ? on : filter.allows(each)) {
                    names.emplace(each);
                }
            }
            filter = MemberFilter::allow(std::move(names));
        }
        if (std::ranges::all_of(all, [&filter](std::string_view each) { return filter.allows(each); })) {
            filter = MemberFilter::all();
        }
    }

    // Номера строк у свёрнутого не пересчитываются: к ним не спускаются.
    void recount() {
        uint32_t start = 0;
        styles_.start = start++;
        if (styles_.expanded) {
            for (StyleGroup& group : groups_) {
                group.start = start++;
                start += group.expanded ? static_cast<uint32_t>(group.keys.size()) : 0;
            }
        }
        plain_.start = start++;
        start += plain_.expanded ? themeFrom_ : 0;
        theme_.start = start++;
        start += theme_.expanded ? static_cast<uint32_t>(brushes_.size()) - themeFrom_ : 0;
        size_ = start;
    }

    Location locate(uint32_t index) const {
        auto* self = const_cast<ResourcesModel*>(this);
        if (index >= theme_.start) {
            return {&self->theme_, nullptr, index == theme_.start ? npos : themeFrom_ + index - theme_.start - 1};
        }
        if (index >= plain_.start) {
            return {&self->plain_, nullptr, index == plain_.start ? npos : index - plain_.start - 1};
        }
        if (index == styles_.start) {
            return {&self->styles_};
        }
        auto& group = *std::prev(std::ranges::upper_bound(self->groups_, index, {}, &StyleGroup::start));
        return {&self->styles_, &group, index == group.start ? npos : index - group.start - 1};
    }

    Editor& editor_;
    TypesModel& types_;

    Section styles_ {"Styles", &styleIcon};
    Section plain_ {"Brushes", &brushIcon};
    Section theme_ {"Theme brushes", &brushIcon};
    std::vector<StyleGroup> groups_;

    // Кисти обоих разделов одним списком — как одним списком их выбирает
    // профиль: сначала brushes, с themeFrom_ — themeBrushes.
    std::vector<std::string_view> brushes_;
    uint32_t themeFrom_ = 0;

    uint32_t size_ = 0;
};
void TypesModel::invoke(uint32_t index) {
    selected_ = locate(index);
    auto const [library, space, entry] = selected_;
    void const* const row = entry ? static_cast<void const*>(entry) : space ? static_cast<void const*>(space) : library;
    editor_.typeName.set(entry ? to_u16(full_name(entry->def)) : std::u16string {});
    editor_.members.set(entry ? intrusive_ptr<TreeModel> {new MembersModel {editor_, *entry}, /*add_ref=*/false}
                              : intrusive_ptr<TreeModel> {});
    editor_.selection.set({intrusive_ptr<TreeModel> {this}, row});
}

std::wstring TypesModel::describe() const {
    auto const [library, space, entry] = selected_;
    if (entry) {
        return typeInfo(*space, *entry);
    }
    if (space) {
        return namespaceInfo(*library, *space);
    }
    return library ? libraryInfo(*library) : std::wstring {};
}

Editor::Editor() = default;

intrusive_ptr<TreeModel> Editor::types() const {
    return types_;
}

intrusive_ptr<TreeModel> Editor::resources() const {
    return resources_;
}

wxl::core::nullable<uint32_t> Editor::reveal(std::wstring_view type) const {
    std::string utf8;
    if (auto const text = wxl::core::unicode::checked(type)) {
        wxl::core::unicode::append_utf8(utf8, *text);
    }
    return types_->reveal(utf8);
}
Editor::~Editor() = default;

intrusive_ptr<Editor> Editor::open(std::filesystem::path const& profile) {
    intrusive_ptr<Editor> editor {new Editor {}, /*add_ref=*/false};

    auto const resolved = resolve_profiles({profile}, default_nuget_root());
    std::vector<std::string> files;
    files.reserve(resolved.metadata.size());
    for (auto&& file : resolved.metadata) {
        files.push_back(file.string());
    }

    editor->data_ = std::make_unique<Data>(load_profile(profile), files);
    for (auto&& dictionary : resolved.resources) {
        auto declared = dictionary_resources(dictionary);
        editor->data_->resources.insert(editor->data_->resources.end(), std::move_iterator {declared.begin()},
                                        std::move_iterator {declared.end()});
    }
    editor->types_ = intrusive_ptr<TypesModel> {new TypesModel {*editor}, /*add_ref=*/false};
    editor->resources_ = intrusive_ptr<ResourcesModel> {new ResourcesModel {*editor, *editor->types_}, /*add_ref=*/false};
    editor->info.follow(editor->selection, editor->revision, [](Selection const& chosen, uint32_t) {
        return chosen.model ? chosen.model->describe() : std::wstring {};
    });
    auto const file = profile.filename().wstring();
    editor->title.set(std::u16string {file.begin(), file.end()} + u" — wxl.gen.ui");
    editor->path.set(std::filesystem::path {profile}.make_preferred().u16string());
    return editor;
}

}  // namespace editor
