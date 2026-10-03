auto example = MediaPlayerElement {
    maxWidth = 400,
    autoPlay = true,
    source = MediaSource::createFromUri(u"Assets/SampleMedia/fishes.wmv"),
    onUnloaded = [](MediaPlayerElement const& self) { self.mediaPlayer().pause(); },
};