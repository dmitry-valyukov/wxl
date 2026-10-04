#pragma once

// wxl::SplitPanel -- две части рядом и зазор между ними, за который границу
// тянут мышью:
//
//     SplitPanel {
//         openPaneLength = 520.0,
//         spacing = 8.0,
//         pane = types,       // левая часть
//         content = members,  // правая
//     }
//
// Словарь -- от SplitView: часть постоянного размера pane, вторая content,
// размер pane -- openPaneLength, её место -- panePlacement (right -- после
// content); зазор -- spacing, ориентация -- orientation, как у StackPanel:
// vertical ставит части одну над другой, и тогда left -- верх, right -- низ.
// Безымянные элементы в скобках -- pane, затем content. В отличие от SplitView
// панель не выдвигается и не закрывается: обе части видны всегда, а границу
// между ними двигает указатель -- в зазоре курсор-стрелка поперёк зазора,
// нажатие захватывает указатель, движение меняет размер pane. Размер
// держится в пределах MinWidth (MinHeight) обеих частей; при смене размера
// самой панели меняется content.
//
// Объект WinUI -- свой: наследник Panel через агрегацию (шаблон cppwinrt
// PanelT), и раскладка -- его MeasureOverride и ArrangeOverride: левая часть,
// зазор, правая часть. В зазоре поверх частей лежит прозрачный Rectangle -- он
// ловит указатель, поэтому фона самой панели не нужно, и она ничего не
// закрашивает. В дереве элементов он зовётся wxl.SplitPanel.

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include "impl/member.h"

namespace wxl {

class SplitPanel : public Panel {
    using base_t = Panel;

public:
    class Impl;

    SplitPanel();

    template <typename... Setters>
        requires impl::setter_pack<SplitPanel, Setters...>
    explicit SplitPanel(Setters&&... setters) : SplitPanel() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    void pane(UIElement const& value) const;
    UIElement pane() const;

    void content(UIElement const& value) const;
    UIElement content() const;

    /// Размер pane по оси панели; его же меняет перетаскивание границы.
    void openPaneLength(double value) const;
    double openPaneLength() const;

    /// Horizontal -- части рядом, vertical -- одна над другой.
    void orientation(Orientation value) const;
    Orientation orientation() const;

    /// Left -- pane первой (слева или сверху), right -- после content.
    void panePlacement(SplitViewPanePlacement value) const;
    SplitViewPanePlacement panePlacement() const;

    /// Ширина зазора -- он же поле, за которое тянут.
    void spacing(double value) const;
    double spacing() const;

    /// Безымянный элемент -- pane, если её ещё нет, иначе content.
    using base_t::setPositional;
    void setPositional(UIElement const& value) const;
    void setPositional(Orientation value) const { orientation(value); }
    void setPositional(SplitViewPanePlacement value) const { panePlacement(value); }

protected:
    explicit SplitPanel(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
