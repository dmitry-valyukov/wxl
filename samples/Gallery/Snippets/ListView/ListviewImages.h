// Items with an image: the objects of the Gallery's own sample data, each shown as a picture over its title.
auto const* objects = &gallery::dataObjects(true);

auto example = ListView {
    width = 450,
    height = 400,
    horizontalAlignment = HorizontalAlignment::Left,
    borderBrush = brushes.Control.Strong.StrokeColorDefault,
    borderThickness = 1,
    itemTemplate = [objects](Object const& item) {
        auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
        return StackPanel {
            orientation.horizontal,
            spacing = 12,
            Margin {0, 6, 0, 6},
            Image {width = 120, height = 80, stretch = Stretch::UniformToFill, source = object.imageLocation},
            StackPanel {verticalAlignment = VerticalAlignment::Center, TextBlock {styles.TextBlock.Base, object.title},
                        TextBlock {styles.TextBlock.Caption, object.views + u" Views"}},
        };
    },
    itemsSource = indexList(static_cast<int64_t>(objects->size())),
};