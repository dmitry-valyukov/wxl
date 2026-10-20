module;

#include "reader.h"

// Контроль: заголовок и потребитель в партициях одного модуля — так, как
// wxl.gen жил до разреза.
//
// PROBE_PURVIEW_COMPLETE — тело партиции само пользуется полным TypeDef, как
// операторы сравнения в :md генератора.
// PROBE_GMF_CALL — во фрагменте функция зовёт метод, как сам winmd_reader.h
// в custom_attribute.h.

#ifdef PROBE_GMF_CALL
inline void probe_gmf_call(reader::TypeDef const& type) {
    (void)type.get_enum_definition();
}
#endif

export module probe.same:reader;

export namespace md {

using reader::TypeDef;
using reader::EnumDefinition;

#ifdef PROBE_PURVIEW_COMPLETE
inline bool operator<(TypeDef const& left, TypeDef const& right) noexcept {
    return left.index < right.index;
}
#endif

}  // namespace md
