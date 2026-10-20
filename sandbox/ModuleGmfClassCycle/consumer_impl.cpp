// То же, но в обычной единице трансляции: полный md::TypeDef нужен без записи
// своего интерфейса.

import probe.common;

#ifdef PROBE_ENUM_FIRST
static_assert(sizeof(md::EnumDefinition) > 0);
#endif

unsigned first_index() {
    md::TypeDef base;
    return base.index;
}
