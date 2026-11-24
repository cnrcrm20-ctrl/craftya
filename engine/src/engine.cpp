// Craftya Engine — birleşik motor cephesi
#include "craftya/engine.hpp"
#include "craftya/json.hpp"
#include <algorithm>
#include <cmath>

namespace craftya {

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

bool Engine::init(const EngineConfig& cfg) {
    cfg_ = cfg;
    seed_ = cfg.seed ? cfg.seed : 424242;

    items_.registerDefaults();
    recipes_.registerDefaults();
    audio_.loadProceduralSfx();
    audio_.setMasterVolume(cfg_.settings.masterVolume);
    audio_.setMusicVolume(cfg_.settings.musicVolume);
    audio_.setSfxVolume(cfg_.settings.sfxVolume);

    applySettings();
    menus_.open(MenuId::Main);
    state_ = EngineState::MainMenu;

    // joystick ve buton yerleşimi
    hud_.resize(static_cast<float>(width_), static_cast<float>(height_), dpi_);
    return true;
}

void Engine::shutdown() {
    if (game_ && cfg_.autoSave) saveWorld();
    audio_.stopAll();
    mesh_.removePeer(localDevice_);
    game_.reset();
    state_ = EngineState::Boot;
}

void Engine::applySettings() {
    if (!game_) return;
    auto& s = game_->settings();
    s.fovDeg = cfg_.settings.fov;
    s.renderChunks = cfg_.settings.renderDistance;
    s.resolutionScale = cfg_.settings.resolutionScale;
    s.shadows = cfg_.settings.shadowsEnabled;
    s.targetFps = cfg_.settings.fpsMode;
    s.touchSensitivity = cfg_.settings.touchSensitivity;
}

// ---------------- yüzey ----------------
void Engine::onSurfaceCreated(int w, int h, float dpi) {
    width_ = w; height_ = h; dpi_ = dpi > 0 ? dpi : 1.f;
    hud_.resize(static_cast<float>(w), static_cast<float>(h), dpi_);
    surfaceValid_ = true;
}

void Engine::onSurfaceChanged(int w, int h, float dpi) {
    onSurfaceCreated(w, h, dpi);
}

void Engine::onSurfaceLost() { surfaceValid_ = false; }

void Engine::onPause() {
    if (state_ == EngineState::Playing) {
        state_ = EngineState::Paused;
        menus_.open(MenuId::Pause);
    }
    if (cfg_.autoSave) saveWorld();
    audio_.stopAll();
}

void Engine::onResume() {
    if (state_ == EngineState::Paused) {
        state_ = EngineState::Playing;
        menus_.close();
    }
}

// ---------------- dünya ----------------
bool Engine::startNewWorld(uint32_t seed) {
    seed_ = seed ? seed : 424242;
    game_ = std::make_unique<Game>(seed_, true);
    applySettings();
    game_->settings().fovDeg = cfg_.settings.fov;
    edits_.clear();
    inv_.clear();

    // başlangıç envanteri
    inv_.add(STONE, 64);
    inv_.add(OAK_PLANKS, 32);
    inv_.add(100, 1);      // tahta kazma
    inv_.add(140, 16);     // meşale
    inv_.select(0);

    // yerel oyuncu varlığı
    entities_ = World2{};
    EntityId pid = entities_.spawn(EntityKind::Player);
    Entity* p = entities_.get(pid);
    if (p) p->transform.pos = game_->player().pos;

    // birkaç mob
    for (int i = 0; i < 4; ++i) {
        EntityId m = entities_.spawn(EntityKind::Mob);
        Entity* e = entities_.get(m);
        if (e) e->transform.pos = game_->player().pos + Vec3f{6.f * (i + 1), 0.f, 4.f};
    }

    sessionSeconds_ = 0.f;
    autoSaveTimer_ = 0.f;
    state_ = EngineState::Playing;
    menus_.close();
    toast_.show("Dunya hazir — " + std::to_string(seed_));
    return true;
}

bool Engine::loadWorld() {
    auto blob = WorldSave::loadFromFile(cfg_.savePath);
    if (blob.empty()) return false;
    auto L = WorldSave::deserialize(blob);
    if (!L.valid) return false;

    game_ = std::make_unique<Game>(L.seed, true);
    applySettings();
    WorldSave::applyEdits(game_->world(), L.edits);
    edits_ = L.edits;
    inv_ = L.inv;
    seed_ = L.seed;
    state_ = EngineState::Playing;
    menus_.close();
    toast_.show("Kayit yuklendi — " + L.name);
    return true;
}

bool Engine::saveWorld() {
    if (!game_) return false;
    auto blob = WorldSave::serialize(game_->world(), seed_, game_->player().pos,
                                     game_->camera().yaw, game_->camera().pitch,
                                     inv_, edits_, "Craftya Dunyasi");
    bool ok = WorldSave::saveToFile(cfg_.savePath, blob);
    if (ok) toast_.show("Kaydedildi");
    return ok;
}

void Engine::setMode(uint8_t mode) {
    mode_ = mode < 3 ? mode : 0;
    static const char* names[3] = {"Duello", "Takimli", "Mega"};
    toast_.show(std::string("Mod: ") + names[mode_]);
}

// ---------------- lobi ----------------
void Engine::refreshLobbyFromServer(const std::string& jsonResponse) {
    // Supabase RPC yanıtı: {"duello":12,"takimli":40,"mega":8,"online":57}
    Json j = Json::parse(jsonResponse);
    if (j.isObject()) {
        lobby.duello = j["duello"].asInt(lobby.duello);
        lobby.takimli = j["takimli"].asInt(lobby.takimli);
        lobby.mega = j["mega"].asInt(lobby.mega);
        lobby.online = j["online"].asInt(lobby.online);
    }
    menus_.showModePanel({
        {"Duello", "1v1", lobby.duello, 2, true},
        {"Takimli", "5v5", lobby.takimli, 10, true},
        {"Mega", "6v6", lobby.mega, 12, true},
    });
}

void Engine::chatMessage(const std::string& author, const std::string& msg) {
    chat_.add(author, msg);
}

PlayerSnapshot Engine::localSnapshot() const {
    PlayerSnapshot s;
    if (game_) {
        s.pos = game_->player().pos;
        s.yaw = game_->camera().yaw;
        s.pitch = game_->camera().pitch;
        const Body& b = game_->player();
        s.flags = (b.onGround ? 8 : 0) | (b.inWater ? 4 : 0);
    }
    s.health = 20.f;
    return s;
}

// ---------------- girdi ----------------
void Engine::onTouchDown(int pointerId, float x, float y) {
    // sohbet klavyesi açıksa girdi oraya gider
    if (chat_.inputOpen() && state_ == EngineState::Playing) {
        handleKeyboardTap(x, y);
        return;
    }
    if (state_ == EngineState::Playing && menus_.isOpen()) {
        menus_.onTap(x, y, static_cast<float>(width_), static_cast<float>(height_));
        return;
    }
    if (state_ == EngineState::MainMenu || state_ == EngineState::Paused) {
        menus_.onTap(x, y, static_cast<float>(width_), static_cast<float>(height_));
        return;
    }
    if (state_ == EngineState::Playing) hud_.onTouchDown(pointerId, x, y);
}

void Engine::onTouchMove(int pointerId, float x, float y) {
    if (state_ == EngineState::Playing) hud_.onTouchMove(pointerId, x, y);
}

void Engine::onTouchUp(int pointerId) {
    if (state_ == EngineState::Playing) hud_.onTouchUp(pointerId);
}

void Engine::onKey(int keyCode, bool down) {
    if (!down) return;
    if (state_ == EngineState::Playing) {
        if (keyCode == 3) {   // ESC
            state_ = EngineState::Paused;
            menus_.open(MenuId::Pause);
        }
        return;
    }
    menus_.onKey(keyCode);
}

void Engine::onBackPressed() {
    if (menus_.isOpen()) { menus_.back(); return; }
    if (state_ == EngineState::Playing) {
        state_ = EngineState::Paused;
        menus_.open(MenuId::Pause);
    }
}

// ---------------- kare ----------------
void Engine::update(float dt) {
    if (dt > 0.1f) dt = 0.1f;   // sekme/duraklatma sıçraması

    switch (state_) {
        case EngineState::MainMenu:
            tickMenu(dt);
            break;
        case EngineState::Playing:
            tickPlaying(dt);
            break;
        case EngineState::Paused:
            toast_.update(dt);
            break;
        default:
            break;
    }
    mesh_.update(dt, static_cast<uint32_t>(sessionSeconds_ * 1000.f));
}

void Engine::tickMenu(float dt) {
    toast_.update(dt);
    // ana menü aksiyonlarını bağla
    auto& scr = menus_.screen();
    if (scr.id == MenuId::Main) {
        for (size_t i = 0; i < scr.items.size(); ++i) {
            scr.items[i].action = nullptr;
        }
        if (scr.items.size() >= 1) scr.items[0].action = [this] { menus_.open(MenuId::Play); };
        if (scr.items.size() >= 2) scr.items[1].action = [this] { menus_.open(MenuId::Settings); };
    } else if (scr.id == MenuId::Play) {
        if (scr.items.size() >= 2) {
            scr.items[0].action = [this] { startNewWorld(cfg_.seed ? cfg_.seed : 424242); };
            scr.items[1].action = [this] { refreshLobbyFromServer("{}"); };
        }
    }
}

void Engine::tickPlaying(float dt) {
    sessionSeconds_ += dt;
    toast_.update(dt);
    chat_.update(dt);

    // dokunmatik girdi → InputState
    InputState in;
    in.moveX = hud_.touch.moveAxis.x;
    in.moveZ = hud_.touch.moveAxis.z;
    in.lookDX = hud_.touch.lookDx;
    in.lookDY = hud_.touch.lookDy;
    in.jump = hud_.touch.jumpHeld;
    in.sprint = hud_.touch.sprintHeld;
    in.breakBlock = hud_.touch.breakHeld;
    in.placeBlock = hud_.touch.useHeld;
    hud_.touch.lookDx = 0.f;
    hud_.touch.lookDy = 0.f;

    // bakış hassasiyeti
    in.lookDX *= 0.0035f * cfg_.settings.touchSensitivity;
    in.lookDY *= 0.0035f * cfg_.settings.touchSensitivity *
                 (cfg_.settings.invertY ? -1.f : 1.f);

    // --- arayüz dokunmaları ---
    if (hud_.touch.inventoryTapped) {
        hud_.touch.inventoryTapped = false;
        if (menus_.current() == MenuId::Inventory) menus_.close();
        else { menus_.open(MenuId::Inventory); menus_.screen().inv = &inv_; }
    }
    if (hud_.touch.settingsTapped) {
        hud_.touch.settingsTapped = false;
        if (menus_.isOpen()) menus_.close();
        else menus_.open(MenuId::Settings);
    }
    if (hud_.touch.chatTapped) {
        hud_.touch.chatTapped = false;
        if (chat_.inputOpen()) chat_.closeInput();
        else chat_.openInput();
    }
    if (hud_.touch.hotbarTapped && hud_.touch.hotbarIndex >= 0) {
        inv_.select(hud_.touch.hotbarIndex);
        hud_.touch.hotbarTapped = false;
        hud_.touch.hotbarIndex = -1;
    }

    game_->update(dt, in);
    applySettings();

    // varlıklar
    EntityId player = INVALID_ENTITY;
    for (auto& e : entities_.entities())
        if (e.active && e.kind == EntityKind::Player) { player = e.id; break; }
    if (player != INVALID_ENTITY) {
        Entity* p = entities_.get(player);
        if (p) p->transform.pos = game_->player().pos;
    }
    entities_.updateMobs(game_->world(), dt, player);
    entities_.updateProjectiles(game_->world(), dt);
    entities_.applyGravityToDrops(game_->world(), dt);
    entities_.updateItemDrops(game_->world(), dt);

    // ses dinleyicisi
    audio_.setListener(game_->camera().pos, game_->camera().forward());

    // ağ yayını (20 Hz) + ışık güncellemesi
    mesh_.publishLocalState(localSnapshot(), static_cast<uint32_t>(sessionSeconds_ * 1000.f));

    // otomatik kayıt
    autoSaveTimer_ += dt;
    if (cfg_.autoSave && autoSaveTimer_ >= cfg_.autoSaveSeconds) {
        autoSaveTimer_ = 0.f;
        saveWorld();
    }
}

// gömülü sohbet klavyesi: dokunma → karakter
void Engine::handleKeyboardTap(float x, float y) {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float kbH = h * 0.40f;
    const float kbY = h - kbH;
    if (y < kbY) { chat_.closeInput(); return; }

    // GÖNDER / kapat şeridi
    float fieldW = w * 0.70f;
    float sendX = w * 0.04f + fieldW + 10.f;
    if (y < kbY + 60.f) {
        if (x >= sendX) {
            std::string msg = chat_.submit();
            if (!msg.empty()) {
                chat_.add("Sen", msg);
                mesh_.sendChat(msg);
                toast_.show("gonderildi", 2.f);
            } else {
                chat_.closeInput();
            }
        }
        return;
    }

    const char* rows[4] = {"1234567890", "qwertyuiop", "asdfghjklg", "zxcvbnm,.?"};
    float availH = kbH - 76.f;
    float keyW = w * 0.075f;
    float keyH = (availH - 4.f * 5.f) / 5.f;
    keyH = std::min(keyH, keyW * 1.15f);

    // boşluk + kapat satırı
    float sy = kbY + 64.f + 4.f * (keyH + 4.f);
    if (y >= sy) {
        float spaceW = w * 0.46f;
        if (x >= (w - spaceW) * 0.5f && x < (w + spaceW) * 0.5f) chat_.typeChar(' ');
        else chat_.closeInput();
        return;
    }

    for (int r = 0; r < 4; ++r) {
        int n = static_cast<int>(std::strlen(rows[r]));
        float rowW = n * (keyW + 4.f);
        float sx = (w - rowW) * 0.5f;
        float ky = kbY + 64.f + r * (keyH + 4.f);
        if (y < ky || y >= ky + keyH) continue;
        for (int i = 0; i < n; ++i) {
            float kx = sx + i * (keyW + 4.f);
            if (x < kx || x >= kx + keyW) continue;
            if (r == 2 && i == n - 1) chat_.backspace();   // SIL
            else chat_.typeChar(rows[r][i]);
            return;
        }
    }
}

void Engine::rebuildGui() {
    if (state_ != EngineState::Playing) return;
}

void Engine::render(DrawList* guiOut) {
    if (!guiOut) return;
    guiOut->clear();

    if (state_ == EngineState::Playing && game_) {
        float health = 20.f;
        for (auto& e : entities_.entities())
            if (e.active && e.kind == EntityKind::Player) health = e.health.current;
        hud_.build(*guiOut, inv_, health, 20.f,
                   game_->stats().fps, game_->player().pos,
                   static_cast<int>(mesh_.stats().avgRttMs),
                   static_cast<int>(mesh_.remotePlayers().size()),
                   sessionSeconds_);
        chat_.build(*guiOut, static_cast<float>(width_), static_cast<float>(height_));
        chat_.buildKeyboard(*guiOut, static_cast<float>(width_), static_cast<float>(height_));
        // sohbet girdisi açıkken dokunma alanları klavyeye gider
        if (chat_.inputOpen() && menus_.current() == MenuId::Inventory)
            menus_.screen().inv = &inv_;
    }
    if (menus_.isOpen() || state_ == EngineState::MainMenu ||
        state_ == EngineState::Paused) {
        menus_.build(*guiOut, static_cast<float>(width_), static_cast<float>(height_));
    }
    toast_.build(*guiOut, static_cast<float>(width_), static_cast<float>(height_));
}

} // namespace craftya
