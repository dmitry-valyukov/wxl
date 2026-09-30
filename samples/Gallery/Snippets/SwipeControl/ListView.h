// The list holds its names in the model and is filled from them; Delete takes
// the name out and fills it again. The model owns the list, and nothing the
// list holds owns the model.
struct Model {
    ListView list;
    std::vector<std::u16string> names {u"Swipe Item 1", u"Swipe Item 2", u"Swipe Item 3", u"Swipe Item 4"};

    void fill() {
        list.items().clear();
        for (auto const& name : names) {
            list.items().append(SwipeControl {
                height = 68,
                minWidth = 200,
                borderBrush = brushes.Control.FillColor.Default,
                BorderThickness {0, 1, 0, 0},
                leftItemsMode = SwipeMode::Reveal,
                rightItemsMode = SwipeMode::Execute,
                leftItems[
                    SwipeItem {
                        background = rgb(0x3e, 0x6f, 0xa7),
                        foreground = colors.white,
                        iconSource = FontIconSource {glyph = u"\uE8C2"},
                        text = u"Reply All",
                    },
                    SwipeItem {
                        background = rgb(0xff, 0x95, 0x01),
                        foreground = colors.white,
                        iconSource = FontIconSource {glyph = u"\uE8C3"},
                        text = u"Open",
                    }
                ],
                rightItems[SwipeItem {
                    background = colors.red,
                    iconSource = FontIconSource {glyph = u"\uE74D"},
                    text = u"Delete",
                    onInvoked = [this](SwipeItem const&, SwipeItemInvokedEventArgs& args) {
                        auto const shown = args.swipeControl().content().try_as<TextBlock>().text();
                        std::erase_if(names, [&](auto const& name) { return shown == std::u16string_view {name}; });
                        fill();
                    },
                }],
                content = TextBlock {
                    Margin {12},
                    hAlign.stretch,
                    vAlign.center,
                    fontSize = 24.0,
                    text = name,
                },
            });
        }
    }
};
auto const model = gallery::hold<Model>();

auto list = [&] {
    model->list.width(800);
    model->list.height(300);
    model->list.minWidth(200);
    model->list.margin(Thickness {12});
    model->fill();
    return model->list;
}();