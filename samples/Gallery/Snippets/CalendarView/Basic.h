struct Model {
    core::observable<bool> groupLabel{true};
    core::observable<bool> outOfScope{true};
    core::observable<int> selection{1};
    core::observable<int> identifier{0};
    core::observable<int> language{0};
};
auto const model = gallery::hold<Model>();

static constexpr char16_t const* identifiers[] = {
    u"GregorianCalendar", u"HebrewCalendar", u"HijriCalendar", u"JapaneseCalendar", u"JulianCalendar",
    u"KoreanCalendar",    u"PersianCalendar", u"TaiwanCalendar", u"ThaiCalendar",    u"UmAlQuraCalendar",
};

auto calendar = CalendarView {
    vAlign.top,
    calendarIdentifier = BindOutput {model->identifier, [](int chosen) { return identifiers[chosen]; }},
    isGroupLabelVisible = BindOutput {model->groupLabel},
    isOutOfScopeEnabled = BindOutput {model->outOfScope},
    language = BindOutput {model->language, [](int chosen) { return gallery::languages[chosen].code; }},
    selectionMode = BindOutput {model->selection, [](int chosen) { return static_cast<CalendarViewSelectionMode>(chosen); }},
};

// A row of the list of choices for every name the original puts in a ComboBox.
auto choice = [](char16_t const* title, int width, std::span<char16_t const* const> names, core::observable<int>& field) {
    auto box = ComboBox {header = title, Margin {0, 10, 0, 0}, selectedIndex = Bind {field}};
    if (width) {
        box.width(width);
    }
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    return box;
};

auto languageNames = [] {
    std::vector<char16_t const*> names;
    for (auto const& entry : gallery::languages) {
        names.push_back(entry.name);
    }
    return names;
}();

static constexpr char16_t const* selectionNames[] = {u"None", u"Single", u"Multiple"};

auto options = StackPanel {
    Margin {0, -5, 0, 0},
    CheckBox {content = u"IsGroupLabelVisible", isChecked = Bind {model->groupLabel}},
    CheckBox {content = u"IsOutOfScopeEnabled", isChecked = Bind {model->outOfScope}},
    choice(u"SelectionMode", 0, selectionNames, model->selection),
    choice(u"CalendarIdentifier", 220, identifiers, model->identifier),
    choice(u"Language", 220, languageNames, model->language),
};