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

std::vector<ChunkPos> ChunkStreamer::unloadList(const Camera& cam, int viewChunks) const {
    std::vector<ChunkPos> out;
    (void)cam; (void)viewChunks;
    return out;
}

} // namespace craftya
