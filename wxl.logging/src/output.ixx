export module wxl.logging:output;

import :entry;
import wxl.core;
import std;

export namespace wxl::logging {

/// Where a finished line goes: a file, a console, a socket, a vector in a test.
///
/// An output receives whole lines and nothing else. It does no formatting, sees
/// no format string and never learns what the arguments were -- by the time a
/// line reaches it there is only text, and the only decision left is whether to
/// write the prefix along with it.
///
/// **Outputs are shared, loggers are not.** One output usually has several
/// loggers behind it, on several threads, so an output that touches state of
/// its own synchronises itself. The logger does no locking on its behalf: see
/// the note on logger for why the split runs this way round.
class log_output : public core::noncopyable
{
public:
    virtual ~log_output() = default;

    /// A short name for diagnostics -- it appears when an output misbehaves and
    /// has to be reported. Not an identity: nothing looks outputs up by it.
    virtual std::string_view id() const noexcept = 0;

    /// Writes one line. Called on whichever thread produced the message, or, for
    /// an asynchronous logger, on the thread of its async_output.
    virtual void write(const log_entry& entry) = 0;
};

/// An output that drops everything, for a logger that has to exist and has
/// nowhere to write yet.
class null_output final : public log_output
{
public:
    std::string_view id() const noexcept override { return "null"; }

    void write(const log_entry&) override {}

    /// The one every such logger can share: it holds nothing and forgets
    /// everything, so a second instance would be indistinguishable.
    static null_output& instance() noexcept;
};

/// Hands each line to every output subscribed to it.
///
/// This is what makes "the console shows the important half, the file keeps
/// everything" one logger and not two: the severity range lives in each
/// end-point output, and the line is formatted once for all of them.
///
/// An output that throws while writing is dropped, with a note on stderr saying
/// which one and why. A log that cannot be written is not worth killing the
/// program over, and an output that has started throwing usually keeps at it --
/// carrying on with the rest is the behaviour the library it was ported from
/// settled on and it has not been regretted since.
class multicast_output : public log_output
{
public:
    std::string_view id() const noexcept override { return "multicast"; }

    /// Adds an end-point. Subscribing the same output twice is a mistake in the
    /// caller, and is refused as one.
    void subscribe(log_output& output);

    /// Removes an end-point. Unsubscribing one that is not subscribed is a
    /// mistake in the caller as well: the usual reason is that it was dropped
    /// after throwing, and finding out quietly is worse than finding out.
    void unsubscribe(log_output& output);

    void write(const log_entry& entry) override;

    /// How many end-points are subscribed.
    std::size_t size() const;

protected:
    /// Writes to every end-point without taking the lock: for a derived class
    /// that already holds it and is flushing a batch of lines through.
    void write_unlocked(const log_entry& entry);

    core::mutex& outputs_mutex() const noexcept { return mutex_; }

private:
    mutable core::mutex mutex_;
    std::vector<log_output*> outputs_;
};

}  // export namespace wxl::logging
