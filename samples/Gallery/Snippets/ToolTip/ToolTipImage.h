// A tip may hold anything: here a picture over a caption.
Button {
    u"Button with a rich ToolTip.",
    toolTipElement = ToolTip {
        content = StackPanel {
            Image {source = gallery::assetPath(L"SampleMedia/cliff.jpg"), width = 160},
            TextBlock {u"Cliff", fontWeight = FontWeight {600}},
        },
    },
}
