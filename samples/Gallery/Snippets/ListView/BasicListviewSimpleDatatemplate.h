auto const* people = &gallery::contacts();

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 16}, textWrapping = TextWrapping::Wrap,
               u"This is a basic ListView that has the full source code below. Other samples on this page display only the "
               u"additional markup needed to customize a simple ListView like this one."},
    ListView {
        width = 350,
        height = 400,
        horizontalAlignment = HorizontalAlignment::Left,
        borderBrush = brushes.Control.Strong.StrokeColorDefault,
        borderThickness = 1,
        itemTemplate = [people](Object const& item) {
            return TextBlock {Margin {0, 5, 0, 5}, (*people)[static_cast<size_t>(intOf(item))].name()};
        },
        itemsSource = indexList(static_cast<int64_t>(people->size())),
    },
};