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
// ---- modern palet (2026 mobil oyun estetiği) ----
// Koyu cam panel + canlı yeşil vurgu + yumuşak beyaz metin.
const Color kPanel      = Color{0.07f, 0.09f, 0.13f, 0.90f};   // cam panel
const Color kPanelTop   = Color{0.13f, 0.16f, 0.22f, 0.96f};   // gradyan üstü
const Color kPanelBot   = Color{0.07f, 0.09f, 0.13f, 0.96f};   // gradyan altı
const Color kCard       = Color{0.16f, 0.19f, 0.26f, 0.92f};   // kart yüzeyi
const Color kCardHi     = Color{0.23f, 0.28f, 0.37f, 0.96f};   // kart (seçili)
const Color kAccent     = Color{0.24f, 0.82f, 0.47f, 1.f};     // ana vurgu
const Color kAccentDim  = Color{0.13f, 0.48f, 0.29f, 1.f};
const Color kTextDim    = Color{0.62f, 0.68f, 0.76f, 1.f};
const Color kStroke     = Color{1.f, 1.f, 1.f, 0.10f};         // ince kenar
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

void DrawList::roundRect(const Rect& r, const Color& c, float corner) {
    DrawItem d;
    d.cmd = DrawCmd::Rect;
    d.rect = r;
    d.color = c;
    d.corner = corner;
    items_.push_back(d);
}

void DrawList::gradient(const Rect& r, const Color& top, const Color& bottom, float corner) {
    DrawItem d;
    d.cmd = DrawCmd::Rect;
    d.rect = r;
    d.color = top;
    d.color2 = bottom;
    d.hasColor2 = true;
    d.corner = corner;
    items_.push_back(d);
}

void DrawList::shadow(const Rect& r, float blur, const Color& c, float corner) {
    // Katmanlı halkalar: yarı saydam kutular büyüyerek alttan taşar.
    // Gerçek bulanıklık (gaussian) yerine 4 katman — mobilde ucuz ve
    // gözle ayırt edilemeyecek kadar yumuşak.
    const int layers = 4;
    for (int i = layers; i >= 1; --i) {
        float t = static_cast<float>(i) / layers;      // 1.0 → 0.25
        float grow = blur * t;
        Color cc = c;
        cc.a *= (0.30f / layers) * (1.2f - t * 0.5f);
        DrawItem d;
        d.cmd = DrawCmd::Rect;
        d.rect = Rect{r.x - grow, r.y - grow + blur * 0.28f,
                      r.w + grow * 2.f, r.h + grow * 2.f};
        d.color = cc;
        d.corner = corner > 0.f ? corner + grow : 0.f;
        items_.push_back(d);
    }
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

    // --- hotbar: alt orta ---
    // KOYU KENAR KORUMASI: telefonda (900x420) eski 0.088*kenar ölçekli yuva
    // + büyük iç boşluk hotbar'ı ekranın altına taşırıyordu. Yuva boyu
    // kısa kenara göre ölçeklenip ekran genişliğine göre sınırlanıyor.
    float slot = std::min(m * 0.088f, w_ * 0.072f);
    float barW = slot * 9 + m * 0.010f * 8;
    hotbarRect_ = Rect{(w_ - barW) * 0.5f,
                       std::min(h_ - slot - m * 0.020f, h_ - slot - m * 0.012f),
                       barW, slot};

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
        float bw = 108.f * s2, bh = 8.f * s2;
        float bx = cx - bw * 0.5f, by = cy - 28.f * s2;
        float brad = bh * 0.5f;
        out.roundRect(Rect{bx - 3.f, by - 3.f, bw + 6.f, bh + 6.f},
                      Color{0.f, 0.f, 0.f, 0.42f}, brad + 3.f);
        out.roundRect(Rect{bx, by, bw, bh}, Color{0.16f, 0.07f, 0.10f, 0.92f}, brad);
        float f = std::clamp(touch.targetHealth01, 0.f, 1.f);
        Color hp = f > 0.5f ? Color{0.42f, 0.85f, 0.36f, 1}
                 : f > 0.25f ? Color{0.95f, 0.75f, 0.25f, 1}
                             : Color{0.92f, 0.28f, 0.24f, 1};
        if (f > 0.01f) {
            Color hpTop{hp.r * 1.25f, hp.g * 1.25f, hp.b * 1.25f, 1.f};
            out.gradient(Rect{bx, by, bw * f, bh}, hpTop, hp, brad);
        }
        out.roundRect(Rect{bx, by, bw, 1.f}, Color{1, 1, 1, 0.14f}, 0.5f);
    }

    // ---- can + açlık barı: hotbar'ın HEMEN ÜSTÜNDE ----
    // Eski 10 ayrı kare kutucuk "BIOS göstergesi" gibi duruyordu.
    // Artık iki yuvarlatılmış, gradyanlı bar + parlak iç kenar.
    {
        const float bh = m * 0.020f;
        const float bw = std::min(m * 0.30f, hotbarRect_.w * 0.52f);
        const float by2 = hotbarRect_.y - bh * 2.f - m * 0.052f;
        const float bx2 = hotbarRect_.x + m * 0.002f;
        const float rad = bh * 0.5f;
        auto bar = [&](float x, float y, float w, float v01,
                       Color a, Color b) {
            out.roundRect(Rect{x - 2.f, y - 2.f, w + 4.f, bh + 4.f},
                          Color{0.f, 0.f, 0.f, 0.38f}, rad + 2.f);
            out.roundRect(Rect{x, y, w, bh}, Color{0.12f, 0.13f, 0.17f, 0.85f}, rad);
            float f = std::clamp(v01, 0.f, 1.f);
            if (f > 0.005f) out.gradient(Rect{x, y, w * f, bh}, a, b, rad);
            // üstteki ışık çizgisi: cam/plastik his
            out.roundRect(Rect{x + rad * 0.5f, y + 1.f, w - rad, 1.2f},
                          Color{1.f, 1.f, 1.f, 0.16f}, 0.6f);
        };
        bar(bx2, by2, bw, health / 20.f,
            Color{1.f, 0.42f, 0.40f, 1.f}, Color{0.86f, 0.16f, 0.26f, 1.f});
        bar(bx2, by2 + bh + m * 0.020f, bw * 0.70f, hunger / 20.f,
            Color{1.f, 0.82f, 0.42f, 1.f}, Color{0.90f, 0.52f, 0.14f, 1.f});
    }

    // ---- hotbar ----
    float slot = hotbarRect_.h;
    float slotRad = slot * 0.26f;
    float slotStep = slot + m * 0.010f;
    float barW = slotStep * 8.f + slot;   // 9 yuva, arada boşluk
    // cam panel
    out.roundRect(Rect{hotbarRect_.x - m * 0.008f, hotbarRect_.y - m * 0.008f,
                       barW + m * 0.016f, slot + m * 0.016f},
                  Color{0.08f, 0.09f, 0.12f, 0.46f}, slotRad + m * 0.008f);
    for (int i = 0; i < 9; ++i) {
        float x = hotbarRect_.x + i * slotStep;
        const ItemStack& it = inv.at(i);
        bool sel = (i == inv.selectedIndex());
        float pulse = 0.5f + 0.5f * std::sin(hotbarFlashTimer * 5.f);
        if (sel) {
            // seçili slot: yumuşak hâle + gradyan çerçeve
            out.roundRect(Rect{x - 3.f, hotbarRect_.y - 3.f, slot + 6.f, slot + 6.f},
                          Color{0.30f, 0.85f, 0.55f, 0.17f}, slotRad + 3.f);
            out.gradient(Rect{x - 1.5f, hotbarRect_.y - 1.5f, slot + 3.f, slot + 3.f},
                         Color{0.96f, 0.99f, 1.f, 0.95f},
                         Color{0.45f, 0.86f, 0.66f, 0.95f}, slotRad + 1.5f);
            out.roundRect(Rect{x, hotbarRect_.y, slot, slot},
                          Color{0.20f, 0.30f, 0.28f, 0.92f}, slotRad);
        } else {
            out.roundRect(Rect{x, hotbarRect_.y, slot, slot},
                          Color{0.15f, 0.17f, 0.21f, 0.72f}, slotRad);
            out.roundRect(Rect{x, hotbarRect_.y, slot, 1.f},
                          Color{1.f, 1.f, 1.f, 0.10f}, 0.5f);
        }
        if (!it.empty()) {
            out.texturedRect(Rect{x + slot * 0.16f, hotbarRect_.y + slot * 0.16f,
                                  slot * 0.68f, slot * 0.68f},
                             static_cast<uint16_t>(it.itemId * 3));
            if (it.count > 1)
                out.text(fmtInt(it.count), x + slot * 0.92f,
                         hotbarRect_.y + slot * 0.60f, 0.55f * s2 * 2.f, kWhite, 2);
        }
        if (sel) {
            // alt gösterge noktası (nabız)
            out.roundRect(Rect{x + slot * 0.32f, hotbarRect_.y + slot + 4.f,
                               slot * 0.36f, 2.5f},
                          Color{0.45f + 0.4f * pulse, 0.95f, 0.70f, 0.9f}, 1.2f);
        }
    }

    // ---- üst sol: FPS / ping / oyuncu ----
    float tx = m * 0.022f;
    float tyStep = m * 0.038f;
    out.roundRect(Rect{tx - m * 0.012f, m * 0.016f, m * 0.166f, tyStep * 3.f},
                  Color{0.07f, 0.08f, 0.11f, 0.34f}, m * 0.016f);
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
        // yumuşak hâle → cam daire → ince parlak halka
        out.circle(b.cx, b.cy, b.radius * 1.22f, Color{0.35f, 0.85f, 0.60f, 0.10f}, 0.f);
        Color fill = b.pressed ? Color{0.36f, 0.86f, 0.56f, 0.88f}
                               : Color{0.09f, 0.11f, 0.15f, 0.66f};
        out.circleButton(b.cx, b.cy, b.radius, fill, Color{1.f, 1.f, 1.f, 0.26f}, b.label);
    }

    // ---- joystick (sol alt) ----
    out.circle(joyCx_, joyCy_, joyRadius_ * 1.04f, Color{0.f, 0.f, 0.f, 0.10f}, 0.f);
    out.circle(joyCx_, joyCy_, joyRadius_, Color{1.f, 1.f, 1.f, 0.07f}, 0.f);
    out.circle(joyCx_, joyCy_, joyRadius_, Color{1.f, 1.f, 1.f, 0.22f}, joyRadius_ * 0.045f);
    out.circle(joyKnobX_, joyKnobY_, joyRadius_ * 0.44f, Color{0.10f, 0.13f, 0.17f, 0.34f}, 0.f);
    out.circle(joyKnobX_, joyKnobY_, joyRadius_ * 0.42f, Color{1.f, 1.f, 1.f, 0.34f}, 0.f);
    out.circle(joyKnobX_, joyKnobY_, joyRadius_ * 0.30f, Color{1.f, 1.f, 1.f, 0.20f}, 0.f);
    out.circle(joyKnobX_, joyKnobY_, joyRadius_ * 0.42f, Color{1.f, 1.f, 1.f, 0.55f}, joyRadius_ * 0.035f);

    // ---- aksiyon butonları (sağ alt) ----
    for (auto& b : buttons_) {
        out.circle(b.cx, b.cy, b.radius * 1.18f, Color{0.f, 0.f, 0.f, 0.12f}, 0.f);
        Color fill = b.pressed ? Color{0.34f, 0.84f, 0.55f, 0.82f}
                               : Color{1.f, 1.f, 1.f, 0.13f};
        out.circleButton(b.cx, b.cy, b.radius, fill, Color{1.f, 1.f, 1.f, 0.34f}, b.label);
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

    // ================= MODERN MENÜ =================
    // Eski görünüm: düz siyah dikdörtgen + kalın yeşil şerit (BIOS gibi).
    // Yeni: bulanık arka plan + gölgeli cam panel + yuvarlatılmış
    // gradyan kartlar + yumuşak tipografi.
    const float m = std::min(screenW, screenH);
    const float rad = m * 0.045f;          // panel köşe yarıçapı
    const float cardRad = m * 0.030f;      // kart köşe yarıçapı

    // arka plan: çok katmanlı dikey gradyan (hafif "blur" hissi)
    out.gradient(Rect{0, 0, screenW, screenH * 0.5f},
                 Color{0.03f, 0.05f, 0.09f, 0.90f},
                 Color{0.05f, 0.07f, 0.12f, 0.86f});
    out.gradient(Rect{0, screenH * 0.5f, screenW, screenH * 0.5f},
                 Color{0.05f, 0.07f, 0.12f, 0.86f},
                 Color{0.02f, 0.03f, 0.06f, 0.94f});

    float pw = std::min(screenW * 0.92f, 720.f);
    float rowH = std::min(76.f, screenH * 0.125f);
    int n = static_cast<int>(current_.items.size());
    float ph;
    if (current_.id == MenuId::Inventory || current_.id == MenuId::Crafting) {
        // Izgara panelin DIŞINA taşmamalı: yuva boyu hem genişliğe hem
        // kalan yüksekliğe göre sınırlanıyor (telefonda 4 satır kesiliyordu).
        const float availW = pw - 56.f;
        float slot = std::min((availW - 8.f * 8.f) / 9.f, screenH * 0.15f);
        const float maxPh = screenH * 0.92f;
        const float slotByH = (maxPh - 108.f - 40.f) / 4.f - 8.f;
        slot = std::max(20.f, std::min(slot, slotByH));
        ph = 108.f + 4.f * (slot + 8.f) + 40.f;
    } else {
        ph = 140.f + n * (rowH + 12.f) + 40.f;
    }
    ph = std::min(ph, screenH * 0.92f);
    float px = (screenW - pw) * 0.5f;
    float py = (screenH - ph) * 0.5f;

    // panel gölgesi + cam yüzey
    out.shadow(Rect{px, py, pw, ph}, m * 0.030f, Color{0, 0, 0, 0.85f}, rad);
    out.gradient(Rect{px, py, pw, ph}, kPanelTop, kPanelBot, rad);
    // üst kenarda ince ışık çizgisi (cam hissi)
    out.gradient(Rect{px + rad, py, pw - rad * 2.f, 2.f},
                 Color{1, 1, 1, 0.16f}, Color{1, 1, 1, 0.0f});

    // başlık: büyük, harf aralıklı, ortalanmış
    out.text(current_.title, px + pw * 0.5f, py + 34.f, 1.7f, kWhite, 1);
    // başlık altı vurgu çizgisi (kısa, ortada)
    float ulW = std::min(pw * 0.16f, 120.f);
    out.gradient(Rect{px + (pw - ulW) * 0.5f, py + 34.f + 46.f, ulW, 3.f},
                 kAccent, Color{kAccent.r, kAccent.g, kAccent.b, 0.15f}, 1.5f);

    for (int i = 0; i < n; ++i) {
        float ry = py + 120.f + i * (rowH + 12.f);
        Rect r{px + 24.f, ry, pw - 48.f, rowH};
        bool sel = (i == current_.selected);

        Color top = kCard, bot = Color{kCard.r * 0.78f, kCard.g * 0.78f, kCard.b * 0.82f, kCard.a};
        if (sel) {
            top = kCardHi;
            bot = Color{kCard.r * 0.95f, kCard.g * 0.95f, kCard.b * 0.95f, kCard.a};
        }
        if (!current_.items[i].enabled) {
            top = Color{0.12f, 0.13f, 0.16f, 0.55f};
            bot = Color{0.10f, 0.11f, 0.14f, 0.55f};
        }

        // kart gölgesi + yuvarlatılmış gradyan yüzey
        out.shadow(r, m * 0.012f, Color{0, 0, 0, 0.55f}, cardRad);
        out.gradient(r, top, bot, cardRad);
        // seçili kartta sol tarafta canlı vurgu hapı
        if (sel) {
            float hh = r.h * 0.44f;
            out.gradient(Rect{r.x + 6.f, r.y + (r.h - hh) * 0.5f, 5.f, hh},
                         kAccent, kAccentDim, 2.5f);
        }
        // ince iç kenar (cam çerçeve)
        out.roundRect(Rect{r.x, r.y, r.w, 1.5f}, kStroke, 0.75f);

        // Dikey hizalama: metin bloğu kartın TAM ortasına oturur.
        // Gerçek glif yüksekliği = 14 * scale (cihazdaki metrikle aynı);
        // önceden 22 varsayıldığı için etiket 8 px yukarı kayıyordu.
        const bool hasSub = !current_.items[i].sublabel.empty();
        const float labelScale = 1.15f;
        const float labelH = 14.f * labelScale;
        const float subScale = 0.80f;
        const float subH = 14.f * subScale;
        const float gap = hasSub ? 8.f : 0.f;
        const float blockH = labelH + gap + (hasSub ? subH : 0.f);
        const float ty = r.y + (r.h - blockH) * 0.5f;
        out.text(current_.items[i].label, r.x + 26.f, ty, labelScale,
                 current_.items[i].enabled ? kWhite : Color{0.45f, 0.48f, 0.54f, 1});
        if (hasSub)
            out.text(current_.items[i].sublabel, r.x + 26.f, ty + labelH + gap,
                     subScale, kTextDim);
        itemRects_.push_back(r);
    }
    // ---- envanter / üretim ızgarası ----
    if (current_.id == MenuId::Inventory && current_.inv) {
        const Inventory& inv = *current_.inv;
        int cols = 9;
        float availW = pw - 56.f;
        float gs = std::min((availW - 8.f * 8.f) / 9.f, screenH * 0.15f);
        gs = std::max(20.f, std::min(gs, (screenH * 0.92f - 148.f) / 4.f - 8.f));
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
            const float slotR = gs * 0.22f;
            if (cursor) {
                out.roundRect(Rect{x - 2.5f, y - 2.5f, gs + 5.f, gs + 5.f},
                              Color{0.32f, 0.86f, 0.58f, 0.22f}, slotR + 2.5f);
                out.gradient(Rect{x - 1.5f, y - 1.5f, gs + 3.f, gs + 3.f},
                             Color{0.96f, 0.99f, 1.f, 0.92f},
                             Color{0.42f, 0.84f, 0.64f, 0.92f}, slotR + 1.5f);
            }
            out.roundRect(Rect{x, y, gs, gs},
                          cursor ? Color{0.24f, 0.34f, 0.31f, 0.92f}
                                 : Color{0.15f, 0.16f, 0.20f, 0.90f}, slotR);
            out.roundRect(Rect{x + 1.f, y + 1.f, gs - 2.f, 1.f},
                          Color{1.f, 1.f, 1.f, cursor ? 0.30f : 0.09f}, 0.5f);
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
            for (int c = 0; c < 3; ++c) {
                Rect cr{gx + c * (cs + 6.f), gy + r * (cs + 6.f), cs, cs};
                out.roundRect(Rect{cr.x - 2.f, cr.y - 2.f, cr.w + 4.f, cr.h + 4.f},
                              Color{0.f, 0.f, 0.f, 0.30f}, cs * 0.24f);
                out.roundRect(cr, Color{0.15f, 0.16f, 0.20f, 0.92f}, cs * 0.22f);
                out.roundRect(Rect{cr.x + 1.f, cr.y + 1.f, cr.w - 2.f, 1.f},
                              Color{1.f, 1.f, 1.f, 0.09f}, 0.5f);
            }
        float ox = gx + 3 * (cs + 6.f) + 12.f;
        out.text("->", ox, gy + cs * 1.2f, 1.0f, kAccent);
        out.roundRect(Rect{ox + 62.f, gy + cs * 0.85f, cs, cs},
                      Color{0.24f, 0.34f, 0.29f, 0.95f}, cs * 0.22f);
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
    const float kbH = screenH * 0.46f;
    const float kbY = screenH - kbH;
    const float rad = std::min(screenW, screenH) * 0.035f;

    // cam panel (üstü yuvarlak) + yumuşak geçiş
    out.gradient(Rect{0.f, kbY - 26.f, screenW, 26.f},
                 Color{0.05f, 0.06f, 0.09f, 0.f}, Color{0.05f, 0.06f, 0.09f, 0.88f});
    // OPAK panel: yarı saydamken alttaki HUD düğmeleri tuşların arasından
    // görünüyordu (klavye açıkken HUD gizlenmeli).
    out.rect(Rect{0.f, kbY, screenW, kbH + rad}, Color{0.05f, 0.06f, 0.09f, 1.f});
    out.roundRect(Rect{0.f, kbY, screenW, kbH + rad}, Color{0.05f, 0.06f, 0.09f, 1.f}, rad);
    // üst kenar ışığı
    out.gradient(Rect{rad, kbY, screenW - rad * 2.f, 2.f},
                 Color{0.32f, 0.86f, 0.58f, 0.9f}, Color{0.32f, 0.86f, 0.58f, 0.15f});

    // yazı alanı + GÖNDER düğmesi
    const float fieldX = screenW * 0.04f;
    const float fieldW = screenW * 0.68f;
    const float fieldH = 46.f;
    const float fieldY = kbY + 14.f;
    out.roundRect(Rect{fieldX - 2.f, fieldY - 2.f, fieldW + 4.f, fieldH + 4.f},
                  Color{0.f, 0.f, 0.f, 0.35f}, (fieldH + 4.f) * 0.5f);
    out.roundRect(Rect{fieldX, fieldY, fieldW, fieldH},
                  Color{0.13f, 0.15f, 0.19f, 0.98f}, fieldH * 0.5f);
    out.roundRect(Rect{fieldX + 1.f, fieldY + 1.f, fieldW - 2.f, 1.2f},
                  Color{1.f, 1.f, 1.f, 0.10f}, 0.6f);
    out.text(input_.empty() ? std::string("mesaj yaz...") : input_,
             fieldX + 20.f, fieldY + fieldH * 0.5f - 9.f, 0.85f,
             input_.empty() ? Color{0.46f, 0.52f, 0.60f, 1} : Color{1, 1, 1, 1});

    const float sendX = fieldX + fieldW + 12.f;
    const float sendW = screenW * 0.92f - fieldW - 12.f;
    out.roundRect(Rect{sendX, fieldY, sendW, fieldH},
                  Color{0.26f, 0.86f, 0.55f, 1.f}, fieldH * 0.5f);
    out.gradient(Rect{sendX + 2.f, fieldY + 1.5f, sendW - 4.f, fieldH * 0.5f},
                 Color{1.f, 1.f, 1.f, 0.22f}, Color{1.f, 1.f, 1.f, 0.02f},
                 fieldH * 0.25f);
    out.text("GONDER", sendX + sendW * 0.5f, fieldY + fieldH * 0.5f - 8.f,
             0.72f, Color{0.02f, 0.07f, 0.04f, 1}, 1);

    // tuş takımı: 4 satır
    const char* rows[4] = {
        "1234567890",
        "qwertyuiop",
        "asdfghjklg",
        "zxcvbnm,.?"
    };
    // 4 harf satırı + boşluk satırı = 5 satır; boşluk satırı da sığmalı
    const float availH = kbH - 78.f;
    float keyW = screenW * 0.076f;
    float keyH = std::min((availH - 5.f * 6.f) / 5.f, keyW * 1.10f);
    const float keyRad = std::min(keyW, keyH) * 0.24f;
    const float gap = std::max(3.f, keyW * 0.09f);

    for (int r = 0; r < 4; ++r) {
        int n = static_cast<int>(std::strlen(rows[r]));
        float rowW = n * (keyW + gap) - gap;
        float sx = (screenW - rowW) * 0.5f;
        for (int i = 0; i < n; ++i) {
            float x = sx + i * (keyW + gap);
            float y = fieldY + fieldH + 14.f + r * (keyH + 6.f);
            bool del = (r == 2 && i == n - 1);     // 'g' tuşu = SİL
            if (del) {
                out.gradient(Rect{x, y, keyW, keyH},
                             Color{0.86f, 0.36f, 0.34f, 1.f},
                             Color{0.62f, 0.16f, 0.20f, 1.f}, keyRad);
            } else {
                out.roundRect(Rect{x, y + 1.5f, keyW, keyH}, Color{0.f, 0.f, 0.f, 0.42f}, keyRad);
                out.gradient(Rect{x, y, keyW, keyH},
                             Color{0.28f, 0.31f, 0.38f, 0.96f},
                             Color{0.17f, 0.19f, 0.24f, 0.96f}, keyRad);
                out.roundRect(Rect{x + keyRad * 0.5f, y + 1.f, keyW - keyRad, 1.f},
                              Color{1.f, 1.f, 1.f, 0.16f}, 0.5f);
            }
            out.text(del ? std::string("SIL") : std::string(1, rows[r][i]),
                     x + keyW * 0.5f, y + keyH * 0.5f - (del ? 7.f : 8.f),
                     del ? 0.55f : 0.80f, Color{1, 1, 1, 1}, 1);
        }
    }
    // en alt satır: boşluk
    float sy = fieldY + fieldH + 14.f + 4.f * (keyH + 6.f);
    if (sy + keyH > screenH - 4.f) sy = screenH - 4.f - keyH;
    float spaceW = screenW * 0.50f;
    out.roundRect(Rect{(screenW - spaceW) * 0.5f, sy + 1.5f, spaceW, keyH},
                  Color{0.f, 0.f, 0.f, 0.40f}, keyRad);
    out.gradient(Rect{(screenW - spaceW) * 0.5f, sy, spaceW, keyH},
                 Color{0.30f, 0.33f, 0.40f, 0.96f},
                 Color{0.18f, 0.20f, 0.25f, 0.96f}, keyRad);
    out.text("BOSLUK", screenW * 0.5f, sy + keyH * 0.5f - 8.f, 0.68f,
             Color{0.82f, 0.87f, 0.93f, 1}, 1);
}

void ChatLog::build(DrawList& out, float screenW, float screenH, float fadeAfter) {
    if (lines_.empty()) return;
    float alpha = sinceLast_ > fadeAfter ? 0.f : 1.f;
    if (alpha <= 0.f) return;
    int show = std::min<int>(8, static_cast<int>(lines_.size()));
    float y = screenH * 0.30f;
    for (int i = static_cast<int>(lines_.size()) - show; i < static_cast<int>(lines_.size()); ++i) {
        const std::string& l = lines_[i];
        float w = std::min(28.f + l.size() * 9.f, screenW * 0.62f);
        // cam balon + sol renk şeridi
        out.roundRect(Rect{14.f, y - 6.f, w, 28.f}, Color{0.05f, 0.07f, 0.10f, 0.46f * alpha}, 14.f);
        out.roundRect(Rect{14.f, y - 6.f, 3.f, 28.f},
                      Color{kAccent.r, kAccent.g, kAccent.b, 0.85f * alpha}, 1.5f);
        out.text(l, 26.f, y, 0.85f, Color{1, 1, 1, alpha});
        y += 32.f;
    }
}

} // namespace craftya
