struct Model {
    TextBlock status {textWrapping = TextWrapping::Wrap};
};
auto const model = gallery::hold<Model>();

// The files are chosen with a picker made over this window, then handed to the clipboard as storage items.
auto const copy = [](Model* model, WindowId window) -> async::detached_task {
    auto picker = FileOpenPicker {window};
    picker.fileType(u"*");

    auto const picked = co_await picker.pickMultipleFilesAsync();
    if (picked.empty()) co_return;

    // A coroutine that ends in an exception ends the program: what can fail is caught.
    std::vector<StorageItem> items;
    try {
        for (auto const& file : picked) {
            auto const stored = co_await StorageFile::getFileFromPathAsync(file.path());
            items.push_back(StorageItem {stored});
        }
    } catch (...) {
        model->status.text(u"Error copying files to clipboard.");
        co_return;
    }

    auto package = DataPackage {requestedOperation = DataPackageOperation::Copy};
    package.setStorageItems(items);

    std::u16string words;
    if (Clipboard::setContentWithOptions(package, ClipboardContentOptions {})) {
        words = core::to_u16(static_cast<int>(picked.size())).plain();
        words += u" file(s) copied to clipboard.";
    } else {
        words = u"Error copying files to clipboard.";
    }
    model->status.text(words);
};

auto const paste = [](Model* model) -> async::detached_task {
    auto const package = Clipboard::getContent();
    if (!package.contains(StandardDataFormats::storageItems())) {
        model->status.text(u"StorageItems format is not available in the clipboard.");
        co_return;
    }
    try {
        auto const items = co_await package.getStorageItemsAsync();
        std::u16string words = u"Requested operation: ";
        switch (package.requestedOperation()) {
        case DataPackageOperation::Copy: words += u"Copy"; break;
        case DataPackageOperation::Move: words += u"Move"; break;
        case DataPackageOperation::Link: words += u"Link"; break;
        default: words += u"None"; break;
        }
        words += u"\nFile(s) on clipboard (";
        words += core::to_u16(static_cast<int>(items.size())).plain();
        words += u"):";
        for (auto const& item : items) {
            words += u"\n  • ";
            words += std::u16string_view {item.name()};
        }
        model->status.text(words);
    } catch (...) {
        model->status.text(u"Error pasting files.");
    }
};

auto example = StackPanel {
    verticalAlignment = VerticalAlignment::Top,
    spacing = 10,
    StackPanel {
        orientation = Orientation::Horizontal,
        spacing = 8,
        Button {
            content = u"Copy Files to Clipboard",
            onClick = [model, copy](Button const& button) { copy(model.get(), button.xamlRoot().contentIslandEnvironment().appWindowId()); },
        },
        Button {content = u"Paste Files from Clipboard", onClick = [model, paste](Button const&) { paste(model.get()); }},
    },
    model->status,
};