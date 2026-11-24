// Craftya Engine — chunk akışı uygulaması
#include "craftya/render.hpp"
#include <algorithm>
#include <cmath>

namespace craftya {

std::vector<ChunkPos> ChunkStreamer::update(const Camera& cam, int viewChunks) {
    std::vector<ChunkPos> out;

    int pcx = static_cast<int>(std::floor(cam.pos.x / CHUNK_W));
    int pcz = static_cast<int>(std::floor(cam.pos.z / CHUNK_W));
    int pcy = static_cast<int>(std::floor(cam.pos.y / CHUNK_H));

    // dikey: 3 kat (alt, orta, üst) — zeminin altı ve gökyüzü
    const int vertRange = 2;
    const int r2 = viewChunks * viewChunks;

    for (int cy = pcy - vertRange; cy <= pcy + vertRange; ++cy) {
        for (int cx = pcx - viewChunks; cx <= pcx + viewChunks; ++cx) {
            for (int cz = pcz - viewChunks; cz <= pcz + viewChunks; ++cz) {
                int dx = cx - pcx, dz = cz - pcz;
                if (dx * dx + dz * dz > r2 + viewChunks) continue;   // dairesel
                out.push_back({cx, cy, cz});
            }
        }
    }

    // kamera yönüne göre önceliklendir (önündekiler önce yüklensin)
    Vec3f fwd = cam.forward();
    float yaw = cam.yaw;
    auto score = [&](const ChunkPos& p) {
        float dx = static_cast<float>((p.x - pcx) * CHUNK_W);
        float dz = static_cast<float>((p.z - pcz) * CHUNK_W);
        float dist = std::sqrt(dx * dx + dz * dz);
        float dot = 0.f;
        if (dist > 1e-3f) dot = (dx * fwd.x + dz * fwd.z) / dist;
        // önde olan (dot>0) daha yüksek puan; yakın olan daha yüksek
        return dist * (dot > 0 ? (1.0f - 0.45f * dot) : 1.35f);
    };
    (void)yaw;
    std::sort(out.begin(), out.end(), [&](const ChunkPos& a, const ChunkPos& b) {
        return score(a) < score(b);
    });
    return out;
}

// Kameranın ARKASINDA kalan chunk'lar hemen silinmez.
//  * Ön yarıkürede (kameranın baktığı yönde): görüş menzili + pay
//  * Arka yarıkürede: görüş menzili + UNLOAD_HYSTERESIS_CHUNKS pay
// Bu sayede oyuncu döndüğünde dünya yeniden yüklenmez (takılma olmaz).
std::vector<ChunkPos> ChunkStreamer::unloadList(const Camera& cam, int viewChunks) const {
    std::vector<ChunkPos> out;
    int pcx = static_cast<int>(std::floor(cam.pos.x / CHUNK_W));
    int pcy = static_cast<int>(std::floor(cam.pos.y / CHUNK_H));
    int pcz = static_cast<int>(std::floor(cam.pos.z / CHUNK_W));

    // arka taraf için ekstra pay (chunk cinsinden). 3 blok ≈ 3/16 chunk,
    // ama pratikte dönüşte takılmamak için 2 chunk (32 blok) pay bırakıyoruz.
    const int BACK_PAD = UNLOAD_HYSTERESIS_CHUNKS;
    const int r2Front = (viewChunks + 1) * (viewChunks + 1);
    const int r2Back  = (viewChunks + BACK_PAD) * (viewChunks + BACK_PAD);

    Vec3f fwd = cam.forward();
    float flen = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
    if (flen < 1e-4f) flen = 1.f;
    float fx = fwd.x / flen, fz = fwd.z / flen;

    for (const ChunkPos& p : loaded_) {
        int dx = p.x - pcx, dz = p.z - pcz, dy = p.y - pcy;
        if (dy < -3 || dy > 3) { out.push_back(p); continue; }
        int d2 = dx * dx + dz * dz;
        // bu chunk kameranın önünde mi?
        float dot = 0.f;
        if (d2 > 0) dot = (static_cast<float>(dx) * fx + static_cast<float>(dz) * fz) /
                         std::sqrt(static_cast<float>(d2));
        int limit = (dot >= -0.15f) ? r2Front : r2Back;
        if (d2 > limit) out.push_back(p);
    }
    return out;
}

} // namespace craftya
