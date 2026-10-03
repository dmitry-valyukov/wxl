auto const compositor = CompositionTarget::getCompositorForCurrentThread();

struct Model {
    Slider fontSize {header = u"Change font size", maximum = 24, minimum = 12};
    Slider margin {header = u"Change text margin", maximum = 100, minimum = 0};
    TextBlock target {
        width = 300,
        textWrapping = TextWrapping::WrapWholeWords,
        u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
        u"enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
        u"reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt "
        u"in culpa qui officia deserunt mollit anim id est laborum.",
    };
    // Notice that the popup is a sibling of the target, not its ancestor.
    Popup popup {Margin {5}};
};
auto const model = gallery::hold<Model>();

model->popup.child(Grid {minWidth = 50, minHeight = 50, maxWidth = 200, background = brushes.SystemControl.Background.Chrome.Medium,
                         borderBrush = brushes.SystemControl.Foreground.Base.High, BorderThickness {2},
                         TextBlock {Margin {6}, vAlign.center, styles.TextBlock.Caption, textWrapping = TextWrapping::WrapWholeWords,
                                    u"I am always right aligned center to the target."}});
model->fontSize.add_onValueChanged([model](Slider const& self) { model->target.fontSize(self.value()); });
model->margin.add_onValueChanged([model](Slider const& self) { model->target.margin(Thickness {self.value()}); });

// The popup is positioned by an expression: beside the text, in the middle of its height, whatever the text's size and place.
auto const animation = compositor.createExpressionAnimation();
animation.expression(u"Vector3(source.ActualOffset.X + source.ActualSize.X, source.ActualOffset.Y + source.ActualSize.Y / 2 - 25, 0)");
animation.target(u"Translation");
animation.setExpressionReferenceParameter(u"source", model->target);
// The popup is opened, and animated, once it is in the tree: an unparented popup has no XamlRoot.
auto const open = [model, animation](auto&&...) {
    model->popup.startAnimation(animation);
    model->popup.isOpen(true);
};

auto example = StackPanel {
    TextBlock {styles.TextBlock.Body, textWrapping = TextWrapping::WrapWholeWords,
               u"This sample positions a popup relative to a block of text that has variable layout size based on font size. Use the "
               u"sliders to move and resize the text."},
    Grid {Margin {0, 12, 0, 0}, hAlign.left, model->target, model->popup, onLoaded = open},
};
auto options = StackPanel {minWidth = 150, model->fontSize, model->margin};