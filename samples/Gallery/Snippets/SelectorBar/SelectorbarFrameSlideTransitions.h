struct Model {
    int previous = 0;
    Frame frame {isNavigationStackEnabled = false};
};
auto const model = gallery::hold<Model>();

auto const bar = SelectorBar {
    items[SelectorBarItem {text = u"Page1", isSelected = true}, SelectorBarItem {text = u"Page2"}, SelectorBarItem {text = u"Page3"},
          SelectorBarItem {text = u"Page4"}, SelectorBarItem {text = u"Page5"}],
    onSelectionChanged = [model](SelectorBar const& sender, SelectorBarSelectionChangedEventArgs&) {
        auto const selected = sender.selectedItem();
        auto const items = sender.items();
        int current = 0;
        for (uint32_t i = 0; i < items.size(); ++i) {
            if (items[i].text() == selected.text()) {
                current = static_cast<int>(i);
            }
        }
        auto const slide = current - model->previous > 0 ? SlideNavigationTransitionEffect::FromRight : SlideNavigationTransitionEffect::FromLeft;
        gallery::showSample(model->frame, current + 1, SlideNavigationTransitionInfo {effect = slide});
        model->previous = current;
    },
};

auto example = StackPanel {bar, model->frame};