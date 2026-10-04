struct Model {
    TextBlock status {visibility = Visibility::Collapsed, foreground = brushes.SystemFillColor.Success};
    Image pasted {width = 200, height = 150, horizontalAlignment = HorizontalAlignment::Left, automationName = u"Pasted image from clipboard", stretch = Stretch::UniformToFill,
                  visibility = Visibility::Collapsed};

    void say(std::u16string const& words) {
        status.text(words);
        status.visibility(Visibility::Visible);
    }
};
auto const model = gallery::hold<Model>();

// The picture is read from where it is: the reference holds the address, the clipboard fetches the file when asked.
std::u16string address = u"file:///" + (applicationFolder() / L"Assets" / L"SampleMedia" / L"rainier.jpg").generic_u16string();

auto const copy = [](Model* model, std::u16string address) {
    try {
        auto package = DataPackage {};
        package.setBitmap(RandomAccessStreamReference::createFromUri(Uri {address}));
        model->say(Clipboard::setContentWithOptions(package, ClipboardContentOptions {}) ? u"Image copied to clipboard."
                                                                                         : u"Error copying image to clipboard.");
    } catch (...) {
        // A file that is not there is an exception at the call.
        model->say(u"Error copying image to clipboard.");
    }
};

auto const paste = [](Model* model) -> async::detached_task {
    auto const package = Clipboard::getContent();
    if (!package.contains(StandardDataFormats::bitmap())) {
        model->say(u"Bitmap format is not available in the clipboard.");
        model->pasted.visibility(Visibility::Collapsed);
        co_return;
    }
    try {
        auto const reference = co_await package.getBitmapAsync();
        auto const stream = co_await reference.openReadAsync();
        auto const picture = BitmapImage {};
        picture.setSource(stream);
        stream.close();
        model->pasted.source(picture);
        model->pasted.visibility(Visibility::Visible);
        model->say(u"Image pasted from clipboard.");
    } catch (...) {
        // A failure of an operation is an exception at the co_await.
        model->say(u"Error pasting image.");
    }
};

auto example = StackPanel {
    verticalAlignment = VerticalAlignment::Top,
    spacing = 10,
    Image {width = 200, height = 150, horizontalAlignment = HorizontalAlignment::Left, automationName = u"Source image to copy", stretch = Stretch::UniformToFill,
           source = u"Assets/SampleMedia/rainier.jpg"},
    StackPanel {
        orientation.horizontal,
        spacing = 8,
        Button {
            content = u"Copy Image to Clipboard",
            onClick = [model, copy, address](Button const&) { copy(model.get(), address); },
        },
        Button {content = u"Paste Image from Clipboard", onClick = [model, paste](Button const&) { paste(model.get()); }},
    },
    model->status,
    model->pasted,
};