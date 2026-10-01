auto number = NumberBox {
    vAlign.top,
    header = u"Enter an integer:",
    largeChange = 100.0,
    smallChange = 10.0,
    spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Compact,
    value = 10.0,
};

auto placement = RadioButtons {
    header = u"SpinButton placement",
    RadioButton {content = u"Inline"},
    RadioButton {content = u"Compact"},
    selectedIndex = 1,
    onSelectionChanged = [number](RadioButtons const& self) {
        number.spinButtonPlacementMode(self.selectedIndex() == 0 ? NumberBoxSpinButtonPlacementMode::Inline
                                                                 : NumberBoxSpinButtonPlacementMode::Compact);
    },
};