struct Model {
    core::observable<int> orientation{0};
    core::observable<int> previous{0};
    core::observable<int> next{0};
    core::observable<int> wrap{0};
};
auto const model = gallery::hold<Model>();

auto pager = PipsPager {
    numberOfPages = 10,
    orientation = BindOutput {model->orientation, [](int chosen) { return chosen == 0 ? orientation.horizontal : orientation.vertical; }},
    previousButtonVisibility = BindOutput {model->previous, [](int chosen) { return static_cast<PipsPagerButtonVisibility>(chosen); }},
    nextButtonVisibility = BindOutput {model->next, [](int chosen) { return static_cast<PipsPagerButtonVisibility>(chosen); }},
    wrapMode = BindOutput {model->wrap, [](int chosen) { return static_cast<PipsPagerWrapMode>(chosen); }},
};

// A choice: its title, its names, and the field the position of the chosen one goes to.
auto choice = [](char16_t const* title, std::initializer_list<char16_t const*> names, core::observable<int>& field) {
    auto box = ComboBox {header = title, selectedIndex = Bind {field}};
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    return box;
};

auto options = StackPanel {
    choice(u"Orientation", {u"Horizontal", u"Vertical"}, model->orientation),
    choice(u"Previous Button Visibility", {u"Visible", u"VisibleOnPointerOver", u"Collapsed"}, model->previous),
    choice(u"Next Button Visibility", {u"Visible", u"VisibleOnPointerOver", u"Collapsed"}, model->next),
    choice(u"Wrap Mode", {u"None", u"Wrap"}, model->wrap),
};