auto const compositor = CompositionTarget::getCompositorForCurrentThread();
auto const spring = compositor.createSpringVector3Animation();
spring.target(u"Scale");
spring.dampingRatio(0.6f);
spring.period(std::chrono::milliseconds {50});
auto const run = [spring](UIElement const& element, float finalValue) {
    spring.finalValue(Vector3 {finalValue, finalValue, finalValue});
    element.startAnimation(spring);
};

auto const rectangle = Rectangle {width = 50, height = 50, fill = resourceBrush(u"SystemAccentColor"),
                                  onPointerEntered = [run](Rectangle const& self) { run(self, 1.5f); },
                                  onPointerExited = [run](Rectangle const& self) { run(self, 1.0f); }};
auto const ellipse = Ellipse {column = 1, width = 50, height = 50, Margin {55, 0}, fill = resourceBrush(u"SystemAccentColor")};

// The scale of the circle is worked out from the scale of the square, by an expression: the one is the other's inverse.
auto const expression = compositor.createExpressionAnimation();
expression.expression(u"Vector3(1/scaleElement.Scale.X, 1/scaleElement.Scale.Y, 1)");
expression.target(u"Scale");
expression.setExpressionReferenceParameter(u"scaleElement", rectangle);
ellipse.startAnimation(expression);

auto example = StackPanel {
    height = 200,
    TextBlock {styles.TextBlock.Body, u"Hover over the square to animate its scale. Notice that the ellipse also animates."},
    TextBlock {Margin {0, 0, 0, 12}, u"The scale of the circle is inversely related to the scale of the square."},
    Grid {vAlign.top, columnDefinitions = u"*,*", rectangle, ellipse},
};