struct Model {
    core::observable<int> direction {0};
};
auto const model = gallery::hold<Model>();

auto expander = Expander {
    verticalAlignment = BindOutput {model->direction, [](int index) { return index == 1 ? VerticalAlignment::Bottom : VerticalAlignment::Top; }},
    header = u"This text is in the header",
    content = u"This is in the content",
    expandDirection = BindOutput {model->direction, [](int index) { return static_cast<ExpandDirection>(index); }},
    isExpanded = false,
};

auto direction = ComboBox {
    header = u"ExpandDirection",
    ComboBoxItem {content = u"Down"},
    ComboBoxItem {content = u"Up"},
    selectedIndex = Bind {model->direction},
};
