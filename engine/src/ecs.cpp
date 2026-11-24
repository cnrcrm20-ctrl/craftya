// Craftya Engine — ECS + mob AI + mermi + düşen eşya sistemleri
#include "craftya/ecs.hpp"
#include <cmath>
#include <algorithm>

namespace craftya {

EntityId World2::spawn(EntityKind kind) {
    Entity e;
    e.id = nextId_++;
    e.kind = kind;
    if (kind == EntityKind::Mob) {
        e.health.max = 20.f; e.health.current = 20.f;
        e.render.modelId = 1;
    } else if (kind == EntityKind::Player) {
        e.health.max = 20.f; e.health.current = 20.f;
        e.render.modelId = 0;
    } else if (kind == EntityKind::Projectile) {
        e.health.max = 1.f; e.health.current = 1.f;
        e.render.modelId = 2;
    } else if (kind == EntityKind::ItemDrop) {
        e.health.max = 1.f; e.health.current = 1.f;
        e.render.modelId = 3;
    }
    entities_.push_back(e);
    return e.id;
}

void World2::despawn(EntityId id) {
    for (auto& e : entities_) {
        if (e.id == id) { e.active = false; break; }
    }
}

Entity* World2::get(EntityId id) {
    for (auto& e : entities_) if (e.id == id && e.active) return &e;
    return nullptr;
}
const Entity* World2::get(EntityId id) const {
    for (auto& e : entities_) if (e.id == id && e.active) return &e;
    return nullptr;
}

size_t World2::liveCount() const {
    size_t n = 0;
    for (auto& e : entities_) if (e.active) ++n;
    return n;
}

size_t World2::countOf(EntityKind kind) const {
    size_t n = 0;
    for (auto& e : entities_) if (e.active && e.kind == kind) ++n;
    return n;
}

bool World2::lineOfSight(World& world, const Vec3f& from, const Vec3f& to) const {
    float dist = (to - from).length();
    if (dist < 0.001f) return true;
    Vec3f dir = (to - from) * (1.f / dist);
    // örnekleyerek blok kontrolü (basit, güvenilir)
    int steps = static_cast<int>(dist * 3.f);
    for (int i = 1; i < steps; ++i) {
        float t = static_cast<float>(i) / steps;
        Vec3f p = from + dir * (dist * t);
        BlockId b = world.getBlock(static_cast<int>(std::floor(p.x)),
                                   static_cast<int>(std::floor(p.y)),
                                   static_cast<int>(std::floor(p.z)));
        if (b != AIR && isOpaque(b)) return false;
    }
    return true;
}

Vec3f World2::stepTowardGrid(World& world, const Vec3f& from, const Vec3f& to) const {
    // 4 yönlü basit yol bulma: blok varsa yanından dolan
    Vec3f d = to - from;
    d.y = 0;
    if (d.length() < 0.001f) return {0, 0, 0};
    d = d.normalized();

    int fx = static_cast<int>(std::floor(from.x + d.x * 0.7f));
    int fy = static_cast<int>(std::floor(from.y));
    int fz = static_cast<int>(std::floor(from.z + d.z * 0.7f));
    BlockId ahead = world.getBlock(fx, fy, fz);
    if (ahead != AIR && isSolid(ahead)) {
        // yanlara sap
        Vec3f perp{-d.z, 0, d.x};
        int px = static_cast<int>(std::floor(from.x + perp.x * 0.7f));
        int pz = static_cast<int>(std::floor(from.z + perp.z * 0.7f));
        BlockId side = world.getBlock(px, fy, pz);
        if (side == AIR || !isSolid(side)) return perp;
        return perp * -1.f;
    }
    return d;
}

void World2::updateMobs(World& world, float dt, EntityId playerId) {
    Entity* player = get(playerId);

    for (auto& e : entities_) {
        if (!e.active || e.kind != EntityKind::Mob) continue;
        if (e.health.dead()) { e.active = false; continue; }

        e.ai.thinkTimer -= dt;
        e.ai.attackCooldown = std::max(0.f, e.ai.attackCooldown - dt);

        // düşünen: hedef seç
        if (e.ai.thinkTimer <= 0.f) {
            e.ai.thinkTimer = 0.4f;
            e.ai.target = INVALID_ENTITY;

            if (player && player->active && !player->health.dead() && e.ai.hostile) {
                float d = (player->transform.pos - e.transform.pos).length();
                if (d < e.ai.sightRange &&
                    lineOfSight(world, e.transform.pos, player->transform.pos)) {
                    e.ai.target = player->id;
                    e.ai.state = (d <= e.ai.attackRange) ? MobAi::State::Attack
                                                        : MobAi::State::Chase;
                }
            }
            if (e.ai.target == INVALID_ENTITY) {
                if (e.ai.state != MobAi::State::Wander) {
                    e.ai.state = MobAi::State::Wander;
                    e.ai.wanderTimer = 2.f + (e.id % 5) * 0.4f;
                    float ang = static_cast<float>((e.id * 2654435761u) % 628) / 100.f;
                    e.ai.wanderTarget = e.transform.pos + Vec3f{std::cos(ang) * 5.f, 0.f, std::sin(ang) * 5.f};
                }
            }
        }

        Vec3f wish{0, 0, 0};
        if (e.ai.state == MobAi::State::Chase && player) {
            wish = stepTowardGrid(world, e.transform.pos, player->transform.pos) * e.ai.speed;
        } else if (e.ai.state == MobAi::State::Attack && player) {
            float d = (player->transform.pos - e.transform.pos).length();
            if (d <= e.ai.attackRange && e.ai.attackCooldown <= 0.f) {
                player->health.current -= e.ai.damage;
                player->health.hurtCooldown = 0.4f;
                e.ai.attackCooldown = 1.0f;
            }
        } else if (e.ai.state == MobAi::State::Wander) {
            e.ai.wanderTimer -= dt;
            if (e.ai.wanderTimer <= 0.f) { e.ai.state = MobAi::State::Idle; }
            else wish = stepTowardGrid(world, e.transform.pos, e.ai.wanderTarget) * (e.ai.speed * 0.4f);
        }

        e.transform.vel.x = wish.x;
        e.transform.vel.z = wish.z;

        // fizik (zıplama: duvara çarpınca)
        Body b;
        b.pos = e.transform.pos;
        b.vel = e.transform.vel;
        b.width = 0.6f; b.height = 1.8f;
        MoveResult mr = Physics::move(world, b, dt);
        if (mr.hitX || mr.hitZ) b.vel.y = Physics::JUMP_VEL * 0.75f;
        e.transform.pos = b.pos;
        e.transform.vel = b.vel;
    }
}

void World2::updateProjectiles(World& world, float dt) {
    for (auto& e : entities_) {
        if (!e.active || e.kind != EntityKind::Projectile) continue;
        e.proj.life -= dt;
        if (e.proj.life <= 0.f) { e.active = false; continue; }

        Vec3f next = e.transform.pos + e.proj.dir * (e.proj.speed * dt);

        // blok çarpması
        BlockId b = world.getBlock(static_cast<int>(std::floor(next.x)),
                                   static_cast<int>(std::floor(next.y)),
                                   static_cast<int>(std::floor(next.z)));
        if (b != AIR && isSolid(b)) { e.active = false; continue; }

        // varlık çarpması
        bool hitEntity = false;
        for (auto& other : entities_) {
            if (!other.active || other.id == e.id) continue;
            if (other.kind != EntityKind::Mob && other.kind != EntityKind::Player) continue;
            Vec3f diff = next - (other.transform.pos + Vec3f{0, 0.9f, 0});
            if (diff.length() < 0.8f) {
                other.health.current -= e.proj.damage;
                other.health.hurtCooldown = 0.4f;
                hitEntity = true;
                break;
            }
        }
        if (hitEntity) { e.active = false; continue; }

        e.transform.pos = next;
    }
}

void World2::applyGravityToDrops(World& world, float dt) {
    for (auto& e : entities_) {
        if (!e.active || e.kind != EntityKind::ItemDrop) continue;
        Body b;
        b.pos = e.transform.pos;
        b.vel = e.transform.vel;
        b.width = 0.3f; b.height = 0.3f;
        Physics::move(world, b, dt);
        e.transform.pos = b.pos;
        e.transform.vel = b.vel;
    }
}

void World2::updateItemDrops(World& world, float dt) {
    (void)world;
    for (auto& e : entities_) {
        if (!e.active || e.kind != EntityKind::ItemDrop) continue;
        e.drop.pickDelay -= dt;
        e.drop.life -= dt;
        if (e.drop.life <= 0.f) e.active = false;
    }
}

bool entityCollidesBlock(World& world, const Vec3f& pos, float width, float height) {
    AABB box{{pos.x - width * 0.5f, pos.y, pos.z - width * 0.5f},
             {pos.x + width * 0.5f, pos.y + height, pos.z + width * 0.5f}};
    return Physics::collides(world, box);
}

} // namespace craftya
