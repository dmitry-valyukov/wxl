export module wxl.async:stop_reason;

import :cancellation;
import std;

export namespace wxl::async {

/// Component's stop reason: either stopped by request (no error) or stopped due to an error.
class stop_reason
{
public:
    static stop_reason stop_requested() {
        return stop_reason();
    }

    /// \return A stop_reason holding a cached operation_canceled_exception.
    static const stop_reason abort();

    stop_reason(const std::exception_ptr & error = std::exception_ptr()) noexcept
        : error_(error)
    {}

    stop_reason(std::string_view reason)
        : error_(std::make_exception_ptr(std::runtime_error(std::string(reason))))
    {}

    stop_reason(const char * reason)
        : error_(std::make_exception_ptr(std::runtime_error(reason)))
    {}

    const std::exception_ptr & error() const {
        return error_;
    }

    bool stopped_by_request() const {
        return !error_;
    }

    bool stopped_due_to_error() const {
        return !!error_;
    }

    template<class t_exception>
    static stop_reason from(const t_exception & ex) {
        return stop_reason(std::make_exception_ptr(ex));
    }

    std::string to_string() const;

private:
    std::exception_ptr error_;
};

inline bool operator==(const stop_reason & l, const stop_reason & r) {
    return l.error() == r.error();
}

std::ostream & operator<<(std::ostream &, const stop_reason &);

}  // export namespace wxl::async
