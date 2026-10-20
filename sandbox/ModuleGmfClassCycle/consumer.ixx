// Интерфейс другого модуля, которому нужен полный md::TypeDef: член по
// значению, как type_facts::base в :metadata генератора.
//
// PROBE_ENUM_FIRST — обход у потребителя: сперва требуется полный
// EnumDefinition, потом TypeDef.

export module probe.consumer;

import probe.common;

#ifdef PROBE_ENUM_FIRST
static_assert(sizeof(md::EnumDefinition) > 0);
#endif

export struct Facts {
    md::TypeDef base;
};
