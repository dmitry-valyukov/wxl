// There is no implicit style in a description: what applies to every control of a type is a function that makes
// the control, so no control is made without it.
auto const caption = [](char16_t const* words) {
    return TextBlock {fontSize = 16, fontFamily = u"Consolas", fontWeight = FontWeight {700}, words};
};

auto example = StackPanel {
    caption(u"This style is applied automatically!"),
    caption(u"No need to set a key."),
};
