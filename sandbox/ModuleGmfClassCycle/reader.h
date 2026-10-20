#pragma once

// Та часть устройства winmd_reader.h, о которой вопрос, и больше ничего:
// класс, объявленный вперёд (enum_traits.h), определённый позже на CRTP-базе
// (schema.h) с методом, чей тип возврата выводится; второй класс, держащий
// первый по значению (key.h); и тело того метода — после второго класса.
//
// Ручки вариантов:
//   PROBE_NO_FORWARD         без объявления вперёд
//   PROBE_NO_CRTP            без CRTP-базы
//   PROBE_EXPLICIT_RETURN    тип возврата назван, а не выведен

namespace reader {

#ifndef PROBE_NO_FORWARD
struct TypeDef;
#endif

#ifdef PROBE_EXPLICIT_RETURN
struct EnumDefinition;
#endif

template <typename T>
struct row_base {
    unsigned index = 0;
};

#ifdef PROBE_NO_CRTP
struct TypeDef {
    unsigned index = 0;
#else
struct TypeDef : row_base<TypeDef> {
#endif

#ifdef PROBE_EXPLICIT_RETURN
    EnumDefinition get_enum_definition() const;
#else
    auto get_enum_definition() const;
#endif
};

struct EnumDefinition {
    explicit EnumDefinition(TypeDef const& type) : m_typedef(type) {}

    TypeDef m_typedef;
};

#ifdef PROBE_EXPLICIT_RETURN
inline EnumDefinition TypeDef::get_enum_definition() const {
#else
inline auto TypeDef::get_enum_definition() const {
#endif
    return EnumDefinition{*this};
}

}  // namespace reader
