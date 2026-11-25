#!/usr/bin/env python3
"""Craftya MC — yapılandırma denetleyicisi.

Neden var: bozuk bir docker-compose.yml veya eksik .env, sunucuyu 3 dakika sonra
"neden açılmadı" sorusuyla baş başa bırakır. Bu script Docker kurmadan önce
kontrol eder: YAML sözdizimi, zorunlu anahtarlar, sır sızıntısı, port çakışması,
plugin jar'larının varlığı ve sağlamlığı.

Kullanım:  python3 scripts/doctor.py
Çıkış kodu: 0 = hazır, 1 = hata var (listelenir)
"""
from __future__ import annotations

import os
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OK, WARN, BAD = "  ✓", "  !", "  ✗"
problems: list[str] = []
warnings: list[str] = []


def ok(msg: str) -> None:
    print(f"{OK} {msg}")


def warn(msg: str) -> None:
    warnings.append(msg)
    print(f"{WARN} {msg}")


def bad(msg: str) -> None:
    problems.append(msg)
    print(f"{BAD} {msg}")


# --------------------------------------------------------------------------
# 1) compose dosyası
# --------------------------------------------------------------------------
def check_compose() -> dict:
    """YAML'ı oku. PyYAML yoksa basit bir yapı denetimine düşer (bağımlılık yok)."""
    path = ROOT / "docker-compose.yml"
    if not path.exists():
        bad("docker-compose.yml yok")
        return {}
    text = path.read_text(encoding="utf-8")

    # girinti tutarlılığı: tab karakteri YAML'de yasak
    if "\t" in text:
        bad("docker-compose.yml içinde TAB var — YAML bunu kabul etmez")
    else:
        ok("YAML: tab karakteri yok")

    # açılış/kapanış parantez dengesi (kaba ama işe yarar)
    for ch_open, ch_close in (("[", "]"), ("{", "}")):
        if text.count(ch_open) != text.count(ch_close):
            bad(f"YAML: {ch_open}{ch_close} dengesi bozuk "
                f"({text.count(ch_open)} aç / {text.count(ch_close)} kapa)")

    try:
        import yaml  # type: ignore

        data = yaml.safe_load(text)
        ok("YAML: PyYAML ile ayrıştırıldı")
        services = (data or {}).get("services", {}) or {}
        ok(f"YAML: {len(services)} servis tanımlı ({', '.join(services)})")
        for need in ("minecraft", "backup"):
            if need not in services:
                bad(f"compose: '{need}' servisi eksik")
        mc = services.get("minecraft", {}) or {}
        env = mc.get("environment", {}) or {}
        for key in ("EULA", "TYPE", "VERSION"):
            if key not in env:
                bad(f"compose: minecraft/{key} tanımsız")
        if env.get("EULA") not in ("TRUE", True):
            bad("compose: EULA 'TRUE' olmalı (Mojang EULA kabulü zorunlu)")
        if str(env.get("TYPE", "")).upper() != "PAPER":
            warn(f"compose: TYPE={env.get('TYPE')} — beklenen PAPER")
        return env
    except ImportError:
        warn("PyYAML yok: derin YAML denetimi atlandı (pip install pyyaml)")
    except Exception as exc:  # noqa: BLE001
        bad(f"YAML ayrıştırılamadı: {exc}")
    return {}


# --------------------------------------------------------------------------
# 2) .env
# --------------------------------------------------------------------------
def parse_env(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    if not path.exists():
        return out
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip().strip('"').strip("'")
    return out


def check_env() -> dict[str, str]:
    env_path = ROOT / ".env"
    example = ROOT / ".env.example"
    if not example.exists():
        bad(".env.example yok — kurulum rehberi kayıp")
    if not env_path.exists():
        warn(".env yok — 'cp .env.example .env' çalıştır (varsayılanlarla açılır ama sırlar boş)")
        return {}
    ok(".env bulundu")

    env = parse_env(env_path)
    example_keys = set(parse_env(example)) if example.exists() else set()
    missing = example_keys - set(env)
    if missing:
        warn(f".env'de eksik anahtar(lar): {', '.join(sorted(missing))}")
    else:
        ok(f".env: {len(env)} anahtar, .env.example ile uyumlu")

    rcon = env.get("RCON_PASSWORD", "")
    if not rcon:
        bad("RCON_PASSWORD boş — uzaktan yönetim çalışmaz")
    elif rcon in ("craftya-rcon", "degistir-beni-uzun-bir-sifre", "changeme", "password"):
        bad("RCON_PASSWORD hâlâ örnek değer — değiştir")
    elif len(rcon) < 12:
        warn(f"RCON_PASSWORD kısa ({len(rcon)} karakter) — en az 16 önerilir")
    else:
        ok("RCON_PASSWORD: özel ve yeterince uzun")

    mem = env.get("MC_MEMORY", "3G")
    if not re.fullmatch(r"\d+[MGmg]", mem):
        bad(f"MC_MEMORY biçimi yanlış: '{mem}' (örn. 3G)")
    else:
        ok(f"MC_MEMORY: {mem}")

    online = env.get("MC_ONLINE_MODE", "TRUE").upper()
    if online != "TRUE":
        warn("MC_ONLINE_MODE=FALSE — korsan hesaplar girebilir, hile denetimi yok")
    else:
        ok("MC_ONLINE_MODE: TRUE (sadece orijinal hesaplar)")

    if env.get("PLAYIT_SECRET"):
        ok("PLAYIT_SECRET: dolu (tünel profili kullanılabilir)")
    else:
        warn("PLAYIT_SECRET boş — '--profile tunnel' çalışmaz, doğrudan port gerekir")

    # sır sızıntısı: .env git'te takip ediliyor mu?
    gi = ROOT.parent / ".gitignore"
    ignored = gi.exists() and any(
        line.strip() in (".env", "mcserver/.env") for line in gi.read_text().splitlines()
    )
    if not ignored:
        bad(".env .gitignore'da değil — sırlar repoya girebilir")
    else:
        ok("Sır koruması: .env .gitignore'da")
    return env


# --------------------------------------------------------------------------
# 3) plugin jar'ları
# --------------------------------------------------------------------------
CORE_PLUGINS = ("ViaVersion", "ViaBackwards", "Geyser", "floodgate")


def check_plugins() -> None:
    # CI'da jar indirilmez (ağ + telif): eksik plugin bilgilendirme olur, hata değil.
    ci = os.environ.get("CI", "").lower() in ("true", "1", "yes")
    report = warn if ci else bad

    pdir = ROOT / "plugins"
    if not pdir.exists():
        report("plugins/ klasörü yok")
        return
    jars = sorted(pdir.glob("*.jar"))
    if not jars:
        report("plugins/ boş — 'scripts/fetch-plugins.sh' çalıştır")
        return

    names = " ".join(j.name.lower() for j in jars)
    for want in CORE_PLUGINS:
        if want.lower() not in names:
            report(f"çekirdek plugin eksik: {want} (fetch-plugins.sh)")
        else:
            ok(f"plugin var: {want}")

    for jar in jars:
        try:
            with zipfile.ZipFile(jar) as zf:
                bad_zip = zf.testzip()
                has_meta = "plugin.yml" in zf.namelist() or "paper-plugin.yml" in zf.namelist()
            if bad_zip is not None:
                bad(f"{jar.name}: bozuk jar (ilk hatalı dosya: {bad_zip})")
            elif not has_meta:
                warn(f"{jar.name}: plugin.yml yok — Paper bunu yüklemeyebilir")
        except zipfile.BadZipFile:
            bad(f"{jar.name}: jar değil (indirme hata sayfası olabilir) — sil ve tekrar indir")
    ok(f"plugin denetimi bitti ({len(jars)} jar)")


# --------------------------------------------------------------------------
# 4) config dosyaları
# --------------------------------------------------------------------------
def check_config() -> None:
    cdir = ROOT / "config"
    must = ["server.properties", "bukkit.yml", "spigot.yml", "paper-global.yml"]
    for name in must:
        p = cdir / name
        if not p.exists():
            bad(f"config/{name} yok")
        elif p.stat().st_size == 0:
            bad(f"config/{name} boş")
    ok(f"config/: {len(list(cdir.rglob('*'))) if cdir.exists() else 0} dosya")

    geyser = cdir / "plugins" / "Geyser-Spigot" / "config.yml"
    if geyser.exists():
        text = geyser.read_text(encoding="utf-8")
        if "auth-type: floodgate" not in text:
            warn("Geyser: auth-type floodgate değil — Bedrock oyuncuları Java hesabı isteyecek")
        else:
            ok("Geyser: floodgate yetkilendirmesi açık")
    else:
        warn("Geyser config yok — ilk açılışta üretilir, sonra kopyalayıp özelleştir")


def main() -> int:
    print("Craftya MC — yapılandırma denetimi\n")
    print("[1/4] docker-compose.yml")
    compose_env = check_compose()

    print("\n[2/4] .env ve sırlar")
    env = check_env()

    # birlikte değerlendirme: compose .env'i okuyabiliyor mu
    if compose_env and env:
        mem = compose_env.get("MEMORY", "")
        if "${MC_MEMORY" in str(mem) and "MC_MEMORY" not in env:
            warn("compose MC_MEMORY'yi .env'den bekliyor ama .env'de yok")

    print("\n[3/4] plugin jar'ları")
    check_plugins()

    print("\n[4/4] sunucu ayarları")
    check_config()

    print("\n" + "-" * 60)
    if problems:
        print(f"HAZIR DEĞİL — {len(problems)} hata, {len(warnings)} uyarı\n")
        for p in problems:
            print(f"  ✗ {p}")
        return 1
    print(f"HAZIR — 0 hata, {len(warnings)} uyarı")
    print("\nBaşlat:  docker compose up -d")
    print("İzle:    docker compose logs -f minecraft")
    return 0


if __name__ == "__main__":
    sys.exit(main())
