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
// watch subscribes through, and the test raises the replacement. And the order of the
// replacement itself -- impl::rendering_devices, which DrawingSurface.cpp runs over the real
// devices -- is run over a backend that writes down what it is asked to make, move and let
// go. That a real loss reaches it -- the removal event, the pool's wait, the turn of the UI
// thread's queue -- that SetRenderingDevice takes the new device, that the device confirms
// the codes, and that BeginDraw on a lost device answers one of them, is seen on a machine
// (impl::signal_rendering_device_removed, a real TDR).
#include <crtdbg.h>

// The classification and the limit first: they reach windows.h, which has to come before
// the wxl.core import the list's and the watch's headers carry.
#include "impl/device_limits.h"
#include "impl/device_loss.h"

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "impl/device_replacement.h"
#include "impl/handler_list.h"
#include "impl/rendering_device.h"

namespace {

using wxl::EventHandler;
using wxl::EventToken;
using wxl::Object;
using wxl::impl::composition_api;
using wxl::impl::device_watch;
using wxl::impl::handler_list;
using wxl::impl::device_loss_code;
using wxl::impl::composition_padding;
using wxl::impl::max_update_side;
using wxl::impl::rendering_devices;

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

using log_t = std::vector<std::string>;

char const* name_of(composition_api api) {
    return api == composition_api::winui ? "winui" : "system";
}

/// The real things as a log: devices and graphics devices are numbers, and every call the
/// replacement makes is written down in the order it is made.
struct fake_backend {
    using owner_t = int;
    struct device_t {
        int id = 0;
        bool watched = false;
    };
    using graphics_t = int;

    log_t* log;
    int devices = 0;

    device_t make() {
        ++devices;
        log->push_back(std::format("make d{}", devices));
        return {devices, true};
    }

    void unwatch(device_t& device) noexcept {
        log->push_back(std::format("unwatch d{}", device.id));
        device.watched = false;
    }

    void release(device_t& device) noexcept { log->push_back(std::format("release d{}", device.id)); }

    void move(graphics_t graphics, composition_api api, device_t const& device) {
        log->push_back(std::format("move g{} {} to d{}", graphics, name_of(api), device.id));
    }

    void keep(owner_t owner) noexcept { log->push_back(std::format("keep o{}", owner)); }
};

using fake_devices = rendering_devices<fake_backend>;

/// The graphics device of compositor `owner`, made as the real one is: on the device handed in,
/// numbered after its compositor.
int graphics_of(fake_devices& all, int owner, composition_api api, log_t& log) {
    return all.graphics_of(owner, api, [&](fake_backend::device_t const& device) {
        log.push_back(std::format("graphics g{} on d{}", owner, device.id));
        return owner;
    });
}

/// A watch on compositor `owner` that writes down that it was called.
void watch(fake_devices& all, int owner, composition_api api, log_t& log) {
    static_cast<void>(all.handlers_of(owner, api).add(wxl::impl::next_event_token(), [&log, owner] {
        log.push_back(std::format("watch o{}", owner));
    }));
}

TEST(RenderingDevices, the_first_device_is_made_with_the_first_graphics_device) {
    log_t log;
    fake_devices all{fake_backend{&log}};
    EXPECT_EQ(all.current(), nullptr);

    EXPECT_EQ(graphics_of(all, 1, composition_api::winui, log), 1);
    EXPECT_EQ(graphics_of(all, 1, composition_api::winui, log), 1) << "made once";
    ASSERT_NE(all.current(), nullptr);
    EXPECT_EQ(all.current()->id, 1);
    EXPECT_EQ(log, (log_t{"keep o1", "make d1", "graphics g1 on d1"}));
}

// Every graphics device is moved onto the new device through its own API, and the old device is
// let go, before the first watch is called: what an owner draws from its call is on the new one.
TEST(RenderingDevices, a_replacement_moves_every_graphics_device_before_any_watch) {
    log_t log;
    fake_devices all{fake_backend{&log}};
    graphics_of(all, 1, composition_api::winui, log);
    graphics_of(all, 2, composition_api::system, log);
    watch(all, 1, composition_api::winui, log);
    watch(all, 2, composition_api::system, log);
    log.clear();

    all.replace();
    EXPECT_EQ(log, (log_t{"unwatch d1", "make d2", "move g1 winui to d2", "move g2 system to d2",
                          "release d1", "watch o1", "watch o2"}));
    EXPECT_EQ(all.current()->id, 2);
    EXPECT_TRUE(all.current()->watched);
}

// A watch put on before its compositor's first surface: called, and there is nothing to move.
TEST(RenderingDevices, a_watch_with_no_surface_yet_is_called_and_nothing_is_moved_for_it) {
    log_t log;
    fake_devices all{fake_backend{&log}};
    graphics_of(all, 1, composition_api::winui, log);
    watch(all, 2, composition_api::winui, log);
    log.clear();

    all.replace();
    EXPECT_EQ(log, (log_t{"unwatch d1", "make d2", "move g1 winui to d2", "release d1", "watch o2"}));
}

// A watch that draws on a compositor nobody drew on before: its graphics device is made on the
// new device, so it is not moved, and the watch put on it there is not called for this one.
TEST(RenderingDevices, what_a_watch_makes_from_its_call_is_on_the_new_device_already) {
    log_t log;
    fake_devices all{fake_backend{&log}};
    graphics_of(all, 1, composition_api::winui, log);
    bool drawn = false;
    static_cast<void>(all.handlers_of(1, composition_api::winui).add(wxl::impl::next_event_token(), [&] {
        log.push_back("watch o1");
        if (std::exchange(drawn, true)) return;
        graphics_of(all, 2, composition_api::winui, log);
        watch(all, 2, composition_api::winui, log);
    }));
    log.clear();

    all.replace();
    EXPECT_EQ(log, (log_t{"unwatch d1", "make d2", "move g1 winui to d2", "release d1", "watch o1",
                          "keep o2", "graphics g2 on d2"}));

    log.clear();
    all.replace();
    EXPECT_EQ(log, (log_t{"unwatch d2", "make d3", "move g1 winui to d3", "move g2 winui to d3",
                          "release d2", "watch o1", "watch o2"}));
}

// The handlers of a compositor: the same list whoever asks, and none for one nobody asked for --
// the answer a watch's remove reads.
TEST(RenderingDevices, a_compositor_nobody_asked_for_has_no_handlers) {
    log_t log;
    fake_devices all{fake_backend{&log}};
    EXPECT_EQ(all.find_handlers(1), nullptr);

    handler_list<Object>& handlers = all.handlers_of(1, composition_api::system);
    EXPECT_EQ(all.find_handlers(1), &handlers);
    EXPECT_EQ(&all.handlers_of(1, composition_api::system), &handlers);
    EXPECT_EQ(all.find_handlers(2), nullptr);
    EXPECT_EQ(log, (log_t{"keep o1"})) << "no device for a watch alone";
}

void ask_under_two_apis() {
    without_a_dialog();
    log_t log;
    fake_devices all{fake_backend{&log}};
    graphics_of(all, 1, composition_api::winui, log);
    static_cast<void>(all.handlers_of(1, composition_api::system));
}

TEST(RenderingDevicesDeathTest, one_compositor_under_two_apis_stops) {
    EXPECT_DEATH(ask_under_two_apis(), "one compositor is asked for under two composition APIs");
}

}  // namespace
