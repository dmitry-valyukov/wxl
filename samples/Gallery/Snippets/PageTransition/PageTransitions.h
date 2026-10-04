// The frame is navigated between two pages made by functions (the pages of the original are XAML); the transition of
// the navigation is the one chosen in the radio buttons.
struct Model {
    gallery::PagedFrame frame {Frame {minHeight = 600, hAlign.stretch, contentTransitions[NavigationThemeTransition {}]}};
    core::nullable<NavigationTransitionInfo> info;  // nothing: the transition of the frame, as it is without an explicit one
};
auto const model = gallery::hold<Model>();

auto const page1 = [] { return gallery::samplePage1().root; };
auto const page2 = [] { return gallery::samplePage2().root; };
model->frame.forward(page1);

auto example = model->frame.frame();

auto options = StackPanel {
    RadioButtons {
        header = u"Transition modes",
        RadioButton {automationName = u"Default NavigationTransitionInfo", content = u"Default"},
        RadioButton {automationName = u"EntranceNavigationTransitionInfo", content = u"Entrance"},
        RadioButton {automationName = u"DrillInNavigationTransitionInfo", content = u"DrillIn"},
        RadioButton {automationName = u"SuppressNavigationTransitionInfo", content = u"Suppress"},
        RadioButton {automationName = u"SlideNavigationTransitionInfo From Right", content = u"Slide from Right"},
        RadioButton {automationName = u"SlideNavigationTransitionInfo From Left", content = u"Slide from Left"},
        RadioButton {automationName = u"CommonNavigationTransitionInfo", content = u"Common"},
        RadioButton {automationName = u"ContinuumNavigationTransitionInfo", content = u"Continuum"},
        selectedIndex = 0,
        onSelectionChanged = [model](RadioButtons const& self) {
            switch (self.selectedIndex()) {
            case 1: model->info = EntranceNavigationTransitionInfo {}; break;
            case 2: model->info = DrillInNavigationTransitionInfo {}; break;
            case 3: model->info = SuppressNavigationTransitionInfo {}; break;
            case 4: model->info = SlideNavigationTransitionInfo {effect = SlideNavigationTransitionEffect::FromRight}; break;
            case 5: model->info = SlideNavigationTransitionInfo {effect = SlideNavigationTransitionEffect::FromLeft}; break;
            case 6: model->info = CommonNavigationTransitionInfo {}; break;
            case 7: model->info = ContinuumNavigationTransitionInfo {}; break;
            default: model->info = nullptr; break;
            }
        },
    },
    TextBlock {Margin {0, 12, 0, 8}, u"Navigate"},
    Button {Margin {0, 0, 0, 4}, hAlign.stretch, content = u"Navigate Forward", onClick = [model, page1, page2](auto&&...) {
                auto const next = model->frame.depth() % 2 == 1 ? gallery::PagedFrame::Builder {page1} : gallery::PagedFrame::Builder {page2};
                if (model->info) {
                    model->frame.forward(next, *model->info);
                } else {
                    model->frame.forward(next);
                }
            }},
    Button {hAlign.stretch, content = u"Navigate Backward", onClick = [model](auto&&...) {
                if (model->frame.depth() > 0) {
                    model->frame.back();
                }
            }},
};