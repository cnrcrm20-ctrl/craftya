// Craftya — gömülü bitmap font + DrawList çizici
// Harici font dosyası YOK: tüm glifler kod içinde tanımlı, doku cihazda üretilir.
#include "ui_renderer.hpp"
#include "font_metrics.hpp"
#include <GLES3/gl3.h>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace craftya {

namespace {
// 5x7 piksel glifler. '#' = dolu, '.' = boş. 7 satır × 5 sütun.
struct Glyph { char c; const char* rows[7]; };

const Glyph kGlyphs[] = {
    {'A', {"..#..", ".#.#.", "#...#", "#...#", "#####", "#...#", "#...#"}},
    {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
    {'C', {".####", "#....", "#....", "#....", "#....", "#....", ".####"}},
    {'D', {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
    {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {'G', {".####", "#....", "#....", "#.###", "#...#", "#...#", ".####"}},
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'I', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"}},
    {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'N', {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}},
    {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"}},
    {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
    {'a', {".....", ".....", ".###.", "....#", ".####", "#...#", ".####"}},
    {'b', {"#....", "#....", "####.", "#...#", "#...#", "#...#", "####."}},
    {'c', {".....", ".....", ".####", "#....", "#....", "#....", ".####"}},
    {'d', {"....#", "....#", ".####", "#...#", "#...#", "#...#", ".####"}},
    {'e', {".....", ".....", ".###.", "#...#", "#####", "#....", ".###."}},
    {'f', {"..##.", ".#...", "####.", ".#...", ".#...", ".#...", ".#..."}},
    {'g', {".....", ".....", ".###.", "#...#", "#...#", ".####", "####."}},
    {'h', {"#....", "#....", "####.", "#...#", "#...#", "#...#", "#...#"}},
    {'i', {"..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###."}},
    {'j', {".....", "..#..", "..#..", "..#..", "..#..", "..#..", "##..."}},
    {'k', {"#....", "#....", "#..#.", "#.#..", "##...", "#.#..", "#..#."}},
    {'l', {".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'m', {".....", ".....", "##.#.", "#.#.#", "#.#.#", "#...#", "#...#"}},
    {'n', {".....", ".....", "####.", "#...#", "#...#", "#...#", "#...#"}},
    {'o', {".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###."}},
    {'p', {".....", ".....", "####.", "#...#", "#...#", "####.", "#...."}},
    {'q', {".....", ".....", ".####", "#...#", "#...#", ".####", "....#"}},
    {'r', {".....", ".....", "#.##.", "##...", "#....", "#....", "#...."}},
    {'s', {".....", ".....", ".####", "#....", ".###.", "....#", "####."}},
    {'t', {".#...", ".#...", "####.", ".#...", ".#...", ".#..#", "..##."}},
    {'u', {".....", ".....", "#...#", "#...#", "#...#", "#...#", ".####"}},
    {'v', {".....", ".....", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'w', {".....", ".....", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#."}},
    {'x', {".....", ".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#"}},
    {'y', {".....", ".....", "#...#", "#...#", "#...#", ".####", "....#"}},
    {'z', {".....", ".....", "#####", "...#.", "..#..", ".#...", "#####"}},
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
    {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {',', {".....", ".....", ".....", ".....", ".##..", ".##..", ".#..."}},
    {':', {".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."}},
    {';', {".....", ".##..", ".##..", ".....", ".##..", ".##..", ".#..."}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {'-', {".....", ".....", ".....", "#####", ".....", ".....", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
    {'%', {"#...#", "...#.", "..#..", "..#..", ".#...", "#...#", "....."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
    {'[', {"..###", "..#..", "..#..", "..#..", "..#..", "..#..", "..###"}},
    {']', {"###..", "..#..", "..#..", "..#..", "..#..", "..#..", "###.."}},
    {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
    {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
    {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
    {'*', {".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."}},
    {'#', {".#.#.", "#####", ".#.#.", ".#.#.", ".#.#.", "#####", ".#.#."}},
    {'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    {'|', {"..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'\'', {"..#..", "..#..", ".....", ".....", ".....", ".....", "....."}},
    {'"', {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."}},
    // Türkçe + yaygın aksanlı harfler (ASCII olmayanlar ayrı hücrelerde)
    {static_cast<char>(1), {"..##.", "..#..", "#####", "#...#", "#...#", "#...#", "#...#"}}, // İ
    {static_cast<char>(2), {".....", ".###.", "....#", ".####", "#...#", "#...#", ".####"}}, // ı
    {static_cast<char>(3), {"##.##", "..#..", "#####", "#...#", "#...#", "#...#", "#...#"}}, // Ş
    {static_cast<char>(4), {"##.##", "###..", "#....", "#....", "#....", "#....", ".####"}}, // Ç
    {static_cast<char>(5), {"##.##", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}}, // Ö
    {static_cast<char>(6), {"##.##", "#...#", "#...#", "#...#", "#...#", "#...#", ".####"}}, // Ü
    {static_cast<char>(7), {"##.##", ".###.", ".#...", ".##..", ".#...", ".#...", ".####"}}, // Ğ
    {static_cast<char>(8), {".....", "##.##", ".###.", "#...#", "#...#", "#...#", ".###."}}, // ö
    {static_cast<char>(9), {".....", "##.##", "#...#", "#...#", "#...#", "#...#", ".####"}}, // ü
    {static_cast<char>(10), {".....", "##.##", ".####", "#....", "#....", "#....", ".####"}}, // ç
    {static_cast<char>(11), {".....", "##.##", ".####", "#....", ".###.", "....#", "####."}}, // ş
    {static_cast<char>(12), {".....", "#...#", "#...#", "#...#", ".####", "....#", "####."}}, // ğ
    {static_cast<char>(13), {".....", ".....", ".....", "..#..", ".....", ".....", "....."}}, // ·
};

// UTF-8 aksanlı harf → glif kodu
struct AccentMap { const char* utf8; uint8_t glyph; };
const AccentMap kAccents[] = {
    {"İ", 1}, {"ı", 2}, {"Ş", 3}, {"ş", 11}, {"Ç", 4}, {"ç", 10},
    {"Ö", 5}, {"ö", 8}, {"Ü", 6}, {"ü", 9}, {"Ğ", 7}, {"ğ", 12},
    {"â", 'a'}, {"î", 'i'}, {"û", 'u'}, {"\u00b7", 13}, {"\u2022", 13},
};

int findGlyph(uint8_t c) {
    for (const auto& g : kGlyphs) if (static_cast<uint8_t>(g.c) == c) return static_cast<uint8_t>(g.c);
    return -1;
}
} // namespace

BitmapFont::BitmapFont()
    : alpha_(static_cast<size_t>(COLS * CELL_W) * (ROWS * CELL_H), 0),
      mask_(COLS * ROWS * 35, 0) {
    for (const auto& g : kGlyphs) {
        uint8_t code = static_cast<uint8_t>(g.c);
        int idx = code;
        if (idx < 0 || idx >= COLS * ROWS) continue;
        buildGlyph(idx, g.rows);
    }
    // boşluk (32) boş kalır; alt çizgi vb. tanımlı
}

// glifin doku içindeki UV köşeleri
void BitmapFont::glyphUV(int glyph, float& u0, float& v0, float& u1, float& v1) {
    int idx = glyph;
    if (idx < 0 || idx >= COLS * ROWS) idx = '?';
    const float TW = static_cast<float>(COLS * CELL_W);
    const float TH = static_cast<float>(ROWS * CELL_H);
    int cx = (idx % COLS) * CELL_W;
    int cy = (idx / COLS) * CELL_H;
    u0 = static_cast<float>(cx) / TW;
    v0 = static_cast<float>(cy) / TH;
    u1 = static_cast<float>(cx + CELL_W) / TW;
    v1 = static_cast<float>(cy + CELL_H) / TH;
}

void BitmapFont::buildGlyph(int index, const char* const rows[7]) {
    const int cx = (index % COLS) * CELL_W;
    const int cy = (index / COLS) * CELL_H;

    // 1 px şeffaf kenar (GL_LINEAR sızmasını önler), glif 3x ölçekle.
    for (int y = 0; y < GH; ++y) {
        for (int x = 0; x < GW; ++x) {
            if (rows[y][x] != '#') continue;
            for (int dy = 0; dy < GSCALE; ++dy) {
                for (int dx = 0; dx < GSCALE; ++dx) {
                    int ax = cx + 1 + x * GSCALE + dx;
                    int ay = cy + 1 + y * GSCALE + dy;
                    alpha_[static_cast<size_t>(ay) * texWidth() + ax] = 255;
                }
            }
        }
    }

    // maske (testler için gerçek 5x7)
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
            if (rows[y][x] == '#')
                mask_[static_cast<size_t>(index) * 35 + y * GW + x] = 1;
}

bool BitmapFont::pixel(int glyph, int x, int y) const {
    if (glyph < 0 || glyph >= COLS * ROWS) return false;
    if (x < 0 || x >= GW || y < 0 || y >= GH) return false;
    return mask_[static_cast<size_t>(glyph) * 35 + y * GW + x] != 0;
}

void BitmapFont::decode(const std::string& s, std::vector<uint8_t>& out) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out.push_back(c);
            ++i;
        } else {
            // 2 baytlık UTF-8 dizisi mi?
            bool mapped = false;
            for (const auto& a : kAccents) {
                size_t len = std::strlen(a.utf8);
                if (i + len <= s.size() && s.compare(i, len, a.utf8) == 0) {
                    out.push_back(a.glyph);
                    i += len;
                    mapped = true;
                    break;
                }
            }
            if (!mapped) {
                out.push_back('?');
                ++i;
                if (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
            }
        }
    }
}

// kırpılmış UV: glifin gerçek genişliği kadar
void BitmapFont::glyphUVClip(int glyph, float advanceW,
                             float& u0, float& v0, float& u1, float& v1) {
    int idx = glyph;
    if (idx < 0 || idx >= COLS * ROWS) idx = '?';
    const float TW = static_cast<float>(COLS * CELL_W);
    const float TH = static_cast<float>(ROWS * CELL_H);
    int cx = (idx % COLS) * CELL_W;
    int cy = (idx / COLS) * CELL_H;

    float aw = advanceW < 1.f ? 1.f : (advanceW > 5.f ? 5.f : advanceW);

    // 1 px şeffaf kenarı ATLA, tam sayı ölçekle örnekle
    u0 = static_cast<float>(cx + 1) / TW;
    u1 = static_cast<float>(cx + 1) / TW + (aw * static_cast<float>(GSCALE)) / TW;
    v0 = static_cast<float>(cy + 1) / TH;
    v1 = static_cast<float>(cy + 1 + GH * GSCALE) / TH;
}

// ---------------- UiRenderer ----------------
void UiRenderer::render(GLESBackend& gl, const DrawList& list, int screenW, int screenH) {
    // font atlasını bir kez GPU'ya yükle (anti-aliased, DOĞRUSAL filtre)
    if (!fontUploaded_) {
        gl.uploadFontAtlas(font_.alpha().data(), font_.texWidth(), font_.texHeight());
        fontUploaded_ = true;
    }
    gl.beginUi(screenW, screenH);
    gl.beginUiText();

    for (const DrawItem& it : list.items()) {
        // TOPLU YAZI: ardışık metin komutları tek çizim çağrısında gider.
        // Araya başka bir çizim girerse tampon boşaltılır; böylece çizim
        // SIRASI korunur (yazı sonradan çizilen panellerin altında kalmaz).
        if (it.cmd != DrawCmd::Text) gl.flushUiText();
        switch (it.cmd) {
            case DrawCmd::Rect: {
                if (it.corner > 0.5f || it.hasColor2) {
                    // modern kart/buton: yuvarlatılmış köşe + gradyan
                    float top[4] = {it.color.r, it.color.g, it.color.b, it.color.a};
                    float bot[4] = {it.color2.r, it.color2.g, it.color2.b, it.color2.a};
                    gl.drawUiQuadStyled(it.rect.x, it.rect.y, it.rect.w, it.rect.h,
                                        it.color.r, it.color.g, it.color.b, it.color.a,
                                        it.corner,
                                        it.hasColor2 ? top : nullptr,
                                        it.hasColor2 ? bot : nullptr);
                } else {
                    gl.drawUiQuad(it.rect.x, it.rect.y, it.rect.w, it.rect.h,
                                  it.color.r, it.color.g, it.color.b, it.color.a);
                }
                break;
            }
            case DrawCmd::Circle: {
                // Gerçek disk / halka: SDF yuvarlatılmış kare hilesiyle.
                // Halka için uRingInner = r - kalınlık veriliyor; önceden
                // iç boşluk delinmediği için çerçeveler dolu disk çıkıyordu.
                float r = it.radius > 0.f ? it.radius : it.rect.w * 0.5f;
                float inner = 0.f;
                if (it.thickness > 0.f && it.thickness < r)
                    inner = r - it.thickness;
                gl.drawUiQuadStyled(it.rect.x, it.rect.y, it.rect.w, it.rect.h,
                                    it.color.r, it.color.g, it.color.b, it.color.a,
                                    r, nullptr, nullptr, -1, inner);
                break;
            }
            case DrawCmd::TexturedRect: {
                gl.drawUiQuad(it.rect.x, it.rect.y, it.rect.w, it.rect.h,
                              it.color.r, it.color.g, it.color.b, it.color.a,
                              static_cast<int>(it.textureId));
                break;
            }
            case DrawCmd::Line: {
                // ince çizgi = eksen hizalı dörtgen
                float x1 = it.rect.x, y1 = it.rect.y, x2 = it.x2, y2 = it.y2;
                float dx = x2 - x1, dy = y2 - y1;
                float w = it.lineWidth == 0.f ? 1.f : it.lineWidth;
                if (std::fabs(dx) >= std::fabs(dy)) {
                    gl.drawUiQuad(std::min(x1, x2), y1 - w * 0.5f, std::fabs(dx), w,
                                  it.color.r, it.color.g, it.color.b, it.color.a);
                } else {
                    gl.drawUiQuad(x1 - w * 0.5f, std::min(y1, y2), w, std::fabs(dy),
                                  it.color.r, it.color.g, it.color.b, it.color.a);
                }
                break;
            }
            case DrawCmd::Text: {
                std::vector<uint8_t> codes;
                BitmapFont::decode(it.text, codes);

                // ---- MODERN YOL: anti-aliased glif atlası, tek çizim/glif ----
                if (gl.fontReady()) {
                    // ESKİ YERLEŞİMLE BİREBİR AYNI METRİK:
                    // eski piksel yolu 7 satırı 2*scale pikselle çiziyordu,
                    // yani glif yüksekliği = 14*scale. 1.56 katsayısı
                    // eklenince tüm yazılar %56 büyüyüp HUD'ı taşırıyordu.
                    // Yumuşaklık atlasın 3x çözünürlüğünden geliyor, boyuttan değil.
                    float glyphH = 14.f * (it.scale <= 0.f ? 1.f : it.scale);
                    float pxUnit = glyphH / 7.f;
                    auto advWidthOf = [&](int g) -> float {
                        return (g >= 32 && g <= 126)
                                   ? static_cast<float>(fontdata::kAdvance[g - 32]) : 3.f;
                    };
                    auto advanceOf = [&](int g) -> float {
                        // eski ilerleme (GW+1)*px = 6*px = 12*scale idi;
                        // 5 + 1.0 = 6 glif pikseli ile aynı.
                        return (advWidthOf(g) + 1.f) * pxUnit;
                    };
                    float total = 0.f;
                    for (int g : codes) total += advanceOf(g);
                    float ox = it.rect.x;
                    if (it.align == 1) ox -= total * 0.5f;
                    else if (it.align == 2) ox -= total;
                    float cursor = ox;
                    for (size_t i = 0; i < codes.size(); ++i) {
                        int g = codes[i];
                        float aw = advWidthOf(g);
                        if (g != ' ') {
                            float u0, v0, u1, v1;
                            // KIRPILMIŞ UV: yalnızca glifin gerçek genişliği
                            BitmapFont::glyphUVClip(g, aw, u0, v0, u1, v1);
                            // TOPLU: çizim çağrısı değil, köşe tamponuna ekle
                            gl.addUiGlyph(cursor, it.rect.y, aw * pxUnit, glyphH,
                                          u0, v0, u1, v1,
                                          it.color.r, it.color.g, it.color.b, it.color.a);
                        }
                        cursor += advanceOf(g);
                    }
                    break;
                }
                // eski yol (doku yüklenmemişse)
                float px = 2.f * (it.scale <= 0.f ? 1.f : it.scale);
                float cw = (BitmapFont::GW + 1) * px;
                float total = codes.size() * cw;
                float ox = it.rect.x;
                if (it.align == 1) ox -= total * 0.5f;
                else if (it.align == 2) ox -= total;

                for (size_t i = 0; i < codes.size(); ++i) {
                    int g = codes[i];
                    if (g == ' ') continue;
                    float gx = ox + static_cast<float>(i) * cw;
                    // glif = 5x7 ayrık pikseller; her piksel için bir dörtgen
                    // (font atlası tek doku olarak yüklenip tek çizimde de birleştirilebilir,
                    //  ancak piksel sayısı az olduğu için bu yol mobilde yeterince hızlı)
                    for (int yy = 0; yy < BitmapFont::GH; ++yy) {
                        for (int xx = 0; xx < BitmapFont::GW; ++xx) {
                            if (!font_.pixel(g, xx, yy)) continue;
                            gl.drawUiQuad(gx + xx * px, it.rect.y + yy * px, px, px,
                                          it.color.r, it.color.g, it.color.b, it.color.a);
                        }
                    }
                }
                break;
            }
        }
    }
    gl.flushUiText();   // tüm yazı tek çizim çağrısında
    gl.endUi();
}

} // namespace craftya
