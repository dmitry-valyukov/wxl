export module wxl.core:environment;

import wxl.stdint;
import :sta_stl;
import :unicode;
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

    /// The command line of the process, split into arguments the way the shell splits it
    /// (CommandLineToArgvW): the program first, then what it was given. On the STA thread
    /// and inside the pool's life: the strings are the pool's.
    /// \throw std::system_error if the system could not split it.
    static sta_vector<sta_wstring> command_line();

    /// The value of the environment variable \p name as the process's environment
    /// block holds it now: UTF-16 units, unchecked, since nothing makes the block
    /// well-formed. A variable that holds a path -- `APPDATA`, `SystemRoot` --
    /// becomes one from these units as they are. Empty when the variable is not
    /// set, and when it is set to nothing.
    ///
    /// Read with GetEnvironmentVariableW, not std::getenv: that one answers from
    /// the copy of the block the C runtime took at startup, where a variable set
    /// since with SetEnvironmentVariableW is missing, and answers in the ANSI code
    /// page. A std::wstring and not the pool's string, as application_folder() is
    /// a std::filesystem::path: a tool or a test with no STA pool asks this.
    /// \throw std::system_error if the system fails to read the block for a
    ///        reason other than the variable being absent.
    static std::wstring variable(wchar_t const* name);

    /// What the system says of an error code -- a Win32 code or an HRESULT -- in
    /// the reader's language: the thread's, else the user's, the system's, and
    /// only then English; without the line break the system ends the phrase
    /// with. Empty for a code the system does not know: what to show instead is
    /// the caller's choice. In wxl.core because whatever reports a system
    /// failure needs the words, a tool without wxl.async among them; and UTF-8
    /// because std::error_code::message() spells them in the ANSI code page.
    static u8_text error_explanation(std::uint32_t code);

    /// An error as a person reads it, in UTF-8: what a tool prints and a window
    /// shows. A file system error is rebuilt from its paths and the system's
    /// words for its code, since its own what() spells both through the ANSI
    /// code page. Any other error is its what(), with U+FFFD where that is not
    /// UTF-8: nothing of it is dropped on the way to the screen.
    static u8_text error_message(std::exception const& error);
};

}  // export namespace wxl::core
