// The animation is made by Lottie-Windows from a file of Adobe After Effects: code that builds composition objects
// (LottieLogo.cpp), which the player takes as its Source.
struct Model {
    AnimatedVisualPlayer player {autoPlay = false, source = gallery::lottieLogoSource()};
    ToggleButton pause {column = 1, hAlign.stretch, automationName = u"Pause", toolTip = u"Pause", isThreeState = false,
                        content = SymbolIcon {symbol = Symbol::Pause}};

    // Plays the animation at the rate that is set, from the beginning, unless it is playing already or is paused -- then it resumes.
    void ensurePlaying() {
        if (pause.isChecked().value_or(false)) {
            pause.isChecked(false);
        } else if (!player.isPlaying()) {
            player.playAsync(0, 1, false);
        }
    }
};
auto const model = gallery::hold<Model>();

// Pausing does not end the animation, stopping does: it goes back to its first frame, and the pause is let go.
model->pause.add_onChecked([model](auto&&...) { model->player.pause(); });
model->pause.add_onUnchecked([model](auto&&...) { model->player.resume(); });

auto example = StackPanel {
    hAlign.center,
    TextBlock {
        textWrapping = TextWrapping::WrapWholeWords,
        Run {u"This AnimatedVisualPlayer consumes an animation created using Adobe AfterEffects and translated into Microsoft.UI.Composition "
             u"objects using "},
        Hyperlink {navigateUri = u"https://aka.ms/lottie", Run {u"Lottie-Windows"}},
        Run {u". Since the "},
        Hyperlink {navigateUri = u"https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.composition.compositionshape",
                   Run {u"CompositionShapes"}},
        Run {u" used here are supported on Windows 10 version 17763+, the AnimatedVisualPlayer falls back to an Image when its Source is "
             u"unavailable."},
    },
    Border {width = 400, height = 400, Margin {0, 20, 0, 20}, background = brushes.Card.BackgroundFillColor.Default,
            borderBrush = brushes.Card.StrokeColorDefault, BorderThickness {1}, model->player},
    Grid {
        width = 400,
        Margin {12},
        vAlign.center,
        columnSpacing = 8,
        columnDefinitions = u"*,*,*,*",
        Button {column = 0, hAlign.stretch, automationName = u"Play", toolTip = u"Play", content = SymbolIcon {symbol = Symbol::Play},
                onClick = [model](auto&&...) {
                    model->player.playbackRate(1);
                    model->ensurePlaying();
                }},
        model->pause,
        Button {column = 2, hAlign.stretch, automationName = u"Stop", toolTip = u"Stop", content = SymbolIcon {symbol = Symbol::Stop},
                onClick = [model](auto&&...) {
                    model->player.stop();
                    model->pause.isChecked(false);
                }},
        Button {column = 3, hAlign.stretch, automationName = u"Reverse", toolTip = u"Reverse", content = SymbolIcon {symbol = Symbol::Previous},
                onClick = [model](auto&&...) {
                    model->player.playbackRate(-1);
                    model->ensurePlaying();
                }},
    },
};