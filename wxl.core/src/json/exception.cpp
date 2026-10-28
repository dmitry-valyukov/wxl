module wxl.core;

import std;

namespace wxl::json {

// The position is written the way a compiler writes it, so that an editor can
// open the document at that line from the message. Without a file name the
// position is still reported, with no invented name in its place.
parsing_exception::parsing_exception(const std::string_view file_name, const int line, const int column,
                                     const std::string_view reason, const std::source_location& rule)
    : exception(file_name.empty()
                    ? std::format("({},{}): {}", line, column, reason)
                    : std::format("{}({},{}): {}", file_name, line, column, reason))
    , file_name_(file_name)
    , reason_(reason)
    , rule_(rule)
    , line_(line)
    , column_(column) {}

}  // namespace wxl::json
