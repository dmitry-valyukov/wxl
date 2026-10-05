// Accept and Flag turn into Cancel and Unflag when invoked, and back: what
// each says is one flag, kept where the page keeps it.
struct Model {
    bool accepted = false;
    bool flagged = false;
};
auto const model = gallery::hold<Model>();

auto mark = [model](char16_t const* symbolCode, char16_t const* caption, bool Model::*flag, char16_t const* on, char16_t const* off) {
    return SwipeItem {
        background = themeBrushes.Button.Background,
        foreground = themeBrushes.AppBar.Item.Foreground,
        iconSource = FontIconSource {glyph = symbolCode},
        text = caption,
        onInvoked = [model, flag, on, off](SwipeItem const& self, SwipeItemInvokedEventArgs&) {
            model.get()->*flag = !(model.get()->*flag);
            self.text(model.get()->*flag ? on : off);
        },
    };
};

auto swipe = SwipeControl {
    width = 500,
    height = 68,
    Margin {12},
    borderBrush = brushes.Control.FillColor.Default,
    BorderThickness {1},
    leftItemsMode = SwipeMode::Reveal,
    leftItems[
        mark(u"\uE8FB", u"Accept", &Model::accepted, u"Cancel", u"Accept"),
        mark(u"\uE7C1", u"Flag", &Model::flagged, u"Unflag", u"Flag")
    ],
    content = TextBlock {Margin {12}, hAlign.center, vAlign.center, u"Swipe Right"},
};