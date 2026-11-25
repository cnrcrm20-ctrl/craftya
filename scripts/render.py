#!/usr/bin/env python3
"""Craftya — config üretici (şablon + .env → runtime/)

Neden: 6 oda × 4 dosya = 24 config. Elle kopyalamak hata demek. Burada her oda
`config/rooms/<oda>/room.yml` ile tanımlanır, ortak şablonlar `config/templates/`
altındadır, çıktı `runtime/` üretilir (git'te tutulmaz).

Sırlar: `${VAR}` yer tutucuları .env'den doldurulur. `.env` yoksa güvenli
varsayılan üretilir (ör. rcon şifresi `openssl rand`). Üretilen dosyalar 0600.

Kullanım:
  python3 scripts/render.py            # hepsini üret
  python3 scripts/render.py --check    # sadece doğrula, yazma
  python3 scripts/render.py --room bedwars
"""
from __future__ import annotations

import argparse
import json
import os
import re
import secrets
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CFG = ROOT / "config"
OUT = ROOT / "runtime"
PLACEHOLDER = re.compile(r"\$\{([A-Z0-9_]+)\}")


def load_env() -> dict[str, str]:
    env: dict[str, str] = {}
    for f in (ROOT / ".env", ROOT / ".env.local"):
        if not f.exists():
            continue
        for line in f.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            env[k.strip()] = v.strip().strip('"').strip("'")
    # os.environ üstün gelsin (CI matrix / secrets)
    env.update({k: v for k, v in os.environ.items() if k.isupper()})
    return env


def ensure_secrets(env: dict[str, str]) -> dict[str, str]:
    """Saklanacak sır sayısını azalt: eksikse üret (run içinde yaşar)."""
    generated = []
    for key, nbytes in (("VELOCITY_SECRET", 24), ("RCON_PASSWORD", 18)):
        if not env.get(key):
            env[key] = secrets.token_urlsafe(nbytes)
            generated.append(key)
    if generated:
        print(f"  ! üretildi (sadece bu çalıştırma için): {', '.join(generated)}")
        print("    kalıcı olması için .env dosyasına yaz.")
    return env


def expand(text: str, env: dict[str, str], where: str, check: bool) -> str:
    missing: list[str] = []

    def sub(m: re.Match) -> str:
        key = m.group(1)
        if key in env and env[key] != "":
            return env[key]
        # bilinçli boş bırakılabilen anahtarlar
        if key.endswith("_OPTIONAL"):
            return ""
        missing.append(key)
        return m.group(0)

    out = PLACEHOLDER.sub(sub, text)
    if missing:
        msg = f"{where}: tanımsız değişken(ler): {', '.join(sorted(set(missing)))}"
        if check:
            print(f"  ✗ {msg}")
        else:
            raise SystemExit(f"HATA: {msg}")
    return out


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    try:
        path.chmod(0o600)
    except OSError:
        pass


# ---------------------------------------------------------------- velocity
def _to_mb(val: str) -> int:
    """'2G' / '1536M' → MB."""
    val = val.strip().upper()
    if val.endswith("G"):
        return int(float(val[:-1]) * 1024)
    return int(float(val.rstrip("M")))


def mem_with_overhead(xmx: str) -> str:
    """Container limidi = heap + JVM'in heap dışı ihtiyacı.

    Neden: MEMORY=Xmx verirsen JVM metaspace/GC/thread'leri için yer kalmaz ve
    Docker container'ı OOM-kill eder (sunucu sebepsiz ölür gibi görünür).
    Ölçü: küçük sunucuda +350M, 2.5G'de +650M (~12-25%).
    """
    heap = _to_mb(xmx)
    extra = max(320, int(heap * 0.22))
    total = heap + extra
    return f"{total // 1024}G" if total % 1024 == 0 else f"{total}M"


def render_velocity(env: dict[str, str], check: bool) -> None:
    tpl = CFG / "velocity" / "velocity.toml.tpl"
    if not tpl.exists():
        print("  ✗ config/velocity/velocity.toml.tpl yok")
        return
    rooms = [d.name for d in room_dirs()]
    # [servers] bölümü: her oda bir satır, ad = oda adı
    lines = []
    for i, r in enumerate(rooms):
        port = env.get(f"PORT_{r.upper()}", "")
        if not port:
            print(f"  ✗ PORT_{r.upper()} tanımsız (.env)")
            continue
        lines.append(f'{r} = "127.0.0.1:{port}"')
    # SERVERS_BLOCK bilinçli yer tutucu: expand'dan önce doldurulur
    raw = tpl.read_text(encoding="utf-8").replace("__SERVERS_BLOCK__", "\n".join(lines))
    text = expand(raw, env, "velocity.toml", check)
    if not check:
        write(OUT / "velocity" / "velocity.toml", text)
    print(f"  ✓ velocity/velocity.toml ({len(lines)} oda)")


# ---------------------------------------------------------------- odalar
def room_dirs() -> list[Path]:
    """Gerçek odalar. '_' ile başlayanlar ortak dosya (oda değil, port tutmaz)."""
    return sorted(d for d in (CFG / "rooms").iterdir() if d.is_dir() and not d.name.startswith("_"))


def _strip_colors(text: str) -> str:
    """§ kodlarını temizle — renk gradyandan gelsin, çift renk olmasın."""
    out, i = [], 0
    while i < len(text):
        if text[i] == "\u00a7" and i + 1 < len(text):
            i += 2
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


mems: dict[str, str] = {}


def render_room(room: Path, env: dict[str, str], check: bool) -> None:
    meta = {}
    mfile = room / "room.yml"
    if mfile.exists():
        meta = parse_simple_yaml(mfile.read_text(encoding="utf-8"))
    name = room.name
    port = env.get(f"PORT_{name.upper()}") or meta.get("port", "")
    if not port:
        print(f"  ✗ {name}: port yok (PORT_{name.upper()})")
        return
    local_env = dict(env)
    # her odanın kendi RCON portu: taban 25575 + sıra (çakışma olmaz)
    rcon_port = str(25575 + [d.name for d in room_dirs()].index(name))
    local_env.update(
        {
            "ROOM": name,
            # MOTD_ROOM sadece METİN: renkleri gradient.py veriyor
            "MOTD_ROOM": _strip_colors(meta.get("motd") or name),
            # limbo odası boşluk dünyası kullanır (auth); diğerleri normal
            # boşsa template'teki generator-settings satırı tamamen düşer
            "PORT": str(port),
            "RCON_PORT": rcon_port,
            "MOTD": meta.get("motd", f"Craftya | {name}"),
            "MODE": meta.get("mode", "survival"),
            "DIFFICULTY": meta.get("difficulty", "normal"),
            "MAX_PLAYERS": str(meta.get("max_players", 20)),
            "VIEW_DISTANCE": str(meta.get("view_distance", 8)),
            "SIM_DISTANCE": str(meta.get("sim_distance", 6)),
            "LEVEL_SEED": meta.get("seed", name),
            "LEVEL_TYPE": meta.get("level_type", "minecraft:normal"),
            # limbo için boş dünya; dolu değilse satır hiç yazılmaz
            "GENERATOR_SETTINGS": meta.get("generator_settings") or "__NONE__",
            "XMX": meta.get("xmx", "2G"),
            "SOFTMAX": meta.get("softmax", "512M"),
        }
    )
    tdir = CFG / "templates"
    produced = 0
    for tpl in sorted(tdir.glob("*.tpl")):
        target_name = tpl.name[:-4]  # ".tpl" at
        raw = tpl.read_text(encoding="utf-8")
        # GENERATOR_SETTINGS boşsa satırı tamamen düşür (boş değer sunucuyu bozar)
        gs = local_env.get("GENERATOR_SETTINGS")
        if gs and gs != "__NONE__":
            # level-type satırının hemen altına generator-settings ekle
            out_lines = []
            for line in raw.splitlines():
                out_lines.append(line)
                if line.startswith("level-type="):
                    out_lines.append(f"generator-settings={gs}")
            raw = "\n".join(out_lines) + "\n"
        text = expand(raw, local_env, f"{name}/{target_name}", check)
        produced += 1
        if not check:
            write(OUT / "rooms" / name / target_name, text)
    if not check:
        jvm = (
            f"-Xms{meta.get('xms', '256M')} -Xmx{local_env['XMX']} "
            f"-XX:+UseG1GC -XX:SoftMaxHeapSize={local_env['SOFTMAX']} "
            f"-XX:+ParallelRefProcEnabled -XX:MaxGCPauseMillis=200 "
            f"-XX:+UnlockExperimentalVMOptions -XX:+DisableExplicitGC "
            f"-XX:G1NewSizePercent=30 -XX:G1MaxNewSizePercent=40 "
            f"-XX:G1HeapRegionSize=8M -XX:G1ReservePercent=20 "
            f"-Djdk.nio.maxCachedBufferSize=262144"
        )
        write(OUT / "rooms" / name / "jvm.args", jvm + "\n")
        meta_out = dict(meta, rcon_port=int(rcon_port), port=int(port))
        write(OUT / "rooms" / name / "room.meta.json",
              json.dumps(meta_out, indent=2, ensure_ascii=False))
        produced += 2
    # ortak komut kısayolları: 6 oyun odası (auth/limbo hariç — orada /hub olmaz)
    common = CFG / "rooms" / "_common" / "commands.yml"
    if common.exists() and not check and name != "auth":
        write(OUT / "rooms" / name / "commands.yml", common.read_text(encoding="utf-8"))
        produced += 1
    if not check:
        mems[name.upper()] = mem_with_overhead(meta.get("xmx", "1G"))
    print(f"  ✓ rooms/{name} ({produced} dosya, port {port})")


def parse_simple_yaml(text: str) -> dict:
    """Minimal YAML: 'anahtar: deger' + yorumlar. Bağımlılık yok, yeterli."""
    out: dict[str, str] = {}
    for line in text.splitlines():
        s = line.split("#", 1)[0].strip()
        if not s or ":" not in s:
            continue
        k, v = s.split(":", 1)
        out[k.strip()] = v.strip().strip('"').strip("'")
    return out


# ---------------------------------------------------------------- pluginler
# Proxy'de çalışan pluginler (jar'ı da config'i de runtime/proxy-plugins'e gider)
PROXY_PLUGINS = {"Geyser-Velocity", "floodgate", "velocity-hub-command", "LibreLogin"}


def render_require_groups(env: dict[str, str], check: bool) -> None:
    """ADMIN_PLAYERS → LuckPerms require-groups.yml (ilk açılışta yetki verir).

    Biçim: players:
             .bozturk2642:
               - group.kurucu
           .bozturk2642 nokta ile başlar → Bedrock (Floodgate) oyuncusu.
    """
    raw = env.get("ADMIN_PLAYERS", "").strip()
    if not raw:
        groups = {}
    else:
        groups = {}
        for item in raw.split(","):
            item = item.strip()
            if not item or ":" not in item:
                continue
            player, grp = item.rsplit(":", 1)
            groups.setdefault(player.strip(), []).append(f"group.{grp.strip()}")
    lines = ["players:"]
    for player, perms in groups.items():
        lines.append(f"  {player}:")
        for perm in perms:
            lines.append(f"    - {perm}")
    block = "\n".join(lines)

    tpl = ROOT / "scripts" / "require-groups.yml"
    if not tpl.exists():
        return
    text = re.sub(r"# >>> AUTO_GROUPS:BEGIN.*?# <<< AUTO_GROUPS:END",
                  "# >>> AUTO_GROUPS:BEGIN\n" + block + "\n# <<< AUTO_GROUPS:END",
                  tpl.read_text(encoding="utf-8"), flags=re.S)
    if not check:
        write(OUT / "plugins" / "LuckPerms" / "require-groups.yml", text)
    n = len(groups)
    print(f"  ✓ LuckPerms require-groups ({n} oyuncu)" if n
          else "  · LuckPerms require-groups (boş — ADMIN_PLAYERS tanımsız)")


def render_plugin_configs(env: dict[str, str], check: bool) -> None:
    pdir = CFG / "plugins"
    if not pdir.exists():
        return
    n = 0
    for src in pdir.rglob("*"):
        if not src.is_file():
            continue
        rel = src.relative_to(pdir)
        text = expand(src.read_text(encoding="utf-8"), env, f"plugins/{rel}", check)
        if not check:
            # proxy pluginleri proxy dizinine, geri kalanı oda plugin dizinine
            base = "proxy-plugins" if Path(rel).parts[0] in PROXY_PLUGINS else "plugins"
            write(OUT / base / rel, text)
        n += 1
    print(f"  ✓ plugin ayarları ({n} dosya)")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--room")
    args = ap.parse_args()

    env = ensure_secrets(load_env())

    # port tablosu: .env'de yoksa varsayılan
    defaults = {
        "PORT_VELOCITY": "25565",
        "PORT_LOBBY": "25566",
        "PORT_SURVIVAL": "25567",
        "PORT_SKYBLOCK": "25568",
        "PORT_BEDWARS": "25569",
        "PORT_DUELLO": "25570",
        "PORT_PARKUR": "25571",
    }
    for k, v in defaults.items():
        env.setdefault(k, v)

    print(f"Craftya config üretimi → {OUT} {'(kontrol)' if args.check else ''}")
    render_velocity(env, args.check)
    rooms = room_dirs()
    for room in rooms:
        if args.room and room.name != args.room:
            continue
        render_room(room, env, args.check)
    render_plugin_configs(env, args.check)
    render_require_groups(env, args.check)
    if not args.check and mems:
        envlines = ["# Üretildi: scripts/render.py — elle düzenleme",
                    "# Container bellek limitleri (heap + JVM heap dışı pay)"]
        envlines += [f"{k}_MEM={v}" for k, v in sorted(mems.items())]
        write(OUT / "rooms.env", "\n".join(envlines) + "\n")
        total = sum(_to_mb(v) for v in mems.values())
        print(f"  ✓ rooms.env ({len(mems)} oda, toplam {total/1024:.1f} GB)")

    print("")
    if args.check:
        print("kontrol bitti (dosya yazılmadı)")
    else:
        print(f"tamam — çıktı: {OUT}")
        print("sıradaki: python3 scripts/doctor.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
