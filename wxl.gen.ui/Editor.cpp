// Метаданные и профиль — только здесь: crawl.h приносит winmd_reader.h вместе с
// <windows.h> в нужном порядке, а окно видит одни модели деревьев.
#include "crawl.h"
#include "documentation.h"
#include "profile.h"
#include "xml_input.h"

#include "Editor.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
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

// IClosable: его close() есть у всякого, кто его поддерживает; в проекции это
// база wxl::Closable.
constexpr std::string_view closableInterface = "Windows.Foundation.IClosable";

// Текст метаданных в разметку HtmlBlock: знаки разметки — сущностями. Текст не
// UTF-8 — сообщение об ошибке, строка метаданных — выходит со знаками U+FFFD на
// месте сломанного, а не пропадает.
void append_text(std::wstring& html, std::string_view utf8) {
    std::wstring wide;
    wxl::core::unicode::append_utf16(wide, wxl::core::unicode::repaired(utf8));
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
    // Пустой запрещающий список — все члены, пустой разрешающий — ни одного:
    // тип выходит ради стилей или как зависимость.
    if (filter->kind == member_filter::kind_t::All ||
        (filter->kind == member_filter::kind_t::Deny && filter->names.empty())) {
        html += L"all members</p>";
        return;
    }
    if (filter->names.empty()) {
        html += L"listed, no members</p>";
        return;
    }
    html += filter->kind == member_filter::kind_t::Allow ? L"only " : L"all members except ";
    bool first = true;
    for (std::string const& name : filter->names) {
        html += first ? L"" : L", ";
        append_text(html, name);
        first = false;
    }
    html += L"</p>";
}

// Имя из метаданных: не UTF-8 — выходит со знаками U+FFFD, как у append_text.
std::u16string to_u16(std::string_view utf8) {
    return wxl::core::unicode::repaired(utf8).to_utf16().plain();
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

// Язык интерфейса Windows — папками справочника Windows SDK, только теми, что
// есть в documentation_languages: у японского и китайского папка называется не
// так, как язык локали, у остальных — так же. Справочник на английском есть
// всегда, он последний.
std::vector<std::wstring> interface_documentation_languages() {
    std::vector<std::wstring> languages;
    auto const add = [&](std::wstring_view folder) {
        // Имена локалей и коды таблицы — ASCII, сужение — копия.
        std::string code(folder.size(), '\0');
        std::ranges::transform(folder, code.begin(), [](wchar_t c) { return static_cast<char>(c); });
        documentation_language const* const known = find_documentation_language(code);
        if (!known) {
            return;
        }
        std::wstring candidate(known->code.begin(), known->code.end());
        if (std::ranges::find(languages, candidate) == languages.end()) {
            languages.push_back(std::move(candidate));
        }
    };
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] {};
    if (::LCIDToLocaleName(::GetUserDefaultUILanguage(), locale, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        std::wstring name {locale};
        std::ranges::transform(name, name.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        std::wstring_view const language = std::wstring_view {name}.substr(0, name.find(L'-'));
        if (language == L"zh") {
            bool const traditional = name == L"zh-tw" || name == L"zh-hk" || name == L"zh-mo" || name.contains(L"hant");
            add(traditional ? L"zh-hant" : L"zh-hans");
        } else if (language == L"ja") {
            add(L"jp");
        } else {
            add(name);
            add(language);
        }
    }
    add(L"en");
    return languages;
}

void append_inline(std::wstring& html, doc_run const& run);

void append_inline_runs(std::wstring& html, doc_text const& text) {
    for (doc_run const& run : text) {
        append_inline(html, run);
    }
}

void append_tagged(std::wstring& html, std::wstring_view tag, doc_text const& text) {
    html += std::format(L"<{}>", tag);
    append_inline_runs(html, text);
    html += std::format(L"</{}>", tag);
}

// Код в обратных кавычках — тегом code, как его показывает Doxygen: текст между
// серией кавычек в начале и такой же в конце, как написан, с пробелами у кавычек.
void append_code(std::wstring& html, std::string_view span) {
    std::size_t const ticks = span.find_first_not_of('`');
    html += L"<code>";
    append_text(html, span.substr(ticks, span.size() - 2 * ticks));
    html += L"</code>";
}

// Строчная часть описания в разметку HtmlBlock: жирный, курсив, индексы,
// перенос и код — её тегами, картинка — своей подписью, сети у панели нет, а без
// подписи — адресом: картинка не пропадает бесследно.
void append_inline(std::wstring& html, doc_run const& run) {
    using kind = doc_run::kind_t;
    switch (run.kind) {
        case kind::text: append_text(html, run.text); break;
        case kind::code: append_code(html, run.text); break;
        case kind::bold: append_tagged(html, L"b", run.runs); break;
        case kind::italic: append_tagged(html, L"i", run.runs); break;
        case kind::superscript: append_tagged(html, L"sup", run.runs); break;
        case kind::subscript: append_tagged(html, L"sub", run.runs); break;
        case kind::line_break: html += L"<br>"; break;
        case kind::image: append_text(html, run.text.empty() ? run.url : run.text); break;
        // Блоки — у append_markup: внутри строчного их не бывает.
        case kind::definitions:
        case kind::definition:
        case kind::table:
        case kind::row:
        case kind::header_cell:
        case kind::cell: break;
    }
}

// Описание в абзац раздела сведений. Таблица и список терминов — блоки HtmlBlock:
// перед ними абзац закрывается, а строчное после них открывает новый; открыт ли
// абзац — в open, закрывает его раздел. Термин — код: в справочниках это схемы
// адресов, ms-appx:///.
void append_markup(std::wstring& html, doc_text const& text, bool& open) {
    using kind = doc_run::kind_t;
    for (doc_run const& run : text) {
        bool const block = run.kind == kind::table || run.kind == kind::definitions;
        if (block && open) {
            html += L"</p>";
            open = false;
        } else if (!block && !open) {
            html += L"<p>";
            open = true;
        }
        if (run.kind == kind::definitions) {
            html += L"<ul>";
            for (doc_run const& definition : run.runs) {
                html += L"<li><code>";
                append_text(html, definition.text);
                html += L"</code> — ";
                append_inline_runs(html, definition.runs);
                html += L"</li>";
            }
            html += L"</ul>";
        } else if (run.kind == kind::table) {
            html += L"<table>";
            for (doc_run const& row : run.runs) {
                html += L"<tr>";
                for (doc_run const& cell : row.runs) {
                    append_tagged(html, cell.kind == kind::header_cell ? L"th" : L"td", cell.runs);
                }
                html += L"</tr>";
            }
            html += L"</table>";
        } else {
            append_inline(html, run);
        }
    }
}

// Раздел сведений: `head` открывает его абзац, за ним — описание.
void append_section(std::wstring& html, std::wstring_view head, doc_text const& text) {
    html += head;
    bool open = true;
    append_markup(html, text, open);
    if (open) {
        html += L"</p>";
    }
}

// Документация члена или типа под сведениями о нём.
void append_documentation(std::wstring& html, member_documentation const& documentation) {
    if (!documentation.deprecated.empty()) {
        append_section(html, L"<p><b>Deprecated:</b> ", documentation.deprecated);
    }
    if (!documentation.summary.empty()) {
        append_section(html, L"<p><b>Summary:</b><br>", documentation.summary);
    }
    // Параметр без описания — такие в файлах есть — не показывается.
    auto const described = [](auto const& param) { return !param.second.empty(); };
    if (std::ranges::any_of(documentation.params, described)) {
        html += L"<p><b>Parameters:</b>";
        bool open = true;
        for (auto const& [name, text] : documentation.params | std::views::filter(described)) {
            html += open ? L"<br><i>" : L"<p><i>";
            open = true;
            append_text(html, name);
            html += L"</i> — ";
            append_markup(html, text, open);
        }
        if (open) {
            html += L"</p>";
        }
    }
    if (!documentation.returns.empty()) {
        append_section(html, L"<p><b>Returns:</b><br>", documentation.returns);
    }
}

// Документация метаданных (documentation_source) на языке интерфейса, а без
// него на английском; справочник — того Windows SDK сборки, из которого прочитаны
// метаданные Windows (`sdk`, из списка восстановления). Открылся
// файл, где разобрано ещё не всё, — зовётся opened: остальное окно дочитывает в
// фоне, шагами parseSome().
class Documentation {
public:
    Documentation(windows_sdk const& sdk, wxl::core::function<void()> opened)
        : opened_(std::move(opened)), source_ {interface_documentation_languages(), sdk} {}

    // Документация типа или члена под сведениями о нём, а файл, который не
    // читается, — своей ошибкой на её месте: имя файла и что с ним не так.
    void append(std::wstring& html, md::TypeDef const& type, std::string const& id) {
        bool const parsing = source_.parsing();
        try {
            if (member_documentation const* const found = source_.find(type, id)) {
                append_documentation(html, *found);
            }
        } catch (std::exception const& error) {
            html += L"<p><b>Documentation:</b> ";
            append_text(html, wxl::core::environment::error_message(error).chars());
            html += L"</p>";
        }
        if (!parsing && source_.parsing()) {
            opened_();
        }
    }

    // Справочник Windows SDK — на английском или на языке интерфейса. Открытые
    // файлы справочника закрываются: при следующей нужде открывается файл
    // нужного языка. Документация рядом с .winmd одна и остаётся.
    void english(bool value) {
        source_.languages(value ? std::vector<std::wstring> {L"en"} : interface_documentation_languages());
    }

    // Разбирает члены открытых файлов до срока; остались ли неразобранные.
    bool parseSome(std::chrono::steady_clock::time_point deadline) { return source_.parse_some(deadline); }

private:
    wxl::core::function<void()> opened_;
    documentation_source source_;
};

// Корни обхода из `own`, которых в замыкании нет без них самих: никакой другой
// корень до них не доходит. Корень, до которого доходят, лежит в компоненте
// сильной связности рёбер обхода вместе с другим корнем или получает ребро
// извне своей компоненты: всё замыкание достижимо из корней, и ребро извне
// идёт от вершины, до которой доходит другой корень, — иначе она лежала бы в
// той же компоненте. Компоненты — Тарьяном, без рекурсии: цепочки рёбер
// длинные.
std::set<md::TypeDef> unpulled_roots(closure const& walked, std::set<md::TypeDef> const& roots,
                                     std::set<md::TypeDef> const& own) {
    std::map<md::TypeDef, uint32_t> number;
    std::vector<md::TypeDef> nodes;
    nodes.reserve(walked.surface.size());
    for (auto&& [type, filter] : walked.surface) {
        number.emplace(type, static_cast<uint32_t>(nodes.size()));
        nodes.push_back(type);
    }
    auto const count = static_cast<uint32_t>(nodes.size());
    std::vector<std::vector<uint32_t>> edges(count);
    for (auto&& [from, targets] : walked.dependencies) {
        if (auto const source = number.find(from); source != number.end()) {
            for (auto&& on : targets) {
                if (auto const target = number.find(on); target != number.end()) {
                    edges[source->second].push_back(target->second);
                }
            }
        }
    }

    constexpr uint32_t unvisited = UINT32_MAX;
    std::vector<uint32_t> order(count, unvisited);
    std::vector<uint32_t> low(count);
    std::vector<uint32_t> component(count, unvisited);
    std::vector<uint32_t> stack;
    std::vector<std::pair<uint32_t, size_t>> calls;  // вершина и её следующее ребро
    uint32_t visited = 0;
    uint32_t components = 0;
    auto const enter = [&](uint32_t node) {
        order[node] = low[node] = visited++;
        stack.push_back(node);
        calls.emplace_back(node, 0);
    };
    for (uint32_t start = 0; start < count; ++start) {
        if (order[start] != unvisited) {
            continue;
        }
        enter(start);
        while (!calls.empty()) {
            uint32_t const node = calls.back().first;
            if (size_t const next = calls.back().second++; next < edges[node].size()) {
                uint32_t const to = edges[node][next];
                if (order[to] == unvisited) {
                    enter(to);
                } else if (component[to] == unvisited) {
                    low[node] = std::min(low[node], order[to]);
                }
                continue;
            }
            calls.pop_back();
            if (!calls.empty()) {
                low[calls.back().first] = std::min(low[calls.back().first], low[node]);
            }
            if (low[node] == order[node]) {
                uint32_t member = unvisited;
                do {
                    member = stack.back();
                    stack.pop_back();
                    component[member] = components;
                } while (member != node);
                ++components;
            }
        }
    }

    std::vector<uint32_t> rootsIn(components);
    std::vector<bool> entered(components);
    for (uint32_t node = 0; node < count; ++node) {
        rootsIn[component[node]] += roots.contains(nodes[node]) ? 1 : 0;
        for (uint32_t const to : edges[node]) {
            if (component[to] != component[node]) {
                entered[component[to]] = true;
            }
        }
    }
    std::set<md::TypeDef> unpulled;
    for (md::TypeDef const& root : own) {
        if (auto const at = number.find(root); at != number.end()) {
            uint32_t const part = component[at->second];
            if (!entered[part] && rootsIn[part] == 1) {
                unpulled.insert(root);
            }
        }
    }
    return unpulled;
}

}  // namespace

struct Editor::Data {
    Data(profile own, profile_set all, std::vector<std::string> const& files, wxl::core::function<void()> opened)
        : prof(std::move(own)), resolved(std::move(all)), db(files),
          documentation(resolved.sdk, std::move(opened)) {}

    // Отметки — то, что говорит сам файл, без профилей, которые он продолжает.
    profile prof;

    // Профиль со всеми, которые он продолжает, каким открыт; типы и стили тех,
    // что он продолжает, — отдельно: поверх них ложатся отметки самого файла.
    profile_set resolved;
    std::map<std::string, member_filter> inheritedTypes;
    std::map<std::string, member_filter> inheritedStyles;

    md::cache db;

    // Замыкание профиля с отметками, какие они сейчас: что генератор выпустит
    // зависимостями выбранного. Считается заново, только когда отметки менялись.
    // Вход обхода: профили, которые этот продолжает, и поверх них — отметки
    // `own`, какими они есть или какими станут.
    profile_set inputFor(profile const& own) const {
        profile_set input = resolved;
        input.types = inheritedTypes;
        input.styles = inheritedStyles;
        for (auto&& [mine, inherited] : {std::pair {&own.types, &input.types}, std::pair {&own.styles, &input.styles}}) {
            for (auto&& [name, filter] : *mine) {
                if (auto [at, added] = inherited->try_emplace(name, filter); !added) {
                    at->second.merge(filter);
                }
            }
        }
        return input;
    }

    closure const& walk() {
        if (walkStale) {
            profile_set const input = inputFor(prof);
            walked = crawl(input, typeMap, db);
            walkStale = false;
            ++generation;
            costs.clear();

            roots.clear();
            std::set<md::TypeDef> all;
            std::set<md::TypeDef> own;
            for (auto&& [name, filter] : input.types) {
                if (md::TypeDef const type = db.find(name)) {
                    all.insert(type);
                    bool const mine = !inheritedTypes.contains(name);
                    roots.emplace(type, mine ? Root::Own : Root::Extended);
                    if (mine) {
                        own.insert(type);
                    }
                }
            }
            for (auto&& name : typeMap.implicit_roots) {
                if (md::TypeDef const type = db.find(name)) {
                    all.insert(type);
                    roots.insert_or_assign(type, Root::TypeMap);
                    own.erase(type);
                }
            }
            auto const unpulled = unpulled_roots(walked, all, own);
            required.clear();
            for (auto&& [type, filter] : walked.surface) {
                if (!unpulled.contains(type)) {
                    required.insert(type);
                }
            }
        }
        return walked;
    }
    closure walked;
    bool walkStale = true;
    uint64_t generation = 0;  // обходов с открытия: по нему устаревает цена

    // Откуда корень обхода: из отметок этого файла, из профиля, который он
    // продолжает, или из карты типов.
    enum class Root : uint8_t { Own, Extended, TypeMap };
    std::map<md::TypeDef, Root> roots;

    // Цена отметки: что изменится в замыкании, если отметки станут `changed`, —
    // типы, которые придут и уйдут, и на сколько изменятся число членов и
    // ключей (имён свойств и событий). Считается обходом заново; запоминается
    // по `key` до следующей отметки.
    struct Cost {
        std::vector<md::TypeDef> added;
        std::vector<md::TypeDef> dropped;
        ptrdiff_t members = 0;
        ptrdiff_t keys = 0;
    };
    Cost const& costOf(std::string const& key, profile const& changed) {
        closure const& now = walk();
        if (auto const found = costs.find(key); found != costs.end()) {
            return found->second;
        }
        closure const then = crawl(inputFor(changed), typeMap, db);
        // Служебные интерфейсы класса — его же члены, обобщённые интерфейсы
        // проецирует карта типов: ни те ни другие отдельным типом не выходят.
        auto const counted = [](md::TypeDef const& type) {
            return !is_exclusive_interface(type) && !type.TypeName().contains('`');
        };
        Cost cost;
        for (auto&& [type, filter] : then.surface) {
            if (counted(type) && !now.surface.contains(type)) {
                cost.added.push_back(type);
            }
        }
        for (auto&& [type, filter] : now.surface) {
            if (counted(type) && !then.surface.contains(type)) {
                cost.dropped.push_back(type);
            }
        }
        for (auto* list : {&cost.added, &cost.dropped}) {
            std::ranges::sort(*list, by_full_name {});
        }
        // Член, который класс объявляет и сам, и в служебном интерфейсе, —
        // один член.
        auto const members = [&counted](closure const& walk) {
            ptrdiff_t count = 0;
            for (auto&& [type, names] : walk.members) {
                count += counted(type) ? static_cast<ptrdiff_t>(names.size()) : 0;
            }
            return count;
        };
        auto const keys = [](closure const& walk) {
            return static_cast<ptrdiff_t>(walk.property_names.size() + walk.event_names.size());
        };
        cost.members = members(then) - members(now);
        cost.keys = keys(then) - keys(now);
        return costs.emplace(key, std::move(cost)).first->second;
    }
    std::map<std::string, Cost> costs;

    // Типы, которые генератор выпустит, что бы ни говорили отметки самого
    // файла: их тянет другой тип замыкания, их называет профиль, который этот
    // продолжает, или карта типов. Отметка у них стоит и не снимается; что
    // записано в файле, остаётся в нём и вернётся в отметку, как только тип
    // тянуть перестанут.
    bool needs(md::TypeDef const& type) {
        walk();
        return required.contains(type);
    }
    std::set<md::TypeDef> required;

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
    bool listed = false;  // выбран явно: в профиле самого файла
    // Нужен замыканию и без своей отметки (Editor::Data::needs): отмечен и не
    // снимается. Помечается при каждом отборе.
    bool needed = false;
    // Не скрыт настройками; показан ли, решает ещё отбор панели инструментов.
    bool offered = true;
    bool shown = true;
    // Класс аргументов события: имя на EventArgs у него самого или у одной из
    // его баз. Помечается при разборе; показывать ли — настройка.
    bool eventArgs = false;
    // Предопределён вне проекции: wxl пишет его сам (predefined в types.json).
    // Показанный — без выбора.
    bool predefined = false;

    // Отметку можно ставить и считать: не скрыт настройками и не
    // предопределён. Отбор панели счёта не меняет.
    bool selectable() const { return offered && !predefined; }

    // Отметка стоит: явная или замыкания.
    bool chosen() const { return listed || needed; }
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

// База класса, если она своя, а не System.Object, и есть в метаданных.
md::TypeDef base_of(md::TypeDef const& type) {
    auto const base = type.Extends();
    if (!base || base.type() == md::TypeDefOrRef::TypeSpec) {
        return {};
    }
    auto const [ns, name] = md::get_type_namespace_and_name(base);
    if (ns == "System" && name == "Object") {
        return {};
    }
    return md::find(base);
}

// Объявляет ли тип IClosable сам, а не через базу.
bool declares_closable(md::TypeDef const& type) {
    for (auto&& impl : type.InterfaceImpl()) {
        auto const ref = impl.Interface();
        if (ref.type() != md::TypeDefOrRef::TypeSpec) {
            auto const [ns, name] = md::get_type_namespace_and_name(ref);
            if (ns == "Windows.Foundation" && name == "IClosable") {
                return true;
            }
        }
    }
    return false;
}

// Встаёт ли Closable в цепочку баз типа: тип — корень своей иерархии, или
// IClosable уже есть у одной из баз. Иначе Closable пришлось бы ставить над
// базой, которая его не поддерживает, и IClosable у типа — обычный требуемый
// интерфейс: его член выбирается в профиле.
bool closable_fits(md::TypeDef const& type) {
    md::TypeDef base = base_of(type);
    if (!base) {
        return true;
    }
    for (; base; base = base_of(base)) {
        if (declares_closable(base)) {
            return true;
        }
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
            // Показываются классы, структуры, перечисления и интерфейсы;
            // пространство, где их нет, не показывается вовсе. Списки кэша, а не категория типа:
            // атрибуты — тоже классы, а контракты — структуры, и кэш держит их
            // отдельно.
            for (auto&& [kind, icon] : {std::pair {&members.classes, &classIcon},
                                        std::pair {&members.structs, &structIcon},
                                        std::pair {&members.enums, &enumIcon},
                                        std::pair {&members.interfaces, &interfaceIcon}}) {
                for (auto&& def : *kind) {
                    // Из интерфейсов — только самостоятельные: служебные
                    // интерфейсы класса видны его членами, обобщённые
                    // проецирует карта типов.
                    if (icon == &interfaceIcon && (def.TypeName().contains('`') || is_exclusive_interface(def))) {
                        continue;
                    }
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

    // Показываемое и нужное замыканию — заново, по отметкам, настройкам и
    // панели инструментов; счёт — среди тех, что не скрыты настройками, так что
    // «только выбранные» его не меняет. Скрытый выбранный тип больше не выбран;
    // отбор панели его не скрывает.
    void refilter() {
        bool const hideArgs = editor_.data_->hideEventArgs;
        bool const hidePredefined = editor_.data_->hidePredefined;
        bool const onlyChosen = editor_.onlyChosen.get();
        // Предопределённый тип не выбирается и не выпускается: его показ — настройка.
        auto const passes = [&](TypeEntry const& entry) {
            return !onlyChosen || entry.chosen() || entry.predefined || &entry == selected_.type;
        };
        shown_.clear();
        for (Library& library : libraries_) {
            library.shown.clear();
            library.total = 0;
            library.chosen = 0;
            for (Namespace& space : library.namespaces) {
                space.shown.clear();
                space.total = 0;
                space.chosen = 0;
                for (TypeEntry& entry : space.types) {
                    entry.needed = editor_.data_->needs(entry.def);
                    entry.offered = !(hideArgs && entry.eventArgs) && !(hidePredefined && entry.predefined);
                    entry.shown = entry.offered && passes(entry);
                    if (entry.shown) {
                        space.shown.push_back(&entry);
                    }
                    if (entry.selectable()) {
                        ++space.total;
                        space.chosen += entry.chosen() ? 1 : 0;
                    }
                }
                if (!space.shown.empty()) {
                    library.shown.push_back(&space);
                }
                library.total += space.total;
                library.chosen += space.chosen;
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
        // У файла и пространства серым — сколько их типов отмечено: явно или замыканием.
        if (!space) {
            detail_ = std::format("{} of {}", library->chosen, library->total);
            return {.text = library->name,
                    .detail = detail_,
                    .icon = &libraryIcon,
                    .expander = library->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = triple(library->chosen, library->total),
                    .selected = library == selected_.library && !selected_.space};
        }
        if (!type) {
            detail_ = std::format("{} of {}", space->chosen, space->total);
            return {.text = space->name,
                    .detail = detail_,
                    .depth = 1,
                    .icon = &namespaceIcon,
                    .expander = space->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = triple(space->chosen, space->total),
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
                .check = type->predefined ? Check::Disabled
                         : type->needed   ? Check::Locked
                         : type->listed   ? Check::Checked
                                          : Check::Unchecked,
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

    // Отметка файла или пространства — всех его типов, которые она считает,
    // показанных или скрытых отбором панели: отмеченное целиком снимается,
    // иначе отмечается целиком. Скрытые настройками остаются, как были;
    // нужные замыканию — тоже: их отметку не снять.
    void toggleChecked(uint32_t index) override {
        auto const [library, space, entry] = locate(index);
        if (entry) {
            if (entry->needed || entry->predefined) {
                return;
            }
            list(*entry, !entry->listed);
        } else if (space) {
            bool const on = space->chosen != space->total;
            for (TypeEntry& each : space->types) {
                if (each.selectable()) {
                    list(each, on);
                }
            }
        } else {
            bool const on = library->chosen != library->total;
            for (Namespace& each : library->namespaces) {
                for (TypeEntry& type : each.types) {
                    if (type.selectable()) {
                        list(type, on);
                    }
                }
            }
        }
        editor_.marksChanged();
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
    // стиля; счёт отмеченных пересчитает отбор.
    void relist(std::string_view full) {
        if (TypeEntry* entry = find(full)) {
            entry->listed = editor_.data_->prof.types.contains(std::string {full});
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

    // Сведения о типе дерева: объявление, база, интерфейсы, запись профиля,
    // почему тип в выводе, цена его отметки и документация; значок — его вид.
    std::wstring typeInfo(md::TypeDef const& def, RowIcon const* icon) const;

    // Цена отметки абзацем: сколько типов придёт и уйдёт — их имена ссылками —
    // и на сколько изменятся члены и ключи; `checking` — ставится отметка или
    // снимается.
    void appendCost(std::wstring& html, Editor::Data::Cost const& cost, bool checking) const;

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
        uint32_t chosen = 0;  // из них отмечено: явно или замыканием
    };

    struct Library {
        md::database const* source = nullptr;
        std::string_view name;  // имя файла winmd
        std::vector<Namespace> namespaces;
        std::vector<Namespace*> shown;  // пространства, где показан хоть один тип
        bool expanded = false;
        uint32_t start = 0;   // номер строки самого файла среди видимых
        uint32_t total = 0;   // типов, которые можно отметить, во всех пространствах
        uint32_t chosen = 0;  // из них отмечено: явно или замыканием
    };

    // Строка дерева: файл, а под ним — пространство и тип, если строка их.
    struct Location {
        Library* library;
        Namespace* space;
        TypeEntry* type;
    };

    // Отметка родителя: все дети, часть или ни одного; отмечать нечего — без
    // выбора.
    static Check triple(uint32_t chosen, uint32_t total) {
        return total == 0 ? Check::Disabled
               : chosen == 0 ? Check::Unchecked
               : chosen == total ? Check::Checked
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

    // Тип входит в профиль со всеми членами или уходит из него вместе со
    // списками членов и стилей. Нужный замыканию остаётся, каким выбран явно:
    // его отметку не снять и не поставить. Счёт отмеченных пересчитает отбор.
    void list(TypeEntry& entry, bool on) {
        if (entry.listed == on || entry.predefined || entry.needed) {
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
        entry.listed = on;
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
        html += std::format(L"<p>{} namespaces, {} classes, structs, enums and interfaces</p>", library.namespaces.size(), types);
        return html;
    }

    static std::wstring namespaceInfo(Library const& library, Namespace const& space) {
        size_t classes = 0;
        size_t structs = 0;
        size_t enums = 0;
        size_t interfaces = 0;
        size_t args = 0;
        for (TypeEntry const& type : space.types) {
            (type.icon == &classIcon     ? classes
             : type.icon == &structIcon  ? structs
             : type.icon == &enumIcon    ? enums
                                         : interfaces) += 1;
            args += type.eventArgs ? 1 : 0;
        }
        std::wstring html = L"namespace ";
        append_bold(html, space.name);
        html += L"<br>Library ";
        append_text(html, library.name);
        html += std::format(L"<p>{} classes ({} of them event args), {} structs, {} enums, {} interfaces</p>", classes,
                            args, structs, enums, interfaces);
        return html;
    }

    Editor& editor_;
    std::vector<Library> libraries_;
    std::vector<Library*> shown_;  // файлы, где показан хоть один тип
    uint32_t size_ = 0;
    Location selected_ {};  // выбранная строка: файл, а под ним — пространство и тип
    mutable std::string detail_;  // серое строки, пока дерево её читает
};

namespace {

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

    // Тип по полному имени — ссылкой, если он есть в метаданных.
    void named(std::string_view full) {
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

private:
    void named(std::string_view ns, std::string_view name) override { named(std::format("{}.{}", ns, name)); }

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

namespace {

// Самостоятельные интерфейсы, которые тип реализует или требует, — строкой
// «Implements», как в документации Microsoft. Служебные интерфейсы класса
// (ExclusiveTo) — его собственные члены, их строка не называет.
void append_implements(std::wstring& html, TypesModel const& types, md::TypeDef const& type) {
    bool first = true;
    for (auto&& impl : type.InterfaceImpl()) {
        auto const ref = impl.Interface();
        if (ref.type() != md::TypeDefOrRef::TypeSpec) {
            if (md::TypeDef const resolved = md::find(ref); resolved && is_exclusive_interface(resolved)) {
                continue;
            }
        }
        html += first ? L"<br>Implements " : L", ";
        first = false;
        SignatureWriter {html, types, type}.type(ref);
    }
}

// Тип ссылкой на себя, подписанной коротким именем: в причинах и цене имён
// много, и пространства имён в них — шум; полное — в самой ссылке.
void append_type_link(std::wstring& html, TypesModel const& types, md::TypeDef const& type) {
    std::string const full = full_name(type);
    std::string_view name = type.TypeName();
    name = name.substr(0, name.find('`'));
    if (types.knows(full)) {
        html += L"<a href=\"";
        append_text(html, full);
        html += L"\">";
        append_text(html, name);
        html += L"</a>";
    } else {
        append_text(html, name);
    }
}

// Ребро обхода словами: чем тип `from` тянет следующий — «base of Button»,
// «in property ButtonBase.Command».
void append_reason(std::wstring& html, TypesModel const& types, md::TypeDef const& from,
                   dependency_reason const& why) {
    using kind = dependency_reason::kind_t;
    switch (why.kind) {
        case kind::Base: html += L"base of "; break;
        case kind::Interface: html += L"implemented by "; break;
        case kind::Statics: html += L"statics of "; break;
        case kind::Constructors: html += L"constructors of "; break;
        case kind::Property: html += L"in property "; break;
        case kind::Method: html += L"in method "; break;
        case kind::Event: html += L"in event "; break;
        case kind::Field: html += L"in field "; break;
        case kind::Delegate: html += L"in delegate "; break;
        case kind::Added: html += L"in added member "; break;
        case kind::InterfaceArgument: html += L"in an interface of "; break;
    }
    append_type_link(html, types, from);
    if (!why.member.empty()) {
        html += L'.';
        append_text(html, why.member);
    }
}

}  // namespace

std::wstring TypesModel::typeInfo(md::TypeDef const& def, RowIcon const* icon) const {
    std::wstring html;
    if (icon == &classIcon) {
        html += def.Flags().Sealed() ? L"sealed class " : L"class ";
    } else if (icon == &interfaceIcon) {
        html += L"interface ";
    } else {
        html += icon == &structIcon ? L"struct " : L"enum ";
    }
    append_bold(html, def.TypeName());

    // Классы WinRT наследуют System.Object; базу называем, только если она
    // своя.
    if (icon == &classIcon) {
        if (auto const base = def.Extends(); base && base.type() != md::TypeDefOrRef::TypeSpec) {
            auto const [ns, name] = md::get_type_namespace_and_name(base);
            if (ns != "System" || name != "Object") {
                html += L" : ";
                SignatureWriter {html, *this, def}.type(base);
            }
        }
    }

    html += L"<br>Member of ";
    append_bold(html, def.TypeNamespace());
    append_implements(html, *this, def);

    Editor::Data& data = *editor_.data_;
    std::string const full = full_name(def);
    auto const found = data.prof.types.find(full);
    bool const listed = found != data.prof.types.end();
    append_filter(html, listed ? &found->second : nullptr);

    // Почему тип в выводе: корень ли он, кто его тянет — каждое ребро с
    // подписью — и кратчайший путь до корня. Нужный замыканию отмечен, что бы
    // ни говорил файл; что говорит файл, вернётся в отметку, когда тип тянуть
    // перестанут.
    closure const& walked = data.walk();
    if (walked.surface.contains(def)) {
        html += L"<p><b>Why generated</b>";
        if (auto const root = data.roots.find(def); root != data.roots.end()) {
            html += root->second == Editor::Data::Root::Own        ? L"<br>Checked in this file."
                    : root->second == Editor::Data::Root::Extended ? L"<br>Listed by a profile this one extends."
                                                                   : L"<br>A root of the type map.";
        }
        // Служебный интерфейс класса — сам класс: член IButtonBase.Command
        // читается как ButtonBase.Command. Ребро от класса к его служебному
        // интерфейсу ничего не объясняет.
        auto const owner = [&](md::TypeDef const& type) {
            if (is_exclusive_interface(type)) {
                for (std::string_view const name : types_named_by(find_attribute(type, "ExclusiveToAttribute"))) {
                    if (md::TypeDef const found = find_type(data.db, name)) {
                        return found;
                    }
                }
            }
            return type;
        };
        if (auto const by = walked.reached_by.find(def); by != walked.reached_by.end()) {
            std::map<md::TypeDef, std::set<dependency_reason>, by_full_name> lines;
            for (auto&& [from, reasons] : by->second) {
                if (md::TypeDef const shown = owner(from); shown != def) {
                    lines[shown].insert(reasons.begin(), reasons.end());
                }
            }
            constexpr size_t shown = 12;
            size_t count = 0;
            for (auto&& [from, reasons] : lines) {
                for (dependency_reason const& why : reasons) {
                    if (count++ < shown) {
                        html += L"<br>• ";
                        append_reason(html, *this, from, why);
                    }
                }
            }
            if (count > shown) {
                html += std::format(L"<br>and {} more", count - shown);
            }

            // Путь до корня — обходом назад по рёбрам, до первого корня, кроме
            // самого типа: у каждого шага его ребро.
            std::map<md::TypeDef, md::TypeDef> toward;  // тип -> следующий к `def`
            std::deque<md::TypeDef> queue {def};
            md::TypeDef root;
            while (!queue.empty() && !root) {
                md::TypeDef const at = queue.front();
                queue.pop_front();
                auto const edges = walked.reached_by.find(at);
                if (edges == walked.reached_by.end()) {
                    continue;
                }
                for (auto&& [from, reasons] : edges->second) {
                    if (from == def || !toward.try_emplace(from, at).second) {
                        continue;
                    }
                    if (data.roots.contains(from)) {
                        root = from;
                        break;
                    }
                    queue.push_back(from);
                }
            }
            // Служебный интерфейс на пути не называется: шаг за ним подписан
            // членом, и читается он как член класса.
            if (root) {
                html += L"<br>Path: ";
                append_type_link(html, *this, root);
                for (md::TypeDef step = root; step != def;) {
                    md::TypeDef const next = toward.at(step);
                    if (next != def && is_exclusive_interface(next)) {
                        step = next;
                        continue;
                    }
                    dependency_reason const& why = *walked.reached_by.at(next).at(step).begin();
                    html += L" → ";
                    append_type_link(html, *this, next);
                    static constexpr std::wstring_view kinds[] {
                        L"base",     L"interface", L"statics", L"constructors", L"property",          L"method",
                        L"event",    L"field",     L"delegate", L"added",       L"interface argument"};
                    html += std::format(L" ({}", kinds[static_cast<size_t>(why.kind)]);
                    if (!why.member.empty()) {
                        html += L' ';
                        append_text(html, why.member);
                    }
                    html += L')';
                    step = next;
                }
            }
        }
        if (data.needs(def)) {
            html += listed ? L"<br>The check stays while anything above needs the type; this file lists it, and "
                             L"keeps it when nothing does."
                           : L"<br>The check stays while anything above needs the type.";
        }
        html += L"</p>";
    }

    // Цена — у типа, отметку которого можно поставить или снять.
    if (!data.needs(def) && !is_predefined(def, data.typeMap) && find(full)) {
        profile changed = data.prof;
        if (listed) {
            changed.types.erase(full);
            changed.styles.erase(full);
        } else {
            changed.types.emplace(full, member_filter::all());
        }
        appendCost(html, data.costOf(std::format("type {} {}", full, !listed), changed), !listed);
    }

    data.documentation.append(html, def, documentation_id(def));
    return html;
}

void TypesModel::appendCost(std::wstring& html, Editor::Data::Cost const& cost, bool checking) const {
    html += checking ? L"<p><b>Checking</b> " : L"<p><b>Unchecking</b> ";
    if (cost.added.empty() && cost.dropped.empty() && cost.members == 0 && cost.keys == 0) {
        html += L"changes nothing else in the output.</p>";
        return;
    }
    constexpr size_t named = 24;
    auto const types = [&](std::vector<md::TypeDef> const& list, wchar_t sign) {
        html += std::format(L"{}{} type{}: ", sign, list.size(), list.size() == 1 ? L"" : L"s");
        for (size_t at = 0; at < list.size() && at < named; ++at) {
            html += at == 0 ? L"" : L", ";
            append_type_link(html, *this, list[at]);
        }
        if (list.size() > named) {
            html += std::format(L" and {} more", list.size() - named);
        }
        html += L"; ";
    };
    if (!cost.added.empty()) {
        types(cost.added, L'+');
    }
    if (!cost.dropped.empty()) {
        types(cost.dropped, L'−');
    }
    html += std::format(L"{:+} members, {:+} keys.</p>", cost.members, cost.keys);
}

// Правое дерево: члены выбранного типа, а за ними — члены его баз, класс за
// классом. У каждого типа — заголовок раздела: имя, вид и пространство имён
// (у базы оно бывает другим); раздел не сворачивается. Отметка члена базы
// пишется при базе: генератор выпускает член там, где его объявляют метаданные.
class MembersModel final : public TreeModel {
public:
    MembersModel(Editor& editor, TypeEntry const& entry) : editor_(editor) {
        addPart(entry.def, entry.predefined);
        // Базы — до System.Object; на предопределённой цепочка обрывается: её
        // члены пишет wxl, и генератор не относит к ней членов, названных у
        // наследников.
        for (md::TypeDef base = base_of(entry.def); base && !is_predefined(base, editor_.data_->typeMap);
             base = base_of(base)) {
            addPart(base, false);
        }
        refilter();
    }

    // Показываемые члены — заново, по настройке редактора и панели
    // инструментов: скрытые остаются в группе и показываются снова, как только
    // их перестают прятать. Выбранный член фильтр панели не скрывает.
    void refilter() {
        bool const hide = editor_.data_->hideDependencyProperties;
        bool const onlyChosen = editor_.onlyChosen.get();
        closure const* const walked = onlyChosen ? &editor_.data_->walk() : nullptr;
        // Член в замыкании — у какого-нибудь из типов, что его объявляют.
        auto const generated = [&](Part const& part, Group const& group, Member const& member) {
            return std::ranges::any_of(declarationsOf(part, group, member), [&](member_declaration const& declaration) {
                auto const found = walked->members.find(declaration.source);
                return found != walked->members.end() && found->second.contains(std::string {member.name});
            });
        };
        // Только выбранные — отмеченные и те, что выпустит замыкание.
        auto const passes = [&](Part const& part, Group const& group, Member const& member) {
            return !onlyChosen || part.predefined || &member == selected_.member || group.locked ||
                   allows(part, member.name) || generated(part, group, member);
        };
        for (Part& part : parts_) {
            for (Group& group : part.groups) {
                group.offered.clear();
                group.shown.clear();
                for (Member const& member : group.all) {
                    if (hide && member.dependency) {
                        continue;
                    }
                    group.offered.push_back(&member);
                    if (passes(part, group, member)) {
                        group.shown.push_back(&member);
                    }
                }
            }
        }
        // Выбранный член, который скрылся, больше не выбран.
        if (selected_.member && !std::ranges::contains(selected_.group->shown, selected_.member)) {
            selected_.member = nullptr;
        }
    }

    uint32_t size() const override {
        uint32_t size = 0;
        for (Part const& part : parts_) {
            size += rows(part);
        }
        return size;
    }

    TreeRow row(uint32_t index) const override {
        auto const [part, group, at] = locate(index);
        if (!group) {
            bool const selected = part == selected_.part && !selected_.group;
            if (at == 0) {
                count_ = std::format("{} of {}", chosen(*part), offered(*part));
                return {.text = part->def.TypeName(),
                        .detail = count_,
                        .badge = part->kind,
                        .style = RowStyle::Heading,
                        .icon = part->icon,
                        .check = partCheck(*part),
                        .selected = selected};
            }
            return {.text = part->def.TypeNamespace(), .style = RowStyle::Subheading, .selected = selected};
        }

        // Группы — под заголовком типа, члены — под группами.
        if (at == npos) {
            count_ = std::format("{} of {}", chosen(*part, *group), group->offered.size());
            return {.text = group->title,
                    .detail = count_,
                    .depth = 1,
                    .icon = group->icon,
                    .expander = group->expanded ? Expander::Expanded : Expander::Collapsed,
                    .check = groupCheck(*part, *group),
                    .selected = group == selected_.group && !selected_.member};
        }

        Member const& member = *group->shown[at];
        return {.text = member.name,
                .detail = member.brief,
                .depth = 2,
                .icon = member.icon,
                .check = memberCheck(*part, *group, member),
                .selected = &member == selected_.member};
    }

    // Сворачиваются группы; раздел типа развёрнут всегда.
    void toggleExpanded(uint32_t index) override {
        if (auto const [part, group, at] = locate(index); group && at == npos) {
            group->expanded = !group->expanded;
        }
    }

    // Отметка типа или группы — всех членов, которые она считает, показанных
    // или скрытых отбором панели; скрытые настройкой остаются, как были. У
    // предопределённого типа отмечать нечего: его члены пишет wxl. Строка
    // пространства имён отметки не несёт.
    void toggleChecked(uint32_t index) override {
        auto const [part, group, at] = locate(index);
        if (part->predefined || (group && group->locked) || (!group && at != 0)) {
            return;
        }
        profile& prof = editor_.data_->prof;
        bool const styled = editor_.data_->styled.contains(part->name);
        auto const mark = [&](std::string_view member, bool on) {
            if (allows(*part, member) != on) {
                mark_member(prof, part->name, member, on, part->enumeration, styled);
            }
        };
        // Отмеченное целиком снимается, иначе отмечается целиком.
        if (!group) {
            bool const on = partCheck(*part) != Check::Checked;
            for (Group const& each : part->groups) {
                if (!each.locked) {
                    for (Member const* member : each.offered) {
                        mark(member->name, on);
                    }
                }
            }
        } else if (at == npos) {
            bool const on = groupCheck(*part, *group) != Check::Checked;
            for (Member const* member : group->offered) {
                mark(member->name, on);
            }
        } else {
            std::string_view const name = group->shown[at]->name;
            mark(name, !allows(*part, name));
        }
        editor_.types_->relist(part->name);
        editor_.marksChanged();
    }

    void invoke(uint32_t index) override {
        auto const [part, group, at] = locate(index);
        Member const* const member = group && at != npos ? group->shown[at] : nullptr;
        selected_ = {part, group, member};
        void const* const row = member ? static_cast<void const*>(member)
                                : group ? static_cast<void const*>(group)
                                        : static_cast<void const*>(part);
        editor_.selection.set({intrusive_ptr<TreeModel> {this}, row});
    }

    wxl::core::nullable<uint32_t> selectedRow() const override {
        auto const [picked, group, member] = selected_;
        uint32_t index = 0;
        for (Part const& part : parts_) {
            if (&part != picked) {
                index += rows(part);
                continue;
            }
            if (!group) {
                return index;
            }
            index += headingRows;
            for (Group const& each : part.groups) {
                if (&each == group) {
                    if (each.shown.empty()) {
                        return {};
                    }
                    if (!member) {
                        return index;
                    }
                    if (!each.expanded) {
                        return {};
                    }
                    auto const at = std::ranges::find(each.shown, member);
                    return index + 1 + static_cast<uint32_t>(at - each.shown.begin());
                }
                index += each.rows();
            }
            return {};
        }
        return {};
    }

    // Двойной щелчок по члену — его отметка; тип и группу он не трогает:
    // первый щелчок группу уже свернул или раскрыл.
    void activate(uint32_t index) override {
        if (auto const [part, group, at] = locate(index); group && at != npos) {
            toggleChecked(index);
        }
    }

    Description describe() const override {
        auto const [part, group, member] = selected_;
        if (!part) {
            return {};
        }
        if (!group) {
            return {part->icon, editor_.types_->typeInfo(part->def, part->icon)};
        }
        if (!member) {
            std::wstring html;
            if (group->source) {
                html += L"interface ";
                SignatureWriter {html, *editor_.types_, group->source}.type(
                    group->source.coded_index<md::TypeDefOrRef>());
                html += L"<br>Implemented by ";
            } else {
                append_bold(html, group->title);
                html += L"<br>Members of ";
            }
            SignatureWriter {html, *editor_.types_, part->def}.type(part->def.coded_index<md::TypeDefOrRef>());
            if (group->locked) {
                html += L"<p>Profile: always generated: a closable type is a wxl::Closable";
            } else {
                html += std::format(L"<p>Profile: {} of {} generated", chosen(*part, *group), group->offered.size());
            }
            if (size_t const hidden = editor_.data_->hideDependencyProperties
                                          ? static_cast<size_t>(std::ranges::count_if(group->all, &Member::dependency))
                                          : 0) {
                html += std::format(L"<br>{} dependency properties hidden", hidden);
            }
            html += L"</p>";
            return {group->icon, std::move(html)};
        }

        // Метод — каждой перегрузкой, строкой на каждую.
        std::string_view const name = member->name;
        std::vector<std::pair<std::wstring, std::wstring>> lines;  // сигнатура, её документация
        for (member_declaration const& declaration : declarationsOf(*part, *group, *member)) {
            std::wstring line;
            SignatureWriter {line, *editor_.types_, declaration.source}.write(declaration, name);
            if (std::ranges::find(lines, line, &decltype(lines)::value_type::first) == lines.end()) {
                std::wstring documentation;
                editor_.data_->documentation.append(documentation, declaration.source,
                                                    documentation_id(declaration.source, declaration, name));
                lines.emplace_back(std::move(line), std::move(documentation));
            }
        }
        std::wstring html;
        for (auto const& [line, documentation] : lines) {
            html += line;
            html += L"<br>";
        }
        html += L"Member of ";
        SignatureWriter {html, *editor_.types_, part->def}.type(part->def.coded_index<md::TypeDefOrRef>());
        if (group->source) {
            html += L", through ";
            SignatureWriter {html, *editor_.types_, group->source}.type(group->source.coded_index<md::TypeDefOrRef>());
        }
        html += group->locked         ? L"<p>Profile: always generated</p>"
                : allows(*part, name) ? L"<p>Profile: generated</p>"
                                      : L"<p>Profile: not generated</p>";

        // Цена отметки члена: что придёт в вывод с ним или уйдёт без него.
        if (!part->predefined && !group->locked) {
            Editor::Data& data = *editor_.data_;
            bool const on = !allows(*part, name);
            profile changed = data.prof;
            mark_member(changed, part->name, name, on, part->enumeration, data.styled.contains(part->name));
            editor_.types_->appendCost(
                html, data.costOf(std::format("member {}.{} {}", part->name, name, on), changed), on);
        }

        // У перегрузок документация у каждой своя — под её сигнатурой.
        for (auto const& [line, documentation] : lines) {
            if (!documentation.empty()) {
                if (lines.size() > 1) {
                    html += L"<p>" + line + L"</p>";
                }
                html += documentation;
            }
        }
        return {member->icon, std::move(html)};
    }

private:
    static constexpr uint32_t npos = UINT32_MAX;

    // Член, каким он прочитан один раз при открытии типа.
    struct Member {
        std::string_view name;
        member_kind kind;
        RowIcon const* icon = nullptr;
        std::string brief;  // объявление серым после имени; у перегрузок — первое и их число
        // Идентификатор свойства зависимостей: статическое свойство только
        // для чтения с именем на Property.
        bool dependency = false;
    };

    struct Group {
        std::string_view title;
        RowIcon const* icon = nullptr;
        // Интерфейс, чьи члены в группе; пусто — свои члены типа.
        md::TypeDef source;
        // IClosable: его close() есть у всякого, кто его поддерживает, —
        // отмечать нечего.
        bool locked = false;
        std::vector<Member> all;
        std::vector<Member const*> offered;  // из all — не скрытые настройкой; их считают и отмечают
        std::vector<Member const*> shown;    // из offered — прошедшие отбор панели
        bool expanded = true;

        // Группа, где не показано ничего, не показывается и сама.
        uint32_t rows() const {
            return shown.empty() ? 0 : 1 + (expanded ? static_cast<uint32_t>(shown.size()) : 0);
        }
    };

    // Раздел типа — выбранного или его базы — со своими группами.
    struct Part {
        md::TypeDef def;
        std::string name;
        std::string_view kind;  // вид в заголовке: class, interface, enum, struct
        RowIcon const* icon = nullptr;
        bool enumeration = false;
        // Предопределён вне проекции: его члены пишет wxl, отмечать нечего.
        bool predefined = false;
        std::vector<Group> groups;  // адреса членов постоянны: группы не растут после разбора
    };

    // Заголовок раздела — две строки: имя и пространство имён.
    static constexpr uint32_t headingRows = 2;

    // Строка дерева: раздел, а в нём — группа и номер члена среди её
    // показанных; группы нет — строка заголовка, номер — какая из двух; номер
    // npos — строка группы.
    struct Location {
        Part* part;
        Group* group;
        uint32_t at;
    };

    struct Selected {
        Part const* part = nullptr;
        Group const* group = nullptr;    // пусто — выбран тип
        Member const* member = nullptr;  // пусто — выбрана группа
    };

    // Свои члены — тремя группами; под ними по группе на каждый
    // самостоятельный интерфейс, который тип реализует или требует.
    void addPart(md::TypeDef const& def, bool predefined) {
        auto const category = md::get_category(def);
        auto const [kind, icon] = category == md::category::interface_type ? std::pair {"interface", &interfaceIcon}
                                  : category == md::category::enum_type    ? std::pair {"enum", &enumIcon}
                                  : category == md::category::struct_type  ? std::pair {"struct", &structIcon}
                                                                           : std::pair {"class", &classIcon};
        Part& part = parts_.emplace_back(Part {.def = def,
                                               .name = full_name(def),
                                               .kind = kind,
                                               .icon = icon,
                                               .enumeration = category == md::category::enum_type,
                                               .predefined = predefined});
        auto const sources = member_sources_of(def);
        if (part.enumeration) {
            addGroup(part, "Values", member_kind::Constant, &constantIcon, sources.own.constants);
        } else {
            addGroup(part, "Properties", member_kind::Property, &propertyIcon, sources.own.properties);
            addGroup(part, "Methods", member_kind::Method, &methodIcon, sources.own.methods);
            addGroup(part, "Events", member_kind::Event, &eventIcon, sources.own.events);
        }
        for (auto const& [source, members] : sources.implemented) {
            addInterfaceGroup(part, source, members);
        }
        // Интерфейс, выбранный безусловно (IClosable), — последним: выбирать
        // в нём нечего.
        std::ranges::stable_partition(part.groups, [](Group const& group) { return !group.locked; });
    }

    void addGroup(Part& part, std::string_view title, member_kind kind, RowIcon const* icon,
                  std::vector<std::string_view> const& names) {
        if (names.empty()) {
            return;
        }
        Group& group = part.groups.emplace_back(Group {title, icon});
        group.all.reserve(names.size());
        for (std::string_view const name : names) {
            addMember(part, group, name, kind, icon);
        }
    }

    // Группа интерфейса: члены всех видов, каждый со своим значком.
    void addInterfaceGroup(Part& part, md::TypeDef const& source, declared_members const& members) {
        if (members.properties.empty() && members.methods.empty() && members.events.empty()) {
            return;
        }
        Group& group = part.groups.emplace_back(Group {source.TypeName(), &interfaceIcon, source});
        group.locked = full_name(source) == closableInterface && closable_fits(part.def);
        for (auto const& [names, kind, icon] : {std::tuple {&members.properties, member_kind::Property, &propertyIcon},
                                                std::tuple {&members.methods, member_kind::Method, &methodIcon},
                                                std::tuple {&members.events, member_kind::Event, &eventIcon}}) {
            for (std::string_view const name : *names) {
                addMember(part, group, name, kind, icon);
            }
        }
    }

    // Объявления члена: свои — как их показывать у типа, члены интерфейса —
    // в самом интерфейсе.
    static std::vector<member_declaration> declarationsOf(Part const& part, Group const& group, Member const& member) {
        return group.source ? declarations_of(group.source, member.kind, member.name)
                            : shown_declarations(part.def, member.kind, member.name);
    }

    static void addMember(Part const& part, Group& group, std::string_view name, member_kind kind,
                          RowIcon const* icon) {
        Member& member = group.all.emplace_back(Member {name, kind, icon});
        std::vector<std::string> briefs;
        for (member_declaration const& declaration : declarationsOf(part, group, member)) {
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
    bool allows(Part const& part, std::string_view member) const {
        auto const& types = editor_.data_->prof.types;
        auto const found = types.find(part.name);
        return found == types.end() ? part.enumeration : found->second.allows(member);
    }

    size_t chosen(Part const& part, Group const& group) const {
        if (group.locked) {
            return group.offered.size();
        }
        return static_cast<size_t>(std::ranges::count_if(
            group.offered, [&](Member const* member) { return allows(part, member->name); }));
    }

    size_t chosen(Part const& part) const {
        size_t count = 0;
        for (Group const& group : part.groups) {
            count += chosen(part, group);
        }
        return count;
    }

    static size_t offered(Part const& part) {
        size_t count = 0;
        for (Group const& group : part.groups) {
            count += group.offered.size();
        }
        return count;
    }

    // Строки раздела: заголовок есть всегда, даже когда не показано ни одного
    // члена, — за ним строки групп.
    static uint32_t rows(Part const& part) {
        uint32_t rows = headingRows;
        for (Group const& group : part.groups) {
            rows += group.rows();
        }
        return rows;
    }

    // Предопределённому типу отмечать нечего; член IClosable есть всегда.
    Check memberCheck(Part const& part, Group const& group, Member const& member) const {
        if (part.predefined) {
            return Check::Disabled;
        }
        if (group.locked) {
            return Check::Locked;
        }
        return allows(part, member.name) ? Check::Checked : Check::Unchecked;
    }

    static Check triple(size_t chosen, size_t total) {
        return chosen == 0 ? Check::Unchecked : chosen == total ? Check::Checked : Check::Indeterminate;
    }

    Check groupCheck(Part const& part, Group const& group) const {
        if (part.predefined) {
            return Check::Disabled;
        }
        if (group.locked) {
            return Check::Locked;
        }
        return triple(chosen(part, group), group.offered.size());
    }

    // Отметка типа — по показанным членам групп, которые можно отмечать; не
    // показано таких — отмечать нечего, или член IClosable есть всегда.
    Check partCheck(Part const& part) const {
        if (part.predefined) {
            return Check::Disabled;
        }
        size_t count = 0;
        size_t total = 0;
        bool locked = false;
        for (Group const& group : part.groups) {
            if (group.locked) {
                locked = locked || !group.offered.empty();
            } else {
                count += chosen(part, group);
                total += group.offered.size();
            }
        }
        if (total == 0) {
            return locked ? Check::Locked : Check::Disabled;
        }
        return triple(count, total);
    }

    Location locate(uint32_t index) const {
        for (Part const& part : parts_) {
            uint32_t const partRows = rows(part);
            if (index >= partRows) {
                index -= partRows;
                continue;
            }
            Part* const at = const_cast<Part*>(&part);
            if (index < headingRows) {
                return {at, nullptr, index};
            }
            index -= headingRows;
            for (Group const& group : part.groups) {
                uint32_t const groupRows = group.rows();
                if (index < groupRows) {
                    return {at, const_cast<Group*>(&group), index == 0 ? npos : index - 1};
                }
                index -= groupRows;
            }
        }
        return {const_cast<Part*>(&parts_.back()), nullptr, 0};
    }

    Editor& editor_;
    std::vector<Part> parts_;  // выбранный тип, за ним — его базы
    Selected selected_;
    mutable std::string count_;  // «N of M» строки типа или группы, пока дерево её читает
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
        append_implements(html, types, def_);
        // Почему типа нет в дереве.
        if (types.lists(full_name(def_))) {
            html += L"<p>Hidden in the Types tree by the settings.</p>";
        } else if (is_exclusive_interface(def_)) {
            html += L"<p>Not in the Types tree: exclusive to ";
            for (std::string_view const owner : types_named_by(find_attribute(def_, "ExclusiveToAttribute"))) {
                SignatureWriter {html, types, def_}.named(owner);
            }
            html += L", whose members it declares.</p>";
        } else if (def_.TypeName().contains('`')) {
            html += L"<p>Not in the Types tree: a generic interface, projected through the type map.</p>";
        } else {
            html += L"<p>Not in the Types tree: delegates and attributes are not listed.</p>";
        }

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

        editor_.data_->documentation.append(html, def_, documentation_id(def_));
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
        editor_.marksChanged();
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
        return {entry->icon, typeInfo(entry->def, entry->icon)};
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

void Editor::refilterTrees() {
    types_->refilter();
    if (auto const& shown = members.get()) {
        static_cast<MembersModel&>(*shown).refilter();
    }
}

void Editor::marksChanged() {
    data_->walkStale = true;
    refilterTrees();
    revision.set(revision.get() + 1);
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
    // Имя из ссылки страницы: сломанная пара — U+FFFD, как у append_text.
    auto const utf8 = wxl::core::unicode::repaired(type).to_utf8();
    if (auto const row = types_->reveal(utf8.chars())) {
        return row;
    }
    if (md::TypeDef const def = find_type(data_->db, utf8.chars())) {
        intrusive_ptr<TreeModel> const card {new TypeCard {*this, def}, /*add_ref=*/false};
        selection.set({card, card.get()});
    }
    return {};
}
Editor::~Editor() = default;

std::wstring errorText(std::exception const& error) {
    std::wstring wide;
    wxl::core::unicode::append_utf16(wide, wxl::core::environment::error_message(error));
    return wide;
}

intrusive_ptr<Editor> Editor::open(std::filesystem::path const& prof, std::filesystem::path const& packages) {
    intrusive_ptr<Editor> editor {new Editor {}, /*add_ref=*/false};

    // Пакеты и Windows SDK — той сборки, из которой редактор запущен: версии
    // решило её восстановление NuGet, редактор их не выводит.
    auto const restored = load_restored_packages(packages);
    auto resolved = resolve_profiles({prof}, restored);
    std::vector<std::string> files;
    files.reserve(resolved.metadata.size());
    for (auto&& file : resolved.metadata) {
        files.push_back(std::string{wxl::core::unicode::to_utf8(file).chars()});
    }
    auto const resources = resolved.resources;

    // Отметки замыкания ложатся поверх профилей, которые этот продолжает: их
    // типы и стили — отдельно от его собственных.
    profile own = load_profile(prof);
    std::vector<std::filesystem::path> parents;
    for (auto&& reference : own.extends) {
        parents.push_back(resolve_reference(reference, own.source));
    }

    editor->data_ = std::make_unique<Data>(std::move(own), std::move(resolved), files,
                                         [raw = editor.get()] { raw->documentationPending.set(true); });
    if (!parents.empty()) {
        auto inherited = resolve_profiles(parents, restored);
        editor->data_->inheritedTypes = std::move(inherited.types);
        editor->data_->inheritedStyles = std::move(inherited.styles);
    }
    for (auto&& dictionary : resources) {
        auto declared = dictionary_resources(dictionary);
        editor->data_->resources.insert(editor->data_->resources.end(), std::move_iterator {declared.begin()},
                                        std::move_iterator {declared.end()});
    }
    // Карта типов — общая у профилей одного каталога, как её ищет генератор.
    if (auto const map = prof.parent_path() / "types.json"; std::filesystem::exists(map)) {
        editor->data_->typeMap = load_type_map(map);
    }
    // Первый обход — здесь, при открытии: тип профиля, которого нет в метаданных
    // восстановления, останавливает обход ошибкой, и окно показывает её вместо
    // редактора. Позже обход идёт из отрисовки деревьев, и ошибку оттуда окно не
    // показывает.
    editor->data_->walk();
    editor->types_ = intrusive_ptr<TypesModel> {new TypesModel {*editor}, /*add_ref=*/false};
    editor->resources_ = intrusive_ptr<ResourcesModel> {new ResourcesModel {*editor, *editor->types_}, /*add_ref=*/false};
    editor->info.follow(editor->selection, editor->revision, [](Selection const& chosen, uint32_t) {
        return chosen.model ? chosen.model->describe() : Description {};
    });
    editor->onlyChosen.on_change([raw = editor.get()](bool) noexcept {
        raw->refilterTrees();
        raw->revision.set(raw->revision.get() + 1);
    });
    auto const file = prof.filename().wstring();
    editor->title.set(std::u16string {file.begin(), file.end()} + u" — wxl.gen.ui");
    editor->path.set(std::filesystem::path {prof}.make_preferred().u16string());
    return editor;
}

}  // namespace editor
