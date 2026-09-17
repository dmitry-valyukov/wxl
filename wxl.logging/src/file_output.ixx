export module wxl.logging:file_output;

import :entry;
import :output;
import :severity;
import wxl.core;
import std;

export namespace wxl::logging {

/// How a file output names its file and when it starts a new one.
///
/// The three rotations are independent and combine: a run of a program with all
/// of them on writes `app-20260826.log`, then `app-20260826-part002.log` when
/// the first fills up, having first pushed the previous run's files down into
/// `app-20260826.log.1` and so on.
struct file_options {
    /// The band of severities this file keeps. The default keeps everything,
    /// which is what a file is usually for: the console beside it is the one
    /// that gets a narrower band.
    severity_range shows = {};

    /// Bytes after which the file is closed and the next part started. Zero is
    /// one file, however large it grows.
    std::uint64_t max_size = 0;

    /// Renames the files left by earlier runs into a numbered history --
    /// `app.log.1`, `app.log.2` -- so this run starts on a file of its own.
    bool rotate_by_run = false;

    /// Puts the date into the name: `app-20260826.log`. UTC, like the
    /// timestamps in the lines.
    bool rotate_by_date = false;

    /// Adds to the file if it is already there instead of truncating it. Has
    /// nothing to say when rotate_by_run has just moved that file aside.
    bool append = false;

    /// How many old files rotate_by_run keeps. The one that would become
    /// number history_depth + 1 is deleted.
    std::size_t history_depth = 5;
};

/// Writes lines to a file, starting a new one when the options say to.
///
/// Every line is flushed as it is written. A log is mostly read after the thing
/// it was recording went wrong, and a buffer still in the process when it died
/// takes the interesting part with it; the write is one call into a stream that
/// was opened for this and nothing else.
///
/// Derived classes get two places to step in: on_before_first_line(), for a banner at
/// the top of each file, and make_part_name(), for a naming scheme of their
/// own. Both are called with the output's lock held, and write_to_file() is the
/// call they use -- it does not take the lock again.
class file_output : public log_output
{
public:
    /// \param path Name of the file as the caller wants it; every rotation
    ///             decorates this name rather than replacing it.
    explicit file_output(const std::filesystem::path& path, file_options options = {});

    ~file_output() override;

    std::string_view id() const noexcept override;

    void write(const log_entry& entry) override;

    /// The directory the files are written to.
    inline const std::filesystem::path& directory() const noexcept { return directory_; }

    /// The name the caller asked for, without directory or extension.
    inline const std::string& stem() const noexcept { return stem_; }

    /// The extension, dot included, or empty.
    inline const std::string& extension() const noexcept { return extension_; }

    /// The file being written to right now.
    std::filesystem::path current_path() const;

    /// Bytes written to the current file so far.
    std::uint64_t current_size() const;

    /// Which part of the log is open, counting from one.
    unsigned part_number() const;

    /// The files an earlier run left behind under \p path, oldest last: the
    /// name itself, then `.1`, `.2` and so on, stopping at \p history_depth or
    /// at the first one that is not there.
    static std::vector<std::filesystem::path> history_of(const std::filesystem::path& path,
                                                         std::size_t history_depth);

protected:
    /// Called once per file, before its first line goes into it, with the file
    /// open and the lock held. The default writes nothing.
    ///
    /// Before the first *line*, and not when the file is opened: the first file
    /// is opened by this constructor, where an override of this would not be
    /// reached yet. A part that never gets a line never gets a banner either.
    virtual void on_before_first_line();

    /// Names part \p number. The default gives part one the plain name and
    /// every part after it a `-partNNN` before the extension.
    virtual std::filesystem::path make_part_name(unsigned number) const;

    /// Writes text to the open file and counts it against max_size. For use
    /// from write() and on_before_first_line() only: it does not take the lock.
    void write_to_file(std::string_view text);

    inline core::mutex& file_mutex() const noexcept { return mutex_; }

private:
    void open_part(unsigned number);
    void rotate_history();

    std::filesystem::path directory_;
    std::string stem_;
    std::string extension_;
    std::string id_;

    file_options options_;

    mutable core::mutex mutex_;
    std::ofstream file_;
    std::filesystem::path current_path_;
    std::uint64_t current_size_ = 0;
    unsigned part_number_ = 0;

    /// Set when a part is opened and cleared when its first line goes in: the
    /// hook is called then and not at open time, so that a derived class is
    /// fully built by the time it is asked for its banner.
    bool header_pending_ = false;
};

}  // export namespace wxl::logging
