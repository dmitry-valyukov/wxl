module;

// The single place winmd_reader.h is included. It drags <windows.h>,
// <regex>, <future> and <filesystem> in behind it, so paying for it once
// here rather than in every unit of the generator is the whole reason this
// partition exists.
#include <winmd_reader.h>

export module wxl.gen:winmd;

// A using-declaration names the entity itself, so `md::TypeDef` *is*
// winmd::reader::TypeDef -- the same type, with its ordering and its
// argument-dependent lookup intact, reaching a unit that never saw the
// header. The alias namespace is what makes that possible at all: a
// namespace may not name its own members in a using-declaration, so the
// re-export cannot be written into winmd::reader itself.
//
// Only what the generator names is listed: every entity here is one more
// thing the importing units must be able to reconcile with their own view
// of the standard library, so the list stays as short as the code allows.
export namespace md {

using winmd::reader::TypeDef;
using winmd::reader::TypeRef;
using winmd::reader::TypeSpec;
using winmd::reader::TypeSig;
using winmd::reader::TypeDefOrRef;
using winmd::reader::GenericTypeInstSig;
using winmd::reader::MethodDef;
using winmd::reader::MethodDefSig;
using winmd::reader::ParamSig;
using winmd::reader::Property;
using winmd::reader::PropertySig;
using winmd::reader::Field;
using winmd::reader::Event;
using winmd::reader::InterfaceImpl;
using winmd::reader::CustomAttribute;
using winmd::reader::ElemSig;
using winmd::reader::ElementType;
using winmd::reader::Constant;
using winmd::reader::EnumDefinition;
using winmd::reader::NestedClass;
using winmd::reader::GenericParam;
using winmd::reader::MethodSemantics;
using winmd::reader::FieldMarshal;
using winmd::reader::HasCustomAttribute;
using winmd::reader::HasConstant;
using winmd::reader::HasSemantics;
using winmd::reader::HasFieldMarshal;
using winmd::reader::TypeOrMethodDef;
using winmd::reader::coded_index;
using winmd::reader::category;
using winmd::reader::cache;

using winmd::reader::find;
using winmd::reader::find_required;
using winmd::reader::get_category;
using winmd::reader::get_type_namespace_and_name;
using winmd::reader::extends_type;
using winmd::reader::is_nested;

}  // namespace md
