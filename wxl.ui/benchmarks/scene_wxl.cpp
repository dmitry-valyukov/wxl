// The scene of scene_shape.h, described the way wxl means it to be described:
// one expression, no element held in a variable, every property written
// inside the braces of the element it belongs to.
//
// Its twin is scene_winrt.cpp, which builds the same scene through the
// cppwinrt projection by hand. What the pair measures is the cost of bringing
// a window's worth of controls into existence -- activations, property
// writes, event subscriptions -- and nothing else: the texts are made before
// the stopwatch starts, and no theme resource is looked up.
//
// A console subsystem executable on purpose, unlike the samples: the window
// still opens, and the table still has somewhere to go.

// The benchmark's own headers first, and with them the standard headers they
// use: every wxl header carries the wxl.core import, and MSVC takes no
// textual standard header after that.
#include "scene_report.h"
#include "scene_shape.h"

#include "launch.h"
#include "ui.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// One card: a titled panel of ten controls. A function rather than the body
// of the repeat below, so that the twelve cards are twelve calls to one piece
// of compiled code -- which is what the loop on the winrt side is too.
Border card(int index, bench::CardText const& text) {
    return Border {
        row = index / bench::columns,
        column = index % bench::columns,
        background = SolidColorBrush {ARGB{bench::cardBackground}},
        borderBrush = SolidColorBrush {ARGB{bench::cardBorder}},
        BorderThickness {1},
        CornerRadius {8},
        Padding {10},

        StackPanel {
            spacing = 6.0,

            TextBlock {
                text.title,
                fontSize = 15.0,
                FontWeight {600},
                foreground = SolidColorBrush {ARGB{bench::titleInk}},
                Margin {0, 0, 0, 4},
            },
            Button {
                bench::actionA,
                hAlign.stretch,
                Padding {8, 4},
                onClick = [] {},
            },
            Button {
                bench::actionB,
                hAlign.stretch,
                Padding {8, 4},
                onClick = [] {},
            },
            CheckBox {
                bench::checkLabel,
                isChecked = index % 2 == 0,
                Margin {0},
            },
            RadioButton {
                bench::firstChoice,
                groupName = text.group,
                isChecked = true,
                Margin {0},
            },
            RadioButton {
                bench::secondChoice,
                groupName = text.group,
                Margin {0},
            },
            ToggleSwitch {
                header = bench::switchHeader,
                onContent = bench::switchOn,
                offContent = bench::switchOff,
                isOn = index % 3 == 0,
                onToggled = [] {},
            },
            Slider {
                minimum = 0.0,
                maximum = 100.0,
                value = bench::sliderValue(index),
                stepFrequency = 1.0,
            },
            ProgressBar {
                minimum = 0.0,
                maximum = 100.0,
                value = bench::progressValue(index),
                height = 6.0,
            },
            TextBlock {
                bench::caption,
                fontSize = 12.0,
                foreground = SolidColorBrush {ARGB{bench::captionInk}},
                textWrapping.wrap,
            },
        },
    };
}

// The whole page. The row and column definitions are written as the strings
// wxl parses, which is the spelling an application would use -- and which
// costs this side a parse the winrt side does not pay, since there the
// definitions are built as objects.
Grid page(std::array<bench::CardText, bench::cards> const& texts) {
    return Grid {
        rowDefinitions = u"auto,*",
        rowSpacing = 12.0,
        Padding {12},
        background = SolidColorBrush {ARGB{bench::pageBackground}},

        Border {
            row = 0,
            background = SolidColorBrush {ARGB{bench::headerBackground}},
            CornerRadius {6},
            Padding {12, 8},
            StackPanel {
                orientation.horizontal,
                spacing = 12.0,
                TextBlock {
                    bench::headerTitle,
                    fontSize = 20.0,
                    FontWeight {600},
                    foreground = SolidColorBrush {ARGB{bench::titleInk}},
                    vAlign.center,
                },
                TextBlock {
                    bench::headerNote,
                    fontSize = 13.0,
                    foreground = SolidColorBrush {ARGB{bench::captionInk}},
                    vAlign.center,
                },
            },
        },

        Grid {
            row = 1,
            columnDefinitions = bench::cardColumns,
            rowDefinitions = bench::cardRows,
            rowSpacing = 10.0,
            columnSpacing = 10.0,
            [&](repeat<bench::cards> i) { return card(i, texts[i]); },
        },
    };
}

// What the framework was actually given, asked of the framework rather than
// counted in the source: a benchmark that cannot show its work happened is
// measuring whatever was left of it, and a mismatch here is the one failure
// that would invalidate the comparison outright.
bench::tree_count walk(Grid const& root) {
    bench::tree_count counted;

    auto const top = root.children();
    counted.elements = 1 + static_cast<int>(top.size());

    if (auto const header = top[0].try_as<Border>()) {
        if (auto const strip = header.child().try_as<StackPanel>()) {
            counted.elements += 1 + static_cast<int>(strip.children().size());
        }
    }

    auto const grid = top[1].try_as<Grid>();
    if (!grid) {
        return counted;
    }

    auto const cards = grid.children();
    counted.cards = static_cast<int>(cards.size());

    for (uint32_t i = 0; i < cards.size(); ++i) {
        auto const border = cards[i].try_as<Border>();
        if (!border) {
            continue;
        }

        auto const panel = border.child().try_as<StackPanel>();
        if (!panel) {
            continue;
        }

        int const held = static_cast<int>(panel.children().size());
        counted.controls += held;
        counted.elements += held + 2;  // the card's Border and its StackPanel
    }

    return counted;
}

// ---- The second measurement: writing to a scene that already exists -------

// Handles into a built card. A harness holds them and a declarative
// description never does -- that is the rule, and this is not a description:
// writing to a scene that already exists is what is being measured, and there
// is nothing to write to without naming the elements first. Collected once,
// before the stopwatch starts.
struct CardHandles {
    Border card;
    TextBlock title;
    Button apply;
    Button reset;
    CheckBox check;
    RadioButton automatic;
    RadioButton manual;
    ToggleSwitch mode;
    Slider slider;
    ProgressBar progress;
    TextBlock caption;
};

std::vector<CardHandles> collect(Grid const& root) {
    std::vector<CardHandles> handles;
    handles.reserve(bench::cards);

    auto const cards = root.children()[1].try_as<Grid>().children();
    for (uint32_t i = 0; i < cards.size(); ++i) {
        auto const border = cards[i].try_as<Border>();
        auto const held = border.child().try_as<StackPanel>().children();
        handles.push_back({
            border,
            held[0].try_as<TextBlock>(),
            held[1].try_as<Button>(),
            held[2].try_as<Button>(),
            held[3].try_as<CheckBox>(),
            held[4].try_as<RadioButton>(),
            held[5].try_as<RadioButton>(),
            held[6].try_as<ToggleSwitch>(),
            held[7].try_as<Slider>(),
            held[8].try_as<ProgressBar>(),
            held[9].try_as<TextBlock>(),
        });
    }
    return handles;
}

// The brushes a pass hands out, made once: a fresh brush per write would put
// an activation back into the measurement, which is what this half of the
// benchmark exists to get out of it.
std::vector<SolidColorBrush> inkPalette() {
    std::vector<SolidColorBrush> palette;
    palette.reserve(bench::paletteSize);
    for (int i = 0; i < bench::paletteSize; ++i) {
        palette.push_back(SolidColorBrush{ARGB{bench::palette[i]}});
    }
    return palette;
}

void writeText(TextBlock const& block, bench::PassValues const& v, std::u16string_view word,
               Brush const& ink, Thickness const& edge) {
    block.text(word);
    block.fontSize(v.fontSize);
    block.foreground(ink);
    block.characterSpacing(v.spacing);
    block.maxLines(v.lines);
    block.opacity(v.opacity);
    block.margin(edge);
}

void writeButton(Button const& button, bench::PassValues const& v, std::u16string_view word,
                 Thickness const& edge) {
    button.content(word);
    button.isEnabled(v.flag);
    button.padding(edge);
    button.horizontalContentAlignment(HorizontalAlignment::Center);
    button.opacity(v.opacity);
    button.margin(edge);
}

// The three-state value, as wxl hands it over: `nullable<bool>` here, and on
// the way in one of the two objects of impl/value_box.h -- placed in the
// program's own storage, never made and never unmade. Nothing is allocated
// and no reference is counted; the empty state is a null pointer.
core::nullable<bool> checkState(int state) {
    return state == 2 ? core::nullable<bool>{} : core::nullable<bool>{state == 1};
}

void writeCheck(CheckBox const& check, bench::PassValues const& v, std::u16string_view word,
                Thickness const& edge) {
    check.isThreeState(true);
    check.isChecked(checkState(v.threeState));
    check.content(word);
    check.isEnabled(v.flag);
    check.opacity(v.opacity);
    check.margin(edge);
}

void writeRadio(RadioButton const& radio, bench::PassValues const& v, std::u16string_view word,
                Thickness const& edge) {
    radio.content(word);
    radio.groupName(word);
    radio.isEnabled(v.flag);
    radio.opacity(v.opacity);
    radio.margin(edge);
}

void pass(std::vector<CardHandles> const& handles, std::vector<SolidColorBrush> const& palette,
          int index) {
    auto const v = bench::valuesOf(index);
    auto const& ink = palette[v.ink];
    auto const& fill = palette[(v.ink + 1) % bench::paletteSize];
    auto const word = bench::labels[v.word];
    Thickness const edge{v.edge};
    CornerRadius const corner{v.edge};

    for (auto const& h : handles) {
        h.card.background(fill);
        h.card.borderBrush(ink);
        h.card.borderThickness(edge);
        h.card.cornerRadius(corner);
        h.card.padding(edge);

        writeText(h.title, v, word, ink, edge);
        writeText(h.caption, v, word, ink, edge);

        writeButton(h.apply, v, word, edge);
        writeButton(h.reset, v, word, edge);

        writeCheck(h.check, v, word, edge);

        writeRadio(h.automatic, v, word, edge);
        writeRadio(h.manual, v, word, edge);

        h.mode.isOn(v.flag);
        h.mode.onContent(word);
        h.mode.offContent(word);
        h.mode.header(word);
        h.mode.opacity(v.opacity);

        h.slider.minimum(0.0);
        h.slider.maximum(100.0);
        h.slider.value(v.value);
        h.slider.stepFrequency(1.0);
        h.slider.isEnabled(v.flag);

        h.progress.minimum(0.0);
        h.progress.maximum(100.0);
        h.progress.value(v.value);
        h.progress.height(v.edge);
    }
}

// One written property of every kind of element in a card, read back out of
// the framework and compared with what the last pass wrote. The count of
// writes is declared in scene_shape.h and is what the nanoseconds are divided
// by; this is what says those writes reached anything.
int verify(std::vector<CardHandles> const& handles) {
    auto const v = bench::valuesOf(bench::passes - 1);
    auto const& h = handles.back();
    auto const title = h.title.text();

    // The box comes back out unboxed, and the third state comes back as no
    // value at all -- which is the whole of what IReference<bool> means.
    auto const checked = h.check.isChecked();
    bool const stateHeld = v.threeState == 2
                               ? !checked.has_value()
                               : checked.has_value() && *checked == (v.threeState == 1);

    int passed = 0;
    auto const held = [&passed](char const* what, bool ok) {
        passed += ok;
        if (!ok) {
            std::printf("  !! %s does not hold what the last pass wrote\n", what);
        }
    };

    held("Border.cornerRadius", h.card.cornerRadius().topLeft == v.edge);
    held("Border.padding", h.card.padding().left == v.edge);
    held("TextBlock.fontSize", h.title.fontSize() == v.fontSize);
    held("TextBlock.text",
         std::u16string_view{title.data(), title.size()} == bench::labels[v.word]);
    held("TextBlock.characterSpacing", h.caption.characterSpacing() == v.spacing);
    held("Button.padding", h.apply.padding().left == v.edge);
    held("CheckBox.isEnabled", h.check.isEnabled() == v.flag);
    held("CheckBox.isChecked", stateHeld);
    // Through a float and back: the property is a double on both sides of the
    // ABI, and the composition visual under it keeps a float -- so 0.95 comes
    // back as 0.949999988. Everything else here round-trips exactly.
    held("RadioButton.opacity",
         static_cast<float>(h.automatic.opacity()) == static_cast<float>(v.opacity));
    held("ToggleSwitch.isOn", h.mode.isOn() == v.flag);
    held("Slider.value", h.slider.value() == v.value);
    return passed;
}

}  // namespace

wxl::Teardown wxl_launched() {
    // Before anything is timed: the two texts that differ from card to card.
    // Formatting a number is not what this benchmark is about, and neither
    // side should be caught doing it with the stopwatch running.
    auto const texts = bench::cardTexts();

    std::vector<double> runs;
    runs.reserve(bench::runs);

    // Each scene is let go as soon as its time is taken -- the destructor
    // runs when `built` leaves the block, which is after the clock is read.
    // The first run is the one a starting application actually pays: every
    // control type in the scene resolves its activation factory there.
    for (int run = 0; run < bench::runs; ++run) {
        auto const started = bench::clock::now();
        Grid const built = page(texts);
        auto const finished = bench::clock::now();
        runs.push_back(bench::milliseconds(finished - started));
    }

    // The second measurement, on a scene of its own so that the window is
    // left showing a scene nobody has written over. The tree, the handles
    // into it and the brushes a pass hands out are all made before the
    // stopwatch starts: what is timed is property writes and nothing else.
    std::vector<double> passTimes;
    passTimes.reserve(bench::passes);
    int checksPassed = 0;
    {
        Grid const target = page(texts);
        auto const handles = collect(target);
        auto const palette = inkPalette();

        for (int index = 0; index < bench::passes; ++index) {
            auto const started = bench::clock::now();
            pass(handles, palette, index);
            auto const finished = bench::clock::now();
            passTimes.push_back(bench::milliseconds(finished - started));
        }

        checksPassed = verify(handles);
    }

    // And once more for the window that stays on screen, timed the way the
    // question is asked: from nothing to a window whose content is set,
    // stopped on the line before activate().
    auto const started = bench::clock::now();
    auto const window = Window {
        title = bench::windowTitle,
        page(texts),
    };
    double const ready = bench::milliseconds(bench::clock::now() - started);

    bench::reportBuild("wxl -- the scene described declaratively", runs, ready,
                       walk(window.content().try_as<Grid>()), bench::elements, bench::events);
    bench::reportPass(passTimes, bench::writesPerPass, bench::writesPerCard, checksPassed,
                      bench::sampledChecks);

    auto const appWindow = window.appWindow();
    appWindow.resize({1560, 940});
    window.activate();

    // Capturing the window is what keeps it alive for the run.
    return [window](TeardownReason) {};
}
