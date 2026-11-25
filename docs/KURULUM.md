# Kurulum — 10 dakika, 4 adım

Sunucu **GitHub Actions** üzerinde koşar. Senin bilgisayarında Docker olması gerekmez.

---

## Adım 1 — iki secret ekle (2 dk)

Terminalde (kendi bilgisayarında, `gh` kuruluysa):

```bash
gh secret set PLAYIT_SECRET --repo cnrcrm20-ctrl/craftya
gh secret set SUPABASE_ANON_KEY --repo cnrcrm20-ctrl/craftya
```

- **PLAYIT_SECRET**: playit.gg → hesap aç → Agents → New Agent → çıkan anahtar
- **SUPABASE_ANON_KEY**: Supabase → craftya projesi → Settings → API → `anon` public

> `VELOCITY_SECRET` ve `RCON_PASSWORD` **gerekmez**. Her çalıştırmada üretilir.
> `service_role` anahtarı **hiç kullanılmaz** — güvenlik gereği.

`gh` yoksa GitHub web arayüzünden: Settings → Secrets and variables → Actions → New repository secret.

---

## Adım 2 — playit'te tünel (2 dk)

1. playit.gg paneline gir
2. **Tunnels → Add tunnel → Minecraft Java**
3. Local address: `127.0.0.1:25565`
4. Sana `xxxx.craft.playit.gg` gibi bir adres verir → not al

Bedrock için ayrıca **Minecraft Bedrock** tüneli ekle: `127.0.0.1:19132`

> **Not:** Geyser artık **proxy'de** (Velocity) çalışıyor, odalarda değil.
> Bu yüzden Bedrock tüneli de aynı 19132'ye bakar — komutta değişiklik yok,
> ama 19132/tcp (sinyalleşme) + 19132/udp (oyun) **ikisi de** açık olmalı.
> playit Bedrock tüneli UDP taşır; NetherNet sinyalleşmesi için TCP de gerekir.
> Sorun çıkarsa Geyser config'te `transport: raknet` yap (tek UDP, sinyalleşme yok).

---

## Adım 3 — Cloudflare DNS (3 dk)

craftya.dev → DNS → kayıt ekle:

| Tür | Ad | İçerik | Proxy |
|---|---|---|---|
| `SRV` | `_minecraft._tcp.mc` | Öncelik `0`, Ağırlık `5`, Port `<playit portu>`, Hedef `xxxx.craft.playit.gg` | DNS only |

Böylece oyuncu sadece **`mc.craftya.dev`** yazar, port yazmaz.

> **Neden A kaydı değil SRV:** IP sabit değil (runner her 6 saatte değişir).
> SRV, adresi bizim yerimize çözer. `mc.craftya.dev` hiç değişmez.

---

## Adım 4 — başlat (10 sn)

GitHub → **Actions** → **Craftya Hub** → **Run workflow** → `mode: start`

İzleme: Actions sekmesinde canlı log. "6 oda hazır" satırını görünce sunucu ayakta.

---

## Sonrası

```bash
# yerelde (Docker varsa) doğrulama
cp .env.example .env
python3 scripts/render.py
python3 scripts/doctor.py       # 20 kontrol
./scripts/mc.sh up
```

| İş | Komut |
|---|---|
| Sağlık | `./scripts/mc.sh health` |
| Log | `./scripts/mc.sh logs bedwars` |
| Yedek | `./scripts/mc.sh backup` |
| Konsol | `./scripts/mc.sh rcon survival "list"` |
| Bot tarama | `./scripts/mc.sh bot scan` |

---

## İlk oyuncu girişi

Cracked sunucu: `/register <şifre>` → sonraki girişlerde `/login <şifre>`

Yetki vermek için (konsoldan):
```
lp user <isim> parent add kurucu
```

---

## 6 saatlik devir

Sunucu 5s40dk sonra kendini yeniler: dünya arşivlenir (artifact), yeni run açar,
dünyayı geri yükler. **Kesinti ~30 sn**, oyuncu envanterini/konumunu kaybetmez.

Duyuru otomatik: "Bakım, 30 saniye içinde geri dönün."
