// Метаданные и профиль — только здесь: crawl.h приносит winmd_reader.h вместе с
// <windows.h> в нужном порядке, а окно видит одни модели деревьев.
#include "crawl.h"
#include "profile.h"
#include "xml_input.h"

#include "Editor.h"

#include <algorithm>
#include <array>
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
// запрещающий списки просто пополняются и худеют.
void mark_member(std::map<std::string, MemberFilter>& types, std::string const& type,
                 std::string_view member, bool on, bool unlistedKeepsAll) {
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

struct Editor::Data {
    Data(Profile own, std::vector<std::string> const& files)
        : profile(std::move(own)), db(files) {}

    // Отметки — то, что говорит сам файл, без профилей, которые он продолжает.
    Profile profile;
    md::cache db;

    // Ресурсы словарей XAML с ключами, как их объявил документ; модель ресурсов
    // смотрит в их строки.
    std::vector<DictionaryResource> resources;
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
            std::string_view const path = source.path();
            libraries_.push_back({.source = &source, .name = path.substr(path.find_last_of("\\/") + 1)});
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
                    .expander = library->expanded ? Expander::Expanded : Expander::Collapsed};
        }
        if (!type) {
            return {.text = space->name,
                    .depth = 1,
                    .icon = &namespaceIcon,
                    .expander = space->expanded ? Expander::Expanded : Expander::Collapsed};
        }

        return {.text = type->def.TypeName(),
                .depth = 2,
                .icon = type->icon,
                .check = type->listed ? Check::Checked : Check::Unchecked,
                .selected = type->def == selected_};
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

    // Тип внесли в профиль или вынесли из него не отсюда — отметкой стиля.
    void relist(std::string_view full) {
        if (TypeEntry* entry = find(full)) {
            entry->listed = editor_.data_->profile.types.contains(std::string {full});
        }
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

    TypeEntry* find(std::string_view full) {
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

    Editor& editor_;
    std::vector<Library> libraries_;
    uint32_t size_ = 0;
    md::TypeDef selected_;
};

class MembersModel final : public TreeModel {
public:
    MembersModel(Editor& editor, TypeEntry& entry)
        : editor_(editor),
          entry_(entry),
          name_(std::format("{}.{}", entry.def.TypeNamespace(), entry.def.TypeName())),
          enum_(md::get_category(entry.def) == md::category::enum_type) {
        auto declared = declared_members_of(entry.def);
        if (enum_) {
            groups_.push_back({"Values", &constantIcon, std::move(declared.constants)});
        } else {
            groups_.push_back({"Properties", &propertyIcon, std::move(declared.properties)});
            groups_.push_back({"Methods", &methodIcon, std::move(declared.methods)});
            groups_.push_back({"Events", &eventIcon, std::move(declared.events)});
        }
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
                    .expander = group->names.empty() ? Expander::None
                                : group->expanded    ? Expander::Expanded
                                                     : Expander::Collapsed};
        }

        std::string_view const name = group->names[member];
        return {.text = name,
                .depth = 1,
                .icon = group->icon,
                .check = allows(name) ? Check::Checked : Check::Unchecked};
    }

    void toggleExpanded(uint32_t index) override {
        auto const [group, member] = locate(index);
        if (member == npos) {
            group->expanded = !group->expanded;
        }
    }

    void toggleChecked(uint32_t index) override {
        auto const [group, member] = locate(index);
        if (member == npos) {
            return;
        }

        std::string_view const name = group->names[member];
        auto& types = editor_.data_->profile.types;
        mark_member(types, name_, name, !allows(name), enum_);
        entry_.listed = types.contains(name_);
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t) override {}

private:
    static constexpr uint32_t npos = UINT32_MAX;

    struct Group {
        std::string_view title;
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
            if (profile.types.contains(group.type)) {
                toggle(profile.styles.try_emplace(group.type, MemberFilter::all()).first->second, key,
                       group.keys);
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
    TypeEntry* const entry = locate(index).type;
    if (!entry) {
        return;
    }

    selected_ = entry->def;
    editor_.typeName.set(to_u16(full_name(entry->def)));
    editor_.members.set(intrusive_ptr<TreeModel> {new MembersModel {editor_, *entry}, /*add_ref=*/false});
}

Editor::Editor() = default;

intrusive_ptr<TreeModel> Editor::types() const {
    return types_;
}

intrusive_ptr<TreeModel> Editor::resources() const {
    return resources_;
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
    auto const file = profile.filename().wstring();
    editor->title.set(std::u16string {file.begin(), file.end()} + u" — wxl.gen.ui");
    return editor;
}

}  // namespace editor
