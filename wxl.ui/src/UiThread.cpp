// The agile half of DispatcherQueue, kept where any thread may reach it.
//
// The projection and the Windows headers come first, and with them every
// standard header they need: wxl's own headers carry the wxl.core import, and
// a standard header included after that import is one the compiler has
// already seen through the std module.
#include <winrt/Microsoft.UI.Dispatching.h>

#include <unknwn.h>

#include "Object.impl.h"
#include "UiThread.h"
#include "generated/Microsoft.UI.Dispatching.impl.h"

namespace wxl {
namespace {

using queue_t = winrt::Microsoft::UI::Dispatching::DispatcherQueue;
using handler_t = winrt::Microsoft::UI::Dispatching::DispatcherQueueHandler;

::IUnknown* as_unknown(::IInspectable* pointer) noexcept {
    return reinterpret_cast<::IUnknown*>(pointer);
}

}  // namespace

UiThread::UiThread(DispatcherQueue const& queue) {
    // A copy of the queue, detached: from here on this class owns a plain COM
    // reference and no wxl wrapper is involved -- which is the whole point,
    // since a wrapper is what may not cross threads.
    queue_t native = Object::Impl::as<queue_t>(queue);
    queue_ = static_cast<::IInspectable*>(winrt::detach_abi(native));
}

UiThread::UiThread(UiThread const& other) noexcept : queue_(other.queue_) {
    if (queue_) {
        as_unknown(queue_)->AddRef();
    }
}

UiThread::UiThread(UiThread&& other) noexcept : queue_(other.queue_) {
    other.queue_ = nullptr;
}

UiThread& UiThread::operator=(UiThread const& other) noexcept {
    if (this != &other) {
        UiThread copy{other};
        std::swap(queue_, copy.queue_);
    }
    return *this;
}

UiThread& UiThread::operator=(UiThread&& other) noexcept {
    std::swap(queue_, other.queue_);
    return *this;
}

UiThread::~UiThread() {
    if (queue_) {
        as_unknown(queue_)->Release();
    }
}

bool UiThread::post(std::function<void()> work) const {
    if (!queue_ || !work) {
        return false;
    }

    // An AddRef/Release pair around the call, which is a pair of interlocked
    // increments and the price of not having to reason about what happens if
    // TryEnqueue throws.
    queue_t native{nullptr};
    winrt::copy_from_abi(native, queue_);
    return native.TryEnqueue(handler_t{std::move(work)});
}

}  // namespace wxl
