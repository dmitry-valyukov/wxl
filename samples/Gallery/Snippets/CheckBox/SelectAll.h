// The box and its three options are one panel: a handler reads the others
// from the panel it is in, so no handler captures an element.
auto const boxes = [](CheckBox const& self) { return self.parent().try_as<StackPanel>().children(); };

// An option changed: the "select all" box shows all, none or in between.
auto const refresh = [boxes](CheckBox const& option) {
    auto const group = boxes(option);
    int checked = 0;
    for (uint32_t i = 1; i < group.size(); ++i) {
        checked += group[i].try_as<CheckBox>().isChecked().value_or(false) ? 1 : 0;
    }
    auto const all = group[0].try_as<CheckBox>();
    if (checked == 3) {
        all.isChecked(true);
    } else if (checked == 0) {
        all.isChecked(false);
    } else {
        all.isChecked(core::nullable<bool>{});
    }
};

// "Select all" changed: every option follows it.
auto const follow = [boxes](CheckBox const& all, bool on) {
    auto const group = boxes(all);
    for (uint32_t i = 1; i < group.size(); ++i) {
        group[i].try_as<CheckBox>().isChecked(on);
    }
};

auto options = StackPanel {
    CheckBox {
        content = u"Select all",
        isThreeState = true,
        onChecked = [follow](CheckBox const& self) { follow(self, true); },
        onUnchecked = [follow](CheckBox const& self) { follow(self, false); },
        // Only a program sets the indeterminate state, never the user: a click
        // on a fully checked box lands here, and means "uncheck all".
        onIndeterminate = [boxes](CheckBox const& self) {
            auto const group = boxes(self);
            bool everything = true;
            for (uint32_t i = 1; i < group.size(); ++i) {
                everything = everything && group[i].try_as<CheckBox>().isChecked().value_or(false);
            }
            if (everything) {
                self.isChecked(false);
            }
        },
    },
    CheckBox {
        Margin {24, 0, 0, 0},
        content = u"Option 1",
        onLoaded = [refresh](CheckBox const& self) { refresh(self); },
        onChecked = [refresh](CheckBox const& self) { refresh(self); },
        onUnchecked = [refresh](CheckBox const& self) { refresh(self); },
    },
    CheckBox {
        Margin {24, 0, 0, 0},
        content = u"Option 2",
        isChecked = true,
        onChecked = [refresh](CheckBox const& self) { refresh(self); },
        onUnchecked = [refresh](CheckBox const& self) { refresh(self); },
    },
    CheckBox {
        Margin {24, 0, 0, 0},
        content = u"Option 3",
        onChecked = [refresh](CheckBox const& self) { refresh(self); },
        onUnchecked = [refresh](CheckBox const& self) { refresh(self); },
    },
};