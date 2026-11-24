// Craftya Engine — GUI/HUD: dokunmatik yerleşim, çizim listesi, menü sistemi
// Renderer'dan bağımsız: sadece "ne çizilecek" listesi üretir (draw list).
#pragma once
#include "craftya/core.hpp"
#include "craftya/inventory.hpp"
#include <vector>
#include <string>
#include <functional>

namespace craftya {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    static Color rgb(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) {
        return Color{r_ / 255.f, g_ / 255.f, b_ / 255.f, a_ / 255.f};
    }
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

enum class DrawCmd : uint8_t { Rect, TexturedRect, Text, Line };

struct DrawItem {
    DrawCmd cmd = DrawCmd::Rect;
    Rect rect;
    Color color;
    uint16_t textureId = 0;   // atlas tile veya font atlası
    std::string text;
    float scale = 1.f;
    int align = 0;            // 0 sol 1 orta 2 sağ
    float lineWidth = 1.f;
    float x2 = 0, y2 = 0;     // Line için bitiş
};

// çizim listesi toplayıcı
class DrawList {
public:
    void clear() { items_.clear(); }
    void rect(const Rect& r, const Color& c);
    void texturedRect(const Rect& r, uint16_t texId, const Color& tint = Color{});
    void text(const std::string& s, float x, float y, float scale = 1.f,
              const Color& c = Color{}, int align = 0);
    void line(float x1, float y1, float x2, float y2, const Color& c, float width = 1.f);
    // dokunmatik yuvarlak buton (dokunma alanı dikdörtgen kalır)
    void circleButton(float cx, float cy, float radius, const Color& fill,
                      const Color& border, const std::string& label);

    const std::vector<DrawItem>& items() const { return items_; }
    size_t size() const { return items_.size(); }

private:
    std::vector<DrawItem> items_;
};

// ---------------- HUD ----------------
class Hud {
public:
    // ekran boyutu değişince yerleşim yeniden hesaplanır
    void resize(float widthPx, float heightPx, float dpiScale);

    // her karede HUD'u üret
    void build(DrawList& out, const Inventory& inv, float health, float hunger,
               float fps, const Vec3f& pos, int pingMs, int peers,
               float hotbarFlashTimer);

    // oyun içi dokunmatik kontroller (sol joystick + sağ bakış + butonlar)
    struct TouchLayout {
        Vec3f moveAxis{0, 0, 0};     // -1..1 (x=sağ, z=ileri)
        float lookDx = 0.f, lookDy = 0.f;
        bool jumpHeld = false;
        bool sprintHeld = false;
        bool attackHeld = false;
        bool useHeld = false;
        bool breakHeld = false;
    };
    TouchLayout touch;

    void setJoystick(float cx, float cy, float radius);
    void setLookArea(const Rect& r);
    void setActionButton(int index, float cx, float cy, float radius, const std::string& label);

    // dokunma olayları — gerçek girdi katmanı bunu çağırır
    void onTouchDown(int pointerId, float x, float y);
    void onTouchMove(int pointerId, float x, float y);
    void onTouchUp(int pointerId);

    float screenW() const { return w_; }
    float screenH() const { return h_; }

private:
    struct ActionButton {
        float cx = 0, cy = 0, radius = 40.f;
        std::string label;
        bool pressed = false;
        int pointerId = -1;
    };

    float w_ = 1920.f, h_ = 1080.f, dpi_ = 1.f;
    float joyCx_ = 0, joyCy_ = 0, joyRadius_ = 150.f;
    float joyKnobX_ = 0, joyKnobY_ = 0;
    int joyPointer_ = -1;
    Rect lookArea_;
    int lookPointer_ = -1;
    float lastLookX_ = 0, lastLookY_ = 0;
    std::vector<ActionButton> buttons_;
};

// ---------------- menü sistemi ----------------
enum class MenuId : uint8_t { None, Main, Play, Settings, ModeSelect, Pause, Inventory, Crafting };

struct MenuItem {
    std::string label;
    std::string sublabel;
    bool enabled = true;
    std::function<void()> action;
};

class MenuScreen {
public:
    MenuId id = MenuId::None;
    std::string title;
    std::vector<MenuItem> items;
    int selected = 0;

    // NPC mod paneli: her mod için canlı oyuncu sayısı
    struct ModeRow {
        std::string name;       // Düello / Takımlı / Mega
        std::string subtitle;   // 1v1 / 5v5 / 6v6
        int livePlayers = 0;    // sunucudan gelen canlı sayı
        int capacity = 2;
        bool available = true;
    };
    std::vector<ModeRow> modes;
};

class MenuSystem {
public:
    void open(MenuId id);
    void close();
    bool isOpen() const { return current_.id != MenuId::None; }
    MenuId current() const { return current_.id; }

    MenuScreen& screen() { return current_; }

    // navigasyon
    void moveSelection(int delta);
    void activate();      // seçili öğeyi çalıştır
    void back();

    void build(DrawList& out, float screenW, float screenH);

    // NPC ile konuşma → mod paneli
    void showModePanel(std::vector<MenuScreen::ModeRow> rows);

    // dokunmatik
    void onTap(float x, float y, float screenW, float screenH);
    void onKey(int keyCode);

private:
    MenuScreen current_;
    std::vector<MenuScreen> stack_;
    std::vector<Rect> itemRects_;   // dokunma için
    std::vector<Rect> modeRects_;
};

// ---------------- bildirim / toast ----------------
class Toast {
public:
    void show(const std::string& msg, float seconds = 2.5f);
    void update(float dt);
    void build(DrawList& out, float screenW, float screenH);
    bool visible() const { return timer_ > 0.f; }
private:
    std::vector<std::pair<std::string, float>> active_;
    float timer_ = 0.f;
};

// ---------------- sohbet ----------------
class ChatLog {
public:
    static constexpr size_t kMaxLines = 50;
    void add(const std::string& author, const std::string& msg);
    void build(DrawList& out, float screenW, float screenH, float fadeAfter = 8.f);
    void update(float dt);
    const std::vector<std::string>& lines() const { return lines_; }
private:
    std::vector<std::string> lines_;
    float sinceLast_ = 999.f;
};

} // namespace craftya
