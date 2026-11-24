// Craftya Engine — iskelet animasyon uygulaması
#include "craftya/anim.hpp"
#include <cmath>
#include <algorithm>

namespace craftya {

namespace {
constexpr float PI = 3.14159265358979f;

int addBone(std::vector<Bone>& v, const char* name, int parent,
            Vec3f pos, Vec3f scale = {1, 1, 1}) {
    Bone b;
    b.name = name;
    b.parent = parent;
    b.restPos = pos;
    b.pos = pos;
    b.restScale = scale;
    b.scale = scale;
    v.push_back(b);
    return static_cast<int>(v.size()) - 1;
}
}

void Skeleton::build(const MobDef& def) {
    bones_.clear();
    plan_ = def.plan;
    seed_ = 1000 + def.id;

    const float w = def.width, h = def.height;
    const float legLen = h * 0.45f;
    const float bodyH = h * 0.40f;

    // kök
    int root = addBone(bones_, "root", -1, {0, 0, 0});
    (void)root;

    switch (plan_) {
        case BodyPlan::Biped: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, legLen + bodyH * 0.5f, 0},
                               {w * 0.8f, bodyH, w * 0.5f});
            addBone(bones_, "head", body, {0, bodyH * 0.5f + h * 0.13f, 0},
                    {w * 0.62f, h * 0.26f, w * 0.62f});
            addBone(bones_, "armL", body, {w * 0.62f, bodyH * 0.30f, 0}, {w * 0.24f, bodyH * 0.9f, w * 0.24f});
            addBone(bones_, "armR", body, {-w * 0.62f, bodyH * 0.30f, 0}, {w * 0.24f, bodyH * 0.9f, w * 0.24f});
            addBone(bones_, "legFL", BONE_ROOT, {w * 0.28f, legLen, 0}, {w * 0.28f, legLen, w * 0.28f});
            addBone(bones_, "legFR", BONE_ROOT, {-w * 0.28f, legLen, 0}, {w * 0.28f, legLen, w * 0.28f});
            break;
        }
        case BodyPlan::Quadruped: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, legLen + bodyH * 0.5f, 0},
                               {w * 0.85f, bodyH, w * 1.5f});
            int head = addBone(bones_, "head", body, {0, bodyH * 0.25f, w * 0.62f},
                               {w * 0.62f, bodyH * 0.85f, w * 0.75f});
            addBone(bones_, "jaw", head, {0, -bodyH * 0.3f, w * 0.25f}, {w * 0.4f, bodyH * 0.25f, w * 0.35f});
            addBone(bones_, "earL", head, {w * 0.24f, bodyH * 0.45f, 0}, {w * 0.16f, bodyH * 0.3f, w * 0.1f});
            addBone(bones_, "earR", head, {-w * 0.24f, bodyH * 0.45f, 0}, {w * 0.16f, bodyH * 0.3f, w * 0.1f});
            addBone(bones_, "legFL", BONE_ROOT, {w * 0.32f, legLen, w * 0.45f}, {w * 0.26f, legLen, w * 0.26f});
            addBone(bones_, "legFR", BONE_ROOT, {-w * 0.32f, legLen, w * 0.45f}, {w * 0.26f, legLen, w * 0.26f});
            addBone(bones_, "legBL", BONE_ROOT, {w * 0.32f, legLen, -w * 0.45f}, {w * 0.26f, legLen, w * 0.26f});
            addBone(bones_, "legBR", BONE_ROOT, {-w * 0.32f, legLen, -w * 0.45f}, {w * 0.26f, legLen, w * 0.26f});
            addBone(bones_, "tail", body, {0, bodyH * 0.35f, -w * 0.75f}, {w * 0.12f, w * 0.12f, w * 0.6f});
            break;
        }
        case BodyPlan::Bird: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, legLen + h * 0.22f, 0},
                               {w * 0.85f, h * 0.34f, w * 1.1f});
            addBone(bones_, "head", body, {0, h * 0.18f, w * 0.35f}, {w * 0.55f, h * 0.20f, w * 0.55f});
            addBone(bones_, "wingL", body, {w * 0.5f, h * 0.06f, 0}, {w * 0.75f, h * 0.05f, w * 0.5f});
            addBone(bones_, "wingR", body, {-w * 0.5f, h * 0.06f, 0}, {w * 0.75f, h * 0.05f, w * 0.5f});
            addBone(bones_, "legFL", BONE_ROOT, {w * 0.18f, legLen, 0}, {w * 0.14f, legLen, w * 0.14f});
            addBone(bones_, "legFR", BONE_ROOT, {-w * 0.18f, legLen, 0}, {w * 0.14f, legLen, w * 0.14f});
            addBone(bones_, "tail", body, {0, 0, -w * 0.6f}, {w * 0.4f, h * 0.12f, w * 0.35f});
            break;
        }
        case BodyPlan::Fish: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, h * 0.5f, 0},
                               {w * 0.55f, h * 0.85f, w * 1.25f});
            addBone(bones_, "head", body, {0, 0, w * 0.62f}, {w * 0.5f, h * 0.7f, w * 0.45f});
            int tail = addBone(bones_, "tail", body, {0, 0, -w * 0.7f}, {w * 0.1f, h * 0.55f, w * 0.5f});
            addBone(bones_, "tail2", tail, {0, 0, -w * 0.45f}, {w * 0.08f, h * 0.5f, w * 0.4f});
            addBone(bones_, "wingL", body, {w * 0.35f, 0, w * 0.15f}, {w * 0.3f, h * 0.15f, w * 0.3f});
            addBone(bones_, "wingR", body, {-w * 0.35f, 0, w * 0.15f}, {w * 0.3f, h * 0.15f, w * 0.3f});
            break;
        }
        case BodyPlan::Amphibian: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, h * 0.45f, 0},
                               {w * 0.9f, h * 0.55f, w * 1.1f});
            addBone(bones_, "head", body, {0, h * 0.15f, w * 0.55f}, {w * 0.8f, h * 0.5f, w * 0.5f});
            addBone(bones_, "legFL", body, {w * 0.45f, -h * 0.15f, w * 0.4f}, {w * 0.2f, h * 0.3f, w * 0.2f});
            addBone(bones_, "legFR", body, {-w * 0.45f, -h * 0.15f, w * 0.4f}, {w * 0.2f, h * 0.3f, w * 0.2f});
            addBone(bones_, "legBL", body, {w * 0.45f, -h * 0.15f, -w * 0.4f}, {w * 0.28f, h * 0.45f, w * 0.28f});
            addBone(bones_, "legBR", body, {-w * 0.45f, -h * 0.15f, -w * 0.4f}, {w * 0.28f, h * 0.45f, w * 0.28f});
            break;
        }
        case BodyPlan::Insect: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, h * 0.35f, 0},
                               {w * 0.7f, h * 0.5f, w * 1.3f});
            addBone(bones_, "head", body, {0, 0, w * 0.7f}, {w * 0.5f, h * 0.45f, w * 0.5f});
            for (int i = 0; i < 3; ++i) {
                float z = (i - 1) * w * 0.4f;
                addBone(bones_, "legL", body, {w * 0.5f, -h * 0.1f, z}, {w * 0.12f, h * 0.45f, w * 0.12f});
                addBone(bones_, "legR", body, {-w * 0.5f, -h * 0.1f, z}, {w * 0.12f, h * 0.45f, w * 0.12f});
            }
            addBone(bones_, "wingL", body, {w * 0.3f, h * 0.2f, 0}, {w * 0.5f, h * 0.05f, w * 0.4f});
            addBone(bones_, "wingR", body, {-w * 0.3f, h * 0.2f, 0}, {w * 0.5f, h * 0.05f, w * 0.4f});
            break;
        }
        case BodyPlan::Floating: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, h * 0.55f, 0},
                               {w * 0.45f, h * 0.3f, w * 0.45f});
            addBone(bones_, "head", body, {0, h * 0.16f, 0}, {w * 0.45f, h * 0.16f, w * 0.45f});
            addBone(bones_, "armL", body, {w * 0.35f, h * 0.1f, 0}, {w * 0.15f, h * 0.42f, w * 0.15f});
            addBone(bones_, "armR", body, {-w * 0.35f, h * 0.1f, 0}, {w * 0.15f, h * 0.42f, w * 0.15f});
            addBone(bones_, "legFL", body, {w * 0.2f, -h * 0.1f, 0}, {w * 0.13f, h * 0.35f, w * 0.13f});
            addBone(bones_, "legFR", body, {-w * 0.2f, -h * 0.1f, 0}, {w * 0.13f, h * 0.35f, w * 0.13f});
            break;
        }
        case BodyPlan::Serpent: {
            int body = addBone(bones_, "body", BONE_ROOT, {0, h * 0.5f, 0},
                               {w * 0.4f, h * 0.5f, w * 1.4f});
            addBone(bones_, "head", body, {0, 0, w * 0.75f}, {w * 0.45f, h * 0.45f, w * 0.4f});
            int t1 = addBone(bones_, "tail", body, {0, 0, -w * 0.75f}, {w * 0.3f, h * 0.4f, w * 0.5f});
            int t2 = addBone(bones_, "tail2", t1, {0, 0, -w * 0.5f}, {w * 0.22f, h * 0.35f, w * 0.45f});
            addBone(bones_, "tail3", t2, {0, 0, -w * 0.4f}, {w * 0.15f, h * 0.3f, w * 0.4f});
            break;
        }
    }
}

void Skeleton::update(float dt, const AnimContext& ctx) {
    if (bones_.empty()) return;

    walkPhase_ += ctx.speed * dt * 2.6f;
    breathPhase_ += dt * 1.6f;

    // durum seçimi
    if (deathPhase_ >= 0.f) state_ = AnimState::Death;
    else if (hurtPhase_ >= 0.f) state_ = AnimState::Hurt;
    else if (attackPhase_ >= 0.f) state_ = AnimState::Attack;
    else if (ctx.inWater) state_ = AnimState::Swim;
    else if (!ctx.onGround) state_ = AnimState::Jump;
    else if (ctx.speed > 2.0f) state_ = AnimState::Run;
    else if (ctx.speed > 0.15f) state_ = AnimState::Walk;
    else state_ = AnimState::Idle;

    // zamanlayıcılar
    if (attackPhase_ >= 0.f) { attackPhase_ += dt * 3.2f; if (attackPhase_ > 1.f) attackPhase_ = -1.f; }
    if (hurtPhase_ >= 0.f)   { hurtPhase_ += dt * 4.0f;   if (hurtPhase_ > 1.f) hurtPhase_ = -1.f; }
    if (deathPhase_ >= 0.f)  { deathPhase_ = std::min(1.f, deathPhase_ + dt * 1.2f); }

    // nefes alma: gövde hafifçe şişer
    float breath = std::sin(breathPhase_ * PI * 2.f) * 0.035f;

    for (auto& b : bones_) b.rot = {0, 0, 0};

    // bacak yürüyüşü
    updateLegs(dt, ctx.speed, ctx.onGround);
    // balık kuyruğu
    if (plan_ == BodyPlan::Fish || plan_ == BodyPlan::Serpent)
        updateFishTail(dt, ctx.speed);
    // kanatlar
    if (plan_ == BodyPlan::Bird || plan_ == BodyPlan::Insect)
        updateWings(dt, !ctx.onGround || ctx.inWater);
    // baş takibi
    aimHead(ctx);

    // gövde nefesi
    for (auto& b : bones_) {
        if (b.name == "body") {
            b.scale.x = b.restScale.x * (1.f + breath);
            b.scale.z = b.restScale.z * (1.f + breath);
            b.scale.y = b.restScale.y * (1.f - breath * 0.5f);
        }
    }

    // zıplama: ezilme-gerilme
    if (state_ == AnimState::Jump) {
        for (auto& b : bones_) {
            if (b.name == "body" || b.name == "root") {
                b.scale.y *= 1.08f;
                b.scale.x *= 0.94f;
                b.scale.z *= 0.94f;
            }
        }
    }

    // vuruş: gövde öne atılır
    if (attackPhase_ >= 0.f) {
        float t = attackPhase_;
        float lunge = std::sin(t * PI) * 0.28f;
        for (auto& b : bones_) {
            if (b.name == "body") b.rot.x = -lunge;
            if (b.name == "armL" || b.name == "armR") b.rot.x = -lunge * 2.2f;
        }
    }

    // hasar: sarsıntı
    if (hurtPhase_ >= 0.f) {
        float t = hurtPhase_;
        float shake = std::sin(t * PI * 8.f) * (1.f - t) * 0.16f;
        for (auto& b : bones_) if (b.name == "body") { b.rot.z = shake; b.rot.x = -shake * 0.5f; }
    }

    // ölüm: yana devrilir ve çöker
    if (deathPhase_ >= 0.f) {
        float t = Ease::smoothstep(deathPhase_);
        for (auto& b : bones_) {
            if (b.name == "root") b.rot.z = t * PI * 0.5f;
            if (b.name == "body") { b.scale.y *= (1.f - t * 0.3f); }
        }
    }

    // ikincil hareket (kulak, kuyruk, kanat sarkması)
    updateSecondaryMotion(dt);

    // göz kırpma / boşta durma
    blinkTimer_ -= dt;
    if (blinkTimer_ < 0.f) blinkTimer_ = 2.5f + float(seed_ % 30) * 0.1f;
    idleTimer_ += dt;

    // taban zıplaması (nefes ile dikey salınım)
    float idleBob = (state_ == AnimState::Idle)
                        ? std::sin(breathPhase_ * PI * 2.f) * 0.012f : 0.f;
    springStep(rootY_, rootVel_, idleBob, 60.f, 10.f, dt);
    if (!bones_.empty()) bones_[BONE_ROOT].pos.y = bones_[BONE_ROOT].restPos.y + rootY_;
}

void Skeleton::updateLegs(float dt, float speed, bool onGround) {
    (void)dt;
    float amp = std::min(1.f, speed / 4.f) * 0.75f;
    if (!onGround && plan_ != BodyPlan::Fish) amp *= 0.35f;

    const float phase = walkPhase_;
    // eklem sırası vücut planına göre değiştiği için İSİMLE bul
    auto swingName = [&](const char* n, float offset) {
        for (auto& b : bones_) {
            if (b.name == n) { b.rot.x = std::sin(phase + offset) * amp; return; }
        }
    };

    if (plan_ == BodyPlan::Quadruped) {
        // dört bacak: çapraz yürüyüş (FL+BR aynı faz)
        swingName("legFL", 0.f);
        swingName("legFR", PI);
        swingName("legBL", PI);
        swingName("legBR", 0.f);
        // gövde salınımı
        for (auto& b : bones_)
            if (b.name == "body") b.rot.z = std::sin(phase) * amp * 0.12f;
    } else if (plan_ == BodyPlan::Biped) {
        swingName("legFL", 0.f);
        swingName("legFR", PI);
        swingName("armL", PI);
        swingName("armR", 0.f);
    } else if (plan_ == BodyPlan::Insect) {
        for (size_t i = 0; i < bones_.size(); ++i) {
            if (bones_[i].name == "legL" || bones_[i].name == "legR") {
                float off = (i % 2) ? PI : 0.f;
                bones_[i].rot.x = std::sin(phase * 1.4f + off + float(i) * 0.6f) * amp * 0.6f;
            }
        }
    } else if (plan_ == BodyPlan::Bird) {
        swingName("legFL", 0.f);
        swingName("legFR", PI);
    } else if (plan_ == BodyPlan::Amphibian) {
        // kurbağa: arka bacaklar birlikte zıplar
        float hop = std::max(0.f, std::sin(phase * 1.3f)) * amp * 1.4f;
        swingName("legBL", hop);
        swingName("legBR", hop);
    }
}

void Skeleton::updateFishTail(float dt, float speed) {
    (void)dt;
    float amp = 0.25f + std::min(1.f, speed / 3.f) * 0.45f;
    float t = walkPhase_ * 1.8f;
    // kuyruk dalgası: her parça bir öncekinden gecikmeli (yılan/balık)
    for (auto& b : bones_) {
        if (b.name == "tail")  b.rot.y = std::sin(t) * amp;
        if (b.name == "tail2") b.rot.y = std::sin(t - 0.7f) * amp * 1.25f;
        if (b.name == "tail3") b.rot.y = std::sin(t - 1.4f) * amp * 1.4f;
    }
    if (plan_ == BodyPlan::Fish) {
        for (auto& b : bones_)
            if (b.name == "body") b.rot.y = std::sin(t + 0.4f) * amp * 0.3f;
    }
}

void Skeleton::updateWings(float dt, bool flying) {
    (void)dt;
    float amp = flying ? 0.85f : 0.12f;
    float t = walkPhase_ * (flying ? 6.5f : 1.5f);
    for (auto& b : bones_) {
        if (b.name == "wingL") b.rot.z = std::sin(t) * amp;
        if (b.name == "wingR") b.rot.z = -std::sin(t) * amp;
    }
}

void Skeleton::aimHead(const AnimContext& ctx) {
    Bone* head = nullptr;
    for (auto& b : bones_) if (b.name == "head") { head = &b; break; }
    if (!head) return;
    Vec3f toTarget = ctx.lookTarget - ctx.rootPos;
    if (toTarget.length() < 0.001f) return;
    float yaw = std::atan2(toTarget.x, toTarget.z);
    float pitch = -std::atan2(toTarget.y, std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z));
    head->rot.y = std::max(-0.9f, std::min(0.9f, yaw));
    head->rot.x = std::max(-0.55f, std::min(0.55f, pitch));
}

void Skeleton::updateSecondaryMotion(float dt) {
    for (auto& b : bones_) {
        bool secondary = (b.name == "earL" || b.name == "earR" ||
                          b.name == "tail" || b.name == "jaw");
        if (!secondary) continue;
        // yay sönümü: hareketten gelen açıyı yumuşakça geri çek (gerçek gecikme)
        springStep(b.rot.x, b.velocity.x, 0.f, b.stiffness, b.damping, dt);
        springStep(b.rot.z, b.velocity.z, 0.f, b.stiffness * 0.8f, b.damping, dt);
    }
}

Vec3f Skeleton::worldPos(int boneIndex) const {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(bones_.size())) return {0, 0, 0};
    Vec3f p = bones_[boneIndex].pos;
    int parent = bones_[boneIndex].parent;
    while (parent >= 0 && parent < static_cast<int>(bones_.size())) {
        p = p + bones_[parent].pos;
        parent = bones_[parent].parent;
    }
    return p;
}

Vec3f Skeleton::worldRot(int boneIndex) const {
    if (boneIndex < 0 || boneIndex >= static_cast<int>(bones_.size())) return {0, 0, 0};
    Vec3f r = bones_[boneIndex].rot;
    int parent = bones_[boneIndex].parent;
    while (parent >= 0 && parent < static_cast<int>(bones_.size())) {
        r = r + bones_[parent].rot;
        parent = bones_[parent].parent;
    }
    return r;
}

} // namespace craftya
