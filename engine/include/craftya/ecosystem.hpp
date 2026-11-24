// Craftya Engine — mob ekosistemi
//
// Amaç: dünya "yaşayan" hissettirsin. Her mob:
//   * biyomuna uygun yerde doğar (inek ovada, kurt taygada, kurbağa bataklıkta)
//   * sürü halinde dolaşır (aile grupları)
//   * otlar / su içer / kaçar / avlanır / dinlenir
//   * gece-gündüz davranışı değişir (gece bazıları uyur, bazıları avlanır)
//   * oyuncudan kaçar (vurulursa) veya ona yaklaşır (evcil)
#pragma once
#include "craftya/mobs_gen.hpp"
#include "craftya/anim.hpp"
#include "craftya/world.hpp"
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace craftya {

// ---------------- davranış durumları ----------------
enum class Mood : uint8_t {
    Idle = 0,      // duruyor, nefes alıyor
    Graze,         // otluyor (baş eğik)
    Walk,          // yürüyor
    Run,           // koşuyor (kaçış/kovalama)
    Flee,          // oyuncudan kaçıyor
    Chase,         // hedefi kovalıyor
    Swim,          // suda
    Sleep,         // uyuyor (gece)
    Look,          // merakla bakıyor
    Eat            // yiyor (animasyonlu)
};

inline const char* moodName(Mood m) {
    switch (m) {
        case Mood::Idle:  return "bekliyor";
        case Mood::Graze: return "otluyor";
        case Mood::Walk:  return "yürüyor";
        case Mood::Run:   return "koşuyor";
        case Mood::Flee:  return "kaçıyor";
        case Mood::Chase: return "kovalıyor";
        case Mood::Swim:  return "yüzüyor";
        case Mood::Sleep: return "uyuyor";
        case Mood::Look:  return "bakıyor";
        case Mood::Eat:   return "yiyor";
        default:          return "?";
    }
}

// ---------------- tek bir canlı ----------------
struct Mob {
    uint16_t  type = MOB_COW;
    Vec3f     pos;              // ayak noktası
    Vec3f     vel;
    float     yaw = 0.f;        // gövde yönü
    float     headYaw = 0.f;    // baş ayrı dönebilir (meraklı bakış)
    float     headPitch = 0.f;
    Mood      mood = Mood::Idle;
    float     moodTimer = 0.f;  // bu durumda ne kadar kaldı
    float     animPhase = 0.f;  // bacak salınımı
    float     headBob = 0.f;
    float     health = 20.f;
    float     hurtFlash = 0.f;
    float     scale = 1.f;      // bebekler 0.6
    bool      baby = false;
    bool      onGround = false;
    bool      inWater = false;
    int       herdId = -1;      // sürü kimliği
    Vec3f     home;             // doğduğu yer (çok uzaklaşmasın)
    float     wanderAngle = 0.f;
    uint32_t  rng = 1;
    Skeleton  sk;

    float nextF() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return static_cast<float>(rng & 0xFFFFFF) / 16777216.f;
    }
    float range(float a, float b) { return a + (b - a) * nextF(); }
};

// ---------------- ekosistem yöneticisi ----------------
class Ecosystem {
public:
    static constexpr int   MAX_MOBS       = 220;
    static constexpr float DESPAWN_DIST   = 150.f;
    static constexpr float SPAWN_MIN_DIST = 14.f;
    static constexpr float SPAWN_MAX_DIST = 88.f;

    void init(uint64_t seed) {
        seed_ = seed ? seed : 12345u;
        mobs_.clear();
        spawnTimer_ = 0.f;
        herds_.clear();
        rng_ = static_cast<uint32_t>(seed_ ^ 0xBEEF);
    }

    // Biyom → hangi moblar doğar, ne kadar sık
    static bool biomeAllows(MobId id, worldgen::Biome b, bool night) {
        // balık ve su canlıları: sadece su
        if (isAquatic(id)) return b == worldgen::Biome::Ocean || b == worldgen::Biome::Swamp;
        switch (b) {
            case worldgen::Biome::Plains:
                return id == MOB_COW || id == MOB_SHEEP || id == MOB_CHICKEN ||
                       id == MOB_HORSE || id == MOB_PIG;
            case worldgen::Biome::Forest:
                return id == MOB_WOLF || id == MOB_FOX || id == MOB_PIG ||
                       id == MOB_CHICKEN || id == MOB_RABBIT || id == MOB_GOAT;
            case worldgen::Biome::Taiga:
                return id == MOB_WOLF || id == MOB_FOX || id == MOB_GOAT || id == MOB_POLAR_BEAR;
            case worldgen::Biome::Desert:
                return id == MOB_RABBIT || id == MOB_RABBIT;
            case worldgen::Biome::Savanna:
                return id == MOB_HORSE || id == MOB_COW || id == MOB_GOAT;
            case worldgen::Biome::Swamp:
                return id == MOB_FROG || id == MOB_AXOLOTL || id == MOB_PIG;
            case worldgen::Biome::Mountains:
                return id == MOB_GOAT || id == MOB_GOAT;
            case worldgen::Biome::Beach:
                return id == MOB_CHICKEN || id == MOB_RABBIT;
            default:
                (void)night;
                return false;
        }
    }

    static bool isAquatic(MobId id) {
        return id == MOB_COD || id == MOB_SALMON || id == MOB_TROPICAL ||
               id == MOB_PUFFERFISH || id == MOB_AXOLOTL || id == MOB_SQUID_GENERAL ||
               id == MOB_TURTLE;
    }
    static bool isAggressive(MobId id) {
        return id == MOB_WOLF || id == MOB_POLAR_BEAR || id == MOB_FOX;
    }
    static bool isPrey(MobId id) {
        return id == MOB_RABBIT || id == MOB_CHICKEN || id == MOB_SHEEP || id == MOB_GOAT;
    }

    // ---------------- ana güncelleme ----------------
    void update(float dt, World& world, const Vec3f& playerPos, float hours, bool creative) {
        float night = (hours < 6.f || hours > 19.f) ? 1.f : 0.f;
        (void)night;

        // 1) mobları güncelle
        for (auto& m : mobs_) {
            updateMood(m, dt, world, playerPos, hours);
            moveMob(m, dt, world);
            animateMob(m, dt);
            if (m.hurtFlash > 0.f) m.hurtFlash = std::max(0.f, m.hurtFlash - dt * 3.f);
        }

        // 2) sürü ayrımı: birbirine çok girmesinler (yumuşak itme)
        separate(dt);

        // 3) oyuncudan çok uzaklaşanları sil
        for (size_t i = 0; i < mobs_.size();) {
            Vec3f d = mobs_[i].pos - playerPos;
            if (std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z) > DESPAWN_DIST) {
                mobs_.erase(mobs_.begin() + static_cast<long>(i));
            } else ++i;
        }

        // 4) yeni doğumlar
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.f) {
            spawnTimer_ = 1.4f;
            trySpawn(world, playerPos, hours, creative);
        }

        // 5) üreme: aynı türden 2 yetişkin yakınsa bebek
        tryBreed(dt, world);
    }

    const std::vector<Mob>& mobs() const { return mobs_; }
    size_t count() const { return mobs_.size(); }

    // oyuncu moba vurdu
    void hurt(Mob& m, float dmg, const Vec3f& fromDir) {
        m.health -= dmg;
        m.hurtFlash = 1.f;
        m.mood = Mood::Flee;
        m.moodTimer = m.range(3.f, 6.f);
        m.wanderAngle = std::atan2(m.pos.x - fromDir.x, m.pos.z - fromDir.z);
    }

private:
    // ---------------- rastgelelik ----------------
    float nextF() {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        return static_cast<float>(rng_ & 0xFFFFFF) / 16777216.f;
    }
    float range(float a, float b) { return a + (b - a) * nextF(); }
    int   irange(int a, int b) { return a + static_cast<int>(nextF() * static_cast<float>(b - a + 1)); }

    // ---------------- zemin sorgusu ----------------
    static bool solidAt(World& w, int x, int y, int z) {
        BlockId b = w.getBlock(x, y, z);
        return b != AIR && !isLiquid(b);
    }
    static int groundY(World& w, int x, int z, int from = 150) {
        for (int y = from; y > 1; --y)
            if (solidAt(w, x, y, z)) return y;
        return 1;
    }
    static bool liquidAt(World& w, int x, int y, int z) {
        return isLiquid(w.getBlock(x, y, z));
    }

    // ---------------- durum makinesi ----------------
    void updateMood(Mob& m, float dt, World& world, const Vec3f& playerPos, float hours) {
        m.moodTimer -= dt;

        // oyuncuya uzaklık
        Vec3f d = playerPos - m.pos;
        float dist = std::sqrt(d.x*d.x + d.z*d.z);

        // yırtıcı: yakınsa kovalar
        if (isAggressive(static_cast<MobId>(m.type)) && dist < 14.f && !m.baby) {
            m.mood = Mood::Chase;
            m.wanderAngle = std::atan2(d.x, d.z);
            m.moodTimer = 1.f;
            return;
        }
        // av: yakınsa kaçar
        if (isPrey(static_cast<MobId>(m.type)) && dist < 6.f) {
            m.mood = Mood::Flee;
            m.wanderAngle = std::atan2(-d.x, -d.z);
            m.moodTimer = m.range(2.f, 4.f);
            return;
        }
        // gece: bazıları uyur
        bool night = (hours < 6.f || hours > 19.f);
        if (night && (m.type == MOB_CHICKEN || m.type == MOB_COW || m.type == MOB_SHEEP) &&
            m.moodTimer <= 0.f) {
            m.mood = Mood::Sleep;
            m.moodTimer = m.range(8.f, 16.f);
            return;
        }
        // su içinde
        if (m.inWater && !isAquatic(static_cast<MobId>(m.type))) {
            m.mood = Mood::Swim;
            m.moodTimer = 1.5f;
            return;
        }
        if (m.moodTimer > 0.f) return;   // mevcut duruma devam

        // yeni durum seç
        float r = nextF();
        if (r < 0.34f) {
            m.mood = Mood::Idle;  m.moodTimer = range(1.2f, 3.5f);
        } else if (r < 0.58f) {
            m.mood = Mood::Graze; m.moodTimer = range(2.f, 5.f);
        } else if (r < 0.80f) {
            m.mood = Mood::Walk;
            m.moodTimer = range(2.f, 5.f);
            // evine yakın kal
            Vec3f toHome = m.home - m.pos;
            float hd = std::sqrt(toHome.x*toHome.x + toHome.z*toHome.z);
            if (hd > 26.f) m.wanderAngle = std::atan2(toHome.x, toHome.z);
            else            m.wanderAngle = range(0.f, 6.28318f);
        } else if (r < 0.90f) {
            m.mood = Mood::Look;  m.moodTimer = range(1.f, 2.5f);
            m.headYaw = range(-0.7f, 0.7f);
        } else {
            m.mood = Mood::Eat;   m.moodTimer = range(1.5f, 3.f);
        }
        (void)world;
    }

    // ---------------- hareket ----------------
    void moveMob(Mob& m, float dt, World& world) {
        float speed = 0.f;
        switch (m.mood) {
            case Mood::Walk:  speed = 1.1f; break;
            case Mood::Run:   speed = 3.4f; break;
            case Mood::Flee:  speed = 4.2f; break;
            case Mood::Chase: speed = 3.8f; break;
            case Mood::Swim:  speed = 1.6f; break;
            default:          speed = 0.f;  break;
        }
        if (m.baby) speed *= 1.15f;

        // hedef yön
        float wishX = std::sin(m.wanderAngle);
        float wishZ = std::cos(m.wanderAngle);

        // yön döndürme (aniden dönmesin)
        float targetYaw = std::atan2(wishX, wishZ);
        float dy = targetYaw - m.yaw;
        while (dy >  3.14159f) dy -= 6.28318f;
        while (dy < -3.14159f) dy += 6.28318f;
        float turn = std::clamp(dy, -3.5f * dt, 3.5f * dt);
        if (speed > 0.f) m.yaw += turn * 4.f;

        // ileri hareket
        m.vel.x = std::sin(m.yaw) * speed;
        m.vel.z = std::cos(m.yaw) * speed;

        // adım: önündeki blok yüksekse zıpla, uçurumsa dur
        int nx = static_cast<int>(std::floor(m.pos.x + m.vel.x * dt * 3.f));
        int nz = static_cast<int>(std::floor(m.pos.z + m.vel.z * dt * 3.f));
        int feet = static_cast<int>(std::floor(m.pos.y));
        bool blocked = solidAt(world, nx, feet, nz) || solidAt(world, nx, feet + 1, nz);
        bool drop    = !solidAt(world, nx, feet - 1, nz) && !solidAt(world, nx, feet - 2, nz);

        if (blocked && m.onGround) {
            if (!solidAt(world, nx, feet + 2, nz)) m.vel.y = 7.4f;   // zıpla
            else { m.wanderAngle += 2.2f; m.vel.x = m.vel.z = 0.f; }  // dön
        }
        if (drop && m.onGround && !isAquatic(static_cast<MobId>(m.type))) {
            m.wanderAngle += 3.0f;
            m.vel.x = m.vel.z = 0.f;
        }

        // yerçekimi
        m.vel.y -= 24.f * dt;
        if (m.vel.y < -40.f) m.vel.y = -40.f;

        // uygula
        m.pos.x += m.vel.x * dt;
        m.pos.y += m.vel.y * dt;
        m.pos.z += m.vel.z * dt;

        // su
        m.inWater = liquidAt(world, static_cast<int>(std::floor(m.pos.x)),
                             static_cast<int>(std::floor(m.pos.y + 0.4f)),
                             static_cast<int>(std::floor(m.pos.z)));
        if (m.inWater) {
            m.vel.y += 26.f * dt;                 // kaldırma
            if (m.vel.y > 2.2f) m.vel.y = 2.2f;
            m.vel.y -= 30.f * dt;
        }

        // zemin çarpışması
        int bx = static_cast<int>(std::floor(m.pos.x));
        int bz = static_cast<int>(std::floor(m.pos.z));
        int by = static_cast<int>(std::floor(m.pos.y));
        if (solidAt(world, bx, by, bz)) {
            m.pos.y = static_cast<float>(by + 1);
            m.vel.y = 0.f;
            m.onGround = true;
        } else {
            m.onGround = solidAt(world, bx, by - 1, bz);
        }
        // dünya dışına düşmesin
        if (m.pos.y < 2.f) { m.pos.y = 2.f; m.vel.y = 0.f; }
    }

    // ---------------- animasyon ----------------
    void animateMob(Mob& m, float dt) {
        float speed = std::sqrt(m.vel.x * m.vel.x + m.vel.z * m.vel.z);
        m.animPhase += dt * (2.2f + speed * 2.8f);

        // baş hareketi: otlarken aşağı, bakarken yana
        float targetPitch = 0.f;
        switch (m.mood) {
            case Mood::Graze: targetPitch = -0.55f; break;
            case Mood::Eat:   targetPitch = -0.45f + std::sin(m.animPhase * 6.f) * 0.12f; break;
            case Mood::Sleep: targetPitch = -0.25f; break;
            default:          targetPitch = std::sin(m.animPhase * 0.7f) * 0.06f; break;
        }
        m.headPitch += (targetPitch - m.headPitch) * std::min(1.f, dt * 6.f);
        m.headBob = std::sin(m.animPhase * 2.f) * (0.02f + speed * 0.01f);

        // iskelet: motorun kendi animasyon sistemi (AnimContext ile)
        if (m.sk.boneCount() > 0) {
            AnimContext ctx;
            ctx.velocity  = m.vel;
            ctx.speed     = speed;
            ctx.onGround  = m.onGround;
            ctx.inWater   = m.inWater;
            ctx.healthPct = m.health / 20.f;
            ctx.rootPos   = m.pos;
            ctx.hurtTimer = m.hurtFlash;
            switch (m.mood) {
                case Mood::Graze:  ctx.state = AnimState::Idle;   break;
                case Mood::Eat:    ctx.state = AnimState::Idle;   break;
                case Mood::Sleep:  ctx.state = AnimState::Idle;   break;
                case Mood::Flee:
                case Mood::Chase:
                case Mood::Run:    ctx.state = AnimState::Run;    break;
                case Mood::Walk:   ctx.state = AnimState::Walk;   break;
                case Mood::Swim:   ctx.state = AnimState::Swim;   break;
                default:           ctx.state = AnimState::Idle;   break;
            }
            // baş hedefi: bakış yönü
            ctx.lookTarget = m.pos + Vec3f{
                std::sin(m.yaw + m.headYaw) * 4.f,
                1.0f + m.headPitch * 3.f,
                std::cos(m.yaw + m.headYaw) * 4.f};
            m.sk.update(dt, ctx);
        }
    }

    // ---------------- sürü ayrımı ----------------
    void separate(float dt) {
        const float minD = 0.85f;
        for (size_t i = 0; i < mobs_.size(); ++i) {
            for (size_t j = i + 1; j < mobs_.size(); ++j) {
                Vec3f d = mobs_[i].pos - mobs_[j].pos;
                d.y = 0.f;
                float dist = std::sqrt(d.x*d.x + d.z*d.z);
                if (dist < 0.001f || dist > minD) continue;
                float push = (minD - dist) * 0.5f;
                d = d * (1.f / dist) * push;
                mobs_[i].pos = mobs_[i].pos + d;
                mobs_[j].pos = mobs_[j].pos - d;
            }
        }
        (void)dt;
    }

    // ---------------- doğum ----------------
    void trySpawn(World& world, const Vec3f& playerPos, float hours, bool creative) {
        if (static_cast<int>(mobs_.size()) >= MAX_MOBS) return;
        (void)creative;

        for (int attempt = 0; attempt < 12; ++attempt) {
            float ang = range(0.f, 6.28318f);
            float dist = range(SPAWN_MIN_DIST, SPAWN_MAX_DIST);
            int x = static_cast<int>(playerPos.x + std::cos(ang) * dist);
            int z = static_cast<int>(playerPos.z + std::sin(ang) * dist);

            // chunk yüklü olmalı
            world.getOrCreate({x >= 0 ? x / CHUNK_W : (x - CHUNK_W + 1) / CHUNK_W, 4,
                               z >= 0 ? z / CHUNK_W : (z - CHUNK_W + 1) / CHUNK_W});

            auto col = world.generator().column(x, z);
            int gy = groundY(world, x, z);
            if (gy < 3) continue;

            // biyoma uygun tür seç
            MobId pick = pickMob(col.biome, hours);
            if (pick == MOB_COUNT) continue;

            // su canlıları: gerçekten su olmalı
            if (isAquatic(pick)) {
                if (!liquidAt(world, x, gy, z)) {
                    gy = world.generator().column(x, z).height;
                    if (!liquidAt(world, x, gy, z)) continue;
                }
            }

            Mob m;
            m.type = static_cast<uint16_t>(pick);
            m.pos = {static_cast<float>(x) + 0.5f, static_cast<float>(gy + 1), static_cast<float>(z) + 0.5f};
            m.home = m.pos;
            m.yaw = range(0.f, 6.28318f);
            m.rng = static_cast<uint32_t>(seed_ + mobs_.size() * 7919u + attempt * 104729u);
            m.baby = nextF() < 0.14f;
            m.scale = m.baby ? 0.62f : range(0.92f, 1.08f);
            m.health = 20.f;
            m.moodTimer = range(0.f, 2.f);
            m.sk.build(mobDef(m.type));

            // sürü: aynı türden yakında varsa ona katıl
            for (const auto& other : mobs_) {
                if (other.type != m.type) continue;
                Vec3f od = other.pos - m.pos;
                if (std::fabs(od.x) < 9.f && std::fabs(od.z) < 9.f) {
                    m.herdId = other.herdId;
                    break;
                }
            }
            // sürü merkezine yakın doğ
            if (m.herdId >= 0) {
                for (const auto& other : mobs_) {
                    if (other.herdId != m.herdId) continue;
                    m.pos.x += (other.pos.x - m.pos.x) * 0.35f;
                    m.pos.z += (other.pos.z - m.pos.z) * 0.35f;
                    break;
                }
            }
            mobs_.push_back(m);
            return;
        }
    }

    MobId pickMob(worldgen::Biome b, float hours) {
        // gece: daha çok yırtıcı
        bool night = (hours < 6.f || hours > 19.f);
        static const MobId table[] = {
            MOB_COW, MOB_SHEEP, MOB_PIG, MOB_CHICKEN, MOB_GOAT, MOB_HORSE,
            MOB_WOLF, MOB_FOX, MOB_RABBIT, MOB_GOAT, MOB_POLAR_BEAR, MOB_FROG,
            MOB_COD, MOB_SALMON, MOB_TROPICAL
        };
        // biyoma uyanları topla
        MobId ok[32];
        int n = 0;
        for (MobId id : table) {
            if (n >= 32) break;
            if (biomeAllows(id, b, night)) ok[n++] = id;
        }
        if (n == 0) return MOB_COUNT;
        // ağırlık: otçullar daha sık
        for (int tries = 0; tries < 8; ++tries) {
            MobId c = ok[static_cast<int>(nextF() * static_cast<float>(n)) % n];
            float w = isAggressive(c) ? (night ? 0.65f : 0.22f) : 0.85f;
            if (nextF() < w) return c;
        }
        return ok[0];
    }

    // ---------------- üreme ----------------
    void tryBreed(float dt, World& world) {
        (void)world;
        breedTimer_ += dt;
        if (breedTimer_ < 9.f) return;
        breedTimer_ = 0.f;
        if (static_cast<int>(mobs_.size()) >= MAX_MOBS - 10) return;

        for (size_t i = 0; i < mobs_.size(); ++i) {
            if (mobs_[i].baby) continue;
            for (size_t j = i + 1; j < mobs_.size(); ++j) {
                if (mobs_[j].baby || mobs_[j].type != mobs_[i].type) continue;
                Vec3f d = mobs_[i].pos - mobs_[j].pos;
                if (std::fabs(d.x) > 3.5f || std::fabs(d.z) > 3.5f) continue;
                if (nextF() > 0.25f) continue;

                Mob baby = mobs_[i];
                baby.baby = true;
                baby.scale = 0.62f;
                baby.pos = (mobs_[i].pos + mobs_[j].pos) * 0.5f;
                baby.pos.y += 0.5f;
                baby.rng = static_cast<uint32_t>(seed_ + mobs_.size() * 6151u);
                baby.mood = Mood::Walk;
                baby.moodTimer = 3.f;
                baby.herdId = mobs_[i].herdId;
                baby.sk.build(mobDef(baby.type));
                mobs_.push_back(baby);
                return;
            }
        }
    }

    std::vector<Mob> mobs_;
    std::vector<int> herds_;
    uint64_t seed_ = 1;
    uint32_t rng_ = 1;
    float spawnTimer_ = 0.f;
    float breedTimer_ = 0.f;
};

} // namespace craftya
