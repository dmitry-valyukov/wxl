StackPanel {
    orientation.horizontal,
    ToggleSwitch {
        header = u"Toggle work",
        isOn = true,
        offContent = u"Do work",
        onContent = u"Working",
        onToggled = [](ToggleSwitch const& self) {
            self.parent().try_as<StackPanel>().children()[1].try_as<ProgressRing>().isActive(self.isOn());
        },
    },
    ProgressRing {width = 32, isActive = true},
}