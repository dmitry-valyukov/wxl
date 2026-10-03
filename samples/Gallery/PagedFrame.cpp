#include "PagedFrame.h"

using namespace wxl;

namespace gallery {

PagedFrame::PagedFrame(Frame frame) : state_(std::make_shared<State>()) {
    state_->frame = std::move(frame);
}

Page PagedFrame::forward(Builder build) const {
    auto const page = navigatePage(state_->frame);
    page.content(build());
    state_->stack.push_back(std::move(build));
    return page;
}

Page PagedFrame::forward(Builder build, NavigationTransitionInfo const& info) const {
    auto const page = navigatePage(state_->frame, info);
    page.content(build());
    state_->stack.push_back(std::move(build));
    return page;
}

Page PagedFrame::back() const {
    if (state_->stack.size() < 2) {
        return state_->frame.content().try_as<Page>();
    }
    state_->stack.pop_back();
    state_->frame.goBack();
    auto const page = state_->frame.content().try_as<Page>();
    page.content(state_->stack.back()());
    return page;
}

int PagedFrame::depth() const {
    return state_->frame.backStackDepth();
}

}  // namespace gallery
