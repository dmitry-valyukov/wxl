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
    ItemsView view {width = 500, height = 300, horizontalAlignment = HorizontalAlignment::Left,
                    layout = UniformGridLayout {minRowSpacing = 5.0, minColumnSpacing = 5.0}};
    TextBlock invoked {Margin {0, 8, 0, 0}};
    TextBlock selected {Margin {0, 8, 0, 0}};
};
auto const model = gallery::hold<Model>();

model->view.itemTemplate(imageOf);
model->view.itemsSource(indexList(static_cast<int64_t>(objects->size())));
model->view.add_onItemInvoked([model, objects](auto const&, ItemsViewItemInvokedEventArgs& args) {
    std::u16string words = u"You invoked ";
    words += (*objects)[static_cast<size_t>(intOf(args.invokedItem()))].title;
    words += u".";
    model->invoked.text(words);
});
model->view.add_onSelectionChanged([model, objects](auto const&, auto&&...) {
    auto const item = model->view.selectedItem();
    std::u16string words = item ? u"You have selected " : u"You have selected nothing.";
    if (item) {
        words += (*objects)[static_cast<size_t>(intOf(item))].title;
        words += u".";
    }
    model->selected.text(words);
});

auto example = Grid {
    rowDefinitions = u"auto,*,auto",
    RichTextBlock {
        Margin {0, 0, 0, 15},
        textWrapping = TextWrapping::Wrap,
        Paragraph {Run {u"You can enable four different selection modes on the right."}},
        Paragraph {Bold {Run {u"None"}}, Run {u" disables selection all together."}},
        Paragraph {Bold {Run {u"Single"}}, Run {u" allows for only one item to be selected."}},
        Paragraph {Bold {Run {u"Multiple"}}, Run {u" lets the user choose any number of items with a click on each."}},
        Paragraph {Bold {Run {u"Extended"}}, Run {u" allows Ctrl+Click for individual items and Shift+Click for a range."}},
    },
    Border {row = 1, model->view},
    StackPanel {row = 2, model->invoked, model->selected},
};

auto options = StackPanel {
    spacing = 8,
    ComboBox {
        header = u"SelectionMode",
        ComboBoxItem {content = u"None"},
        ComboBoxItem {content = u"Single"},
        ComboBoxItem {content = u"Multiple"},
        ComboBoxItem {content = u"Extended"},
        selectedIndex = 1,
        onSelectionChanged = [model](ComboBox const& self) {
            static constexpr ItemsViewSelectionMode modes[] = {ItemsViewSelectionMode::None, ItemsViewSelectionMode::Single,
                                                               ItemsViewSelectionMode::Multiple, ItemsViewSelectionMode::Extended};
            if (self.selectedIndex() >= 0) model->view.selectionMode(modes[self.selectedIndex()]);
        },
    },
    CheckBox {content = u"Item invocation", isChecked = true,
              onChecked = [model](auto&&...) { model->view.isItemInvokedEnabled(true); },
              onUnchecked = [model](auto&&...) { model->view.isItemInvokedEnabled(false); }},
};