// What the DataTemplate of the original says -- a picture above, a title on a strip below -- is a function
// from an item of the catalogue to the element that shows it.
auto example = FlipView {
    height = 180,
    maxWidth = 400,
    borderBrush = rgb(0, 0, 0),
    borderThickness = 1,
};
for (auto const& group : gallery::catalog().groups | std::views::take(3)) {
    for (auto const& item : group.items) {
        example.items().append(Grid {
            rowDefinitions = u"*,auto",
            Image {width = 36, verticalAlignment = VerticalAlignment::Center, stretch = Stretch::Uniform,
                   source = gallery::assetPath(item.imagePath)},
            Border {row = 1, height = 60, background = rgba(255, 255, 255, 0.65),
                    TextBlock {styles.TextBlock.Title, Padding {12, 12}, horizontalAlignment = HorizontalAlignment::Center,
                               foreground = rgb(0, 0, 0), item.title}},
        });
    }
}