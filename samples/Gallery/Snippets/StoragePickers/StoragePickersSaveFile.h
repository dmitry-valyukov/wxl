struct Model {
    core::observable<bool> txt {false};
    core::observable<bool> json {false};
    core::observable<bool> xml {false};
    core::observable<int> extension {0};
    core::observable<int> location {0};
    TextBox content {header = u"File content", textWrapping = TextWrapping::Wrap, acceptsReturn = true, width = 500, height = 200,
                     horizontalAlignment = HorizontalAlignment::Left, isSpellCheckEnabled = false, u"Hello, WinUI!"};
    TextBox name {header = u"Suggested file name", u"NewDocument"};
    TextBox commit {header = u"Commit button text", placeholderText = u"Save", u"Save File"};
    TextBox folder {header = u"Suggested folder ", width = 148, placeholderText = u"Optional", isReadOnly = true,
                    foreground = brushes.Accent.TextFillColor.Primary};
    TextBlock saved {u"No file saved"};
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
auto const save = [](Model* model, Button button) -> async::detached_task {
    button.isEnabled(false);

    auto picker = FileSavePicker {button.xamlRoot().contentIslandEnvironment().appWindowId()};
    if (model->txt.get()) picker.fileTypeChoice(FileTypeChoice {u"Text Files", {u".txt"}});
    if (model->json.get()) picker.fileTypeChoice(FileTypeChoice {u"JSON Files", {u".json"}});
    if (model->xml.get()) picker.fileTypeChoice(FileTypeChoice {u"XML Files", {u".xml"}});
    static constexpr char16_t const* extensions[] = {u".txt", u".json", u".xml"};
    picker.defaultFileExtension(extensions[model->extension.get()]);
    picker.suggestedFileName(model->name.text());
    picker.commitButtonText(model->commit.text());
    picker.suggestedStartLocation(locations[model->location.get()]);
    picker.suggestedFolder(model->folder.text());

    auto const result = co_await picker.pickSaveFileAsync();
    if (result) {
        // The picker only names the file; writing it is the application's business.
        auto const bytes = core::unicode::repaired(std::u16string_view {model->content.text()}).to_utf8();
        auto const path = result.path();
        auto output = core::file::create(reinterpret_cast<wchar_t const*>(path.c_str()));
        output.write(std::as_bytes(std::span {bytes.plain()}));

        std::u16string words = u"File saved to: ";
        words += std::u16string_view {result.path()};
        model->saved.text(words);
    } else {
        model->saved.text(u"File save canceled.");
    }
    button.isEnabled(true);
};

auto const selectFolder = [](Model* model, Button button) -> async::detached_task {
    button.isEnabled(false);
    auto picker = FolderPicker {button.xamlRoot().contentIslandEnvironment().appWindowId()};
    picker.commitButtonText(u"Select folder");
    auto const folder = co_await picker.pickSingleFolderAsync();
    if (folder) model->folder.text(folder.path());
    button.isEnabled(true);
};

auto example = StackPanel {
    spacing = 8,
    model->content,
    Button {content = u"Save a file", onClick = [model, save](Button const& button) { save(model.get(), button); }},
    model->saved,
};

auto options = StackPanel {
    spacing = 8,
    TextBlock {u"File types:"},
    CheckBox {content = u"Text Files (*.txt)", isChecked = Bind {model->txt}},
    CheckBox {content = u"JSON Files (*.json)", isChecked = Bind {model->json}},
    CheckBox {content = u"XML Files (*.xml)", isChecked = Bind {model->xml}},
    choice(u"Default extension", {u".txt", u".json", u".xml"}, model->extension),
    model->name,
    model->commit,
    choice(u"Suggested start location", locationNames, model->location),
    Grid {
        columnDefinitions = u"*,auto",
        model->folder,
        Button {
            column = 1,
            Margin {8, 0, 0, 0},
            verticalAlignment = VerticalAlignment::Bottom,
            automationName = u"Select folder",
            toolTip = u"Select folder",
            content = FontIcon {glyph = u"\uF89A"},
            onClick = [model, selectFolder](Button const& button) { selectFolder(model.get(), button); },
        },
    },
};