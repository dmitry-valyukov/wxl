// Метаданные и профиль — только здесь: crawl.h приносит winmd_reader.h вместе с
// <windows.h> в нужном порядке, а окно видит одни модели деревьев.
#include "crawl.h"
#include "profile.h"

#include "Editor.h"

#include <algorithm>
#include <array>
#include <format>
#include <map>
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

        auto& types = editor_.data_->profile.types;
        std::string const name = full_name(entry->def);
        if (entry->listed) {
            types.erase(name);
        } else {
            types.emplace(name, MemberFilter::all());
        }
        entry->listed = !entry->listed;
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override;

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
    editor->types_ = intrusive_ptr<TreeModel> {new TypesModel {*editor}, /*add_ref=*/false};
    auto const file = profile.filename().wstring();
    editor->title.set(std::u16string {file.begin(), file.end()} + u" — wxl.gen.ui");
    return editor;
}

}  // namespace editor
