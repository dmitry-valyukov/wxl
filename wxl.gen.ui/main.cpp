// Визуальный редактор профилей генератора проекции.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include <string_view>

#include "Bind.h"
#include "Card.h"
#include "CompositionWindow.h"
#include "Editor.h"
#include "FocusEffect.h"
#include "HtmlBlock.h"
#include "UiThread.h"
#include "MagnifyEffect.h"
#include "Settings.h"
#include "SplitPanel.h"
#include "ThemeBrush.h"
#include "ZoomEffect.h"
#include "VirtualTree.h"
#include <wxl/brushes.h>
#include "launch.h"
#include "ui.h"

using namespace wxl;
using namespace wxl::dsl;
using namespace editor;

namespace {

// Профиль, который открывается по умолчанию, — из дерева исходников, где
// лежат профили генератора.
std::filesystem::path defaultProfile() {
    return std::filesystem::path {u8"" WXL_GEN_PROFILES} / u8"base.json";
}

// Знаки кнопок панели — размер 20 шрифта Fluent UI System Icons (Assets):
// контурные из Regular, open и save — заливочные из Filled.
namespace glyphs {
    constexpr char16_t newProfile[] {0xE4DB, 0};         // document_add
    constexpr char16_t open[] {0xF432, 0};               // folder_open, Filled
    constexpr char16_t save[] {0xF689, 0};               // save, Filled
    constexpr char16_t undo[] {0xF199, 0};               // arrow_undo
    constexpr char16_t redo[] {0xF16E, 0};               // arrow_redo
    constexpr char16_t zoomOut[] {0xF8C6, 0};            // zoom_out
    constexpr char16_t zoomIn[] {0xF8C4, 0};             // zoom_in
    constexpr char16_t types[] {0xE762, 0};              // library
    constexpr char16_t resources[] {0xF356, 0};          // design_ideas
    constexpr char16_t settings[] {0xF6A9, 0};           // settings
}

// Пункты панели навигации — виды левого дерева; текст пункта — и его имя,
// по которому узнают выбранный.
constexpr zstring_view typesTab = u"Types";
constexpr zstring_view resourcesTab = u"Resources";
constexpr zstring_view settingsName = u"Settings";

// Знаки пунктов панели навигации — из того же шрифта, что знаки панели.
constexpr zstring_view navigationFont = u"ms-appx:///Assets/FluentSystemIcons-Regular.ttf#FluentSystemIcons-Regular";

core::u16_text percent(double factor) {
    core::u16_text text = core::to_u16(factor * 100, std::chars_format::fixed, 0);
    text += u" %";
    return text;
}

// Подпись кнопки «100 %» — масштаб модели словами.
struct ZoomLabel {
    core::observable<core::u16_text> text;
};

// Где был пользователь — для стрелки «назад» в заголовке, как в Gallery: пункт
// панели навигации и выбранный тип. Переход назад сам мест не записывает.
struct History {
    struct Place {
        std::u16string page;
        std::u16string type;

        bool operator==(Place const&) const = default;
    };

    std::vector<Place> back;
    Place current;
    bool replaying = false;
    core::observable<bool> canGoBack {false};

    void visit(Place place) {
        if (replaying || place == current) {
            return;
        }
        if (!current.page.empty()) {
            back.push_back(std::move(current));
        }
        current = std::move(place);
        canGoBack.set(!back.empty());
    }
};

// Документация дочитывается в потоке окна: шаг в несколько миллисекунд, и
// окно отрабатывает ввод и отрисовку до следующего.
async::task<> prepareDocumentation(core::intrusive_ptr<Editor> document) {
    do {
        co_await UiThread::onIdle();
    } while (document->prepareDocumentation());
}

// Дочитывание документации — задача окна: живёт до Teardown, а на выходе её
// ожидание onIdle кончается отменой, и тело уходит раньше.
struct DocumentationReading {
    async::task<> reading;
};

}  // namespace

wxl::Teardown wxl_launched() {
    core::intrusive_ptr<Editor> document;
    try {
        document = Editor::open(defaultProfile());
    } catch (std::exception const& error) {
        auto window = Window {
            title = u"wxl.gen.ui",
            TextBlock {text = errorText(error), textWrapping.wrap, Margin {24}},
        };
        window.activate();
        return {};
    }

    auto const types = VirtualTree::create();
    auto const members = VirtualTree::create();
    VirtualTree* const left = types.get();
    VirtualTree* const right = members.get();

    left->model(document->types());
    document->members.on_change([right](core::intrusive_ptr<TreeModel> const& model) noexcept {
        right->model(model);
    });
    // Пункты панели навигации слева: два вида левого дерева, а под ними
    // свой пункт настроек — внизу, на месте встроенного: тот подписан на языке
    // системы, а не приложения. Выбранный пункт узнают по имени.
    auto const typesItem = NavigationViewItem {
        content = typesTab,
        icon = FontIcon {fontFamily = FontFamily {navigationFont}, glyph = glyphs::types},
    };
    typesItem.name(typesTab);
    auto const resourcesItem = NavigationViewItem {
        content = resourcesTab,
        icon = FontIcon {fontFamily = FontFamily {navigationFont}, glyph = glyphs::resources},
    };
    resourcesItem.name(resourcesTab);
    auto const settingsItem = NavigationViewItem {
        content = settingsName,
        icon = FontIcon {fontFamily = FontFamily {navigationFont}, glyph = glyphs::settings},
    };
    settingsItem.name(settingsName);

    auto const settings = core::make_refcounted<Settings>();
    loadSettings(*settings);

    // Сведения читают подолгу: кегль крупнее 14 у деревьев, свой в настройках.
    // Шаг строк — целый: при своём, дробном (21,3 у кегля 16), строки одного и
    // того же текста вставали то на пиксель выше, то ниже, смотря по остальному
    // содержимому.
    auto const details = HtmlBlock {
        textWrapping.wrap,
        lineStackingStrategy = LineStackingStrategy::BlockLineHeight,
    };
    // Значок строки — перед первым словом сведений, как в дереве. Разметка
    // разбирается под кегль блока, поэтому новый кегль разбирает её заново.
    // Наблюдатели настроек и сведений держат редактор и настройки простыми
    // указателями: живут они столько же, сколько окно, — до Teardown.
    auto const showInfo = [details, preferences = settings.get()](Description const& info) {
        double const size = preferences->detailsFontSize.get();
        details.fontSize(size);
        details.lineHeight(std::round(size * 22.0 / 16.0));
        if (!info.icon) {
            details.html(info.html);
            return;
        }
        details.clear();
        details.appendParagraph();
        auto const icon = iconText(*info.icon, size);
        Apply {icon, Margin {0, 0, 6, 0}};
        details.appendElement(icon);
        details.append(info.html);
    };
    document->info.on_change([showInfo](Description const& info) noexcept { showInfo(info); });

    // Настройки действуют сразу и помнятся: каждое изменение пишется в реестр.
    left->details(settings->showSignatures.get());
    right->details(settings->showSignatures.get());
    document->hideDependencyProperties(settings->hideDependencyProperties.get());
    document->hideEventArgs(settings->hideEventArgs.get());
    document->hidePredefined(settings->hidePredefined.get());
    settings->hideEventArgs.on_change([raw = settings.get(), editor = document.get()](bool hide) noexcept {
        editor->hideEventArgs(hide);
        saveSettings(*raw);
    });
    settings->hidePredefined.on_change([raw = settings.get(), editor = document.get()](bool hide) noexcept {
        editor->hidePredefined(hide);
        saveSettings(*raw);
    });
    document->englishDocumentation(settings->englishDocumentation.get());
    settings->showSignatures.on_change([left, right, raw = settings.get()](bool show) noexcept {
        left->details(show);
        right->details(show);
        saveSettings(*raw);
    });
    settings->hideDependencyProperties.on_change([raw = settings.get(), editor = document.get()](bool hide) noexcept {
        editor->hideDependencyProperties(hide);
        saveSettings(*raw);
    });
    settings->englishDocumentation.on_change([raw = settings.get(), editor = document.get()](bool english) noexcept {
        editor->englishDocumentation(english);
        saveSettings(*raw);
    });
    settings->detailsFontSize.on_change([raw = settings.get(), editor = document.get(), showInfo](int) noexcept {
        showInfo(editor->info.get());
        saveSettings(*raw);
    });
    // Ссылка в сведениях — тип: он открывается в дереве Types.
    details.onLink([left, document, typesItem](zstring_view target) {
        if (auto const row = document->reveal(target.wide())) {
            typesItem.isSelected(true);
            left->reveal(*row);
        }
    });

    document->revision.on_change([left, right](uint32_t) noexcept {
        left->refresh();
        right->refresh();
    });

    // Флаг поднимается, только когда опущен, так что дочитывает одна корутина:
    // прежняя к тому времени кончилась, и поле занимает новая.
    auto const documentation = core::make_refcounted<DocumentationReading>();
    document->documentationPending.on_change([raw = document.get(), holder = documentation.get()](bool pending) noexcept {
        if (pending) {
            holder->reading = prepareDocumentation(core::intrusive_ptr<Editor> {raw});
        }
    });

    // Панель инструментов — одна на всё окно, карточкой, как в образце HelloHere.
    auto const topPanel = Preset {
        hAlign.stretch,
        CornerRadius {2},
        Padding {16, 0},
    };

    // Рамка деревьев — той же кистью, что обводка карточки панели.
    auto const framed = Preset {
        BorderThickness {1},
        borderBrush = brushes.Card.StrokeColorDefault,
    };

    // Один эффект на все кнопки, как в образце Effects: копии — это тот же
    // эффект, и анимации у всех кнопок общие. Кнопка ловит указатель всей
    // площадью, а растёт её знак: у кнопки, которая растёт сама, XAML
    // закрывает подсказку через миг после того, как показал.
    auto const pop = MagnifyEffect {1.2, MagnifyTarget::Content, maximum = 1.35, minimum = 0.95};

    auto const toolButton = Preset {
        styles.Button.CommandBarFlyoutEllipsis,
        pop,
    };

    // Шрифт — по ms-appx:///, как велит FontFamily.h: относительный путь в знаке
    // на заголовке окна шрифт не находил, и знаки пустели во всём окне.
    auto const toolGlyph = Preset {
        fontFamily = FontFamily {u"ms-appx:///Assets/FluentSystemIcons-Regular.ttf#FluentSystemIcons-Regular"},
        fontSize = 20.0,
    };
    auto const toolFilledGlyph = Preset {
        fontFamily = FontFamily {u"ms-appx:///Assets/FluentSystemIcons-Filled.ttf#FluentSystemIcons-Filled"},
        fontSize = 20.0,
    };

    // Масштаб окна — с клавиатуры (Ctrl и «+», «−», «0»), колесом с Ctrl и кнопками
    // в заголовке, по ступеням от 50 до 300 %.
    auto const zoom = ZoomEffect {};
    auto const label = core::make_refcounted<ZoomLabel>();
    label->text.follow(zoom.model().zoomFactor(), percent);

    // Фокус с клавиатуры — подъёмом или свечением, по настройке; рамка системы —
    // для сравнения.
    auto const focus = FocusEffect {static_cast<FocusLook>(settings->focusLook.get())};
    settings->focusLook.on_change([focus, raw = settings.get()](int look) noexcept {
        focus.look(static_cast<FocusLook>(look));
        saveSettings(*raw);
    });

    // Редактор: слева дерево типов или ресурсов, справа члены выбранного и
    // под ними, меньше, сведения о выбранном — как у просмотрщиков библиотек
    // типов.
    auto const editorView = SplitPanel {
        openPaneLength = 520.0,
        spacing = 8.0,
        // Дерево — на сплошном фоне, белом в светлой теме.
        pane = Border {
            framed,
            background = brushes.SolidBackgroundFillColor.Quarternary,
            left->view(),
        },
        content = SplitPanel {
            orientation.vertical,
            panePlacement.right,
            openPaneLength = 180.0,
            spacing = 8.0,
            content = Border {
                framed,
                background = brushes.SolidBackgroundFillColor.Quarternary,
                right->view(),
            },
            pane = Border {
                framed,
                background = brushes.SolidBackgroundFillColor.Quarternary,
                ScrollViewer {
                    Padding {12, 8},
                    content = details,
                },
            },
        },
    };

    // Левая часть — по ширине дерева: самая широкая строка, рамка и поле.
    // Граница перетаскивается и дальше, до следующей подгонки.
    left->wantedWidth.on_change([editorView](double width) noexcept {
        editorView.openPaneLength(std::clamp(std::ceil(width) + 14.0, 240.0, 900.0));
    });

    // Настройки — страницей, на месте редактора, как у NavigationView принято.
    auto const settingsPage = ScrollViewer {
        content = StackPanel {
            width = 360.0,
            hAlign.left,
            Padding {24, 16},
            spacing = 12.0,
            TextBlock {u"Settings", styles.TextBlock.Subtitle},
            ToggleSwitch {
                header = u"Hide dependency properties",
                Bind {settings->hideDependencyProperties},
            },
            ToggleSwitch {
                header = u"Hide event args",
                Bind {settings->hideEventArgs},
            },
            ToggleSwitch {
                header = u"Hide pre-defined types",
                Bind {settings->hidePredefined},
            },
            ToggleSwitch {
                header = u"Show signatures in trees",
                Bind {settings->showSignatures},
            },
            ToggleSwitch {
                header = u"Documentation in English",
                Bind {settings->englishDocumentation},
            },
            NumberBox {
                header = u"Details font size",
                minimum = 12.0,
                maximum = 24.0,
                spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
                Bind {settings->detailsFontSize},
            },
            TextBlock {u"Keyboard focus"},
            ComboBox {
                hAlign.stretch,
                ComboBoxItem {u"System frame"},
                ComboBoxItem {u"Lift"},
                ComboBoxItem {u"Glow"},
                Bind {settings->focusLook},
            },
        },
    };

    // Под панелью инструментов — редактор или настройки, по пункту навигации.
    auto const host = Border {row = 1, editorView};

    // Слева — панель навигации, как принято в приложениях Windows: виды левого
    // дерева и пункт настроек внизу. Справа от неё — панель
    // инструментов, под ней выбранное, внизу строка состояния.
    // Стрелка назад и кнопка панели — в заголовке окна, как в Gallery; у самой
    // панели их нет.
    auto const history = core::make_refcounted<History>();
    auto const navigation = NavigationView {
        isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
        isPaneToggleButtonVisible = false,
        isPaneOpen = false,
        isSettingsVisible = false,
        onSelectionChanged += [left, document, host, editorView, settingsPage, places = history.get()](
                                 Object const& sender, NavigationViewSelectionChangedEventArgs&) {
            auto const item = sender.try_as<NavigationView>().selectedItem().try_as<FrameworkElement>();
            if (item && std::u16string_view {item.name()} == settingsName) {
                host.child(settingsPage);
                places->visit({std::u16string {settingsName.view()}, {}});
                return;
            }
            bool const resources = item && std::u16string_view {item.name()} == resourcesTab;
            left->model(resources ? document->resources() : document->types());
            host.child(editorView);
            places->visit({std::u16string {(resources ? resourcesTab : typesTab).view()}, document->typeName.get()});
        },
        content = Grid {
            rowDefinitions = u"40,*,auto",
            Card {
                topPanel,
                // Панель профиля пока макет: действий на кнопках нет; работает
                // только отбор показанного.
                Grid {
                    columnDefinitions = u"auto,*",
                    StackPanel {
                        orientation.horizontal,
                        spacing = 4,
                        Button {
                            toolButton,
                            toolTip = u"Новый профиль",
                            content = FontIcon {toolGlyph, glyph = glyphs::newProfile},
                        },
                        Button {
                            toolButton,
                            toolTip = u"Открыть профиль",
                            content = FontIcon {
                                toolFilledGlyph,
                                glyph = glyphs::open,
                                foreground = iconFill(rgb(109, 55, 16), rgb(255, 167, 38)),
                            },
                        },
                        Button {
                            toolButton,
                            toolTip = u"Сохранить профиль",
                            content = FontIcon {
                                toolFilledGlyph,
                                glyph = glyphs::save,
                                foreground = iconFill(rgb(10, 14, 28), rgb(41, 121, 255)),
                            },
                        },
                        CheckBox {
                            vAlign.center,
                            Margin {8, 0, 8, 0},
                            isChecked = Bind {document->onlyChosen},
                            toolTip = u"Выбранные в профиле и всё, что они тянут за собой",
                            content = u"Показать только выбранные",
                        },
                        Button {
                            toolButton,
                            toolTip = u"Отменить",
                            content = FontIcon {toolGlyph, glyph = glyphs::undo},
                        },
                        Button {
                            toolButton,
                            toolTip = u"Повторить",
                            content = FontIcon {toolGlyph, glyph = glyphs::redo},
                        },
                    },
                    TextBlock {
                        column = 1,
                        text = BindOutput {document->typeName},
                        vAlign.center,
                        Margin {24, 0, 0, 0},
                    },
                },
            },
            host,
            Border {
                row = 2,
                background = brushes.SolidBackgroundFillColor.Secondary,
                borderBrush = brushes.Card.StrokeColorDefault,
                BorderThickness {0, 1, 0, 0},
                Padding {12, 4},
                TextBlock {
                    text = BindOutput {document->path},
                    foreground = brushes.Text.FillColor.Secondary,
                    fontSize = 12.0,
                },
            },
        },
    };
    navigation.menuItems().append(typesItem);
    navigation.menuItems().append(resourcesItem);
    navigation.footerMenuItems().append(settingsItem);

    // Раскрытая панель — по самому длинному пункту: отступ под значок, текст и
    // поле справа.
    double longest = 0;
    for (zstring_view const label : {typesTab, resourcesTab, settingsName}) {
        TextBlock const text {label};
        text.measure({std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()});
        longest = std::max(longest, static_cast<double>(text.desiredSize().width));
    }
    navigation.openPaneLength(std::ceil(48 + longest + 28));
    typesItem.isSelected(true);

    // Выбранный тип — тоже место: переход по ссылке или по дереву записывается.
    document->typeName.on_change([places = history.get()](std::u16string const& name) noexcept {
        if (!name.empty() && places->current.page == typesTab.view()) {
            places->visit({places->current.page, name});
        }
    });
    auto const goBack = [typesItem, resourcesItem, settingsItem, left, editor = document.get(), places = history.get()] {
        if (places->back.empty()) {
            return;
        }
        History::Place const place = std::move(places->back.back());
        places->back.pop_back();
        places->replaying = true;
        if (place.page == settingsName.view()) {
            settingsItem.isSelected(true);
        } else {
            (place.page == resourcesTab.view() ? resourcesItem : typesItem).isSelected(true);
            if (place.page == typesTab.view() && !place.type.empty()) {
                if (auto const row = editor->reveal(std::wstring {place.type.begin(), place.type.end()})) {
                    left->reveal(*row);
                }
            }
        }
        places->replaying = false;
        places->current = place;
        places->canGoBack.set(!places->back.empty());
    };

    // Окно со своим заголовком, как в образце CustomTitleBar: заголовок — строка
    // над содержимым, кнопки окна рисует само окно его высотой, и масштаб
    // увеличивает весь остров разом — заголовок, кнопки окна и панели.
    CompositionWindow const window {
        zoom,
        focus,
        title = BindOutput {document->title},
        minSize = {640, 400},
        extendsContentIntoTitleBar = true,
        // Mica, как у Gallery: заголовок и панель навигации прозрачны, сквозь
        // них виден материал.
        systemBackdrop = MicaBackdrop {},
        titleBar = {
            title = BindOutput {document->title},
            // Стрелка на месте всегда, как в Gallery; возвращаться некуда — она
            // неактивна.
            isBackButtonVisible = true,
            isBackButtonEnabled = BindOutput {history->canGoBack},
            onBackRequested += [goBack] { goBack(); },
            isPaneToggleButtonVisible = true,
            onPaneToggleRequested += [navigation] { navigation.isPaneOpen(!navigation.isPaneOpen()); },
            rightHeader = StackPanel {
                orientation.horizontal,
                spacing = 4,
                vAlign.center,
                Margin {0, 0, 8, 0},
                Button {
                    toolButton,
                    toolTip = u"Уменьшить масштаб",
                    isEnabled = BindOutput {zoom.model().canZoomOut()},
                    onClick += [zoom] { zoom.model().zoomOut(); },
                    content = FontIcon {toolGlyph, glyph = glyphs::zoomOut},
                },
                Button {
                    toolButton,
                    width = 64.0,
                    toolTip = u"Масштаб 100 %",
                    onClick += [zoom] { zoom.model().resetZoom(); },
                    content = TextBlock {text = BindOutput {label->text}},
                },
                Button {
                    toolButton,
                    toolTip = u"Увеличить масштаб",
                    isEnabled = BindOutput {zoom.model().canZoomIn()},
                    onClick += [zoom] { zoom.model().zoomIn(); },
                    content = FontIcon {toolGlyph, glyph = glyphs::zoomIn},
                },
            },
        },
        navigation,
    };

    window.background(rgb(243, 243, 243));
    window.centreWithClientSize({1200, 800});
    window.activate();

    return [document, types, members, label, settings, focus, history, documentation](TeardownReason) {};
}
