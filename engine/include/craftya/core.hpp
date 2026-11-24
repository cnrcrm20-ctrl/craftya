// Craftya Engine — çekirdek tipler, sabitler, blok kayıt defteri
// Hedef: Android (arm64) + Windows. Bağımlılık yok, saf C++17.
#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>

#include "craftya/blocks_gen.hpp"

namespace craftya {

// ---------- sabitler ----------
constexpr int CHUNK_W = 16;          // chunk genişlik (x,z)
constexpr int CHUNK_H = 16;          // chunk yükseklik (y) — dikey de bölünür
constexpr int CHUNK_AREA = CHUNK_W * CHUNK_W;
constexpr int CHUNK_VOL = CHUNK_AREA * CHUNK_H;

constexpr int VIEW_CHUNKS = 6;        // görüş mesafesi (chunk)
constexpr int PROCESS_CHUNKS_X2 = 13; // işlem mesafesi 6.5 chunk → 13 yarım birim
constexpr float FOG_START_CHUNK = 5.5f;
constexpr float FOG_END_CHUNK   = 6.0f;

// ---------- vektörler ----------
struct Vec3i {
    int x = 0, y = 0, z = 0;
    Vec3i() = default;
    Vec3i(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}
    Vec3i operator+(const Vec3i& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3i operator-(const Vec3i& o) const { return {x - o.x, y - o.y, z - o.z}; }
    bool operator==(const Vec3i& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct Vec3f {
    float x = 0, y = 0, z = 0;
    Vec3f() = default;
    Vec3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3f operator+(const Vec3f& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3f operator-(const Vec3f& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3f operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3f& operator+=(const Vec3f& o) { x += o.x; y += o.y; z += o.z; return *this; }
    float dot(const Vec3f& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3f cross(const Vec3f& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3f normalized() const {
        float l = length();
        return l > 1e-6f ? Vec3f{x / l, y / l, z / l} : Vec3f{0, 0, 0};
    }
};


} // namespace craftya
