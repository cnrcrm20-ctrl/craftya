// Craftya Engine — birleşik motor cephesi
#include "craftya/engine.hpp"
#include "craftya/json.hpp"
#include <algorithm>
#include <cmath>

namespace craftya {

// Dokunmatik bakış: 1 px ≈ 0.0026 rad (~0.15°) — mobilde doğal his.
// Ayar çarpanı (0.5–2.0) kullanıcı tarafında uygulanır.
static constexpr float LOOK_SENS_MOBILE = 0.0026f;

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
    menus_.closeAll();      // menü tamamen kapanmalı, yoksa HUD girdisi çalışmaz
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
    menus_.closeAll();  // yüklemede de menü tamamen kapansın
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
        if (menus_.current() == MenuId::Inventory) {
            handleInventoryTap(x, y);
            return;
        }
        menus_.onTap(x, y, static_cast<float>(width_), static_cast<float>(height_));
        return;
    }
    if (state_ == EngineState::MainMenu || state_ == EngineState::Paused) {
        // Ana menü/duraklatma: dokunuşu gerçekten menüye ilet.
        // (Önceden onTap çağrılıyordu ama itemRects_ her kare
        //  yeniden hesaplandığı için dokunma hiçbir zaman eşleşmiyordu.)
        menus_.onTap(x, y, static_cast<float>(width_), static_cast<float>(height_));
        return;
    }
    if (state_ == EngineState::Playing) hud_.onTouchDown(pointerId, x, y);
}

void Engine::onTouchMove(int pointerId, float x, float y) {
    if (state_ == EngineState::Playing) hud_.onTouchMove(pointerId, x, y);
}

void Engine::onTouchUp(int pointerId) {
    // Menüde de takılı kalan parmak durumlarını temizle.
    // Eskiden yalnızca Playing'de çağrılıyordu; menüde basılı kalan
    // pointer'lar bir sonraki dokunuşu yutuyordu.
    hud_.onTouchUp(pointerId);
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
    // Sıra önemli:
    //  1) Oyun içinde AÇIK bir menü varsa (envanter/mod paneli/ayarlar)
    //     önce onu kapat.
    //  2) Menü yoksa geri tuşu oyunu DURAKLATIR.
    // (Eskiden oyun içinde back() çağrılıp menü kapatılıyor, oyun
    //  Playing'de kalıyor ve oyuncu arayüzsüz kalıyordu.)
    if (state_ == EngineState::Playing) {
        if (menus_.isOpen()) { menus_.back(); return; }
        state_ = EngineState::Paused;
        menus_.open(MenuId::Pause);
        return;
    }
    if (menus_.isOpen()) { menus_.back(); return; }
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
            wirePauseMenu();
            break;
        default:
            break;
    }
    mesh_.update(dt, static_cast<uint32_t>(sessionSeconds_ * 1000.f));
    rebuildGui();
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
        if (scr.items.size() >= 3) {
            scr.items[0].action = [this] {
                startNewWorld(cfg_.seed ? cfg_.seed : 424242);
            };
            scr.items[1].action = [this] {
                // lobi: gerçek mod listesi
                std::vector<MenuScreen::ModeRow> rows = {
                    {"Duello", "1v1 arena", lobby.duello, 2, true},
                    {"Takimli", "5v5 savas", lobby.takimli, 10, true},
                    {"Mega", "6v6 karmasa", lobby.mega, 12, true},
                };
                menus_.showModePanel(rows);
            };
            scr.items[2].action = [this] { menus_.close(); };
        }
    } else if (scr.id == MenuId::Settings) {
        // ayarları gerçekten değiştir (dokununca değer döner)
        if (scr.items.size() >= 4) {
            scr.items[0].sublabel = "mesafe " + std::to_string(cfg_.settings.renderDistance) +
                                    " chunk / golge " + (cfg_.settings.shadowsEnabled ? "acik" : "kapali");
            scr.items[1].sublabel = "muzik " + std::to_string(static_cast<int>(cfg_.settings.musicVolume * 100)) +
                                    "% / efekt " + std::to_string(static_cast<int>(cfg_.settings.sfxVolume * 100)) + "%";
            scr.items[2].sublabel = "hassasiyet " + std::to_string(static_cast<int>(cfg_.settings.touchSensitivity * 100)) +
                                    "% / ters Y " + (cfg_.settings.invertY ? "acik" : "kapali");
            scr.items[0].action = [this] {
                cfg_.settings.renderDistance = cfg_.settings.renderDistance >= 10 ? 4
                                             : cfg_.settings.renderDistance + 2;
                cfg_.settings.shadowsEnabled = !cfg_.settings.shadowsEnabled;
                applySettings();
                toast_.show("mesafe: " + std::to_string(cfg_.settings.renderDistance) + " chunk", 1.6f);
            };
            scr.items[1].action = [this] {
                cfg_.settings.musicVolume = cfg_.settings.musicVolume > 0.3f ? 0.f : 0.6f;
                audio_.setMusicVolume(cfg_.settings.musicVolume);
                toast_.show(cfg_.settings.musicVolume > 0.f ? "muzik acik" : "muzik kapali", 1.6f);
            };
            scr.items[2].action = [this] {
                cfg_.settings.touchSensitivity = cfg_.settings.touchSensitivity >= 2.0f ? 0.5f
                                              : cfg_.settings.touchSensitivity * 1.5f;
                toast_.show("hassasiyet guncellendi", 1.6f);
            };
            scr.items[3].action = [this] { menus_.back(); };
        }
    } else if (scr.id == MenuId::ModeSelect) {
        for (size_t i = 0; i + 1 < scr.items.size(); ++i) {
            uint8_t m = static_cast<uint8_t>(i);
            scr.items[i].action = [this, m] {
                setMode(m);
                startNewWorld(cfg_.seed ? cfg_.seed : 424242);
                toast_.show(mode_ == 0 ? "Duello basladi" : (mode_ == 1 ? "Takimli basladi" : "Mega basladi"), 2.f);
            };
        }
        scr.items.back().action = [this] { menus_.back(); };
    }
}

void Engine::wirePauseMenu() {
    auto& scr = menus_.screen();
    if (scr.id != MenuId::Pause) return;
    if (scr.items.size() >= 3) {
        scr.items[0].action = [this] {
            menus_.close();
            state_ = EngineState::Playing;
        };
        scr.items[1].action = [this] { menus_.open(MenuId::Settings); };
        scr.items[2].action = [this] {
            if (cfg_.autoSave) saveWorld();
            game_.reset();
            state_ = EngineState::MainMenu;
            menus_.open(MenuId::Main);
        };
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
    // DİKKAT: hassasiyet yalnızca BURADA uygulanır. Game katmanı da
    // MOUSE_SENS ile çarptığı için eskiden iki kez ölçekleniyordu ve
    // 100 px'lik kaydırma 0.0009 rad'a düşüp ekran hiç dönmüyordu.
    in.lookDX *= LOOK_SENS_MOBILE;
    in.lookDY *= LOOK_SENS_MOBILE * (cfg_.settings.invertY ? -1.f : 1.f);

    // --- arayüz dokunmaları ---
    if (hud_.touch.inventoryTapped) {
        hud_.touch.inventoryTapped = false;
        if (menus_.current() == MenuId::Inventory) menus_.close();
        else { menus_.open(MenuId::Inventory); menus_.screen().inv = &inv_; }
        rebuildGui();
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

    // --- SES + TİTREŞİM (mobil geri bildirim) ---
    // Motor ses kütüphanesi vardı ama hiçbir yerde çalınmıyordu: oyun sessizdi.
    // "KIR" butonu: önce önündeki canlıya vur, yoksa bloğu kır.
    // (Mobilde tek buton hem savaş hem kazma olarak çalışır.)
    if (in.breakBlock && game_->player().onGround) {
        float damage = 4.0f;
        bool died = false;
        uint16_t diedType = 0;
        Vec3f eye = game_->camera().pos;
        auto* hitMob = game_->ecosystem().hitTarget(eye, game_->camera().forward(),
                                                    3.2f, damage, died, diedType);
        if (hitMob) {
            audio_.play(audio_.sfx(SfxId::Hit), 0.9f);
            if (cfg_.settings.vibration) vibration_ = 0.045f;
            if (died) {
                audio_.play(audio_.sfx(SfxId::Eat), 0.7f);
                toast_.show("avlandi", 1.2f);
                game_->ecosystem().reapDead();
            }
        } else if (in.breakBlock) {
            audio_.play(audio_.sfx(SfxId::Break), 0.85f);
            if (cfg_.settings.vibration) vibration_ = 0.05f;
        }
    }
    if (in.placeBlock) {
        audio_.play(audio_.sfx(SfxId::Place), 0.8f);
        if (cfg_.settings.vibration) vibration_ = 0.03f;
    }
    if (in.jump && game_->player().onGround) {
        audio_.play(audio_.sfx(SfxId::Jump), 0.6f);
    }

    // ---- hedef (nişangâh) güncelle: mobil savaş geri bildirimi ----
    {
        Vec3f eye = game_->camera().pos;
        Vec3f fwd = game_->camera().forward();
        auto* t = game_->ecosystem().findTarget(eye, fwd, 3.4f);
        hud_.touch.hasTarget = (t != nullptr);
        if (t) {
            const MobDef& def = mobDef(t->type);
            hud_.touch.targetMaxHealth = def.health;
            hud_.touch.targetMobHealth = t->health;
            hud_.touch.targetHealth01 =
                def.health > 0.f ? std::clamp(t->health / def.health, 0.f, 1.f) : 0.f;
        }
        if (hud_.touch.hitFlash > 0.f) hud_.touch.hitFlash -= dt;
    }

    game_->update(dt, in);
    applySettings();

    // yürüme sesi: zeminde ve hız varsa adım aralığıyla
    {
        const Body& pb = game_->player();
        float hspeed = std::sqrt(pb.vel.x * pb.vel.x + pb.vel.z * pb.vel.z);
        if (pb.onGround && hspeed > 1.2f) {
            stepTimer_ += dt * (hspeed / 4.6f);
            if (stepTimer_ > 0.42f) {
                stepTimer_ = 0.f;
                audio_.play(audio_.sfx(SfxId::Step), 0.45f);
            }
        } else {
            stepTimer_ = 0.f;
        }
    }

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

// envanter ızgarası: dokunulan yuvayı seçer, hotbar'a taşır
void Engine::handleInventoryTap(float x, float y) {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float pw = std::min(w * 0.94f, 760.f);
    const float px = (w - pw) * 0.5f;
    const float availW = pw - 56.f;
    const float slot = std::min((availW - 8.f * 8.f) / 9.f, h * 0.15f);
    const float gw = 9 * (slot + 8.f) - 8.f;
    const float gx = px + (pw - gw) * 0.5f;

    // panel dikey konumu (menü çizimiyle aynı hesap)
    const float ph = std::min(130.f + 4.f * (slot + 8.f) + 36.f, h * 0.92f);
    const float py = (h - ph) * 0.5f;
    const float gy = py + 108.f;

    int idx = -1;
    for (int i = 0; i < Inventory::TOTAL && idx < 0; ++i) {
        int c = i % 9;
        int row = (i < Inventory::HOTBAR) ? 3 : ((i - Inventory::HOTBAR) / 9);
        float sx = gx + c * (slot + 8.f);
        float sy = gy + row * (slot + 8.f);
        if (x >= sx && x < sx + slot && y >= sy && y < sy + slot) idx = i;
    }
    if (idx < 0) {
        // panel dışına dokunma → envanteri kapat
        if (x < px || x > px + pw || y < py || y > py + ph) menus_.close();
        return;
    }
    menus_.screen().cursorSlot = idx;
    if (idx < Inventory::HOTBAR) {
        inv_.select(idx);
        toast_.show("yuva " + std::to_string(idx + 1) + " secildi", 1.2f);
    } else {
        // ana envanterden hotbar'a taşı
        ItemStack st = inv_.take(idx, 999);
        if (!st.empty()) {
            int left = inv_.add(st.itemId, st.count, st.durability);
            if (left > 0) { inv_.at(idx) = st; toast_.show("hotbar dolu", 1.4f); }
            else toast_.show("hotbar'a tasindi", 1.4f);
        }
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
    // envanter menüsü açıksa motorun envanterini göster
    if (menus_.current() == MenuId::Inventory) menus_.screen().inv = &inv_;
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
