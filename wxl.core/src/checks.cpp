module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

[[noreturn]]
void abort(std::string_view err) noexcept {
#ifdef _DEBUG
    assert(false);
#else
    std::cerr << err << '\n';
#endif
    std::abort();
}

[[noreturn]]
void fail(std::string_view cond_str, std::source_location loc) {
    std::cout << "Precondition " << cond_str << " failed at " << loc.file_name() << ":"
              << loc.line() << std::endl;
    std::abort();
}

}  // namespace wxl::core
