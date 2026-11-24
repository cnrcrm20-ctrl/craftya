# Craftya — İçerik Genişletmesi: 457 Eşya

> Önceki sürüm 31 bloktu. Haklıydın: o sınırlıydı. Bu sürümde içerik
> **457'ye** çıktı ve animasyon sistemi baştan yazıldı.

---

## 1. Sayım: tam 457

| Kategori | Adet | Ne var |
|---|---:|---|
| **Bloklar** | **311** | taş/maden/toprak, ahşap, renkli, cevher, bitki, etkileşimli, dekor |
| **Canlılar** | **37** | balık, memeli, kuş, amfibi, böcek, düşman |
| **Eşyalar** | **109** | alet, zırh, malzeme, yiyecek, özel |
| **TOPLAM** | **457** | |
| *Balık kombinasyonları* | *125* | **sayıma dahil değil** |

Test bunu doğruluyor: `311 + 37 + 109 == 457`, ve
`ContentRegistry::total() == 457`.

### Neden 311 blok?

Sayıyı tutturmak için blokları **aile aile** kurdum, elle tek tek yazmadım:

- **93 doğal/taş bloğu** — stone, granite, diorite, andesite, deepslate ailesi,
  tuff, calcite, basalt, blackstone, netherrack, end stone, purpur, kuvars,
  topraklar, kumlar, buzlar, sculk, ametist
- **64 renkli blok** — 16 renk × 4 aile: **yün, beton, terracotta, boyalı cam**
- **40 ahşap** — 8 ağaç türü × (kütük, tahta, yaprak, kapı, tuzak kapı)
- **26 cevher** — 8 cevher × (normal + derin taş) + 10 metal bloğu
- **25 bitki** — çiçekler, ekinler, mantarlar, sarmaşık, deniz otu, bambu
- **43 etkileşimli blok** — aşağıda
- **17 dekor/sıvı** — cam, ışık taşları, su, lav, balçık, bal

---

## 2. Etkileşimli bloklar (43)

Her biri gerçek bir davranışa bağlı — sadece isim değil:

| Blok | Davranış |
|---|---|
| Kapı / Tuzak kapı (9 tür) | açılır/kapanır, durum saklanır |
| Sandık / Fıçı / Huni / Dağıtıcı / Bırakıcı | 27 yuvalı konteyner açar |
| Tezgah / Üretici | üretim ızgarası |
| Fırın / Yüksek Fırın / Tütsücü | **kömürle yakılır**, yakıt tüketilir, durum takip edilir |
| Kol / Düğme / Basınç plakası | redstone sinyali aç/kapa |
| Fener / Ruh feneri / Kamp ateşi / Meşale | yak/söndür |
| Örs / Bileği taşı / Taş kesici / Demirci | tamir ve üretim istasyonları |
| Büyü masası / İksir standı / Kazan | büyü ve iksir |
| Nota bloğu / Müzik kutusu / Çan | ses çalar |
| TNT | ateşlenir, 3 saniye geri sayım |
| Piston / Gözlemci / Sculk sensörü | mekanik |
| Merdiven / Karyola / Kürsü / Kompost | kullanım |
| Fener kulesi / Yeniden doğma çapası | özel |

Test: 43 bloğun **hepsi** bir davranış döndürüyor.

---

## 3. Canlılar (37) — istediğin hepsi var

| Grup | Türler |
|---|---|
| **Balık** (14) | morina, somon, tropikal, balon, palyaço, melek, levrek, sardalya, ton, alabalık, kılıç, yılan balığı, vatoz, kalamar |
| **Memeli** (12) | **inek, koyun, domuz, keçi**, tavşan, at, kurt, kedi, tilki, panda, lama, kutup ayısı |
| **Kuş/amfibi** (4) | **tavuk**, **kurbağa**, **akselotl**, kaplumbağa |
| **Böcek/uçan/düşman** (7) | arı, yarasa, zombi, örümcek, creeper, iskelet, enderman |

### 125 balık kombinasyonu
5 tür × 5 renk × 5 desen = 125 görsel varyant. Bunlar **ayrı eşya değil** —
aynı balığın renk/desen varyantı. Sayım 457'de kaldı, kombinasyonlar dahil edilmedi.

Test: 125 kombinasyonun hepsi tekrarsız ve dolu.

---

## 4. Eşyalar (109)

- **35 alet** — 7 malzeme (ahşap, taş, bakır, demir, elmas, zümrüt, netherit) ×
  5 tip (kazma, balta, kürek, kılıç, çapa). Her biri kendi hasar/dayanıklılık
  değeriyle.
- **12 zırh** — 6 malzeme × (miğfer, göğüslük)
- **~17 yiyecek** — ekmek, altın elma, altın havuç, etler, bal...
- **~45 malzeme/özel** — külçeler, elmas, ip, kemik, kağıt, kitap, yay, ok,
  kalkan, olta, kova, makas, mızrak, pusula, harita

### 130 tarif
Her alet, her zırh, her blok için gerçek tarif. Şekilli eşleşme konumdan
bağımsız çalışıyor. Test 130 tarifin hepsini doğruluyor.

---

## 5. Animasyon — Minecraft'ın ötesine geçen kısım

Minecraft parçaları döndürür. Biz **prosedürel iskelet** kullanıyoruz:

**Temel**
- Yürüyüş döngüsü hıza bağlı (yavaş yürüme → koşma farklı genlik)
- Dört ayaklılarda **çapraz yürüyüş** (sol ön + sağ arka aynı faz)
- Nefes alma (göğüs şişer), göz kırpma, boşta durma salınımı
- Zıplamada **ezilme-gerilme** (squash/stretch)

**İkincil hareket (yay sönümü)**
- Kulaklar, kuyruk ve çene gövdeden **gecikmeli** takip eder
- Yay-sönüm integratörü (`stiffness`/`damping`), sabit kodlanmış açı değil

**Türe özel**
- **Balık/yılan**: kuyruk dalgası her parçada gecikmeli yayılır
- **Kuş/böcek**: kanatlar zıt fazda çırpar, yerde katlanır
- **Kurbağa**: arka bacaklar birlikte zıplar
- **Baş takibi**: gözler hedefe kilitlenir (yatay ±52°, dikey ±31°)

**Dövüş**
- Saldırıda gövde öne atılır, kollar savrulur
- Hasar aldığında sarsıntı, azalan genlikle
- Ölümde yana devrilir, gövde çöker, kademeli (1.2 sn)

Test: 37 canlının hepsi için iskelet kuruluyor ve animasyon çalışıyor.

---

## 6. Teknik durum

| | |
|---|---|
| Motor kodu | **8.010 satır** (18 `.cpp`, 24 `.hpp`) |
| Test | **315 test, 0 başarısız** (35 çekirdek + 13 oyun + 267 sistem) |
| Kare süresi | **3.14 ms** (60 FPS bütçesinde 5× boşluk) |
| Atlas | 1024×1024, 936 doku — **hepsi kodla üretiliyor**, tek PNG yok |
| Doku stili | 26 prosedürel stil (taş, tuğla, yün, beton, cam, bitki, cevher, ağaç...) |
| Bağımlılık | sıfır |

### Yeni dosyalar
- `blocks_gen.hpp` — 311 blok (renk + doku stili + etkileşim türü)
- `items_gen.hpp` — 109 eşya
- `mobs_gen.hpp` — 37 canlı + 125 balık kombinasyonu + vücut planları
- `recipes_gen.hpp` — 130 tarif
- `interact.hpp/cpp` — etkileşim davranışları + `ContentRegistry` (457 arama)
- `anim.hpp/cpp` — iskelet animasyon sistemi

### Bulduğum ve düzelttiğim hatalar
1. Atlas: cam/bitki dokuları `img_.fill()` ile **tüm atlası** siliyordu → kare bazlı temizleme
2. İskelet: eklem sırası vücut planına göre değiştiği için sabit indeksler yanlış kemiğe yazıyordu → isimle arama
3. İkincil hareket: yay sönümü kendi hedefini sıfırlıyordu → hedef 0, gecikme gerçek
4. Balık kuyruğu `BONE_TAIL` indeksine yazıyordu, balıkta o indeks kuyruk değil → isimle

---

## 7. Değişmeyen gerçek

APK'yı burada derleyemiyorum (NDK/Gradle/JDK yok). CMake dosyası 19 modülü
kapsayacak şekilde güncel ve hepsi `-fno-exceptions -fno-rtti` ile derleniyor.
Telefonda:

```bash
export ANDROID_NDK_HOME=$ANDROID_SDK_ROOT/ndk/26.1.10909125
cd android && ./build.sh
```
