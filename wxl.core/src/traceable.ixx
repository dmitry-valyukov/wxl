export module wxl.core:traceable;

import std;

export namespace wxl::core {

/// Gives a component a human-readable name, usable for diagnostics.
class traceable
{
public:
    /// Constructs a component with the name \p name.
    inline explicit traceable(std::string_view name) noexcept : name_(name) {}

    /// Gets the name of the component.
    inline const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
};

}  // export namespace wxl::core
