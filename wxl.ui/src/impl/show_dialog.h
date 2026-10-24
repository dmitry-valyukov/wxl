#pragma once

// What wxl::showDialog does, over any dialog whose showAsync takes a token: the library's
// ContentDialog wrapper (ShowDialog.cpp), and a fake one in a test.
//
// Two halves, and the line between them is the point. The call shows the dialog: ShowAsync
// either starts its operation or refuses at once -- XAML refuses a second dialog over an island
// while one is up, and a dialog with no XamlRoot to show over -- and a refusal is an exception
// out of the call itself, for its caller to catch, with nothing left running behind it. What is
// left after a dialog that did come up is the wait for its answer: a task the caller keeps in a
// field of whoever closes the dialog. Its frame is a body under way, which has to be over before
// the exit.
//
// A body that has to end before the exit cannot wait for the reader, who may never answer. So the
// operation is started under the caller's token -- the form with a token of showAsync
// (impl/operation.h) -- and the token is the owner's way to close the dialog: cancelled, it posts
// a turn to the thread's queue, and that turn asks the dialog to close -- without Closing, which
// could refuse -- and ends the wait with the cancellation, whatever the platform does after. Dropping the task
// closes it as well: the operation still out is asked to stop as its producer goes. A token with
// no source is one nobody can cancel; a dialog under it would be the reader's alone to close, and
// the call stops on it rather than leave a frame alive at the exit.
//
// The wait does not read the answer: the dialog's events carry it (PrimaryButtonClick, Closed
// and the rest), the way every answer in wxl comes.

#include <utility>

#include "../Operation.h"

namespace wxl::impl {

/// The wait for the answer: holds the operation already started until it answers or is
/// aborted. The answer is dropped -- the dialog's events have given it; the cancellation or a
/// failure is the end the task keeps, for its owner to read or not.
template <class R>
async::task<> await_answer(async::task<R> answer) {
    co_await std::move(answer);
}

/// Shows `dialog` under `token` and returns the wait for its answer. A refusal is thrown from
/// here, before any frame is made; under a token cancelled already the dialog is not shown,
/// and the wait ends at once.
template <class Dialog>
[[nodiscard]] async::task<> show_dialog(Dialog const& dialog, async::cancellation_token token) {
    if (!async::detail::cancellation_state_of(token))
        core::abort("wxl: showDialog under a token nobody can cancel -- a dialog only the reader "
                    "can close would keep its wait alive at the exit");

    return await_answer(dialog.showAsync(std::move(token)));
}

}  // namespace wxl::impl
