auto const pictures = [](auto&&... extra) {
    return FlipView {
        height = 270,
        maxWidth = 400,
        Image {source = u"Assets/SampleMedia/cliff.jpg"},
        Image {source = u"Assets/SampleMedia/grapes.jpg"},
        Image {source = u"Assets/SampleMedia/rainier.jpg"},
        Image {source = u"Assets/SampleMedia/sunset.jpg"},
        Image {source = u"Assets/SampleMedia/valley.jpg"},
        extra...,
    };
};
// ItemsPanel is a template that only XAML text makes; the direction is told to the control instead.
auto example = pictures(itemsPanelOrientation = orientation.vertical);