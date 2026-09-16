module wxl.xml;

import std;

namespace wxl::xml {

// A position the way a compiler reports one, so an editor can open the document
// at it straight from the message. Without a name there is still a position,
// and saying so beats pretending to a file.
parsing_exception::parsing_exception(const std::string_view file_name, const int line, const int column,
                                     const std::source_location& rule)
    : exception(file_name.empty() ? std::format("({},{}): the document does not parse", line, column)
                                  : std::format("{}({},{}): the document does not parse", file_name, line, column))
    , file_name_(file_name)
    , rule_(rule)
    , line_(line)
    , column_(column) {}

}  // namespace wxl::xml
