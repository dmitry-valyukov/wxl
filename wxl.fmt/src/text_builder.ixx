// Assembling text: many pieces, one buffer, no string built and thrown away
// between them.
//
// Formatting and appending are two calls and not one overload set, and that
// is deliberate. A format string and a piece of text are different things --
// a brace in the first is an argument and in the second is a brace -- so a
// single append() that guessed by type would turn a book title with a "{" in
// it into an error at run time. What one wrote is what one gets: format()
// formats, append() copies.

export module wxl.fmt:text_builder;

import :text_buffer;
import fmt;
import std;

export namespace wxl::core {

/// A buffer with fmt pointed at it.
///
/// The three ways to give it a format string are three separate calls in
/// fmt's own design, and they stay three here:
///
///     wxl::core::text_builder<> out;
///     out.format("{:.1f}%", share * 100.0);            // checked at compile time
///     out.format("{}x{}"_cf, width, height);           // parsed at compile time
///     out.format(fmt::runtime(from_config), value);    // a string decided at run time
///     out.append(" -- braces {} and all");
///     write(out.view());
///
/// The compiled string -- "…"_cf, from fmt::literals -- is the one worth
/// reaching for in a loop: it leaves the call
/// with no format-string walk and no packing of the arguments into a
/// type-erased list, which measures at about 1.5 times the speed of the same
/// run without it. It only applies to a literal, and it quietly falls back to
/// the ordinary path for the spellings it cannot parse -- dynamic width and
/// precision among them -- so a loop that matters is worth measuring rather
/// than assuming.
///
/// Formatting appends: the builder is meant to be filled by several calls and
/// read once. reset() empties it without giving the memory back, which is what
/// makes one builder kept across many lines cheaper than one built per line.
///
/// The allocator is named as a template, the way text_buffer names it, and
/// defaults to the ordinary heap. text_builder<wxl::core::sta_allocator> puts
/// the buffer on the STA pool instead, which is for the thread that owns that
/// pool and no other: on the GUI thread that is the one to write.
template <template <typename> class Allocator = std::allocator>
class text_builder {
public:
    text_builder() = default;

    /// Formats the arguments and appends the result. The format string is a
    /// literal, which fmt checks against the arguments at compile time, or a
    /// string decided at run time wrapped in fmt::runtime().
    template <typename... Args>
    void format(fmt::format_string<Args...> form, Args&&... args) {
        fmt::format_to(fmt::appender(buffer_), form, std::forward<Args>(args)...);
    }

    /// The same for a compiled string, "…"_cf.
    ///
    /// A second overload rather than one template over the format string: the
    /// compile-time check above happens while the *literal* is being turned
    /// into a fmt::format_string, and a string that has already travelled into
    /// a template parameter is no longer a constant expression to check.
    template <typename Compiled, typename... Args>
        requires std::derived_from<Compiled, fmt::compiled_string>
    void format(const Compiled& form, Args&&... args) {
        fmt::format_to(fmt::appender(buffer_), form, std::forward<Args>(args)...);
    }

    /// Appends the text as it stands, braces and all.
    void append(std::string_view text) { buffer_.append(text.data(), text.data() + text.size()); }

    void append(char character) { buffer_.push_back(character); }

    /// What has been assembled so far, as a view into the buffer: valid until
    /// the next append and no longer.
    std::string_view view() const noexcept { return view_of(buffer_); }

    std::size_t size() const noexcept { return buffer_.size(); }

    bool empty() const noexcept { return buffer_.size() == 0; }

    /// Forgets the text, keeps the memory.
    void reset() noexcept { buffer_.clear(); }

    /// The buffer itself, for a caller that wants to hand fmt an appender of
    /// its own -- fmt::format_to(fmt::appender(out.buffer()), ...) -- or to
    /// use anything else that writes into an fmt buffer.
    text_buffer<Allocator>& buffer() noexcept { return buffer_; }

    const text_buffer<Allocator>& buffer() const noexcept { return buffer_; }

private:
    text_buffer<Allocator> buffer_;
};

}  // export namespace wxl::core
