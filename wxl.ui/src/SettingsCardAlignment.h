#pragma once

// Where the setting of a SettingsCard stands: right of the text, left of it, or under it and as wide as the card.
// A header of its own, because the DSL tag `contentAlignment` names the type in Members.h, which a class that includes
// Members.h could not provide.

#include <cstdint>

namespace wxl {

enum class SettingsCardContentAlignment : int32_t {
    Right,
    Left,
    Vertical,
};

}  // namespace wxl
