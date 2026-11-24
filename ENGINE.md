# Craftya — Oyun Motoru İş Listesi

Rust + wgpu ile sıfırdan yazılacak motorun tüm parçaları.
Efor: **g** = gün (1 geliştirici, odaklı çalışma). Sadece motor; sunucu/iş mantığı ayrı.

---

## 0. Proje altyapısı ve build

| # | İş | Detay | Efor |
|---|---|---|---|
| 0.1 | Cargo workspace | `engine`, `world`, `net`, `ui`, `game`, `android`, `desktop` | 1g |
| 0.2 | Android pipeline | `cargo-ndk`, `gradle` köprüsü, NativeActivity, arm64-v8a + armeabi-v7a | 2g |
| 0.3 | Windows build | `winit` + wgpu, tek `cargo build --release`, EXE | 0.5g |
| 0.4 | Loglama | `tracing` + Android logcat, hata raporlama | 0.5g |
| 0.5 | Config | `settings.toml` (grafik, ses, kontrol), cihazdan cihaza taşınabilir | 0.5g |
| 0.6 | Asset paketleme | doku atlası üretici, sıkıştırma (Zstd), APK içine gömme | 1g |
| 0.7 | APK boyut kontrolü | CI'da 80 MB üstü uyarı, gereksiz bağımlılık denetimi | 0.5g |

## 1. Platform katmanı (pencere, yaşam döngüsü)

| # | İş | Detay | Efor |
|---|---|---|---|
| 1.1 | Pencere/surface | winit + wgpu surface, Android + Windows | 1g |
| 1.2 | Yaşam döngüsü | `pause / resume / surface_lost` — Android'de uygulama arkaya atılınca GPU kaybı | 1g |
| 1.3 | Ekran uyumu | DPI, çentik (cutout), güvenli alan, 16:9 / 19.5:9 / tablet | 1g |
| 1.4 | Yönlendirme | yatay kilit, dikey menü desteği | 0.5g |
| 1.5 | Dokunmatik input | çoklu dokunuş, joystick, kamera sürükleme, buton alanları | 2g |
| 1.6 | Klavye/fare (Windows) | WASD, mouse look, tuş eşleme | 0.5g |

## 2. Grafik çekirdeği (wgpu)

| # | İş | Detay | Efor |
|---|---|---|---|
| 2.1 | Device/queue kurulumu | Vulkan (Android), DX12 (Windows), fallback GLES | 1g |
| 2.2 | Kamera | perspektif, FOV, near/far, view/proj matrisleri | 0.5g |
| 2.3 | Doku atlası | tek atlas → tek material, nearest-neighbor (pixel art) | 1g |
| 2.4 | Temel pipeline | opaque + alpha-tested + transparent sıralı geçişler | 2g |
| 2.5 | Depth/stencil | z-fighting önleme, ters-Z opsiyonu | 0.5g |
| 2.6 | Gökyüzü + sis | gradient sky, güneş yönü, sis 5,5→6,0 chunk, sis rengi = gökyüzü | 1g |
| 2.7 | Işıklandırma | güneş + ambient + blok ışığı (chunk başına ışık haritası) | 3g |
| 2.7b | **Basit gölge** | tek yönlü shadow map 1024, tek cascade 48-64 blok, 2×2 PCF, sadece entity cast | 2.5g |
| 2.8 | AO (ambient occlusion) | köşe bazlı yumuşak gölge, vertex verisine gömülü | 1g |
| 2.9 | Su/şeffaf yüzeyler | ayrı geçiş, dalga (opsiyonel), kırılma basit | 1.5g |
| 2.10 | Frame pacing | 45/60/90/120 hedef FPS, VSync aç/kapa, 60 FPS taban | 0.5g |
| 2.11 | Dinamik çözünürlük | FPS düşünce render ölçeğini düşür (0.6×'e kadar) | 1g |

## 3. Voxel render (45 FPS'in kalbi)

| # | İş | Detay | Efor |
|---|---|---|---|
| 3.1 | Chunk temsili | 16×16×16, **palet sıkıştırma** (5-8 bit/blok), RLE | 2g |
| 3.2 | **Greedy meshing** | aynı yüzleri birleştir → draw call 10-20× düşer | 3g |
| 3.3 | Yüz ayıklama | komşu blok opak ise yüz üretilmez | 0.5g |
| 3.4 | Chunk streaming | görüş 6 / işlem 6,5 chunk, kamera yönü önceliği, dairesel yükleme | 2g |
| 3.5 | Arka plan meshing | `rayon` ile ayrı thread'de mesh üret, ana thread'i bloklamaz | 2g |
| 3.6 | Frustum culling | ekran dışı chunk'lar çizilmez | 1g |
| 3.7 | Uzaklık LOD | 6+ chunk uzakta düşük detay (opsiyonel, sonra) | 2g |
| 3.8 | Draw call batching | chunk başına tek çizim, atlas tek texture | 1g |
| 3.9 | Blok kırma animasyonu | çatlak katmanları, kırılma parçacıkları | 1g |

## 3b. Blok ölçeği (250+ blok nasıl kaldırılır)

### Blok kimliği
- id: **u16** (65.535'e kadar) — 250 farklı tip kayıt defterinde 250 satır
- Kayıt defteri **JSON** (sunucudan, sadece metin): isim, doku **indeksleri**, sertlik, araç, ışık, opaklık, ses
- Faz 0: 40-50 tip → 250+ tip sunucudan eklenir (APK'sız)

### Sorumluluk bölüşümü: metin sunucuda, doku cihazda
| Nerede | Ne | Boyut |
|---|---|---|
| Supabase (metin) | blok tanımı: `{id, name, tex:{top,side,bottom}, hardness, tool, light, opaque}` | ~200 B/blok → **250 blok = 50 KB** |
| Cihaz (asset paketi) | doku atlası, cache'lenir | ~2-4 MB, bir kere |
| Oyun içi trafik | **sıfır** (doku zaten cihazda, tanımlar önbellekte) | — |

Kurallar:
1. Sunucu **hiçbir zaman doku göndermez**, sadece indeks gönderir
2. Bilinmeyen doku indeksi → **yer tutucu (damalı) doku**, oyun çökmez
3. Yeni blok = JSON satırı + atlasa doku → asset paketi güncellemesi (APK değil)
4. Denge ayarı (sertlik/hasar) sunucudan anında tüm oyunculara yayılır
5. Blok tanımları cihazda version'lu önbellek; sunucu sürümü değişince delta indirilir

### Chunk belleği — palet sıkıştırma
| Palet büyüklüğü | Bit/blok | Chunk (4096 blok) |
|---|---|---|
| ≤ 32 tip | 5 bit | 2,5 KB |
| ≤ 256 tip | 8 bit | 4 KB |
| 196 chunk toplam | — | **~800 KB** |

250 blok dünyada olması RAM'i şişirmez; bir chunk'ta genelde 20-60 tip bulunur.

### Doku — atlas
250 blok × 3-4 yüz ≈ **900-1000 doku karesi** (+ animasyonlu su/lav/ateş)

| Yöntem | Kapasite | Değerlendirme |
|---|---|---|
| **Atlas 1024×1024, 16×16 döşeme, 1 px dolgu** | **~3100 kare** | ✅ seçildi |
| Atlas 2048×2048 | ~12.400 kare | 16 MB VRAM, gereksiz |
| Texture array (256 katman) | 256/array | Mali'de katman sınırı, karmaşık |

**Seçim gerekçesi:** 4 MB VRAM, **nearest filtre, mipmap yok** → kenar sızması yok,
pixel art'ta mipmap zaten gereksiz (uzakta sis var). 3100 kare → 250 blok için 3× fazla yer.

### Animasyonlu dokular
Su, lav, ateş, portal: atlas içinde ardışık kareler + shader'da zaman bazlı UV kaydırma.
Ek draw call yok, sadece uniform güncellemesi.

### Draw call — blok sayısından bağımsız
Tek atlas = tek material → chunk başına 1 çizim, 3 geçiş (opaque / alpha-test / şeffaf).
196 chunk × 3 = **en fazla 588 draw call** (greedy meshing sonrası çoğu boş).

### Gerçek maliyet: sanat
16×16 pixel art, blok başına ~5-10 dk → 250 blok ≈ **25-40 saat sanat emeği**.
Bu yüzden bloklar **içerik katmanı** sayılır (sunucudan gelir), kod katmanı değil.

## 4. Dünya verisi

| # | İş | Detay | Efor |
|---|---|---|---|
| 4.1 | Blok kayıt defteri | u16 id, JSON tabanlı, 6 yüz dokusu, sertlik, araç, ışık, opaklık, ses | 2g |
| 4.2 | Dünya üretimi | Perlin/Simplex gürültü, yükseklik, biyom, mağara, ağaç, cevher | 3g |
| 4.3 | Chunk yönetimi | hashmap, komşu takibi, dirty flag | 1g |
| 4.4 | Işık yayılımı | skylight + blok ışığı BFS yayılımı, chunk sınırında taşma | 3g |
| 4.5 | Blok güncelleme | kır/koy, komşu chunk'ı kirlet, olay yayını | 1g |
| 4.6 | Kaydetme formatı | kendi kompakt binary formatı (chunk başına ~3-8 KB) | 2g |
| 4.7 | Harita yükleme | `.craftmap` dosyası (arenas), düzenlenebilir | 1.5g |
| 4.8 | Snapshot | dünyayı Storage'a yaz/oku (survival için) | 1.5g |

## 5. Fizik ve çarpışma

| # | İş | Detay | Efor |
|---|---|---|---|
| 5.1 | AABB çarpışma | oyuncu kutusu vs blok, eksen ayrık çözüm | 2g |
| 5.2 | Süpürme testi | hızlı hareket eden cismin blok içinden geçmemesi | 1g |
| 5.3 | Yerçekimi/zıplama | sabit zaman adımı (fixed timestep), sub-step | 1g |
| 5.4 | Basamak/merdiven | otomatik tırmanma, su yüzme, tırmanma | 1.5g |
| 5.5 | Işın izleme (raycast) | blok hedefleme (DDA), temas noktası ve yüz normali | 1g |
| 5.6 | Varlık çarpışması | item toplama, mob itme | 0.5g |

## 6. Oyuncu ve kontrol

| # | İş | Detay | Efor |
|---|---|---|---|
| 6.1 | Hareket | yürü/koş/zıpla/eğil, ivme ve sürtünme | 1g |
| 6.2 | Kamera | birinci/üçüncü şahıs, dokunmatik hassasiyet, yumuşatma | 1g |
| 6.3 | Mobil HUD | joystick, zıpla, eğil, envanter, dokunma alanı ayarlanabilir | 2g |
| 6.4 | Model | bloklu karakter, skin (doku) desteği, 6 parça | 1.5g |
| 6.5 | Animasyon | yürüme/koşma/kol sallama, basit iskelet | 1.5g |

## 7. Varlıklar (entity)

| # | İş | Detay | Efor |
|---|---|---|---|
| 7.1 | ECS-lite | bileşen tablosu, sistem sırası, determinist güncelleme | 2g |
| 7.2 | Mob'lar | basit yol bulma (grid A*), hedef takibi, hasar | 3g |
| 7.3 | Mermiler | ok/ateş topu, çarpışma, hasar | 1g |
| 7.4 | Düşen eşya | fizik, toplama, ömür | 1g |
| 7.5 | **Sahiplik modeli** | her varlığın bir sahibi cihaz var (hostsuz mimari) | 2g |
| 7.6 | Yumuşatma | uzaktan gelen pozisyonu interpolate et | 1g |

## 8. Envanter, item, crafting

| # | İş | Detay | Efor |
|---|---|---|---|
| 8.1 | Item kayıt defteri | id, ikon, yığın limiti, araç tipi/dayanıklılık | 1g |
| 8.2 | Envanter | 36 slot + hotbar, sürükle-bırak | 2g |
| 8.3 | Hotbar | seçim, dokunmatik hızlı geçiş | 0.5g |
| 8.4 | Koyma/kırma kuralları | yerleşim geçerliliği, araca göre kırma süresi | 1g |
| 8.5 | Crafting | tarif tablosu (JSON), 2×2 ve 3×3 | 2g |

## 9. Arayüz (pixel art, motor içi)

| # | İş | Detay | Efor |
|---|---|---|---|
| 9.1 | GUI çekirdeği | immediate-mode, 9-slice, düğme/kaydırıcı/liste | 3g |
| 9.2 | Bitmap font | pixel font, Türkçe karakter (ı, ş, ğ, ç, ö, ü) | 0.5g |
| 9.3 | Ekranlar | giriş, ana menü, lobi, mod seçim, HUD, duraklat, ayarlar | 3g |
| 9.4 | Ekran uyumu | farklı en/boy, çentik güvenli alanı, tablet | 1g |
| 9.5 | Yerelleştirme | TR + EN, metinler sunucudan (JSON) | 1g |
| 9.6 | Bildirim/ipucu | toast, bağlantı durumu, sürüm uyarısı | 0.5g |

## 10. Ses

| # | İş | Detay | Efor |
|---|---|---|---|
| 10.1 | Mikser | kanal bazlı ses seviyesi, master | 1g |
| 10.2 | Konumsal ses | blok/mob seslerinin mesafeye göre azalması | 1g |
| 10.3 | SFX havuzu | adım, kırma, koyma, hasar, UI tıklama | 1g |
| 10.4 | Müzik | lobi ve maç müziği, geçiş | 0.5g |
| 10.5 | Gecikme | AAudio düşük gecikme yolu (Android) | 1g |

## 11. Ağ (P2P mesh, hostsuz)

| # | İş | Detay | Efor |
|---|---|---|---|
| 11.1 | Taşıma katmanı | UDP (`quinn` QUIC veya ham UDP + güvenilirlik) | 3g |
| 11.2 | Paket protokolü | ikili (binary), sürüm alanı, delta sıkıştırma | 2g |
| 11.3 | Snapshot + interpolasyon | 10-15 Hz pozisyon, arada interpolate | 2g |
| 11.4 | Mesh kurulumu | herkes herkese bağlanır, bağlantı sağlığı izleme | 3g |
| 11.5 | Signaling | Supabase üzerinden tanışma (SDP/offer benzeri) | 2g |
| 11.6 | NAT geçişi | STUN + Cloudflare TURN fallback, otomatik seçim | 3g |
| 11.7 | Kopma/yeniden bağlanma | 5 sn tolerans, durum geri alma | 2g |
| 11.8 | Gecikme telafisi | lag compensation, rollback (PvP için) | 3g |
| 11.9 | Doğrulama | hareket hızı sınırı, ışınlanma reddi (hile önleme) | 2g |
| 11.10 | Bant genişliği bütçesi | oyuncu başına <40 kbps hedefi, ölçüm | 1g |

## 12. Script motoru (Luau)

| # | İş | Detay | Efor |
|---|---|---|---|
| 12.1 | Gömme | Rust ↔ Luau FFI, güvenli sınır | 3g |
| 12.2 | API yüzeyi | `world`, `player`, `inventory`, `ui`, `events` | 3g |
| 12.3 | Sandbox | dosya/ağ/native çağrı kapalı, bellek limiti | 2g |
| 12.4 | Manifest + imza | Ed25519 doğrulama, sürüm kontrolü | 2g |
| 12.5 | Sıcak yükleme | oyun açıkken script değişimi, rollback | 2g |
| 12.6 | Hata izolasyonu | script patlarsa oyun çökmez, log + devre dışı bırak | 1g |

## 13. Performans ve profil

| # | İş | Detay | Efor |
|---|---|---|---|
| 13.1 | Profiler overlay | FPS, kare süresi, chunk sayısı, draw call, bellek, gölge ms | 1g |
| 13.2 | Isı/pil farkındalığı | ısınınca render distance düşür | 1g |
| 13.3 | Bellek bütçesi | chunk cache sınırı, LRU boşaltma | 1g |
| 13.4 | Kare bütçesi | **60 FPS = 16.6 ms**; iş kuyruğu böl, uzun frame'i kes | 2g |
| 13.5 | Cihaz testi | Note 11 Pro 4G'de 60 FPS doğrulama + Note 7 Pro'da 45 FPS | 1g |

## 14. Araçlar (editör tarafı)

| # | İş | Detay | Efor |
|---|---|---|---|
| 14.1 | Harita editörü | masaüstünde arena tasarla, `.craftmap` kaydet | 3g |
| 14.2 | Blok/doku editörü | yeni blok tanımı, atlas güncelleme | 1.5g |
| 14.3 | Test sahnesi | hızlı açılan düz dünya, bot oyuncu | 1g |
| 14.4 | Headless benchmark | CI'da FPS ölçümü (GPU'suz yaklaşık) | 1g |

## 15. Test ve güvenlik

| # | İş | Detay | Efor |
|---|---|---|---|
| 15.1 | Birim testleri | chunk, meshing, fizik, protokol kodlama | 2g |
| 15.2 | Deterministik dünya testi | aynı seed → aynı dünya | 0.5g |
| 15.3 | Ağ simülasyonu | gecikme/kayıp enjeksiyonu ile test | 1.5g |
| 15.4 | APK imza | release imzası, hash yayını | 0.5g |
| 15.5 | Girdi doğrulama | sunucudan gelen her paket sınır kontrolü | 1g |

---

## Faz 0 için asgari alt küme (ilk çalışan sürüm)

Sadece bunlar: **0.1-0.4, 1.1-1.2, 2.1-2.4, 3.1-3.4, 4.1-4.3, 5.1-5.3, 5.5, 6.1-6.2, 9.1-9.2, 13.1**

Yani: açılan pencere + dünya render + gezinen oyuncu + blok kırma + FPS sayacı.
Hedef: **Redmi Note 11 Pro 4G'de 60 FPS taban**, Redmi Note 7 Pro / Redmi 7'de 45 FPS.

## Motor dışında kalanlar (script/backend tarafında)

- Giriş/hesap sistemi (Supabase Auth)
- Lobi iş mantığı, NPC, kuyruk, mod seçim akışı
- Mağaza, kasa, kozmetik, rütbe
- Mod kuralları (Bedwars/PvP mantığı) → Luau script
- Arkadaş listesi, sohbet
- Dünya snapshot politikası, arşivleme
