auto output = TextBlock {};

auto button = Button {
    width = 50,
    height = 50,
    content = Image {source = u"Assets/Slices.png"},
    onClick = [output] { output.text(u"You clicked: Button2"); },
};
