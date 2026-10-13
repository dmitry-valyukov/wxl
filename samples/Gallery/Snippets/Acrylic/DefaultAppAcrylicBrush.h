// Three shapes under the glass: it blurs what is behind it.
auto behind = [] {
    return Grid {
        Rectangle {width = 100, height = 200, hAlign.left, vAlign.top, fill = rgb(0, 255, 255)},
        Ellipse {width = 152, height = 152, hAlign.center, vAlign.center, fill = rgb(255, 0, 255)},
        Rectangle {width = 80, height = 100, hAlign.right, vAlign.bottom, fill = rgb(255, 255, 0)},
    };
};

// The size follows the room the page has, as the states of the original follow the window: from 500 the sample is 400 by 252.
struct Model {
    core::observable<bool> roomy;
};
auto const model = gallery::hold<Model>();
model->roomy.follow(gallery::pageSize().width, [](double room) { return room >= 500; });

auto example = Grid {
    minWidth = 320,
    width = BindOutput {model->roomy, [](bool roomy) { return roomy ? 400.0 : std::numeric_limits<double>::quiet_NaN(); }},
    height = BindOutput {model->roomy, [](bool roomy) { return roomy ? 252.0 : 200.0; }},
    behind(),
    Rectangle {Margin {12}, fill = brushes.Acrylic.InAppFillColor.Default},
};