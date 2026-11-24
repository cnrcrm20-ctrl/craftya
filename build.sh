#!/usr/bin/env bash
# Craftya — masaüstü derleme + tüm testler
set -e
cd "$(dirname "$0")"

CXX=${CXX:-g++}
FLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Iengine/include"

# tüm motor kaynakları
ENGINE_SRC="
engine/src/json.cpp
engine/src/lighting.cpp
engine/src/atlas.cpp
engine/src/engine.cpp
engine/src/mesher.cpp
engine/src/physics.cpp
engine/src/inventory.cpp
engine/src/render.cpp
engine/src/renderer.cpp
engine/src/game.cpp
engine/src/platform.cpp
engine/src/audio.cpp
engine/src/net.cpp
engine/src/save.cpp
engine/src/gui.cpp
engine/src/interact.cpp
engine/src/anim.cpp
engine/src/ecs.cpp
"

# world.cpp yok (header-only); varsa ekle, yoksa atla
SRC=""
for f in $ENGINE_SRC; do
  [ -f "$f" ] && SRC="$SRC $f"
done

mkdir -p build

echo "▸ motor testleri derleniyor…"
$CXX $FLAGS engine/tests/test_engine.cpp engine/src/mesher.cpp engine/src/physics.cpp \
     engine/src/atlas.cpp engine/src/lighting.cpp -o build/craftya_test

echo "▸ oyun testleri derleniyor…"
$CXX $FLAGS engine/tests/test_game.cpp $SRC -o build/craftya_game_test

echo "▸ sistem testleri derleniyor (json/atlas/envanter/kayit/ag/ecs/ses/gui)…"
$CXX $FLAGS engine/tests/test_systems.cpp \
     engine/src/json.cpp engine/src/lighting.cpp engine/src/atlas.cpp \
     engine/src/inventory.cpp engine/src/save.cpp engine/src/net.cpp \
     engine/src/ecs.cpp engine/src/audio.cpp engine/src/gui.cpp \
     engine/src/mesher.cpp engine/src/physics.cpp engine/src/engine.cpp \
     engine/src/game.cpp engine/src/render.cpp engine/src/renderer.cpp \
     engine/src/platform.cpp engine/src/interact.cpp engine/src/anim.cpp -o build/craftya_sys_test

echo "▸ ASCII görüntüleyici derleniyor…"
$CXX $FLAGS apps/desktop/viewer.cpp $SRC -o build/craftya_viewer

echo "▸ arayüz font verisi üretiliyor…"
python3 - <<'PYFONT'
import re
src = open('android/app/src/main/cpp/ui_renderer.cpp').read()
start = src.index('const Glyph kGlyphs[] = {')
end = src.index('};', src.index('{static_cast<char>(13)')) + 2
table = src[start:end]
out = ['// OTOMATİK ÜRETİLDİ (ui_renderer.cpp glif tablosu) — elle düzenleme',
       '#include <cstdint>', '', 'namespace {',
       'struct Glyph { char c; const char* rows[7]; };', table,
       'static const Glyph* findG(uint8_t code){ for(const auto& g: kGlyphs) if((uint8_t)g.c==code) return &g; return nullptr; }',
       '} // namespace', '',
       'bool glyphPixelRaw(uint8_t code, int x, int y){',
       '    const Glyph* g=findG(code); if(!g||x<0||x>=5||y<0||y>=7) return false;',
       "    return g->rows[y][x]=='#';", '}', '',
       'int glyphCount(){ return (int)(sizeof(kGlyphs)/sizeof(kGlyphs[0])); }', '']
open('build/ui_font_data.cpp', 'w').write('\n'.join(out))
PYFONT

echo "▸ arayüz önizleyici derleniyor…"
$CXX $FLAGS apps/desktop/ui_preview.cpp build/ui_font_data.cpp \
     engine/src/gui.cpp engine/src/inventory.cpp -o build/craftya_uipreview

echo "▸ simülasyon rasterizörü derleniyor…"
$CXX $FLAGS apps/desktop/render_sim.cpp \
     engine/src/game.cpp engine/src/render.cpp engine/src/mesher.cpp \
     engine/src/physics.cpp engine/src/platform.cpp engine/src/lighting.cpp \
     engine/src/atlas.cpp engine/src/anim.cpp -o build/craftya_sim

echo ""
echo "▸ motor testleri:"; ./build/craftya_test | tail -2
echo "▸ oyun testleri:";  ./build/craftya_game_test | tail -2
echo "▸ sistem testleri:"; ./build/craftya_sys_test | tail -2
echo ""
echo "✓ hazır: build/craftya_test  build/craftya_game_test  build/craftya_sys_test  build/craftya_viewer"
echo "  görüntüleyici: ./build/craftya_viewer 78 26"
echo "  simülasyon:    ./build/craftya_sim 900 30 /tmp/frames 854 480"
