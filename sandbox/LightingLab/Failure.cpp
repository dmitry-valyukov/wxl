// Что система ответила отказом: код и её собственное объяснение.
//
// Обёртки wxl зовут WinRT через проекцию cppwinrt, и отказ системы выходит из
// них исключением этой проекции. Приложению на wxl его тип не виден, а стенду
// нужен именно ответ системы -- поэтому этот один файл смотрит в проекцию сам.

#include "platform.h"

#include <winrt/base.h>

#include "Lab.h"

using namespace wxl;

hstring lab::refusalOfCurrentException(char16_t const* what) {
    try {
        throw;
    } catch (winrt::hresult_error const& error) {
        winrt::hstring const message = error.message();
        std::u16string why;
        for (wchar_t const unit : message) {
            // Текст системы приходит со своим переводом строки в конце.
            if (unit != L'\r' && unit != L'\n') {
                why.push_back(static_cast<char16_t>(unit));
            }
        }
        return refusal(what, static_cast<std::int32_t>(error.code()), why.c_str());
    } catch (wxl::hresult_error const& error) {
        return refusal(what, error.code(), u"");
    } catch (...) {
        return refusal(what, u"исключение не от системы");
    }
}
