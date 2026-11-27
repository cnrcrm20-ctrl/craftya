# Operasyon Notları

## Bellek havuzu (governor)

5 GB ortak havuz, yoğun odaya `jcmd` ile **restart olmadan** dağıtılır.

```bash
./scripts/mc.sh governor          # tek tur (test)
./scripts/mem-governor.sh         # döngü, 30 sn
tail -f logs/mem-governor.log     # kararları izle
```

Nasıl çalışır:
- Boş odalar → tabana iner (havuz boşalır)
- Dolu odalar → oyuncu başına 160 MB, tavan `Xmx` ile sınırlı
- Havuz toplamı 5 GB'ı aşamaz
- Dolu odanın payı asla tabanın altına inmez
- Governor çökerse: herkes tabanında kalır, sunucu çalışır (fail-safe)

Örnek: BedWars etkinliği başlarken bedwars 768 MB → 4.8 GB, survival 3.5 → 1.5 GB.
Etkinlik bitince geri döner.

## 6 saatlik devir

`hub.yml`: 5s40dk çalışır → duyuru → `save-all flush` → `world-sync upload` →
artifact → yeni run → `world-sync restore` (sha256 doğrulamalı) → sunucular.

Kesinti hedefi 25-35 sn. Sıfır olamaz: Minecraft oturumu process'e bağlı.

## Yedekler

- **Artifact**: `craftya-world-*.tar.gz`, 30 gün saklanır, otomatik
- **Yerel**: `artifacts/` (son 3 arşiv, `CRAFTYA_KEEP_ARCHIVES` ile değişir)
- Bütünlük: her arşivde `.sha256`; bozuk yedek **sessizce geri yüklenmez**

## Bot

```bash
cd tools/craftya-bot
cp config.example.json config.json   # sunucu adresi + bot şifresi
npm install

node src/scan.js                     # dünyayı oku → out/scan.json
node src/apply.js --dry-run          # planı göster (uygulama yok)
node src/apply.js                    # uygula
node src/verify.js --rescan          # gerçekten oldu mu?
node src/guard.js                    # moderasyon (gözlemci modu)
```

Bot `op` **değildir**. Sadece beyaz listedeki komutları çalıştırır:
`fill, setblock, summon, tp, gamemode, time, weather, gamerule, npc, hologram, bw, parkour, save-all, say, title, setworldspawn`

## Türkçe

- LibreLogin: `config/plugins/LibreLogin/config.yml` (proxy'de, limbo = `auth`)
- EssentialsX: `locale: tr`
- LuckPerms grupları: Türkçe isimler (`oyuncu, vip, mod, admin, kurucu, bot`)
- MOTD / TAB / Geyser: `config/` altında, hepsi Türkçe

## Teşhis sırası (bir şey bozulunca)

1. `python3 scripts/doctor.py` — config mi, ortam mı?
2. `./scripts/mc.sh health` — hangi oda ölü?
3. `./scripts/mc.sh logs <oda>` — Java hatası ne?
4. `docker exec hub-<oda> rcon-cli tps` — lag mi?
5. `spark profiler start` (oyun içi) — ne yiyor?

## Komutlar — oyuncu tarafı

| Komut | Ne yapar | Nerede tanımlı |
|---|---|---|
| `/hub` | Çekirdek oda (lobby) — tüm modları birbirine bağlayan oda | proxy (`velocity-hub-command`) |
| `/lobby` | `/hub` ile aynı (alias) | proxy (`useLobbyAlias: true`) |
| `/lobi` | `/hub` ile aynı (Türkçe) | her odada (`_common/commands.yml`) |
| `/l` | `/hub` ile aynı (kısa) | her odada |
| `/merkez` | `/hub` ile aynı (Türkçe) | her odada |

**Neden iki katman?** `/hub` + `/lobby` proxy plugininden geliyor; tek kopya,
6 odada birden geçerli, oda başına ayar yok. Ama o plugin Türkçe alias tanımıyor.
Bu yüzden `/lobi`, `/l`, `/merkez` her odada Essentials `aliases` ile `/hub`'a
yönlendiriliyor. Akış:

```
/lobi  →  Essentials aliases  →  /hub  →  proxy  →  lobby odası
```

Hub odası **lobby** (port 25566) — `velocity.toml` içinde `try = ["lobby"]`,
yani oyuncu sunucuya ilk girdiğinde de buraya düşer.

**Bozulursa teşhis:** `docker compose logs velocity | grep -i hub` — plugin
yüklenmediyse `/hub` oyunda hiç görünmez.
