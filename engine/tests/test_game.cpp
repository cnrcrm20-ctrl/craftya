// Craftya — oyun döngüsü / kamera / culling / akış testleri
#include "craftya/game.hpp"
#include <cstdio>
#include <chrono>

using namespace craftya;
static int failures = 0;
#define CHECK(c,m) do{ if(!(c)){std::printf("  ❌ %s\n",m);++failures;} else std::printf("  ✅ %s\n",m);}while(0)

int main(){
  std::printf("\n=== 1) Kamera ve matrisler ===\n");
  {
    Camera c; c.pos={0,0,0}; c.yaw=0; c.pitch=0;
    Vec3f f=c.forward();
    std::printf("  ℹ ileri=(%.3f, %.3f, %.3f)\n",f.x,f.y,f.z);
    CHECK(std::fabs(f.z+1.f)<1e-4f, "yaw=0 → -Z yönüne bakıyor");
    c.yaw=3.14159f/2.f;
    f=c.forward();
    CHECK(std::fabs(f.x-1.f)<1e-3f, "yaw=90° → +X yönüne bakıyor");
    Mat4 p=c.proj(16.f/9.f);
    CHECK(p.m[11]==-1.f, "perspektif matrisi doğru (m11=-1)");
  }

  std::printf("\n=== 2) Frustum culling ===\n");
  {
    Camera c; c.pos={0,0,0}; c.yaw=0; c.pitch=0; c.farZ=1000;
    Mat4 vp = c.proj(16.f/9.f) * c.view();
    Frustum fr = Frustum::fromCameraMatrix(vp);
    bool inFront = fr.boxVisible({-1,-1,-20}, {1,1,-18});
    bool behind  = fr.boxVisible({-1,-1, 20}, {1,1, 22});
    std::printf("  ℹ önde=%d arkada=%d\n",(int)inFront,(int)behind);
    CHECK(inFront, "kameranın önündeki kutu görünür");
    CHECK(!behind, "kameranın arkasındaki kutu culled");
  }

  std::printf("\n=== 3) Oyun başlangıcı ===\n");
  {
    Game g(424242, true);
    CHECK(g.player().pos.y > 20.f, "oyuncu yüzeyin üstünde başlıyor");
    std::printf("  ℹ başlangıç y=%.2f\n", g.player().pos.y);
  }

  std::printf("\n=== 4) 60 FPS simülasyonu (3 saniye) ===\n");
  {
    Game g(424242, true);
    InputState in;
    const float dt = 1.0f/60.0f;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int i=0;i<180;++i){
      g.update(dt, in);
    }
    auto t1=std::chrono::high_resolution_clock::now();
    float ms=std::chrono::duration<float,std::milli>(t1-t0).count();
    const auto& st=g.stats();
    std::printf("  ℹ 180 kare: %.1f ms toplam, %.2f ms/kare\n", ms, ms/180.f);
    std::printf("  ℹ görünür=%.0f chunk, yüklü=%d, çizim=%d, mesh=%.2f ms, vram=%.0f KB\n",
      (double)st.visibleChunks, st.loadedChunks, st.drawCalls, st.meshMs, st.vramBytes/1024.0);
    CHECK(st.loadedChunks > 20, "20+ chunk mesh'lendi");
    CHECK(st.visibleChunks > 0, "frustum sonrası görünür chunk var");
    CHECK(st.visibleChunks < st.loadedChunks, "culling işe yarıyor (görünür < yüklü)");
    CHECK(ms/180.f < 16.6f, "kare süresi 16.6 ms altında (60 FPS)");
  }

  std::printf("\n=== 5) Blok kırma / koyma (oyun içi) ===\n");
  {
    Game g(7, false);
    // oyuncunun önüne blok koy
    Vec3i target{(int)std::floor(g.player().pos.x),
                 (int)std::floor(g.player().pos.y)+1,
                 (int)std::floor(g.player().pos.z)-3};
    g.world().setBlock(target.x,target.y,target.z, STONE);
    std::printf("  ℹ hedef blok=(%d,%d,%d)\n",target.x,target.y,target.z);
    CHECK(g.world().getBlock(target.x,target.y,target.z)==STONE, "blok yerleştirildi");
    g.world().setBlock(target.x,target.y,target.z, AIR);
    CHECK(g.world().getBlock(target.x,target.y,target.z)==AIR, "blok kırıldı");
  }

  std::printf("\n=== 6) Hareket simülasyonu ===\n");
  {
    Game g(424242, true);
    float y0 = g.player().pos.y;
    for(int i=0;i<60;++i){ InputState in; in.jump=true; g.update(1.f/60.f,in); }
    std::printf("  ℹ zıplama sonrası y=%.2f (başlangıç %.2f)\n", g.player().pos.y, y0);
    CHECK(std::fabs(g.player().pos.y - y0) < 2.0f, "zıplama sonrası yüzeye yakın kaldı");
  }

  std::printf("\n=== SONUÇ ===\n");
  if(!failures){ std::printf("🎉 TÜM OYUN TESTLERİ GEÇTİ\n\n"); return 0; }
  std::printf("❌ %d başarısız\n\n", failures); return 1;
}
