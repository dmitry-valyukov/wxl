auto iconBadge = InfoBadge {hAlign.right, style = styles.InfoBadge.AttentionIcon};
auto valueBadge = InfoBadge {hAlign.right, style = styles.InfoBadge.AttentionValue, value = 10};
auto dotBadge = InfoBadge {vAlign.center, style = styles.InfoBadge.AttentionDot};

auto kind = ComboBox {
    header = u"Styles",
    ComboBoxItem {content = u"Attention"},
    ComboBoxItem {content = u"Informational"},
    ComboBoxItem {content = u"Success"},
    ComboBoxItem {content = u"Critical"},
    selectedIndex = 0,
    onSelectionChanged = [iconBadge, valueBadge, dotBadge](ComboBox const& self) {
        switch (self.selectedIndex()) {
            case 0:
                iconBadge.style(styles.InfoBadge.AttentionIcon);
                valueBadge.style(styles.InfoBadge.AttentionValue);
                dotBadge.style(styles.InfoBadge.AttentionDot);
                break;
            case 1:
                iconBadge.style(styles.InfoBadge.InformationalIcon);
                valueBadge.style(styles.InfoBadge.InformationalValue);
                dotBadge.style(styles.InfoBadge.InformationalDot);
                break;
            case 2:
                iconBadge.style(styles.InfoBadge.SuccessIcon);
                valueBadge.style(styles.InfoBadge.SuccessValue);
                dotBadge.style(styles.InfoBadge.SuccessDot);
                break;
            case 3:
                iconBadge.style(styles.InfoBadge.CriticalIcon);
                valueBadge.style(styles.InfoBadge.CriticalValue);
                dotBadge.style(styles.InfoBadge.CriticalDot);
                break;
        }
    },
};