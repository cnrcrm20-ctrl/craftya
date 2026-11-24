// Craftya Engine — greedy meshing: chunk → optimize edilmiş yüz listesi
// Amaç: draw call ve vertex sayısını 10-20x düşürmek (45/60 FPS'in anahtarı)
#pragma once
#include "craftya/world.hpp"
#include <vector>

namespace craftya {

// Tek vertex: konum + normal + doku atlası UV + ışık (AO/parlaklık)
struct Vertex {
    float x, y, z;      // 3
    float nx, ny, nz;   // 3  (normal)
    float u, v;         // 2  (atlas UV)
    float light;        // 1  (0..1)
};
static_assert(sizeof(Vertex) == 36, "Vertex 36 bayt olmalı (hizalama)");

struct MeshData {
    std::vector<Vertex> opaque;
    std::vector<Vertex> transparent;
    uint32_t quadCount = 0;
    void clear() { opaque.clear(); transparent.clear(); quadCount = 0; }
    size_t bytes() const {
        return (opaque.size() + transparent.size()) * sizeof(Vertex);
    }
};

// Yüz yönleri: 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z
struct FaceDir { int dx, dy, dz; };
static constexpr FaceDir FACE_DIRS[6] = {
    { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}, { 0, 0, 1}, { 0, 0,-1}
};

class Mesher {
public:
    // Bir chunk için greedy mesh üretir. Komşu chunk'lar dünyadan okunur (yüz gizleme).
    static void build(World& world, Chunk& chunk, MeshData& out);
};

} // namespace craftya
