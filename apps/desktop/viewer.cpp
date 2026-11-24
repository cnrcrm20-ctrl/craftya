// Craftya — masaüstü ASCII görselleştirici
// Ray-casting ile motorun gerçek dünya verisinden görüntü üretir.
// Amaç: renderer olmadan motorun dünya/mesh/fizik hattını gözle doğrulamak.
#include "craftya/game.hpp"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>

using namespace craftya;

static const char* shadeChars = " .:-=+*#%@";

int main(int argc, char** argv) {
    int W = 64, H = 24;
    if (argc >= 3) { W = atoi(argv[1]); H = atoi(argv[2]); }

    Game game(424242, true);
    std::printf("Craftya motor — ASCII görünüm (%dx%d)\n", W, H);
    std::printf("yükseklik(0,0)=%.1f  oyuncu y=%.2f\n\n", game.world().heightAt(0,0), game.player().pos.y);

    // kamerayı yüksek bir noktaya al, ufka baksın (arazi kabartısı görünsün)
    game.camera().pos = {8.0f, 46.0f, 8.0f};
    game.camera().yaw = 0.6f;
    game.camera().pitch = -0.28f;
    game.camera().farZ = 260.f;
    for (int frame = 0; frame < 1; ++frame) {
        InputState in;
        game.update(1.0f/60.0f, in);
    }

    const auto& st = game.stats();
    std::printf("yüklü chunk=%d  görünür=%d  çizim=%d  mesh=%.2f ms  vram=%.0f KB\n\n",
                st.loadedChunks, st.visibleChunks, st.drawCalls, st.meshMs, st.vramBytes/1024.0);

    // basit ışın izleme: her piksel için kameradan ışın at, ilk bloğu bul
    const Camera& cam = game.camera();
    Vec3f fwd = cam.forward();
    Vec3f right = cam.right();
    Vec3f up = fwd.cross(right).normalized() * -1.f;

    std::string frame;
    frame.reserve(static_cast<size_t>(W + 1) * H);
    float aspect = 1.9f;
    float fovScale = std::tan(cam.fovDeg * 0.5f * 3.14159265f / 180.f);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float u = (2.f * (x + 0.5f) / W - 1.f) * fovScale * aspect;
            float v = (1.f - 2.f * (y + 0.5f) / H) * fovScale;
            Vec3f dir = (fwd + right * u + up * v).normalized();

            RayHit hit = Raycast::cast(game.world(), cam.pos, dir, 220.f);
            char c = ' ';
            if (hit.hit) {
                BlockId b = game.world().getBlock(hit.block.x, hit.block.y, hit.block.z);
                // yüz normaline göre gölgeleme + mesafeye göre sis
                float lightOut = 0.5f;
                if (hit.normal.y > 0) lightOut = 1.0f;
                else if (hit.normal.y < 0) lightOut = 0.35f;
                else lightOut = 0.68f;
                // biyom/derinlik farkı için blok tipine göre karakter
                int ch = static_cast<int>(lightOut * 8.f);
                // yüksekliğe göre ton (arazi kabartısı görünsün)
                int hgt = hit.block.y;
                if (b == WATER) ch = std::max(2, 4 - hgt/12);
                else if (b == GRASS) ch = 4 + (lightOut > 0.9f ? 4 : 2);
                else if (b == SAND) ch = 5;
                else if (b == STONE || b == BEDROCK) ch = 3 + (hgt % 4);
                else if (b == OAK_LEAVES || b == OAK_LOG) ch = 8;
                else ch = static_cast<int>(lightOut * 7.f) + (hgt % 3);
                float fog = 1.f - hit.dist / 190.f;
                ch = static_cast<int>(ch * std::max(0.15f, fog));
                ch = std::max(0, std::min(9, ch));
                c = shadeChars[ch];
            }
            frame.push_back(c);
        }
        frame.push_back('\n');
    }
    std::printf("%s", frame.c_str());

    std::printf("\n=== motor durumu ===\n");
    std::printf("hedef blok: ");
    const RayHit& h = game.crosshair();
    if (h.hit) {
        BlockId b = game.world().getBlock(h.block.x, h.block.y, h.block.z);
        std::printf("(%d,%d,%d) %s  mesafe=%.2f\n",
                    h.block.x, h.block.y, h.block.z, blockDef(b).name, h.dist);
    } else {
        std::printf("yok\n");
    }
    return 0;
}
