auto const button = Button {content = u"Show TeachingTip"};
auto const tip = TeachingTip {
    title = u"This is the title",
    preferredPlacement = TeachingTipPlacementMode::Bottom,
    subtitle = u"And this is the subtitle",
    target = button,
    heroContent = Image {source = u"Assets/SampleMedia/sunset.jpg"},
    content = TextBlock {Margin {0, 16, 0, 0}, textWrapping = TextWrapping::WrapWholeWords, u"Description can go here"},
};
button.add_onClick([tip](auto&&...) { tip.isOpen(true); });

auto example = Grid {button, tip};