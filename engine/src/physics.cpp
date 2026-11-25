// Craftya Engine — fizik uygulaması
#include "craftya/physics.hpp"
#include <algorithm>
#include <cmath>

namespace craftya {

bool Physics::collides(World& world, const AABB& box) {
    int x0 = static_cast<int>(std::floor(box.min.x));
    int x1 = static_cast<int>(std::floor(box.max.x));
    int y0 = static_cast<int>(std::floor(box.min.y));
    int y1 = static_cast<int>(std::floor(box.max.y));
    int z0 = static_cast<int>(std::floor(box.min.z));
    int z1 = static_cast<int>(std::floor(box.max.z));

    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (isSolid(world.getBlock(x, y, z)))
                    return true;
    return false;
}

bool Physics::pointInWater(World& world, const Vec3f& p) {
    BlockId b = world.getBlock(static_cast<int>(std::floor(p.x)),
                               static_cast<int>(std::floor(p.y)),
                               static_cast<int>(std::floor(p.z)));
    return b == WATER;
}

MoveResult Physics::move(World& world, Body& body, float dt) {
    MoveResult res;

    // büyük dt'leri alt adımlara böl (tünelleme ve zemin kaybını önler)
    const float MAX_STEP = 1.0f / 120.0f;
    int steps = static_cast<int>(std::ceil(dt / MAX_STEP));
    if (steps < 1) steps = 1;
    if (steps > 8) steps = 8;
    float h = dt / static_cast<float>(steps);

    for (int step = 0; step < steps; ++step) {
        Vec3f center{body.pos.x, body.pos.y + body.height * 0.5f, body.pos.z};
        body.inWater = pointInWater(world, center);

        if (body.inWater) {
            body.vel.y = std::max(body.vel.y + GRAVITY * 0.28f * h, -6.0f);
            body.vel.x *= (1.0f - WATER_DRAG * h);
            body.vel.z *= (1.0f - WATER_DRAG * h);
        } else {
            body.vel.y = std::max(body.vel.y + GRAVITY * h, TERMINAL_VEL);
        }

        // eksen ayrık hareket
        auto tryAxis = [&](int axis, float delta) {
            if (std::fabs(delta) < 1e-7f) return;
            Vec3f oldPos = body.pos;
            if (axis == 0) body.pos.x += delta;
            if (axis == 1) body.pos.y += delta;
            if (axis == 2) body.pos.z += delta;

            if (collides(world, body.box())) {
                body.pos = oldPos;
                // YATAY çarpışma: MOBİL OTOMATİK ZIPLAMA (Bedrock'taki auto-jump).
                // Dokunmatik kontrolde oyuncu 1 blokluk basamağa takılıp
                // "oyun hareket etmiyor" hissine kapılıyordu.
                // Kolay tırmanılabilir çıkıntı varsa yukarı hız ver.
                bool autoJumped = false;
                if ((axis == 0 || axis == 2) && body.onGround) {
                    Vec3f save = body.pos;
                    bool ledgeAbove = false, headroomOk = false;
                    for (float stepH = 0.6f; stepH <= 1.1f; stepH += 0.25f) {
                        body.pos = save;
                        if (axis == 0) body.pos.x += delta; else body.pos.z += delta;
                        // çıkıntının üstü boş mu?
                        Vec3f up = body.pos;
                        up.y += stepH;
                        body.pos = up;
                        if (!collides(world, body.box())) {
                            ledgeAbove = true;
                            // baş üstü boşluğu: zıplayınca sıkışmayalım
                            Vec3f hd = body.pos;
                            hd.y += 1.2f;
                            body.pos = hd;
                            if (!collides(world, body.box())) headroomOk = true;
                            break;
                        }
                    }
                    body.pos = save;
                    if (ledgeAbove && headroomOk) {
                        body.vel.y = Physics::JUMP_VEL * 0.92f;
                        body.onGround = false;
                        res.landed = false;
                        autoJumped = true;
                    }
                }
                if (autoJumped) return;
                if (axis == 0) { body.vel.x = 0; res.hitX = true; }
                if (axis == 2) { body.vel.z = 0; res.hitZ = true; }
                if (axis == 1) {
                    if (delta < 0) {
                        // zemine tam otur: blok üstüne hizala
                        float snapped = std::floor(oldPos.y + 1e-4f);
                        body.pos = oldPos;
                        body.pos.y = snapped;
                        if (collides(world, body.box())) body.pos = oldPos;
                        res.landed = true; body.onGround = true;
                    } else { res.headHit = true; }
                    body.vel.y = 0;
                    res.hitY = true;
                }
            }
        };

        // düşme sırasında gövde içinde kalma kontrolü: altımızda katı varsa zemine otur
        tryAxis(1, body.vel.y * h);
        tryAxis(0, body.vel.x * h);
        tryAxis(2, body.vel.z * h);

        // zemin yapış: gövde katıya gömülmüşse yukarı sıyır
        int guard = 0;
        while (collides(world, body.box()) && guard++ < 8) {
            body.pos.y = std::floor(body.pos.y) + 1.0f;
            body.vel.y = 0;
            body.onGround = true;
            res.landed = true;
        }
        // basamak çıkıldıysa yatay hız korunur; çarpışma bayrağını temizle
        if (res.landed && (std::fabs(body.vel.x) > 0.01f || std::fabs(body.vel.z) > 0.01f)) {
            res.hitX = res.hitZ = false;
        }
    }

    if (!res.hitY) body.onGround = false;
    // kare sonunda zemin/su durumunu raporla (Game bunu gövdeye yazar)
    {
        AABB b = body.box();
        AABB probe = b;
        probe.min.y -= 0.06f;
        probe.max.y = b.min.y + 0.02f;
        res.onGround = body.onGround || Physics::collides(world, probe);
        Vec3f c{body.pos.x, body.pos.y + body.height * 0.5f, body.pos.z};
        res.inWater = pointInWater(world, c);
    }
    return res;
}

RayHit Raycast::cast(World& world, const Vec3f& origin, const Vec3f& dir, float maxDist) {
    RayHit hit;
    Vec3f d = dir.normalized();
    if (d.length() < 1e-6f) return hit;

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    int stepX = d.x > 0 ? 1 : (d.x < 0 ? -1 : 0);
    int stepY = d.y > 0 ? 1 : (d.y < 0 ? -1 : 0);
    int stepZ = d.z > 0 ? 1 : (d.z < 0 ? -1 : 0);

    auto tDelta = [](float dir_, int step) {
        if (dir_ == 0.f || step == 0) return std::numeric_limits<float>::infinity();
        return std::fabs(1.0f / dir_);
    };
    float tDeltaX = tDelta(d.x, stepX);
    float tDeltaY = tDelta(d.y, stepY);
    float tDeltaZ = tDelta(d.z, stepZ);

    auto tMax = [](float origin_, float d_, int step, int cell) {
        if (step == 0) return std::numeric_limits<float>::infinity();
        float boundary = static_cast<float>(cell + (step > 0 ? 1 : 0));
        return (boundary - origin_) / d_;
    };
    float tMaxX = tMax(origin.x, d.x, stepX, x);
    float tMaxY = tMax(origin.y, d.y, stepY, y);
    float tMaxZ = tMax(origin.z, d.z, stepZ, z);

    float t = 0.f;
    Vec3i normal{0, 0, 0};

    while (t <= maxDist) {
        BlockId b = world.getBlock(x, y, z);
        if (b != AIR && isSolid(b)) {
            hit.hit = true;
            hit.block = {x, y, z};
            hit.normal = normal;
            hit.dist = t;
            return hit;
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX; t = tMaxX; tMaxX += tDeltaX;
            normal = {-stepX, 0, 0};
        } else if (tMaxY < tMaxZ) {
            y += stepY; t = tMaxY; tMaxY += tDeltaY;
            normal = {0, -stepY, 0};
        } else {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ;
            normal = {0, 0, -stepZ};
        }
    }
    return hit;
}

} // namespace craftya
