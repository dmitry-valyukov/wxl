// Five sections of coloured tiles, in rows as wide as the panel holds; every
// section has a label on the bar, at the offset of its first row. The tiles
// stand in a WrapPanel, which for a few hundred of them does what an
// ItemsRepeater with a UniformGridLayout does.
struct Section {
    char16_t const* name;
    int count;
    Color color;
};
static constexpr Section sections[] = {
    {u"Azure", 32, rgb(0xf0, 0xff, 0xff)},
    {u"Crimson", 50, rgb(0xdc, 0x14, 0x3c)},
    {u"Cyan", 8, rgb(0x00, 0xff, 0xff)},
    {u"Fuchsia", 70, rgb(0xff, 0x00, 0xff)},
    {u"Gold", 90, rgb(0xff, 0xd7, 0x00)},
};

// Each tile is 112x82 with a margin of 4: 120x90 in the panel.
static constexpr int tileWidth = 120;
static constexpr int tileHeight = 90;

// The model owns the panel and the bar, which handlers of both reach by
// address; nothing they hold owns the model.
struct Model {
    core::observable<double> barHeight {500.0};
    WrapPanel tiles {
        Margin {2},
        // When the panel is resized the rows change, and with them the labels.
        onSizeChanged = [this](Object const&, SizeChangedEventArgs&) { placeLabels(); },
    };

    AnnotatedScrollBar bar {
        column = 1,
        maxHeight = BindOutput {barHeight},
        Margin {4, 0, 48, 0},
        hAlign.right,
        // The labels are placed when the bar is on the screen, and again when the rows change.
        onLoaded = [this](Object const&, RoutedEventArgs&) { placeLabels(); },
        // What the tooltip over the rail says: the section the pointer is in.
        onDetailLabelRequested = [this](Object const&, AnnotatedScrollBarDetailLabelRequestedEventArgs& args) {
            args.content(Object::from_text(labelAt(args.scrollOffset())));
        },
    };

    Model() {
        for (auto const& section : sections) {
            for (int index = 0; index < section.count; ++index) {
                tiles.children().append(Grid {width = 112, height = 82, Margin {4}, background = section.color, CornerRadius {4}});
            }
        }
    }

    int itemsPerRow() const {
        return tiles.actualWidth() == 0 ? 1 : std::max(static_cast<int>(tiles.actualWidth()) / tileWidth, 1);
    }

    double offsetOfItem(int index) const { return tileHeight * (index / itemsPerRow()); }

    // The section an offset falls in: that of the last item of the row it ends in.
    char16_t const* labelAt(double offset) const {
        int end = 0;
        for (auto const& section : sections) {
            end += section.count;
            if (&section == std::end(sections) - 1 || offset <= offsetOfItem(end - 1)) {
                return section.name;
            }
        }
        return sections[0].name;
    }

    void placeLabels() {
        bar.labels().clear();
        int first = 0;
        for (auto const& section : sections) {
            bar.labels().append(AnnotatedScrollBarLabel {Object::from_text(section.name), offsetOfItem(first)});
            first += section.count;
        }
    }
};
auto const model = gallery::hold<Model>();

auto example = Grid {
    columnDefinitions = u"*,auto",
    ScrollView {
        maxWidth = 800,
        maxHeight = 500,
        background = rgb(0xd3, 0xd3, 0xd3),
        verticalScrollBarVisibility = ScrollingScrollBarVisibility::Hidden,
        // The bar drives the view and is told where it is.
        verticalScrollController = model->bar,
        // Once it is connected, the labels are placed against what the bar now knows of the extent.
        onLoaded = [model](Object const&, RoutedEventArgs&) { model->placeLabels(); },
        content = model->tiles,
    },
    model->bar,
};

auto options = Grid {
    minWidth = 200,
    rowDefinitions = u"auto,auto",
    TextBlock {vAlign.center, u"Changing the AnnotatedScrollBar height refreshes its Labels layout."},
    Slider {
        row = 1,
        Margin {0, 10, 0, 0},
        header = u"AnnotatedScrollBar maximum height:",
        minimum = 100.0,
        maximum = 500.0,
        // The labels that no longer fit are hidden, as they would collide.
        value = Bind {model->barHeight},
    },
};