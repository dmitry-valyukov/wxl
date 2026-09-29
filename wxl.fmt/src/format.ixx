// Text for the interface, formatted in one call.
//
// The result is checked text, and no scan earns that: the format string is a
// literal fmt has already checked against the arguments at compile time, and
// the arguments are numbers, real or complex, whose text is ASCII, or text
// checked before. Nothing else is accepted -- a raw u16string_view or a
// single unit would carry its own well-formedness into the result unseen, so
// it does not compile. Text from outside goes through checked() first, and
// arrives here as a u16_view.

export module wxl.fmt:format;

import :text_buffer;
import fmt;
import wxl.core;
import std;

namespace wxl::core {

namespace impl {

/// Text the result may carry as it stands: a number or checked UTF-16.
template <typename T>
concept u16_argument = std::same_as<T, u16_view> || std::same_as<T, u16_text>;

template <typename T>
concept number_argument = std::is_arithmetic_v<T> && !std::same_as<T, char> &&
                          !std::same_as<T, wchar_t> && !std::same_as<T, char8_t> &&
                          !std::same_as<T, char16_t> && !std::same_as<T, char32_t>;

template <typename T>
struct is_complex : std::false_type {};

template <number_argument T>
struct is_complex<std::complex<T>> : std::true_type {};

}  // namespace impl

}  // namespace wxl::core

/// A complex number as (re+imi), the spec of a number applied to both parts:
/// {:.9g} on a root prints nine digits of each. Always in this shape, so a
/// real root still reads as a complex one.
namespace fmt {

template <wxl::core::impl::number_argument T, typename Char>
struct formatter<std::complex<T>, Char> : formatter<T, Char> {
    template <typename Context>
    auto format(const std::complex<T>& value, Context& ctx) const -> typename Context::iterator {
        using part = formatter<T, Char>;
        auto out = ctx.out();
        *out++ = Char('(');
        ctx.advance_to(out);
        out = part::format(value.real(), ctx);
        *out++ = Char(std::signbit(value.imag()) ? '-' : '+');
        ctx.advance_to(out);
        out = part::format(std::abs(value.imag()), ctx);
        *out++ = Char('i');
        *out++ = Char(')');
        return out;
    }
};

}  // namespace fmt

namespace wxl::core {

namespace impl {

template <typename T>
concept format_argument = number_argument<std::remove_cvref_t<T>> ||
                          is_complex<std::remove_cvref_t<T>>::value ||
                          u16_argument<std::remove_cvref_t<T>>;

/// What fmt sees of an argument: the number itself, checked text as its view.
template <typename T>
struct plain_argument {
    using type = T;
};

template <u16_argument T>
struct plain_argument<T> {
    using type = std::u16string_view;
};

template <typename T>
using plain_t = plain_argument<std::remove_cvref_t<T>>::type;

template <typename T>
constexpr plain_t<T> plain(const T& value) noexcept {
    if constexpr (u16_argument<T>)
        return value.plain();
    else
        return value;
}

using u16_buffer = fmt::basic_memory_buffer<char16_t, buffered_capacity>;
using u16_format_args = fmt::basic_format_args<fmt::buffered_context<char16_t>>;

/// The one copy, out of the buffer into the string the caller keeps.
inline u16_text text_of(const u16_buffer& buffer) {
    return u16_text{unicode::assume_valid(std::u16string_view(buffer.data(), buffer.size()))};
}

u16_text format_u16(fmt::basic_string_view<char16_t> form, u16_format_args args);

/// The arguments are named here so that fmt can take their addresses: its
/// argument store refers to them and lives no longer than this call.
template <typename... Plain>
u16_text format_plain(const fmt::basic_string_view<char16_t> form, Plain... plain) {
    return format_u16(form, fmt::make_format_args<fmt::buffered_context<char16_t>>(plain...));
}

}  // namespace impl

export {

/// Formats numbers and checked text into checked UTF-16 -- the text a
/// control shows:
///
///     format(u"{:.9g}", root)   // double or std::complex<double> alike
///
/// The format string is a literal, checked against the arguments at compile
/// time. The arguments are numbers, real or std::complex, and u16_view or
/// u16_text; anything else does not compile, since the result promises to be
/// well-formed and copies its arguments as they are.
template <impl::format_argument... Args>
u16_text format(fmt::basic_format_string<char16_t, std::type_identity_t<impl::plain_t<Args>>...> form,
                const Args&... args) {
    return impl::format_plain(form.get(), impl::plain(args)...);
}

/// The same for a compiled string, u"…"_cf, parsed at compile time. A second
/// overload for the reason text_builder gives: the check above happens while
/// the literal becomes a format string, and a compiled one is past that.
template <typename Compiled, impl::format_argument... Args>
    requires std::derived_from<Compiled, fmt::compiled_string> &&
             std::same_as<typename Compiled::char_type, char16_t>
u16_text format(const Compiled& form, const Args&... args) {
    impl::u16_buffer buffer;
    fmt::format_to(fmt::basic_appender<char16_t>(buffer), form, impl::plain(args)...);
    return impl::text_of(buffer);
}

}  // export

}  // namespace wxl::core
