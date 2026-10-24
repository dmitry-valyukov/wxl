#pragma once

// The seam with COM and WinRT where the projection alone does not reach: an args object asked for
// the interface a member lives on, a stream reference the platform reads for a file address, and
// the Windows App Runtime brought up for the process.
//
// One header for the area and two sources split by what they need, as items_helpers.h has
// items_helpers.cpp, item_template.cpp and item_factory.cpp: winrt_helpers.cpp takes wxl.core and
// the file addresses, bootstrap.cpp only the bootstrapper -- which is what lets a sandbox recipe
// compile it without wxl.core.
//
// Private: this header names winrt types, so only wxl's own sources include it.

#include "platform.h"

#include <unknwn.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

#include "hresult.h"

namespace wxl::impl {

/// The interface `I` of an args object, owned for the full expression the call appears in. An
/// args view holds the raw ABI pointer the runtime handed the delegate: the default interface of
/// the type the *event* declares, not the one a member inherited from a base args class lives on
/// -- WinRT interfaces derive from IInspectable alone -- so the object is asked for it. Per call
/// and not cached: an args object lives for one callback, and a cache would charge every event
/// that reads nothing, which are the ones raised on every mouse move.
template <typename I>
I args_as(void* abi) {
    void* iface{};
    check_hresult(static_cast<::IUnknown*>(abi)->QueryInterface(
        reinterpret_cast<GUID const&>(winrt::guid_of<I>()), &iface));

    I result{nullptr};
    winrt::attach_abi(result, iface);
    return result;
}

/// RandomAccessStreamReference.CreateFromUri for an address with the file scheme: the same
/// argument, the same result. The platform reads a reference only for http, https, ms-appx and
/// ms-appdata, and an application that is not packaged has no ms-appx, so a file address gets the
/// reference CreateFromFile makes -- opened when it is read, no handle held meanwhile. The file is
/// looked up here, on the calling thread, which waits; one that is not there is an error here and
/// not at the read. It has to be the platform's own reference: when another process reads the
/// clipboard the platform asks it for an interface of its own and drops one without it. Any other
/// address is the platform's, as it was.
winrt::Windows::Storage::Streams::RandomAccessStreamReference stream_reference_from_uri(
    winrt::Windows::Foundation::Uri const& uri);

/// Ensures the Windows App Runtime framework package -- the redistributable backing
/// WinUI3/Microsoft.UI.Xaml at run time, separate from the .winmd metadata the generator reads --
/// is resolved and available to this process, through the Windows App SDK bootstrapper
/// (MddBootstrapInitialize). Idempotent: the initialization happens once, the first time any
/// caller reaches it. wWinMain calls it, and so do the sandbox recipes that run WinUI without
/// wxl's entry point. Defined in bootstrap.cpp.
///
/// So that *using* wxl never requires an application author to write the bootstrap of the
/// Windows App SDK: wxl owns it, once, rather than pushing it onto every consuming application.
///
/// Throws hresult_error if the framework package cannot be resolved -- the Windows App Runtime
/// is not installed on the machine, or the process is not otherwise eligible to use it.
void ensure_windows_app_runtime_initialized();

}  // namespace wxl::impl
