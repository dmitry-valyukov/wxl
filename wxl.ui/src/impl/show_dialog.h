#pragma once

// What wxl::showDialog does, over any dialog whose showAsync takes a token: the library's
// ContentDialog wrapper (ShowDialog.cpp), and a fake one in a test.
//
// Two halves, and the line between them is the point. The call shows the dialog: ShowAsync
// either starts its operation or refuses at once -- XAML refuses a second dialog over an island
// while one is up, and a dialog with no XamlRoot to show over -- and a refusal is an exception
// out of the call itself, for its caller to catch, with nothing left running behind it. What is
// left after a dialog that did come up is the wait for its answer, which returns to nobody: a
// detached frame, so that the process counts it at exit (detached_task::live_frames()) and does
// not end while it is alive, as it does not end while any detached coroutine is.
//
// A frame the exit waits for has to end, and the reader may never answer. So the operation is
// started under the caller's token -- the form with a token of showAsync (impl/operation.h) --
// and the token is the owner's way to close the dialog: cancelled, it posts a turn to the
// thread's queue, and that turn asks the dialog to close -- without Closing, which could refuse --
// and ends the frame with the cancellation, whatever the platform does after. A token with no
// source is one nobody can cancel; a dialog under it would be the reader's alone to close, and
// the call stops on it rather than leave the exit waiting.
//
// The frame does not read the answer: the dialog's events carry it (PrimaryButtonClick, Closed
// and the rest), the way every answer in wxl comes.

#include <utility>

#include "../Operation.h"

namespace wxl::impl {

/// The wait for the answer: holds the operation already started until it answers or is cut
/// short. The answer is dropped -- the dialog's events have given it; the cancellation is the
/// ordinary end of a detached frame, and a failure stops the process through
/// on_detached_task_failure(), as any failure of a detached frame does.
template <class R>
async::detached_task await_answer(async::task<R> answer) {
    co_await std::move(answer);
}

/// Shows `dialog` under `stop` and leaves the wait for its answer running. A refusal is thrown
/// from here, before any frame is made; under a token cancelled already the dialog is not shown,
/// and the frame ends at once.
template <class Dialog>
void show_under(Dialog const& dialog, async::cancellation_token stop) {
    if (!async::detail::state_of(stop))
        core::abort("wxl: showDialog under a token nobody can cancel -- the exit would wait for a "
                    "dialog only the reader can close");

    await_answer(dialog.showAsync(std::move(stop)));
}

}  // namespace wxl::impl
