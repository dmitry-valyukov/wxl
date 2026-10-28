#pragma once

// What CompositionWindow::backgroundAsync does with a picture loaded on the way: the load of one
// window's background under way, under a source of its own made under its owner's token, and the
// task that waits for it and puts the picture up. Templates over what is asked for, what is
// loaded and how it is shown, so that a test hands in its own.
//
// The window owns its loads, and the wait of each is a task kept with it: a background set later
// makes the load under way unneeded, and the window cancels it and drops its wait; the token the
// load was asked under -- the application's, or one made under it -- ends it too. While it waits,
// its body is under way, and has to be over before the exit. A load that fails -- a file that is
// not there -- ends the wait with the failure, kept in the task with the load until the next
// replaces it.
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
class background_load
{
public:
    /// A load as it was asked for: to ask for it again after a replacement of the device.
    struct load_request {
        async::cancellation_token owner;
        Request request;
    };

    /// The token the next load is registered with: a source made under `owner`, once the load
    /// before it is cancelled.
    async::cancellation_token start(async::cancellation_token owner, Request request) {
        cancel();
        load_.emplace(owner, load_request{owner, std::move(request)});
        return load_->source.token();
    }

    /// The wait for the load `load` names -- the token start() gave for it: kept with the load, so
    /// that the next load, or cancel(), stops it as well. The wait has run as far as its first
    /// suspension by now, and what it reached on the way may have replaced or cancelled its load
    /// already -- a replacement of the device asks for the picture again -- which is no broken
    /// rule: that wait is nobody's any more, its load cancelled, and it goes here.
    void hold(async::cancellation_token const& load, async::task<> wait) noexcept {
        if (load_ && async::detail::cancellation_state_of(load_->source.token()) ==
                         async::detail::cancellation_state_of(load))
            load_->wait = std::move(wait);
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
    std::optional<load_request> last() const {
        return load_ ? std::optional<load_request>{load_->what} : std::nullopt;
    }

private:
    struct load {
        load(async::cancellation_token const& owner, load_request what)
            : source(owner), what(std::move(what)) {}

        async::cancellation_source source;
        load_request what;

        // Last: it goes first, before the source it is registered with.
        async::task<> wait;
    };

    std::optional<load> load_;
};

/// Waits for `loading`, started already under `token`, and shows what it loaded through `show` --
/// unless `token` was cancelled meanwhile: a load whose answer reached the queue before its
/// cancellation still answers, and the background set after it keeps the last word. A wait
/// aborted by the cancellation ends with it; a load the lost device aborted
/// (DrawingSurface::DeviceLost) ends the wait quietly, and the owner asks for it again once the
/// new device stands.
template <class Loaded, class Show>
async::task<> show_when_loaded(async::task<Loaded> loading, async::cancellation_token token,
                               Show show) {
    std::optional<Loaded> loaded;
    try {
        loaded.emplace(co_await std::move(loading));
    } catch (DrawingSurface::DeviceLost const&) {
        co_return;
    }
    if (token.is_canceled()) co_return;
    show(*loaded);
}

}  // namespace wxl::impl
