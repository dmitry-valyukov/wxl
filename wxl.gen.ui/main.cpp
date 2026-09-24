// Визуальный редактор профилей генератора проекции.

#include <exception>
#include <filesystem>
#include <string>

#include "Bind.h"
#include "Card.h"
#include "Editor.h"
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

// Знаки кнопок панели — размер 20 шрифта Fluent UI System Icons (Assets).
namespace glyphs {
    constexpr char16_t newProfile[] {0xE4DB, 0};         // document_add
    constexpr char16_t open[] {0xF42E, 0};               // folder_open
    constexpr char16_t save[] {0xF67F, 0};               // save
    constexpr char16_t chosen[] {0xF28D, 0};             // checkbox_checked
    constexpr char16_t dependencies[] {0xE2FC, 0};       // checkbox_checked_sync
    constexpr char16_t notChosen[] {0xF291, 0};          // checkbox_unchecked
    constexpr char16_t undo[] {0xF199, 0};               // arrow_undo
    constexpr char16_t redo[] {0xF16E, 0};               // arrow_redo
}

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

    // Верхняя панель — карточка, как в образце HelloHere, высотой в кнопку.
    auto const topPanel = Preset {
        row = 0,
        hAlign.stretch,
        CornerRadius {2},
        Padding {16, 0},
        height = 40.0,
    };

    auto const toolGlyph = Preset {
        fontFamily = FontFamily {u"Assets/FluentSystemIcons-Regular.ttf#FluentSystemIcons-Regular"},
        fontSize = 20.0,
    };

    // Верхние панели — как в образце HelloHere. Панель профиля пока макет:
    // действий на кнопках нет; переключатели показа — знаки тех же отметок,
    // что в дереве.
    auto window = Window {
        title = BindOutput {document->title},
        SplitView {
            displayMode = SplitViewDisplayMode::Inline,
            isPaneOpen = true,
            openPaneLength = 520.0,
            pane = Grid {
                rowDefinitions = u"auto,*",
                Card {
                    topPanel,
                    StackPanel {
                        orientation.horizontal,
                        spacing = 4,
                        Button {
                            styles.Button.CommandBarFlyoutEllipsis,
                            toolTip = u"Новый профиль",
                            content = FontIcon {toolGlyph, glyph = glyphs::newProfile},
                        },
                        Button {
                            styles.Button.CommandBarFlyoutEllipsis,
                            toolTip = u"Открыть профиль",
                            content = FontIcon {toolGlyph, glyph = glyphs::open},
                        },
                        Button {
                            styles.Button.CommandBarFlyoutEllipsis,
                            toolTip = u"Сохранить профиль",
                            content = FontIcon {toolGlyph, glyph = glyphs::save},
                        },
                        ToggleButton {
                            isChecked = true,
                            toolTip = u"Показывать выбранные",
                            content = FontIcon {toolGlyph, glyph = glyphs::chosen},
                        },
                        ToggleButton {
                            isChecked = true,
                            toolTip = u"Показывать автоматически включённые зависимости",
                            content = FontIcon {toolGlyph, glyph = glyphs::dependencies},
                        },
                        ToggleButton {
                            isChecked = true,
                            toolTip = u"Показывать невыбранные",
                            content = FontIcon {toolGlyph, glyph = glyphs::notChosen},
                        },
                        Button {
                            styles.Button.CommandBarFlyoutEllipsis,
                            toolTip = u"Отменить",
                            content = FontIcon {toolGlyph, glyph = glyphs::undo},
                        },
                        Button {
                            styles.Button.CommandBarFlyoutEllipsis,
                            toolTip = u"Повторить",
                            content = FontIcon {toolGlyph, glyph = glyphs::redo},
                        },
                    },
                },
                Border {row = 1, left->view()},
            },
            content = Grid {
                rowDefinitions = u"auto,*",
                Card {
                    topPanel,
                    TextBlock {
                        text = BindOutput {document->typeName},
                        vAlign.center,
                    },
                },
                Border {row = 1, right->view()},
            },
        },
    };

    window.appWindow().resize({1200, 800});
    window.activate();

    return [document, types, members](TeardownReason) {};
}
