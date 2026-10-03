#include "VirtualTree.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>

#include "FocusEffect.h"
#include "FontFamily.h"
#include "generated/Microsoft.UI.Input.h"
#include "generated/brushes.h"

using namespace wxl;
using namespace wxl::dsl;

namespace editor {

using wxl::core::intrusive_ptr;

namespace {

// Шевроны Segoe Fluent Icons: ChevronRight и ChevronDown.
constexpr zstring_view collapsedGlyph = u"\uE76C";
constexpr zstring_view expandedGlyph = u"\uE70D";

// Отметка — знак шрифта значков, а не CheckBox: у того галочка — AnimatedIcon,
// и при листании, когда строка получает другой тип, она рисовалась заново.
// checkbox_checked, checkbox_unchecked и checkbox_indeterminate, размер 20.
constexpr zstring_view checkedGlyph = u"\uF28D";
constexpr zstring_view uncheckedGlyph = u"\uF291";
constexpr zstring_view indeterminateGlyph = u"\uE2FE";

Brush fillOf(RowIcon const& icon) {
    if (icon.outer == icon.inner) {
        return Template<SolidColorBrush> {icon.outer}.build();
    }
    return iconFill(icon.outer, icon.inner).build();
}

constexpr zstring_view iconFont =u"ms-appx:///Assets/FluentSystemIcons-Regular.ttf#FluentSystemIcons-Regular";

// Колесо даёт 120 на щелчок; щелчок листает три строки, как в проводнике.
constexpr int32_t wheelUnitsPerLine = 40;

}  // namespace

intrusive_ptr<VirtualTree> VirtualTree::create() {
    return {new VirtualTree{}, /*add_ref=*/false};
}

VirtualTree::VirtualTree() {
    // Пул выше окна просмотра, и ScrollViewer без вертикальной прокрутки поставил
    // бы его по центру: строки ложатся от верхнего края, лишнее срезается снизу.
    // Колесо ловят сами строки: ScrollViewer, у которого есть куда листать
    // вбок, иначе забрал бы его себе и листал бы вбок. Прозрачный фон и высота
    // не меньше окна — чтобы ловить его и над пустым местом под последней
    // строкой.
    Apply {rows_,
        vAlign.top,
        background = brushes.SubtleFillColor.Transparent,
        onPointerWheelChanged = [this](StackPanel const& panel, PointerRoutedEventArgs& args) { wheel(panel, args); },
    };

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

    // Клавиши — у корня: он берёт фокус, когда по дереву щёлкают. Дерево в
    // фокусе метит выбранную строку чертой цвета акцента.
    Apply {root_,
        columnDefinitions = u"*,auto",
        background = brushes.SubtleFillColor.Transparent,
        isTabStop = true,
        onKeyDown = [this](Grid const&, KeyRoutedEventArgs& args) { key(args); },
        onCharacterReceived = [this](Grid const&, CharacterReceivedRoutedEventArgs& args) {
            if (typeAhead(args.character())) {
                args.handled(true);
            }
        },
        onGotFocus = [this](Grid const&, RoutedEventArgs&) { focus(true); },
        onLostFocus = [this](Grid const&, RoutedEventArgs&) { focus(false); },
        onSizeChanged = [this](Grid const&, SizeChangedEventArgs& args) {
            resize(args.newSize().height);
        },
        // Колесо над полосой.
        onPointerWheelChanged = [this](Grid const& grid, PointerRoutedEventArgs& args) { wheel(grid, args); },
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
    // Фокус дерево показывает само, чертой у выбранной строки.
    FocusEffect::exclude(root_);
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
            takeFocus();
            if (model_) {
                model_->toggleExpanded(first() + slot);
                updateBar();
                render();
            }
            args.handled(true);
        },
        // Второй щелчок двойного по шеврону и по отметке — такой же щелчок, а
        // не двойной по строке.
        onDoubleTapped = [this, slot](TextBlock const&, DoubleTappedRoutedEventArgs& args) {
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
            takeFocus();
            if (model_) {
                model_->toggleChecked(first() + slot);
                render();
            }
            args.handled(true);
        },
        onDoubleTapped = [this, slot](TextBlock const&, DoubleTappedRoutedEventArgs& args) {
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
        Margin {4, 0, 0, 0},
    };

    Apply {row.detail,
        vAlign.center,
        foreground = brushes.Text.FillColor.Secondary,
        Margin {8, 0, 12, 0},
    };

    Apply {row.marker,
        width = 3.0,
        height = 16.0,
        CornerRadius {1.5},
        vAlign.center,
        background = brushes.Accent.FillColor.Default,
        opacity = 0.0,
    };

    Apply {row.panel,
        orientation.horizontal,
        height = rowHeight_,
        background = brushes.SubtleFillColor.Transparent,
        // Второй щелчок двойного приходит как DoubleTapped, а не Tapped:
        // узел переключается один раз.
        onTapped = [this, slot](StackPanel const&, TappedRoutedEventArgs&) {
            takeFocus();
            if (model_) {
                uint32_t const index = first() + slot;
                model_->invoke(index);
                if (pool_[slot].expandable) {
                    model_->toggleExpanded(index);
                    updateBar();
                }
                render();
            }
        },
        onDoubleTapped = [this, slot](StackPanel const&, DoubleTappedRoutedEventArgs&) {
            if (model_) {
                model_->activate(first() + slot);
                render();
            }
        },
        onPointerEntered = [this, slot](StackPanel const&, PointerRoutedEventArgs&) {
            hovered_ = slot;
            paint(slot);
        },
        // Выход из одной ячейки может прийти и после входа в соседнюю.
        onPointerExited = [this, slot](StackPanel const&, PointerRoutedEventArgs&) {
            if (hovered_ == slot) {
                hovered_.reset();
                paint(slot);
            }
        },
        row.marker,
        row.indent,
        row.glyph,
        row.check,
        row.icon,
        row.text,
        row.detail,
    };

    return row;
}

Brush const& VirtualTree::iconBrush(RowIcon const& icon) {
    for (auto const& [known, brush] : fills_) {
        if (known == &icon) {
            return brush;
        }
    }

    return fills_.emplace_back(&icon, fillOf(icon)).second;
}

Template<RadialGradientBrush> iconFill(Color outer, Color inner) {
    // Радиус 0.5 доходит до краёв знака: светлая середина, тёмный край.
    return {
        center = {0.5, 0.5},
        gradientOrigin = {0.5, 0.5},
        radiusX = 0.5,
        radiusY = 0.5,
        GradientStop {inner, offset = 0.0},
        GradientStop {outer, offset = 1.0},
    };
}

TextBlock iconText(RowIcon const& icon, double size) {
    std::u16string glyph;
    core::unicode::append_utf16(glyph, icon.glyph);
    return TextBlock {
        fontFamily = FontFamily {iconFont},
        fontSize = size,
        foreground = fillOf(icon),
        text = glyph,
    };
}

void VirtualTree::model(intrusive_ptr<TreeModel> value) {
    if (value != model_) {
        model_ = std::move(value);
        top_ = 0;
    }
    updateBar();
    render();
}

void VirtualTree::reveal(uint32_t index) {
    updateBar();
    if (index < top_ || index >= top_ + visible_) {
        top_ = index > visible_ / 3 ? index - visible_ / 3 : 0;
        updateBar();
    }
    render();
}

void VirtualTree::refresh() {
    updateBar();
    render();
}

void VirtualTree::details(bool show) {
    details_ = show;
    render();
}

void VirtualTree::resize(double height) {
    // Ширину меняет граница раздвижки на каждом движении мыши, а пулу и полосе
    // до ширины дела нет: их меняет только высота.
    if (height == height_) {
        return;
    }
    height_ = height;

    visible_ = static_cast<uint32_t>(height / rowHeight_);
    // Панель сдвинута вверх на строки запаса: им — сверх высоты окна.
    rows_.minHeight(height + reserve_ * rowHeight_);

    // Частично видимая нижняя строка тоже строится, и запас — с обеих сторон.
    auto const needed = static_cast<uint32_t>(std::ceil(height / rowHeight_)) + 2 * reserve_;
    while (pool_.size() < needed) {
        pool_.push_back(makeRow(static_cast<uint32_t>(pool_.size())));
        rows_.children().append(pool_.back().panel);
    }

    updateBar();
    render();
}

void VirtualTree::wheel(UIElement const& source, PointerRoutedEventArgs& args) {
    // Колесо с Ctrl — масштаб окна (ZoomEffect), а наклон колеса — ScrollViewer'у,
    // листать вбок.
    if (static_cast<uint32_t>(args.keyModifiers()) & static_cast<uint32_t>(VirtualKeyModifiers::Control)) {
        return;
    }
    auto const properties = args.getCurrentPoint(source).properties();
    if (properties.isHorizontalMouseWheel()) {
        return;
    }
    scrollBy(-properties.mouseWheelDelta() / wheelUnitsPerLine);
    args.handled(true);
}

// Стрелки и страницы ходят по видимым строкам, влево и вправо — сворачивают,
// раскрывают и ходят к родителю и первому ребёнку, пробел — отметка, Enter —
// как двойной щелчок.
void VirtualTree::key(KeyRoutedEventArgs& args) {
    uint32_t const count = size();
    if (count == 0) {
        return;
    }
    auto const current = model_->selectedRow();
    int64_t const at = current ? int64_t {*current} : int64_t {top_};
    int64_t const page = visible_ > 1 ? visible_ - 1 : 1;
    auto const go = [this, count](int64_t index) { select(static_cast<uint32_t>(std::clamp<int64_t>(index, 0, count - 1))); };

    switch (args.key()) {
        case VirtualKey::Up: go(current ? at - 1 : at); break;
        case VirtualKey::Down: go(current ? at + 1 : at); break;
        case VirtualKey::PageUp: go(at - page); break;
        case VirtualKey::PageDown: go(at + page); break;
        case VirtualKey::Home: go(0); break;
        case VirtualKey::End: go(count - 1); break;
        case VirtualKey::Left:
        case VirtualKey::Right: {
            if (!current) {
                go(at);
                break;
            }
            TreeRow const data = model_->row(*current);
            bool const right = args.key() == VirtualKey::Right;
            if (data.expander == (right ? Expander::Collapsed : Expander::Expanded)) {
                model_->toggleExpanded(*current);
                updateBar();
            } else if (right) {
                if (data.expander == Expander::Expanded) {
                    go(at + 1);
                }
            } else {
                // Родитель — ближайшая строка выше с меньшей глубиной.
                for (int64_t index = at - 1; index >= 0; --index) {
                    if (model_->row(static_cast<uint32_t>(index)).depth < data.depth) {
                        go(index);
                        break;
                    }
                }
            }
            break;
        }
        // + и - цифрового блока раскрывают и сворачивают, * раскрывает всё
        // под строкой.
        case VirtualKey::Add:
        case VirtualKey::Subtract:
            if (current && model_->row(*current).expander ==
                               (args.key() == VirtualKey::Add ? Expander::Collapsed : Expander::Expanded)) {
                model_->toggleExpanded(*current);
                updateBar();
            }
            break;
        case VirtualKey::Multiply:
            if (current) {
                expandAll(*current);
            }
            break;
        case VirtualKey::Space:
            if (current && model_->row(*current).check != Check::None) {
                model_->toggleChecked(*current);
            }
            break;
        // Enter у раскрывающейся строки раскрывает или сворачивает её, у
        // остальных — как двойной щелчок.
        case VirtualKey::Enter:
            if (current) {
                if (model_->row(*current).expander != Expander::None) {
                    model_->toggleExpanded(*current);
                    updateBar();
                } else {
                    model_->activate(*current);
                }
            }
            break;
        default: return;
    }
    args.handled(true);
    render();
}

// Строки под раскрытой встают следом за ней, поэтому раскрывается всё, что
// ниже, пока глубина больше, чем у самой строки.
void VirtualTree::expandAll(uint32_t index) {
    uint16_t const depth = model_->row(index).depth;
    for (uint32_t at = index; at < size(); ++at) {
        TreeRow const data = model_->row(at);
        if (at > index && data.depth <= depth) {
            break;
        }
        if (data.expander == Expander::Collapsed) {
            model_->toggleExpanded(at);
        }
    }
    updateBar();
}

void VirtualTree::select(uint32_t index) {
    model_->invoke(index);
    scrollIntoView(index);
}

// Строка встаёт у ближнего края окна, если её не видно.
void VirtualTree::scrollIntoView(uint32_t index) {
    if (index < top_) {
        top_ = index;
    } else if (visible_ > 0 && index >= top_ + visible_) {
        top_ = index - visible_ + 1;
    } else {
        return;
    }
    updateBar();
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
        Row& row = pool_[slot];
        uint32_t const index = base + slot;
        if (index >= count) {
            row.panel.visibility(Visibility::Collapsed);
            continue;
        }

        TreeRow const data = model_->row(index);
        row.panel.visibility(Visibility::Visible);
        row.selected = data.selected;
        row.expandable = data.expander != Expander::None;
        paint(slot);
        row.indent.width(data.depth * indentStep_);
        row.glyph.text(data.expander == Expander::Collapsed ? collapsedGlyph
                       : data.expander == Expander::Expanded ? expandedGlyph
                                                             : zstring_view{});
        row.check.visibility(data.check == Check::None ? Visibility::Collapsed : Visibility::Visible);
        bool const checked = data.check != Check::Unchecked;
        row.check.text(data.check == Check::Checked         ? checkedGlyph
                       : data.check == Check::Indeterminate ? indeterminateGlyph
                                                            : uncheckedGlyph);
        row.check.foreground(checked ? static_cast<Brush const&>(brushes.Accent.FillColor.Default)
                                     : static_cast<Brush const&>(brushes.Text.FillColor.Secondary));

        glyph_.clear();
        if (data.icon) {
            core::unicode::append_utf16(glyph_, data.icon->glyph);
            row.icon.foreground(iconBrush(*data.icon));
        }
        row.icon.text(glyph_);

        text_.clear();
        if (auto const utf8 = core::unicode::checked(data.text)) {
            core::unicode::append_utf16(text_, *utf8);
        }
        row.text.text(text_);

        detail_.clear();
        if (auto const utf8 = core::unicode::checked(details_ ? data.detail : std::string_view {})) {
            core::unicode::append_utf16(detail_, *utf8);
        }
        row.detail.text(detail_);
    }

    // Строки запаса сверху уходят за край: панель сдвинута на их высоту.
    rows_.translation(Vector3 {0, -static_cast<float>((top_ - base) * rowHeight_), 0});
}

void VirtualTree::paint(uint32_t slot) {
    // Строка под указателем светлее выбранной: выбор виден и под указателем.
    Row const& row = pool_[slot];
    Brush const& fill = row.selected       ? static_cast<Brush const&>(brushes.SubtleFillColor.Secondary)
                        : hovered_ == slot ? static_cast<Brush const&>(brushes.SubtleFillColor.Tertiary)
                                           : static_cast<Brush const&>(brushes.SubtleFillColor.Transparent);
    row.panel.background(fill);
    row.marker.opacity(row.selected && focused_ ? 1.0 : 0.0);
}

void VirtualTree::takeFocus() {
    root_.focus(FocusState::Pointer);
}

void VirtualTree::focus(bool focused) {
    if (focused != focused_) {
        focused_ = focused;
        for (uint32_t slot = 0; slot < pool_.size(); ++slot) {
            paint(slot);
        }
    }
}

// Поиск по первым буквам, как в проводнике: буквы, набранные подряд, — начало
// имени; та же буква снова — следующая строка на неё. Пауза больше секунды
// начинает поиск заново. Ищется среди видимых строк, от выбранной и по кругу.
bool VirtualTree::typeAhead(char16_t character) {
    uint32_t const count = size();
    if (count == 0 || character <= u' ') {
        return false;
    }
    auto const now = std::chrono::steady_clock::now();
    if (now - typedAt_ > std::chrono::seconds {1}) {
        typed_.clear();
    }
    typedAt_ = now;
    char16_t const lower = static_cast<char16_t>(std::towlower(character));
    bool const repeat = typed_.size() == 1 && typed_.front() == lower;
    if (!repeat) {
        typed_ += lower;
    }

    // Новый поиск и повтор буквы — со следующей строки, продолжение слова — с
    // той же: она может подходить и дальше.
    auto const current = model_->selectedRow();
    uint32_t const from = current ? (typed_.size() == 1 ? *current + 1 : *current) : top_;
    for (uint32_t step = 0; step < count; ++step) {
        uint32_t const index = (from + step) % count;
        std::string_view const text = model_->row(index).text;
        if (text.size() < typed_.size()) {
            continue;
        }
        bool const matches = std::ranges::equal(typed_, text.substr(0, typed_.size()), {}, {}, [](char c) {
            return static_cast<char16_t>(std::tolower(static_cast<unsigned char>(c)));
        });
        if (matches) {
            select(index);
            render();
            return true;
        }
    }
    return true;
}

}  // namespace editor
