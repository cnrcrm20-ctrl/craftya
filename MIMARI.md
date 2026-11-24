# Craftya — Mimari ve Yol Haritası

> Durum: **TASLAK — onay bekliyor.** Bu dosya kod değil, karar dokümanı.
> Onaylanan maddeler işaretlenir, ondan sonra kod yazılmaya başlanır.

---

## 1. Kilitlenen kararlar

| # | Karar | Sonuç |
|---|---|---|
| K1 | Oyun **sadece bizim APK'dan** girilir | Cross-play yok, Java/Bedrock protokolü yok |
| K2 | Motor **bizim** — Mojang kodu, APK'sı, dosyası kullanılmaz | Telif riski sıfır, güncelleme bağımsız |
| K3 | İsim ve tüm varlıklar **Craftya** | "Minecraft" markası hiçbir yerde kullanılmaz |
| K4 | Hedef platform: **Android (APK) + Windows (EXE)** | Tek kod tabanı, iki export |
| K5 | **0 TL** — VPS yok, 7/24 açık makine yok | Sunucusuz mimari zorunlu |
| K6 | Performans: **Note 11 Pro 4G'de 60 FPS taban (üstü serbest)**, Redmi 7/Note 7 Pro'da 45 FPS | Motor bu cihazlara göre kalibre edilir |
| K7 | Host yok — **P2P mesh** | Kimse 7/24 açık kalmak zorunda değil |
| K8 | Play Store yok — **sideload APK/EXE** | Mağaza denetimi ve komisyon yok |
| K9 | **APK kabuk** — oyun kodu ve içerik sunucudan | Mod/kural/UI/içerik değişikliği APK'sız |
| K10 | **Otomatik güncelleme** — sessiz arka plan indirme | Script/içerik anında, APK tek dokunuş |

### Bilinçli olarak YAPILMAYACAKLAR
- Korsan/patchlenmiş Minecraft APK'sı paketlemek ve dağıtmak
- Mojang'ın doku, ses, font, model dosyalarını kullanmak
- Gerçek Minecraft client'larını kabul eden protokol katmanı yazmak
- 7/24 JVM süreci gerektiren herhangi bir çözüm (Paper/Purpur/PocketMine)

---

## 2. Sistem mimarisi

```
        ┌──────────────────────────────────────────┐
        │            SUPABASE (free)               │
        │  Postgres: hesap, skin, stats, kasa      │
        │  Realtime Presence: lobi (200 kişi)      │
        │  RPC + unique constraint: blok hakemi    │
        │  Storage: dünya snapshot, mod paketi     │
        └───────────────┬──────────────────────────┘
                        │ signaling + lobi
        ┌───────────────┴──────────────────────────┐
        │   OYUNCU CİHAZLARI (P2P mesh, host yok)   │
        │   pozisyon / hasar / mob → direkt peer    │
        └───────────────┬──────────────────────────┘
                        │ NAT arkasındaysa
        ┌───────────────┴──────────────────────────┐
        │   CLOUDFLARE TURN (1 TB/ay bedava)        │
        └──────────────────────────────────────────┘
```

### Neden bu bölüşüm
Supabase free tier: **200 eşzamanlı Realtime bağlantısı, 2 milyon mesaj/ay.**
8 kişilik bir maçta saniyede 80 paket aksa 2 milyon mesaj **~7 saatte** biter.
Yani oyun trafiği Realtime'dan geçemez. Bu yüzden:

- **Supabase** → lobi, hesap, kayıt, hakem, dünya yedeği (düşük frekans)
- **P2P mesh** → pozisyon, hasar, mob (yüksek frekans, sunucudan geçmez)
- **Cloudflare TURN** → sadece CGNAT'a takılan oyuncular için köprü

Türkiye mobilinde CGNAT yaygın olduğu için TURN köprüsü **opsiyonel değil, gerekli**.

### Kim neyi simüle eder (hostsuz model)
| Varlık | Sahibi |
|---|---|
| Oyuncu kendi karakteri | O oyuncunun cihazı |
| Mob, düşen eşya, mermi | **Doğuran** cihaz |
| Blok kırma/yerleştirme | Supabase atomik yaz (`unique(world,x,y,z)`) — ilk yazan kazanır |

Böylece yük dağılır, tek cihaza yığılma olmaz. Bir oyuncu çıkarsa sadece kendi
doğurduğu varlıklar donar; maç devam eder.

---

## 3. Oyun akışı (kullanıcının gördüğü)

1. **Uygulama açılır** → pixel art splash, Supabase auth (tek tuş: anonim/Google)
2. **Lobi** → herkes birbirini görür, sohbet açık, üstte online sayısı
3. **NPC** → lobinin ortasında, üstünde canlı sayaç
4. **NPC'ye tıkla** → mod seçim paneli:

```
┌─────────────────────────────┐
│   ⚔  MAÇ SEÇ                │
├─────────────────────────────┤
│ 🗡  DÜELLO                   │
│    Tek tek, bire bir        │
│    ▸ 3 oyuncu bekliyor      │
├─────────────────────────────┤
│ 🛡  TAKIMLI                 │
│    4'e 4                    │
│    ▸ 5 oyuncu bekliyor      │
├─────────────────────────────┤
│ 🔥 MEGA TAKIM               │
│    6'ya 6, kaos             │
│    ▸ 8 oyuncu bekliyor      │
└─────────────────────────────┘
```

5. **Kuyruğa gir** → sayı canlı güncellenir (Supabase Realtime)
6. **Dolunca maç başlar** → `rooms` kaydı açılır, TURN/STUN bilgisi dağıtılır,
   oyuncular birbirine mesh bağlanır, maç çalışır
7. **Maç biter** → stats Supabase'e yazılır, oyuncular lobiye döner

### Kapasite kuralları
| Mod | Oyuncu | Hostsuz mesh yükü |
|---|---|---|
| Düello | 2 | çok hafif |
| Takımlı (5×2) | 10 | rahat |
| Mega (4×3) | **12 tavan** | sınırda ama oynanır |
| 16 kişi ve üstü | — | **desteklenmez** (mesh patlar) |

### Kapasite — kesin sayılar (Supabase free)

Üç ayrı tavan var, en sıkısı mesaj bütçesi:

| Tavan | Değer |
|---|---|
| Eşzamanlı bağlantı | 200 |
| Realtime mesaj | 2.000.000 / ay |
| Veritabanı (Postgres) | 500 MB |
| Dosya depolama (Storage) | 1 GB |
| Storage egress | 5 GB / ay |

**500 MB yeter mi? Evet — dünya verisi oraya yazılmazsa.** İki ayrı havuz var:

| Havuz | Boyut | Ne konur | Kapasite |
|---|---|---|---|
| Postgres | 500 MB | hesap, stats, kasa, queue, oda | 5 KB/oyuncu → **~100.000 oyuncu** |
| Storage | 1 GB | harita, chunk blob, mod paketi | 3-5 MB/harita → **~200-300 harita** |

### 500 MB'ı patlatan tek şey: blok olayları
Her blok kırma/yerleştirme bir satır (~100 B). 8 oyuncu saniyede 4 blok →
1 saatte ~1,5 MB → 100 maç/gün = **~1,5 GB/ay** ❌

**Zorunlu kurallar:**
1. Blok olayı Postgres'e **yazılmaz** — maç belleğinde yaşar, maç bitince atılır
2. Dünya/harita verisi Storage'a **Zstd sıkıştırılmış blob** olarak yazılır
3. Oda/maç kayıtları maç bitince silinir, yalnızca özet kalır
4. 6 aydan eski maç özetleri Storage'a arşivlenir
5. Haritalar client'ta cache'lenir (Storage egress 5 GB/ay korunur)

**Mesaj bütçesi (200 kişi lobide):**

| Pozisyon sıklığı | Mesaj/sn | Aylık dayanma |
|---|---|---|
| 1 Hz | 200 | 2,8 saat ❌ |
| 0,2 Hz (5 sn) | 40 | 14 saat ❌ |
| 0,1 Hz (10 sn) | 20 | 28 saat ❌ |
| Sadece giriş/çıkış + sohbet | 5-10 | ~sınırsız ✅ |

**Sonuç:** lobide 200 canlı yürüyen avatar **imkânsız**. Lobi tasarımı buna göre:
- Avatar pozisyonları lobide **yayınlanmaz** (sabit yerleşim / rastgele duruş)
- Sohbet mesajı hız sınırlı (oyuncu başına 1 mesaj / 10 sn)
- Canlı pozisyon akışı **sadece maç içinde**, ve orada da P2P (Supabase'e hiç uğramaz)

**Ölçülen kapasite:**

| Senaryo | Anlık aktif |
|---|---|
| Canlı yürüyen avatarlı lobi | 50-80 |
| Statik avatar + sohbet + sayaç | **200** (tavan) |
| Maç içi eşzamanlı oda | backend sınırı yok (P2P) |
| Toplam online | 200 |

201. oyuncu **kuyruğa** alınır, biri çıkınca otomatik girer.

### Mod sayısı — kesin cevap: 3-5

Altyapı mod sayısını sınırlamıyor (mod = DB satırı + kural seti + harita).
Sınır **kuyruk matematiği**:

| Mod sayısı | 200 kişiden moda düşen | 8 kişilik maç | Bekleme |
|---|---|---|---|
| 3 | ~67 | 8 maç/mod | hızlı ✅ |
| 5 | ~40 | 5 maç/mod | iyi ✅ |
| 8 | ~25 | 3 maç/mod | yavaş ⚠️ |
| 12 | ~17 | 2 maç/mod | ölü ❌ |

**Kural: 5 modu geçme.** Her yeni mod kuyruğu böler; mod arttıkça kimse maç bulamaz.
Harita başına maliyet ~0,5-1 MB sıkıştırılmış → Storage'da 10-15 harita rahat sığar.
500 MB'ı asıl yiyen şey dünya snapshot'larıdır → snapshot sayısı ve boyutu sınırlanmalı.

### Blok kaydı — sadece survival SMP'de
Maç modlarının hepsinde harita **maç bitince sıfırlanır**. Yani:

| | Blok kalıcılığı | Nerede tutulur |
|---|---|---|
| Düello, Takımlı, Mega | **Yok** — maç sonunda harita sıfır | Bellekte, P2P senkron |
| Survival SMP | **Var** — dünya kalıcı | Storage'a periyodik snapshot |

Sonuç: maç modlarında Postgres'e **tek satır blok yazılmaz**, Supabase hakemliğine de
gerek kalmaz → blok kırmada 200 ms gecikme yok, çakışmayı P2P + sahiplik çözer.
500 MB sorunu maç modlarında tamamen ortadan kalkıyor.

### Takım formatları (onaylandı)
| Mod | Takım büyüklüğü | Takım sayısı | Toplam | Durum |
|---|---|---|---|---|
| Düello | 1 | — | 2 | ✅ |
| Takımlı | **2 kişi** | 5 takım (2x2x2x2x2) | **10** | ✅ |
| Mega | **3 kişi** | 4 takım (3x3x3x3) | **12** | ✅ mesh tavanı |
| 16 kişi ve üstü | — | — | — | ❌ mesh patlar |

İki mod birbirinin kopyası değil: Takımlı ikili koordinasyon (duo), Mega üçlü rol dağılımı
(tank/okçu/destek) — farklı oynanış. Sayılar yakın olduğu için kuyruk bölünmesini izle;
bir mod boş kalırsa rotasyona alınır.

---

## 4. Motor katmanı — KARAR: kendi motorumuz (Rust)

### Neden kendi motor
Voxel oyunda motorun %80'i zaten bize ait: chunk yönetimi, meshing, blok verisi, dünya
streaming. Hazır bir motor bize sadece pencere + input + ses veriyor, karşılığında
lisans, bağımlılık ve şişkin binary getiriyor. Kendi mini motor daha küçük, daha hızlı,
tamamen bizim.

### Stack
| Katman | Teknoloji | Neden |
|---|---|---|
| Dil | **Rust** (edition 2021, arm64 + x86_64) | GC yok → kare atlaması yok; C++ hızı, bellek güvenliği |
| Grafik | **wgpu** (Vulkan / GLES fallback) | Adreno 612 Vulkan destekli, düşük CPU yükü |
| Pencere/Input | `winit` | Android NativeActivity + Windows aynı API |
| Android paketleme | `cargo-ndk`, NativeActivity, AAB/APK | Java katmanı minimum |
| Windows | `winit` + wgpu | Aynı kod, EXE çıkar |
| Ses | `rodio` / `kira` | hafif, Android (AAudio) destekli |
| İş parçacığı | `rayon` | chunk meshing arka planda |
| Ağ | QUIC (`quinn`) veya UDP + `webrtc-rs` | P2P mesh, düşük gecikme |
| Backend | Supabase (auth, presence, stats, storage) | free tier |

### Hedef cihazlar (doğrulandı)

| Cihaz | Çip / GPU | RAM | Ekran | Android | Hedef |
|---|---|---|---|---|---|
| **Redmi Note 11 Pro 4G** (ana hedef, test cihazı) | Helio G96 / **Mali-G57 MC2** | 6/8 GB | 120 Hz AMOLED | **13 (HyperOS 1.0.3.0)** | **60 FPS taban, üstü serbest** |
| Redmi Note 7 Pro (taban) | Snapdragon 675 / Adreno 612 | 4/6 GB | 60 Hz | 10 | **45 FPS** |
| Redmi 7 (alt sınır) | Snapdragon 632 / Adreno 506 | 3/4 GB | 60 Hz | 10 | 45 FPS (en düşük ayar) |

İkisi de Vulkan destekli (Adreno 612, Mali-G57) → wgpu iki cihazda da çalışır.

**Performans hedefi: 60 FPS taban (ana cihaz), üstü serbest**

60 FPS = kare başına **16.6 ms** (45 FPS'te 22 ms). Bütçe %25 daha dar:

| Parametre | 45 FPS planı | **60 FPS planı** |
|---|---|---|
| Render distance | 6 chunk | **6 chunk sabit** (işlem 6,5) |
| Shadow map | kapalı | **açık, basit** (1024, tek cascade, ~1,5-2 ms) |
| AO | açık | açık (vertex'e gömülü, bedelsiz) |
| Çözünürlük ölçeği | 1.0× | **0.8×, dinamik** (FPS düşerse 0.7×) |
| Meshing | arka plan thread | arka plan + chunk başına mesh bütçesi |
| Parçacık | orta | sınırlı (kırma max 40) |

**Kare hızı seçenekleri (ayarlarda):**
- `60 FPS (sabit)` — varsayılan, pil dostu
- `120 FPS (kilit açık)` — 120 Hz ekran için, güçlü cihaz/şarjda
- `Sınırsız` — benchmark

### Chunk mesafeleri (sabit)
| Parametre | Değer | Sonuç |
|---|---|---|
| **Görüş mesafesi** | **6 chunk (sabit)** | 13×13 = 169 chunk (dairesel ~130) |
| **İşlem mesafesi** | **6,5 chunk** | +1 halka = ~196 chunk |
| Ön-yükleme marjı | ~27 chunk | Dönüş/hareket sırasında pop yok |
| Sis başlangıç | 5,5 chunk | görüş sınırından önce başlar |
| Sis bitiş | 6,0 chunk | tamamen kapanır |
| Sis rengi | gökyüzü rengi | sınır görünmez |

6,5 chunk'a kadar veri hazır olduğu için sisin arkasındaki 0,5 chunk anında görünür
olur → **detay kaybı yok**. Chunk yükleme önceliği kameranın baktığı yöne göre.

**Ayar profilleri:**
| Profil | Görüş | Gölge | Çözünürlük | Taban | Üst |
|---|---|---|---|---|---|
| Düşük (Redmi 7) | 4 chunk | 1024 | 0.7× | 45 FPS | 60 |
| Orta (Note 7 Pro) | 5 chunk | 1024 | 0.8× | 45 FPS | 60 |
| **Yüksek (Note 11 Pro 4G)** | **6 chunk (hedef)** | 1024 | 0.8× dinamik | **60 FPS** | 90-120 |
| Ultra (yeni cihazlar) | 8 chunk | 2048 | 1.0× | 60 FPS | 120 |

**Not:** 6 chunk, 60 FPS için agresif hedef. Mali-G57'de tutmazsa sırasıyla:
0,7× çözünürlük → sis 5,5'e → 5 chunk. Hedef ilk denemede 6'da kalır.

**Mali notu:** Mali sürücüleri Adreno kadar olgun değil; bazı texture formatları ve
extension'lar kısıtlı olabilir. Mali cihazda test edilip gerekiyorsa format fallback'i
devreye girer. Bu, ayar profilinden bağımsız bir kod yolu.

**Android sürüm notu (doğrulandı):**
- Ana test cihazı **Redmi Note 11 Pro 4G = Android 13 / HyperOS 1.0.3.0.TGDMIXM** (viva)
  → sessiz kendi kendini güncelleme **çalışır** (API 33, target SDK 33)
- Redmi Note 7 Pro = Android 10, Redmi 7 = Android 10 → `PackageInstaller` tek onay
- Sessiz güncelleme **Android 12+** ister; kod iki yolu da destekler, sürüme göre seçer

**HyperOS/MIUI tuzağı:** MIUI ve HyperOS arka plan servislerini agresif öldürür.
Güncelleme indirmesi için zorunlu:
1. **Foreground service** + kalıcı bildirim ("Craftya güncelleniyor…")
2. Kullanıcıdan **pil optimizasyonu muafiyeti** (tek seferlik, `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`)
3. HyperOS'ta **Otomatik başlatma** izni yönlendirmesi
4. İndirme kesilirse **kaldığı yerden devam** (range request) + hash doğrulama

### Dürüst not: FPS'in %70'i mimaride
Dil tavanı yükseltir, 45 FPS'e ulaşmak mimariye bağlı. Rust bile yanlış mimariyle 20 FPS verir.

### Gölge (shadow) — basit ve ucuz

**Açık, ama RTX değil.** Hedef ~1,5-2 ms (16,6 ms bütçenin ~%10'u).

| Yapılır | Yapılmaz |
|---|---|
| Tek yönlü güneş gölgesi (directional) | Işın izleme (ray tracing) |
| 1024×1024 gölge dokusu | 2048+ (yalnız "yüksek" profilde) |
| Tek cascade, 48-64 blok çevre | 4 cascade |
| 2×2 PCF yumuşatma | PCSS / VSM (yumuşak gürültülü gölge) |
| Oyuncu + mob + eşya gölge düşürür | Her blok gölge düşürür |
| Dünya gölge alır | Meşale/nokta ışık gölgesi |
| Sabit %35 karartma | SSAO / SSR / ekran uzayı efektler |

Ayarlar: `Kapalı` / `Açık (1024, varsayılan)` / `Yüksek (2048, yeni cihazlar)`

### Performans için teknik zorunluluklar
- **Chunk boyutu 16×16×16**, sadece görünen chunk'lar mesh'lenir
- **Greedy meshing** (yüz birleştirme) — çizim çağrısı sayısını 10-20× düşürür
- **Render distance 6 chunk sabit**, işlem mesafesi 6,5 chunk (ön-yükleme marjı)
- **Atlas doku** — tek material, tek draw call prensibi
- **Pixel art** (16×16 doku, nearest-neighbor filtre) — hem stil hem performans
- Hedef: **APK 80 MB altı**
- **Ana cihazda 60 FPS taban**, 45 FPS alt sınır (Redmi 7 / Note 7 Pro)

---

## 5. Dinamik içerik ve güncelleme (thin shell)

APK **kabuktur**. Oyunun mantığı ve içeriği sunucudan gelir.

### Üç katman
| Katman | İçerik | Güncelleme |
|---|---|---|
| 0 — Çekirdek (native Rust) | render, chunk meshing, ağ taşıma, input | ❌ APK ister |
| 1 — Oyun kodu (Luau script) | modlar, kurallar, ekonomi, NPC, UI akışı | ✅ anında, sessiz |
| 2 — İçerik (asset) | harita, doku, ses, item tanımı, denge sayıları | ✅ anında, sessiz |

**Her şey dinamik, tek istisna render çekirdeği.** Native kütüphaneyi çalışırken değiştirmek
Android'de crash üretir; onun üstündeki her şey sunucudan gelir.

### Script motoru: Luau (Lua)
Modlar `.luau` dosyası → Supabase Storage'dan indirilir → çalışır.
**Yeni mod eklemek = yeni dosya yüklemek.** APK'ya dokunulmaz.

### APK güncellemesi — Android sürümüne göre

Android 12 (API 31) ile **sessiz kendi kendini güncelleme** geldi. Şartlar:
- Uygulama kendini güncelliyor (installer = kendisi / update owner)
- `UPDATE_PACKAGES_WITHOUT_USER_ACTION` izni
- `REQUEST_INSTALL_PACKAGES` izni verilmiş
- `setRequireUserAction(USER_ACTION_NOT_REQUIRED)`
- Yeni sürüm **yeni izin istemiyorsa** (izin değişirse tek onay çıkar)

| Cihaz / durum | Güncelleme |
|---|---|
| Android 12+ (API 31+) | ✅ **tamamen sessiz** (Play Store'un yaptığı yöntem) |
| Android 8-11 (Redmi Note 7 Pro = Android 10, Redmi 7 = Android 10) | ⚠️ `PackageInstaller` ile tek onay ekranı |
| İzin değişikliği içeren sürüm | ⚠️ tek onay |
| Script + içerik | ✅ **her sürümde sessiz** |

**Not:** Eski cihazlarda da tarayıcıya atmıyoruz — `PackageInstaller` oturumu açılıyor,
Android'in kendi onay ekranı geliyor, tek dokunuş. Play Store dışı dağıtımın (F-Droid,
Neo Store) kullandığı standart yöntem bu.

**Hedef cihaz gerçeği:** Redmi Note 7 Pro resmi son sürüm Android 10 / MIUI 12 → sessiz
güncelleme yok. Android 12+ cihazlarda tam sessiz. İki yol da kodda desteklenir, sürüm
tespit edilip uygun yöntem seçilir.

### Güncelleme akışı
1. Açılışta Supabase'den `manifest.json` çekilir (sürüm, hash, imza, min APK sürümü)
2. Yeni sürüm varsa **arka planda** indirilir (oyun oynanırken)
3. İndirme bitince **Ed25519 imzası** APK'ya gömülü public key ile doğrulanır
4. İmza geçerliyse bir sonraki lobiye dönüşte aktifleşir
5. Bozuk çıkarsa **son çalışan sürüme rollback**

### Güvenlik — zorunlu
1. Storage bucket **read-only public**, yazma sadece service_role
2. Her paket **Ed25519 imzalı**; imza geçmezse yükleme reddedilir
3. Script'ler **sandbox**: dosya sistemi, ağ, native çağrı yok
4. Manifest'te `min_apk_version` → eski çekirdek yeni script'i çalıştırmaz
5. Kademeli dağıtım (canary): %10 oyuncuya aç, hata yoksa %100

---

## 6. Fazlar

### Faz 0 — İskelet (bu onaydan sonra)
- Repo yapısı (Cargo workspace), Rust + wgpu, `cargo-ndk` ile Android APK pipeline
- Supabase şeması: `accounts`, `lobby_presence`, `queues`, `rooms`, `blocks`, `world_snapshots`, `stats`
- Sadece **tek oyunculu** voxel dünya: gez, blok kır, blok koy
- **Çıkış ölçütü:** telefonda 45 FPS, APK telefonda çalışıyor

### Faz 1 — Çok oyunculu çekirdek
- WebRTC mesh + Supabase signaling + Cloudflare TURN fallback
- 2 oyuncu bir odada birbirini görüyor, blok kırma senkron
- **Çıkış ölçütü:** 8 oyuncu aynı odada, blok çakışması yok

### Faz 2 — Lobi + NPC + kuyruk
- Lobi sahnesi, presence, sohbet, NPC + mod seçim paneli, canlı sayaçlar
- Kuyruk dolunca otomatik maç başlatma
- **Çıkış ölçütü:** 2 cihazdan NPC → maç akışı uçtan uca çalışıyor

### Faz 3 — Düello (ilk gerçek mod)
- 1v1 arena, hasar, respawn, skor, maç sonu ekranı
- **Çıkış ölçütü:** oynanabilir düello, stats yazılıyor

### Faz 4 — Takımlı + Mega
- Takım atama, 4v4 ve 6v6 haritalar, takım skoru
- **Çıkış ölçütü:** 8 ve 12 oyuncuyla stabil maç

### Faz 5 — Kalıcılık + kozmetik
- Skin sistemi, kasa, mağaza iskeleti, arkadaş listesi, dünya snapshot/devam
- **Çıkış ölçütü:** hesap cihaz değiştirince taşınıyor

### Faz 6 — Dinamik içerik + dağıtım
- Luau script motoru gömme, manifest/imza doğrulama, arka plan indirme, rollback
- **GitHub = sadece bedava dağıtım** (CI/Actions/pipeline YOK)
  - **Releases** → APK + EXE indirme (sınırsız, CDN'li)
  - **Pages** → craftya sitesi (indirme sayfası, sürüm notları, hash)
  - İçerik paketleri → Supabase Storage
- Uygulama içi sürüm kontrolü + tek dokunuş/sessiz güncelleme
- **Çıkış ölçütü:** oyun açıkken script güncellemesi iniyor, APK'ya dokunmadan aktifleşiyor

### Dağıtım kanalları
| Ne | Nerede | Maliyet |
|---|---|---|
| APK / EXE | GitHub Releases | 0 |
| Web sitesi | GitHub Pages | 0 |
| Doku/harita paketleri | Supabase Storage (1 GB) | 0 |
| Script güncellemesi | Supabase Storage + manifest | 0 |
| Keep-alive ping | UptimeRobot / GitHub Actions cron | 0 |

**Kullanılmayacak:** GitHub Actions build pipeline, CI testleri, Docker, karmaşık workflow.
Build yerelde yapılır, `gh release create` ile dosya yüklenir.

---

## 7. Backend

Supabase projesi kuruldu: `craftya` — ref `dznywlwdvvkqunkmnnas`, bölge eu-central-1.
Detaylar: **BACKEND.md** (API URL, public anahtarlar, şema, seed veriler).

---

## 8. Riskler ve karşılıkları

| Risk | Etki | Karşılık |
|---|---|---|
| Voxel motoru uzun sürer | Oyuncuya geç ulaşırız | Faz 0 küçük tutuldu; dünya, mod ve kozmetik ayrı fazlarda |
| CGNAT yaygın | P2P bağlanamaz | Cloudflare TURN zorunlu katman |
| Lobi 200 tavanı | 201. oyuncu giremez | Kuyruk sistemi |
| Mesh 12+ kişide patlar | Mega keyifsiz | 12 oyuncu tavan, sayı değil "kaos" pazarlaması |
| Sideload güvensiz görünür | İndirme oranı düşer | Site + hash + imza + açık kaynak |
| Hile (uçan adam) | PvP bozulur | Sunucu tarafı hareket doğrulaması (Faz 3) |
| Düşük cihaz ısınması | FPS düşer | Düşük ayar profili, dinamik render distance |
| Sunucudan kod çekme = RCE riski | Tüm oyunculara zarar | Ed25519 imza, read-only bucket, sandbox, rollback |
| Bozuk script tüm oyuncuları kırar | Oyun açılmaz | Canary dağıtım (%10), son çalışan sürüme rollback |

---

## 9. Onay gereken açık maddeler

| # | Madde | Durum |
|---|---|---|
| 1 | Motor: kendi motorumuz — Rust + wgpu (Godot değil) | ✅ onaylandı |
| 2 | Mega tavanı: 4 takım × 3 = 12 oyuncu | ✅ onaylandı |
| 3 | Takımlı: 5 takım × 2 = 10 oyuncu | ✅ onaylandı |
| 4 | İlk mod: Düello mu, Takımlı mı? | ⏳ bekliyor |
| 5 | Survival: kapsam dışı, sonraki sürüm | ⏳ bekliyor |
| 6 | Hub tipi: 3D lobi / panel / hibrit | ⏳ bekliyor |
| 7 | Script motoru: Luau (Lua) | ⏳ bekliyor |
| 8 | APK güncellemesi | ✅ Android 12+ sessiz, eski sürümlerde tek onay |

Bu maddeler netleşince Faz 0'a başlanır.
