export module wxl.core:environment;

import wxl.stdint;
import std;

export namespace wxl::core {

/// Provides information about, and means to manipulate, the current environment and platform.
class environment
{
public:
    /// Returns the number of processors on the current machine.
    static size_t processor_count();

    /// The folder the running executable lives in: where a relative name an
    /// application writes -- "Assets/logo.png" -- points, because the build
    /// copies its assets there. A std::filesystem::path and not a core::path:
    /// this is asked once, at startup or when a data file is first named, and
    /// by code that may have no STA pool at all -- a test, a tool, a library
    /// below the window.
    static std::filesystem::path application_folder();
};

}  // export namespace wxl::core
