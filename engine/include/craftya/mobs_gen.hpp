// Craftya Engine — OTOMATİK ÜRETİLMİŞ canlı kayıt defteri
// 37 ana canlı türü + 125 balık kombinasyonu (47 maddelik sayıma DAHİL DEĞİL)
#pragma once
#include <cstdint>
#include <array>

namespace craftya {

// vücut planı — animasyon sistemi buna göre uzuv üretir
enum class BodyPlan : uint8_t {
    Biped,      // 2 bacak, 2 kol (oyuncu, iskelet, zombi)
    Quadruped,  // 4 bacak (inek, koyun, domuz, at)
    Bird,       // 2 bacak + kanat (tavuk, muhabbet)
    Fish,       // gövde + kuyruk yüzgeci
    Amphibian,  // 4 kısa bacak + uzun arka (kurbağa, aksolotl)
    Insect,     // 6 bacak (arı, örümcek)
    Floating,   // süzülen (enderman, bat)
    Serpent     // yılansı (eel)
};

// yürüyüş tipi — animasyonda bacak fazı/kayma üretir
enum class Gait : uint8_t {
    Walk, Hop, Swim, Fly, Crawl, Slither, Hover, Gallop
};

enum class MobAi2 : uint8_t {
    Passive,    // otlanır, kaçar
    Skittish,   // yaklaşınca kaçar (tavşan, kuş)
    Neutral,    // vurulursa saldırır (arı, kurt)
    Hostile,    // saldırır (zombi, iskelet)
    Aquatic,    // suda yaşar
    Flying      // uçar
};

struct MobDef {
    uint16_t id;
    const char* key;
    const char* name;
    BodyPlan plan;
    Gait gait;
    MobAi2 ai;
    float health;
    float speed;        // blok/sn
    float width, height;
    float damage;
    uint32_t bodyColor;
    uint32_t accentColor;
    uint8_t legCount;
    bool canSwim, canFly, tameable, rideable;
    uint8_t animStyle;  // 0=standart 1=zipzıp 2=kayma 3=kanat 4=yılan
};

enum MobId : uint16_t {
    MOB_NONE = 0,
    MOB_COD, MOB_SALMON, MOB_TROPICAL, MOB_PUFFERFISH, MOB_CLOWNFISH, MOB_ANGELFISH,
    MOB_BASS, MOB_SARDINE, MOB_TUNA, MOB_TROUT, MOB_MARLIN, MOB_EEL, MOB_RAY,
    MOB_SQUID_GENERAL,
    MOB_COW, MOB_SHEEP, MOB_PIG, MOB_GOAT, MOB_RABBIT, MOB_HORSE, MOB_WOLF, MOB_CAT,
    MOB_FOX, MOB_PANDA, MOB_LLAMA, MOB_POLAR_BEAR,
    MOB_CHICKEN, MOB_FROG, MOB_AXOLOTL, MOB_TURTLE,
    MOB_BEE, MOB_BAT, MOB_ZOMBIE,
    MOB_SPIDER, MOB_CREEPER, MOB_SKELETON, MOB_ENDERMAN,
    MOB_COUNT
};

inline const std::array<MobDef, MOB_COUNT>& mobRegistry() {
    static const std::array<MobDef, MOB_COUNT> reg = {{
        {MOB_NONE, "none", "-", BodyPlan::Biped, Gait::Walk, MobAi2::Passive, 1, 0, 0, 0, 0, 0, 0, 0, false,false,false,false,0},
        // ---- balıklar (14) ----
        {MOB_COD,"cod","Morina",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,1.2f,0.5f,0.5f,0,0x8a9a7a,0xd8d8c0,0,true,false,false,false,2},
        {MOB_SALMON,"salmon","Somon",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,1.4f,0.5f,0.5f,0,0xc87a5a,0xe8b0a0,0,true,false,false,false,2},
        {MOB_TROPICAL,"tropical","Tropikal Balık",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,1.6f,0.4f,0.4f,0,0xf0a020,0x40c0e0,0,true,false,false,false,2},
        {MOB_PUFFERFISH,"pufferfish","Balon Balığı",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,0.9f,0.5f,0.5f,1,0xe0c040,0xf0e080,0,true,false,false,false,2},
        {MOB_CLOWNFISH,"clownfish","Palyaço Balığı",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,1.5f,0.4f,0.4f,0,0xe05010,0xf0f0f0,0,true,false,false,false,2},
        {MOB_ANGELFISH,"angelfish","Melek Balığı",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,1.3f,0.5f,0.6f,0,0xe0e040,0x303030,0,true,false,false,false,2},
        {MOB_BASS,"bass","Levrek",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,4,1.4f,0.6f,0.5f,0,0x5a7a5a,0x9ab09a,0,true,false,false,false,2},
        {MOB_SARDINE,"sardine","Sardalya",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,2,1.8f,0.3f,0.3f,0,0x8aa0b0,0xd0e0f0,0,true,false,false,false,2},
        {MOB_TUNA,"tuna","Ton Balığı",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,8,1.6f,0.9f,0.7f,0,0x3a4a6a,0x8aa0c0,0,true,false,false,false,2},
        {MOB_TROUT,"trout","Alabalık",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,3,1.5f,0.5f,0.5f,0,0x6a8a6a,0xd0a0a0,0,true,false,false,false,2},
        {MOB_MARLIN,"marlin","Kılıç Balığı",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,10,2.2f,1.1f,0.8f,2,0x2a3a5a,0xc0d0e0,0,true,false,false,false,2},
        {MOB_EEL,"eel","Yılan Balığı",BodyPlan::Serpent,Gait::Slither,MobAi2::Aquatic,4,1.3f,0.4f,0.4f,1,0x3a4a3a,0x7a8a6a,0,true,false,false,false,4},
        {MOB_RAY,"ray","Vatoz",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,6,1.1f,1.0f,0.3f,0,0x5a5a6a,0x9a9aaa,0,true,false,false,false,2},
        {MOB_SQUID_GENERAL,"squid_general","Kalamar",BodyPlan::Fish,Gait::Swim,MobAi2::Aquatic,6,1.0f,0.6f,0.8f,0,0xd0a0a0,0xe8c0c0,8,true,false,false,false,4},
        // ---- memeliler (12) ----
        {MOB_COW,"cow","İnek",BodyPlan::Quadruped,Gait::Walk,MobAi2::Passive,10,1.6f,1.2f,1.3f,0,0x4a3a2a,0xe8e0d8,4,false,false,false,false,0},
        {MOB_SHEEP,"sheep","Koyun",BodyPlan::Quadruped,Gait::Walk,MobAi2::Passive,8,1.5f,1.0f,1.2f,0,0xe8e8e0,0xd0c8b8,4,false,false,false,false,0},
        {MOB_PIG,"pig","Domuz",BodyPlan::Quadruped,Gait::Walk,MobAi2::Passive,10,1.7f,1.0f,1.0f,0,0xe09aa0,0xc07a80,4,false,false,true,true,0},
        {MOB_GOAT,"goat","Keçi",BodyPlan::Quadruped,Gait::Walk,MobAi2::Neutral,10,2.2f,1.0f,1.3f,2,0xd8d0c0,0x8a8068,4,false,false,false,true,0},
        {MOB_RABBIT,"rabbit","Tavşan",BodyPlan::Quadruped,Gait::Hop,MobAi2::Skittish,3,2.8f,0.5f,0.5f,0,0xb0a090,0xe8e0d8,4,false,false,true,false,1},
        {MOB_HORSE,"horse","At",BodyPlan::Quadruped,Gait::Gallop,MobAi2::Passive,20,3.6f,1.3f,1.7f,0,0x8a6a4a,0x4a3424,4,false,false,true,true,0},
        {MOB_WOLF,"wolf","Kurt",BodyPlan::Quadruped,Gait::Walk,MobAi2::Neutral,8,2.6f,0.8f,1.0f,3,0x8a8a92,0xd8d8e0,4,false,false,true,false,0},
        {MOB_CAT,"cat","Kedi",BodyPlan::Quadruped,Gait::Walk,MobAi2::Skittish,10,2.2f,0.6f,0.8f,1,0xd0a060,0xf0e0c0,4,false,false,true,false,0},
        {MOB_FOX,"fox","Tilki",BodyPlan::Quadruped,Gait::Walk,MobAi2::Skittish,10,2.4f,0.7f,0.9f,1,0xd07030,0xf0f0e8,4,false,false,false,false,0},
        {MOB_PANDA,"panda","Panda",BodyPlan::Quadruped,Gait::Walk,MobAi2::Passive,20,1.4f,1.4f,1.5f,0,0xe8e8e8,0x2a2a2a,4,false,false,false,false,0},
        {MOB_LLAMA,"llama","Lama",BodyPlan::Quadruped,Gait::Walk,MobAi2::Neutral,15,2.0f,1.1f,1.9f,2,0xc8b090,0xf0e8d8,4,false,false,true,true,0},
        {MOB_POLAR_BEAR,"polar_bear","Kutup Ayısı",BodyPlan::Quadruped,Gait::Walk,MobAi2::Hostile,30,2.4f,1.4f,1.5f,6,0xf0f0e8,0xd8d8d0,4,true,false,false,false,0},
        // ---- kuş / amfibi / diğer (4) ----
        {MOB_CHICKEN,"chicken","Tavuk",BodyPlan::Bird,Gait::Walk,MobAi2::Skittish,4,1.4f,0.4f,0.7f,0,0xf0f0e8,0xe03020,2,false,false,false,false,3},
        {MOB_FROG,"frog","Kurbağa",BodyPlan::Amphibian,Gait::Hop,MobAi2::Skittish,4,2.0f,0.4f,0.4f,0,0x5a9a4a,0x8ac87a,4,true,false,false,false,1},
        {MOB_AXOLOTL,"axolotl","Aksolotl",BodyPlan::Amphibian,Gait::Swim,MobAi2::Aquatic,6,1.8f,0.5f,0.4f,1,0xf0a0c0,0xe070a0,4,true,false,false,false,2},
        {MOB_TURTLE,"turtle","Kaplumbağa",BodyPlan::Quadruped,Gait::Crawl,MobAi2::Passive,15,0.7f,0.8f,0.5f,0,0x4a8a4a,0x8a6a3a,4,true,false,false,false,0},
        // ---- böcek / uçan / düşman (5) ----
        {MOB_BEE,"bee","Arı",BodyPlan::Insect,Gait::Fly,MobAi2::Neutral,6,3.2f,0.4f,0.4f,2,0xe0c020,0x2a2a2a,6,false,true,false,false,3},
        {MOB_BAT,"bat","Yarasa",BodyPlan::Floating,Gait::Fly,MobAi2::Flying,4,3.8f,0.4f,0.3f,0,0x4a3a2a,0x6a5a4a,2,false,true,false,false,3},
        {MOB_ZOMBIE,"zombie","Zombi",BodyPlan::Biped,Gait::Walk,MobAi2::Hostile,20,1.6f,0.6f,1.9f,4,0x4a7a4a,0x2a3a5a,2,false,false,false,false,0},
        {MOB_SPIDER,"spider","Örümcek",BodyPlan::Insect,Gait::Crawl,MobAi2::Hostile,16,2.4f,1.0f,0.7f,3,0x3a2a2a,0xa02020,8,false,false,false,false,0},
        {MOB_CREEPER,"creeper","Creeper",BodyPlan::Biped,Gait::Walk,MobAi2::Hostile,20,1.4f,0.6f,1.7f,8,0x4aa04a,0x2a5a2a,0,false,false,false,false,0},
        {MOB_SKELETON,"skeleton","İskelet",BodyPlan::Biped,Gait::Walk,MobAi2::Hostile,20,1.8f,0.6f,1.9f,3,0xd8d8d8,0xb0b0b0,2,false,false,false,false,0},
        {MOB_ENDERMAN,"enderman","Enderman",BodyPlan::Floating,Gait::Hover,MobAi2::Hostile,40,3.0f,0.6f,2.6f,7,0x1a1a2a,0x8a4ac0,2,false,false,false,false,0},
    }};
    return reg;
}

inline const MobDef& mobDef(uint16_t id) {
    const auto& r = mobRegistry();
    return id < MOB_COUNT ? r[id] : r[0];
}

// ---------------- balık kombinasyonları (125) ----------------
// 5 tür × 5 renk × 5 desen. Görsel varyant; eşya sayımına DAHİL DEĞİL.
struct FishVariant {
    uint8_t species;   // 0..4
    uint8_t color;     // 0..4
    uint8_t pattern;   // 0..4
    uint16_t mobId;    // hangi balık temel alınacak
};

constexpr int kFishCombos = 125;

inline const std::array<FishVariant, kFishCombos>& fishVariants() {
    static std::array<FishVariant, kFishCombos> v{};
    static bool init = false;
    if (!init) {
        init = true;
        const uint16_t base[5] = {MOB_SARDINE, MOB_BASS, MOB_TROUT, MOB_TUNA, MOB_MARLIN};
        for (int s = 0; s < 5; ++s)
            for (int c = 0; c < 5; ++c)
                for (int p = 0; p < 5; ++p) {
                    int i = (s * 5 + c) * 5 + p;
                    v[i].species = static_cast<uint8_t>(s);
                    v[i].color = static_cast<uint8_t>(c);
                    v[i].pattern = static_cast<uint8_t>(p);
                    v[i].mobId = base[s];
                }
    }
    return v;
}

// renk paleti (kombinasyon)
constexpr uint32_t kFishColors[5] = {0x3a7ad0, 0xd03a3a, 0x3ad05a, 0xe0c030, 0x8a3ad0};
constexpr uint32_t kFishPatterns[5] = {0xffffff, 0x202020, 0xffe080, 0x404860, 0xf0c020};

} // namespace craftya
