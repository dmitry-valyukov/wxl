// The scene of scene_shape.h, built the way it is built without wxl: the
// cppwinrt projection, by hand, one control and one property call at a time.
//
// This is the other half of the comparison -- scene_wxl.cpp describes the
// same scene declaratively. Nothing here is written to be slow: it is what an
// ordinary WinUI3 application in C++ looks like when there is no XAML file,
// down to the idioms -- a string reaches a Text property as a view, so
// cppwinrt makes a stack HSTRING reference for it rather than allocating one,
// and wxl's own setters now do exactly the same (impl/conversions.h).
//
// Not one line of wxl is linked into this executable, deliberately: what is
// being measured is the projection, and a library beside it would be a
// library beside it.
//
// A console subsystem executable, like its twin: the window still opens, and
// the table has somewhere to go.

// The benchmark's own headers first, with the standard headers they use.
#include "scene_report.h"
#include "scene_shape.h"

// The projection, before <windows.h>: that header's GetCurrentTime macro
// would otherwise be applied to the projection's own method of that name.
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.h>

#include <windows.h>

#include <MddBootstrap.h>

namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;

using winrt::Windows::Foundation::IInspectable;

namespace {

winrt::Windows::UI::Color argb(uint32_t value) {
    return {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
            static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
}

// The two spellings of UTF-16 are the same sixteen bits on Windows and differ
// only in type name, so this is a rename and not a conversion -- the same
// step wxl's string_param makes, on the same literals.
std::wstring_view wide(std::u16string_view text) {
    return {reinterpret_cast<wchar_t const*>(text.data()), text.size()};
}

// A Content property takes an IInspectable, so a string has to be boxed into
// one. This is what both sides do with a button's caption.
IInspectable boxed(std::u16string_view text) {
    return winrt::box_value(winrt::hstring{wide(text)});
}

// Nothing is done with a click here, on either side: what is being measured
// is the subscription -- a delegate object, allocated and reference counted.
void clicked(IInspectable const&, mux::RoutedEventArgs const&) {}

muxc::Border card(int index, bench::CardText const& text) {
    muxc::Border border;
    muxc::Grid::SetRow(border, index / bench::columns);
    muxc::Grid::SetColumn(border, index % bench::columns);
    border.Background(muxm::SolidColorBrush{argb(bench::cardBackground)});
    border.BorderBrush(muxm::SolidColorBrush{argb(bench::cardBorder)});
    border.BorderThickness({1, 1, 1, 1});
    border.CornerRadius({8, 8, 8, 8});
    border.Padding({10, 10, 10, 10});

    muxc::StackPanel panel;
    panel.Spacing(6);
    auto const children = panel.Children();

    muxc::TextBlock title;
    title.Text(wide(text.title));
    title.FontSize(15);
    title.FontWeight({600});
    title.Foreground(muxm::SolidColorBrush{argb(bench::titleInk)});
    title.Margin({0, 0, 0, 4});
    children.Append(title);

    muxc::Button apply;
    apply.Content(boxed(bench::actionA));
    apply.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
    apply.Padding({8, 4, 8, 4});
    apply.Click(clicked);
    children.Append(apply);

    muxc::Button reset;
    reset.Content(boxed(bench::actionB));
    reset.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
    reset.Padding({8, 4, 8, 4});
    reset.Click(clicked);
    children.Append(reset);

    muxc::CheckBox check;
    check.Content(boxed(bench::checkLabel));
    check.IsChecked(index % 2 == 0);
    check.Margin({0, 0, 0, 0});
    children.Append(check);

    muxc::RadioButton automatic;
    automatic.Content(boxed(bench::firstChoice));
    automatic.GroupName(wide(text.group));
    automatic.IsChecked(true);
    automatic.Margin({0, 0, 0, 0});
    children.Append(automatic);

    muxc::RadioButton manual;
    manual.Content(boxed(bench::secondChoice));
    manual.GroupName(wide(text.group));
    manual.Margin({0, 0, 0, 0});
    children.Append(manual);

    muxc::ToggleSwitch mode;
    mode.Header(boxed(bench::switchHeader));
    mode.OnContent(boxed(bench::switchOn));
    mode.OffContent(boxed(bench::switchOff));
    mode.IsOn(index % 3 == 0);
    mode.Toggled(clicked);
    children.Append(mode);

    muxc::Slider slider;
    slider.Minimum(0);
    slider.Maximum(100);
    slider.Value(bench::sliderValue(index));
    slider.StepFrequency(1);
    children.Append(slider);

    muxc::ProgressBar progress;
    progress.Minimum(0);
    progress.Maximum(100);
    progress.Value(bench::progressValue(index));
    progress.Height(6);
    children.Append(progress);

    muxc::TextBlock caption;
    caption.Text(wide(bench::caption));
    caption.FontSize(12);
    caption.Foreground(muxm::SolidColorBrush{argb(bench::captionInk)});
    caption.TextWrapping(mux::TextWrapping::Wrap);
    children.Append(caption);

    border.Child(panel);
    return border;
}

muxc::RowDefinition row(mux::GridLength const& height) {
    muxc::RowDefinition definition;
    definition.Height(height);
    return definition;
}

muxc::ColumnDefinition column(mux::GridLength const& width) {
    muxc::ColumnDefinition definition;
    definition.Width(width);
    return definition;
}

muxc::Grid page(std::array<bench::CardText, bench::cards> const& texts) {
    muxc::Grid root;
    root.RowSpacing(12);
    root.Padding({12, 12, 12, 12});
    root.Background(muxm::SolidColorBrush{argb(bench::pageBackground)});
    root.RowDefinitions().Append(row({0, mux::GridUnitType::Auto}));
    root.RowDefinitions().Append(row({1, mux::GridUnitType::Star}));

    muxc::Border header;
    muxc::Grid::SetRow(header, 0);
    header.Background(muxm::SolidColorBrush{argb(bench::headerBackground)});
    header.CornerRadius({6, 6, 6, 6});
    header.Padding({12, 8, 12, 8});

    muxc::StackPanel strip;
    strip.Orientation(muxc::Orientation::Horizontal);
    strip.Spacing(12);

    muxc::TextBlock heading;
    heading.Text(wide(bench::headerTitle));
    heading.FontSize(20);
    heading.FontWeight({600});
    heading.Foreground(muxm::SolidColorBrush{argb(bench::titleInk)});
    heading.VerticalAlignment(mux::VerticalAlignment::Center);
    strip.Children().Append(heading);

    muxc::TextBlock note;
    note.Text(wide(bench::headerNote));
    note.FontSize(13);
    note.Foreground(muxm::SolidColorBrush{argb(bench::captionInk)});
    note.VerticalAlignment(mux::VerticalAlignment::Center);
    strip.Children().Append(note);

    header.Child(strip);
    root.Children().Append(header);

    muxc::Grid grid;
    muxc::Grid::SetRow(grid, 1);
    grid.RowSpacing(10);
    grid.ColumnSpacing(10);
    for (int i = 0; i < bench::columns; ++i) {
        grid.ColumnDefinitions().Append(column({1, mux::GridUnitType::Star}));
    }
    for (int i = 0; i < bench::rows; ++i) {
        grid.RowDefinitions().Append(row({1, mux::GridUnitType::Star}));
    }

    auto const cards = grid.Children();
    for (int i = 0; i < bench::cards; ++i) {
        cards.Append(card(i, texts[i]));
    }

    root.Children().Append(grid);
    return root;
}

// The same read-back its twin does: what the framework actually received,
// asked of the framework.
bench::tree_count walk(muxc::Grid const& root) {
    bench::tree_count counted;

    auto const top = root.Children();
    counted.elements = 1 + static_cast<int>(top.Size());

    if (auto const header = top.GetAt(0).try_as<muxc::Border>()) {
        if (auto const strip = header.Child().try_as<muxc::StackPanel>()) {
            counted.elements += 1 + static_cast<int>(strip.Children().Size());
        }
    }

    auto const grid = top.GetAt(1).try_as<muxc::Grid>();
    if (!grid) {
        return counted;
    }

    auto const cards = grid.Children();
    counted.cards = static_cast<int>(cards.Size());

    for (uint32_t i = 0; i < cards.Size(); ++i) {
        auto const border = cards.GetAt(i).try_as<muxc::Border>();
        if (!border) {
            continue;
        }

        auto const panel = border.Child().try_as<muxc::StackPanel>();
        if (!panel) {
            continue;
        }

        int const held = static_cast<int>(panel.Children().Size());
        counted.controls += held;
        counted.elements += held + 2;  // the card's Border and its StackPanel
    }

    return counted;
}

// ---- The second measurement: writing to a scene that already exists -------

// Handles into a built card, collected once before the stopwatch starts. On
// this side a handle is the class's own default interface; every property
// that does not live on that interface costs a QueryInterface per call, which
// is the difference this half of the benchmark exists to show.
struct CardHandles {
    muxc::Border card;
    muxc::TextBlock title;
    muxc::Button apply;
    muxc::Button reset;
    muxc::CheckBox check;
    muxc::RadioButton automatic;
    muxc::RadioButton manual;
    muxc::ToggleSwitch mode;
    muxc::Slider slider;
    muxc::ProgressBar progress;
    muxc::TextBlock caption;
};

std::vector<CardHandles> collect(muxc::Grid const& root) {
    std::vector<CardHandles> handles;
    handles.reserve(bench::cards);

    auto const cards = root.Children().GetAt(1).as<muxc::Grid>().Children();
    for (uint32_t i = 0; i < cards.Size(); ++i) {
        auto const border = cards.GetAt(i).as<muxc::Border>();
        auto const held = border.Child().as<muxc::StackPanel>().Children();
        handles.push_back({
            border,
            held.GetAt(0).as<muxc::TextBlock>(),
            held.GetAt(1).as<muxc::Button>(),
            held.GetAt(2).as<muxc::Button>(),
            held.GetAt(3).as<muxc::CheckBox>(),
            held.GetAt(4).as<muxc::RadioButton>(),
            held.GetAt(5).as<muxc::RadioButton>(),
            held.GetAt(6).as<muxc::ToggleSwitch>(),
            held.GetAt(7).as<muxc::Slider>(),
            held.GetAt(8).as<muxc::ProgressBar>(),
            held.GetAt(9).as<muxc::TextBlock>(),
        });
    }
    return handles;
}

// The brushes a pass hands out, made once: a fresh brush per write would put
// an activation back into the measurement, which is what this half of the
// benchmark exists to get out of it.
std::vector<muxm::SolidColorBrush> inkPalette() {
    std::vector<muxm::SolidColorBrush> palette;
    palette.reserve(bench::paletteSize);
    for (int i = 0; i < bench::paletteSize; ++i) {
        palette.push_back(muxm::SolidColorBrush{argb(bench::palette[i])});
    }
    return palette;
}

void writeText(muxc::TextBlock const& block, bench::PassValues const& v,
               std::u16string_view word, muxm::Brush const& ink, mux::Thickness const& edge) {
    block.Text(wide(word));
    block.FontSize(v.fontSize);
    block.Foreground(ink);
    block.CharacterSpacing(v.spacing);
    block.MaxLines(v.lines);
    block.Opacity(v.opacity);
    block.Margin(edge);
}

void writeButton(muxc::Button const& button, bench::PassValues const& v,
                 std::u16string_view word, mux::Thickness const& edge) {
    button.Content(boxed(word));
    button.IsEnabled(v.flag);
    button.Padding(edge);
    button.HorizontalContentAlignment(mux::HorizontalAlignment::Center);
    button.Opacity(v.opacity);
    button.Margin(edge);
}

// The three-state value, as cppwinrt hands it over: a fresh IReference<bool>
// per write -- a COM object built around the bool, activated and reference
// counted, and released again as soon as XAML has unboxed it. Only the empty
// state is free, because a null pointer is a null pointer on either side.
winrt::Windows::Foundation::IReference<bool> checkState(int state) {
    if (state == 2) {
        return nullptr;
    }
    return state == 1;
}

void writeCheck(muxc::CheckBox const& check, bench::PassValues const& v,
                std::u16string_view word, mux::Thickness const& edge) {
    check.IsThreeState(true);
    check.IsChecked(checkState(v.threeState));
    check.Content(boxed(word));
    check.IsEnabled(v.flag);
    check.Opacity(v.opacity);
    check.Margin(edge);
}

void writeRadio(muxc::RadioButton const& radio, bench::PassValues const& v,
                std::u16string_view word, mux::Thickness const& edge) {
    radio.Content(boxed(word));
    radio.GroupName(wide(word));
    radio.IsEnabled(v.flag);
    radio.Opacity(v.opacity);
    radio.Margin(edge);
}

void pass(std::vector<CardHandles> const& handles,
          std::vector<muxm::SolidColorBrush> const& palette, int index) {
    auto const v = bench::valuesOf(index);
    auto const& ink = palette[v.ink];
    auto const& fill = palette[(v.ink + 1) % bench::paletteSize];
    auto const word = bench::labels[v.word];
    mux::Thickness const edge{v.edge, v.edge, v.edge, v.edge};
    mux::CornerRadius const corner{v.edge, v.edge, v.edge, v.edge};

    for (auto const& h : handles) {
        h.card.Background(fill);
        h.card.BorderBrush(ink);
        h.card.BorderThickness(edge);
        h.card.CornerRadius(corner);
        h.card.Padding(edge);

        writeText(h.title, v, word, ink, edge);
        writeText(h.caption, v, word, ink, edge);

        writeButton(h.apply, v, word, edge);
        writeButton(h.reset, v, word, edge);

        writeCheck(h.check, v, word, edge);

        writeRadio(h.automatic, v, word, edge);
        writeRadio(h.manual, v, word, edge);

        h.mode.IsOn(v.flag);
        h.mode.OnContent(boxed(word));
        h.mode.OffContent(boxed(word));
        h.mode.Header(boxed(word));
        h.mode.Opacity(v.opacity);

        h.slider.Minimum(0.0);
        h.slider.Maximum(100.0);
        h.slider.Value(v.value);
        h.slider.StepFrequency(1.0);
        h.slider.IsEnabled(v.flag);

        h.progress.Minimum(0.0);
        h.progress.Maximum(100.0);
        h.progress.Value(v.value);
        h.progress.Height(v.edge);
    }
}

// One written property of every kind of element in a card, read back out of
// the framework and compared with what the last pass wrote.
int verify(std::vector<CardHandles> const& handles) {
    auto const v = bench::valuesOf(bench::passes - 1);
    auto const& h = handles.back();

    // The box comes back out as a box again -- a different one, freshly made
    // by the property store -- and the third state comes back as no object at
    // all, which is the whole of what IReference<bool> means.
    auto const checked = h.check.IsChecked();
    bool const stateHeld = v.threeState == 2
                               ? !checked
                               : checked && checked.Value() == (v.threeState == 1);

    int passed = 0;
    auto const held = [&passed](char const* what, bool ok) {
        passed += ok;
        if (!ok) {
            std::printf("  !! %s does not hold what the last pass wrote\n", what);
        }
    };

    held("Border.cornerRadius", h.card.CornerRadius().TopLeft == v.edge);
    held("Border.padding", h.card.Padding().Left == v.edge);
    held("TextBlock.fontSize", h.title.FontSize() == v.fontSize);
    held("TextBlock.text", std::wstring_view{h.title.Text()} == wide(bench::labels[v.word]));
    held("TextBlock.characterSpacing", h.caption.CharacterSpacing() == v.spacing);
    held("Button.padding", h.apply.Padding().Left == v.edge);
    held("CheckBox.isEnabled", h.check.IsEnabled() == v.flag);
    held("CheckBox.isChecked", stateHeld);
    // Through a float and back: the property is a double on both sides of the
    // ABI, and the composition visual under it keeps a float -- so 0.95 comes
    // back as 0.949999988. Everything else here round-trips exactly.
    held("RadioButton.opacity",
         static_cast<float>(h.automatic.Opacity()) == static_cast<float>(v.opacity));
    held("ToggleSwitch.isOn", h.mode.IsOn() == v.flag);
    held("Slider.value", h.slider.Value() == v.value);
    return passed;
}

// XAML will not start without an Application object, and the framework's own
// control resources are merged through it -- without them every control comes
// up with no template at all, which would make this measure something else
// entirely. Both executables bring this up the same way; wxl simply does it
// inside the library instead of here.
struct App : mux::ApplicationT<App, mux::Markup::IXamlMetadataProvider> {
    void OnLaunched(mux::LaunchActivatedEventArgs const&) {
        Resources().MergedDictionaries().Append(muxc::XamlControlsResources{});

        auto const texts = bench::cardTexts();

        std::vector<double> runs;
        runs.reserve(bench::runs);

        // Each scene is let go as soon as its time is taken: the local leaves
        // the block after the clock is read. The first run is the one a
        // starting application pays, factories and all.
        for (int run = 0; run < bench::runs; ++run) {
            auto const started = bench::clock::now();
            muxc::Grid const built = page(texts);
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
            muxc::Grid const target = page(texts);
            auto const handles = collect(target);
            auto const palette = inkPalette();

            for (int index = 0; index < bench::passes; ++index) {
                auto const begun = bench::clock::now();
                pass(handles, palette, index);
                auto const done = bench::clock::now();
                passTimes.push_back(bench::milliseconds(done - begun));
            }

            checksPassed = verify(handles);
        }

        // And once more for the window that stays on screen, stopped on the
        // line before Activate().
        auto const started = bench::clock::now();
        window_ = mux::Window{};
        window_.Title(wide(bench::windowTitle));
        window_.Content(page(texts));
        double const ready = bench::milliseconds(bench::clock::now() - started);

        bench::reportBuild("winrt -- the same scene built by hand", runs, ready,
                           walk(window_.Content().as<muxc::Grid>()), bench::elements,
                           bench::events);
        bench::reportPass(passTimes, bench::writesPerPass, bench::writesPerCard, checksPassed,
                          bench::sampledChecks);

        window_.AppWindow().Resize({1560, 940});
        window_.Activate();
    }

    auto GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type) const {
        return provider_.GetXamlType(type);
    }

    auto GetXamlType(winrt::hstring const& fullName) const {
        return provider_.GetXamlType(fullName);
    }

    auto GetXmlnsDefinitions() const { return provider_.GetXmlnsDefinitions(); }

    mux::XamlTypeInfo::XamlControlsXamlMetaDataProvider provider_;
    mux::Window window_{nullptr};
};

}  // namespace

int main() {
    // The framework package first: without it no WinUI3 class can be
    // activated at all. Release 2 -- the minor version is not consulted -- and
    // no version tag, matching what wxl's own bootstrap asks for.
    winrt::check_hresult(::MddBootstrapInitialize(0x00020000, nullptr, {}));

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    mux::Application::Start([](auto&&) { winrt::make<App>(); });
    return 0;
}
