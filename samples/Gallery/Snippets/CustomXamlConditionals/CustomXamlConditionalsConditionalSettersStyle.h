// The setters of a style that depend on a flag: a preset made by a function of the flag.
auto const heading = [](bool newExperience) {
    return Preset {fontWeight = FontWeight {static_cast<uint16_t>(newExperience ? 600 : 400)}, fontSize = newExperience ? 28.0 : 18.0};
};

auto const headingText = TextBlock {u"Heading styled with conditional Setters"};
heading(flags->newExperience.get())(headingText);
flags->newExperience.on_change([headingText, heading](bool on) noexcept { heading(on)(headingText); });

auto example = StackPanel {spacing = 8.0, headingText};
