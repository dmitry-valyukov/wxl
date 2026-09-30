auto output = TextBlock {u"0"};

auto slider = Slider {
    width = 200,
    onValueChanged = [output](Slider const& self) {
        output.text(core::to_u16(self.value(), std::chars_format::fixed, 0));
    },
};