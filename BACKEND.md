# Craftya — Backend Bilgileri

> Bu dosya **public** (anon/publishable) anahtarları içerir — client içine gömülmesi normaldir.
> `service_role` / `secret` anahtarları **asla** bu dosyaya yazılmaz; sadece sunucu tarafında,
> Supabase Secrets altında tutulur.

## Proje

| Alan | Değer |
|---|---|
| Proje adı | craftya |
| Ref | `dznywlwdvvkqunkmnnas` |
| Bölge | eu-central-1 (Frankfurt) |
| Plan | free |
| API URL | https://dznywlwdvvkqunkmnnas.supabase.co |
| Realtime URL | wss://dznywlwdvvkqunkmnnas.supabase.co/realtime/v1 |
| Storage URL | https://dznywlwdvvkqunkmnnas.supabase.co/storage/v1 |

## Public anahtarlar (client'a gömülebilir)

- **Publishable (yeni):** `sb_publishable_ptdYXqZ76f-xDhY0O1SulA_Q01mxOnu`
- **Anon (legacy JWT):** `eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImR6bnl3bHdkdnZrcXVua21ubmFzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTU1NDk5NzIsImV4cCI6MjExMTEyNTk3Mn0.07SX0L3zgtaE6TAGORhbtnBQcdbdYaGR1HdEQPSg6qY`

## Kurulan şema

### Tablolar
| Tablo | Amaç |
|---|---|
| `players` | oyuncu profili, skin indeksi, ban |
| `sessions` | lobi oturumları, kalp atışı, cihaz skoru |
| `modes` | oyun modları (duello / takimli / mega) |
| `queue_entries` | mod bazlı kuyruk |
| `rooms` | maç kaydı, seed, durum |
| `room_players` | maçtaki oyuncular, takım, skor |
| `block_types` | blok tanımları (metin + doku indeksi) — **31 blok seed** |
| `content_versions` | kanal bazlı içerik sürümü (delta indirme) |
| `match_results` | maç sonucu satırları |
| `player_stats` | toplam istatistik |
| `flags` | feature flags (chat, shop, survival, shadow) |

### Görünümler
| View | Amaç |
|---|---|
| `lobby_presence` | son 60 sn kalp atışı olan oturumlar → lobide kim var |
| `queue_counts` | mod bazlı bekleyen oyuncu sayısı → NPC üstündeki sayı |

### Realtime yayınında
`sessions`, `queue_entries`, `rooms`

### Storage
| Bucket | Public | Limit | Kullanım |
|---|---|---|---|
| `content` | ✅ | 100 MB/dosya | doku atlası, harita, script paketleri |

Yazma yalnızca `service_role`; okuma herkese açık.

## Modlar (seed)

| key | ad | format | oyuncu |
|---|---|---|---|
| `duello` | Düello | 1v1 | 2 |
| `takimli` | Takımlı | 5 takım × 2 | 10 |
| `mega` | Mega Takım | 4 takım × 3 | 12 |

## Bloklar (seed, 0-30)

0 hava · 1 taş · 2 toprak · 3 çim · 4 arnavut · 5 kum · 6 çakıl · 7 meşe kütük ·
8 meşe tahta · 9 meşe yaprak · 10 huş kütük · 11 huş tahta · 12 su · 13 lav · 14 cam ·
15 tuğla · 16 taş tuğla · 17 kömür · 18 demir · 19 altın · 20 elmas · 21 kar · 22 buz ·
23 obsidyen · 24 nether taşı · 25 ışık taşı · 26 meşale · 27 tezgah · 28 fırın ·
29 sandık · 30 ana kaya

Blok tanımı formatı (sunucudan gelen metin):
```json
{ "id": 47, "key": "mossy_stone", "display_name": "Yosunlu Taş",
  "tex_top": 112, "tex_side": 113, "tex_bottom": 112,
  "hardness": 1.5, "tool": "pickaxe", "light": 0,
  "opaque": true, "solid": true, "drops": [{"item":"mossy_stone","count":1}] }
```
Doku **indeks** olarak gelir; gerçek doku cihazdaki atlasta durur.

## RPC'ler (Postgres fonksiyonları)

| Fonksiyon | Kim çağırır | Ne yapar |
|---|---|---|
| `player_register(username)` | authenticated | profil açar, kullanıcı adı doğrular |
| `session_start(platform, app_version, device_score)` | authenticated | oturum açar, ölü oturumları temizler |
| `heartbeat(session_id, device_score?, app_version?)` | authenticated | 10 sn'de bir; state + room_id döner |
| `session_end(session_id)` | authenticated | oturumu kapatır |
| `queue_join(session_id, mode_key)` | authenticated | kuyruğa girer; **dolunca oda açar** (atomik) |
| `queue_leave(mode_key)` | authenticated | kuyruktan çıkar |
| `room_leave(room_id)` | authenticated | odadan ayrıl; ≤1 kalırsa maç biter |
| `match_finish(room_id, winner, scores)` | **service_role** | sonucu + istatistikleri yazar |
| `keepalive()` | anon | proje uyumasın diye ping |
| `maintenance(keep_days)` | service_role | ölü oturum/oda/temizlik |
| `publish_content(channel, ...)` | service_role | içerik sürümünü artırır |

### Test edildi ✅
2 test oyuncusu düello kuyruğuna girdi → 2. oyuncuda **oda otomatik açıldı**
(`arena_duel`, seed üretildi, ikisi de `state=room`). `match_finish` çağrıldı →
`match_results` yazıldı, `player_stats` doğru güncellendi (1 win / 3 kill / 1 death).
Test verileri sonra silindi.

## Görünümler (client'ın okuduğu)

| View | Kullanım |
|---|---|
| `lobby_presence` | son 60 sn kalp atışı olan oyuncular (kim online) |
| `queue_counts` | mod bazlı bekleyen sayı (NPC üstündeki rakam) |
| `online_counts` | online / kuyrukta / maçta / çalışan oda sayısı |
| `leaderboard_kills` | kill sıralaması (ilk 100) |
| `leaderboard_wins` | win + win-rate sıralaması (ilk 100) |

## Auth ayarları (uygulandı)

| Ayar | Değer | Sebep |
|---|---|---|
| E-posta kaydı | açık | hesap sistemi |
| E-posta doğrulama | **kapalı** (autoconfirm) | oyuncu doğrulama mailiyle uğraşmasın |
| JWT süresi | 604800 sn (7 gün) | haftada bir giriş yeter |
| Refresh token rotasyonu | açık | güvenlik |
| Site URL | https://craftya.app | redirect |
| Telefon girişi | kapalı | maliyet/SMS yok |

### Giriş yöntemi — KARAR: e-posta + sentetik adres

20+ sağlayıcı listesi web uygulamaları için. APK'da tablo değişiyor:

| Yöntem | APK'da | Karar |
|---|---|---|
| Google | OAuth client + APK imza SHA-1 şart; imza değişince kırılır | ❌ şimdilik |
| Discord / GitHub / Twitch | tarayıcıya atar, oyuna dönüş gerekir | ❌ oyun UX'ine aykırı |
| Apple | iOS | ❌ |
| Telefon (SMS) | ücret + TR teslimat sorunu | ❌ |
| **E-posta (sentetik)** | doğrulama kapalı, 10 sn kayıt | ✅ **seçildi** |
| Anonim | sıfır sürtünme ama cihaz değişince hesap gider | ❌ iptal |

**Akış:** oyuncu kullanıcı adı + şifre yazar → client arka planda
`kullaniciadi@craftya.local` sentetik adres üretir → `signUp` → `player_register(username)`.
Oyuncu mail kutusuna hiç bakmaz. Hesap kalıcıdır, telefon değişince geri gelir.
Sonradan Google bağlamak istenirse hesap bağlama ile eklenir.

> Panelde açılacak hiçbir şey yok — Email zaten ENABLED.

## Sonraki adımlar (backend)

- [ ] **Keep-alive zamanlayıcı** — `keepalive()` fonksiyonu hazır; haftada bir çağrılmalı
  (GitHub Actions cron veya UptimeRobot ücretsiz)
- [ ] **Atlas + harita yükleme** — `content` bucket'a service_role ile
- [ ] **Edge Function: `match-server`** — `match_finish` çağrısını güvenli yapmak için
- [ ] **Sıra (queue) üst sınırı** — 200 lobi tavanı için
- [ ] **Hile kontrolü** — heartbeat'te hareket hızı doğrulaması (maç içi P2P olduğu için Faz 3)
