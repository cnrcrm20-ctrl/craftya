// Craftya Engine — GUI/HUD uygulaması
#include "craftya/gui.hpp"
#include <cmath>
#include <cstring>
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

void DrawList::circle(float cx, float cy, float radius, const Color& fill, float thickness) {
    // Yuvarlak görünüm: dolu daire = tek komut (arka uç gerçek disk çizer),
    // halka = kalınlık belirtilerek aynı komut.
    DrawItem d;
    d.cmd = DrawCmd::Circle;
    d.rect = Rect{cx - radius, cy - radius, radius * 2, radius * 2};
    d.radius = radius;
    d.thickness = thickness;
    d.color = fill;
    items_.push_back(d);
}

void DrawList::circleButton(float cx, float cy, float radius, const Color& fill,
                            const Color& border, const std::string& label) {
    circle(cx, cy, radius, fill, 0.f);
    circle(cx, cy, radius, border, std::max(2.f, radius * 0.06f));
    if (!label.empty()) {
        // etiket daireye SIĞMALI: genişlik = n * 6 * px <= 1.55 * radius
        // arka uç: gerçek piksel = 2 * scale; hücre genişliği 6 px
        float n = std::max(1.f, static_cast<float>(label.size()));
        float scale = radius / (8.f * n);          // 2*scale*6*n <= 1.5*radius
        scale = std::max(0.45f, std::min(scale, radius / 9.f));
        float textH = 7.f * 2.f * scale;
        text(label, cx, cy - textH * 0.5f, scale, kWhite, 1);
    }
}

// ---------------- Hud ----------------
void Hud::resize(float widthPx, float heightPx, float dpiScale) {
    w_ = widthPx; h_ = heightPx; dpi_ = dpiScale;

    // Ekran kısa kenarı = yerleşim birimi (telefon yatayda 854x480 gibi küçük)
    const float m = std::min(w_, h_);

    // --- hotbar: alt orta, ekranın %62 genişliği ---
    float slot = m * 0.088f;
    float barW = slot * 9 + m * 0.010f * 8;
    hotbarRect_ = Rect{(w_ - barW) * 0.5f, h_ - slot - m * 0.020f, barW, slot};

    // --- joystick: SOL ALT, hotbar'ın üstünde ---
    joyRadius_ = m * 0.115f;
    joyCx_ = m * 0.155f;
    joyCy_ = h_ - m * 0.235f;
    joyKnobX_ = joyCx_; joyKnobY_ = joyCy_;

    // --- bakış alanı: sağ %60, üst %80 (hotbar'ı kapsamaz) ---
    lookArea_ = Rect{w_ * 0.38f, 0, w_ * 0.62f, h_ - m * 0.24f};

    // --- aksiyon butonları: SAĞ ALT küme ---
    buttons_.clear();
    float br = m * 0.082f;
    float bx = w_ - m * 0.118f;
    float by = h_ - m * 0.120f;
    buttons_.push_back(ActionButton{bx, by, br, "ZIPLA", false, -1});
    buttons_.push_back(ActionButton{bx - br * 2.75f, by, br, "KIR", false, -1});
    buttons_.push_back(ActionButton{bx - br * 1.35f, by - br * 2.65f, br, "KOY", false, -1});

    // --- üst köşe düğmeleri: sağ üst (envanter/ayarlar/sohbet) ---
    float tr = m * 0.042f;
    float ty = m * 0.075f;
    topButtons_.clear();
    topButtons_.push_back(TopButton{w_ - m * 0.070f, ty, tr, "E", Hud::TopButton::Inventory, -1, false});
    topButtons_.push_back(TopButton{w_ - m * 0.070f - tr * 2.7f, ty, tr, "A", Hud::TopButton::Settings, -1, false});
    topButtons_.push_back(TopButton{w_ - m * 0.070f - tr * 5.4f, ty, tr, "S", Hud::TopButton::Chat, -1, false});
}

void Hud::setTopButtons(float cx, float cy, float radius) {
    float tr = radius;
    topButtons_.clear();
    topButtons_.push_back(TopButton{cx, cy, tr, "E", Hud::TopButton::Inventory, -1, false});
    topButtons_.push_back(TopButton{cx - tr * 2.7f, cy, tr, "A", Hud::TopButton::Settings, -1, false});
    topButtons_.push_back(TopButton{cx - tr * 5.4f, cy, tr, "S", Hud::TopButton::Chat, -1, false});
}

void Hud::setHotbarRects(const Rect& bar) { hotbarRect_ = bar; }

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
    // hotbar: dokunulan yuvayı seç
    if (hotbarRect_.contains(x, y)) {
        float slot = hotbarRect_.h;
        float rel = x - hotbarRect_.x;
        int idx = static_cast<int>(rel / (slot + (hotbarRect_.w - slot * 9.f) / 8.f));
        if (idx >= 0 && idx < 9) {
            touch.hotbarTapped = true;
            touch.hotbarIndex = idx;
        }
        hotbarPointer_ = pointerId;
        return;
    }

    if (joyPointer_ < 0) {
        float dx = x - joyCx_, dy = y - joyCy_;
        // ÇOK cömert yakalama: parmak joystick'in neresine basarsa bassın çalışsın
        // (ekranın sol alt çeyreği) — aksi halde ince bir halkaya basmak gerekiyordu.
        bool inLeftBottom = (x < w_ * 0.45f) && (y > h_ * 0.45f) && (y < hotbarRect_.y - 2.f);
        if (std::sqrt(dx*dx + dy*dy) < joyRadius_ * 1.9f || inLeftBottom) {
            joyPointer_ = pointerId;
            // MOBİL DAVRANIŞ: taban SABİT kalır (görsel joystick yerinde durur),
            // sadece parmağın merkeze göre sapması yönü belirler. Eskiden taban
            // parmağın altına taşınıyordu; bu durumda mutlak konuma göre yapılan
            // sürüklemede eksen 0 çıkıyor ve oyuncu hareket etmiyordu.
            if (inLeftBottom && !(std::sqrt(dx*dx + dy*dy) < joyRadius_ * 1.9f)) {
                // ekranın sol altına basıldı: tabanı parmağa getir (dinamik joystick)
                joyCx_ = x; joyCy_ = y;
                dx = dy = 0.f;
            }
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
            if (b.label == "ZIPLA") touch.jumpHeld = true;
            else if (b.label == "KIR") { touch.breakHeld = true; touch.attackHeld = true; }
            else if (b.label == "KOY") touch.useHeld = true;
            return;
        }
    }
    // üst köşe düğmeleri
    for (auto& b : topButtons_) {
        float dx = x - b.cx, dy = y - b.cy;
        if (dx*dx + dy*dy < b.radius * b.radius * 1.55f) {
            b.pressed = true;
            b.pointerId = pointerId;
            switch (b.kind) {
                case TopButton::Inventory: touch.inventoryTapped = true; break;
                case TopButton::Settings:  touch.settingsTapped = true;  break;
                case TopButton::Chat:      touch.chatTapped = true;      break;
            }
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
    if (pointerId == hotbarPointer_) hotbarPointer_ = -1;
    for (auto& b : topButtons_) {
        if (b.pointerId == pointerId) { b.pressed = false; b.pointerId = -1; }
    }
    for (auto& b : buttons_) {
        if (b.pointerId == pointerId) {
            b.pressed = false;
            b.pointerId = -1;
            if (b.label == "ZIPLA") touch.jumpHeld = false;
            else if (b.label == "KIR") { touch.breakHeld = false; touch.attackHeld = false; }
            else if (b.label == "KOY") touch.useHeld = false;
        }
    }
}

void Hud::build(DrawList& out, const Inventory& inv, float health, float hunger,
                float fps, const Vec3f& pos, int pingMs, int peers,
                float hotbarFlashTimer) {
    (void)hunger;
    const float m = std::min(w_, h_);
    const float s2 = m / 480.f;      // 480 px kısa kenar = 1.0 ölçek

    // ---- nişangâh ----
    float cx = w_ * 0.5f, cy = h_ * 0.5f;
    // Hedef varsa nişangâh kırmızıya döner (mobil savaş geri bildirimi).
    Color cross = touch.hasTarget ? Color{1.f, 0.35f, 0.28f, 0.95f}
                                  : Color{1, 1, 1, 0.8f};
    if (touch.hitFlash > 0.f) cross = Color{1.f, 0.85f, 0.3f, 1.f};
    out.rect(Rect{cx - 11*s2, cy - 1.f*s2, 22*s2, 2.f*s2}, cross);
    out.rect(Rect{cx - 1.f*s2, cy - 11*s2, 2.f*s2, 22*s2}, cross);

    // ---- hedef can barı: nişangâhın hemen üstünde ----
    if (touch.hasTarget) {
        float bw = 96.f * s2, bh = 7.f * s2;
        float bx = cx - bw * 0.5f, by = cy - 26.f * s2;
        out.rect(Rect{bx - 1.f, by - 1.f, bw + 2.f, bh + 2.f}, Color{0, 0, 0, 0.65f});
        out.rect(Rect{bx, by, bw, bh}, Color{0.25f, 0.09f, 0.09f, 0.9f});
        float f = std::clamp(touch.targetHealth01, 0.f, 1.f);
        Color hp = f > 0.5f ? Color{0.42f, 0.85f, 0.36f, 1}
                 : f > 0.25f ? Color{0.95f, 0.75f, 0.25f, 1}
                             : Color{0.92f, 0.28f, 0.24f, 1};
        out.rect(Rect{bx, by, bw * f, bh}, hp);
    }

    // ---- can + açlık barı: hotbar'ın HEMEN ÜSTÜNDE ----
    float heart = m * 0.034f;
    float gap = heart * 1.16f;
    float barY = hotbarRect_.y - heart * 1.75f;
    float barX = hotbarRect_.x + m * 0.002f;
    for (int i = 0; i < 10; ++i) {
        float fill = std::clamp((health - i * 2.f) / 2.f, 0.f, 1.f);
        Rect r{barX + i * gap, barY, heart, heart};
        out.rect(r, Color{0.07f, 0.02f, 0.03f, 0.85f});
        out.rect(Rect{r.x, r.y, r.w, 1.f}, Color{1, 1, 1, 0.18f});
        if (fill > 0.f) {
            // dolgu alttan yukarı (hasar aldıkça azalır)
            out.rect(Rect{r.x + 1.5f, r.y + 1.5f + (r.h - 3.f) * (1.f - fill),
                          r.w - 3.f, (r.h - 3.f) * fill},
                     Color{0.90f, 0.22f, 0.28f, 1.f});
        }
    }

    // ---- hotbar ----
    float slot = hotbarRect_.h;
    out.rect(Rect{hotbarRect_.x - 3.f, hotbarRect_.y - 3.f,
                  hotbarRect_.w + 6.f, slot + 6.f}, Color{0,0,0,0.42f});
    for (int i = 0; i < 9; ++i) {
        float x = hotbarRect_.x + i * (slot + m * 0.010f);
        const ItemStack& it = inv.at(i);
        bool sel = (i == inv.selectedIndex());
        float pulse = 0.5f + 0.5f * std::sin(hotbarFlashTimer * 5.f);
        Color bg = sel ? Color{0.92f, 0.90f, 0.55f, 0.95f}
                       : Color{0.16f, 0.17f, 0.21f, 0.80f};
        out.rect(Rect{x, hotbarRect_.y, slot, slot}, bg);
        if (sel)
            out.rect(Rect{x, hotbarRect_.y, slot, slot * 0.06f},
                     Color{1.f, 0.95f, 0.6f, 0.55f + 0.45f * pulse});
        if (!it.empty()) {
            out.texturedRect(Rect{x + slot * 0.14f, hotbarRect_.y + slot * 0.14f,
                                  slot * 0.72f, slot * 0.72f},
                             static_cast<uint16_t>(it.itemId * 3));
            if (it.count > 1)
                out.text(fmtInt(it.count), x + slot * 0.92f,
                         hotbarRect_.y + slot * 0.60f, 0.55f * s2 * 2.f, kWhite, 2);
        }
    }

    // ---- üst sol: FPS / ping / oyuncu ----
    float tx = m * 0.022f;
    float tyStep = m * 0.038f;
    out.rect(Rect{tx - 6.f, m * 0.020f, m * 0.20f, tyStep * 3.f}, Color{0, 0, 0, 0.22f});
    out.text(fmtInt(static_cast<long>(fps)) + " FPS", tx, m * 0.030f, 0.85f * s2,
             fps >= 55.f ? kAccent : (fps >= 40.f ? Color{0.95f,0.8f,0.2f,1} : kDanger));
    out.text(fmtInt(pingMs) + " ms", tx, m * 0.030f + tyStep, 0.75f * s2,
             pingMs < 80 ? kAccent : (pingMs < 160 ? Color{0.95f,0.8f,0.2f,1} : kDanger));
    out.text(fmtInt(peers) + " oyuncu", tx, m * 0.030f + tyStep * 2.f, 0.75f * s2,
             Color{0.78f,0.84f,0.92f,1});

    // ---- üst orta: konum (küçük) ----
    out.text(fmtInt((long)pos.x) + " / " + fmtInt((long)pos.y) + " / " + fmtInt((long)pos.z),
             w_ * 0.5f, m * 0.028f, 0.65f * s2, Color{0.9f,0.94f,1.f,0.7f}, 1);

    // ---- üst sağ: envanter / ayarlar / sohbet ----
    for (auto& b : topButtons_) {
        Color fill = b.pressed ? Color{0.32f, 0.80f, 0.48f, 0.9f} : Color{0.10f, 0.12f, 0.16f, 0.72f};
        out.circleButton(b.cx, b.cy, b.radius, fill, Color{1, 1, 1, 0.45f}, b.label);
    }

    // ---- joystick (sol alt) ----
    out.circle(joyCx_, joyCy_, joyRadius_, Color{1, 1, 1, 0.09f}, 0.f);
    out.circle(joyCx_, joyCy_, joyRadius_, Color{1, 1, 1, 0.30f}, joyRadius_ * 0.06f);
    out.circle(joyKnobX_, joyKnobY_, joyRadius_ * 0.40f, Color{1, 1, 1, 0.42f}, 0.f);
    out.circle(joyKnobX_, joyKnobY_, joyRadius_ * 0.40f, Color{1, 1, 1, 0.55f}, joyRadius_ * 0.035f);

    // ---- aksiyon butonları (sağ alt) ----
    for (auto& b : buttons_) {
        Color fill = b.pressed ? Color{0.30f, 0.78f, 0.45f, 0.85f} : Color{1, 1, 1, 0.16f};
        out.circleButton(b.cx, b.cy, b.radius, fill, Color{1, 1, 1, 0.38f}, b.label);
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
                {"Oyna", "dunyan olustur", true, nullptr},
                {"Ayarlar", "grafik / ses / kontrol", true, nullptr},
                {"Cikis", "", true, nullptr},
            };
            break;
        case MenuId::Play:
            current_.title = "OYNA";
            current_.items = {
                {"Tek Oyunculu", "sonsuz dunya", true, nullptr},
                {"Lobi", "oyun modlari", true, nullptr},
                {"Geri", "", true, nullptr},
            };
            break;
        case MenuId::Settings:
            current_.title = "AYARLAR";
            current_.items = {
                {"Grafik", "golus / mesafe / FPS", true, nullptr},
                {"Ses", "muzik / efekt", true, nullptr},
                {"Kontrol", "hassasiyet / ters Y", true, nullptr},
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
            current_.items.clear();
            break;
        case MenuId::Crafting:
            current_.title = "URETIM";
            current_.items.clear();
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
    current_.title = "MOD SE\xC3\x87";
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

    float pw = std::min(screenW * 0.94f, 760.f);
    float rowH = std::min(78.f, screenH * 0.13f);
    int n = static_cast<int>(current_.items.size());
    float ph;
    if (current_.id == MenuId::Inventory || current_.id == MenuId::Crafting) {
        // 9 sütun x 4 satır ızgara ekrana sığacak şekilde
        float availW = pw - 56.f;
        float slot = std::min((availW - 8.f * 8.f) / 9.f, screenH * 0.15f);
        ph = 130.f + 4.f * (slot + 8.f) + 36.f;
    } else {
        ph = 140.f + n * (rowH + 12.f) + 40.f;
    }
    ph = std::min(ph, screenH * 0.92f);
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
    // ---- envanter / üretim ızgarası ----
    if (current_.id == MenuId::Inventory && current_.inv) {
        const Inventory& inv = *current_.inv;
        int cols = 9;
        float availW = pw - 56.f;
        float gs = std::min((availW - 8.f * 8.f) / 9.f, screenH * 0.15f);
        float gw = cols * (gs + 8.f) - 8.f;
        float gx = px + (pw - gw) * 0.5f;
        float gy = py + 108.f;
        for (int i = 0; i < Inventory::TOTAL; ++i) {
            int c = i % cols;
            int row = (i < Inventory::HOTBAR) ? 3 : ((i - Inventory::HOTBAR) / cols);
            float x = gx + c * (gs + 8.f);
            float y = gy + row * (gs + 8.f);
            const ItemStack& it = inv.at(i);
            bool cursor = (i == current_.cursorSlot);
            out.rect(Rect{x, y, gs, gs},
                     cursor ? Color{0.30f, 0.48f, 0.34f, 0.95f}
                            : Color{0.17f, 0.18f, 0.22f, 0.92f});
            out.rect(Rect{x, y, gs, 2.f}, Color{1, 1, 1, cursor ? 0.55f : 0.16f});
            if (!it.empty()) {
                out.texturedRect(Rect{x + gs * 0.12f, y + gs * 0.12f,
                                      gs * 0.76f, gs * 0.76f},
                                 static_cast<uint16_t>(it.itemId * 3));
                if (it.count > 1)
                    out.text(std::to_string(it.count), x + gs * 0.94f,
                             y + gs * 0.66f, 0.45f, kWhite, 2);
            }
        }
    }
    if (current_.id == MenuId::Crafting) {
        float cs = std::min(84.f, screenH * 0.15f);
        float gx = px + 40.f;
        float gy = py + 108.f;
        out.text("uretim izgarasi", gx, py + 82.f, 0.8f, Color{0.7f, 0.75f, 0.8f, 1});
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                out.rect(Rect{gx + c * (cs + 6.f), gy + r * (cs + 6.f), cs, cs},
                         Color{0.17f, 0.18f, 0.22f, 0.92f});
        float ox = gx + 3 * (cs + 6.f) + 12.f;
        out.text("->", ox, gy + cs * 1.2f, 1.0f, kAccent);
        out.rect(Rect{ox + 62.f, gy + cs * 0.85f, cs, cs},
                 Color{0.22f, 0.30f, 0.26f, 0.95f});
    }

    // dokunmatik IPC bu rect listesini kullanır
    (void)modeRects_;
}

void MenuSystem::onTap(float x, float y, float screenW, float screenH) {
    (void)screenW; (void)screenH;
    for (size_t i = 0; i < itemRects_.size(); ++i) {
        if (!itemRects_[i].contains(x, y)) continue;

        current_.selected = static_cast<int>(i);
        if (i >= current_.items.size()) return;

        // ÖNEMLİ: aksiyon menüyü kapatıp current_ vektörünü boşaltabilir.
        // Eskiden aksiyondan SONRA current_.items[i] okunuyordu; bu,
        // "Tek Oyunculu"ya dokununca current_.items boşaldığı için
        // sınır dışı erişim → SEGFAULT (telefonda oyuna girerken çökme)
        // demekti. Artık gereken her şey aksiyondan ÖNCE kopyalanıyor.
        const bool hasAction = static_cast<bool>(current_.items[i].action);
        const std::string label = current_.items[i].label;

        if (hasAction) {
            // kopyayı çağır: lambda menüyü kapatmış olsa da güvenli
            auto act = current_.items[i].action;
            act();
        } else if (label == "Geri") {
            close();
        }
        return;
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

std::string ChatLog::submit() {
    if (!inputOpen_) return {};
    std::string out = input_;
    if (!out.empty()) {
        lines_.push_back("Sen: " + out);
        if (lines_.size() > kMaxLines) lines_.erase(lines_.begin());
        sinceLast_ = 0.f;
    }
    inputOpen_ = false;
    input_.clear();
    return out;
}

void ChatLog::buildKeyboard(DrawList& out, float screenW, float screenH) {
    if (!inputOpen_) return;
    float kbH = screenH * 0.40f;
    float kbY = screenH - kbH;
    out.rect(Rect{0, kbY, screenW, kbH}, Color{0.06f, 0.07f, 0.10f, 0.96f});
    out.rect(Rect{0, kbY, screenW, 3.f}, Color{0.24f, 0.78f, 0.44f, 1.f});

    // yazı alanı + GÖNDER düğmesi
    float fieldW = screenW * 0.70f;
    out.rect(Rect{screenW * 0.04f, kbY + 10.f, fieldW, 44.f},
             Color{0.14f, 0.15f, 0.19f, 1.f});
    out.text(input_.empty() ? std::string("mesaj yaz...") : input_,
             screenW * 0.04f + 12.f, kbY + 26.f, 0.85f,
             input_.empty() ? Color{0.5f, 0.55f, 0.6f, 1} : Color{1, 1, 1, 1});
    float sendX = screenW * 0.04f + fieldW + 10.f;
    float sendW = screenW * 0.92f - fieldW - 10.f;
    out.rect(Rect{sendX, kbY + 10.f, sendW, 44.f}, Color{0.24f, 0.78f, 0.44f, 1.f});
    out.text("GONDER", sendX + sendW * 0.5f, kbY + 26.f, 0.75f, Color{0.02f,0.06f,0.03f,1}, 1);

    // tuş takımı: 4 satır
    const char* rows[4] = {
        "1234567890",
        "qwertyuiop",
        "asdfghjklg",
        "zxcvbnm,.?"
    };
    // 4 satır + en altta boşluk/sil/gönder satırı
    float availH = kbH - 76.f;
    float keyW = screenW * 0.075f;
    float keyH = (availH - 4.f * 5.f) / 5.f;
    keyH = std::min(keyH, keyW * 1.15f);
    for (int r = 0; r < 4; ++r) {
        int n = static_cast<int>(std::strlen(rows[r]));
        float rowW = n * (keyW + 4.f);
        float sx = (screenW - rowW) * 0.5f;
        for (int i = 0; i < n; ++i) {
            float x = sx + i * (keyW + 4.f);
            float y = kbY + 64.f + r * (keyH + 4.f);
            bool del = (r == 2 && i == n - 1);     // 'g' tuşu = SİL
            out.rect(Rect{x, y, keyW, keyH},
                     del ? Color{0.60f, 0.24f, 0.24f, 1.f} : Color{0.22f, 0.24f, 0.30f, 1.f});
            out.text(del ? std::string("SIL") : std::string(1, rows[r][i]),
                     x + keyW * 0.5f, y + keyH * 0.34f, del ? 0.55f : 0.80f,
                     Color{1, 1, 1, 1}, 1);
        }
    }
    // en alt satır: boşluk + kapat
    float sy = kbY + 64.f + 4.f * (keyH + 4.f);
    float spaceW = screenW * 0.46f;
    out.rect(Rect{(screenW - spaceW) * 0.5f, sy, spaceW, keyH},
             Color{0.26f, 0.28f, 0.34f, 1.f});
    out.text("BOSLUK", screenW * 0.5f, sy + keyH * 0.34f, 0.7f, Color{0.8f,0.85f,0.9f,1}, 1);
}

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
