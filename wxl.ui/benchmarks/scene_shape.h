#pragma once

// The scene both scene benchmarks build, written down once.
//
// Two applications answer the same question here -- what does it cost to
// bring a window's worth of controls into existence -- one through wxl's
// declarative syntax, one through the cppwinrt projection by hand. The answer
// means nothing unless the two build the *same* scene, so everything that
// decides what the scene is (how many cards, what sits in one, what the
// labels say, what the colours are) lives in this header and neither
// application invents any of it.
//
// What the two do *not* share is how the scene is described: that is the
// thing being measured, and each writes it the way an application of its kind
// would.
//
// Nothing here names a wxl or a winrt type. It is included before either
// projection, which for the wxl side also means before the wxl.core import.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace bench {

// ---- The layout ----------------------------------------------------------
//
// A header strip, and under it a grid of cards -- the shape of an ordinary
// settings page, which is the closest ordinary thing to "a window full of
// controls".

inline constexpr int columns = 6;
inline constexpr int rows = 2;
inline constexpr int cards = columns * rows;

// The same grid, in the spelling wxl parses -- the winrt side builds the
// definitions as objects instead, because it has no parser to write them for.
// Pinned to the counts above, so a change to one is a compile error until the
// other follows.
inline constexpr std::u16string_view cardColumns = u"*,*,*,*,*,*";
inline constexpr std::u16string_view cardRows = u"*,*";

constexpr int definitionCount(std::u16string_view text) {
    int counted = 1;
    for (char16_t unit : text) {
        if (unit == u',') {
            ++counted;
        }
    }
    return counted;
}

static_assert(definitionCount(cardColumns) == columns);
static_assert(definitionCount(cardRows) == rows);

// What one card holds, in order: a title, two buttons, a check box, two radio
// buttons, a switch, a slider, a progress bar and a caption.
inline constexpr int controlsPerCard = 10;

// Plus the card's own Border and the StackPanel inside it.
inline constexpr int elementsPerCard = controlsPerCard + 2;

// The root Grid, the header Border and its StackPanel, the two TextBlocks in
// it, and the Grid the cards sit in.
inline constexpr int fixedElements = 6;

inline constexpr int elements = cards * elementsPerCard + fixedElements;

// Two Click handlers and one Toggled handler per card: a delegate object each,
// which is part of what building a scene costs.
inline constexpr int eventsPerCard = 3;
inline constexpr int events = cards * eventsPerCard;

// How many times the scene is built. The first build is the one that matters
// for a starting application -- it pays for every activation factory the scene
// touches -- and the rest are what the same work costs once the process is
// warm, which is the only number stable enough to compare.
inline constexpr int runs = 25;

// ---- The second measurement: writing to a scene that already exists -------
//
// Building a scene is dominated by the framework's own work -- an activation,
// a property store, a control template -- and on that scale what a projection
// costs is small. A pass of property writes over a tree that already exists
// takes the framework's construction out of the picture and leaves the thing
// the two sides really differ in: how a property write reaches its interface.
// cppwinrt asks the object for that interface on every call; wxl asks once per
// object and keeps the answer.
//
// "On every call" is meant exactly: cppwinrt's property bodies reach their
// interface through try_as_with_reason, which is an unconditional
// QueryInterface -- there is no shortcut for the class's own default
// interface, because the object is held as the class and not as that
// interface. So a pass of N writes is N QueryInterface calls on that side and,
// after the first pass has filled the caches, none at all on wxl's.
//
// The writes are grouped the way a real update is all the same, several to an
// object and several of those to one interface, because that is what decides
// how much the *first* touch of an object costs wxl.

inline constexpr int writesPerBorder = 5;        // background, borderBrush, borderThickness,
                                                 // cornerRadius, padding -- all IBorder
inline constexpr int writesPerTextBlock = 7;     // 5 on ITextBlock, then opacity and margin
inline constexpr int writesPerButton = 6;        // 3 on IControl, content, opacity, margin
inline constexpr int writesPerCheckBox = 6;      // isThreeState and the three-state isChecked
                                                 // among them -- see threeState() below
inline constexpr int writesPerRadioButton = 5;
inline constexpr int writesPerToggleSwitch = 5;  // 4 on IToggleSwitch, then opacity
inline constexpr int writesPerSlider = 5;        // 3 on IRangeBase, stepFrequency, isEnabled
inline constexpr int writesPerProgressBar = 4;   // 3 on IRangeBase, then height

inline constexpr int writesPerCard = writesPerBorder + 2 * writesPerTextBlock +
                                     2 * writesPerButton + writesPerCheckBox +
                                     2 * writesPerRadioButton + writesPerToggleSwitch +
                                     writesPerSlider + writesPerProgressBar;

inline constexpr int writesPerPass = cards * writesPerCard;

inline constexpr int passes = 25;

// How many written properties each application reads back out of the
// framework after the last pass. The count above is the divisor of the
// nanoseconds-per-write figure and is declared, not measured; this is what
// says the writes actually landed -- one property of every kind of element in
// a card, compared against what the last pass wrote.
inline constexpr int sampledChecks = 11;

// ---- The colours ---------------------------------------------------------
//
// Named rather than looked up in the theme: a theme resource lookup is a
// dictionary walk whose cost belongs to the framework, not to either way of
// describing a scene, and the two projections would reach it through
// different machinery.

inline constexpr uint32_t pageBackground = 0xFF1B1B1F;
inline constexpr uint32_t headerBackground = 0xFF2B2B33;
inline constexpr uint32_t cardBackground = 0xFF26262C;
inline constexpr uint32_t cardBorder = 0xFF3A3A44;
inline constexpr uint32_t titleInk = 0xFFE8E8EE;
inline constexpr uint32_t captionInk = 0xFF9A9AA6;

// ---- The words -----------------------------------------------------------
//
// char16_t throughout: that is the unit an HSTRING is made of, and both sides
// reach it without converting anything -- wxl's string_param takes it as it
// is, and the winrt side renames the pointer, which is what the two spellings
// of UTF-16 differ by on Windows.

inline constexpr std::u16string_view windowTitle = u"wxl scene benchmark";
inline constexpr std::u16string_view headerTitle = u"Scene construction benchmark";
inline constexpr std::u16string_view headerNote =
    u"the same scene, built twice over: declaratively and by hand";

inline constexpr std::u16string_view actionA = u"Apply";
inline constexpr std::u16string_view actionB = u"Reset";
inline constexpr std::u16string_view checkLabel = u"Enabled";
inline constexpr std::u16string_view firstChoice = u"Automatic";
inline constexpr std::u16string_view secondChoice = u"Manual";
inline constexpr std::u16string_view switchHeader = u"Mode";
inline constexpr std::u16string_view switchOn = u"On";
inline constexpr std::u16string_view switchOff = u"Off";
inline constexpr std::u16string_view caption =
    u"Created, configured and wired to a handler.";

// The two texts that differ from card to card. They are built once, before
// any timing starts, so that what the stopwatch sees is the scene being
// built and never a number being formatted.
struct CardText {
    std::u16string title;
    std::u16string group;
};

inline std::array<CardText, cards> cardTexts() {
    // No std::to_u16string, and nothing here needs one: the indices are two
    // digits at most.
    auto const number = [](int value) {
        std::u16string text;
        if (value >= 10) {
            text.push_back(static_cast<char16_t>(u'0' + value / 10));
        }
        text.push_back(static_cast<char16_t>(u'0' + value % 10));
        return text;
    };

    std::array<CardText, cards> texts;
    for (int i = 0; i < cards; ++i) {
        texts[i].title = u"Group " + number(i + 1);
        texts[i].group = u"group" + number(i + 1);
    }
    return texts;
}

// What a card's slider and progress bar are set to. Values rather than a
// constant, so that neither compiler can fold twelve identical property
// writes into one.
inline constexpr double sliderValue(int index) { return 5.0 * index + 10.0; }
inline constexpr double progressValue(int index) { return 100.0 - 7.0 * index; }

// ---- What one property pass writes ---------------------------------------

inline constexpr int paletteSize = 4;
inline constexpr uint32_t palette[paletteSize] = {0xFFE8E8EE, 0xFF9AD0FF, 0xFFFFC38A, 0xFF9EE7B8};

inline constexpr int labelCount = 4;
inline constexpr std::u16string_view labels[labelCount] = {u"Alpha", u"Beta", u"Gamma", u"Delta"};

// The values one pass writes, all of them moving with the pass number: a
// property store is free to be quick about a value it already holds, and
// there is nothing to learn from that.
struct PassValues {
    double fontSize;
    double opacity;
    double edge;      // the thickness, the padding and the corner radius alike
    double value;     // what a slider and a bar are set to
    int32_t spacing;  // character spacing
    int32_t lines;    // maximum lines of a text block
    bool flag;        // on, enabled
    int threeState;   // 0 unchecked, 1 checked, 2 indeterminate -- see below
    int ink;          // which brush of the palette
    int word;         // which label of the set
};

inline constexpr PassValues valuesOf(int pass) {
    return {
        13.0 + pass % 5,
        0.75 + 0.05 * (pass % 5),
        4.0 + pass % 4,
        10.0 + 3.0 * (pass % 30),
        static_cast<int32_t>(10 * (pass % 7)),
        static_cast<int32_t>(2 + pass % 3),
        (pass & 1) == 0,
        pass % 3,
        pass % paletteSize,
        pass % labelCount,
    };
}

// The three-state check box, and the one value in this benchmark where the
// two sides genuinely part company.
//
// WinRT spells "checked, unchecked, or neither" as IReference<bool> -- a COM
// object wrapped around a bool, with everything that entails. cppwinrt builds
// a fresh one, reference counted, for every single write. wxl builds none: it
// hands over one of two objects that live in the program's own storage, are
// never made and never unmade, and whose AddRef and Release do nothing
// (wxl.ui/src/impl/value_box.h) -- so setting the property costs a pointer
// and the call. The empty state is a null pointer on both sides and costs
// neither of them anything.
//
// A pass therefore writes all three states in turn, one per pass.

}  // namespace bench
