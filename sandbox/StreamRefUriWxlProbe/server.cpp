#include "platform.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Storage.Streams.h>

#include <chrono>
#include <format>
#include <future>
#include <memory>
#include <thread>

#include "server.h"

namespace wss = winrt::Windows::Storage::Streams;
namespace wgi = winrt::Windows::Graphics::Imaging;
namespace wns = winrt::Windows::Networking::Sockets;

namespace probe {

// Свой поток с MTA: блокирующие .get() на потоке окна (STA) не годятся, а делать эту работу больше негде.
std::vector<std::uint8_t> makeImage() {
    std::vector<std::uint8_t> bytes;
    std::thread worker([&bytes] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        constexpr std::uint32_t width = 320, height = 240;
        std::vector<std::uint8_t> pixels(width * height * 4);
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                auto* const pixel = &pixels[(y * width + x) * 4];
                pixel[0] = static_cast<std::uint8_t>(x * 255 / width);
                pixel[1] = static_cast<std::uint8_t>(y * 255 / height);
                pixel[2] = ((x / 16 + y / 16) & 1) ? 220 : 40;
                pixel[3] = 255;
            }
        }
        wss::InMemoryRandomAccessStream memory;
        auto const encoder = wgi::BitmapEncoder::CreateAsync(wgi::BitmapEncoder::JpegEncoderId(), memory).get();
        encoder.SetPixelData(wgi::BitmapPixelFormat::Bgra8, wgi::BitmapAlphaMode::Ignore, width, height, 96, 96, pixels);
        encoder.FlushAsync().get();
        bytes.resize(static_cast<std::size_t>(memory.Size()));
        wss::DataReader reader{memory.GetInputStreamAt(0)};
        reader.LoadAsync(static_cast<std::uint32_t>(bytes.size())).get();
        reader.ReadBytes(bytes);
        reader.DetachStream();
    });
    worker.join();
    return bytes;
}

std::uint16_t serve(std::vector<std::uint8_t> image) {
    auto const shared = std::make_shared<std::vector<std::uint8_t> const>(std::move(image));
    std::promise<std::uint16_t> port;
    auto ready = port.get_future();
    // Поток не кончается: пока в нём живёт MTA, слушатель и его обработчики работают; конец процесса -- конец сервера.
    std::thread([shared, &port] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        // Слушатель живёт до конца процесса и намеренно не освобождается: его уничтожение при выходе -- лишний шум.
        // new у типов проекции удалён, поэтому -- construct_at на своём месте.
        alignas(wns::StreamSocketListener) static unsigned char storage[sizeof(wns::StreamSocketListener)];
        auto* const listener = std::construct_at(reinterpret_cast<wns::StreamSocketListener*>(storage));
        listener->ConnectionReceived(
            [shared](wns::StreamSocketListener const&, wns::StreamSocketListenerConnectionReceivedEventArgs const& args) -> winrt::fire_and_forget {
                auto const socket = args.Socket();
                try {
                    wss::DataReader request{socket.InputStream()};
                    request.InputStreamOptions(wss::InputStreamOptions::Partial);
                    co_await request.LoadAsync(4096);  // что просили, неважно
                    request.DetachStream();
                    wss::DataWriter response{socket.OutputStream()};
                    response.WriteString(winrt::to_hstring(std::format(
                        "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: {}\r\nConnection: close\r\n\r\n", shared->size())));
                    response.WriteBytes(*shared);
                    co_await response.StoreAsync();
                    co_await response.FlushAsync();
                    response.DetachStream();
                } catch (...) {
                }
                socket.Close();
            });
        listener->BindEndpointAsync(winrt::Windows::Networking::HostName{L"127.0.0.1"}, L"0").get();
        port.set_value(static_cast<std::uint16_t>(std::stoi(winrt::to_string(listener->Information().LocalPort()))));
        for (;;) std::this_thread::sleep_for(std::chrono::hours(24));
    }).detach();
    return ready.get();
}

std::string describe(std::exception_ptr error) {
    try {
        std::rethrow_exception(error);
    } catch (winrt::hresult_error const& e) {
        auto message = winrt::to_string(e.message());
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' ')) message.pop_back();
        return std::format("0x{:08X} {}", static_cast<std::uint32_t>(e.code().value), message);
    } catch (std::exception const& e) {
        return std::string("std::exception: ") + e.what();
    } catch (...) {
        return "an exception of an unknown kind";
    }
}

}  // namespace probe
