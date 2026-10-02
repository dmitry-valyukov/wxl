#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

// RandomAccessStreamReference.CreateFromUri for an address with the file scheme.
//
// The platform reads such a reference only for http, https, ms-appx and ms-appdata: for any
// other scheme OpenReadAsync fails with E_NOTIMPL ("this API supports only URI schemes http,
// https, ms-appx and ms-appdata"), with the reference used directly and with the one the
// clipboard hands back alike. An application that is not packaged has no ms-appx, so a file
// address is the one a picture of its own folder is written with.
//
// A file address gets the reference CreateFromFile makes for the file at that path: opened
// when it is read, no handle held meanwhile. The file is looked up here, on the calling
// thread, which waits for the answer, and one that is not there is an error here and not at
// the read. It has to be the platform's own reference and not one of wxl's: when another
// process reads the clipboard the platform asks the reference for an interface of its own,
// and a reference that does not have it is dropped without a word -- the format is on the
// clipboard and there is nothing behind it. Any other address is the platform's, as it was.
//
// Private: this header names winrt types, so only wxl's own sources include it.

namespace wxl::impl {

/// Drop-in for CreateFromUri: the same argument, the same result.
winrt::Windows::Storage::Streams::RandomAccessStreamReference stream_reference_from_uri(winrt::Windows::Foundation::Uri const& uri);

}  // namespace wxl::impl
