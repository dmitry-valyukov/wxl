// A category is a name and the foods under it; the outer repeater is given the position of each category,
// the inner one the position of each food.
struct Category {
    std::u16string name;
    std::vector<std::u16string> const* foods;
};
auto const categories = std::make_shared<std::vector<Category>>(std::vector<Category> {
    {u"Fruits", &gallery::fruits()},
    {u"Vegetables", &gallery::vegetables()},
    {u"Grains", &gallery::grains()},
    {u"Proteins", &gallery::proteins()},
});

auto const category = [categories](Object const& item) {
    auto const& each = (*categories)[static_cast<size_t>(intOf(item))];
    auto const* foods = each.foods;
    return StackPanel {
        TextBlock {Padding {8}, styles.TextBlock.Title, each.name},
        ItemsRepeater {
            layout = StackLayout {orientation.horizontal},
            itemTemplate = [foods](Object const& food) {
                return Grid {Margin {10},
                             background = brushes.SystemControl.Background.Accent,
                             TextBlock {Padding {10}, foreground = brushes.SystemControl.Foreground.Chrome.White, hAlign.center,
                                        vAlign.center, textWrapping = TextWrapping::Wrap, (*foods)[static_cast<size_t>(intOf(food))]}};
            },
            itemsSource = indexList(static_cast<int64_t>(foods->size())),
        },
    };
};

auto example = ScrollViewer {
    horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
    horizontalScrollMode = ScrollMode::Auto,
    content = ItemsRepeater {vAlign.top,
                             layout = StackLayout {orientation.vertical},
                             itemTemplate = category,
                             itemsSource = indexList(static_cast<int64_t>(categories->size()))},
};