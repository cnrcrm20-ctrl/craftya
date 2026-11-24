// Craftya Engine — dünya kaydı (.craftmap), ayarlar, profil
#pragma once
#include "craftya/core.hpp"
#include "craftya/world.hpp"
#include "craftya/inventory.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace craftya {

// blok bölgesi delta kaydı (tam chunk değil, sadece değişenler)
struct BlockEdit {
    int32_t x, y, z;
    uint8_t block;
};

constexpr char kMagic[8] = {'C', 'R', 'F', 'T', 'Y', 'A', '0', '1'};
constexpr uint16_t kSaveVersion = 1;

class WorldSave {
public:
    // dünya + oyuncu konumu + envanter → ikili blob
    static std::vector<uint8_t> serialize(const World& world, uint32_t seed,
                                          const Vec3f& playerPos, float yaw, float pitch,
                                          const Inventory& inv,
                                          const std::vector<BlockEdit>& edits,
                                          const std::string& name);

    struct Loaded {
        uint32_t seed = 0;
        Vec3f playerPos{0, 64, 0};
        float yaw = 0, pitch = 0;
        Inventory inv;
        std::vector<BlockEdit> edits;
        std::string name;
        bool valid = false;
    };
    static Loaded deserialize(const std::vector<uint8_t>& blob);
    static bool applyEdits(World& world, const std::vector<BlockEdit>& edits);

    // dosya yardımcıları (platform dosya sistemi enjekte edilebilir)
    static bool saveToFile(const std::string& path, const std::vector<uint8_t>& blob);
    static std::vector<uint8_t> loadFromFile(const std::string& path);

    // RLE: 5-bit paletli chunk verisini sıkıştır
    static std::vector<uint8_t> rleCompress(const std::vector<uint8_t>& in);
    static std::vector<uint8_t> rleDecompress(const std::vector<uint8_t>& in, size_t expected);
};

// oyun ayarları (JSON ile uyumlu, sunucudan güncellenebilir)
struct GameSettings {
    // grafik
    int renderDistance = 6;
    int chunkProcessing = 7;
    int shadowMapSize = 1024;
    bool shadowsEnabled = true;
    float shadowDistance = 56.f;
    bool fogEnabled = true;
    float fov = 70.f;
    float mouseSensitivity = 0.5f;
    int fpsMode = 60;           // 60 / 120 / 0 = sınırsız
    int vsyncMode = 1;
    float resolutionScale = 0.8f;   // 0.7 / 0.8 / 1.0
    bool antialias = false;

    // ses
    float masterVolume = 1.f;
    float musicVolume = 0.6f;
    float sfxVolume = 0.9f;
    bool vibration = true;

    // kontrol
    bool invertY = false;
    float touchSensitivity = 1.f;
    bool autoJump = false;
    bool showCrosshair = true;
    bool showFps = true;
    bool showCoordinates = false;

    static GameSettings loadDefaults();
    std::string toJson() const;
    static GameSettings fromJson(const std::string& json);
};

// oyuncu profili
struct PlayerProfile {
    std::string username;
    std::string email;
    uint32_t totalPlaySeconds = 0;
    uint32_t blocksMined = 0;
    uint32_t blocksPlaced = 0;
    uint32_t mobsKilled = 0;
    uint32_t duelsWon = 0;
    uint32_t duelsLost = 0;
    uint32_t deaths = 0;
    uint32_t sessionsPlayed = 0;
    float bestSurvivalSeconds = 0.f;
};

} // namespace craftya
