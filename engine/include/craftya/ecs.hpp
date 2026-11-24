// Craftya Engine — ECS-lite: bileşen tabloları, sistem sırası
// Amaç: mob, düşen eşya, mermi, oyuncu — hepsi aynı döngüde deterministic güncellenir.
#pragma once
#include "craftya/core.hpp"
#include "craftya/physics.hpp"
#include <vector>
#include <cstdint>

namespace craftya {

using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY = 0;

enum class EntityKind : uint8_t { Player, Mob, Projectile, ItemDrop, COUNT };

// temel bileşenler (SoA yerine AoS — küçük ölçek için yeterli, cache dostu boyutta)
struct Transform {
    Vec3f pos{0, 0, 0};
    Vec3f vel{0, 0, 0};
    float yaw = 0.f, pitch = 0.f;
};

struct Health {
    float current = 20.f;
    float max = 20.f;
    float hurtCooldown = 0.f;
    float lastAttacker = 0.f;   // EntityId tutmuyor: döngü kırılmasın
    bool dead() const { return current <= 0.f; }
};

struct MobAi {
    enum class State : uint8_t { Idle, Wander, Chase, Attack, Flee };
    State state = State::Idle;
    float thinkTimer = 0.f;
    float wanderTimer = 0.f;
    Vec3f wanderTarget{0, 0, 0};
    EntityId target = INVALID_ENTITY;
    float attackCooldown = 0.f;
    float speed = 2.2f;
    float sightRange = 16.f;
    float attackRange = 1.8f;
    float damage = 2.f;
    bool hostile = true;
};

struct Projectile {
    Vec3f dir{0, 0, -1};
    float speed = 24.f;
    float life = 5.f;
    float damage = 4.f;
    uint16_t ownerItem = 0;
};

struct ItemDrop {
    uint16_t itemId = 0;
    uint8_t count = 1;
    float pickDelay = 0.5f;   // hemen toplanmasın
    float life = 300.f;
};

struct Renderable {
    uint16_t modelId = 0;     // hangi model (skin, mob türü)
    float scale = 1.f;
    bool visible = true;
};

struct Entity {
    EntityId id = INVALID_ENTITY;
    EntityKind kind = EntityKind::Mob;
    Transform transform;
    Health health;
    MobAi ai;
    Projectile proj;
    ItemDrop drop;
    Renderable render;
    bool active = true;
    // sahiplik (hostsuz mimari: hangi cihaz simüle ediyor)
    uint8_t ownerDevice = 0;
};

class World2 {   // entity dünyası (isim çakışmasın diye World2)
public:
    EntityId spawn(EntityKind kind);
    void despawn(EntityId id);
    Entity* get(EntityId id);
    const Entity* get(EntityId id) const;

    std::vector<Entity>& entities() { return entities_; }
    const std::vector<Entity>& entities() const { return entities_; }
    size_t liveCount() const;
    size_t countOf(EntityKind kind) const;

    // sistemler
    void updateMobs(World& world, float dt, EntityId playerId);
    void updateProjectiles(World& world, float dt);
    void updateItemDrops(World& world, float dt);
    void applyGravityToDrops(World& world, float dt);

private:
    std::vector<Entity> entities_;
    EntityId nextId_ = 1;

    bool lineOfSight(World& world, const Vec3f& from, const Vec3f& to) const;
    Vec3f stepTowardGrid(World& world, const Vec3f& from, const Vec3f& to) const;
};

// yardımcı: AABB ile varlık çarpışması
bool entityCollidesBlock(World& world, const Vec3f& pos, float width, float height);

} // namespace craftya
