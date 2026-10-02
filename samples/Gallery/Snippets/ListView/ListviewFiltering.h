// The ContactListViewTemplate of the original -- an ellipse, a name over a company -- as a function from the
// item (the position of a contact in the application's vector) to the element that shows it.
auto const* people = &gallery::contacts();
auto const contactOf = [people](Object const& item) {
    auto const& contact = (*people)[static_cast<size_t>(intOf(item))];
    return Grid {
        rowDefinitions = u"*,*",
        columnDefinitions = u"auto,*",
        Ellipse {rowSpan = 2, width = 32, height = 32, Margin {6}, horizontalAlignment = HorizontalAlignment::Center,
                 verticalAlignment = VerticalAlignment::Center, fill = brushes.Control.Strong.FillColorDefault},
        TextBlock {column = 1, Margin {12, 6, 0, 0}, styles.TextBlock.Base, contact.name()},
        TextBlock {row = 1, column = 1, Margin {12, 0, 0, 6}, styles.TextBlock.Body, contact.company},
    };
};
struct Model {
    ListView list {width = 400, height = 400, borderBrush = brushes.Control.Strong.StrokeColorDefault, borderThickness = 1,
                   selectionMode = ListViewSelectionMode::Single};
    TextBox firstName {Margin {8}, horizontalAlignment = HorizontalAlignment::Stretch, header = u"First name"};
    TextBox lastName {Margin {8}, horizontalAlignment = HorizontalAlignment::Stretch, header = u"Last name"};
    TextBox company {Margin {8}, horizontalAlignment = HorizontalAlignment::Stretch, header = u"Company"};
    std::vector<int64_t> shown;  // the positions of the contacts the filter lets through

    static bool contains(std::u16string_view text, std::u16string_view part) {
        // The comparison is by letters, whatever their case: the same rule the original names, InvariantCultureIgnoreCase.
        auto const lower = [](std::u16string_view source) {
            std::u16string result{source};
            for (auto& letter : result) letter = static_cast<char16_t>(std::towlower(static_cast<wint_t>(letter)));
            return result;
        };
        return lower(text).find(lower(part)) != std::u16string::npos;
    }

    // Items that no longer match go, those that match again come back, in the order of the data.
    void filter(std::vector<gallery::Contact> const& all) {
        auto const items = list.items();
        items.clear();
        shown.clear();
        for (size_t index = 0; index < all.size(); ++index) {
            if (contains(all[index].firstName, firstName.text()) && contains(all[index].lastName, lastName.text()) &&
                contains(all[index].company, company.text())) {
                items.append(intBox(static_cast<int64_t>(index)));
            }
        }
    }
};
auto const model = gallery::hold<Model>();

model->list.itemTemplate(contactOf);
model->list.itemsSource(indexList(static_cast<int64_t>(people->size())));

auto const onChanged = [model, people](auto&&...) { model->filter(*people); };
model->firstName.add_onTextChanged(onChanged);
model->lastName.add_onTextChanged(onChanged);
model->company.add_onTextChanged(onChanged);

auto example = model->list;

auto options = StackPanel {
    width = 200,
    TextBlock {Margin {8, 8, 8, 4}, styles.TextBlock.Base, u"Filter by..."},
    model->firstName,
    model->lastName,
    model->company,
};