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

class TypesModel final : public TreeModel {
public:
    explicit TypesModel(Editor& editor) : editor_(editor) {
        auto const& db = editor_.data_->db;
        for (auto&& [name, members] : db.namespaces()) {
            if (name.empty()) {
                continue;
            }
            // Показываются только классы, структуры и перечисления; пространство,
            // где их нет, не показывается вовсе.
            auto const shown = members.classes.size() + members.structs.size() + members.enums.size();
            if (shown == 0) {
                continue;
            }
            Namespace& space = namespaces_.emplace_back();
            space.name = name;
            space.types.reserve(shown);
            // Списки кэша, а не категория типа: атрибуты — тоже классы, а
            // контракты — структуры, и кэш держит их отдельно.
            for (auto&& [kind, icon] : {std::pair {&members.classes, &classIcon},
                                        std::pair {&members.structs, &structIcon},
                                        std::pair {&members.enums, &enumIcon}}) {
                for (auto&& def : *kind) {
                    space.types.push_back({def, icon});
                }
            }
            std::ranges::sort(space.types, {}, [](TypeEntry const& entry) { return entry.def.TypeName(); });
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
        auto const [space, type] = locate(index);
        if (type == npos) {
            return {.text = space->name,
                    .icon = &namespaceIcon,
                    .expander = space->expanded ? Expander::Expanded : Expander::Collapsed};
        }

        TypeEntry const& entry = space->types[type];
        return {.text = entry.def.TypeName(),
                .depth = 1,
                .icon = entry.icon,
                .check = entry.listed ? Check::Checked : Check::Unchecked,
                .selected = entry.def == selected_};
    }

    void toggleExpanded(uint32_t index) override {
        auto const [space, type] = locate(index);
        if (type == npos) {
            space->expanded = !space->expanded;
            recount();
        }
    }

    void toggleChecked(uint32_t index) override {
        auto const [space, type] = locate(index);
        if (type == npos) {
            return;
        }

        TypeEntry& entry = space->types[type];
        auto& types = editor_.data_->profile.types;
        std::string const name = full_name(entry.def);
        if (entry.listed) {
            types.erase(name);
        } else {
            types.emplace(name, MemberFilter::all());
        }
        entry.listed = !entry.listed;
        editor_.revision.set(editor_.revision.get() + 1);
    }

    void invoke(uint32_t index) override;

private:
    static constexpr uint32_t npos = UINT32_MAX;

    struct Namespace {
        std::string_view name;
        std::vector<TypeEntry> types;
        bool expanded = false;
        uint32_t start = 0;  // номер строки самого пространства среди видимых
    };

    static std::string full_name(md::TypeDef const& type) {
        return std::format("{}.{}", type.TypeNamespace(), type.TypeName());
    }

    void recount() {
        uint32_t start = 0;
        for (Namespace& space : namespaces_) {
            space.start = start;
            start += 1 + (space.expanded ? static_cast<uint32_t>(space.types.size()) : 0);
        }
        size_ = start;
    }

    // Пространство имён видимой строки и номер типа в нём; npos — строка самого
    // пространства.
    std::pair<Namespace*, uint32_t> locate(uint32_t index) const {
        auto const after = std::ranges::upper_bound(namespaces_, index, {}, &Namespace::start);
        auto* space = const_cast<Namespace*>(&*std::prev(after));
        return {space, index == space->start ? npos : index - space->start - 1};
    }

    TypeEntry* find(std::string_view full) {
        auto const dot = full.rfind('.');
        if (dot == std::string_view::npos) {
            return nullptr;
        }
        auto const space_name = full.substr(0, dot);
        auto const type_name = full.substr(dot + 1);

        auto const space = std::ranges::lower_bound(namespaces_, space_name, {}, &Namespace::name);
        if (space == namespaces_.end() || space->name != space_name) {
            return nullptr;
        }
        auto const type = std::ranges::lower_bound(
            space->types, type_name, {}, [](TypeEntry const& entry) { return entry.def.TypeName(); });
        if (type == space->types.end() || type->def.TypeName() != type_name) {
            return nullptr;
        }
        return &*type;
    }

    Editor& editor_;
    std::vector<Namespace> namespaces_;
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
    auto const [space, type] = locate(index);
    if (type == npos) {
        return;
    }

    TypeEntry& entry = space->types[type];
    selected_ = entry.def;
    editor_.typeName.set(to_u16(full_name(entry.def)));
    editor_.members.set(intrusive_ptr<TreeModel> {new MembersModel {editor_, entry}, /*add_ref=*/false});
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
