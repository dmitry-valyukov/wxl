module;

#include "platform.h"

export module wxl.async:last_error;

import std;

export namespace wxl::async {

/// Ошибка системного вызова: `std::system_error` в системной категории, чей текст -- наше
/// сообщение, код и объяснение системы: «CreateFileW (system error code = 2): The system
/// cannot find the file specified.» По коду ошибку ищут в документации; объяснение система
/// даёт на языке и в кодировке потока. Сам код -- `code().value()`; сравнение с `std::errc`
/// тоже работает, категория переводит коды Windows в условия POSIX.
///
/// Код по умолчанию берётся из ::GetLastError() прямо в точке броска -- то есть пока его
/// не затёр никакой другой вызов, включая те, что делает сам конструктор исключения.
class last_error : public std::system_error
{
public:
    /// \param msg что мы пытались сделать; попадает в начало сообщения.
    /// \param err_code код системы; по умолчанию -- последняя её ошибка.
    explicit last_error(std::string_view msg, int err_code = static_cast<int>(::GetLastError()));
};

}  // export namespace wxl::async
