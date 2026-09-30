module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core::impl {

// The process heap, and nothing else: an HSTRING is freed by whoever drops the
// last reference -- possibly the operating system or another component, with
// HeapFree (see hstring.ixx). The layout is the one cppwinrt writes.
hstring_header* create_hstring(const char16_t* text, std::uint32_t length) {
    if (length == 0) return nullptr;

    const std::uint64_t bytes = sizeof(shared_hstring_header) + std::uint64_t{sizeof(char16_t)} * length;

    if (bytes > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("length");

    auto* header = static_cast<shared_hstring_header*>(
        ::HeapAlloc(::GetProcessHeap(), 0, static_cast<std::size_t>(bytes)));

    if (header == nullptr) throw std::bad_alloc();

    header->flags = 0;
    header->length = length;
    header->padding1 = 0;
    header->padding2 = 0;
    header->ptr = header->buffer;
    header->count.store(1, std::memory_order_relaxed);
    std::memcpy(header->buffer, text, std::size_t{length} * sizeof(char16_t));
    header->buffer[length] = 0;

    return header;
}

void free_hstring(hstring_header* header) noexcept {
    ::HeapFree(::GetProcessHeap(), 0, header);
}

}  // namespace wxl::core::impl
