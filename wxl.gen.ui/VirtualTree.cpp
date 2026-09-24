#include "VirtualTree.h"

#include <algorithm>
#include <cmath>

#include "FontFamily.h"
#include "generated/Microsoft.UI.Input.h"
#include "generated/brushes.h"

using namespace wxl;
using namespace wxl::dsl;

namespace editor {

using wxl::core::intrusive_ptr;

namespace {

// Шевроны Segoe Fluent Icons: ChevronRight и ChevronDown.
constexpr std::u16string_view collapsedGlyph = u"\uE76C";
constexpr std::u16string_view expandedGlyph = u"\uE70D";

// Отметка — знак шрифта значков, а не CheckBox: у того галочка — AnimatedIcon,
// и при листании, когда строка получает другой тип, она рисовалась заново.
// checkbox_checked_20_regular и checkbox_unchecked_20_regular.
constexpr std::u16string_view checkedGlyph = u"\uF28D";
constexpr std::u16string_view uncheckedGlyph = u"\uF291";

constexpr std::u16string_view iconFont = u"Assets/FluentSystemIcons-Regular.ttf#FluentSystemIcons-Regular";

// Колесо даёт 120 на щелчок; щелчок листает три строки, как в проводнике.
constexpr int32_t wheelUnitsPerLine = 40;

}  // namespace

intrusive_ptr<VirtualTree> VirtualTree::create() {
    return {new VirtualTree{}, /*add_ref=*/false};
}

VirtualTree::VirtualTree() {
    // Пул выше окна просмотра, и ScrollViewer без вертикальной прокрутки поставил
    // бы его по центру: строки ложатся от верхнего края, лишнее срезается снизу.
    Apply {rows_, vAlign.top};

    Apply {bar_,
        column = 1,
        orientation.vertical,
        indicatorMode = ScrollingIndicatorMode::MouseIndicator,
        minimum = 0.0,
        smallChange = 1.0,
        onValueChanged = [this](ScrollBar const& bar, RangeBaseValueChangedEventArgs&) {
            scrollTo(static_cast<uint32_t>(std::lround(bar.value())));
        },
    };

    Apply {root_,
        columnDefinitions = u"*,auto",
        // Прозрачный фон ловит колесо и над пустым местом под последней строкой.
        background = brushes.SubtleFillColor.Transparent,
        onSizeChanged = [this](Grid const&, SizeChangedEventArgs& args) {
            resize(args.newSize().height);
        },
        onPointerWheelChanged = [this](Grid const& grid, PointerRoutedEventArgs& args) {
            scrollBy(-args.getCurrentPoint(grid).properties().mouseWheelDelta() / wheelUnitsPerLine);
            args.handled(true);
        },
        ScrollViewer {
            column = 0,
            horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
            verticalScrollBarVisibility = ScrollBarVisibility::Disabled,
            horizontalScrollMode = ScrollMode::Enabled,
            verticalScrollMode = ScrollMode::Disabled,
            content = rows_,
        },
        bar_,
    };
}

VirtualTree::Row VirtualTree::makeRow(uint32_t slot) {
    Row row;

    Apply {row.glyph,
        width = indentStep_,
        fontFamily = FontFamily {u"Segoe Fluent Icons"},
        fontSize = 10.0,
        textAlignment.center,
        vAlign.center,
        onTapped = [this, slot](TextBlock const&, TappedRoutedEventArgs& args) {
            if (model_) {
                model_->toggleExpanded(first() + slot);
                updateBar();
                render();
            }
            args.handled(true);
        },
    };

    // Щелчок по отметке строку не выбирает: он до неё не доходит. Поле шире
    // знака, чтобы попадать в него не целясь.
    Apply {row.check,
        fontFamily = FontFamily {iconFont},
        fontSize = 20.0,
        width = 28.0,
        textAlignment.center,
        vAlign.center,
        Margin {2, 0, 0, 0},
        onTapped = [this, slot](TextBlock const&, TappedRoutedEventArgs& args) {
            if (model_) {
                model_->toggleChecked(first() + slot);
                render();
            }
            args.handled(true);
        },
    };

    // Знаки размера 16 и кегль 16: знак ложится на свою сетку без масштаба.
    Apply {row.icon,
        fontFamily = FontFamily {iconFont},
        fontSize = 16.0,
        vAlign.center,
        Margin {2, 0, 0, 0},
    };

    Apply {row.text,
        vAlign.center,
        Margin {4, 0, 12, 0},
    };

    Apply {row.panel,
        orientation.horizontal,
        height = rowHeight_,
        background = brushes.SubtleFillColor.Transparent,
        onTapped = [this, slot](StackPanel const&, TappedRoutedEventArgs&) {
            if (model_) {
                model_->invoke(first() + slot);
                render();
            }
        },
        onDoubleTapped = [this, slot](StackPanel const&, DoubleTappedRoutedEventArgs&) {
            if (model_) {
                model_->toggleExpanded(first() + slot);
                updateBar();
                render();
            }
        },
        row.indent,
        row.glyph,
        row.check,
        row.icon,
        row.text,
    };

    return row;
}

Brush const& VirtualTree::iconBrush(RowIcon const& icon) {
    for (auto const& [known, brush] : fills_) {
        if (known == &icon) {
            return brush;
        }
    }

    // Радиус 0.5 доходит до краёв знака: светлая середина, тёмный край.
    return fills_.emplace_back(&icon, RadialGradientBrush {
        center = {0.5, 0.5},
        gradientOrigin = {0.5, 0.5},
        radiusX = 0.5,
        radiusY = 0.5,
        GradientStop {icon.inner, offset = 0.0},
        GradientStop {icon.outer, offset = 1.0},
    }).second;
}

void VirtualTree::model(intrusive_ptr<TreeModel> value) {
    model_ = std::move(value);
    top_ = 0;
    updateBar();
    render();
}

void VirtualTree::refresh() {
    updateBar();
    render();
}

void VirtualTree::resize(double height) {
    visible_ = static_cast<uint32_t>(height / rowHeight_);

    // Частично видимая нижняя строка тоже строится, и запас — с обеих сторон.
    auto const needed = static_cast<uint32_t>(std::ceil(height / rowHeight_)) + 2 * reserve_;
    while (pool_.size() < needed) {
        pool_.push_back(makeRow(static_cast<uint32_t>(pool_.size())));
        rows_.children().append(pool_.back().panel);
    }

    updateBar();
    render();
}

void VirtualTree::scrollBy(int64_t lines) {
    scrollTo(static_cast<uint32_t>(std::max<int64_t>(0, int64_t{top_} + lines)));
}

void VirtualTree::scrollTo(uint32_t top) {
    uint32_t const last = size() > visible_ ? size() - visible_ : 0;
    top = std::min(top, last);
    if (top == top_) {
        return;
    }

    top_ = top;
    // Полоса сообщит о своём значении обратно; оно уже равно top_ и ничего не
    // перерисует.
    bar_.value(top_);
    render();
}

void VirtualTree::updateBar() {
    uint32_t const last = size() > visible_ ? size() - visible_ : 0;
    top_ = std::min(top_, last);

    bar_.maximum(last);
    bar_.viewportSize(visible_);
    bar_.largeChange(visible_ > 1 ? visible_ - 1 : 1);
    bar_.value(top_);
    bar_.visibility(last > 0 ? Visibility::Visible : Visibility::Collapsed);
}

void VirtualTree::render() {
    uint32_t const count = size();
    uint32_t const base = first();

    for (uint32_t slot = 0; slot < pool_.size(); ++slot) {
        Row const& row = pool_[slot];
        uint32_t const index = base + slot;
        if (index >= count) {
            row.panel.visibility(Visibility::Collapsed);
            continue;
        }

        TreeRow const data = model_->row(index);
        row.panel.visibility(Visibility::Visible);
        Brush const& fill = data.selected ? static_cast<Brush const&>(brushes.SubtleFillColor.Secondary)
                                          : static_cast<Brush const&>(brushes.SubtleFillColor.Transparent);
        row.panel.background(fill);
        row.indent.width(data.depth * indentStep_);
        row.glyph.text(data.expander == Expander::Collapsed ? collapsedGlyph
                       : data.expander == Expander::Expanded ? expandedGlyph
                                                             : std::u16string_view{});
        row.check.visibility(data.check == Check::None ? Visibility::Collapsed : Visibility::Visible);
        bool const checked = data.check == Check::Checked;
        row.check.text(checked ? checkedGlyph : uncheckedGlyph);
        row.check.foreground(checked ? static_cast<Brush const&>(brushes.Accent.FillColor.Default)
                                     : static_cast<Brush const&>(brushes.Text.FillColor.Secondary));

        glyph_.clear();
        if (data.icon) {
            core::unicode::append_utf16(glyph_, data.icon->glyph);
            row.icon.foreground(iconBrush(*data.icon));
        }
        row.icon.text(std::wstring_view {glyph_});

        text_.clear();
        if (auto const utf8 = core::unicode::checked(data.text)) {
            core::unicode::append_utf16(text_, *utf8);
        }
        row.text.text(std::wstring_view {text_});
    }

    // Строки запаса сверху уходят за край: панель сдвинута на их высоту.
    rows_.translation(Vector3 {0, -static_cast<float>((top_ - base) * rowHeight_), 0});
}

}  // namespace editor
