auto const translate = TranslateTransform {};
auto const animation = DoubleAnimation {duration = Duration {std::chrono::milliseconds {300}, DurationType::TimeSpan},
                                        easingFunction = ExponentialEase {easingMode = EasingMode::EaseOut, exponent = 7.0}};
auto const storyboard = Storyboard {animation};
Storyboard::setTarget(animation, translate);
Storyboard::setTargetProperty(animation, u"X");

auto const animate = [translate, animation, storyboard](auto&&...) {
    animation.from(translate.x());
    animation.to(translate.x() > 0 ? 0.0 : 200.0);
    storyboard.begin();
};

auto example = Grid {
    minWidth = 420,
    columnDefinitions = u"auto,*",
    Button {content = u"Animate", automationName = u"Animate rectangle using Decelerate Easing Function", onClick = animate},
    Rectangle {column = 1, width = 50, height = 50, hAlign.left, fill = resourceBrush(u"SystemAccentColor"), renderTransform = translate},
};

auto options = NumberBox {header = u"Exponent", automationName = u"Decelerate easing exponent", value = 7.0, onValueChanged = [animation](NumberBox const& self) {
                              if (auto const ease = animation.easingFunction().try_as<ExponentialEase>()) {
                                  ease.exponent(self.value());
                              }
                          }};