// Визуальный редактор профилей генератора проекции.

#include <exception>
#include <filesystem>
#include <string>

#include "Bind.h"
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

    auto window = Window {
        title = u"wxl.gen.ui",
        SplitView {
            displayMode = SplitViewDisplayMode::Inline,
            isPaneOpen = true,
            openPaneLength = 520.0,
            pane = Grid {
                rowDefinitions = u"auto,*",
                CommandBar {
                    content = TextBlock {
                        text = BindOutput {document->profileName},
                        vAlign.center,
                        Margin {12, 0},
                    },
                },
                Border {row = 1, left->view()},
            },
            content = Grid {
                rowDefinitions = u"auto,*",
                CommandBar {
                    content = TextBlock {
                        text = BindOutput {document->typeName},
                        vAlign.center,
                        Margin {12, 0},
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
