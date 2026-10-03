// A storyboard of one animation: it moves the X of a TranslateTransform, over half a second, by a CircleEase.
auto const translate = TranslateTransform {};
auto const animation = DoubleAnimation {duration = Duration {std::chrono::milliseconds {500}, DurationType::TimeSpan},
                                        easingFunction = CircleEase {easingMode = EasingMode::EaseInOut}};
auto const storyboard = Storyboard {animation};
Storyboard::setTarget(animation, translate);
Storyboard::setTargetProperty(animation, u"X");

// To the right and back: from where the rectangle is to 200, or to 0 when it is there already.
auto const animate = [translate, animation, storyboard](auto&&...) {
    animation.from(translate.x());
    animation.to(translate.x() > 0 ? 0.0 : 200.0);
    storyboard.begin();
};

auto example = Grid {
    minWidth = 420,
    columnDefinitions = u"auto,*",
    Button {content = u"Animate", onClick = animate},
    Rectangle {column = 1, width = 50, height = 50, hAlign.left, fill = resourceBrush(u"SystemAccentColor"), renderTransform = translate},
};