auto const compositor = CompositionTarget::getCompositorForCurrentThread();
auto const panel = Grid {width = 200, height = 200, Margin {12}};

// Buttons laid out in a circle: X = radius * cos(theta) + xOffset, Y = radius * sin(theta) + yOffset, the radius is half
// the size of the parent, theta the angle of each element.
std::u16string const radius = u"(source.ActualSize.X / 2)";
std::u16string const theta = u".02 * " + radius + u" + ((2 * Pi)/total)*index";
std::u16string const expression = u"Vector3(" + radius + u"*cos(" + theta + u")+" + radius + u", " + radius + u"*sin(" + theta + u")+0,0)";

constexpr int total = 8;
for (int i = 0; i < total; ++i) {
    auto const button = Button {content = u"Button"};
    panel.children().append(button);

    auto const animation = compositor.createExpressionAnimation();
    animation.expression(expression);
    animation.setScalarParameter(u"index", static_cast<float>(i + 1));
    animation.setScalarParameter(u"total", static_cast<float>(total));
    animation.target(u"Translation");
    animation.setExpressionReferenceParameter(u"source", panel);
    button.startAnimation(animation);
}

auto example = panel;
auto options = Slider {minWidth = 150, header = u"Change radius", maximum = 400, minimum = 200,
                       onValueChanged = [panel](Slider const& self) {
                           panel.width(self.value());
                           panel.height(self.value());
                       }};