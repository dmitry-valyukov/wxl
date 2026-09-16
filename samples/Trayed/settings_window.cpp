// The settings window: the application's second window, built on demand and
// closing for real. The first window is the console host -- itself a wxl::Window
// now, so WinUI does not end the application when this one closes -- and this is
// the ordinary dialog wxl exists to make, written in the declarative syntax the
// library provides.
//
// Not one control carries a handler of its own: every one is `Bind`ed to a field
// of the settings model, which keeps the control and the model in step both ways,
// and Trayed watches those same observables to save and to apply. The binding
// lives inside the field: its watch owns the control (never a borrowed pointer to
// a temporary in this description), and the control holds nothing back. Here the
// model outlives the window -- the settings are kept for the run, the dialog
// opens and closes -- so the watches would keep the closed window's controls
// alive until the end; Trayed cuts them with unsubscribe_all from the window's
// Closed handler, which is what lets the window truly go.

#include "Settings.h"

#include "aliases.h"
#include "Panels.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"

#include "Bind.h"
#include "WindowFit.h"
#include "settings_window.h"

using namespace wxl;
using namespace wxl::dsl;

namespace trayed {
namespace {

// The width the form is given, in DIPs: a vertical stack of text fields has no
// width of its own -- the fields would collapse to their minimum -- so one is
// set, and fitToContent sizes the window to it.
constexpr double kFormWidth = 460.0;

// A ComboBox filled from a runtime list and bound to the index of the chosen
// row, under a label -- ComboBox has no Header in this profile. Built here
// rather than in the window's braces because the rows come from a vector, and
// the declarative form spreads fixed children, not a run-time list.
StackPanel labeledCombo(std::wstring_view label, std::vector<std::wstring> const& options,
                        core::observable<int>& model) {
    ComboBox combo;
    for (std::wstring const& option : options) combo.setPositional(ComboBoxItem{option});
    Bind{model}(combo);

    return StackPanel{
        spacing = 4.0,
        TextBlock{label},
        combo,
    };
}

}  // namespace

Window buildSettingsWindow(Settings& settings) {
    Window window{
        title = L"Настройки Trayed",
        // A floor well under the fitted size, so it only stops the user shrinking
        // the window into nothing -- the fit below sets the size it opens at.
        minSize = {360, 320},
        StackPanel{
            width = kFormWidth,
            Padding{24},
            spacing = 16.0,
            ToggleSwitch{
                header = L"Сворачивать в трей при закрытии окна",
                Bind{settings.minimizeOnClose},
            },
            TextBox{
                header = L"Заголовок окна",
                Bind{settings.windowTitle},
            },
            labeledCombo(L"Шрифт консоли", fontFamilies(), settings.fontFamily),
            NumberBox{
                header = L"Размер шрифта, пикселей",
                minimum = 8.0,
                maximum = 48.0,
                Bind{settings.fontSize},
            },
            labeledCombo(L"Цветовая тема", themeNames(), settings.theme),
        },
    };

    fitToContent(window);
    return window;
}

}  // namespace trayed
