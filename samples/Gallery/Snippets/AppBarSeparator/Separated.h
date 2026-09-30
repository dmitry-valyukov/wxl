ScrollViewer {
    horizontalScrollBarVisibility = ScrollBarVisibility::Hidden,
    horizontalScrollMode = ScrollMode::Auto,
    verticalScrollBarVisibility = ScrollBarVisibility::Hidden,
    verticalScrollMode = ScrollMode::Disabled,
    content = CommandBar {
        primaryCommands[
            AppBarButton {icon = SymbolIcon {symbol = Symbol::AttachCamera}, label = u"Attach Camera"},
            AppBarSeparator {},
            AppBarButton {icon = SymbolIcon {symbol = Symbol::Like}, label = u"Like"},
            AppBarButton {icon = SymbolIcon {symbol = Symbol::Dislike}, label = u"Dislike"},
            AppBarSeparator {},
            AppBarButton {icon = SymbolIcon {symbol = Symbol::Orientation}, label = u"Orientation"}
        ]
    },
}