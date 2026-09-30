// The model holds what was chosen and follows it: the control is looked up
// once, and what the details show is read off that.
struct Model {
    core::observable<std::u16string> name;
    core::observable<gallery::ControlInfo const*> control;
    core::observable<Visibility> visibility;
    core::observable<std::wstring> image;
    core::observable<std::wstring> subtitle;

    Model() {
        control.follow(name, [](std::u16string const& name) { return gallery::controlByTitle(name); });
        visibility.follow(control, [](gallery::ControlInfo const* control) {
            return control ? Visibility::Visible : Visibility::Collapsed;
        });
        image.follow(control, [](gallery::ControlInfo const* control) {
            return control ? gallery::assetPath(control->imagePath) : std::wstring{};
        });
        subtitle.follow(control, [](gallery::ControlInfo const* control) {
            return control ? control->subtitle : std::wstring{};
        });
    }
};
auto const model = gallery::hold<Model>();

auto details = StackPanel {
    orientation.horizontal,
    Margin {0, 8, 0, 0},
    visibility = BindOutput {model->visibility},
    Image {height = 75, source = BindOutput {model->image}},
    StackPanel {
        vAlign.center,
        TextBlock {Margin {8, 0, 0, 0}, styles.TextBlock.BodyStrong, text = BindOutput {model->name}},
        TextBlock {
            Margin {8, 0, 0, 0},
            textWrapping.wrapWholeWords,
            text = BindOutput {model->subtitle},
        },
    },
};
auto box = AutoSuggestBox {
    width = 300,
    hAlign.left,
    placeholderText = u"Type a control name",
    queryIcon = SymbolIcon {symbol = FluentSymbol::Search},
    onTextChanged = [](AutoSuggestBox const& self, AutoSuggestBoxTextChangedEventArgs& args) {
        // Only a user's typing asks for suggestions; a chosen one fills the text.
        if (args.reason() != AutoSuggestionBoxTextChangeReason::UserInput) {
            return;
        }
        auto found = gallery::controlTitles(self.text());
        if (found.empty()) {
            found.emplace_back(u"No results found");
        }
        self.itemsSource(stringList(found));
    },
    onSuggestionChosen = [](AutoSuggestBox const& self, AutoSuggestBoxSuggestionChosenEventArgs& args) {
        // No results is not something to complete the text with.
        auto const title = stringOf(args.selectedItem());
        if (title != u"No results found") {
            self.text(title);
        }
    },
    onQuerySubmitted = [model](Object const&, AutoSuggestBoxQuerySubmittedEventArgs& args) {
        if (args.chosenSuggestion()) {
            model->name.set(std::u16string{stringOf(args.chosenSuggestion())});
        } else if (!args.queryText().empty()) {
            // No suggestion picked: the first control that matches the text.
            auto const found = gallery::controlTitles(args.queryText());
            if (!found.empty()) {
                model->name.set(std::u16string{found.front()});
            }
        }
    },
};