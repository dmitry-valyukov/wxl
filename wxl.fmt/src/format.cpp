module wxl.fmt;

import fmt;
import wxl.core;
import std;

namespace wxl::core::impl {

u16_text format_u16(const fmt::basic_string_view<char16_t> form, const u16_format_args args) {
    u16_buffer buffer;
    fmt::vformat_to(fmt::basic_appender<char16_t>(buffer), form, args);
    return text_of(buffer);
}

}  // namespace wxl::core::impl
