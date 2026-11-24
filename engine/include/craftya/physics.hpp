// Craftya Engine — fizik: AABB çarpışma, yerçekimi, ışın izleme (DDA)
#pragma once
#include "craftya/world.hpp"

namespace craftya {

struct AABB {
    Vec3f min, max;
};

// Oyuncu fizik gövdesi
struct Body {
    Vec3f pos{0, 40, 0};      // ayak merkezi
    Vec3f vel{0, 0, 0};
    float width = 0.6f;        // tam genişlik
    float height = 1.8f;       // tam yükseklik
    bool onGround = false;
    bool inWater = false;

    AABB box() const {
        float hw = width * 0.5f;
        return AABB{{pos.x - hw, pos.y, pos.z - hw},
                    {pos.x + hw, pos.y + height, pos.z + hw}};
    }
};

struct MoveResult {
    bool hitX = false, hitY = false, hitZ = false;
    bool landed = false;   // yere indi
    bool headHit = false;  // tavana çarptı
};

class Physics {
public:
    static constexpr float GRAVITY = -28.0f;      // blok/sn^2 (oyun hissi için abartılı)
    static constexpr float TERMINAL_VEL = -55.0f;
    static constexpr float JUMP_VEL = 9.2f;
    static constexpr float WALK_SPEED = 4.6f;
    static constexpr float SPRINT_SPEED = 6.6f;
    static constexpr float SNEAK_SPEED = 1.8f;
    static constexpr float WATER_DRAG = 0.55f;
    static constexpr float SWIM_UP_VEL = 3.5f;
    static constexpr float EPS = 1e-4f;

    // Hareketi çarpışma duyarlı uygula (eksen ayrık çözüm, sweep'siz ama stabil)
    static MoveResult move(World& world, Body& body, float dt);
    static bool collides(World& world, const AABB& box);
    static bool pointInWater(World& world, const Vec3f& p);
};

// ışın izleme sonucu
struct RayHit {
    bool hit = false;
    Vec3i block{0, 0, 0};        // vurulan blok
    Vec3i normal{0, 0, 0};       // yüz normali (yerleştirme için)
    float dist = 0.f;
};

class Raycast {
public:
    // Amanatides & Woo DDA — blok ızgarasında yürü
    static RayHit cast(World& world, const Vec3f& origin, const Vec3f& dir, float maxDist);
};

} // namespace craftya
