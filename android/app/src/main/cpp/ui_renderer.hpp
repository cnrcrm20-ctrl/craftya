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
    static constexpr int CELL = 8;    // doku hücresi
    static constexpr int COLS = 16;
    static constexpr int ROWS = 8;

    BitmapFont();

    // 8-bit alfa dokusu (COLS*CELL x ROWS*CELL)
    const std::vector<uint8_t>& alpha() const { return alpha_; }
    int texWidth() const { return COLS * CELL; }
    int texHeight() const { return ROWS * CELL; }

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
