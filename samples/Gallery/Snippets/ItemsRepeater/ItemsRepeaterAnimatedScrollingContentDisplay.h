// The colors of the original are names; here each is a color of the application's own, and the repeater is
// given the position of each.
auto const* colors = &gallery::namedColors();

struct Model {
    ScrollViewer scroll {width = 250, height = 175};
    ItemsRepeater repeater;
    Rectangle swatch {column = 1, width = 150, height = 150, Margin {10, 0, 0, 0}, stroke = brushes.SystemControl.Foreground.Base.High};
    int previouslyFocusedIndex = -1;
};
auto const model = gallery::hold<Model>();

auto const colorButton = [model, colors](Object const& item) {
    auto const& entry = (*colors)[static_cast<size_t>(intOf(item))];
    return Button {
        hAlign.stretch,
        background = entry.color,
        foreground = brushes.Text.FillColor.Inverse,
        content = entry.name,
        onClick = [model](Button const& self) { model->swatch.fill(self.background()); },
        onGotFocus = [model](Button const& self) {
            // Remember the index, so focus lands on it again when it leaves the repeater and comes back.
            model->previouslyFocusedIndex = model->repeater.getElementIndex(self);
            BringIntoViewOptions options;
            options.verticalAlignmentRatio(0.5);
            options.animationDesired(true);
            self.startBringIntoView(options);
        },
    };
};

model->repeater = ItemsRepeater {
    itemTemplate = colorButton,
    itemsSource = indexList(static_cast<int64_t>(colors->size())),
    onGettingFocus = [model](auto const&, GettingFocusEventArgs& args) {
        // Focus coming into the repeater from outside goes to the item that had it last.
        auto const lastFocus = args.oldFocusedElement().try_as<UIElement>();
        if (model->previouslyFocusedIndex != -1 && lastFocus && model->repeater.getElementIndex(lastFocus) == -1) {
            args.newFocusedElement(model->repeater.tryGetElement(model->previouslyFocusedIndex));
        }
    },
    onKeyDown = [model](auto const&, KeyRoutedEventArgs& args) {
        if (args.handled()) {
            return;
        }
        int target = -1;
        int const last = model->repeater.itemsSourceView().count() - 1;
        if (args.key() == VirtualKey::Home) {
            target = model->previouslyFocusedIndex != 0 ? 0 : -1;
        } else if (args.key() == VirtualKey::End) {
            target = model->previouslyFocusedIndex != last ? last : -1;
        }
        if (target != -1) {
            auto const element = model->repeater.getOrCreateElement(target);
            element.startBringIntoView();
            if (auto const control = element.try_as<Control>()) {
                control.focus(FocusState::Programmatic);
            }
            args.handled(true);
        }
    },
    // Each element, as it is made ready, scales with its distance from the middle of the viewport.
    onElementPrepared = [model](auto const&, ItemsRepeaterElementPreparedEventArgs& args) {
        auto const item = ElementCompositionPreview::getElementVisual(args.element());
        auto const viewport = ElementCompositionPreview::getElementVisual(model->scroll);
        auto const scrollProperties = ElementCompositionPreview::getScrollViewerManipulationPropertySet(model->scroll);
        auto const compositor = scrollProperties.compositor();

        auto const scale = compositor.createExpressionAnimation();
        scale.setReferenceParameter(u"svVisual", viewport);
        scale.setReferenceParameter(u"scrollProperties", scrollProperties);
        scale.setReferenceParameter(u"item", item);
        scale.expression(u"1 - abs((svVisual.Size.Y/2 - scrollProperties.Translation.Y) - (item.Offset.Y + item.Size.Y/2))*(.25/(svVisual.Size.Y/2))");
        item.startAnimation(u"Scale.X", scale);
        item.startAnimation(u"Scale.Y", scale);

        auto const centerPoint = compositor.createExpressionAnimation();
        centerPoint.setReferenceParameter(u"item", item);
        centerPoint.expression(u"Vector3(item.Size.X/2, item.Size.Y/2, 0)");
        item.startAnimation(u"CenterPoint", centerPoint);
    },
};
model->scroll.content(model->repeater);

auto example = Grid {columnDefinitions = u"*,*", model->scroll, model->swatch};