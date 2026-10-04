// The option is one field of a model: the checkbox says "disable" over it and
// the control shows it as it is. The model lives while the page is on the
// screen.
struct Model {
    core::observable<bool> enabled{true}, on{false};
};
auto const model = gallery::hold<Model>();
auto output = TextBlock {text = BindOutput {model->on, [](bool on) { return on ? u"On" : u"Off"; }}};

auto toggle = ToggleButton {
    content = u"ToggleButton",
    isEnabled = BindOutput {model->enabled},
    isChecked = Bind {model->on},
};

auto disable = CheckBox {
    content = u"Disable ToggleButton",
    isChecked = BindInput {model->enabled, std::logical_not {}},
};
