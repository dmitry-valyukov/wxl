// The rectangles are the colors of the original, one after another in a scroll view.
static constexpr Color shades[] = {rgb(240, 248, 255), rgb(250, 235, 215), rgb(0, 255, 255), rgb(127, 255, 212), rgb(240, 255, 255),
                                   rgb(245, 245, 220), rgb(255, 228, 196), rgb(255, 235, 205), rgb(138, 43, 226), rgb(165, 42, 42),
                                   rgb(222, 184, 135), rgb(95, 158, 160), rgb(127, 255, 0), rgb(210, 105, 30), rgb(255, 127, 80),
                                   rgb(100, 149, 237), rgb(255, 248, 220), rgb(220, 20, 60), rgb(0, 255, 255)};
auto const stack = StackPanel {};
for (auto const shade : shades) {
    stack.children().append(Rectangle {height = 150, fill = shade});
}
auto const scroll = ScrollView {width = 150, hAlign.left, content = stack};

auto example = Grid {
    height = 750,
    ParallaxView {hAlign.left, vAlign.top, source = scroll, verticalShift = 500, child = Image {source = u"Assets/SampleMedia/cliff.jpg"}},
    TextBlock {maxWidth = 280, hAlign.center, vAlign.top, fontSize = 28, foreground = colors.white, textWrapping = TextWrapping::WrapWholeWords,
               u"Scroll the rectangles to see parallaxing of image"},
    scroll,
};