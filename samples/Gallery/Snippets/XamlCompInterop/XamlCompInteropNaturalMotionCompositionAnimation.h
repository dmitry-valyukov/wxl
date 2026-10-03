auto const compositor = CompositionTarget::getCompositorForCurrentThread();
auto const spring = compositor.createSpringVector3Animation();
spring.target(u"Scale");

struct Model {
    RadioButtons damping {header = u"Damping Ratio", RadioButton {content = u"0.2"}, RadioButton {content = u"0.4"},
                          RadioButton {content = u"0.6"}, RadioButton {content = u"0.8"}, selectedIndex = 2};
    Slider period {header = u"Period (in ms)", maximum = 200, minimum = 25, stepFrequency = 25, tickFrequency = 25, value = 50};
};
auto const model = gallery::hold<Model>();

// The spring is set up anew each time: where it ends up, the damping and the period of the options.
auto const run = [compositor, spring, model](UIElement const& element, float finalValue) {
    static constexpr float ratios[] = {0.2f, 0.4f, 0.6f, 0.8f};
    spring.finalValue(Vector3 {finalValue, finalValue, finalValue});
    spring.dampingRatio(model->damping.selectedIndex() >= 0 ? ratios[model->damping.selectedIndex()] : 0.6f);
    spring.period(std::chrono::milliseconds {static_cast<int>(model->period.value())});
    element.startAnimation(spring);
};

auto example = StackPanel {
    vAlign.stretch,
    TextBlock {Margin {0, 0, 0, 12}, styles.TextBlock.Body, u"Hover over the button to animate its scale."},
    Button {width = 100, height = 50, content = u"Item",
            onPointerEntered = [run](Button const& self) { run(self, 1.5f); },
            onPointerExited = [run](Button const& self) { run(self, 1.0f); }},
};
auto options = StackPanel {model->damping, model->period};