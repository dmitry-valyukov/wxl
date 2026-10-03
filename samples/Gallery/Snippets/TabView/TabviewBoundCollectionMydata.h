// The data of the tabs are the application's own; the TabView is given their numbers, and a template whose root is a TabViewItem
// with the properties made from the data.
struct MyData {
    std::u16string header;
    Frame content;
};
struct Model {
    std::vector<MyData> datas;
    Object order = observableIndexList();

    void add() {
        auto const index = static_cast<int>(datas.size());
        MyData data {u"MyData Doc " + gallery::numberText(index), Frame {}};
        gallery::showSample(data.content, index % 3 + 1);
        datas.push_back(std::move(data));
        indexListAppend(order, index);
    }
};
auto const model = gallery::hold<Model>();
for (int i = 0; i < 3; ++i) {
    model->add();
}

auto const data = [model](Object const& item) -> MyData const& { return model->datas[static_cast<size_t>(intOf(item))]; };

auto example = TabView {
    minHeight = 475,
    Margin {-12},
    selectedIndex = 0,
    tabItemsSource = model->order,
    tabItemTemplate = boundTemplate(u"TabViewItem",
                                    {{u"Content", [data](Object const& item) { return data(item).content; }},
                                     {u"Header", [data](Object const& item) { return stringBox(data(item).header); }},
                                     {u"IconSource", [](Object const&) { return SymbolIconSource {symbol = Symbol::Placeholder}; }}}),
    onAddTabButtonClick = [model](TabView const&) { model->add(); },
    onTabCloseRequested = [model](TabView const&, TabViewTabCloseRequestedEventArgs& args) {
        for (uint32_t i = 0; i < indexListSize(model->order); ++i) {
            if (indexListAt(model->order, i) == intOf(args.item())) {
                indexListRemoveAt(model->order, i);
                break;
            }
        }
    },
};