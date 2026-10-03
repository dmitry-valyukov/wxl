// A thousand recipes in the application's vector. The repeater is given the positions of those that pass the
// filter, in the order chosen; a new filter or order is a new list of positions, which the layout takes as a change
// of all its items.
auto const recipes = std::make_shared<std::vector<gallery::Recipe>>(gallery::makeRecipes(1000));
auto const colors = &gallery::namedColors();

auto const recipeTemplate = [recipes, colors](Object const& item) {
    auto const& recipe = (*recipes)[static_cast<size_t>(intOf(item))];
    return StackPanel {
        Margin {5},
        background = brushes.SystemControl.Background.Base.Low,
        StackPanel {height = 75, Margin {8}, background = (*colors)[static_cast<size_t>(recipe.colorIndex)].color, opacity = 0.8,
                    TextBlock {Padding {12}, fontSize = 35, foreground = brushes.SystemControl.Foreground.AltHigh,
                               textAlignment = TextAlignment::Center, gallery::numberText(recipe.num)}},
        TextBlock {Margin {15, 0, 10, 0}, styles.TextBlock.Title, textWrapping = TextWrapping::Wrap, recipe.name},
        TextBlock {Margin {15, 0, 15, 15}, styles.TextBlock.Body, recipe.ingredients},
    };
};

struct Model {
    ItemsRepeater repeater;
    TextBox filter {header = u"Filter by ingredient...", width = 200, Margin {0, 0, 0, 20}, hAlign.left, vAlign.top};
    bool descending = false;
};
auto const model = gallery::hold<Model>();

auto const update = [model, recipes] {
    // The recipes whose ingredients hold what was typed, in the order of the number of ingredients (least to most by default).
    std::vector<int64_t> shown;
    for (std::size_t i = 0; i < recipes->size(); ++i) {
        if (gallery::containsIgnoringCase((*recipes)[i].ingredients, std::u16string_view {model->filter.text()})) {
            shown.push_back(static_cast<int64_t>(i));
        }
    }
    std::stable_sort(shown.begin(), shown.end(), [&](int64_t a, int64_t b) {
        auto const left = (*recipes)[static_cast<size_t>(a)].ingredientList.size();
        auto const right = (*recipes)[static_cast<size_t>(b)].ingredientList.size();
        return model->descending ? left > right : left < right;
    });
    model->repeater.itemsSource(indexList(shown.data(), shown.size()));
};

model->repeater = ItemsRepeater {layout = gallery::variedImageSizeLayout(200),
                                  itemTemplate = recipeTemplate,
                                  itemsSource = indexList(static_cast<int64_t>(recipes->size()))};
model->filter.add_onTextChanged([update](auto&&...) { update(); });

auto example = Grid {
    height = 600,
    columnDefinitions = u"*,*",
    ScrollViewer {content = model->repeater},
    StackPanel {
        column = 1,
        Margin {10, 0, 0, 0},
        model->filter,
        TextBlock {Margin {0, 0, 0, 10}, text = u"Sort by number of ingredients"},
        Button {Margin {0, 0, 0, 5}, content = u"Least to most",
                onClick = [model, update](auto&&...) {
                    if (model->descending) {
                        model->descending = false;
                        update();
                    }
                }},
        Button {content = u"Most to least",
                onClick = [model, update](auto&&...) {
                    if (!model->descending) {
                        model->descending = true;
                        update();
                    }
                }},
    },
};