// Проба движка композиции ОС.
//
// Windows App SDK 2.3 добавил CompositionEngine::TrySetProcessEngine: движок
// композиции ОС (System) вместо движка в процессе (InProcess, по умолчанию) для
// тех же API Microsoft.UI.Composition. Документация: «unstable Limited Access
// Feature»; вызывать до первого объекта композиции, потом не сменить. Из
// wxl_launched вызов вернул false -- там XAML уже поднят. Здесь -- раньше
// всего: рантайм, затем сразу TrySetProcessEngine, затем композитор на
// потоке с DispatcherQueue.

#include "platform.h"

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>

#include <cstdio>

#include <MddBootstrap.h>

namespace muc = winrt::Microsoft::UI::Composition;

namespace {

void trySet(muc::CompositionEngineType type, char const* name) {
    try {
        bool const set = muc::CompositionEngine::TrySetProcessEngine(type);
        std::printf("TrySetProcessEngine(%s): %d\n", name, set ? 1 : 0);
    } catch (winrt::hresult_error const& error) {
        std::printf("TrySetProcessEngine(%s) threw %08x %s\n", name, static_cast<unsigned>(error.code().value),
                    winrt::to_string(error.message()).c_str());
    }
}

}  // namespace

int main() {
    // Как wxl::impl::ensure_windows_app_runtime_initialized: «release 2».
    HRESULT const bootstrap = ::MddBootstrapInitialize(0x00020000, nullptr, {});
    std::printf("MddBootstrapInitialize: %08x\n", static_cast<unsigned>(bootstrap));
    if (FAILED(bootstrap)) return 1;
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    std::printf("runtime initialized, no composition object yet\n");

    trySet(muc::CompositionEngineType::System, "System");
    trySet(muc::CompositionEngineType::System, "System, again");
    trySet(muc::CompositionEngineType::InProcess, "InProcess");

    auto const queue = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();
    try {
        muc::Compositor const compositor;
        auto const visual = compositor.CreateSpriteVisual();
        std::printf("Compositor and SpriteVisual created\n");
        try {
            auto const system = muc::CompositionEngine::GetForSystemEngine(visual);
            std::printf("GetForSystemEngine(visual): %s\n", system ? "object" : "null");
        } catch (winrt::hresult_error const& error) {
            std::printf("GetForSystemEngine threw %08x %s\n", static_cast<unsigned>(error.code().value),
                        winrt::to_string(error.message()).c_str());
        }
        try {
            auto const inproc = muc::CompositionEngine::GetForInProcessEngine(visual);
            std::printf("GetForInProcessEngine(visual): %s\n", inproc ? "object" : "null");
        } catch (winrt::hresult_error const& error) {
            std::printf("GetForInProcessEngine threw %08x %s\n", static_cast<unsigned>(error.code().value),
                        winrt::to_string(error.message()).c_str());
        }
    } catch (winrt::hresult_error const& error) {
        std::printf("Compositor threw %08x %s\n", static_cast<unsigned>(error.code().value),
                    winrt::to_string(error.message()).c_str());
    }
    queue.ShutdownQueue();
    return 0;
}
