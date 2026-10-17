#pragma once

// What CompositionWindow::backgroundAsync does with a picture loaded on the way: the load of one
// window's background under way, under a source of its own made under its owner's token, and the
// frame that waits for it and puts the picture up. Templates over what is loaded and how it is
// shown, so that a test hands in its own.
//
// The frame is a detached one, the process's like every other: the exit counts it
// (detached_task::live_frames()) and does not end while it waits, and a load that fails -- a file
// that is not there -- is named on the way out by on_detached_task_failure(), as the failure of
// any detached frame is. The window owns its loads: a background set later makes the load under
// way unneeded, and the window cancels it; the token the load was asked under -- the
// application's, or one made under it -- ends it too.

#include <optional>
#include <utility>

#include "../Operation.h"

namespace wxl::impl {

/// The load of one window's background under way, if there is one: a source of its own, made
/// under the token the load was asked under. One at a time -- the next cancels the one before it.
class background_loads {
public:
    /// The token the next load stands under: a source made under `owner`, once the load before it
    /// is cancelled.
    async::cancellation_token next(async::cancellation_token owner) {
        cancel();
        load_.emplace(std::move(owner));
        return load_->token();
    }

    /// The background is set some other way: the load under way, if any, is not wanted.
    void cancel() noexcept {
        if (!load_) return;
        load_->cancel();
        load_.reset();
    }

private:
    std::optional<async::cancellation_source> load_;
};

/// Waits for `loading`, started already under `stop`, and shows what it loaded through `show` --
/// unless `stop` was cancelled meanwhile: a load whose answer reached the queue before its
/// cancellation still answers, and the background set after it keeps the last word. A wait cut
/// short by the cancellation is the ordinary end of the frame.
template <class Loaded, class Show>
async::detached_task show_when_loaded(async::task<Loaded> loading, async::cancellation_token stop, Show show) {
    Loaded const loaded = co_await std::move(loading);
    if (stop.is_canceled()) co_return;
    show(loaded);
}

}  // namespace wxl::impl
