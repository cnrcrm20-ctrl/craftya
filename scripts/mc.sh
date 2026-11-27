#!/usr/bin/env bash
# ============================================================================
#  Craftya — tek komutluk yönetim
# ============================================================================
#  ./scripts/mc.sh up          başlat (config + plugin yoksa hazırlar)
#  ./scripts/mc.sh down        durdur (dünyalar korunur)
#  ./scripts/mc.sh restart     yeniden başlat
#  ./scripts/mc.sh logs [oda]  canlı log (varsayılan: hepsi)
#  ./scripts/mc.sh health      TPS + oyuncu + bellek
#  ./scripts/mc.sh rcon <oda> <komut>
#  ./scripts/mc.sh backup      dünyayı arşivle
#  ./scripts/mc.sh restore     son arşivi geri yükle
#  ./scripts/mc.sh doctor      20 kontrol
#  ./scripts/mc.sh bot <mod>   craftya-bot (scan/apply/verify/guard)
# ============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"
COMPOSE="docker-compose.hub.yml"
ROOMS="lobby survival skyblock bedwars duello parkur"
AUTH_ROOM="auth"          # limbo: oyun odası değil

# RCON portu = 25575 + odanın alfabetik sırası (render.py ile BİREBİR aynı kural).
# Elle yazılmış harita = oda eklendikçe kayar, yanlış sunucuya bağlanır.
# Tek kaynak: render.py'nin ürettiği room.meta.json → runtime'da hazır.
rcon_port() {
  local room="$1" meta
  meta="$HERE/runtime/rooms/$room/room.meta.json"
  if [[ -f "$meta" ]]; then
    python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['rcon_port'])" "$meta" 2>/dev/null && return
  fi
  # meta yoksa: odaları alfabetik diz, sırayı bul (render.py ile aynı)
  python3 - "$room" <<'PYEOF'
import sys
from pathlib import Path
rooms = sorted(d.name for d in Path("config/rooms").iterdir()
               if d.is_dir() and not d.name.startswith("_"))
try:
    print(25575 + rooms.index(sys.argv[1]))
except ValueError:
    sys.exit(1)
PYEOF
}

# container bellek limitleri: render.py üretir, compose okur
load_room_env() {
  if [[ -f runtime/rooms.env ]]; then
    set -a; . runtime/rooms.env; set +a
  fi
}

need_docker() {
  command -v docker >/dev/null 2>&1 || { echo "HATA: docker yok. Sunucu docker ile çalışır."; exit 1; }
}

case "${1:-help}" in
  up)
    need_docker
    [[ -f .env ]] || { cp .env.example .env; echo ".env oluşturuldu (sırlar render sırasında üretilir)"; }
    python3 scripts/render.py
    if ! ls runtime/plugins/*.jar >/dev/null 2>&1; then
      echo "pluginler yok, indiriliyor..."
      node scripts/plugins.mjs download
    fi
    python3 scripts/doctor.py || { echo; echo "doctor hata verdi — düzeltmeden başlatma."; exit 1; }
    load_room_env
    docker compose -f "$COMPOSE" up -d
    echo
    echo "başlatıldı. hazır olması 2-5 dk: ./scripts/mc.sh logs"
    ;;
  down)
    need_docker
    docker compose -f "$COMPOSE" down
    echo "durdu — dünyalar volume'da korunuyor"
    ;;
  restart)
    need_docker
    docker compose -f "$COMPOSE" restart
    ;;
  logs)
    need_docker
    if [[ -n "${2:-}" ]]; then
      docker logs -f --tail 120 "hub-$2"
    else
      docker compose -f "$COMPOSE" logs -f --tail 60
    fi
    ;;
  health)
    need_docker
    echo "── konteynerler ──"
    for r in $ROOMS; do
      st=$(docker inspect -f '{{.State.Status}}' "hub-$r" 2>/dev/null || echo "yok")
      mem=$(docker stats --no-stream --format '{{.MemUsage}}' "hub-$r" 2>/dev/null || echo "-")
      players=$(docker exec "hub-$r" rcon-cli --port "$(rcon_port "$r")" list 2>/dev/null | grep -oE 'There are [0-9]+' | grep -oE '[0-9]+' || echo 0)
      printf '  %-10s %-8s %-24s %s oyuncu\n' "$r" "$st" "$mem" "${players:-0}"
    done
    echo
    echo "── proxy ──"
    docker inspect -f '  velocity: {{.State.Status}}' hub-velocity 2>/dev/null || echo "  velocity: yok"
    echo
    if [[ -f logs/mem-governor.log ]]; then
      echo "── bellek governor (son 5) ──"
      tail -5 logs/mem-governor.log
    fi
    ;;
  rcon)
    need_docker
    [[ -n "${2:-}" && -n "${3:-}" ]] || { echo "kullanım: mc.sh rcon <oda> <komut>"; exit 2; }
    shift 2
    docker exec "hub-$2" rcon-cli --port "$(rcon_port "${2}")" "$*"
    ;;
  backup)
    need_docker
    ./scripts/world-sync.sh save
    ./scripts/world-sync.sh upload
    ;;
  restore)
    need_docker
    ./scripts/world-sync.sh restore
    ;;
  doctor)
    python3 scripts/doctor.py
    ;;
  bot)
    [[ -n "${2:-}" ]] || { echo "kullanım: mc.sh bot <scan|apply|verify|guard>"; exit 2; }
    cd tools/craftya-bot
    [[ -d node_modules ]] || npm install --silent
    node "src/$2.js" "${@:3}"
    ;;
  players)
    need_docker
    for r in $ROOMS; do
      out=$(docker exec "hub-$r" rcon-cli --port "$(rcon_port "$r")" list 2>/dev/null || true)
      [[ -n "$out" ]] && printf '%-10s %s\n' "$r" "$out"
    done
    ;;
  governor)
    ./scripts/mem-governor.sh "${2:---once}"
    ;;
  help|*)
    sed -n '3,14p' "$0" | sed 's/^# \{0,1\}//'
    ;;
esac
