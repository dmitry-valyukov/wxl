#pragma once

#include <unknwn.h>

#include <winrt/base.h>

#include "hresult.h"

// How an args member reaches the interface that declares it.
//
// An args view holds the raw ABI pointer the runtime handed the delegate and
// owns nothing. That pointer is the default interface of the type the *event*
// declares, which is not the interface a member inherited from a base args
// class lives on -- WinRT interfaces derive from IInspectable and from
// nothing else -- so the object has to be asked for the one the member needs.
//
// The query is per call and deliberately not cached: an args object exists
// for the duration of one callback and a handler reads a property or two out
// of it, so a cache would charge every event that reads nothing at all --
// and those are the ones that fire on every mouse move.

namespace wxl::impl {

// The interface, owned for the duration of the full expression the call
// appears in: the query hands back a reference and the returned projection
// object is what releases it.
template <typename I>
I args_as(void* abi) {
    void* iface{};
    check_hresult(static_cast<::IUnknown*>(abi)->QueryInterface(
        reinterpret_cast<GUID const&>(winrt::guid_of<I>()), &iface));

    I result{nullptr};
    winrt::attach_abi(result, iface);
    return result;
}

}  // namespace wxl::impl
