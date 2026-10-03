// Метаданные и профиль — только здесь: crawl.h приносит winmd_reader.h вместе с
// <windows.h> в нужном порядке, а окно видит одни модели деревьев.
#include "crawl.h"
#include "profile.h"
#include "xml_input.h"

#include "Editor.h"

#include <algorithm>
#include <chrono>
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
constexpr RowIcon interfaceIcon {0xE9E9, rgb(0, 77, 64), rgb(128, 203, 196)};    // puzzle_piece: тёмно-светло-зелёный (карточка)
constexpr RowIcon delegateIcon {0xE999, rgb(26, 35, 126), rgb(159, 168, 218)};   // plug_connected: индиго (карточка)
// Члены — сплошным цветом.
constexpr RowIcon propertyIcon {0xEE85, rgb(97, 97, 97), rgb(97, 97, 97)};       // wrench: серый
constexpr RowIcon methodIcon {0xF334, rgb(94, 53, 177), rgb(94, 53, 177)};       // cube: фиолетовый
constexpr RowIcon eventIcon {0xE617, rgb(239, 108, 0), rgb(239, 108, 0)};        // flash: оранжевый
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
void append_filter(std::wstring& html, member_filter const* filter) {
    html += L"<p>Profile: ";
    if (!filter || filter->kind == member_filter::kind_t::None) {
        html += L"not listed</p>";
        return;
    }
    if (filter->kind == member_filter::kind_t::All) {
        html += L"all members</p>";
        return;
    }
    html += filter->kind == member_filter::kind_t::Allow ? L"only " : L"all members except ";
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
void mark_member(profile& prof, std::string const& type, std::string_view member, bool on,
                 bool unlistedKeepsAll, bool styled) {
    auto& types = prof.types;
    std::string const name {member};
    auto const found = types.find(type);
    if (found == types.end()) {
        if (on != unlistedKeepsAll) {
            types.emplace(type, on ? member_filter::allow({name}) : member_filter::deny({name}));
        }
        return;
    }

    member_filter& filter = found->second;
    switch (filter.kind) {
        case member_filter::kind_t::All:
        case member_filter::kind_t::None:
            filter = on ? member_filter::all() : member_filter::deny({name});
            break;
        case member_filter::kind_t::Allow:
            if (on) {
                filter.names.insert(name);
            } else {
                filter.names.erase(name);
                auto const styles = prof.styles.find(type);
                bool const generatesStyles = styles == prof.styles.end()
                                                 ? styled
                                                 : styles->second.kind != member_filter::kind_t::Allow ||
                                                       !styles->second.names.empty();
                if (filter.names.empty() && !generatesStyles) {
                    types.erase(found);
                    if (styles != prof.styles.end()) {
                        prof.styles.erase(styles);
                    }
                }
            }
            break;
        case member_filter::kind_t::Deny:
            if (on) {
                filter.names.erase(name);
                if (filter.names.empty()) {
                    filter = member_filter::all();
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
// интерфейса, а без него на английском. Открытый файл знает только, где какой
// член; остальное разбирается шагами parseSome().
class Documentation {
public:
    // opened — открылся файл, где разобрано ещё не всё.
    explicit Documentation(wxl::core::function<void()> opened) : opened_(std::move(opened)) {}

    wxl::core::nullable<member_documentation const> find(md::TypeDef const& type, std::string const& id) {
        documentation_file* const file = fileOf(type);
        return file ? file->find(id) : nullptr;
    }

    // Справочник Windows SDK — на английском или на языке интерфейса. Открытые
    // файлы справочника закрываются: при следующей нужде открывается файл
    // нужного языка. Документация рядом с .winmd одна и остаётся.
    void english(bool value) {
        auto languages = value ? std::vector<std::wstring> {L"en"} : documentation_languages();
        if (languages == languages_) {
            return;
        }
        languages_ = std::move(languages);
        std::erase_if(unfinished_, [this](documentation_file const* file) {
            return std::ranges::any_of(contracts_, [file](auto const& each) { return each.second.get() == file; });
        });
        contracts_.clear();
    }

    // Разбирает члены открытых файлов до срока; остались ли неразобранные.
    bool parseSome(std::chrono::steady_clock::time_point deadline) {
        while (!unfinished_.empty()) {
            if (unfinished_.front()->parse_some(deadline)) {
                return true;
            }
            unfinished_.erase(unfinished_.begin());
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
        }
        return !unfinished_.empty();
    }

private:
    using File = std::unique_ptr<documentation_file>;

    // Файл находится по файлу метаданных, а у метаданных Windows — по
    // контракту; и тот, и другой ищется и читается один раз.
    documentation_file* fileOf(md::TypeDef const& type) {
        auto const& database = type.get_database();
        auto [beside, fresh] = beside_.try_emplace(&database);
        if (fresh) {
            beside->second = open(std::filesystem::path {database.path()}.replace_extension(L".xml"));
            started(beside->second);
        }
        if (beside->second) {
            return beside->second.get();
        }

        auto [contract, added] = contracts_.try_emplace(contract_of(type));
        if (added) {
            contract->second = open(sdkFile(contract->first));
            started(contract->second);
        }
        return contract->second.get();
    }

    void started(File const& file) {
        if (file && file->size() > 0) {
            unfinished_.push_back(file.get());
            opened_();
        }
    }

    static File open(std::filesystem::path const& file) {
        if (file.empty() || !std::filesystem::exists(file)) {
            return {};
        }
        try {
            return std::make_unique<documentation_file>(file);
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

    wxl::core::function<void()> opened_;
    std::vector<std::wstring> languages_ = documentation_languages();
    std::filesystem::path references_;
    std::map<md::database const*, File> beside_;
    std::map<std::string, File> contracts_;
    std::vector<documentation_file*> unfinished_;  // в порядке открытия
};

}  // namespace

struct Editor::Data {
    Data(profile own, std::vector<std::string> const& files, wxl::core::function<void()> opened)
        : prof(std::move(own)), db(files), documentation(std::move(opened)) {}

    // Отметки — то, что говорит сам файл, без профилей, которые он продолжает.
    profile prof;
    md::cache db;

    // Ресурсы словарей XAML с ключами, как их объявил документ; модель ресурсов
    // смотрит в их строки.
    std::vector<dictionary_resource> resources;

    // Полные имена типов, у которых в словарях есть стили.
    std::set<std::string, std::less<>> styled;

    Documentation documentation;

    // Что wxl пишет сам (types.json рядом с профилем): типы, предопределённые вне
    // проекции.
    type_map typeMap;

    // Настройки редактора: что не показывается.
    bool hideDependencyProperties = true;
    bool hideEventArgs = true;
    bool hidePredefined = true;
};

namespace {

// Тип по полному имени. Ссылка на обобщённый тип называет его без числа
// параметров (Windows.Foundation.Collections.IVector), а метаданные — с ним
// (IVector`1): такой ищется по имени с `.
md::TypeDef find_type(md::cache const& db, std::string_view full) {
    if (md::TypeDef const def = db.find(full)) {
        return def;
    }
    auto const dot = full.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    auto const space = db.namespaces().find(full.substr(0, dot));
    if (space == db.namespaces().end()) {
        return {};
    }
    std::string const generic = std::format("{}`", full.substr(dot + 1));
    auto const& types = space->second.types;
    auto const found = types.lower_bound(generic);
    return found != types.end() && std::string_view {found->first}.starts_with(generic) ? found->second
                                                                                          : md::TypeDef {};
}

}  // namespace

// Тип в левом дереве; адрес постоянен, пока жив редактор.
struct TypeEntry {
    md::TypeDef def;
    RowIcon const* icon = nullptr;
    bool listed = false;
    bool shown = true;
    // Класс аргументов события: имя на EventArgs у него самого или у одной из
    // его баз. Помечается при разборе; показывать ли — настройка.
    bool eventArgs = false;
    // Предопределён вне проекции: wxl пишет его сам (predefined в types.json).
    // Показанный — без выбора.
    bool predefined = false;

    // Отметку можно ставить и считать: показан и не предопределён.
    bool selectable() const { return shown && !predefined; }
};

namespace {

bool is_event_args(md::TypeDef type) {
    while (type) {
        if (type.TypeName().ends_with("EventArgs")) {
            return true;
        }
        auto const base = type.Extends();
        if (!base || base.type() == md::TypeDefOrRef::TypeSpec) {
            return false;
        }
        auto const [ns, name] = md::get_type_namespace_and_name(base);
        if (name.ends_with("EventArgs")) {
            return true;
        }
        if (ns == "System" && name == "Object") {
            return false;
        }
        type = md::find(base);
    }
    return false;
}

}  // namespace

// Левое дерево: файлы winmd профиля, в них — пространства имён, в тех — типы.
// Все типы разобраны один раз; показываются не все — настройка прячет классы
// аргументов событий, и пространство или файл, где не показано ничего, не
// показываются сами.
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
                    library.namespaces.back().types.push_back(
                        {.def = def,
                         .icon = icon,
                         .eventArgs = icon == &classIcon && is_event_args(def),
                         .predefined = is_predefined(def, editor_.data_->typeMap)});
                }
            }
        }

        std::erase_if(libraries_, [](Library const& library) { return library.namespaces.empty(); });
        for (Library& library : libraries_) {
            for (Namespace& space : library.namespaces) {
                std::ranges::sort(space.types, {}, [](TypeEntry const& entry) { return entry.def.TypeName(); });
            }
        }

        for (auto&& [type, filter] : editor_.data_->prof.types) {
            if (TypeEntry* entry = find(type)) {
                entry->listed = true;
            }
        }
        refilter();
    }

    // Показываемое — заново, по настройкам; счёт — среди тех, что можно
    // отметить. Скрытый выбранный тип больше не выбран.
    void refilter() {
        bool const hideArgs = editor_.data_->hideEventArgs;
        bool const hidePredefined = editor_.data_->hidePredefined;
        shown_.clear();
        for (Library& library : libraries_) {
            library.shown.clear();
            library.total = 0;
            library.listed = 0;
            for (Namespace& space : library.namespaces) {
                space.shown.clear();
                space.total = 0;
                space.listed = 0;
                for (TypeEntry& entry : space.types) {
                    entry.shown = !(hideArgs && entry.eventArgs) && !(hidePredefined && entry.predefined);
                    if (entry.shown) {
                        space.shown.push_back(&entry);
                    }
                    if (entry.selectable()) {
                        ++space.total;
                        space.listed += entry.listed ? 1 : 0;
                    }
                }
                if (!space.shown.empty()) {
                    library.shown.push_back(&space);
                    library.total += space.total;
                    library.listed += space.listed;
                }
            }
            if (!library.shown.empty()) {
                shown_.push_back(&library);
            }
        }
        recount();

        auto const [library, space, type] = selected_;
        if (type && !type->shown) {
            selected_ = {library, space, nullptr};
            editor_.typeName.set({});
            editor_.members.set({});
            editor_.selection.set({intrusive_ptr<TreeModel> {this}, space});
        }
    }

    uint32_t size() const override { return size_; }

    TreeRow row(uint32_t index) const override {
        auto const [library, space, type] = locate(index);
        // У файла и пространства серым — сколько их типов в профиле.
        if (!space) {
            detail_ = std::format("{} of {}", library->listed, library->total);
            return {.text = library->name,
                    .detail = detail_,
                    .icon = &libraryIcon,
                    .expander = library->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = triple(library->listed, library->total),
                    .selected = library == selected_.library && !selected_.space};
        }
        if (!type) {
            detail_ = std::format("{} of {}", space->listed, space->total);
            return {.text = space->name,
                    .detail = detail_,
                    .depth = 1,
                    .icon = &namespaceIcon,
                    .expander = space->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = triple(space->listed, space->total),
                    .selected = space == selected_.space && !selected_.type};
        }

        // У класса серым — база, если она своя, а не System.Object; у
        // предопределённого — что он предопределён.
        detail_.clear();
        if (type->predefined) {
            detail_ = "pre-defined";
        } else if (type->icon == &classIcon) {
            if (auto const base = type->def.Extends(); base && base.type() != md::TypeDefOrRef::TypeSpec) {
                auto const [ns, name] = md::get_type_namespace_and_name(base);
                if (ns != "System" || name != "Object") {
                    detail_ = std::format(": {}", name);
                }
            }
        }
        return {.text = type->def.TypeName(),
                .detail = detail_,
                .depth = 2,
                .icon = type->icon,
                .check = type->predefined ? Check::Disabled : type->listed ? Check::Checked : Check::Unchecked,
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

    // Отметка файла или пространства — всех его показанных типов: отмеченное
    // целиком снимается, иначе отмечается целиком. Скрытые остаются, как были.
    void toggleChecked(uint32_t index) override {
        auto const [library, space, entry] = locate(index);
        if (entry) {
            list(*library, *space, *entry, !entry->listed);
        } else if (space) {
            bool const on = space->listed != space->total;
            for (TypeEntry* each : space->shown) {
                list(*library, *space, *each, on);
            }
        } else {
            bool const on = library->listed != library->total;
            for (Namespace* each : library->shown) {
                for (TypeEntry* type : each->shown) {
                    list(*library, *each, *type, on);
                }
            }
        }
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override;
    Description describe() const override;

    wxl::core::nullable<uint32_t> selectedRow() const override {
        auto const [library, space, type] = selected_;
        if (!library || library->shown.empty() || (space && !library->expanded) || (type && !space->expanded)) {
            return {};
        }
        if (type) {
            auto const at = std::ranges::find(space->shown, type);
            return space->start + 1 + static_cast<uint32_t>(at - space->shown.begin());
        }
        return space ? space->start : library->start;
    }

    // Тип внесли в профиль или вынесли из него не отсюда — отметкой члена или
    // стиля.
    void relist(std::string_view full) {
        if (TypeEntry* entry = find(full)) {
            Location const at = place(entry);
            count(*at.library, *at.space, *entry, editor_.data_->prof.types.contains(std::string {full}));
        }
    }

    // Есть ли тип в дереве, показанный или скрытый настройкой.
    bool lists(std::string_view full) const { return find(full) != nullptr; }

    // Есть ли тип в дереве, показанный: только на такой ссылка ведёт в дерево.
    bool shows(std::string_view full) const {
        TypeEntry const* const entry = find(full);
        return entry && entry->shown;
    }

    // Есть ли тип в метаданных: на такой ведёт ссылка из сведений — в дерево,
    // а нет его там (интерфейс, делегат) или он скрыт — в карточку в сведениях.
    bool knows(std::string_view full) const { return static_cast<bool>(find_type(editor_.data_->db, full)); }

    // Раскрывает файл и пространство типа и выбирает его; номер его строки
    // среди видимых.
    wxl::core::nullable<uint32_t> reveal(std::string_view full) {
        TypeEntry* const entry = find(full);
        if (!entry || !entry->shown) {
            return {};
        }
        Location const at = place(entry);
        at.library->expanded = true;
        at.space->expanded = true;
        recount();
        auto const position = std::ranges::find(at.space->shown, entry);
        uint32_t const index = at.space->start + 1 + static_cast<uint32_t>(position - at.space->shown.begin());
        invoke(index);
        return index;
    }

private:
    struct Namespace {
        std::string_view name;
        std::vector<TypeEntry> types;
        std::vector<TypeEntry*> shown;  // показываемые из types
        bool expanded = false;
        uint32_t start = 0;   // номер строки самого пространства среди видимых
        uint32_t total = 0;   // типов, которые можно отметить
        uint32_t listed = 0;  // из них в профиле
    };

    struct Library {
        md::database const* source = nullptr;
        std::string_view name;  // имя файла winmd
        std::vector<Namespace> namespaces;
        std::vector<Namespace*> shown;  // пространства, где показан хоть один тип
        bool expanded = false;
        uint32_t start = 0;   // номер строки самого файла среди видимых
        uint32_t total = 0;   // типов, которые можно отметить, во всех пространствах
        uint32_t listed = 0;  // из них в профиле
    };

    // Строка дерева: файл, а под ним — пространство и тип, если строка их.
    struct Location {
        Library* library;
        Namespace* space;
        TypeEntry* type;
    };

    // Отметка родителя: все дети, часть или ни одного; отмечать нечего — без
    // выбора.
    static Check triple(uint32_t listed, uint32_t total) {
        return total == 0 ? Check::Disabled
               : listed == 0 ? Check::Unchecked
               : listed == total ? Check::Checked
                                 : Check::Indeterminate;
    }

    // Файл и пространство типа: тип лежит в векторе своего пространства.
    Location place(TypeEntry* entry) {
        for (Library& library : libraries_) {
            for (Namespace& space : library.namespaces) {
                if (entry >= space.types.data() && entry < space.types.data() + space.types.size()) {
                    return {&library, &space, entry};
                }
            }
        }
        return {};
    }

    // Счёт отмеченных у пространства и файла идёт за отметкой показанного типа.
    static void count(Library& library, Namespace& space, TypeEntry& entry, bool listed) {
        if (entry.listed == listed) {
            return;
        }
        entry.listed = listed;
        if (!entry.selectable()) {
            return;
        }
        if (listed) {
            ++library.listed;
            ++space.listed;
        } else {
            --library.listed;
            --space.listed;
        }
    }

    // Тип входит в профиль со всеми членами или уходит из него вместе со
    // списками членов и стилей.
    void list(Library& library, Namespace& space, TypeEntry& entry, bool on) {
        if (entry.listed == on || entry.predefined) {
            return;
        }
        profile& prof = editor_.data_->prof;
        std::string const name = full_name(entry.def);
        if (on) {
            prof.types.emplace(name, member_filter::all());
        } else {
            prof.types.erase(name);
            prof.styles.erase(name);
        }
        count(library, space, entry, on);
    }

    // Номера строк у свёрнутого не пересчитываются: к ним не спускаются.
    void recount() {
        uint32_t start = 0;
        for (Library* library : shown_) {
            library->start = start++;
            if (!library->expanded) {
                continue;
            }
            for (Namespace* space : library->shown) {
                space->start = start++;
                start += space->expanded ? static_cast<uint32_t>(space->shown.size()) : 0;
            }
        }
        size_ = start;
    }

    Location locate(uint32_t index) const {
        Library* const library =
            *std::prev(std::ranges::upper_bound(shown_, index, {}, [](Library const* each) { return each->start; }));
        if (index == library->start) {
            return {library, nullptr, nullptr};
        }
        Namespace* const space = *std::prev(
            std::ranges::upper_bound(library->shown, index, {}, [](Namespace const* each) { return each->start; }));
        if (index == space->start) {
            return {library, space, nullptr};
        }
        return {library, space, space->shown[index - space->start - 1]};
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
        size_t args = 0;
        for (TypeEntry const& type : space.types) {
            (type.icon == &classIcon ? classes : type.icon == &structIcon ? structs : enums) += 1;
            args += type.eventArgs ? 1 : 0;
        }
        std::wstring html = L"namespace ";
        append_bold(html, space.name);
        html += L"<br>Library ";
        append_text(html, library.name);
        html += std::format(L"<p>{} classes ({} of them event args), {} structs, {} enums</p>", classes, args, structs,
                            enums);
        return html;
    }

    std::wstring typeInfo(Namespace const& space, TypeEntry const& type) const;

    Editor& editor_;
    std::vector<Library> libraries_;
    std::vector<Library*> shown_;  // файлы, где показан хоть один тип
    uint32_t size_ = 0;
    Location selected_ {};  // выбранная строка: файл, а под ним — пространство и тип
    mutable std::string detail_;  // серое строки, пока дерево её читает
};

namespace {

// Ключ документации .NET, которым файлы документации называют члены:
// "M:Ns.Type.Method(System.String,Ns.Other@)". Типы — полными именами,
// встроенные — именами System, аргументы обобщённого — в фигурных скобках,
// параметр по ссылке (выходной) — с @.
class DocumentationId {
public:
    static std::string of(md::TypeDef const& type) { return std::format("T:{}.{}", type.TypeNamespace(), type.TypeName()); }

    static std::string of(md::TypeDef const& owner, member_declaration const& declaration, std::string_view name) {
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
void append_documentation(std::wstring& html, member_documentation const& documentation) {
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

// Типы сигнатуры словами: встроенные — именами WinRT, обобщённые — с
// аргументами в угловых скобках, параметры обобщения — по имени. Как писать имя
// типа и текст, решает наследник.
class TypeSpeller {
public:
    void type(md::coded_index<md::TypeDefOrRef> const& ref) {
        if (ref.type() == md::TypeDefOrRef::TypeSpec) {
            generic(ref.TypeSpec().Signature().GenericTypeInst());
            return;
        }
        auto const [ns, name] = md::get_type_namespace_and_name(ref);
        // У обобщённого имени в метаданных число параметров после `.
        named(ns, name.substr(0, name.find('`')));
    }

    void type(md::TypeSig const& signature) {
        std::visit([this](auto const& value) { element(value); }, signature.Type());
        if (signature.is_szarray()) {
            text("[]");
        }
    }

protected:
    // source — тип, где объявлен член: по нему называются параметры обобщения.
    explicit TypeSpeller(md::TypeDef const& source) : source_(source) {}
    ~TypeSpeller() = default;

    virtual void named(std::string_view ns, std::string_view name) = 0;
    virtual void text(std::string_view utf8) = 0;

    md::TypeDef const& source_;

private:
    void generic(md::GenericTypeInstSig const& instance) {
        type(instance.GenericType());
        text("<");
        bool first = true;
        for (auto&& argument : instance.GenericArgs()) {
            text(first ? "" : ", ");
            first = false;
            type(argument);
        }
        text(">");
    }

    void element(md::coded_index<md::TypeDefOrRef> const& ref) { type(ref); }
    void element(md::GenericTypeInstSig const& instance) { generic(instance); }

    // Параметр обобщённого интерфейса, где член объявлен, — по его имени.
    void element(md::GenericTypeIndex index) {
        uint32_t number = 0;
        for (auto&& param : source_.GenericParam()) {
            if (number++ == index.index) {
                text(param.Name());
                return;
            }
        }
        text("T");
    }

    void element(md::GenericMethodTypeIndex) { text("T"); }

    void element(md::ElementType element) {
        switch (element) {
            case md::ElementType::Boolean: text("Boolean"); break;
            case md::ElementType::Char: text("Char16"); break;
            case md::ElementType::I1: text("Int8"); break;
            case md::ElementType::U1: text("UInt8"); break;
            case md::ElementType::I2: text("Int16"); break;
            case md::ElementType::U2: text("UInt16"); break;
            case md::ElementType::I4: text("Int32"); break;
            case md::ElementType::U4: text("UInt32"); break;
            case md::ElementType::I8: text("Int64"); break;
            case md::ElementType::U8: text("UInt64"); break;
            case md::ElementType::R4: text("Single"); break;
            case md::ElementType::R8: text("Double"); break;
            case md::ElementType::String: text("String"); break;
            case md::ElementType::Object: text("Object"); break;
            default: text("?"); break;
        }
    }
};

// Сигнатура члена в разметку сведений. Типы — полными именами; тип, который
// есть в левом дереве, — ссылкой на него.
class SignatureWriter final : public TypeSpeller {
public:
    SignatureWriter(std::wstring& html, TypesModel const& types, md::TypeDef const& source)
        : TypeSpeller(source), html_(html), types_(types) {}

    void write(member_declaration const& declaration, std::string_view name) {
        std::visit([this, name](auto const& definition) { member(definition, name); }, declaration.definition);
    }

private:
    void named(std::string_view ns, std::string_view name) override {
        std::string const full = std::format("{}.{}", ns, name);
        if (types_.knows(full)) {
            html_ += L"<a href=\"";
            append_text(html_, full);
            html_ += L"\">";
            append_text(html_, full);
            html_ += L"</a>";
        } else {
            append_text(html_, full);
        }
    }

    void text(std::string_view utf8) override { append_text(html_, utf8); }

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

    std::wstring& html_;
    TypesModel const& types_;
};

// Объявление в строку дерева, серым после имени: типы короткими именами, из
// модификаторов — только static. Свойство — с аксессорами ({ get; } или
// { get; set; }), метод — с возвратом и параметрами, событие — с делегатом,
// значение — с числом.
class BriefSignature final : public TypeSpeller {
public:
    static std::string of(member_declaration const& declaration, std::string_view name) {
        BriefSignature brief {declaration.source, name};
        std::visit([&brief](auto const& definition) { brief.member(definition); }, declaration.definition);
        return std::move(brief.text_);
    }

private:
    BriefSignature(md::TypeDef const& source, std::string_view name) : TypeSpeller(source), name_(name) {}

    void named(std::string_view, std::string_view name) override { text_ += name; }
    void text(std::string_view utf8) override { text_ += utf8; }

    void modifiers(md::MethodDef const& method) {
        if (method.Flags().Static()) {
            text_ += "static ";
        }
    }

    void member(md::Property const& property) {
        bool setter = false;
        for (auto&& semantic : property.MethodSemantic()) {
            setter = setter || semantic.Semantic().Setter();
            if (semantic.Semantic().Getter()) {
                modifiers(semantic.Method());
            }
        }
        type(property.Type().Type());
        text_ += ' ';
        text_ += name_;
        text_ += setter ? " { get; set; }" : " { get; }";
    }

    void member(md::MethodDef const& method) {
        modifiers(method);
        auto const signature = method.Signature();
        if (signature.ReturnType()) {
            type(signature.ReturnType().Type());
        } else {
            text_ += "void";
        }
        text_ += ' ';
        text_ += name_;
        text_ += '(';
        uint16_t sequence = 0;
        for (auto&& param : signature.Params()) {
            ++sequence;
            text_ += sequence > 1 ? ", " : "";
            md::Param definition;
            for (auto&& each : method.ParamList()) {
                if (each.Sequence() == sequence) {
                    definition = each;
                }
            }
            if (definition && definition.Flags().Out()) {
                text_ += "out ";
            }
            type(param.Type());
            if (definition) {
                text_ += ' ';
                text_ += definition.Name();
            }
        }
        text_ += ')';
    }

    void member(md::Event const& event) {
        for (auto&& semantic : event.MethodSemantic()) {
            if (semantic.Semantic().AddOn()) {
                modifiers(semantic.Method());
            }
        }
        text_ += "event ";
        type(event.EventType());
        text_ += ' ';
        text_ += name_;
    }

    void member(md::Field const& field) {
        text_ += name_;
        if (auto const constant = field.Constant()) {
            std::visit(
                [this](auto const& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char16_t>) {
                        text_ += std::format(" = {}", value);
                    }
                },
                constant.Value());
        }
    }

    std::string_view const name_;
    std::string text_;
};

// Объявления члена, как их показывать. Класс объявляет член и сам, и в своём
// интерфейсе; своё объявление полнее — с доступом и виртуальностью, — и
// интерфейсное тогда не показывается.
std::vector<member_declaration> shown_declarations(md::TypeDef const& type, member_kind kind, std::string_view name) {
    auto declarations = declarations_of(type, kind, name);
    if (std::ranges::any_of(declarations, [&type](member_declaration const& each) { return each.source == type; })) {
        std::erase_if(declarations, [&type](member_declaration const& each) { return each.source != type; });
    }
    return declarations;
}

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

    auto const& types = editor_.data_->prof.types;
    auto const found = types.find(full_name(type.def));
    append_filter(html, found == types.end() ? nullptr : &found->second);
    if (auto const documentation = editor_.data_->documentation.find(type.def, DocumentationId::of(type.def))) {
        append_documentation(html, **documentation);
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
            addGroup("Values", member_kind::Constant, &constantIcon, declared.constants);
        } else {
            addGroup("Properties", member_kind::Property, &propertyIcon, declared.properties);
            addGroup("Methods", member_kind::Method, &methodIcon, declared.methods);
            addGroup("Events", member_kind::Event, &eventIcon, declared.events);
        }
        refilter();
    }

    // Показываемые члены — заново, по настройке редактора: скрытые остаются
    // в группе и показываются снова, как только настройка снята.
    void refilter() {
        bool const hide = editor_.data_->hideDependencyProperties;
        for (Group& group : groups_) {
            group.shown.clear();
            for (Member const& member : group.all) {
                if (!(hide && member.dependency)) {
                    group.shown.push_back(&member);
                }
            }
        }
        // Выбранный член, который скрылся, больше не выбран.
        if (auto const [group, member] = selected_; member && !std::ranges::contains(group->shown, member)) {
            selected_ = {group, nullptr};
        }
    }

    uint32_t size() const override {
        uint32_t size = 0;
        for (Group const& group : groups_) {
            size += group.rows();
        }
        return size;
    }

    TreeRow row(uint32_t index) const override {
        auto const [group, at] = locate(index);
        if (at == npos) {
            count_ = std::format("{} of {}", chosen(*group), group->shown.size());
            return {.text = group->title,
                    .detail = count_,
                    .icon = group->icon,
                    .expander = group->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = entry_.predefined ? Check::Disabled : groupCheck(*group),
                    .selected = group == selected_.first && !selected_.second};
        }

        Member const& member = *group->shown[at];
        return {.text = member.name,
                .detail = member.brief,
                .depth = 1,
                .icon = group->icon,
                .check = entry_.predefined ? Check::Disabled : allows(member.name) ? Check::Checked : Check::Unchecked,
                .selected = &member == selected_.second};
    }

    void toggleExpanded(uint32_t index) override {
        auto const [group, at] = locate(index);
        if (at == npos) {
            group->expanded = !group->expanded;
        }
    }

    // Отметка группы — только её показанных членов: скрытые остаются, как были.
    // У предопределённого типа отмечать нечего: его члены пишет wxl.
    void toggleChecked(uint32_t index) override {
        if (entry_.predefined) {
            return;
        }
        auto const [group, at] = locate(index);
        profile& prof = editor_.data_->prof;
        bool const styled = editor_.data_->styled.contains(name_);
        if (at == npos) {
            // Отмеченная целиком группа снимается, иначе отмечается целиком.
            bool const on = groupCheck(*group) != Check::Checked;
            for (Member const* member : group->shown) {
                if (allows(member->name) != on) {
                    mark_member(prof, name_, member->name, on, enum_, styled);
                }
            }
        } else {
            std::string_view const name = group->shown[at]->name;
            mark_member(prof, name_, name, !allows(name), enum_, styled);
        }
        editor_.types_->relist(name_);
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override {
        auto const [group, at] = locate(index);
        Member const* const member = at == npos ? nullptr : group->shown[at];
        selected_ = {group, member};
        void const* const row = member ? static_cast<void const*>(member) : group;
        editor_.selection.set({intrusive_ptr<TreeModel> {this}, row});
    }

    wxl::core::nullable<uint32_t> selectedRow() const override {
        auto const [picked, member] = selected_;
        uint32_t index = 0;
        for (Group const& group : groups_) {
            if (&group == picked) {
                if (group.shown.empty()) {
                    return {};
                }
                if (!member) {
                    return index;
                }
                if (!group.expanded) {
                    return {};
                }
                auto const at = std::ranges::find(group.shown, member);
                return index + 1 + static_cast<uint32_t>(at - group.shown.begin());
            }
            index += group.rows();
        }
        return {};
    }

    // Двойной щелчок по члену — его отметка; группу он не трогает: её первый
    // щелчок уже свернул или раскрыл.
    void activate(uint32_t index) override {
        if (locate(index).second != npos) {
            toggleChecked(index);
        }
    }

    Description describe() const override {
        auto const [group, member] = selected_;
        if (!group) {
            return {};
        }
        if (!member) {
            std::wstring html;
            append_bold(html, group->title);
            html += L"<br>Members of ";
            SignatureWriter {html, *editor_.types_, entry_.def}.type(entry_.def.coded_index<md::TypeDefOrRef>());
            html += std::format(L"<p>Profile: {} of {} generated", chosen(*group), group->shown.size());
            if (size_t const hidden = group->all.size() - group->shown.size()) {
                html += std::format(L"<br>{} dependency properties hidden", hidden);
            }
            html += L"</p>";
            return {group->icon, std::move(html)};
        }

        // Метод — каждой перегрузкой, строкой на каждую.
        std::string_view const name = member->name;
        std::vector<std::pair<std::wstring, wxl::core::nullable<member_documentation const>>> lines;
        for (member_declaration const& declaration : shown_declarations(entry_.def, group->kind, name)) {
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
                append_documentation(html, **documentation);
            }
        }
        return {group->icon, std::move(html)};
    }

private:
    static constexpr uint32_t npos = UINT32_MAX;

    // Член, каким он прочитан один раз при открытии типа.
    struct Member {
        std::string_view name;
        std::string brief;  // объявление серым после имени; у перегрузок — первое и их число
        // Идентификатор свойства зависимостей: статическое свойство только
        // для чтения с именем на Property.
        bool dependency = false;
    };

    struct Group {
        std::string_view title;
        member_kind kind;
        RowIcon const* icon = nullptr;
        std::vector<Member> all;
        std::vector<Member const*> shown;  // показываемые из all
        bool expanded = true;

        // Группа, где не показано ничего, не показывается и сама.
        uint32_t rows() const {
            return shown.empty() ? 0 : 1 + (expanded ? static_cast<uint32_t>(shown.size()) : 0);
        }
    };

    void addGroup(std::string_view title, member_kind kind, RowIcon const* icon,
                  std::vector<std::string_view> const& names) {
        if (names.empty()) {
            return;
        }
        Group& group = groups_.emplace_back(Group {title, kind, icon});
        group.all.reserve(names.size());
        for (std::string_view const name : names) {
            Member& member = group.all.emplace_back(Member {name});
            std::vector<std::string> briefs;
            for (member_declaration const& declaration : shown_declarations(entry_.def, kind, name)) {
                if (auto const* property = std::get_if<md::Property>(&declaration.definition)) {
                    member.dependency = member.dependency || isDependencyProperty(*property, name);
                }
                if (std::string brief = BriefSignature::of(declaration, name); !std::ranges::contains(briefs, brief)) {
                    briefs.push_back(std::move(brief));
                }
            }
            if (!briefs.empty()) {
                member.brief = std::move(briefs.front());
            }
            if (briefs.size() > 1) {
                member.brief += std::format("  +{} overload{}", briefs.size() - 1, briefs.size() > 2 ? "s" : "");
            }
        }
    }

    static bool isDependencyProperty(md::Property const& property, std::string_view name) {
        if (!name.ends_with("Property")) {
            return false;
        }
        bool getter = false;
        for (auto&& semantic : property.MethodSemantic()) {
            if (semantic.Semantic().Setter()) {
                return false;
            }
            if (semantic.Semantic().Getter()) {
                getter = semantic.Method().Flags().Static();
            }
        }
        return getter;
    }

    // Перечисление, которого профиль не называет, генератор выпускает целиком.
    bool allows(std::string_view member) const {
        auto const& types = editor_.data_->prof.types;
        auto const found = types.find(name_);
        return found == types.end() ? enum_ : found->second.allows(member);
    }

    size_t chosen(Group const& group) const {
        return static_cast<size_t>(
            std::ranges::count_if(group.shown, [this](Member const* member) { return allows(member->name); }));
    }

    Check groupCheck(Group const& group) const {
        size_t const count = chosen(group);
        if (count == 0) {
            return Check::Unchecked;
        }
        return count == group.shown.size() ? Check::Checked : Check::Indeterminate;
    }

    // Группа строки и номер члена среди её показанных; npos — строка самой группы.
    std::pair<Group*, uint32_t> locate(uint32_t index) const {
        for (Group const& group : groups_) {
            uint32_t const rows = group.rows();
            if (index < rows) {
                return {const_cast<Group*>(&group), index == 0 ? npos : index - 1};
            }
            index -= rows;
        }
        return {const_cast<Group*>(&groups_.back()), npos};
    }

    Editor& editor_;
    TypeEntry& entry_;
    std::string const name_;
    bool const enum_;
    std::vector<Group> groups_;  // адреса членов постоянны: группы не растут после разбора
    std::pair<Group const*, Member const*> selected_ {nullptr, nullptr};  // член пуст — выбрана группа
    mutable std::string count_;  // «N of M» строки группы, пока дерево её читает
};


// Карточка типа, которого нет в дереве Types, — интерфейса, делегата: по
// ссылке из сведений она встаёт в сведения на место выбранной строки. Строк у
// неё нет; модель она потому, что сведения следуют за выбором модели.
class TypeCard final : public TreeModel {
public:
    TypeCard(Editor& editor, md::TypeDef def) : editor_(editor), def_(def) {}

    uint32_t size() const override { return 0; }
    TreeRow row(uint32_t) const override { return {}; }
    void toggleExpanded(uint32_t) override {}
    void toggleChecked(uint32_t) override {}
    void invoke(uint32_t) override {}
    wxl::core::nullable<uint32_t> selectedRow() const override { return {}; }

    Description describe() const override {
        TypesModel const& types = *editor_.types_;
        std::wstring html;
        auto const category = md::get_category(def_);
        html += category == md::category::interface_type  ? L"interface "
                : category == md::category::delegate_type ? L"delegate "
                : category == md::category::struct_type   ? L"struct "
                : category == md::category::enum_type     ? L"enum "
                                                          : L"class ";
        std::string_view const name = def_.TypeName();
        append_bold(html, name.substr(0, name.find('`')));
        // Параметры обобщения — по именам.
        bool first = true;
        for (auto&& param : def_.GenericParam()) {
            html += first ? L"&lt;" : L", ";
            first = false;
            append_text(html, param.Name());
        }
        if (!first) {
            html += L"&gt;";
        }
        // Интерфейс требует других — как база у класса.
        first = true;
        for (auto&& impl : def_.InterfaceImpl()) {
            html += first ? L" : " : L", ";
            first = false;
            SignatureWriter {html, types, def_}.type(impl.Interface());
        }
        html += L"<br>Member of ";
        append_bold(html, def_.TypeNamespace());
        html += types.lists(full_name(def_)) ? L"<p>Hidden in the Types tree by the settings.</p>"
                         : L"<p>Not in the Types tree: the profile lists classes, structs and enums.</p>";

        // Члены — объявлениями, со ссылками на их типы. У делегата член один —
        // Invoke, сигнатура вызова; он помечен особым именем, и обход членов
        // его пропускает.
        auto const declared = declared_members_of(def_);
        std::wstring lines;
        if (category == md::category::delegate_type) {
            for (auto&& method : def_.MethodList()) {
                if (method.Name() == "Invoke") {
                    SignatureWriter {lines, types, def_}.write(member_declaration {def_, method}, method.Name());
                    lines += L"<br>";
                }
            }
        }
        for (auto const& [kind, names] : {std::pair {member_kind::Property, &declared.properties},
                                          std::pair {member_kind::Method, &declared.methods},
                                          std::pair {member_kind::Event, &declared.events}}) {
            for (std::string_view const member : *names) {
                for (member_declaration const& declaration : declarations_of(def_, kind, member)) {
                    SignatureWriter {lines, types, declaration.source}.write(declaration, member);
                    lines += L"<br>";
                }
            }
        }
        if (!lines.empty()) {
            html += L"<p>" + lines + L"</p>";
        }

        if (auto const documentation = editor_.data_->documentation.find(def_, DocumentationId::of(def_))) {
            append_documentation(html, **documentation);
        }
        RowIcon const* const icon = category == md::category::interface_type  ? &interfaceIcon
                                    : category == md::category::delegate_type ? &delegateIcon
                                    : category == md::category::struct_type   ? &structIcon
                                    : category == md::category::enum_type     ? &enumIcon
                                                                              : &classIcon;
        return {icon, std::move(html)};
    }

private:
    Editor& editor_;
    md::TypeDef const def_;
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
        for (dictionary_resource const& declared : editor_.data_->resources) {
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
        bool const selected = at == selected_;
        if (at.group) {
            if (at.key == npos) {
                return {.text = at.group->target,
                        .depth = 1,
                        .icon = &classIcon,
                        .expander = at.group->expanded ? Expander::Expanded : Expander::Collapsed,
                        .selected = selected};
            }
            std::string_view const key = at.group->keys[at.key];
            return {.text = key,
                    .depth = 2,
                    .icon = &styleIcon,
                    .check = styleCheck(*at.group, key),
                    .selected = selected};
        }
        if (at.key == npos) {
            return {.text = at.section->title,
                    .icon = at.section->icon,
                    .expander = at.section->expanded ? Expander::Expanded : Expander::Collapsed,
                    .selected = selected};
        }
        bool const chosen = editor_.data_->prof.brushes.allows(brushes_[at.key]);
        return {.text = brushes_[at.key],
                .depth = 1,
                .icon = &brushIcon,
                .check = chosen ? Check::Checked : Check::Unchecked,
                .selected = selected};
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

        profile& prof = editor_.data_->prof;
        if (!at.group) {
            toggle(prof.brushes, brushes_[at.key], brushes_);
        } else {
            StyleGroup const& group = *at.group;
            std::string_view const key = group.keys[at.key];
            if (group.type.empty()) {
                return;
            }
            if (auto const type = prof.types.find(group.type); type != prof.types.end()) {
                auto const styles = prof.styles.try_emplace(group.type, member_filter::all()).first;
                toggle(styles->second, key, group.keys);
                // Тип без членов и без стилей не генерирует ничего — уходит из
                // профиля вместе с пустым списком стилей.
                auto const empty = [](member_filter const& filter) {
                    return filter.kind == member_filter::kind_t::Allow && filter.names.empty();
                };
                if (empty(styles->second) && empty(type->second)) {
                    prof.styles.erase(styles);
                    prof.types.erase(type);
                    types_.relist(group.type);
                }
            } else {
                // Без своего типа стиль не генерируется: отметка вносит тип
                // корнем обхода без собственных членов и с одним этим стилем.
                prof.types.emplace(group.type, member_filter::allow({}));
                prof.styles.insert_or_assign(group.type, member_filter::allow({std::string {key}}));
                types_.relist(group.type);
            }
        }
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override {
        selected_ = locate(index);
        void const* const row = selected_.key == npos
                                    ? (selected_.group ? static_cast<void const*>(selected_.group) : selected_.section)
                                    : selected_.group ? static_cast<void const*>(&selected_.group->keys[selected_.key])
                                                      : &brushes_[selected_.key];
        editor_.selection.set({intrusive_ptr<TreeModel> {this}, row});
    }

    wxl::core::nullable<uint32_t> selectedRow() const override {
        auto const [section, group, key] = selected_;
        if (!section) {
            return {};
        }
        if (group) {
            if (!styles_.expanded || (key != npos && !group->expanded)) {
                return {};
            }
            return key == npos ? group->start : group->start + 1 + key;
        }
        if (key == npos) {
            return section->start;
        }
        if (!section->expanded) {
            return {};
        }
        return section->start + 1 + (section == &theme_ ? key - themeFrom_ : key);
    }

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
        Section* section = nullptr;
        StyleGroup* group = nullptr;
        uint32_t key = npos;

        bool operator==(Location const&) const = default;
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
        profile const& prof = editor_.data_->prof;
        if (!prof.types.contains(group.type)) {
            return Check::Unchecked;
        }
        auto const filter = prof.styles.find(group.type);
        return filter == prof.styles.end() || filter->second.allows(key) ? Check::Checked : Check::Unchecked;
    }

    // Отметка в списке, который без записи значит «все»: снятая превращает
    // «все» в список остальных, а список, где выбрано всё, снова становится «все».
    static void toggle(member_filter& filter, std::string_view key, std::vector<std::string_view> const& all) {
        if (filter.kind == member_filter::kind_t::Allow) {
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
            filter = member_filter::allow(std::move(names));
        }
        if (std::ranges::all_of(all, [&filter](std::string_view each) { return filter.allows(each); })) {
            filter = member_filter::all();
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
    Location selected_;
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

Description TypesModel::describe() const {
    auto const [library, space, entry] = selected_;
    if (entry) {
        return {entry->icon, typeInfo(*space, *entry)};
    }
    if (space) {
        return {&namespaceIcon, namespaceInfo(*library, *space)};
    }
    return library ? Description {&libraryIcon, libraryInfo(*library)} : Description {};
}

Editor::Editor() = default;

intrusive_ptr<TreeModel> Editor::types() const {
    return types_;
}

intrusive_ptr<TreeModel> Editor::resources() const {
    return resources_;
}

void Editor::hideDependencyProperties(bool hide) {
    if (data_->hideDependencyProperties == hide) {
        return;
    }
    data_->hideDependencyProperties = hide;
    if (auto const& shown = members.get()) {
        static_cast<MembersModel&>(*shown).refilter();
    }
    revision.set(revision.get() + 1);
}

void Editor::hideEventArgs(bool hide) {
    if (data_->hideEventArgs != hide) {
        data_->hideEventArgs = hide;
        types_->refilter();
        revision.set(revision.get() + 1);
    }
}

void Editor::hidePredefined(bool hide) {
    if (data_->hidePredefined != hide) {
        data_->hidePredefined = hide;
        types_->refilter();
        revision.set(revision.get() + 1);
    }
}

void Editor::englishDocumentation(bool english) {
    data_->documentation.english(english);
    revision.set(revision.get() + 1);
}

bool Editor::prepareDocumentation() {
    bool const more = data_->documentation.parseSome(std::chrono::steady_clock::now() + std::chrono::milliseconds {4});
    if (!more) {
        documentationPending.set(false);
    }
    return more;
}

wxl::core::nullable<uint32_t> Editor::reveal(std::wstring_view type) {
    std::string utf8;
    if (auto const text = wxl::core::unicode::checked(type)) {
        wxl::core::unicode::append_utf8(utf8, *text);
    }
    if (auto const row = types_->reveal(utf8)) {
        return row;
    }
    if (md::TypeDef const def = find_type(data_->db, utf8)) {
        intrusive_ptr<TreeModel> const card {new TypeCard {*this, def}, /*add_ref=*/false};
        selection.set({card, card.get()});
    }
    return {};
}
Editor::~Editor() = default;

intrusive_ptr<Editor> Editor::open(std::filesystem::path const& prof) {
    intrusive_ptr<Editor> editor {new Editor {}, /*add_ref=*/false};

    auto const resolved = resolve_profiles({prof}, default_nuget_root());
    std::vector<std::string> files;
    files.reserve(resolved.metadata.size());
    for (auto&& file : resolved.metadata) {
        files.push_back(file.string());
    }

    editor->data_ = std::make_unique<Data>(load_profile(prof), files,
                                         [raw = editor.get()] { raw->documentationPending.set(true); });
    for (auto&& dictionary : resolved.resources) {
        auto declared = dictionary_resources(dictionary);
        editor->data_->resources.insert(editor->data_->resources.end(), std::move_iterator {declared.begin()},
                                        std::move_iterator {declared.end()});
    }
    // Карта типов — общая у профилей одного каталога, как её ищет генератор.
    if (auto const map = prof.parent_path() / "types.json"; std::filesystem::exists(map)) {
        editor->data_->typeMap = load_type_map(map);
    }
    editor->types_ = intrusive_ptr<TypesModel> {new TypesModel {*editor}, /*add_ref=*/false};
    editor->resources_ = intrusive_ptr<ResourcesModel> {new ResourcesModel {*editor, *editor->types_}, /*add_ref=*/false};
    editor->info.follow(editor->selection, editor->revision, [](Selection const& chosen, uint32_t) {
        return chosen.model ? chosen.model->describe() : Description {};
    });
    auto const file = prof.filename().wstring();
    editor->title.set(std::u16string {file.begin(), file.end()} + u" — wxl.gen.ui");
    editor->path.set(std::filesystem::path {prof}.make_preferred().u16string());
    return editor;
}

}  // namespace editor
