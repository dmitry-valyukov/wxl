export module wxl.logging:console_output;

import :entry;
import :output;
import :severity;
import wxl.core;
import std;

export namespace wxl::logging {

/// Which of the two standard streams a console output writes to.
enum class console_stream {
    standard_output,
    standard_error,
};

/// What the console shows besides the message itself.
struct console_options {
    /// Marks the level by colour, using the escape sequences every terminal
    /// Windows 10 ships with understands. Worth turning off when the output is
    /// being captured rather than read.
    bool colored = true;

    /// Writes the timestamp, level and facility. Off leaves the message alone,
    /// which is what a console used as a running commentary usually wants.
    bool show_prefix = true;
};

/// Writes lines to a console.
///
/// The band of severities is the console's own: the usual arrangement gives it
/// everything down to info while the file beside it also keeps trace and debug,
/// and neither of them has to be a second logger.
///
/// The stream is written to under a lock of the output's own. Colour is two
/// escape sequences around the line, and a line torn between them would leave
/// the terminal dyed.
class console_output final : public log_output
{
public:
    explicit console_output(console_stream stream = console_stream::standard_output,
                            severity_range shows = {}, console_options options = {});

    std::string_view id() const noexcept override;

    void write(const log_entry& entry) override;

private:
    console_stream stream_;
    severity_range shows_;
    console_options options_;
    core::mutex mutex_;
};

}  // export namespace wxl::logging
