auto picture = Image {height = 75};
auto nameText = TextBlock {Margin {8, 0, 0, 0}, styles.TextBlock.BodyStrong};
auto subtitleText = TextBlock {Margin {8, 0, 0, 0}, textWrapping.wrapWholeWords};

auto details = StackPanel {
    orientation.horizontal,
    Margin {0, 8, 0, 0},
    visibility = Visibility::Collapsed,
    picture,
    StackPanel {vAlign.center, nameText, subtitleText},
};

// What was chosen is shown under the box.
auto const show = [picture, nameText, subtitleText, details](std::u16string_view name) {
    if (auto const* control = gallery::controlByTitle(name)) {
        details.visibility(Visibility::Visible);
        picture.source(gallery::assetPath(control->imagePath).c_str());
        nameText.text(name);
        subtitleText.text(control->subtitle);
    }
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
    onQuerySubmitted = [show](Object const&, AutoSuggestBoxQuerySubmittedEventArgs& args) {
        if (args.chosenSuggestion()) {
            show(stringOf(args.chosenSuggestion()));
        } else if (!args.queryText().empty()) {
            // No suggestion picked: the first control that matches the text.
            auto const found = gallery::controlTitles(args.queryText());
            if (!found.empty()) {
                show(found.front());
            }
        }
    },
};