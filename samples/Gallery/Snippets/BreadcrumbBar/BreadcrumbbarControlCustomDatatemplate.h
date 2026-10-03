struct Model {
    std::vector<std::u16string> names {u"Home", u"Folder1", u"Folder2", u"Folder3"};
    Object folders = observableIndexList();
};
auto const model = gallery::hold<Model>();
for (size_t i = 0; i < model->names.size(); ++i) {
    indexListAppend(model->folders, static_cast<int64_t>(i));
}

auto const name = [model](Object const& item) { return stringBox(model->names[static_cast<size_t>(intOf(item))]); };
auto const crumb = boundTemplate(u"TextBlock", {{u"Text", name}, {u"AutomationProperties.Name", name}});

auto example = BreadcrumbBar {
    itemsSource = model->folders,
    itemTemplate = boundTemplate(u"BreadcrumbBarItem", {{u"ContentTemplate", [crumb](Object const&) { return crumb; }}}),
    onItemClicked = [model](auto const&, BreadcrumbBarItemClickedEventArgs& args) {
        for (auto last = indexListSize(model->folders); last > static_cast<uint32_t>(args.index()) + 1; --last) {
            indexListRemoveAt(model->folders, last - 1);
        }
    },
};

auto options = Button {content = u"Reset sample", onClick = [model](Button const& self) {
                           while (indexListSize(model->folders) < model->names.size()) {
                               indexListAppend(model->folders, indexListSize(model->folders));
                           }
                           announce(self, u"BreadcrumbBar sample reset successful.", u"BreadCrumbBarSampleResetNotificationId");
                       }};