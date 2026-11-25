# Craftya — Minecraft Sunucusu (Java + Bedrock)

Hub mimarisi: **Velocity** ön kapı, arkada **6 oda**. Java ve Bedrock (telefon/konsol)
oyuncuları **aynı dünyada**. Oyuncu tek adres yazar: `mc.craftya.dev`.

> **Durum:** altyapı hazır, oyun içi kurulum (lobi, NPC, arena) sıradaki adım.
> Mimari kararlar ve gerekçeleri: [`BRAIN.md`](BRAIN.md)
> Plugin listesi ve gerekçeleri: [`docs/PLUGINS.md`](docs/PLUGINS.md)
> Kurulum: [`docs/KURULUM.md`](docs/KURULUM.md)

---

## Odalar

| Oda | Mod | Java portu | Bellek (taban/havuz) |
|---|---|---|---|
| `velocity` | Proxy (ön kapı) | **25565** | 384 MB / — |
| `lobby` | adventure | 25566 | 384 MB / 2 GB |
| `survival` | survival | 25567 | 1.5 GB / 5 GB |
| `skyblock` | survival (void) | 25568 | 768 MB / 4 GB |
| `bedwars` | adventure | 25569 | 768 MB / 5 GB |
| `duello` | arena | 25570 | 512 MB / 3 GB |
| `parkur` | adventure | 25571 | 256 MB / 1.5 GB |

Bellek **paylaşılan havuz** modeliyle dağıtılır: 5 GB ortak havuz, yoğun olan odaya
`jcmd` ile **restart olmadan** aktarılır. Ayrıntı: `BRAIN.md` §4.6.

---

## Hızlı bakış

```bash
cp .env.example .env         # ayarlar
./scripts/fetch-plugins.sh   # plugin jar'ları (pinli sürümler)
python3 scripts/render.py    # config şablonları → runtime/
python3 scripts/doctor.py    # 12 kontrol: hazır mı?
./scripts/mc.sh up           # başlat
./scripts/mc.sh health       # TPS + oyuncu + bellek
```

Sunucu **GitHub Actions runner'ında** koşar (kullanıcının kendi bilgisayarı yok).
6 saatlik devir döngüsü: `.github/workflows/hub.yml`.

---

## Nasıl girilir

**Java (PC):** `mc.craftya.dev` — port yazmadan
**Bedrock (telefon/konsol):** `mc.craftya.dev` + port `19132`

Bedrock'ta Geyser **proxy'de** çalışır: Java ve Bedrock aynı dünyaya girer,
Bedrock oyuncu Java hesabı açmak zorunda değil (Floodgate).

Cracked sunucu: ilk girişte `/register <şifre>`, sonraki girişlerde `/login <şifre>`.
Giriş **proxy'de tek sefer** yapılır — oda değiştirince şifre tekrar sorulmaz
(oturum 30 gün). Bedrock oyuncusu hiç şifre girmez (Floodgate).

**Hub'a dönmek:** `/hub` · `/lobby` · `/lobi` · `/l` · `/merkez` (hepsi çekirdek lobi odasına atar)

---

## Depo yapısı

```
config/            # şablonlar (${VAR} yer tutucu) — git'te durur
  velocity/        #   velocity.toml.tpl
  rooms/<oda>/     #   server.properties, bukkit.yml, spigot.yml, paper-global.yml
  plugins/         #   LibreLogin, LuckPerms, TAB, Geyser-Velocity, floodgate...
runtime/           # üretilen dosyalar (git'te YOK, render.py yazar)
scripts/           # render, gradient, doctor, plugins.mjs, mem-governor, world-sync, mc.sh
tools/craftya-bot/ # mineflayer botu: dünyayı okur, inşa eder, doğrular
supabase/          # şema + RLS (global veri katmanı)
web/               # Cloudflare Pages sitesi
docs/              # kurulum, plugin, operasyon
.github/workflows/ # hub.yml (6 saatlik döngü), ci.yml (doğrulama)
```

**Sırlar:** `runtime/` git'te yok, `.env` git'te yok. `config/*.tpl` içinde sadece
`${PLACEHOLDER}` var, gerçek değer yok. Üretimde `VELOCITY_SECRET` ve `RCON_PASSWORD`
her turda `openssl rand` ile üretilir → saklanacak secret sayısı 2'ye iner.
