struct Model {
    Button show {content = u"Show Popup"};
    Button close {hAlign.center, content = u"Close Popup"};
    Popup popup {Margin {-75}, childTransitions[PopupThemeTransition {}]};
};
auto const model = gallery::hold<Model>();

model->popup.child(Grid {
    Ellipse {width = 200, height = 200, hAlign.stretch, vAlign.stretch, fill = brushes.SystemControl.Background.Chrome.Medium,
             stroke = brushes.SystemControl.Foreground.Base.High, strokeThickness = 2},
    StackPanel {width = 200, Margin {24}, vAlign.center,
                TextBlock {Margin {12}, hAlign.center, fontFamily = u"Segoe UI", textAlignment = TextAlignment::Center,
                           textWrapping = TextWrapping::WrapWholeWords, u"This is a popup using PopupThemeTransition"},
                model->close},
});
model->show.add_onClick([model](auto&&...) {
    model->popup.isOpen(true);
    model->close.focus(FocusState::Programmatic);
});
model->close.add_onClick([model](auto&&...) {
    model->popup.isOpen(false);
    model->show.focus(FocusState::Programmatic);
});

auto example = Grid {model->show, model->popup};