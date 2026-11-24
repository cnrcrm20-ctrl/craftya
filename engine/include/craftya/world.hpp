// Craftya Engine — dünya: chunk (palet sıkıştırma), gürültü, dünya üretimi
#pragma once
#include "craftya/core.hpp"
#include "craftya/worldgen.hpp"
#include <unordered_map>
#include <memory>
#include <cstring>

namespace craftya {

// ---------- hash ----------
struct ChunkPos {
    int x = 0, y = 0, z = 0;
    bool operator==(const ChunkPos& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct ChunkPosHash {
    size_t operator()(const ChunkPos& p) const {
        // 21 bit shiftli karışım — negatif koordinatlarda da iyi dağılır
        size_t h = 1469598103934665603ull;
        auto mix = [&h](int v) {
            h ^= static_cast<uint64_t>(static_cast<int64_t>(v));
            h *= 1099511628211ull;
        };
        mix(p.x); mix(p.y); mix(p.z);
        return h;
    }
};

// ---------- 16x16x16 chunk, palet sıkıştırmalı ----------
// Depolama: 4096 blok. Palet <=32 tip ise 5 bit/blok (2.5 KB), <=256 ise 8 bit (4 KB).
class Chunk {
public:
    static constexpr int BITS5_MAX = 32;

    Chunk(ChunkPos pos) : pos_(pos) {
        palette_.push_back(AIR);
        bits_.assign(kPacked5Bytes, 0);   // 5 bit/blok → 2561 bayt
        light_.assign(CHUNK_VOL, 0);
        packed5_ = true;
    }

    ChunkPos pos() const { return pos_; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    BlockId get(int lx, int ly, int lz) const {
        if (lx < 0 || lx >= CHUNK_W || ly < 0 || ly >= CHUNK_H || lz < 0 || lz >= CHUNK_W)
            return AIR;
        int idx = (ly * CHUNK_W + lz) * CHUNK_W + lx;
        uint16_t pi = packed5_ ? get5(idx) : get8(idx);
        return pi < palette_.size() ? palette_[pi] : AIR;
    }

    void set(int lx, int ly, int lz, BlockId id) {
        if (lx < 0 || lx >= CHUNK_W || ly < 0 || ly >= CHUNK_H || lz < 0 || lz >= CHUNK_W)
            return;
        int idx = (ly * CHUNK_W + lz) * CHUNK_W + lx;
        uint16_t pi = paletteIndex(id);
        if (packed5_ && pi >= BITS5_MAX) repackTo8();
        if (packed5_) set5(idx, pi); else set8(idx, pi);
        dirty_ = true;
        meshed_ = false;
    }

    // --- ışık (4 bit sky + 4 bit block = 1 bayt/blok) ---
    uint8_t skyLight(int lx, int ly, int lz) const {
        if (lx < 0 || lx >= CHUNK_W || ly < 0 || ly >= CHUNK_H || lz < 0 || lz >= CHUNK_W) return 0;
        return light_[(ly * CHUNK_W + lz) * CHUNK_W + lx] >> 4;
    }
    uint8_t blockLightAt(int lx, int ly, int lz) const {
        if (lx < 0 || lx >= CHUNK_W || ly < 0 || ly >= CHUNK_H || lz < 0 || lz >= CHUNK_W) return 0;
        return light_[(ly * CHUNK_W + lz) * CHUNK_W + lx] & 0xF;
    }
    void setSkyLight(int lx, int ly, int lz, uint8_t v) {
        if (lx < 0 || lx >= CHUNK_W || ly < 0 || ly >= CHUNK_H || lz < 0 || lz >= CHUNK_W) return;
        auto& cell = light_[(ly * CHUNK_W + lz) * CHUNK_W + lx];
        cell = static_cast<uint8_t>((cell & 0x0F) | ((v & 0xF) << 4));
    }
    void setBlockLight(int lx, int ly, int lz, uint8_t v) {
        if (lx < 0 || lx >= CHUNK_W || ly < 0 || ly >= CHUNK_H || lz < 0 || lz >= CHUNK_W) return;
        auto& cell = light_[(ly * CHUNK_W + lz) * CHUNK_W + lx];
        cell = static_cast<uint8_t>((cell & 0xF0) | (v & 0x0F));
    }
    bool lightReady() const { return lightReady_; }
    void markLightReady() { lightReady_ = true; }

    bool needsMesh() const { return !meshed_; }
    void markMeshed() { meshed_ = true; }
    void markDirty() { dirty_ = true; meshed_ = false; }

    size_t paletteSize() const { return palette_.size(); }
    bool packed5() const { return packed5_; }
    size_t bytes() const { return bits_.size(); }

private:
    static constexpr size_t kPacked5Bytes = (CHUNK_VOL * 5 + 7) / 8;  // 2561
    uint16_t paletteIndex(BlockId id) {
        for (size_t i = 0; i < palette_.size(); ++i)
            if (palette_[i] == id) return static_cast<uint16_t>(i);
        palette_.push_back(id);
        return static_cast<uint16_t>(palette_.size() - 1);
    }
    uint16_t get5(int idx) const {
        size_t bit = static_cast<size_t>(idx) * 5;
        size_t byte = bit >> 3;
        int shift = static_cast<int>(bit & 7);
        uint16_t v = bits_[byte];
        if (byte + 1 < bits_.size()) v |= static_cast<uint16_t>(bits_[byte + 1]) << 8;
        return static_cast<uint16_t>((v >> shift) & 0x1F);
    }
    void set5(int idx, uint16_t val) {
        size_t bit = static_cast<size_t>(idx) * 5;
        size_t byte = bit >> 3;
        int shift = static_cast<int>(bit & 7);
        uint16_t v = bits_[byte];
        if (byte + 1 < bits_.size()) v |= static_cast<uint16_t>(bits_[byte + 1]) << 8;
        v = static_cast<uint16_t>((v & ~(0x1Fu << shift)) | ((val & 0x1F) << shift));
        bits_[byte] = static_cast<uint8_t>(v & 0xFF);
        if (byte + 1 < bits_.size()) bits_[byte + 1] = static_cast<uint8_t>(v >> 8);
    }
    uint16_t get8(int idx) const { return bits_[static_cast<size_t>(idx)]; }
    void set8(int idx, uint16_t val) { bits_[static_cast<size_t>(idx)] = static_cast<uint8_t>(val & 0xFF); }

    void repackTo8() {
        std::vector<uint8_t> next(CHUNK_VOL);
        for (int i = 0; i < CHUNK_VOL; ++i)
            next[i] = static_cast<uint8_t>(get5(i));
        bits_.swap(next);
        packed5_ = false;
    }

    ChunkPos pos_;
    std::vector<BlockId> palette_;
    std::vector<uint8_t> bits_;
    std::vector<uint8_t> light_;      // 4096 bayt (sky<<4 | block)
    bool packed5_ = true;
    bool dirty_ = true;
    bool meshed_ = false;
    bool lightReady_ = false;
};

// ---------- gürültü (deterministik, tohumlu) ----------
class Noise {
public:
    explicit Noise(uint64_t seed = 1337) : seed_(seed) {}
    // 2B value-noise + fBm
    float noise2(float x, float z) const {
        int xi = static_cast<int>(std::floor(x));
        int zi = static_cast<int>(std::floor(z));
        float fx = x - xi, fz = z - zi;
        float u = fade(fx), v = fade(fz);
        float a = hash2(xi, zi), b = hash2(xi + 1, zi);
        float c = hash2(xi, zi + 1), d = hash2(xi + 1, zi + 1);
        return lerp(lerp(a, b, u), lerp(c, d, u), v);
    }
    float fbm2(float x, float z, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) const {
        float sum = 0, amp = 1, norm = 0, freq = 1;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * noise2(x * freq, z * freq);
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return norm > 0 ? sum / norm : 0.f;
    }
private:
    static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }
    float hash2(int x, int z) const {
        uint64_t h = seed_;
        h ^= static_cast<uint64_t>(static_cast<int64_t>(x)) * 0x9E3779B97F4A7C15ull;
        h ^= static_cast<uint64_t>(static_cast<int64_t>(z)) * 0xC2B2AE3D27D4EB4Full;
        h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull;
        h ^= h >> 32;
        return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
    }
    uint64_t seed_;
};

// ---------- dünya ----------
class World {
public:
    explicit World(uint64_t seed = 424242, bool generate_terrain = true)
        : gen_(seed), seed_(seed), genOn_(generate_terrain) {}

    Chunk* chunkAt(const ChunkPos& p) {
        auto it = chunks_.find(p);
        return it == chunks_.end() ? nullptr : it->second.get();
    }
    Chunk* getOrCreate(const ChunkPos& p) {
        auto it = chunks_.find(p);
        if (it != chunks_.end()) return it->second.get();
        auto c = std::make_unique<Chunk>(p);
        generate(*c);
        Chunk* raw = c.get();
        chunks_[p] = std::move(c);
        return raw;
    }

    // dünya koordinatında blok
    BlockId getBlock(int wx, int wy, int wz) {
        ChunkPos cp{floordiv(wx, CHUNK_W), floordiv(wy, CHUNK_H), floordiv(wz, CHUNK_W)};
        Chunk* c = chunkAt(cp);
        if (!c) return AIR;
        return c->get(wx - cp.x * CHUNK_W, wy - cp.y * CHUNK_H, wz - cp.z * CHUNK_W);
    }
    // yazma: chunk yoksa oluşturur; komşu chunk'ları da kirletir (yüz gizleme için)
    void setBlock(int wx, int wy, int wz, BlockId id) {
        ChunkPos cp{floordiv(wx, CHUNK_W), floordiv(wy, CHUNK_H), floordiv(wz, CHUNK_W)};
        Chunk* c = getOrCreate(cp);
        int lx = wx - cp.x * CHUNK_W, ly = wy - cp.y * CHUNK_H, lz = wz - cp.z * CHUNK_W;
        c->set(lx, ly, lz, id);

        // chunk sınırındaysa komşuyu kirlet
        auto dirtyNeighbor = [&](int dx, int dy, int dz) {
            Chunk* n = chunkAt({cp.x + dx, cp.y + dy, cp.z + dz});
            if (n) n->markDirty();
        };
        if (lx == 0) dirtyNeighbor(-1, 0, 0);
        if (lx == CHUNK_W - 1) dirtyNeighbor(1, 0, 0);
        if (ly == 0) dirtyNeighbor(0, -1, 0);
        if (ly == CHUNK_H - 1) dirtyNeighbor(0, 1, 0);
        if (lz == 0) dirtyNeighbor(0, 0, -1);
        if (lz == CHUNK_W - 1) dirtyNeighbor(0, 0, 1);
    }

    // dünya koordinatında ışık (chunk yoksa 0)
    uint8_t skyLightAt(int wx, int wy, int wz) {
        ChunkPos cp{floordiv(wx, CHUNK_W), floordiv(wy, CHUNK_H), floordiv(wz, CHUNK_W)};
        Chunk* c = chunkAt(cp);
        if (!c) return 0;
        return c->skyLight(wx - cp.x * CHUNK_W, wy - cp.y * CHUNK_H, wz - cp.z * CHUNK_W);
    }
    uint8_t blockLightAt(int wx, int wy, int wz) {
        ChunkPos cp{floordiv(wx, CHUNK_W), floordiv(wy, CHUNK_H), floordiv(wz, CHUNK_W)};
        Chunk* c = chunkAt(cp);
        if (!c) return 0;
        return c->blockLightAt(wx - cp.x * CHUNK_W, wy - cp.y * CHUNK_H, wz - cp.z * CHUNK_W);
    }
    void setSkyLightAt(int wx, int wy, int wz, uint8_t v) {
        ChunkPos cp{floordiv(wx, CHUNK_W), floordiv(wy, CHUNK_H), floordiv(wz, CHUNK_W)};
        Chunk* c = chunkAt(cp);
        if (!c) return;
        c->setSkyLight(wx - cp.x * CHUNK_W, wy - cp.y * CHUNK_H, wz - cp.z * CHUNK_W, v);
    }
    void setBlockLightAt(int wx, int wy, int wz, uint8_t v) {
        ChunkPos cp{floordiv(wx, CHUNK_W), floordiv(wy, CHUNK_H), floordiv(wz, CHUNK_W)};
        Chunk* c = chunkAt(cp);
        if (!c) return;
        c->setBlockLight(wx - cp.x * CHUNK_W, wy - cp.y * CHUNK_H, wz - cp.z * CHUNK_W, v);
    }

    size_t chunkCount() const { return chunks_.size(); }
    uint64_t seed() const { return seed_; }

    const worldgen::Generator& generator() const { return gen_; }

    float heightAt(int wx, int wz) const { return gen_.heightAt(wx, wz); }
    worldgen::Biome biomeAt(int wx, int wz) const { return gen_.column(wx, wz).biome; }

    // ---------- dünya üretimi ----------
    void generate(Chunk& c) {
        if (!genOn_) { c.clearDirty(); return; }
        ChunkPos p = c.pos();

        // kolon bilgilerini bir kez hesapla (çok pahalı — tekrar tekrar çağırma)
        worldgen::Column cols[CHUNK_W][CHUNK_W];
        for (int lx = 0; lx < CHUNK_W; ++lx)
            for (int lz = 0; lz < CHUNK_W; ++lz)
                cols[lx][lz] = gen_.column(p.x * CHUNK_W + lx, p.z * CHUNK_W + lz);

        // 1) temel arazi + mağara + maden
        for (int lx = 0; lx < CHUNK_W; ++lx) {
            for (int lz = 0; lz < CHUNK_W; ++lz) {
                int wx = p.x * CHUNK_W + lx;
                int wz = p.z * CHUNK_W + lz;
                const worldgen::Column& col = cols[lx][lz];

                BlockId topBlock = topForBiome(col);
                for (int ly = 0; ly < CHUNK_H; ++ly) {
                    int wy = p.y * CHUNK_H + ly;
                    BlockId id = AIR;

                    if (wy <= col.height) {
                        id = gen_.surfaceBlock(col, wy, topBlock);
                        // mağara oy
                        if (gen_.isCave(wx, wy, wz, col.height)) {
                            id = (wy < 10) ? LAVA : AIR;
                        } else if (id == STONE) {
                            BlockId ore = gen_.oreAt(wx, wy, wz, col.height);
                            if (ore != AIR) id = ore;
                        }
                        if (wy <= 0) id = BEDROCK;
                    } else if (wy <= worldgen::Generator::SEA_LEVEL) {
                        id = WATER;
                    }
                    c.set(lx, ly, lz, id);
                }
            }
        }

        // 2) bitki örtüsü (yüzeydeki sütunlara)
        for (int lx = 0; lx < CHUNK_W; ++lx) {
            for (int lz = 0; lz < CHUNK_W; ++lz) {
                int wx = p.x * CHUNK_W + lx;
                int wz = p.z * CHUNK_W + lz;
                const worldgen::Column& col = cols[lx][lz];
                int ly = col.height + 1 - p.y * CHUNK_H;
                if (ly < 0 || ly >= CHUNK_H) continue;
                BlockId base = c.get(lx, ly - 1, lz);
                if (base != topForBiome(col)) continue;
                BlockId veg = vegetationFor(col, wx, wz);
                if (veg != AIR) c.set(lx, ly, lz, veg);
            }
        }

        // 3) ağaçlar — 4 blokluk ızgara üzerinden tara, kolonu sadece gerekince oku
        //    (komşu kolonlar da taranır → chunk sınırını aşan taçlar doğru çizilir)
        const int PAD = 6;   // en geniş taç yarıçapı + pay
        int gx0 = floorDiv(p.x * CHUNK_W - PAD, 4);
        int gx1 = floorDiv((p.x + 1) * CHUNK_W + PAD, 4);
        int gz0 = floorDiv(p.z * CHUNK_W - PAD, 4);
        int gz1 = floorDiv((p.z + 1) * CHUNK_W + PAD, 4);
        for (int gx = gx0; gx <= gx1; ++gx) {
            for (int gz = gz0; gz <= gz1; ++gz) {
                auto cand = gen_.treeCandidate(gx, gz);
                if (!cand.exists) continue;
                worldgen::Column col = gen_.column(cand.x, cand.z);
                auto tree = gen_.treeAt(cand.x, cand.z, col);
                if (!tree.exists) continue;
                placeTree(c, p, cand.x, cand.z, col, tree);
            }
        }

        c.clearDirty();
    }

private:
    static int floordiv(int a, int b) { return a >= 0 ? a / b : -(((-a) + b - 1) / b); }
    static int floorDiv(int a, int b) { return a >= 0 ? a / b : -(((-a) + b - 1) / b); }

    static BlockId topForBiome(const worldgen::Column& c) {
        switch (c.biome) {
            case worldgen::Biome::Desert:    return SAND;
            case worldgen::Biome::Beach:     return SAND;
            case worldgen::Biome::Badlands:  return RED_SAND;
            case worldgen::Biome::Ocean:     return (c.height < worldgen::Generator::SEA_LEVEL - 6) ? GRAVEL : SAND;
            case worldgen::Biome::Taiga:     return SNOW_BLOCK;
            case worldgen::Biome::Mountains: return (c.height > 112) ? SNOW_BLOCK : ((c.height > 96) ? STONE : GRASS_BLOCK);
            case worldgen::Biome::Swamp:     return GRASS_BLOCK;
            default:                         return GRASS_BLOCK;
        }
    }

    static BlockId vegetationFor(const worldgen::Column& c, int wx, int wz) {
        worldgen::Rng r(worldgen::blockSeed(wx, 7, wz, 0xBEEF));
        float u = r.unit();
        switch (c.biome) {
            case worldgen::Biome::Plains:
                if (u < 0.28f) return SHORT_GRASS;
                if (u < 0.34f) return TALL_GRASS;
                if (u < 0.40f) return (r.unit() < 0.5f) ? DANDELION : POPPY;
                return AIR;
            case worldgen::Biome::Forest:
                if (u < 0.22f) return SHORT_GRASS;
                if (u < 0.27f) return FERN;
                if (u < 0.31f) return (r.unit() < 0.5f) ? DANDELION : POPPY;
                if (u < 0.325f) return (r.unit() < 0.5f) ? BROWN_MUSHROOM : RED_MUSHROOM;
                return AIR;
            case worldgen::Biome::Taiga:
                if (u < 0.18f) return FERN;
                if (u < 0.22f) return SHORT_GRASS;
                return AIR;
            case worldgen::Biome::Swamp:
                if (u < 0.30f) return SHORT_GRASS;
                if (u < 0.34f) return (r.unit() < 0.6f) ? BROWN_MUSHROOM : RED_MUSHROOM;
                if (u < 0.37f) return SUGAR_CANE;
                return AIR;
            case worldgen::Biome::Desert:
                if (u < 0.012f) return DEAD_BUSH;
                if (u < 0.016f) return BAMBOO;
                return AIR;
            case worldgen::Biome::Savanna:
                if (u < 0.34f) return TALL_GRASS;
                return AIR;
            case worldgen::Biome::Badlands:
                if (u < 0.02f) return DEAD_BUSH;
                return AIR;
            default: return AIR;
        }
    }

    // Bir ağacı dünyaya yerleştir — sadece bu chunk'a düşen blokları yazar
    void placeTree(Chunk& c, const ChunkPos& cp, int tx, int tz,
                   const worldgen::Column& col, const worldgen::TreeSpec& t) {
        worldgen::Rng r(t.rngSeed ^ 0x5EED);
        int baseY = col.height + 1;
        int cxMin = cp.x * CHUNK_W, cxMax = cxMin + CHUNK_W - 1;
        int czMin = cp.z * CHUNK_W, czMax = czMin + CHUNK_W - 1;
        int cyMin = cp.y * CHUNK_H, cyMax = cyMin + CHUNK_H - 1;

        // gövde eğimi → her katmanda hafif kayma
        auto leanOffset = [&](int ly) -> std::pair<int,int> {
            if (t.lean <= 0.f) return {0, 0};
            float d = t.lean * static_cast<float>(ly);
            int dx = 0, dz = 0;
            switch (t.leanDir) {
                case 0: dx =  static_cast<int>(d); break;
                case 1: dx = -static_cast<int>(d); break;
                case 2: dz =  static_cast<int>(d); break;
                default: dz = -static_cast<int>(d); break;
            }
            return {dx, dz};
        };

        auto put = [&](int wx, int wy, int wz, BlockId id, bool overwrite) {
            if (wx < cxMin || wx > cxMax || wz < czMin || wz > czMax) return;
            if (wy < cyMin || wy > cyMax) return;
            int lx = wx - cxMin, ly = wy - cyMin, lz = wz - czMin;
            if (!overwrite) {
                BlockId cur = c.get(lx, ly, lz);
                if (cur != AIR && cur != SHORT_GRASS && cur != TALL_GRASS &&
                    cur != WATER && cur != FERN && cur != SNOW_LAYER) return;
            }
            c.set(lx, ly, lz, id);
        };

        // --- bambu: ince, uzun, yapraksız gövde + tepede küçük taç ---
        if (t.type == 3) {
            for (int i = 0; i < t.height; ++i) put(tx, baseY + i, tz, BAMBOO, false);
            put(tx, baseY + t.height, tz, OAK_LEAVES, false);
            put(tx + 1, baseY + t.height - 1, tz, OAK_LEAVES, false);
            put(tx - 1, baseY + t.height - 1, tz, OAK_LEAVES, false);
            return;
        }
        BlockId logB = gen_.logFor(t.type);
        BlockId leafB = gen_.leavesFor(t.type);

        // --- gövde ---
        int topY = baseY;
        for (int i = 0; i < t.height; ++i) {
            auto [ox, oz] = leanOffset(i);
            put(tx + ox, baseY + i, tz + oz, logB, true);
            topY = baseY + i;
        }

        // --- dallar (ladin/meşe için) ---
        for (int b = 0; b < t.branches; ++b) {
            int by = baseY + t.height * 2 / 3 + r.irange(0, 2);
            int dir = r.irange(0, 3);
            int len = r.irange(1, 2);
            int dx = (dir == 0) ? 1 : (dir == 1) ? -1 : 0;
            int dz = (dir == 2) ? 1 : (dir == 3) ? -1 : 0;
            auto [ox, oz] = leanOffset(by - baseY);
            for (int i = 1; i <= len; ++i)
                put(tx + ox + dx * i, by, tz + oz + dz * i, logB, false);
            // dal ucunda küçük yaprak kümesi
            put(tx + ox + dx * (len + 1), by, tz + oz + dz * (len + 1), leafB, false);
            put(tx + ox + dx * (len + 1), by + 1, tz + oz + dz * (len + 1), leafB, false);
        }

        // --- taç ---
        if (t.type == 2) {
            // ladin: koni biçimli katmanlar
            int layers = t.height / 2 + 1;
            for (int l = 0; l < layers; ++l) {
                int y = baseY + t.height - 1 - l;
                float rad = t.crownRadius * (1.f - static_cast<float>(l) / static_cast<float>(layers + 1));
                int ir = static_cast<int>(std::ceil(rad));
                for (int dx = -ir; dx <= ir; ++dx)
                    for (int dz = -ir; dz <= ir; ++dz) {
                        if (dx*dx + dz*dz > rad * rad) continue;
                        if (dx == 0 && dz == 0) continue;
                        put(tx + dx, y, tz + dz, leafB, false);
                    }
            }
        } else if (t.type == 4) {
            // akasya: yassı şemsiye taç, gövde çatalı
            int splitY = baseY + t.height - 2;
            auto [ox, oz] = leanOffset(t.height - 2);
            put(tx + ox + 1, splitY, tz + oz, logB, false);
            put(tx + ox - 1, splitY, tz + oz, logB, false);
            int ir = static_cast<int>(std::ceil(t.crownRadius));
            for (int dx = -ir; dx <= ir; ++dx)
                for (int dz = -ir; dz <= ir; ++dz) {
                    float d2 = static_cast<float>(dx*dx + dz*dz) / (t.crownRadius * t.crownRadius);
                    if (d2 > 1.f) continue;
                    put(tx + dx, topY + 1, tz + dz, leafB, false);
                    if (d2 < 0.45f) put(tx + dx, topY + 2, tz + dz, leafB, false);
                }
        } else if (t.blobby) {
            // meşe/huş/karaağaç: düzensiz küre — her ağaçta farklı
            float rad = t.crownRadius;
            int ir = static_cast<int>(std::ceil(rad)) + 1;
            for (int dy = -2; dy <= 2; ++dy) {
                int y = topY + dy;
                float layerRad = rad * (1.f - std::fabs(static_cast<float>(dy)) * 0.20f);
                if (layerRad <= 0.6f) continue;
                for (int dx = -ir; dx <= ir; ++dx)
                    for (int dz = -ir; dz <= ir; ++dz) {
                        // kenarları düzensizleştir (ağaç başına farklı gürültü)
                        float jitter = (worldgen::blockUnit(tx + dx, y, tz + dz, t.rngSeed) - 0.5f) * 1.1f;
                        float d2 = static_cast<float>(dx*dx + dz*dz);
                        if (d2 > (layerRad + jitter) * (layerRad + jitter)) continue;
                        if (dx == 0 && dz == 0 && dy <= 0) continue;   // gövde zaten orada
                        put(tx + dx, y, tz + dz, leafB, false);
                    }
            }
        }
    }

public:
    worldgen::Generator gen_;
    uint64_t seed_;
    bool genOn_ = true;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> chunks_;
};

} // namespace craftya
