// Визуальный редактор профилей генератора проекции.

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>

#include "Bind.h"
#include "Card.h"
#include "CompositionWindow.h"
#include "Editor.h"
#include "MagnifyEffect.h"
#include "SplitPanel.h"
#include "ThemeBrush.h"
#include "VirtualTree.h"
#include "generated/brushes.h"
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

std::wstring widen(std::string_view utf8) {
    std::wstring wide;
    if (auto const text = core::unicode::checked(utf8)) {
        core::unicode::append_utf16(wide, *text);
    }
    return wide;
}

// Знаки кнопок панели — размер 20 шрифта Fluent UI System Icons (Assets):
// контурные из Regular, open и save — заливочные из Filled.
namespace glyphs {
    constexpr char16_t newProfile[] {0xE4DB, 0};         // document_add
    constexpr char16_t open[] {0xF432, 0};               // folder_open, Filled
    constexpr char16_t save[] {0xF689, 0};               // save, Filled
    constexpr char16_t chosen[] {0xF28D, 0};             // checkbox_checked
    constexpr char16_t dependencies[] {0xE2FC, 0};       // checkbox_checked_sync
    constexpr char16_t notChosen[] {0xF291, 0};          // checkbox_unchecked
    constexpr char16_t undo[] {0xF199, 0};               // arrow_undo
    constexpr char16_t redo[] {0xF16E, 0};               // arrow_redo
    constexpr char16_t zoomOut[] {0xF8C6, 0};            // zoom_out
    constexpr char16_t zoomIn[] {0xF8C4, 0};             // zoom_in
}

// Вкладки над левым деревом. Выбранную узнают по тексту: вкладка — это её имя.
constexpr std::u16string_view typesTab = u"Types";
constexpr std::u16string_view resourcesTab = u"Resources";

// Масштаб всего окна — по сетке, а не множителем: из любого шага кнопки
// приходят в те же точки, и 100 % всегда среди них.
constexpr double zoomSteps[] {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
constexpr uint32_t zoomDefault = 2;
constexpr uint32_t zoomLast = std::size(zoomSteps) - 1;

core::u16_text percent(uint32_t step) {
    core::u16_text text = core::to_u16(zoomSteps[step] * 100, std::chars_format::fixed, 0);
    text += u" %";
    return text;
}

// Шаг сетки, который ставят кнопки, и то, что за ним следует: подпись и
// доступность кнопок на краях сетки.
struct Zoom {
    core::observable<uint32_t> step {zoomDefault};
    core::observable<core::u16_text> caption;
    core::observable<bool> canZoomOut;
    core::observable<bool> canZoomIn;

    Zoom() {
        caption.follow(step, percent);
        canZoomOut.follow(step, [](uint32_t step) { return step > 0; });
        canZoomIn.follow(step, [](uint32_t step) { return step < zoomLast; });
    }
};

}  // namespace

wxl::Teardown wxl_launched() {
    core::intrusive_ptr<Editor> document;
    try {
        document = Editor::open(defaultProfile());
    } catch (std::exception const& error) {
        auto window = Window {
            title = u"wxl.gen.ui",
            TextBlock {text = widen(error.what()), textWrapping.wrap, Margin {24}},
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
    document->revision.on_change([left, right](uint32_t) noexcept {
        left->refresh();
        right->refresh();
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
    // эффект, и анимации у всех кнопок общие.
    auto const pop = MagnifyEffect {1.2, maximum = 1.35, minimum = 0.95};

    auto const toolButton = Preset {
        styles.Button.CommandBarFlyoutEllipsis,
        pop,
    };

    // Переключатель показа — в тон кнопкам командной панели: без рамки, в
    // обычном состоянии прозрачный, нажатый — сдержанной подложкой, а не
    // акцентом шаблона ToggleButton; знак нажатого — цветом текста.
    auto const toolToggle = Preset {
        pop,
        width = 44.0,
        height = 40.0,
        Padding {0},
        BorderThickness {0},
        ThemeBrush {u"ToggleButtonBackground", brushes.SubtleFillColor.Transparent},
        ThemeBrush {u"ToggleButtonBackgroundPointerOver", brushes.SubtleFillColor.Secondary},
        ThemeBrush {u"ToggleButtonBackgroundPressed", brushes.SubtleFillColor.Tertiary},
        ThemeBrush {u"ToggleButtonBackgroundChecked", brushes.SubtleFillColor.Secondary},
        ThemeBrush {u"ToggleButtonBackgroundCheckedPointerOver", brushes.SubtleFillColor.Secondary},
        ThemeBrush {u"ToggleButtonBackgroundCheckedPressed", brushes.SubtleFillColor.Tertiary},
        ThemeBrush {u"ToggleButtonForegroundChecked", brushes.Text.FillColor.Primary},
        ThemeBrush {u"ToggleButtonForegroundCheckedPointerOver", brushes.Text.FillColor.Primary},
        ThemeBrush {u"ToggleButtonForegroundCheckedPressed", brushes.Text.FillColor.Secondary},
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

    auto const zoom = core::make_refcounted<Zoom>();

    // Окно со своим заголовком, как в образце CustomTitleBar: заголовок — строка
    // над содержимым, кнопки окна рисует само окно его высотой, и масштаб
    // увеличивает весь остров разом — заголовок, кнопки окна и панели.
    //
    // Панель профиля пока макет: действий на кнопках нет; переключатели показа —
    // знаки тех же отметок, что в дереве.
    CompositionWindow const window {
        title = BindOutput {document->title},
        minSize = {640, 400},
        extendsContentIntoTitleBar = true,
        titleBar = {
            background = brushes.SolidBackgroundFillColor.Secondary,
            title = BindOutput {document->title},
            rightHeader = StackPanel {
                orientation.horizontal,
                spacing = 4,
                vAlign.center,
                Margin {0, 0, 8, 0},
                Button {
                    toolButton,
                    toolTip = u"Уменьшить масштаб",
                    isEnabled = BindOutput {zoom->canZoomOut},
                    onClick = [zoom] {
                        if (uint32_t const step = zoom->step.get(); step > 0) {
                            zoom->step.set(step - 1);
                        }
                    },
                    content = FontIcon {toolGlyph, glyph = glyphs::zoomOut},
                },
                Button {
                    toolButton,
                    width = 64.0,
                    toolTip = u"Масштаб 100 %",
                    onClick = [zoom] { zoom->step.set(zoomDefault); },
                    content = TextBlock {text = BindOutput {zoom->caption}},
                },
                Button {
                    toolButton,
                    toolTip = u"Увеличить масштаб",
                    isEnabled = BindOutput {zoom->canZoomIn},
                    onClick = [zoom] {
                        if (uint32_t const step = zoom->step.get(); step < zoomLast) {
                            zoom->step.set(step + 1);
                        }
                    },
                    content = FontIcon {toolGlyph, glyph = glyphs::zoomIn},
                },
            },
        },
        // Панель инструментов наверху, раздвижка под ней, строка состояния внизу.
        Grid {
            rowDefinitions = u"40,*,auto",
            Card {
                topPanel,
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
                        ToggleButton {
                            toolToggle,
                            isChecked = true,
                            toolTip = u"Показывать выбранные",
                            content = FontIcon {toolGlyph, glyph = glyphs::chosen},
                        },
                        ToggleButton {
                            toolToggle,
                            isChecked = true,
                            toolTip = u"Показывать автоматически включённые зависимости",
                            content = FontIcon {toolGlyph, glyph = glyphs::dependencies},
                        },
                        ToggleButton {
                            toolToggle,
                            isChecked = true,
                            toolTip = u"Показывать невыбранные",
                            content = FontIcon {toolGlyph, glyph = glyphs::notChosen},
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
            SplitPanel {
                row = 1,
                openPaneLength = 520.0,
                spacing = 8.0,
                pane = Grid {
                    rowDefinitions = u"auto,*",
                    SelectorBar {
                        onSelectionChanged = [left, document](SelectorBar const& bar,
                                                              SelectorBarSelectionChangedEventArgs&) {
                            if (auto const item = bar.selectedItem()) {
                                bool const resources = std::u16string_view {item.text()} == resourcesTab;
                                left->model(resources ? document->resources() : document->types());
                            }
                        },
                        SelectorBarItem {text = typesTab, isSelected = true},
                        SelectorBarItem {text = resourcesTab},
                    },
                    Border {row = 1, framed, left->view()},
                },
                content = Border {framed, right->view()},
            },
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

    // Окно — ручка, и единственный наблюдатель, который его масштабирует,
    // держит его по значению; кнопки держат модель и окна не видят.
    zoom->step.on_change([window](uint32_t const& step) noexcept { window.zoom(zoomSteps[step]); });

    window.background(rgb(243, 243, 243));
    window.centreWithClientSize({1200, 800});
    window.activate();

    return [document, types, members, zoom](TeardownReason) {};
}
