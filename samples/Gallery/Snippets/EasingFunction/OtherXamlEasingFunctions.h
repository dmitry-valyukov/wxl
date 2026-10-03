// The easing functions of XAML, each with its name; the one chosen in the combo box gets the mode chosen
// in the radio buttons when the rectangle is sent off.
struct Named {
    char16_t const* name;
    EasingFunctionBase function;
};
auto const functions = std::make_shared<std::vector<Named>>(std::vector<Named> {
    {u"BackEase", BackEase {}},
    {u"BounceEase", BounceEase {}},
    {u"CircleEase", CircleEase {}},
    {u"CubicEase", CubicEase {}},
    {u"ElasticEase", ElasticEase {}},
    {u"ExponentialEase", ExponentialEase {}},
    {u"PowerEase", PowerEase {}},
    {u"QuadraticEase", QuadraticEase {}},
    {u"QuarticEase", QuarticEase {}},
    {u"QuinticEase", QuinticEase {}},
    {u"SineEase", SineEase {}},
});

struct Model {
    TranslateTransform translate;
    DoubleAnimation animation {duration = Duration {std::chrono::milliseconds {500}, DurationType::TimeSpan}};
    Storyboard storyboard;
    ComboBox choice {selectedIndex = 0};
    int mode = 0;
};
auto const model = gallery::hold<Model>();
model->storyboard.children().append(model->animation);
Storyboard::setTarget(model->animation, model->translate);
Storyboard::setTargetProperty(model->animation, u"X");
for (auto const& each : *functions) {
    model->choice.items().append(ComboBoxItem {content = each.name});
}

auto example = Grid {
    minWidth = 420,
    columnDefinitions = u"auto,*",
    Button {content = u"Animate", onClick = [model, functions](auto&&...) {
                if (model->choice.selectedIndex() < 0) {
                    return;
                }
                auto const& ease = (*functions)[static_cast<size_t>(model->choice.selectedIndex())].function;
                static constexpr EasingMode modes[] = {EasingMode::EaseOut, EasingMode::EaseIn, EasingMode::EaseInOut};
                ease.easingMode(modes[model->mode]);
                model->animation.easingFunction(ease);
                model->animation.from(model->translate.x());
                model->animation.to(model->translate.x() > 0 ? 0.0 : 200.0);
                model->storyboard.begin();
            }},
    Rectangle {column = 1, width = 50, height = 50, hAlign.left, fill = brushes.Accent.FillColor.Default, renderTransform = model->translate},
};

auto options = StackPanel {
    model->choice,
    RadioButtons {RadioButton {content = u"EaseOut"}, RadioButton {content = u"EaseIn"}, RadioButton {content = u"EaseInOut"},
                  selectedIndex = 0, onSelectionChanged = [model](RadioButtons const& self) { model->mode = std::max(self.selectedIndex(), 0); }},
};