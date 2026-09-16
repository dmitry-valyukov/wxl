module;

#include "abi.h"

export module wxl.logging:facility;

import :severity;
import wxl.core;
import std;

export namespace wxl::logging {

/// A named part of the program, and the level *that part* is logged at.
///
/// A facility is what turns one log into several without splitting the file:
/// every line carries the name of the subsystem that wrote it, and each
/// subsystem can be turned up or down on its own while the rest of the program
/// keeps its own level. Turning one worker up to debug while the service around
/// it stays at info is the whole point.
///
/// Facilities nest by holding a pointer to their parent, and the name in the
/// log is the whole chain: a watcher called `Watcher` inside `App.Storage`
/// writes lines marked `[App.Storage.Watcher]`. The chain is joined once, when
/// the facility is built, because it is read on every line and never changes.
///
/// The usual way to have one is to inherit it:
///
///     class file_watcher : public async::threaded_component,
///                          public wxl::logging::facility { ... };
///     ...
///     log.trace(*this, "stopped after {} files", count);
///
/// **A facility must outlive its children and every logger call naming it.**
/// It holds a raw pointer to its parent and hands out views into its own
/// string; nothing here keeps anything alive. In the arrangement above that
/// falls out of the objects being nested anyway.
///
/// The level is read from other threads while the thread that owns the
/// subsystem may be changing it, so it is atomic -- a facility turned up in the
/// middle of a run is a normal thing to do and not a data race.
class facility : public core::noncopyable
{
public:
    /// \param name The name of this part alone; the full name is built by
    ///             joining the parent's to it with a dot.
    /// \param parent The facility this one is part of, or nullptr for a root.
    explicit facility(std::string_view name, const facility* parent = nullptr,
                      severity level = severity::info)
        : full_name_(parent ? std::string(parent->full_name()) + '.' + std::string(name)
                            : std::string(name)),
          own_size_(name.size()),
          parent_(parent),
          level_(level) {
        ensure(!name.empty());
    }

    /// The name of this part alone, as a view into the full name -- the two
    /// cannot disagree because there is only one string. A name with a dot in
    /// it is stored as given and reads back as given; the dots this class adds
    /// are the ones between the links of the chain.
    std::string_view name() const noexcept {
        return std::string_view(full_name_).substr(full_name_.size() - own_size_);
    }

    /// The whole chain, parents first, dot-separated. This is what a log line
    /// carries.
    std::string_view full_name() const noexcept { return full_name_; }

    const facility* parent() const noexcept { return parent_; }

    severity level() const noexcept { return level_.load(std::memory_order_relaxed); }

    void set_level(severity level) noexcept { level_.store(level, std::memory_order_relaxed); }

    /// Whether this part of the program is being logged at that level. A
    /// message still has to pass the logger's own level as well -- the two
    /// filters are in series, and the quieter one wins.
    bool enabled(severity of_message) const noexcept {
        return is_enabled(of_message, level());
    }

private:
    std::string full_name_;
    std::size_t own_size_;
    const facility* parent_;
    std::atomic<severity> level_;
};

}  // export namespace wxl::logging
