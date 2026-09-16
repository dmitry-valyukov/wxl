module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

semaphore::semaphore(long initial_count, long max_count)
    : handle_(::CreateSemaphoreW(nullptr, initial_count, max_count, nullptr)) {
    if (handle_ == nullptr) [[unlikely]]
        abort("Failed to create semaphore");
}

}  // namespace wxl::core
