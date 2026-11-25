#!/usr/bin/env python3
"""Craftya — gradyan (renk geçişi) üretici

Neden: Minecraft'ta "gradyan" diye bir şey yok. Tek yolu her harfe ayrı renk
kodu basmak. Elle 8 harfe 8 hex yazmak = hata + tutarsızlık. Burası tek kaynak:

    PALETLER (aşağıda)  →  tag/başlık metinleri  →  config dosyaları

Renk geçişi sRGB'de düz çizgiyle yapılmaz (arada çamur rengi çıkar).
Doğrusu: sRGB → doğrusal ışık → interpolasyon → sRGB. Bu yüzden geçişler
"parlak" duruyor, griye kaçmıyor.

Kullanım:
  python3 scripts/gradient.py --apply      # config dosyalarındaki blokları doldur
  python3 scripts/gradient.py --check      # dosyalar güncel mi (CI)
  python3 scripts/gradient.py --preview    # docs/tag-onizleme.html üret
  python3 scripts/gradient.py --demo "CRAFTYA" --from 00ff88 --to 00aaff
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# --------------------------------------------------------------------- palet
# Renk seçimi keyfi değil: her grup tek bir "renk ailesi" içinde kalıyor,
# sadece ton değişiyor. Böylece kafada 6 tag yan yana geldiğinde bile
# hangisinin hangisi olduğu bir bakışta anlaşılıyor.
PALETLER: dict[str, list[str]] = {
    # grup tag'leri
    "kurucu": ["ff2d55", "ff6b3d", "ffd166"],   # ateş: koyu kırmızı → altın
    "admin":  ["ff4d4d", "ff8a3d"],              # kırmızı → turuncu
    "mod":    ["4d9fff", "6ee7ff"],              # mavi → buz
    "vip":    ["ffc93c", "ffe9a3"],              # altın → saman
    "oyuncu": ["b8c0cc", "e6ebf2"],              # gri (sade, tag değil isim)
    "bot":    ["00d4a0", "4de3c0"],              # turkuaz → nane
    # marka / başlık
    "marka":  ["21d07a", "3fe0b0", "49c6ff"],    # yeşil → turkuaz → mavi
    "baslik": ["7ef29d", "49c6ff"],              # CRAFTYA başlığı
    "sonuk":  ["6b7280", "9aa4b2"],              # ikincil yazı
    "uyari":  ["ff9f43", "ffd166"],              # duyuru
}

# MOTD için kurgulanmış legacy dizileri: server.properties hex kabul etmiyor,
# 16 renge "en yakın" seçmek geçişi yok ediyor (hepsi aynı renge yuvarlanıyor).
# Burada renk sırası elle seçildi: komşu kodlar gerçekten farklı ton.
LEGACY_RAMP = {
    "marka":  ["a", "b", "3", "9"],        # yeşil → aqua → camgöbeği → mavi
    "baslik": ["a", "b", "3"],             # yeşil → aqua → camgöbeği
    "uyari":  ["6", "e", "f"],             # turuncu → sarı → beyaz
    "sonuk":  ["8", "7"],
    "kurucu": ["c", "6", "e"],             # kırmızı → turuncu → sarı
    "admin":  ["c", "6"],
    "mod":    ["9", "b"],
    "vip":    ["6", "e"],
    "bot":    ["2", "b"],
    "oyuncu": ["7", "f"],
}

# 16 eski renk (server.properties MOTD için — hex kabul etmiyor)
LEGACY = {
    "0": (0, 0, 0), "1": (0, 0, 170), "2": (0, 170, 0), "3": (0, 170, 170),
    "4": (170, 0, 0), "5": (170, 0, 170), "6": (170, 170, 0), "7": (170, 170, 170),
    "8": (85, 85, 85), "9": (85, 85, 255), "a": (85, 255, 85), "b": (85, 255, 255),
    "c": (255, 85, 85), "d": (255, 255, 85), "e": (255, 255, 85), "f": (255, 255, 255),
}


# ------------------------------------------------------------------- renk işi
def hex2rgb(h: str) -> tuple[int, int, int]:
    h = h.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def srgb2lin(c: float) -> float:
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lin2srgb(c: float) -> float:
    return c * 12.92 if c <= 0.0031308 else 1.055 * (c ** (1 / 2.4)) - 0.055


def lerp_light(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    """Doğrusal ışık uzayında karıştır — sRGB'de düz lerp griye/çamura kaçar."""
    out = []
    for ca, cb in zip(a, b):
        la, lb = srgb2lin(ca / 255), srgb2lin(cb / 255)
        v = la + (lb - la) * t
        out.append(round(lin2srgb(v) * 255))
    return tuple(out)


def ramp(stops: list[str], n: int) -> list[tuple[int, int, int]]:
    """n adım için duraklardan renk dizisi (çok duraklı destekler)."""
    pts = [hex2rgb(s) for s in stops]
    if n <= 0:
        return []
    if n == 1:
        return [pts[0]]
    segs = len(pts) - 1
    out = []
    for i in range(n):
        pos = i / (n - 1) * segs
        k = min(int(pos), segs - 1)
        out.append(lerp_light(pts[k], pts[k + 1], pos - k))
    return out


def gradient(text: str, stops: list[str], *, bold: bool = False, italic: bool = False,
             reset: bool = True) -> str:
    """Her harfe hex kodu → gerçek gradyan. EssentialsX / TAB / LuckPerms okur.

    reset=False: sondaki &r eklenmez. Bir tırnak içine gömülecekse şart —
    aksi halde &r tırnağın dışına düşer ve YAML bozulur.
    """
    if not text:
        return ""
    colors = ramp(stops, len(text))
    parts = []
    if bold:
        parts.append("&l")
    if italic:
        parts.append("&o")
    for ch, (r, g, b) in zip(text, colors):
        parts.append(f"&#{r:02x}{g:02x}{b:02x}{ch}")
    if reset:
        parts.append("&r")
    return "".join(parts)


def nearest_legacy(rgb: tuple[int, int, int]) -> str:
    r, g, b = rgb
    best, bd = "f", 10**9
    for code, (lr, lg, lb) in LEGACY.items():
        d = (r - lr) ** 2 + (g - lg) ** 2 + (b - lb) ** 2
        if d < bd:
            best, bd = code, d
    return best


def legacy_gradient(text: str, stops: list[str], *, bold: bool = False,
                    key: str | None = None) -> str:
    """MOTD için bantlı geçiş.

    server.properties hex kabul etmiyor. "En yakın 16 renk" yöntemi işe yaramıyor
    (gradyanın tamamı tek renge yuvarlanıyor). Bu yüzden renk sırası LEGACY_RAMP'ten
    geliyor: harfler eşit dilimlere bölünür, her dilim bir sonraki renk kodu.
    """
    codes = LEGACY_RAMP.get(key or "", None) or [nearest_legacy(c) for c in ramp(stops, max(2, len(stops)))]
    if not text:
        return ""
    n = len(codes)
    out = ["§l"] if bold else []
    for i, ch in enumerate(text):
        seg = min(int(i / len(text) * n), n - 1)
        out.append(f"§{codes[seg]}{ch}")
    out.append("§r")
    return "".join(out)


# ------------------------------------------------------------------ fallback
def mini_gradient(stops: list[str]) -> str:
    """Velocity/MiniMessage destekleyen yerler için kısa biçim."""
    return "<gradient:" + ":".join("#" + s.lstrip("#") for s in stops) + ">"


# ------------------------------------------------------- config blok üretimi
BEGIN = "# >>> GRADIENT:BEGIN {key}"
END = "# <<< GRADIENT:END {key}"

TARGETS = [
    "config/plugins/LuckPerms/groups.yml",
    "config/plugins/TAB/config.yml",
    "config/plugins/EssentialsXChat/config.yml",
    "config/plugins/Essentials/config.yml",
    "config/templates/server.properties.tpl",
    "config/velocity/velocity.toml.tpl",
]


TAG_ISIM = {"kurucu": "KURUCU", "admin": "ADMIN", "mod": "MOD",
            "vip": "VIP", "bot": "BOT", "oyuncu": ""}
GRUPLAR = ["kurucu", "admin", "mod", "vip", "oyuncu", "bot"]

# chat formatının geri kalanı: isim, ayırıcı, mesaj. Gradyan sadece tag'de —
# mesajın kendisi renkli olursa okunmaz.
CHAT_TAIL = "§f{DISPLAYNAME} §8» §f{MESSAGE}"


def tag_of(g: str, *, reset: bool = True) -> str:
    return gradient(TAG_ISIM[g], PALETLER[g], bold=True, reset=reset) if TAG_ISIM[g] else ""


def blocks_for(key: str) -> list[str]:
    """Bir anahtar için üretilecek TAM satırlar (marker'ın içine girer)."""
    if key.startswith("lp."):
        g = key[3:]
        t = tag_of(g)
        return [f"prefix: '{t}'" if t else "prefix: ''"]
    if key.startswith("chat."):
        g = key[5:]
        t = tag_of(g) or gradient("{DISPLAYNAME}", PALETLER[g])
        if g == "oyuncu":
            return [f"{g}: '§7{{DISPLAYNAME}} §8» §f{{MESSAGE}}'"]
        return [f"{g}: '{t}{CHAT_TAIL}'"]
    if key == "tab.header":
        return [f"- '{gradient('CRAFTYA', PALETLER['baslik'], bold=True)} §8| §7sunucu dunyasi'",
                "- '§8online: §a%server_online% §8· §7TPS: §a%server_tps_1_colored%'"]
    if key == "tab.footer":
        return [f"- '{gradient('mc.craftya.dev', PALETLER['marka'])}'",
                "- '§8bakim: 6 saatte bir, ~30sn'"]
    if key == "tab.baslik":
        return [f"format: '{gradient('CRAFTYA', PALETLER['baslik'], bold=True)} §8| §7durum'"]
    if key == "join":
        return ["custom-join-message: '"
                + gradient("CRAFTYA", PALETLER["baslik"], bold=True)
                + " §8— §ahos geldin §f%player%'"]
    if key == "quit":
        return ["custom-quit-message: '§8— §7gorusuruz §f%player%'"]
    if key == "proxy.motd":
        # MiniMessage'da <gradient> KAPATILMALI, yoksa ayrıcı da gradyana girer
        return ['motd = "' + mini_gradient(PALETLER["baslik"]) + 'CRAFTYA</gradient>'
                '<dark_gray> | <gray>sunucu dunyasi"']
    if key == "motd":
        # server.properties hex kabul ETMEZ → bantlı legacy geçiş.
        # Ön ek olarak odanın kendi MOTD'u korunur.
        lead = legacy_gradient("Craftya", PALETLER["marka"], bold=True, key="marka")
        # oda adı + açıklama; renk gradyandan gelir, room.yml sadece metin verir
        room = " §7${ROOM} §8— §7${MOTD_ROOM}"
        return [f"motd={lead} §8|{room}"]
    return [gradient(key.upper(), PALETLER.get(key, PALETLER["marka"]), bold=True)]


def _norm(text: str) -> str:
    """Karşılaştırma için: girintiyi yok say, satır satır."""
    return "\n".join(ln.strip() for ln in text.strip().splitlines() if ln.strip())


def apply_or_check(check: bool) -> int:
    drift = 0
    pat = re.compile(r"(?P<indent>[ \t]*)# >>> GRADIENT:BEGIN (?P<key>[a-z_.]+)\n"
                     r"(?P<body>.*?)"
                     r"[ \t]*# <<< GRADIENT:END (?P=key)", re.S)
    dup = re.compile(r"([ \t]*)# >>> GRADIENT:BEGIN (?P<k>[a-z_.]+)\n"
                     r"[ \t]*# <<< GRADIENT:END (?P=k)\n"
                     r"(?=[ \t]*# >>> GRADIENT:BEGIN (?P=k)\n)")
    for rel in TARGETS:
        f = ROOT / rel
        if not f.exists():
            continue
        src = f.read_text(encoding="utf-8")
        while dup.search(src):          # elle düzenleme artığı boş bloklar
            src = dup.sub("", src)

        def sub(m: re.Match) -> str:
            nonlocal drift
            ind, key = m.group("indent"), m.group("key")
            lines = blocks_for(key)
            # içerik marker ile aynı hizada (YAML için doğru olan bu)
            want = "\n".join(lines)
            have = _norm(m.group("body"))
            if have != _norm(want):
                drift += 1
                if check:
                    print(f"  ✗ {rel}: '{key}' güncel değil")
            body = "".join(f"{ind}{ln}\n" for ln in lines)
            return f"{ind}{BEGIN.format(key=key)}\n{body}{ind}{END.format(key=key)}"

        out = pat.sub(sub, src)
        if not check and out != src:
            f.write_text(out, encoding="utf-8")
            print(f"  ✓ {rel}")
    if check:
        print("  ✓ gradyanlar güncel" if drift == 0 else
              f"  ✗ {drift} blok eski — 'python3 scripts/gradient.py --apply'")
        return 0 if drift == 0 else 1
    return 0


# ------------------------------------------------------------------- önizleme
def mc_html(text: str) -> str:
    """&/§ kodlarını HTML'e çevir — oyundaki görünüme yakın önizleme.

    Hem hex (&#rrggbb) hem 16 eski renk kodu (§a, &7...) desteklenir;
    MOTD gibi legacy yerler de doğru renklenir.
    """
    text = text.replace("\u00a7", "&")
    legacy = {"0": "#000000", "1": "#0000aa", "2": "#00aa00", "3": "#00aaaa",
              "4": "#aa0000", "5": "#aa00aa", "6": "#ffaa00", "7": "#aaaaaa",
              "8": "#555555", "9": "#5555ff", "a": "#55ff55", "b": "#55ffff",
              "c": "#ff5555", "d": "#ffff55", "e": "#ffff55", "f": "#ffffff"}
    out, color, bold = [], "#e8e8e8", False
    i = 0
    while i < len(text):
        if text[i] == "&" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "#" and re.fullmatch(r"#[0-9a-fA-F]{6}", text[i + 1:i + 8] or ""):
                color = text[i + 1:i + 8]
                i += 8
                continue
            low = nxt.lower()
            if low in legacy:
                color = legacy[low]
                i += 2
                continue
            if low == "l":
                bold = True
                i += 2
                continue
            if low == "r":
                color, bold = "#e8e8e8", False
                i += 2
                continue
            i += 2          # bilinmeyen kod: yut
            continue
        esc = text[i].replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        weight = "font-weight:700" if bold else "font-weight:400"
        out.append(f'<span style="color:{color};{weight}">{esc}</span>')
        i += 1
    return "".join(out)


def preview() -> None:
    rows = []
    rows.append(("Kurucu", mc_html(tag_of("kurucu")) + " " + mc_html("&fAhmet") + "  " + mc_html("&8» &7kanka selam")))
    rows.append(("Admin", mc_html(tag_of("admin")) + " " + mc_html("&fAhmet") + "  " + mc_html("&8» &7kayit acildi")))
    rows.append(("Mod", mc_html(tag_of("mod")) + " " + mc_html("&fAhmet") + "  " + mc_html("&8» &7kural ihlali yok")))
    rows.append(("VIP", mc_html(tag_of("vip")) + " " + mc_html("&fAyse") + "  " + mc_html("&8» &7selam")))
    rows.append(("Oyuncu", mc_html("&7Ayse") + "  " + mc_html("&8» &7merhaba")))
    rows.append(("Bot", mc_html(tag_of("bot")) + " " + mc_html("&7craftya-bot") + "  " + mc_html("&8» &7dunya taramasi bitti")))
    rows.append(("Başlık", mc_html(gradient("CRAFTYA", PALETLER["baslik"], bold=True)) + " " + mc_html("&8| &7sunucu dunyasi")))
    rows.append(("Adres", mc_html(gradient("mc.craftya.dev", PALETLER["marka"]))))
    rows.append(("İkincil", mc_html(gradient("bakim: 6 saatte bir, ~30sn", PALETLER["sonuk"]))))
    rows.append(("Giriş", mc_html(gradient("CRAFTYA", PALETLER["baslik"], bold=True))
                 + mc_html(" §8— §ahos geldin §fAhmet")))
    rows.append(("MOTD", mc_html(legacy_gradient("Craftya", PALETLER["marka"], bold=True, key="marka"))
                 + mc_html(" §8| §7lobby §8— §7bir oda sec")))

    body = "\n".join(
        f'<div class="row"><div class="lbl">{lbl}</div><div class="val">{val}</div></div>'
        for lbl, val in rows
    )
    html = f"""<!doctype html>
<html lang="tr"><head><meta charset="utf-8"><title>Craftya — tag önizleme</title>
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin:0; padding:32px; background:#0d1117; color:#c9d1d9;
         font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif; }}
  h1 {{ font-size:20px; font-weight:600; margin:0 0 4px; }}
  p.sub {{ color:#8b949e; font-size:13px; margin:0 0 28px; }}
  .card {{ background:#161b22; border:1px solid #21262d; border-radius:10px; padding:8px 20px; }}
  .row {{ display:flex; align-items:baseline; gap:20px; padding:14px 0; border-bottom:1px solid #21262d; }}
  .row:last-child {{ border-bottom:none; }}
  .lbl {{ width:90px; flex:none; font-size:11px; text-transform:uppercase;
          letter-spacing:.08em; color:#6e7681; }}
  .val {{ font-family:ui-monospace,SFMono-Regular,Menlo,monospace; font-size:17px; }}
  .note {{ margin-top:24px; font-size:12.5px; color:#8b949e; line-height:1.7; }}
  .note b {{ color:#c9d1d9; font-weight:600; }}
</style></head><body>
<h1>Tag önizleme — oyundaki görünüm</h1>
<p class="sub">Aynı tag kafada, chatte ve karakterin üstünde. Renkler <code>scripts/gradient.py</code> paletinden üretiliyor.</p>
<div class="card">{body}</div>
<div class="note">
  <b>Nasıl çalışıyor:</b> Minecraft'ta gradyan yok, her harfe ayrı hex kodu basılıyor.
  Geçiş sRGB yerine doğrusal ışık uzayında yapılıyor — arada gri/çamur ton çıkmıyor.<br>
  <b>Sınır:</b> <code>server.properties</code> MOTD hex kabul etmiyor; orada 16 eski renge yuvarlanan
  bantlı geçiş kullanılıyor (daha kaba ama gerçek geçiş).
</div>
</body></html>"""
    out = ROOT / "docs/tag-onizleme.html"
    out.write_text(html, encoding="utf-8")
    print(f"  ✓ {out.relative_to(ROOT)}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Craftya gradyan üretici")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("--demo")
    ap.add_argument("--from", dest="frm")
    ap.add_argument("--to", dest="to")
    ap.add_argument("--legacy", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    if a.list:
        for k, v in PALETLER.items():
            print(f"  {k:8s} {' → '.join(v)}")
        return 0
    if a.demo:
        stops = [a.frm or "21d07a", a.to or "49c6ff"]
        print(legacy_gradient(a.demo, stops, bold=True, key="marka") if a.legacy
              else gradient(a.demo, stops, bold=True))
        return 0
    rc = 0
    if a.apply or a.check:
        rc = apply_or_check(check=a.check)
    if a.preview:
        preview()
    if not any([a.apply, a.check, a.preview]):
        ap.print_help()
    return rc


if __name__ == "__main__":
    sys.exit(main())
