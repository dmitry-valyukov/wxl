// The categories are the application's own data; the navigation view is given their positions and, for each, a template whose
// root is a NavigationViewItem with the properties made from the category.
struct Category {
    std::u16string name;
    Symbol glyph;
    std::u16string tooltip;
};
struct Model {
    Frame frame;
    Object source = indexList(4);
    std::vector<Category> categories {
        {u"Category 1", Symbol::Home, u"This is category 1"},
        {u"Category 2", Symbol::Keyboard, u"This is category 2"},
        {u"Category 3", Symbol::Library, u"This is category 3"},
        {u"Category 4", Symbol::Mail, u"This is category 4"},
    };
};
auto const model = gallery::hold<Model>();
auto const category = [model](Object const& item) -> Category const& { return model->categories[static_cast<size_t>(intOf(item))]; };

auto const view = NavigationView {
    row = 2,
    height = 460,
    isTabStop = false,
    menuItemsSource = model->source,
    menuItemTemplate = boundTemplate(u"NavigationViewItem",
                                     {{u"Content", [category](Object const& item) { return stringBox(category(item).name); }},
                                      {u"Icon", [category](Object const& item) { return SymbolIcon {symbol = category(item).glyph}; }},
                                      {u"ToolTipService.ToolTip", [category](Object const& item) { return stringBox(category(item).tooltip); }}}),
    content = StackPanel {model->frame},
    onSelectionChanged = [model, category](NavigationView const& sender, NavigationViewSelectionChangedEventArgs& args) {
        if (args.isSettingsSelected()) {
            gallery::showSettings(model->frame);
            return;
        }
        auto const& selected = category(args.selectedItem());
        sender.header(u"Sample Page " + std::u16string {selected.name.back()});
        gallery::showSample(model->frame, 1);
    },
};
view.add_onLoaded([view, model](auto&&...) { view.selectedItem(indexListObject(model->source, 0)); });

auto example = Grid {
    rowDefinitions = u"*,auto",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"When data binding, use the MenuItemsSource property to bind to an observable collection of items, and do not set the MenuItems "
               u"property. In addition, set the MenuItemTemplate property and use a NavigationViewItem as the data template. If you wish to bind "
               u"to the header content as well, use data template selectors via the MenuItemTemplateSelector property. "},
    view,
};