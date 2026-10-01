auto rounder = IncrementNumberRounder {
    increment = 0.25,
    roundingAlgorithm = RoundingAlgorithm::RoundHalfUp,
};

auto formatter = DecimalFormatter {
    integerDigits = 1,
    fractionDigits = 2,
    numberRounder = rounder,
};

auto amount = NumberBox {
    header = u"Enter a dollar amount:",
    placeholderText = u"0.00",
    numberFormatter = formatter,
};