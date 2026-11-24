// Craftya Engine — prosedürel pixel art doku üretimi
#include "craftya/atlas.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <random>
#include <cmath>
#include <fstream>

namespace craftya {

void Image::blit(const Image& src, int dx, int dy) {
    for (int y = 0; y < src.height; ++y)
        for (int x = 0; x < src.width; ++x) {
            size_t si = (static_cast<size_t>(y) * src.width + x) * 4;
            if (src.pixels[si + 3] == 0) continue;
            set(dx + x, dy + y, src.pixels[si], src.pixels[si+1], src.pixels[si+2], src.pixels[si+3]);
        }
}

bool Image::savePPM(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << width << " " << height << "\n255\n";
    for (size_t i = 0; i < pixels.size(); i += 4) {
        f.put(static_cast<char>(pixels[i]));
        f.put(static_cast<char>(pixels[i+1]));
        f.put(static_cast<char>(pixels[i+2]));
    }
    return true;
}

// --- PNG yazıcı (sıfır bağımlılık, zlib'siz "stored" bloklar) ---
namespace {
uint32_t crc32(const uint8_t* data, size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));  v.push_back((uint8_t)x);
}
void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    put32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, crc32(out.data() + start, out.size() - start));
}
} // namespace

bool Image::savePNG(const std::string& path) const {
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> ihdr;
    put32(ihdr, (uint32_t)width);
    put32(ihdr, (uint32_t)height);
    ihdr.push_back(8);   // bit derinliği
    ihdr.push_back(6);   // RGBA
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);

    // ham taranmış veri (her satır başına filtre baytı 0)
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (1 + width * 4));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = pixels.data() + static_cast<size_t>(y) * width * 4;
        raw.insert(raw.end(), row, row + width * 4);
    }

    // zlib: stored (sıkıştırmasız) deflate blokları
    std::vector<uint8_t> zlib = {0x78, 0x01};
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool finalBlock = (pos + n >= raw.size());
        zlib.push_back(finalBlock ? 0x01 : 0x00);
        zlib.push_back((uint8_t)(n & 0xFF));
        zlib.push_back((uint8_t)((n >> 8) & 0xFF));
        zlib.push_back((uint8_t)(~n & 0xFF));
        zlib.push_back((uint8_t)((~n >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    // adler32
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    put32(zlib, (b << 16) | a);
    chunk(out, "IDAT", zlib);
    chunk(out, "IEND", {});

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
    return true;
}

// ---------------- atlas ----------------
Atlas::Atlas() : img_(SIZE, SIZE) {
    img_.fill(255, 0, 255);   // magenta = eksik doku (kolay teşhis)
}

// ---------------- stile dayalı doku üretici ----------------
namespace {
inline void put(Image& a, int tx, int ty, int x, int y, int r, int g, int b, int alpha = 255) {
    auto cl = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
    a.set(tx + x, ty + y, (uint8_t)cl(r), (uint8_t)cl(g), (uint8_t)cl(b), (uint8_t)alpha);
}
inline void unpack(uint32_t c, int& r, int& g, int& b) {
    r = (c >> 16) & 0xFF; g = (c >> 8) & 0xFF; b = c & 0xFF;
}
// yalnızca bu kareyi saydam yapar (atlasın tamamını DEĞİL)
inline void clearTile(Image& a, int tx, int ty) {
    for (int y = 0; y < Atlas::TILE; ++y)
        for (int x = 0; x < Atlas::TILE; ++x)
            a.set(tx + x, ty + y, 0, 0, 0, 0);
}
}

bool Atlas::drawStyled(int tileIndex, BlockId id, int face) {
    const BlockDef& d = blockDef(id);
    int tx = (tileIndex % GRID) * TILE;
    int ty = (tileIndex / GRID) * TILE;
    if (id == AIR) { clearTile(img_, tx, ty); return true; }
    uint32_t pc = (face == 0 && d.topColor) ? d.topColor : d.color;
    int R, G, B; unpack(pc, R, G, B);

    std::mt19937 rng(static_cast<uint32_t>(tileIndex) * 2654435761u + 0x9e3779b9u);
    std::uniform_int_distribution<int> jitter(-16, 16);
    std::uniform_int_distribution<int> small(0, 15);

    switch (d.style) {
        case kStyle_stone:
        case kStyle_noise: {
            int v = (d.style == kStyle_noise) ? 26 : 14;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = std::uniform_int_distribution<int>(-v, v)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_cobble: {
            // yuvarlak taşlar: hücre merkezinden uzaklık
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int cx = x % 8, cy = y % 8;
                    float dist = std::sqrt(float((cx - 4) * (cx - 4) + (cy - 4) * (cy - 4)));
                    int n = dist > 3.4f ? -34 : std::uniform_int_distribution<int>(-10, 10)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_smooth: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = ((x == 0 || y == 0) ? -18 : 4) + small(rng) % 5;
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_brick: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int row = y / 4;
                    int off = (row % 2) * 4;
                    bool mortar = (y % 4 == 0) || ((x + off) % 8 == 0);
                    int n = mortar ? -42 : std::uniform_int_distribution<int>(-8, 10)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_speckle: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = std::uniform_int_distribution<int>(-12, 12)(rng);
                    if (small(rng) == 0) n += 30;
                    if (small(rng) == 1) n -= 30;
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_grain: {
            for (int y = 0; y < TILE; ++y) {
                int band = (y % 3 == 0) ? -20 : 0;
                for (int x = 0; x < TILE; ++x) {
                    int n = band + jitter(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            }
            break;
        }
        case kStyle_grass: {
            if (face == 2) {
                for (int y = 0; y < TILE; ++y)
                    for (int x = 0; x < TILE; ++x) {
                        int n = jitter(rng);
                        put(img_, tx, ty, x, y, 139 + n, 90 + n, 60 + n);
                    }
            } else if (face == 1) {
                for (int y = 0; y < TILE; ++y)
                    for (int x = 0; x < TILE; ++x) {
                        bool grass = y < 4 + (small(rng) % 3);
                        int n = jitter(rng) / 2;
                        if (grass) put(img_, tx, ty, x, y, R + n, G + n, B + n);
                        else put(img_, tx, ty, x, y, 139 + n, 90 + n, 60 + n);
                    }
            } else {
                for (int y = 0; y < TILE; ++y)
                    for (int x = 0; x < TILE; ++x) {
                        int n = jitter(rng);
                        put(img_, tx, ty, x, y, R + n, G + n, B + n);
                    }
            }
            break;
        }
        case kStyle_log: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    if (face == 0) {   // üst: halkalar
                        int dx = x - 8, dy = y - 8;
                        int dist = (int)std::sqrt(float(dx * dx + dy * dy));
                        int n = (dist % 3 == 0) ? -26 : 8;
                        put(img_, tx, ty, x, y, R + n, G + n, B + n);
                    } else {           // yan: dikey damar
                        int n = ((x % 5 == 0) ? -30 : 0) + jitter(rng) / 2;
                        put(img_, tx, ty, x, y, R + n, G + n, B + n);
                    }
                }
            break;
        }
        case kStyle_plank: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool seam = (y % 4 == 3) || ((x + (y / 4) * 6) % 11 == 0);
                    int n = seam ? -30 : std::uniform_int_distribution<int>(-8, 10)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_leaves: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = std::uniform_int_distribution<int>(-26, 18)(rng);
                    bool hole = (small(rng) < 3);
                    if (hole) put(img_, tx, ty, x, y, 0, 0, 0, 0);
                    else put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_wool: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = (((x + y) % 4 == 0) ? 14 : -8) + small(rng) % 6;
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_concrete: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = small(rng) % 7;
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_terracotta: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = std::uniform_int_distribution<int>(-14, 8)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_glass: {
            clearTile(img_, tx, ty);
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool border = (x == 0 || y == 0 || x == TILE - 1 || y == TILE - 1);
                    bool shine = ((x + y) % 13 == 0) && x > 2 && y > 2;
                    if (border) put(img_, tx, ty, x, y, R, G, B, 210);
                    else if (shine) put(img_, tx, ty, x, y, 255, 255, 255, 90);
                    else put(img_, tx, ty, x, y, R, G, B, 26);
                }
            break;
        }
        case kStyle_ore: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = std::uniform_int_distribution<int>(-16, 12)(rng);
                    put(img_, tx, ty, x, y, 118 + n, 118 + n, 124 + n);
                }
            // cevher taneleri
            for (int i = 0; i < 10; ++i) {
                int x = 2 + small(rng) % 12, y = 2 + small(rng) % 12;
                put(img_, tx, ty, x, y, R, G, B);
                put(img_, tx, ty, x + 1, y, R * 3 / 4, G * 3 / 4, B * 3 / 4);
                if (small(rng) % 2) put(img_, tx, ty, x, y + 1, R * 4 / 5, G * 4 / 5, B * 4 / 5);
            }
            break;
        }
        case kStyle_metal: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = ((x + y) % 8 < 2) ? 26 : std::uniform_int_distribution<int>(-8, 8)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_gem: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int dx = std::abs(x - 8), dy = std::abs(y - 8);
                    int facet = (dx + dy) % 5;
                    int n = facet == 0 ? 34 : std::uniform_int_distribution<int>(-10, 10)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_cross: {   // bitki: saydam zemin + sap
            clearTile(img_, tx, ty);
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int dx = std::abs(x - 8);
                    bool stem = (dx <= 1 && y > 5);
                    bool leaf = (std::abs(y - (6 + (x % 7))) < 2) && dx < 6 && small(rng) < 9;
                    int n = std::uniform_int_distribution<int>(-16, 14)(rng);
                    if (stem) put(img_, tx, ty, x, y, (R * 2) / 3, (G * 2) / 3, (B * 2) / 3);
                    else if (leaf) put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_machine: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool edge = (x == 0 || y == 0 || x == TILE - 1 || y == TILE - 1);
                    bool panel = (x > 2 && x < 13 && y > 2 && y < 13 && (x % 5 == 0 || y % 5 == 0));
                    int n = edge ? -28 : (panel ? -14 : std::uniform_int_distribution<int>(-6, 8)(rng));
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            // ön yüzde gösterge
            if (face == 1) {
                for (int y = 5; y < 11; ++y)
                    for (int x = 5; x < 11; ++x)
                        put(img_, tx, ty, x, y, 240, 180, 60);
            }
            break;
        }
        case kStyle_lamp: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = ((x % 4 == 0) || (y % 4 == 0)) ? -30 : 30;
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_lava: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int v = (int)(std::sin(x * 0.7f + y * 0.3f) * 24.f + jitter(rng));
                    put(img_, tx, ty, x, y, R + v, G + v / 2, B + v / 3);
                }
            break;
        }
        case kStyle_liquid: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = (int)(std::sin(x * 0.5f + y * 0.9f) * 16.f);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n, 165);
                }
            break;
        }
        case kStyle_books: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool shelf = (y % 8 == 0 or y % 8 == 7);
                    if (shelf) put(img_, tx, ty, x, y, R - 40, G - 40, B - 40);
                    else {
                        int book = x / 2;
                        int hue = (book * 53 + (y / 8) * 31) % 5;
                        const int cr[5] = {178, 62, 46, 92, 150};
                        const int cg[5] = {52, 96, 140, 62, 40};
                        const int cb[5] = {44, 58, 62, 140, 96};
                        put(img_, tx, ty, x, y, cr[hue], cg[hue], cb[hue]);
                    }
                }
            break;
        }
        case kStyle_door: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool frame = (x == 0 || x == TILE - 1 || y % 5 == 0);
                    int n = frame ? -32 : std::uniform_int_distribution<int>(-8, 8)(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        case kStyle_flat: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = jitter(rng);
                    if (y > 10) put(img_, tx, ty, x, y, R + n, G + n, B + n, 0);
                    else put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
        default: {
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    int n = jitter(rng);
                    put(img_, tx, ty, x, y, R + n, G + n, B + n);
                }
            break;
        }
    }
    return true;
}

void Atlas::drawTile(int tileIndex, BlockId id, int face) {
    drawStyled(tileIndex, id, face);
}

void Atlas::generateProcedural(uint64_t seed) {
    (void)seed;
    img_.fill(255, 0, 255);
    for (int id = 1; id < BLOCK_COUNT; ++id) {
        for (int face = 0; face < 3; ++face) {
            int tile = tileForBlock(static_cast<BlockId>(id), face);
            if (tile >= GRID * GRID) break;
            drawStyled(tile, static_cast<BlockId>(id), face);
        }
    }
}

} // namespace craftya
