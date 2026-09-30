struct Model {
    core::observable<int> layout{0};
    core::observable<double> spacing{8};
};
auto const model = gallery::hold<Model>();

// FeaturedTileLayout of the original: the first child takes the whole width,
// the rest stand in two columns, a pair to a row as tall as the taller of the two.
auto rowHeight = [](Collection<UIElement> const& children, uint32_t first) {
    float height = children[first].desiredSize().height;
    return first + 1 < children.size() ? std::max(height, children[first + 1].desiredSize().height) : height;
};

auto featured = CustomLayout {
    [model, rowHeight](Collection<UIElement> const& children, Size available) {
        float const spacing = static_cast<float>(model->spacing.get());
        float const width = std::isinf(available.width) ? 520.0f : available.width;
        float const column = std::max(0.0f, (width - spacing) / 2);

        for (uint32_t index = 0; index < children.size(); ++index) {
            children[index].measure({index == 0 ? width : column, std::numeric_limits<float>::infinity()});
        }

        float total = children.empty() ? 0.0f : children[0].desiredSize().height;
        for (uint32_t index = 1; index < children.size(); index += 2) {
            total += spacing + rowHeight(children, index);
        }
        return Size {width, total};
    },
    [model, rowHeight](Collection<UIElement> const& children, Size final) {
        if (children.empty()) {
            return final;
        }
        float const spacing = static_cast<float>(model->spacing.get());
        float const column = std::max(0.0f, (final.width - spacing) / 2);
        float y = children[0].desiredSize().height;
        children[0].arrange({{0, 0}, {final.width, y}});

        for (uint32_t index = 1; index < children.size(); index += 2) {
            y += spacing;
            float const height = rowHeight(children, index);
            children[index].arrange({{0, y}, {column, height}});
            if (index + 1 < children.size()) {
                children[index + 1].arrange({{column + spacing, y}, {column, height}});
            }
            y += height;
        }
        return Size {final.width, y};
    },
};

auto stack = StackLayout {orientation = Orientation::Vertical, spacing = BindOutput {model->spacing}};

auto tile = [](double extent, char16_t const* text) {
    return Border {
        height = extent,
        Padding {12},
        background = brushes.Control.FillColor.Default,
        CornerRadius {4},
        TextBlock {text, vAlign.center},
    };
};

auto panel = LayoutPanel {
    maxWidth = 560,
    hAlign.stretch,
    borderBrush = brushes.Card.StrokeColorDefault,
    BorderThickness {1},
    CornerRadius {4},
    Padding {12},
    layout = BindOutput {model->layout, [featured, stack](int chosen) -> Layout {
                             if (chosen == 0) {
                                 return featured;
                             }
                             return stack;
                         }},
    Border {
        height = 96,
        Padding {16},
        background = brushes.Accent.FillColor.Default,
        CornerRadius {4},
        StackPanel {
            vAlign.center,
            TextBlock {u"Featured item", foreground = brushes.Text.OnAccent.FillColor.Primary, fontWeight = FontWeight {600}},
            TextBlock {u"The first child spans the available width.", foreground = brushes.Text.OnAccent.FillColor.Secondary},
        },
    },
    tile(72, u"Second"),
    tile(92, u"Third (taller)"),
    tile(80, u"Fourth"),
    tile(64, u"Fifth"),
    tile(88, u"Sixth (taller)"),
    tile(68, u"Seventh"),
    tile(76, u"Eighth"),
    tile(96, u"Ninth (taller)"),
};

auto options = StackPanel {
    width = 220,
    spacing = 12.0,
    RadioButtons {
        header = u"Layout",
        RadioButton {content = u"Featured tile"},
        RadioButton {content = u"Vertical stack"},
        selectedIndex = Bind {model->layout},
    },
    Slider {
        header = u"Spacing",
        maximum = 24.0,
        snapsTo = SliderSnapsTo::Ticks,
        stepFrequency = 2.0,
        tickFrequency = 2.0,
        value = Bind {model->spacing},
        onValueChanged = [featured](Object const&, RangeBaseValueChangedEventArgs&) { featured.invalidate(); },
    },
    TextBlock {
        u"LayoutPanel also applies its BorderBrush, BorderThickness, CornerRadius, and Padding around the layout.",
        textWrapping = TextWrapping::Wrap,
    },
};