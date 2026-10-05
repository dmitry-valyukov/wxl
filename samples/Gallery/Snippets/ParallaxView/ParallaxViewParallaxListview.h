// The titles of all the controls, in a list; the picture behind moves slower than the list scrolls.
auto const titles = gallery::sortedControlTitles();
auto const list = ListView {
    hAlign.stretch,
    vAlign.top,
    background = rgba(0, 0, 0, 0.5),
    itemTemplate = [](Object const& item) { return TextBlock {foreground = brushes.SystemControl.Foreground.AltHigh, stringOf(item)}; },
    itemsSource = stringList(titles),
    header = TextBlock {maxWidth = 280, hAlign.center, vAlign.center, fontSize = 28, foreground = colors.white,
                        textWrapping = TextWrapping::WrapWholeWords, u"Scroll the list to see parallaxing of image"},
};

auto example = Grid {
    height = 750,
    ParallaxView {hAlign.left, vAlign.top, source = list, verticalShift = 500, child = Image {source = u"Assets/SampleMedia/cliff.jpg"}},
    list,
};