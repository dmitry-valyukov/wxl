// A grid of pictures; a click shows the picture large over the grid, on the same page, flying from its tile.
auto const objects = &gallery::dataObjects(true);

struct Model {
    GridView collection {maxWidth = 1400, hAlign.center, isItemClickEnabled = true};
    Grid smoke {hAlign.stretch, vAlign.stretch, background = brushes.SmokeFillColorDefault, visibility = Visibility::Collapsed};
    Image image {stretch = Stretch::UniformToFill};
    TextBlock title {styles.TextBlock.Subtitle};
    TextBlock description {Margin {0, 4, 0, 0}, foreground = brushes.Text.FillColor.Secondary, maxLines = 3, styles.TextBlock.Body,
                           textWrapping = TextWrapping::Wrap};
    Grid destination;
    core::nullable<Object> stored;
};
auto const model = gallery::hold<Model>();

auto const items = indexList(static_cast<int64_t>(objects->size()));
model->collection.itemMargin(Thickness {4});
model->collection.itemTemplate([objects](Object const& item) {
    auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
    return Grid {name = u"connectedElement", width = 150, height = 110, CornerRadius {4},
                 Image {source = object.imageLocation, stretch = Stretch::UniformToFill},
                 Border {Padding {8, 4}, vAlign.bottom, background = brushes.Acrylic.BackgroundFillColor.Base,
                         TextBlock {styles.TextBlock.Caption, object.title}}};
});
model->collection.itemsSource(items);

auto const back = [model](auto&&...) {
    auto const animation = ConnectedAnimationService::getForCurrentView().prepareToAnimate(u"backwardsAnimation", model->destination);
    model->smoke.children().clear();
    animation.add_onCompleted([model](auto&&...) {
        model->smoke.visibility(Visibility::Collapsed);
        model->smoke.children().append(model->destination);
    });
    // The item may be outside the viewport: it is scrolled into view first, and the animation goes to its tile once the grid has laid itself out.
    model->collection.scrollIntoView(*model->stored, ScrollIntoViewAlignment::Default);
    animation.configuration(DirectConnectedAnimationConfiguration {});
    DispatcherQueue::getForCurrentThread().tryEnqueue(DispatcherQueuePriority::Low, [model, animation] {
        if (auto const tile = itemElement(model->collection, *model->stored)) {
            animation.tryStart(*tile);
        }
    });
};

model->destination = Grid {
    width = 400,
    height = 320,
    hAlign.center,
    vAlign.center,
    borderBrush = brushes.Card.StrokeColorDefault,
    BorderThickness {1},
    CornerRadius {8},
    rowDefinitions = u"*,auto",
    model->image,
    Button {width = 36, height = 36, Margin {8}, hAlign.right, vAlign.top, toolTip = u"Close", onClick = back,
            content = FontIcon {fontSize = 14, glyph = u"\xE711"}},
    StackPanel {row = 1, Padding {16, 12}, background = brushes.Card.BackgroundFillColor.Default, model->title, model->description},
};
model->smoke.children().append(model->destination);

model->collection.add_onItemClick([model, objects](auto const&, ItemClickEventArgs& args) {
    model->stored = args.clickedItem();
    if (auto const tile = itemElement(model->collection, *model->stored)) {
        ConnectedAnimationService::getForCurrentView().prepareToAnimate(u"forwardAnimation", *tile);
    }
    auto const& object = (*objects)[static_cast<size_t>(intOf(*model->stored))];
    model->image.source(ImageSource {object.imageLocation});
    model->title.text(object.title);
    model->description.text(object.description);
    model->smoke.visibility(Visibility::Visible);
    if (auto const animation = ConnectedAnimationService::getForCurrentView().getAnimation(u"forwardAnimation")) {
        animation.tryStart(model->destination);
    }
});

auto example = Grid {model->collection, model->smoke};