// The three item templates of the original, each a function from the item (the position of an object in the
// application's vector) to an ItemContainer with what it shows.
auto const* objects = &gallery::dataObjects(true);

auto const imageOf = [objects](Object const& item) {
    auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
    return ItemContainer {width = 200, height = 140, horizontalAlignment = HorizontalAlignment::Left,
                          child = Image {Margin {4}, horizontalAlignment = HorizontalAlignment::Center,
                                 verticalAlignment = VerticalAlignment::Center, stretch = Stretch::UniformToFill,
                                 source = object.imageLocation}};
};

auto const strip = [](gallery::CustomDataObject const& object) {
    return StackPanel {
        height = 40,
        Padding {5, 1, 5, 1},
        verticalAlignment = VerticalAlignment::Bottom,
        background = brushes.SystemControl.Background.Base.Medium,
        opacity = 0.75,
        orientation.vertical,
        TextBlock {foreground = brushes.SystemControl.Foreground.AltHigh, object.title},
        StackPanel {orientation.horizontal,
                    TextBlock {styles.TextBlock.Caption, foreground = brushes.SystemControl.Foreground.AltHigh, object.likes},
                    TextBlock {styles.TextBlock.Caption, foreground = brushes.SystemControl.Foreground.AltHigh, u" Likes"}},
    };
};
struct Model {
    ItemsView view {width = 500, height = 400, horizontalAlignment = HorizontalAlignment::Left};
    StackLayout stack {spacing = 5.0};
    UniformGridLayout grid {minRowSpacing = 5.0, minColumnSpacing = 5.0, maximumRowsOrColumns = 3};
    LinedFlowLayout lined {itemsStretch = LinedFlowLayoutItemsStretch::Fill, lineHeight = 160.0, lineSpacing = 5.0, minItemSpacing = 5.0};
    StackPanel linedOptions {minHeight = 300};
    StackPanel stackOptions {minHeight = 300, visibility = Visibility::Collapsed};
    StackPanel gridOptions {minHeight = 300, visibility = Visibility::Collapsed};
};
auto const model = gallery::hold<Model>();

auto const linedTemplate = [objects, strip](Object const& item) {
    auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
    return ItemContainer {child = Grid {Image {minWidth = 70, horizontalAlignment = HorizontalAlignment::Center,
                                       verticalAlignment = VerticalAlignment::Center, stretch = Stretch::UniformToFill,
                                       source = object.imageLocation},
                                strip(object)}};
};
auto const stackTemplate = [objects](Object const& item) {
    auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
    auto picture = Image {width = 24, height = 16, Margin {0, 4, 0, 0}, alignLeftWithPanel = true, alignTopWithPanel = true,
                          stretch = Stretch::UniformToFill, source = object.imageLocation};
    auto caption = TextBlock {styles.TextBlock.Base, Margin {8, 0, 0, 0}, alignTopWithPanel = true, rightOf = picture, object.title};
    return ItemContainer {child = RelativePanel {width = 480, minHeight = 80, maxHeight = 100, picture, caption,
                                         TextBlock {styles.TextBlock.Caption, Margin {0, 4, 8, 4}, below = caption,
                                                    textTrimming = TextTrimming::WordEllipsis, textWrapping = TextWrapping::Wrap,
                                                    object.description}}};
};
auto const gridTemplate = [objects, strip](Object const& item) {
    auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
    return ItemContainer {child = Grid {width = 150, Image {horizontalAlignment = HorizontalAlignment::Center,
                                                    verticalAlignment = VerticalAlignment::Center,
                                                    stretch = Stretch::UniformToFill, source = object.imageLocation},
                                strip(object)}};
};

model->view.layout(model->lined);
model->view.itemTemplate(linedTemplate);
model->view.itemsSource(indexList(static_cast<int64_t>(objects->size())));

// A number box that tells one number to a layout.
auto const number = [](char16_t const* label, double from, double to, double start, auto apply) {
    auto box = NumberBox {header = label, minimum = from, maximum = to, value = start, smallChange = 1.0, maxWidth = 250,
                          spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, Margin {0, 0, 0, 16}};
    box.add_onValueChanged([apply](NumberBox const& self, NumberBoxValueChangedEventArgs&) { apply(self.value()); });
    return box;
};
auto const heading = [](char16_t const* label) { return TextBlock {Margin {0, 15, 0, 10}, FontWeight {600}, label}; };

model->linedOptions.children().append(heading(u"LinedFlowLayout settings"));
model->linedOptions.children().append(number(u"Space between lines", 0, 100, 5, [model](double v) { model->lined.lineSpacing(v); }));
model->linedOptions.children().append(number(u"Minimum space between items on a line", 0, 100, 5, [model](double v) { model->lined.minItemSpacing(v); }));
model->linedOptions.children().append(RadioButtons {
    header = u"Line height",
    RadioButton {content = u"Small"},
    RadioButton {content = u"Large"},
    selectedIndex = 1,
    onSelectionChanged = [model](RadioButtons const& self) { if (self.selectedIndex() >= 0) model->lined.lineHeight(self.selectedIndex() == 0 ? 80.0 : 160.0); },
});
model->stackOptions.children().append(heading(u"StackLayout settings"));
model->stackOptions.children().append(number(u"Space between rows", 0, 100, 5, [model](double v) { model->stack.spacing(v); }));
model->gridOptions.children().append(heading(u"UniformGridLayout settings"));
model->gridOptions.children().append(number(u"Minimum space between columns", 0, 100, 5, [model](double v) { model->grid.minColumnSpacing(v); }));
model->gridOptions.children().append(number(u"Minimum space between rows", 0, 100, 5, [model](double v) { model->grid.minRowSpacing(v); }));
model->gridOptions.children().append(number(u"Maximum number of items per row before wrapping", 1, 8, 3, [model](double v) { model->grid.maximumRowsOrColumns(static_cast<int>(v)); }));

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 15}, textWrapping = TextWrapping::Wrap,
               u"Use the options on the right to control different layout customizations to the ItemsView below."},
    model->view,
};

auto options = StackPanel {
    minWidth = 300,
    RadioButtons {
        header = u"Layout",
        FontWeight {600},
        RadioButton {content = u"LinedFlowLayout", FontWeight {400}},
        RadioButton {content = u"UniformGridLayout", FontWeight {400}},
        RadioButton {content = u"StackLayout", FontWeight {400}},
        selectedIndex = 0,
        onSelectionChanged = [=](RadioButtons const& self) {
            auto const choice = self.selectedIndex();
            if (choice < 0) return;
            if (choice == 0) { model->view.layout(model->lined); model->view.itemTemplate(linedTemplate); }
            if (choice == 1) { model->view.layout(model->grid); model->view.itemTemplate(gridTemplate); }
            if (choice == 2) { model->view.layout(model->stack); model->view.itemTemplate(stackTemplate); }
            model->linedOptions.visibility(choice == 0 ? Visibility::Visible : Visibility::Collapsed);
            model->gridOptions.visibility(choice == 1 ? Visibility::Visible : Visibility::Collapsed);
            model->stackOptions.visibility(choice == 2 ? Visibility::Visible : Visibility::Collapsed);
        },
    },
    model->linedOptions,
    model->stackOptions,
    model->gridOptions,
};