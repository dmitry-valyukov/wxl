module;

#include <winmd_reader.h>

// Настоящий заголовок в партиции того же модуля, что и потребитель: как
// :winmd в wxl.gen до разреза.

export module probe.real_same:reader;

export namespace md {

using winmd::reader::TypeDef;
using winmd::reader::EnumDefinition;

}  // namespace md
