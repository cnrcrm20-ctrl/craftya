// Craftya Engine — greedy meshing uygulaması
#include "craftya/mesher.hpp"
#include "craftya/atlas.hpp"
#include "craftya/lighting.hpp"
#include <array>
#include <algorithm>

namespace craftya {

// 6 komşu yönü (ışık sızması için)
static constexpr int DX6[6] = {1, -1, 0, 0, 0, 0};
static constexpr int DY6[6] = {0, 0, 1, -1, 0, 0};
static constexpr int DZ6[6] = {0, 0, 0, 0, 1, -1};

namespace {

// Bir yüzün 4 köşe konumu (0..1 birim kare), düzleme göre
// uv de atlas döşeme koordinatı (0..1)
struct QuadVerts {
    Vec3f p[4];
};

// yön indeksine göre düzlem bazlı yerel köşeler
inline void faceCorners(int axis, int sign, int u, int v, int w,
                        float& outX, float& outY, float& outZ, int corner) {
    (void)axis; (void)sign; (void)u; (void)v; (void)w; (void)outX; (void)outY; (void)outZ; (void)corner;
}

} // namespace

void Mesher::build(World& world, Chunk& chunk, MeshData& out) {
    out.clear();
    ChunkPos cp = chunk.pos();

    // 3 eksen için mask: 16x16 dilim, yüz var mı + blok id
    // Standart greedy meshing: her eksen-yön için dilim taraması
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = 0; sign < 2; ++sign) {
            int d = sign == 0 ? 1 : -1;
            // iki eksen (u, v) axis'e dik
            int ua = (axis + 1) % 3;
            int va = (axis + 2) % 3;
            const int dims[3] = {CHUNK_W, CHUNK_H, CHUNK_W};
            int dimU = dims[ua], dimV = dims[va], dimD = dims[axis];

            struct Cell { bool present; BlockId id; };
            std::vector<Cell> mask(static_cast<size_t>(dimU) * dimV);

            for (int slice = 0; slice < dimD; ++slice) {
                // maske doldur
                for (int vi = 0; vi < dimV; ++vi) {
                    for (int ui = 0; ui < dimU; ++ui) {
                        int coord[3];
                        coord[axis] = slice;
                        coord[ua] = ui;
                        coord[va] = vi;
                        BlockId here = chunk.get(coord[0], coord[1], coord[2]);

                        int ncoord[3] = {coord[0], coord[1], coord[2]};
                        ncoord[axis] = slice + d;
                        BlockId neighbor;
                        if (ncoord[axis] < 0 || ncoord[axis] >= dims[axis]) {
                            // chunk dışı: dünyadan oku (komşu chunk)
                            int wx = cp.x * CHUNK_W + ncoord[0];
                            int wy = cp.y * CHUNK_H + ncoord[1];
                            int wz = cp.z * CHUNK_W + ncoord[2];
                            neighbor = world.getBlock(wx, wy, wz);
                        } else {
                            neighbor = chunk.get(ncoord[0], ncoord[1], ncoord[2]);
                        }

                        bool draw = false;
                        if (here != AIR) {
                            if (isOpaque(here)) {
                                // opak blok: yalnızca komşu opak değilse yüz çiz
                                draw = !isOpaque(neighbor);
                                if (here == neighbor) draw = false;
                            } else {
                                // şeffaf (su/cam/yaprak): komşu aynı tip ise çizme
                                draw = (neighbor != here) && !isOpaque(neighbor);
                            }
                        }
                        mask[static_cast<size_t>(vi) * dimU + ui] = {draw, here};
                    }
                }

                // greedy genişletme: dikdörtgen birleştir
                for (int vi = 0; vi < dimV; ++vi) {
                    for (int ui = 0; ui < dimU; ) {
                        Cell& c = mask[static_cast<size_t>(vi) * dimU + ui];
                        if (!c.present) { ++ui; continue; }
                        BlockId id = c.id;

                        // cutout bloklar (yaprak/bitki) BİRLEŞTİRİLMEZ:
                        // birleşince tek doku dev kareye geriliyor.
                        bool noMerge = (blockDef(id).style == kStyle_cross) ||
                                       (blockDef(id).style == kStyle_leaves);

                        // genişlik (u yönü)
                        int w = 1;
                        if (noMerge) { /* w=1 kalır */ }
                        while (!noMerge && ui + w < dimU) {
                            Cell& n = mask[static_cast<size_t>(vi) * dimU + ui + w];
                            if (!n.present || n.id != id) break;
                            ++w;
                        }
                        // yükseklik (v yönü)
                        int h = 1;
                        bool ok = true;
                        while (!noMerge && vi + h < dimV && ok) {
                            for (int k = 0; k < w; ++k) {
                                Cell& n = mask[static_cast<size_t>(vi + h) * dimU + ui + k];
                                if (!n.present || n.id != id) { ok = false; break; }
                            }
                            if (ok) ++h;
                        }

                        // bu dikdörtgeni yüz olarak üret
                        int coord[3];
                        coord[axis] = slice + (sign == 0 ? 1 : 0);
                        coord[ua] = ui;
                        coord[va] = vi;

                        // 4 köşe (dünya uzayında, chunk içi birim)
                        float base[3] = {
                            static_cast<float>(coord[0]),
                            static_cast<float>(coord[1]),
                            static_cast<float>(coord[2])
                        };

                        Vec3f n{0, 0, 0};
                        n.x = axis == 0 ? static_cast<float>(d) : 0.f;
                        n.y = axis == 1 ? static_cast<float>(d) : 0.f;
                        n.z = axis == 2 ? static_cast<float>(d) : 0.f;

                        float du[3] = {0, 0, 0};
                        du[ua] = static_cast<float>(w);
                        float dv[3] = {0, 0, 0};
                        dv[va] = static_cast<float>(h);

                        Vec3f p0{base[0], base[1], base[2]};
                        Vec3f p1{base[0] + du[0], base[1] + du[1], base[2] + du[2]};
                        Vec3f p3{base[0] + dv[0], base[1] + dv[1], base[2] + dv[2]};
                        Vec3f p2{base[0] + du[0] + dv[0],
                                 base[1] + du[1] + dv[1],
                                 base[2] + du[2] + dv[2]};

                        // yöne göre sargı düzeni (yüz normali dışa baksın)
                        Vec3f q[4];
                        if (n.x + n.y + n.z > 0) { q[0]=p0; q[1]=p1; q[2]=p2; q[3]=p3; }
                        else                     { q[0]=p1; q[1]=p0; q[2]=p3; q[3]=p2; }

                        // atlas UV: blok + yüz → gerçek kare (üst / yan / alt)
                        int faceSel = (axis == 1) ? (d > 0 ? 0 : 2) : 1;
                        int tileIdx = Atlas::tileForBlock(id, faceSel);
                        float aU0, aV0, aU1, aV1;
                        Atlas::tileUV(tileIdx, aU0, aV0, aU1, aV1);

                        float uv[4][2] = {
                            {aU0,       aV0},
                            {aU1,       aV0},
                            {aU1,       aV1},
                            {aU0,       aV1}
                        };

                        // Bitki örtüsü (cross) ve yapraklar KESKİN CUTOUT olmalı.
                        // Alfa karışımına bırakılırsa saydam pikseller siyah kalıyor
                        // ve zemin/leke görüntüsü oluşuyor.
                        bool cutout = (blockDef(id).style == kStyle_cross) ||
                                      (blockDef(id).style == kStyle_leaves);
                        bool transparentPass = !isOpaque(id) && !cutout;
                        auto& dst = transparentPass ? out.transparent : out.opaque;

                        // yön parlaklığı (ucuz yüz gölgeleme)
                        float faceShade = 1.0f;
                        if (axis == 1 && d > 0)      faceShade = 1.00f;   // üst
                        else if (axis == 1 && d < 0) faceShade = 0.45f;   // alt
                        else if (axis == 0)          faceShade = 0.78f;   // doğu/batı
                        else                         faceShade = 0.68f;   // kuzey/güney

                        // gerçek ışık: yüzün baktığı komşu hücrenin gökyüzü + blok ışığı
                        int lp[3] = {coord[0], coord[1], coord[2]};
                        lp[axis] += d;
                        int lwx = cp.x * CHUNK_W + lp[0];
                        int lwy = cp.y * CHUNK_H + lp[1];
                        int lwz = cp.z * CHUNK_W + lp[2];
                        float sky = static_cast<float>(world.skyLightAt(lwx, lwy, lwz)) / 15.f;
                        float blk = static_cast<float>(world.blockLightAt(lwx, lwy, lwz)) / 15.f;
                        // gündüz skylight baskın, gece blok ışığı baskın
                        float lightLevel = std::max(sky, blk);
                        // YANDAN AYDINLATMA: derin toprak/taş hücresi kapkaranlık olur.
                        // Bu yüzler yandan bakıldığında (kamera alçakken) ekranın
                        // büyük bölümünü kaplar → güneşe açık komşulardan ışık sızdır.
                        if (lightLevel < 0.55f) {
                            for (int dd = 0; dd < 6; ++dd) {
                                if (dd == axis) continue;              // bakan yön zaten ölçüldü
                                float nsky = static_cast<float>(world.skyLightAt(
                                    lwx + DX6[dd], lwy + DY6[dd], lwz + DZ6[dd])) / 15.f;
                                // yandan gelen ışık sönümlenir (2 hücre uzaklık etkisi)
                                nsky *= 0.62f;
                                if (nsky > lightLevel) lightLevel = nsky;
                            }
                        }
                        // GAMA: ham ışık doğrusal; göz ve ekran için üsse çevir.
                        // Bu olmadan yarık/mağara duvarları kapkara kalıyordu.
                        lightLevel = std::pow(std::clamp(lightLevel, 0.f, 1.f), 0.62f);
                        // Taban aydınlık: meşalesiz derin yerler ekranda delik gibi
                        // görünmesin. (Yüz gölgelemesi bunun üstüne biner.)
                        lightLevel = 0.40f + lightLevel * 0.60f;
                        if (id == GLOWSTONE || id == LAVA || id == TORCH) lightLevel = 1.f;

                        float light = lightLevel * faceShade;

                        // köşe AO: her köşe için yan/çapraz komşulara bak
                        auto solidAt = [&](int ox, int oy, int oz) {
                            return isOpaque(world.getBlock(
                                cp.x * CHUNK_W + coord[0] + ox + n.x,
                                cp.y * CHUNK_H + coord[1] + oy + n.y,
                                cp.z * CHUNK_W + coord[2] + oz + n.z));
                        };
                        int cor[4][3];
                        for (int k = 0; k < 4; ++k) {
                            // köşe ofsetleri: (u,v) yönünde 0/1 → -1/+1
                            int ou = ((k == 1 || k == 2) ? 1 : -1);
                            int ov = ((k == 2 || k == 3) ? 1 : -1);
                            cor[k][0] = ou * (ua == 0 ? 1 : 0) + ov * (va == 0 ? 1 : 0);
                            cor[k][1] = ou * (ua == 1 ? 1 : 0) + ov * (va == 1 ? 1 : 0);
                            cor[k][2] = ou * (ua == 2 ? 1 : 0) + ov * (va == 2 ? 1 : 0);
                        }

                        for (int k = 0; k < 4; ++k) {
                            bool s1 = solidAt(cor[k][0], cor[k][1], cor[k][2]);
                            // çapraz komşu (iki eksen birleşimi) — kaba yaklaşım
                            bool s2 = solidAt(cor[k][0] / 2, cor[k][1] / 2, cor[k][2] / 2);
                            bool cr = solidAt(cor[k][0], cor[k][1], cor[k][2]) &&
                                      solidAt(cor[k][0] / 2, cor[k][1] / 2, cor[k][2] / 2);
                            // AO köşe karartması. Taban ışıkla çarpıldığında
                            // kapalı köşeler (0.55) siyaha yaklaşıyordu; tabanı yükselt.
                            float ao = AmbientOcclusion::vertexFactor(s1, s2, cr);
                            ao = 0.78f + ao * 0.22f;   // yumuşat: 0.78..1.00

                            Vertex vtx{};
                            vtx.x = q[k].x; vtx.y = q[k].y; vtx.z = q[k].z;
                            vtx.nx = n.x; vtx.ny = n.y; vtx.nz = n.z;
                            vtx.u = uv[k][0]; vtx.v = uv[k][1];
                            vtx.light = std::clamp(light * ao, 0.02f, 1.f);
                            dst.push_back(vtx);
                        }
                        out.quadCount++;

                        // maskeden temizle
                        for (int hh = 0; hh < h; ++hh)
                            for (int ww = 0; ww < w; ++ww)
                                mask[static_cast<size_t>(vi + hh) * dimU + ui + ww].present = false;

                        ui += w;
                    }
                }
            }
        }
    }
    out.opaque.shrink_to_fit();
    out.transparent.shrink_to_fit();
}

} // namespace craftya
