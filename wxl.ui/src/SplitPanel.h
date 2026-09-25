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
// Словарь -- от SplitView: левая часть pane, правая content, ширина левой
// openPaneLength; зазор -- spacing, как у StackPanel. Безымянные элементы в
// скобках -- левая часть, затем правая. В отличие от SplitView панель не
// выдвигается и не закрывается: обе части видны всегда, а границу между ними
// двигает указатель -- в зазоре курсор SizeWestEast, нажатие захватывает
// указатель, движение меняет ширину левой части. Ширина держится в пределах
// MinWidth обеих частей.
//
// Объект WinUI -- свой: наследник Panel через агрегацию (шаблон cppwinrt
// PanelT), и раскладка -- его MeasureOverride и ArrangeOverride: левая часть,
// зазор, правая часть. В зазоре поверх частей лежит прозрачный Rectangle -- он
// ловит указатель, поэтому фона самой панели не нужно, и она ничего не
// закрашивает. В дереве элементов он зовётся wxl.SplitPanel.

#include "generated/Microsoft.UI.Xaml.Controls.h"
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

    /// Ширина левой части; её же меняет перетаскивание границы.
    void openPaneLength(double value) const;
    double openPaneLength() const;

    /// Ширина зазора -- он же поле, за которое тянут.
    void spacing(double value) const;
    double spacing() const;

    /// Безымянный элемент -- левая часть, если её ещё нет, иначе правая.
    using base_t::setPositional;
    void setPositional(UIElement const& value) const;

protected:
    explicit SplitPanel(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
