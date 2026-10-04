auto const toggle = ToggleSwitch {offContent = u"Sound Off", onContent = u"Sound On", isOn = ElementSoundPlayer::state() == ElementSoundPlayerState::On};
toggle.add_onToggled([](ToggleSwitch const& self) {
    if (self.isOn()) {
        ElementSoundPlayer::state(ElementSoundPlayerState::On);
    } else {
        ElementSoundPlayer::state(ElementSoundPlayerState::Off);
        ElementSoundPlayer::spatialAudioMode(ElementSpatialAudioMode::Off);
    }
});

auto example = toggle;