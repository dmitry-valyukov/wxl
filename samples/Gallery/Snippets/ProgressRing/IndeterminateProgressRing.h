// The ring spins while it is active; the switch is the only thing that writes.
struct Model {
    core::observable<bool> active{true};
};
auto const model = gallery::hold<Model>();

auto ring = ProgressRing {
    automationName = u"Progress image",
    isActive = BindOutput {model->active},
    Margin {10, 10, 0, 0},
};

auto activeSwitch = ToggleSwitch {
    automationName = u"Progress Options",
    header = u"Active",
    onContent = u"On",
    offContent = u"Off",
    isOn = Bind {model->active},
};
