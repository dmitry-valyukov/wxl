export module wxl.core:member_offset;

import std;

export namespace wxl::core {

/// typesafe object to field offset
template <typename t_obj, typename t_member>
std::ptrdiff_t field_offset(t_member t_obj::* member_ptr) {
    return reinterpret_cast<std::ptrdiff_t>(&reinterpret_cast<const volatile char&>(
        static_cast<t_obj*>(nullptr)->*member_ptr));
}

/// makes typed object from source address and offset
template <typename t_obj>
t_obj* typed_offset(void* source, std::ptrdiff_t offset) {
    char* s = reinterpret_cast<char*>(source);
    return reinterpret_cast<t_obj*>(s + offset);
}

/// makes typed object from const source address and offset
template <typename t_obj>
const t_obj* typed_offset(const void* source, std::ptrdiff_t offset) {
    const char* s = reinterpret_cast<const char*>(source);
    return reinterpret_cast<t_obj*>(s + offset);
}

/// typesafe obect from field address
template <typename t_obj, typename t_member>
t_obj* object_from_field(t_member t_obj::* member_ptr, t_member* member_addr) {
    return typed_offset<t_obj>(member_addr, -field_offset(member_ptr));
}

/// typesafe obect from const field address
template <typename t_obj, typename t_member>
const t_obj* object_from_field(t_member t_obj::* member_ptr, const t_member* member_addr) {
    return typed_offset<t_obj>(member_addr, -field_offset(member_ptr));
}

}  // export namespace wxl::core
