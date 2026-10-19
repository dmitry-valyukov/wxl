module;

#include "abi.h"

export module wxl.async:sta_loop;

import :async_op;
import :cancellation;
import :coroutine_checks;
import :overlapped_op;
import :io_port;
import :spsc_channel;
import :thread_group;
import :task;
import :threaded_component;
import wxl.core;
import std;

export namespace wxl::async {

/// How the STA side of a loop learns that an operation has come back.
///
/// Two modes, and an application picks one when it starts the loop. A thread
/// with nothing else to do sleeps on an event and is woken by it -- that is a
/// test, a tool, a console program. A thread with a message loop of its own
/// cannot sleep here at all: it has a window to keep answering. That one hands
/// over a callback instead, and the worker calls it -- on the worker's thread,
/// so all the callback may do is arrange, by whatever its dispatcher offers,
/// for run_pending() to be called back on the STA thread.
///
/// And one moment when even a driven thread has to sleep here: a task giving up
/// its operation waits, in a destructor, until the worker has let go of it. The thread
/// cannot return to its dispatcher from there, so for the length of that wait hold()
/// has every handover set the event as well, and the waiter sleeps on it in
/// wait_held().
class sta_signal
{
public:
    using wake_t = core::function<void() noexcept>;

    /// The sleeping mode. Auto-reset: a wakeup means "look again".
    sta_signal() = default;

    /// The worker's call, at the end of every handover.
    ///
    /// A held signal calls back all the same, and not out of caution: the worker reads
    /// the flag after taking the trigger, and the trigger it took may have been armed
    /// before the hold began -- by a drain that has since returned to its dispatcher and
    /// is owed exactly this callback. Withheld, it would be lost; made, it costs a drain
    /// that may find nothing, and only while somebody is waiting in place.
    inline void set() {
        if (wake_ && held_.load(std::memory_order_relaxed)) event_.set();

        if (wake_)
            (*wake_)();
        else
            event_.set();
    }

    /// The STA side's call, and only in the sleeping mode -- a loop with a
    /// dispatcher behind it has no business blocking the thread that owns it.
    inline void wait() {
        ensure(!wake_ && "sta_loop: a loop driven by a callback never sleeps");

        event_.wait();
    }

    /// The STA side's call before it arms the trigger for a wait in place: handovers
    /// set the event from now on, in either mode.
    ///
    /// Relaxed, and still seen in time by every handover that matters: one that takes
    /// the trigger the waiter arms afterwards reads it through that exchange. One that
    /// took an older arm may miss it, and then only calls back -- the waiter, arming
    /// after it, finds its element by the look it takes before sleeping.
    inline void hold() noexcept { held_.store(true, std::memory_order_relaxed); }

    /// Ends what hold() began. A handover that took the trigger just before may still
    /// set the event -- a spurious wakeup for whoever sleeps on it next, which every
    /// sleeper here already loops over.
    inline void release() noexcept { held_.store(false, std::memory_order_relaxed); }

    /// Sleeps on the event, in either mode; the one wait a driven loop is allowed.
    inline void wait_held() { event_.wait(); }

    /// The STA side's call, for a turn the loop owes a waiter answered on this thread
    /// (sta_loop::queue_loop_turn): a driven loop is called back from here, as a handover would
    /// call it back. The sleeping one looks at its turns before it sleeps, and is not woken.
    inline void wake_from_sta_thread() {
        if (wake_) (*wake_)();
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
    std::atomic<bool> held_{false};
};

namespace detail {

/// A producer answered on the STA thread outside the loop, in line for the loop to resume its
/// waiter (sta_loop::queue_loop_turn). Built into the producer, so a turn allocates nothing.
struct loop_turn {
    async_op* op;
    loop_turn* next = nullptr;
};

/// The half of an orphanable call under a token that its task holds. Defined after the loop.
template <class R>
class orphanable_wait;

/// Hands an orphanable operation to the loop the way async_call(orphanable, fn) does, for
/// async_call_waiting_for_body(), which makes the operation itself. Defined after the loop.
void send_orphanable(async_op& op);

}  // namespace detail

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
///
/// **An operation given up while out is waited for, not resumed around.** When its
/// task goes away first (`async_op::abandon`), the STA thread waits in place --
/// looking ahead in the return channel, never taking anything out of it, never
/// resuming anybody, because it may be in the middle of unwinding and a coroutine
/// resumed from there could throw into it. Giving up is the exception's path and pays
/// for itself: the channel is walked from its head for every operation given up, and
/// the path with no error is not touched for it.
class sta_loop
{
    /// The worker sleeps on its completion port, and the queue it reads wakes it
    /// through the same port; the STA side sleeps or is called back, and that is
    /// what `sta_signal` decides. Either way the arm/disarm protocol inside the
    /// channel makes sure a wakeup is never lost.
    using to_worker_t = spsc_channel<async_op*, 256, io_port>;
    using from_worker_t = spsc_channel<async_op*, 256, sta_signal>;

    /// The worker thread. It has no state of its own beyond the channels of the
    /// run: what to do arrives as an operation or as a completion from the
    /// kernel, and where to put the answer is the same for all of them.
    class worker : public threaded_component
    {
    public:
        inline worker(std::string_view name, thread_group* group)
            : threaded_component(name, group) {}

        inline ~worker() override { dispose(); }

    protected:
        /// In sta_loop.cpp.
        void run() override;

        /// Closing the channel is what ends run(), and the forced signal wakes
        /// the thread if it is asleep on the port at that moment.
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

    static inline to_worker_t to_worker_;
    static inline from_worker_t from_worker_;

    /// Operations out and not taken back yet, counted by the STA thread alone: up
    /// where one is sent or handed to the kernel, down where one is taken out of the
    /// return channel. stop() checks that it is back at zero.
    static inline std::size_t outstanding_ = 0;

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

    /// Producers answered here outside the loop -- an orphanable call whose token fired --
    /// in the order they were, each waiting for the loop to resume its waiter. Counted among
    /// the outstanding, so that the end of a process and stop() see them as well.
    static inline detail::loop_turn* loop_turns_ = nullptr;
    static inline detail::loop_turn** loop_turns_end_ = &loop_turns_;

    friend class async_op;
    friend void detail::send_orphanable(async_op& op);

    template <class R>
    friend class detail::orphanable_wait;

    /// What async_op::abandon() asks of the loop, in sta_loop.cpp: the look into the
    /// return channel, the wait in place, and the tidying after it. wait_until_back()
    /// answers whether it took over a callback the dispatcher is owed, which
    /// return_dispatcher_trigger() then settles.
    ///@{
    static bool is_back(async_op* op) noexcept;
    static bool wait_until_back(async_op* op) noexcept;
    static void take_if_next(async_op* op) noexcept;
    static void return_dispatcher_trigger() noexcept;
    ///@}

    /// What a loop with a dispatcher queue under it does differently from one on the
    /// worker alone, chosen as the loop starts.
    struct loop_mode
    {
        /// Carries out an orphanable operation: on the worker, or on the system's
        /// thread pool, so that the worker never stands in a call that goes by a name.
        void (*send_orphanable)(async_op&);

        /// stop()'s last word, once the worker is gone.
        void (*release)() noexcept;

        /// run_until()'s wait: turns what this mode turns until the condition holds. The
        /// condition comes erased, a function and its object, so that the mode under a
        /// queue can live in sta_queue.cpp with the system headers.
        void (*wait_until)(bool (*done)(void*), void* condition);

        /// Sees to it that run_pending() comes for a turn the loop owes a waiter, from the
        /// STA thread: the sleeping loop needs nothing, since run_one() looks at its turns
        /// before it sleeps; a driven one is called back; the one under a queue posts its
        /// drain, and a queue that refuses it stops the process, since that waiter would
        /// never be resumed.
        void (*wake_for_turn)() noexcept;
    };

    inline static void send_to_worker(async_op& op) { enqueue(core::as_not_null<async_op>(&op)); }

    inline static void release_nothing() noexcept {}

    inline static void wake_host_for_turn() noexcept {
        from_worker_.wakeup().wake_from_sta_thread();
    }

    /// The sleeping mode's wait: in run_one() until the condition holds.
    inline static void sleep_until(bool (*done)(void*), void* condition) {
        while (!done(condition)) run_one();
    }

    static constexpr loop_mode on_worker{&send_to_worker, &release_nothing, &sleep_until,
                                         &wake_host_for_turn};

    /// In sta_queue.cpp.
    ///@{
    static void send_to_pool(async_op& op);
    static void release_queue() noexcept;

    /// The wait under a queue: turns the thread's message loop, which is what delivers
    /// the worker's handovers, the thread pool's answers and the queue's timers alike.
    static void pump_messages_until(bool (*done)(void*), void* condition) noexcept;

    /// The idle hook's call: arms the trigger a drain standing in a modal loop holds, and
    /// posts for what is waiting, so that the modal loop is called back into.
    static void post_if_pending() noexcept;
    /// The turn's drain, posted; a refusal is a waiter never resumed, and stops the process.
    static void post_drain_for_turn() noexcept;

    static constexpr loop_mode on_queue{&send_to_pool, &release_queue, &pump_messages_until,
                                        &post_drain_for_turn};
    ///@}

    static inline const loop_mode* mode_ = &on_worker;

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
    ///        that comes back, and on the STA thread, for a turn the loop owes a
    ///        waiter answered there -- from the two threads, possibly at the same
    ///        moment. All it may do is ask the STA thread to call run_pending()
    ///        later; calling it here, synchronously, is not allowed, and neither
    ///        is refusing: a wake-up refused is a waiter never resumed. In a WinUI
    ///        application that is one `DispatcherQueue.TryEnqueue`.
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
        // sleeper makes before it sleeps, made here once, for the mode that
        // never sleeps.
        from_worker_.arm();

        threads_ = thread_group::create();
        worker_.emplace(worker_name, threads_.get());
        worker_->start_async().get();
    }

    /// Starts the loop for a thread with a message loop, on that thread's own
    /// dispatcher queue -- the system's, `Windows.System.DispatcherQueue`, made here
    /// if the thread has none. The worker's handovers are posted to it, and so is
    /// every orphanable operation on its way back from the thread pool.
    ///
    /// The queue has to outlive stop(): a post it refuses is an operation that never
    /// comes back, which stop() then finds still out. One made here is shut down by
    /// stop() itself; one the thread already had is its owner's to keep.
    ///
    /// In sta_queue.cpp.
    static void start_dispatched(std::string_view worker_name = "sta_loop worker");

    /// Stops the worker. Returns once the worker's thread has exited -- its component
    /// waits for that -- so a process that ends right after does not kill it on its
    /// way out. Under start_dispatched() the queue this made is shut down at the end,
    /// the way the thread would have to before it exits.
    ///
    /// Nothing may be out by then, and that is checked rather than waited for: the loop is
    /// stopped once its work is over -- an application turns it with run_until() until its
    /// last detached frame is gone and nothing is outstanding() -- so an operation still out
    /// here is one nobody is going to see back. Taken back here, it would answer nobody, and
    /// a coroutine waiting for it would be destroyed where it stood rather than finished; so
    /// it is an error of the program, and the process stops on it. An operation that never
    /// finishes by itself -- a read from a pipe nobody writes to -- is given up by its owner,
    /// which waits until the worker has let go of it; an orphanable one given up is left to
    /// finish alone, and is waited for with the rest.
    ///
    /// Doing nothing when there is no run is the point rather than an
    /// indulgence: a teardown path has no business knowing how far a startup
    /// path got before it threw.
    inline static void stop() {
        if (!worker_) return;

        ensure(outstanding_ == 0 &&
               "sta_loop: an operation is still out at stop() -- it was to be back first");

        if (worker_->was_started()) worker_->stop_async().get();

        worker_.reset();
        threads_.reset();

        // The wake-up goes with the run it belonged to. The channel holding it
        // is static and the callback's body is in the STA pool, so this is
        // where it is given back -- while there is still a pool to give it to.
        from_worker_.wakeup().forget_wake();

        mode_->release();
    }

    /// Whether there is a run in progress -- for the infrastructure that starts
    /// and stops it.
    inline static bool running() noexcept { return worker_.has_value(); }

    /// How many operations are out and not back yet: handed to the worker, to the pool or to
    /// the kernel, and not taken out of the return channel -- an orphan a frame let go of to
    /// finish alone counts, and so does one lying in the channel already, and a turn the loop
    /// owes a waiter. What the end of a process waits for, beside the detached frames, and
    /// what stop() checks is zero.
    inline static std::size_t outstanding() noexcept { return outstanding_; }

    /// Hands an operation to the worker. The STA thread's call.
    ///
    /// After stop() this is an error of the program and fails as one: the
    /// thread that stopped the loop is the thread sending, there is no worker
    /// left to carry the operation and nothing to carry it back with, and a
    /// coroutine that reaches this point was one the caller had promised to
    /// finish before stopping.
    inline static void enqueue(core::not_null<async_op> op) {
        to_worker_.send(op.get());
        ++outstanding_;
    }

    /// The port the worker sleeps on, for whoever opens a file: that is where the
    /// file's overlapped operations are told to finish.
    inline static io_port& port() noexcept { return to_worker_.wakeup(); }

    /// Starts an operation whose body is a lambda and returns the task the coroutine
    /// awaits.
    ///
    /// The operation is sent before the task exists -- and so, in principle,
    /// before there is a coroutine to name. That is safe for one reason only: the
    /// thread that would resume the coroutine is the STA thread, and it is here,
    /// inside this call, and cannot be in run_one() at the same time.
    ///
    /// Sent from here rather than from await_suspend() on purpose: the work starts
    /// at the call, so `auto a = f.read(x); auto b = g.read(y); co_await a;
    /// co_await b;` has both in the worker's queue before either is waited on. That
    /// is also why the op is a block of its own rather than a member of the awaiter:
    /// an awaiter in the frame can only send once its address is final, and the
    /// variant that does so measured 20 ns more per operation on the floor, not
    /// less (sta_loop_benchmark.cpp, "in the frame"), against 2 ns for the block.
    ///
    /// The body is the application's code, and so is its answer to a cancellation: a
    /// coroutine given a token asks it itself, before the call and after the co_await. The
    /// body on the worker does not read the token -- its state is the STA thread's, and the
    /// flag in it is not atomic.
    template <class Fn>
    [[nodiscard]] static task<std::invoke_result_t<std::decay_t<Fn>&>> async_call(Fn&& fn) {
        using result_t = std::invoke_result_t<std::decay_t<Fn>&>;

        // Held as the base on the way in: async_run() deduces its result type
        // from the pointer, and deduction does not see through a derived class.
        std::unique_ptr<async_op_t<result_t>> op(new async_op_f<std::decay_t<Fn>>(
            std::forward<Fn>(fn)));

        return async_run(std::move(op));
    }

    /// The same for a body that touches nothing but what it owns -- its captures are
    /// copies, and the result is a value of its own. Given up, it is left to finish
    /// alone rather than waited for, and what it made is let go of at once
    /// (`orphanable_t`). A body that writes into the caller's frame must not be passed
    /// here: nothing would stop it from writing after the frame is gone.
    ///
    /// Under start_dispatched() it runs on the system's thread pool and not on the
    /// worker: what is orphanable here is what goes by a name -- opening a file,
    /// making a directory -- and may take the system as long as it likes.
    ///
    /// A cancellation is the coroutine's to ask about, as with the form above; the forms with
    /// a token are below.
    template <class Fn>
    [[nodiscard]] static task<orphanable_result_t<std::decay_t<Fn>>> async_call(
        orphanable_t, Fn&& fn) {
        using result_t = orphanable_result_t<std::decay_t<Fn>>;

        std::unique_ptr<async_op_t<result_t>> op(new orphanable_op_f<std::decay_t<Fn>>(
            std::forward<Fn>(fn)));

        mode_->send_orphanable(*op);

        return task<result_t>(std::move(op));
    }

    /// The body on the worker, under a token. One the worker has not reached when the token
    /// is cancelled is never started, and the co_await ends with the cancellation once it is
    /// back. One already running is not told: whether work of the application's may stop
    /// halfway only its own code knows, so it runs to its end and answers what it made, and
    /// its coroutine asks the token after the co_await. Under a token cancelled already
    /// nothing is made and the co_await ends with the cancellation at once; under one with no
    /// source this is the form without a token.
    template <class Fn>
    [[nodiscard]] static task<std::invoke_result_t<std::decay_t<Fn>&>> async_call(
        Fn&& fn, cancellation_token stop);

    /// The orphanable body waited for under its owner's token: for work the owner may stop
    /// wanting before it is done -- a picture no longer shown, the application closing.
    ///
    /// Cancelled, the co_await ends with operation_canceled_exception, always, and without
    /// waiting for the body: the owner cancels in order not to wait, and an orphanable body
    /// touches nothing of the frame, so there is nothing to wait for. From that moment the body
    /// is an orphan, exactly as when its task is dropped: one not started never starts; one
    /// running has the synchronous call to the system it stands in cancelled
    /// (CancelSynchronousIo), where the system can cancel it, and sees
    /// `orphan_state::stop_requested()` -- nothing else interrupts it, and a body of several
    /// steps asks between them; what it makes is thrown away; and outstanding() counts it
    /// until it is back, so the end of a process waits for it. The body never takes the
    /// token, whose state is the STA thread's.
    ///
    /// What the body holds, it holds until its end: a body that has a file open keeps it
    /// open after the cancel, and the next operation on the same file may meet it -- refused
    /// on sharing, or the file renamed or deleted under it. That is why wxl's own operations
    /// on the disk do not take this form, and wait for their body under a token instead.
    ///
    /// The race with a body ending as the token fires is settled on the STA thread: a body
    /// the loop has taken back before the token fires answers what it made, its value or its
    /// failure; one not taken back yet answers the cancellation, even if it has finished on
    /// its own thread. The waiter is resumed by the loop's next turn, never from inside the
    /// cancel, which resumes nobody. Under a token cancelled already nothing is made and the
    /// co_await ends with the cancellation at once; under one with no source this is the form
    /// without a token.
    template <class Fn>
    [[nodiscard]] static task<orphanable_result_t<std::decay_t<Fn>>> async_call(
        orphanable_t, Fn&& fn, cancellation_token stop);

    /// The same for an operation written out as a class of its own: whoever
    /// needs more than a lambda can do -- state that survives being executed
    /// twice, a body that answers `false` and waits for something to fire --
    /// derives from `async_op_t<R>` and starts it here.
    ///
    /// Ownership moves: from this call on, the operation belongs to the
    /// task, which lives in the frame of the coroutine that awaits it.
    template <class R>
    [[nodiscard]] static task<R> async_run(std::unique_ptr<async_op_t<R>> op) {
        enqueue(core::as_not_null<async_op>(op.get()));

        return task<R>(std::move(op));
    }

    /// The same under a token: `Op` is made here out of `args`, and stands under `stop` while
    /// it lives (`detail::cancellable_operation`) -- cancelled, it is asked to stop
    /// (`async_op::cancel()`, its on_cancel()), and answers what then happens to it. The token
    /// comes first, ahead of whatever the operation is made of. Under a token cancelled
    /// already nothing is made; under one with no source this is the form above.
    template <class Op, class... Args>
    [[nodiscard]] static task<typename Op::result_type> async_run(cancellation_token stop,
                                                                  Args&&... args);

    /// Starts an overlapped operation here, on the STA thread, without the trip to the
    /// worker. One the system finishes inside the call is delivered at once, and the
    /// coroutine awaiting it does not suspend; one the kernel takes comes back through
    /// the port and the return channel.
    ///
    /// Only for what is known not to hold the calling thread. The rest goes through
    /// async_run(), and is started on the worker.
    [[nodiscard]] inline static task<std::size_t> async_start(std::unique_ptr<overlapped_op> op) {
        // A completion with no worker to take it would never arrive; enqueue() fails the
        // same way, through send().
        assert(running() && "sta_loop: the loop is not running");

        if (op->start())
            op->complete_in_place();
        else
            ++outstanding_;

        return task<std::size_t>(std::move(op));
    }

    /// Runs the body here, on the STA thread, and returns it already delivered: for a
    /// call short enough not to be worth a trip, which still answers through a
    /// co_await and still fails there.
    ///
    /// The body is the application's code: a coroutine given a token asks it itself,
    /// before the call and after the co_await.
    template <class Fn>
    [[nodiscard]] static task<std::invoke_result_t<std::decay_t<Fn>&>> call_here(Fn&& fn) {
        using result_t = std::invoke_result_t<std::decay_t<Fn>&>;

        std::unique_ptr<async_op_t<result_t>> op(new async_op_f<std::decay_t<Fn>>(
            std::forward<Fn>(fn)));

        op->packaged_execute();
        op->complete_in_place();

        return task<result_t>(std::move(op));
    }

    /// Takes one finished operation and gives control back to the coroutine that
    /// was waiting for it -- or, with nobody waiting, deletes it if it was given up
    /// and otherwise leaves it for the co_await still to come. Sleeps while there is
    /// nothing to take. A turn the loop owes is taken first, without a look into the
    /// channel: it is here already, and no handover would end the sleep for it.
    ///
    /// \return `false` on a wakeup with nothing behind it -- the caller loops on a
    ///         condition of its own, as it does with the channel underneath.
    inline static bool run_one() {
        if (loop_turns_) [[unlikely]] {
            take_loop_turn();
            return true;
        }

        async_op* op = nullptr;

        if (!from_worker_reader_.receive(op)) return false;

        --outstanding_;

        // The op may be gone by the time this returns: deleted if it was given up, or
        // taken with the co_await that resumes here.
        op->complete();

        return true;
    }

    /// Runs until the caller's own condition says the work is done: at the end of a
    /// process, no detached frame being alive; a flag a coroutine sets on its way. A task
    /// having ended is the overload below.
    ///
    /// The wait is the mode's. The sleeping loop sleeps in run_one(); the loop under a
    /// dispatcher queue turns the thread's message loop, so that what reaches the
    /// thread through the queue -- the thread pool's answers, the queue's timers -- is
    /// delivered on the way as well. A loop driven by a callback has no wait of its own
    /// and refuses: its host's loop is where it is waited in.
    inline static void run_until(std::predicate auto done) {
        using condition_t = std::remove_reference_t<decltype(done)>;

        mode_->wait_until(
            [](void* condition) {
                return static_cast<bool>((*static_cast<condition_t*>(condition))());
            },
            std::addressof(done));
    }

    /// Runs until the task ends, and gives what it ended with: the value, moved out, or
    /// the exception that left it, thrown here. The synchronous way through a coroutine,
    /// for code with no coroutine of its own to await it in -- a destructor with a last
    /// write to see through, a test. The task is the caller's and has to last the call;
    /// a temporary does, for the length of its statement.
    ///
    /// \param where the caller's line, for a strict build's report; left to its default.
    template <class R>
    inline static R run_until(task<R>& work, detail::site where = detail::site::current()) {
        run_until([&work] { return work.done(); });

        return work.result(where);
    }

    template <class R>
    inline static R run_until(task<R>&& work, detail::site where = detail::site::current()) {
        return run_until(work, where);
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
    /// One trip, and not one *callback* each either. In the driven mode this
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
    /// In the sleeping mode none of that happens: run_one() runs the protocol
    /// around its own sleep, and this only takes what is there.
    ///
    /// A continuation delivered here may open a modal loop -- a message box, a
    /// menu -- and stand in it; the trigger is taken for as long as it does, so
    /// nothing posts into that loop by itself. The loop's idle is where this is
    /// called again, nested: that drain arms the trigger, and from then on the
    /// handovers post into the modal loop like any other.
    ///
    /// Turns the loop owes -- a waiter answered here, outside it -- are taken on the way in,
    /// and again whenever a continuation resumed on the way gives another. A waiter resumed
    /// here that opens a modal loop leaves the turns behind it to that loop's idle, as it
    /// leaves the handovers: the drain called there, nested, takes them -- under a queue the
    /// idle hook posts for them too.
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
            while (loop_turns_) [[unlikely]] resumed += take_loop_turn();

            for (async_op* op = nullptr; take(op);) resumed += op->complete();

            // A continuation resumed above may have cancelled a token and given a turn.
            if (loop_turns_) [[unlikely]] continue;

            if (!driven) return resumed;

            // Set rather than asserted clear: a continuation delivered above may have run
            // a modal loop, and a drain nested in it leaves the trigger armed.
            from_worker_.rearm();
            std::atomic_thread_fence(std::memory_order_seq_cst);

            async_op* op = nullptr;

            if (!take(op)) return resumed;

            from_worker_.disarm();
            resumed += op->complete();
        }
    }

private:
    /// Takes the next operation out of the return channel, if there is one.
    inline static bool take(async_op*& op) noexcept {
        if (!from_worker_reader_.read(op)) return false;

        --outstanding_;
        return true;
    }

    /// Gives a producer answered on this thread a turn: its waiter is resumed by the loop's
    /// next look, never from inside the call that answered it. For the first turn in line the
    /// mode sees to that look (loop_mode::wake_for_turn), as a handover does.
    inline static void queue_loop_turn(detail::loop_turn& turn) noexcept {
        const bool first = !loop_turns_;

        turn.next = nullptr;
        *loop_turns_end_ = &turn;
        loop_turns_end_ = &turn.next;
        ++outstanding_;

        if (first) mode_->wake_for_turn();
    }

    /// Takes the first turn in line and gives it to its producer, which resumes its waiter --
    /// or, given up meanwhile, goes. \return whether a coroutine was resumed.
    inline static bool take_loop_turn() {
        detail::loop_turn* const turn = loop_turns_;

        loop_turns_ = turn->next;
        if (!loop_turns_) loop_turns_end_ = &loop_turns_;
        --outstanding_;

        // The producer, and the turn built into it, may be gone by the time this returns.
        return turn->op->complete();
    }
};

namespace detail {

/// What a form with a token returns under a token cancelled already: nothing is made that
/// does work, and the task answers the cancellation at once.
template <class R>
[[nodiscard]] task<R> canceled_task() {
    return sta_loop::call_here([]() -> R { throw operation_canceled_exception(); });
}

inline void send_orphanable(async_op& op) { sta_loop::mode_->send_orphanable(op); }

/// The orphanable body under a token, its co_await waiting for the body: for wxl's own
/// operations on the disk by name -- async_file's and async_directory's. The public form lets
/// the waiter go at the request and leaves the body to finish alone, holding what it holds;
/// for the disk that would let the next operation on the same name run beside it. A read let
/// go of keeps its file open, and a save of the same name that follows fails on its rename; a
/// write_all() repeated at once would find the temporary file still held by the orphan and
/// fail on a sharing violation, have its temporary deleted by the orphan before its rename,
/// or have it renamed by the orphan and fail itself. So here the request is passed to the
/// body and the co_await ends once the body has let go -- which cancelling its call makes
/// short: one not started never starts, the
/// synchronous call it stands in is cancelled where the system can cancel it, and one of
/// several steps reads `orphan_state::stop_requested()` between them. The answer is what the
/// body answered: the cancellation if it threw operation_canceled_exception, if a call
/// cancelled under it failed with ERROR_OPERATION_ABORTED, or if it never started; otherwise
/// its value or its own failure -- a change that got through answers that it did.
template <class Fn>
[[nodiscard]] task<orphanable_result_t<std::decay_t<Fn>>> async_call_waiting_for_body(
    orphanable_t, Fn&& fn, cancellation_token stop) {
    using result_t = orphanable_result_t<std::decay_t<Fn>>;

    if (!cancellation_state_of(stop)) return sta_loop::async_call(orphanable, std::forward<Fn>(fn));
    if (stop.is_canceled()) return canceled_task<result_t>();

    std::unique_ptr<async_op_t<result_t>> op(
        new cancellable_operation<orphanable_op_f<std::decay_t<Fn>>>(std::move(stop),
                                                                     std::forward<Fn>(fn)));

    send_orphanable(*op);

    return task<result_t>(std::move(op));
}

}  // namespace detail

template <class Fn>
task<std::invoke_result_t<std::decay_t<Fn>&>> sta_loop::async_call(Fn&& fn,
                                                                   cancellation_token stop) {
    using result_t = std::invoke_result_t<std::decay_t<Fn>&>;

    if (!detail::cancellation_state_of(stop)) return async_call(std::forward<Fn>(fn));
    if (stop.is_canceled()) return detail::canceled_task<result_t>();

    std::unique_ptr<async_op_t<result_t>> op(
        new detail::cancellable_operation<async_op_f<std::decay_t<Fn>>>(std::move(stop),
                                                                         std::forward<Fn>(fn)));

    return async_run(std::move(op));
}

namespace detail {

// sta_loop::async_call(orphanable, fn, stop) is made of two producers, because the waiter
// reads its answer while the body, given up, may still be writing one: the wait, which the
// task holds and which lives on the STA thread alone, and the body, an orphan no task holds,
// carried out on the worker or the pool. While the body is out the two point at each other,
// and the first end cuts the link: the body back first hands over what it made; the token
// first, or the task dropped first, leaves the body an orphan.

/// The wait: the answer the waiter reads, and the call's place in the token's event.
template <class R>
class orphanable_wait final : public async_op_t<R>
{
public:
    /// Made only under a token that has a source and is not cancelled yet.
    inline explicit orphanable_wait(cancellation_token stop)
        : async_op_t<R>(orphanable), stop_(std::move(stop)), node_(*this), turn_{this} {
        cancellation_state_of(stop_)->event().add(
            core::as_not_null<cancellation_event::func_t>(&node_));
    }

    /// Out of the event before the token goes; once the token has been cancelled, the event
    /// the node stood in has been fired and is gone, and remove() finds nothing.
    inline ~orphanable_wait() override {
        cancellation_state_of(stop_)->event().remove(
            core::cookie_t{static_cast<cancellation_event::func_t*>(&node_)});
    }

    /// The body has been sent: from now on the two are linked.
    inline void attach_body(async_op_t<R>& body) noexcept { body_ = &body; }

    /// The body came back while linked: its answer is the answer. The body's caller resumes
    /// the waiter.
    inline void accept_answer_from(async_op_t<R>& body) noexcept {
        body_ = nullptr;
        this->take_answer_from(body);
    }

    /// The task lets go. A body still out is left an orphan, and the wait goes at once; an
    /// answered wait goes; one in line for its turn is given up, and the loop deletes it
    /// with the turn.
    void release() override {
        if (body_) {
            detach_body();
            delete this;
            return;
        }

        async_op::release();
    }

protected:
    /// Never called: the wait is not sent anywhere -- its body is the other producer, and the
    /// wait is answered here, on the STA thread, by the body or by the token.
    bool execute() override { return true; }

private:
    /// Cuts the link. The body hears it as its cancellation, and nothing else cancels it: it
    /// lets go of the wait and finishes as an orphan, given up.
    inline void detach_body() noexcept { std::exchange(body_, nullptr)->cancel(); }

    /// The token was cancelled. A body still out is left an orphan, the answer becomes the
    /// cancellation, and the loop is given the turn that resumes the waiter. Answered
    /// already, the wait keeps its answer.
    inline void on_token_canceled() noexcept {
        if (!body_) return;

        detach_body();
        this->answer_canceled();
        sta_loop::queue_loop_turn(turn_);
    }

    /// The wait's place in the token's event.
    class node final : public cancellation_event::func_t
    {
    public:
        inline explicit node(orphanable_wait& wait) noexcept : wait_(wait) {}

        inline void operator()() noexcept override { wait_.on_token_canceled(); }

        /// The wait owns its node.
        inline void release() noexcept override {}

    private:
        orphanable_wait& wait_;
    };

    cancellation_token stop_;
    node node_;
    loop_turn turn_;

    /// The body while the two are linked; null once either end has cut the link.
    async_op_t<R>* body_ = nullptr;
};

/// The body: an orphan like any other, which no task holds. Taken back while still linked,
/// it hands its answer to the wait and resumes the wait's waiter.
template <class Fn, class R = orphanable_result_t<Fn>>
class orphanable_body final : public orphanable_op_f<Fn, R>
{
public:
    template <class Fn2>
    inline orphanable_body(orphanable_wait<R>& wait, Fn2&& fn)
        : orphanable_op_f<Fn, R>(std::forward<Fn2>(fn)), wait_(&wait) {
        this->start_without_task();
    }

protected:
    bool complete_without_task() override {
        orphanable_wait<R>* const wait = wait_;

        if (wait) wait->accept_answer_from(*this);

        delete this;

        return wait && wait->complete();
    }

    /// The wait let go: the one thing that asks a body no task holds to stop.
    void on_cancel() noexcept override {
        wait_ = nullptr;
        orphanable_op_f<Fn, R>::on_cancel();
    }

private:
    orphanable_wait<R>* wait_;
};

}  // namespace detail

template <class Fn>
task<orphanable_result_t<std::decay_t<Fn>>> sta_loop::async_call(orphanable_t, Fn&& fn,
                                                                 cancellation_token stop) {
    using result_t = orphanable_result_t<std::decay_t<Fn>>;

    // The answer moves from the body to the wait inside the loop's call that takes the body
    // back, which has nowhere to throw to and has to delete the body whatever happens.
    static_assert(std::is_void_v<result_t> || std::is_nothrow_move_constructible_v<result_t>,
                  "sta_loop::async_call(orphanable, fn, stop): the result must move without "
                  "throwing");

    if (!detail::cancellation_state_of(stop)) return async_call(orphanable, std::forward<Fn>(fn));
    if (stop.is_canceled()) return detail::canceled_task<result_t>();

    std::unique_ptr<detail::orphanable_wait<result_t>> wait(
        new detail::orphanable_wait<result_t>(std::move(stop)));
    std::unique_ptr<async_op_t<result_t>> body(
        new detail::orphanable_body<std::decay_t<Fn>>(*wait, std::forward<Fn>(fn)));

    mode_->send_orphanable(*body);
    wait->attach_body(*body.release());

    return task<result_t>(std::move(wait));
}

template <class Op, class... Args>
task<typename Op::result_type> sta_loop::async_run(cancellation_token stop, Args&&... args) {
    using result_t = typename Op::result_type;

    if (!detail::cancellation_state_of(stop))
        return async_run(
            std::unique_ptr<async_op_t<result_t>>(new Op(std::forward<Args>(args)...)));

    if (stop.is_canceled()) return detail::canceled_task<result_t>();

    return async_run(std::unique_ptr<async_op_t<result_t>>(
        new detail::cancellable_operation<Op>(std::move(stop), std::forward<Args>(args)...)));
}

}  // export namespace wxl::async
