module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

auto_sync_root_holder::auto_sync_root_holder(nullable<sync_root> root)
    // Два разных случая, и различает их только пустота. Свой, только что
    // созданный sync_root рождается со ссылкой, и её мы принимаем. Чужой,
    // переданный снаружи, принадлежит кому-то ещё, и на него надо завести свою
    // ссылку -- иначе мы отбираем чужую и объект умирает раньше своего владельца.
    : base(root ? sync_root_holder(*root) : sync_root_holder(new sync_root())) {}

auto_sync_root_holder::auto_sync_root_holder(not_null<sync_root> root) : base(root) {}

auto_sync_root_holder::auto_sync_root_holder(const sync_root_holder& root) : base(root) {}

auto_sync_root_holder::~auto_sync_root_holder() = default;

synchronized_::synchronized_(nullable<core::sync_root> root) : sync_root_(root) {}

synchronized_::synchronized_(not_null<core::sync_root> root) : sync_root_(root) {}

synchronized_::synchronized_(const sync_root_holder& root) : sync_root_(root) {}

synchronized_::~synchronized_() = default;

}  // namespace wxl::core
