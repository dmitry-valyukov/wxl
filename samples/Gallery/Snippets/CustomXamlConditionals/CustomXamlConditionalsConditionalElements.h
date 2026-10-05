// A condition of XAML is evaluated once, when the markup is parsed. Here it is an ordinary condition of the program,
// evaluated whenever the flag changes: an element is shown while its flag is true.
auto example = StackPanel {
    spacing = 8.0,
    InfoBar {title = u"New experience", isClosable = false, isOpen = true, severity = InfoBarSeverity::Success,
             message = u"This InfoBar is included because the 'NewExperience' flag is true.",
             visibility = BindOutput {flags->newExperience, [](bool on) { return on ? Visibility::Visible : Visibility::Collapsed; }}},
    InfoBar {title = u"Legacy mode", isClosable = false, isOpen = true, severity = InfoBarSeverity::Warning,
             message = u"This InfoBar is included because the 'LegacyMode' flag is true.",
             visibility = BindOutput {flags->legacyMode, [](bool on) { return on ? Visibility::Visible : Visibility::Collapsed; }}},
};
