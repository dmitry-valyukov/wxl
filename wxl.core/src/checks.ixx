export module wxl.core:checks;

import std;

export namespace wxl::core {

/**
 * Reports a failed `ensure()` check the way `abort()` reports its reason:
 * `cond_str` and its source location. Called by the release-build expansion
 * of `ensure()`; not intended to be called directly.
 */
[[noreturn]] void fail(std::string_view cond_str,
                       std::source_location loc = std::source_location::current());

/**
 * Ends the process over a broken rule there is no way back from. The reason
 * goes, as `file(line): why`, to stderr and to the debugger's output; with a
 * debugger attached the process stops right there, as at a failed assert, and
 * otherwise it aborts. The place is the caller's own line unless one is given:
 * a check that knows the line of the code that broke the rule passes that one.
 * Nothing is allocated on the way out, so it serves a pool that has run dry.
 */
[[noreturn]] void abort(std::string_view why,
                        std::source_location where = std::source_location::current()) noexcept;

/**
 * Says what went wrong and goes on: the line `abort()` writes, `file(line): what`,
 * to stderr and to the debugger's output, and no stop. For a failure that is
 * the process's to report and nobody's to answer any more -- one that comes when
 * there is nothing left to show it in -- so that it is not lost. Nothing is
 * allocated.
 */
void report(std::string_view what,
            std::source_location where = std::source_location::current()) noexcept;

// ---- What std::terminate says --------------------------------------------------------
//
// A noexcept call that an exception reaches ends in std::terminate, and the handler the
// runtime starts with ends the process through abort without a word. The one below names
// the exception that got there and stops through abort() above. MSVC keeps a terminate
// handler for each thread, so it is set where wxl starts code on a thread: by wWinMain on
// the thread of the windows, by the entry of each thread of a thread_group, by the callback
// wxl.async hands the thread pool, for as long as it runs, and by terminate_with_exception
// on whichever thread a coroutine nobody holds fails. A thread wxl does not start keeps the
// runtime's own until then. Machinery of wxl's own layers, hence impl: a consumer has no
// handler to set.

namespace impl {

/**
 * A report put together in a fixed buffer, owned by the caller: what does not fit is
 * dropped, and a report that was cut ends in "...", after the last whole code point that
 * fits. Nothing is allocated -- the terminate may have come from a std::bad_alloc.
 */
class terminate_report {
public:
    /// Room for "..." at least, which a cut report ends in.
    template <std::size_t N>
    explicit terminate_report(char (&text)[N]) noexcept : text_{text} {
        static_assert(N >= 3, "wxl: a terminate report needs room for \"...\"");
    }

    /// Bytes as they are; a cut that falls inside a sequence of UTF-8 goes back to its start.
    void put(std::string_view part) noexcept;

    /// UTF-16 as Windows hands it out, written as UTF-8, a lone surrogate as U+FFFD.
    void put(std::wstring_view part) noexcept;

    /// The report so far. Once cut, it takes nothing more.
    std::string_view text() const noexcept { return {text_.data(), used_}; }

private:
    void cut() noexcept;

    std::span<char> text_;
    std::size_t used_ = 0;
    bool cut_ = false;
};

/**
 * What names an exception of a type wxl.core does not know -- given by the layer that
 * knows the type. It is called from inside a catch of the exception being described,
 * looks at it with `throw;` inside a try of its own, and either puts the whole line for
 * it into the report and answers true, or answers false and leaves the report as it was.
 */
using exception_namer = bool (*)(terminate_report& report) noexcept;

/**
 * Gives describe_terminate() the namer it asks, from any thread, about an exception that
 * is not a std::exception. Once in a process, before the threads it is to serve start: a
 * second call, while one is set, is a broken rule and stops the process.
 */
void set_exception_namer(exception_namer namer) noexcept;

/**
 * Puts into `report` how std::terminate was reached: the exception being handled, if there
 * is one, named by its type -- what() of a std::exception, what the namer says, or that the
 * type is unknown -- or that there was none. The exception is the original object, rethrown
 * with `throw;`, not a copy: once terminate is entered because of a throw, the standard
 * counts an implicit handler of it as active ([except.handle]). std::current_exception() is
 * asked only whether there is one: at MSVC it makes a copy, which allocates -- the copy of a
 * std::exception copies its text, lost with no memory left -- and with no memory it hands
 * out a static std::bad_alloc instead, which still says that there is one.
 */
void describe_terminate(terminate_report& report) noexcept;

/**
 * The terminate handler: the report, then abort() above. Describing the exception may bring
 * std::terminate back -- a namer, a what() or the exception's destructor that throws -- and
 * the handler entered again on the same thread stops with a line of its own rather than
 * describing it over and over.
 */
[[noreturn]] void terminate_with_reason() noexcept;

/**
 * Sets terminate_with_reason() as the calling thread's terminate handler -- the
 * thread's own, since MSVC keeps one for each thread -- and returns the one it replaces,
 * as std::set_terminate does.
 */
std::terminate_handler set_terminate_with_reason() noexcept;

/**
 * Ends the process over `error`, an exception nobody is left to catch -- what a coroutine
 * nobody holds does when it fails: terminate is called from inside the catch of the
 * rethrown exception, so the handler finds it as the one being handled and names it, once.
 * The handler is set for the calling thread first, so that a thread wxl did not start --
 * a console's loop, a test -- names it too.
 */
[[noreturn]] void terminate_with_exception(std::exception_ptr error) noexcept;

}  // namespace impl

}  // export namespace wxl::core
