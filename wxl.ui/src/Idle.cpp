#include "platform.h"

#include <winrt/Microsoft.UI.Dispatching.h>

#include "Idle.h"

namespace wxl {

namespace dispatching = winrt::Microsoft::UI::Dispatching;

Idle::~Idle() {
    if (self_) {
        *self_ = nullptr;
    }
    if (waiting()) {
        disarm();
    }
}

bool Idle::await_suspend(std::coroutine_handle<> waiter) {
    self_ = std::make_shared<Idle*>(this);
    arm(waiter);

    // Low: everything the queue holds at normal priority -- input among it --
    // runs before the coroutine does.
    auto const queue = dispatching::DispatcherQueue::GetForCurrentThread();
    if (queue && queue.TryEnqueue(dispatching::DispatcherQueuePriority::Low, [self = self_] {
            if (Idle* const wait = *self) {
                wait->deliver();
            }
        })) {
        return true;
    }

    disarm();
    refused_ = true;
    return false;
}

}  // namespace wxl
