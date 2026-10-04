struct Model {
    RichEditBox editor {width = 800, height = 100, automationName = u"editor with custom menu"};
    TextBlock confirmation {Padding {20, 5, 0, 0}, visibility = Visibility::Collapsed, u"Text copied to clipboard!"};
    DispatcherQueueTimer timer = DispatcherQueue::getForCurrentThread().createTimer();

    void copy() {
        auto package = DataPackage {};
        package.setText(editor.document().getText(TextGetOptions::None));
        Clipboard::setContent(package);

        confirmation.visibility(Visibility::Visible);
        // The confirmation hides itself after two seconds.
        timer.start();
    }
};
auto const model = gallery::hold<Model>();
model->editor.document().setText(TextSetOptions::None, u"This text will be copied to the clipboard.");
model->timer.interval(std::chrono::seconds {2});
model->timer.isRepeating(false);
model->timer.add_onTick([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock()) {
        model->confirmation.visibility(Visibility::Collapsed);
        model->timer.stop();
    }
});

auto example = StackPanel {
    verticalAlignment = VerticalAlignment::Top,
    StackPanel {
        orientation.horizontal,
        Button {Margin {0, 0, 0, 10}, content = u"Copy Text to the Clipboard", onClick = [model](Button const&) { model->copy(); }},
        model->confirmation,
    },
    model->editor,
};