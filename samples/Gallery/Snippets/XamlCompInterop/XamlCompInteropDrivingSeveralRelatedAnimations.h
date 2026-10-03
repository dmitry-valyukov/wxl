auto const compositor = CompositionTarget::getCompositorForCurrentThread();
auto const spring = compositor.createSpringVector3Animation();
spring.target(u"Scale");
spring.dampingRatio(0.6f);
spring.period(std::chrono::milliseconds {50});
auto const run = [spring](UIElement const& element, float finalValue) {
    spring.finalValue(Vector3 {finalValue, finalValue, finalValue});
    element.startAnimation(spring);
};

auto const item = [run](char16_t const* title) {
    return Button {width = 100, height = 50, Margin {5}, content = title,
                   onPointerEntered = [run](Button const& self) { run(self, 1.5f); },
                   onPointerExited = [run](Button const& self) { run(self, 1.0f); }};
};
std::vector<Button> buttons = {item(u"Item 1"), item(u"Item 2"), item(u"Item 3"), item(u"Item 4")};

// Each button after the first animates as a function of the one above it: its scale and translation.
auto const expression = compositor.createExpressionAnimation();
expression.expression(u"(above.Scale.Y - 1) * 50 + above.Translation.Y % (50 * index)");
expression.target(u"Translation.Y");
for (size_t i = 1; i < buttons.size(); ++i) {
    expression.setExpressionReferenceParameter(u"above", buttons[i - 1]);
    expression.setScalarParameter(u"index", static_cast<float>(i));
    buttons[i].startAnimation(expression);
}

auto example = StackPanel {
    Margin {0, 0, 0, 50},
    TextBlock {styles.TextBlock.Body, u"Hover over any button to animate its scale. Notice that the other buttons move out of the way."},
    TextBlock {Margin {0, 0, 0, 12}, u"Each button animates as a function of the previous button's scale and translation."},
    buttons[0],
    buttons[1],
    buttons[2],
    buttons[3],
};