// The blue square has the transition: when the green one between them goes, the blue one slides over.
auto const middle = Rectangle {column = 1, width = 75, height = 75, Margin {5}, fill = colors.green};

auto example = Grid {
    columnDefinitions = u"auto,auto,auto",
    Rectangle {width = 75, height = 75, Margin {5}, fill = colors.red},
    middle,
    Rectangle {column = 2, width = 75, height = 75, Margin {5}, fill = colors.blue, transitions[RepositionThemeTransition {}]},
};

auto options = StackPanel {Button {content = u"Reposition", onClick = [middle](auto&&...) {
                                       middle.visibility(middle.visibility() == Visibility::Visible ? Visibility::Collapsed : Visibility::Visible);
                                   }}};