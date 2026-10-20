module;

#include <winmd_reader.h>

// Настоящий заголовок в межмодульной раскладке: как :winmd в wxl.gen.common.

export module probe.real;

export namespace md {

using winmd::reader::TypeDef;
using winmd::reader::EnumDefinition;

}  // namespace md
