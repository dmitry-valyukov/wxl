struct Model {
    bool archived = false;
};
auto const model = gallery::hold<Model>();

auto swipe = SwipeControl {
    width = 500,
    height = 68,
    Margin {12},
    borderBrush = brushes.Control.FillColor.Default,
    BorderThickness {1},
    rightItemsMode = SwipeMode::Execute,
    rightItems[SwipeItem {
        behaviorOnInvoked = SwipeBehaviorOnInvoked::Close,
        iconSource = FontIconSource {glyph = u"\uE7B8"},
        text = u"Archive",
        onInvoked = [model](SwipeItem const&, SwipeItemInvokedEventArgs& args) {
            model->archived = !model->archived;
            args.swipeControl().content().try_as<TextBlock>().text(model->archived ? u"Archived - Swipe Left" : u"Swipe Left");
        },
    }],
    content = TextBlock {Margin {12}, hAlign.center, vAlign.center, u"Swipe Left"},
};