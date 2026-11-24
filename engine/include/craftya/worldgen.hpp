// Craftya Engine — dünya oluşumu (biyom, mağara, maden, ağaç, yarık)
//
// Tasarım notları:
//  * Her şey tohumdan deterministik — aynı tohum aynı dünyayı verir.
//  * Yapılar (ağaç, kaktüs) chunk sınırını aşabilir; bu yüzden her chunk
//    komşu kolonları da tarar ve SADECE kendi içine düşen blokları yazar.
//  * Hiçbir adım diğerinin sonucuna bakmaz (saf fonksiyon) → paralel güvenli.
#pragma once
#include "craftya/blocks_gen.hpp"
#include "craftya/core.hpp"
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace craftya {
namespace worldgen {

// ---------------- biyomlar ----------------
enum class Biome : uint8_t {
    Ocean = 0, Beach, Plains, Forest, Taiga, Desert, Mountains, Swamp, Savanna, Badlands, Count
};

inline const char* biomeName(Biome b) {
    switch (b) {
        case Biome::Ocean:     return "Okyanus";
        case Biome::Beach:     return "Plaj";
        case Biome::Plains:    return "Ova";
        case Biome::Forest:    return "Orman";
        case Biome::Taiga:     return "Tayga";
        case Biome::Desert:    return "Çöl";
        case Biome::Mountains: return "Dağlar";
        case Biome::Swamp:     return "Bataklık";
        case Biome::Savanna:   return "Savan";
        case Biome::Badlands:  return "Kızıl Kayalık";
        default:               return "?";
    }
}

// ---------------- deterministik hash ----------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint32_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return static_cast<uint32_t>(s >> 32);
    }
    // [0,1)
    float unit() { return static_cast<float>(next() & 0xFFFFFF) / 16777216.f; }
    float range(float a, float b) { return a + (b - a) * unit(); }
    int irange(int a, int b) { return a + static_cast<int>(next() % static_cast<uint32_t>(b - a + 1)); }
};

inline uint64_t blockSeed(int x, int y, int z, uint64_t worldSeed) {
    uint64_t h = worldSeed;
    h ^= static_cast<uint64_t>(static_cast<int64_t>(x)) * 0x9E3779B97F4A7C15ull;
    h ^= static_cast<uint64_t>(static_cast<int64_t>(y)) * 0xC2B2AE3D27D4EB4Full;
    h ^= static_cast<uint64_t>(static_cast<int64_t>(z)) * 0x165667B19E3779F9ull;
    h ^= h >> 31; h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    return h;
}

inline float blockUnit(int x, int y, int z, uint64_t seed) {
    return static_cast<float>(blockSeed(x, y, z, seed) & 0xFFFFFF) / 16777216.f;
}

// ---------------- gelişmiş gürültü ----------------
class WNoise {
public:
    explicit WNoise(uint64_t seed = 1337) : seed_(seed) {}

    // 2B simlex benzeri gradyan gürültüsü — value noise'a göre eksen yanlılığı yok
    float noise2(float x, float z) const {
        float s = (x + z) * 0.36602540378f;
        int i = fastFloor(x + s), j = fastFloor(z + s);
        float t = (i + j) * 0.2113248654f;
        float X0 = i - t, Z0 = j - t;
        float x0 = x - X0, z0 = z - Z0;
        int i1 = (x0 > z0) ? 1 : 0;
        int j1 = (x0 > z0) ? 0 : 1;
        float x1 = x0 - i1 + 0.2113248654f, z1 = z0 - j1 + 0.2113248654f;
        float x2 = x0 - 1.f + 0.4226497308f, z2 = z0 - 1.f + 0.4226497308f;

        auto grad = [&](int gi, int gj, float gx, float gz) {
            uint32_t h = static_cast<uint32_t>(hashInt(gi, gj));
            float ang = static_cast<float>(h & 0xFFFF) / 65536.f * 6.2831853f;
            return std::cos(ang) * gx + std::sin(ang) * gz;
        };
        float n0 = 0.f, n1 = 0.f, n2 = 0.f;
        float t0 = 0.5f - x0*x0 - z0*z0;
        if (t0 > 0) { t0 *= t0; n0 = t0 * t0 * grad(i, j, x0, z0); }
        float t1 = 0.5f - x1*x1 - z1*z1;
        if (t1 > 0) { t1 *= t1; n1 = t1 * t1 * grad(i + i1, j + j1, x1, z1); }
        float t2 = 0.5f - x2*x2 - z2*z2;
        if (t2 > 0) { t2 *= t2; n2 = t2 * t2 * grad(i + 1, j + 1, x2, z2); }
        return (n0 + n1 + n2) * 70.f;   // ~[-1,1]
    }

    float fbm2(float x, float z, int oct = 4, float lac = 2.f, float gain = 0.5f) const {
        float sum = 0, amp = 1, norm = 0, f = 1;
        for (int i = 0; i < oct; ++i) { sum += amp * noise2(x * f, z * f); norm += amp; amp *= gain; f *= lac; }
        return norm > 0 ? sum / norm : 0.f;
    }

    // 3B value noise (mağara/maden için yeterli, hızlı)
    float noise3(float x, float y, float z) const {
        int xi = fastFloor(x), yi = fastFloor(y), zi = fastFloor(z);
        float fx = x - xi, fy = y - yi, fz = z - zi;
        float u = fade(fx), v = fade(fy), w = fade(fz);
        auto h = [&](int a, int b, int c) { return hash3(a, b, c); };
        float c000 = h(xi,yi,zi),     c100 = h(xi+1,yi,zi);
        float c010 = h(xi,yi+1,zi),   c110 = h(xi+1,yi+1,zi);
        float c001 = h(xi,yi,zi+1),   c101 = h(xi+1,yi,zi+1);
        float c011 = h(xi,yi+1,zi+1), c111 = h(xi+1,yi+1,zi+1);
        float x00 = lerp(c000, c100, u), x10 = lerp(c010, c110, u);
        float x01 = lerp(c001, c101, u), x11 = lerp(c011, c111, u);
        return lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
    }
    float fbm3(float x, float y, float z, int oct = 3) const {
        float sum = 0, amp = 1, norm = 0, f = 1;
        for (int i = 0; i < oct; ++i) { sum += amp * noise3(x*f, y*f, z*f); norm += amp; amp *= 0.5f; f *= 2.f; }
        return norm > 0 ? sum / norm : 0.f;
    }

private:
    static int fastFloor(float v) { int i = static_cast<int>(v); return (v < i) ? i - 1 : i; }
    static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }
    uint64_t hashInt(int x, int z) const {
        uint64_t h = seed_;
        h ^= static_cast<uint64_t>(static_cast<int64_t>(x)) * 0x9E3779B97F4A7C15ull;
        h ^= static_cast<uint64_t>(static_cast<int64_t>(z)) * 0xC2B2AE3D27D4EB4Full;
        h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33;
        return h;
    }
    float hash3(int x, int y, int z) const {
        return static_cast<float>(blockSeed(x, y, z, seed_) & 0xFFFFFF) / 16777216.f;
    }
    uint64_t seed_;
};

struct TreeSpec {
    bool      exists = false;
    int       type = 0;       // 0=meşe 1=huş 2=ladin 3=bambu 4=akasya 5=karaağaç
    int       height = 6;
    float     lean = 0.f;     // gövde eğimi (radyan)
    int       leanDir = 0;
    float     crownRadius = 2.6f;
    bool      blobby = true;  // yuvarlak taç mı, katman mı
    int       branches = 0;
    uint64_t  rngSeed = 0;
};


// ---------------- dünya şekli ----------------
struct Column {
    int   height = 64;      // yüzey yüksekliği
    int   seaFloor = 40;
    Biome biome = Biome::Plains;
    bool  underwater = false;
    float temperature = 0.5f;
    float humidity = 0.5f;
    int   biomeBlend = 0;
};

class Generator {
public:
    static constexpr int SEA_LEVEL = 62;

    explicit Generator(uint64_t seed = 424242)
        : seed_(seed),
          nCont_(seed ^ 0x1D1D1D1Dull),
          nEro_(seed ^ 0x2E2E2E2Eull),
          nPeak_(seed ^ 0x3F3F3F3Full),
          nTemp_(seed ^ 0x4A4A4A4Aull),
          nHum_(seed ^ 0x5B5B5B5Bull),
          nCave_(seed ^ 0x6C6C6C6Cull),
          nCave2_(seed ^ 0x7D7D7D7Dull),
          nRavine_(seed ^ 0x8E8E8E8Eull),
          nOre_(seed ^ 0x9F9F9F9Full),
          nDetail_(seed ^ 0xA0A0A0A0ull) {}

    uint64_t seed() const { return seed_; }

    // --- sütun bilgisi (biyom + yükseklik) ---
    Column column(int wx, int wz) const {
        Column c;

        // kıtasal: okyanus ↔ kara
        float cont = nCont_.fbm2(wx * 0.0018f, wz * 0.0018f, 4);
        // erozyon: düz ova ↔ engebeli
        float ero = nEro_.fbm2(wx * 0.0032f, wz * 0.0032f, 4);
        // zirveler: keskin dağlar
        float peak = nPeak_.fbm2(wx * 0.0060f, wz * 0.0060f, 3);

        float base = SEA_LEVEL;
        base += cont * 26.f;                       // kıta yüksekliği
        base += ero * 9.f;                         // engebe
        // dağ sırtları: peak yüksekse keskinleş
        float ridge = 1.f - std::fabs(peak);
        ridge = ridge * ridge;
        float mountainMask = std::clamp((cont + 0.15f) * 1.6f, 0.f, 1.f);
        base += ridge * 34.f * mountainMask * std::clamp(ero + 0.4f, 0.f, 1.f);
        base += nDetail_.fbm2(wx * 0.05f, wz * 0.05f, 2) * 1.6f;   // mikro kabartma

        c.height = std::clamp(static_cast<int>(std::lround(base)), 4, 190);
        c.seaFloor = static_cast<int>(SEA_LEVEL + cont * 26.f - 14.f);
        c.underwater = base < SEA_LEVEL - 0.5f;

        // sıcaklık/nem: enlem + gürültü
        c.temperature = std::clamp(0.5f + nTemp_.fbm2(wx * 0.00045f, wz * 0.00045f, 3) * 1.15f, 0.f, 1.f);
        c.humidity    = std::clamp(0.5f + nHum_.fbm2(wx * 0.00060f, wz * 0.00060f, 3) * 1.15f, 0.f, 1.f);

        c.biome = pickBiome(c);
        return c;
    }

    float heightAt(int wx, int wz) const { return static_cast<float>(column(wx, wz).height); }

    // --- mağara / yarık testi ---
    bool isCave(int wx, int wy, int wz, int surface) const {
        if (wy <= 1) return false;                       // yatak kayası
        // YÜZEYİN 5 BLOK ALTINA KADAR MAĞARA YOK — aksi halde çim örtüsü oyuluyor
        // ve gün ışığı göremeyen dev çukurlar oluşuyor.
        if (wy > surface - 5) return false;
        float depthFade = std::clamp(static_cast<float>(surface - wy) / 6.f, 0.f, 1.f);

        // spagetti mağara: iki gürültünün sıfıra yakın kesişimi
        float n1 = nCave_.fbm3(wx * 0.013f, wy * 0.021f, wz * 0.013f, 3);
        float n2 = nCave2_.fbm3(wx * 0.013f, wy * 0.021f, wz * 0.013f, 3);
        float tunnel = std::fabs(n1) + std::fabs(n2);
        if (tunnel < 0.055f) return true;

        // peynir mağara: derinlerde geniş odalar
        if (wy < 48) {
            float cheese = nCave_.fbm3(wx * 0.007f, wy * 0.011f, wz * 0.007f, 3);
            float thr = 0.42f - static_cast<float>(48 - wy) * 0.0018f;
            if (cheese > thr) return true;
        }

        // yarık: dikey düzlem kesişimi (nadir, derin)
        float rv = nRavine_.fbm2(wx * 0.0045f, wz * 0.0045f, 2);
        if (std::fabs(rv) < 0.014f && wy < surface - 8 && depthFade > 0.3f) return true;

        return false;
    }

    // --- madde: bu blok cevher mi? ---
    BlockId oreAt(int wx, int wy, int wz, int surface) const {
        if (wy <= 1) return AIR;
        float r = blockUnit(wx, wy, wz, seed_ ^ 0xABCDull);

        // kömür: yüzeye yakın, bol
        if (wy < surface - 4 && wy > 20) {
            if (r < 0.014f) return COAL_ORE;
        }
        // bakır: orta derinlik
        if (wy < 70 && wy > 18) {
            if (r > 0.014f && r < 0.026f) return COPPER_ORE;
        }
        // demir
        if (wy < 64 && wy > 10) {
            if (r > 0.026f && r < 0.036f) return IRON_ORE;
        }
        // altın: derin
        if (wy < 34) {
            if (r > 0.036f && r < 0.042f) return GOLD_ORE;
        }
        // kızıltaş: derin, kümeler
        if (wy < 20) {
            if (r > 0.042f && r < 0.052f) return REDSTONE_ORE;
        }
        // elmas: çok derin, nadir
        if (wy < 16) {
            if (r > 0.052f && r < 0.0555f) return DIAMOND_ORE;
        }
        // lapis: dağlarda derin
        if (wy < 30) {
            if (r > 0.0555f && r < 0.0585f) return LAPIS_ORE;
        }
        // zümrüt: sadece dağ biyomunun derininde
        if (wy < 40 && (nOre_.noise2(wx * 0.02f, wz * 0.02f) > 0.55f)) {
            if (r > 0.0585f && r < 0.0598f) return EMERALD_ORE;
        }
        return AIR;
    }

    // --- yüzey blokları ---
    BlockId surfaceBlock(const Column& c, int wy, BlockId defaultTop) const {
        if (wy > c.height) return AIR;
        if (wy == c.height) return defaultTop;
        int d = c.height - wy;
        if (d <= 3) {
            switch (c.biome) {
                case Biome::Desert:    return SAND;
                case Biome::Beach:     return SAND;
                case Biome::Badlands:  return RED_SAND;
                case Biome::Taiga:     return DIRT;
                case Biome::Mountains: return (c.height > 120 ? STONE : DIRT);
                case Biome::Swamp:     return DIRT;
                case Biome::Savanna:   return DIRT;
                default:               return DIRT;
            }
        }
        if (d <= 4 && (c.biome == Biome::Desert || c.biome == Biome::Beach)) return SANDSTONE;
        return STONE;
    }

    Biome pickBiome(const Column& c) const {
        if (c.underwater) {
            if (c.height < SEA_LEVEL - 8) return Biome::Ocean;
            return Biome::Beach;
        }
        if (c.height > 120) return Biome::Mountains;
        if (c.height <= SEA_LEVEL + 2) {
            if (c.temperature > 0.66f && c.humidity < 0.34f) return Biome::Desert;
            return Biome::Beach;
        }
        // sıcaklık/nem uzayında ayrık bölgeler (kenarlarda keskin geçiş)
        if (c.temperature < 0.28f)                          return Biome::Taiga;
        if (c.temperature > 0.68f) {
            if (c.humidity < 0.36f)                         return Biome::Badlands;
            if (c.humidity < 0.52f)                         return Biome::Desert;
            return Biome::Savanna;
        }
        if (c.temperature > 0.56f && c.humidity < 0.40f)    return Biome::Savanna;
        if (c.humidity > 0.70f && c.height < SEA_LEVEL + 8) return Biome::Swamp;
        if (c.humidity > 0.55f)                             return Biome::Forest;
        return Biome::Plains;
    }

    // ---------------- AĞAÇLAR ----------------
    // Her ağaç tohumdan türetilir → her biri farklı boy, gövde eğimi, dal yapısı,
    // taç şekli. Aynı tür iki ağaç asla aynı görünmez.
    // Ağaç adayı — kolon bilgisi GEREKTİRMEZ (ucuz). Sadece ızgara + tohum.
    // Dolu dönerse gerçek kolon okunup ağaç yerleştirilir.
    struct Candidate { bool exists; int x, z; uint64_t seed; };
    Candidate treeCandidate(int gx, int gz) const {
        uint64_t s = blockSeed(gx, 0, gz, seed_ ^ 0xA11CEull);
        float roll = static_cast<float>(s & 0xFFFF) / 65535.f;
        if (roll > 0.06f) return {false, 0, 0, 0};   // en yoğun biyomdan bile fazla
        int ox = static_cast<int>((s >> 16) & 3);
        int oz = static_cast<int>((s >> 18) & 3);
        return {true, gx * 4 - 3 + ox, gz * 4 - 3 + oz, s};
    }

    TreeSpec treeAt(int wx, int wz, const Column& c) const {
        TreeSpec t;
        // yoğunluk biyoma göre
        float density = 0.f;
        switch (c.biome) {
            case Biome::Forest:    density = 0.055f; break;
            case Biome::Taiga:     density = 0.038f; break;
            case Biome::Plains:    density = 0.007f; break;
            case Biome::Swamp:     density = 0.030f; break;
            case Biome::Savanna:   density = 0.006f; break;
            case Biome::Desert:    density = 0.004f; break;
            case Biome::Mountains: density = 0.004f; break;
            case Biome::Badlands:  density = 0.002f; break;
            default:               density = 0.f;     break;
        }
        if (c.underwater || c.height > 128) return t;
        if (c.biome == Biome::Beach || c.biome == Biome::Ocean) return t;

        // 4 blokluk ızgara → ağaçlar üst üste binmesin
        int gx = (wx >= 0 ? wx : wx - 3) / 4;
        int gz = (wz >= 0 ? wz : wz - 3) / 4;
        uint64_t s = blockSeed(gx, 0, gz, seed_ ^ 0xA11CEull);
        float roll = static_cast<float>(s & 0xFFFF) / 65535.f;
        if (roll > density) return t;
        // ama sadece ızgaranın tek bir kolonunda doğsun
        int ox = static_cast<int>((s >> 16) & 3);
        int oz = static_cast<int>((s >> 18) & 3);
        if (wx != gx * 4 - 3 + ox || wz != gz * 4 - 3 + oz) return t;

        Rng r(s);
        t.exists = true;
        t.rngSeed = s;

        // tür seçimi
        if (c.biome == Biome::Taiga)          t.type = 2;
        else if (c.biome == Biome::Swamp)     t.type = 0;
        else if (c.biome == Biome::Desert)    t.type = 4;
        else if (c.biome == Biome::Savanna)   t.type = 4;
        else if (c.biome == Biome::Forest)    t.type = (r.unit() < 0.35f) ? 1 : 0;
        else if (c.biome == Biome::Badlands)  t.type = 3;
        else                                  t.type = (r.unit() < 0.22f) ? 5 : 0;

        // boy: türe göre aralık, her ağaçta farklı
        switch (t.type) {
            case 0: t.height = r.irange(5, 9);  t.crownRadius = r.range(2.4f, 3.4f); break;
            case 1: t.height = r.irange(6, 10); t.crownRadius = r.range(2.0f, 2.8f); break;
            case 2: t.height = r.irange(9, 16); t.crownRadius = r.range(2.0f, 3.0f); break;
            case 3: t.height = r.irange(6, 13); t.crownRadius = 0.8f; break;
            case 4: t.height = r.irange(4, 6);  t.crownRadius = r.range(3.2f, 4.4f); break;
            case 5: t.height = r.irange(7, 12); t.crownRadius = r.range(2.6f, 3.8f); break;
            case 6: t.height = r.irange(4, 7);  t.crownRadius = r.range(2.2f, 3.0f); break;
        }
        // eğim: %30 ağaç hafif yatık
        if (r.unit() < 0.30f) {
            t.lean = r.range(0.06f, 0.22f);
            t.leanDir = r.irange(0, 3);
        }
        t.branches = static_cast<int>(r.unit() * 3.f);
        t.blobby = (t.type == 0 || t.type == 5);
        return t;
    }

    // Ağaç gövdesi/kökü nerede başlar (zemin yüksekliği + 1)
    BlockId logFor(int type) const {
        switch (type) {
            case 1: return BIRCH_LOG;
            case 2: return SPRUCE_LOG;
            case 3: return BAMBOO;
            case 4: return ACACIA_LOG;
            case 5: return DARK_OAK_LOG;
            default: return OAK_LOG;
        }
    }
    BlockId leavesFor(int type) const {
        switch (type) {
            case 1: return BIRCH_LEAVES;
            case 2: return SPRUCE_LEAVES;
            case 3: return OAK_LEAVES;
            case 4: return ACACIA_LEAVES;
            case 5: return DARK_OAK_LEAVES;
            default: return OAK_LEAVES;
        }
    }

private:
    uint64_t seed_;
    WNoise nCont_, nEro_, nPeak_, nTemp_, nHum_;
    WNoise nCave_, nCave2_, nRavine_, nOre_, nDetail_;
};

} // namespace worldgen
} // namespace craftya
