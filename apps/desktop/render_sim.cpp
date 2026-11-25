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
#include "craftya/ecosystem.hpp"
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

static constexpr uint64_t SEED = 20261124u;

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
// Kamera uzayında tutulan vertex: yakın düzlem kırpması burada yapılır.
struct CVert {
    float cx, cy, cz;   // kamera uzayı konum
    float w;            // view uzayı derinliği (projeksiyonun w'si)
    float u, v, light, fog;
    bool valid = false;
};

struct SV {
    float x, y;         // NDC
    float invW;         // 1/w
    float viewDepth;    // w
    float u, v;
    float light;
    float fog;
    bool valid = false;
};

// yakın düzlem: bu derinliğin önündeki parçalar kırpılır
static constexpr float kNearZ = 0.08f;

static Mat4 g_viewProj;

// dünya → kamera uzayı (kırpma öncesi)
static CVert camVert(const Vec3f& p, const Vec3f& camPos,
                     float u, float v, float light) {
    CVert o;
    Mat4 vp = g_viewProj;
    float cx = p.x, cy = p.y, cz = p.z;
    o.cx = vp.m[0]*cx + vp.m[4]*cy + vp.m[8]*cz  + vp.m[12];
    o.cy = vp.m[1]*cx + vp.m[5]*cy + vp.m[9]*cz  + vp.m[13];
    o.cz = vp.m[2]*cx + vp.m[6]*cy + vp.m[10]*cz + vp.m[14];
    o.w  = vp.m[3]*cx + vp.m[7]*cy + vp.m[11]*cz + vp.m[15];
    o.u = u; o.v = v; o.light = light;
    float dx = cx - camPos.x, dy = cy - camPos.y, dz = cz - camPos.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    const float fogStart = FOG_START_CHUNK * CHUNK_W;
    const float fogEnd = FOG_END_CHUNK * CHUNK_W;
    o.fog = std::clamp((dist - fogStart) / (fogEnd - fogStart), 0.f, 1.f);
    o.valid = true;
    return o;
}

// kamera uzayı → ekran
static SV toScreen(const CVert& c) {
    SV o;
    if (c.w < kNearZ) return o;
    float invW = 1.f / c.w;
    o.x = c.cx * invW;
    o.y = c.cy * invW;
    o.invW = invW;
    o.viewDepth = c.w;
    o.u = c.u; o.v = c.v; o.light = c.light; o.fog = c.fog;
    o.valid = true;
    return o;
}

static SV project(const Vec3f& p, const Vec3f& camPos, const Camera& cam,
                  float u, float v, float light) {
    (void)cam;
    return toScreen(camVert(p, camPos, u, v, light));
}

// ---------------- triangle raster ----------------

// ---- atlas örnekleme: çift-doğrusal, 16px karo sınırlarına kilitli ----
static constexpr int   TILE_PX    = 16;      // atlas karosu 16x16 piksel
static inline float    sampleClamp(float t, float lo, float hi) {
    return t < lo ? lo : (t > hi ? hi : t);
}

static void sampleAtlas(const Image& atlas, float u, float v,
                        float& r, float& g, float& b, float& a) {
    // 16x16 piksel sanatı → NEAREST (çift-doğrusal dokuyu bulanıklaştırır)
    // ama karo sınırına kilitli: komşu karodan renk sızmasın
    float fu = u * atlas.width;
    float fv = v * atlas.height;
    int tx = static_cast<int>(std::floor(fu));
    int ty = static_cast<int>(std::floor(fv));
    // karo içine kilitle
    int maxX = atlas.width  - 1;
    int maxY = atlas.height - 1;
    tx = std::clamp(tx, 0, maxX);
    ty = std::clamp(ty, 0, maxY);
    size_t i = (static_cast<size_t>(ty) * atlas.width + tx) * 4;
    r = atlas.pixels[i]     / 255.f;
    g = atlas.pixels[i + 1] / 255.f;
    b = atlas.pixels[i + 2] / 255.f;
    a = atlas.pixels[i + 3] / 255.f;
}

static bool g_noDepth = false;
static float g_sunScale = 1.f;   // güneş çarpanı (motorun shader'ındaki gibi)

static void drawTri(Framebuffer& fb, const SV& a, const SV& b, const SV& c,
                    const Image& atlas, const float fogCol[3], bool wire = false) {
    const float sunScale = g_sunScale;
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
            if (!g_noDepth && depth >= fb.depth[di]) continue;

            float uu = (w0 * a.u * a.invW + w1 * b.u * b.invW + w2 * c.u * c.invW) / iw;
            float vv = (w0 * a.v * a.invW + w1 * b.v * b.invW + w2 * c.v * c.invW) / iw;
            float lt = (w0 * a.light * a.invW + w1 * b.light * b.invW +
                        w2 * c.light * c.invW) / iw;
            float fg = (w0 * a.fog * a.invW + w1 * b.fog * b.invW + w2 * c.fog * c.invW) / iw;

            // atlas örnekle: çift-doğrusal, KARO SINIRLARINA KİLİTLİ
            // (kilit olmazsa komşu karo renkleri sızar)
            float r, g, bl, alpha;
            sampleAtlas(atlas, uu, vv, r, g, bl, alpha);
            // cutout: saydam pikseller ne renk ne derinlik yazar (yaprak delikleri)
            if (alpha < 0.5f) continue;
            fb.depth[di] = depth;

            // motorun gölgelendiricisiyle AYNI: koyu yüzleri tabanla EZME,
            // sadece gece/gündüz güneş çarpanıyla ölçekle
            // NOT: lt zaten mesher'dan geliyor (skyLight * faceShade * AO).
            // Burada ikinci kez güneş/normal çarpmak görüntüyü karartıyordu.
            lt = getenv("SIM_FORCELIGHT") ? 1.0f : (0.05f + lt * 0.95f);
            if (getenv("SIM_MARKDARK") && lt < 0.35f) {
                // teshis: karanlik pikselleri KIRMIZI isaretle
                fb.putA(px, py, 1.f, 0.f, 0.f, 1.f);
                fb.depth[di] = depth;
                continue;
            }
            r *= lt; g *= lt; bl *= lt;
            // sis karışımı
            r = r + (fogCol[0] - r) * fg;
            g = g + (fogCol[1] - g) * fg;
            bl = bl + (fogCol[2] - bl) * fg;

            if (getenv("SIM_DEBUGPIX") && px == 400 && py == 440) {
                float dxs = uu * 1024.f - 73.f, dys = vv * 1024.f - 11.f;
                std::printf("[px400,440] u=%.5f v=%.5f | atlas piksel=%.2f,%.2f | "
                            "lt=%.3f ton=%.1f depth=%.1f\n",
                            uu, vv, uu * 1024.f, vv * 1024.f,
                            lt, r * 10.f,
                            depth);
                int ax = (int)(uu * 1024.f), ay = (int)(vv * 1024.f);
                if (ax >= 0 && ay >= 0 && ax < 1024 && ay < 1024) {
                    size_t i2 = ((size_t)ay * atlas.width + ax) * 4;
                    std::printf("   atlas RGBA = %d,%d,%d,%d\n",
                                atlas.pixels[i2], atlas.pixels[i2+1],
                                atlas.pixels[i2+2], atlas.pixels[i2+3]);
                }
                (void)dxs; (void)dys;
            }
            // cutout: alpha>=0.5 pikseller TAM OPAK yazılır.
            // Karışım yapılırsa yaprak kenarları siyah zeminle karışıp leke oluyor.
            fb.putA(px, py, r, g, bl, 1.f);
        }
    }
}

// ---------------- yakın düzlem kırpma ----------------
// Kamera arkasında kalan köşe yüzünden üçgenin TAMAMEN kaybolmasını engeller.
// (Eskiden tek bir vertex w<0.05 ise kuad tamamen atılıyordu → kameranın
//  altındaki büyük zemin yüzleri siyah kalıyordu.)
struct CVList { CVert v[5]; int n = 0; };

static inline CVert lerpCV(const CVert& a, const CVert& b, float t) {
    CVert o;
    o.cx = a.cx + (b.cx - a.cx) * t;
    o.cy = a.cy + (b.cy - a.cy) * t;
    o.cz = a.cz + (b.cz - a.cz) * t;
    o.w  = a.w  + (b.w  - a.w)  * t;
    o.u  = a.u  + (b.u  - a.u)  * t;
    o.v  = a.v  + (b.v  - a.v)  * t;
    o.light = a.light + (b.light - a.light) * t;
    o.fog   = a.fog   + (b.fog   - a.fog)   * t;
    o.valid = true;
    return o;
}

// Sutherland–Hodgman: tek üçgeni yakın düzleme göre kırp (0..2 üçgen çıkabilir)
static int clipNear(const CVert& a, const CVert& b, const CVert& c,
                    CVert& o0, CVert& o1, CVert& o2,
                    CVert& o3, CVert& o4, CVert& o5) {
    CVert in[3] = {a, b, c};
    CVert out[5];
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        const CVert& cur = in[i];
        const CVert& nxt = in[(i + 1) % 3];
        bool curIn = cur.w >= kNearZ;
        bool nxtIn = nxt.w >= kNearZ;
        if (curIn) out[n++] = cur;
        if (curIn != nxtIn) {
            float t = (kNearZ - cur.w) / (nxt.w - cur.w);
            out[n++] = lerpCV(cur, nxt, t);
        }
    }
    if (n < 3) return 0;
    o0 = out[0]; o1 = out[1]; o2 = out[2];
    if (n == 4) { o3 = out[0]; o4 = out[2]; o5 = out[3]; return 2; }
    return 1;
}

// kuadı iki üçgene bölerek kırpıp çiz
static void drawQuadClipped(Framebuffer& fb, const CVert& a, const CVert& b,
                            const CVert& c, const CVert& d,
                            const Image& atlas, const float fogCol[3], bool wire = false) {
    CVert t0, t1, t2, t3, t4, t5;
    int n1 = clipNear(a, b, c, t0, t1, t2, t3, t4, t5);
    if (n1 >= 1) drawTri(fb, toScreen(t0), toScreen(t1), toScreen(t2), atlas, fogCol, wire);
    if (n1 == 2) drawTri(fb, toScreen(t3), toScreen(t4), toScreen(t5), atlas, fogCol, wire);
    int n2 = clipNear(a, c, d, t0, t1, t2, t3, t4, t5);
    if (n2 >= 1) drawTri(fb, toScreen(t0), toScreen(t1), toScreen(t2), atlas, fogCol, wire);
    if (n2 == 2) drawTri(fb, toScreen(t3), toScreen(t4), toScreen(t5), atlas, fogCol, wire);
}

// ---------------- mob küpü çiz ----------------
struct Box {
    Vec3f c;        // merkez
    Vec3f half;     // yarı boyut
    Vec3f rot;      // radyan
    uint32_t color;
    float shade;
    int texKind = 0;   // 0=düz renk  1=mob derisi  2=mob yünü  3=tavuk tüyü
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


// ---- prosedürel mob dokusu: renk çeşitlemesi + tüy/yün gürültüsü ----
// deterministik hash → döşeme yönünde tekrar eder, böylece küp yüzleri tutarlı
static inline float mobHash(int x, int y, int seed) {
    int h = x * 374761393 + y * 668265263 + seed * 2147483647;
    h = (h ^ (h >> 13)) * 1274126177;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.f;
}

static void mobTexel(uint32_t base, int kind, int x, int y, float& r, float& g, float& b) {
    int R = (base >> 16) & 0xFF, G = (base >> 8) & 0xFF, B = base & 0xFF;
    float n = mobHash(x, y, kind * 31 + 7);
    float mul = 1.f;
    if (kind == 1) {              // deri: ince benek
        mul = 0.86f + n * 0.24f;
    } else if (kind == 2) {       // yün: kaba kıvırcık
        float k = mobHash(x / 2, y / 2, 13);
        mul = 0.80f + k * 0.34f;
    } else if (kind == 3) {       // tüy: yatay çizgiler
        float k = mobHash(x, y * 3, 29);
        mul = (y % 3 == 0) ? 0.78f + k * 0.10f : 0.94f + k * 0.14f;
    }
    r = std::clamp(R / 255.f * mul, 0.f, 1.f);
    g = std::clamp(G / 255.f * mul, 0.f, 1.f);
    b = std::clamp(B / 255.f * mul, 0.f, 1.f);
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
        // yüz düzleminde uv: köşe sırasına göre 0/1
        static const float FUV[4][2] = {{0,0},{1,0},{1,1},{0,1}};
        for (int k = 0; k < 4; ++k)
            sv[k] = project(corners[FACES[f][k]], cam.pos, cam, FUV[k][0], FUV[k][1],
                            box.shade * faceLight[f] * sun);
        for (int k = 0; k < 4; ++k) if (!sv[k].valid) ok = false;
        if (!ok) continue;
        // iki üçgen
        float lr = R / 255.f, lg = G / 255.f, lb = B / 255.f;
        auto flatTri = [&](const SV& A, const SV& Bv, const SV& C) {
            SV a = A, b = Bv, c = C;

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
                    lt = getenv("SIM_FORCELIGHT") ? 1.0f : (0.05f + lt * 0.95f);
            if (getenv("SIM_MARKDARK") && lt < 0.35f) {
                // teshis: karanlik pikselleri KIRMIZI isaretle
                fb.putA(px, py, 1.f, 0.f, 0.f, 1.f);
                fb.depth[di] = depth;
                continue;
            }
                    float fg = (w0*a.fog*a.invW + w1v*b.fog*b.invW + w2*c.fog*c.invW)/iw;
                    float rr = lr, gg = lg, bb = lb;
                    if (box.texKind) {
                        // yüz düzleminde doku koordinatı (dünya uzayında sabit)
                        float du = (w0*a.u*a.invW + w1v*b.u*b.invW + w2*c.u*c.invW)/iw;
                        float dv = (w0*a.v*a.invW + w1v*b.v*b.invW + w2*c.v*c.invW)/iw;
                        int px8 = (int)std::floor(du * 8.f);
                        int py8 = (int)std::floor(dv * 8.f);
                        mobTexel(box.color, box.texKind, px8, py8, rr, gg, bb);
                    }
                    rr *= lt; gg *= lt; bb *= lt;
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
        // türe göre doku karakteri
        if (std::strstr(def.name, "sheep") != nullptr)      bx.texKind = 2;
        else if (std::strstr(def.name, "chicken") != nullptr) bx.texKind = 3;
        else if (std::strstr(def.name, "fish") != nullptr)    bx.texKind = 1;
        else                                                    bx.texKind = 1;
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
    World world(SEED, true);
    Game game(SEED, true);
    game.settings().renderChunks = 6;

    // spawn: deniz seviyesinin üstünde, düz, kara bir nokta bul (spiral tarama)
    float g_penX = 0.f, g_penZ = 0.f;
    {
        float bestScore = -1e9f;
        for (int r = 0; r < 220; r += 4) {
            for (int a = 0; a < 24; ++a) {
                float ang = a * 6.28318f / 24.f;
                int x = static_cast<int>(std::cos(ang) * r);
                int z = static_cast<int>(std::sin(ang) * r);
                auto col = world.generator().column(x, z);
                if (col.underwater) continue;
                if (col.height < 66 || col.height > 96) continue;
                if (col.biome == worldgen::Biome::Ocean) continue;
                // düzlük ölç: çevredeki yükseklik farkı
                float hmin = 1e9f, hmax = -1e9f;
                for (int dz = -10; dz <= 10; dz += 2)
                    for (int dx = -10; dx <= 10; dx += 2) {
                        float h = world.heightAt(x + dx, z + dz);
                        hmin = std::min(hmin, h); hmax = std::max(hmax, h);
                    }
                // 21x21 alanda 3 bloktan fazla düşüş varsa orada yarık/uçurum var
                float flat = -(hmax - hmin) * 3.f;
                float score = flat * 4.f - static_cast<float>(r) * 0.03f;
                if (hmax - hmin > 3.f) score -= 70.f;   // yarık / uçurum: cezalandır
                // yakında su/manzara olması bonus (görsel zenginlik)
                if (col.biome == worldgen::Biome::Plains) score += 10.f;
                if (col.biome == worldgen::Biome::Forest) score += 4.f;
                if (score > bestScore) { bestScore = score; g_penX = (float)x; g_penZ = (float)z; }
            }
        }
        std::printf("  spawn: (%.0f, %.0f)  biyom=%s  yukseklik=%.0f\n",
                    g_penX, g_penZ, worldgen::biomeName(world.biomeAt((int)g_penX, (int)g_penZ)),
                    world.heightAt((int)g_penX, (int)g_penZ));
    }

    Framebuffer fb;
    fb.resize(W, H);

    auto t0 = std::chrono::high_resolution_clock::now();
    long meshQuads = 0;

    // 30 saniyelik kamera senaryosu
    // 0-8 sn: ormanda yürüyüş, 8-16: gün batımına bakış, 16-24: gece, 24-30: gündoğumu
    struct Key { float t; float yaw, pitch, yawRate; };
    (void)Key{};

    // ekosistem: gerçek canlı simülasyonu (biyoma uygun doğum, sürü, otlama)
    Ecosystem eco;
    eco.init(SEED ^ 0xEC0ull);

    // ağaçlar artık dünya üreticisinden geliyor (bkz. worldgen.hpp)

    // ---- mesh önbelleği: bir kez üret, sonra sadece rasterize et ----
    struct ChunkMesh { ChunkPos pos; MeshData data; };
    std::vector<ChunkMesh> meshCache;
    {
        // SPAWN çevresini kapsayacak şekilde yükle (sabit 0,0 değil!)
        int scx = static_cast<int>(std::floor(g_penX / CHUNK_W));
        int scz = static_cast<int>(std::floor(g_penZ / CHUNK_W));
        // 1) TÜM chunk'ları oluştur (ışık komşuya yayılabilsin)
        for (int cz = scz - 12; cz <= scz + 12; ++cz)
            for (int cx = scx - 12; cx <= scx + 12; ++cx)
                for (int cy = 0; cy <= 6; ++cy)
                    world.getOrCreate({cx, cy, cz});

        // 2) BÖLGESEL ışık: tek BFS ile 25x25 chunk aydınlatılır.
        //    Chunk başına ayrı hesap yapılırsa ışık sınırı geçemez ve
        //    yüzey/ağaç altı kapkara kalır (asıl siyah alan hatası buydu).
        int lit = LightEngine::computeRegion(world, {scx, 3, scz}, 12, 3, 12);
        std::printf("  isik: %d chunk aydinlatildi\n", lit);

        // 3) mesh'leri üret
        for (int cz = scz - 12; cz <= scz + 12; ++cz)
            for (int cx = scx - 12; cx <= scx + 12; ++cx)
                for (int cy = 0; cy <= 6; ++cy) {
                    ChunkPos cp{cx, cy, cz};
                    Chunk* c = world.chunkAt(cp);
                    if (!c) continue;
                    ChunkMesh cm;
                    cm.pos = cp;
                    Mesher::build(world, *c, cm.data);
                    if (cm.data.quadCount > 0 || !cm.data.transparent.empty())
                        meshCache.push_back(std::move(cm));
                }
        std::printf("  mesh onbellegi: %zu chunk\n", meshCache.size());
    }

    // ekosistemi ısıt: ilk karelerde moblar doğsun
    for (int i = 0; i < 90; ++i) eco.update(0.1f, world, Vec3f{g_penX, 74.f, g_penZ}, 10.f, true);
    

    char path[512];
    for (int f = 0; f < frames; ++f) {
        float t = static_cast<float>(f) / fps;
        float dt = 1.f / fps;

        // ---- kamera senaryosu: mob kümesinin (0,-5) etrafında dön ----
        // 0-9 sn: uzaktan yaklaş, 9-18: yakın orbit, 18-24: geniş manzara,
        // 24-30: gündoğumu için geri çekil
        const float penX = g_penX, penZ = g_penZ;
        float radius, height, yawOff;
        if (t < 9.f) {
            float k = t / 9.f;
            radius = 13.f - k * 4.f;              // 13 → 9
            height = 3.4f + std::sin(k * 3.14159f) * 0.8f;
            yawOff = -0.75f + k * 0.45f;
        } else if (t < 18.f) {
            float k = (t - 9.f) / 9.f;
            radius = 9.f - k * 2.5f;               // 9 → 6.5
            height = 2.8f + std::sin(k * 6.28318f) * 0.5f;
            yawOff = -0.30f + k * 1.10f;           // orbit
        } else if (t < 24.f) {
            float k = (t - 18.f) / 6.f;
            radius = 6.5f + k * 9.f;               // 6.5 → 15.5
            height = 3.2f + k * 3.0f;              // yüksel
            yawOff = 0.80f + k * 0.55f;
        } else {
            float k = (t - 24.f) / 6.f;
            radius = 15.5f - k * 3.5f;
            height = 6.2f - k * 2.0f;
            yawOff = 1.35f + k * 0.50f;
        }

        float camX = penX + std::sin(yawOff) * radius;
        float camZ = penZ + std::cos(yawOff) * radius;
        // mob kümesine bak
        float yaw = std::atan2(penX - camX, -(penZ - camZ));
        // GERÇEK blok yüksekliği (heightAt sadece üretici tahmini — ağaç/su dahil değil)
        auto solidTop = [&](int bx, int bz) {
            for (int y = 190; y > 1; --y) {
                BlockId b = world.getBlock(bx, y, bz);
                if (b != AIR && !isLiquid(b)) return y;
            }
            return 1;
        };
        float groundHere = static_cast<float>(solidTop((int)std::floor(camX), (int)std::floor(camZ)));
        float groundPen  = static_cast<float>(solidTop((int)penX, (int)penZ));
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                groundHere = std::max(groundHere, static_cast<float>(solidTop((int)(camX + dx), (int)(camZ + dz))));
                groundPen  = std::max(groundPen,  static_cast<float>(solidTop((int)(penX + dx), (int)(penZ + dz))));
            }
        float camY = groundHere + height;
        float eyeY = groundPen + 1.5f;
        // hedefe bak: mob kümesi + hafif aşağı (gökyüzüne bakma)
        float pitch = std::atan2(eyeY - camY, radius) * 0.55f - 0.10f;

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
        g_sunScale = 0.42f + 0.58f * sun;   // gece de tamamen karanlik olmasin
        float sky[3];
        LightEngine::skyColor(hours, sky);
        // gökyüzü gradyanı: zenit biraz daha koyu
        float top[3] = {sky[0] * 0.78f, sky[1] * 0.84f, sky[2] * 1.05f};
        float bottom[3] = {sky[0], sky[1], sky[2]};

        if (f == 0) {
            int bx = (int)camX, bz = (int)camZ;
            std::printf("    [dbg] cam=(%.1f,%.1f,%.1f) pitch=%.2f | blok@cam=%s | ust=%s | alt=%s\n",
                camX, camY, camZ, pitch,
                blockDef(world.getBlock(bx,(int)camY,bz)).key,
                blockDef(world.getBlock(bx,(int)camY+2,bz)).key,
                blockDef(world.getBlock(bx,(int)camY-2,bz)).key);
        }
        Mat4 proj = Mat4::perspective(cam.fovDeg * 3.14159265f / 180.f,
                                      static_cast<float>(W) / H, 0.08f, 400.f);
        Mat4 view = cam.view();
        g_viewProj = proj * view;

        fb.clearSky(top, bottom);
        g_noDepth = getenv("SIM_NODEPTH") != nullptr;

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

            // kuad → 4 kamera-uzayı köşesi → yakın düzlem kırpması → çiz
            auto quad = [&](const Vertex* v, bool wire) {
                CVert q[4];
                bool front = false;
                for (int k = 0; k < 4; ++k)
                    q[k] = camVert({cp.x*16.f + v[k].x, cp.y*16.f + v[k].y,
                                    cp.z*16.f + v[k].z},
                                   cam.pos, v[k].u, v[k].v, v[k].light);
                for (int k = 0; k < 4; ++k) if (q[k].w >= kNearZ) { front = true; break; }
                if (!front) return;      // kuadın tamamı kameranın arkasında
                drawQuadClipped(fb, q[0], q[1], q[2], q[3], atlas, sky, wire);
            };

            for (size_t i = 0; i + 3 < mesh.opaque.size(); i += 4)
                quad(&mesh.opaque[i], false);
            for (size_t i = 0; i + 3 < mesh.transparent.size(); i += 4)
                quad(&mesh.transparent[i], false);
            ++drawnChunks;
        }

        // ---- moblar: GERÇEK ekosistem (canlı davranış + iskelet animasyonu) ----
        std::vector<Box> boxes;
        eco.update(dt, world, cam.pos, 10.f + t * 0.4f, true);
        for (const auto& m : eco.mobs()) {
            const MobDef& def = mobDef(m.type);
            // kameradan çok uzaktakileri çizme
            float dist2 = (m.pos - cam.pos).length();
            if (dist2 > 110.f) continue;
            skeletonBoxes(m.sk, def, m.pos, m.yaw, boxes, m.hurtFlash);
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
                        f, frames, t, drawnChunks, eco.count(), meshQuads, secs);
            std::fflush(stdout);
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    float total = std::chrono::duration<float>(t1 - t0).count();
    std::printf("✓ %d kare uretildi: %.1f saniye (%.2f ms/kare)\n",
                frames, total, total * 1000.f / frames);
    return 0;
}
