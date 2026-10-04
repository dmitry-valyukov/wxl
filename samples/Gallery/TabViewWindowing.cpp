// Окно образца TabView, в котором вкладки вытаскиваются в новые окна и перекладываются между окнами.

#include "TabViewWindowing.h"

#include <memory>
#include <string>
#include <vector>

#include "RepeaterData.h"
#include "Shell.h"
#include <wxl/Microsoft.UI.Windowing.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include "pch.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Одно окно образца: само окно и полоса вкладок в нём.
struct TabWindow {
    Window window;
    TabView tabs;
    // Окно, которое сделано для вкладок, вытаскиваемых сейчас: между «нужно окно» и «вот вкладки».
    std::shared_ptr<TabWindow> tearOutWindow;
};

std::vector<std::weak_ptr<TabWindow>>& allWindows() {
    static std::vector<std::weak_ptr<TabWindow>> windows;
    return windows;
}

std::shared_ptr<TabWindow> makeWindow();

// TabContentSampleControl оригинала: заголовок страницы, пояснение и переключатель, который включает кольцо.
UIElement tabContent(std::u16string const& page) {
    auto const ring = ProgressRing {hAlign.left};
    return StackPanel {
        Padding {12},
        TextBlock {styles.TextBlock.Title, page},
        TextBlock {styles.TextBlock.Subtitle, u"Drag the Tab outside of the window to spawn a new window."},
        TextBlock {styles.TextBlock.Body, textWrapping = TextWrapping::Wrap,
                   u"Notice that the state of the Tab is maintained in the new window. For example, if you toggle the ToggleSwitch ON, it will "
                   u"remain ON in the new window."},
        ToggleSwitch {Margin {0, 8}, header = u"Turn on ProgressRing", onToggled = [ring](ToggleSwitch const& self) { ring.isActive(self.isOn()); }},
        ring,
    };
}

TabViewItem tab(std::u16string const& title, std::u16string const& page) {
    return TabViewItem {header = title, iconSource = SymbolIconSource {symbol = Symbol::Placeholder}, content = tabContent(page)};
}

// GetParentTabView: the strip a tab sits in now.
core::nullable<TabView> parentTabView(UIElement const& tab) {
    DependencyObject current = tab;
    while (current) {
        if (current.is<TabView>()) {
            return current.try_as<TabView>();
        }
        current = VisualTreeHelper::getParent(current);
    }
    return {};
}

// CloseWindowIfEmpty: the window of a strip that has no tabs left closes.
void closeWindowIfEmpty(TabView const& tabs) {
    if (tabs.tabItems().size() != 0) {
        return;
    }
    for (auto const& weak : allWindows()) {
        if (auto const model = weak.lock(); model && model->tabs.is_same_object(tabs)) {
            model->window.close();
            return;
        }
    }
}

std::shared_ptr<TabWindow> makeWindow() {
    auto const model = std::make_shared<TabWindow>();
    std::weak_ptr<TabWindow> const weak = model;
    auto const dragRegion = Grid {background = colors.transparent, minWidth = 188};

    model->tabs = TabView {
        vAlign.stretch,
        canTearOutTabs = true,
        tabStripHeader = Grid {background = colors.transparent},
        tabStripFooter = dragRegion,
        onAddTabButtonClick = [](TabView const& sender) { sender.tabItems().append(tab(u"New Item", u"New Item")); },
        onTabCloseRequested = [](TabView const& sender, TabViewTabCloseRequestedEventArgs& args) {
            sender.tabItems().remove(args.tab());
            closeWindowIfEmpty(sender);
        },
        // A tab is dragged out of the strip: the window it lands in is made now and named to the TabView.
        onTabTearOutWindowRequested = [weak](TabView const&, TabViewTabTearOutWindowRequestedEventArgs& args) {
            if (auto const self = weak.lock()) {
                self->tearOutWindow = makeWindow();
                args.newWindowId(self->tearOutWindow->window.appWindow().id());
            }
        },
        onTabTearOutRequested = [weak](TabView const& sender, TabViewTabTearOutRequestedEventArgs& args) {
            auto const self = weak.lock();
            if (!self || !self->tearOutWindow) {
                return;
            }
            for (auto const& torn : args.tabs()) {
                if (auto const source = parentTabView(torn)) {
                    (*source).tabItems().remove(torn);
                }
                self->tearOutWindow->tabs.tabItems().append(torn);
            }
            // Cleared now that the tear-out is complete, so that a second one does not find a stale window.
            self->tearOutWindow.reset();
            closeWindowIfEmpty(sender);
        },
        onExternalTornOutTabsDropping = [](TabView const&, TabViewExternalTornOutTabsDroppingEventArgs& args) { args.allowDrop(true); },
        onExternalTornOutTabsDropped = [](TabView const& sender, TabViewExternalTornOutTabsDroppedEventArgs& args) {
            int position = 0;
            for (auto const& torn : args.tabs()) {
                auto const source = parentTabView(torn);
                if (source) {
                    (*source).tabItems().remove(torn);
                }
                sender.tabItems().insertAt(static_cast<uint32_t>(args.dropIndex() + position), torn);
                ++position;
                if (source && (*source).tabItems().size() == 0) {
                    closeWindowIfEmpty(*source);
                }
            }
        },
    };

    model->window = Window {
        extendsContentIntoTitleBar = true,
        content = Grid {hAlign.stretch, vAlign.stretch, background = brushes.SolidBackgroundFillColor.Base, model->tabs},
    };
    model->window.setTitleBar(dragRegion);
    model->window.appWindow().setIcon(u"Assets/Tiles/GalleryIcon.ico");

    // The smallest the window can be, in physical pixels: the size in the units of the layout, once the scale is known.
    model->tabs.add_onLoaded([weak](auto&&...) {
        if (auto const self = weak.lock()) {
            auto const scale = self->tabs.xamlRoot().rasterizationScale();
            auto presenter = OverlappedPresenter::create();
            presenter.preferredMinimumWidth(static_cast<int32_t>(500 * scale));
            presenter.preferredMinimumHeight(static_cast<int32_t>(300 * scale));
            self->window.appWindow().setPresenter(presenter);
        }
    });

    allWindows().push_back(model);
    gallery::trackWindow(model->window, model);
    return model;
}

}  // namespace

void gallery::openTabViewWindow() {
    auto const model = makeWindow();
    for (int i = 0; i < 3; ++i) {
        model->tabs.tabItems().append(tab(u"Item " + gallery::numberText(i), u"Page " + gallery::numberText(i)));
    }
    model->tabs.selectedIndex(0);
    model->window.activate();
}
