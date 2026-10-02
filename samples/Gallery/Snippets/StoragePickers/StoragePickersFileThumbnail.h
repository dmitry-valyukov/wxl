struct Model {
    core::observable<int> mode {0};
    NumberBox size {header = u"Requested size (px)", value = 200.0, minimum = 16.0, maximum = 1024.0,
                    spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, width = 200};
    TextBlock details {textWrapping = TextWrapping::Wrap, u"No file picked"};
    Image thumbnail {stretch = Stretch::Uniform};
};
auto const model = gallery::hold<Model>();

auto choice = [](char16_t const* title, std::initializer_list<char16_t const*> names, core::observable<int>& field) {
    auto box = ComboBox {header = title, width = 200, selectedIndex = Bind {field}};
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    return box;
};

static constexpr ThumbnailMode modes[] = {ThumbnailMode::PicturesView, ThumbnailMode::VideosView, ThumbnailMode::MusicView,
                                          ThumbnailMode::DocumentsView, ThumbnailMode::ListView,   ThumbnailMode::SingleItem};
static constexpr char16_t const* modeNames[] = {u"PicturesView", u"VideosView", u"MusicView", u"DocumentsView", u"ListView", u"SingleItem"};

auto const pick = [](Model* model, Button button) -> async::detached_task {
    button.isEnabled(false);

    auto picker = FileOpenPicker {button.xamlRoot().contentIslandEnvironment().appWindowId()};
    picker.fileType(u"*");
    auto const picked = co_await picker.pickSingleFileAsync();
    if (!picked) {
        model->details.text(u"No file selected.");
        model->thumbnail.source(ImageSource {});
        button.isEnabled(true);
        co_return;
    }

    // The new picker answers with a path; the thumbnail is a property of the older storage file.
    auto const file = co_await StorageFile::getFileFromPathAsync(picked.path());
    auto const mode = modes[model->mode.get()];

    // The number box is empty (NaN) when the user clears it: a sane size instead.
    double const raw = model->size.value();
    uint32_t const size = std::isnan(raw) || raw <= 0 ? 200u : static_cast<uint32_t>(raw);

    try {
        auto const thumbnail = co_await file.getThumbnailAsync(mode, size, ThumbnailOptions::UseCurrentScale);
        auto const picture = BitmapImage {};
        co_await picture.setSourceAsync(thumbnail);
        model->thumbnail.source(picture);

        std::u16string words = u"File: ";
        words += std::u16string_view {file.name()};
        words += u"\nMode: ThumbnailMode.";
        words += modeNames[model->mode.get()];
        words += u"\nRequested size: ";
        words += core::to_u16(size).plain();
        words += u"\nReturned size: ";
        words += core::to_u16(thumbnail.originalWidth()).plain();
        words += u" x ";
        words += core::to_u16(thumbnail.originalHeight()).plain();
        model->details.text(words);
        thumbnail.close();
    } catch (...) {
        model->thumbnail.source(ImageSource {});
        model->details.text(u"Could not retrieve a thumbnail for this file with this mode.\nTry a different mode (for example, SingleItem) or a different file.");
    }
    button.isEnabled(true);
};

auto example = StackPanel {
    orientation = Orientation::Horizontal,
    spacing = 8,
    StackPanel {
        spacing = 8,
        Button {content = u"Pick a file", onClick = [model, pick](Button const& button) { pick(model.get(), button); }},
        model->details,
    },
    Border {width = 160, height = 160, CornerRadius {4}, horizontalAlignment = HorizontalAlignment::Left,
            background = brushes.SubtleFillColor.Tertiary, model->thumbnail},
};

auto options = StackPanel {
    spacing = 8,
    choice(u"Thumbnail mode", {u"PicturesView", u"VideosView", u"MusicView", u"DocumentsView", u"ListView", u"SingleItem"}, model->mode),
    model->size,
};