// Craftya Engine — tek cephe: tüm alt sistemleri birleştiren Engine sınıfı
// Platform katmanı (Android NativeActivity / masaüstü pencere) yalnızca bunu çağırır.
#pragma once
#include "craftya/core.hpp"
#include "craftya/world.hpp"
#include "craftya/game.hpp"
#include "craftya/gui.hpp"
#include "craftya/inventory.hpp"
#include "craftya/audio.hpp"
#include "craftya/net.hpp"
#include "craftya/save.hpp"
#include "craftya/ecs.hpp"
#include "craftya/lighting.hpp"
#include "craftya/renderer.hpp"
#include <memory>
#include <string>
#include <vector>

namespace craftya {

// motor durumu — platform katmanı buna göre güncelleme/çizim yapar
enum class EngineState : uint8_t { Boot, MainMenu, Loading, Playing, Paused, Failed };

struct EngineConfig {
    GameSettings settings;
    std::string savePath = "craftya_world.craftmap";
    std::string settingsPath = "craftya_settings.json";
    bool autoSave = true;
    float autoSaveSeconds = 60.f;
    uint32_t seed = 0;
};

class Engine {
public:
    Engine();
    ~Engine();

    // ---- yaşam döngüsü ----
    bool init(const EngineConfig& cfg);
    void shutdown();

    // Android: yüzey oluştu/kayboldu (GLES bağlamı)
    void onSurfaceCreated(int widthPx, int heightPx, float dpiScale);
    void onSurfaceChanged(int widthPx, int heightPx, float dpiScale);
    void onSurfaceLost();
    void onPause();
    void onResume();

    // ---- kare ----
    void update(float dt);      // simülasyon
    void render(DrawList* guiOut);  // 3B çizim çağrıları + GUI listesi

    // ---- girdi ----
    void onTouchDown(int pointerId, float x, float y);
    void onTouchMove(int pointerId, float x, float y);
    void onTouchUp(int pointerId);
    void onKey(int keyCode, bool down);
    void onBackPressed();

    // ---- durum sorguları ----
    EngineState state() const { return state_; }
    const FrameStats& stats() const { return game_ ? game_->stats() : emptyStats_; }
    const MeshNetwork::NetStats& netStats() const { return mesh_.stats(); }
    Game& game() { return *game_; }
    Hud& hud() { return hud_; }
    MenuSystem& menus() { return menus_; }
    AudioMixer& audio() { return audio_; }
    MeshNetwork& net() { return mesh_; }
    World2& entities() { return entities_; }
    Inventory& inventory() { return inv_; }
    const GameSettings& settings() const { return cfg_.settings; }
    GameSettings& settings() { return cfg_.settings; }

    // ---- oyun akışı ----
    bool startNewWorld(uint32_t seed);
    bool loadWorld();
    bool saveWorld();
    void setMode(uint8_t mode);     // 0=düello 1=takımlı 2=mega
    uint8_t mode() const { return mode_; }

    // lobi/oyuncu listesi (NPC paneli için)
    struct LobbyStatus {
        int duello = 0;
        int takimli = 0;
        int mega = 0;
        int online = 0;
    };
    LobbyStatus lobby;
    void refreshLobbyFromServer(const std::string& jsonResponse);

    // bildirim
    void toast(const std::string& msg) { toast_.show(msg); }
    void chatMessage(const std::string& author, const std::string& msg);

    // yerel oyuncu durumu (ağ yayını için)
    PlayerSnapshot localSnapshot() const;

private:
    void applySettings();
    void tickPlaying(float dt);
    void tickMenu(float dt);
    void rebuildGui();
    void handleKeyboardTap(float x, float y);
    void handleInventoryTap(float x, float y);
    void wirePauseMenu();

    EngineConfig cfg_;
    EngineState state_ = EngineState::Boot;
    std::unique_ptr<Game> game_;
    Hud hud_;
    MenuSystem menus_;
    AudioMixer audio_;
    MeshNetwork mesh_;
    World2 entities_;
    Inventory inv_;
    ItemRegistry items_;
    CraftingRegistry recipes_;
    ChatLog chat_;
    Toast toast_;
    FrameStats emptyStats_;

    int width_ = 1280, height_ = 720;
    float dpi_ = 1.f;
    float autoSaveTimer_ = 0.f;
    float sessionSeconds_ = 0.f;
    uint8_t mode_ = 0;
    uint8_t localDevice_ = 1;
    uint32_t seed_ = 0;
    std::vector<BlockEdit> edits_;
    bool surfaceValid_ = false;
};

} // namespace craftya
