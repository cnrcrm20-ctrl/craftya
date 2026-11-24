// Craftya Engine — oyun döngüsü uygulaması
#include "craftya/game.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include "craftya/lighting.hpp"

namespace craftya {

namespace {
constexpr float MOUSE_SENS = 0.0022f;
constexpr float REACH = 5.0f;          // blok erişim mesafesi
}

Game::Game(uint64_t seed, bool generateTerrain)
    : world_(seed, generateTerrain) {
    // başlangıç konumu: yüzeyin üstü
    float h = world_.heightAt(0, 0);
    body_.pos = {0.5f, h + 2.0f, 0.5f};
    camera_.pos = body_.pos + Vec3f{0, 1.62f, 0};
}

void Game::update(float dt, const InputState& in) {
    // -- FPS/istatistik --
    stats_.frameMs = dt * 1000.f;
    avgFrameMs_ = avgFrameMs_ * 0.9f + stats_.frameMs * 0.1f;
    fpsTimer_ += dt;
    ++frames_;
    if (fpsTimer_ >= 0.5f) {
        stats_.fps = frames_ / fpsTimer_;
        frames_ = 0;
        fpsTimer_ = 0.f;
    }

    // -- bakış --
    camera_.yaw += in.lookDX * MOUSE_SENS * settings_.touchSensitivity;
    camera_.pitch -= in.lookDY * MOUSE_SENS * settings_.touchSensitivity;
    const float maxPitch = 1.5533f;  // ~89 derece
    camera_.pitch = std::max(-maxPitch, std::min(maxPitch, camera_.pitch));

    // -- hareket yönü kameraya göre --
    Vec3f fwd = camera_.forward();
    fwd.y = 0;
    fwd = fwd.normalized();
    Vec3f right = camera_.right();

    Vec3f wish = fwd * in.moveZ + right * in.moveX;
    float wishLen = wish.length();
    if (wishLen > 1.f) wish = wish * (1.f / wishLen);

    float speed = in.sneak ? Physics::SNEAK_SPEED
                : in.sprint ? Physics::SPRINT_SPEED
                            : Physics::WALK_SPEED;

    // su içinde yukarı yüzme
    if (body_.inWater && in.jump) body_.vel.y = Physics::SWIM_UP_VEL;

    // yatay hız (ivme ile)
    Vec3f targetVel = wish * speed;
    const float accel = body_.onGround ? 18.0f : 6.0f;
    body_.vel.x += (targetVel.x - body_.vel.x) * std::min(1.f, accel * dt);
    body_.vel.z += (targetVel.z - body_.vel.z) * std::min(1.f, accel * dt);

    // zıplama
    if (in.jump && body_.onGround && !body_.inWater) {
        body_.vel.y = Physics::JUMP_VEL;
        body_.onGround = false;
    }

    // -- fizik --
    Physics::move(world_, body_, dt);

    // dünyadan düşerse geri koy
    if (body_.pos.y < -20.f) {
        float h = world_.heightAt(0, 0);
        body_.pos = {0.5f, h + 2.0f, 0.5f};
        body_.vel = {0, 0, 0};
    }

    // -- kamera gövdeyi takip et --
    updateCameraFromBody(dt);

    // -- blok hedefleme --
    hit_ = Raycast::cast(world_, camera_.pos, camera_.forward(), REACH);

    // -- blok kırma / koyma --
    if (in.breakBlock && hit_.hit) {
        world_.setBlock(hit_.block.x, hit_.block.y, hit_.block.z, AIR);
        LightEngine::onBlockChanged(world_, hit_.block.x, hit_.block.y, hit_.block.z, GRASS, AIR);
        meshes_.erase({hit_.block.x >> 4, hit_.block.y >> 4, hit_.block.z >> 4});
    }
    if (in.placeBlock && hit_.hit) {
        Vec3i p = hit_.block + hit_.normal;
        // oyuncunun içine blok koyma
        AABB pb = body_.box();
        bool insidePlayer =
            p.x + 1 > pb.min.x && p.x < pb.max.x &&
            p.y + 1 > pb.min.y && p.y < pb.max.y &&
            p.z + 1 > pb.min.z && p.z < pb.max.z;
        if (!insidePlayer) {
            world_.setBlock(p.x, p.y, p.z, selected_);
            LightEngine::onBlockChanged(world_, p.x, p.y, p.z, AIR, selected_);
            meshes_.erase({p.x >> 4, p.y >> 4, p.z >> 4});
        }
    }
    if (in.cycleBlock) {
        selected_ = static_cast<BlockId>((selected_ % (BLOCK_COUNT - 2)) + 1);
        if (selected_ == BEDROCK) selected_ = STONE;
    }

    // -- chunk akışı + mesh --
    auto t0 = std::chrono::high_resolution_clock::now();
    meshChunks(6.0f);   // kare bütçesi: 6 ms mesh
    auto t1 = std::chrono::high_resolution_clock::now();
    stats_.meshMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // -- görünür chunk listesi (frustum culling) --
    Mat4 vp = camera_.proj(16.f / 9.f) * camera_.view();
    Frustum fr = Frustum::fromCameraMatrix(vp);
    visible_.clear();
    size_t vram = 0;
    for (auto& [pos, mesh] : meshes_) {
        if (!mesh) continue;
        Vec3f mn{static_cast<float>(pos.x * CHUNK_W),
                 static_cast<float>(pos.y * CHUNK_H),
                 static_cast<float>(pos.z * CHUNK_W)};
        Vec3f mx{mn.x + CHUNK_W, mn.y + CHUNK_H, mn.z + CHUNK_W};
        if (!fr.boxVisible(mn, mx)) continue;
        visible_.push_back({pos, mesh.get(), mn, mx});
        vram += mesh->bytes();
    }
    stats_.visibleChunks = static_cast<int>(visible_.size());
    stats_.loadedChunks = static_cast<int>(meshes_.size());
    stats_.drawCalls = stats_.visibleChunks * 2;  // opak + şeffaf geçiş
    stats_.vramBytes = vram;

    // -- dinamik çözünürlük (60 FPS taban koruması) --
    scaleCooldown_ -= dt;
    if (scaleCooldown_ <= 0.f) {
        float targetMs = 1000.f / static_cast<float>(std::max(30, settings_.targetFps));
        if (avgFrameMs_ > targetMs * 1.15f && settings_.resolutionScale > 0.6f) {
            settings_.resolutionScale -= 0.05f;
            scaleCooldown_ = 2.0f;
        } else if (avgFrameMs_ < targetMs * 0.75f && settings_.resolutionScale < 1.0f) {
            settings_.resolutionScale += 0.05f;
            scaleCooldown_ = 3.0f;
        }
        settings_.resolutionScale = std::max(0.6f, std::min(1.0f, settings_.resolutionScale));
    }
}

void Game::updateCameraFromBody(float dt) {
    Vec3f target{body_.pos.x, body_.pos.y + 1.62f, body_.pos.z};
    // yumuşatma (kamera sarsıntısını azaltır)
    float k = std::min(1.f, 22.f * dt);
    camera_.pos.x += (target.x - camera_.pos.x) * k;
    camera_.pos.y += (target.y - camera_.pos.y) * k;
    camera_.pos.z += (target.z - camera_.pos.z) * k;
}

void Game::meshChunks(float budgetMs) {
    auto start = std::chrono::high_resolution_clock::now();

    // gerekli chunk'ları öncelik sırasıyla al
    ChunkStreamer streamer;
    auto needed = streamer.update(camera_, settings_.renderChunks);

    for (const auto& pos : needed) {
        auto elapsed = std::chrono::duration<float, std::milli>(
            std::chrono::high_resolution_clock::now() - start).count();
        if (elapsed > budgetMs) break;

        Chunk* c = world_.getOrCreate(pos);
        if (!c) continue;

        auto it = meshes_.find(pos);
        bool needMesh = (it == meshes_.end()) || !it->second || c->needsMesh();
        if (!needMesh) continue;

        // ışık henüz hesaplanmadıysa hesapla (mesh ışığa bağlı)
        if (!c->lightReady()) {
            LightEngine::computeChunk(world_, *c);
            c->markLightReady();
        }

        auto mesh = std::make_unique<MeshData>();
        Mesher::build(world_, *c, *mesh);
        c->markMeshed();
        meshes_[pos] = std::move(mesh);
    }

    // görüş dışındaki chunk mesh'lerini boşalt (bellek)
    float maxDist = static_cast<float>(settings_.renderChunks) * CHUNK_W * 1.6f;
    float maxDist2 = maxDist * maxDist;
    for (auto it = meshes_.begin(); it != meshes_.end(); ) {
        Vec3f center{
            (it->first.x + 0.5f) * CHUNK_W,
            (it->first.y + 0.5f) * CHUNK_H,
            (it->first.z + 0.5f) * CHUNK_W};
        Vec3f d = center - camera_.pos;
        if (d.dot(d) > maxDist2) it = meshes_.erase(it);
        else ++it;
    }
}

} // namespace craftya
