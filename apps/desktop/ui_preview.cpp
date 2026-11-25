// Craftya — arayüz önizleme aracı (masaüstü, GL gerektirmez)
// DrawList + gömülü bitmap fontu yazılım rasterizörüyle PNG'e basar.
// Amaç: APK derlemeden HUD'un doğru çizildiğini görsel olarak doğrulamak.
#include "craftya/gui.hpp"
#include "craftya/inventory.hpp"
#include "craftya/items_gen.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>

using namespace craftya;

struct Canvas {
    int w, h;
    std::vector<uint8_t> px;   // RGB

    Canvas(int w_, int h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_ * 3, 0) {}

    void blend(int x, int y, float r, float g, float b, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.f) return;
        size_t i = (static_cast<size_t>(y) * w + x) * 3;
        float ia = 1.f - a;
        px[i]     = static_cast<uint8_t>(px[i] * ia + r * 255.f * a);
        px[i + 1] = static_cast<uint8_t>(px[i + 1] * ia + g * 255.f * a);
        px[i + 2] = static_cast<uint8_t>(px[i + 2] * ia + b * 255.f * a);
    }

    void fill(float r, float g, float b) {
        for (size_t i = 0; i < px.size(); i += 3) {
            px[i] = static_cast<uint8_t>(r * 255.f);
            px[i + 1] = static_cast<uint8_t>(g * 255.f);
            px[i + 2] = static_cast<uint8_t>(b * 255.f);
        }
    }

    // dolu daire / halka
    void circle(float cx, float cy, float radius, float r, float g, float b, float a,
                float thickness) {
        int x0 = static_cast<int>(cx - radius) - 1, x1 = static_cast<int>(cx + radius) + 1;
        int y0 = static_cast<int>(cy - radius) - 1, y1 = static_cast<int>(cy + radius) + 1;
        for (int yy = y0; yy <= y1; ++yy) {
            for (int xx = x0; xx <= x1; ++xx) {
                float dx = xx + 0.5f - cx, dy = yy + 0.5f - cy;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d > radius) continue;
                if (thickness > 0.f && d < radius - thickness) continue;
                // kenar yumuşatma (1 px)
                float edge = std::min(1.f, radius - d);
                blend(xx, yy, r, g, b, a * edge);
            }
        }
    }

    void rect(float x, float y, float ww, float hh, float r, float g, float b, float a) {
        int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
        int x1 = static_cast<int>(std::ceil(x + ww)), y1 = static_cast<int>(std::ceil(y + hh));
        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx)
                blend(xx, yy, r, g, b, a);
    }

    // 5x7 glif tablosu (ui_renderer.cpp ile aynı) — bağımsız kopya, test amaçlı
    static bool glyphPixel(uint8_t code, int gx, int gy);
};

bool glyphPixelRaw(uint8_t code, int x, int y);
bool Canvas::glyphPixel(uint8_t code, int gx, int gy) { return glyphPixelRaw(code, gx, gy); }

void putText(Canvas& c, const std::string& s, float x, float y, float px,
             float r, float g, float b, int align) {
    std::vector<uint8_t> codes;
    // basit ASCII + Türkçe harf eşlemesi
    size_t i = 0;
    while (i < s.size()) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch < 0x80) { codes.push_back(ch); ++i; }
        else {
            // 2 bayt UTF-8
            char two[3] = { s[i], (i + 1 < s.size() ? s[i + 1] : '\0'), 0 };
            uint8_t g2 = '?';
            if (!std::strcmp(two, "İ")) g2 = 1;
            else if (!std::strcmp(two, "ı")) g2 = 2;
            else if (!std::strcmp(two, "Ş")) g2 = 3;
            else if (!std::strcmp(two, "ş")) g2 = 11;
            else if (!std::strcmp(two, "Ç")) g2 = 4;
            else if (!std::strcmp(two, "ç")) g2 = 10;
            else if (!std::strcmp(two, "Ö")) g2 = 5;
            else if (!std::strcmp(two, "ö")) g2 = 8;
            else if (!std::strcmp(two, "Ü")) g2 = 6;
            else if (!std::strcmp(two, "ü")) g2 = 9;
            else if (!std::strcmp(two, "Ğ")) g2 = 7;
            else if (!std::strcmp(two, "ğ")) g2 = 12;
            else if (!std::strcmp(two, "\u00b7")) g2 = 13;
            codes.push_back(g2);
            i += 2;
        }
    }

    float cw = 6.f * px;
    float total = codes.size() * cw;
    float ox = x;
    if (align == 1) ox -= total * 0.5f;
    else if (align == 2) ox -= total;

    for (size_t k = 0; k < codes.size(); ++k) {
        for (int gy = 0; gy < 7; ++gy)
            for (int gx = 0; gx < 5; ++gx)
                if (Canvas::glyphPixel(codes[k], gx, gy))
                    c.rect(ox + k * cw + gx * px, y + gy * px, px, px, r, g, b, 1.f);
    }
}

void savePng(const Canvas& c, const char* path) {
    // PPM yaz (bağımlılık yok), sonra p2p ile PNG'e çevrilir
    FILE* f = std::fopen(path, "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", c.w, c.h);
    std::fwrite(c.px.data(), 1, c.px.size(), f);
    std::fclose(f);
}

int main(int argc, char** argv) {
    int W = argc > 1 ? std::atoi(argv[1]) : 1280;
    int H = argc > 2 ? std::atoi(argv[2]) : 720;
    const char* out = argc > 3 ? argv[3] : "/tmp/ui_preview.ppm";
    std::string mode = argc > 4 ? argv[4] : "hud";

    Canvas c(W, H);
    // arka plan: gökyüzü + zemin (HUD okunabilsin)
    for (int y = 0; y < H; ++y) {
        float t = static_cast<float>(y) / H;
        c.rect(0, static_cast<float>(y), static_cast<float>(W), 1.f,
               0.45f + 0.12f * t, 0.68f + 0.10f * t, 0.95f - 0.05f * t, 1.f);
    }
    c.rect(0, H * 0.62f, W, H * 0.38f, 0.36f, 0.52f, 0.26f, 1.f);

    Inventory inv;
    inv.add(STONE, 64);
    inv.add(OAK_PLANKS, 32);
    inv.add(GRASS_BLOCK, 12);
    inv.add(DIRT, 8);
    inv.add(COBBLESTONE, 24);
    inv.select(1);

    DrawList list;
    Hud hud;
    hud.resize(static_cast<float>(W), static_cast<float>(H), 2.0f);

    ChatLog chat;
    MenuSystem menus;

    if (mode == "hud") {
        hud.build(list, inv, 17.f, 15.f, 58.f, Vec3f{12, 74, -30}, 34, 3, 0.f);
        list.text("CRAFTYA", W * 0.5f, H * 0.14f, 2.6f, Color{1, 1, 1, 0.95f}, 1);
        list.text("canli ekosistem · 457 icerik", W * 0.5f, H * 0.14f + 46.f, 1.1f,
                  Color{0.96f, 0.94f, 0.72f, 1}, 1);
    } else if (mode == "inventory") {
        hud.build(list, inv, 17.f, 15.f, 58.f, Vec3f{12, 74, -30}, 34, 3, 0.f);
        menus.open(MenuId::Inventory);
        menus.screen().inv = &inv;
        menus.screen().cursorSlot = 1;
        menus.build(list, static_cast<float>(W), static_cast<float>(H));
    } else if (mode == "chat") {
        hud.build(list, inv, 17.f, 15.f, 58.f, Vec3f{12, 74, -30}, 34, 3, 0.f);
        chat.add("Ahmet", "kanka bu ne guzel dunya");
        chat.add("Zeynep", "agaclar hep farkli!");
        chat.build(list, static_cast<float>(W), static_cast<float>(H));
        chat.openInput();
        chat.typeChar('s'); chat.typeChar('e'); chat.typeChar('l');
        chat.typeChar('a'); chat.typeChar('m');
        chat.buildKeyboard(list, static_cast<float>(W), static_cast<float>(H));
    } else if (mode == "pause") {
        hud.build(list, inv, 17.f, 15.f, 58.f, Vec3f{12, 74, -30}, 34, 3, 0.f);
        menus.open(MenuId::Pause);
        menus.build(list, static_cast<float>(W), static_cast<float>(H));
    } else if (mode == "main") {
        menus.open(MenuId::Main);
        menus.build(list, static_cast<float>(W), static_cast<float>(H));
    } else if (mode == "modes") {
        std::vector<MenuScreen::ModeRow> rows = {
            {"Duello", "1v1 arena", 3, 2, true},
            {"Takimli", "5v5 savas", 8, 10, true},
            {"Mega", "6v6 karmasa", 11, 12, true},
        };
        menus.showModePanel(rows);
        menus.build(list, static_cast<float>(W), static_cast<float>(H));
    } else if (mode == "settings") {
        menus.open(MenuId::Settings);
        menus.screen().items[0].sublabel = "mesafe 6 chunk / golge acik";
        menus.screen().items[1].sublabel = "muzik 60% / efekt 90%";
        menus.screen().items[2].sublabel = "hassasiyet 100% / ters Y kapali";
        menus.build(list, static_cast<float>(W), static_cast<float>(H));
    }

    // DrawList'i rasterize et
    for (const DrawItem& it : list.items()) {
        switch (it.cmd) {
            case DrawCmd::Rect:
                c.rect(it.rect.x, it.rect.y, it.rect.w, it.rect.h,
                       it.color.r, it.color.g, it.color.b, it.color.a);
                break;
            case DrawCmd::TexturedRect:
                // atlas yok: renkli blok olarak temsil et
                c.rect(it.rect.x, it.rect.y, it.rect.w, it.rect.h,
                       0.55f, 0.45f, 0.35f, 1.f);
                c.rect(it.rect.x, it.rect.y, it.rect.w, 2.f, 0.f, 0.f, 0.f, 0.5f);
                break;
            case DrawCmd::Text:
                putText(c, it.text, it.rect.x, it.rect.y, it.scale <= 0 ? 1.f : it.scale * 2.f,
                        it.color.r, it.color.g, it.color.b, it.align);
                break;
            case DrawCmd::Circle:
                c.circle(it.rect.x + it.rect.w * 0.5f, it.rect.y + it.rect.h * 0.5f,
                         it.radius, it.color.r, it.color.g, it.color.b, it.color.a,
                         it.thickness);
                break;
            case DrawCmd::Line:
                c.rect(it.rect.x, it.rect.y,
                       std::fabs(it.x2 - it.rect.x) + 1.f,
                       std::fabs(it.y2 - it.rect.y) + 1.f,
                       it.color.r, it.color.g, it.color.b, it.color.a);
                break;
        }
    }

    savePng(c, out);
    std::printf("✓ arayüz önizlemesi: %s  (%dx%d, %zu çizim öğesi)\n", out, W, H, list.size());
    return 0;
}
