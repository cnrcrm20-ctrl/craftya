# Craftya Motoru — Durum Raporu (kapsamlı sürüm)

> Bu, ilk "çekirdek motor"un üzerine **tam kapsamlı** ikinci geçiş.
> Önceki sürüm sadece dünya + mesh + fizik + döngüydü. Şimdi ışık, doku atlası,
> envanter/üretim, ses, ağ, ECS+mob AI, kayıt ve GUI de var.

---

## 1. Ne var, ne çalışıyor

### 1.1 Platform ve oyun çekirdeği
| Bileşen | Dosya | Durum |
|---|---|---|
| Çekirdek tipler, blok tanımları (31 blok) | `core.hpp` | ✅ |
| Chunk dünyası, palet sıkıştırma, 5-bit depolama | `world.hpp` | ✅ |
| Greedy meshing (558× quad azaltma) | `mesher.cpp` | ✅ |
| Fizik: AABB, eksen ayrık çarpışma, su, zıplama | `physics.cpp` | ✅ |
| Kamera, frustum culling, chunk streamer | `render.cpp` | ✅ |
| Oyun döngüsü, kırma/koyma, mesh bütçesi | `game.cpp` | ✅ |
| Platform soyutlaması (dosya, saat, log) | `platform.cpp` | ✅ |
| **Birleşik cephe: `Engine` sınıfı** | `engine.cpp` | ✅ **yeni** |

### 1.2 Işık ve gölge
- 0-15 ölçeği, **1 bayt/blok** (`sky<<4 | block`), chunk başına 4 KB.
- Sky light BFS: yukarı yönde kayıpsız, diğer yönlerde -1.
- Block light BFS: ışık yayan bloklar (glowstone, lava, torch).
- `LightEngine::onBlockChanged` → 3×3×3 chunk komşuluğunu yeniden hesaplar.
- `DayCycle`: 20 dakikada 1 gün, 08:00 başlangıç, gün faktörü 0.08 (gece) → 1.0 (öğle).
- Gök renkleri: gündüz mavi, şafak/akşam turuncu, gece lacivert geçişli.
- **Ambient occlusion**: 1.00 / 0.86 / 0.72 / 0.60 (iki taraf kapalıysa 0.55).
- Mesh vertex'ine gömülü: `light = ışık_seviyesi × yön_parlaklığı × AO`.

### 1.3 Doku atlası
- 256×256 atlas, 16×16 kare, **31 blok × 3 yüz** (üst / yan / alt).
- Tamamen prosedürel üretim — hiçbir asset dosyası yok, APK'ya doku koymak gerekmiyor.
- `tileForBlock(id, face) = id * 3 + face`; eksik kare macenta.
- Sıfır bağımlılıklı PNG yazıcı (CRC32 + stored deflate + adler32) — `build/atlas.png` ile görsel doğrulandı.
- Mesher artık UV'leri bu atlasa göre üretiyor.

### 1.4 Envanter, üretim, kazma
- 36 yuva (9 hotbar + 27 ana) + yığın birleştirme + dayanıklılık.
- Eşya kayıt defteri: 30 blok + 13 alet (tahta→elmas kazma/balta/kürek/kılıç) + 8 malzeme.
- 13 varsayılan tarif; şekilli eşleşme **konumdan bağımsız** (normalize edilir).
- Kazma süresi = `sertlik × 1.5`; doğru alet `1 + tier×0.6` ile böler.
- Alet aşınması (`Mining::wearTool`).

### 1.5 Ses
- Prosedürel SFX üretimi: 14 tür (adım, kırma, koyma, vuruş, zıplama, iniş, UI, üretim, su, zombi, ok, hasar, yeme, kapı).
- 32 kanallı mikser, **konumsal ses** (32 blok menzil, mesafe zayıflatma + sabit güçlü pan).
- Prosedürel ambient müzik döngüsü (pentatonik pad + rüzgâr dokusu).
- Platform arka ucu (AAudio/OpenSL/WASAPI) sadece `mix()` çağırır.

### 1.6 Ağ — hostsuz P2P mesh
- **Host yok**: her cihaz her cihaza doğrudan paket atar (`broadcast` → tüm peer'lar).
- 15 paket türü: Hello/HelloAck, PlayerState (20 Hz), BlockEdit, EntitySpawn/State/Despawn, Chat, Ping/Pong, SyncRequest/Response, MatchState, Disconnect.
- İkili paket yazıcı/okuyucu (little-endian, float bit-kopyalama, kısa paket koruması).
- Uzak oyuncu interpolasyonu (önceki/sonraki snapshot + `interpTime`), istemci tahmini ve hata ölçümü.
- 1 Hz otomatik ping → RTT; 5 sn sessizlik → bağlantı düşer.
- Peer tavanı 16 (Mega 12 + yedek).
- `RemotePlayer`: ad, public key (imza doğrulaması için alan), paket kaybı, RTT.

**Doğrulanan senaryo:** 3 sanal telefon, her biri diğer ikisine bağlı; 1. cihazın konumu, blok düzenlemesi ve sohbeti 2 ve 3'e ulaştı.

### 1.7 ECS + mob AI
- 4 varlık türü: Player, Mob, Projectile, ItemDrop.
- Mob durumları: Idle → Wander → Chase → Attack; görüş hattı (blok örnekleme) + 4 yönlü engel dolanma.
- Mermiler blok/varlık çarpması, ömür; düşen eşya yerçekimi + toplama gecikmesi.
- Tamamı deterministik ve ağ paketleriyle senkronlanabilir.

### 1.8 Kayıt / yükleme / ayarlar
- `.craftmap` ikili format: magic `CRAFTYA01`, sürüm, tohum, oyuncu konumu + bakış, 36 yuva envanter, blok deltaları.
- **Delta kaydı**: dünyanın tamamı değil, sadece değişen bloklar.
- Bozuk dosya koruması (magic/sürüm/boyut sınırı).
- RLE sıkıştırma (tekrar eden veri ~10× küçülüyor, kayıpsız doğrulandı).
- `GameSettings` JSON gidiş-dönüşü + güvenli kırpma (renderDistance 2-12, fov 50-110).

### 1.9 GUI / HUD
- `DrawList` komut toplayıcı: dikdörtgen, dokulu dikdörtgen, yazı, çizgi, yuvarlak buton.
- HUD: nişangâh, 10 kalp can barı, 9 yuvalı hotbar (seçili yuva parlar), FPS/ms/oyuncu sayısı, koordinatlar.
- **Dokunmatik**: sol joystick (çoklu dokunma), sağ bakış alanı, JUMP/BREAK/USE butonları.
- Menü sistemi: Ana / Oyna / Ayarlar / Duraklat / Envanter / Üretim + **NPC mod paneli** (Düello / Takımlı / Mega + canlı oyuncu sayısı).
- Toast bildirimleri, solan sohbet günlüğü.

### 1.10 JSON
- Sıfır bağımlılık, hata mesajlı ayrıştırıcı; nesne/dizi/string/sayı/bool.
- `parseBlockDefinitions` → sunucudan gelen blok tanımları (id, doku indeksleri, sertlik, alet, ışık).
- **Bu, "her şey sunucudan dinamik" kuralının motor tarafı**: blok/mod/ayar tanımları APK'ya gömülü değil, JSON ile geliyor.

---

## 2. Ölçülen performans

Bu makinede (masaüstü, `-O2`):

| Ölçüm | Sonuç |
|---|---|
| Kare süresi (simülasyon + mesh) | **2.4-3.0 ms** |
| 60 FPS bütçesi | 16.6 ms → **~6× boşluk** |
| Chunk mesh süresi (bütçeli) | 0.09 ms/kare |
| Görünür chunk / yüklü chunk | 199 / 685 |
| VRAM (mesh) | 803 KB |
| Greedy mesh azaltma | 73.728 → 132 quad (**558×**) |
| Chunk üretimi | 0.07-0.09 ms |
| Atlas üretimi | ~1 ms (açılışta 1 kez) |

Not: Bu sayılar masaüstü. Telefonda Adreno 612 / Mali-G57 MC2 darboğazı GPU tarafında
olacak; CPU tarafı rahat. Hedef 60 FPS (Note 11 Pro 4G), alt sınır 45 FPS.

---

## 3. Testler

```
build/craftya_test        motor çekirdeği      35 ✅
build/craftya_game_test   oyun döngüsü         13 ✅
build/craftya_sys_test    genişletilmiş        199 ✅   (0 başarısız)
```

Genişletilmiş test grupları: json, lighting, atlas, inventory, crafting, save,
net, ecs, audio, gui, mesh senaryo (3 telefon), engine cephesi.

`./build.sh` hepsini derler ve çalıştırır.

---

## 4. Kod hacmi

| | |
|---|---|
| Motor kaynak + başlık | **5.888 satır** (16 `.cpp`, 18 `.hpp`) |
| Motor testleri | ~1.500 satır |
| Bağımlılık | **sıfır** (STL dışında hiçbir şey) |
| Dil | C++17 |

---

## 5. Android tarafı

- `android/app/src/main/cpp/CMakeLists.txt` **tüm 16 modülü** kapsayacak şekilde güncellendi.
- Tüm modüller `-fno-exceptions -fno-rtti` ile sözdizimi kontrolünden geçti (NDK'nın kullandığı bayraklar).
- NativeActivity kabuğu + `android_platform.cpp` + `native_main.cpp` hazır.
- Release imzası: `android/keys/craftya-release.p12`.

**APK bu ortamda üretilemez** — burada NDK/Gradle/JDK yok. Telefonda/kendi bilgisayarında:

```bash
export ANDROID_NDK_HOME=$ANDROID_SDK_ROOT/ndk/26.1.10909125
cd android && ./build.sh
```

---

## 6. Sırada ne var

Motor "kapsamlı" hale geldi. Kalan işler uygulama katmanında:

1. **Gerçek GPU arka ucu** — `IRenderBackend` arayüzü hazır; GLES3 implementasyonu (VAO/VBO yükleme, gölge map'i, skybox) Android tarafında bağlanacak.
2. **WebRTC veri kanalı** — `PeerLink::SendFn` hazır; gerçek soket/DC bağlantısı Supabase signaling + Cloudflare TURN ile.
3. **Gölge geçişi** — tek 1024² güneş haritası, 2×2 PCF, 48-64 blok menzil (ışık altyapısı hazır).
4. **Sunucu tarafı içerik akışı** — JSON tanımları Supabase Storage'dan çekme + önbellek.
5. **APK derleme ve telefonda profil çıkarma.**

Android APK'yı burada derleyemiyorum; o adımı kendi makinende çalıştırman gerekiyor.
