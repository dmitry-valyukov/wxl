// The items of the original are "Item 1" ... "Item 500"; here the repeater is given the number of each.
struct Model {
    FlowLayout flow {orientation.horizontal, lineAlignment = FlowLayoutLineAlignment::Start, lineSpacing = 8.0, minItemSpacing = 8.0};
    ScrollViewer scroll {
        width = 520,
        height = 320,
        hAlign.left,
        horizontalScrollBarVisibility = ScrollBarVisibility::Disabled,
        horizontalScrollMode = ScrollMode::Disabled,
        isHorizontalScrollChainingEnabled = false,
        isVerticalScrollChainingEnabled = false,
        verticalScrollBarVisibility = ScrollBarVisibility::Auto,
        verticalScrollMode = ScrollMode::Enabled,
    };
    TextBlock realizedCount {text = u"Realized elements: 0 of 500"};
    std::vector<UIElement> realized;
    bool updateQueued = false;
};
auto const model = gallery::hold<Model>();

// The count is written once the pass that prepared or cleared elements is over, however many there were.
auto const queueCountUpdate = [model] {
    if (model->updateQueued) {
        return;
    }
    model->updateQueued = DispatcherQueue::getForCurrentThread().tryEnqueue([model] {
        model->updateQueued = false;
        std::u16string words = u"Realized elements: ";
        words += gallery::numberText(static_cast<int>(model->realized.size()));
        words += u" of 500";
        model->realizedCount.text(words);
    });
};

auto const flowItem = [](Object const& item) {
    std::u16string label = u"Item ";
    label += gallery::numberText(static_cast<int>(intOf(item)) + 1);
    return Border {width = 96,
                   height = 64,
                   background = brushes.Control.FillColor.Default,
                   borderBrush = brushes.Control.StrokeColor.Default,
                   BorderThickness {1},
                   CornerRadius {4},
                   TextBlock {hAlign.center, vAlign.center, label}};
};

model->scroll.content(ItemsRepeater {
    horizontalCacheLength = 0,
    verticalCacheLength = 0,
    layout = model->flow,
    itemTemplate = flowItem,
    itemsSource = indexList(500),
    onElementPrepared = [model, queueCountUpdate](auto const&, ItemsRepeaterElementPreparedEventArgs& args) {
        model->realized.push_back(args.element());
        queueCountUpdate();
    },
    onElementClearing = [model, queueCountUpdate](auto const&, ItemsRepeaterElementClearingEventArgs& args) {
        std::erase_if(model->realized, [&](UIElement const& each) { return each.is_same_object(args.element()); });
        queueCountUpdate();
    },
});

auto example = StackPanel {
    spacing = 8,
    TextBlock {text = u"Scroll through 500 items. ItemsRepeater creates and recycles only the elements needed near the viewport.",
               textWrapping = TextWrapping::Wrap},
    model->scroll,
    model->realizedCount,
};

auto options = StackPanel {
    spacing = 12,
    RadioButtons {
        header = u"Orientation",
        RadioButton {content = u"Horizontal"},
        RadioButton {content = u"Vertical"},
        selectedIndex = 0,
        onSelectionChanged = [model](RadioButtons const& self) {
            bool const horizontal = self.selectedIndex() == 0;
            model->flow.orientation(horizontal ? Orientation::Horizontal : Orientation::Vertical);
            model->scroll.horizontalScrollMode(horizontal ? ScrollMode::Disabled : ScrollMode::Enabled);
            model->scroll.horizontalScrollBarVisibility(horizontal ? ScrollBarVisibility::Disabled : ScrollBarVisibility::Auto);
            model->scroll.verticalScrollMode(horizontal ? ScrollMode::Enabled : ScrollMode::Disabled);
            model->scroll.verticalScrollBarVisibility(horizontal ? ScrollBarVisibility::Auto : ScrollBarVisibility::Disabled);
        },
    },
    ComboBox {
        header = u"LineAlignment",
        ComboBoxItem {content = u"Start"},
        ComboBoxItem {content = u"Center"},
        ComboBoxItem {content = u"Space between"},
        ComboBoxItem {content = u"Space evenly"},
        selectedIndex = 0,
        onSelectionChanged = [model](ComboBox const& self) {
            static constexpr FlowLayoutLineAlignment alignments[] = {FlowLayoutLineAlignment::Start, FlowLayoutLineAlignment::Center,
                                                                     FlowLayoutLineAlignment::SpaceBetween,
                                                                     FlowLayoutLineAlignment::SpaceEvenly};
            if (self.selectedIndex() >= 0) {
                model->flow.lineAlignment(alignments[self.selectedIndex()]);
            }
        },
    },
    Slider {header = u"MinItemSpacing", maximum = 32, snapsTo = SliderSnapsTo::Ticks, stepFrequency = 2, tickFrequency = 2, value = 8,
            onValueChanged = [model](Slider const& self) { model->flow.minItemSpacing(self.value()); }},
    Slider {header = u"LineSpacing", maximum = 32, snapsTo = SliderSnapsTo::Ticks, stepFrequency = 2, tickFrequency = 2, value = 8,
            onValueChanged = [model](Slider const& self) { model->flow.lineSpacing(self.value()); }},
};