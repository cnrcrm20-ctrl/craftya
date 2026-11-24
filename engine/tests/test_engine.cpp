// Craftya Engine — çekirdek testler (derleyici + mantık doğrulaması)
#include "craftya/core.hpp"
#include "craftya/world.hpp"
#include "craftya/mesher.hpp"
#include "craftya/physics.hpp"
#include <cstdio>
#include <chrono>

using namespace craftya;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  ❌ %s\n", msg); ++failures; } \
    else { std::printf("  ✅ %s\n", msg); } \
} while(0)

int main() {
    std::printf("\n=== 1) Blok kayıt defteri ===\n");
    CHECK(blockRegistry().size() == BLOCK_COUNT, "31 blok tanımlı");
    CHECK(!isOpaque(AIR), "hava opak değil");
    CHECK(isOpaque(STONE), "taş opak");
    CHECK(!isOpaque(GLASS), "cam opak değil");
    CHECK(!isSolid(WATER), "su katı değil");
    CHECK(blockLight(GLOWSTONE) == 15, "ışık taşı 15 ışık verir");

    std::printf("\n=== 2) Chunk palet sıkıştırma ===\n");
    {
        Chunk c({0,0,0});
        c.set(0,0,0, STONE);
        c.set(1,0,0, DIRT);
        c.set(2,0,0, STONE);
        CHECK(c.get(0,0,0) == STONE, "blok yaz/oku taş");
        CHECK(c.get(1,0,0) == DIRT, "blok yaz/oku toprak");
        CHECK(c.paletteSize() == 3, "palet 3 tip (hava, taş, toprak)");
        CHECK(c.packed5(), "palet <=32 → 5 bit paketli");
        CHECK(c.bytes() == 2560, "5 bit/blok → 2560 bayt (2.5 KB)");

        // paleti taşır: 33+ farklı tip → 8 bite geçmeli (kayıt defteri dışı id'lerle)
        for (int i = 0; i < 40; ++i)
            c.set(i % 16, 1 + i / 16, 0, static_cast<BlockId>(100 + i));
        CHECK(!c.packed5(), "palet >32 → 8 bite yükseltildi");
        CHECK(c.bytes() == CHUNK_VOL, "8 bit/blok → 4 KB");
    }

    std::printf("\n=== 3) Dünya üretimi ===\n");
    {
        World w(424242);
        int baseH = static_cast<int>(w.heightAt(0, 0));
        int cy = baseH / CHUNK_H;
        Chunk* c0 = w.getOrCreate({0, 0, 0});
        Chunk* c = w.getOrCreate({0, cy, 0});
        CHECK(c0 != nullptr && c != nullptr, "chunk üretildi");
        CHECK(w.chunkCount() == 2, "2 chunk bellekte");

        // yüzeyde çim olmalı
        BlockId surface = c->get(0, baseH - cy * CHUNK_H, 0);
        CHECK(surface == GRASS, "yüzey çim bloğu");
        CHECK(c0->get(0, 0, 0) == BEDROCK, "en alt katman ana kaya");
        // deniz seviyesi altında su
        int seaY = 26;
        if (baseH < seaY) {
            int sy = seaY - (seaY / CHUNK_H) * CHUNK_H;
            Chunk* cs = w.getOrCreate({0, seaY / CHUNK_H, 0});
            CHECK(cs->get(0, sy, 0) == WATER, "deniz seviyesinde su");
        }
        printf("  ℹ yükseklik(0,0) = %d\n", baseH);
    }

    std::printf("\n=== 4) Greedy meshing ===\n");
    {
        World w(1);
        // düz zemin: 16x16 tek kat taş → greedy sonrası 1 quad (alt yüz)
        ChunkPos cp{0,1,0};
        Chunk* c = w.getOrCreate(cp);
        for (int x = 0; x < CHUNK_W; ++x)
            for (int z = 0; z < CHUNK_W; ++z)
                c->set(x, 0, z, STONE);
        c->clearDirty();

        MeshData mesh;
        Mesher::build(w, *c, mesh);
        printf("  ℹ 16x16x1 taş düzlem → %u quad, %zu vertex (opak)\n",
               mesh.quadCount, mesh.opaque.size());

        // 16x16 düzlem: üst 1 + alt 1 + 4 yan = 6 quad (naif 1536 yüz)
        CHECK(mesh.quadCount <= 6, "greedy: 1536 yüz → 6 quad");
        CHECK(mesh.opaque.size() % 4 == 0, "vertex sayısı 4'ün katı");
        CHECK(mesh.opaque.size() == mesh.quadCount * 4, "quad başına 4 vertex");

        // vertex boyutu
        CHECK(sizeof(Vertex) == 36, "Vertex 36 bayt");
        printf("  ℹ mesh boyutu: %zu bayt\n", mesh.bytes());
    }

    std::printf("\n=== 5) Greedy meshing — gerçek arazi ===\n");
    {
        World w(424242);
        for (int cy = 0; cy < 3; ++cy) w.getOrCreate({0, cy, 0});
        // yüzey 27 civarı → chunk 1 (y 16-31) en çok yüz üretir
        size_t totalQuads = 0, totalVerts = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int cy = 0; cy < 3; ++cy) {
            MeshData m;
            Mesher::build(w, *w.chunkAt({0, cy, 0}), m);
            totalQuads += m.quadCount;
            totalVerts += m.opaque.size() + m.transparent.size();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        printf("  ℹ 3 chunk mesh: %zu quad, %zu vertex, %.2f ms\n", totalQuads, totalVerts, ms);
        CHECK(totalQuads > 0, "arazi mesh üretildi");
        CHECK(ms < 100.0, "3 chunk mesh < 100 ms (hedef 60 FPS)");
        // naif yaklaşımda her blok 6 yüz olurdu: 3*4096*6 = 73728
        printf("  ℹ greedy kazancı: naif %d quad → %zu quad (%.1fx azalma)\n",
               3 * CHUNK_VOL * 6, totalQuads,
               3.0 * CHUNK_VOL * 6 / (totalQuads > 0 ? totalQuads : 1));
    }

    std::printf("\n=== 6) Fizik — düşme ve zemin ===\n");
    {
        World w(7, false);   // boş dünya
        // 0..10 y arası taş sütun oluştur, oyuncu üstünde düşsün
        for (int y = 0; y <= 10; ++y)
            for (int x = -2; x <= 2; ++x)
                for (int z = -2; z <= 2; ++z)
                    w.setBlock(x, y, z, STONE);

        Body b;
        b.pos = {0.5f, 20.0f, 0.5f};
        int steps = 0;
        while (steps < 600 && !b.onGround) {
            b.vel.x = 0; b.vel.z = 0;
            Physics::move(w, b, 1.0f / 60.0f);
            ++steps;
        }
        printf("  ℹ %d adımda yere indi, y=%.3f\n", steps, b.pos.y);
        CHECK(b.onGround, "oyuncu yere indi");
        CHECK(b.pos.y >= 10.999f && b.pos.y <= 11.001f, "zemin y=11.000 üstünde tam durdu");
    }

    std::printf("\n=== 7) Fizik — duvar çarpışması ===\n");
    {
        World w(8, false);
        // zemin (y=0) + z=3'te duvar (y=1..5)
        for (int x = -8; x <= 8; ++x)
            for (int z = -8; z <= 8; ++z)
                w.setBlock(x, 0, z, STONE);
        for (int y = 1; y <= 5; ++y)
            for (int x = -5; x <= 5; ++x)
                w.setBlock(x, y, 3, STONE);

        Body b;
        b.pos = {0.5f, 5.0f, 0.5f};
        b.vel = {0, 0, 5.0f};
        for (int i = 0; i < 120; ++i) {
            b.vel.z = 5.0f;
            Physics::move(w, b, 1.0f / 60.0f);
        }
        printf("  ℹ z=%.3f (duvar z=3'te)\n", b.pos.z);
        CHECK(b.pos.z < 3.0f, "duvarın içinden geçmedi");
        CHECK(b.pos.z > 2.3f && b.pos.z <= 2.7f, "duvara kadar yaklaşıp durdu (z≈2.67)");
    }

    std::printf("\n=== 8) Işın izleme (blok hedefleme) ===\n");
    {
        World w(9, false);
        w.setBlock(5, 5, 5, STONE);

        RayHit h = Raycast::cast(w, {0.5f, 5.5f, 5.5f}, {1, 0, 0}, 10.0f);
        printf("  ℹ isabet=%d blok=(%d,%d,%d) normal=(%d,%d,%d)\n",
               (int)h.hit, h.block.x, h.block.y, h.block.z, h.normal.x, h.normal.y, h.normal.z);
        CHECK(h.hit, "blok bulundu");
        CHECK(h.block == Vec3i(5, 5, 5), "doğru blok (5,5,5)");
        CHECK(h.normal == Vec3i(-1, 0, 0), "yüz normali -X");

        RayHit miss = Raycast::cast(w, {0.5f, 5.5f, 5.5f}, {0, 1, 0}, 3.0f);
        CHECK(!miss.hit, "yukarı bakan ışın boşluğa gitti");
    }

    std::printf("\n=== 9) Blok kırma / koyma ===\n");
    {
        World w(10, false);
        w.setBlock(3, 4, 5, STONE);
        CHECK(w.getBlock(3, 4, 5) == STONE, "blok yerleştirildi");
        w.setBlock(3, 4, 5, AIR);
        CHECK(w.getBlock(3, 4, 5) == AIR, "blok kırıldı");
    }

    std::printf("\n=== 10) Performans ölçümü (6 chunk yükleme) ===\n");
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        World w(99);
        int loaded = 0;
        for (int cx = -3; cx <= 3; ++cx)
            for (int cz = -3; cz <= 3; ++cz)
                for (int cy = 0; cy < 3; ++cy) {
                    w.getOrCreate({cx, cy, cz});
                    ++loaded;
                }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        printf("  ℹ %d chunk üretimi: %.1f ms (chunk başı %.2f ms)\n",
               loaded, ms, ms / loaded);
        CHECK(w.chunkCount() == static_cast<size_t>(loaded), "tüm chunk'lar yüklendi");
        CHECK(ms / loaded < 5.0, "chunk üretimi < 5 ms (kare bütçesine sığar)");
    }

    std::printf("\n=== SONUÇ ===\n");
    if (failures == 0) {
        std::printf("🎉 TÜM TESTLER GEÇTİ\n\n");
        return 0;
    }
    std::printf("❌ %d test başarısız\n\n", failures);
    return 1;
}
