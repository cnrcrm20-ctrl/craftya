# Tag sistemi — nerede, nasıl, neden

## Tek kaynak: LuckPerms prefix

```
config/plugins/LuckPerms/groups.yml   ← tag burada tanımlı (prefix)
        │
        ├──→ TAB/config.yml           → kafada (tab listesi) görünür
        ├──→ EssentialsXChat/config  → chatte görünür
        └──→ isim etiketi (nametag)   → karakterin üstünde görünür
```

Tag'i değiştirmek için **tek yer**: `groups.yml` içindeki `prefix` satırı.
Yeniden başlatma sonrası üçü birden değişir. Üç ayrı yerde tag yazmak = kaçınılmaz tutarsızlık.

## Görünüm

| Grup | Kafada | Chatte |
|---|---|---|
| kurucu | `KURUCU` (koyu kırmızı, kalın) | `[KURUCU] Ahmet » merhaba` |
| admin | `ADMIN` (kırmızı, kalın) | `[ADMIN] Ahmet » merhaba` |
| mod | `MOD` (mavi, kalın) | `[MOD] Ahmet » merhaba` |
| vip | `VIP` (sarı, kalın) | `[VIP] Ayse » selam` |
| oyuncu | (tag yok, sadece isim) | `Ahmet » merhaba` |
| bot | `[BOT]` (gri/mavi) | `[BOT] craftya-bot » tarama bitti` |

Renk mantığı: yukarı çıktıkça daha sıcak renk. Süs değil, hiyerarşi.

## Neden "havalı" duruyor (tasarım kararları)

1. **Tutarlılık > süs.** Aynı tag, aynı renk, aynı sıra — üç yerde. Göz alışıyor, profesyonel duruyor.
2. **Kalın sadece yetkili gruplarda.** Oyuncuda tag yok → sohbet okunur kalıyor. Herkes renkliyse hiçbir şey öne çıkmaz.
3. **Ayırıcı `»`** — kare parantez yerine ince ok. Daha az gürültü, modern görünüm.
4. **Kurucuya `★`** — tek karakter, tek yerde. Abartmıyoruz.
5. **Kafa sıralaması:** kurucu en üstte, bot en altta. Kalabalıkta kimin kim olduğu bir bakışta belli.
6. **Kafada 4 satır** (ayırıcı + isim + online/TPS + mc.craftya.dev). Daha fazlası ekranı yiyor.

## Renk geçişleri (gradyan)

Minecraft'ta gradyan diye bir şey **yok** — tek yolu her harfe ayrı hex kodu basmak:

```
&#ff2d55K&#ff4d4cU&#ff6243R&#ff8848U&#ffb158C&#ffd166U
```

Bunu elle yazmak = hata. Bu yüzden renkler **üretiliyor**:

```
python3 scripts/gradient.py --apply     # config'lere yaz
python3 scripts/gradient.py --check     # güncel mi (CI bunu çalıştırır)
python3 scripts/gradient.py --preview   # docs/tag-onizleme.html
python3 scripts/gradient.py --list      # paletler
```

Tek kaynak: `PALETLER` sözlüğü (`scripts/gradient.py`). Renk değiştirmek için
orayı düzelt, `--apply` çalıştır. Config'lerdeki `# >>> GRADIENT:BEGIN ...` /
`# <<< GRADIENT:END ...` arası **elle düzenlenmez** — bir sonraki `--apply` üzerine yazar.

### Neden renkler "temiz" geçiyor

Geçiş sRGB uzayında düz çizgiyle yapılsaydı arada gri/çamur ton çıkardı
(ör. kırmızıdan maviye giderken kirli mor). Burada:

```
sRGB → doğrusal ışık → interpolasyon → sRGB
```

Yani ışık şiddeti eşit adımlarla değişiyor, algılanan geçiş pürüzsüz oluyor.

### Nerede hex, nerede hex yok

| Yer | Format | Not |
|---|---|---|
| LuckPerms prefix | `&#rrggbb` | chat/tab plugini okur |
| TAB (header/footer) | `&#rrggbb` | 1.16+ |
| EssentialsXChat | `&#rrggbb` | EssentialsX 2.18+ |
| EssentialsX join/quit | `&#rrggbb` | aynı |
| `server.properties` MOTD | `§a` (16 renk) | **hex kabul etmiyor** |

MOTD için `LEGACY_RAMP` kullanılıyor: harfler dilimlere bölünüp sırayla
16 renge atanıyor. Daha kaba, ama gerçek geçiş (tek renge yuvarlanmıyor).

## Komutlarla atama

```
lp user Ahmet parent add kurucu
lp user Ayse parent add vip
lp user craftya-bot parent add bot
lp user Ahmet parent remove vip
```

## Test (sunucu açıldıktan sonra)

```
/tab reload          → kafa formatı yenilenir
/ess reload          → chat formatı yenilenir
lp user <isim> info  → hangi grupta, prefix ne
```
