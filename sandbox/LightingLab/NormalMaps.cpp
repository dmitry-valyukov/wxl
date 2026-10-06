// Карты нормалей клавиши, маски формы и тень края отверстия: считаются здесь
// по формуле формы и пишутся в поверхность композитора одним битмапом.

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

// Край скруглённого прямоугольника из точки, отсчитанной от его середины:
// расстояние от края внутрь и куда край в этом месте смотрит -- наружу; оси
// экрана.
struct Edge {
    double inside;
    double gx, gy;
};

Edge edgeOf(double halfWidth, double halfHeight, double corner, double px, double py) {
    double const radius = std::clamp(corner, 0.0, std::min(halfWidth, halfHeight));
    double const qx = std::abs(px) - (halfWidth - radius), qy = std::abs(py) - (halfHeight - radius);
    double const ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);

    Edge edge {radius - std::sqrt(ox * ox + oy * oy) - std::min(std::max(qx, qy), 0.0), 0.0, 0.0};
    if (qx > 0.0 && qy > 0.0) {
        double const length = std::sqrt(qx * qx + qy * qy);
        edge.gx = qx / length;
        edge.gy = qy / length;
    } else if (qx > qy) {
        edge.gx = 1.0;
    } else {
        edge.gy = 1.0;
    }
    edge.gx = std::copysign(edge.gx, px);
    edge.gy = std::copysign(edge.gy, py);
    return edge;
}

// Край клавиши из точки, отсчитанной от её середины в DIP.
Edge edgeAt(lab::KeyRelief const& key, double px, double py) {
    return edgeOf(key.width / 2.0, key.height / 2.0, key.corner, px, py);
}

// Глубина точки в лице: расстояние от внутреннего края канта внутрь, меньше
// нуля на самом канте. Пока внутренняя часть не сдвинута, это расстояние от
// края клавиши за вычетом ширины канта. Сдвинутая, она меряется от своей формы
// -- формы клавиши, уменьшенной на ширину канта и смещённой: внешний край канта
// стоит, и кант с одной стороны становится шире, с другой уже.
double faceDepth(lab::KeyRelief const& key, Edge const& outer, double px, double py) {
    if (key.shiftX == 0.0 && key.shiftY == 0.0) {
        return outer.inside - key.rim;
    }
    return edgeOf(std::max(key.width / 2.0 - key.rim, 0.0), std::max(key.height / 2.0 - key.rim, 0.0),
                  std::max(key.corner - key.rim, 0.0), px - key.shiftX, py - key.shiftY)
        .inside;
}

// Доля лица в точке: ноль на канте, единица на лице, между ними полоса
// шириной width вокруг стыка.
double faceShare(double depth, double width) {
    if (width <= 0.0) {
        return depth >= 0.0 ? 1.0 : 0.0;
    }
    return std::clamp(depth / width + 0.5, 0.0, 1.0);
}

// Нормаль клавиши в точке, отсчитанной от её середины в DIP; оси экрана.
Normal keyNormalAt(lab::KeyRelief const& key, double px, double py) {
    // Лицо -- шаровой сегмент одной кривизны по обеим осям: наклон растёт от
    // середины линейно и у края длинной стороны при полном emboss равен 0,6
    // (около 31 градуса). Купол клонит нормаль от середины, чаша -- к ней.
    // Середина лица сдвигается вместе со всей внутренней частью.
    double const reach = std::max(std::max(key.width, key.height) / 2.0 - key.rim, 1e-6);
    double const steepness = key.emboss * 0.6 / reach;
    double const faceX = (px - key.shiftX) * steepness, faceY = (py - key.shiftY) * steepness;
    if (key.rim <= 0.0) {
        return unit(faceX, faceY, 1.0);
    }

    // Кант продолжается и за краем формы: край там сглаживает маска, и под её
    // полупрозрачными пикселями должна лежать нормаль канта, а не плоскость --
    // иначе под лампой над указателем они светлее самого канта.
    Edge const edge = edgeAt(key, px, py);
    double const depth = faceDepth(key, edge, px, py);
    double slope = key.slope;
    if (key.rounded) {
        // Четверть круга: у лица кант пологий, у края отвесный. Ширина канта в
        // этом месте -- от края клавиши до внутреннего края канта.
        double const width = std::max(edge.inside - depth, 1e-6);
        double const t = std::clamp(1.0 - edge.inside / width, 0.0, 1.0);
        slope = std::clamp(key.slope * t / std::sqrt(std::max(1.0 - t * t, 1e-4)), -8.0, 8.0);
    }
    double const rimX = edge.gx * slope, rimY = edge.gy * slope;

    // В полосе стыка смешиваются наклоны поверхности, а не нормали: так выглядит
    // скруглённое ребро между кантом и лицом.
    double const share = faceShare(depth, key.smooth);
    return unit(rimX + (faceX - rimX) * share, rimY + (faceY - rimY) * share, 1.0);
}

// Высота точки клавиши над внешним краем канта: кант поднимается от края по
// своему профилю до внутреннего края, лицо стоит на его высоте. Та же ширина
// канта в этом месте, что и у нормали.
double keyHeightAt(lab::KeyRelief const& key, Edge const& edge, double depth) {
    if (key.rim <= 0.0) {
        return 0.0;
    }
    double const width = std::max(edge.inside - depth, 1e-6);
    double const climb = std::clamp(edge.inside, 0.0, width);
    if (key.rounded) {
        double const t = 1.0 - climb / width;
        return key.slope * width * std::sqrt(std::max(1.0 - t * t, 0.0));
    }
    return key.slope * climb;
}

}  // namespace

void lab::drawHoleShadow(DrawingSurface const& surface, KeyRelief const& key, HoleShadow const& shadow) {
    SizeInt32 const size = surface.size();
    int const width = size.width, height = size.height;
    if (width <= 0 || height <= 0) {
        return;
    }

    double const a = shadow.azimuth * 3.14159265358979 / 180.0;
    double const e = shadow.elevation * 3.14159265358979 / 180.0;
    // Свет идёт по экрану от лампы и за единицу спуска проходит cot e; к лампе
    // -- обратно. Общий свет и свет лампы на плоском месте -- как у стенда.
    double const run = std::cos(e) / std::max(std::sin(e), 1e-3);
    double const towardX = std::cos(a), towardY = -std::sin(a);
    Normal const toLamp = unit(towardX * std::cos(e), towardY * std::cos(e), std::sin(e));
    double const ambient = 1.0 - shadow.share;
    double const lamp = shadow.share / std::max(std::sin(e), 1e-3);
    // Полутень не уже пикселя поверхности, иначе край тени ступенчатый.
    double const soft = std::max(shadow.soft, key.width / width);

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double const px = ((x + 0.5) / width - 0.5) * key.width, py = ((y + 0.5) / height - 0.5) * key.height;
            Edge const edge = edgeAt(key, px, py);
            double const depth = faceDepth(key, edge, px, py);
            // Точка над панелью края не видит.
            double const below = shadow.depth - keyHeightAt(key, edge, depth);
            if (below <= 0.0) {
                continue;
            }
            // Где луч к этой точке прошёл плоскость панели: в тени, если вне
            // отверстия, а отверстие -- след клавиши.
            Edge const opening = edgeAt(key, px + towardX * run * below, py + towardY * run * below);
            double const cover = std::clamp(0.5 - opening.inside / soft, 0.0, 1.0);
            if (cover <= 0.0) {
                continue;
            }
            // Тень снимает долю лампы в свете этой точки, остальное -- общий свет.
            Normal const normal = keyNormalAt(key, px, py);
            double const lit = lamp * std::max(normal.x * toLamp.x + normal.y * toLamp.y + normal.z * toLamp.z, 0.0);
            double const alpha = cover * shadow.amount * lit / std::max(ambient + lit, 1e-6);
            pixels[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint32_t>(std::lround(std::clamp(alpha, 0.0, 1.0) * 255.0)) << 24;
        }
    }
    blit(surface, pixels, width, height);
}

void lab::drawKeyMap(DrawingSurface const& surface, KeyRelief const& key) {
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
            Normal const normal = keyNormalAt(key, ((x + 0.5) / width - 0.5) * key.width,
                                              ((y + 0.5) / height - 0.5) * key.height);
            // Зелёный -- вверх экрана: y оси экрана меняет знак.
            pixels[static_cast<std::size_t>(y) * width + x] =
                0xFF000000u | (channel(normal.x) << 16) | (channel(-normal.y) << 8) | channel(normal.z);
        }
    }
    blit(surface, pixels, width, height);
}

void lab::drawFaceMask(DrawingSurface const& surface, KeyRelief const& key) {
    SizeInt32 const size = surface.size();
    int const width = size.width, height = size.height;
    if (width <= 0 || height <= 0) {
        return;
    }

    // Не уже пикселя поверхности: без этого край маски ступенчатый.
    double const band = std::max(key.smooth, key.width / width);

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Edge const edge =
                edgeAt(key, ((x + 0.5) / width - 0.5) * key.width, ((y + 0.5) / height - 0.5) * key.height);
            auto const value =
                static_cast<std::uint32_t>(std::lround(faceShare(edge.inside - key.rim, band) * 255.0));
            // Белый с умноженной альфой: все четыре канала равны.
            pixels[static_cast<std::size_t>(y) * width + x] = (value << 24) | (value << 16) | (value << 8) | value;
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

void lab::drawShape(DrawingSurface const& surface, float radius) {
    SizeInt32 const size = surface.size();
    surface.draw([&](ID2D1DeviceContext* context) {
        ComPtr<ID2D1SolidColorBrush> white;
        wxl::check_hresult(context->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &white));
        D2D1_ROUNDED_RECT const shape {
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height)), radius, radius};
        context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        context->FillRoundedRectangle(shape, white.Get());
    });
}
