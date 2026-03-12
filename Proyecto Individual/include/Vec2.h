#pragma once

#include <cmath>

// ============================================================
// 2D Vector utility
// ============================================================
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const  { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const  { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s)       const  { return {x * s, y * s}; }
    Vec2 operator/(float s)       const  { return {x / s, y / s}; }

    Vec2& operator+=(const Vec2& o)  { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o)  { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s)        { x *= s; y *= s;       return *this; }

    float dot(const Vec2& o)   const { return x * o.x + y * o.y; }
    float lengthSq()           const { return x * x + y * y; }
    float length()             const { return std::sqrt(lengthSq()); }

    Vec2 normalized() const {
        float len = length();
        return (len > 1e-8f) ? Vec2{x / len, y / len} : Vec2{0, 0};
    }
};

inline Vec2 operator*(float s, const Vec2& v) { return {v.x * s, v.y * s}; }
