module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

namespace {
void write_to_stream(std::ostream& s, const stop_reason& r) {
    if (r.stopped_by_request()) {
        s << "Stopped by request";
        return;
    }

    try {
        std::rethrow_exception(r.error());
    } catch (const std::exception& ex) {
        s << ex.what();
    } catch (...) {
        s << "Unknown exception";
    }
}
}  // namespace

std::string stop_reason::to_string() const {
    std::ostringstream oss;
    write_to_stream(oss, *this);
    return std::move(oss).str();
}

std::ostream& operator<<(std::ostream& ostr, const stop_reason& sr) {
    write_to_stream(ostr, sr);
    return ostr;
}

// stop_reason::abort() is defined in component.cpp: it shares a cached,
// ref-counted std::exception_ptr with StartableComponent's hot "stopped without error"
// path, to avoid allocating a fresh exception on every abort.

}  // namespace wxl::async
