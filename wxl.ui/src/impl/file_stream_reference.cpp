#include "file_stream_reference.h"

#include <winrt/Windows.Storage.h>

#include "file_uri.h"

// Imports last, after every plain header.
import wxl.core;

namespace wxl::impl {
namespace {

// The file at the path, waited for on the calling thread. The operation completes on a thread
// of the pool and wakes this one with an event; nothing here needs the calling thread to
// handle a message, so waiting on a thread that has a message loop is safe.
winrt::Windows::Storage::StorageFile get_file(winrt::hstring const& path) {
    auto const operation = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path);
    auto const done = std::make_shared<core::hevent>(true);
    operation.Completed([done](auto const&, winrt::Windows::Foundation::AsyncStatus) { done->set(); });
    done->wait();
    return operation.GetResults();  // throws what the operation failed with: no such file, no access
}

}  // namespace

winrt::Windows::Storage::Streams::RandomAccessStreamReference stream_reference_from_uri(winrt::Windows::Foundation::Uri const& uri) {
    using winrt::Windows::Storage::Streams::RandomAccessStreamReference;

    // The scheme as the platform parsed it; the path from the text it was written as.
    if (uri.SchemeName() != L"file") {
        return RandomAccessStreamReference::CreateFromUri(uri);
    }
    winrt::hstring const raw = uri.RawUri();
    std::wstring_view const text = raw;
    std::u16string path;
    if (!try_path_of_file_uri(std::u16string_view {reinterpret_cast<char16_t const*>(text.data()), text.size()}, path)) {
        throw winrt::hresult_invalid_argument(L"The address is not a file address that names a file: its escapes are not UTF-8.");
    }
    return RandomAccessStreamReference::CreateFromFile(get_file(winrt::hstring {std::wstring_view {reinterpret_cast<wchar_t const*>(path.data()), path.size()}}));
}

}  // namespace wxl::impl
