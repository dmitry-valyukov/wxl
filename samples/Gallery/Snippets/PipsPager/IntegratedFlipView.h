// The flip view and the pips are two views of one field: turning either turns the other.
struct Model {
    core::observable<int> page{0};
};
auto const model = gallery::hold<Model>();

auto picture = [](char16_t const* path) { return Image {source = path}; };

auto example = StackPanel {
    FlipView {
        height = 270,
        maxWidth = 400,
        automationControlType = AutomationControlType::List,
        automationLocalizedControlType = u"list",
        picture(u"Assets/SampleMedia/LandscapeImage1.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage2.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage3.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage4.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage5.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage6.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage7.jpg"),
        picture(u"Assets/SampleMedia/LandscapeImage8.jpg"),
        selectedIndex = Bind {model->page},
    },
    PipsPager {
        Margin {0, 12, 0, 0},
        hAlign.center,
        numberOfPages = 8,
        selectedPageIndex = Bind {model->page},
    },
};