// Unlike ListView and GridView, an ItemsRepeater has no connected-animation methods of its own: the element to prepare is
// found by hand -- here it is the first child of the item's grid.
auto const objects = &gallery::dataObjects(true);
auto const items = indexList(static_cast<int64_t>(objects->size()));

struct Model {
    gallery::PagedFrame frame {Frame {height = 400, minWidth = 500}};
    ItemsRepeater repeater;
    ScrollViewer scroll;
    int stored = -1;  // the position of the item tapped
    double offset = 0;  // where the list was scrolled to
    gallery::DetailedInfoPage detail;
};
auto const model = gallery::hold<Model>();

auto const repeaterPage = [model, objects, items] {
    model->repeater = ItemsRepeater {
        layout = UniformGridLayout {minColumnSpacing = 8, minItemHeight = 120, minItemWidth = 150, minRowSpacing = 8},
        itemTemplate = [objects](Object const& item) {
            auto const& object = (*objects)[static_cast<size_t>(intOf(item))];
            return Grid {CornerRadius {4},
                         Image {source = object.imageLocation, stretch = Stretch::UniformToFill},
                         Border {vAlign.bottom, Padding {8, 4}, background = brushes.Acrylic.BackgroundFillColor.Base,
                                 TextBlock {styles.TextBlock.Caption, foreground = brushes.Text.FillColor.Primary, object.title}}};
        },
        itemsSource = items,
        onElementPrepared = [model](auto const&, ItemsRepeaterElementPreparedEventArgs& args) {
            // Tapping an item is what navigates; the handler is given to each element as it is made ready.
            args.element().add_onTapped([model](auto const& sender, TappedRoutedEventArgs&) {
                auto const element = sender.try_as<UIElement>();
                model->stored = model->repeater.getElementIndex(element);
                model->offset = model->scroll.verticalOffset();
                if (auto const grid = element.try_as<Grid>()) {
                    ConnectedAnimationService::getForCurrentView().prepareToAnimate(u"ForwardConnectedAnimation", grid.children()[0]);
                }
                model->frame.forward(
                    [model] {
                        model->detail = gallery::detailedInfoPage(gallery::dataObjects(true)[static_cast<size_t>(model->stored)]);
                        model->detail.goBack.add_onClick([model](auto&&...) {
                            ConnectedAnimationService::getForCurrentView().prepareToAnimate(u"BackConnectedAnimation", model->detail.image);
                            model->frame.back();
                            // The list is made again by back(); it is put to where it was.
                            model->scroll.changeView(core::nullable<double> {}, model->offset, core::nullable<float> {}, true);
                            // The animation goes back to the element of the item.
                            if (auto const animation = ConnectedAnimationService::getForCurrentView().getAnimation(u"BackConnectedAnimation")) {
                                animation.configuration(DirectConnectedAnimationConfiguration {});
                                if (auto const element = model->repeater.tryGetElement(model->stored).try_as<Grid>()) {
                                    animation.tryStart(element.children()[0]);
                                }
                            }
                        });
                        return model->detail.root;
                    },
                    SuppressNavigationTransitionInfo {});
                if (auto const animation = ConnectedAnimationService::getForCurrentView().getAnimation(u"ForwardConnectedAnimation")) {
                    animation.tryStart(model->detail.image, std::vector<UIElement> {model->detail.coordinated});
                }
            });
        },
    };
    model->scroll = ScrollViewer {content = model->repeater};
    return model->scroll;
};
model->frame.forward(repeaterPage);

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"Unlike ListView and GridView, ItemsRepeater does not have built-in ConnectedAnimation methods. Use "
               u"ConnectedAnimationService.PrepareToAnimate() directly, and manually find the target element in the visual tree. Click an item to "
               u"navigate with a connected animation."},
    model->frame.frame(),
};