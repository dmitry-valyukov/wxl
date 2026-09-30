// The option is one field of a model: the checkbox says "disable" over it and
// the control shows it as it is. The model lives while the page is on the
// screen.
struct Model {
    core::observable<bool> enabled{true};
};
auto const model = gallery::hold<Model>();
auto link = HyperlinkButton {
    content = u"Microsoft home page",
    navigateUri = u"https://www.microsoft.com",
    isEnabled = BindOutput {model->enabled},
};

auto disable = CheckBox {
    content = u"Disable hyperlink button",
    isChecked = Bind {model->enabled, std::logical_not {}},
};