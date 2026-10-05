struct Model {
    TextBlock sourceName {vAlign.center};
    MediaPlayerElement preview {autoPlay = true, stretch = Stretch::Uniform};
    TextBlock capturedText {vAlign.center, u"Captured:", visibility = Visibility::Collapsed};
    StackPanel snapshots {spacing = 2};
    ComboBox cameras {displayMemberPath = u"DisplayName", header = u"Camera source"};
    ToggleSwitch mirror {header = u"Mirror preview", isOn = false, toolTip = u"Mirrors only the preview, not captured photos"};
    Button capture {content = u"Capture Photo"};
    core::nullable<MediaCapture> camera;
};
auto const model = gallery::hold<Model>();
static auto const words = [](hstring const& text) { return std::u16string {text.data(), text.size()}; };

// The camera is started for the group chosen in the list: a player takes the frames of its first source.
auto const start = [](Model* model, MediaFrameSourceGroup group) -> async::detached_task {
    try {
        if (model->camera) {
            model->preview.mediaPlayer().pause();
            (*model->camera).close();
            model->camera = nullptr;
        }
        model->sourceName.text(u"Viewing: " + words(group.displayName()));
        MediaCapture camera;
        co_await camera.initializeAsync(MediaCaptureInitializationSettings {
            sourceGroup = group,
            sharingMode = MediaCaptureSharingMode::SharedReadOnly,
            streamingCaptureMode = StreamingCaptureMode::Video,
            memoryPreference = MediaCaptureMemoryPreference::Cpu,
        });
        model->camera = camera;
        model->preview.source(MediaSource::createFromMediaFrameSource(frameSource(camera, group.sourceInfos()[0].id())));
    } catch (...) {
        model->sourceName.text(u"Error: " + words(failureOf(std::current_exception()).message));
    }
};
model->cameras.add_onSelectionChanged([model, start](auto&&...) {
    if (auto const selected = model->cameras.selectedItem(); selected && selected.is<MediaFrameSourceGroup>()) {
        start(model.get(), selected.try_as<MediaFrameSourceGroup>());
    }
});

// What happens when the page is shown: the cameras are looked for, and a refusal of access is told with the way to the settings.
auto const find = [](Model* model, UIElement host) -> async::detached_task {
    Failure failure;  // a co_await cannot stand in a catch: the failure is kept and dealt with after it
    bool failed = false;
    try {
        auto const groups = co_await MediaFrameSourceGroup::findAllAsync();
        if (groups.size() == 0) {
            model->sourceName.text(u"No camera devices found.");
            co_return;
        }
        model->cameras.itemsSource(groups);
        model->cameras.selectedIndex(0);
    } catch (...) {
        failure = failureOf(std::current_exception());
        failed = true;
    }
    if (!failed) {
        co_return;
    }
    auto dialog = ContentDialog {
        xamlRoot = host.xamlRoot(),
        title = failure.accessDenied() ? u"Camera access denied" : u"Error",
        content = failure.accessDenied() ? std::u16string {u"Please enable camera access in the privacy settings."} : words(failure.message),
        closeButtonText = failure.accessDenied() ? u"Cancel" : u"OK",
    };
    if (failure.accessDenied()) {
        dialog.primaryButtonText(u"Privacy Settings");
        dialog.defaultButton(ContentDialogButton::Primary);
    }
    if (co_await dialog.showAsync() == ContentDialogResult::Primary) {
        co_await Launcher::launchUriAsync(u"ms-settings:privacy-webcam");
    }
};

model->mirror.add_onToggled([model](auto&&...) {
    if (model->mirror.isOn()) {
        model->preview.renderTransform(ScaleTransform {scaleX = -1});
        model->preview.renderTransformOrigin(Point {0.5, 0.5});
    } else {
        model->preview.renderTransform(ScaleTransform {scaleX = 1});
    }
});

auto const takePhoto = [](Model* model) -> async::detached_task {
    if (!model->camera) {
        co_return;
    }
    InMemoryRandomAccessStream stream;
    co_await (*model->camera).capturePhotoToStreamAsync(ImageEncodingProperties::createJpeg(), stream);
    stream.seek(0);

    BitmapImage photo;
    co_await photo.setSourceAsync(stream);
    model->snapshots.children().insertAt(0, Image {source = photo});
    model->capturedText.visibility(Visibility::Visible);
    announce(model->capture, u"Photo successfully captured.", u"CameraPreviewSampleCaptureNotificationId");
};
model->capture.add_onClick([model, takePhoto](auto&&...) { takePhoto(model.get()); });

// The snapshots column takes the height the preview leaves, without adding to it: its content is measured as short as can be.
auto const fill = CustomLayout {
    [](Collection<UIElement> const& children, Size available) {
        Size wanted {};
        for (auto const& child : children) {
            child.measure(Size {available.width, 100});
            wanted = Size {std::max(wanted.width, child.desiredSize().width), std::max(wanted.height, child.desiredSize().height)};
        }
        return wanted;
    },
    [](Collection<UIElement> const& children, Size final) {
        for (auto const& child : children) {
            child.arrange(Rect {0, 0, final.width, final.height});
        }
        return final;
    },
};

auto example = Grid {
    minWidth = 400,
    minHeight = 300,
    columnDefinitions = u"*,100",
    columnSpacing = 4,
    rowDefinitions = u"auto,*",
    rowSpacing = 10,
    model->sourceName,
    Grid {row = 1, model->preview},
    Grid {row = 0, column = 1, model->capturedText},
    Grid {row = 1, column = 1, LayoutPanel {layout = fill, ScrollViewer {verticalScrollMode = ScrollMode::Enabled, content = model->snapshots}}},
    onLoaded = [model, find](UIElement const& self) { find(model.get(), self); },
};
auto options = StackPanel {spacing = 16, model->cameras, model->mirror, model->capture};