// Craftya Engine — prosedürel pixel art doku üretimi
#include "craftya/atlas.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <random>
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

int Atlas::tileForBlock(BlockId id, int face) {
    // doku indeksi = blok id (0-30), üst/yan/alt için 3 ayrı karo
    int base = static_cast<int>(id) * 3;
    int f = (face < 0 || face > 2) ? 1 : face;
    return base + f;
}

namespace {
// yardımcı: gürültülü doku
void noiseTile(Image& atlas, int tileIndex, uint8_t r, uint8_t g, uint8_t b,
               int variation, std::mt19937& rng) {
    int tx = (tileIndex % Atlas::GRID) * Atlas::TILE;
    int ty = (tileIndex / Atlas::GRID) * Atlas::TILE;
    std::uniform_int_distribution<int> d(-variation, variation);
    for (int y = 0; y < Atlas::TILE; ++y)
        for (int x = 0; x < Atlas::TILE; ++x) {
            int dr = d(rng), dg = d(rng), db = d(rng);
            auto clamp = [](int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
            atlas.set(tx + x, ty + y, clamp(r + dr), clamp(g + dg), clamp(b + db));
        }
}

// yardımcı: yatay çizgili doku (kütük, tahta)
void grainTile(Image& atlas, int tileIndex, uint8_t r, uint8_t g, uint8_t b,
               bool vertical, std::mt19937& rng) {
    int tx = (tileIndex % Atlas::GRID) * Atlas::TILE;
    int ty = (tileIndex / Atlas::GRID) * Atlas::TILE;
    std::uniform_int_distribution<int> d(-14, 14);
    for (int y = 0; y < Atlas::TILE; ++y) {
        int lineDark = ((vertical ? y : 0) % 4 == 0) ? -22 : 0;
        for (int x = 0; x < Atlas::TILE; ++x) {
            int extra = ((!vertical && x % 5 == 0) ? -18 : 0);
            int dr = d(rng) + lineDark + extra;
            int dg = d(rng) + lineDark + extra;
            int db = d(rng) + lineDark + extra;
            auto clamp = [](int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
            atlas.set(tx + x, ty + y, clamp(r + dr), clamp(g + dg), clamp(b + db));
        }
    }
}

// cevher: taş zemin + renkli benekler
void oreTile(Image& atlas, int tileIndex, uint8_t cr, uint8_t cg, uint8_t cb, std::mt19937& rng) {
    noiseTile(atlas, tileIndex, 120, 120, 127, 18, rng);
    int tx = (tileIndex % Atlas::GRID) * Atlas::TILE;
    int ty = (tileIndex / Atlas::GRID) * Atlas::TILE;
    std::uniform_int_distribution<int> pos(1, Atlas::TILE - 2);
    for (int i = 0; i < 12; ++i) {
        int x = pos(rng), y = pos(rng);
        atlas.set(tx + x, ty + y, cr, cg, cb);
        if (pos(rng) % 2) atlas.set(tx + x + 1, ty + y, (uint8_t)(cr * 0.8f), (uint8_t)(cg * 0.8f), (uint8_t)(cb * 0.8f));
    }
}
} // namespace

void Atlas::drawTile(int tileIndex, BlockId id, int face) {
    std::mt19937 rng(static_cast<uint32_t>(tileIndex) * 2654435761u + static_cast<uint32_t>(id));
    switch (id) {
        case AIR:          img_.fill(255, 0, 255); break;
        case STONE:        noiseTile(img_, tileIndex, 122, 122, 127, 16, rng); break;
        case DIRT:         noiseTile(img_, tileIndex, 124, 88, 56, 20, rng); break;
        case GRASS:
            if (face == 0) noiseTile(img_, tileIndex, 92, 152, 62, 18, rng);
            else if (face == 2) noiseTile(img_, tileIndex, 124, 88, 56, 20, rng);
            else {   // yan: üstte çim şeridi
                noiseTile(img_, tileIndex, 124, 88, 56, 20, rng);
                int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
                std::uniform_int_distribution<int> d(0, 12);
                for (int x = 0; x < TILE; ++x) {
                    int h = 3 + d(rng) % 3;
                    for (int y = 0; y < h; ++y)
                        img_.set(tx + x, ty + y, (uint8_t)(92 + d(rng)), (uint8_t)(152 + d(rng)), 62);
                }
            }
            break;
        case COBBLE: {
            noiseTile(img_, tileIndex, 110, 110, 115, 10, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int i = 0; i < TILE; i += 5) {
                for (int j = 0; j < TILE; ++j) {
                    img_.set(tx + i, ty + j, 76, 76, 80);
                    img_.set(tx + j, ty + i, 76, 76, 80);
                }
            }
            break;
        }
        case SAND:         noiseTile(img_, tileIndex, 219, 205, 148, 12, rng); break;
        case GRAVEL:       noiseTile(img_, tileIndex, 128, 122, 118, 26, rng); break;
        case OAK_LOG:
            if (face == 0 || face == 2) noiseTile(img_, tileIndex, 160, 128, 82, 12, rng);
            else grainTile(img_, tileIndex, 132, 100, 60, true, rng);
            break;
        case OAK_PLANKS:   grainTile(img_, tileIndex, 168, 136, 88, false, rng); break;
        case OAK_LEAVES: {
            noiseTile(img_, tileIndex, 62, 122, 48, 28, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            std::uniform_int_distribution<int> p(0, TILE - 1);
            for (int i = 0; i < 26; ++i)
                img_.set(tx + p(rng), ty + p(rng), 40, 88, 32);
            break;
        }
        case BIRCH_LOG:
            if (face == 0 || face == 2) noiseTile(img_, tileIndex, 216, 210, 196, 10, rng);
            else {
                noiseTile(img_, tileIndex, 228, 226, 214, 8, rng);
                int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
                std::uniform_int_distribution<int> p(0, TILE - 1);
                for (int i = 0; i < 10; ++i) {
                    int x = p(rng), y = p(rng);
                    img_.set(tx + x, ty + y, 60, 60, 58);
                    img_.set(tx + x, ty + y + 1, 60, 60, 58);
                }
            }
            break;
        case BIRCH_PLANKS: grainTile(img_, tileIndex, 216, 204, 168, false, rng); break;
        case WATER: {
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    float w = std::sin((x + y * 0.5f) * 0.7f) * 0.5f + 0.5f;
                    img_.set(tx + x, ty + y,
                        (uint8_t)(44 + w * 18), (uint8_t)(98 + w * 26), (uint8_t)(190 + w * 30), 170);
                }
            break;
        }
        case LAVA: {
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    float w = std::sin((x * 0.8f + y * 1.3f)) * 0.5f + 0.5f;
                    img_.set(tx + x, ty + y,
                        (uint8_t)(210 + w * 45), (uint8_t)(88 + w * 70), 24);
                }
            break;
        }
        case GLASS: {
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool border = (x == 0 || y == 0 || x == TILE-1 || y == TILE-1);
                    if (border) img_.set(tx + x, ty + y, 200, 225, 235, 220);
                    else if ((x + y) % 9 == 0) img_.set(tx + x, ty + y, 225, 240, 250, 90);
                    else img_.set(tx + x, ty + y, 210, 235, 245, 40);
                }
            break;
        }
        case BRICKS: {
            noiseTile(img_, tileIndex, 150, 72, 58, 12, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool mortar = (y % 5 == 0) || ((x + (y / 5) * 4) % 8 == 0);
                    if (mortar) img_.set(tx + x, ty + y, 186, 178, 168);
                }
            break;
        }
        case STONE_BRICKS: {
            noiseTile(img_, tileIndex, 126, 126, 130, 12, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    if (y % 8 == 0 || ((x + (y / 8) * 8) % 16 == 0))
                        img_.set(tx + x, ty + y, 96, 96, 100);
                }
            break;
        }
        case COAL_ORE:     oreTile(img_, tileIndex, 38, 38, 42, rng); break;
        case IRON_ORE:     oreTile(img_, tileIndex, 196, 160, 128, rng); break;
        case GOLD_ORE:     oreTile(img_, tileIndex, 240, 202, 72, rng); break;
        case DIAMOND_ORE:  oreTile(img_, tileIndex, 96, 226, 226, rng); break;
        case SNOW:         noiseTile(img_, tileIndex, 244, 248, 252, 6, rng); break;
        case ICE: {
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    float w = std::sin(x * 0.6f) * std::cos(y * 0.7f) * 0.5f + 0.5f;
                    img_.set(tx + x, ty + y, (uint8_t)(170 + w * 40), (uint8_t)(210 + w * 35), 245, 200);
                }
            break;
        }
        case OBSIDIAN: {
            noiseTile(img_, tileIndex, 34, 24, 54, 10, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            std::uniform_int_distribution<int> p(0, TILE - 1);
            for (int i = 0; i < 8; ++i)
                img_.set(tx + p(rng), ty + p(rng), 96, 70, 140);
            break;
        }
        case NETHERRACK:   noiseTile(img_, tileIndex, 122, 56, 54, 22, rng); break;
        case GLOWSTONE: {
            noiseTile(img_, tileIndex, 232, 198, 108, 16, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            std::uniform_int_distribution<int> p(0, TILE - 1);
            for (int i = 0; i < 14; ++i)
                img_.set(tx + p(rng), ty + p(rng), 255, 236, 160);
            break;
        }
        case TORCH: {
            // küçük dikey çubuk (ortada), gerisi saydam
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y)
                for (int x = 0; x < TILE; ++x) {
                    bool stick = (x >= 7 && x <= 8 && y >= 6);
                    bool flame = (x >= 6 && x <= 9 && y >= 3 && y <= 6);
                    if (flame) img_.set(tx + x, ty + y, 255, 208, 84);
                    else if (stick) img_.set(tx + x, ty + y, 138, 102, 60);
                    else img_.set(tx + x, ty + y, 0, 0, 0, 0);
                }
            break;
        }
        case CRAFTING_TABLE:
            if (face == 0) {
                grainTile(img_, tileIndex, 168, 136, 88, false, rng);
                int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
                for (int i = 2; i < TILE - 2; i += 4)
                    for (int j = 2; j < TILE - 2; ++j) {
                        img_.set(tx + i, ty + j, 96, 72, 44);
                        img_.set(tx + j, ty + i, 96, 72, 44);
                    }
            } else grainTile(img_, tileIndex, 148, 116, 72, true, rng);
            break;
        case FURNACE:
            if (face == 1) {
                noiseTile(img_, tileIndex, 110, 110, 114, 12, rng);
                int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
                for (int y = 6; y < 12; ++y)
                    for (int x = 3; x < 13; ++x)
                        img_.set(tx + x, ty + y, 40, 36, 34);
                for (int x = 5; x < 11; ++x)
                    img_.set(tx + x, ty + 11, 214, 120, 40);
            } else noiseTile(img_, tileIndex, 118, 118, 122, 14, rng);
            break;
        case CHEST: {
            grainTile(img_, tileIndex, 152, 110, 62, false, rng);
            int tx = (tileIndex % GRID) * TILE, ty = (tileIndex / GRID) * TILE;
            for (int y = 0; y < TILE; ++y) {
                img_.set(tx + 0, ty + y, 96, 68, 36);
                img_.set(tx + TILE - 1, ty + y, 96, 68, 36);
            }
            for (int x = 6; x < 10; ++x)
                for (int y = 7; y < 10; ++y)
                    img_.set(tx + x, ty + y, 226, 190, 74);
            break;
        }
        case BEDROCK:      noiseTile(img_, tileIndex, 54, 54, 58, 30, rng); break;
        default:           noiseTile(img_, tileIndex, 140, 140, 145, 20, rng); break;
    }
}

void Atlas::generateProcedural(uint64_t seed) {
    (void)seed;
    for (int id = 0; id < BLOCK_COUNT; ++id) {
        for (int face = 0; face < 3; ++face) {
            int tile = tileForBlock(static_cast<BlockId>(id), face);
            drawTile(tile, static_cast<BlockId>(id), face);
        }
    }
}

} // namespace craftya
