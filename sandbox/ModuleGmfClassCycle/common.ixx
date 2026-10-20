module;

#include "reader.h"

// Модуль, который несёт заголовок в своём глобальном фрагменте и отдаёт его
// имена using-объявлениями под коротким псевдонимом, — как :winmd в
// wxl.gen.common.
//
// Ручки обходов со стороны модуля:
//   PROBE_GMF_CALL        во фрагменте, после заголовка, функция зовёт метод
//   PROBE_PURVIEW_CALL    то же в теле модуля, экспортом
//   PROBE_PURVIEW_TYPE    тело модуля называет выведенный тип возврата
//   PROBE_PURVIEW_COMPLETE тело модуля пользуется полным TypeDef

#ifdef PROBE_GMF_CALL
inline void probe_gmf_call(reader::TypeDef const& type) {
    (void)type.get_enum_definition();
}
#endif

export module probe.common;

export namespace md {

using reader::TypeDef;
using reader::EnumDefinition;

#ifdef PROBE_PURVIEW_CALL
inline auto probe_purview_call(TypeDef const& type) {
    return type.get_enum_definition();
}
#endif

#ifdef PROBE_PURVIEW_TYPE
using probe_enum_definition = decltype(TypeDef{}.get_enum_definition());
#endif

#ifdef PROBE_PURVIEW_COMPLETE
inline bool operator<(TypeDef const& left, TypeDef const& right) noexcept {
    return left.index < right.index;
}
#endif

}  // namespace md
