// Three shapes under the glass: it blurs what is behind it.
auto behind = [] {
    return Grid {
        Rectangle {width = 100, height = 200, hAlign.left, vAlign.top, fill = rgb(0, 255, 255)},
        Ellipse {width = 152, height = 152, hAlign.center, vAlign.center, fill = rgb(255, 0, 255)},
        Rectangle {width = 80, height = 100, hAlign.right, vAlign.bottom, fill = rgb(255, 255, 0)},
    };
};

auto example = Grid {
    height = 252,
    width = 400,
    behind(),
    Rectangle {Margin {12}, fill = brushes.Acrylic.InAppFillColor.Default},
};