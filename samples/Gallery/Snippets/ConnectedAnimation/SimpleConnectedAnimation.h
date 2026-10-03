// Two pages of a frame; the tile of the first flies to the picture of the second and back, as the options say.
struct Model {
    gallery::PagedFrame frame {Frame {height = 500, minWidth = 500, minHeight = 300}};
    RadioButtons configurations {RadioButton {content = u"Default"}, RadioButton {content = u"Gravity"}, RadioButton {content = u"Direct"},
                                 RadioButton {content = u"Basic"}, selectedIndex = 0};
    gallery::SamplePage1 page1;
    gallery::SamplePage2 page2;
    bool onFirst = true;
};
auto const model = gallery::hold<Model>();

// The configuration chosen in the radio buttons; the first one is the animation as it is without one.
auto const configure = [model](ConnectedAnimation const& animation) {
    switch (model->configurations.selectedIndex()) {
    case 1: animation.configuration(GravityConnectedAnimationConfiguration {}); break;
    case 2: animation.configuration(DirectConnectedAnimationConfiguration {}); break;
    case 3: animation.configuration(BasicConnectedAnimationConfiguration {}); break;
    default: break;
    }
};

auto const first = [model] {
    model->page1 = gallery::samplePage1();
    return model->page1.root;
};
auto const second = [model] {
    model->page2 = gallery::samplePage2();
    return model->page2.root;
};
model->frame.forward(first);

auto const navigate = [model, configure, first, second](auto&&...) {
    auto const service = ConnectedAnimationService::getForCurrentView();
    if (model->onFirst) {
        auto const animation = service.prepareToAnimate(u"ForwardConnectedAnimation", model->page1.source);
        configure(animation);
        model->frame.forward(second, SuppressNavigationTransitionInfo {});
        // The destination page: its text comes in with an entrance transition, the tile with the connected animation.
        model->page2.content.transitions().append(EntranceThemeTransition {});
        service.getAnimation(u"ForwardConnectedAnimation").tryStart(model->page2.destination);
    } else {
        auto const animation = service.prepareToAnimate(u"BackwardConnectedAnimation", model->page2.destination);
        configure(animation);
        model->frame.forward(first, SuppressNavigationTransitionInfo {});
        service.getAnimation(u"BackwardConnectedAnimation").tryStart(model->page1.source);
    }
    model->onFirst = !model->onFirst;
};

auto example = model->frame.frame();
auto options = StackPanel {
    Button {hAlign.stretch, content = u"Navigate", onClick = navigate},
    TextBlock {Margin {0, 6}, styles.TextBlock.Base, u"Configurations"},
    model->configurations,
};