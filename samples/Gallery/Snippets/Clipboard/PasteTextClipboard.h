struct Model {
    TextBlock pasted {Padding {5, 5, 0, 0}, textWrapping = TextWrapping::Wrap, u"Click the button!"};
};
auto const model = gallery::hold<Model>();

// A coroutine: the clipboard is asked for its text and the answer is waited for without stopping the
// window. It resumes on the interface thread, where the text block may be written.
auto const paste = [](Model* model) -> async::detached_task {
    auto const package = Clipboard::getContent();
    if (package.contains(StandardDataFormats::text())) {
        model->pasted.text(co_await package.getTextAsync());
    }
};

auto example = StackPanel {
    verticalAlignment = VerticalAlignment::Top,
    Button {Margin {0, 0, 0, 10}, content = u"Paste Text from the Clipboard", onClick = [model, paste](Button const&) { paste(model.get()); }},
    TextBlock {Padding {5, 5, 0, 0}, Underline {Run {u"Clipboard:"}}},
    model->pasted,
};