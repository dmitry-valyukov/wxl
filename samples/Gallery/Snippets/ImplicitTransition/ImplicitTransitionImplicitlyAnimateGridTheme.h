// The grid is in the Light theme; a change of its theme changes the brush of its background, which fades.
auto const grid = Grid {
    width = 300,
    minHeight = 200,
    vAlign.top,
    background = brushes.SolidBackgroundFillColor.Base,
    borderBrush = brushes.SystemControl.Foreground.Base.High,
    BorderThickness {1},
    requestedTheme = ElementTheme::Light,
    backgroundTransition = BrushTransition {},
    StackPanel {Margin {12}, spacing = 6,
                TextBlock {styles.TextBlock.Subtitle, u"Lorem Ipsum"},
                TextBlock {textWrapping = TextWrapping::WrapWholeWords, u"The background of this grid animates when the theme changes."},
                Button {content = u"Action"},
                CheckBox {content = u"Option"}},
};

auto example = grid;
auto options = Button {vAlign.top, content = u"Change Theme", onClick = [grid](auto&&...) {
                           grid.requestedTheme(grid.requestedTheme() == ElementTheme::Dark ? ElementTheme::Light : ElementTheme::Dark);
                       }};