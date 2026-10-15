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
using ABI::Windows::Foundation::IAsyncInfo;
using ABI::Windows::System::IDispatcherQueue;
using ABI::Windows::System::IDispatcherQueueController;
using ABI::Windows::System::IDispatcherQueueHandler;
using ABI::Windows::System::IDispatcherQueueStatics;

/// The queue of the thread the loop was started on, from start_dispatched() to stop().
IDispatcherQueue* sta_queue = nullptr;

/// Holds the queue this module made itself, where the thread had none.
IDispatcherQueueController* own_controller = nullptr;

/// Called whenever the foreground thread is about to go idle, in whichever message loop
/// it runs: a modal one opened by a continuation included. Installed by
/// start_dispatched(), removed by stop().
HHOOK idle_hook = nullptr;

/// What the handler bringing an operation back from the thread pool does with it: takes it
/// out of the loop's count and delivers it. Set by start_dispatched(), which is the loop's
/// own and may touch the count; the handler is not.
void (*on_return)(async_op&) noexcept = nullptr;

/// Turns the message loop until the condition holds: what the queue is posted arrives
/// as a message. This is the thread's only loop while it turns -- at the end of a process
/// the wait for the last frames and stop() both come after the application's own loop has
/// ended -- so nothing but messages is expected: a failed GetMessageW, and a WM_QUIT
/// asking the thread to leave while it still waits for what it owes, are broken rules, not
/// cases, and the process stops on them rather than going on as if the wait were over.
void pump_until(auto done) noexcept {
    MSG message{};

    while (!done()) {
        const BOOL got = ::GetMessageW(&message, nullptr, 0, 0);

        ensure(got != -1 && "sta_loop: GetMessageW failed while the loop waits");
        ensure(got != 0 && "sta_loop: WM_QUIT reached the loop while it waits for what it owes");

        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
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
        sta_loop::run_pending();
        return S_OK;
    }
};

draining_handler drain;

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
    // nobody is left to take the operation, and it stays out, for stop() to find.
    post(handler);

    handler.Release();
}

}  // namespace

void sta_loop::start_dispatched(std::string_view worker_name) {
    ensure(!worker_ && "sta_loop: the loop is already running");

    on_return = [](async_op& op) noexcept {
        --outstanding_;
        op.come_back();
    };

    sta_queue = queue_of_this_thread();

    // Nothing is drained from inside the hook: the idle callback must not run code that
    // pumps or calls COM, which a continuation would. It only sees to it that the drain
    // is posted, and the post arrives as a message into whatever loop is running.
    idle_hook = ::SetWindowsHookExW(
        WH_FOREGROUNDIDLE,
        [](int code, WPARAM wparam, LPARAM lparam) -> LRESULT {
            if (code == HC_ACTION) post_if_pending();

            return ::CallNextHookEx(nullptr, code, wparam, lparam);
        },
        nullptr, ::GetCurrentThreadId());

    // The shape changes last: a start that failed leaves a loop that was never started,
    // and stop() has nothing to let go of.
    try {
        start_driven([]() noexcept { post(drain); }, worker_name);
    } catch (...) {
        if (idle_hook) {
            ::UnhookWindowsHookEx(idle_hook);
            idle_hook = nullptr;
        }

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

void sta_loop::post_if_pending() noexcept {
    // The same move as pay_owed_callback(), on a trigger that may be armed already: the
    // thread is idle, so nobody is looking, and what lies in the channel is posted about.
    from_worker_.rearm();
    std::atomic_thread_fence(std::memory_order_seq_cst);

    auto at = from_worker_reader_.look_ahead();

    if (from_worker_reader_.peek(at) && from_worker_.disarm()) post(drain);
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

void sta_loop::pump_messages_until(bool (*done)(void*), void* condition) noexcept {
    pump_until([done, condition] { return done(condition); });
}

void sta_loop::let_go_of_queue() noexcept {
    if (idle_hook) {
        ::UnhookWindowsHookEx(idle_hook);
        idle_hook = nullptr;
    }

    sta_queue->Release();
    sta_queue = nullptr;

    if (own_controller) {
        // A queue made on this thread is shut down by this thread: it drains what it still
        // holds through the message loop, and the action completes from inside it. So the
        // wait is the loop turning until the action's own status says it is over, read
        // between messages -- the action keeps that state itself, and nobody is told.
        IAsyncAction* action = nullptr;
        const HRESULT started = own_controller->ShutdownQueueAsync(&action);
        ensure(SUCCEEDED(started) && "sta_loop: ShutdownQueueAsync refused");

        IAsyncInfo* info = nullptr;
        const HRESULT asked =
            action->QueryInterface(__uuidof(IAsyncInfo), reinterpret_cast<void**>(&info));
        ensure(SUCCEEDED(asked) && "sta_loop: the shutdown action has no IAsyncInfo");

        const auto status = [info] {
            AsyncStatus value{};
            const HRESULT read = info->get_Status(&value);
            ensure(SUCCEEDED(read) && "sta_loop: IAsyncInfo::get_Status failed");
            return value;
        };

        pump_until([&status] { return status() != AsyncStatus::Started; });
        ensure(status() == AsyncStatus::Completed &&
               "sta_loop: the dispatcher queue did not shut down");

        info->Release();
        action->Release();

        own_controller->Release();
        own_controller = nullptr;
    }

    shape_ = &on_worker;
}

}  // namespace wxl::async
