module;

#include "abi.h"

export module wxl.async:sta_loop;

import :async_op;
import :awaitable;
import :spsc_channel;
import :thread_group;
import :threaded_component;
import wxl.core;
import std;

export namespace wxl::async {

/// How the STA side of a loop learns that an operation has come back.
///
/// Two shapes, and an application picks one when it starts the loop. A thread
/// with nothing else to do sleeps on an event and is woken by it -- that is a
/// test, a tool, a console program. A thread with a message loop of its own
/// cannot sleep here at all: it has a window to keep answering. That one hands
/// over a callback instead, and the worker calls it -- on the worker's thread,
/// so all the callback may do is arrange, by whatever its dispatcher offers,
/// for run_pending() to be called back on the STA thread.
class sta_signal
{
public:
    using wake_t = core::function<void() noexcept>;

    /// The sleeping shape. Auto-reset: a wakeup means "look again".
    sta_signal() = default;

    /// The worker's call, at the end of every handover.
    inline void set() {
        if (wake_)
            (*wake_)();
        else
            event_.set();
    }

    /// The STA side's call, and only in the sleeping shape -- a loop with a
    /// dispatcher behind it has no business blocking the thread that owns it.
    inline void wait() {
        ensure(!wake_ && "sta_loop: a loop driven by a callback never sleeps");

        event_.wait();
    }

    /// Chosen before the worker starts, and never again: from then on the
    /// callback is read from the worker's thread.
    template <core::invocable<void() noexcept> F>
    void wake_with(F&& wake) {
        wake_ = std::forward<F>(wake);
    }

    /// Given back with the run that installed it.
    ///
    /// Not tidiness: the callback's body is in the STA pool, and this signal
    /// lives in the loop's static state, which the pool does not outlive. So
    /// it is handed back while the pool is still standing -- the same thing
    /// wWinMain does with its teardown handler, and for the same reason.
    inline void forget_wake() noexcept { wake_.reset(); }

    /// Whether a callback was given. A sleeping reader is signalled only when
    /// it is really asleep, and a driven one only when it has declared, at the
    /// end of a run_pending(), that it is not looking any more: the channel's
    /// trigger says which, and this says whose declaration it is.
    inline bool driven() const noexcept { return static_cast<bool>(wake_); }

private:
    core::nullable<wake_t> wake_;
    core::hevent event_{false};
};

/// The two threads an asynchronous operation lives between, and the loop that
/// drives them.
///
/// One side is the STA thread -- the thread that writes the coroutines, and the
/// only thread they ever run on. The other is a single worker, where everything
/// that blocks happens. Between them, two single-producer/single-consumer
/// channels: operations out, finished operations back. Nothing is shared but the
/// operations themselves, and each of those belongs to exactly one of the two
/// threads at any moment.
///
/// The worker is asleep whenever there is nothing to do, and is woken by the
/// channel itself -- there is no polling anywhere in here. The STA side either
/// sleeps the same way, in run_one(), or is driven by its own dispatcher; that
/// is the difference between start() and start_driven().
///
/// **There is nothing here to build.** The loop is one and it is the process's:
/// the state is static, the entry points are static, and an instance would be a
/// second name for the only thing there is. Which is why nobody is handed a loop
/// either -- `async_file::open_read(path)` names no loop, because an argument
/// for it would be a parameter every caller had to carry down to say the only
/// thing it could say. What the infrastructure does is start() it and stop() it:
/// the application at startup, a test binary in its one global fixture.
///
/// **Once, and once only.** A stopped loop stays stopped: the channels live as
/// long as the process, `spsc_queue` takes a single reader for the whole of its
/// life, and the `turnstile` that guards sending latches shut for good. There is
/// nothing to restart, which is why a test binary starts the loop once for all
/// of its tests rather than once per test.
class sta_loop
{
    /// The worker sleeps on an event of the channel's own making; the STA side
    /// sleeps or is called back, and that is what `sta_signal` decides. Either
    /// way the arm/disarm protocol inside the channel makes sure
    /// a wakeup is never lost.
    using to_worker_t = spsc_channel<async_op*, 256>;
    using from_worker_t = spsc_channel<async_op*, 256, sta_signal>;

    /// The worker thread. It has no state of its own beyond the channels of the
    /// run: what to do arrives as an operation, and where to put the answer is
    /// the same for all of them.
    class worker : public threaded_component
    {
    public:
        inline worker(std::string_view name, thread_group* group)
            : threaded_component(name, group) {}

        inline ~worker() override { dispose(); }

    protected:
        inline void run() override {
            // Built here rather than beside the channels: the reader belongs to
            // the thread that reads, and this is it.
            to_worker_t::reader reader(to_worker_);

            async_op* op = nullptr;

            // closed() is asked only when a receive came back empty, never once
            // per operation: close() forces a wakeup, and that wakeup comes
            // back through receive() with nothing in its hands, so the empty
            // path sees every close there will ever be.
            for (;;) {
                if (reader.receive(op)) {
                    execute(op);
                    continue;
                }

                if (to_worker_.closed()) break;
            }

            // A latched close is the promise that nothing more can be sent, so what
            // is left in the queue is everything that will ever be there -- and it
            // was accepted, so it gets done.
            while (reader.read(op)) execute(op);
        }

        /// Closing the channel is what ends run(): it makes the loop above fall
        /// through, and the forced signal wakes the thread if it is asleep in
        /// receive() at that moment.
        inline void on_stopping() override {
            threaded_component::on_stopping();

            to_worker_.close();
            to_worker_.signal(true);
        }

    private:
        inline static void execute(async_op* op) {
            if (!op->packaged_execute()) return;

            // send() signals only when the STA side has said it is not looking:
            // a sleeper says so before it sleeps, and a driven loop says so at
            // the end of every run_pending(). Either way one handover per burst
            // reaches the kernel or the dispatcher, and the rest are found there.
            from_worker_.send(op);
        }
    };

    static inline to_worker_t to_worker_{false};
    static inline from_worker_t from_worker_;

    /// The STA side of the return channel. Its owner is the STA thread, and
    /// there is one of it, which is what the channel asks.
    static inline from_worker_t::reader from_worker_reader_{from_worker_};

    /// The worker is a managed thread rather than a detached one, and that is
    /// not decoration: a detached thread is counted by
    /// `thread_group::detached_thread_count()` and waited for by
    /// `join_detached_threads()`, and a loop that lives as long as the process
    /// would make both of those answer "never". Its own group answers instead.
    static inline thread_group_ptr threads_;

    /// Empty until start(), and empty again after stop(). It is the one piece
    /// that cannot simply sit here beside the channels: a component is named
    /// when it is built, and what the worker thread is called is the
    /// application's to choose.
    static inline std::optional<worker> worker_;

public:
    /// Static from top to bottom, and therefore never made.
    sta_loop() = delete;

    /// Starts the loop for a thread that has nothing else to do, and will
    /// therefore sleep in run_one() until an answer comes back.
    ///
    /// \param worker_name what the worker thread is called in a debugger and in
    ///        a log.
    inline static void start(std::string_view worker_name = "sta_loop worker") {
        ensure(!worker_ && "sta_loop: the loop is already running");

        threads_ = thread_group::create();
        worker_.emplace(worker_name, threads_.get());
        worker_->start_async().get();
    }

    /// Starts the loop for a thread that cannot sleep in it, because it has a
    /// message loop of its own.
    ///
    /// \param wake called on the *worker's* thread, once for every operation
    ///        that comes back, and all it may do is ask the STA thread to call
    ///        run_pending(). In a WinUI application that is one
    ///        `DispatcherQueue.TryEnqueue`.
    /// \param worker_name what the worker thread is called in a debugger and in
    ///        a log.
    ///
    /// A separate entry point rather than a wake_with() to be called first: the
    /// callback has to be in place before the worker can hand anything over, and
    /// an order between two calls is a rule that can be got wrong, while an
    /// argument cannot.
    template <core::invocable<void() noexcept> F>
    static void start_driven(F&& wake, std::string_view worker_name = "sta_loop worker") {
        ensure(!worker_ && "sta_loop: the loop is already running");

        from_worker_.wakeup().wake_with(std::forward<F>(wake));

        // Nobody is looking at the return channel until the first callback, and
        // the channel has to be told so: it is what makes the first handover the
        // one that calls back, and every later one until run_pending() has been
        // round find the trigger clear and stay quiet. The same declaration a
        // sleeper makes before it sleeps, made here once, for the shape that
        // never sleeps.
        from_worker_.arm();

        threads_ = thread_group::create();
        worker_.emplace(worker_name, threads_.get());
        worker_->start_async().get();
    }

    /// Stops the worker and waits for it to finish what it had accepted.
    ///
    /// What it does not do is finish the coroutines. An operation still on its
    /// way back when the worker leaves is never taken out of the return channel,
    /// so the coroutine waiting for it never resumes; its frame is destroyed by
    /// its `task`, suspended where it stood. That is the honest end for a loop
    /// that is being shut down -- there is nothing left to resume it *onto* --
    /// but a caller that wants its coroutines finished runs them to their end
    /// before stopping.
    ///
    /// Doing nothing when there is no run is the point rather than an
    /// indulgence: a teardown path has no business knowing how far a startup
    /// path got before it threw.
    inline static void stop() {
        // TODO: здесь надо продолжать разгребать корутины, которые завершаются.
        // Это ошибка логики.
        if (!worker_) return;

        if (worker_->was_started()) worker_->stop_async().get();

        worker_.reset();
        threads_.reset();

        // The wake-up goes with the run it belonged to. The channel holding it
        // is static and the callback's body is in the STA pool, so this is
        // where it is given back -- while there is still a pool to give it to.
        from_worker_.wakeup().forget_wake();
    }

    /// Whether there is a run in progress -- for the infrastructure that starts
    /// and stops it.
    inline static bool running() noexcept { return worker_.has_value(); }

    /// Hands an operation to the worker. The STA thread's call.
    ///
    /// After stop() this is an error of the program and fails as one: the
    /// thread that stopped the loop is the thread sending, there is no worker
    /// left to carry the operation and nothing to carry it back with, and a
    /// coroutine that reaches this point was one the caller had promised to
    /// finish before stopping.
    inline static void enqueue(core::not_null<async_op> op) { to_worker_.send(op.get()); }

    /// Starts an operation whose body is a lambda and returns what the coroutine
    /// awaits.
    ///
    /// The operation is sent before the awaitable exists -- and so, in principle,
    /// before there is a coroutine to name. That is safe for one reason only: the
    /// thread that would resume the coroutine is the STA thread, and it is here,
    /// inside this call, and cannot be in run_one() at the same time.
    template <class Fn>
    [[nodiscard]] static awaitable<std::invoke_result_t<std::decay_t<Fn>&>> async_call(Fn&& fn) {
        using result_t = std::invoke_result_t<std::decay_t<Fn>&>;

        // Held as the base on the way in: async_run() deduces its result type
        // from the pointer, and deduction does not see through a derived class.
        std::unique_ptr<async_op_t<result_t>> op(new async_op_f<std::decay_t<Fn>>(
            std::forward<Fn>(fn)));

        return async_run(std::move(op));
    }

    /// The same for an operation written out as a class of its own: whoever
    /// needs more than a lambda can do -- state that survives being executed
    /// twice, a body that answers `false` and waits for something to fire --
    /// derives from `async_op_t<R>` and starts it here.
    ///
    /// Ownership moves: from this call on, the operation belongs to the
    /// awaitable, which lives in the frame of the coroutine that awaits it.
    template <class R>
    [[nodiscard]] static awaitable<R> async_run(std::unique_ptr<async_op_t<R>> op) {
        enqueue(core::as_not_null<async_op>(op.get()));

        return awaitable<R>(std::move(op));
    }

    /// Takes one finished operation and gives control back to the coroutine that
    /// was waiting for it. Sleeps while there is nothing to take.
    ///
    /// \return `false` on a wakeup with nothing behind it -- the caller loops on a
    ///         condition of its own, as it does with the channel underneath.
    inline static bool run_one() {
        async_op* op = nullptr;

        if (!from_worker_reader_.receive(op)) return false;

        // The op is gone by the time this returns: the coroutine resumes inside its
        // co_await, and the awaitable that owns the op dies with that expression.
        op->resume();

        return true;
    }

    /// Runs until the caller's own condition says the work is done -- a task
    /// having finished, usually.
    inline static void run_until(auto done) {
        while (!done()) run_one();
    }

    /// Resumes every coroutine whose operation has come back, and returns
    /// without waiting for any that have not.
    ///
    /// This is what a thread with a message loop of its own calls, from the
    /// callback given to start_driven(): it has a window to keep answering and
    /// cannot sit in run_one(). More may have come back than the one handover
    /// that prompted the call, and that is the point -- a burst of finished
    /// operations costs one trip through the dispatcher, not one each.
    ///
    /// One trip, and not one *callback* each either. In the driven shape this
    /// runs the channel's own waiting protocol, with the sleep taken out: it
    /// takes the trigger on the way in -- the callback that brought it here is
    /// consumed, and from now on it is the one looking -- and sets it on the
    /// way out, having found the channel empty, so that the next handover is
    /// the one that calls back and the ones after it find the trigger clear.
    /// The re-check between setting the trigger and leaving is the same one a
    /// sleeper makes before it sleeps, and for the same reason: an operation
    /// that came back between the last read and the declaration must be taken
    /// here, since its handover found nobody to tell. A callback with nothing
    /// behind it -- the worker took the trigger a moment before this side did
    /// -- is the ordinary spurious wakeup, and costs one empty look.
    ///
    /// In the sleeping shape none of that happens: run_one() runs the protocol
    /// around its own sleep, and this only takes what is there.
    ///
    /// \return how many coroutines were resumed.
    inline static std::size_t run_pending() {
        std::size_t resumed = 0;
        const bool driven = from_worker_.wakeup().driven();

        if (driven) {
            from_worker_.disarm();
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }

        for (;;) {
            for (async_op* op = nullptr; from_worker_reader_.read(op); ++resumed) op->resume();

            if (!driven) return resumed;

            from_worker_.arm();
            std::atomic_thread_fence(std::memory_order_seq_cst);

            async_op* op = nullptr;

            if (!from_worker_reader_.read(op)) return resumed;

            from_worker_.disarm();
            op->resume();
            ++resumed;
        }
    }
};

}  // export namespace wxl::async
