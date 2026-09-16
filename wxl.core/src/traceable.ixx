export module wxl.core:traceable;

import std;

export namespace wxl::core {

/// Gives a component a human-readable name, usable for diagnostics.
class traceable
{
public:
    explicit traceable(std::string_view name) noexcept : name_(name) {}

    const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
};

}  // export namespace wxl::core
