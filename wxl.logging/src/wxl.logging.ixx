
// wxl::logging -- a log built around one call per message.
//
//     wxl::logging::file_output file("app.log", {.max_size = 16u << 20});
//     wxl::logging::console_output console({}, {.from = severity::fatal, .to = severity::info});
//     wxl::logging::multicast_output both;
//     both.subscribe(file);
//     both.subscribe(console);
//
//     wxl::logging::logger log(both, severity::debug);
//     log.info("listening on {}:{}", host, port);
//     log.debug(receiver, "queue holds {} of {}", used, capacity);
//
// The library this one was ported from built a message with a chain of
// operator<<, into a typed tuple that was formatted only if the level let it
// through. The idea it was after -- pay nothing for a message nobody will
// read, and never let two threads interleave halves of a line -- is the same
// here, and a format string does it more plainly: the arguments arrive with
// the string they belong to, the level is checked before any of it is
// formatted, and what the compiler cannot check about a stream chain it checks
// here while it is compiling the literal.
//
// The pieces, and what each of them is for:
//
//  - severity -- the six levels, the band an output takes, and the level below
//    which messages are not compiled at all;
//  - facility -- a named part of the program with a level of its own, so one
//    subsystem can be turned up without the rest of the log following it;
//  - entry -- one finished line, prefix and message in one buffer;
//  - output -- where lines go: multicast, null, console, file;
//  - logger -- the calls, and the synchronous logger;
//  - async_logger -- the same calls, with the writing moved off the thread that
//    made the message.
//
// What a caller has to keep to:
//
//  - a logger belongs to one thread. It owns the buffer its lines are built in;
//    two threads sharing one would write over each other. Outputs are the
//    opposite -- shared by construction, and synchronised themselves.
//  - an output must outlive every logger pointed at it, and a facility must
//    outlive the lines that name it. Nothing here holds anything alive.
//  - the timestamps are UTC, and the fractional part is printed to nanoseconds
//    whatever the clock's real resolution is.

export module wxl.logging;

export import :severity;
export import :entry;
export import :facility;
export import :output;
export import :console_output;
export import :file_output;
export import :logger;
export import :async_logger;
