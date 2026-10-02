auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto box = AutoSuggestBox {
    width = 300,
    onTextChanged = [](AutoSuggestBox const& self, AutoSuggestBoxTextChangedEventArgs& args) {
        // Choosing a suggestion changes the text too: only typing is a query.
        if (args.reason() != AutoSuggestionBoxTextChangeReason::UserInput) {
            return;
        }
        std::vector<std::u16string> found;
        for (auto const cat : cats) {
            if (gallery::containsWords(cat, self.text())) {
                found.emplace_back(cat);
            }
        }
        if (found.empty()) {
            found.emplace_back(u"No results found");
        }
        self.itemsSource(stringList(found));
    },
    onSuggestionChosen = [output](Object const&, AutoSuggestBoxSuggestionChosenEventArgs& args) {
        output.text(stringOf(args.selectedItem()));
    },
};