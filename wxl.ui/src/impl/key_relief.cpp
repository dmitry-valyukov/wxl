// wxl::impl::draw_key_map, draw_face_mask, draw_hole_shadow, draw_shape --
// see key_relief.h. The formula of the shape, run per pixel, and the pixels
// written to the surface as one bitmap.

#include "platform.h"

#include <winrt/Microsoft.UI.Composition.h>

#include <d2d1_1.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "impl/key_relief.h"

namespace wxl::impl {

namespace {

constexpr double radiansPerDegree = 3.14159265358979 / 180.0;

struct Normal {
    double x, y, z;
};

Normal unit(double x, double y, double z) {
    double const length = std::sqrt(x * x + y * y + z * z);
    return {x / length, y / length, z / length};
}

// The edge of a rounded rectangle, from a point counted from its middle:
// the distance from the edge inwards, and which way the edge faces there --
// outwards; screen axes.
struct Edge {
    double inside;
    double gx, gy;
};

Edge edge_of(double halfWidth, double halfHeight, double corner, double px, double py) {
    double const radius = std::clamp(corner, 0.0, std::min(halfWidth, halfHeight));
    double const qx = std::abs(px) - (halfWidth - radius), qy = std::abs(py) - (halfHeight - radius);
    double const ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);

    Edge edge{radius - std::sqrt(ox * ox + oy * oy) - std::min(std::max(qx, qy), 0.0), 0.0, 0.0};
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

Edge edge_at(key_relief const& key, double px, double py) {
    return edge_of(key.width / 2.0, key.height / 2.0, key.corner, px, py);
}

// How deep a point is in the face: the distance from the rim's inner edge
// inwards, below zero on the rim itself. While the inner part is not
// shifted, that is the distance from the key's edge less the rim. Shifted,
// it is measured from its own outline -- the key's, shrunk by the rim and
// moved: the outer edge of the rim stays, and the rim grows wider on one
// side and narrower on the other.
double face_depth(key_relief const& key, Edge const& outer, double px, double py) {
    if (key.shiftX == 0.0 && key.shiftY == 0.0) {
        return outer.inside - key.rim;
    }
    return edge_of(std::max(key.width / 2.0 - key.rim, 0.0), std::max(key.height / 2.0 - key.rim, 0.0),
                   std::max(key.corner - key.rim, 0.0), px - key.shiftX, py - key.shiftY)
        .inside;
}

// The face's share at a point: zero on the rim, one on the face, a band
// `width` wide around the join between.
double face_share(double depth, double width) {
    if (width <= 0.0) {
        return depth >= 0.0 ? 1.0 : 0.0;
    }
    return std::clamp(depth / width + 0.5, 0.0, 1.0);
}

// The normal of the key at a point counted from its middle; screen axes.
Normal key_normal_at(key_relief const& key, double px, double py) {
    // The face is a spherical cap of one curvature along both axes: the
    // slope grows from the middle linearly, and at the edge of the long side
    // under full emboss is 0.6 (about 31 degrees). A dome tilts the normal
    // away from the middle, a dish towards it. The middle of the face moves
    // with the whole inner part.
    double const reach = std::max(std::max(key.width, key.height) / 2.0 - key.rim, 1e-6);
    double const steepness = key.emboss * 0.6 / reach;
    double const faceX = (px - key.shiftX) * steepness, faceY = (py - key.shiftY) * steepness;
    if (key.rim <= 0.0) {
        return unit(faceX, faceY, 1.0);
    }

    // The rim goes on past the outline: the mask smooths the edge there, and
    // under its half-transparent pixels the normal must be the rim's, not a
    // flat one -- or under the lamp over the pointer they come out brighter
    // than the rim itself.
    Edge const edge = edge_at(key, px, py);
    double const depth = face_depth(key, edge, px, py);
    double slope = key.slope;
    if (key.rounded) {
        // A quarter circle: shallow by the face, sheer at the edge. The width
        // of the rim here runs from the key's edge to the rim's inner edge.
        double const width = std::max(edge.inside - depth, 1e-6);
        double const t = std::clamp(1.0 - edge.inside / width, 0.0, 1.0);
        slope = std::clamp(key.slope * t / std::sqrt(std::max(1.0 - t * t, 1e-4)), -8.0, 8.0);
    }
    double const rimX = edge.gx * slope, rimY = edge.gy * slope;

    // In the band of the join it is the slopes of the surface that blend,
    // not the normals: that is what a rounded edge between rim and face
    // looks like.
    double const share = face_share(depth, key.smooth);
    return unit(rimX + (faceX - rimX) * share, rimY + (faceY - rimY) * share, 1.0);
}

// The height of a point of the key over the rim's outer edge: the rim climbs
// from the edge along its profile to the inner edge, and the face stands at
// that height. The same width of rim here as the normal uses.
double key_height_at(key_relief const& key, Edge const& edge, double depth) {
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

std::uint32_t channel(double value) {
    return static_cast<std::uint32_t>(std::lround((value * 0.5 + 0.5) * 255.0));
}

// One bitmap, copied without smoothing: a pixel holds a number, not a colour.
void blit(DrawingSurface const& surface, std::vector<std::uint32_t> const& pixels, int width, int height) {
    surface.draw([&](ID2D1DeviceContext* context) {
        D2D1_BITMAP_PROPERTIES1 const properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        winrt::com_ptr<ID2D1Bitmap1> bitmap;
        winrt::check_hresult(context->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)),
                                                   pixels.data(), static_cast<UINT32>(width) * 4, &properties,
                                                   bitmap.put()));
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        context->DrawBitmap(bitmap.get(), D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
                            1.0f, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    });
}

// Runs `pixel(px, py)` for every pixel of the surface, the point in layout
// units from the key's middle, and writes what it returns.
template <class Pixel>
void draw_pixels(DrawingSurface const& surface, key_relief const& key, Pixel pixel) {
    SizeInt32 const size = surface.size();
    int const width = size.width, height = size.height;
    if (width <= 0 || height <= 0) {
        return;
    }
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels[static_cast<std::size_t>(y) * width + x] =
                pixel(((x + 0.5) / width - 0.5) * key.width, ((y + 0.5) / height - 0.5) * key.height);
        }
    }
    blit(surface, pixels, width, height);
}

}  // namespace

void draw_key_map(DrawingSurface const& surface, key_relief const& key) {
    draw_pixels(surface, key, [&](double px, double py) {
        Normal const normal = key_normal_at(key, px, py);
        // Green up the screen: the screen's y changes sign.
        return 0xFF000000u | (channel(normal.x) << 16) | (channel(-normal.y) << 8) | channel(normal.z);
    });
}

void draw_face_mask(DrawingSurface const& surface, key_relief const& key) {
    SizeInt32 const size = surface.size();
    if (size.width <= 0) {
        return;
    }
    // Never under one pixel of the surface: without that the edge of the mask steps.
    double const band = std::max(key.smooth, key.width / size.width);
    draw_pixels(surface, key, [&](double px, double py) {
        Edge const edge = edge_at(key, px, py);
        auto const value = static_cast<std::uint32_t>(std::lround(face_share(edge.inside - key.rim, band) * 255.0));
        // White with premultiplied alpha: all four channels alike.
        return (value << 24) | (value << 16) | (value << 8) | value;
    });
}

void draw_hole_shadow(DrawingSurface const& surface, key_relief const& key, hole_shadow const& shadow) {
    SizeInt32 const size = surface.size();
    if (size.width <= 0) {
        return;
    }
    double const a = shadow.azimuth * radiansPerDegree;
    double const e = shadow.elevation * radiansPerDegree;
    // The light runs across the screen away from the lamp, cot e per unit of
    // descent; towards the lamp is the other way. Ambient and the lamp's
    // light on a flat spot are as the effect sets them.
    double const run = std::cos(e) / std::max(std::sin(e), 1e-3);
    double const towardX = std::cos(a), towardY = -std::sin(a);
    Normal const toLamp = unit(towardX * std::cos(e), towardY * std::cos(e), std::sin(e));
    double const ambient = 1.0 - shadow.share;
    double const lamp = shadow.share / std::max(std::sin(e), 1e-3);
    // The penumbra never under one pixel of the surface, or the shadow's edge steps.
    double const soft = std::max(shadow.soft, key.width / size.width);

    draw_pixels(surface, key, [&](double px, double py) -> std::uint32_t {
        Edge const edge = edge_at(key, px, py);
        double const depth = face_depth(key, edge, px, py);
        // A point above the panel sees no edge.
        double const below = shadow.depth - key_height_at(key, edge, depth);
        if (below <= 0.0) {
            return 0;
        }
        // Where the ray to this point crossed the plane of the panel: in
        // shadow if outside the opening, and the opening is the key's print.
        Edge const opening = edge_at(key, px + towardX * run * below, py + towardY * run * below);
        double const cover = std::clamp(0.5 - opening.inside / soft, 0.0, 1.0);
        if (cover <= 0.0) {
            return 0;
        }
        // The shadow takes the lamp's share of the light at this point; the
        // rest is ambient.
        Normal const normal = key_normal_at(key, px, py);
        double const lit = lamp * std::max(normal.x * toLamp.x + normal.y * toLamp.y + normal.z * toLamp.z, 0.0);
        double const alpha = cover * shadow.amount * lit / std::max(ambient + lit, 1e-6);
        return static_cast<std::uint32_t>(std::lround(std::clamp(alpha, 0.0, 1.0) * 255.0)) << 24;
    });
}

void draw_shape(DrawingSurface const& surface, float radius) {
    SizeInt32 const size = surface.size();
    surface.draw([&](ID2D1DeviceContext* context) {
        winrt::com_ptr<ID2D1SolidColorBrush> white;
        winrt::check_hresult(context->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), white.put()));
        D2D1_ROUNDED_RECT const shape{
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height)), radius, radius};
        context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        context->FillRoundedRectangle(shape, white.get());
    });
}

}  // namespace wxl::impl
