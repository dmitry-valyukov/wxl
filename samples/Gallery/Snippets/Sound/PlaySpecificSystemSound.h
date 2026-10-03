auto const soundButton = [](char16_t const* label, ElementSoundKind kind) {
    return Button {content = std::u16string {u"\u25B6 "} + label, elementSoundMode = ElementSoundMode::Off, automationName = label,
                   onClick = [kind](auto&&...) { ElementSoundPlayer::play(kind); }};
};

auto example = StackPanel {
    orientation.vertical,
    spacing = 5,
    soundButton(u"Focus", ElementSoundKind::Focus),
    soundButton(u"Invoke", ElementSoundKind::Invoke),
    soundButton(u"Show", ElementSoundKind::Show),
    soundButton(u"Hide", ElementSoundKind::Hide),
    soundButton(u"MovePrevious", ElementSoundKind::MovePrevious),
    soundButton(u"MoveNext", ElementSoundKind::MoveNext),
    soundButton(u"GoBack", ElementSoundKind::GoBack),
};