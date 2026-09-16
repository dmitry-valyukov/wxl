#pragma once

// An output that keeps what it is given, which is what most of these tests want
// to look at. Include it *after* `import wxl.logging;` -- it names the module's
// types and does not import it itself, because a standard header pulled in
// after a module import is what MSVC will not have.

class recording_output final : public wxl::logging::log_output
{
public:
    std::string_view id() const noexcept override { return "recording"; }

    void write(const wxl::logging::log_entry& entry) override {
        const std::lock_guard<std::mutex> lock(mutex_);

        // Copied and not kept as a view: the synchronous logger reuses one
        // entry for every line, so the buffer behind this text is about to say
        // something else.
        lines_.emplace_back(entry.text());
        messages_.emplace_back(entry.message());
        levels_.push_back(entry.level);
        times_.push_back(entry.time);
    }

    std::vector<std::string> lines() const {
        const std::lock_guard<std::mutex> lock(mutex_);

        return lines_;
    }

    std::vector<std::string> messages() const {
        const std::lock_guard<std::mutex> lock(mutex_);

        return messages_;
    }

    std::vector<wxl::logging::severity> levels() const {
        const std::lock_guard<std::mutex> lock(mutex_);

        return levels_;
    }

    std::vector<wxl::logging::log_time> times() const {
        const std::lock_guard<std::mutex> lock(mutex_);

        return times_;
    }

    std::size_t size() const {
        const std::lock_guard<std::mutex> lock(mutex_);

        return lines_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;
    std::vector<std::string> messages_;
    std::vector<wxl::logging::severity> levels_;
    std::vector<wxl::logging::log_time> times_;
};

/// An output that fails on every line, for the rule that a broken output is
/// dropped rather than allowed to take the program with it.
class throwing_output final : public wxl::logging::log_output
{
public:
    std::string_view id() const noexcept override { return "throwing"; }

    void write(const wxl::logging::log_entry&) override {
        ++attempts;

        throw std::runtime_error("this output is broken");
    }

    int attempts = 0;
};
