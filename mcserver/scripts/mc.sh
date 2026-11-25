#!/usr/bin/env bash
# Craftya MC — tek komutluk yönetim aracı
#
#   ./scripts/mc.sh up           başlat (ilk seferde plugin'leri de indirir)
#   ./scripts/mc.sh down         durdur
#   ./scripts/mc.sh restart      yeniden başlat
#   ./scripts/mc.sh logs         canlı log
#   ./scripts/mc.sh console      sunucu konsolu (rcon, çıkmak için /quit)
#   ./scripts/mc.sh health       TPS, oyuncu sayısı, bellek
#   ./scripts/mc.sh backup       hemen yedek al
#   ./scripts/mc.sh doctor       yapılandırmayı denetle
#   ./scripts/mc.sh players      çevrimiçi oyuncular
#   ./scripts/mc.sh tunnel       playit tüneli aç (dışarıdan erişim)
#   ./scripts/mc.sh update       sunucu + plugin'leri güncelle (dünya korunur)

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"

if [[ ! -f .env ]]; then
  echo ".env yok, .env.example'dan oluşturuluyor..."
  cp .env.example .env
  echo "ÖNEMLİ: .env içindeki RCON_PASSWORD'ü değiştir, sonra tekrar çalıştır."
  exit 1
fi

RCON() {
  docker compose exec -T minecraft rcon-cli "$@" 2>/dev/null
}

need_plugins() {
  local n
  n=$(find plugins -maxdepth 1 -name '*.jar' 2>/dev/null | wc -l)
  if [[ "$n" -eq 0 ]]; then
    echo "plugin'ler yok, indiriliyor..."
    ./scripts/fetch-plugins.sh
  fi
}

case "${1:-help}" in
  up)
    need_plugins
    docker compose up -d
    echo
    echo "sunucu başlıyor — hazır olana kadar izlemek için: ./scripts/mc.sh logs"
    ;;
  down)
    docker compose down
    echo "durdu. dünya korundu."
    ;;
  restart)
    docker compose restart minecraft
    ;;
  logs)
    docker compose logs -f --tail 100 minecraft
    ;;
  console)
    docker compose exec minecraft rcon-cli
    ;;
  players)
    echo "çevrimiçi: $(RCON list || echo 'sunucu kapalı ya da RCON yanıt vermiyor')"
    ;;
  health)
    echo "--- TPS (20.0 = ideal) ---"
    RCON tps || true
    echo "--- oyuncular ---"
    RCON list || true
    echo "--- konteyner ---"
    docker stats --no-stream --format \
      'CPU {{.CPUPerc}}  RAM {{.MemUsage}}  NET {{.NetIO}}' craftya-mc 2>/dev/null || true
    ;;
  backup)
    # mc-backup konteynerine tek seferlik yedek aldır
    docker compose exec -T backup save-off 2>/dev/null || true
    docker compose exec -T backup backup now 2>/dev/null \
      || echo "otomatik yedek konteyneri yok; elle yedek alınıyor..."
    docker run --rm -v mcserver_mc-data:/data:ro -v "$PWD/backups":/backup \
      alpine tar czf "/backup/world-$(date +%Y%m%d-%H%M).tar.gz" -C /data world
    ls -lh backups | tail -3
    ;;
  doctor)
    python3 scripts/doctor.py
    ;;
  tunnel)
    if ! grep -qE '^PLAYIT_SECRET=.+' .env; then
      echo "PLAYIT_SECRET boş. playit.gg → Agents → New Agent → çıkan anahtarı .env'ye yaz."
      exit 1
    fi
    docker compose --profile tunnel up -d playit
    echo "tünel açıldı. playit.gg panelinde 'Minecraft Java' tunnel'ı ekle: 127.0.0.1:25565"
    ;;
  update)
    echo "dünya ve ayarlar korunuyor, sadece imaj + plugin'ler yenileniyor."
    ./scripts/fetch-plugins.sh --force
    docker compose pull
    docker compose up -d
    ;;
  help|*)
    sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
    ;;
esac
