auto expander = Expander {
    vAlign.top,
    header = u"This text is in the header",
    content = u"This is in the content",
    expandDirection = ExpandDirection::Down,
    isExpanded = false,
};

auto direction = ComboBox {
    header = u"ExpandDirection",
    ComboBoxItem {content = u"Down"},
    ComboBoxItem {content = u"Up"},
    selectedIndex = 0,
    onSelectionChanged = [expander](ComboBox const& self) {
        bool const up = self.selectedIndex() == 1;
        expander.expandDirection(up ? ExpandDirection::Up : ExpandDirection::Down);
        expander.verticalAlignment(up ? VerticalAlignment::Bottom : VerticalAlignment::Top);
    },
};