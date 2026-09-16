export module wxl.logging:async_logger;

import :entry;
import :logger;
import :output;
import :severity;
import wxl.async;
import wxl.core;
import std;

export namespace wxl::logging {

/// Where the entries of an asynchronous logger come from and go back to.
///
/// A pool per logger, and a logger per thread, so taking an entry costs no
/// atomic and no allocation once the pool has warmed up. The entry carries a
/// pointer to the pool it came from, which is what lets the logging thread hand
/// it back to the right one after writing it -- that part *is* thread-safe, and
/// is the whole reason this is a safe_pool and not a plain one.
///@{
using entry_pool = async::safe_pool<log_entry>;
using pooled_entry = entry_pool::element_type;
using entry_ptr = core::pool_ptr<pooled_entry>;
///@}

/// Where an async_logger hands its finished lines. async_output is the one that
/// matters; the interface exists so that a logger can be pointed at something
/// else -- a test that keeps the entries, a queue of a different shape --
/// without the component model coming with it.
class async_sink
{
public:
    virtual ~async_sink() = default;

    /// Takes the entry over, leaving \p entry empty. Called on the logger's
    /// thread, and expected not to block it for long.
    virtual void enqueue(entry_ptr& entry) = 0;
};

/// How long the logging thread holds a line before writing it, so that lines
/// made at nearly the same moment on different threads come out in the order
/// they were made rather than the order they were queued.
inline constexpr core::duration default_window = core::duration::from_ms(100);

/// The thread that writes the log, and the fan-out to the outputs subscribed
/// to it.
///
/// Lines arrive from any number of loggers on any number of threads, are held
/// briefly so they can be sorted by the time they were *made*, and are then
/// written to every subscribed output. Everything expensive about logging --
/// the file, the console, the ordering -- happens here and not on the thread
/// that had something to say.
///
/// **The window is what buys the ordering.** Two threads that logged a
/// microsecond apart can reach the queue in either order; holding a line for
/// the window and writing the oldest first puts them back. A line is therefore
/// written no sooner than `window` after it was made, which is the price, and
/// flush_all() is how a caller that cannot wait says so.
///
/// **The thread sleeps until there is a reason to wake.** With nothing held it
/// waits on the queue with no deadline at all; with something held it waits
/// until that line comes due, and no longer. There is no interval on which it
/// wakes to look around.
///
/// Started and stopped like any other component: `start_async()` before the
/// first line, and the destructor stops it and waits for the thread. Stopping
/// writes out everything still held or queued -- a log that dropped its last
/// lines would be worst exactly when it mattered most.
class async_output final : public async::threaded_component,
                           public async_sink,
                           private multicast_output
{
public:
    explicit async_output(core::duration window = default_window,
                          async::thread_group* group = nullptr);

    ~async_output() override;

    /// The end-points lines are written to. Subscribing while the thread is
    /// running is allowed and takes effect for the next line written.
    ///@{
    using multicast_output::subscribe;
    using multicast_output::unsubscribe;
    using multicast_output::size;
    ///@}

    void enqueue(entry_ptr& entry) override;

    /// Waits until everything enqueued before this call has been written.
    ///
    /// Everything, including the lines still inside their window -- this is the
    /// call for the moment before a crash dump, a test assertion or an exit.
    ///
    /// Must not be called from an output's own write(), which runs on the
    /// thread this waits for.
    void flush_all();

    /// Waits until everything that has come due has been written, leaving the
    /// lines still inside their window where they are.
    void flush_window();

    core::duration window() const noexcept { return window_; }

protected:
    void run() override;

    void on_stopping() override;

private:
    /// Ordered so that a request for the whole queue outranks one for the part
    /// of it that has come due, when both are waiting for the same round.
    enum class flush_kind {
        none,
        window,
        all,
    };

    /// What one round of the thread owes its callers: what was asked for, and
    /// how many are waiting to hear that it is done.
    struct flush_request {
        flush_kind kind = flush_kind::none;
        std::size_t waiters = 0;
    };

    void ask_for_flush(flush_kind kind);
    flush_request take_request();
    std::size_t close_requests();
    void answer(std::size_t waiters);

    void take_everything_queued();
    void hold(entry_ptr entry);
    void write_due(log_time now);
    void write_everything();
    void wait_for_more();
    core::duration time_until_due(log_time now) const;

    async::mpsc_channel<pooled_entry> channel_;

    /// Lines waiting out their window, oldest first. The logging thread's own:
    /// nothing else touches it.
    std::deque<entry_ptr> held_;

    const core::duration window_;

    /// Guards the three fields below and nothing else. The waiting itself
    /// happens outside it, so the thread can answer without waiting for the
    /// caller that is waiting for the answer.
    core::mutex flush_mutex_;
    flush_request request_;
    bool accepting_requests_ = true;

    core::semaphore answered_{0};
};

/// A logger that hands its lines to another thread to write.
///
/// The call costs the formatting and a push onto a lock-free queue; the file,
/// the console and the ordering all happen elsewhere. Nothing here locks: the
/// pool belongs to this logger and the queue is built for many writers.
///
/// One per thread, like every logger -- and here that is not only about the
/// buffer: a pool shared by two threads would hand the same entry to both.
class async_logger final : public base_logger
{
public:
    /// \param warm_entries How many entries to make up front. Zero leaves the
    ///        pool to grow into its work, which costs an allocation per line
    ///        until it has; a thread that must not allocate at all once it is
    ///        running asks for as many as it can have in flight at once.
    explicit async_logger(async_sink& sink, severity level = severity::info,
                          std::size_t warm_entries = 0, std::size_t pool_size = 256);

protected:
    log_entry& begin_entry() override;

    void commit_entry(log_entry& entry) override;

private:
    async_sink& sink_;
    entry_pool pool_;

    /// The entry being filled in, held between begin_entry() and commit_entry().
    /// A formatting error that throws in between leaves it here, and the next
    /// message takes it back -- one entry out of the pool for the lifetime of
    /// the logger, and no line half-written into the log.
    entry_ptr current_;
};

}  // export namespace wxl::logging
