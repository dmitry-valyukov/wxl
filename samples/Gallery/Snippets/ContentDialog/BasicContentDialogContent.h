struct Model {
    TextBlock result {Margin {8, 8, 0, 0}};

    // Last: it goes first, and stops what it runs before anything it touches.
    async::task<> showing;
};
auto const model = gallery::hold<Model>();

// ShowAsync answers with the button that was pressed; awaiting it keeps the window working meanwhile. The dialog
// is made over the island of the button (XamlRoot), in the theme the button is in. The answer is waited for under
// the exit token: closing the main window cancels it and closes the dialog, and the wait ends while the window's
// queue still turns -- a window on its way out shuts the queue down, and a dialog left open would never answer.
auto const show = [](Model* model, Button button) -> async::task<> {
    auto dialog = ContentDialog {
        xamlRoot = button.xamlRoot(),
        styles.ContentDialog.Default,
        requestedTheme = button.actualTheme(),
        title = u"Save your work?",
        primaryButtonText = u"Save",
        secondaryButtonText = u"Don't Save",
        closeButtonText = u"Cancel",
        defaultButton = ContentDialogButton::Primary,
        content = StackPanel {hAlign.stretch, vAlign.stretch,
                              TextBlock {textWrapping = TextWrapping::Wrap, u"Lorem ipsum dolor sit amet, adipisicing elit."},
                              CheckBox {content = u"Upload your content to the cloud."}},
    };

    switch (co_await dialog.showAsync(gallery::exitToken())) {
    case ContentDialogResult::Primary: model->result.text(u"User saved their work"); break;
    case ContentDialogResult::Secondary: model->result.text(u"User did not save their work"); break;
    default: model->result.text(u"User cancelled the dialog"); break;
    }
};

auto example = StackPanel {
    orientation.horizontal,
    Button {content = u"Show dialog", onClick += [model, show](Button const& self) { model->showing = show(model.get(), self); }},
    model->result,
};