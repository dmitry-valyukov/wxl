// An ItemsPanelTemplate is markup too: the panel that lays the items out is named in XAML.
std::u16string items;
for (int i = 1; i <= 20; ++i) {
    auto const number = std::to_string(i);
    items += u"<ListViewItem>Item ";
    items += i < 10 ? u"0" : u"";
    items.append(number.begin(), number.end());
    items += u"</ListViewItem>";
}

auto const markup = loadXaml(
    u"<ListView xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' HorizontalAlignment='Stretch'>"
    u"<ListView.ItemsPanel><ItemsPanelTemplate><ItemsWrapGrid Orientation='Horizontal'/></ItemsPanelTemplate></ListView.ItemsPanel>" +
    items + u"</ListView>");

auto example = ScrollViewer {height = 200, content = markup.try_as<UIElement>()};
