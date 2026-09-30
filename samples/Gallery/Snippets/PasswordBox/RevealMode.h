// The checkbox writes the field, the box shows it as a reveal mode.
struct Model {
    core::observable<bool> revealed;
};
auto const model = gallery::hold<Model>();

auto box = PasswordBox {
    width = 250,
    Margin {0, 0, 8, 0},
    passwordRevealMode = BindOutput {
        model->revealed,
        [](bool on) { return on ? PasswordRevealMode::Visible : PasswordRevealMode::Hidden; }},
};

auto reveal = CheckBox {
    content = u"Show password",
    isChecked = BindInput {model->revealed},
};