module;

#include <utility>

// The generator's view of winmd's reader: the names it uses, under a short
// alias, and the two functions a range-based for needs to find.

export module wxl.gen:md;

export import :winmd;

// Ranges over a pair of iterators. winmd declares these next to its tables
// -- every `for (auto&& m : type.MethodList())` in the generator goes
// through them -- and a range-based for looks begin/end up by
// argument-dependent lookup alone, which reaches only declarations a module
// makes visible. The ones the header carries are merely reachable through
// :winmd, so the pair is declared here, in winmd's own namespace, where the
// lookup will find it.
export namespace winmd::reader {

template <typename T>
auto const& begin(std::pair<T, T> const& values) noexcept {
    return values.first;
}

template <typename T>
auto const& end(std::pair<T, T> const& values) noexcept {
    return values.second;
}

template <typename T>
auto distance(std::pair<T, T> const& values) noexcept {
    return values.second - values.first;
}


// Comparing a coded index with the row it points at, which is how the
// reader's own std::equal_range over a table finds a row's children. winmd
// declares these beside its tables and they are found by argument-dependent
// lookup, which reaches only what a module makes visible -- so they are
// declared here, in winmd's namespace, one for one with the reader's own.
inline bool operator<(coded_index<HasCustomAttribute> const& left,
                      CustomAttribute const& right) noexcept {
    return left < right.Parent();
}

inline bool operator<(CustomAttribute const& left,
                      coded_index<HasCustomAttribute> const& right) noexcept {
    return left.Parent() < right;
}

inline bool operator<(coded_index<TypeOrMethodDef> const& left,
                      GenericParam const& right) noexcept {
    return left < right.Owner();
}

inline bool operator<(GenericParam const& left,
                      coded_index<TypeOrMethodDef> const& right) noexcept {
    return left.Owner() < right;
}

inline bool operator<(coded_index<HasConstant> const& left, Constant const& right) noexcept {
    return left < right.Parent();
}

inline bool operator<(Constant const& left, coded_index<HasConstant> const& right) noexcept {
    return left.Parent() < right;
}

inline bool operator<(coded_index<HasSemantics> const& left,
                      MethodSemantics const& right) noexcept {
    return left < right.Association();
}

inline bool operator<(MethodSemantics const& left,
                      coded_index<HasSemantics> const& right) noexcept {
    return left.Association() < right;
}

inline bool operator<(coded_index<HasFieldMarshal> const& left,
                      FieldMarshal const& right) noexcept {
    return left < right.Parent();
}

inline bool operator<(FieldMarshal const& left,
                      coded_index<HasFieldMarshal> const& right) noexcept {
    return left.Parent() < right;
}

inline bool operator<(NestedClass const& left, TypeDef const& right) noexcept {
    return left.NestedType() < right;
}

inline bool operator<(TypeDef const& left, NestedClass const& right) noexcept {
    return left < right.NestedType();
}

}  // namespace winmd::reader
