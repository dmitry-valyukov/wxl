auto const tip = TeachingTip {
    title = u"This is the title",
    actionButtonContent = u"Action button",
    closeButtonContent = u"Close button",
    isLightDismissEnabled = true,
    placementMargin = Thickness {20},
    preferredPlacement = TeachingTipPlacementMode::Auto,
    subtitle = u"And this is the subtitle",
};

auto example = Grid {
    Button {content = u"Show TeachingTip", onClick = [tip](auto&&...) { tip.isOpen(true); }},
    tip,
};