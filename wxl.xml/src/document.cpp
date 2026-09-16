module wxl.xml;

import std;
import :arena;
import :parser;
import wxl.core;

namespace wxl::xml {
namespace {

/// The three bytes UTF-8 spells a byte order mark in.
constexpr std::string_view byte_order_mark = "\xEF\xBB\xBF";

/// How much is read from a stream at a time, whose length is not known ahead.
constexpr std::size_t stream_chunk = 64 * 1024;

/// The whole of a stream, which does not say ahead of time how long it is.
///
/// @throw exception when the stream fails before it ends -- a short read means
/// both, and only the stream can say which.
std::string read_whole_stream(std::istream& source) {
    std::string bytes;

    for (;;) {
        const std::size_t at = bytes.size();
        bytes.resize(at + stream_chunk);
        source.read(bytes.data() + at, stream_chunk);

        const auto read = static_cast<std::size_t>(source.gcount());
        bytes.resize(at + read);

        if (read == stream_chunk)
            continue;

        if (!source.eof() || source.bad())
            throw exception(std::format("the stream failed after {} bytes", bytes.size()));

        return bytes;
    }
}
}  // namespace

// ---- Reading a file ----

std::string read_file(const std::filesystem::path& path) {
    // Straight to the platform, at some 3 GB/s: on a document of a few
    // megabytes reading it would otherwise cost more than parsing it does.
    // wxl::core::file is that call with the handle owned and the read looped;
    // it takes the characters rather than a path, so it is given them.
    core::file source = core::file::open_read(path.c_str());

    if (!source.opened())
        throw exception(std::format("cannot open {}", wxl::core::to_utf8(path).chars()));

    const core::nullable<std::uint64_t> size = source.size();

    if (!size)
        throw exception(std::format("cannot measure {}", wxl::core::to_utf8(path).chars()));

    std::string bytes;

    // resize_and_overwrite rather than resize, which would first fill a whole
    // document's worth of buffer with zeroes nobody will ever read.
    bytes.resize_and_overwrite(static_cast<std::size_t>(*size),
                               [&source](char* const data, const std::size_t room) {
                                   return source.read(
                                       std::as_writable_bytes(std::span(data, room)));
                               });

    if (bytes.size() != *size)
        throw exception(std::format("cannot read {}", wxl::core::to_utf8(path).chars()));

    return bytes;
}

// ---- document ----

const node& document::parse_source() {
    std::string_view source = document_;

    // Dropping the mark here is what keeps every rule of the grammar free of
    // the question.
    if (source.starts_with(byte_order_mark))
        source.remove_prefix(byte_order_mark.size());
    else if (source.starts_with("\xFF\xFE") || source.starts_with("\xFE\xFF"))
        throw exception("the document is UTF-16; this reader parses UTF-8");

    // Once, over the whole document: the grammar walks bytes and would read a
    // broken sequence as content without noticing. Done here rather than
    // inside the parser because it is one pass over the buffer, not a rule.
    validate_utf8(source);

    // The arena comes into being with the parse it serves and goes away with
    // the tree it held: there is no tree before this line, and no arena for
    // one either.
    arena_ = std::make_unique<arena>();

    // The parse lives exactly here. Everything it has to hold -- where it
    // stands, the elements whose content is half-read, the attributes of the
    // element being read -- goes away with this object, so there is nothing to
    // clear before the next document and nothing to null after this one.
    parser reader(source, *arena_, options_, file_name_);

    root_element_ = &reader.parse();
    xml_version_ = reader.xml_version();

    return *root_element_;
}

const node& document::load(std::string&& source, const std::string_view file_name) {
    // The previous tree goes first, and in this order: its nodes live in the
    // arena and its views point into the buffer about to be replaced.
    root_element_ = nullptr;
    arena_.reset();
    xml_version_ = u8"1.0";
    file_name_.assign(file_name);

    document_ = std::move(source);

    return parse_source();
}

const node& document::load(std::istream& source, const std::string_view file_name) {
    if (!source.good())
        throw exception("the stream cannot be read from at all");

    return load(read_whole_stream(source), file_name);
}

const node& document::load_file(const std::filesystem::path& path) {
    return load(read_file(path), wxl::core::to_utf8(path).chars());
}

}  // namespace wxl::xml
