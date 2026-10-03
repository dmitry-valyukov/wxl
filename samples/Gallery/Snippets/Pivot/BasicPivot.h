auto example = Pivot {
    title = u"EMAIL",
    minHeight = 400,
    items[PivotItem {header = u"All", content = TextBlock {u"all emails go here."}},
          PivotItem {header = u"Unread", content = TextBlock {u"unread emails go here."}},
          PivotItem {header = u"Flagged", content = TextBlock {u"flagged emails go here."}},
          PivotItem {header = u"Urgent", content = TextBlock {u"urgent emails go here."}}],
};