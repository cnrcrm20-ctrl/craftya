// Craftya Engine — çekirdek tipler, sabitler, blok kayıt defteri
// Hedef: Android (arm64) + Windows. Bağımlılık yok, saf C++17.
#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>

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

// ---------- blok kimlikleri (Supabase block_types ile aynı) ----------
enum BlockId : uint16_t {
    AIR = 0, STONE = 1, DIRT = 2, GRASS = 3, COBBLE = 4, SAND = 5, GRAVEL = 6,
    OAK_LOG = 7, OAK_PLANKS = 8, OAK_LEAVES = 9, BIRCH_LOG = 10, BIRCH_PLANKS = 11,
    WATER = 12, LAVA = 13, GLASS = 14, BRICKS = 15, STONE_BRICKS = 16,
    COAL_ORE = 17, IRON_ORE = 18, GOLD_ORE = 19, DIAMOND_ORE = 20,
    SNOW = 21, ICE = 22, OBSIDIAN = 23, NETHERRACK = 24, GLOWSTONE = 25,
    TORCH = 26, CRAFTING_TABLE = 27, FURNACE = 28, CHEST = 29, BEDROCK = 30,
    BLOCK_COUNT = 31
};

// ---------- blok özellikleri (sunucudan gelen JSON'un derleme zamanı kopyası) ----------
struct BlockDef {
    BlockId id;
    const char* key;
    const char* name;
    bool opaque;    // ışığı keser, komşu yüz gizlenir
    bool solid;     // çarpışma var
    bool liquid;
    uint8_t light;  // kendi yaydığı ışık (0-15)
    float hardness;
};

inline const std::array<BlockDef, BLOCK_COUNT>& blockRegistry() {
    static const std::array<BlockDef, BLOCK_COUNT> reg = {{
        {AIR,           "air",           "Hava",            false, false, false,  0,   0.0f},
        {STONE,         "stone",         "Taş",             true,  true,  false,  0,   1.5f},
        {DIRT,          "dirt",          "Toprak",          true,  true,  false,  0,   0.5f},
        {GRASS,         "grass_block",   "Çim",             true,  true,  false,  0,   0.6f},
        {COBBLE,        "cobblestone",   "Arnavut",         true,  true,  false,  0,   2.0f},
        {SAND,          "sand",          "Kum",             true,  true,  false,  0,   0.5f},
        {GRAVEL,        "gravel",        "Çakıl",           true,  true,  false,  0,   0.6f},
        {OAK_LOG,       "oak_log",       "Meşe Kütük",      true,  true,  false,  0,   2.0f},
        {OAK_PLANKS,    "oak_planks",    "Meşe Tahta",      true,  true,  false,  0,   2.0f},
        {OAK_LEAVES,    "oak_leaves",    "Meşe Yaprak",     false, true,  false,  0,   0.2f},
        {BIRCH_LOG,     "birch_log",     "Huş Kütük",       true,  true,  false,  0,   2.0f},
        {BIRCH_PLANKS,  "birch_planks",  "Huş Tahta",       true,  true,  false,  0,   2.0f},
        {WATER,         "water",         "Su",              false, false, true,   0, 100.0f},
        {LAVA,          "lava",          "Lav",             false, false, true,  15, 100.0f},
        {GLASS,         "glass",         "Cam",             false, true,  false,  0,   0.3f},
        {BRICKS,        "bricks",        "Tuğla",           true,  true,  false,  0,   2.0f},
        {STONE_BRICKS,  "stone_bricks",  "Taş Tuğla",       true,  true,  false,  0,   1.5f},
        {COAL_ORE,      "coal_ore",      "Kömür",           true,  true,  false,  0,   3.0f},
        {IRON_ORE,      "iron_ore",      "Demir",           true,  true,  false,  0,   3.0f},
        {GOLD_ORE,      "gold_ore",      "Altın",           true,  true,  false,  0,   3.0f},
        {DIAMOND_ORE,   "diamond_ore",   "Elmas",           true,  true,  false,  0,   3.0f},
        {SNOW,          "snow",          "Kar",             true,  true,  false,  0,   0.3f},
        {ICE,           "ice",           "Buz",             false, true,  false,  0,   0.5f},
        {OBSIDIAN,      "obsidian",      "Obsidyen",        true,  true,  false,  0,  50.0f},
        {NETHERRACK,    "netherrack",    "Nether Taşı",     true,  true,  false,  0,   0.4f},
        {GLOWSTONE,     "glowstone",     "Işık Taşı",        true,  true,  false, 15,   0.3f},
        {TORCH,         "torch",         "Meşale",          false, false, false, 14,   0.1f},
        {CRAFTING_TABLE,"crafting_table","Tezgah",          true,  true,  false,  0,   2.5f},
        {FURNACE,       "furnace",       "Fırın",           true,  true,  false,  0,   3.5f},
        {CHEST,         "chest",         "Sandık",          true,  true,  false,  0,   2.5f},
        {BEDROCK,       "bedrock",       "Ana Kaya",        true,  true,  false,  0, 99999.0f},
    }};
    return reg;
}

inline const BlockDef& blockDef(BlockId id) {
    const auto& reg = blockRegistry();
    if (id < BLOCK_COUNT) return reg[id];
    return reg[0];
}
inline bool isOpaque(BlockId id)  { return blockDef(id).opaque; }
inline bool isSolid(BlockId id)   { return blockDef(id).solid; }
inline uint8_t blockLight(BlockId id) { return blockDef(id).light; }
inline bool isAir(BlockId id)     { return id == AIR; }

} // namespace craftya
