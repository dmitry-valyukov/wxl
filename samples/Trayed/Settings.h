#pragma once

// core.h brings the standard library (string and vector among it) and then
// `import wxl.core;`, which is where observable lives. Included first so its std
// headers precede the import, the rule every wxl-touching translation unit
// keeps -- and the reason no <string>/<vector> follows it here: after the import
// a standard header is too late.
#include "core.h"

// u16_text -- validated UTF-16 -- is what the typed title binds on: a text
// control's model is checked text, not a bare std::wstring.
import wxl.core;

// What the settings window edits and what the application remembers between
// runs. Kept apart from the UI (Settings.cpp is plain Win32 and registry, the
// window is wxl) the way the calculator keeps Calc apart from its keys.
//
// Every bound field is an `observable`, not a bare value: that is what makes the
// binding two-way. The control shows it, edits write it back, and anyone
// watching (here, the saving and the applying to the console) hears every
// change -- none of which a plain field can do. The fields are members, held by
// value: Settings is the one object with an identity, and there is no copying
// it -- it is filled in place and lives for the run.
//
// The font family and the theme are indices, not names: a ComboBox picks a row,
// and the row's meaning (a face name, a palette) is looked up from the tables in
// Settings.cpp. The size is the *base* size -- what a fresh run starts at and
// what Ctrl+0 returns to; the transient zoom (Ctrl+wheel, Ctrl+plus/minus) moves
// the console's font without touching it.

struct Settings {
    // The close button: fold the window into the tray, or end the program.
    wxl::core::observable<bool> minimizeOnClose{true};

    // The console font family, by index into fontFamilies().
    wxl::core::observable<int> fontFamily{0};

    // The console font's base cell height in pixels (console fonts are sized in
    // pixels, not points).
    wxl::core::observable<int> fontSize{18};

    // The colour theme, by index into themeNames().
    wxl::core::observable<int> theme{0};

    // The window's title bar text, typed in the dialog. An observable of
    // u16_text -- validated UTF-16 -- so the TextBox binds on checked text; the
    // literal is checked where it is written.
    wxl::core::observable<wxl::core::u16_text> windowTitle{
        wxl::core::u16_text{wxl::core::u16_view{u"Trayed"}}};
};

/// Fills the settings with what was last saved, leaving the defaults on a first
/// run. From the registry (HKCU\Software\wxl\Trayed) -- a handful of values, no
/// file and no locale to get wrong. In place rather than returned: the fields
/// are observables, and an observable is a member, not a value to hand around.
void loadSettings(Settings& settings);

/// Writes the settings back. Called after every change the window makes.
void saveSettings(Settings const& settings);

/// The monospace families offered for the console, curated rather than
/// enumerated: what a console is actually readable in, not every face installed.
/// The fontFamily index is a position in this list.
std::vector<std::wstring> const& fontFamilies();

/// The colour themes offered, by name. The theme index is a position in this
/// list; the palette behind each lives in Settings.cpp.
std::vector<std::wstring> const& themeNames();

/// Puts a font family (by index into fontFamilies()) and a cell height in
/// pixels on the console the application owns. The size is taken as given --
/// the base from the settings, or a transient one the zoom is at -- so the one
/// function serves the dialog and Ctrl+wheel / Ctrl+plus/minus / Ctrl+0 alike.
void applyConsoleFont(int familyIndex, int sizePx);

/// Puts a colour theme (by index into themeNames()) on the console: its palette
/// recolours everything on screen at once, since a cell holds a palette index
/// and the table behind those indices is what changes.
void applyConsoleTheme(int themeIndex);
