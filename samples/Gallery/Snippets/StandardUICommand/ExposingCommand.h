// One Delete command, shared: the menu item, the swipe item and the button
// that shows on hover all take it, and it comes with its icon, label, key and
// description. The model owns the list and the command; nothing the list
// holds owns the model.
struct Model {
    ListView list;
    std::vector<std::u16string> names;
    StandardUICommand deleteCommand {
        kind = StandardUICommandKind::Delete,
        onExecuteRequested = [this](XamlUICommand const&, ExecuteRequestedEventArgs& args) {
            // From a swipe item or a button: the row named. From the menu, which has no row in
            // hand: the row selected.
            if (args.parameter() && args.parameter().is_text()) {
                std::erase_if(names, [&](auto const& name) { return args.parameter().text() == std::u16string_view {name}; });
            } else if (auto const selected = list.selectedIndex(); selected >= 0) {
                names.erase(names.begin() + selected);
            }
            fill();
        },
    };

    Model() {
        for (char16_t index = 0; index < 15; ++index) {
            std::u16string name {u"List item "};
            if (index >= 10) {
                name += u'1';
            }
            names.push_back(name + static_cast<char16_t>(u'0' + index % 10));
        }
    }

    // A row: the text, and a button that shows while the pointer is over it;
    // swiped left it deletes, and so does a right click on the menu.
    FrameworkElement row(std::u16string const& name) const {
        return Grid {
            onPointerEntered = [](Grid const& self, PointerRoutedEventArgs& args) {
                auto const pointer = args.pointer().pointerDeviceType();
                if (pointer == PointerDeviceType::Mouse || pointer == PointerDeviceType::Pen) {
                    self.children()[0].try_as<SwipeControl>().content().try_as<Grid>().children()[1].visibility(Visibility::Visible);
                }
            },
            onPointerExited = [](Grid const& self) {
                self.children()[0].try_as<SwipeControl>().content().try_as<Grid>().children()[1].visibility(Visibility::Collapsed);
            },
            SwipeControl {
                rightItemsMode = SwipeMode::Execute,
                rightItems[SwipeItem {background = colors.red, command = deleteCommand, commandParameter = name}],
                content = Grid {
                    vAlign.center,
                    TextBlock {Margin {10}, hAlign.left, vAlign.center, fontSize = 18.0, text = name},
                    AppBarButton {
                        hAlign.right,
                        command = deleteCommand,
                        commandParameter = name,
                        isTabStop = false,
                        visibility = Visibility::Collapsed,
                    },
                },
            },
        };
    }

    void fill() {
        list.items().clear();
        for (auto const& name : names) {
            list.items().append(row(name));
        }
    }
};
auto const model = gallery::hold<Model>();

auto example = [&] {
    model->list.height(500);
    Grid::setRow(model->list, 2);
    model->list.isItemClickEnabled(true);
    model->list.selectionMode(ListViewSelectionMode::Single);
    model->fill();

    return Grid {
        hAlign.stretch,
        rowDefinitions = u"auto,auto,*",
        TextBlock {
            Margin {0, 0, 0, 12},
            textWrapping = TextWrapping::Wrap,
            u"StandardUICommand allows the sharing of the UX associated with a command. "
            u"In this instance we are using a StandardUICommand to quickly place the delete command in multiple controls. "
            u"The StandardUICommand contains the icon, label, keyboard shortcut, and a description.",
        },
        MenuBar {
            row = 1,
            MenuBarItem {
                title = u"File",
                MenuFlyoutItem {text = u"New"},
                MenuFlyoutItem {text = u"Open..."},
                MenuFlyoutItem {text = u"Save"},
                MenuFlyoutItem {text = u"Exit"},
            },
            MenuBarItem {title = u"Edit", MenuFlyoutItem {command = model->deleteCommand}},
            MenuBarItem {title = u"Help", MenuFlyoutItem {text = u"About"}},
        },
        model->list,
    };
}();