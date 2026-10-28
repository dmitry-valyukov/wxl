export module wxl.core:async.stop_reason;

import :async.cancellation;
import std;

export namespace wxl::async {

/// Component's stop reason: either stopped by request (no error) or stopped due to an error.
class stop_reason
{
public:
    /// Returns a stop reason without an error: stopped by request.
    inline static stop_reason stop_requested() { return stop_reason(); }

    /// \return A stop_reason holding a cached operation_canceled_exception.
    static const stop_reason abort();

    /// Constructs a stop reason with the error \p error. An empty \p error means stopped by
    /// request.
    inline stop_reason(const std::exception_ptr& error = std::exception_ptr()) noexcept
        : error_(error) {}

    /// Constructs a stop reason whose error is a `std::runtime_error` with the message
    /// \p reason.
    inline stop_reason(std::string_view reason)
        : error_(std::make_exception_ptr(std::runtime_error(std::string(reason)))) {}

    /// Constructs a stop reason whose error is a `std::runtime_error` with the message
    /// \p reason.
    inline stop_reason(const char* reason)
        : error_(std::make_exception_ptr(std::runtime_error(reason))) {}

    /// Gets the error; empty when stopped by request.
    inline const std::exception_ptr& error() const { return error_; }

    /// Tests whether the stop was requested, without an error.
    inline bool stopped_by_request() const { return !error_; }

    /// Tests whether the stop was due to an error.
    inline bool stopped_due_to_error() const { return !!error_; }

    /// Creates a stop reason whose error is a copy of the exception \p ex.
    template <class t_exception>
    static stop_reason from(const t_exception& ex) {
        return stop_reason(std::make_exception_ptr(ex));
    }

    /// Returns a description of the stop: `Stopped by request`, the message of the error, or
    /// `Unknown exception` for an error not derived from `std::exception`.
    std::string to_string() const;

private:
    std::exception_ptr error_;
};

/// Tests whether \p l and \p r hold the same error: the same exception object, or none.
inline bool operator==(const stop_reason& l, const stop_reason& r) {
    return l.error() == r.error();
}

/// Writes the description of a stop reason, as `wxl::async::stop_reason::to_string` returns it,
/// to a stream.
std::ostream& operator<<(std::ostream&, const stop_reason&);

}  // export namespace wxl::async
