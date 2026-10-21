struct Model {
    TextBlock result {Margin {8, 8, 0, 0}};

    // Last: it goes first, and stops what it runs before anything it touches.
    async::task<> showing;
};
auto const model = gallery::hold<Model>();

// No default button: Enter does not press any of them. The answer is waited for under the exit token, which
// closing the main window cancels: the dialog closes, and the wait ends.
auto const show = [](Model* model, Button button) -> async::task<> {
    auto dialog = ContentDialog {
        xamlRoot = button.xamlRoot(),
        styles.ContentDialog.Default,
        requestedTheme = button.actualTheme(),
        title = u"Replace file?",
        primaryButtonText = u"Replace",
        secondaryButtonText = u"Keep",
        closeButtonText = u"Cancel",
        defaultButton = ContentDialogButton::None,
        content = StackPanel {hAlign.stretch, vAlign.stretch,
                              TextBlock {textWrapping = TextWrapping::Wrap, u"Lorem ipsum dolor sit amet, adipisicing elit."},
                              CheckBox {content = u"Upload your content to the cloud."}},
    };

    switch (co_await dialog.showAsync(gallery::exitToken())) {
    case ContentDialogResult::Primary: model->result.text(u"User replaced the file"); break;
    case ContentDialogResult::Secondary: model->result.text(u"User kept the file"); break;
    default: model->result.text(u"User cancelled the dialog"); break;
    }
};

auto example = StackPanel {
    orientation.horizontal,
    Button {content = u"Show dialog without a default action", onClick = [model, show](Button const& self) { model->showing = show(model.get(), self); }},
    model->result,
};