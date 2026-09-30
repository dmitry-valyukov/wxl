// The option is a field of a model: the checkbox writes it, the toggle shows
// it. The model lives while the page is on the screen.
struct Model {
    core::observable<bool> disabled{false};
    core::observable<bool> enabled{true};

    Model() {
        enabled.follow(disabled, [](bool off) { return !off; });
    }
};
auto const model = gallery::hold(std::make_shared<Model>());

auto output = TextBlock {u"Off"};

auto toggle = ToggleButton {
    content = u"ToggleButton",
    isEnabled = BindOutput {model->enabled},
    onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"On"); },
    onUnchecked = [output](Object const&, RoutedEventArgs&) { output.text(u"Off"); },
};

auto disable = CheckBox {
    content = u"Disable ToggleButton",
    isChecked = BindInput {model->disabled},
};