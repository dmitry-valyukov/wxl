module;

#include "abi.h"

export module wxl.logging:severity;

import wxl.core;
import std;

export namespace wxl::logging {

/// How important a message is and -- read the other way round -- how much of
/// the log a reader asked for. The order is the one the levels are usually
/// named in, worst first, which makes "is this enabled?" a single comparison:
/// a logger set to info passes everything from fatal down to info and nothing
/// below it.
///
/// What each level is for, kept from the library this one was ported from:
///
///  - `fatal` -- the program cannot go on;
///  - `error` -- this algorithm or mode of work cannot go on;
///  - `warn`  -- the user did something wrong, or a run-time situation is off,
///               and the program carries on regardless;
///  - `info`  -- an ordinary message about a top-level subsystem ("the handler
///               has started"), the level a release build is expected to run at;
///  - `trace` -- *where* the program is: call chains, state changes;
///  - `debug` -- *what* the program holds: values of variables and fields.
///
/// The pair worth keeping straight is the last two, and the rule of thumb is a
/// question. "Where did it get to?" is trace and stays useful in a release
/// build; "what was in it?" is debug and usually matters only while the code is
/// being written. Two separate calls rather than one crammed message: the trace
/// half is the half that survives.
enum class severity {
    fatal,
    error,
    warn,
    info,
    trace,
    debug,
};

/// The name a log line spells the level with. Uppercase and ASCII, so it is the
/// same in every locale and can be searched for as it is written here.
constexpr std::string_view severity_name(severity level) noexcept {
    constexpr std::string_view names[] = {"FATAL", "ERROR", "WARN", "INFO", "TRACE", "DEBUG"};

    const auto index = static_cast<std::size_t>(level);

    ensure(index < std::size(names));

    return names[index];
}

/// Whether a message of \p level passes a filter standing at \p limit.
constexpr bool is_enabled(severity level, severity limit) noexcept {
    return level <= limit;
}

/// The lowest severity this build compiles at all: a call below it is thrown
/// away by the compiler rather than skipped at run time, arguments and format
/// string and all.
///
/// It is one constant and not a build option on purpose. The library it was
/// ported from had a set of macros for this (`MINIMAL_LOGGING`, `EXTRA_DEBUG`)
/// because in 2015 a disabled message still cost a chain of `operator<<` calls
/// that the optimiser had to be talked out of; here a disabled message costs one
/// comparison and a return, so there is nothing left to switch off for speed.
/// What the constant is still good for is size -- a binary that has no business
/// carrying its debug logging around -- and that is a decision made once, in
/// this line, rather than a flag every build has to agree on.
inline constexpr severity compiled_severity = severity::debug;

/// A band of severities, for an output that takes only part of what a logger
/// sends it: a console showing everything down to info while the file beside it
/// keeps the debug lines too.
///
/// `from` is the most severe end and `to` the mildest, which is the order the
/// enumerators are in; the default band is everything.
struct severity_range {
    severity from = severity::fatal;
    severity to = severity::debug;

    constexpr bool contains(severity level) const noexcept {
        return from <= level && level <= to;
    }
};

/// Whether messages of this level are compiled in at all.
constexpr bool is_compiled(severity level) noexcept {
    return is_enabled(level, compiled_severity);
}

}  // export namespace wxl::logging
