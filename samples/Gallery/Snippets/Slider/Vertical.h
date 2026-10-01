auto output = TextBlock {u"0"};

auto slider = Slider {
    width = 100,
    height = 100,
    minimum = -50.0,
    maximum = 50.0,
    orientation.vertical,
    tickFrequency = 10.0,
    tickPlacement = TickPlacement::Outside,
    onValueChanged = [output](Slider const& self) {
        output.text(core::to_u16(self.value(), std::chars_format::fixed, 0));
    },
};