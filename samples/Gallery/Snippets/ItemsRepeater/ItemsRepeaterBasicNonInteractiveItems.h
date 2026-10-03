// The bars are the application's own, in a vector that only grows. The repeater is given the position of
// each bar in a list it follows as it changes, so a bar added or removed shows at once.
auto const bars = std::make_shared<std::vector<gallery::Bar>>();
auto const items = observableIndexList();
auto const addBar = [bars, items](double length) {
    bars->emplace_back(length, 425);
    indexListAppend(items, static_cast<int64_t>(bars->size()) - 1);
};
addBar(300);
addBar(25);
addBar(175);

auto const horizontalBar = [bars](Object const& item) {
    auto const& bar = (*bars)[static_cast<size_t>(intOf(item))];
    return Border {width = bar.maxLength,
                   background = brushes.SystemControl.Background.Chrome.MediumLow,
                   Rectangle {width = bar.length, height = 24, hAlign.left, fill = brushes.Accent.FillColor.Default}};
};
auto const verticalBar = [bars](Object const& item) {
    auto const& bar = (*bars)[static_cast<size_t>(intOf(item))];
    return Border {height = bar.maxHeight,
                   background = brushes.SystemControl.Background.Chrome.MediumLow,
                   Rectangle {width = 48, height = bar.height, vAlign.top, fill = brushes.Accent.FillColor.Default}};
};
auto const circle = [bars](Object const& item) {
    auto const& bar = (*bars)[static_cast<size_t>(intOf(item))];
    return Grid {
        Ellipse {width = bar.maxDiameter, height = bar.maxDiameter, hAlign.center, vAlign.center,
                 fill = brushes.SystemControl.Background.Chrome.MediumLow},
        Ellipse {width = bar.diameter, height = bar.diameter, hAlign.center, vAlign.center, fill = brushes.Accent.FillColor.Default},
    };
};

struct Model {
    ItemsRepeater repeater;
    Button remove {content = u"Remove Item", minWidth = 150};
    StackLayout vertical {orientation.vertical, spacing = 8.0};
    StackLayout horizontal {orientation.horizontal, spacing = 8.0};
    UniformGridLayout grid {minRowSpacing = 8.0, minColumnSpacing = 8.0};
};
auto const model = gallery::hold<Model>();
model->repeater = ItemsRepeater {layout = model->vertical, itemTemplate = horizontalBar, itemsSource = items};

model->remove.add_onClick([model, items](auto&&...) {
    if (indexListSize(items) > 0) {
        indexListRemoveAt(items, 0);
    }
    if (indexListSize(items) == 0) {
        model->remove.isEnabled(false);
    }
});

auto example = ScrollViewer {
    maxHeight = 500,
    horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
    horizontalScrollMode = ScrollMode::Auto,
    isVerticalScrollChainingEnabled = false,
    verticalScrollBarVisibility = ScrollBarVisibility::Auto,
    content = model->repeater,
};

auto options = StackPanel {
    spacing = 12,
    Button {content = u"Add Item", minWidth = 150,
            onClick = [model, bars, addBar](auto&&...) {
                addBar(static_cast<double>((bars->size() * 137 + 61) % 425));
                model->remove.isEnabled(true);
            }},
    model->remove,
    RadioButtons {
        header = u"Layout",
        RadioButton {content = u"StackLayout - Vertical"},
        RadioButton {content = u"StackLayout - Horizontal"},
        RadioButton {content = u"UniformGridLayout"},
        selectedIndex = 0,
        onSelectionChanged = [=](RadioButtons const& self) {
            auto const& repeater = model->repeater;
            switch (self.selectedIndex()) {
            case 0:
                repeater.maxWidth(425 + 12);
                repeater.layout(model->vertical);
                repeater.itemTemplate(horizontalBar);
                break;
            case 1:
                repeater.maxWidth(6000);
                repeater.layout(model->horizontal);
                repeater.itemTemplate(verticalBar);
                break;
            case 2:
                repeater.maxWidth(540);
                repeater.layout(model->grid);
                repeater.itemTemplate(circle);
                break;
            default:
                return;
            }
            repeater.itemsSource(items);
        },
    },
};