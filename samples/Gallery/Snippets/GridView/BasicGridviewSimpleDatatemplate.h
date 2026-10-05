// The data: objects of the application's own, in a vector. The control is given the positions (indexList),
// and the item template -- a function from the item to the element that shows it -- reads the object back.
auto const* data = &gallery::dataObjects();

auto const output = TextBlock {Margin {0, 8, 0, 0}};

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 15}, u"This is a basic GridView that has the full source code below."},
    GridView {
        isItemClickEnabled = true,
        selectionMode = ListViewSelectionMode::Single,
        itemTemplate = [data](Object const& item) {
            return Image {width = 190, height = 130, stretch = Stretch::UniformToFill,
                          source = (*data)[static_cast<size_t>(intOf(item))].imageLocation};
        },
        itemsSource = indexList(static_cast<int64_t>(data->size())),
        onItemClick = [data, output](auto const&, ItemClickEventArgs& args) {
            std::u16string words = u"You clicked ";
            words += (*data)[static_cast<size_t>(intOf(args.clickedItem()))].title;
            words += u".";
            output.text(words);
        },
    },
    output,
};