module wxl.json;

import std;

namespace wxl::json {

// Место так, как его пишет компилятор: из такого сообщения редактор
// открывает документ прямо на нужной строке. Без имени место всё равно
// есть, и сказать это честнее, чем выдумать файл.
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
