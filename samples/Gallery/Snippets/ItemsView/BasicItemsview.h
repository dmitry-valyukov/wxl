auto const* objects = &gallery::dataObjects(true);
auto const output = TextBlock {Margin {0, 8, 0, 0}};

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 15}, textWrapping = TextWrapping::Wrap,
               u"This is a basic ItemsView which uses its default StackLayout layout and a simple ItemTemplate. "
               u"Hit the Enter key, double-click or double-tap an item to invoke it."},
    ItemsView {
        width = 220,
        height = 400,
        horizontalAlignment = HorizontalAlignment::Left,
        isItemInvokedEnabled = true,
        itemTemplate = [objects](Object const& item) {
            auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
            return ItemContainer {width = 200, height = 140, horizontalAlignment = HorizontalAlignment::Left,
                                  child = Image {Margin {4}, horizontalAlignment = HorizontalAlignment::Center,
                                         verticalAlignment = VerticalAlignment::Center, stretch = Stretch::UniformToFill,
                                         source = object.imageLocation}};
        },
        itemsSource = indexList(static_cast<int64_t>(objects->size())),
        onItemInvoked = [objects, output](auto const&, ItemsViewItemInvokedEventArgs& args) {
            std::u16string words = u"You invoked ";
            words += (*objects)[static_cast<size_t>(intOf(args.invokedItem()))].title;
            words += u".";
            output.text(words);
        },
    },
    output,
};