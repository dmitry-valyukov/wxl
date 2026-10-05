auto example = StackPanel {
    hAlign.left,
    TextBlock {Margin {0, 0, 0, 10}, hAlign.left, styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap,
               u"The sample below showcases headings. To navigate headings in Narrator, press H or Shift+H while in Scan mode."},
    StackPanel {
        maxWidth = 500,
        hAlign.left,
        // Here is the main header for the whole text. It gets HeadingLevel 1
        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level1, fontSize = 26, u"Lorem ipsums"},
        // The following TextBlock is the header for the standard lorem ipsum text, thus it is only HeadingLevel 2
        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level2, fontSize = 22, u"Lorem ipsum"},
        TextBlock {textWrapping = TextWrapping::WrapWholeWords,
                   u"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque feugiat velit pulvinar, vehicula nisi at, molestie "
                   u"risus. Duis consequat auctor libero vitae consectetur. Nullam efficitur euismod lacinia."},
        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level2, fontSize = 22, u"Cat ipsum"},
        // This is the header for the standard cat ipsum section, which is hierarchically below the cat ipsum header, resulting in HeadingLevel 3
        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level3, fontSize = 18, u"Standard"},
        TextBlock {textWrapping = TextWrapping::WrapWholeWords,
                   u"Mice litter kitter kitty litty little kitten big roar roar feed me but i will ruin the couch with my claws and hunt by "
                   u"meowing loudly at 5am next to human."},
        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level3, fontSize = 18, u"Cat breeds"},
        TextBlock {textWrapping = TextWrapping::WrapWholeWords,
                   u"Tabby abyssinian for jaguar. Thai russian blue and ragdoll, ocicat. Mouser puma so american bobtail for donskoy balinese "
                   u". Scottish fold manx so siamese."},
        TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level2, fontSize = 22, u"Bacon ipsum"},
        TextBlock {textWrapping = TextWrapping::WrapWholeWords,
                   u"Bacon ipsum dolor amet meatball nulla labore, tempor sirloin chicken frankfurter tail drumstick ex cupim ground round."},
    },
};
