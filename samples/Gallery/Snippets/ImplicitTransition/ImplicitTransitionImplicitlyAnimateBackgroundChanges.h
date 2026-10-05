// A change of Background fades: the transition animates the brush.
auto const presenter = ContentPresenter {width = 50, height = 50, Margin {45, 5, 5, 5}, vAlign.top, background = colors.blue,
                                         backgroundTransition = BrushTransition {}};
auto const blue = std::make_shared<bool>(true);

auto example = presenter;
auto options = Button {content = u"Change Background Color", onClick = [presenter, blue](auto&&...) {
                           *blue = !*blue;
                           presenter.background(SolidColorBrush {*blue ? colors.blue : rgb(255, 255, 0)});
                       }};