// The option is one field of a model: the checkbox says "disable" over it and
// the control shows it as it is. The model lives while the page is on the
// screen.
struct Model {
    core::observable<bool> enabled{true};
};
auto const model = gallery::hold(std::make_shared<Model>());
auto output = TextBlock {};

auto button = Button {
    content = u"Standard button",
    isEnabled = BindOutput {model->enabled},
    onClick = [output] { output.text(u"You clicked: Button1"); },
};

auto disable = CheckBox {
    content = u"Disable button",
    isChecked = Bind {model->enabled, std::logical_not {}},
};