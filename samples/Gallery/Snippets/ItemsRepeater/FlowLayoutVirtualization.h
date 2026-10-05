struct Model {
    core::observable<int> direction {0}, alignment {0};
    core::observable<double> itemGap {8.0}, lineGap {8.0};
    FlowLayout flow {
        orientation = BindOutput {direction, [](int index) { return index == 0 ? Orientation::Horizontal : Orientation::Vertical; }},
        lineAlignment = BindOutput {alignment, [](int index) {
            static constexpr FlowLayoutLineAlignment alignments[] = {FlowLayoutLineAlignment::Start, FlowLayoutLineAlignment::Center,
                                                                     FlowLayoutLineAlignment::SpaceBetween, FlowLayoutLineAlignment::SpaceEvenly};
            return alignments[index];
        }},
        lineSpacing = BindOutput {lineGap},
        minItemSpacing = BindOutput {itemGap},
    };
    ScrollViewer scroll {
        width = 520,
        height = 320,
        hAlign.left,
        horizontalScrollBarVisibility = BindOutput {direction, [](int index) { return index == 0 ? ScrollBarVisibility::Disabled : ScrollBarVisibility::Auto; }},
        horizontalScrollMode = BindOutput {direction, [](int index) { return index == 0 ? ScrollMode::Disabled : ScrollMode::Enabled; }},
        isHorizontalScrollChainingEnabled = false,
        isVerticalScrollChainingEnabled = false,
        verticalScrollBarVisibility = BindOutput {direction, [](int index) { return index == 0 ? ScrollBarVisibility::Auto : ScrollBarVisibility::Disabled; }},
        verticalScrollMode = BindOutput {direction, [](int index) { return index == 0 ? ScrollMode::Enabled : ScrollMode::Disabled; }},
    };
    TextBlock realizedCount {text = u"Realized elements: 0 of 500", automationLiveSetting = AutomationLiveSetting::Polite};
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
        automationId = u"FlowLayoutOrientation",
        automationName = u"FlowLayout orientation",
        header = u"Orientation",
        RadioButton {content = u"Horizontal", automationId = u"FlowLayoutHorizontal", automationName = u"Horizontal flow"},
        RadioButton {content = u"Vertical", automationId = u"FlowLayoutVertical", automationName = u"Vertical flow"},
        selectedIndex = Bind {model->direction},
    },
    ComboBox {
        automationId = u"FlowLayoutLineAlignment",
        automationName = u"Line alignment",
        header = u"LineAlignment",
        ComboBoxItem {content = u"Start"},
        ComboBoxItem {content = u"Center"},
        ComboBoxItem {content = u"Space between"},
        ComboBoxItem {content = u"Space evenly"},
        selectedIndex = Bind {model->alignment},
    },
    Slider {automationId = u"FlowLayoutItemSpacing", automationName = u"Minimum item spacing", header = u"MinItemSpacing", maximum = 32, snapsTo = SliderSnapsTo::Ticks, stepFrequency = 2, tickFrequency = 2, value = Bind {model->itemGap}},
    Slider {automationId = u"FlowLayoutLineSpacing", automationName = u"Line spacing", header = u"LineSpacing", maximum = 32, snapsTo = SliderSnapsTo::Ticks, stepFrequency = 2, tickFrequency = 2, value = Bind {model->lineGap}},
};