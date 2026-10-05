auto const playback = StackPanel {spacing = 8, visibility = Visibility::Collapsed};
auto const clickToPlay = BitmapImage {autoPlay = false, uriSource = u"Assets/SampleMedia/animated.gif"};
clickToPlay.add_onImageOpened([playback, clickToPlay](auto&&...) {
    if (clickToPlay.isAnimatedBitmap()) {
        playback.visibility(Visibility::Visible);
    }
});
playback.children().append(Button {content = u"Play", onClick = [clickToPlay](auto&&...) { clickToPlay.play(); }});
playback.children().append(Button {content = u"Stop", onClick = [clickToPlay](auto&&...) { clickToPlay.stop(); }});

auto example = StackPanel {
    spacing = 12,
    TextBlock {textWrapping = TextWrapping::Wrap, u"An Image element automatically plays an animated GIF source."},
    Image {height = 40, hAlign.left, source = u"Assets/SampleMedia/animated.gif"},

    TextBlock {textWrapping = TextWrapping::Wrap, u"Set AutoPlay to False to prevent the GIF from playing automatically."},
    Image {height = 40, hAlign.left, source = BitmapImage {autoPlay = false, uriSource = u"Assets/SampleMedia/animated.gif"}},

    TextBlock {textWrapping = TextWrapping::Wrap, u"Control playback manually using BitmapImage.Play() and Stop()."},
    Image {height = 40, hAlign.left, source = clickToPlay},
};
auto options = playback;