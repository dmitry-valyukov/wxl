// This example is a bit contrived, since you could fix the tab order by reordering the elements,
// but sometimes that is not easy to do.
auto example = Grid {
    columnSpacing = 8.0,
    rowSpacing = 8.0,
    rowDefinitions = u"auto,auto,auto",
    columnDefinitions = u"auto,auto,auto",
    TextBlock {column = 1, hAlign.center, u"Column 1"},
    TextBlock {column = 2, hAlign.center, u"Column 2"},
    TextBlock {row = 1, vAlign.center, u"Row 1"},
    Button {row = 1, column = 1, hAlign.stretch, content = u"First stop", tabIndex = 1},
    Button {row = 1, column = 2, content = u"Third stop", tabIndex = 3},
    TextBlock {row = 2, vAlign.center, u"Row 2"},
    Button {row = 2, column = 1, hAlign.stretch, content = u"Second stop", tabIndex = 2},
    Button {row = 2, column = 2, hAlign.stretch, content = u"Not a stop", isTabStop = false},
};
