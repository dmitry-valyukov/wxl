auto example = StackPanel {
    orientation.vertical,
    CheckBox {content = u"Enable Spatial Audio",
              isEnabled = ElementSoundPlayer::state() == ElementSoundPlayerState::On,
              isChecked = ElementSoundPlayer::spatialAudioMode() == ElementSpatialAudioMode::On &&
                          ElementSoundPlayer::state() == ElementSoundPlayerState::On,
              onChecked = [](auto&&...) {
                  if (ElementSoundPlayer::state() == ElementSoundPlayerState::On) {
                      ElementSoundPlayer::spatialAudioMode(ElementSpatialAudioMode::On);
                  }
              },
              onUnchecked = [](auto&&...) {
                  if (ElementSoundPlayer::state() == ElementSoundPlayerState::On) {
                      ElementSoundPlayer::spatialAudioMode(ElementSpatialAudioMode::Off);
                  }
              }},
    TextBlock {Margin {0, 5, 0, 0}, fontStyle = FontStyle::Italic, foreground = brushes.Text.FillColor.Secondary, styles.TextBlock.Caption,
               u"Can only enable spatial audio when sound is on!"},
};