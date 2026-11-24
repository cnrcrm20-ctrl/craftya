// Craftya Engine — iskelet animasyon sistemi
// Minecraft'ın ağır "parça döndürme" animasyonunun ötesine geçer:
// yay (squash/stretch), ikincil hareket (kulak/kuyruk gecikmesi), bakış takibi,
// prosedürel yürüyüş döngüsü, nefes alma, vuruş hazırlığı ve ölüm dağılması.
#pragma once
#include "craftya/core.hpp"
#include "craftya/mobs_gen.hpp"
#include <vector>
#include <string>

namespace craftya {

// tek eklem: yerel dönüşüm
struct Bone {
    Vec3f restPos{0, 0, 0};    // dinlenme konumu (ebeveyne göre)
    Vec3f restScale{1, 1, 1};
    float rotX = 0, rotY = 0, rotZ = 0;   // radyan

    // çalışma zamanı
    Vec3f pos{0, 0, 0};
    Vec3f scale{1, 1, 1};
    Vec3f rot{0, 0, 0};

    // ikincil hareket (sönüm yayı) — gecikmeli takip
    Vec3f velocity{0, 0, 0};
    float stiffness = 120.f;
    float damping = 12.f;

    int parent = -1;           // ebeveyn indeksi (-1 = kök)
    std::string name;
};

// önceden tanımlı eklem isimleri
enum BoneId : int {
    BONE_ROOT = 0, BONE_BODY, BONE_HEAD, BONE_ARM_L, BONE_ARM_R,
    BONE_LEG_FL, BONE_LEG_FR, BONE_LEG_BL, BONE_LEG_BR,
    BONE_TAIL, BONE_WING_L, BONE_WING_R, BONE_EAR_L, BONE_EAR_R,
    BONE_JAW, BONE_COUNT
};

// animasyon durumu
enum class AnimState : uint8_t { Idle, Walk, Run, Jump, Attack, Hurt, Death, Swim, Fly, Sit };

struct AnimContext {
    Vec3f velocity{0, 0, 0};
    float speed = 0.f;          // yatay hız büyüklüğü
    bool onGround = true;
    bool inWater = false;
    float healthPct = 1.f;
    Vec3f lookTarget{0, 0, 0};  // başın bakacağı yer (dünya uzayı)
    Vec3f rootPos{0, 0, 0};
    float hurtTimer = 0.f;
    float attackTimer = 0.f;
    float deathTimer = 0.f;
    AnimState state = AnimState::Idle;
};

// animasyon eğrisi yardımcıları
struct Ease {
    static float smoothstep(float t) { return t * t * (3.f - 2.f * t); }
    static float easeOutBack(float t) {
        const float c1 = 1.70158f, c3 = c1 + 1.f;
        return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
    }
    static float easeInOutSine(float t) { return -(std::cos(3.14159265f * t) - 1.f) / 2.f; }
};

// prosedürel iskelet — her canlı için üretilir, eklem sayısı vücut planına göre
class Skeleton {
public:
    void build(const MobDef& def);
    void update(float dt, const AnimContext& ctx);

    std::vector<Bone>& bones() { return bones_; }
    const std::vector<Bone>& bones() const { return bones_; }
    int boneCount() const { return static_cast<int>(bones_.size()); }

    // isme göre kemik indeksi (-1 = yok). Eklem sırası vücut planına göre değişir.
    int findBone(const char* name) const {
        for (size_t i = 0; i < bones_.size(); ++i)
            if (bones_[i].name == name) return static_cast<int>(i);
        return -1;
    }
    Bone* bone(const char* name) {
        int i = findBone(name);
        return i < 0 ? nullptr : &bones_[i];
    }
    const Bone* bone(const char* name) const {
        int i = findBone(name);
        return i < 0 ? nullptr : &bones_[i];
    }

    // dünya uzayında eklem konumu (çizim için)
    Vec3f worldPos(int boneIndex) const;
    Vec3f worldRot(int boneIndex) const;

    // durum geçişleri
    void triggerAttack() { attackPhase_ = 0.f; }
    void triggerHurt() { hurtPhase_ = 0.f; }
    void triggerDeath() { deathPhase_ = 0.f; }

    AnimState state() const { return state_; }
    float walkPhase() const { return walkPhase_; }
    float breathPhase() const { return breathPhase_; }

private:
    void updateLegs(float dt, float speed, bool onGround);
    void updateFishTail(float dt, float speed);
    void updateWings(float dt, bool flying);
    void updateSecondaryMotion(float dt);
    void aimHead(const AnimContext& ctx);

    std::vector<Bone> bones_;
    BodyPlan plan_ = BodyPlan::Biped;
    AnimState state_ = AnimState::Idle;
    float walkPhase_ = 0.f;
    float breathPhase_ = 0.f;
    float attackPhase_ = -1.f;
    float hurtPhase_ = -1.f;
    float deathPhase_ = -1.f;
    float blinkTimer_ = 3.f;
    float idleTimer_ = 0.f;
    float rootY_ = 0.f;
    float rootVel_ = 0.f;
    uint32_t seed_ = 12345;
};

// yardımcı: yay sönümü (ikincil hareket)
inline void springStep(float& value, float& velocity, float target,
                       float stiffness, float damping, float dt) {
    float force = (target - value) * stiffness - velocity * damping;
    velocity += force * dt;
    value += velocity * dt;
}

} // namespace craftya
