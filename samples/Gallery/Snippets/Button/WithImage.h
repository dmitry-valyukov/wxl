auto output = TextBlock {};

auto button = Button {
    width = 50,
    height = 50,
    automationName = u"Pie",
    content = Image {automationName = u"Slice", source = u"Assets/Slices.png"},
    onClick = [output] { output.text(u"You clicked: Button2"); },
};
