module;

#include "platform.h"

export module wxl.async:last_error;

import std;

export namespace wxl::async {

/// Ошибка системного вызова: `std::system_error` в системной категории. Текст `what()` -- для
/// программиста: наше сообщение, код и объяснение системы, которое STL берёт по-английски:
/// «CreateFileW (system error code = 2): The system cannot find the file specified.» По коду
/// ошибку ищут в документации. Для человека -- `explanation()`: те же слова системы, но на
/// его языке. Сам код -- `code().value()`; сравнение с `std::errc` тоже работает, категория
/// переводит коды Windows в условия POSIX.
///
/// Код по умолчанию берётся из ::GetLastError() прямо в точке броска -- то есть пока его
/// не затёр никакой другой вызов, включая те, что делает сам конструктор исключения.
class last_error : public std::system_error
{
public:
    /// \param msg что мы пытались сделать; попадает в начало сообщения.
    /// \param err_code код системы; по умолчанию -- последняя её ошибка.
    explicit last_error(std::string_view msg, int err_code = static_cast<int>(::GetLastError()));

    /// Объяснение системы на языке читателя -- том, что у потока, а нет его -- у
    /// пользователя, у системы, и лишь потом по-английски; без перевода строки, которым
    /// система заканчивает фразу. Пусто, если система кода не знает: тогда остаётся `what()`.
    std::wstring explanation() const;
};

}  // export namespace wxl::async
