#include "statics.h"

namespace wxl::impl {
namespace {

// The settled state: the interface sits in arg1_, and the slot hands out the
// address of that storage, which the proxy reads as the projection object.
void* settled(statics_slot* slot) noexcept {
    return &slot->arg1_;
}

} // namespace

void* statics_slot::init(statics_slot* slot) {
    void* itf = nullptr;
    check_hresult(resolve_activation_factory(static_cast<wchar_t const*>(slot->arg1_),
                                             *static_cast<GUID const*>(slot->arg2_), &itf));

    // Patched only on success, and both fields together: from here on the
    // dispatch is the shared return of what was resolved.
    slot->arg1_ = itf;
    slot->fn_ = &settled;
    return &slot->arg1_;
}

} // namespace wxl::impl
