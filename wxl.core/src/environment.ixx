export module wxl.core:environment;

import wxl.stdint;

export namespace wxl::core {

/// Provides information about, and means to manipulate, the current environment and platform.
class environment
{
public:
    /// Returns the number of processors on the current machine.
    static size_t processor_count();
};

}  // export namespace wxl::core
