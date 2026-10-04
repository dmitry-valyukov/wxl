// Один эффект на всю панель: копии — это тот же эффект, и анимации у
// всех кнопок общие.
auto const pop = MagnifyEffect {1.2, maximum = 1.35, minimum = 0.95};

auto const tool = [pop](FluentSymbol glyph, Color tint) {
    return Button {
        content = SymbolIcon {symbol = glyph, foreground = tint},
        width = 52,
        height = 52,
        pop,
    };
};

auto example =
StackPanel {
    orientation.horizontal,
    spacing = 22.0,
    hAlign.center,
    Margin {0, 20},
    tool(FluentSymbol::Home, rgb(15, 108, 189)),
    tool(FluentSymbol::Mail, rgb(216, 59, 1)),
    tool(FluentSymbol::Camera, rgb(135, 100, 184)),
    tool(FluentSymbol::MusicNote, rgb(227, 0, 140)),
    tool(FluentSymbol::Globe, rgb(16, 124, 16)),
};
