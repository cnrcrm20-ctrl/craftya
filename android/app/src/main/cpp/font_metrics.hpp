#pragma once
// OTOMATİK ÜRETİLDİ — elle düzenlemeyin.
// Kaynak: ui_renderer.cpp kGlyphs tablosu (5x7 gliflerin gerçek sağ kenarı).
// İlerleme = kAdvance[c-32] glif pikseli + 1 boşluk pikseli.
#include <cstdint>
namespace craftya { namespace fontdata {
const uint8_t kAdvance[95] = {
    3,3,4,5,3,5,3,3,4,4,5,5,3,5,3,
    5,5,4,5,5,5,5,5,5,5,5,3,3,4,5,
    5,5,3,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    3,3,3,5,3,5,5,5,5,5,4,5,5,4,3,
    4,4,5,5,5,5,5,4,5,5,5,5,5,5,5,
    5,3,3,3,3,
};
}} // namespace
