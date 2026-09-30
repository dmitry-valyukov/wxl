// Страницы, которые показывают плитки: All controls, раздел и выдача поиска —
// AllControlsPage, SectionPage и SearchResultsPage оригинала.

#include "Pages.h"

#include <algorithm>
#include <cwctype>
#include <memory>

using namespace wxl;
using namespace wxl::dsl;

namespace {

std::vector<gallery::ControlInfo const*> sortedByTitle(std::vector<gallery::ControlInfo const*> items) {
    std::ranges::sort(items, [](gallery::ControlInfo const* a, gallery::ControlInfo const* b) {
        return a->title < b->title;
    });
    return items;
}

std::wstring lower(std::wstring_view text) {
    std::wstring result{text};
    std::ranges::transform(result, result.begin(),
                           [](wchar_t symbol) { return static_cast<wchar_t>(std::towlower(symbol)); });
    return result;
}

bool contains(std::wstring_view haystack, std::wstring_view needle) {
    return lower(haystack).find(needle) != std::wstring::npos;
}

// Слова запроса, в нижнем регистре.
std::vector<std::wstring> tokens(std::wstring_view query) {
    std::vector<std::wstring> words;
    std::wstring word;
    for (wchar_t const symbol : lower(query)) {
        if (symbol == L' ') {
            if (!word.empty()) {
                words.push_back(std::move(word));
                word.clear();
            }
        } else {
            word += symbol;
        }
    }
    if (!word.empty()) {
        words.push_back(std::move(word));
    }
    return words;
}

// Контрол подходит, когда каждое слово запроса есть в названии, подзаголовке
// или тегах: чем больше слов, тем точнее выдача.
bool matches(gallery::ControlInfo const& item, std::vector<std::wstring> const& words) {
    return std::ranges::all_of(words, [&](std::wstring const& word) {
        return contains(item.title, word) || contains(item.subtitle, word) ||
               std::ranges::any_of(item.tags, [&](std::wstring const& tag) { return contains(tag, word); });
    });
}

FrameworkElement titled(std::wstring_view title, FrameworkElement const& body, Thickness titleMargin) {
    return Grid {
        rowDefinitions = u"auto,*",
        TextBlock {
            title,
            margin = titleMargin,
            styles.TextBlock.Title,
        },
        Border {row = 1, body},
    };
}

}  // namespace

wxl::FrameworkElement gallery::allControlsPage() {
    std::vector<ControlInfo const*> items;
    for (auto const& group : catalog().groups) {
        for (auto const& item : group.items) {
            items.push_back(&item);
        }
    }
    items = sortedByTitle(std::move(items));
    return titled(L"Controls", tileGrid(items, Thickness {24, 16, 24, 36}), Thickness {36, 24, 16, 0});
}

wxl::FrameworkElement gallery::sectionPage(ControlGroup const& group) {
    std::vector<ControlInfo const*> items;
    for (auto const& item : group.items) {
        items.push_back(&item);
    }
    items = sortedByTitle(std::move(items));
    return titled(group.title, tileGrid(items, Thickness {36, 0, 36, 0}), Thickness {36, 24, 16, 24});
}

wxl::FrameworkElement gallery::searchResultsPage(std::wstring_view query) {
    auto const words = tokens(query);

    // Отбор по разделам; первым идёт «All» со всем найденным.
    struct Filter {
        std::wstring name;
        std::vector<ControlInfo const*> items;
    };
    std::vector<Filter> filters;
    std::vector<ControlInfo const*> everything;
    for (auto const& group : catalog().groups) {
        Filter filter{group.title, {}};
        for (auto const& item : group.items) {
            if (matches(item, words)) {
                filter.items.push_back(&item);
                everything.push_back(&item);
            }
        }
        if (!filter.items.empty()) {
            filters.push_back(std::move(filter));
        }
    }

    if (everything.empty()) {
        return TextBlock {
            u"No results match your search.",
            Margin {24, 24, 0, 0},
            styles.TextBlock.Title,
        };
    }
    filters.insert(filters.begin(), Filter{L"All", everything});

    // Нижний уровень — то, что оригинал держит в resultsNavView: верхняя
    // панель отборов и под ней плитки выбранного.
    auto host = Border {};
    auto results = std::make_shared<std::vector<Filter>>(std::move(filters));
    auto show = [host, results](std::size_t index) {
        host.child(tileGrid((*results)[index].items, Thickness {36, 24, 36, 36}));
    };

    auto navigation = NavigationView {
        isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
        isSettingsVisible = false,
        paneDisplayMode = NavigationViewPaneDisplayMode::Top,
        content = host,
        onSelectionChanged = [show, results](Object const& sender, NavigationViewSelectionChangedEventArgs&) {
            auto const selected = sender.try_as<NavigationView>().selectedItem().try_as<FrameworkElement>();
            if (!selected) {
                return;
            }
            auto const name = gallery::wide(selected.name());
            for (std::size_t i = 0; i < results->size(); ++i) {
                if ((*results)[i].name == name.substr(0, name.rfind(L" ("))) {
                    show(i);
                    return;
                }
            }
        },
    };
    for (auto const& filter : *results) {
        auto const label = filter.name + L" (" + std::to_wstring(filter.items.size()) + L")";
        auto item = NavigationViewItem {content = label, name = label};
        navigation.menuItems().append(item);
        if (&filter == &results->front()) {
            navigation.selectedItem(item);
        }
    }    show(0);
    return navigation;
}
