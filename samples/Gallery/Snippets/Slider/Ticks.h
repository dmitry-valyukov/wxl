auto output = TextBlock {u"0"};

auto slider = Slider {
    width = 290,
    tickFrequency = 20.0,
    tickPlacement = TickPlacement::Outside,
    onValueChanged = [output](Slider const& self) {
        output.text(core::to_u16(self.value(), std::chars_format::fixed, 0));
    },
};

auto snaps = RadioButtons {
    header = u"Snaps to:",
    RadioButton {content = u"StepValues"},
    RadioButton {content = u"Ticks"},
    selectedIndex = 0,
    onSelectionChanged = [slider](RadioButtons const& self) {
        slider.snapsTo(self.selectedIndex() == 0 ? SliderSnapsTo::StepValues : SliderSnapsTo::Ticks);
    },
};