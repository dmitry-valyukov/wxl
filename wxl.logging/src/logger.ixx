module;

#include <fmt/format.h>

export module wxl.logging:logger;

import :entry;
import :facility;
import :output;
import :severity;
import wxl.core;
import std;

export namespace wxl::logging {

/// What every logger can do, and the shape of a logging call.
///
///     log.info("connected to {}:{}", host, port);
///     log.debug(*this, "queue holds {} of {}", used, capacity);
///
/// **One call is one message.** The format string and its arguments arrive
/// together, are formatted together into one buffer and reach the output as one
/// line. That is what a stream-style `log << a << b` cannot promise: with
/// several calls per message, two threads interleave halves of their lines and
/// the lock has to be held across all of them.
///
/// **A message that is not logged costs a comparison.** The severity is checked
/// before anything is formatted, so the arguments are evaluated -- the language
/// gives no way around that without macros -- but nothing is converted, joined
/// or allocated. Below `compiled_severity` there is not even the comparison:
/// the level is a template argument, so the whole body goes away at compile
/// time.
///
/// **The format string is checked while it is being compiled.** It is a
/// literal, and fmt turns it into a `fmt::format_string` in a consteval
/// constructor that walks it against the argument types. A mismatched brace or
/// a `{}` too many is an error in the build, not a surprise in the log.
///
/// **A logger belongs to one thread.** It holds the buffer it formats into, so
/// two threads sharing one would write over each other's half-built lines. The
/// arrangement this library is built for is a logger per thread over a shared
/// output: the output does the synchronising, and it does it once per finished
/// line instead of once per argument. An async_logger goes further and does not
/// even need that -- see :async_logger.
class base_logger : public core::noncopyable
{
public:
    virtual ~base_logger() = default;

    /// The level this logger passes; everything worse than it, and it, go
    /// through. Belongs to the logger's own thread, like the rest of it.
    severity level() const noexcept { return level_; }

    void set_level(severity level) noexcept { level_ = level; }

    bool enabled(severity of_message) const noexcept {
        return is_enabled(of_message, level_);
    }

    /// Both filters in series: the logger's own level and the facility's. The
    /// quieter of the two decides, which is what lets one noisy subsystem be
    /// turned down without touching the logger every other subsystem shares.
    bool enabled(severity of_message, const facility& where) const noexcept {
        return enabled(of_message) && where.enabled(of_message);
    }

    /// Logs at a level known while compiling. The named calls below are this
    /// one with the level filled in, and are what code normally writes; this
    /// form is for code that is generic over the level.
    ///@{
    template <severity Level, typename... Args>
    void log(fmt::format_string<Args...> form, Args&&... args) {
        if constexpr (is_compiled(Level)) {
            if (!enabled(Level)) return;

            write_line(Level, nullptr, form, std::forward<Args>(args)...);
        }
    }

    /// \param where The part of the program the message is about; its name goes
    ///              into the line and its level joins the filter.
    template <severity Level, typename... Args>
    void log(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        if constexpr (is_compiled(Level)) {
            if (!enabled(Level, where)) return;

            write_line(Level, &where, form, std::forward<Args>(args)...);
        }
    }
    ///@}

    /// The program cannot go on.
    ///@{
    template <typename... Args>
    void fatal(fmt::format_string<Args...> form, Args&&... args) {
        log<severity::fatal>(form, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void fatal(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        log<severity::fatal>(where, form, std::forward<Args>(args)...);
    }
    ///@}

    /// This algorithm or mode of work cannot go on.
    ///@{
    template <typename... Args>
    void error(fmt::format_string<Args...> form, Args&&... args) {
        log<severity::error>(form, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        log<severity::error>(where, form, std::forward<Args>(args)...);
    }
    ///@}

    /// Something is wrong and the program carries on regardless.
    ///@{
    template <typename... Args>
    void warn(fmt::format_string<Args...> form, Args&&... args) {
        log<severity::warn>(form, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        log<severity::warn>(where, form, std::forward<Args>(args)...);
    }
    ///@}

    /// An ordinary message: the level a release build runs at.
    ///@{
    template <typename... Args>
    void info(fmt::format_string<Args...> form, Args&&... args) {
        log<severity::info>(form, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        log<severity::info>(where, form, std::forward<Args>(args)...);
    }
    ///@}

    /// Where the program got to.
    ///@{
    template <typename... Args>
    void trace(fmt::format_string<Args...> form, Args&&... args) {
        log<severity::trace>(form, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        log<severity::trace>(where, form, std::forward<Args>(args)...);
    }
    ///@}

    /// What the program holds.
    ///@{
    template <typename... Args>
    void debug(fmt::format_string<Args...> form, Args&&... args) {
        log<severity::debug>(form, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(const facility& where, fmt::format_string<Args...> form, Args&&... args) {
        log<severity::debug>(where, form, std::forward<Args>(args)...);
    }
    ///@}

protected:
    explicit base_logger(severity level) noexcept : level_(level) {}

    /// Hands out the entry the next line is built in, emptied and ready. What
    /// it is differs: the synchronous logger keeps one and reuses it, the
    /// asynchronous one takes a fresh one from its pool.
    virtual log_entry& begin_entry() = 0;

    /// Takes the finished line away. The entry is complete, newline included.
    virtual void commit_entry(log_entry& entry) = 0;

private:
    /// Writes the fixed part of a line: timestamp, level, facility, and the
    /// space the message starts after.
    void start_line(log_entry& entry, severity level, const facility* where) const;

    /// The whole of a logged message, once it is known that it will be logged.
    /// A throw from formatting leaves the entry uncommitted and the line
    /// unwritten, which is the outcome that loses the least.
    template <typename... Args>
    void write_line(severity level, const facility* where, fmt::format_string<Args...> form,
                    Args&&... args) {
        log_entry& entry = begin_entry();

        start_line(entry, level, where);
        entry.buffer.format(form, std::forward<Args>(args)...);
        entry.buffer.append('\n');

        commit_entry(entry);
    }

    severity level_;
};

/// A logger that writes each line as it is made, on the thread that made it.
///
/// The entry it formats into is a member and is reused, so a logger that has
/// been running for a while allocates nothing per line: the buffer has long
/// since grown to the size the lines need.
class logger final : public base_logger
{
public:
    explicit logger(log_output& output, severity level = severity::info)
        : base_logger(level), output_(output) {}

    log_output& output() const noexcept { return output_; }

protected:
    log_entry& begin_entry() override {
        entry_.reset();

        return entry_;
    }

    void commit_entry(log_entry& entry) override { output_.write(entry); }

private:
    log_output& output_;
    log_entry entry_;
};

}  // export namespace wxl::logging
