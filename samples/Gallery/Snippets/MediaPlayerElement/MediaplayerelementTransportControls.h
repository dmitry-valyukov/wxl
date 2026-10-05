struct Model {
    MediaPlayerElement player {
        maxWidth = 400,
        areTransportControlsEnabled = true,
        autoPlay = false,
        source = MediaSource::createFromUri(u"Assets/SampleMedia/ladybug.wmv"),
        onUnloaded = [](MediaPlayerElement const& self) { self.mediaPlayer().pause(); },
    };
};
auto const model = gallery::hold<Model>();

auto const open = [](Model* model, Button button) -> async::detached_task {
    auto picker = FileOpenPicker {button.xamlRoot().contentIslandEnvironment().appWindowId()};
    picker.fileType(u"*");
    auto const picked = co_await picker.pickSingleFileAsync();
    if (!picked) {
        co_return;
    }
    auto const file = co_await StorageFile::getFileFromPathAsync(picked.path());
    model->player.source(MediaSource::createFromStorageFile(file));
};

auto example = model->player;
auto options = Button {content = u"Open a file", onClick = [model, open](Button const& self) { open(model.get(), self); }};