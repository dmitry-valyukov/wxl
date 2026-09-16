module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

namespace {

constexpr uintptr_t kuser_shared_data_address = 0x7FFE0000;
constexpr uintptr_t interrupt_time_offset = 0x008;  // KUSER_SHARED_DATA::InterruptTime

struct ksystem_time_light {
    volatile uint32_t low_part;
    volatile int32_t high1_time;
    volatile int32_t high2_time;
};

}  // namespace

time_stamp time_stamp::now() noexcept {
    const auto* interrupt_time = reinterpret_cast<const ksystem_time_light*>(
        kuser_shared_data_address + interrupt_time_offset);
    uint32_t low;
    int32_t high;

    do {
        high = interrupt_time->high1_time;
        low = interrupt_time->low_part;
    } while (high != interrupt_time->high2_time);

    const uint64_t ticks = (static_cast<uint64_t>(high) << 32) | low;

    return time_stamp::from_ticks(static_cast<int64_t>(ticks));
}

}  // namespace wxl::core
