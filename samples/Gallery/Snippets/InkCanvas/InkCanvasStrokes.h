struct Model {
    InkCanvas sheet {automationName = u"Strokes drawing canvas"};
    TextBlock count {u"0 strokes on canvas"};
    Button restore {automationName = u"Restore saved drawing", content = u"Restore saved drawing", isEnabled = false};
    TextBlock status {styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap, u"Draw something, then save it."};
    core::nullable<InMemoryRandomAccessStream> saved;
    int savedCount = 0;

    void updateCount() {
        auto const strokes = static_cast<int>(sheet.inkPresenter().strokeContainer().getStrokes().size());
        count.text(gallery::numberText(strokes) + u" stroke(s) on canvas");
        restore.isEnabled(saved && strokes == 0);
    }
};
auto const model = gallery::hold<Model>();
model->sheet.inkPresenter().add_onStrokesCollected([model](auto&&...) { model->updateCount(); });
model->sheet.inkPresenter().add_onStrokesErased([model](auto&&...) { model->updateCount(); });

auto const save = [](Model* model) -> async::detached_task {
    auto const container = model->sheet.inkPresenter().strokeContainer();
    auto const strokes = static_cast<int>(container.getStrokes().size());
    if (strokes == 0) {
        model->status.text(u"Draw something before saving.");
        co_return;
    }
    InMemoryRandomAccessStream stream;
    co_await container.saveAsync(stream.getOutputStreamAt(0));
    model->saved = stream;
    model->savedCount = strokes;
    model->updateCount();
    model->status.text(u"Saved " + gallery::numberText(strokes) + u" stroke(s). Clear the canvas to restore them.");
};
auto const restoreSaved = [](Model* model) -> async::detached_task {
    if (!model->saved) {
        model->status.text(u"Save a drawing before restoring it.");
        co_return;
    }
    auto const container = model->sheet.inkPresenter().strokeContainer();
    if (container.getStrokes().size() != 0) {
        model->status.text(u"Clear the canvas before restoring saved ink.");
        co_return;
    }
    co_await container.loadAsync((*model->saved).getInputStreamAt(0));
    model->updateCount();
    model->status.text(u"Restored " + gallery::numberText(static_cast<int>(container.getStrokes().size())) + u" stroke(s).");
};
model->restore.add_onClick([model, restoreSaved](auto&&...) { restoreSaved(model.get()); });

auto example = Border {height = 280, maxWidth = 600, hAlign.stretch, background = colors.white,
                       borderBrush = brushes.SystemControl.Foreground.Chrome.High, BorderThickness {1}, model->sheet};
auto options = StackPanel {
    width = 240,
    spacing = 8,
    model->count,
    Button {automationName = u"Save drawing", content = u"Save drawing", onClick = [model, save](auto&&...) { save(model.get()); }},
    Button {automationName = u"Clear saved drawing canvas", content = u"Clear canvas", onClick = [model](auto&&...) {
                model->sheet.inkPresenter().strokeContainer().clear();
                model->updateCount();
                model->status.text(model->saved ? gallery::numberText(model->savedCount) + u" saved stroke(s) are ready to restore."
                                                : std::u16string {u"Canvas cleared. Draw and save something to restore it."});
            }},
    model->restore,
    model->status,
};