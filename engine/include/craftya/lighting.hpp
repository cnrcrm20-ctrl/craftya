// Craftya Engine — ışık motoru: skylight + blok ışığı, BFS yayılım, AO, gün döngüsü
// Minecraft benzeri 0-15 ölçeği. Chunk sınırında taşma destekli.
#pragma once
#include "craftya/world.hpp"
#include <array>
#include <deque>

namespace craftya {

// Işık seviyeleri: 4 bit skylight + 4 bit blok ışığı = 1 bayt
struct LightValue {
    uint8_t sky = 0;
    uint8_t block = 0;
    uint8_t effective() const { return sky > block ? sky : block; }
};

class LightEngine {
public:
    // chunk yüklendiğinde ışığı hesapla
    static void computeChunk(World& world, Chunk& chunk);

    // BÖLGESEL ışık: birden çok chunk'ı TEK BFS ile aydınlatır.
    // Chunk başına ayrı hesaplamak yanlıştır: komşu chunk henüz yokken
    // ışık sınırı geçemez ve yüzey kapkara kalır.
    // center: bölgenin merkez chunk'ı, rx/ry/rz: yarıçap (chunk).
    // Dönen: aydınlatılan chunk sayısı.
    static int computeRegion(World& world, ChunkPos center, int rx, int ry, int rz);

    // bir blok değiştiğinde yerel ışığı güncelle (artımlı BFS silme + ekleme)
    static void onBlockChanged(World& world, int wx, int wy, int wz, BlockId oldId, BlockId newId);

    // güneş yüksekliğine göre skylight çarpanı (0..1) — gece/gündüz
    static float sunFactor(float timeOfDayHours);

    // gök rengi (gün döngüsüne göre)
    static void skyColor(float timeOfDayHours, float out[3]);
};

// Gün döngüsü
class DayCycle {
public:
    float hours = 8.0f;              // 0..24, 8 = sabah
    float dayLengthSeconds = 1200.f; // 20 dakikada 1 gün (Minecraft benzeri)

    void update(float dt) {
        hours += (dt / dayLengthSeconds) * 24.f;
        while (hours >= 24.f) hours -= 24.f;
    }
    float sunFactor() const { return LightEngine::sunFactor(hours); }
    bool isNight() const { return hours < 6.f || hours > 19.f; }
};

// ---------- Ambient Occlusion ----------
// Vertex başına 0..1 karartma. 3 komşu bloğa bakar (klasik voxel AO).
class AmbientOcclusion {
public:
    // side1, side2: yüz düzlemindeki iki komşu; corner: köşegen komşu
    // dönüş: 0..3 (0 = açık, 3 = en koyu) → çarpana çevrilir
    static float vertexFactor(bool side1, bool side2, bool corner) {
        if (side1 && side2) return 0.55f;             // iki yan dolu → en koyu
        int n = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
        switch (n) {
            case 0: return 1.00f;
            case 1: return 0.86f;
            case 2: return 0.72f;
            default: return 0.60f;
        }
    }
};

} // namespace craftya
