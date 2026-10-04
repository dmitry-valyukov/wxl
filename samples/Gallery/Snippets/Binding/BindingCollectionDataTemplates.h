// The data stay C++ objects: the list holds positions in the application's own vector, the item template reads the
// object back by its position, and the details are fields of the model that the selection sets.
struct Item {
    char16_t const* title;
    char16_t const* created;
    char16_t const* text;
};
static constexpr Item items[] = {
    {u"Item 1", u"Jun 15, 2025 9:30 AM",
     u"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer id facilisis lectus. Cras nec convallis ante, quis pulvinar tellus."},
    {u"Item 2", u"Jul 22, 2025 2:15 PM",
     u"Quisque accumsan pretium ligula in faucibus. Mauris sollicitudin augue vitae lorem cursus condimentum quis ac mauris."},
    {u"Item 3", u"Aug 3, 2025 11:00 AM",
     u"Ut consequat magna luctus justo egestas vehicula. Integer pharetra risus libero, et posuere justo mattis et."},
    {u"Item 4", u"Sep 10, 2025 4:45 PM",
     u"Duis facilisis, quam ut laoreet commodo, elit ex aliquet massa, non varius tellus lectus et nunc."},
};

struct Model {
    core::observable<int> selected {0};
    core::observable<core::u16_text> title, created, text;

    Model() {
        title.follow(selected, [](int at) { return core::unicode::repaired(items[at].title); });
        created.follow(selected, [](int at) { return core::unicode::repaired(items[at].created); });
        text.follow(selected, [](int at) { return core::unicode::repaired(items[at].text); });
    }
};
auto const model = gallery::hold<Model>();
auto* const selected = &model->selected;

auto example = Grid {
    columnSpacing = 16.0,
    minHeight = 250,
    columnDefinitions = u"200,*",
    ListView {
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness {1},
        cornerRadius = CornerRadius {4},
        itemsSource = indexList(4),
        itemTemplate = [](Object const& item) {
            auto const& at = items[static_cast<size_t>(intOf(item))];
            return StackPanel {Padding {4}, spacing = 2.0, TextBlock {FontWeight {600}, at.title},
                               TextBlock {foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption, at.created}};
        },
        onSelectionChanged = [selected](ListView const& list, SelectionChangedEventArgs&) {
            if (list.selectedIndex() >= 0) {
                selected->set(list.selectedIndex());
            }
        },
    },
    StackPanel {
        column = 1,
        Padding {16},
        spacing = 8.0,
        TextBlock {fontSize = 20, FontWeight {600}, text = BindOutput {model->title}},
        TextBlock {foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption, text = BindOutput {model->created}},
        TextBlock {textWrapping = TextWrapping::Wrap, text = BindOutput {model->text}},
    },
};
