#pragma once

#include "generated/Microsoft.UI.Xaml.h"

struct Settings;

namespace trayed {

// Builds the settings window, its controls two-way bound to `settings`. Returned
// unshown -- the caller activates it and owns its lifetime. There is no
// "onChanged" callback: the controls bind to the model's observables, and
// whoever wants to act on a change watches the observable itself.
wxl::Window buildSettingsWindow(Settings& settings);

}  // namespace trayed
