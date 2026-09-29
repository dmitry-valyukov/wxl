module;
#include "pch.h"

#include <DispatcherQueue.h>
#include <roapi.h>
#include <windows.system.h>
#include <winstring.h>

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {
namespace {

using ABI::Windows::System::IDispatcherQueue;
using ABI::Windows::System::IDispatcherQueueController;
using ABI::Windows::System::IDispatcherQueueHandler;
using ABI::Windows::System::IDispatcherQueueStatics;

/// The queue of the thread the loop was started on. Kept for as long as the process
/// lives: an operation on the pool may be on its way to it after the loop has stopped.
IDispatcherQueue* sta_queue = nullptr;

/// Holds the queue this module made itself, where the thread had none.
IDispatcherQueueController* own_controller = nullptr;

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

    if (FAILED(got)) throw system_exception("IDispatcherQueueController::get_DispatcherQueue", got);

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
        op_->come_back();
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

    sta_queue = queue_of_this_thread();
    send_orphan_ = &send_to_pool;

    start_driven([]() noexcept { post(drain); }, worker_name);
}

void sta_loop::send_to_pool(async_op& op) {
    returning_handler* const handler = new returning_handler(op);

    if (!::TrySubmitThreadpoolCallback(run_on_pool, handler, nullptr)) {
        const system_exception refused("TrySubmitThreadpoolCallback");

        handler->Release();
        throw refused;
    }
}

}  // namespace wxl::async
