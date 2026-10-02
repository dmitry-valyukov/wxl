struct Model {
    core::observable<int> type {0};
    core::observable<int> location {0};
    core::observable<int> view {0};
    TextBox commit {header = u"Commit button text", placeholderText = u"Open", u"Pick File"};
    TextBlock picked {u"No file picked"};
};
auto const model = gallery::hold<Model>();

// A choice: its title, its names, and the field the position of the chosen one goes to.
auto choice = [](char16_t const* title, std::initializer_list<char16_t const*> names, core::observable<int>& field) {
    auto box = ComboBox {header = title, width = 200, selectedIndex = Bind {field}};
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    return box;
};

static constexpr PickerLocationId locations[] = {
    PickerLocationId::DocumentsLibrary, PickerLocationId::ComputerFolder, PickerLocationId::Desktop,
    PickerLocationId::Downloads,        PickerLocationId::MusicLibrary,   PickerLocationId::PicturesLibrary,
    PickerLocationId::VideosLibrary,    PickerLocationId::Objects3D,      PickerLocationId::Unspecified};
const std::initializer_list<char16_t const*> locationNames = {u"DocumentsLibrary", u"ComputerFolder", u"Desktop",
                                                              u"Downloads",        u"MusicLibrary",   u"PicturesLibrary",
                                                              u"VideosLibrary",    u"Objects3D",      u"Unspecified"};
// The picker is made over the window the button hangs in. Its answer is waited for with co_await: the window
// goes on working, and the coroutine resumes on the interface thread, where the text block may be written.
auto const pick = [](Model* model, Button button) -> async::detached_task {
    button.isEnabled(false);  // no second click while the dialog is open

    auto picker = FileOpenPicker {button.xamlRoot().contentIslandEnvironment().appWindowId()};
    switch (model->type.get()) {
    case 0: picker.fileType(u"*"); break;
    case 1: picker.fileType(u".txt"); break;
    default:
        picker.fileType(u".jpg");
        picker.fileType(u".png");
    }
    picker.commitButtonText(model->commit.text());
    picker.suggestedStartLocation(locations[model->location.get()]);
    picker.viewMode(model->view.get() == 0 ? PickerViewMode::List : PickerViewMode::Thumbnail);

    auto const file = co_await picker.pickSingleFileAsync();
    if (file) {
        std::u16string words = u"Picked: ";
        words += std::u16string_view {file.path()};
        model->picked.text(words);
    } else {
        model->picked.text(u"No file selected.");
    }
    button.isEnabled(true);
};

auto example = StackPanel {
    spacing = 8,
    Button {content = u"Pick a single file", onClick = [model, pick](Button const& button) { pick(model.get(), button); }},
    model->picked,
};

auto options = StackPanel {
    spacing = 8,
    choice(u"File type", {u"All Files (*)", u"Text Files (*.txt)", u"Images (*.jpg, *.png)"}, model->type),
    model->commit,
    choice(u"Suggested start location", locationNames, model->location),
    choice(u"View mode", {u"List", u"Thumbnail"}, model->view),
};