// Craftya Engine — oyun durumu ve döngüsü (platformdan bağımsız)
#pragma once
#include "craftya/core.hpp"
#include "craftya/world.hpp"
#include "craftya/mesher.hpp"
#include "craftya/physics.hpp"
#include "craftya/render.hpp"
#include <unordered_map>
#include <memory>

namespace craftya {

// oyuncu girdisi (platform katmanı doldurur)
struct InputState {
    float moveX = 0.f;      // -1..1 (sağ)
    float moveZ = 0.f;      // -1..1 (ileri)
    float lookDX = 0.f;     // dokunmatik/fare delta (piksel)
    float lookDY = 0.f;
    bool jump = false;
    bool sprint = false;
    bool sneak = false;
    bool placeBlock = false;   // bu karede tıklandı
    bool breakBlock = false;   // bu karede tıklandı
    bool cycleBlock = false;   // blok seçimi değiştir
};

struct Settings {
    float fovDeg = 70.f;
    int renderChunks = VIEW_CHUNKS;
    float resolutionScale = 0.8f;
    bool shadows = true;
    bool ao = true;
    int targetFps = 60;
    float touchSensitivity = 1.0f;
};

class Game {
public:
    Game(uint64_t seed = 424242, bool generateTerrain = true);

    // her karede çağrılır; dt saniye
    void update(float dt, const InputState& in);

    // render için hazır mesh listesi (platform katmanı GPU'ya yükler)
    struct VisibleChunk {
        ChunkPos pos;
        const MeshData* mesh;
        Vec3f min, max;
    };
    const std::vector<VisibleChunk>& visibleChunks() const { return visible_; }

    World& world() { return world_; }
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }
    const Body& player() const { return body_; }
    const FrameStats& stats() const { return stats_; }
    const Settings& settings() const { return settings_; }
    Settings& settings() { return settings_; }
    BlockId selectedBlock() const { return selected_; }
    const RayHit& crosshair() const { return hit_; }

    void setResolutionScale(float s) { settings_.resolutionScale = std::max(0.5f, std::min(1.0f, s)); }

private:
    void meshChunks(float budgetMs);
    void updateCameraFromBody(float dt);

    World world_;
    Camera camera_;
    Body body_;
    RayHit hit_;
    Settings settings_;
    FrameStats stats_;
    BlockId selected_ = STONE;

    std::unordered_map<ChunkPos, std::unique_ptr<MeshData>, ChunkPosHash> meshes_;
    std::vector<VisibleChunk> visible_;

    float accumFrame_ = 0.f;
    int frames_ = 0;
    float fpsTimer_ = 0.f;

    // kare hızı adaptasyonu
    float avgFrameMs_ = 16.6f;
    float scaleCooldown_ = 0.f;
};

} // namespace craftya
