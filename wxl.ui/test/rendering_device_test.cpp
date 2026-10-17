// A lost rendering device, as far as it can be told without one.
//
// Which answers of the drawing calls name a lost device: those, confirmed by the device,
// skip the drawing, and everything else stays a failure. And the watch an owner keeps for the replacement: a
// member of its own, called each time the device under a compositor's surfaces is
// replaced, and taken off by the watch's end with the answer checked. And the longest side
// a device draws at once, by its feature level.
//
// No GPU, no compositor. The compositor is a fake: a handle to the handler list the real
// graphics device's entry keeps -- impl::handler_list itself -- with the pair of calls a
// watch subscribes through, and the test raises the replacement. That a real loss reaches
// that list -- the removal event, the pool's wait, the turn of the UI thread's queue,
// SetRenderingDevice to every graphics device, then the watches -- that the device confirms
// the codes, and that BeginDraw on a lost device answers one of them, is seen on a machine
// (impl::signal_rendering_device_removed, a real TDR).
#include <crtdbg.h>

// The classification and the limit first: they reach windows.h, which has to come before
// the wxl.core import the list's and the watch's headers carry.
#include "impl/device_limits.h"
#include "impl/device_loss.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <type_traits>

#include "impl/handler_list.h"
#include "impl/rendering_device.h"

namespace {

using wxl::EventHandler;
using wxl::EventToken;
using wxl::Object;
using wxl::impl::device_watch;
using wxl::impl::handler_list;
using wxl::impl::device_loss_code;
using wxl::impl::composition_padding;
using wxl::impl::max_update_side;

// The codes are the SDK's, and their values the documented ones: a header that spelled
// one differently would be seen here, not on a machine whose GPU was just reset.
static_assert(static_cast<std::uint32_t>(DXGI_ERROR_DEVICE_REMOVED) == 0x887A0005u);
static_assert(static_cast<std::uint32_t>(DXGI_ERROR_DEVICE_RESET) == 0x887A0007u);
static_assert(static_cast<std::uint32_t>(D2DERR_RECREATE_TARGET) == 0x8899000Cu);

TEST(DeviceLossCode, the_codes_of_a_lost_device) {
    EXPECT_TRUE(device_loss_code(DXGI_ERROR_DEVICE_REMOVED));
    EXPECT_TRUE(device_loss_code(DXGI_ERROR_DEVICE_RESET));
    EXPECT_TRUE(device_loss_code(D2DERR_RECREATE_TARGET));
}

// Everything else a drawing call says is not a lost device, and is not skipped: a bug of
// the application's (BeginDraw on a second surface before EndDraw on the first is
// DXGI_ERROR_INVALID_CALL or E_INVALIDARG; a bitmap of another device drawn is
// D2DERR_WRONG_RESOURCE_DOMAIN), the system's want of memory, success, and the codes that
// are not in the list.
TEST(DeviceLossCode, everything_else_is_not) {
    EXPECT_FALSE(device_loss_code(S_OK));
    EXPECT_FALSE(device_loss_code(S_FALSE));
    EXPECT_FALSE(device_loss_code(E_FAIL));
    EXPECT_FALSE(device_loss_code(E_INVALIDARG));
    EXPECT_FALSE(device_loss_code(E_OUTOFMEMORY));
    EXPECT_FALSE(device_loss_code(E_ACCESSDENIED));
    EXPECT_FALSE(device_loss_code(DXGI_ERROR_INVALID_CALL));
    EXPECT_FALSE(device_loss_code(DXGI_ERROR_WAS_STILL_DRAWING));
    EXPECT_FALSE(device_loss_code(DXGI_ERROR_DEVICE_HUNG));
    EXPECT_FALSE(device_loss_code(DXGI_ERROR_DRIVER_INTERNAL_ERROR));
    EXPECT_FALSE(device_loss_code(D2DERR_WRONG_RESOURCE_DOMAIN));
}

// The longest side one update of a surface may have, by the device's feature level -- the
// column Max Texture Dimension of the table of Direct3D feature levels less the three pixels
// composition pads an allocation with, each level and both ends of each step: a level above
// the last one known still draws the most.
static_assert(composition_padding == 3);
static_assert(max_update_side(D3D_FEATURE_LEVEL_9_1) == 2045);
static_assert(max_update_side(D3D_FEATURE_LEVEL_9_2) == 2045);
static_assert(max_update_side(D3D_FEATURE_LEVEL_9_3) == 4093);
static_assert(max_update_side(D3D_FEATURE_LEVEL_10_0) == 8189);
static_assert(max_update_side(D3D_FEATURE_LEVEL_10_1) == 8189);
static_assert(max_update_side(D3D_FEATURE_LEVEL_11_0) == 16381);
static_assert(max_update_side(D3D_FEATURE_LEVEL_11_1) == 16381);
static_assert(max_update_side(D3D_FEATURE_LEVEL_12_0) == 16381);
static_assert(max_update_side(D3D_FEATURE_LEVEL_12_1) == 16381);
static_assert(max_update_side(static_cast<D3D_FEATURE_LEVEL>(0xc200)) == 16381);

/// A compositor as a watch has it: a handle, copied by value, to the handlers of the
/// replacement of its graphics device.
struct fake_compositor {
    std::shared_ptr<handler_list<Object>> replaced = std::make_shared<handler_list<Object>>();
};

/// The pair of calls, as impl::rendering_device_replaced has it.
struct fake_replaced {
    static EventToken add(fake_compositor const& compositor, EventHandler<Object> handler) {
        return compositor.replaced->add(wxl::impl::next_event_token(), std::move(handler));
    }

    static bool remove(fake_compositor const& compositor, EventToken token) {
        return compositor.replaced->remove(token);
    }
};

void replace(fake_compositor const& compositor) {
    compositor.replaced->fire(Object::Impl::empty<Object>());
}

/// An owner as an application has one: it keeps the watch as a member and hears the
/// replacement through a noexcept member that takes nothing.
struct Page {
    int redraws = 0;

    void deviceReplaced() noexcept { ++redraws; }
};

using replaced_watch = device_watch<fake_compositor, fake_replaced, &Page::deviceReplaced>;

// The handler calls the owner the watch was put on with, so there is nowhere to copy or move
// the watch to.
static_assert(!std::is_copy_constructible_v<replaced_watch>);
static_assert(!std::is_move_constructible_v<replaced_watch>);
static_assert(!std::is_copy_assignable_v<replaced_watch>);
static_assert(!std::is_move_assignable_v<replaced_watch>);

TEST(RenderingDeviceWatch, calls_its_owner_each_time_the_device_is_replaced) {
    fake_compositor compositor;
    Page page;
    replaced_watch watch{compositor, page};
    EXPECT_TRUE(watch.attached());

    replace(compositor);
    replace(compositor);
    EXPECT_EQ(page.redraws, 2);
}

TEST(RenderingDeviceWatch, starts_empty_and_is_attached_later) {
    fake_compositor compositor;
    Page page;
    replaced_watch watch;
    EXPECT_FALSE(watch.attached());

    replace(compositor);
    EXPECT_EQ(page.redraws, 0);

    watch.attach(compositor, page);
    replace(compositor);
    EXPECT_EQ(page.redraws, 1);
}

TEST(RenderingDeviceWatch, an_empty_watch_goes_quietly) {
    { replaced_watch watch; }
    SUCCEED();
}

// Each owner of surfaces on one compositor hears the replacement; one gone, the other goes
// on hearing it.
TEST(RenderingDeviceWatch, comes_off_when_it_goes) {
    fake_compositor compositor;
    Page staying;
    Page leaving;
    replaced_watch stays{compositor, staying};
    {
        replaced_watch leaves{compositor, leaving};
        replace(compositor);
    }
    replace(compositor);
    EXPECT_EQ(staying.redraws, 2);
    EXPECT_EQ(leaving.redraws, 1);
}

// The watch of one compositor is not called for another's device.
TEST(RenderingDeviceWatch, hears_only_its_compositor) {
    fake_compositor scene;
    fake_compositor island;
    Page page;
    replaced_watch watch{scene, page};

    replace(island);
    EXPECT_EQ(page.redraws, 0);
    replace(scene);
    EXPECT_EQ(page.redraws, 1);
}

// An owner whose listener lets go of the watch -- a popup that closes itself on the
// replacement: the handler comes off inside its own call, and answers.
struct SelfRemoving {
    int redraws = 0;

    void deviceReplaced() noexcept {
        ++redraws;
        watch.reset();
    }

    std::optional<device_watch<fake_compositor, fake_replaced, &SelfRemoving::deviceReplaced>> watch;
};

TEST(RenderingDeviceWatch, goes_from_inside_its_own_call) {
    fake_compositor compositor;
    SelfRemoving owner;
    owner.watch.emplace(compositor, owner);

    replace(compositor);
    EXPECT_FALSE(owner.watch.has_value());
    replace(compositor);
    EXPECT_EQ(owner.redraws, 1);
}

void without_a_dialog() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void attach_twice() {
    without_a_dialog();
    fake_compositor first;
    fake_compositor second;
    Page page;
    replaced_watch watch{first, page};
    watch.attach(second, page);
}

TEST(RenderingDeviceWatchDeathTest, attaching_twice_stops) {
    EXPECT_DEATH(attach_twice(), "a RenderingDeviceWatch is attached twice");
}

// The token is the watch's: whoever takes the handler off by it takes away what the watch's
// destructor counts on. The list's last token is the watch's: nothing else was added.
void take_off_by_someone_else() {
    without_a_dialog();
    fake_compositor compositor;
    Page page;
    replaced_watch watch{compositor, page};
    EventToken const theirs = fake_replaced::add(compositor, [] {});
    static_cast<void>(compositor.replaced->remove(EventToken{theirs.value - 1}));
}

TEST(RenderingDeviceWatchDeathTest, taken_off_by_someone_else_stops) {
    EXPECT_DEATH(take_off_by_someone_else(), "a RenderingDeviceWatch is no longer on its compositor");
}

}  // namespace
