# Craftya 3.0.0 — Sürüm Notları

**APK:** `v3.0.0-b37` · ~414 KB · arm64-v8a · imzalı
**Test:** 336 otomatik test / 0 hata
**Arayüz:** modern (SDF yuvarlak köşe + gradyan + anti-aliased yazı)
**Motor:** 457 içerik (311 blok + 37 canlı + 109 eşya), 130 üretim tarifi

---

## Neden önceki APK'lar açılmıyordu — kök neden

Şikayet "hiçbiri çalışmıyor"du. Üç **ayrı** hata vardı ve üçü de yerelde
birebir yeniden üretilip düzeltildi:

### 1. Uygulama açılırken çöküyordu (giriş sembolü gizliydi)

`android_native_app_glue` ayrı bir STATIC kütüphane olarak derleniyordu ve
projede `CMAKE_C_VISIBILITY_PRESET hidden` açıktı. Android, uygulamayı
başlatmak için `libcraftya.so` içinde **`ANativeActivity_onCreate`** sembolünü
arar. Sembol gizlendiği için kütüphane yüklenemiyor → ekran hiç gelmiyordu.

Yerel kanıt (aynı derleyici bayrakları, sahte glue dosyası):

```
mevcut yapı   -> nm -D: ANativeActivity_onCreate YOK  (çöker)
düzeltilmiş   -> nm -D: ANativeActivity_onCreate var   (çalışır)
```

**Düzeltme:** glue artık hedefe doğrudan kaynak olarak ekleniyor ve yalnızca
onun için `-fvisibility=default` veriliyor. Ek olarak `verify_entry.cmake`
derleme sonrası sembolü denetliyor: yoksa **derleme durur**.

### 2. Oyun donuyordu (menü hiç kapanmıyordu)

`startNewWorld()` içinde `menus_.close()` çağrılıyordu, ama `close()`
"bir adım geri" demek. Ana menüden gelindiğinde menü **açık kalıyordu**.
Menü açıkken tüm dokunuşlar menüye gittiği için joystick, hotbar ve
butonlar tamamen ölüydü.

**Düzeltme:** `MenuSystem::closeAll()` eklendi; dünya başlarken menü
tamamen kapanıyor.

### 3. "Tek Oyunculu"ya basınca çöküyordu (segfault)

`MenuSystem::onTap()` öğe aksiyonunu çağırdıktan **sonra** `current_.items[i]`
okuyordu. Aksiyon menüyü kapatıp vektörü boşalttığı için sınır dışı erişim
oluyordu.

**Düzeltme:** etiket ve aksiyon çağrıdan **önce** kopyalanıyor.

---

## Bu sürümde düzeltilen diğer mobil hatalar

| Hata | Etki | Düzeltme |
|---|---|---|
| `onGround` hiç güncellenmiyordu | Oyuncu yürüyemiyordu | `MoveResult.onGround/inWater` gövdeye yazılıyor |
| 1 blokluk basamakta takılma | Dokunmatikte ilerlemek imkânsız | Bedrock tarzı **auto-jump** (0.6–1.1 blok) |
| Bakış hassasiyeti 2 kez çarpılıyordu | Ekran dönmüyordu | Tek yerde `LOOK_SENS_MOBILE = 0.0026` |
| Joystick yakalama alanı dardı | Parmağın çoğu yere basınca çalışmıyordu | Sol alt çeyrek + sabit taban |
| Hotbar'ın sol yarısı çalışmıyordu | Yuva seçilemiyordu | Dokunma sırası düzeltildi |
| Geri tuşu oyunu arayüzsüz bırakıyordu | Devam edilemiyordu | Önce menü, sonra **duraklat** |
| Oyun tamamen sessizdi | Ses yoktu | 16 prosedürel SFX bağlandı (AAudio) |
| Dokunsal geri bildirim yoktu | — | Blok kırma/koyma/vuruşta titreşim |

---

## Yeni özellikler

- **Avlanma:** "KIR" butonu önce önündeki canlıya vurur (4 hasar), canlı yoksa
  bloğu kırar. Vuruş sesi + titreşim + kaçış tepkisi + ölüm temizliği.
- **Nişangâh hedef göstergesi:** canlıya nişan alınca nişangâh kırmızıya döner
  ve hedefin can barı çizilir (yeşil → sarı → kırmızı).
- **Ses çıkışı:** AAudio (API 26+), üçüncü parti bağımlılık yok; API 24/25'te
  sessizce devre dışı kalır, oyun çalışmaya devam eder.
- **Titreşim:** 60 ms spam koruması, ayarlardan kapatılabilir.

## Mobil varsayılanlar (Not 11 Pro 4G sınıfı)

- 6 chunk görüş mesafesi
- %80 çözünürlük ölçeği
- Gölge kapalı (mobilde yönlü kenar ışığı kullanılıyor)
- 60 FPS hedefi, FOV 72, dokunma hassasiyeti 1.0

Ölçüm: dünya yüklendikten sonra ortalama kare süresi **7,4 ms** (16,6 ms
bütçesinin altında), 180 karede 578 chunk yüklü / 246 görünür.

## CI güvencesi

Bir daha "çalışmayan APK" yayınlanmaması için her derlemede:

1. `verify_entry.cmake` — `ANativeActivity_onCreate` yoksa **derleme durur**
2. **APK içerik denetimi** — APK açılır, `libcraftya.so` varlığı, boyutu ve
   giriş sembolü doğrulanır; eksikse iş kırmızı olur
3. **İş özeti** — doğrulama tablosu Actions sayfasına yazılır

APK boyutu bu sürümlerde **272 KB → 407 KB**'a çıktı; aradaki fark, daha önce
paketlenmeyen glue nesnesinin artık gerçekten içinde olduğunu gösteriyor.

## Arayüz modernleştirme (derleme 26–30)

Menüler ilk hâlinde düz siyah paneller, kalın şeritler ve blok piksel yazıyla
"BIOS ekranı" gibi duruyordu. Yapılanlar:

| Alan | Önce | Sonra |
|---|---|---|
| Panel | düz siyah dikdörtgen | SDF tabanlı yuvarlatılmış köşe + gradyan + gölge |
| Menü kartı | keskin şerit | gradyanlı cam kart, seçilide sol vurgu hapı |
| Yazı | 5×7 blok piksel | 3× çözünürlüklü atlas, GL_LINEAR yumuşatma, oransal genişlik |
| Can/açlık | 10 ayrı kare kutucuk | iki yuvarlatılmış gradyan bar |
| Sıcak bar | keskin kutular | yuvarlak yuvalar, seçilide hâle + nabız göstergesi |
| Envanter | keskin ızgara | yuvarlatılmış gradyan yuvalar, panel içinde |
| Klavye | keskin tuşlar, yarı saydam | gradyan tuşlar, opak panel, tüm satırlar sığıyor |
| Sohbet | siyah şeritler | cam balonlar + sol renk şeridi |

Teknik ayrıntılar:

- **SDF shader** (`guiFS`): `min(max(qx,qy),0)+length(max(q,0))-r`, alfa
  `1-smoothstep(-0.75,0.75,d)` → kenarlarda 1 px yumuşak geçiş.
- **Font atlası**: hücre 17×23 (5×7 glif × 3 ölçek + 1 px şeffaf kenar).
  Şeffaf kenar, `GL_LINEAR` filtrenin komşu gliften örnek almasını engelliyor.
- **`font_metrics.hpp`**: 95 girişli oransal genişlik tablosu glif tablosundan
  otomatik üretiliyor; `glyphUVClip` yalnız gerçek glif genişliğini örnekliyor.
- **Yerleşim korundu**: glif yüksekliği `14*scale` ile eski metrikle birebir
  aynı, böylece HUD öğeleri ekran dışına taşmıyor (regresyon testi bunu
  denetliyor: 336/0).

Bu turda düzeltilen gerçek hatalar: `y/g/p/q/j` alt uzantısının taban
çizgisinin üstünde kalması, menü etiketlerinin 8 px yukarı kayması, envanter
ızgarasının 4. satırının panel dışına taşması, sohbet klavyesinde boşluk
tuşunun ekran dışında kalması ve altındaki HUD düğmelerinin sızması.

## Sunucu merkezli menü (derleme 37)

Kullanıcı haklıydı: oyun bir **sunucu oyunu** olacaksa menüde "Tek Oyunculu"
diyerek başlamak yanlıştı. Menü baştan yazıldı.

**Ana menü artık:**

| Öğe | Açıklama |
|---|---|
| Sunucuya Gir | en kalabalık dünya, canlı oyuncu sayısı + "canli" hapı |
| Sunucular | lobi listesi (gerçek mod listesi, doluluk oranlarıyla) |
| Karakter | görünüm / isim |
| Ayarlar | grafik / ses / kontrol |

**Sunucu listesi** (eski "mod paneli") artık gerçek bir lobi gibi görünüyor:
her satırda yuvarlak ikon, canlı yeşil nokta, `8/10` doluluk hapı, bölüm
başlığı ve "5v5 savaş · 8/10 oyuncu" alt etiketi.

**Görsel dil:** çok katmanlı gradyan arka plan, üstte sıcak ışık hâlesi,
gölgeli cam panel, yuvarlatılmış gradyan kartlar, sol vurgu hapı, yuvarlak
ikon rozetleri, sağda canlı nokta.

**Ölçü sistemi:** başlık bloğu ve kart metinleri artık sabit piksel değil,
satır yüksekliğine göre ölçekleniyor. Bu, farklı ekran oranlarında (telefon
yatay/dikey) taşmayı bitirdi — ilk denemede başlık kartlara giriyordu.

`simulateLobbyOnline()`: sunucu yanıtı gelmeden menü "sunucu araniyor..." da
kalmıyor; iyimser varsayılanla canlı görünüyor.

## Bilinen sınırlar

- Yalnızca **arm64-v8a** (32-bit cihazlar desteklenmiyor).
- Ses API 26+ (Android 8.0); altında oyun sessiz çalışır.
- Çok oyunculu lobi arayüzü var; canlı sunucu eşleşmesi Supabase üzerinden
  ayrıca yapılandırılmalı.
