module;
#include "pch.h"

module wxl.core;
import std;

using namespace wxl::core;

size_t environment::processor_count() {
    SYSTEM_INFO sysinfo;
    ::GetSystemInfo(&sysinfo);

    return sysinfo.dwNumberOfProcessors;
}
