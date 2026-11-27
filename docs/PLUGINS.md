# Plugin Listesi — hangi plugin, neden, hangi oda

Son güncelleme: 2026-11-25

İki kategori var: **zorunlu** (biri eksikse sunucu çalışmaz/oynanmaz) ve **moda özel**.
Sürüm notasyonu: `pin` = `scripts/fetch-plugins.sh` içinde sabitlenmiş sürüm,
`latest` = eklentinin kendi yayın kanalından en yenisi.

---

## 1. Altyapı (her odada)

| Plugin | Sürüm | Ne yapar | Neden zorunlu |
|---|---|---|---|
| **PaperMC** | pin (`MC_VERSION`) | Sunucu çekirdeği | Vanilla'nın 2–3× hızlı hâli. Tüm ciddi sunucular bunu kullanır |
| **Velocity** | latest | Proxy, ön kapı | 6 oda tek adreste; oda geçişi oyuncuya görünmez |
| **ViaVersion** | pin 5.12.3 | Eski istemci → yeni sunucu | Oyuncu 1.20'deyse de girebilsin |
| **ViaBackwards** | pin 5.12.3 | Yeni istemci → eski sunucu | Sürüm sabitlediğimizde güncel istemciler girebilsin |
| **Geyser** | latest | Bedrock → Java köprüsü | Telefon/konsol oyuncusu aynı dünyaya |
| **Floodgate** | latest | Bedrock kimliği | Bedrock oyuncusu Java hesabı açmadan girsin |
| **LibreLogin** | latest | `/register` `/login` | **Cracked sunucu → zorunlu.** Proxy'de tek giriş: bir kez login → tüm odalarda oturum. Bedrock (Floodgate) şifresiz |
| **LuckPerms** | latest | Yetki + prefix | `[KURUCU]`, `[MOD]`, `[BOT]` tag'leri; kim neyi yapabilir |
| **Vault** | latest | Ekonomi API köprüsü | EssentialsX ↔ oyunlar arası para arayüzü |
| **PlaceholderAPI** | latest | Değişken sistemi | TAB, hologram, scoreboard bunu kullanır |
| **ProtocolLib** | latest | Paket erişimi | Birçok eklentinin bağımlılığı |
| **TAB** | latest | Tab listesi | Üstte sunucu adı + TPS, isimlerde prefix |
| **EssentialsXChat** | latest | Chat formatı | Tag'ler chatte görünsün (`[KURUCU] isim » mesaj`) |
| **CoreProtect** | latest | Blok geçmişi | "Evimi yıktılar" → `/co rollback`, 2 komutla geri |
| **GrimAC** | latest | Anti-cheat | Uçma/hız/aura. Paper'ın kendi kontrolü zayıf. Bot grubuna bypass |
| **spark** | latest | Profiler | "Neden lag var" → `/spark profiler`. Tahmin değil, ölçüm |
| **Chunky** | latest | Harita ön üretimi | İlk gün chunk üretim lag'i biter |

## 2. Temel oyun deneyimi (survival, skyblock, lobi)

| Plugin | Ne yapar | Neden |
|---|---|---|
| **EssentialsX** | `/home`, `/tpa`, `/spawn`, `/msg`, warp | QoL'in bel kemiği. Türkçe dili var |
| **EssentialsXSpawn** | spawn yönetimi | Lobi/survival spawn |
| **GriefPrevention** | arsa/claim | Survival'da oyuncu evini korur (Lands alternatif, GriefPrevention daha hafif) |
| **DecentHolograms** | hologram | Lobi tabelalarının üstünde yazı ("↑ Hayatta Kalma") |
| **Citizens** | NPC | Lobide sağ tık → odaya git |

## 3. Moda özel

| Oda | Plugin | Not |
|---|---|---|
| **bedwars** | **BedWars1058** | Arena kurulumu botla yapılacak (~30 konum). Solo + doubles ayrı arena |
| **skyblock** | **BentoBox + BSkyBlock** | Ada yönetimi; Türkçe çeviri kısmi → eksikleri ben tamamlarım |
| **parkur** | **Parkour** (A5H73Y) | Hafif, CPU ≈ 0, tek oyuncuyla çalışır |
| **duello** | **kendi skriptimiz** | 1v1, kit seçimi, 3 raunt. Hazır eklenti yerine 200 satır kod: kontrol bizde |
| **lobby** | Citizens + DecentHolograms | Oda seçim NPC'leri |

## 4. Opsiyonel (kapalı, açmak `.env`'de tek satır)

| Plugin | Ne katar | Neden kapalı |
|---|---|---|
| **SimpleVoiceChat** | Yakın sesli sohbet | Oyuncu tarafında mod kurulumu gerektirir |
| **Dynmap / Bluemap** | Web'de canlı harita | RAM yer, sitenin işini ileride yapacağız |
| **Plan** | Detaylı istatistik paneli | Bizim kendi sitemiz + Supabase var |
| **DiscordSRV** | Chat ↔ Discord köprüsü | Moderasyon botuyla birlikte açılacak |

## 5. Elenenler (neden kurmuyoruz)

| Plugin | Neden elendi |
|---|---|
| **WorldEdit** | Sunucuda bırakılırsa `//wall 100 100` ile lag. Arena üretimi script ile yapılıyor |
| **Multiverse** | Odalar zaten ayrı sunucu. Gereksiz katman |
| **HolographicDisplays** | Bakımı yavaşladı, DecentHolograms aynı işi daha iyi yapıyor |
| **ClearLag** | Entity'yi silerek lag'i "gizler", sebebi çözmez. Kalıcı mob kaybı |
| **Anti-Xray plugin** | Paper'ın kendi `anti-xray` motoru var (`engine-mode: 2`), ekstra plugin gereksiz |
| **NoCheatPlus** | GrimAC'in yanında eski; yanlış pozitif üretir |
| **EssentialsX Chat** | TAB + LuckPerms ile kendi chat formatımızı yazıyoruz, çakışır |

## 6. Türkçe çeviri durumu (dürüst)

| Plugin | Türkçe | Ne yapacağız |
|---|---|---|
| EssentialsX | ✅ Var (`messages_tr.properties`) | Kullan, eksik anahtarları tamamla |
| LibreLogin | ✅ Var (`tr` desteği) | Aç |
| LuckPerms | ✅ Var | Aç |
| CoreProtect | ❌ Yok | Kritik mesajları bizimkilerle değiştir (config override) |
| GrimAC | ⚠️ Kısmi | Uyarı metinlerini kendi kanalımıza yönlendir |
| BedWars1058 | ⚠️ Kısmi | Eksikleri `language.yml`'e ekle |
| BentoBox | ⚠️ Kısmi | Ada mesajlarını çevir |
| TAB / DecentHolograms | — | Metinleri biz yazıyoruz (Türkçe) |

Hiç Türkçesi olmayan ve çevirisi pratik olmayan bir plugin çıkarsa **kurulum sırasında
tek tek söylenir**, sessizce İngilizce bırakılmaz.

## 6b. Kimlik doğrulama — neden AuthMe'den LibreLogin'e geçtik

**Sorun (kullanıcı bildirdi):** her oda değişiminde şifre soruyordu.

**Kök neden:** AuthMe oda başına kuruluyordu. 6 oda = 6 ayrı plugin, 6 ayrı
SQLite dosyası, 6 ayrı oturum. Oyuncu survival → bedwars geçince yeni sunucu
onun "giriş yapmış" olduğunu bilmiyordu.

**Çözüm:** kimlik doğrulamayı **proxy'ye** taşıdık (LibreLogin). Proxy oyuncunun
tek girişini hatırlıyor; oda geçişi proxy içinde olduğu için oturum kopmuyor.

| | AuthMe (eski) | LibreLogin (yeni) |
|---|---|---|
| Kurulum yeri | her odada | **proxy'de tek** |
| Oda geçişinde şifre | **her seferinde** | yok (oturum 30 gün) |
| Veritabanı | 6 ayrı SQLite | tek |
| Bedrock oyuncu | şifre girmek zorunda | **şifresiz** (Floodgate) |
| 2FA | eklenti ile | yerleşik (TOTP) |
| Hash | BCrypt | Argon2id |
| Limbo | yok (dünyada bekliyor) | ayrı boş oda |

**Bedeli:** ayrı bir `auth` (limbo) konteyneri — 768 MB. Oyuncu `/login`
yazana kadar orada bekler, sonra lobiye geçer. Bu oda dışarıya kapalı
(sadece `127.0.0.1`).


## 8b. Yetki matrisi — kim ne yapabilir

Kaynak: `config/plugins/LuckPerms/groups.yml`. İzin adları **resmi listeden**
doğrulandı (essinfo.xeya.me); doctor + CI yazım hatasını yakalar.

| Yetenek | oyuncu | vip | mod | admin | kurucu |
|---|:--:|:--:|:--:|:--:|:--:|
| `/home` `/sethome` (1 ev) | ✅ | 5 ev | ✅ | ✅ | ✅ |
| `/tpa` `/spawn` `/warp` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/msg` `/r` `/mail` `/helpop` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/balance` `/pay` `/baltop` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/kit oyuncu` (1 saatte 1) | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/claim` `/trust` (ilk 4000 blok) | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/bw join` `/parkour join` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/hub` `/lobby` `/lobi` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `/fly` `/nick` `/hat` `/enderchest` | ❌ | ✅ | ✅ | ✅ | ✅ |
| `/kick` `/mute` `/ban` `/jail` | ❌ | ❌ | ✅ | ✅ | ✅ |
| `/vanish` `/invsee` `/socialspy` | ❌ | ❌ | ✅ | ✅ | ✅ |
| `/tp` (başkasına) `/tphere` | ❌ | ❌ | ✅ | ✅ | ✅ |
| `coreprotect inspect` (bakma) | ❌ | ❌ | ✅ | ✅ | ✅ |
| `coreprotect rollback` (geri alma) | ❌ | ❌ | ❌ | ✅ | ✅ |
| `/gamemode` `/give` `/stop` | ❌ | ❌ | ❌ | ✅ | ✅ |
| `/op` verme | ❌ | ❌ | ❌ | ❌ | ✅ |
| Sohbette renk | ❌ | ❌ | ❌ | ✅ | ✅ |

### Bilinçli eksikler ve gerekçeleri

| Verilmeyen | Neden |
|---|---|
| `essentials.chat.color` (oyuncu/vip) | **tag taklidi** — biri kendini `[KURUCU]` gibi gösterebilirdi |
| `essentials.gamemode` / `give` | oyun dengesi; item/yaratıcı hilesi |
| `essentials.tp` (oyuncuda) | başkasının yanına izinsiz ışınlanma |
| `coreprotect.rollback` (mod'da) | yanlış komut = dünya silinir; admin ve üstü |
| `bukkit.command.op` (admin'de) | yetki sızması; sadece kurucu |

> Not: `admin` grubunda `*` var ama `-bukkit.command.op`, `-essentials.stop`,
> `-essentials.reloadall` ile sınırlandı. LuckPerms'te **son yazılan kazanır**,
> bu yüzden negatifler `*`'dan sonra geliyor.

## 7. Tag sistemi (kafa + chat + isim etiketi)

Renkler **gradyan**: her harfe ayrı hex kodu, `scripts/gradient.py` üretiyor
(elle yazılmıyor). `--apply` / `--check` / `--preview`. Önizleme:
`docs/tag-onizleme.html`.

Tek kaynak **LuckPerms prefix** → üç yerde aynı tag. Ayrıntı:
`config/plugins/LuckPerms/README-taglar.md`

| Grup | Kafada | Chatte |
|---|---|---|
| kurucu | `KURUCU` | `[KURUCU] Ahmet » merhaba` |
| admin | `ADMIN` | `[ADMIN] Ahmet » merhaba` |
| mod | `MOD` | `[MOD] Ahmet » merhaba` |
| vip | `VIP` | `[VIP] Ayse » selam` |
| oyuncu | — (tag yok) | `Ahmet » merhaba` |
| bot | `[BOT]` | `[BOT] craftya-bot » ...` |

Not: EssentialsXChat bu yüzden **zorunlu** listeye alındı — onsuz chatte tag görünmüyordu.

## 8. Bot (`craftya-bot`) yetkileri

Bot **oyuncu gibi girer**, `op` **verilmez**. LuckPerms `bot` grubu:

```
craftya-bot  →  [BOT]  prefix
  izinler: essentials.tp, essentials.gamemode, worldedit.* (inşa),
           grimac.bypass (uçma), librelogin.* (kendi kaydı)
  YOK:    op, sunucu kapatma, whitelist yönetimi, ban (karar insanın)
```

Bot yaptığı her işlemi `logs/bot.log`'a yazar → "bot niye bunu yaptı" sorusunun cevabı
her zaman var.
