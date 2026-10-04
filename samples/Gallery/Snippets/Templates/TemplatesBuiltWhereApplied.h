// A Template<T> keeps its arguments as a preset does, but builds the object only where it is applied -- and every time
// it is applied it builds a new one. So it may hold what a preset may not: an object that must be made afresh (a child
// has one parent, a brush made at namespace scope would be made before the runtime is up).
//
// Templates nest like presets do: a template inside a template, a preset inside a template, a template inside a preset.
auto const look = Preset {
    Padding {12},
    cornerRadius = CornerRadius {8},
    // a Template inside a Preset: the brush is built for each element that wears the preset, its stops are templates too
    background = Template<LinearGradientBrush> {
        startPoint = Point {0.0f, 0.0f},
        endPoint = Point {1.0f, 1.0f},
        Template<GradientStop> {rgb(96, 165, 250), offset = 0.0},
        Template<GradientStop> {rgb(167, 139, 250), offset = 1.0},
    },
};

// A Preset inside a Template, and a child that is made again for each use: written as a TextBlock it would be made once, here,
// and handed to two parents; written as a Template<TextBlock> it is built for each.
auto const badge = Template<Border> {look, hAlign.left, Template<TextBlock> {foreground = colors.white, FontWeight {600}, u"New"}};

// A Template inside a Template: two Borders from one description, which a plain Border written once could not be.
auto const row = Template<StackPanel> {orientation.horizontal, spacing = 8.0, badge, badge};

auto const built = row.build();  // the object itself, asked for by hand

auto example = StackPanel {
    spacing = 12.0,
    row,          // a template in a real description: it is built here
    built,        // the object that was built above
    Border {look, TextBlock {foreground = colors.white, u"A preset in a real description"}},
};
