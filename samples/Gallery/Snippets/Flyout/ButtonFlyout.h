auto const confirmation = Flyout {};
confirmation.content(StackPanel {
    TextBlock {Margin {0, 0, 0, 12}, styles.TextBlock.Base, u"All items will be removed. Do you want to continue?"},
    Button {content = u"Yes, empty my cart", onClick = [confirmation](auto&&...) { confirmation.hide(); }},
});

auto example = Button {content = u"Empty cart", flyout = confirmation};