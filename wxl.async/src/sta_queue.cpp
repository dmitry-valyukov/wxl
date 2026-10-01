module;
#include "pch.h"

#include <DispatcherQueue.h>
#include <roapi.h>
#include <windows.foundation.h>
#include <windows.system.h>
#include <winstring.h>

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {
namespace {

using ABI::Windows::Foundation::AsyncStatus;
using ABI::Windows::Foundation::IAsyncAction;
using ABI::Windows::Foundation::IAsyncActionCompletedHandler;
using ABI::Windows::System::IDispatcherQueue;
using ABI::Windows::System::IDispatcherQueueController;
using ABI::Windows::System::IDispatcherQueueHandler;
using ABI::Windows::System::IDispatcherQueueStatics;

/// The queue of the thread the loop was started on, from start_dispatched() to stop().
IDispatcherQueue* sta_queue = nullptr;

/// Holds the queue this module made itself, where the thread had none.
IDispatcherQueueController* own_controller = nullptr;

/// What the two handlers do with what arrives: deliver it while the loop runs, settle
/// it while the loop is being stopped.
void (*on_drain)() noexcept = nullptr;
void (*on_return)(async_op&) noexcept = nullptr;

/// Turns the message loop until the condition holds: what the queue is posted arrives
/// as a message. A WM_QUIT met on the way is put back for whoever was meant to see it.
void pump_until(auto done) noexcept {
    std::optional<WPARAM> quit;
    MSG message{};

    while (!done()) {
        const BOOL got = ::GetMessageW(&message, nullptr, 0, 0);

        if (got == -1) break;

        if (got == 0) {
            quit = message.wParam;
            continue;
        }

        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    if (quit) ::PostQuitMessage(static_cast<int>(*quit));
}

IDispatcherQueue* queue_of_this_thread() {
    static constexpr wchar_t class_name[] = L"Windows.System.DispatcherQueue";

    HSTRING_HEADER header{};
    HSTRING name = nullptr;
    IDispatcherQueueStatics* statics = nullptr;
    IDispatcherQueue* queue = nullptr;

    // Fails on a thread that has not entered an apartment yet, which is a thread with
    // no queue; making one below enters it.
    if (SUCCEEDED(::WindowsCreateStringReference(class_name,
                                                 static_cast<UINT32>(std::size(class_name) - 1),
                                                 &header, &name)) &&
        SUCCEEDED(::RoGetActivationFactory(name, __uuidof(IDispatcherQueueStatics),
                                           reinterpret_cast<void**>(&statics)))) {
        statics->GetForCurrentThread(&queue);
        statics->Release();
    }

    if (queue) return queue;

    const DispatcherQueueOptions options{sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT,
                                         DQTAT_COM_STA};

    const HRESULT made = ::CreateDispatcherQueueController(options, &own_controller);

    if (FAILED(made)) throw system_exception("CreateDispatcherQueueController", made);

    const HRESULT got = own_controller->get_DispatcherQueue(&queue);

    if (FAILED(got)) {
        own_controller->Release();
        own_controller = nullptr;
        throw system_exception("IDispatcherQueueController::get_DispatcherQueue", got);
    }

    return queue;
}

/// What the queue is handed: the delegate it asks for, written out by hand -- IUnknown
/// and one Invoke() -- so that posting goes through no projection. Agile, since it is
/// posted from one thread and invoked on another.
class queue_handler : public IDispatcherQueueHandler
{
public:
    HRESULT __stdcall QueryInterface(const IID& iid, void** out) noexcept override {
        if (iid == __uuidof(IDispatcherQueueHandler) || iid == __uuidof(IUnknown) ||
            iid == __uuidof(IAgileObject)) {
            *out = static_cast<IDispatcherQueueHandler*>(this);
            AddRef();
            return S_OK;
        }

        *out = nullptr;
        return E_NOINTERFACE;
    }

protected:
    ~queue_handler() = default;
};

/// \return whether the queue took it; it refuses once it is shutting down.
bool post(queue_handler& handler) noexcept {
    boolean taken = false;

    return SUCCEEDED(sta_queue->TryEnqueue(&handler, &taken)) && taken;
}

/// What the worker's handover posts: the drain of the return channel. One for the
/// process and never deleted, so it counts nothing.
class draining_handler final : public queue_handler
{
public:
    ULONG __stdcall AddRef() noexcept override { return 2; }
    ULONG __stdcall Release() noexcept override { return 1; }

    HRESULT __stdcall Invoke() noexcept override {
        on_drain();
        return S_OK;
    }
};

draining_handler drain;

/// Told when the queue this module made has shut down. One for the process, like the
/// drain: the action may keep the pointer for as long as it lives.
class shutdown_watch final : public IAsyncActionCompletedHandler
{
public:
    HRESULT __stdcall QueryInterface(const IID& iid, void** out) noexcept override {
        if (iid == __uuidof(IAsyncActionCompletedHandler) || iid == __uuidof(IUnknown) ||
            iid == __uuidof(IAgileObject)) {
            *out = static_cast<IAsyncActionCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }

        *out = nullptr;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef() noexcept override { return 2; }
    ULONG __stdcall Release() noexcept override { return 1; }

    /// On whichever thread the action chooses; the message wakes the one pumping.
    HRESULT __stdcall Invoke(IAsyncAction*, AsyncStatus) noexcept override {
        done_.store(true, std::memory_order_release);
        ::PostThreadMessageW(thread_, WM_NULL, 0, 0);
        return S_OK;
    }

    inline void arm() noexcept {
        thread_ = ::GetCurrentThreadId();
        done_.store(false, std::memory_order_relaxed);
    }

    inline bool done() const noexcept { return done_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> done_{false};
    DWORD thread_ = 0;
};

shutdown_watch shut_down;

/// Brings one operation back from the thread pool. The queue keeps it past Invoke(),
/// by which time the operation may be gone, so it is an object of its own -- and on
/// the heap rather than in the STA pool, since it is counted from two threads and
/// nothing promises which of them lets go of it last.
class returning_handler final : public queue_handler
{
public:
    explicit returning_handler(async_op& op) noexcept : op_(&op) {}

    ULONG __stdcall AddRef() noexcept override {
        return refs_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG __stdcall Release() noexcept override {
        const ULONG left = refs_.fetch_sub(1, std::memory_order_acq_rel) - 1;

        if (left == 0) delete this;

        return left;
    }

    HRESULT __stdcall Invoke() noexcept override {
        on_return(*op_);
        return S_OK;
    }

    inline async_op& op() const noexcept { return *op_; }

private:
    ~returning_handler() = default;

    async_op* op_;

    /// The one the pool's callback holds, until it has posted.
    std::atomic<ULONG> refs_{1};
};

void __stdcall run_on_pool(PTP_CALLBACK_INSTANCE, void* context) noexcept {
    returning_handler& handler = *static_cast<returning_handler*>(context);

    handler.op().packaged_execute();

    // Refused only by a queue that is shutting down, which is a thread on its way out:
    // nobody is left to take the operation, and it stays where it is.
    post(handler);

    handler.Release();
}

}  // namespace

void sta_loop::start_dispatched(std::string_view worker_name) {
    ensure(!worker_ && "sta_loop: the loop is already running");

    on_drain = []() noexcept { run_pending(); };
    on_return = [](async_op& op) noexcept {
        --outstanding_;
        op.come_back();
    };

    sta_queue = queue_of_this_thread();

    // The shape changes last: a start that failed leaves a loop that was never started,
    // and stop() has nothing to let go of.
    try {
        start_driven([]() noexcept { post(drain); }, worker_name);
    } catch (...) {
        sta_queue->Release();
        sta_queue = nullptr;

        if (own_controller) {
            own_controller->Release();
            own_controller = nullptr;
        }

        throw;
    }

    shape_ = &on_queue;
}

void sta_loop::send_to_pool(async_op& op) {
    returning_handler* const handler = new returning_handler(op);

    ++outstanding_;

    if (!::TrySubmitThreadpoolCallback(run_on_pool, handler, nullptr)) {
        const system_exception refused("TrySubmitThreadpoolCallback");

        --outstanding_;
        handler->Release();
        throw refused;
    }
}

void sta_loop::take_back_dispatched() noexcept {
    on_drain = &settle_pending;
    on_return = [](async_op& op) noexcept {
        --outstanding_;
        op.settle();
    };

    // What is in the channel already may have been posted about before this began, or
    // not at all: taken here, so that the wait below only ever waits for a message
    // still to come.
    settle_pending();

    pump_until([] { return outstanding_ == 0; });

    // Left armed by the last drain, and nothing is coming any more: cleared, so that
    // the next start finds it the way the first did.
    from_worker_.disarm();
}

void sta_loop::let_go_of_queue() noexcept {
    sta_queue->Release();
    sta_queue = nullptr;

    if (own_controller) {
        IAsyncAction* action = nullptr;

        shut_down.arm();

        if (SUCCEEDED(own_controller->ShutdownQueueAsync(&action))) {
            if (SUCCEEDED(action->put_Completed(&shut_down)))
                pump_until([] { return shut_down.done(); });

            action->Release();
        }

        own_controller->Release();
        own_controller = nullptr;
    }

    shape_ = &on_worker;
}

}  // namespace wxl::async
