struct Model {
    std::vector<std::u16string> names {u"Home", u"Folder1", u"Folder2", u"Folder3"};
    IndexList folders = IndexList::observable();
};
auto const model = gallery::hold<Model>();
for (size_t i = 0; i < model->names.size(); ++i) {
    model->folders.append(static_cast<int64_t>(i));
}

auto const name = [model](Object const& item) { return boxValue(model->names[static_cast<size_t>(unboxValue<int64_t>(item))]); };
auto const crumb = boundTemplate(u"TextBlock", {{u"Text", name}, {u"AutomationProperties.Name", name}});

auto example = BreadcrumbBar {
    itemsSource = model->folders,
    itemTemplate = boundTemplate(u"BreadcrumbBarItem", {{u"ContentTemplate", [crumb](Object const&) { return crumb; }}}),
    onItemClicked += [model](auto const&, BreadcrumbBarItemClickedEventArgs& args) {
        for (auto last = model->folders.size(); last > static_cast<uint32_t>(args.index()) + 1; --last) {
            model->folders.removeAt(last - 1);
        }
    },
};

auto options = Button {content = u"Reset sample", onClick += [model](Button const& self) {
                           while (model->folders.size() < model->names.size()) {
                               model->folders.append(model->folders.size());
                           }
                           announce(self, u"BreadcrumbBar sample reset successful.", u"BreadCrumbBarSampleResetNotificationId");
                       }};