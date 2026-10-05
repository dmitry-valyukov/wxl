auto const panel = StackPanel {orientation.horizontal, childrenTransitions[EntranceThemeTransition {isStaggeringEnabled = true}]};
auto const rectangle = [] { return Rectangle {width = 50, height = 50, Margin {5}, fill = rgb(173, 216, 230)}; };
for (int i = 0; i < 5; ++i) {
    panel.children().append(rectangle());
}

auto const add = [panel, rectangle](int count) {
    return [=](auto&&...) {
        for (int i = 0; i < count; ++i) {
            panel.children().append(rectangle());
        }
    };
};

auto example = panel;
auto options = StackPanel {
    Button {hAlign.stretch, content = u"Add one", onClick = add(1)},
    Button {hAlign.stretch, content = u"Add five", onClick = add(5)},
    Button {hAlign.stretch, content = u"Clear all", onClick = [panel](auto&&...) { panel.children().clear(); }},
};