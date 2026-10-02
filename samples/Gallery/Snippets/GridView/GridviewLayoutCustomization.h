struct Model {
    GridView grid;
    NumberBox columnSpace {header = u"Space between columns", minimum = 0.0, maximum = 100.0, value = 5.0,
                           spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, smallChange = 1.0, maxWidth = 250,
                           Margin {0, 0, 0, 16}};
    NumberBox rowSpace {header = u"Space between rows", minimum = 0.0, maximum = 100.0, value = 5.0,
                        spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, smallChange = 1.0, maxWidth = 250,
                        Margin {0, 0, 0, 16}};
    NumberBox wrapCount {header = u"Maximum number of items before wrapping", minimum = 1.0, maximum = 8.0, value = 3.0,
                         spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, smallChange = 1.0, maxWidth = 250,
                         Margin {0, 0, 0, 16}};

    // What the ItemContainerStyle of the original sets (a Margin) is a tag of the control: the margin
    // of every item container, the ones on screen and the ones made later.
    void space() {
        auto const column = static_cast<float>(columnSpace.value());
        auto const row = static_cast<float>(rowSpace.value());
        grid.itemMargin(Thickness {column, row, column, row});
    }

    // The panel of a GridView is an ItemsWrapGrid, which the control hands out once it is loaded.
    void wrap() {
        if (auto const panel = grid.itemsPanelRoot().try_as<ItemsWrapGrid>()) {
            panel.maximumRowsOrColumns(static_cast<int>(wrapCount.value()));
        }
    }
};
auto const model = gallery::hold<Model>();

auto const* data = &gallery::dataObjects();

// The template of the original: a picture with a strip over it that carries the title and the likes.
model->grid = GridView {
    itemMargin = Thickness {5},
    itemTemplate = [data](Object const& item) {
        auto const& object = (*data)[static_cast<size_t>(intOf(item))];
        return Grid {
            width = 100,
            Image {stretch = Stretch::UniformToFill, source = object.imageLocation},
            StackPanel {
                height = 40,
                Padding {5, 1, 5, 1},
                verticalAlignment = VerticalAlignment::Bottom,
                background = brushes.SystemControl.Background.Base.Medium,
                opacity = 0.75,
                orientation.vertical,
                TextBlock {foreground = brushes.SystemControl.Foreground.AltHigh, object.title},
                StackPanel {
                    orientation.horizontal,
                    TextBlock {styles.TextBlock.Caption, foreground = brushes.SystemControl.Foreground.AltHigh, object.likes},
                    TextBlock {styles.TextBlock.Caption, foreground = brushes.SystemControl.Foreground.AltHigh, u" Likes"},
                },
            },
        };
    },
    itemsSource = indexList(static_cast<int64_t>(data->size())),
    onLoaded = [model](auto&&...) { model->wrap(); },
};

auto const respond = [model](NumberBox const&, NumberBoxValueChangedEventArgs&) {
    model->space();
    model->wrap();
};
model->columnSpace.add_onValueChanged(respond);
model->rowSpace.add_onValueChanged(respond);
model->wrapCount.add_onValueChanged(respond);

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 15}, textWrapping = TextWrapping::Wrap,
               u"Use the options on the right to control different layout customizations to the GridView below."},
    model->grid,
};

auto options = StackPanel {model->columnSpace, model->rowSpace, model->wrapCount};