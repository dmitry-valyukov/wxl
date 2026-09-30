// The one thing the model holds is what was chosen; the picture, the name and
// the subtitle under the box are all derived from it.
struct Model {
    core::observable<std::u16string> name;
};
auto const model = gallery::hold<Model>();

auto details = StackPanel {
    orientation.horizontal,
    Margin {0, 8, 0, 0},
    visibility = BindOutput {model->name,
        [](std::u16string const& name) {
            return gallery::controlByTitle(name) ? Visibility::Visible : Visibility::Collapsed;
        }},
    Image {
        height = 75,
        source = BindOutput {model->name,
            [](std::u16string const& name) {
                auto const* control = gallery::controlByTitle(name);
                return control ? gallery::assetPath(control->imagePath) : std::wstring{};
            }},
    },
    StackPanel {
        vAlign.center,
        TextBlock {Margin {8, 0, 0, 0}, styles.TextBlock.BodyStrong, text = BindOutput {model->name}},
        TextBlock {
            Margin {8, 0, 0, 0},
            textWrapping.wrapWholeWords,
            text = BindOutput {model->name,
                [](std::u16string const& name) {
                    auto const* control = gallery::controlByTitle(name);
                    return control ? control->subtitle : std::wstring{};
                }},
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