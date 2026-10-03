struct Model {
    ToggleSwitch lightDismiss {header = u"IsLightDismissEnabled", isOn = true, offContent = u"False", onContent = u"True"};
    Popup popup {horizontalOffset = 200, verticalOffset = 0, isLightDismissEnabled = true};
};
auto const model = gallery::hold<Model>();

// The light-dismiss switch cannot be turned while the popup is up.
model->popup.child(Grid {
    minWidth = 240,
    Padding {16},
    background = brushes.Acrylic.BackgroundFillColor.Default,
    borderBrush = brushes.SurfaceStrokeColor.Default,
    BorderThickness {1},
    CornerRadius {8},
    StackPanel {
        spacing = 8,
        TextBlock {fontSize = 16, u"Simple Popup"},
        Button {content = u"Close", onClick = [model](auto&&...) {
                    if (model->popup.isOpen()) {
                        model->popup.isOpen(false);
                    }
                    model->lightDismiss.isEnabled(true);
                }},
    },
});
model->popup.add_onClosed([model](auto&&...) { model->lightDismiss.isEnabled(true); });
model->lightDismiss.add_onToggled([model](ToggleSwitch const& self) { model->popup.isLightDismissEnabled(self.isOn()); });

auto example = Grid {
    hAlign.left,
    vAlign.top,
    Button {content = u"Show Popup (using Offset)", onClick = [model](auto&&...) {
                if (!model->popup.isOpen()) {
                    model->popup.isOpen(true);
                }
                model->lightDismiss.isEnabled(false);
            }},
    model->popup,
};

auto options = StackPanel {
    spacing = 8,
    model->lightDismiss,
    NumberBox {header = u"VerticalOffset", largeChange = 100.0, maximum = 100.0, minimum = -100.0, smallChange = 10.0,
               spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, value = 0.0,
               onValueChanged = [model](NumberBox const& self) { model->popup.verticalOffset(self.value()); }},
    NumberBox {header = u"HorizontalOffset", largeChange = 100.0, maximum = 500.0, minimum = -100.0, smallChange = 10.0,
               spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline, value = 200.0,
               onValueChanged = [model](NumberBox const& self) { model->popup.horizontalOffset(self.value()); }},
};