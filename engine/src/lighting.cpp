// Craftya Engine — ışık yayılımı uygulaması (BFS)
#include "craftya/lighting.hpp"
#include <vector>
#include <cmath>
#include <cstring>

namespace craftya {

namespace {

// dünya genelinde ışık deposu (chunk başına ayrı tutmak yerine seyrek harita:
// bellek dostu — yalnızca ışıklı hücreler saklanır)
struct LightKey {
    int x, y, z;
    bool operator==(const LightKey& o) const { return x==o.x&&y==o.y&&z==o.z; }
};
struct LightKeyHash {
    size_t operator()(const LightKey& k) const {
        size_t h = 0;
        h ^= static_cast<size_t>(k.x) * 73856093u;
        h ^= static_cast<size_t>(k.y) * 19349663u;
        h ^= static_cast<size_t>(k.z) * 83492791u;
        return h;
    }
};

struct Cell { int x, y, z; uint8_t level; };

constexpr int DX[6] = {1,-1,0,0,0,0};
constexpr int DY[6] = {0,0,1,-1,0,0};
constexpr int DZ[6] = {0,0,0,0,1,-1};

// basit yerel ışık haritası (chunk bazında 18x18x18 tampon)
class LocalLight {
public:
    static constexpr int P = 1;                        // dolgu
    static constexpr int W = CHUNK_W + P * 2;          // 18
    static constexpr int H = CHUNK_H + P * 2;

    LocalLight() { std::memset(sky_, 0, sizeof(sky_)); std::memset(blk_, 0, sizeof(blk_)); }

    int idx(int x, int y, int z) const {
        int ix = x + P, iy = y + P, iz = z + P;
        if (ix < 0 || ix >= W || iy < 0 || iy >= H || iz < 0 || iz >= W) return -1;
        return (iy * W + iz) * W + ix;
    }
    uint8_t& sky(int x,int y,int z){ int i=idx(x,y,z); return sky_[i<0?0:i]; }
    uint8_t& blk(int x,int y,int z){ int i=idx(x,y,z); return blk_[i<0?0:i]; }
    bool inside(int x,int y,int z) const { return idx(x,y,z) >= 0; }

private:
    uint8_t sky_[H * W * W];
    uint8_t blk_[H * W * W];
};

} // namespace

void LightEngine::computeChunk(World& world, Chunk& chunk) {
    ChunkPos cp = chunk.pos();
    LocalLight L;
    std::deque<Cell> skyQ, blkQ;

    // --- 1) tohumlama ---
    for (int ly = 0; ly < CHUNK_H; ++ly) {
        for (int lz = 0; lz < CHUNK_W; ++lz) {
            for (int lx = 0; lx < CHUNK_W; ++lx) {
                BlockId id = chunk.get(lx, ly, lz);
                int wx = cp.x * CHUNK_W + lx;
                int wy = cp.y * CHUNK_H + ly;
                int wz = cp.z * CHUNK_W + lz;

                // skylight tohumlama: gökyüzüne açık kolonlarda ışık 15.
                //   * hava hücresi → 15
                //   * yüzey bloğunun KENDİSİ → 15 (yoksa çim yüzü kapkara kalır)
                //   * yüzeyin 3 blok altına kadar sönümlü ışık (yatay yayılım için)
                {
                    bool openAbove = true;
                    for (int ay = wy + 1; ay < wy + 48 && openAbove; ++ay) {
                        BlockId above = world.getBlock(wx, ay, wz);
                        if (above != AIR && isOpaque(above)) openAbove = false;
                    }
                    if (openAbove) {
                        // Maske: üstü açık AMA kendisi opak olan hücre de 15 alır,
                        // sadece "bu hücre opak ve üstü kapalı" olan 0 kalır.
                        int lvl = 15;
                        L.sky(lx, ly, lz) = static_cast<uint8_t>(lvl);
                        skyQ.push_back({lx, ly, lz, static_cast<uint8_t>(lvl)});
                    }
                }
                // blok ışığı
                uint8_t emit = blockLight(id);
                if (emit > 0) {
                    L.blk(lx, ly, lz) = emit;
                    blkQ.push_back({lx, ly, lz, emit});
                }
            }
        }
    }

    // --- 2) BFS yayılım (skylight) ---
    auto spread = [&](std::deque<Cell>& q, bool isSky) {
        while (!q.empty()) {
            Cell c = q.front(); q.pop_front();
            for (int d = 0; d < 6; ++d) {
                int nx = c.x + DX[d], ny = c.y + DY[d], nz = c.z + DZ[d];
                if (!L.inside(nx, ny, nz)) continue;
                BlockId nid = chunk.get(nx, ny, nz);
                if (isOpaque(nid)) continue;
                // yayılma kaybı
                int loss = isSky && (d == 2) ? 0 : 1;   // yukarı yön skylight'ta kayıpsız
                int next = static_cast<int>(c.level) - loss;
                if (next <= 0) continue;
                uint8_t& target = isSky ? L.sky(nx, ny, nz) : L.blk(nx, ny, nz);
                if (target >= next) continue;
                target = static_cast<uint8_t>(next);
                q.push_back({nx, ny, nz, static_cast<uint8_t>(next)});
            }
        }
    };

    spread(skyQ, true);
    spread(blkQ, false);

    // --- 3) chunk'a yaz ---
    for (int ly = 0; ly < CHUNK_H; ++ly)
        for (int lz = 0; lz < CHUNK_W; ++lz)
            for (int lx = 0; lx < CHUNK_W; ++lx) {
                uint8_t s = L.sky(lx, ly, lz);
                uint8_t b = L.blk(lx, ly, lz);
                chunk.setSkyLight(lx, ly, lz, s);
                chunk.setBlockLight(lx, ly, lz, b);
            }
}


// ---------- bölgesel ışık: tek BFS, tüm bölge ----------
// Neden gerekli: computeChunk tek chunk üzerinde çalışır ve komşu chunk
// henüz üretilmemişse ışık oraya yayılamaz. Bölge bazlı hesap, sınırları
// doğal olarak aşar ve yüzey kararması (ağaç altı / tepe gölgesi) oluşmaz.
int LightEngine::computeRegion(World& world, ChunkPos center, int rx, int ry, int rz) {
    // bölge sınırları (dünya koordinatı)
    const int x0 = (center.x - rx) * CHUNK_W;
    const int x1 = (center.x + rx + 1) * CHUNK_W;
    const int y0 = (center.y - ry) * CHUNK_H;
    const int y1 = (center.y + ry + 1) * CHUNK_H;
    const int z0 = (center.z - rz) * CHUNK_W;
    const int z1 = (center.z + rz + 1) * CHUNK_W;

    const int WX = x1 - x0, WY = y1 - y0, WZ = z1 - z0;
    if (WX <= 0 || WY <= 0 || WZ <= 0) return 0;

    // bölge ışık tamponu
    std::vector<uint8_t> skyBuf(static_cast<size_t>(WX) * WY * WZ, 0);
    std::vector<uint8_t> blkBuf(static_cast<size_t>(WX) * WY * WZ, 0);
    auto idx = [&](int x, int y, int z) {
        return (static_cast<size_t>(y) * WZ + z) * WX + x;
    };

    // bölgede hangi chunk'lar var?
    std::vector<Chunk*> chunks;
    int litCount = 0;
    for (int cz = center.z - rz; cz <= center.z + rz; ++cz)
        for (int cx = center.x - rx; cx <= center.x + rx; ++cx)
            for (int cy = center.y - ry; cy <= center.y + ry; ++cy) {
                Chunk* c = world.chunkAt({cx, cy, cz});
                if (c) chunks.push_back(c);
            }
    if (chunks.empty()) return 0;

    struct Cell { int x, y, z; uint8_t level; };
    std::deque<Cell> skyQ, blkQ;

    // --- 1) tohumlama: gökyüzüne açık kolonlar ---
    for (int z = 0; z < WZ; ++z) {
        for (int x = 0; x < WX; ++x) {
            bool blocked = false;
            for (int y = WY - 1; y >= 0; --y) {
                int wy = y0 + y;
                BlockId id = world.getBlock(x0 + x, wy, z0 + z);
                if (id != AIR && isOpaque(id)) blocked = true;
                if (!blocked) {
                    size_t i = idx(x, y, z);
                    skyBuf[i] = 15;
                    skyQ.push_back({x, y, z, 15});
                }
                uint8_t emit = blockLight(id);
                if (emit > 0) {
                    size_t i = idx(x, y, z);
                    if (blkBuf[i] < emit) {
                        blkBuf[i] = emit;
                        blkQ.push_back({x, y, z, emit});
                    }
                }
            }
        }
    }

    // --- 2) BFS yayılım ---
    auto spread = [&](std::deque<Cell>& q, std::vector<uint8_t>& buf, bool isSky) {
        while (!q.empty()) {
            Cell c = q.front(); q.pop_front();
            for (int d = 0; d < 6; ++d) {
                int nx = c.x + DX[d], ny = c.y + DY[d], nz = c.z + DZ[d];
                if (nx < 0 || ny < 0 || nz < 0) continue;
                if (nx >= WX || ny >= WY || nz >= WZ) continue;
                BlockId nid = world.getBlock(x0 + nx, y0 + ny, z0 + nz);
                if (isOpaque(nid)) continue;
                int loss = (isSky && DY[d] == 1) ? 0 : 1;
                int next = static_cast<int>(c.level) - loss;
                if (next <= 0) continue;
                size_t i = idx(nx, ny, nz);
                if (buf[i] >= next) continue;
                buf[i] = static_cast<uint8_t>(next);
                q.push_back({nx, ny, nz, static_cast<uint8_t>(next)});
            }
        }
    };
    spread(skyQ, skyBuf, true);
    spread(blkQ, blkBuf, false);

    // --- 3) chunk'lara yaz ---
    for (Chunk* c : chunks) {
        ChunkPos cp{c->pos().x, c->pos().y, c->pos().z};
        for (int ly = 0; ly < CHUNK_H; ++ly)
            for (int lz = 0; lz < CHUNK_W; ++lz)
                for (int lx = 0; lx < CHUNK_W; ++lx) {
                    int wx = cp.x * CHUNK_W + lx;
                    int wy = cp.y * CHUNK_H + ly;
                    int wz = cp.z * CHUNK_W + lz;
                    int bx = wx - x0, by = wy - y0, bz = wz - z0;
                    if (bx < 0 || by < 0 || bz < 0 || bx >= WX || by >= WY || bz >= WZ) continue;
                    size_t i = idx(bx, by, bz);
                    c->setSkyLight(lx, ly, lz, skyBuf[i]);
                    c->setBlockLight(lx, ly, lz, blkBuf[i]);
                }
        c->markLightReady();
        ++litCount;
    }
    return litCount;
}

void LightEngine::onBlockChanged(World& world, int wx, int wy, int wz, BlockId oldId, BlockId newId) {
    ChunkPos cp{
        wx < 0 ? -(((-wx) + CHUNK_W - 1) / CHUNK_W) : wx / CHUNK_W,
        wy < 0 ? -(((-wy) + CHUNK_H - 1) / CHUNK_H) : wy / CHUNK_H,
        wz < 0 ? -(((-wz) + CHUNK_W - 1) / CHUNK_W) : wz / CHUNK_W};
    Chunk* c = world.chunkAt(cp);
    if (!c) return;

    // yerel chunk'ı ve 6 komşusunu kkirlet (ışık sınırı geçebilir)
    c->markDirty();
    for (int d = 0; d < 6; ++d) {
        Chunk* n = world.chunkAt({cp.x + DX[d], cp.y + DY[d], cp.z + DZ[d]});
        if (n) n->markDirty();
    }

    // tam yeniden hesaplama (artımlı BFS silme karmaşık; chunk küçük olduğu için hızlı)
    for (int dy = -1; dy <= 1; ++dy)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                Chunk* nc = world.chunkAt({cp.x + dx, cp.y + dy, cp.z + dz});
                if (nc) LightEngine::computeChunk(world, *nc);
            }
    (void)oldId; (void)newId; (void)wx; (void)wy; (void)wz;
}

float LightEngine::sunFactor(float hours) {
    // 0-4 gece, 4-7 şafak, 7-18 gündüz, 18-21 akşam, 21-24 gece
    if (hours < 4.f) return 0.08f;
    if (hours < 7.f) return 0.08f + (hours - 4.f) / 3.f * 0.92f;
    if (hours < 18.f) return 1.0f;
    if (hours < 21.f) return 1.0f - (hours - 18.f) / 3.f * 0.92f;
    return 0.08f;
}

void LightEngine::skyColor(float hours, float out[3]) {
    // gündüz: açık mavi, şafak/akşam: turuncu, gece: koyu lacivert
    auto lerp3 = [](const float a[3], const float b[3], float t, float o[3]) {
        for (int i = 0; i < 3; ++i) o[i] = a[i] + (b[i] - a[i]) * t;
    };
    const float day[3]   = {0.52f, 0.72f, 0.95f};
    const float dusk[3]  = {0.92f, 0.48f, 0.22f};
    const float night[3] = {0.04f, 0.05f, 0.12f};

    if (hours < 4.f)       { std::memcpy(out, night, sizeof(night)); return; }
    if (hours < 6.5f)      { lerp3(night, dusk, (hours - 4.f) / 2.5f, out); return; }
    if (hours < 8.5f)      { lerp3(dusk, day,  (hours - 6.5f) / 2.0f, out); return; }
    if (hours < 17.f)      { std::memcpy(out, day, sizeof(day)); return; }
    if (hours < 19.5f)     { lerp3(day, dusk, (hours - 17.f) / 2.5f, out); return; }
    if (hours < 22.f)      { lerp3(dusk, night, (hours - 19.5f) / 2.5f, out); return; }
    std::memcpy(out, night, sizeof(night));
}

} // namespace craftya
