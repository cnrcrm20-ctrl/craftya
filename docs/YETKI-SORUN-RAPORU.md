# Yetki sistemi — durum raporu

Tarih: 2026-11-24 · Branch: `arena/deab440e-craftya`

Senin isteğin "üst rütbe alt rütbenin izinlerine erişsin, kurucuya her şey açık olsun"
idi. Hiyerarşiyi yazmaya başlamadan önce mevcut yapıyı kontrol ettim. **Hiyerarşi
zinciri zaten doğru yazılmış** (`vip→oyuncu`, `mod→vip`, `admin→mod`,
`kurucu→admin`) ama **zincirin hiçbir önemi yok**, çünkü LuckPerms bu dosyayı hiç
okumuyor.

Aşağıdaki 5 maddenin hepsi LuckPerms'in kendi kaynak kodundan doğrulandı. Tahmin
yok; hangi dosyanın hangi satırına baktığım yazılı.

---

## 1. `groups.yml` LuckPerms'in formatı değil

`config/plugins/LuckPerms/groups.yml` şöyle başlıyor:

```yaml
groups:
  oyuncu:
    display-name: 'Oyuncu'
    weight: 10
    prefix: ''
    suffix: ''
    parents: [oyuncu]
    permissions: [...]
```

LuckPerms'in grup dosyası **kök seviyede grup adı** bekler ve tanıdığı alanlar
sadece şunlar (kaynak: `AbstractConfigurateStorage.readNodes`):

| Alan | Biçim |
|---|---|
| `permissions` | liste: `- 'essentials.home'` veya `- 'x':` + `value: false` |
| `parents` | liste: `- oyuncu` |
| `prefixes` | liste: `- '&l&#ff2d55K...':` + `priority: 100` |
| `suffixes` | liste: aynı biçim |
| `meta` | liste: `- Kurucu:` + `value: 'x'` |

Yani:

- `groups:` kök anahtarı → LuckPerms bunu **grup adı** sanır, adı "groups" olan
  tek bir grup okur, altındakileri görmez.
- `weight: 10` → **tanınmıyor**, yok sayılır. LuckPerms'te ağırlık
  `weight.10` adlı bir node'dur (`Weight.parse`).
- `display-name: 'Oyuncu'` → **tanınmıyor**. Doğrusu `displayname.Oyuncu`
  node'u (`DisplayName.parse`).
- `prefix: '...'` / `suffix: '...'` → **tanınmıyor**. Doğrusu `prefixes` /
  `suffixes` listesi.

**Sonuç:** 73 izin, 6 grup, gradyanlı tag'ler — hiçbiri okunmuyor.

### Bunun görünmeyen ikinci etkisi: tag'ler de yok

`gradient.py` tag'i `prefix: '&l&#ff2d55K...'` satırına yazıyor. LuckPerms bu
satırı okumadığı için `%luckperms_prefix%` **boş** döner. TAB ve EssentialsXChat
o placeholder'ı kullanıyor → oyunda kafada ve chatte tag **görünmez**.

Yani "gradyanlı tag'ler hazır" dediğim kısım, oyunda çalışmayacaktı.

---

## 2. Depolama yöntemi yüzünden dosya zaten okunmuyor

`config/plugins/LuckPerms/config.yml`:

```yaml
storage-method: sqlite
storage-file: luckperms.db     # ← LuckPerms bu anahtarı tanımıyor
```

LuckPerms'in kendi dokümanı (`/wiki/Storage-types`):

> **Flatfile/local databases (H2 & SQLite)** — All data is stored within one file
> in the LuckPerms folder. **The data cannot be easily edited with a text editor.**

Ve `StorageFactory`'de yöntem → dosya eşleşmesi şöyle:

| `storage-method` | Dosya yeri |
|---|---|
| `h2` (varsayılan) | `luckperms-h2.db` |
| `sqlite` | `luckperms-sqlite.db` |
| `yaml` | `yaml-storage/groups/<ad>.yml` (grup başına dosya) |
| `yaml-combined` | `yaml-storage/groups.yml` (tek dosya) |
| `json-combined` | `json-storage/groups.json` |

`PLUGINS` klasörüne attığımız `groups.yml` bu listelerin **hiçbirinde yok**.
`plugins/LuckPerms/groups.yml` diye bir yol LuckPerms'te yok.

**Sonuç:** formatı düzeltsem bile `storage-method: sqlite` kaldığı sürece dosya
okunmaz. Ayrıca `yaml-storage/` alt klasörüne yazmamız gerekir.

> Not: `yaml-combined` bizim mevcut tek-dosya düzenimize birebir uyar.

---

## 3. Negatif izinler çalışmıyor → admin kendine OP verebilir

`admin` grubunda:

```yaml
permissions:
  - '*'
  - '-bukkit.command.op'
  - '-essentials.stop'
  - '-essentials.reloadall'
```

LuckPerms kaynak kodunda negasyon, node'un `value: false` olmasıyla ifade edilir.
`Permission.Builder.permission(String)` metodu **baştaki `-` işaretini
soymuyor** (kontrol ettim). Yani `-bukkit.command.op`, adı gerçekten
`-bukkit.command.op` olan ve **değeri `true`** olan bir izin olarak yüklenir —
hiçbir şeyle eşleşmeyen çöp bir node.

Doğru biçim:

```yaml
permissions:
  - '*'
  - bukkit.command.op:
      value: false
  - essentials.stop:
      value: false
  - essentials.reloadall:
      value: false
```

**Sonuç:** `admin` şu an `*` ile birlikte `bukkit.command.op`, `essentials.stop`
ve `essentials.reloadall` izinlerine de sahip. Yani mod'dan bir üst rütbe olması
gereken admin, sunucuyu durdurabilir ve kendine OP verebilir.

Bu, kullanıcının "kurucu en üst" isteğini de boşa çıkarıyor: admin kurucuyla
aynı güce sahip.

> Bu hata `doctor.py`'deki "kurucu op verebilir (admin yasağı ezildi)" kontrolünü
> **geçiyordu** — o kontrol de aynı yanlış sözdizimini aradığı için sahte güven
> veriyordu.

---

## 4. Kimse `oyuncu` grubuna girmiyor

LuckPerms'te yeni oyuncu **her zaman** `default` adlı gruba atanır. Bu sabit,
değiştirilebilir bir ayar değil (`GroupManager.DEFAULT_GROUP_NAME = "default"`,
`AbstractUserManager.giveDefaultIfNeeded`).

Bizim en alt grubumuzun adı `oyuncu`. `default` diye bir grup yok.

**Sonuç:** sunucuya ilk kez giren oyuncu boş bir gruba düşer; 73 iznin hiçbirini
almaz. `/spawn`, `/home`, `/claim`, `/bw join` — hiçbiri çalışmaz. Tam olarak
senin şikayet ettiğin durum, ve sebebi izin listesi değil, grup adı.

---

## 5. `require-groups.yml` diye bir LuckPerms özelliği yok

`scripts/require-groups.yml` → `runtime/plugins/LuckPerms/require-groups.yml`
üretiliyor ve şunu içeriyor:

```yaml
players:
  .bozturk2642:
    - group.kurucu
```

Fikrin kaynağı "LuckPerms'in kendi grupları dosyadan yükle özelliği var" idi.
LuckPerms kaynak ağacında ve wiki'de böyle bir dosya/özellik **yok**. Bu dosya
bir plugin tarafından okunmuyor.

**Sonuç:** Bedrock'tan `bozturk2642` olarak girdiğinde kurucu olmayacak, OP
olmayacak. Birlikte sunucuya girme planı bu yüzden tıkanır.

---

## Hiyerarşi kısmı: aslında doğru

İyi haber — senin asıl istediğin şey (üst rütbe alt rütbeyi miras alsın, kurucu
en üst, oyuncu en alt) zaten yazılmış:

```yaml
vip:     parents: [oyuncu]   weight: 20
mod:     parents: [vip]      weight: 50
admin:   parents: [mod]      weight: 80
kurucu:  parents: [admin]    weight: 100
```

Sorun zincirde değil, zincirin hiç yüklenmemesinde. Doğru biçime çevrildiğinde bu
zincir çalışır: `kurucu` → `admin` → `mod` → `vip` → `oyuncu` → `default`.

"Kurucuya allah ne verdiyse" kısmı için doğru kurgu: `kurucu`'ya `*` verilir,
`admin`'e `*` verilip op/stop/reloadall **doğru sözdizimiyle** negatiflenir.

---

## Düzeltme planı (senin onayına göre)

| # | İş | Dosya |
|---|---|---|
| 1 | Biçimi LuckPerms formatına çevir; `weight`/`display-name`/`prefix` → node | `groups.yml` + `gradient.py` |
| 2 | `default` grubu ekle (en alt), `oyuncu` onu miras alsın | `groups.yml` |
| 3 | Negatifleri `value: false` biçimine çevir | `groups.yml` |
| 4 | `storage-method: yaml-combined`, dosya `yaml-storage/groups.yml` | `config.yml` + `render.py` |
| 5 | `require-groups.yml` yerine gerçekten çalışan yol | `render.py` + `docker-compose.hub.yml` |
| 6 | Tüm odalar tek LuckPerms verisi paylaşsın | `docker-compose.hub.yml` |
| 7 | `doctor.py` + `ci.yml` kontrollerini doğru sözdizimine göre düzelt | `doctor.py`, `ci.yml` |

### 5. madde için seçenekler

`require-groups.yml` çalışmadığı için `.bozturk2642`'yi kurucu yapmanın gerçek
yolları:

- **A) Başlangıç komutu:** konteyner açılırken `lp user .bozturk2642 parent add kurucu`
  RCON ile çalıştırılır. `mc.sh`/workflow'a 3 satır.
- **B) YAML kullanıcı dosyası:** `yaml-storage/users/<uuid>.yml` — ama Floodgate
  UUID'si takma addan türetiliyor, kırılgan.
- **C) Web editör:** sen `/lp editor` ile tek seferde elle verirsin.

**Önerim A** — tekrar çalışabilir, sır yok, elle iş yok. B kırılgan, C her devirde
tekrar eder.

---

## Dürüst kapanış

Bu 5 maddeyi şimdiye kadar fark etmemişim. `groups.yml`'i geçen turda
"134 izin doğrulandı" diye rapor ettim — izin **adları** doğruydu, ama dosya
hiç yüklenmiyordu. `doctor.py` de aynı yanlış varsayımla yazıldığı için yeşil
gösteriyordu. Test edilebilir bir ortam (Docker/Java) bu sandbox'ta olmadığı
için ancak kaynak koduna bakınca çıktı.

Söylediklerin doğrultusunda sıradaki adım: raporu okuduktan sonra "devam" dersen
7 maddeyi sırayla yaparım, her adımda `doctor.py`'yi gerçek sözdizimine göre
düzeltip yeşile çekerim.
