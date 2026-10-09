// A list that follows another field: `follow` assigns it anew whenever the query changes, and the control hears one
// reset and builds the rows it shows, no more. An ItemsRepeater takes the list the same way a ListView does.
struct Model {
    core::observable<core::u16_text> query;
    core::observable_list<core::u16_text> shown;

    Model() {
        shown.follow(query, [](core::u16_text const& text) {
            static constexpr std::u16string_view words[] = {
                u"apple", u"apricot", u"avocado", u"banana", u"blackberry", u"blueberry", u"cherry", u"coconut",
                u"cranberry", u"date", u"fig", u"grape", u"grapefruit", u"kiwi", u"lemon", u"lime", u"mango",
                u"melon", u"nectarine", u"orange", u"papaya", u"peach", u"pear", u"pineapple", u"plum",
                u"pomegranate", u"raspberry", u"strawberry", u"tangerine", u"watermelon"};
            std::u16string needle = text.plain();
            for (char16_t& unit : needle) {
                if (unit >= u'A' && unit <= u'Z') unit = static_cast<char16_t>(unit - u'A' + u'a');
            }
            core::sta_vector<core::u16_text> found;
            for (std::u16string_view const word : words) {
                if (word.find(needle) != std::u16string_view::npos) found.push_back(core::unicode::repaired(word));
            }
            return found;
        });
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8.0,
    TextBox {width = 260, hAlign.left, placeholderText = u"Filter the fruit", text = BindInput {model->query}},
    TextBlock {text = BindOutput {model->shown.count(), [](uint32_t count) { return core::format(u"{} found", count); }}},
    ScrollViewer {
        height = 200,
        ItemsRepeater {
            itemsSource = BindOutput {model->shown, [](core::u16_text const& word) {
                return Border {Padding {4}, TextBlock {text = word}};
            }},
        },
    },
};
