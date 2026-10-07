// Карты нормалей проб стенда -- рельефы страницы материала, конус и наклон
// страницы матрицы: считаются здесь по формуле и пишутся в поверхность
// композитора одним битмапом. Карты клавиши рисует сама библиотека
// (impl/key_relief.h).

#include "platform.h"

#include <d2d1_1.h>
#include <wrl/client.h>

#include "Lab.h"

using namespace wxl;
using Microsoft::WRL::ComPtr;

namespace {

struct Normal {
    double x, y, z;
};

Normal unit(double x, double y, double z) {
    double const length = std::sqrt(x * x + y * y + z * z);
    return {x / length, y / length, z / length};
}

// Нормаль в точке клавиши. Точка -- от центра, в долях стороны; оси экрана:
// x вправо, y вниз, z к зрителю.
Normal normalAt(lab::Relief relief, lab::ReliefShape const& shape, double px, double py) {
    using lab::Relief;

    if (relief == Relief::flat) {
        return {0.0, 0.0, 1.0};
    }

    if (relief == Relief::sphere) {
        double const x = px / 0.5, y = py / 0.5, squared = x * x + y * y;
        if (squared >= 1.0) {
            return {0.0, 0.0, 1.0};
        }
        return {x, y, std::sqrt(1.0 - squared)};
    }

    // Купол и чаша -- пологий шаровой сегмент на всё лицо: наклон растёт от
    // центра к краю линейно и у середины стороны равен depth.
    if (relief == Relief::dome || relief == Relief::dish) {
        double const sign = relief == Relief::dome ? 1.0 : -1.0;
        return unit(sign * 2.0 * px * shape.depth, sign * 2.0 * py * shape.depth, 1.0);
    }

    // Подушка и фаска: плоский верх и плечо шириной shoulder вдоль края
    // скруглённого прямоугольника. inside -- расстояние от края внутрь.
    double const half = 0.5;
    double const radius = std::clamp(shape.corner, 0.0, half);
    double const qx = std::abs(px) - (half - radius), qy = std::abs(py) - (half - radius);
    double const ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);
    double const inside = radius - std::sqrt(ox * ox + oy * oy) - std::min(std::max(qx, qy), 0.0);
    if (inside <= 0.0 || shape.shoulder <= 0.0 || inside >= shape.shoulder) {
        return {0.0, 0.0, 1.0};
    }

    // Куда смотрит край в этой точке: наружу от клавиши.
    double gx = 0.0, gy = 0.0;
    if (qx > 0.0 && qy > 0.0) {
        double const length = std::sqrt(qx * qx + qy * qy);
        gx = qx / length;
        gy = qy / length;
    } else if (qx > qy) {
        gx = 1.0;
    } else {
        gy = 1.0;
    }
    gx = std::copysign(gx, px);
    gy = std::copysign(gy, py);

    double slope = shape.depth;
    if (relief == Relief::cushion) {
        // Плечо -- четверть круга: наклон нулевой у верха и отвесный у края.
        double const t = 1.0 - inside / shape.shoulder;
        slope = std::clamp(shape.depth * t / std::sqrt(std::max(1.0 - t * t, 1e-4)), -8.0, 8.0);
    }
    return unit(gx * slope, gy * slope, 1.0);
}

void blit(DrawingSurface const& surface, std::vector<std::uint32_t> const& pixels, int width, int height) {
    surface.draw([&](ID2D1DeviceContext* context) {
        D2D1_BITMAP_PROPERTIES1 const properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ComPtr<ID2D1Bitmap1> bitmap;
        wxl::check_hresult(context->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)),
                                                 pixels.data(), static_cast<UINT32>(width) * 4, &properties,
                                                 &bitmap));
        // Копией и без сглаживания: в пикселе число, а не цвет.
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        context->DrawBitmap(bitmap.Get(), D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
                            1.0f, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    });
}

}  // namespace

void lab::drawNormalMap(DrawingSurface const& surface, Relief relief, ReliefShape const& shape) {
    SizeInt32 const size = surface.size();
    int const width = size.width, height = size.height;
    if (width <= 0 || height <= 0) {
        return;
    }

    auto const channel = [](double value) {
        return static_cast<std::uint32_t>(std::lround((value * 0.5 + 0.5) * 255.0));
    };

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Normal normal = normalAt(relief, shape, (x + 0.5) / width - 0.5, (y + 0.5) / height - 0.5);
            if (shape.yUp) {
                normal.y = -normal.y;
            }
            // B8G8R8A8: в слове старший байт -- альфа, младший -- синий.
            pixels[static_cast<std::size_t>(y) * width + x] =
                0xFF000000u | (channel(normal.x) << 16) | (channel(normal.y) << 8) | channel(normal.z);
        }
    }
    blit(surface, pixels, width, height);
}

namespace {

// Пишет карту, нормаль каждой точки которой даёт функция от места в долях
// поверхности, от середины; оси экрана. Зелёный канал растёт вверх экрана.
template <class NormalOf>
void drawMap(DrawingSurface const& surface, NormalOf normalOf) {
    SizeInt32 const size = surface.size();
    int const width = size.width, height = size.height;
    if (width <= 0 || height <= 0) {
        return;
    }
    auto const channel = [](double value) {
        return static_cast<std::uint32_t>(std::lround((value * 0.5 + 0.5) * 255.0));
    };
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Normal const normal = normalOf((x + 0.5) / width - 0.5, (y + 0.5) / height - 0.5);
            pixels[static_cast<std::size_t>(y) * width + x] =
                0xFF000000u | (channel(normal.x) << 16) | (channel(-normal.y) << 8) | channel(normal.z);
        }
    }
    blit(surface, pixels, width, height);
}

}  // namespace

void lab::drawConeMap(DrawingSurface const& surface, double slope) {
    drawMap(surface, [slope](double u, double v) {
        double const distance = std::sqrt(u * u + v * v);
        if (distance < 1e-6 || distance > 0.45) {
            return Normal {0.0, 0.0, 1.0};
        }
        return unit(u / distance * slope, v / distance * slope, 1.0);
    });
}

void lab::drawTiltMap(DrawingSurface const& surface, double slopeRight, double slopeUp) {
    drawMap(surface, [=](double, double) { return unit(slopeRight, -slopeUp, 1.0); });
}

void lab::drawSolid(DrawingSurface const& surface, Color color) {
    SizeInt32 const size = surface.size();
    surface.draw([&](ID2D1DeviceContext* context) {
        ComPtr<ID2D1SolidColorBrush> fill;
        wxl::check_hresult(context->CreateSolidColorBrush(
            D2D1::ColorF(color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f), &fill));
        context->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height)),
                               fill.Get());
    });
}
