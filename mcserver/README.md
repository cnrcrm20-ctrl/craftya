# Craftya Minecraft Sunucusu

Sunucu burada ikiye ayrılıyor, karışmasın:

| | Ne | Kime |
|---|---|---|
| **Bu klasör (`mcserver/`)** | Gerçek Minecraft: **Java + Bedrock** oyuncuları aynı dünyada. PaperMC, Geyser, ViaVersion, anti-Xray, otomatik yedek | Arkadaşların kendi Minecraft'ından girer |
| `engine/`, `android/` | Craftya'nın kendi mobil istemcisi (ayrı proje) | Play Store APK |

Buradaki sunucu **vanilla uyumlu**: oyuncular mod kurmaz, sadece sunucu adresini yazar.

---

## 1. Üç komutta çalıştır

```bash
cd mcserver
cp .env.example .env          # 1) ayarları aç, RCON_PASSWORD değiştir
./scripts/fetch-plugins.sh    # 2) çekirdek plugin'leri indir (~15 MB)
docker compose up -d          # 3) başlat
```

İlk açılış **3–5 dakika** sürer (dünya üretimi + plugin yükleme). İzlemek için:

```bash
docker compose logs -f minecraft
```

Hazır olduğunda log'da `Done (12.345s)! For help, type "help"` görürsün.

---

## 2. Oyuncular nasıl girer?

### PC (Java Edition)
Sunucu listesine ekle:

```
sunucuadresin:25565
```

Yerelde test ediyorsan: `localhost`

### Telefon / tablet / konsol (Bedrock Edition)
Bedrock sunucusu ekle:

| Alan | Değer |
|---|---|
| Sunucu adresi | IP veya alan adı |
| Port | **19132** |

Geyser plugin'i bunu Java dünyasına çeviriyor. Bedrock oyuncuları **hiçbir şey kurmaz**.
Floodgate sayesinde Bedrock oyuncusunun Java hesabı olması da gerekmez.

### Dışarıdan erişim (IP'nde port açamıyorsan)
```bash
# playit.gg → hesap aç → Agents → New Agent → SECRET_KEY'i .env'ye yaz
docker compose --profile tunnel up -d
```
Panelde **Minecraft Java** tunnel'ı ekle, local adres `127.0.0.1:25565`.
Sana `xxxx.craft.playit.gg` gibi **sabit** bir adres verir — sunucu yeniden başlasa da değişmez.
Bedrock için ayrıca **Minecraft Bedrock** tunnel'ı ekle, port `19132`.

---

## 3. Ne kurulu, ne işe yarıyor

| Bileşen | Görev | Neden gerekli |
|---|---|---|
| **PaperMC** | Sunucu çekirdeği | Vanilla'nın aynısı ama 2–3× daha az CPU. Tüm ciddi sunucular bunu kullanır |
| **ViaVersion + ViaBackwards** | Sürüm çevirisi | Oyuncu 1.20'de, sunucu 1.21'de olsa da girebilsin |
| **Geyser + Floodgate** | Bedrock köprüsü | Telefon/konsol oyuncusu aynı dünyaya girsin |
| **anti-Xray** (`config/paper-global.yml`) | Hile engeli | Duvarın arkasındaki elması görmeyi engeller |
| **mc-backup** | Yedek | Her 6 saatte dünya yedeği, 7 gün saklanır |

Opsiyonel eklentiler (`.env` → `MC_EXTRA_PLUGINS`):

```
luckperms,spark,coreprotect,chunky,simple-voice-chat
```

| Eklenti | Ne yapar |
|---|---|
| `luckperms` | Yetki/grup: kim neyi yapabilir (`/lp editor`) |
| `spark` | Profiler: "neden lag var" sorusunun cevabı (`/spark profiler start`) |
| `coreprotect` | Blok geçmişi: kim neyi kırdı, geri alma (`/co rollback`) |
| `chunky` | Haritayı önceden üret: ilk gün lag'i biter (`/chunky world world radius 2000`) |
| `simple-voice-chat` | Yakın sesli sohbet (oyuncular mod kurar, opsiyonel) |

---

## 4. Yönetim

```bash
docker compose logs -f minecraft            # canlı log
docker compose exec minecraft rcon-cli      # sunucu konsolu
docker compose restart minecraft            # yeniden başlat
docker compose down                         # durdur (dünya korunur)
```

Konsoldan en sık kullanılanlar:

```
whitelist add OyuncuAdi
whitelist on
op OyuncuAdi
ban OyuncuAdi
tps                       # sunucu sağlığı (20.0 = mükemmel)
```

### Yapılandırma doğrulaması
Docker'ı çalıştırmadan önce her şeyi denetler:

```bash
python3 scripts/doctor.py
```

Kontrol ettikleri: YAML sözdizimi, `.env` sırları (örnek değer kalmış mı), `.env`'nin git'te takip edilmemesi, plugin jar'larının gerçekten jar olup olmadığı (indirme hata sayfasını yakalar), config dosyalarının varlığı, Geyser yetkilendirmesi.

---

## 5. Profesyonel ayarlar (neden bunlar)

| Ayar | Değer | Sebep |
|---|---|---|
| `view-distance` | 8 | 12'de RAM 2×, kazanç hissedilmez |
| `simulation-distance` | 6 | Mob/redstone mantığı sadece yakında çalışsın |
| `hopper-can-load-chunks` | false | Chunk yükleyen hopper = en pahalı hata |
| `entity-activation-range` | 24–28 | Uzaktaki mob tick almasın |
| `anti-xray engine-mode` | 2 | Hem gizle hem obfuscate et |
| `enforce-secure-profile` | true | Sahte istemci/korsan engeli |

Yedekler: `docker volume inspect mcserver_mc-backups` ile yolunu görürsün.
Dünya: `mcserver_mc-data` volume'unda.

---

## 6. Sunucu nerede koşacak?

| Yol | Bedel | Not |
|---|---|---|
| **Kendi bilgisayarın + playit.gg** | 0 | Bilgisayar açık kaldıkça çalışır. Başlamak için en hızlı yol |
| **Oracle Cloud Always Free (ARM)** | 0 | 7/24 açık kalır. 2 OCPU/12 GB RAM yeter. Kart doğrulaması ister, boşta kalırsa geri alınabilir → keepalive şart |
| **Kiralık Minecraft host** | ~€3/ay | Hazır panel, DDoS koruması, sıfır uğraş |

Aynı `docker-compose.yml` üçünde de çalışır — sadece `docker compose up -d` diyeceğin makine değişir.

---

## 7. Bilinen sınırlar

- **Sürüm pinlenmedi**: `MC_VERSION=LATEST`. Paper yeni sürüm çıkarınca plugin'ler birkaç gün geride kalabilir. Canlı sunucuda sürümü pinle (`.env` → `MC_VERSION=1.21.4`) ve yükseltmeyi test ederek yap.
- **Geyser sürümü sabit değil**: Bedrock istemcileri hızlı güncelleniyor, eski Geyser bağlanamaz. Bu yüzden Geyser hep "latest".
- **Bedrock portu (19132/udp)** ayrıca açılmalı; sadece Java portu yetmez.
- `ONLINE_MODE=FALSE` yaparsan hile denetimi biter — sadece test için.
- Bu klasör Craftya'nın C++ motorunu **kullanmaz**; ondan bağımsızdır.
