// The two radii the framework gives: ControlCornerRadius for what sits in a page, OverlayCornerRadius for what lies over it.
auto example = StackPanel {
    hAlign.stretch,
    gallery::geometryRow(8, u"8px", u"Top-level containers such as app windows, flyouts, cards and dialogs.", u"OverlayCornerRadius", true),
    gallery::geometryRow(4, u"4px", u"In-page elements such as controls and list backplates.", u"ControlCornerRadius", false),
    gallery::geometryRow(0, u"0px", u"Straight edges that intersect with other straight edges.", u"N/a", true),
};
