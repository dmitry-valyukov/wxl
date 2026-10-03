auto const list = ListView {itemContainerTransitions[AddDeleteThemeTransition {}]};
auto const count = std::make_shared<int>(10);
for (int i = 0; i < *count; ++i) {
    list.items().append(ListViewItem {content = u"Item " + gallery::numberText(i)});
}

auto const add = [list, count](auto&&...) {
    list.items().append(ListViewItem {content = u"New Item " + gallery::numberText(*count)});
    ++*count;
};
auto const remove = [list](auto&&...) {
    if (list.items().size() > 0) {
        list.items().removeAt(0);
    }
};

auto example = Grid {list};
auto options = StackPanel {
    Button {hAlign.stretch, content = u"Add", onClick = add},
    Button {hAlign.stretch, content = u"Delete", onClick = remove},
    Button {hAlign.stretch, content = u"Add and Del", onClick = [add, remove](auto&&...) {
                add();
                remove();
            }},
};