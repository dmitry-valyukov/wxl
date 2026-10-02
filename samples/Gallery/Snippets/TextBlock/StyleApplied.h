// The style is a preset: a name given to a set of properties.
const Preset custom {
    fontFamily = u"Comic Sans MS",
    fontStyle = FontStyle::Italic,
};

auto styled = TextBlock {custom, u"I am a styled TextBlock."};