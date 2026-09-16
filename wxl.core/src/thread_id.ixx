module;


#include "platform.h"
#include <intrin.h>

export module wxl.core:thread_id;

import wxl.stdint;

export namespace wxl::core {

using thread_id = uint32_t;

/// Returns the calling thread's OS identifier.
inline thread_id current_thread_id() noexcept {
#if defined(_M_X64)
    // GS:[0x48] UniqueThread (TID)
    return static_cast<DWORD>(__readgsqword(0x48));
#elif defined(_M_IX86)
    // FS:[0x24] UniqueThread (TID)
    return __readfsdword(0x24);
#else
#error Unsupported architecture
#endif
}

}  // export namespace wxl::core
