// Craftya Engine — GUI/HUD uygulaması
#include "craftya/gui.hpp"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace craftya {

namespace {
constexpr float PI = 3.14159265358979f;
const Color kWhite = Color{1, 1, 1, 1};
const Color kBlack = Color{0, 0, 0, 1};
const Color kPanel = Color{0.10f, 0.11f, 0.14f, 0.94f};
const Color kAccent = Color{0.24f, 0.78f, 0.44f, 1.f};
const Color kDanger = Color{0.86f, 0.26f, 0.26f, 1.f};

std::string fmtInt(long v) { return std::to_string(v); }
}

// ---------------- DrawList ----------------
void DrawList::rect(const Rect& r, const Color& c) {
    DrawItem d;
    d.cmd = DrawCmd::Rect;
    d.rect = r;
    d.color = c;
    items_.push_back(d);
}

void DrawList::texturedRect(const Rect& r, uint16_t texId, const Color& tint) {
    DrawItem d;
    d.cmd = DrawCmd::TexturedRect;
    d.rect = r;
    d.textureId = texId;
    d.color = (tint.a == 1.f && tint.r == 1.f && tint.g == 1.f && tint.b == 1.f)
              ? kWhite : tint;
    items_.push_back(d);
}

void DrawList::text(const std::string& s, float x, float y, float scale, const Color& c, int align) {
    DrawItem d;
    d.cmd = DrawCmd::Text;
    d.rect = Rect{x, y, 0, 0};
    d.text = s;
    d.scale = scale;
    d.color = c;
    d.align = align;
    items_.push_back(d);
}

void DrawList::line(float x1, float y1, float x2, float y2, const Color& c, float width) {
    DrawItem d;
    d.cmd = DrawCmd::Line;
    d.rect = Rect{x1, y1, 0, 0};
    d.x2 = x2; d.y2 = y2;
    d.color = c;
    d.lineWidth = width;
    items_.push_back(d);
}

void DrawList::circleButton(float cx, float cy, float radius, const Color& fill,
                            const Color& border, const std::string& label) {
    // yuvarlak görünüm: iki eş merkezli dikdörtgen yerine çokgen yaklaşımı
    // (gerçek daire çizimi arka uçta; burada sınırlayıcı kutu + etiket)
    rect(Rect{cx - radius, cy - radius, radius * 2, radius * 2}, border);
    rect(Rect{cx - radius + 2, cy - radius + 2, radius * 2 - 4, radius * 2 - 4}, fill);
    if (!label.empty())
        text(label, cx, cy - 6.f, 0.9f, kWhite, 1);
}

// ---------------- Hud ----------------
void Hud::resize(float widthPx, float heightPx, float dpiScale) {
    w_ = widthPx; h_ = heightPx; dpi_ = dpiScale;
    // sol alt joystick
    joyRadius_ = std::min(w_, h_) * 0.13f;
    joyCx_ = joyRadius_ * 1.5f;
    joyCy_ = h_ - joyRadius_ * 1.5f;
    joyKnobX_ = joyCx_; joyKnobY_ = joyCy_;

    // sağ yarı bakış alanı
    lookArea_ = Rect{w_ * 0.35f, 0, w_ * 0.65f, h_ * 0.75f};

    // sağ alt aksiyon butonları
    buttons_.clear();
    float br = std::min(w_, h_) * 0.075f;
    float bx = w_ - br * 1.8f;
    float by = h_ - br * 1.8f;
    buttons_.push_back(ActionButton{bx, by, br, "JUMP", false, -1});
    buttons_.push_back(ActionButton{bx - br * 2.3f, by, br, "BREAK", false, -1});
    buttons_.push_back(ActionButton{bx - br * 1.15f, by - br * 2.1f, br, "USE", false, -1});
}

void Hud::setJoystick(float cx, float cy, float radius) {
    joyCx_ = cx; joyCy_ = cy; joyRadius_ = radius;
}

void Hud::setLookArea(const Rect& r) { lookArea_ = r; }

void Hud::setActionButton(int index, float cx, float cy, float radius, const std::string& label) {
    if (index < 0) return;
    if (index >= static_cast<int>(buttons_.size())) buttons_.resize(index + 1);
    buttons_[index].cx = cx;
    buttons_[index].cy = cy;
    buttons_[index].radius = radius;
    buttons_[index].label = label;
}

void Hud::onTouchDown(int pointerId, float x, float y) {
    // joystick
    if (joyPointer_ < 0) {
        float dx = x - joyCx_, dy = y - joyCy_;
        if (std::sqrt(dx*dx + dy*dy) < joyRadius_ * 1.6f) {
            joyPointer_ = pointerId;
            joyKnobX_ = x; joyKnobY_ = y;
            float len = std::sqrt(dx*dx + dy*dy);
            float norm = len > joyRadius_ ? joyRadius_ / len : 1.f;
            touch.moveAxis.x = std::clamp(dx * norm / joyRadius_, -1.f, 1.f);
            touch.moveAxis.z = std::clamp(-dy * norm / joyRadius_, -1.f, 1.f);
            return;
        }
    }
    // aksiyon butonları
    for (auto& b : buttons_) {
        float dx = x - b.cx, dy = y - b.cy;
        if (dx*dx + dy*dy < b.radius * b.radius * 1.4f) {
            b.pressed = true;
            b.pointerId = pointerId;
            if (b.label == "JUMP") touch.jumpHeld = true;
            else if (b.label == "BREAK") { touch.breakHeld = true; touch.attackHeld = true; }
            else if (b.label == "USE") touch.useHeld = true;
            return;
        }
    }
    // bakış
    if (lookArea_.contains(x, y) && lookPointer_ < 0) {
        lookPointer_ = pointerId;
        lastLookX_ = x; lastLookY_ = y;
    }
}

void Hud::onTouchMove(int pointerId, float x, float y) {
    if (pointerId == joyPointer_) {
        float dx = x - joyCx_, dy = y - joyCy_;
        float len = std::sqrt(dx*dx + dy*dy);
        float maxLen = joyRadius_;
        float norm = len > maxLen ? maxLen / len : 1.f;
        joyKnobX_ = joyCx_ + dx * norm;
        joyKnobY_ = joyCy_ + dy * norm;
        touch.moveAxis.x = std::clamp(dx * norm / maxLen, -1.f, 1.f);
        touch.moveAxis.z = std::clamp(-dy * norm / maxLen, -1.f, 1.f);
        return;
    }
    if (pointerId == lookPointer_) {
        touch.lookDx += x - lastLookX_;
        touch.lookDy += y - lastLookY_;
        lastLookX_ = x; lastLookY_ = y;
    }
}

void Hud::onTouchUp(int pointerId) {
    if (pointerId == joyPointer_) {
        joyPointer_ = -1;
        joyKnobX_ = joyCx_; joyKnobY_ = joyCy_;
        touch.moveAxis = Vec3f{0, 0, 0};
    }
    if (pointerId == lookPointer_) lookPointer_ = -1;
    for (auto& b : buttons_) {
        if (b.pointerId == pointerId) {
            b.pressed = false;
            b.pointerId = -1;
            if (b.label == "JUMP") touch.jumpHeld = false;
            else if (b.label == "BREAK") { touch.breakHeld = false; touch.attackHeld = false; }
            else if (b.label == "USE") touch.useHeld = false;
        }
    }
}

void Hud::build(DrawList& out, const Inventory& inv, float health, float hunger,
                float fps, const Vec3f& pos, int pingMs, int peers,
                float hotbarFlashTimer) {
    (void)hunger;
    const float s = dpi_;

    // ---- nişangâh ----
    float cx = w_ * 0.5f, cy = h_ * 0.5f;
    out.rect(Rect{cx - 12*s, cy - 1*s, 24*s, 2*s}, Color{1,1,1,0.75f});
    out.rect(Rect{cx - 1*s, cy - 12*s, 2*s, 24*s}, Color{1,1,1,0.75f});

    // ---- can barı (10 kalp) ----
    float hs = 22.f * s;
    float hx = 24.f * s;
    float hy = h_ - 96.f * s;
    for (int i = 0; i < 10; ++i) {
        float fill = std::clamp((health - i * 2.f) / 2.f, 0.f, 1.f);
        Rect r{hx + i * (hs + 3*s), hy, hs, hs};
        out.rect(r, Color{0.15f, 0.05f, 0.05f, 0.85f});
        if (fill > 0.f) {
            out.rect(Rect{r.x + 2*s, r.y + 2*s, (r.w - 4*s) * fill, r.h - 4*s},
                     Color{0.85f, 0.18f, 0.24f, 1.f});
        }
    }

    // ---- hotbar (9 yuva) ----
    float slot = 56.f * s;
    float totalW = slot * 9 + 4 * 8 * s;
    float bx = (w_ - totalW) * 0.5f;
    float by = h_ - slot - 14.f * s;
    out.rect(Rect{bx - 4*s, by - 4*s, totalW + 8*s, slot + 8*s}, Color{0,0,0,0.45f});
    for (int i = 0; i < 9; ++i) {
        float x = bx + i * (slot + 4*s);
        const ItemStack& it = inv.at(i);
        bool sel = (i == inv.selectedIndex());
        // seçili yuvada flash
        float glow = sel ? (0.85f + 0.15f * std::sin(hotbarFlashTimer * 6.f)) : 0.f;
        out.rect(Rect{x, by, slot, slot},
                 sel ? Color{0.9f * glow, 0.9f * glow, 0.9f * glow, 0.9f}
                     : Color{0.2f, 0.2f, 0.24f, 0.8f});
        if (!it.empty()) {
            out.texturedRect(Rect{x + 4*s, by + 4*s, slot - 8*s, slot - 8*s},
                             static_cast<uint16_t>(it.itemId * 3));
            if (it.count > 1)
                out.text(fmtInt(it.count), x + slot - 4*s, by + slot - 20*s, 0.85f, kWhite, 2);
        }
    }

    // ---- FPS / ping / konum ----
    out.text(fmtInt(static_cast<long>(fps)) + " FPS", 12*s, 12*s, 0.9f,
             fps >= 55.f ? kAccent : (fps >= 40.f ? Color{0.95f,0.8f,0.2f,1} : kDanger));
    out.text(fmtInt(pingMs) + " ms", 12*s, 32*s, 0.85f,
             pingMs < 80 ? kAccent : (pingMs < 160 ? Color{0.95f,0.8f,0.2f,1} : kDanger));
    out.text(fmtInt(peers) + " oyuncu", 12*s, 52*s, 0.85f, Color{0.75f,0.8f,0.9f,1});
    out.text(fmtInt((long)pos.x) + " " + fmtInt((long)pos.y) + " " + fmtInt((long)pos.z),
             w_ - 12*s, 12*s, 0.8f, Color{0.75f,0.8f,0.9f,0.85f}, 2);

    // ---- joystick ----
    out.circleButton(joyCx_, joyCy_, joyRadius_, Color{1,1,1,0.10f}, Color{1,1,1,0.25f}, "");
    out.circleButton(joyKnobX_, joyKnobY_, joyRadius_ * 0.42f,
                     Color{1,1,1,0.30f}, Color{1,1,1,0.5f}, "");

    // ---- aksiyon butonları ----
    for (auto& b : buttons_) {
        Color fill = b.pressed ? Color{0.3f, 0.75f, 0.45f, 0.75f} : Color{1,1,1,0.18f};
        out.circleButton(b.cx, b.cy, b.radius, fill, Color{1,1,1,0.35f}, b.label);
    }
}

// ---------------- MenuSystem ----------------
void MenuSystem::open(MenuId id) {
    if (current_.id != MenuId::None) stack_.push_back(current_);
    current_ = MenuScreen{};
    current_.id = id;

    switch (id) {
        case MenuId::Main:
            current_.title = "CRAFTYA";
            current_.items = {
                {"Oyna", "hostsuz P2P", true, nullptr},
                {"Ayarlar", "", true, nullptr},
                {"Cikis", "", true, nullptr},
            };
            break;
        case MenuId::Play:
            current_.title = "OYNA";
            current_.items = {
                {"Tek Oyunculu", "dunya olustur", true, nullptr},
                {"Lobi", "NPC ile konus", true, nullptr},
                {"Geri", "", true, nullptr},
            };
            break;
        case MenuId::Settings:
            current_.title = "AYARLAR";
            current_.items = {
                {"Grafik", "golge acik / 1024", true, nullptr},
                {"Ses", "", true, nullptr},
                {"Kontrol", "", true, nullptr},
                {"Geri", "", true, nullptr},
            };
            break;
        case MenuId::Pause:
            current_.title = "DURAKLATILDI";
            current_.items = {
                {"Devam Et", "", true, nullptr},
                {"Ayarlar", "", true, nullptr},
                {"Ana Menu", "", true, nullptr},
            };
            break;
        case MenuId::Inventory:
            current_.title = "ENVANTER";
            break;
        case MenuId::Crafting:
            current_.title = "URETIM";
            break;
        default: break;
    }
    current_.selected = 0;
}

void MenuSystem::close() {
    if (!stack_.empty()) { current_ = stack_.back(); stack_.pop_back(); }
    else current_ = MenuScreen{};
}

void MenuSystem::moveSelection(int delta) {
    int n = static_cast<int>(current_.items.size());
    if (n == 0) return;
    current_.selected = (current_.selected + delta + n) % n;
}

void MenuSystem::activate() {
    if (current_.selected < 0 ||
        current_.selected >= static_cast<int>(current_.items.size())) return;
    auto& it = current_.items[current_.selected];
    if (it.enabled && it.action) it.action();
}

void MenuSystem::back() { close(); }

void MenuSystem::showModePanel(std::vector<MenuScreen::ModeRow> rows) {
    open(MenuId::ModeSelect);
    current_.title = "MOD SEC";
    current_.modes = std::move(rows);
    current_.items.clear();
    for (const auto& m : current_.modes)
        current_.items.push_back({m.name, m.subtitle + "  " +
                                  std::to_string(m.livePlayers) + " oyuncu", m.available, nullptr});
    current_.items.push_back({"Geri", "", true, nullptr});
}

void MenuSystem::build(DrawList& out, float screenW, float screenH) {
    if (current_.id == MenuId::None) return;
    itemRects_.clear();
    modeRects_.clear();

    // karartma
    out.rect(Rect{0, 0, screenW, screenH}, Color{0.02f, 0.03f, 0.05f, 0.82f});

    float pw = std::min(screenW * 0.86f, 760.f);
    float rowH = 84.f;
    int n = static_cast<int>(current_.items.size());
    float ph = 140.f + n * (rowH + 12.f) + 40.f;
    float px = (screenW - pw) * 0.5f;
    float py = (screenH - ph) * 0.5f;

    out.rect(Rect{px, py, pw, ph}, kPanel);
    out.rect(Rect{px, py, pw, 4.f}, kAccent);
    out.text(current_.title, px + pw * 0.5f, py + 36.f, 1.5f, kWhite, 1);

    for (int i = 0; i < n; ++i) {
        float ry = py + 120.f + i * (rowH + 12.f);
        Rect r{px + 28.f, ry, pw - 56.f, rowH};
        bool sel = (i == current_.selected);
        Color bg = sel ? Color{0.22f, 0.42f, 0.30f, 0.95f} : Color{0.16f, 0.17f, 0.21f, 0.9f};
        if (!current_.items[i].enabled) bg = Color{0.12f, 0.12f, 0.13f, 0.7f};
        out.rect(r, bg);
        if (sel) out.rect(Rect{r.x, r.y, 5.f, r.h}, kAccent);
        out.text(current_.items[i].label, r.x + 22.f, r.y + 18.f, 1.15f,
                 current_.items[i].enabled ? kWhite : Color{0.5f,0.5f,0.5f,1});
        if (!current_.items[i].sublabel.empty())
            out.text(current_.items[i].sublabel, r.x + 22.f, r.y + 50.f, 0.85f,
                     Color{0.68f, 0.75f, 0.7f, 1});
        itemRects_.push_back(r);
    }
    // dokunmatik IPC bu rect listesini kullanır
    (void)modeRects_;
}

void MenuSystem::onTap(float x, float y, float screenW, float screenH) {
    (void)screenW; (void)screenH;
    for (size_t i = 0; i < itemRects_.size(); ++i) {
        if (itemRects_[i].contains(x, y)) {
            current_.selected = static_cast<int>(i);
            activate();
            if (!current_.items[i].action) {
                // varsayılan davranış: geri = kapat
                if (current_.items[i].label == "Geri") close();
            }
            return;
        }
    }
}

void MenuSystem::onKey(int keyCode) {
    // 0=yukarı 1=aşağı 2=enter 3=esc
    switch (keyCode) {
        case 0: moveSelection(-1); break;
        case 1: moveSelection(1); break;
        case 2: activate(); break;
        case 3: back(); break;
        default: break;
    }
}

// ---------------- Toast ----------------
void Toast::show(const std::string& msg, float seconds) {
    active_.push_back({msg, seconds});
    if (active_.size() > 4) active_.erase(active_.begin());
    timer_ = seconds;
}

void Toast::update(float dt) {
    for (auto& t : active_) t.second -= dt;
    active_.erase(std::remove_if(active_.begin(), active_.end(),
                                 [](const std::pair<std::string,float>& p) { return p.second <= 0.f; }),
                  active_.end());
    timer_ = active_.empty() ? 0.f : active_.back().second;
}

void Toast::build(DrawList& out, float screenW, float screenH) {
    float y = screenH * 0.18f;
    for (auto& t : active_) {
        float w = 300.f + t.first.size() * 9.f;
        float x = (screenW - w) * 0.5f;
        out.rect(Rect{x, y, w, 52.f}, Color{0.08f, 0.10f, 0.14f, 0.92f});
        out.rect(Rect{x, y, 4.f, 52.f}, kAccent);
        out.text(t.first, x + 18.f, y + 16.f, 0.95f, kWhite);
        y += 62.f;
    }
}

// ---------------- ChatLog ----------------
void ChatLog::add(const std::string& author, const std::string& msg) {
    lines_.push_back(author + ": " + msg);
    if (lines_.size() > kMaxLines) lines_.erase(lines_.begin());
    sinceLast_ = 0.f;
}

void ChatLog::update(float dt) { sinceLast_ += dt; }

void ChatLog::build(DrawList& out, float screenW, float screenH, float fadeAfter) {
    if (lines_.empty()) return;
    float alpha = sinceLast_ > fadeAfter ? 0.f : 1.f;
    if (alpha <= 0.f) return;
    int show = std::min<int>(8, static_cast<int>(lines_.size()));
    float y = screenH * 0.30f;
    for (int i = static_cast<int>(lines_.size()) - show; i < static_cast<int>(lines_.size()); ++i) {
        const std::string& l = lines_[i];
        float w = 24.f + l.size() * 9.f;
        out.rect(Rect{16.f, y - 4.f, std::min(w, screenW * 0.6f), 26.f},
                 Color{0, 0, 0, 0.35f * alpha});
        out.text(l, 24.f, y, 0.85f, Color{1, 1, 1, alpha});
        y += 28.f;
    }
}

} // namespace craftya
