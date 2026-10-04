// A ControlTemplate is markup: the parts of a control are named in XAML (ContentElement is the one the TextBox looks for),
// so the template is written as XAML and the control made from the text by loadXaml.
auto const markup = loadXaml(
    u"<TextBox xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' Padding='8' Header='Enter text here' "
    u"BorderBrush='{ThemeResource AccentFillColorDefaultBrush}'>"
    u"<TextBox.Template><ControlTemplate TargetType='TextBox'>"
    u"<StackPanel Spacing='8'>"
    u"<TextBlock Text='{TemplateBinding Header}'/>"
    u"<Border MinWidth='200' Background='{ThemeResource CardBackgroundFillColorDefaultBrush}' BorderBrush='{TemplateBinding BorderBrush}' "
    u"BorderThickness='2' CornerRadius='4'>"
    u"<StackPanel Margin='4' Orientation='Horizontal' Spacing='4'><SymbolIcon Symbol='Edit'/>"
    u"<ScrollViewer x:Name='ContentElement' xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' Padding='{TemplateBinding Padding}'/>"
    u"</StackPanel></Border></StackPanel>"
    u"</ControlTemplate></TextBox.Template></TextBox>");

auto example = Grid {markup.try_as<UIElement>()};
