#pragma once

// Общее у обеих программ пробы (окно -- probes.cpp, без окна -- windowless.cpp): элемент, смерть которого видна,
// фабрика повторителя и шаблон ListView, повторяющий машинку wxl. Только проекция cppwinrt, без wxl.

#include "platform.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace probe {

namespace wf = winrt::Windows::Foundation;
namespace wfc = winrt::Windows::Foundation::Collections;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

// Сколько элементов построено и сколько ещё живо. Живёт в статической памяти пробы: XAML может отпустить элемент и
// после того, как проба подвела итог.
struct Census {
    int built = 0;
    int alive = 0;
};

// Элемент, чья смерть видна: панель высотой в строку поверх настоящего Panel (агрегация cppwinrt, как
// sandbox/CppWinRTPanel), считает себя в конструкторе и деструкторе. Деструктор зовётся, когда XAML отпустил последнюю
// ссылку на внешний объект, -- то есть утечка видна счётчиком, а не памятью процесса.
struct Counted : controls::PanelT<Counted> {
    Counted(Census* census, float height) : census_(census), height_(height) {
        ++census_->built;
        ++census_->alive;
    }

    ~Counted() { --census_->alive; }

    // Своё имя: без него cppwinrt отвечает именем первого интерфейса из implements.
    winrt::hstring GetRuntimeClassName() const { return L"wxl.probe.Counted"; }

    wf::Size MeasureOverride(wf::Size available) {
        return {std::isfinite(available.Width) ? available.Width : 120.0f, height_};
    }

    wf::Size ArrangeOverride(wf::Size final) { return final; }

private:
    Census* census_;
    float height_;
};

inline xaml::UIElement makeCounted(Census* census, float height) {
    return winrt::make_self<Counted>(census, height).as<xaml::UIElement>();
}

// Что фабрика делает с элементом, который повторитель ей вернул.
enum class Recycle {
    Nothing,           // как impl/item_factory.cpp сейчас: пустой RecycleElement
    RemoveFromParent,  // снимает элемент с детей повторителя -- то, что ViewManager делает сам, когда фабрики нет
};

// IElementFactory для ItemsRepeater и ItemsView: каждый элемент -- новый Counted (у ItemsView -- в ItemContainer, иначе
// он его не примет). На winrt::implements: проба спрашивает о поведении XAML, а не о своём COM фабрики.
struct Factory : winrt::implements<Factory, xaml::IElementFactory> {
    Factory(Census* census, Recycle recycle, bool inContainer) : census_(census), recycle_(recycle), inContainer_(inContainer) {}

    xaml::UIElement GetElement(xaml::ElementFactoryGetArgs const&) {
        auto const element = makeCounted(census_, 24.0f);
        if (!inContainer_) return element;
        controls::ItemContainer container;
        container.Child(element);
        return container;
    }

    void RecycleElement(xaml::ElementFactoryRecycleArgs const& args) {
        ++recycled;
        if (recycle_ != Recycle::RemoveFromParent) return;
        if (auto const panel = args.Parent().try_as<controls::Panel>()) {
            uint32_t at = 0;
            if (panel.Children().IndexOf(args.Element(), at)) panel.Children().RemoveAt(at);
        }
    }

    int recycled = 0;

private:
    Census* census_;
    Recycle recycle_;
    bool inContainer_;
};

struct TemplateStats {
    int phase0 = 0;    // ContainerContentChanging не из очереди повторного использования
    int phase1 = 0;    // обратный вызов фазы 1 -- там строится элемент
    int recycled = 0;  // InRecycleQueue -- там Content(nullptr)
};

// Шаблон ListView, как его ставит wxl.ui/src/impl/item_template.cpp: одноразовый DataTemplate с пустым ContentControl,
// элемент кладётся в него на фазе 1, а в очереди повторного использования место очищается Content(nullptr). Повторено
// здесь, а не взято из wxl, чтобы проба спрашивала XAML, а не нынешний код wxl.
inline void hookTemplate(controls::ListViewBase const& list, Census* census, TemplateStats* stats) {
    list.ItemTemplate(xaml::Markup::XamlReader::Load(
                          L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'>"
                          L"<ContentControl HorizontalContentAlignment='Stretch' VerticalContentAlignment='Stretch'/>"
                          L"</DataTemplate>")
                          .as<xaml::DataTemplate>());
    list.ContainerContentChanging([census, stats](controls::ListViewBase const&,
                                                  controls::ContainerContentChangingEventArgs const& args) {
        auto const container = args.ItemContainer().try_as<controls::ContentControl>();
        if (!container) return;
        if (args.InRecycleQueue()) {
            ++stats->recycled;
            if (auto const place = container.ContentTemplateRoot().try_as<controls::ContentControl>()) place.Content(nullptr);
            return;
        }
        ++stats->phase0;
        args.RegisterUpdateCallback(1, [census, stats](controls::ListViewBase const&,
                                                       controls::ContainerContentChangingEventArgs const& later) {
            ++stats->phase1;
            auto const target = later.ItemContainer().try_as<controls::ContentControl>();
            if (!target) return;
            if (auto const place = target.ContentTemplateRoot().try_as<controls::ContentControl>()) {
                place.Content(makeCounted(census, 24.0f));
            }
        });
    });
}

// Сколько контейнеров списка сейчас держат элемент в своём месте.
inline int placedIn(controls::ListViewBase const& list) {
    int placed = 0;
    auto const panel = list.ItemsPanelRoot();
    if (!panel) return 0;
    for (auto const child : panel.Children()) {
        auto const container = child.try_as<controls::ContentControl>();
        if (!container) continue;
        auto const place = container.ContentTemplateRoot().try_as<controls::ContentControl>();
        if (place && place.Content()) ++placed;
    }
    return placed;
}

// Первый потомок типа T в дереве визуальных элементов (обход в ширину), пустой -- если нет.
template <typename T>
T findDescendant(xaml::DependencyObject const& root) {
    if (!root) return nullptr;
    std::vector<xaml::DependencyObject> level {root};
    while (!level.empty()) {
        std::vector<xaml::DependencyObject> next;
        for (auto const& node : level) {
            int32_t const count = xaml::Media::VisualTreeHelper::GetChildrenCount(node);
            for (int32_t i = 0; i < count; ++i) {
                auto const child = xaml::Media::VisualTreeHelper::GetChild(node, i);
                if (auto const found = child.try_as<T>()) return found;
                next.push_back(child);
            }
        }
        level = std::move(next);
    }
    return nullptr;
}

}  // namespace probe
