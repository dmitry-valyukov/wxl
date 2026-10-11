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
// A header of its own, for the import: a generated header that declares such a call includes
// this one, so wxl.async reaches the headers of those groups and no others -- after core.h, which
// has seen every standard header first.

#include "core.h"

import wxl.async;
