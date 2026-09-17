module wxl.xml;

import wxl.core;
import std;

namespace wxl::xml {

void validate_utf8(const std::string_view text) {
    // The walk itself is wxl::core's, eight bytes at a time; what belongs here
    // is only the shape of the complaint. Qualified from wxl:: because the
    // parameter is called text as well.
    if (const auto broken = wxl::core::unicode::find_invalid_utf8(text))
        throw exception(std::format("invalid UTF-8 at byte {}", *broken));
}

}  // namespace wxl::xml
