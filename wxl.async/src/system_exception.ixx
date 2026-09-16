module;

#include "platform.h"

export module wxl.async:system_exception;

import std;

export namespace wxl::async {

/// Ошибка системного вызова: своё сообщение, объяснение Windows и её код.
///
/// Код по умолчанию берётся из ::GetLastError() прямо в точке броска — то
/// есть пока его не затёр никакой другой вызов, включая те, что делает сам
/// конструктор исключения.
class system_exception : public std::runtime_error
{
public:
    /// \param msg что мы пытались сделать; попадает в начало сообщения.
    /// \param err_code код системы; по умолчанию — последняя её ошибка.
    explicit system_exception(std::string_view msg,
                              int err_code = static_cast<int>(::GetLastError()));

    /// Код, который вернула система.
    int err_code() const noexcept { return err_code_; }

private:
    int err_code_;
};

}  // export namespace wxl::async
