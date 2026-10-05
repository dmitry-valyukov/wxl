auto host = SystemBackdropElement {cornerRadius = CornerRadius {8}, systemBackdrop = DesktopAcrylicBackdrop {}};

auto example = Grid {
    width = 300,
    height = 200,
    hAlign.center,
    host,
    Button {hAlign.center, vAlign.center, content = u"Click Me"},
};

auto options = StackPanel {
    spacing = 12,
    ComboBox {
        header = u"Backdrop Type",
        ComboBoxItem {content = u"Acrylic"},
        ComboBoxItem {content = u"Mica"},
        ComboBoxItem {content = u"Mica Alt"},
        selectedIndex = 0,
        onSelectionChanged = [host](ComboBox const& self) {
            switch (self.selectedIndex()) {
                case 0: host.systemBackdrop(DesktopAcrylicBackdrop {}); break;
                case 1: host.systemBackdrop(MicaBackdrop {kind = MicaKind::Base}); break;
                default: host.systemBackdrop(MicaBackdrop {kind = MicaKind::BaseAlt}); break;
            }
        },
    },
    Slider {
        header = u"Corner radius",
        minimum = 0.0,
        maximum = 50.0,
        stepFrequency = 1.0,
        value = 8.0,
        onValueChanged = [host](Slider const& self) { host.cornerRadius(CornerRadius {self.value()}); },
    },
};