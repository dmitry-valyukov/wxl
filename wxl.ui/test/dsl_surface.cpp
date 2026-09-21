// Compile-time check of the declarative surface.
//
// It is built as its own target *without* the cppwinrt include path, so it
// proves two things at once: that the builder syntax below compiles against
// the generated wrappers, and that reaching that syntax needs no winrt
// header at all -- if a winrt:: name ever leaked into a public header, this
// translation unit would stop compiling rather than quietly get slower.
//
// Nothing here runs: the WinUI3 runtime is not up in a test, and activating
// a real control without it fails. This is about the syntax and the types.

#include "BbBlock.h"
#include "Bind.h"
#include "event_awaitable.h"
#include "Card.h"
#include "CompositionWindow.h"
#include "DrawingSurface.h"
#include "FormattedBlock.h"
#include "HtmlBlock.h"
#include "RsdnBlock.h"
#include "ShowDialog.h"
#include "ThemeBrush.h"
#include "Panels.h"
#include "UiThread.h"
#include "ui.h"
#include "generated/Microsoft.UI.Dispatching.h"
#include "schema.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.Hosting.h"
#include "generated/brushes.h"

namespace {

using namespace wxl;

// The vocabulary of the syntax lives in a namespace of its own and is brought
// in deliberately, so that naming a wxl type does not drag every property
// name into scope with it. The two never collide: a type is PascalCase and a
// member is camelCase, so `Margin` is the type and `margin` the tag.
using namespace wxl::dsl;

// Named arguments, an event handler, and unnamed arguments routed by type:
// a HorizontalAlignment can only mean HorizontalAlignment, and a string on
// a ContentControl can only mean Content.
[[maybe_unused]] void named_and_positional() {
    Button explicit_names{
        content = L"Click",
        horizontalAlignment = HorizontalAlignment::Center,
        onClick = [](Object const&, RoutedEventArgs&) {},
    };

    Button short_form{
        L"Click",
        hAlign.center,
        vAlign.center,
    };

    TextBlock text{
        L"WinUI 3 in C++ Without XAML!",
        hAlign.center,
    };

    StackPanel panel{
        Orientation::Vertical,
        spacing = 8.0,
    };
}

// FormattedBlock: the declarative form only creates -- properties of the
// RichTextBlock underneath -- while content arrives through the procedural
// appenders. Both halves have to compile without a winrt header in sight.
[[maybe_unused]] void formatted_block() {
    FormattedBlock block{
        fontSize = 14.0,
        isTextSelectionEnabled = true,
    };

    block.onLink([](std::wstring_view) {});
    block.onError([](HtmlError const& error) {
        (void)error.kind;
        (void)error.detail;
    });
    block.appendParagraph();
    block.appendParagraph({.margin = Thickness{0, 0, 0, 8}, .alignment = TextAlignment::Center});
    block.pushStyle({.bold = true});
    block.appendText(L"WinUI 3");
    block.popStyle();
    block.appendText(L" in C++ Without XAML!");
    block.appendLineBreak();
    block.appendLink(L"wxl", L"https://example.invalid/wxl");
    block.pushLink(L"note:1");
    block.appendText(L"сноска");
    block.popLink();
    block.appendScript(L"2", 0.62, 0.25);
    block.appendImage(L"Assets/logo.png", {24, 24}, L"логотип");
    block.appendElement(Button{content = L"Click"});
    block.clear();
}

// HtmlBlock: markup as the positional string, styles registered by name --
// and none of it needing a winrt header, the parser being a module behind
// the library's own wall.
[[maybe_unused]] void html_block() {
    HtmlBlock markup{
        L"Привет, <b>мир</b>! <a href='faq'>Подробности</a>.",
        isTextSelectionEnabled = true,
    };

    markup.registerStyle(L"quote", {.text = {.italic = true},
                                    .block = {.margin = Thickness{24, 4, 0, 4}}});
    markup.registerStyles({
        {L"warn", {.text = {.color = ARGB{0xFFC0392B}, .bold = true}}},
        {L"note", {.text = {.color = ARGB{0xFF606060}}}},
    });
    markup.onLink([](std::wstring_view) {});
    markup.onError([](HtmlError const&) {});
    markup.theme(HtmlTheme{.quoteColor = {ARGB{0xFF808080}},
                           .preRadius = 6,
                           .preElevation = 24,
                           .preBackground = ARGB{0xFFF2F2F2},
                           .listIndent = 32});
    markup.baseDirectory(LR"(C:\docs)");
    markup.append(L"<p style=\"note\">дописано <i>куском</i></p>");
    markup.html(L"<h1>Заново</h1>");
}

// Братья HtmlBlock над общим MarkupBlock: своя только дверь разметки, всё
// остальное — тема, база картинок, onLink/onError — семейное.
[[maybe_unused]] void bb_and_rsdn_blocks() {
    BbBlock forum{
        L"Привет, [b]мир[/b]! [url=https://example.invalid]пример[/url]",
        isTextSelectionEnabled = true,
    };
    forum.theme(HtmlTheme{});
    forum.baseDirectory(LR"(C:\docs)");
    forum.onLink([](std::wstring_view) {});
    forum.onError([](HtmlError const&) {});
    forum.append(L"[quote=Вася]дописано[/quote]");
    forum.bb(L"[list][*]заново[/list]");

    RsdnBlock post{
        L"AVK>цитата\nответ [c#]var x = 1;[/c#]",
    };
    post.onError([](HtmlError const&) {});
    post.append(L"[q]ещё[/q]");
    post.rsdn(L"[h1]Заново[/h1]");
}

// A collection-valued property is subscripted, not assigned to, and the
// items arrive as a parameter pack -- each keeps its own type instead of
// being sliced to a common base before the collection sees it.
[[maybe_unused]] void children_list() {
    StackPanel panel{
        hAlign.center,
        children[
            TextBlock{L"WinUI 3 in C++ Without XAML!", hAlign.center},
            Button{content = L"Click"}
        ],
    };
}

// The same list without the property name: an element written straight
// inside its parent's braces goes to the class's content collection, which
// the metadata names itself.
[[maybe_unused]] void unnamed_children() {
    StackPanel panel{
        hAlign.center,
        orientation.horizontal,
        Image{source = L"Assets/logo.png", hAlign.right},
        TextBlock{L"WinUI 3 in C++ Without XAML!"},
        Button{content = L"Click"},
    };

    // A lone child is still a child as long as it is not of the parent's own
    // kind -- that case belongs to the copy constructor.
    StackPanel single{Button{content = L"Click"}};
}

// A repeated child, and the properties a child carries for its parent's
// layout: the count is part of repeat's type, and `row` reaches
// Grid::setRow(child, value) although it is written inside the child. The
// rows are the same property the collection form subscripts, said as text.
[[maybe_unused]] void repeated_children() {
    Grid grid{
        rowDefinitions = L"2*,*,*",
        columnDefinitions = L"*,*",
        TextBlock{L"header", row = 0, columnSpan = 2},
        [](repeat<4> index) {
            return Button{
                content = L"key",
                row = index / 2 + 1,
                column = index % 2,
            };
        },
    };
}

// The same over given values: a literal's characters, a list written out, a
// constexpr array, or any mixture of those -- one parameter pack takes all
// of them, and the body is handed the element and its position. `text()` is
// there only where the elements are text.
[[maybe_unused]] void iterated_children() {
    StackPanel keypad{
        [](iterate<L"789"> key) {
            return Button{
                key.text(),
                onClick = [key](Object const&, RoutedEventArgs&) {
                    wchar_t const pressed = key;
                    (void)pressed;
                },
            };
        },
    };

    // What counts as an element, checked where it is cheapest to check: a
    // literal's terminator is not one, an array that merely ends in a zero
    // keeps it, and a character array without a terminator keeps all of it.
    static constexpr char unterminated[]{'a', 'b', 'c'};
    static constexpr int ending_in_zero[]{1, 4, 0};
    static_assert(iterate<L"abc">::count == 3);
    static_assert(iterate<L"">::count == 0);
    static_assert(iterate<unterminated>::count == 3);
    static_assert(iterate<ending_in_zero>::count == 3);
    static_assert(iterate<0>::count == 1);
    static_assert(iterate<L"ab", L'c', 'd'>::count == 4);

    // A run of characters is terminated in storage whether its source was
    // or not, so the elements are always a valid string of `count`.
    static constexpr impl::values from_literal = L"abc";
    static constexpr impl::values from_array = unterminated;
    static constexpr impl::values from_value = L'x';
    static_assert(from_literal.count == 3 && from_literal.items[from_literal.count] == L'\0');
    static_assert(from_array.count == 3 && from_array.items[from_array.count] == '\0');
    static_assert(from_value.count == 1 && from_value.items[from_value.count] == L'\0');

    static constexpr int spans[]{1, 2, 4};
    Grid layout{
        columnDefinitions = L"*,*,*,*",
        [](iterate<0, 1, 2> at) { return TextBlock{L"head", column = at}; },
        [](iterate<spans> span) { return TextBlock{L"body", row = 1, columnSpan = span}; },
        [](iterate<L"ab", L'c'> mixed) { return TextBlock{mixed.text(), row = 2}; },
    };
}

// Margin and Padding are wxl types of their own, so they read as braced
// lists and route positionally without the property name.
[[maybe_unused]] void thicknesses() {
    Button uniform{margin = {20}};
    Button pairs{margin = {20, 8}, padding = {4, 2}};
    Button sides{margin = {1, 2, 3, 4}};
    Button positional{Margin{20}, Padding{4}};
}

// A tag over a built-in: thousandths of an em is an int32, and an int says
// nothing about which property it belongs to, so the unnamed form needs the
// tag while the named one stays an ordinary assignment.
[[maybe_unused]] void spacing() {
    TextBlock tagged{L"wide", CharacterSpacing{60}};
    TextBlock named{L"wide", characterSpacing = 60};
}

// A property wxl declares although WinRT has none: the minimum size lives
// on the window's presenter, three hops away, and reads here as one word.
// A static member is a call on the class, not on an object.
[[maybe_unused]] void window_constraints(Window const& window) {
    Window bounded{minSize = {600, 400}};

    OverlappedPresenter const presenter = OverlappedPresenter::create();
    presenter.preferredMinimumWidth(600);
    core::nullable<int32_t> const width = presenter.preferredMinimumWidth();
    presenter.preferredMaximumWidth({});
    window.appWindow().setPresenter(presenter);
    (void)width;
}

// A method written as a tag: Window.SetTitleBar takes one argument and hands
// nothing back, and the profile narrows it to the TitleBar control -- so the
// tag takes one built, builds one from braces, and a lone TitleBar inside the
// window's braces is routed to it by type. The button colours are nullable:
// empty hands the colour back to the system.
[[maybe_unused]] void window_title_bar(Window const& window, TitleBar const& built) {
    Window assigned{extendsContentIntoTitleBar = true, titleBar = built};
    Window braced{
        extendsContentIntoTitleBar = true,
        titleBar = {leftHeader = TextBlock{L"App"}, rightHeader = Button{L"Sign in"}},
    };
    Window routed{built};
    window.titleBar(built);

    AppWindowTitleBar const bar = window.appWindow().titleBar();
    bar.preferredHeightOption(TitleBarHeightOption::Tall);
    bar.buttonBackgroundColor(colors.transparent);
    bar.buttonHoverBackgroundColor({});
    core::nullable<Color> const background = bar.buttonBackgroundColor();
    (void)background;
}

// wxl's own window written the same way: a handle, so every member is const
// and the tags apply to it; its title bar is placed by the window itself, so
// it is built right in the braces; its own events take the same tags, and a
// copy captured by a lambda is the same window.
[[maybe_unused]] void composition_window(TitleBar const& built, Grid const& page) {
    CompositionWindow window{
        title = L"App",
        minSize = {820, 560},
        extendsContentIntoTitleBar = true,
        titleBar = {leftHeader = TextBlock{L"App"}, rightHeader = Button{L"Sign in"}},
        zoom = 1.25,
        onClosed = [] {},
        onGeometryChanged = [] {},
        onClientSizeChanged = [](Object const&, ClientSize const& client) { (void)client.scale; },
        onKeyDown = [](Object const&, VirtualKey const& key) { (void)key; },
        page,
    };
    CompositionWindow routed{built, page};
    CompositionWindow const legacy{L"App", SizeInt32{820, 560}};

    // The scene's background is a tag as well: a colour, or a picture with the
    // way it covers the window and the colour under it.
    CompositionWindow const tiled{
        background = BackgroundImage{L"Assets/paper.png", BackgroundFill::TileMirrored},
        page,
    };
    CompositionWindow const coloured{background = ARGB{0xFF202020}};
    tiled.backgroundAsync(BackgroundImage{L"Assets/cover.png", BackgroundFill::None, ARGB{0xFF000000}});
    (void)coloured;

    auto const closeLater = [window] { window.close(); };
    (void)closeLater;
    window.zoom(window.zoom() * 1.1);
    window.appWindow().title(legacy.title());
    auto sizes = wxl::on_event<EventKey::ClientSizeChanged>(window);
    (void)sizes;
}

// The even stack: children claim a star row (column) apiece instead of
// being numbered by hand, and everything of Grid's -- spacing here -- still
// applies, because a Rows is a Grid.
[[maybe_unused]] void even_stacks() {
    Rows rows{
        rowSpacing = 8.0,
        TextBlock{L"top third"},
        Button{L"middle third"},
        TextBlock{L"bottom third"},
    };
    Columns columns{
        Button{L"left half"},
        Button{L"right half"},
    };
}

// Elevation, the web-card way: the shadow names what is cast and the
// translation's z lifts the caster, and only the pair reads as depth. A
// ThemeShadow is also a Shadow, so the unnamed form routes it by the base.
[[maybe_unused]] void elevation() {
    Border named{
        shadow = ThemeShadow{},
        translation = {0, 0, 32},
    };
    Border positional{ThemeShadow{}};
    Vector3 const lifted = named.translation();
    (void)lifted;
}

// The math door behind a RichEditBox: content goes in whole -- plain or RTF
// text in one SetText call, a formula as MathML -- and the mode gates what
// typing builds up. Display-only math is a read-only box with the content
// set first: a read-only document refuses programmatic writes too.
[[maybe_unused]] void rich_document(RichEditBox const& box) {
    RichEditTextDocument const document = box.document();
    document.setText(TextSetOptions::FormatRtf, LR"({\rtf1 plain, \b bold\b0 , \i italic\i0 })");
    document.setMathMode(RichEditMathMode::MathOnly);
    document.setMathML(LR"(<math xmlns="http://www.w3.org/1998/Math/MathML"><mi>x</mi></math>)");
}

// Formatting range by range -- the first wrapped WinRT *interfaces*:
// getRange hands back a TextRange (ITextRange under the I-less wxl name),
// whose characterFormat is live -- setting an effect on it changes the
// slice alone. Effects are tri-state, so a mixed range reads Undefined.
[[maybe_unused]] void range_format(RichEditTextDocument const& document) {
    TextRange const range = document.getRange(0, 3);
    TextCharacterFormat const format = range.characterFormat();
    format.bold(FormatEffect::On);
    format.italic(FormatEffect::Off);
    format.subscript(FormatEffect::Off);
    format.superscript(FormatEffect::On);
    [[maybe_unused]] FormatEffect const mixed = format.bold();
    range.characterFormat(format);
}

// A named framework brush, reached as a path like a style but assigned by
// name: a Brush fits background, borderBrush and foreground alike, so
// routing one by type alone would be a guess.
[[maybe_unused]] void named_brush() {
    Border card{
        background = brushes.Card.BackgroundFillColor.Default,
        borderBrush = brushes.Card.StrokeColorDefault,
    };
    TextBlock text{L"secondary", foreground = brushes.Text.FillColor.Secondary};
}

// A synthetic property with two homes: the same text is the hover hint
// (ToolTipService) and the automation name a screen reader speaks. It
// exists for controls whose face is a glyph rather than a word.
[[maybe_unused]] void tool_tip() {
    Button glyphOnly{toolTip = L"Switch the theme"};
    glyphOnly.toolTip(L"Switch the theme");
}

// A theme flip from code: RequestedTheme on the content root re-themes every
// control under it, ActualThemeChanged is where hand-assigned brushes are
// re-read -- an assigned brush is a value, not a reference the theme can
// retarget -- and the call form of a brush path is the themed lookup itself.
[[maybe_unused]] void theme_flip(Grid const& page) {
    page.requestedTheme(page.actualTheme() == ElementTheme::Dark ? ElementTheme::Light
                                                                 : ElementTheme::Dark);
    EventToken const token = page.add_onActualThemeChanged([page](Object const&, Object const&) {
        page.background(brushes.SolidBackgroundFillColor.Base(page.actualTheme()));
    });
    page.remove_onActualThemeChanged(token);
}

// A named framework resource, reached as a path and routed by its type: a
// Style can only be the Style property.
[[maybe_unused]] void named_style() {
    TextBlock title{
        L"WinUI 3 in C++ Without XAML!",
        styles.TextBlock.Title,
        hAlign.center,
    };
}

// A look kept at namespace scope. Values are laid out by the compiler, and
// an object inside it is a template, built where the look is applied -- a
// fresh one per application, so a child has no second parent and a shadow is
// nobody's before the runtime is up. The look with values only is a constant;
// the one carrying a template is a constant object whose template is made
// at start-up, in the pool that stands from before main().
namespace library_presets {
    inline constexpr Preset shape{CornerRadius{12}, translation = {0, 0, 32}};

    inline Preset const card{
        shape,
        background = brushes.Card.BackgroundFillColor.Default,
        shadow = Template<ThemeShadow>{},
    };

    inline Preset const stamp{
        Template<Button>{content = L"fresh per application"},
    };
}

[[maybe_unused]] void deferred_values() {
    Border card{library_presets::card};
    Border direct{
        background = Template<SolidColorBrush>{colors.white},
        Template<ThemeShadow>{},  // unnamed: routed by what it builds
    };
    StackPanel one{library_presets::stamp};
    StackPanel two{library_presets::stamp};

    // The empty template assigned to a property is the plain object, built at
    // application like everything else a template defers; a copy of a preset
    // is a copy of its arguments.
    Border spelled{shadow = Template<ThemeShadow>{}};
    auto const copy = library_presets::card;
    Border dressed{copy};
}

// A named look, made the control it describes: the look is worn in the
// constructor with the call site's setters after it, and where a fresh
// control is wanted per use, a template builds it.
[[maybe_unused]] void built_look() {
    Border card{library_presets::card, width = 450, height = 380};
    Grid host{Template<Border>{library_presets::card, TextBlock{L"inside"}}};
    Border const copied{card};
}

// A preset: the arguments, kept in a variable and written once. Two forms,
// and each of them means one thing only -- unnamed inside braces it dresses
// the object being built, and called it dresses one that already exists. The
// object a property asks for is a template's to build.
[[maybe_unused]] void own_preset() {
    constexpr Preset glow{
        center = {0.33, 0.33},
        gradientOrigin = {0.33, 0.33},
        radiusX = 1.1,
        radiusY = 1.3,
    };

    Border built{background = RadialGradientBrush{
                     glow,
                     GradientStop{ARGB{0x333333B4}, offset = 0.0},
                     GradientStop{ARGB{0x20202087}, offset = 1.0},
                 }};

    Border given{background = Template<RadialGradientBrush>{glow}};

    RadialGradientBrush plain;
    glow(plain);
}

// A template with parameters is a function returning one, and a look built
// on a preset is that preset written inside the template's braces -- unnamed,
// the same way it would be written inside the object's.
[[maybe_unused]] void templates_of_presets() {
    constexpr Preset shape{
        center = {0.33, 0.33},
        gradientOrigin = {0.33, 0.33},
        radiusX = 1.1,
        radiusY = 1.3,
    };

    auto const glowing = [shape](uint32_t centre, uint32_t edge) {
        return Template<RadialGradientBrush>{
            shape,
            GradientStop{ARGB{centre}, offset = 0.0},
            GradientStop{ARGB{edge}, offset = 1.0},
        };
    };

    Border keypad{background = glowing(0x333333B4, 0x20202087)};
    Border display{background = glowing(0xFFDCE8B4, 0xFFA6B287)};
}

// A template assigned to a property is the object that property asks for, and
// always that: it is built and set, never applied to the object on the left.
// `build()` is the same step, said out loud; wearing is the preset's form.
[[maybe_unused]] void template_as_the_value() {
    auto const ink = Template<SolidColorBrush>{color = ARGB{0xFF7C8768}};

    Border given{borderBrush = ink};

    SolidColorBrush const brush = ink.build();
    Border same{borderBrush = brush};

    constexpr Preset inkLook{color = ARGB{0xFF7C8768}};
    SolidColorBrush worn{inkLook};
}

// A preset fits whatever has the members it names: the arguments are checked
// against the object wearing it, so font settings written once go on a
// Control and on a TextBlock, which share no base that has a font.
[[maybe_unused]] void preset_fits_by_its_members() {
    constexpr Preset wide{fontSize = 18.0, hAlign.stretch};

    Button key{L"7", wide};
    TextBox entry{wide};
    TextBlock label{wide};
}

// The braced handler shorthand: the body is assignments applied to the
// object the event is attached to.
[[maybe_unused]] void handler_shorthand() {
    Button button{
        content = L"Click",
        onClick = {content = L"Thank You!"},
    };
}

// A binding written on a property. Which way it runs is the property's: one
// way where the control only shows, both ways where it writes the property
// itself -- decided by the pair in impl/binding.h, not by a mode. The model is
// one refcounted object whose observables are its fields, not handles.
namespace {
struct BoundModel : core::sta_refcounted {
    core::observable<std::u16string> title{u"WXL"};
    core::observable<bool> busy;
    core::observable<core::u16_text> name;
    core::observable<int> row;
};
}  // namespace

[[maybe_unused]] void binding_by_property() {
    core::intrusive_ptr<BoundModel> const model{new BoundModel{}};

    TextBlock{text = Bind{model->title}};   // one way: no pair for text on a TextBlock
    Button{isEnabled = Bind{model->busy}};       // one way, and named: bool alone could not choose
    ProgressRing{isActive = Bind{model->busy}};  // the same bool, another property
    TextBox{text = Bind{model->name}};      // two ways: a TextBox writes its text
    ToggleSwitch{isOn = Bind{model->busy}};      // two ways, named
    ToggleSwitch{Bind{model->busy}};             // two ways, by the data's type
    ComboBox{selectedIndex = Bind{model->row}};
}

// Copy construction must not be hijacked by the variadic constructor -- the
// single-argument case is what the constraint on it exists for.
[[maybe_unused]] void copying(Button const& source) {
    Button copy = source;
    Button moved{Button{}};
    (void)copy;
    (void)moved;
}

// The other side of that same constraint: a bare string is the whole of the
// argument list. It is a pointer once it decays, so it sits one step away
// from the exclusion the copy constructor needs, and only the pointed-to
// type tells the two apart.
[[maybe_unused]] void singleUnnamedString() {
    TextBlock caption{L"the only argument"};
    Button action{L"Click"};
    (void)caption;
    (void)action;
}

// A window's geometry is not on the window: Microsoft.UI.Xaml.Window exposes
// none of it and hands out an AppWindow, which is where the whole surface is.
[[maybe_unused]] void windowGeometry(Window const& window) {
    AppWindow const app = window.appWindow();
    app.resize({1000, 700});
    app.move({120, 80});
    app.moveAndResize({120, 80, 1000, 700});
    app.title(L"wxl");
    SizeInt32 const size = app.size();
    (void)size;
}

// Timing and the UI thread, both reached through the window's dispatcher.
[[maybe_unused]] void dispatching(Window const& window) {
    DispatcherQueue const queue = window.dispatcherQueue();
    if (!queue.hasThreadAccess()) {
        return;
    }
    DispatcherQueueTimer const timer = queue.createTimer();
    timer.interval(std::chrono::milliseconds{250});
    timer.isRepeating(true);

    // Tick carries no arguments of its own, so the handler is handed a plain
    // wxl::Object -- the same bridge out of WinRT the sender comes through.
    EventToken const tick = timer.add_onTick([](Object const&, Object const&) {});
    timer.remove_onTick(tick);

    timer.start();
    timer.stop();

    // Work handed to the queue runs after this call returns, so the delegate
    // keeps a copy of the callable rather than a reference to it.
    queue.tryEnqueue([] {});
    queue.tryEnqueue(DispatcherQueuePriority::Low, [] {});

    queue.enqueueEventLoopExit();
}

// The procedural side of the same members: ordinary calls, all const.
[[maybe_unused]] void procedural(TextBlock const& text, Button const& button) {
    text.text(L"changed");
    wstring const current = text.text();
    button.content(L"Click");
    EventToken const token = button.add_onClick([](Object const&, RoutedEventArgs&) {});
    button.remove_onClick(token);
    (void)current;
}

// What a handler may read out of its args, and write back into them.
//
// The view is handed over mutably on purpose -- Handled is the whole reason --
// and it is the one hierarchy in wxl whose const-ness says something. A
// wrapper of the Object family is a smart pointer, so all of its members are
// const and the qualifier means nothing; an args view *is* the object, on the
// stack for the length of the call, so reading a property is const and
// changing one is not.
[[maybe_unused]] void event_arguments(Button const& button, ListView const& list) {
    button.add_onClick([](Object const&, RoutedEventArgs& args) {
        Object const source = args.originalSource();
        (void)source;
    });

    button.add_onKeyDown([](Object const&, KeyRoutedEventArgs& args) {
        if (args.key() == VirtualKey::Enter) {
            args.handled(true);
        }
    });

    list.add_onSelectionChanged([](Object const&, SelectionChangedEventArgs& args) {
        Collection<Object> const added = args.addedItems();
        for (uint32_t i = 0; i != added.size(); ++i) {
            Object const item = added[i];
            (void)item;
        }
    });

    button.add_onKeyDown([](Object const&, KeyRoutedEventArgs const& args) {
        // A getter reaches through const, which is what lets a handler take
        // the args by const reference when it only reads them.
        if (args.key() == VirtualKey::Enter && !args.handled()) {
        }
    });
}

// And the line itself, as the member pointers say it: a property getter is
// const, a property setter is not, and neither is a method -- GetCurrentPoint
// is a call the args make on the pointer, not a property of theirs. Written as
// exact types rather than as `requires` because a member call rejected for the
// const-ness of `this` is not a substitution failure in MSVC but an error.
static_assert(std::is_same_v<decltype(&KeyRoutedEventArgs::key),
                             VirtualKey (KeyRoutedEventArgs::*)() const>);

[[maybe_unused]] constexpr auto args_are_read_through_const =
    static_cast<bool (KeyRoutedEventArgs::*)() const>(&KeyRoutedEventArgs::handled);

[[maybe_unused]] constexpr auto args_are_written_through_non_const =
    static_cast<void (KeyRoutedEventArgs::*)(bool)>(&KeyRoutedEventArgs::handled);

static_assert(std::is_same_v<decltype(&PointerRoutedEventArgs::getCurrentPoint),
                             PointerPoint (PointerRoutedEventArgs::*)(UIElement const&)>);

// A wrapper that may be empty is a type of its own, so a definite one never
// has to be tested: Style means a style, core::nullable<Style> means a style
// or nothing, and only the second can be written as nullptr -- the empty
// wrapper is a null thing, and its sentinel says so (empty_is_nullptr).
[[maybe_unused]] void nullability(Style const& definite) {
    core::nullable<Style> maybe = nullptr;
    maybe = definite;
    if (maybe) {
        Style const& back = maybe.value();
        (void)back;
    }
    core::nullable<DependencyObject> widened = maybe;
    (void)widened;

    maybe = nullptr;

    // And the empty wrapper costs nothing beside the wrapper: the sentinel is
    // a null Impl, which the pointer already had room for.
    static_assert(sizeof(core::nullable<Style>) == sizeof(Style));

    // A value with no null thing in it gets no such spelling, so no type ends
    // up with two ways to say empty.
    static_assert(!std::is_constructible_v<core::nullable<core::duration>, decltype(nullptr)>);
}

// The nullable box on the way through the projection. Six element types cross
// as WinRT's IReference<T>, and five of them say "empty" in a value of their
// own rather than in a byte beside it -- so the property costs exactly what
// the bare value costs.
//
// bool is the sixth and the exception, because both of its values are meant
// and a third bit pattern read back as bool is undefined. Its selector names
// std::optional and says so, which is the whole reason every other line in
// wxl can write nullable<T> without asking: a type nobody has given a
// sentinel still does not compile.
[[maybe_unused]] void projected_nullables(ToggleButton const& toggle, ScrollViewer const& scroll,
                                          TimePicker const& time, DatePicker const& date) {
    toggle.isChecked(true);
    toggle.isChecked({});
    core::nullable<bool> const checked = toggle.isChecked();
    static_assert(std::is_same_v<std::remove_const_t<decltype(checked)>, std::optional<bool>>);

    // A coordinate that is not being changed is a NaN, which is what
    // arithmetic already answers where there is no answer.
    (void)scroll.changeView(120.0, {}, {});
    static_assert(sizeof(core::nullable<double>) == sizeof(double));
    static_assert(sizeof(core::nullable<float>) == sizeof(float));

    // A length of time empties into the maximum it never counts to; a point
    // in time into WinRT's own zero, the year 1601, which no picker offers.
    time.selectedTime(core::duration{std::chrono::hours{9}});
    core::nullable<DateTime> const picked = date.selectedDate();
    static_assert(sizeof(core::nullable<core::duration>) == sizeof(core::duration));
    static_assert(sizeof(core::nullable<DateTime>) == sizeof(DateTime));
    (void)picked;

    // A signed integer empties into its minimum and an unsigned one into its
    // maximum: the value at each end that no count arrives at meaning it.
    static_assert(sizeof(core::nullable<int32_t>) == sizeof(int32_t));
    static_assert(sizeof(core::nullable<std::size_t>) == sizeof(std::size_t));
    static_assert(!core::nullable<int32_t>{}.has_value());
    static_assert(!core::nullable<std::size_t>{}.has_value());
    static_assert(!core::nullable<double>{}.has_value());
}


// The composition surface, which the rich profile added and which is where
// an application puts drawing of its own.
//
// It is here for one reason beyond the syntax: Microsoft.UI.Composition is
// the first namespace whose metadata contains a cycle running *through
// inheritance* -- CompositionObject::startAnimation takes a
// CompositionAnimation, which derives from CompositionObject. A closure
// ordered without regard for that lands the base class in the file after
// its heirs, and this function is what stops compiling if it ever does
// again.
//
// The numerics are the second thing under test: Vector3 and Vector2 are wxl
// types, not cppwinrt's float3/float2, and a braced literal is the whole of
// writing one.
[[maybe_unused]] void composition(Compositor const& compositor, UIElement const& host) {
    ContainerVisual root = compositor.createContainerVisual();
    root.size({640, 480});

    SpriteVisual sheet = compositor.createSpriteVisual();
    sheet.size({420, 600});
    sheet.offset({40, 20, 0});
    sheet.centerPoint({0, 300, 0});
    sheet.rotationAngleInDegrees(-12.0f);
    sheet.opacity(0.0f);
    sheet.brush(compositor.createColorBrush(Color{0xFF, 0xF4, 0xEC, 0xD8}));

    // The crop is applied in the sheet's own coordinates, before its
    // rotation -- which is what makes a turned page a turned crop.
    InsetClip crop = compositor.createInsetClip();
    crop.rightInset(60.0f);
    sheet.clip(crop);

    DropShadow shadow = compositor.createDropShadow();
    shadow.blurRadius(24.0f);
    shadow.opacity(0.35f);
    shadow.offset({-8, 4, 0});
    sheet.shadow(shadow);

    // The sheen: a gradient whose opacity is animated rather than redrawn.
    CompositionLinearGradientBrush sheen = compositor.createLinearGradientBrush();
    sheen.startPoint({0, 0});
    sheen.endPoint({1, 0});

    root.children().insertAtTop(sheet);

    // Fading in with a stagger: the same pair of animations on every child,
    // the delay equal to its index times a step. The visual XAML already
    // made for the host is what an animation is started on.
    ScalarKeyFrameAnimation fade = compositor.createScalarKeyFrameAnimation();
    fade.duration(std::chrono::milliseconds{200});
    fade.delayTime(std::chrono::milliseconds{80});
    fade.insertKeyFrame(1.0f, 1.0f, compositor.createLinearEasingFunction());

    Vector3KeyFrameAnimation rise = compositor.createVector3KeyFrameAnimation();
    rise.duration(std::chrono::milliseconds{200});
    rise.insertKeyFrame(0.0f, Vector3{0, 12, 0});
    rise.insertKeyFrame(1.0f, Vector3{0, 0, 0});

    sheet.startAnimation(L"Opacity", fade);
    sheet.startAnimation(L"Offset", rise);

    Visual hosted = ElementCompositionPreview::getElementVisual(host);
    hosted.opacity(1.0f);
    ElementCompositionPreview::setElementChildVisual(host, root);
}

// The application's own Direct2D on a surface the compositor shows.
//
// Everything below the last line is interop -- a D3D11 device, a D2D device,
// ICompositorInterop, BeginDraw -- and none of it appears here, which is the
// point: what reaches an application is a size in pixels, a callback holding
// a device context, and a brush.
//
// ID2D1DeviceContext is only forward-declared by DrawingSurface.h, so this
// translation unit names the pointer without parsing Direct2D at all.
void own_drawing(Compositor const& compositor, FrameworkElement const& host, XamlRoot const& root) {
    auto const scale = static_cast<float>(root.rasterizationScale());
    auto const width = static_cast<float>(host.actualWidth());
    auto const height = static_cast<float>(host.actualHeight());
    SizeInt32 const wanted{static_cast<int32_t>(width * scale),
                           static_cast<int32_t>(height * scale)};

    DrawingSurface page{compositor, wanted};
    page.resize(wanted);
    SizeInt32 const pixels = page.size();
    (void)pixels;

    page.draw([](ID2D1DeviceContext*) {});

    SpriteVisual sprite = compositor.createSpriteVisual();
    sprite.brush(page.brush());
    sprite.size({width, height});
    ElementCompositionPreview::setElementChildVisual(host, sprite);
}

// Work handed to the UI thread, both ways round.
//
// tryEnqueue is the generated one: a delegate parameter is projected as a
// std::function of the delegate's own signature, so a lambda goes in
// unwrapped. It may only be called on the UI thread, because the wrapper it
// is called on may only be touched there -- which is what UiThread is for:
// constructed here, copied into whatever runs elsewhere, called from there.
void handing_work_over(DispatcherQueue const& queue) {
    bool const taken = queue.tryEnqueue([] {});
    (void)taken;
    bool const soon = queue.tryEnqueue(DispatcherQueuePriority::Low, [] {});
    (void)soon;

    UiThread const ui{queue};
    auto const background = [ui](std::wstring found) {
        ui.post([found = std::move(found)] { (void)found; });
    };
    background(L"a book");
}

// A class that is only its statics derives from Statics, not from Object.
//
// WinRT gives it System.Object as its base like every other class, and
// reading that off the metadata unchanged used to make it a wrapper: a smart
// pointer that is always null, over an Impl field that is never filled, with
// a constructor nothing can call. Nothing was wrong at runtime -- an instance
// cannot exist -- but the type said otherwise, and a core::nullable of it
// would have compiled and meant nothing.
static_assert(std::is_base_of_v<Statics, ElementCompositionPreview>,
              "a statics-only class must say so by deriving from Statics");
static_assert(!std::is_base_of_v<Object, ElementCompositionPreview>,
              "a statics-only class must not be given Object as a base");
static_assert(!std::is_default_constructible_v<ElementCompositionPreview>,
              "a statics-only class must not be constructible");

// A constructor that takes arguments. WinRT declares those on a factory
// interface rather than on the class, and reading every ActivatableAttribute
// -- not only the bare one -- is what turns them into constructors of their
// own, beside the default constructor when metadata declares one too.
[[maybe_unused]] void constructors_taking_arguments(Collection<GradientStop> const& stops) {
    LinearGradientBrush ramp{stops, 45.0};

    // An interface-typed parameter is taken as Object and asked for the
    // interface at the call, so any wrapper is accepted here and it is the
    // runtime that refuses one not implementing it. A parameter only: the
    // same property's getter would hand back an ICommand, which is not a
    // class wxl wraps, so `command` keeps its setter and has no getter at
    // all.
    Button{}.command(ramp);
}

// The library looks are Borders of their own, and the look goes on ahead of
// the caller's setters, so anything in it is a default the page can argue
// with -- here the face, for a card that lies over artwork and must not
// drown it. A card passes wherever its base does, and one written with a
// single child claims the positional route Border already has.
[[maybe_unused]] void library_looks(UIElement const& content) {
    Card plain{content};
    OverlayCard over_picture{content};

    Card over_artwork{
        hAlign.right,
        vAlign.top,
        Margin{0, 64, 72, 0},
        background = SolidColorBrush{ARGB{0x6C1C1208}},
        content,
    };

    // The copy guard in setter_pack: one argument of the look's own type is
    // a copy, not a setter pack of one.
    Card const copied{plain};
    Border const& asBorder = copied;
    static_assert(std::derived_from<Card, Border>);
    (void)asBorder;

    // And a look is storable, not merely spendable: a screen keeps the card
    // it built the way it keeps any other control. The wrapper sentinel
    // builds the empty state through a maker derived from the look, which
    // hands its own base a null Impl -- so this line is the whole test of
    // the Impl constructor a hand-written control needs.
    core::nullable<OverlayCard> kept = nullptr;
    kept = over_picture;
    core::nullable<Border> const widened = kept;
    (void)widened;
}

// The two spellings of a brush resource, and the difference between them.
// The path follows the element's theme -- the setter keeps the key and
// subscribes -- while the call form reads one named theme and pins it there,
// which is what ink over a scrim that is dark in both themes needs. A literal
// brush is a literal brush and follows nothing.
[[maybe_unused]] void themed_brushes() {
    Border follows{background = brushes.Card.BackgroundFillColor.Default};
    TextBlock pinned{foreground = brushes.Text.FillColor.Primary(ElementTheme::Dark)};
    Border literal{background = SolidColorBrush{ARGB{0x6C1A1A1A}}};

    (void)follows;
    (void)pinned;
    (void)literal;
}

// A colour written for a brush is a solid brush of that colour -- in the
// syntax only: the setter itself still takes a brush and nothing else.
inline constexpr Preset tinted{background = colors.white, borderBrush = Color{255, 0, 0, 0}};

[[maybe_unused]] void colours_for_brushes() {
    Border scrim{background = ARGB{0xA0E0E0D0}, tinted};
    TextBlock ink{foreground = ARGB{0xFF2C3A1C}};
    Button swatch{borderBrush = colors.gray, background = Color{255, 0, 0, 0}};
    Apply{scrim, background = colors.transparent};

    (void)ink;
    (void)swatch;
}

template <typename Value>
concept background_takes = requires(Border const& border, Value value) { border.background(value); };

static_assert(!std::is_convertible_v<Color, Brush>);
static_assert(!background_takes<Color> && !background_takes<ARGB>);

// A hand-written wrapper is read from a sender like any generated control.
// EventHandler adapts a handler that names the concrete type, and it does so
// through try_as, which builds the wrapper from an Impl -- so the protected
// constructor these four declare is what this compiles into existence. A
// handler that could not name its own type would have to capture the control,
// which is the one thing a wxl handler must not do.
[[maybe_unused]] void hand_written_looks_as_senders() {
    Card{}.add_onActualThemeChanged([](Card const& self, Object const&) { self.padding({8}); });
    OverlayCard{}.add_onActualThemeChanged(
        [](OverlayCard const& self, Object const&) { self.padding({8}); });
    Rows{}.add_onActualThemeChanged([](Rows const& self, Object const&) { self.rowSpacing(4); });
    Columns{}.add_onActualThemeChanged(
        [](Columns const& self, Object const&) { self.columnSpacing(4); });
}




// Dressing an object that already exists. The runtime form of a description,
// written the way a description is: same braces, same commas, target first.
// Every route the constructor has is here, so a change that drops one of them
// from Apply -- or lets something other than a control stand first -- breaks
// this rather than an application.
[[maybe_unused]] void apply_dresses_what_already_exists(Button const& button,
                                                        StackPanel const& panel) {
    Apply {button, content = L"Switch", isEnabled = false};
    Apply {button, Margin {16, 0}, Padding {3}};
    Apply {button, styles.ButtonBase.TextBlockButton};
    Apply {button, onClick = [](Button const& self) { self.isEnabled(false); }};
    Apply {button, Preset{content = L"Switch"}};
    Apply {panel, TextBlock {L"a child goes in the same way"}};

    // A pack of one, and a pack of none: both legal, neither special.
    Apply {button, content = L"Switch"};
    Apply {button};

    static_assert(!std::is_constructible_v<Apply, decltype(content = L"a")>,
                  "the first argument is the target, and only a control may stand there");
}

// The comma is not a way to write two setters. Written in parentheses it is
// the built-in comma operator, which keeps only the last -- so wxl deletes it
// for anything that takes part in a description (see impl/member.h). What can
// silently regress is not the deletion, which is one line, but the list it
// works from: a family added later and not enrolled would quietly lose the
// guard. So these assert the enrolment, family by family.
//
// Asserting the deletion itself would be better still, and is not possible
// here: MSVC reports a deleted operator as a hard error rather than letting a
// requires-expression turn it into false.
// A brush of the framework's own, replaced for one control. What is proved
// here is the route: it is an unnamed argument like any other, so it goes into
// a description, into a preset, and into an Apply without any of the three
// knowing about it -- and it is refused on an object that has no resources of
// its own, which is everything that is not a FrameworkElement.
[[maybe_unused]] void theme_brushes_go_in_the_braces() {
    Button{
        content = L"Go",
        background = SolidColorBrush{ARGB{0xFF101215}},
        ThemeBrush{L"ButtonBackgroundPointerOver", SolidColorBrush{ARGB{0xFF1B1F25}}},
    };

    auto const look = Preset{
        ThemeBrush{L"ButtonBackgroundPressed", SolidColorBrush{ARGB{0xFF090A0C}}},
    };

    Button const button{look};

    Apply{button, ThemeBrush{L"ButtonForegroundPointerOver", SolidColorBrush{ARGB{0xFFFFFFFF}}}};

    static_assert(!std::invocable<ThemeBrush, SolidColorBrush const&>);
}

[[maybe_unused]] void the_comma_is_not_a_separator() {
    static_assert(impl::describes<std::remove_cvref_t<decltype(content = L"a")>>);
    static_assert(impl::describes<std::remove_cvref_t<decltype(onClick = [](Button const&) {})>>);
    static_assert(impl::describes<std::remove_cvref_t<decltype(content)>>);
    static_assert(impl::describes<std::remove_cvref_t<decltype(onClick)>>);
    static_assert(impl::describes<Margin>);
    static_assert(impl::describes<std::remove_cvref_t<decltype(styles.TextBlock.Header)>>);
    static_assert(impl::describes<std::remove_cvref_t<decltype(brushes.Card.StrokeColorDefault)>>);
    static_assert(impl::describes<Preset<>>);
    static_assert(impl::describes<decltype(Preset{content = L"a"})>);
    static_assert(impl::describes<Template<Button>>);
    static_assert(impl::describes<ThemeBrush>);

    // And what it deliberately does not reach. A control is an ordinary object,
    // and so is an enum shortcut like hAlign.center -- poisoning the comma for
    // either would reach far outside the DSL. Neither is silent anyway: the
    // comma yields something that is not callable and not a setter, so the
    // mistake surfaces at the very next step.
    static_assert(!impl::describes<Button>);
    static_assert(!impl::describes<std::remove_cvref_t<decltype(hAlign.center)>>);
}

// The schema, written the way it is meant to be read.
//
// Every element of it is compiled by the generated schema_surface.cpp beside
// this file; what is here is the part a generator cannot write down -- what
// it is *for*, and the one thing it can do that the flat vocabulary cannot.
[[maybe_unused]] void the_schema_names_the_class(LinearGradientBrush const& brush) {
    namespace x = dsl::schema;

    // Two classes declare StartPoint with different types, so the flat tag
    // has none and `startPoint = {0, 0}` cannot compile through it: a braced
    // list initialises a parameter of a known type, and template deduction
    // never gives it one. Through the class the type is known again.
    impl::apply_argument(brush, x::LinearGradientBrush::startPoint = {0.0f, 0.0f});
    impl::apply_argument(brush, x::LinearGradientBrush::endPoint = Point{1.0f, 1.0f});

    // And it is the same op the bare name produces, so the two forms mix
    // inside one set of braces.
    Button {
        x::Button::flyout = MenuFlyout{},
        x::ContentControl::content = L"Click me",
        content = L"and the flat name still means the same thing",
        x::Button::onClick = [](Object const&, RoutedEventArgs&) {},
    };
}

// A handler may take the sender alone. Most events carry args nobody reads, and
// the parameter written only to be ignored is what this form removes -- both
// spellings are here so neither disappears, and so the two concepts behind them
// stay disjoint (a change that let one swallow the other would break this).
[[maybe_unused]] void handlers_may_drop_the_args() {
    Button{
        content = L"Switch",
        onClick = [](Button const& self) { self.isEnabled(false); },
    };

    Button{
        content = L"Switch",
        onClick = [](Button const& self, RoutedEventArgs&) { self.isEnabled(false); },
    };

    ToggleSwitch{}.add_onToggled([](ToggleSwitch const& self) { self.isOn(false); });
}

// And drop both, when the handler carries what it needs. The house callback is
// what makes that worth writing: `type` is one shared body, so each of these
// buttons captures a reference count rather than a copy of the closure.
[[maybe_unused]] void handlers_may_take_nothing_at_all() {
    core::function const type = [](wchar_t) {};

    Grid{
        [type](iterate<L"789"> key) {
            return Button{
                key.text(),
                column = key.index,
                onClick = [type, symbol = key.value] { type(symbol); },
            };
        },
    };

    Button{}.add_onClick([] {});
}

// Tabs at the bottom of a window and the dialog that answers by events: both
// arrived for the forum client, and both are here so that a change to the
// profile or to showDialog's signature stops this translation unit rather
// than an application.
[[maybe_unused]] void tabs_and_dialogs(Window const& window) {
    SelectorBar tabs{
        hAlign.center,
        onSelectionChanged = [](Object const&, SelectorBarSelectionChangedEventArgs&) {},
        SelectorBarItem{L"Forums", icon = FontIcon{glyph = L""}},
        SelectorBarItem{L"Watched", icon = FontIcon{glyph = L""}},
    };

    tabs.selectedItem(tabs.items()[0]);

    ContentDialog about{
        closeButtonText = L"Close",
        content = TextBlock{L"Nothing to say yet"},
    };

    showDialog(about, window);
}

// One property, two vocabularies. SymbolIcon already knows the font, so a
// glyph is named and nothing else is said -- with WinRT's Symbol where the
// name is in it, and with wxl's FluentSymbol where it is not. QuietHours and
// Brightness are exactly that case: the moon and the sun of a theme switch,
// which Symbol never named.
[[maybe_unused]] void icons_by_name() {
    SymbolIcon fromWinrt{symbol = Symbol::Play};
    SymbolIcon fromFluent{symbol = FluentSymbol::QuietHours};

    // The positional route is Symbol's alone: a route has to be unambiguous,
    // and the property already carries the name in the assigned form.
    SymbolIcon positional{Symbol::Play};

    fromWinrt.symbol(Symbol::Pause);
    fromFluent.symbol(FluentSymbol::Brightness);

    // Asking gives back the vocabulary the metadata declares, whichever one
    // set it: there is one property under both spellings.
    static_assert(std::is_same_v<decltype(fromFluent.symbol()), Symbol>);
}

// The pointer's shape over an element is a property like any other -- set
// declaratively on a plain Border, no control subclass in between. It stands
// on UIElement.ProtectedCursor, "protected" in the metadata and reachable all
// the same: see impl/cursor.h.
[[maybe_unused]] void cursor_over_an_element() {
    Border sizer{width = 6, cursor = InputSystemCursorShape::SizeWestEast};

    sizer.cursor(InputSystemCursorShape::Arrow);
}

static_assert(std::is_constructible_v<LinearGradientBrush, Collection<GradientStop> const&, double>,
              "the factory interface's CreateInstanceWithGradientStopCollectionAndAngle "
              "is a constructor of its own");
static_assert(std::is_constructible_v<SolidColorBrush, Color const&>,
              "and so is the one-argument shape");
static_assert(!std::is_convertible_v<Color const&, SolidColorBrush>,
              "both are explicit, like every other wrapper constructor");

// The shape of Color itself: the ABI struct is { A, R, G, B } bytes in that
// order, and every cast across the boundary reads it that way. Asserted here
// rather than in Color.h, which half of wxl.ui includes.
static_assert(sizeof(Color) == 4);
static_assert(alignof(Color) == 1);
static_assert(offsetof(Color, A) == 0 && offsetof(Color, R) == 1 && offsetof(Color, G) == 2
              && offsetof(Color, B) == 3);

// CSS hex puts alpha last, so it is RGBA and never ARGB.
static_assert(RGBA{"#dff9f9d8"} == ARGB{0xD8DFF9F9});
static_assert(RGBA{"#102030"} == ARGB{0xFF102030});
static_assert(RGBA{"#aBc"} == ARGB{0xFFAABBCC});
static_assert(RGBA{"#1234"} == ARGB{0x44112233});
static_assert(!std::is_constructible_v<ARGB, char const (&)[10]>);

[[maybe_unused]] void colour_literals() {
    Border hex{background = RGBA{"#dff9f9d8"}};
    (void)hex;
}

// What a string parameter takes. Every spelling of UTF-16 the code has --
// and the wxl::wstring a getter hands back, so a value read off one control
// goes straight into another without naming a unit at the call site.
static_assert(std::is_convertible_v<wchar_t const*, string_param>);
static_assert(std::is_convertible_v<char16_t const*, string_param>);
static_assert(std::is_convertible_v<std::wstring_view, string_param>);
static_assert(std::is_convertible_v<std::u16string_view, string_param>);
static_assert(std::is_convertible_v<std::wstring const&, string_param>);
// And checked text, straight: a model's u16_text reaches a control's setter
// without a step back through units.
static_assert(std::is_convertible_v<core::u16_view, string_param>);
static_assert(std::is_convertible_v<core::u16_text const&, string_param>);
static_assert(std::is_convertible_v<std::u16string const&, string_param>);
static_assert(std::is_convertible_v<wstring const&, string_param>);

// And nothing implicit going back: a parameter that decayed into a wchar_t
// view would be a silent way back to the unit this type exists to leave.
static_assert(!std::is_convertible_v<string_param, std::wstring_view>);

// The char16_t half is a constant expression, so what it carries can be read
// here. The wchar_t half cannot be -- the rename is a reinterpret_cast, which
// constant evaluation never enters -- and is checked in
// test/string_param_test.cpp instead.
static_assert(string_param{u"text"}.text() == std::u16string_view{u"text"});

// The rename itself is only sound while the two units are the same width.
// MSVC gives that; the standard promises it nowhere, which is the whole
// reason the code is moving off wchar_t.
static_assert(sizeof(wchar_t) == sizeof(char16_t));

// Awaiting an event. A wait is named by what makes it rather than spelled
// out: the type carries how it reaches its event, and that is the factory's
// business, not the caller's.
using click_wait_t = decltype(on_event<EventKey::Click>(std::declval<Button const&>()));
using split_click_wait_t = decltype(on_event<EventKey::Click>(std::declval<SplitButton const&>()));
using layout_wait_t = decltype(on_event<EventKey::LayoutUpdated>(std::declval<TextBlock const&>()));

// The args are read off the signature of the element's own add_onX, and the
// pair below is why that is not the same as reading them off the event: one
// EventKey, two classes, two different args. A subscription made from a key
// has to land on the right one of them.
static_assert(std::is_same_v<click_wait_t::args_t, RoutedEventArgs>);
static_assert(std::is_same_v<split_click_wait_t::args_t,
                             SplitButtonClickEventArgs>);

// And they reach the coroutine exactly as they reach a handler: event args
// proper by non-const reference, since they are a non-copyable view the
// runtime owns; the events whose args are an ordinary object by const
// reference, since that one is a wrapper the coroutine must not replace.
static_assert(
    std::is_same_v<click_wait_t::args_ref_t, RoutedEventArgs&>);
static_assert(
    std::is_same_v<layout_wait_t::args_ref_t, Object const&>);

// Neither copied nor moved: the handler it hands the element captures it by
// address, so a proxy that could travel would leave that handler pointing at
// where it used to be. on() still returns one by value -- a prvalue
// initialises the variable in place.
static_assert(!std::is_copy_constructible_v<click_wait_t>);
static_assert(!std::is_move_constructible_v<click_wait_t>);
static_assert(std::is_same_v<decltype(on_event<EventKey::Click>(std::declval<Button const&>())),
                             click_wait_t>);

// The two forms differ in one thing and say so in their result: the bare wait
// hands back the args, the answering one hands back either the args or the
// reason there are none. A reference cannot be the value of an expected, so
// it travels wrapped; the error side is a nullable exception_ptr, whose empty
// state is a null pointer and therefore costs nothing.
using key_wait_t = decltype(on_event<EventKey::KeyDown>(std::declval<Button const&>()));

static_assert(std::is_same_v<decltype(std::declval<key_wait_t&>().operator co_await().await_resume()),
                             KeyRoutedEventArgs&>);
static_assert(std::is_same_v<decltype(std::declval<key_wait_t&>().next().await_resume()),
                             std::expected<std::reference_wrapper<KeyRoutedEventArgs>,
                                           core::nullable<std::exception_ptr>>>);

// And the empty error state really is free: a null exception_ptr is what
// "cancelled, and that is all there is to say" is spelled as.
static_assert(sizeof(core::nullable<std::exception_ptr>) == sizeof(std::exception_ptr));

// Awaiting writes down who is waiting, so the proxy has to be non-const.
// Only the positive half is assertable: the const overload is deleted rather
// than absent, and MSVC reports naming a deleted function as an error where
// it stands instead of letting a requires-expression answer false.
static_assert(
    requires(click_wait_t& proxy) { proxy.operator co_await(); });

// The protocol itself, proven the only way it can be: by a coroutine. Never
// called -- activating a control needs a runtime that is not up here -- and
// compiled, which is the whole of what this file does.
struct probe_task {
    struct promise_type {
        probe_task get_return_object() const noexcept { return {}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };
};

[[maybe_unused]] probe_task awaiting_an_event(Button button, TextBlock text) {
    // Kept in a variable: one subscription, waited on as many times as the
    // loop goes round.
    auto clicks = on_event<EventKey::Click>(button);

    while (true) {
        RoutedEventArgs& args = co_await clicks;
        static_cast<void>(args);

        // Answering the framework from inside a coroutine, which is the one
        // thing this whole shape has to keep possible: the handler resumes
        // the frame before it returns, so Handled set here is set in time.
        KeyRoutedEventArgs& key = co_await on_event<EventKey::KeyDown>(button);
        key.handled(true);

        // And as a temporary: one wait, subscribed and unsubscribed by the
        // full expression it is written in. The temporary outlives the
        // suspension inside it, which is what makes this legal.
        Object const& sender = co_await on_event<EventKey::LayoutUpdated>(text);
        static_cast<void>(sender);

        // The same wait named by its own two members instead of by a key --
        // for an event the generated vocabulary does not name, and for the
        // events of a type, which have no object to be asked of.
        RoutedEventArgs& again =
            co_await on_event(button, &Button::add_onClick, &Button::remove_onClick);
        static_cast<void>(again);

        // The answering form, for a body that must let nothing escape. An
        // empty error side is a plain cancellation; a full one carries what
        // went wrong, so nothing is lost by not throwing.
        if (auto const got = co_await on_event<EventKey::KeyUp>(button).next()) {
            got->get().handled(true);
        } else if (got.error()) {
            std::rethrow_exception(*got.error());
        }
    }
}

// MagnifyEffect -- written by hand, so its schema lines are too.
[[maybe_unused]] void MagnifyEffect_scale_assigned(::wxl::MagnifyEffect const& object, ::wxl::Size value) {
    ::wxl::impl::apply_argument(object, ::wxl::dsl::schema::MagnifyEffect::scale = value);
}
[[maybe_unused]] void MagnifyEffect_scale_braced(::wxl::MagnifyEffect const& object) {
    ::wxl::impl::apply_argument(object, ::wxl::dsl::schema::MagnifyEffect::scale = {1.3, 1.1});
}
[[maybe_unused]] void MagnifyEffect_maximum_assigned(::wxl::MagnifyEffect const& object, double value) {
    ::wxl::impl::apply_argument(object, ::wxl::dsl::schema::MagnifyEffect::maximum = value);
}
[[maybe_unused]] void MagnifyEffect_minimum_assigned(::wxl::MagnifyEffect const& object, double value) {
    ::wxl::impl::apply_argument(object, ::wxl::dsl::schema::MagnifyEffect::minimum = value);
}
[[maybe_unused]] void MagnifyEffect_duration_assigned(::wxl::MagnifyEffect const& object, ::wxl::core::duration value) {
    ::wxl::impl::apply_argument(object, ::wxl::dsl::schema::MagnifyEffect::duration = value);
}
[[maybe_unused]] void MagnifyEffect_delayTime_assigned(::wxl::MagnifyEffect const& object, ::wxl::core::duration value) {
    ::wxl::impl::apply_argument(object, ::wxl::dsl::schema::MagnifyEffect::delayTime = value);
}

}  // namespace
