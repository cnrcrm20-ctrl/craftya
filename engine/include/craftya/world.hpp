// Craftya Engine — dünya: chunk (palet sıkıştırma), gürültü, dünya üretimi
#pragma once
#include "craftya/core.hpp"
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
        : noise_(seed), seed_(seed), gen_(generate_terrain) {}

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
    float heightAt(int wx, int wz) const {
        float base = noise_.fbm2(wx * 0.008f, wz * 0.008f, 4) * 40.0f;
        float hills = noise_.fbm2(wx * 0.03f, wz * 0.03f, 3) * 8.0f;
        return 24.0f + base + hills;
    }

private:
    static int floordiv(int a, int b) { return a >= 0 ? a / b : -(((-a) + b - 1) / b); }

    void generate(Chunk& c) {
        if (!gen_) { c.clearDirty(); return; }   // boş dünya (arena/test)
        ChunkPos p = c.pos();
        for (int lx = 0; lx < CHUNK_W; ++lx) {
            for (int lz = 0; lz < CHUNK_W; ++lz) {
                int wx = p.x * CHUNK_W + lx;
                int wz = p.z * CHUNK_W + lz;
                int h = static_cast<int>(heightAt(wx, wz));
                for (int ly = 0; ly < CHUNK_H; ++ly) {
                    int wy = p.y * CHUNK_H + ly;
                    BlockId id = AIR;
                    if (wy <= h) {
                        if (wy == h) id = GRASS;
                        else if (wy > h - 4) id = DIRT;
                        else id = STONE;
                        if (wy <= 0) id = BEDROCK;
                    } else if (wy <= 26) {
                        id = WATER;   // deniz seviyesi
                    }
                    c.set(lx, ly, lz, id);
                }
            }
        }
        c.clearDirty();
    }

    Noise noise_;
    uint64_t seed_;
    bool gen_ = true;
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> chunks_;
};

} // namespace craftya
