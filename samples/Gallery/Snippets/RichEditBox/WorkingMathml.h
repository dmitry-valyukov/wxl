// What the editor holds is shown under it as MathML, as often as it changes.
auto mathml = RsdnBlock {isTextSelectionEnabled = true};

auto editor = RichEditBox {
    fontSize = 16,
    height = 80,
    width = 724,
    hAlign.left,
    onTextChanged = [mathml](RichEditBox const& self) {
        // Only a formula in one line has MathML to give: more lines give an empty string.
        auto const text = self.document().getMathML();
        mathml.rsdn(L"[code=xml]" +
                    (text.empty() ? std::wstring{L"<!-- No MathML content -->"}
                                  : gallery::indentXml(gallery::wide(text))) +
                    L"[/code]");
    },
};
editor.document().setMathMode(RichEditMathMode::MathOnly);

auto const formula = std::u16string {
    u"<mml:math xmlns:mml=\"http://www.w3.org/1998/Math/MathML\" display=\"block\">\r\n"
    u"  <mml:mi mathcolor=\"#000000\">x</mml:mi>\r\n"
    u"  <mml:mo mathcolor=\"#000000\">\u2208</mml:mo>\r\n"
    u"  <mml:mi mathcolor=\"#000000\">P</mml:mi>\r\n"
    u"  <mml:mfenced>\r\n"
    u"    <mml:mrow>\r\n"
    u"      <mml:mi mathcolor=\"#000000\">A</mml:mi>\r\n"
    u"    </mml:mrow>\r\n"
    u"  </mml:mfenced>\r\n"
    u"  <mml:mo mathcolor=\"#000000\">\u2194</mml:mo>\r\n"
    u"  <mml:mi mathcolor=\"#000000\">x</mml:mi>\r\n"
    u"  <mml:mo mathcolor=\"#000000\">\u2286</mml:mo>\r\n"
    u"  <mml:mi mathcolor=\"#000000\">A</mml:mi>\r\n"
    u"</mml:math>"};

auto setFormula = Button {
    u"Set sample formula",
    styles.Button.Accent,
    onClick = [editor, formula] {
        // The formula's own colour has to be the text colour of the theme.
        auto text = formula;
        if (editor.actualTheme() == ElementTheme::Dark) {
            for (std::size_t at; (at = text.find(u"#000000")) != std::u16string::npos;) {
                text.replace(at, 7, u"#FFFFFF");
            }
        }
        editor.document().setMathML(text);
    },
};