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
