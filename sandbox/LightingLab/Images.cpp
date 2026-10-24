// Картинка из папки Assets в поверхность композитора: файл разбирает WIC,
// пиксели кладёт Direct2D. Так же поступает загрузчик образцов Microsoft --
// рисует разобранную картинку в CompositionDrawingSurface.

#include "platform.h"

#include <d2d1_1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>

#include "Lab.h"

using namespace wxl;
using Microsoft::WRL::ComPtr;

namespace {

// Папка Assets лежит рядом с исполняемым файлом.
std::wstring assetPath(wchar_t const* name) {
    wchar_t module[MAX_PATH] {};
    DWORD const length = ::GetModuleFileNameW(nullptr, module, MAX_PATH);
    std::wstring path(module, length);
    path.erase(path.find_last_of(L"\\/") + 1);
    path += L"Assets\\NormalMaps\\";
    path += name;
    return path;
}

// Первый кадр файла в формате поверхности: BGRA с умноженной альфой.
ComPtr<IWICFormatConverter> decoded(wchar_t const* name) {
    ComPtr<IWICImagingFactory> factory;
    wxl::impl::check_hresult(
        ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

    ComPtr<IWICBitmapDecoder> decoder;
    wxl::impl::check_hresult(factory->CreateDecoderFromFilename(assetPath(name).c_str(), nullptr, GENERIC_READ,
                                                          WICDecodeMetadataCacheOnLoad, &decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    wxl::impl::check_hresult(decoder->GetFrame(0, &frame));

    ComPtr<IWICFormatConverter> converter;
    wxl::impl::check_hresult(factory->CreateFormatConverter(&converter));
    wxl::impl::check_hresult(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                             nullptr, 0.0, WICBitmapPaletteTypeCustom));
    return converter;
}

}  // namespace

lab::Pixels lab::readImage(wchar_t const* name) {
    auto const converter = decoded(name);
    UINT width = 0, height = 0;
    wxl::impl::check_hresult(converter->GetSize(&width, &height));

    Pixels image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    wxl::impl::check_hresult(converter->CopyPixels(nullptr, width * 4, width * height * 4,
                                             reinterpret_cast<BYTE*>(image.pixels.data())));
    return image;
}

void lab::loadImage(DrawingSurface& surface, wchar_t const* name) {
    auto const converter = decoded(name);

    UINT width = 0, height = 0;
    wxl::impl::check_hresult(converter->GetSize(&width, &height));
    surface.resize({static_cast<int>(width), static_cast<int>(height)});

    surface.draw([&](ID2D1DeviceContext* context) {
        ComPtr<ID2D1Bitmap1> bitmap;
        wxl::impl::check_hresult(context->CreateBitmapFromWicBitmap(converter.Get(), &bitmap));
        // Копией и без сглаживания: в пикселе число, а не цвет.
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        context->DrawBitmap(bitmap.Get(),
                            D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)), 1.0f,
                            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    });
}
