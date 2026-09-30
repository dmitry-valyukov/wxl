// The option is one field of a model: the checkbox says "disable" over it and
// the control shows it as it is. The model lives while the page is on the
// screen.
struct Model {
    core::observable<bool> enabled{true};
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
    isChecked = Bind {model->enabled, std::logical_not {}},
};