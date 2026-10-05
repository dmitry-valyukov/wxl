struct Model {
    core::observable<bool> open{true};
    core::observable<bool> iconVisible{true};
    core::observable<bool> closable{true};
};
auto const model = gallery::hold<Model>();

auto bar = InfoBar {
    title = u"Title",
    isClosable = BindOutput {model->closable},
    isIconVisible = BindOutput {model->iconVisible},
    isOpen = BindOutput {model->open},
    message = u"Essential app message for your users to be informed of, acknowledge, or take action on.",
    onClosed = [model](InfoBar const&, InfoBarClosedEventArgs&) { model->open.set(false); },
};

auto isOpen = CheckBox {content = u"Is Open", isChecked = Bind {model->open}};
auto isIconVisible = CheckBox {content = u"Is Icon Visible", isChecked = Bind {model->iconVisible}};
auto isClosable = CheckBox {content = u"Is Closable", isChecked = Bind {model->closable}};