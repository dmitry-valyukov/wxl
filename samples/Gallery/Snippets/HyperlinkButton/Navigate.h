// The option is a field of a model: the checkbox writes it, the link shows it.
// The model lives while the page is on the screen.
struct Model {
    core::observable<bool> disabled{false};
    core::observable<bool> enabled{true};

    Model() {
        enabled.follow(disabled, [](bool off) { return !off; });
    }
};
auto const model = gallery::hold(std::make_shared<Model>());

auto link = HyperlinkButton {
    content = u"Microsoft home page",
    navigateUri = u"https://www.microsoft.com",
    isEnabled = BindOutput {model->enabled},
};

auto disable = CheckBox {
    content = u"Disable hyperlink button",
    isChecked = BindInput {model->disabled},
};