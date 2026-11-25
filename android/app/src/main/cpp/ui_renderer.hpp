// Craftya — 2B arayüz çizici: DrawList → GLES ekran
// Bitmap font motor içinde gömülü üretilir (harici dosya/varlık yok).
#pragma once
#include "craftya/gui.hpp"
#include "gles_backend.hpp"
#include <vector>
#include <string>

namespace craftya {

// 5x7 piksel bitmap font — ASCII 32..126 + Türkçe karakter eşlemesi.
// Doku cihazda üretilir: bağımlılık yok.
class BitmapFont {
public:
    static constexpr int GW = 5;      // glif genişliği (piksel)
    static constexpr int GH = 7;      // glif yüksekliği
    // Hücre: 5x7 glif × 3 ölçek = 15x21 + her yanda 1 px ŞEFFAF kenar = 17x23.
    // Şeffaf kenar, GL_LINEAR filtrenin komşu gliften örnek almasını
    // (hayalet çizgi) engeller. Ölçek tam sayı olduğu için glif pikselleri
    // keskin kalır; yumuşatma yalnızca büyütmeden gelir.
    static constexpr int GSCALE = 3;                 // glif piksel ölçeği
    static constexpr int CELL_W = GW * GSCALE + 2;   // 17
    static constexpr int CELL_H = GH * GSCALE + 2;   // 23
    static constexpr int CELL = CELL_H;              // geriye dönük
    static constexpr int COLS = 16;
    static constexpr int ROWS = 8;

    BitmapFont();

    // 8-bit alfa dokusu (COLS*CELL x ROWS*CELL)
    const std::vector<uint8_t>& alpha() const { return alpha_; }
    int texWidth() const { return COLS * CELL_W; }
    int texHeight() const { return ROWS * CELL_H; }
    // tek glifin doku içindeki UV köşeleri (tam hücre)
    static void glyphUV(int glyph, float& u0, float& v0, float& u1, float& v1);
    // KIRPILMIŞ UV: yalnızca glifin gerçek genişliği kadar.
    // advanceW: 1..5 glif piksel genişliği. Genişliği dar harflerde
    // dokunun boş kısmı çizilmez → harfler üst üste binmez.
    static void glyphUVClip(int glyph, float advanceW,
                            float& u0, float& v0, float& u1, float& v1);

    // UTF-8 metni glif indekslerine çevir (Türkçe harfler eşlenir)
    static void decode(const std::string& utf8, std::vector<uint8_t>& out);
    // tek glif için dolu pikselleri oku (test/ölçüm)
    bool pixel(int glyph, int x, int y) const;

private:
    void buildGlyph(int index, const char* const rows[7]);
    std::vector<uint8_t> alpha_;
    std::vector<uint8_t> mask_;   // glif başına 35 bit
};

// DrawList'i ekrana çizer
class UiRenderer {
public:
    void render(GLESBackend& gl, const DrawList& list, int screenW, int screenH);

private:
    BitmapFont font_;
    bool fontUploaded_ = false;
};

} // namespace craftya
