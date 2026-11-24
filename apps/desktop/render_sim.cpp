// Craftya — yazılım rasterizörü ile gerçek motor simülasyonu
// Motorun KENDİ çıktısını (mesh + ışık + kamera + mob animasyonu) GPU olmadan çizer.
// Kullanım: craftya_sim <kare_sayisi> <fps> <cikis_dizini> [genislik] [yukseklik]
#include "craftya/world.hpp"
#include "craftya/game.hpp"
#include "craftya/mesher.hpp"
#include "craftya/render.hpp"
#include "craftya/lighting.hpp"
#include "craftya/atlas.hpp"
#include "craftya/mobs_gen.hpp"
#include "craftya/anim.hpp"
#include "craftya/gui.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>

using namespace craftya;

// ---------------- framebuffer ----------------
struct Framebuffer {
    int w = 0, h = 0;
    std::vector<float> color;   // rgb
    std::vector<float> depth;

    void resize(int W, int H) {
        w = W; h = H;
        color.assign(static_cast<size_t>(W) * H * 3, 0.f);
        depth.assign(static_cast<size_t>(W) * H, 1e9f);
    }
    void clearSky(const float top[3], const float bottom[3]) {
        for (int y = 0; y < h; ++y) {
            float t = static_cast<float>(y) / (h - 1);
            // ekranın üstü gökyüzü, altı ufuk
            float tt = std::pow(t, 0.75f);
            float r = top[0] + (bottom[0] - top[0]) * tt;
            float g = top[1] + (bottom[1] - top[1]) * tt;
            float b = top[2] + (bottom[2] - top[2]) * tt;
            for (int x = 0; x < w; ++x) {
                size_t i = (static_cast<size_t>(y) * w + x) * 3;
                color[i] = r; color[i+1] = g; color[i+2] = b;
            }
        }
    }
    // alpha < 1 ise gerçek alfa karışımı yap
    inline void putA(int x, int y, float r, float g, float b, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        if (a <= 0.004f) return;
        size_t i = (static_cast<size_t>(y) * w + x) * 3;
        if (a >= 0.996f) { color[i] = r; color[i+1] = g; color[i+2] = b; return; }
        color[i]   = color[i]   * (1.f - a) + r * a;
        color[i+1] = color[i+1] * (1.f - a) + g * a;
        color[i+2] = color[i+2] * (1.f - a) + b * a;
    }
    inline void put(int x, int y, float r, float g, float b) { putA(x, y, r, g, b, 1.f); }
};

// ---------------- köşe ----------------
struct SV {
    float x, y;         // NDC
    float invW;         // 1/w — perspektif doğru interpolasyon anahtarı
    float viewDepth;    // w (kamera düzleminden uzaklık)
    float u, v;
    float light;
    float fog;          // 0..1 (1 = tam sis)
    bool valid = false;
};

static Mat4 g_viewProj;

static SV project(const Vec3f& p, const Vec3f& camPos, const Camera& cam,
                  float u, float v, float light) {
    SV o;
    // dünya → kamera
    Mat4 vp = g_viewProj;
    float cx = p.x, cy = p.y, cz = p.z;
    float x = vp.m[0]*cx + vp.m[4]*cy + vp.m[8]*cz  + vp.m[12];
    float y = vp.m[1]*cx + vp.m[5]*cy + vp.m[9]*cz  + vp.m[13];
    float z = vp.m[2]*cx + vp.m[6]*cy + vp.m[10]*cz + vp.m[14];
    float w = vp.m[3]*cx + vp.m[7]*cy + vp.m[11]*cz + vp.m[15];
    if (w < 0.05f) return o;   // kameranın arkasında
    float invW = 1.f / w;
    o.x = x * invW;
    o.y = y * invW;
    o.invW = invW;
    o.viewDepth = w;
    (void)z;
    o.u = u; o.v = v; o.light = light;
    // sis: kameradan uzaklık
    float dx = cx - camPos.x, dy = cy - camPos.y, dz = cz - camPos.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    const float fogStart = FOG_START_CHUNK * CHUNK_W;
    const float fogEnd = FOG_END_CHUNK * CHUNK_W;
    o.fog = std::clamp((dist - fogStart) / (fogEnd - fogStart), 0.f, 1.f);
    o.valid = true;
    (void)cam;
    return o;
}

// ---------------- triangle raster ----------------
static void drawTri(Framebuffer& fb, const SV& a, const SV& b, const SV& c,
                    const Image& atlas, const float fogCol[3], bool wire = false) {
    if (!a.valid || !b.valid || !c.valid) return;

    // ekran koordinatı: NDC → piksel
    auto sx = [&](float ndc) { return (ndc * 0.5f + 0.5f) * fb.w; };
    auto sy = [&](float ndc) { return (1.f - (ndc * 0.5f + 0.5f)) * fb.h; };

    float x0 = sx(a.x), y0 = sy(a.y);
    float x1 = sx(b.x), y1 = sy(b.y);
    float x2 = sx(c.x), y2 = sy(c.y);

    int minX = static_cast<int>(std::floor(std::min({x0, x1, x2})));
    int maxX = static_cast<int>(std::ceil (std::max({x0, x1, x2})));
    int minY = static_cast<int>(std::floor(std::min({y0, y1, y2})));
    int maxY = static_cast<int>(std::ceil (std::max({y0, y1, y2})));
    minX = std::max(0, minX); minY = std::max(0, minY);
    maxX = std::min(fb.w - 1, maxX); maxY = std::min(fb.h - 1, maxY);
    if (minX > maxX || minY > maxY) return;

    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (std::fabs(area) < 1e-6f) return;
    float invArea = 1.f / area;

    if (wire) {
        auto line = [&](float ax, float ay, float bx, float by) {
            int steps = static_cast<int>(std::max(std::fabs(bx-ax), std::fabs(by-ay)));
            for (int i = 0; i <= steps; ++i) {
                float t = steps ? static_cast<float>(i) / steps : 0.f;
                fb.put(static_cast<int>(ax + (bx-ax)*t), static_cast<int>(ay + (by-ay)*t), 0.2f, 1.f, 0.5f);
            }
        };
        line(x0, y0, x1, y1); line(x1, y1, x2, y2); line(x2, y2, x0, y0);
        return;
    }

    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            float fx = px + 0.5f, fy = py + 0.5f;
            // barycentric
            float w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) * invArea;
            float w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) * invArea;
            float w2 = 1.f - w0 - w1;
            if (w0 < -0.0001f || w1 < -0.0001f || w2 < -0.0001f) continue;
            (void)w2;

            // perspektif doğru: 1/w ekran uzayında doğrusal
            float iw = w0 * a.invW + w1 * b.invW + w2 * c.invW;
            if (iw <= 1e-9f) continue;
            float depth = 1.f / iw;      // kamera düzleminden gerçek uzaklık
            size_t di = static_cast<size_t>(py) * fb.w + px;
            if (depth >= fb.depth[di]) continue;

            float uu = (w0 * a.u * a.invW + w1 * b.u * b.invW + w2 * c.u * c.invW) / iw;
            float vv = (w0 * a.v * a.invW + w1 * b.v * b.invW + w2 * c.v * c.invW) / iw;
            float lt = (w0 * a.light * a.invW + w1 * b.light * b.invW +
                        w2 * c.light * c.invW) / iw;
            float fg = (w0 * a.fog * a.invW + w1 * b.fog * b.invW + w2 * c.fog * c.invW) / iw;

            // atlas örnekle (nearest)
            int tx = static_cast<int>(uu * atlas.width);
            int ty = static_cast<int>(vv * atlas.height);
            tx = std::clamp(tx, 0, atlas.width - 1);
            ty = std::clamp(ty, 0, atlas.height - 1);
            size_t ai = (static_cast<size_t>(ty) * atlas.width + tx) * 4;
            float r = atlas.pixels[ai]     / 255.f;
            float g = atlas.pixels[ai + 1] / 255.f;
            float bl = atlas.pixels[ai + 2] / 255.f;
            float alpha = atlas.pixels[ai + 3] / 255.f;
            // cutout: saydam pikseller ne renk ne derinlik yazar (yaprak delikleri)
            if (alpha < 0.5f) continue;
            fb.depth[di] = depth;

            // çevre ışığı tabanı: tam siyah yüz olmasın
            lt = 0.34f + lt * 0.66f;
            r *= lt; g *= lt; bl *= lt;
            // sis karışımı
            r = r + (fogCol[0] - r) * fg;
            g = g + (fogCol[1] - g) * fg;
            bl = bl + (fogCol[2] - bl) * fg;

            fb.putA(px, py, r, g, bl, alpha);
        }
    }
}

// ---------------- mob küpü çiz ----------------
struct Box {
    Vec3f c;        // merkez
    Vec3f half;     // yarı boyut
    Vec3f rot;      // radyan
    uint32_t color;
    float shade;
};

static Vec3f rotateEuler(const Vec3f& p, const Vec3f& r) {
    float cx = std::cos(r.x), sx = std::sin(r.x);
    float cy = std::cos(r.y), sy = std::sin(r.y);
    float cz = std::cos(r.z), sz = std::sin(r.z);
    // ZYX sırası
    Vec3f a{p.x * cz - p.y * sz, p.x * sz + p.y * cz, p.z};
    Vec3f b{a.x * cy + a.z * sy, a.y, -a.x * sy + a.z * cy};
    Vec3f c{b.x, b.y * cx - b.z * sx, b.y * sx + b.z * cx};
    return c;
}

static void drawBox(Framebuffer& fb, const Box& box, const Camera& cam,
                    const float fogCol[3], float sun) {
    static const int FACES[6][4] = {
        {0,1,2,3}, {6,5,4,7}, {4,5,1,0}, {7,6,2,3}, {5,6,7,4}, {0,3,7,4}
    };
    // 8 köşe
    Vec3f corners[8];
    for (int i = 0; i < 8; ++i) {
        Vec3f p{
            (i & 1) ? box.half.x : -box.half.x,
            (i & 2) ? box.half.y : -box.half.y,
            (i & 4) ? box.half.z : -box.half.z
        };
        corners[i] = box.c + rotateEuler(p, box.rot);
    }
    int R = (box.color >> 16) & 0xFF, G = (box.color >> 8) & 0xFF, B = box.color & 0xFF;
    static const float faceLight[6] = {0.78f, 0.78f, 1.0f, 0.45f, 0.68f, 0.68f};

    for (int f = 0; f < 6; ++f) {
        SV sv[4];
        bool ok = true;
        for (int k = 0; k < 4; ++k)
            sv[k] = project(corners[FACES[f][k]], cam.pos, cam, 0, 0,
                            box.shade * faceLight[f] * sun);
        for (int k = 0; k < 4; ++k) if (!sv[k].valid) ok = false;
        if (!ok) continue;
        // iki üçgen
        float lr = R / 255.f, lg = G / 255.f, lb = B / 255.f;
        auto flatTri = [&](const SV& A, const SV& Bv, const SV& C) {
            SV a = A, b = Bv, c = C;
            a.u = b.u = c.u = 0; a.v = b.v = c.v = 0;
            // düz renk için atlas yerine özel yol
            auto sx = [&](float ndc) { return (ndc * 0.5f + 0.5f) * fb.w; };
            auto sy = [&](float ndc) { return (1.f - (ndc * 0.5f + 0.5f)) * fb.h; };
            float x0 = sx(a.x), y0 = sy(a.y), x1 = sx(b.x), y1 = sy(b.y), x2 = sx(c.x), y2 = sy(c.y);
            int mnX = std::max(0, (int)std::floor(std::min({x0,x1,x2})));
            int mxX = std::min(fb.w-1, (int)std::ceil(std::max({x0,x1,x2})));
            int mnY = std::max(0, (int)std::floor(std::min({y0,y1,y2})));
            int mxY = std::min(fb.h-1, (int)std::ceil(std::max({y0,y1,y2})));
            float area = (x1-x0)*(y2-y0) - (x2-x0)*(y1-y0);
            if (std::fabs(area) < 1e-6f) return;
            float ia = 1.f/area;
            for (int py = mnY; py <= mxY; ++py)
                for (int px = mnX; px <= mxX; ++px) {
                    float fx = px+0.5f, fy = py+0.5f;
                    float w0 = ((x1-fx)*(y2-fy)-(x2-fx)*(y1-fy))*ia;
                    float w1v = ((x2-fx)*(y0-fy)-(x0-fx)*(y2-fy))*ia;
                    float w2 = 1.f-w0-w1v;
                    if (w0 < 0 || w1v < 0 || w2 < 0) continue;
                    float iw = w0*a.invW + w1v*b.invW + w2*c.invW;
                    if (iw <= 1e-9f) continue;
                    float depth = 1.f/iw;
                    size_t di = (size_t)py*fb.w+px;
                    if (depth >= fb.depth[di]) continue;
                    fb.depth[di] = depth;
                    float lt = (w0*a.light*a.invW + w1v*b.light*b.invW + w2*c.light*c.invW)/iw;
                    lt = 0.34f + lt * 0.66f;
                    float fg = (w0*a.fog*a.invW + w1v*b.fog*b.invW + w2*c.fog*c.invW)/iw;
                    float rr = lr*lt, gg = lg*lt, bb = lb*lt;
                    rr += (fogCol[0]-rr)*fg; gg += (fogCol[1]-gg)*fg; bb += (fogCol[2]-bb)*fg;
                    fb.put(px, py, rr, gg, bb);
                }
        };
        flatTri(sv[0], sv[1], sv[2]);
        flatTri(sv[0], sv[2], sv[3]);
    }
}

// ---------------- iskeleti küplere çevir ----------------
static void skeletonBoxes(const Skeleton& sk, const MobDef& def, const Vec3f& rootWorld,
                          float bodyYaw, std::vector<Box>& out, float hurtFlash) {
    for (int i = 0; i < sk.boneCount(); ++i) {
        const Bone& b = sk.bones()[i];
        if (b.name == "root") continue;
        // kemik dünya konumu (yerel zincir)
        Vec3f lp = sk.worldPos(i);
        // gövde yaw'ı ile döndür
        Vec3f rp = rotateEuler(lp, {0, bodyYaw, 0});
        Box bx;
        bx.c = rootWorld + rp;
        bx.half = {b.restScale.x * 0.5f, b.restScale.y * 0.5f, b.restScale.z * 0.5f};
        Vec3f wr = sk.worldRot(i);
        bx.rot = {wr.x, wr.y + bodyYaw, wr.z};
        bool head = (b.name == "head" || b.name == "jaw");
        bx.color = head ? def.accentColor : def.bodyColor;
        bx.shade = 1.f;
        if (hurtFlash > 0.f) {
            int R = (bx.color>>16)&0xFF, G=(bx.color>>8)&0xFF, B=bx.color&0xFF;
            R = (int)(R + (255-R)*hurtFlash);
            G = (int)(G + (60-G)*hurtFlash);
            B = (int)(B + (60-B)*hurtFlash);
            bx.color = (R<<16)|(G<<8)|B;
        }
        out.push_back(bx);
    }
}

int main(int argc, char** argv) {
    int frames = argc > 1 ? std::atoi(argv[1]) : 900;
    int fps = argc > 2 ? std::atoi(argv[2]) : 30;
    std::string outDir = argc > 3 ? argv[3] : "sim_frames";
    int W = argc > 4 ? std::atoi(argv[4]) : 640;
    int H = argc > 5 ? std::atoi(argv[5]) : 360;
    if (fps <= 0) fps = 30;

    std::printf("▸ Craftya simulasyonu: %d kare @ %d fps = %.1f saniye, %dx%d\n",
                frames, fps, float(frames) / fps, W, H);

    // ---- atlas ----
    Atlas atlasObj;
    atlasObj.generateProcedural();
    const Image& atlas = atlasObj.image();

    // ---- dünya ----
    World world(20261124u, true);
    Game game(20261124u, true);
    game.settings().renderChunks = 6;

    Framebuffer fb;
    fb.resize(W, H);

    auto t0 = std::chrono::high_resolution_clock::now();
    long meshQuads = 0;

    // 30 saniyelik kamera senaryosu
    // 0-8 sn: ormanda yürüyüş, 8-16: gün batımına bakış, 16-24: gece, 24-30: gündoğumu
    struct Key { float t; float yaw, pitch, yawRate; };
    (void)Key{};

    // mob yerleşimi: oyuncunun önünde bir çayır
    struct MobInst {
        uint16_t mobId;
        Vec3f pos;
        Skeleton sk;
        float yaw;
        float phase;
    };
    std::vector<MobInst> mobs;
    const uint16_t mobList[] = {MOB_COW, MOB_SHEEP, MOB_PIG, MOB_GOAT, MOB_CHICKEN, MOB_FROG, MOB_HORSE, MOB_WOLF};
    for (int i = 0; i < 8; ++i) {
        MobInst m;
        m.mobId = mobList[i];
        m.pos = {-4.5f + (i % 4) * 3.0f, 30.f, -5.5f - (i / 4) * 2.6f};
        m.yaw = 0.4f * i;
        m.phase = i * 0.7f;
        m.sk.build(mobDef(m.mobId));
        mobs.push_back(m);
    }
    // zemini bul (moblar otursun)
    for (auto& m : mobs) {
        int gx = (int)std::floor(m.pos.x), gz = (int)std::floor(m.pos.z);
        float h = world.heightAt(gx, gz);
        // en yüksek komşu zemin (blok üstü)
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                h = std::max(h, world.heightAt(gx + dx, gz + dz));
        m.pos.y = h + 1.3f;
    }

    // ---- ağaçlar (dünyayı zenginleştir) ----
    {
        uint32_t st = 9176u;
        auto rnd = [&]() { st = st * 1664525u + 1013904223u; return (st >> 16) & 0x7FFF; };
        for (int i = 0; i < 1400; ++i) {
            int tx = static_cast<int>(rnd() % 200) - 100;
            int tz = static_cast<int>(rnd() % 200) - 100;
            float h = world.heightAt(tx, tz);
            int base = static_cast<int>(h) + 1;
            if (base < 26) continue;               // suyun altına dikme
            if (world.getBlock(tx, base - 1, tz) != GRASS) continue;
            // mob sahnesinin etrafını boş bırak
            if (tx > -14 && tx < 12 && tz > -16 && tz < 10) continue;
            // kamera yolunu boş bırak
            if (tx > -8 && tx < 6 && tz > -6 && tz < 8) continue;
            int th = 4 + static_cast<int>(rnd() % 3);
            for (int y = 0; y < th; ++y)
                world.setBlock(tx, base + y, tz, OAK_LOG);
            for (int dy = th - 2; dy <= th + 1; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    for (int dz = -2; dz <= 2; ++dz) {
                        if (dx * dx + dz * dz > 5) continue;
                        if (dx == 0 && dz == 0 && dy < th) continue;
                        if (world.getBlock(tx + dx, base + dy, tz + dz) == AIR)
                            world.setBlock(tx + dx, base + dy, tz + dz, OAK_LEAVES);
                    }
        }
    }

    // ---- mesh önbelleği: bir kez üret, sonra sadece rasterize et ----
    struct ChunkMesh { ChunkPos pos; MeshData data; };
    std::vector<ChunkMesh> meshCache;
    {
        // kameranın dolaştığı alanı kapsayacak şekilde geniş yükle
        for (int cz = -12; cz <= 12; ++cz)
            for (int cx = -12; cx <= 12; ++cx)
                for (int cy = 0; cy <= 3; ++cy) {
                    ChunkPos cp{cx, cy, cz};
                    Chunk* c = world.getOrCreate(cp);
                    if (!c) continue;
                    if (!c->lightReady()) {
                        LightEngine::computeChunk(world, *c);
                        c->markLightReady();
                    }
                    ChunkMesh cm;
                    cm.pos = cp;
                    Mesher::build(world, *c, cm.data);
                    if (cm.data.quadCount > 0 || !cm.data.transparent.empty())
                        meshCache.push_back(std::move(cm));
                }
        std::printf("  mesh onbellegi: %zu chunk\n", meshCache.size());
    }

    char path[512];
    for (int f = 0; f < frames; ++f) {
        float t = static_cast<float>(f) / fps;
        float dt = 1.f / fps;

        // ---- kamera senaryosu: mob kümesinin (0,-5) etrafında dön ----
        // 0-9 sn: uzaktan yaklaş, 9-18: yakın orbit, 18-24: geniş manzara,
        // 24-30: gündoğumu için geri çekil
        const float penX = 0.f, penZ = -5.f;
        float radius, height, yawOff;
        if (t < 9.f) {
            float k = t / 9.f;
            radius = 13.f - k * 4.f;              // 13 → 9
            height = 2.2f + std::sin(k * 3.14159f) * 0.5f;
            yawOff = -0.75f + k * 0.45f;
        } else if (t < 18.f) {
            float k = (t - 9.f) / 9.f;
            radius = 9.f - k * 2.5f;               // 9 → 6.5
            height = 1.9f + std::sin(k * 6.28318f) * 0.35f;
            yawOff = -0.30f + k * 1.10f;           // orbit
        } else if (t < 24.f) {
            float k = (t - 18.f) / 6.f;
            radius = 6.5f + k * 9.f;               // 6.5 → 15.5
            height = 2.6f + k * 2.2f;              // yüksel
            yawOff = 0.80f + k * 0.55f;
        } else {
            float k = (t - 24.f) / 6.f;
            radius = 15.5f - k * 3.5f;
            height = 4.8f - k * 1.6f;
            yawOff = 1.35f + k * 0.50f;
        }

        float camX = penX + std::sin(yawOff) * radius;
        float camZ = penZ + std::cos(yawOff) * radius;
        // mob kümesine bak
        float yaw = std::atan2(penX - camX, -(penZ - camZ));
        float groundHere = world.heightAt((int)std::floor(camX), (int)std::floor(camZ));
        float groundPen  = world.heightAt((int)penX, (int)penZ);
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                groundHere = std::max(groundHere, world.heightAt((int)(camX + dx), (int)(camZ + dz)));
                groundPen  = std::max(groundPen,  world.heightAt((int)(penX + dx), (int)(penZ + dz)));
            }
        float camY = groundHere + height;
        float eyeY = groundPen + 1.5f;
        float pitch = std::atan2(eyeY - camY, radius) * 0.9f;

        Camera cam;
        cam.pos = {camX, camY, camZ};
        cam.yaw = yaw;
        cam.pitch = pitch;
        cam.fovDeg = 72.f;
        game.camera() = cam;

        // ---- mesh üret (motorun kendi mesher'ı + ışığı) ----
        fb.depth.assign(fb.depth.size(), 1e9f);

        // günün saati
        float hours = 8.f + t * 0.55f;    // 8:00 → 24.5 (gün batımı + gece)
        if (hours > 24.f) hours -= 24.f;
        float sun = LightEngine::sunFactor(hours);
        float sky[3];
        LightEngine::skyColor(hours, sky);
        // gökyüzü gradyanı: zenit biraz daha koyu
        float top[3] = {sky[0] * 0.78f, sky[1] * 0.84f, sky[2] * 1.05f};
        float bottom[3] = {sky[0], sky[1], sky[2]};

        Mat4 proj = Mat4::perspective(cam.fovDeg * 3.14159265f / 180.f,
                                      static_cast<float>(W) / H, 0.08f, 400.f);
        Mat4 view = cam.view();
        g_viewProj = proj * view;

        fb.clearSky(top, bottom);

        // güneş diski (gökyüzünde)
        if (sun > 0.15f) {
            Vec3f sunDir{std::cos(hours / 24.f * 6.2831853f - 1.5708f) * 0.4f,
                         std::sin(hours / 24.f * 6.2831853f - 1.5708f) * 0.7f + 0.35f,
                         0.6f};
            sunDir = sunDir.normalized();
            SV sp = project(cam.pos + sunDir * 120.f, cam.pos, cam, 0, 0, 1.f);
            if (sp.valid) {
                int sx = (int)((sp.x * 0.5f + 0.5f) * W);
                int sy = (int)((1.f - (sp.y * 0.5f + 0.5f)) * H);
                int rad = 14;
                for (int yy = -rad; yy <= rad; ++yy)
                    for (int xx = -rad; xx <= rad; ++xx) {
                        float d = std::sqrt(float(xx*xx + yy*yy));
                        if (d > rad * 0.55f) continue;
                        fb.put(sx + xx, sy + yy, 1.f, 0.96f, 0.82f);
                    }
            }
        }

        // ---- chunk'ları çiz (önbellekten) ----
        int drawnChunks = 0;
        for (const auto& cm : meshCache) {
            const ChunkPos& cp = cm.pos;
            // kaba mesafe eleme
            Vec3f cc{(cp.x + 0.5f) * 16.f, (cp.y + 0.5f) * 16.f, (cp.z + 0.5f) * 16.f};
            float d2 = (cc - cam.pos).dot(cc - cam.pos);
            if (d2 > (5.6f * 16.f) * (5.6f * 16.f)) continue;
            const MeshData& mesh = cm.data;

            for (size_t i = 0; i + 3 < mesh.opaque.size(); i += 4) {
                const Vertex* v = &mesh.opaque[i];
                SV p0 = project({cp.x*16.f + v[0].x, cp.y*16.f + v[0].y, cp.z*16.f + v[0].z},
                                cam.pos, cam, v[0].u, v[0].v, v[0].light);
                if (!p0.valid) continue;
                SV p1 = project({cp.x*16.f + v[1].x, cp.y*16.f + v[1].y, cp.z*16.f + v[1].z},
                                cam.pos, cam, v[1].u, v[1].v, v[1].light);
                SV p2 = project({cp.x*16.f + v[2].x, cp.y*16.f + v[2].y, cp.z*16.f + v[2].z},
                                cam.pos, cam, v[2].u, v[2].v, v[2].light);
                SV p3 = project({cp.x*16.f + v[3].x, cp.y*16.f + v[3].y, cp.z*16.f + v[3].z},
                                cam.pos, cam, v[3].u, v[3].v, v[3].light);
                drawTri(fb, p0, p1, p2, atlas, sky);
                drawTri(fb, p0, p2, p3, atlas, sky);
            }
            for (size_t i = 0; i + 3 < mesh.transparent.size(); i += 4) {
                const Vertex* v = &mesh.transparent[i];
                SV p0 = project({cp.x*16.f + v[0].x, cp.y*16.f + v[0].y, cp.z*16.f + v[0].z},
                                cam.pos, cam, v[0].u, v[0].v, v[0].light);
                if (!p0.valid) continue;
                SV p1 = project({cp.x*16.f + v[1].x, cp.y*16.f + v[1].y, cp.z*16.f + v[1].z},
                                cam.pos, cam, v[1].u, v[1].v, v[1].light);
                SV p2 = project({cp.x*16.f + v[2].x, cp.y*16.f + v[2].y, cp.z*16.f + v[2].z},
                                cam.pos, cam, v[2].u, v[2].v, v[2].light);
                SV p3 = project({cp.x*16.f + v[3].x, cp.y*16.f + v[3].y, cp.z*16.f + v[3].z},
                                cam.pos, cam, v[3].u, v[3].v, v[3].light);
                drawTri(fb, p0, p1, p2, atlas, sky);
                drawTri(fb, p0, p2, p3, atlas, sky);
            }
            ++drawnChunks;
        }

        // ---- moblar (iskelet animasyonu) ----
        std::vector<Box> boxes;
        for (auto& m : mobs) {
            const MobDef& def = mobDef(m.mobId);
            AnimContext ctx;
            ctx.rootPos = m.pos;
            ctx.speed = 1.2f + std::sin(t * 1.3f + m.phase) * 0.8f;
            ctx.onGround = true;
            ctx.lookTarget = cam.pos;
            ctx.healthPct = 1.f;
            m.sk.update(dt, ctx);
            m.yaw += dt * 0.15f * std::sin(t * 0.7f + m.phase);
            skeletonBoxes(m.sk, def, m.pos, m.yaw, boxes, 0.f);
            // gölge: basit elips
            int gx = (int)std::floor(m.pos.x), gz = (int)std::floor(m.pos.z);
            float h = world.heightAt(gx, gz) + 1.0f;
            (void)h;
        }
        // sıralama: kamera uzaklığına göre (arkadan öne)
        std::sort(boxes.begin(), boxes.end(), [&](const Box& A, const Box& B) {
            float da = (A.c - cam.pos).length();
            float db = (B.c - cam.pos).length();
            return da > db;
        });
        for (auto& b : boxes) drawBox(fb, b, cam, sky, sun);

        // ---- basit HUD ----
        // nişangah
        for (int i = -8; i <= 8; ++i) {
            fb.put(W/2 + i, H/2, 1, 1, 1);
            fb.put(W/2, H/2 + i, 1, 1, 1);
        }
        // FPS/MOB/CHUNK göstergesi (küçük renkli çubuklar)
        int barW = 60;
        for (int i = 0; i < barW; ++i) {
            float v = static_cast<float>(i) / barW;
            fb.put(10 + i, 10, 0.3f + v * 0.6f, 0.9f - v * 0.3f, 0.3f);
        }

        // ---- yaz ----
        snprintf(path, sizeof(path), "%s/frame_%04d.ppm", outDir.c_str(), f);
        FILE* fp = std::fopen(path, "wb");
        if (!fp) { std::fprintf(stderr, "yazilamadi: %s\n", path); return 1; }
        std::fprintf(fp, "P6\n%d %d\n255\n", W, H);
        std::vector<uint8_t> rgb(static_cast<size_t>(W) * H * 3);
        for (size_t i = 0; i < rgb.size(); ++i) {
            // hafif kontrast + doygunluk (dokular zaten sRGB, gamma UYGULANMAZ)
            float c = std::clamp(fb.color[i], 0.f, 1.f);
            c = c * 0.94f + 0.03f;
            rgb[i] = static_cast<uint8_t>(c * 255.f);
        }
        std::fwrite(rgb.data(), 1, rgb.size(), fp);
        std::fclose(fp);

        if (f % 60 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            float secs = std::chrono::duration<float>(now - t0).count();
            std::printf("  kare %4d/%d  t=%5.1fs  chunk=%d  mob=%zu  quad=%ld  (%.1f sn)\n",
                        f, frames, t, drawnChunks, mobs.size(), meshQuads, secs);
            std::fflush(stdout);
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    float total = std::chrono::duration<float>(t1 - t0).count();
    std::printf("✓ %d kare uretildi: %.1f saniye (%.2f ms/kare)\n",
                frames, total, total * 1000.f / frames);
    return 0;
}
