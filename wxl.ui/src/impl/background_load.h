#pragma once

// What CompositionWindow::backgroundAsync does with a picture loaded on the way: the load of one
// window's background under way, under a source of its own made under its owner's token, and the
// frame that waits for it and puts the picture up. Templates over what is asked for, what is
// loaded and how it is shown, so that a test hands in its own.
//
// The frame is a detached one, the process's like every other: the exit counts it
// (detached_task::live_frames()) and does not end while it waits, and a load that fails -- a file
// that is not there -- is named on the way out by on_detached_task_failure(), as the failure of
// any detached frame is. The window owns its loads: a background set later makes the load under
// way unneeded, and the window cancels it; the token the load was asked under -- the
// application's, or one made under it -- ends it too.
//
// A picture loaded is on the rendering device, and goes with it: the window keeps what it asked
// for and the token it was asked under until the background is set some other way, and loads it
// again when its RenderingDeviceWatch says the device was replaced.

#include <optional>
#include <utility>

#include "../DrawingSurface.h"
#include "../Operation.h"

namespace wxl::impl {

/// The load of one window's background, under way or done, until the background is set some
/// other way: a source of its own, made under the token the load was asked under, and what was
/// asked for. One at a time -- the next cancels the one before it.
template <class Request>
class background_loads {
public:
    /// A load as it was asked for: to ask for it again after a replacement of the device.
    struct asked {
        async::cancellation_token owner;
        Request request;
    };

    /// The token the next load stands under: a source made under `owner`, once the load before it
    /// is cancelled.
    async::cancellation_token next(async::cancellation_token owner, Request request) {
        cancel();
        load_.emplace(owner, asked{owner, std::move(request)});
        return load_->source.token();
    }

    /// The background is set some other way: the load under way, if any, is not wanted, and what
    /// it was asked for is forgotten.
    void cancel() noexcept {
        if (!load_) return;
        load_->source.cancel();
        load_.reset();
    }

    /// The last load asked for, if the background has not been set some other way since; a copy,
    /// since asking for it again replaces it.
    std::optional<asked> last() const { return load_ ? std::optional<asked>{load_->what} : std::nullopt; }

private:
    struct load {
        load(async::cancellation_token const& owner, asked what) : source(owner), what(std::move(what)) {}

        async::cancellation_source source;
        asked what;
    };

    std::optional<load> load_;
};

/// Waits for `loading`, started already under `stop`, and shows what it loaded through `show` --
/// unless `stop` was cancelled meanwhile: a load whose answer reached the queue before its
/// cancellation still answers, and the background set after it keeps the last word. A wait cut
/// short by the cancellation is the ordinary end of the frame; so is a load the lost device cut
/// short (DrawingSurface::DeviceLost), which the owner asks for again once the new device stands.
template <class Loaded, class Show>
async::detached_task show_when_loaded(async::task<Loaded> loading, async::cancellation_token stop, Show show) {
    std::optional<Loaded> loaded;
    try {
        loaded.emplace(co_await std::move(loading));
    } catch (DrawingSurface::DeviceLost const&) {
        co_return;
    }
    if (stop.is_canceled()) co_return;
    show(*loaded);
}

}  // namespace wxl::impl
