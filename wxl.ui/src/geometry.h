#pragma once

// Standard headers come through core.h, never directly: a wxl header that
// included one of its own would place it after the wxl.core import in some
// translation unit, which MSVC rejects.
#include "core.h"

namespace wxl {

struct Size {
    float width, height;
};

struct Offset {
    float x, y;
};

using Point = Offset;

struct Rect {
    Offset offset;
    Size size;
};

inline Size operator+(Size a, Size b) { return {a.width + b.width, a.height + b.height}; }
inline Size operator-(Size a, Size b) { return {a.width - b.width, a.height - b.height}; }

inline Offset operator+(Offset a, Offset b) { return {a.x + b.x, a.y + b.y}; }
inline Offset operator-(Offset a, Offset b) { return {a.x - b.x, a.y - b.y}; }

inline bool intersects(const Rect& a, const Rect& b) {
    float ax1 = a.offset.x;
    float ay1 = a.offset.y;
    float ax2 = ax1 + a.size.width;
    float ay2 = ay1 + a.size.height;

    float bx1 = b.offset.x;
    float by1 = b.offset.y;
    float bx2 = bx1 + b.size.width;
    float by2 = by1 + b.size.height;

    return (ax1 < bx2) && (ax2 > bx1) && (ay1 < by2) && (ay2 > by1);
}

inline bool contains(Rect r, Point p) {
    return (p.x >= r.offset.x) && (p.x <= r.offset.x + r.size.width) && (p.y >= r.offset.y) &&
           (p.y <= r.offset.y + r.size.height);
}

inline float distance_sq(Point a, Point b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

inline float distance(Point a, Point b) { return std::sqrt(distance_sq(a, b)); }

inline float length(Point p) { return std::sqrt(p.x * p.x + p.y * p.y); }

inline Point normalize(Point p) {
    float len = length(p);
    if (len == 0.0f) return {0.0f, 0.0f};
    return {p.x / len, p.y / len};
}

inline float dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }

inline Point lerp(Point a, Point b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

inline Point rotate(Point p, float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    return {p.x * c - p.y * s, p.x * s + p.y * c};
}

inline Point clamp(Point p, Rect r) {
    float left = r.offset.x;
    float top = r.offset.y;
    float right = r.offset.x + r.size.width;
    float bottom = r.offset.y + r.size.height;
    float x = std::max(left, std::min(p.x, right));
    float y = std::max(top, std::min(p.y, bottom));
    return {x, y};
}

inline float area(Size s) { return s.width * s.height; }

inline float aspect_ratio(Size s) {
    if (s.height == 0.0f) return 0.0f;
    return s.width / s.height;
}

inline Size scale(Size s, float sx, float sy) { return {s.width * sx, s.height * sy}; }

inline Size fit_to(Size src, Size dest) {
    if (src.width == 0.0f || src.height == 0.0f) return {0.0f, 0.0f};
    float sx = dest.width / src.width;
    float sy = dest.height / src.height;
    float s = std::min(sx, sy);
    return {src.width * s, src.height * s};
}

inline Size fill_to(Size src, Size dest) {
    if (src.width == 0.0f || src.height == 0.0f) return {0.0f, 0.0f};
    float sx = dest.width / src.width;
    float sy = dest.height / src.height;
    float s = std::max(sx, sy);
    return {src.width * s, src.height * s};
}

inline Point rect_center(Rect r) {
    return {r.offset.x + r.size.width * 0.5f, r.offset.y + r.size.height * 0.5f};
}

inline Rect translate(Rect r, Offset ofs) {
    r.offset.x += ofs.x;
    r.offset.y += ofs.y;
    return r;
}

inline Rect inflate(Rect r, float dx, float dy) {
    r.offset.x -= dx;
    r.offset.y -= dy;
    r.size.width += dx * 2.0f;
    r.size.height += dy * 2.0f;
    return r;
}

inline Rect deflate(Rect r, float dx, float dy) { return inflate(r, -dx, -dy); }

inline Rect normalize_rect(Rect r) {
    if (r.size.width < 0.0f) {
        r.offset.x += r.size.width;
        r.size.width = -r.size.width;
    }
    if (r.size.height < 0.0f) {
        r.offset.y += r.size.height;
        r.size.height = -r.size.height;
    }
    return r;
}

inline Rect intersection(Rect a, Rect b) {
    a = normalize_rect(a);
    b = normalize_rect(b);
    float ax1 = a.offset.x;
    float ay1 = a.offset.y;
    float ax2 = ax1 + a.size.width;
    float ay2 = ay1 + a.size.height;

    float bx1 = b.offset.x;
    float by1 = b.offset.y;
    float bx2 = bx1 + b.size.width;
    float by2 = by1 + b.size.height;

    float ix1 = std::max(ax1, bx1);
    float iy1 = std::max(ay1, by1);
    float ix2 = std::min(ax2, bx2);
    float iy2 = std::min(ay2, by2);

    if (ix2 <= ix1 || iy2 <= iy1) {
        return {{0, 0}, {0, 0}};
    }
    Rect r;
    r.offset.x = ix1;
    r.offset.y = iy1;
    r.size.width = ix2 - ix1;
    r.size.height = iy2 - iy1;
    return r;
}

inline Rect union_rect(Rect a, Rect b) {
    a = normalize_rect(a);
    b = normalize_rect(b);
    float ax1 = a.offset.x;
    float ay1 = a.offset.y;
    float ax2 = ax1 + a.size.width;
    float ay2 = ay1 + a.size.height;

    float bx1 = b.offset.x;
    float by1 = b.offset.y;
    float bx2 = bx1 + b.size.width;
    float by2 = by1 + b.size.height;

    float ux1 = std::min(ax1, bx1);
    float uy1 = std::min(ay1, by1);
    float ux2 = std::max(ax2, bx2);
    float uy2 = std::max(ay2, by2);

    Rect r;
    r.offset.x = ux1;
    r.offset.y = uy1;
    r.size.width = ux2 - ux1;
    r.size.height = uy2 - uy1;
    return r;
}

inline bool contains(Rect r, Rect s) {
    r = normalize_rect(r);
    s = normalize_rect(s);
    return (s.offset.x >= r.offset.x) && (s.offset.x + s.size.width <= r.offset.x + r.size.width) &&
           (s.offset.y >= r.offset.y) && (s.offset.y + s.size.height <= r.offset.y + r.size.height);
}

inline Point top_left(Rect r) {
    r = normalize_rect(r);
    return {r.offset.x, r.offset.y};
}
inline Point top_right(Rect r) {
    r = normalize_rect(r);
    return {r.offset.x + r.size.width, r.offset.y};
}
inline Point bottom_left(Rect r) {
    r = normalize_rect(r);
    return {r.offset.x, r.offset.y + r.size.height};
}
inline Point bottom_right(Rect r) {
    r = normalize_rect(r);
    return {r.offset.x + r.size.width, r.offset.y + r.size.height};
}


// ---- The numerics the composition surface speaks ----
//
// A visual's offset, scale and centre are Vector3; a brush's gradient ends
// are Vector2; a transform is a Matrix4x4. They are plain float aggregates
// in metadata, and wxl declares them for the same reason it declares Size
// and Point: cppwinrt's own float2/float3/float4x4 carry constructors and
// arithmetic, which costs an eight-byte value its return in a register and
// puts a projection type in every signature that names a position.
//
// Distinct types rather than reuses of Offset and Point, even where the
// fields coincide: the conversions below differ only in what they return,
// and two of those over one parameter type would not be an overload set.

struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;
};

// Row-major, as the ABI lays it out and as DirectX writes it.
struct Matrix4x4 {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
};

inline Vector3 operator+(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vector3 operator-(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vector3 operator*(Vector3 v, float k) { return {v.x * k, v.y * k, v.z * k}; }

/// The identity, which is what a transform starts as and returns to.
inline constexpr Matrix4x4 identity_matrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
}  // namespace wxl
