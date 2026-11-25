# BRAIN.md — Craftya Sunucu Projesi Hafızası

> Bu dosya, konuşma bağlamı sıfırlanırsa projeye kaldığı yerden devam etmek için var.
> Yeni bir oturumda ilk iş: **bu dosyayı baştan sona oku**, sonra kullanıcıya
> "buraya kadar geldik, şuradan devam ediyoruz" diye tek cümlelik özet ver.
>
> Son güncelleme: 2026-11-25 (oyun modları kesinleşti)

---

## 0. TEK CÜMLELİK DURUM

Kullanıcı **"Sen yap bitir, tüm kodları altyapıyı hallet"** dedi → altyapı YAZILDI.
Bitti: config üretimi, 6 oda, proxy, plugin manifestosu+çözücü, bot (scan/apply/
verify/guard), governor, world-sync, compose, CI, site, Supabase şeması, dokümanlar.
`scripts/doctor.py` = **0 hata**. Commit'lenmedi, push edilmedi (kullanıcı istemedi).

**SIRADAKİ:** kullanıcı "işin bitince bekle, sunucu içi değişiklikleri birlikte
yapacağız" dedi → **oyun içi kurulum (lobi/NPC/arena) kullanıcı isteyince**.
Ayrıca bkz. §7 (eksik kalanlar) ve §13 (bu turun dosya envanteri).

Kararlaşan son şeyler: 6 oda (lobi/survival/skyblock/bedwars/düello/parkur),
creative iptal, BedWars haritası "Maden Çekirdeği" (dev bloğun içi mağara,
4 kapı + 2 gizli tünel), global tek ekonomi, zorunlu sezon sıfırlama yok.

---

## 1. PROJE NE

**Craftya** — Minecraft sunucusu. Kullanıcının kendi Minecraft istemcisinden
(Java **ve** Bedrock/telefon) girilecek. Hedef: *profesyonel, 0 TL, kartsız,
kullanıcının kendi bilgisayarına ihtiyaç duymayan* sunucu.

Konuşmanın başında "Craftya" adında bir mobil oyun motoru/uygulaması vardı
(C++ motor, Android APK, modern menü — v3.0.0-b38'e kadar çıkmıştı).
**Kullanıcı "özel uygulama yok, sadece sunucu" dedi ve tüm repoyu sildirdi.**
O iş **rafa kalktı**, gerekirse git geçmişinden geri alınır (bkz. §9).

---

## 2. DEĞİŞMEZ KURALLAR (kullanıcının açık talimatları)

1. **Onay olmadan eylem yok.** Kullanıcı: *"kodlama araştırma benden onay almadan"*.
   Kod yazma, web araması yapma, dosya oluşturma → önce sor.
2. **0 TL.** Kullanıcı: *"Lan 0 tl ile accaz diyom sen bana para li servis diyon"*.
   Ücretli hiçbir servis önerilmez (Spectrum, kiralık host vb. konu dışı).
3. **Kart yok, kullanılamaz.** Kullanıcı: *"kart yok olsa bile Oracle kabul etmez"*.
   Kart isteyen her servis **elenir** (Oracle, GCP, AWS, Azure, Koyeb, R2).
4. **Kullanıcının kendi PC'si kullanılamaz.** *"Kendi pc mi kullanamam"*.
   → Sunucu GitHub Actions runner'ında koşacak.
5. **Domain var: `craftya.dev`** (Cloudflare'de). Port sorunu yok, adres üstünden gidilecek.
6. **Cracked sunucu olacak** (online-mode kapalı). → AuthMe zorunlu (bkz. §5).
7. **Hub olacak: 5-6 sunucu/oda.**
8. **Supabase'e bağlanacak + SQLite altyapısı + web sitesi** — kullanıcının fikri,
   onaylandı (mimarisi §6'da).
9. **Türkçe her şey**: MOTD, mesajlar, menüler, eklenti dili. Ayrıca sunucuya
   özel marka (renk, isim, karşılama).
10. **Dil/ton:** Türkçe, samimi ("kanka"), abartısız, teknik olarak dürüst.
   Bilinmeyen şey "bilmiyorum" denir; uydurulmaz.

---

## 3. MİMARİ KARARLARI (alındı, tartışma kapandı)

### 3.1 Nerede koşacak
```
GitHub Actions runner (public repo) → 16 GB RAM / 4 çekirdek
  + playit.gg tunnel → sabit adres (secret sabit kaldıkça)
  + craftya.dev DNS'te SRV kaydı → oyuncu sadece "mc.craftya.dev" yazar
```
**Neden Oracle değil:** kart istiyor, kullanıcıda kart yok/geçmiyor.
**Neden Falix/Aternos değil:** kullanıcı 89 saat sıra bekledi, uyuyorlar.
**Neden Fly/Koyeb/Render değil:** ücretsiz katmanları öldü/uyuyor/kart istiyor.

### 3.2 Hub yapısı — TEK runner, TEK makine
```
Velocity (ön kapı, 25565)
 ├── lobby      (adventure, 1 GB)   → NPC + oda seçimi
 ├── survival   (survival,  3 GB)   → seed craftya
 ├── skyblock   (survival,  2 GB)   → void dünya
 ├── bedwars    (adventure, 2 GB)   → "Maden Çekirdeği" haritası
 ├── duello     (arena,    1.5 GB)  → 1v1
 └── parkur     (adventure, 0.5 GB) → Çekirdek Parkuru
                        toplam ≈ 10 GB (16 GB'de 6 GB pay)
```
**Not:** creative ve pvp odaları İPTAL edildi (bkz. §4.1, §4.4).
**Kritik:** 6 ayrı runner = 6 ayrı makine = 20-80 ms gecikme. **Yanlış.**
Doğrusu: tek runner içinde Velocity + 6 Paper, hepsi localhost → 0 ms geçiş.

### 3.3 6 saatlik devir (GitHub Actions limiti)
```
05:50  duyuru (chat + title + Discord): "10 dk sonra bakım"
05:58  "2 dk"
05:59  /save-all flush → dünya tar.gz → **artifact** olarak yükle
06:00  yeni runner: artifact indir → aç → Paper başlat (warm standby ile ~30 sn)
       playit aynı secret → adres DEĞİŞMEZ
06:01  durum sayfası: "bakım tamamlandı (34 sn)"
```
**Ölçülen hedef:** 25-35 sn kesinti. Sıfır olamaz (Minecraft oturumu process'e bağlı,
proxy de 7/24 duramaz). **Oyuncu hiçbir şey kaybetmez** (envanter/konum/XP Paper'da).
**Yedek yeri:** GitHub **artifact** (10 GB/dosya, 90 gün). **Drive YOK** — kullanıcı
haklı olarak "uğraştırır" dedi; Drive'a otomatik atmayı da agent yapamaz (bkz. §8).
**İletişim dili:** "gizli çökme" değil, **şeffaf planlı bakım** (durum sayfasında yazar).

### 3.4 Cloudflare'in sınırı (bunu tekrar tartışma)
Cloudflare ücretsiz planı **HTTP/HTTPS** proxy'ler. Minecraft **ham TCP 25565**
(+ Bedrock **UDP 19132**) konuşur → proxy'den geçmez. Spectrum ücretli.
```
mc.craftya.dev        A → DNS-only (gri bulut)       → Java oyuncusu
_minecraft._tcp.mc    SRV → playit adresi/portu      → port gizleme
craftya.dev + panel   Cloudflare Tunnel (proxied)    → sadece WEB tarafı
mc.craftya.dev:19132  Bedrock (SRV yok, port yazılır)
```

---

## 4. OYUN MODLARI VE HARİTA TASARIMI (KESİNLEŞTİ)

### 4.1 Oda listesi — 6 oda, karar verildi

| # | Oda | Mod | RAM | Kapasite | Not |
|---|---|---|---|---|---|
| 1 | **Lobi** | adventure | 1 GB | 100 | NPC + oda seçimi, Citizens |
| 2 | **Hayatta Kalma** | survival | 3 GB | 20 | Ana oda, seed `craftya`, Lands/CoreProtect |
| 3 | **Gökyüzü Adası** | survival | 2 GB | 20 | BentoBox, void dünya |
| 4 | **BedWars** | adventure | 2 GB | 12 | 6 renk × solo(1) + doubles(2) |
| 5 | **Düello** | arena | 1.5 GB | 8 | 1v1 (2v2 sonra) |
| 6 | **Çekirdek Parkuru** | adventure | 0.5 GB | 30 | **agent seçti** — gerekçe §4.4 |
| | | | **10 GB** | | 16 GB'de **6 GB pay** |

**Creative ELENDİ** (kullanıcı: "çok sıkıcı" + cracked sunucuda moloz tarlası olur).

### 4.2 BedWars haritası: "Maden Çekirdeği" (kullanıcının konsepti)

Kullanıcının tarifi: *"herkes farklı bir maden bloğu üstünde olur, dev bloğun
en ortasında 4 tarafında girişi olan dev gibi bir mağaramsı yer olur"*.

**6 ada, altıgen dizilim (60° aralık), merkez yarıçap ~55 blok → arena 130×130×21.**

Takım → zemin bloğu eşleşmesi (agent düzeltti: elmas=turkuaz, demir=beyaz):

| Takım | Zemin | Renk kodu |
|---|---|---|
| Kırmızı | Redstone bloğu | `§c` |
| Mavi | Lapis bloğu | `§9` |
| Sarı | Altın bloğu | `§e` |
| Yeşil | Zümrüt bloğu | `§a` |
| Turkuaz | **Elmas** bloğu | `§b` |
| Beyaz | **Demir** bloğu | `§f` |

**Ada içeriği (her biri aynı):** 9×9 platform, ortada yatak (2×1), yanında takım
jeneratörü, 1 köylü (yükseltme). **Jeneratör hızları TÜM takımlarda aynı** —
sadece zemin görseli farklı (adalet).

**Merkez — dev blok:** 33×33×21 dış kabuk (dev elmas cevheri görünümlü), içi oyulmuş
mağara: sarkıtlar, parlayan kristal (deniz feneri ile sınırlı → lag yok), taş duvar.
İçinde 2 elmas jeneratörü + **ortada "çekirdek" kristal sütun** (görsel odak + 4 yön).

**Girişler — KARAR: B şıkkı (4 büyük kapı + 2 gizli yan tünel)**
- Dışarıdan 4 kemerli kapı (K/G/D/B), 3 blok yüksek → kullanıcının istediği görüntü
- Çaprazdaki 2 takım için **alçak, dar yan tünel** (2 yüksek) → adalet
- Neden: 4 kapı tam simetrik olsa çapraz 2 ada merkeze uzun yoldan gider, "önce varan
  elması alır" kuralı yüzünden doğrudan oyun kaybeder. 6 kapı ise "4 taraflı" hissini
  bozar. **Görsel 4, oyunda 6.**

**Kurallar:** yatak kırılınca respawn yok · son takım kazanır · maç üst sınırı 25 dk ·
boş takım 3 dk sonra dağılır · solo ve doubles ayrı arena dosyası (aynı harita, farklı
takım boyutu).

**Harita ÜRETİMİ script ile** (elle inşa günler sürer): aynı seed → aynı harita, her
devirde yeniden üretilebilir. Dünyayla birlikte artifact'a yedeklenir.
BedWars1058 arena tanımı botla: 6 ada × (spawn + yatak + jeneratör) ≈ 30 konum →
`/bw setupArena` komutları sırayla gönderilir, sonunda "arena ready" doğrulanır.

### 4.3 Düello 1v1
Ayrı ve hafif oda. **2 kişiyle anında döner**, beklemez (BedWars'ın aksine).
Kit seçimi (demir/bow/elmas), 3 raunt, skor tablosu. 2v2 aynı odada sonra açılır.

### 4.4 6. oda neden "Çekirdek Parkuru"
- **CPU ≈ 0**: parkur plugini neredeyse yük üretmez → 6 GB payı bozmaz
- **Tek oyuncu oynar**: boş sunucu sorununun tek gerçek çözümü
- **Tema bağı**: BedWars merkezindeki dev bloğun içinden geçen parkur → aynı harita
  üreticisini yeniden kullanır, ekstra tasarım maliyeti yok
- **Hile riski yok** (KitPvP'nin aksi), GrimAC yükü artmaz
- KitPvP elendi: cracked sunucuda en çok hile şikayeti gelen mod
- Arsa elendi: creative'in "sıkıcı" sorununu çözmez

### 4.5 Ekonomi ve sezon (agent kararı)
- **Tek global para** (Supabase): oda geçince para seninle gelir. Sunucuya özel his.
- Yerel eklenti hesapları (Vault) global bakiyeyi **aynalar**, kaynak Supabase.
- **Zorunlu sezon sıfırlama YOK.** Dünya elle "sezon arşivi" alınabilir (artifact +
  README). Oyuncu kaybı korkusu olmadan oynasın.

### 4.6 PAYLAŞILAN BELLEK HAVUZU (kullanıcı isteği — kesin tasarım)

**Kullanıcı:** *"bedwars çok yoğun ya ona 5 gb'lik alanı da açalım, orayı da kullansın;
yoğun olan yere versin RAM'i, dağıtsın o 5 GB'ı yoğunluğa göre."*

**Doğru model: sabit bölüşüm DEĞİL, ortak havuz.** 6 GB pay → 1 GB emniyet + **5 GB
havuz**. Havuz kimsenin malı değil; governor her 30 saniyede kime vereceğine karar verir.

```
TOPLAM 16 GB
├── 1.5 GB   işletim sistemi + docker
├── 4.5 GB   odaların TABANI (herkesin her zaman aldığı)
├── 5.0 GB   >>> HAVUZ <<<  (yoğun olan odaya, canlı olarak)
└── 5.0 GB   boşta / yedek (ani yük, dünya kaydı, yedekleme anı)
```

**Nasıl "canlı" dağıtılır — iki gerçek mekanizma:**

1. **`jcmd VM.set_flag SoftMaxHeapSize <N>`** → JVM çalışırken heap'in yumuşak
   üst sınırını değiştirir. **Restart YOK.** Java 12+ özelliği, Paper'da çalışır.
   Boş oda tabanına iner, yoğun oda havuzdan pay alır.
2. **`docker update --memory <N>`** → cgroup limitini çalışırken değiştirir.
   Havuzun *fiziksel* sınırı burada zorlanır (JVM kendini sınırlamazsa OOM).

**Kritik teknik gerçek (bunu bilmeden tasarım yanlış olur):**
`-Xmx` (sert tavan) **açılışta sabitlenir, canlı değişmez.** Bu yüzden her odanın
`Xmx`'i **havuzu da kapsayacak şekilde yüksek** başlatılır (bedwars `-Xmx=5G`).
Gerçek kullanımı `SoftMaxHeapSize` + cgroup kontrol eder. Yani "5 GB izin var ama
ancak hak edersen kullanırsın".

| Oda | `Xmx` (izin) | Taban (SoftMax) | Havuzdan alabileceği |
|---|---|---|---|
| bedwars | 5 GB | 768 MB | **+4.2 GB** |
| survival | 5 GB | 1.5 GB | +3.5 GB |
| skyblock | 4 GB | 768 MB | +3.2 GB |
| duello | 3 GB | 512 MB | +2.5 GB |
| lobby | 2 GB | 384 MB | +1.6 GB |
| parkur | 1.5 GB | 256 MB | +1.2 GB |
| velocity | 512 MB | 384 MB | — (proxy, havuza girmez) |

**Governor algoritması (`scripts/mem-governor.sh`, 30 sn):**
```
her oda için: oyuncuSayisi (RCON list), heapKullanim (docker stats), TPS (spark/rcon)
talepPuan = oyuncu*1.0 + (20-TPS)*1.5 + heapBaski*0.5
1. ÖNCE: boş odaların SoftMax'ını tabana indir  (havuzu boşalt)
2. SONRA: talepPuan'a göre havuzu dağıt (aç oda öncelikli)
   - alıcı oda: jcmd SoftMaxHeapSize = taban + pay
   - aynı anda docker update --memory = SoftMax + 0.5 GB
3. KURAL: havuz toplamı 5 GB'ı aşamaz → aşarsa en düşük puanlı odadan kes
4. KURAL: oyuncusu olan odanın payı, o oda boşalmadan ASLA tabanın altına inmez
5. LOG: her karar logs/mem-governor.log'a (kim ne kadar aldı, neden)
```

**Örnek (senin senaryon):**
```
20:55  survival 6 oyuncu, bedwars 0     → survival 3.5 GB, bedwars 768 MB
21:00  BedWars etkinliği başladı, 12 oyuncu bedwars'ta
       → bedwars SoftMax 768 MB → 4.8 GB (havuzdan +4 GB), survival 3.5 → 1.5 GB
       → 15 saniyede yeni dağıtım, RESTART YOK, oyuncu düşmedi
21:40  etkinlik bitti, bedwars 0 oyuncu
       → bedwars 768 MB'a indi, havuz boşaldı, survival isterse alır
```

**Emniyet kuralları:**
- Havuzun **1 GB'ı asla dağıtılmaz** (ani yük + dünya kaydı için)
- `docker update --memory` **softmax+0.5 GB**'ın altına inmez → OOM-kill olmaz
- Governor çökerse: herkes tabanında kalır, sunucu **çalışmaya devam eder** (fail-safe)
- Sadece **boş** odanın tabanı düşürülür; dolu oda asla küçültülmez
- BedWars maçı sırasında bedwars'a dokunulmaz (etkinlik modu bayrağı)

**Ölçüm:** governor, her dağıtımı ve o anki TPS'i loglar → "RAM dağıtımı işe yaradı mı"
sorusunun cevabı tahmin değil, sayı olur.

### 4.7 Eklenti seti (kesinleşen)
**Çekirdek:** Paper · ViaVersion + ViaBackwards · Geyser + Floodgate · **AuthMe** ·
LuckPerms · GrimAC · CoreProtect · spark · Chunky · TAB · Vault
**Proxy:** Velocity (offline mode, forwarding secret)
**Modlar:** BedWars1058 · BentoBox (skyblock) · Citizens + HolographicDisplays (lobi) ·
Lands veya GriefPrevention (survival koruma) · parkur plugini
*Türkçe çevirisi olmayan eklenti çıkarsa: ya çeviri dosyası yazarım ya alternatif seçerim
— kurulum sırasında tek tek söylenir.*

**Bot yetkileri:** LuckPerms grubu `bot`, prefix `[BOT]`, **`op` VERİLMEZ**,
sadece `kick/mute/tp`, GrimAC bypass sadece bu grupta.

## 5. CRACKED SUNUCU — ZORUNLU ÖNLEMLER

Cracked = **isim = kimlik**. Biri "Kurucu" diye girip yetki devralabilir.
- **AuthMe**: `/register` + `/login`, şifresiz hareket/blok/komut yok
- **İsim rezervi**: `Kurucu`, `Admin`, `craftya-bot` kilitli
- **Bot**: ilk girişte `/register`, sonra `/login` (otomatik)
- **Ban**: isim + IP + subnet (isim değiştirmek 10 saniye)

---

## 6. VERİ KATMANI — 3 katman (kullanıcının "Supabase + SQLite" fikri)

| Katman | Nerede | Ne | Neden |
|---|---|---|---|
| Yerel | her odanın diski, **SQLite** | CoreProtect, bölgeler, arena dosyaları | SQLite tek yazıcı; paylaşılırsa **bozulur** |
| Global | **Supabase (Postgres)** | profil, rank, **global ban**, istatistik, kozmetik | 6 oda aynı gerçeği görsün |
| Web | **Cloudflare Pages** | site, liderlik, canlı durum | bedava, hızlı |

**Kritik:** Minecraft eklentileri **Postgres bilmez** (SQLite/MySQL). Doğrudan
Supabase'e bağlanmaz → **Cloudflare Worker köprüsü** + **toplu yazma** (5 dk'da bir;
yoksa 100k istek/gün 1 günde biter). `service_role` anahtarı **asla** eklentiye/siteye
gömülmez — anon anahtar + RLS.

**Kimlik bağı:** oyun = isim + AuthMe şifresi; site = Discord (Supabase Auth);
`/link` komutu → tek kullanımlık kod → hesap eşleşir.

**Mevcut Supabase projesi:** `craftya` — ref `dznywlwdvvkqunkmnnas`, eu-central-1,
durum ACTIVE_HEALTHY. Yeni proje açılmayacak, bu kullanılacak.
(Not: free plan boşta 1 hafta kalırsa uyur.)

---

## 7. SIRADAKİ İŞLER (onay gelince bu sırayla)

```
1. tools/craftya-bot/  → package.json, config.example.json, src/bot.js, src/scan.js
   (GÖZÜMÜZ: dünyayı JSON'a döker — blok, NPC, tabela, spawn)
2. world-sync.sh + .github/workflows/hub.yml → 6 saatlik devir, artifact, duyuru
3. Velocity + 6 oda docker-compose + her odanın config'i
4. Lobi/NPC kurulumu (bot scan → plan → apply → verify), LuckPerms tag/moderasyon
5. Supabase şeması + Worker köprüsü + site (Pages)
```

**Neden bot önce:** sunucuyu kuracak ama onu okuyamayacaksak kör kalırız.

**Kullanıcıdan gereken secret'lar** (GitHub Secrets'a kendisi ekler, bana yazmaz):
`PLAYIT_SECRET`, `VELOCITY_SECRET`, `RCON_PASSWORD`, `SUPABASE_URL`,
Supabase storage anahtarı. (Drive yok.)

---

## 8. YAPABİLDİKLERİM / YAPAMADIKLARIM (dürüst sınır)

**Yapabilirim:** tüm config/kod/script yazımı, Türkçe çeviri dosyaları, şema tasarımı,
teşhis (log okuyup sebep bulma), CI ile sözdizimi doğrulama, plan/tablo çıkarma.

**Yapamam:**
- **Oyun içine giremem**, ekran görmem, tıklamam (Minecraft istemcim yok).
  → Çözüm: `mineflayer` botu **kullanıcının runner'ında** çalışır; bot okur,
  **ben JSON'u yorumlarım**; bot uygular (`fill`/`setblock`/`summon`), ben doğrularım.
- **Bu sandbox'ta Docker/Java/npm sunucu çalıştıramam**; oturum bitince ortam ölür.
  → Sunucunun gerçek çalışması kullanıcının runner'ında test edilir.
- **Drive'a dosya atamam** (dış servis erişimi yok). → Artifact kullanıyoruz.
- **Hesap açamam** (GitHub/Cloudflare/playit) — kullanıcı açar.
- **Karar veremem**: hile şüphesinde son karar insan (yanlış ban = güven biter).

---

## 9. GEÇMİŞ / GERİ DÖNÜŞ NOKTALARI

Repo: `cnrcrm20-ctrl/craftya`, dal: `arena/deab440e-craftya`, taban: `e893f28`.

| Commit | Ne |
|---|---|
| `8b997ed` | mcserver/ (Paper + Geyser + yedek + denetim) — **silinen son hâl** |
| `ffbcb85` | v3.0.0-b38 doküman |
| `9d1265a` | v3.0.0-b37 sunucu merkezli modern menü (mobil uygulama) |

**Dosyalar silindi ama HENÜZ COMMIT EDİLMEDİ.** (97 dosya `D` olarak sahnelendi.)
- Geri almak: `git reset` → her şey geri gelir
- Kalıcı yapmak: `git commit -m "temiz baslangic"`
- Eski `mcserver/` dosyaları işe yarar (Paper config'leri, Geyser ayarı, doctor.py,
  fetch-plugins.sh) → `git show 8b997ed:mcserver/...` ile tek tek alınabilir.

**Yeniden kullanılabilir eski parçalar:**
`mcserver/config/{server.properties, bukkit.yml, spigot.yml, paper-global.yml}`,
`mcserver/scripts/{fetch-plugins.sh, doctor.py}` — hepsi test edilmişti.

---

## 10. AÇIK SORULAR (cevap gelmedi, varsayılan var)

**Cevaplananlar (artık sorma):** oyun modları (§4.1), 6. oda = parkur (§4.4),
bedwars harita konsepti (§4.2), giriş tipi = 4 kapı + 2 yan tünel (§4.2),
creative iptal, ekonomi = global tek para, zorunlu sezon sıfırlama yok.

| Hâlâ açık soru | Varsayılan (aksi söylenmezse) |
|---|---|
| Minecraft sürümü sabit mi, LATEST mi? | **Sabit** (dünya her 6 saatte yeniden açılıyor; sürüm atlarsa bozulur) |
| Whitelist açık mı? | **Kapalı** (ilk gün herkes girsin), AuthMe zorunlu |
| Discord sunucusu var mı? | Yok say, sonradan eklenir (moderasyon botu opsiyonel) |
| BedWars solo + doubles ikisi de mi? | **İkisi de** (aynı harita, ayrı arena dosyası) |
| Düello 2v2 olsun mu? | Şimdilik **hayır**, sonra açılır |

---

## 11. DOĞRULANMIŞ vs DOĞRULANMAMIŞ BİLGİ

**Bu oturumda doğrulandı (kaynağı güvenilir):**
- Paper 26.x Java 25 ister; Geyser 26.3'ü destekliyor (üçüncü taraf 2026 yazıları)
- ViaVersion son sürüm: **5.12.3** (GitHub API ile çekildi)
- Supabase free: 2 proje, 500 MB DB, 5 GB egress, 200 realtime bağlantı, 7 gün boşta uyur
- Cloudflare Workers/Pages free: 100k istek/gün, bedava bant genişliği; DO free SQLite
- Oracle Always Free 2026'da **2 OCPU/12 GB**'a düşürüldü + idle reclamation (7 gün)
- Fly.io ücretsiz katmanı bitti (Eki 2024), Koyeb kart istiyor (Şub 2026)

**Doğrulanmadı (sandbox'ta API kapalı — PaperMC, Modrinth, geysermc erişilemedi):**
- Kesin Paper sürüm numarası / Geyser build numarası
- Aternos/Minehut/Falix güncel limitleri (kullanıcı deneyimi: 89 saat sıra)
- playit.gg ücretsiz planda TCP kısıtlaması durumu (2025'te kısıtlama hareketi olmuş)
- Termux + Paper telefon kurulumu

**Kural:** Bu maddelerden biri kritikse → önce arama yap (izin iste), sonra yaz.

---

## 12. YENİ OTURUMDA İLK 3 ADIM

1. Bu dosyayı oku, kullanıcıya 2 cümlelik "nerede kaldık" özeti ver.
2. `git status` → silme commit edilmiş mi, workspace boş mu, kontrol et.
3. **"Başla" onayı var mı?** Yoksa kod yazma; sadece §7'deki planı hatırlat ve
   §10'daki varsayılanları teyit ettir.


---

## 13. YAZILAN ALTYAPI (2026-11-25 turu)

`doctor.py` 0 hata. Docker/Java olmadığı için **sunucu burada çalıştırılmadı**;
config/YAML/JS/bash/python sözdizimi ve uçtan uca config üretimi doğrulandı.

```
README.md  BRAIN.md  docs/{PLUGINS,KURULUM,OPERASYON}.md
.env.example  .gitignore  docker-compose.hub.yml
config/
  templates/{server.properties,bukkit.yml,spigot.yml,paper-global.yml}.tpl
  velocity/velocity.toml.tpl
  rooms/<6 oda>/room.yml                  (port, mod, xmx, softmax, seed, motd)
  plugins/{AuthMe,LuckPerms,TAB,EssentialsXChat,Geyser-Spigot,floodgate,Essentials}/
scripts/
  render.py       şablon + .env → runtime/ (jvm.args, server.properties, velocity.toml)
  plugins.mjs     Modrinth/Geyser/PaperMC API ile sürüm çözümü + sha512 doğrulama
  doctor.py       7 bölüm ~25 kontrol
  mem-governor.sh 5 GB havuz, jcmd SoftMaxHeapSize (restart YOK), cgroup
  world-sync.sh   save/upload/restore/status, sha256 bütünlük
  mc.sh           up/down/logs/health/rcon/backup/restore/doctor/bot/players
  fetch-plugins.sh  (eski/yedek: sabit URL ile indirme)
plugins/manifest.json   19 plugin (12 zorunlu) — slug ile çözülür, URL sabit değil
tools/craftya-bot/      package.json, config.example.json, src/{lib,scan,apply,verify,guard}.js,
                        out/plan.ornek.json
supabase/schema.sql     6 tablo + RLS + view + register_player()
web/index.html          canlı durum (dürüst: veri yoksa "bilinmiyor", sahte online yok)
.github/workflows/{hub.yml,ci.yml}
```

**Doğrulanan ölçümler:** 6 oda config üretildi, RCON portları ayrık (25575–25580),
velocity.toml'da 6 oda kayıtlı, compose'da sadece 25565 + 19132/udp dışa açık,
`jvm.args` her odada `SoftMaxHeapSize` içeriyor, yedek→geri yükleme turu sha256 ile geçti.

**Tag sistemi (kullanıcı isteği: "kafada ve chatte görünsün, havalı dursun"):**
tek kaynak `LuckPerms/groups.yml` → `prefix` → TAB (kafa) + EssentialsXChat (chat)
+ nametag. Gruplar: kurucu (koyu kırmızı + ★), admin, mod, vip, oyuncu (tag yok),
bot (`[BOT]`). Ayırıcı `»`. Ayrıntı: `config/plugins/LuckPerms/README-taglar.md`.
EssentialsXChat bu yüzden zorunlu listeye alındı; doctor artık "tag render edildi mi"
ve "prefix üç katmanda tutarlı mı" kontrolünü yapıyor.

**Renk geçişleri (gradyan) — kullanıcı "elini korkak alıştırma" dedi:**
`scripts/gradient.py` eklendi. Minecraft'ta gradyan yok → her harfe hex.
Tek kaynak `PALETLER` (kurucu ateş, admin kırmızı-turuncu, mod mavi-buz,
vip altın, bot turkuaz, marka yeşil→turkuaz→mavi). Geçiş **doğrusal ışık
uzayında** (sRGB→lin→lerp→sRGB), yoksa arada çamur ton çıkıyor.
Config'ler `# >>> GRADIENT:BEGIN/END` bloklarıyla işaretli, `--apply` doldurur,
`--check` CI'da drift yakalar, `--preview` → `docs/tag-onizleme.html`.
**Sınır:** `server.properties` MOTD hex kabul etmiyor → `LEGACY_RAMP` ile
16 renge bantlı geçiş. `render.py` MOTD'u `motd:` (saf metin) + gradyandan kurar.

**Sır sayısı 2'ye indi:** `PLAYIT_SECRET`, `SUPABASE_ANON_KEY`.
`VELOCITY_SECRET` + `RCON_PASSWORD` her çalıştırmada `render.py` üretir.
`service_role` **hiç kullanılmıyor**.

## 14. Denetim turu — bulunan ve düzeltilen 6 hata (2026-11-25)

Kullanıcı "her şey hazır mı" diye sordu; denetim **hazır değil** dedi. Düzeltilenler:

| # | Hata | Etki | Düzeltme |
|---|---|---|---|
| 1 | **Geyser tüm odalara kuruluydu** | Bedrock oyuncusu **hiç giremezdi** (port çakışması, auth bozuk) | Geyser+Floodgate → `rooms: ["proxy"]`, `platform: "velocity"`, jar'lar Velocity'ye, `19132` odalardan kaldırıldı |
| 2 | Sürüm `1.21.4` ama imaj `java21`; BRAIN 26.3 diyordu | 26.3 java25 ister → sunucu **hiç açılmaz** | `MC_VERSION=26.3`, `MC_IMAGE/PROXY_IMAGE` env'e taşındı, doctor sürüm↔Java eşleşmesini kontrol ediyor |
| 3 | `coreprotect` odası `creative` | var olmayan oda (creative iptal edilmişti) | `survival, skyblock` |
| 4 | `essentialsx` sadece 3 odada | diğer 3 odada `/spawn` `/tpa` yok | `all` |
| 5 | Floodgate mount `:ro` | `key.pem` üretilemez → Floodgate ölür | `:rw` |
| 6 | `fetch-plugins.sh` (sabit URL, 404) | ölü kod | silindi, tek yol `plugins.mjs` |

Ayrıca: `server.properties` MOTD hex kabul etmediği için `LEGACY_RAMP`;
Velocity MOTD'u MiniMessage `<gradient>…</gradient>` (kapatma etiketi şart,
yoksa ayrıcı da gradyana girer); `plugins.mjs` Geyser için sha256 doğrulaması yapıyor.

**Yeni doctor bölümleri:** `4b/7 sürüm + Bedrock mimarisi` (8 kontrol),
`6b/7 tag render`, `6c/7 gradyan`. Toplam ~40 kontrol.
**CI:** Geyser `rooms==["proxy"]` + `platform=="velocity"` assert;
19132 sadece velocity'de (oda portlarında yasak).

## 15. Komutlar: /hub · /lobby · /lobi · /l · /merkez

**Kullanıcı tanımı:** "lobi olan oda = hub, tüm modları birbirine bağlayan çekirdek oda."
Doğru: `lobby` (port 25566), `velocity.toml` içinde `try = ["lobby"]`.

### İki katman — neden

| Katman | Nerede | Ne | Neden orada |
|---|---|---|---|
| 1 | **proxy** | `/hub`, `/lobby` | 6 odada birden geçerli, tek kopya, oda başına bakım yok |
| 2 | **her oda** | `/lobi`, `/l`, `/merkez` → `/hub` | proxy plugin Türkçe alias tanımıyor |

Katman 1: `velocity-hub-command` (`rooms:["proxy"]`, `target:"proxy-plugin"`),
config `hubServerName: lobby` + `useLobbyAlias: true`.
Katman 2: `config/rooms/_common/commands.yml` → her odanın `commands.yml`'i
(Essentials `aliases`). `render.py` dağıtır.

### Yolda yakalanan tuzaklar

1. **`_common` oda sanıldı** → `render.py` `config/rooms/*` altındaki her dizini oda
   sayıyordu. Sonuç: 7 "oda", **portlar ve RCON numaraları kayıyor**, velocity.toml'a
   olmayan `_common` sunucusu giriyordu. Çalışan sunucuyu bozardı.
   Çözüm: `room_dirs()` — `_` ile başlayan dizin oda değil. 3 yerde tek kaynak.
2. **`/server/plugins` üç ayrı mount** → dosya mount'u dizin oluşturur, `:ro` yaparsan
   proxy kendi yazamaz. Çözüm: **tek dizin** `runtime/proxy-plugins` (jar+config),
   `PLUGINS: ""` (itzg kendi indirmesin → çift indirme/sürüm kayması olmasın).
3. **Plugin yüklenmezse sessiz ölüm** → `/hub` oyunda hiç görünmez, oyuncu "komut yok"
   der. Çözüm: `hub.yml`'de yeni adım — velocity log'unda `velocity-hub-command`,
   `Geyser`, `Floodgate` aranır, biri yoksa **iş başarısız** olur.

### Doğrulama

- `doctor.py` §4b: hub hedefi gerçek oda mı, `/lobby` alias açık mı, jar proxy'ye mi
  gidiyor, `/lobi /l /merkez` 6 odada tanımlı mı (4 yeni kontrol)
- `ci.yml`: oda sayısı **tam 6**, `_common` oda değil, hub hedefi ∈ odalar,
  proxy plugin olduğu assert
- `hub.yml`: 17 adım, plugin yükleme kanıtı log'dan

## 16. AuthMe → LibreLogin (kullanıcı: "eski değil mi, modern alternatif" + "odada şifre sormasın")

**Asıl şikâyet:** her oda geçişinde şifre. **Kök neden:** AuthMe oda başına
kuruluyordu → 6 plugin, 6 SQLite, 6 oturum. Yeni sunucu "giriş yapmış" bilmiyordu.

| | AuthMe | LibreLogin |
|---|---|---|
| Yer | her odada | **proxy'de tek** |
| Oda geçişi | şifre **her seferinde** | yok (oturum 30 gün) |
| DB | 6 ayrı SQLite | tek |
| Bedrock | şifre zorunlu | **şifresiz** (Floodgate) |
| 2FA | eklenti | yerleşik TOTP |
| Hash | BCrypt | **Argon2id** |
| Bekleme | oyun dünyasında | **ayrı limbo odası** |

**Yeni mimari:** `auth` (limbo) odası — port 25572, **sadece 127.0.0.1**,
boşluk dünyası (`generator-settings` void), 768M, komut kısayolu YOK.
LibreLogin: `limbo: [auth]`, `lobby.root: [lobby]`, `session-timeout: 2592000`,
`floodgate.enabled: true`, `ip-storage-mode: HASHED` (KVKK).

**Bedel, açıkça:** +1 konteyner, +768 MB. 16 GB bütçede kaldı (toplam ~17.8 GB
Xmx ama SoftMaxHeapSize havuzu yönetiyor; auth 768M'in çoğu boş).

### Yolda yakalanan tuzaklar (4 hata)

1. **`.env` port değerine yorum sızdı** → `PORT_AUTH=25572  # limbo...` değerin
   parçası oldu. Çözüm: yorum ayrı satıra.
2. **`generator-settings` şablonda yoktu** → boşluk dünyası üretilmedi. Çözüm:
   render satırı `level-type=` altına **koşullu** ekler; boşsa hiç yazmaz
   (boş `generator-settings=` sunucuyu bozar).
3. **`GENERATOR_SETTINGS` tanımsız** → `expand()` patladı, tüm render durdu.
   Çözüm: `__NONE__` sentinel + boşsa satırı düşür.
4. **CI'da `'...'` fazladan tırnak** → Python assert satırı bozuldu. Çözüm:
   YAML'dan çözüp çalıştırınca yakalandı (yerelde birebir test şart).

### Doğrulama
`doctor.py` §4c (8 kontrol): AuthMe yok, LibreLogin proxy'de, limbo gerçek oda,
lobby hedefi, oturum >0, Floodgate açık, auth konteyneri var, 25572 localhost.
`ci.yml` 14 adım, hepsi yerelde YAML'dan çözülüp çalıştırıldı → ✓
`hub.yml`: 7 oda bekleniyor (6 oyun + limbo), LibreLogin yüklemesi log'dan kontrol.

## 17. Son kontrol turu — 2 kritik hata (sunucu açılmadan önce)

### Hata 1: RCON portları kaydı (sessiz, tehlikeli)
`auth` odası eklenince `render.py` RCON portlarını yeniden numaralandırdı
(alfabetik: auth 25575, bedwars 25576 … survival 25581) ama `mc.sh` içindeki
**elle yazılmış harita** eskiydi. Sonuç: `mc.sh rcon lobby tps` yazınca başka
sunucuya bağlanırdı. **Çözüm:** harita silindi, `rcon_port()` artık
`runtime/rooms/<oda>/room.meta.json` okuyor (render.py yazar) → tek kaynak.

### Hata 2: bellek bütçesi aşımı (sunucu açılırdı ama ölürdü)
İki katmanlı hata:
- **`MEMORY` = `Xmx`** veriliyordu → JVM'in heap dışı ihtiyacı (metaspace, GC,
  thread, direkt buffer) için yer yok → Docker **OOM-kill**, "sunucu sebepsiz ölüyor"
- **Toplam 21.6 GB / 16 GB runner** → iki oda şişince diğerleri ölür

**Çözüm:** `room.yml` tek kaynak; `render.py` container limitini hesaplar
(`xmx + max(320M, %22)`), `runtime/rooms.env` üretir, compose `${X_MEM:?}` okur.
Yeni dağılım: auth 640M/heap, lobby 1280, parkur 1024, duello 1536, skyblock 1920,
survival 2304, bedwars 2304 → **toplam 13.4 GB ≤ 14.2 GB bütçe**.

### OP / yetki (kullanıcı: Bedrock'tan girecek, adı bozturk2642)
**Bedrock oyuncusuna ops.json ile op verilmez** — Floodgate farklı UUID üretir,
ops.json'daki UUID tutmaz. Doğru yol LuckPerms.
- `ADMIN_PLAYERS=.bozturk2642:kurucu` → `render.py` → `require-groups.yml`
  (nokta ön eki = Floodgate/Bedrock)
- `kurucu` grubuna `bukkit.command.op` verildi (admin'in `-bukkit.command.op`
  yasağını ezer) → istediğinde `/op` verebilir
- doctor: op izni **sadece kurucu'da** olabilir (sızma kontrolü)

### Yeni doctor bölümleri
`4d/7 bellek bütçesi` (4 kontrol). Toplam bölüm: 1,2,3,4,4b,4c,4d,5,6,6b,6c,7.
`ci.yml` 15 adım — 5 Python assert'inin hepsi yerelde YAML'dan çözülüp çalıştırıldı.
`hub.yml` 18 adım (yetki doğrulama dahil).

## 18. Yetki matrisi (kullanıcı: "oyuncu rütbesinde komutlara yetki olsun")

**Sorun:** `oyuncu` grubunda 8 izin vardı. `/spawn` vardı ama `/warp`, `/back`,
sohbet, `/sell`, claim, bedwars, parkur, `/hub` yoktu → oyuncu girince eli kolu bağlı.

**Yöntem:** izin adları **resmi listeden** doğrulandı (essinfo.xeya.me,
2026-11). Tahminle yazsaydım oyuncu yine hiçbir şey yapamazdı.

### Bulunan yazım hataları (uydurma izinler)
| Yazdığım (yanlış) | Doğrusu |
|---|---|
| `essentials.baltop` | `essentials.balancetop` |
| `essentials.warps` | `essentials.warp.list` |
| `essentials.vanish.effect` | `essentials.vanish.see` |
| `essentials.tempmute` / `unmute` | yok — `/mute` süreli çalışır |
| `essentials.unjail` | yok — `essentials.togglejail` |
| `essentials.warn` | yok (EssentialsX'te bu komut yok) |
| `essentials.chat.color` (VIP'de) | **resmi listede doğrulanamadı** → çıkarıldı |

### Yeni dağılım
| Grup | İzin | Ne yapar |
|---|---|---|
| oyuncu | 73 | ev, tpa, spawn, warp, sohbet, para, kit, claim, bedwars, parkur, hub |
| vip | 15 | + fly, nick, 5 ev, enderchest, anvil/tezgah, 10 arsa |
| mod | 28 | + kick, mute, ban, jail, vanish, invsee, tp, coreprotect inspect |
| admin | 4 | `*` + 3 negatif (op/stop/reloadall yasak) |
| kurucu | 3 | + op verme, librelogin.admin, luckperms.editor |

### Bilinçli eksikler
`essentials.chat.color` **oyuncu ve vip'e verilmedi** — biri kendini `[KURUCU]`
gibi gösterebilirdi, tag sistemi çöker. `coreprotect.rollback` mod'da yok
(yanlış komut = dünya silinir). `admin` op veremez.

### Yeni dosyalar
- `config/plugins/GriefPrevention/config.yml` — ilk 4000 blok bedava arsa
- `config/plugins/Essentials/config.yml` — `oyuncu`/`vip` kit'i + sethome-multiple

### Doğrulama
- `doctor.py` **§4e/7 yetkiler**: 6 kontrol — izin adı geçerliliği (resmi liste
  gömülü, 180+ izin), oyuncu temel komutları, tehlikeli izin yok, tag taklidi
  kapalı, hiyerarşi zinciri, prefix'ler
- `ci.yml` 13 adım: "Yetki sağlık kontrolü" (6 assert)
- `hub.yml`: `/lp listgroups` + `/lp user .bozturk2642 info` ile canlı teyit
- `docs/PLUGINS.md` §8b: yetki matrisi + bilinçli eksikler tablosu
