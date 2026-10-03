auto const button = Button {content = u"Show TeachingTip"};
auto const tip = TeachingTip {
    title = u"This is the title",
    subtitle = u"And this is the subtitle",
    target = button,
    iconSource = SymbolIconSource {symbol = Symbol::Refresh},
};
button.add_onClick([tip](auto&&...) { tip.isOpen(true); });

auto example = Grid {button, tip};