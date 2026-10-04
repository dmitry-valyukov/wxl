// A theme dictionary of one's own is XAML (ThemeDictionaries and {ThemeResource} are markup), so this example is the
// markup itself, made by loadXaml.
auto const markup = loadXaml(
    u"<Grid xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
    u"<Grid.Resources><ResourceDictionary><ResourceDictionary.ThemeDictionaries>"
    u"<ResourceDictionary x:Key='Default'>"
    u"<SolidColorBrush x:Key='BackgroundBrush' Color='#EEE'/><SolidColorBrush x:Key='TextBrush' Color='#333'/>"
    u"<x:String x:Key='ThemeString'>Light theme</x:String></ResourceDictionary>"
    u"<ResourceDictionary x:Key='Dark'>"
    u"<SolidColorBrush x:Key='BackgroundBrush' Color='#333'/><SolidColorBrush x:Key='TextBrush' Color='#EEE'/>"
    u"<x:String x:Key='ThemeString'>Dark theme</x:String></ResourceDictionary>"
    u"</ResourceDictionary.ThemeDictionaries></ResourceDictionary></Grid.Resources>"
    u"<StackPanel MaxWidth='700' Padding='8' HorizontalAlignment='Center' VerticalAlignment='Center' Background='{ThemeResource BackgroundBrush}' "
    u"CornerRadius='4' Spacing='4'>"
    u"<TextBlock Foreground='{ThemeResource TextBrush}' Text='{ThemeResource ThemeString}' FontSize='20'/>"
    u"</StackPanel></Grid>");

auto example = StackPanel {
    spacing = 12.0,
    TextBlock {u"Toggle the theme using the theme switch button in the top right corner."},
    markup.try_as<UIElement>(),
};
