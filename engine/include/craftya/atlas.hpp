// Craftya Engine — doku atlası: 16x16 pixel art, 1024x1024, prosedürel üretim
// Sunucudan doku gelmez; atlas cihazda üretilir/indirilir. Bu üretici
// fallback'tir: ağ yoksa bile oyun dokulu çalışır.
#pragma once
#include "craftya/core.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace craftya {

// RGBA8 görüntü
struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;   // RGBA

    Image() = default;
    Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h * 4, 0) {}

    void set(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        size_t i = (static_cast<size_t>(y) * width + x) * 4;
        pixels[i] = r; pixels[i+1] = g; pixels[i+2] = b; pixels[i+3] = a;
    }
    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        for (size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i] = r; pixels[i+1] = g; pixels[i+2] = b; pixels[i+3] = a;
        }
    }
    void blit(const Image& src, int dx, int dy);
    bool savePNG(const std::string& path) const;
    bool savePPM(const std::string& path) const;  // PNG bağımlılığı olmadan hata ayıklama
};

// --- atlas ---
class Atlas {
public:
    static constexpr int TILE = 16;         // doku karesi 16x16
    static constexpr int GRID = 16;         // 16x16 kare
    static constexpr int SIZE = TILE * GRID; // 256x256 (31 blok için fazlasıyla yeter)

    Atlas();

    // prosedürel olarak tüm blok dokularını üret
    void generateProcedural(uint64_t seed = 12345);

    const Image& image() const { return img_; }

    // doku indeksi → atlas UV (0..1)
    static void tileUV(int tileIndex, float& u0, float& v0, float& u1, float& v1) {
        int tx = tileIndex % GRID;
        int ty = tileIndex / GRID;
        u0 = static_cast<float>(tx * TILE) / SIZE;
        v0 = static_cast<float>(ty * TILE) / SIZE;
        u1 = static_cast<float>((tx + 1) * TILE) / SIZE;
        v1 = static_cast<float>((ty + 1) * TILE) / SIZE;
    }

    // blok + yüz → doku indeksi (0=üst,1=yan,2=alt)
    static int tileForBlock(BlockId id, int face);

private:
    Image img_;
    void drawTile(int tileIndex, BlockId id, int face);
};

} // namespace craftya
