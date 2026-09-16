#include "hresult.h"

namespace wxl {

void throw_hresult(std::int32_t hr) {
    throw hresult_error(hr);
}

} // namespace wxl
