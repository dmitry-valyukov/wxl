struct Model {
    InkCanvas sheet {automationName = u"Toolbar drawing canvas"};
    InkToolbarBallpointPenButton ballpoint {automationName = u"Ballpoint pen"};
    InkToolbarPencilButton pencil {automationName = u"Pencil"};
    InkToolbarHighlighterButton highlighter {automationName = u"Highlighter"};
    InkToolbarEraserButton eraser {automationName = u"Eraser"};
    InkToolbar toolbar {automationName = u"Drawing tools", buttonFlyoutPlacement = InkToolbarButtonFlyoutPlacement::Auto,
                        initialControls = InkToolbarInitialControls::None, targetInkCanvas = sheet};

    CheckBox showPencil {automationName = u"Show pencil", content = u"Pencil", isChecked = true};
    CheckBox showHighlighter {automationName = u"Show highlighter", content = u"Highlighter", isChecked = true};
    CheckBox showEraser {automationName = u"Show eraser", content = u"Eraser", isChecked = true};
    ComboBox activeTool {horizontalAlignment = HorizontalAlignment::Stretch, automationName = u"Active tool", header = u"Active tool",
                         ComboBoxItem {content = u"Ballpoint pen"}, ComboBoxItem {content = u"Pencil"}, ComboBoxItem {content = u"Highlighter"},
                         ComboBoxItem {content = u"Eraser"}, selectedIndex = 0};
    ComboBox flyoutPlacement {horizontalAlignment = HorizontalAlignment::Stretch, automationName = u"Button flyout placement",
                              header = u"Button flyout placement", ComboBoxItem {content = u"Auto"}, ComboBoxItem {content = u"Top"},
                              ComboBoxItem {content = u"Bottom"}, ComboBoxItem {content = u"Left"}, ComboBoxItem {content = u"Right"}, selectedIndex = 0};
    bool updating = false;  // the options and the toolbar change each other; one change is not answered twice

    static int indexOf(InkToolbarTool tool) {
        switch (tool) {
            case InkToolbarTool::BallpointPen: return 0;
            case InkToolbarTool::Pencil: return 1;
            case InkToolbarTool::Highlighter: return 2;
            case InkToolbarTool::Eraser: return 3;
            default: return -1;
        }
    }
    static InkToolbarTool toolAt(int index) {
        static constexpr InkToolbarTool tools[] = {InkToolbarTool::BallpointPen, InkToolbarTool::Pencil, InkToolbarTool::Highlighter, InkToolbarTool::Eraser};
        return tools[std::clamp(index, 0, 3)];
    }

    void syncActiveTool() {
        auto const active = toolbar.activeTool();
        int const index = active ? indexOf(active.toolKind()) : -1;
        if (index >= 0 && activeTool.selectedIndex() != index) {
            updating = true;
            activeTool.selectedIndex(index);
            updating = false;
        }
    }

    // The tool buttons that are shown follow the checkboxes; the active one is the requested tool when it is shown, the ballpoint pen when not.
    void updateButtons(InkToolbarTool requested) {
        InkToolbarToolButton selected = ballpoint;
        if (requested == InkToolbarTool::Pencil && showPencil.isChecked().value_or(false)) {
            selected = pencil;
        } else if (requested == InkToolbarTool::Highlighter && showHighlighter.isChecked().value_or(false)) {
            selected = highlighter;
        } else if (requested == InkToolbarTool::Eraser && showEraser.isChecked().value_or(false)) {
            selected = eraser;
        }
        pencil.visibility(showPencil.isChecked().value_or(false) ? Visibility::Visible : pencil.visibility());
        highlighter.visibility(showHighlighter.isChecked().value_or(false) ? Visibility::Visible : highlighter.visibility());
        eraser.visibility(showEraser.isChecked().value_or(false) ? Visibility::Visible : eraser.visibility());

        auto const active = toolbar.activeTool();
        if (!active || !active.is_same_object(selected)) {
            toolbar.activeTool(selected);
        }

        pencil.visibility(showPencil.isChecked().value_or(false) ? pencil.visibility() : Visibility::Collapsed);
        highlighter.visibility(showHighlighter.isChecked().value_or(false) ? highlighter.visibility() : Visibility::Collapsed);
        eraser.visibility(showEraser.isChecked().value_or(false) ? eraser.visibility() : Visibility::Collapsed);
    }

    void toolsChanged() {
        if (updating) {
            return;
        }
        auto const active = toolbar.activeTool();
        auto const previous = active ? active.toolKind() : InkToolbarTool::BallpointPen;
        updating = true;
        updateButtons(previous);
        updating = false;
        syncActiveTool();
    }

    void activeToolPicked() {
        if (updating || activeTool.selectedIndex() < 0) {
            return;
        }
        auto const requested = toolAt(activeTool.selectedIndex());
        updating = true;
        if (requested == InkToolbarTool::Pencil) {
            showPencil.isChecked(true);
        } else if (requested == InkToolbarTool::Highlighter) {
            showHighlighter.isChecked(true);
        } else if (requested == InkToolbarTool::Eraser) {
            showEraser.isChecked(true);
        }
        updateButtons(requested);
        updating = false;
        syncActiveTool();
    }
};
auto const model = gallery::hold<Model>();

for (UIElement const& button : std::initializer_list<UIElement> {model->ballpoint, model->pencil, model->highlighter, model->eraser}) {
    model->toolbar.children().append(button);
}
for (auto const& box : {model->showPencil, model->showHighlighter, model->showEraser}) {
    box.add_onChecked([model](auto&&...) { model->toolsChanged(); });
    box.add_onUnchecked([model](auto&&...) { model->toolsChanged(); });
}
model->activeTool.add_onSelectionChanged([model](auto&&...) { model->activeToolPicked(); });
model->flyoutPlacement.add_onSelectionChanged([model](auto&&...) {
    static constexpr InkToolbarButtonFlyoutPlacement placements[] = {
        InkToolbarButtonFlyoutPlacement::Auto, InkToolbarButtonFlyoutPlacement::Top, InkToolbarButtonFlyoutPlacement::Bottom,
        InkToolbarButtonFlyoutPlacement::Left, InkToolbarButtonFlyoutPlacement::Right};
    model->toolbar.buttonFlyoutPlacement(placements[std::clamp(model->flyoutPlacement.selectedIndex(), 0, 4)]);
});
model->toolbar.add_onActiveToolChanged([model](auto&&...) {
    if (!model->updating) {
        model->syncActiveTool();
    }
});
model->toolbar.add_onLoaded([model](auto&&...) {
    if (!model->toolbar.activeTool()) {
        model->toolbar.activeTool(model->ballpoint);
    }
    model->syncActiveTool();
});

auto example = StackPanel {
    spacing = 8,
    model->toolbar,
    Border {height = 280, maxWidth = 600, hAlign.stretch, background = colors.white,
            borderBrush = brushes.SystemControl.Foreground.Chrome.High, BorderThickness {1}, model->sheet},
};
auto options = StackPanel {
    width = 240,
    spacing = 8,
    TextBlock {fontWeight = FontWeight {600}, u"Available tools"},
    TextBlock {styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap, u"Ballpoint pen is always available."},
    model->showPencil,
    model->showHighlighter,
    model->showEraser,
    model->activeTool,
    model->flyoutPlacement,
    Button {automationName = u"Clear toolbar drawing", content = u"Clear drawing", onClick = [model](auto&&...) { model->sheet.inkPresenter().strokeContainer().clear(); }},
};