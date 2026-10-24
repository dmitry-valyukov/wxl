#include <string>

#include <winrt/Windows.Foundation.h>

#include "Failure.h"
#include "impl/conversions.h"

namespace wxl {

Failure failureOf(std::exception_ptr error) {
    try {
        std::rethrow_exception(error);
    } catch (winrt::hresult_error const& failure) {
        return {static_cast<int32_t>(failure.code()), impl::from_winrt(failure.message())};
    } catch (std::exception const& failure) {
        std::string const what = failure.what();
        return {impl::e_fail, impl::from_winrt(winrt::to_hstring(what))};
    } catch (...) {
        return {impl::e_fail, {}};
    }
}

}  // namespace wxl
