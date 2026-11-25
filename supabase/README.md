# Supabase — global veri katmanı

## Ne var
| Tablo | İçerik | Kim yazar |
|---|---|---|
| `players` | isim, ilk/son görülme, Discord bağı, `/link` kodu | Edge Function |
| `player_stats` | blok, mob, ölüm, bedwars/duello/parkur | Edge Function (toplu) |
| `balances` | global para (6 odada aynı) | Edge Function |
| `bans` | **tüm odalarda geçerli** ban | Edge Function |
| `cosmetics_owned` | kozmetik sahipliği | Edge Function |
| `sessions` | kim hangi odada → site canlı durum | Edge Function |

## Kurulum
1. Supabase → craftya projesi → SQL Editor
2. `supabase/schema.sql` tamamını yapıştır → Run
3. Settings → API → `URL` ve `anon` anahtarını GitHub Secrets'a ekle

## Güvenlik kuralları (tartışmasız)
- **`service_role` anahtarı** yalnızca Edge Function içinde (Supabase secret olarak).
  Eklentiye, sitedeki JS'e, repoya **asla** girmez.
- RLS her tabloda açık. `anon` sadece **okur** — yazma policy'si yok.
- Yazma toplu (5 dk'da bir paket): her blok kırışında yazmak 100k istek/gün'ü 1 günde bitirir.

## Neden SQLite değil Postgres?
Minecraft eklentileri (CoreProtect vb.) SQLite/MySQL bilir, Postgres bilmez.
Bu yüzden: yerel veri SQLite, **global** veri Postgres (bu şema), aralarında
Cloudflare Worker / Edge Function köprüsü.

## Sıradaki adım (sunucu ayakta kaldıktan sonra)
`supabase/functions/` altına `player-sync` Edge Function'ı: toplu güncelleme alır,
`register_player` çağırır, ban kontrolü yapar.
