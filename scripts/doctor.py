#!/usr/bin/env python3
"""Craftya — yapılandırma doktoru.

Tek komutla: "sunucu açılmaya hazır mı?"
Docker kurmadan, sunucu açmadan, 20+ kontrol. Bozuk config → 3 dakika sonra
"neden açılmadı" sorusu yerine, burada isimlendirilmiş hata.

Kullanım:
  python3 scripts/doctor.py
  python3 scripts/doctor.py --verbose

Çıkış kodu: 0 = hazır, 1 = en az bir HATA var.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OK, WARN, BAD = "  \033[32m✓\033[0m", "  \033[33m!\033[0m", "  \033[31m✗\033[0m"
problems: list[str] = []
warnings: list[str] = []
VERBOSE = "--verbose" in sys.argv
CI = os.environ.get("CI", "").lower() in ("true", "1", "yes")

ROOMS = ["lobby", "survival", "skyblock", "bedwars", "duello", "parkur"]
PORTS = {"velocity": 25565, "lobby": 25566, "survival": 25567,
         "skyblock": 25568, "bedwars": 25569, "duello": 25570, "parkur": 25571}


def ok(m: str) -> None:
    if VERBOSE:
        print(f"{OK} {m}")


def warn(m: str) -> None:
    warnings.append(m)
    print(f"{WARN} {m}")


def bad(m: str) -> None:
    problems.append(m)
    print(f"{BAD} {m}")


def section(n: str) -> None:
    print(f"\n\033[36m[{n}]\033[0m")


# --------------------------------------------------------------------------
def check_env() -> dict[str, str]:
    section("1/7 ortam değişkenleri")
    env: dict[str, str] = {}
    f = ROOT / ".env"
    if not f.exists():
        warn(".env yok — 'cp .env.example .env' (varsayılanlarla çalışır, sırlar üretilir)")
        return env
    for line in f.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#") or "=" not in s:
            continue
        k, v = s.split("=", 1)
        env[k.strip()] = v.strip().strip('"').strip("'")
    ok(f".env okundu ({len(env)} anahtar)")

    mc_version = env.get("MC_VERSION", "")
    if not mc_version:
        warn("MC_VERSION boş — sürüm sabitlenmedi (devirde sürüm atlarsa dünya bozulabilir)")
    elif mc_version.upper() == "LATEST":
        warn("MC_VERSION=LATEST — devir döngüsünde sürüm atlarsa dünya bozulur; sabitlemek önerilir")
    else:
        ok(f"MC_VERSION sabit: {mc_version}")

    supabase_url = env.get("SUPABASE_URL", "")
    if supabase_url and not supabase_url.startswith("https://"):
        bad(f"SUPABASE_URL https:// ile başlamalı: {supabase_url}")
    if env.get("SUPABASE_SERVICE_KEY") or env.get("SUPABASE_SERVICE_ROLE"):
        bad("SUPABASE_SERVICE_KEY .env'de — bu anahtar İSTEMCİDE/REPODA ASLA durmamalı. Sadece anon kullan.")
    return env


def check_gitignore() -> None:
    section("2/7 sır hijyeni")
    gi = ROOT / ".gitignore"
    if not gi.exists():
        bad(".gitignore yok — .env ve runtime/ repoya girebilir")
        return
    text = gi.read_text(encoding="utf-8")
    required = [".env", "runtime/", "artifacts/", "logs/", "node_modules/"]
    miss = [r for r in required if r not in text]
    if miss:
        bad(f".gitignore eksik: {', '.join(miss)}")
    else:
        ok(".gitignore: .env, runtime/, artifacts/, logs/ hariç")

    # gerçekten takip ediliyor mu?
    try:
        tracked = subprocess.run(
            ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, timeout=20
        ).stdout.splitlines()
        leaks = [t for t in tracked if t.endswith(".env") or t.startswith("runtime/") or t.startswith("logs/")]
        if leaks:
            bad(f"repoda takip edilen hassas dosya: {', '.join(leaks[:5])}")
        else:
            ok("git: takip edilen .env/runtime/logs yok")
    except Exception as e:  # noqa: BLE001
        warn(f"git kontrolü yapılamadı: {e}")


def check_compose() -> None:
    section("3/7 docker-compose")
    p = ROOT / "docker-compose.hub.yml"
    if not p.exists():
        bad("docker-compose.hub.yml yok")
        return
    try:
        import yaml
    except ImportError:
        warn("PyYAML yok — derin compose kontrolü atlandı (pip install pyyaml)")
        return
    try:
        d = yaml.safe_load(p.read_text(encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        bad(f"compose YAML bozuk: {e}")
        return
    svc = d.get("services", {}) or {}
    need = {"velocity", *ROOMS}
    missing = need - set(svc)
    if missing:
        bad(f"compose: eksik servis {sorted(missing)}")
        return
    ok(f"compose: {len(svc)} servis, 6 oda + proxy")

    # sadece proxy dışa açık
    for name, s in svc.items():
        for port in s.get("ports", []):
            ps = str(port)
            if name == "velocity":
                continue
            if ps.startswith("127.0.0.1"):
                continue
            if "19132" in ps:
                continue  # Geyser UDP
            bad(f"{name}: dışarıya açık port {ps} — odalar sadece proxy'den erişilmeli")
    ok("portlar: sadece velocity (25565) + Geyser (19132/udp) dışa açık")

    env = svc.get("survival", {}).get("environment", {}) or {}
    if str(env.get("EULA", "")).upper() != "TRUE":
        bad("compose: EULA TRUE değil")
    if str(env.get("TYPE", "")).upper() != "PAPER":
        bad(f"compose: TYPE={env.get('TYPE')} (PAPER bekleniyor)")
    if "VELOCITY_SECRET" not in env:
        bad("compose: VELOCITY_SECRET env'de yok — proxy forwarding çalışmaz")
    if str(env.get("ONLINE_MODE", "")).upper() != "FALSE":
        warn("compose: ONLINE_MODE FALSE değil — cracked sunucu için FALSE olmalı")
    ok("compose: EULA/PAPER/VELOCITY_SECRET/ONLINE_MODE tutarlı")


def check_rendered() -> None:
    section("4/7 üretilen config (runtime/)")
    vel = ROOT / "runtime" / "velocity" / "velocity.toml"
    if not vel.exists():
        bad("runtime/velocity/velocity.toml yok — 'python3 scripts/render.py' çalıştır")
    else:
        text = vel.read_text(encoding="utf-8")
        if 'player-info-forwarding-mode = "modern"' not in text:
            bad("velocity.toml: modern forwarding kapalı — odalar oyuncuyu tanımaz")
        for r in ROOMS:
            if f"{r} = \"127.0.0.1:{PORTS[r]}\"" not in text:
                bad(f"velocity.toml: '{r}' sunucusu kayıtlı değil")
        if 'try = ["lobby"]' not in text:
            bad("velocity.toml: try listesinde lobby yok")
        ok("velocity.toml: modern forwarding + 6 oda kayıtlı")

    rcon_ports = []
    for r in ROOMS:
        props = ROOT / "runtime" / "rooms" / r / "server.properties"
        if not props.exists():
            bad(f"runtime/rooms/{r}/server.properties yok")
            continue
        t = props.read_text(encoding="utf-8")
        m = re.search(r"^server-port=(\d+)$", t, re.M)
        if not m:
            bad(f"{r}: server-port yok")
        elif int(m.group(1)) != PORTS[r]:
            bad(f"{r}: server-port {m.group(1)} ≠ beklenen {PORTS[r]}")
        if "online-mode=false" not in t:
            warn(f"{r}: online-mode=false değil (cracked sunucu)")
        if "rcon.password=" in t:
            pw = t.split("rcon.password=", 1)[1].splitlines()[0]
            if len(pw) < 8:
                bad(f"{r}: rcon şifresi çok kısa")
        jvm = ROOT / "runtime" / "rooms" / r / "jvm.args"
        if jvm.exists():
            jt = jvm.read_text()
            if "SoftMaxHeapSize" not in jt:
                bad(f"{r}: jvm.args'ta SoftMaxHeapSize yok — bellek havuzu çalışmaz")
            ok(f"{r}: config + jvm ok")
        m2 = re.search(r"^rcon.port=(\d+)$", t, re.M)
        if m2:
            rcon_ports.append(m2.group(1))
    if len(rcon_ports) != len(set(rcon_ports)):
        bad(f"RCON portları çakışıyor: {rcon_ports}")
    else:
        ok(f"RCON portları ayrık: {', '.join(sorted(set(rcon_ports)))}")


def check_plugins() -> None:
    section("5/7 pluginler")
    manifest_p = ROOT / "plugins" / "manifest.json"
    if not manifest_p.exists():
        bad("plugins/manifest.json yok")
        return
    try:
        manifest = json.loads(manifest_p.read_text(encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        bad(f"manifest bozuk: {e}")
        return
    slugs = [p["slug"] for p in manifest["plugins"]]
    dup = {s for s in slugs if slugs.count(s) > 1}
    if dup:
        bad(f"manifest: tekrarlanan slug {dup}")
    mandatory = [p for p in manifest["plugins"] if p.get("mandatory")]
    ok(f"manifest: {len(slugs)} plugin, {len(mandatory)} zorunlu")

    pdir = ROOT / "runtime" / "plugins"
    if not pdir.exists() or not any(pdir.glob("*.jar")):
        msg = "runtime/plugins boş — 'node scripts/plugins.mjs download'"
        if CI:
            warn(msg + " (CI: beklenen, jar indirilmez)")
        else:
            warn(msg)
    else:
        jars = sorted(pdir.glob("*.jar"))
        broken = []
        for j in jars:
            try:
                with zipfile.ZipFile(j) as zf:
                    if zf.testzip() is not None:
                        broken.append(j.name)
                    elif not any(n in zf.namelist() for n in ("plugin.yml", "paper-plugin.yml", "velocity-plugin.json")):
                        broken.append(j.name + " (plugin.yml yok)")
            except zipfile.BadZipFile:
                broken.append(j.name + " (jar değil)")
        if broken:
            bad(f"bozuk plugin: {', '.join(broken)}")
        else:
            ok(f"{len(jars)} plugin jar sağlam")

    # cracked sunucu: proxy'de bir auth plugin ŞART (AuthMe VEYA LibreLogin)
    auth = [p["slug"] for p in manifest["plugins"]
            if p["slug"] in ("authmereloaded", "librelogin")]
    if not auth:
        bad("manifest: auth plugin yok — cracked sunucuda isim taklidi olur")
    else:
        ok(f"auth plugin manifestte: {', '.join(auth)}")


def check_plugin_configs() -> None:
    section("6/7 Türkçe plugin ayarları")
    checks = [
        ("config/plugins/LibreLogin/config.yml", "kimlik doğrulama (proxy)"),
        ("config/plugins/LuckPerms/groups.yml", "yetki grupları"),
        ("config/plugins/Geyser-Velocity/config.yml", "Bedrock köprüsü (proxy)"),
        ("config/plugins/TAB/config.yml", "tab listesi"),
    ]
    for rel, what in checks:
        p = ROOT / rel
        if not p.exists():
            bad(f"eksik: {rel} ({what})")
        else:
            ok(f"{rel}")
    # tag tutarlılığı: prefix hem LuckPerms'te hem chat formatında olmalı
    echat = ROOT / "config/plugins/EssentialsXChat/config.yml"
    groups = ROOT / "config/plugins/LuckPerms/groups.yml"
    tab = ROOT / "config/plugins/TAB/config.yml"
    if not echat.exists():
        bad("eksik: config/plugins/EssentialsXChat/config.yml (chatte tag görünmez)")
    if groups.exists() and echat.exists() and tab.exists():
        g = groups.read_text(encoding="utf-8")
        c = echat.read_text(encoding="utf-8")
        t = tab.read_text(encoding="utf-8")
        for grp in ("kurucu", "admin", "mod", "vip", "oyuncu", "bot"):
            if f"{grp}:" not in g:
                bad(f"LuckPerms: '{grp}' grubu yok")
            if f"{grp}:" not in c:
                warn(f"EssentialsXChat: '{grp}' için chat formatı yok")
        if "prefix:" not in g:
            bad("LuckPerms: gruplarda 'prefix' yok — tag kafada/chatte görünmez")
        if "%luckperms_prefix%" not in t:
            bad("TAB: '%luckperms_prefix%' kullanılmıyor — kafada tag görünmez")
        if "{DISPLAYNAME}" not in c:
            bad("EssentialsXChat: {DISPLAYNAME} yok — chatte isim görünmez")
        ok("tag tutarlılığı: LuckPerms prefix → TAB (kafa) + Chat (chat)")


    gp = ROOT / "config/plugins/LuckPerms/groups.yml"
    if gp.exists():
        t = gp.read_text(encoding="utf-8")
        for g in ("oyuncu", "mod", "kurucu", "bot"):
            if g not in t:
                bad(f"LuckPerms: '{g}' grubu yok")
        if "grimac.bypass" not in t:
            warn("LuckPerms: bot grubunda grimac.bypass yok — bot uçarken kicklenebilir")
        # op izni SADECE kurucu'da olabilir (başkasına verilirse yetki sızar)
        for grp_name in ("oyuncu", "vip", "mod", "admin", "bot"):
            blk = re.search(rf"^  {grp_name}:(.*?)(?=^  \w|\Z)", t, re.M | re.S)
            # pozitif izin: "- 'bukkit.command.op'" (negatif "-'-bukkit..." sayılmaz)
            if blk and re.search(r"^\s*-\s*'bukkit\.command\.op'", blk.group(1), re.M):
                bad(f"LuckPerms: '{grp_name}' grubuna op verilmiş (sadece kurucu alabilir)")
        ok("LuckPerms: gruplar + bot izinleri kontrol edildi")


def check_tag_render() -> None:
    """Tag sisteminin üç katmanı runtime'a gerçekten kopyalandı mı?"""
    section("6b/7 tag sistemi render")
    need = {
        "runtime/plugins/LuckPerms/groups.yml": "prefix tanımları",
        "runtime/plugins/TAB/config.yml": "kafa formatı",
        "runtime/plugins/EssentialsXChat/config.yml": "chat formatı",
    }
    for rel, what in need.items():
        p = ROOT / rel
        if not p.exists():
            bad(f"render edilmemiş: {rel} ({what}) — 'python3 scripts/render.py'")
        else:
            ok(f"render ok: {rel}")
    rp = ROOT / "runtime/plugins/LuckPerms/groups.yml"
    if rp.exists():
        t = rp.read_text(encoding="utf-8")
        if "prefix:" not in t:
            bad("runtime LuckPerms: prefix yok — tag görünmez")
        for g in ("kurucu:", "admin:", "mod:", "vip:", "oyuncu:", "bot:"):
            if g not in t:
                bad(f"runtime LuckPerms: '{g}' grubu eksik")
        ok("runtime LuckPerms: 6 grup + prefix")


def check_versions() -> None:
    """Sürüm ↔ Java eşleşmesi + Bedrock mimarisi (Geyser proxy'de olmalı)."""
    section("4b/7 sürüm + Bedrock mimarisi")
    envf = ROOT / ".env"
    envtxt = (envf.read_text() if envf.exists() else (ROOT / ".env.example").read_text())
    mc = re.search(r"^MC_VERSION=(.+)$", envtxt, re.M)
    img = re.search(r"^MC_IMAGE=(.+)$", envtxt, re.M)
    ver = (mc.group(1).strip() if mc else "26.3")
    image = (img.group(1).strip() if img else "itzg/minecraft-server:java25")

    # 26.x → java25, 1.21.x → java21
    major = ver.split(".")[0]
    need = "java25" if major.isdigit() and int(major) >= 26 else "java21"
    if need in image:
        ok(f"sürüm {ver} ↔ {image} (uyumlu)")
    else:
        bad(f"sürüm {ver} için {need} gerekir, imaj '{image}' — sunucu açılmaz")

    # manifest oyun sürümü ile .env aynı mı
    man = json.loads((ROOT / "plugins/manifest.json").read_text(encoding="utf-8"))
    if man.get("game_version") == ver:
        ok(f"manifest game_version = {ver}")
    else:
        warn(f"manifest {man.get('game_version')} ≠ .env {ver} — plugin çözümü kayabilir")

    # Geyser proxy'de mi (backend'lerde değil)
    d = man
    gy = [p for p in d["plugins"] if p["slug"] == "geyser"]
    if gy and gy[0].get("rooms") == ["proxy"]:
        ok("Geyser proxy'de (doğru mimari)")
    elif gy:
        bad("Geyser backend'lere kurulu — proxy'ye taşınmalı (Bedrock girişi bozulur)")
    if gy and gy[0].get("platform") == "velocity":
        ok("Geyser platformu velocity")
    else:
        bad("Geyser platformu velocity olmalı (jar adı ve config farklı)")

    # compose: 19132 proxy'de mi
    comp = (ROOT / "docker-compose.hub.yml").read_text(encoding="utf-8")
    vel = comp.split("velocity:", 1)[-1].split("\n  # ---", 1)[0]
    if "19132" in vel and "19132/udp" in vel:
        ok("19132/tcp+udp proxy'de açık")
    else:
        bad("19132 proxy'de açık değil — Bedrock oyuncu giremez")

    # proxy plugin dizini mount edilmiş mi
    if "runtime/proxy-plugins:/server/plugins" in vel:
        ok("proxy plugin dizini tek yerden mount edilmiş (/server/plugins)")
    else:
        bad("proxy'ye plugin mount'u yok — /hub /lobby ve Geyser çalışmaz")

    # /hub /lobby → çekirdek lobi odası
    hub = ROOT / "config/plugins/velocity-hub-command/config.yml"
    vtoml = ROOT / "config/velocity/velocity.toml.tpl"
    if not hub.exists():
        bad("eksik: config/plugins/velocity-hub-command/config.yml (/hub /lobby)")
    else:
        t = hub.read_text(encoding="utf-8")
        target = re.search(r"^hubServerName:\s*(\S+)", t, re.M)
        if not target:
            bad("hub komutu: hubServerName tanımsız")
        else:
            dest = target.group(1).strip().strip('"').strip("'")
            # hedef velocity.toml'daki [servers] anahtarı olmalı
            vt = vtoml.read_text(encoding="utf-8") if vtoml.exists() else ""
            if f'__SERVERS_BLOCK__' in vt or re.search(rf"^\s*{re.escape(dest)}\s*=", vt, re.M):
                ok(f"/hub /lobby → '{dest}' (velocity.toml'da tanımlı)")
            else:
                bad(f"hub hedefi '{dest}' velocity.toml [servers] içinde yok")
            # /lobby alias açık mı
            if re.search(r"^useLobbyAlias:\s*true", t, re.M):
                ok("/lobby alias aktif")
            else:
                bad("/lobby alias kapalı — oyuncu /lobby yazamaz")
    # /lobi /l gibi Türkçe kısayollar: Essentials aliases her odada olmalı
    missing = []
    for d in sorted((ROOT / "config/rooms").iterdir()):
        if not d.is_dir() or d.name.startswith("_") or d.name == "auth":
            continue   # auth/limbo'da /hub olmaz (oyuncu daha giriş yapmadı)
        f = ROOT / "runtime/rooms" / d.name / "commands.yml"
        if not f.exists():
            missing.append(d.name)
            continue
        t = f.read_text(encoding="utf-8")
        for alias in ("lobi:", "l:", "merkez:"):
            if alias not in t:
                missing.append(f"{d.name}/{alias}")
    if missing:
        bad(f"komut kısayolu eksik: {', '.join(missing[:4])} — 'python3 scripts/render.py'")
    else:
        ok("/lobi /l /merkez → /hub (6 odada tanımlı)")

    # jar'lar proxy dizinine mi gidiyor
    man = json.loads((ROOT / "plugins/manifest.json").read_text(encoding="utf-8"))
    hubp = [x for x in man["plugins"] if x["slug"] == "velocity-hub-command"]
    if hubp and hubp[0].get("target") == "proxy-plugin" and hubp[0].get("rooms") == ["proxy"]:
        ok("/hub jar'ı proxy'ye gidiyor (tüm odalarda geçerli)")
    else:
        bad("/hub plugin'i proxy-plugin + rooms=[proxy] olmalı")

    # Geyser config: velocity için floodgate auth + velocity remote
    gv = ROOT / "config/plugins/Geyser-Velocity/config.yml"
    if gv.exists():
        t = gv.read_text(encoding="utf-8")
        if "auth-type: floodgate" in t:
            ok("Geyser: auth-type floodgate")
        else:
            bad("Geyser: auth-type floodgate değil — Bedrock oyuncu şifre ister")
        if "address: velocity" in t:
            ok("Geyser remote → velocity servisi")
        else:
            bad("Geyser remote adresi velocity olmalı (compose iç ağı)")


def check_auth() -> None:
    """Kimlik doğrulama: proxy'de tek giriş, oda geçişinde şifre YOK."""
    section("4c/7 kimlik doğrulama (auth)")
    man = json.loads((ROOT / "plugins/manifest.json").read_text(encoding="utf-8"))
    slugs = {p["slug"]: p for p in man["plugins"]}

    # AuthMe kalmamalı: oda başına ayrı DB = her geçişte şifre
    if "authmereloaded" in slugs:
        bad("AuthMe manifestte — oda başına oturum demek, kaldırılmalı")
    else:
        ok("AuthMe kaldırıldı (eski mimari)")

    ll = slugs.get("librelogin")
    if not ll:
        bad("LibreLogin manifestte yok — kimlik doğrulama çalışmaz")
    elif ll.get("target") != "proxy-plugin" or ll.get("rooms") != ["proxy"]:
        bad("LibreLogin proxy plugin olmalı (backend'de ikinci auth hattı olur)")
    else:
        ok("LibreLogin proxy'de (tek giriş, tüm odalarda oturum geçerli)")

    cfg = ROOT / "config/plugins/LibreLogin/config.yml"
    if not cfg.exists():
        bad("eksik: config/plugins/LibreLogin/config.yml")
        return
    t = cfg.read_text(encoding="utf-8")

    # limbo + lobby hedefleri gerçek oda olmalı
    rooms = {d.name for d in (ROOT / "config/rooms").iterdir()
             if d.is_dir() and not d.name.startswith("_")}
    m = re.search(r"^limbo:\s*\n((?:\s+-\s*\S+\n)+)", t, re.M)
    limbo = re.findall(r"-\s*(\S+)", m.group(1)) if m else []
    if not limbo:
        bad("LibreLogin: limbo sunucusu tanımsız — oyuncu nerede bekleyecek?")
    for l in limbo:
        if l not in rooms:
            bad(f"LibreLogin limbo '{l}' gerçek oda değil")
        elif l not in (ROOT / "runtime/velocity/velocity.toml").read_text(encoding="utf-8"):
            bad(f"limbo '{l}' velocity.toml [servers] içinde yok")
        else:
            ok(f"limbo '{l}' tanımlı")
    lob = re.search(r"^lobby:\s*\n\s+root:\s*\n((?:\s+-\s*\S+\n)+)", t, re.M)
    lobbies = re.findall(r"-\s*(\S+)", lob.group(1)) if lob else []
    if lobbies and lobbies[0] == "lobby":
        ok("giriş sonrası hedef: lobby (çekirdek hub)")
    else:
        bad(f"LibreLogin lobby hedefi 'lobby' olmalı, bulunan: {lobbies or 'YOK'}")

    # oturum: her girişte şifre sormasın
    st = re.search(r"^session-timeout:\s*(\d+)", t, re.M)
    if not st:
        warn("session-timeout yok — varsayılan 12 saat")
    elif int(st.group(1)) == 0:
        bad("session-timeout=0 → HER GİRİŞTE şifre sorar (istenmiyor)")
    else:
        ok(f"oturum {int(st.group(1)) // 86400} gün (girişte tekrar şifre yok)")

    # Bedrock şifresiz
    if re.search(r"^floodgate:\s*\n\s+enabled:\s*true", t, re.M):
        ok("Floodgate: Bedrock oyuncu şifresiz girer")
    else:
        warn("floodgate.enabled kapalı — Bedrock oyuncu şifre girmek zorunda")

    # ---- yetki: admin/op ----
    rg = ROOT / "runtime/plugins/LuckPerms/require-groups.yml"
    envf = ROOT / ".env" if (ROOT / ".env").exists() else (ROOT / ".env.example")
    envtxt = envf.read_text(encoding="utf-8")
    adm = re.search(r"^ADMIN_PLAYERS=(.*)$", envtxt, re.M)
    adm_val = (adm.group(1).strip() if adm else "")
    if rg.exists():
        rgt = rg.read_text(encoding="utf-8")
        players = re.findall(r"^  (\.?\S+):$", rgt, re.M)
        if players:
            ok(f"otomatik yetki: {', '.join(players)} (ilk açılışta uygulanır)")
            for pl in players:
                if not pl.startswith(".") and pl in envtxt:
                    warn(f"'{pl}' Bedrock'ta giriş yapacaksan '.{pl}' yaz (Floodgate eki)")
        elif adm_val:
            bad("ADMIN_PLAYERS tanımlı ama require-groups boş — 'python3 scripts/render.py'")
        else:
            warn("ADMIN_PLAYERS boş — kimseye otomatik yetki verilmiyor")
    elif adm_val:
        bad("ADMIN_PLAYERS tanımlı ama require-groups üretilmemiş")

    # kurucu op verebilmeli (admin yasağını ezmeli)
    g = ROOT / "config/plugins/LuckPerms/groups.yml"
    if g.exists():
        gt = g.read_text(encoding="utf-8")
        kur = re.search(r"^  kurucu:(.*?)(?=^  \w)", gt, re.M | re.S)
        ad = re.search(r"^  admin:(.*?)(?=^  \w)", gt, re.M | re.S)
        if kur and "bukkit.command.op" in kur.group(1):
            ok("kurucu op verebilir (admin yasağı ezildi)")
        else:
            bad("kurucu 'bukkit.command.op' izni yok — op veremez")
        if ad and "-bukkit.command.op" in ad.group(1):
            ok("admin op veremez (bilinçli)")
        else:
            warn("admin'de op yasağı yok")

    # limbo odası proxy dışından erişilemez olmalı
    comp = (ROOT / "docker-compose.hub.yml").read_text(encoding="utf-8")
    if "hub-auth" in comp:
        ok("auth (limbo) konteyneri var")
    else:
        bad("auth (limbo) servisi compose'da yok")
    for line in comp.splitlines():
        if "25572" in line and "ports" not in line:
            if "127.0.0.1" not in line:
                bad(f"limbo portu dışarıya açık: {line.strip()}")
                break
    else:
        ok("limbo sadece 127.0.0.1'de (dışarıdan erişilemez)")



# ---------------------------------------------------------------------------
#  EssentialsX resmi izin listesi (essinfo.xeya.me — 2026-11 doğrulandı)
#  Sadece burada olanlar geçerli; yazım hatası = oyuncu komutu kullanamaz.
# ---------------------------------------------------------------------------
ESS_PERMS = {
    # ev
    "home", "home.bed", "home.others", "sethome", "sethome.bed", "sethome.multiple",
    "sethome.others", "delhome", "delhome.others", "renamehome",
    # ışınlanma
    "tpa", "tpahere", "tpaall", "tpaccept", "tpdeny", "tpacancel", "tptoggle",
    "tptoggle.others", "tp", "tp.others", "tp.position", "tphere", "tpo", "tpohere",
    "tppos", "tpoffline", "tpauto", "back", "back.ondeath", "back.onteleport",
    "spawn", "setspawn", "warp", "warp.list", "warp.others", "warp.otherplayers", "setwarp",
    "delwarp", "warpinfo", "tpr", "settpr", "getpos", "getpos.others",
    "teleport.timer.bypass", "teleport.timer.move", "teleport.cooldown.bypass",
    # iletişim
    "msg", "msg.color", "msg.url", "msg.multiple", "reply", "mail", "mail.send",
    "mail.sendall", "mail.clearall", "helpop", "helpop.receive", "help", "list",
    "list.hidden", "me", "seen", "seen.ip", "seen.banreason", "seen.uuid",
    "seen.location", "realname", "ping", "ignore", "ignoreexempt", "socialspy",
    "socialspy.others", "msgtoggle", "broadcast", "whois", "near",
    # ekonomi
    "balance", "balance.others", "balancetop", "balancetop.force", "pay",
    "pay.multiple", "pay.offline", "eco", "worth", "sell", "sell.bulk", "sell.hand",
    # oyuncu komutları
    "afk", "rules", "motd", "suicide", "kit", "kits", "showkit", "back", "hat",
    "workbench", "anvil", "grindstone", "stonecutter", "cartographytable", "loom",
    "smithingtable", "enderchest", "enderchest.others", "enderchest.modify",
    "repair", "repair.all", "repair.armor", "repair.enchanted",
    "nick", "nick.others", "nick.rgb", "skull", "item", "itemname", "itemlore",
    "itemdb", "condense", "disposal", "compass", "depth", "top", "bottom", "jump",
    "recipe", "book", "customtext", "kill", "clearinventory", "clearinventory.others",
    "feed", "heal", "heal.others", "rest", "ext", "exp", "unlimited",
    "speed", "speed.fly", "speed.walk", "speed.others", "god", "god.others",
    "gamemode", "gamemode.all", "gamemode.others", "give", "give.item-all",
    "fly", "fly.safelogin", "fly.others", "tpr", "world",
    # moderasyon
    "kick", "kick.exempt", "kick.notify", "kickall", "kickall.exempt",
    "mute", "mute.exempt", "mute.unlimited", "mute.offline",
    "ban", "ban.exempt", "ban.offline", "banip", "unban", "unbanip",
    "tempban", "tempban.exempt", "tempban.offline", "tempban.unlimited", "tempbanip",
    "jail", "jail.allow-break", "jail.allow-place", "jail.allow-interact",
    "jail.exempt", "togglejail", "togglejail.offline", "unjail", "jails", "setjail",
    "vanish", "vanish.see", "vanish.others", "vanish.pvp",
    "invsee", "invsee.modify", "invsee.preventmodify", "invsee.equip",
    "sudo", "sudo.exempt", "time", "time.set", "time.world.all", "weather",
    "gamemode", "gc", "stop", "reloadall", "reload", "essentials", "version",
    "updatecheck", "backup", "spawner", "spawnmob", "lightning", "thunder",
    "burn", "turn", "nuke", "antioch", "kittycannon", "beezooka", "fireball",
    "firework", "ice", "tree", "bigtree", "break", "break.bedrock", "remove",
    "powertool", "powertool.append", "powertooltoggle", "sethome", "setworth",
    "createkit", "delkit", "kitreset", "kitreset.others", "kit.others",
    "kit.exemptdelay", "editsign", "editsign.color", "editsign.rgb",
    "signs.color", "signs.format", "signs.magic", "signs.rgb",
    "signs.use.protection", "signs.create.protection", "signs.break.protection",
    "signs.protection.override", "signs.trade.override", "silentjoin",
    "silentjoin.vanish", "silentquit", "sleepingignored", "keepxp",
    "joinfullserver", "commandcooldowns.bypass", "nocommandcost.all",
    "ptime", "ptime.others", "pweather", "pweather.others", "tpaall", "tpall",
    "chat.spy", "chat.spy.exempt", "chat.ignoreexempt",
}



def yaml_load(path):
    """YAML oku — PyYAML varsa tam, yoksa basit çözümleme."""
    try:
        import yaml  # type: ignore
        return yaml.safe_load(path.read_text(encoding="utf-8"))
    except ImportError:
        return {"groups": {}}


def check_permissions() -> None:
    """LuckPerms izinleri: yazım doğru mu, her rol işini yapabiliyor mu?"""
    section("4e/7 yetkiler (LuckPerms)")
    f = ROOT / "config/plugins/LuckPerms/groups.yml"
    if not f.exists():
        bad("config/plugins/LuckPerms/groups.yml yok")
        return
    g = yaml_load(f) or {}
    groups = g.get("groups") or {}
    if not groups:
        bad("LuckPerms: grup tanımı yok")
        return

    # 1) yazım: essentials.<x> gerçekten var mı
    typos = []
    for name, body in groups.items():
        for perm in (body.get("permissions") or []):
            if perm.startswith("essentials.") and not perm.endswith("*"):
                key = perm[len("essentials."):]
                # bunlar DİNAMİK: adı config'ten gelir (kit/ev/warp adı)
                if (key in ESS_PERMS
                        or key.startswith(("kits.", "sethome.multiple.", "warp."))):
                    continue
                if True:
                    typos.append(f"{name}: {perm}")
    if typos:
        for t in typos[:8]:
            bad(f"geçersiz izin: {t}")
        if len(typos) > 8:
            bad(f"... ve {len(typos)-8} tane daha")
    else:
        n = sum(len(b.get("permissions") or []) for b in groups.values())
        ok(f"{n} izin: essentials adları doğrulandı (resmi liste)")

    # 2) oyuncu temel işleri yapabiliyor mu
    oy = set(groups.get("oyuncu", {}).get("permissions") or [])
    temel = {
        "ev kurma": "essentials.sethome",
        "eve gitme": "essentials.home",
        "ışınlanma": "essentials.tpa",
        "spawn": "essentials.spawn",
        "sohbet": "essentials.msg",
        "para": "essentials.balance",
        "kit": "essentials.kit",
        "arsa": "griefprevention.claims",
        "bedwars": "bw.cmd.join",
        "hub": "essentials.spawn",
    }
    eksik = [ad for ad, perm in temel.items() if perm not in oy]
    if eksik:
        bad(f"oyuncu şunları yapamaz: {', '.join(eksik)}")
    else:
        ok(f"oyuncu temel komutları kullanabilir ({len(oy)} izin)")

    # 3) tehlikeli izinler oyuncuda olmamalı
    tehlikeli = {
        "essentials.gamemode": "oyun modu değiştirme",
        "essentials.give": "item verme",
        "essentials.fly": "uçuş",
        "essentials.tp": "başkasına ışınlanma",
        "essentials.god": "ölümsüzlük",
        "coreprotect.rollback": "dünya geri alma",
        "*": "tüm izinler",
    }
    for perm, ne in tehlikeli.items():
        if perm in oy:
            bad(f"oyuncu '{perm}' ({ne}) alabiliyor — dengesizlik")
    if not any(p in oy for p in tehlikeli):
        ok("oyuncuda tehlikeli izin yok (gamemode/give/fly/tp/rollback)")

    # 4) tag taklidi: renk izni sadece vip ve üstünde olmalı, oyuncuda değil
    if "essentials.chat.color" in oy:
        bad("oyuncuda sohbet rengi var — tag taklidi mümkün")
    else:
        ok("oyuncu sohbet rengini kullanamaz (tag taklidi kapalı)")

    # 5) hiyerarşi: parent zinciri doğru mu
    beklenen = {"vip": "oyuncu", "mod": "vip", "admin": "mod", "kurucu": "admin"}
    for grp, par in beklenen.items():
        gs = groups.get(grp, {})
        parents = gs.get("parents") or []
        if par in parents:
            continue
        bad(f"hiyerarşi: {grp} → {par} bağı yok")
    else:
        ok("hiyerarşi: oyuncu → vip → mod → admin → kurucu")

    # 6) her grubun prefix'i var mı (tag görünürlüğü)
    for name in ("vip", "mod", "admin", "kurucu", "bot"):
        if not groups.get(name, {}).get("prefix"):
            warn(f"{name} grubunda prefix yok — kafada tag görünmez")


def check_memory() -> None:
    """Bellek bütçesi: toplam limit 16 GB runner'a sığmalı + heap dışı pay olmalı."""
    section("4d/7 bellek bütçesi")
    import re as _re
    rooms = [d for d in (ROOT / "config/rooms").iterdir()
             if d.is_dir() and not d.name.startswith("_")]

    def to_mb(v: str) -> int:
        v = v.strip().upper()
        return int(float(v[:-1]) * 1024) if v.endswith("G") else int(float(v.rstrip("M")))

    total = 0
    problems = []
    for d in sorted(rooms):
        y = (d / "room.yml").read_text(encoding="utf-8")
        xmx = _re.search(r"^xmx:\s*(\S+)", y, _re.M)
        soft = _re.search(r"^softmax:\s*(\S+)", y, _re.M)
        if not xmx:
            problems.append(f"{d.name}: xmx yok")
            continue
        heap = to_mb(xmx.group(1))
        limit = heap + max(320, int(heap * 0.22))
        total += limit
        if soft and to_mb(soft.group(1)) > heap:
            problems.append(f"{d.name}: softmax > xmx")
        if heap < 512:
            problems.append(f"{d.name}: xmx {heap}M çok düşük (Paper açılmaz)")
    if problems:
        for p_ in problems:
            bad(p_)
    else:
        ok(f"7 oda: xmx/softmax tutarlı")

    # runner 16 GB; OS + docker + governor payı ~1.5 GB
    budget = 16_384 - 1_500 - 384   # runner - OS/docker - proxy heap
    if total <= budget:
        ok(f"toplam {total/1024:.1f} GB ≤ {budget/1024:.1f} GB bütçesi (16 GB runner'a sığar)")
    else:
        bad(f"toplam {total/1024:.1f} GB > {budget/1024:.1f} GB bütçesi — OOM riski")

    # compose MEMORY env'den mi geliyor (sabit yazılmamalı)
    comp = (ROOT / "docker-compose.hub.yml").read_text(encoding="utf-8")
    # velocity proxy'nin sabit 384M'i normal (oda değil); odalarda sabit olmamalı
    fixed = _re.findall(r'container_name: hub-(?!velocity)([a-z]+)\n(?:.*\n)*?\s+MEMORY: "([0-9]+[GM])"', comp)
    if fixed:
        bad(f"compose'da sabit MEMORY: {[f[0] for f in fixed]} — Xmx değişince kayar")
    else:
        ok("compose MEMORY env'den (tek kaynak room.yml)")

    # rooms.env üretilmiş mi
    re_ = ROOT / "runtime/rooms.env"
    if re_.exists():
        ok(f"runtime/rooms.env üretilmiş ({len(re_.read_text().strip().splitlines())-2} oda)")
    else:
        warn("runtime/rooms.env yok — 'python3 scripts/render.py'")


def check_gradient() -> None:
    """Gradyan blokları güncel mi — elle renk yazılmışsa yakala."""
    section("6c/7 gradyan (renk geçişleri)")
    import subprocess
    r = subprocess.run([sys.executable, str(ROOT / "scripts/gradient.py"), "--check"],
                       capture_output=True, text=True)
    if r.returncode == 0:
        ok("gradyanlar güncel (LuckPerms + TAB + Chat + Essentials + MOTD)")
    else:
        bad("gradyan blokları eski — 'python3 scripts/gradient.py --apply'")
        for ln in (r.stdout or "").strip().splitlines():
            if "✗" in ln:
                print("      " + ln.strip())
    # elle yazılmış hex var mı: GRADIENT bloğu dışında &#
    import re as _re
    block = _re.compile(r"# >>> GRADIENT:BEGIN.*?# <<< GRADIENT:END [a-z_.]+", _re.S)
    for rel in ("config/plugins/LuckPerms/groups.yml",
                "config/plugins/TAB/config.yml",
                "config/plugins/EssentialsXChat/config.yml"):
        f = ROOT / rel
        if not f.exists():
            continue
        outside = block.sub("", f.read_text(encoding="utf-8"))
        outside = "\n".join(ln for ln in outside.splitlines() if not ln.lstrip().startswith("#"))
        if "&#" in outside:
            bad(f"{rel}: GRADIENT bloğu dışında elle hex var — jeneratörü kullan")


def check_tools() -> None:
    section("7/7 araçlar")
    for rel in ("scripts/render.py", "scripts/plugins.mjs", "scripts/mem-governor.sh",
                "scripts/world-sync.sh", "tools/craftya-bot/src/scan.js"):
        if not (ROOT / rel).exists():
            bad(f"eksik dosya: {rel}")
    ok("scriptler yerinde")
    if not (ROOT / ".github/workflows/hub.yml").exists():
        bad(".github/workflows/hub.yml yok — 6 saatlik devir çalışmaz")
    else:
        ok("hub.yml var (devir döngüsü)")
    if not (ROOT / "tools/craftya-bot/package.json").exists():
        bad("craftya-bot package.json yok")
    else:
        ok("craftya-bot yerinde")


def main() -> int:
    print("Craftya — yapılandırma doktoru")
    check_env()
    check_gitignore()
    check_compose()
    check_rendered()
    check_plugins()
    check_plugin_configs()
    check_versions()
    check_auth()
    check_memory()
    check_permissions()
    check_tag_render()
    check_gradient()
    check_tools()

    print("\n" + "─" * 62)
    if problems:
        print(f"\033[31mHAZIR DEĞİL\033[0m — {len(problems)} hata, {len(warnings)} uyarı\n")
        for p in problems:
            print(f"  ✗ {p}")
        print("\nDüzelt, sonra tekrar: python3 scripts/doctor.py")
        return 1
    print(f"\033[32mHAZIR\033[0m — 0 hata, {len(warnings)} uyarı")
    print("\nBaşlat:   ./scripts/mc.sh up")
    print("İzle:     ./scripts/mc.sh logs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
