// Each row is an example text in the style, the font it is set in, the size and the line height, and the name of the style.
auto example = StackPanel {
    hAlign.stretch,
    gallery::typographyRow(u"Caption", styles.TextBlock.Caption, u"CaptionTextBlockStyle", u"Small, Regular", u"12/16 epx", true),
    gallery::typographyRow(u"Body", styles.TextBlock.Body, u"BodyTextBlockStyle", u"Text, Regular", u"14/20 epx", false),
    gallery::typographyRow(u"Body Strong", styles.TextBlock.BodyStrong, u"BodyStrongTextBlockStyle", u"Text, SemiBold", u"14/20 epx", true),
    gallery::typographyRow(u"Body Large", styles.TextBlock.BodyLarge, u"BodyLargeTextBlockStyle", u"Text, Regular", u"18/24 epx", false),
    gallery::typographyRow(u"Body Large Strong", styles.TextBlock.BodyLargeStrong, u"BodyLargeStrongTextBlockStyle", u"Text, SemiBold", u"18/24 epx", true),
    gallery::typographyRow(u"Subtitle", styles.TextBlock.Subtitle, u"SubtitleTextBlockStyle", u"Display, SemiBold", u"20/28 epx", false),
    gallery::typographyRow(u"Title", styles.TextBlock.Title, u"TitleTextBlockStyle", u"Display, SemiBold", u"28/36 epx", true),
    gallery::typographyRow(u"Title Large", styles.TextBlock.TitleLarge, u"TitleLargeTextBlockStyle", u"Display, SemiBold", u"40/52 epx", false),
    gallery::typographyRow(u"Display", styles.TextBlock.Display, u"DisplayTextBlockStyle", u"Display, SemiBold", u"68/92 epx", true),
};
