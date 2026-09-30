const Preset block {width = 50, height = 50};

auto first = Rectangle {block, fill = colors.red};
auto second = Rectangle {block, Margin {8, 0, 0, 0}, fill = colors.blue, rightOf = first};
auto third = Rectangle {block, fill = colors.green, alignRightWithPanel = true};
auto fourth = Rectangle {
    block,
    Margin {0, 8, 0, 0},
    fill = rgb(255, 255, 0),
    alignHorizontalCenterWith = third,
    below = third,
};

auto panel = RelativePanel {width = 300, first, second, third, fourth};