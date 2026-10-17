#pragma once

// What a WinRT asynchronous operation -- IAsyncOperation<T>, IAsyncAction, with progress or
// without -- is on this side: async::task<R>, the one type wxl awaits, with R the result in wxl's
// terms (a text, a wrapper, a number) and never the projection's own type.
//
//     hstring text = co_await package.getTextAsync();
//
// The task answers on the thread that made the call: with the result, with
// async::operation_canceled_exception for an operation that was cancelled, and with the
// platform's failure for one that failed -- the same exception any other call of a wrapper throws.
// Letting the task go before the answer cancels the operation. impl/operation.h makes it.
//
// Every such method has its form with a token beside it, the token taken last, as wxl.async's
// own operations take one:
//
//     if (co_await dialog.showAsync(stop) != ContentDialogResult::Primary) co_return;
//
// Cancelled while the operation is out, the token posts a turn to the calling thread's queue, and
// that turn asks the platform to stop it -- a ContentDialog closes, without its Closing, which
// could refuse -- and ends the co_await with operation_canceled_exception, whether or not the
// platform has answered by then; an answer that got there first is the answer. Under a token
// cancelled already the call is not made at all.
//
// A header of its own, for the import: a generated header that declares such a call includes
// this one, so wxl.async reaches the headers of those groups and no others -- after core.h, which
// has seen every standard header first.

#include "core.h"

import wxl.async;
