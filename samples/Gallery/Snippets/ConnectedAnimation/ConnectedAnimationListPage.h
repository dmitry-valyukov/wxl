// A list of pictures, and a page for the picture clicked. The list holds the position of each object of the
// application, and what is clicked is that position, boxed: the same boxes serve as the items of the list again when the
// list page is made anew, which is how the animation back finds the item.
auto const objects = &gallery::dataObjects();
auto const items = indexList(static_cast<int64_t>(objects->size()));

// The picture of an item: the first child of the grid the function made for it. ListView has a method for an element of its
// template by name; for one made by a function the way is wxl::itemElement.
auto const connectedImage = [](ListView const& list, Object const& item) -> core::nullable<UIElement> {
    if (auto const element = itemElement(list, item)) {
        return element->try_as<Grid>().children()[0];
    }
    return {};
};

struct Model {
    gallery::PagedFrame frame {Frame {height = 750, minWidth = 500}};
    ListView list;
    core::nullable<Object> stored;  // the item clicked, nothing before the first click
    gallery::DetailedInfoPage detail;
};
auto const model = gallery::hold<Model>();

auto const listPage = [model, objects, items, connectedImage] {
    model->list = ListView {
        isItemClickEnabled = true,
        selectionMode = ListViewSelectionMode::None,
        itemTemplate = [objects](Object const& item) {
            auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
            return Grid {Margin {0, 12, 0, 12},
                         columnDefinitions = u"auto,*",
                         Image {name = u"connectedElement", maxHeight = 100, minWidth = 150, stretch = Stretch::Fill, source = object.imageLocation},
                         StackPanel {column = 1, Margin {12, 0, 0, 0},
                                     TextBlock {Margin {0, 0, 0, 6}, hAlign.left, styles.TextBlock.Subtitle, object.title},
                                     StackPanel {orientation.horizontal,
                                                 TextBlock {FontWeight {700}, styles.TextBlock.Caption, u"Views: "},
                                                 TextBlock {Margin {5, 0, 0, 0}, styles.TextBlock.Caption, object.views},
                                                 TextBlock {Margin {8, 0, 0, 0}, FontWeight {700}, styles.TextBlock.Caption, u"Likes: "},
                                                 TextBlock {Margin {5, 0, 0, 0}, styles.TextBlock.Caption, object.likes}},
                                     StackPanel {orientation.horizontal,
                                                 TextBlock {maxWidth = 500, maxHeight = 40, Margin {0, 8, 0, 0}, fontStyle = FontStyle::Italic,
                                                            styles.TextBlock.Body, textTrimming = TextTrimming::CharacterEllipsis,
                                                            textWrapping = TextWrapping::Wrap, object.description}}}};
        },
        itemsSource = items,
        onLoaded = [model, connectedImage](auto&&...) {
            if (!model->stored) {
                return;
            }
            // Back from the detail page: the item may be outside the viewport, so it is scrolled into view, and the animation back
            // goes to its picture -- once the list has laid itself out, which is after this handler.
            model->list.scrollIntoView(*model->stored, ScrollIntoViewAlignment::Default);
            DispatcherQueue::getForCurrentThread().tryEnqueue(DispatcherQueuePriority::Low, [model, connectedImage] {
                if (auto const animation = ConnectedAnimationService::getForCurrentView().getAnimation(u"BackConnectedAnimation")) {
                    animation.configuration(DirectConnectedAnimationConfiguration {});
                    if (auto const image = connectedImage(model->list, *model->stored)) {
                        animation.tryStart(*image);
                    }
                }
                model->list.focus(FocusState::Programmatic);
            });
        },
        onItemClick = [model, connectedImage](auto const&, ItemClickEventArgs& args) {
            model->stored = args.clickedItem();
            // Prepared with the picture of the item; the animation starts on the detail page.
            if (auto const image = connectedImage(model->list, *model->stored)) {
                ConnectedAnimationService::getForCurrentView().prepareToAnimate(u"ForwardConnectedAnimation", *image);
            }
            model->frame.forward(
                [model] {
                    auto const& object = gallery::dataObjects()[static_cast<size_t>(intOf(*model->stored))];
                    model->detail = gallery::detailedInfoPage(object);
                    model->detail.goBack.add_onClick([model](auto&&...) {
                        ConnectedAnimationService::getForCurrentView().prepareToAnimate(u"BackConnectedAnimation", model->detail.image);
                        model->frame.back();
                    });
                    return model->detail.root;
                },
                SuppressNavigationTransitionInfo {});
            // The picture of the detail page is where the animation ends; the panel beside it comes in with it.
            if (auto const animation = ConnectedAnimationService::getForCurrentView().getAnimation(u"ForwardConnectedAnimation")) {
                animation.tryStart(model->detail.image, std::vector<UIElement> {model->detail.coordinated});
            }
        },
    };
    return model->list;
};
model->frame.forward(listPage);

auto example = model->frame.frame();