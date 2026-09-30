auto output = TextBlock {u"-1", FontWeight {700}};

auto rating = RatingControl {
    hAlign.left,
    caption = u"312 ratings",
    onValueChanged = [output](RatingControl const& self) {
        self.caption(u"Your rating");
        output.text(core::to_u16(self.value(), std::chars_format::fixed, 0));
    },
};

auto options = StackPanel {
    width = 220,
    CheckBox {
        content = u"IsClearEnabled",
        onClick = [rating](CheckBox const& self) { rating.isClearEnabled(self.isChecked().value_or(false)); },
    },
    TextBlock {u"Swipe left or click again to clear your rating.", textWrapping.wrapWholeWords},
    CheckBox {
        Margin {0, 12, 0, 0},
        content = u"IsReadOnly",
        onClick = [rating](CheckBox const& self) { rating.isReadOnly(self.isChecked().value_or(false)); },
    },
};