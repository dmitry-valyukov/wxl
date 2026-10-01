auto rating = RatingControl {hAlign.left, vAlign.top};

auto slider = Slider {
    header = u"PlaceholderValue",
    maximum = 5.0,
    minimum = 0.0,
    smallChange = 0.5,
    stepFrequency = 0.5,
    onValueChanged = [rating](Slider const& self) { rating.placeholderValue(self.value()); },
};