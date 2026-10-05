// Four item templates of the original -- each a function from the item to its element.
enum class Kind { Image, IconText, ImageText, Text };

struct Model {
    GridView grid {flowDirection = FlowDirection::LeftToRight};
    TextBlock clicked {Margin {0, 8, 0, 0}};
    TextBlock selected {Margin {0, 8, 0, 0}};

    static ItemBuilder templateOf(Kind kind, std::vector<gallery::CustomDataObject> const* data) {
        return [kind, data](Object const& item) -> UIElement {
            auto const& object = (*data)[static_cast<size_t>(intOf(item))];
            switch (kind) {
            case Kind::Image:
                return Image {width = 190, height = 130, stretch = Stretch::UniformToFill, source = object.imageLocation};
            case Kind::IconText: {
                auto picture = Image {width = 18, Margin {0, 4, 0, 0}, alignLeftWithPanel = true, alignTopWithPanel = true,
                                      stretch = Stretch::Uniform, source = object.imageLocation};
                auto caption = TextBlock {styles.TextBlock.Base, Margin {8, 0, 0, 0}, alignTopWithPanel = true,
                                          rightOf = picture, object.title};
                return RelativePanel {
                    width = 280,
                    minHeight = 160,
                    picture,
                    caption,
                    TextBlock {styles.TextBlock.Caption, Margin {0, 4, 8, 0}, below = caption, textTrimming = TextTrimming::WordEllipsis,
                               textWrapping = TextWrapping::Wrap, object.description},
                };
            }
            case Kind::ImageText:
                return Grid {
                    width = 280,
                    columnDefinitions = u"auto,*",
                    Image {height = 100, verticalAlignment = VerticalAlignment::Top, stretch = Stretch::Fill, source = object.imageLocation},
                    StackPanel {
                        column = 1,
                        Margin {8, 0, 0, 8},
                        TextBlock {styles.TextBlock.Subtitle, Margin {0, 0, 0, 8}, object.title},
                        StackPanel {orientation.horizontal, TextBlock {styles.TextBlock.Caption, object.views},
                                    TextBlock {styles.TextBlock.Caption, u" Views "}},
                        StackPanel {orientation.horizontal, TextBlock {styles.TextBlock.Caption, object.likes},
                                    TextBlock {styles.TextBlock.Caption, u" Likes"}},
                    },
                };
            default:
                return StackPanel {width = 240, orientation.horizontal,
                                   TextBlock {styles.TextBlock.Title, Margin {8, 0, 0, 0}, object.title}};
            }
        };
    }
};
auto const model = gallery::hold<Model>();

auto const* data = &gallery::dataObjects();
model->grid.itemTemplate(Model::templateOf(Kind::Image, data));
model->grid.itemsSource(indexList(static_cast<int64_t>(data->size())));

model->grid.add_onItemClick([model, data](auto const&, ItemClickEventArgs& args) {
    std::u16string words = u"You clicked ";
    words += (*data)[static_cast<size_t>(intOf(args.clickedItem()))].title;
    words += u".";
    model->clicked.text(words);
});
model->grid.add_onSelectionChanged([model](auto const&, auto&&...) {
    std::u16string words = u"You have selected ";
    words += core::to_u16(static_cast<int>(model->grid.selectedItems().size())).plain();
    words += u" item(s).";
    model->selected.text(words);
});

// A check box that turns one property of the grid on and off.
auto const switches = [](char16_t const* title, auto apply) {
    auto box = CheckBox {content = title, isChecked = false};
    box.add_onChecked([apply](auto&&...) { apply(true); });
    box.add_onUnchecked([apply](auto&&...) { apply(false); });
    return box;
};

auto example = Grid {
    rowDefinitions = u"*,auto",
    model->grid,
    StackPanel {row = 1, model->clicked, model->selected},
};

auto options = StackPanel {
    spacing = 4,
    RadioButtons {
        header = u"ItemTemplate",
        RadioButton {content = u"Image"},
        RadioButton {content = u"Icon/Text"},
        RadioButton {content = u"Image/Text"},
        RadioButton {content = u"Text"},
        selectedIndex = 0,
        onSelectionChanged = [model, data](RadioButtons const& self) {
            static constexpr Kind kinds[] = {Kind::Image, Kind::IconText, Kind::ImageText, Kind::Text};
            if (self.selectedIndex() >= 0) model->grid.itemTemplate(Model::templateOf(kinds[self.selectedIndex()], data));
        },
    },
    ComboBox {
        header = u"SelectionMode",
        width = 140,
        ComboBoxItem {content = u"None"},
        ComboBoxItem {content = u"Single"},
        ComboBoxItem {content = u"Multiple"},
        ComboBoxItem {content = u"Extended"},
        selectedIndex = 0,
        onSelectionChanged = [model](ComboBox const& self) {
            static constexpr ListViewSelectionMode modes[] = {ListViewSelectionMode::None, ListViewSelectionMode::Single,
                                                              ListViewSelectionMode::Multiple, ListViewSelectionMode::Extended};
            if (self.selectedIndex() >= 0) model->grid.selectionMode(modes[self.selectedIndex()]);
            if (self.selectedIndex() == 0) model->selected.text(u"");
        },
    },
    switches(u"Enable item click", [model](bool on) { model->grid.isItemClickEnabled(on); model->clicked.text(u""); }),
    switches(u"Enable drag", [model](bool on) { model->grid.canDragItems(on); }),
    switches(u"Enable reorder", [model](bool on) { model->grid.canReorderItems(on); }),
    switches(u"Enable drop", [model](bool on) { model->grid.allowDrop(on); }),
    switches(u"Right-to-left flow direction", [model](bool on) {
        model->grid.flowDirection(on ? FlowDirection::RightToLeft : FlowDirection::LeftToRight);
    }),
};