#!/usr/bin/env bash
# Craftya MC — plugin indirici
#
# Ne yapar: plugins/ klasörüne sunucunun ihtiyaç duyduğu jar'ları indirir.
# Neden script: jar'ları repoya koymak (≈20 MB binary) kötü pratik. Sürümler burada
# pinli, tek komutla aynı sürümü herkes alır. Güncelleme = bir satır değiştir.
#
# Kullanım:
#   ./scripts/fetch-plugins.sh          # eksikleri indir
#   ./scripts/fetch-plugins.sh --force  # hepsini yeniden indir
#   ./scripts/fetch-plugins.sh --check  # sadece hangi URL'ler ayakta, indirmeden bak

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$HERE/plugins"
FORCE=0
CHECK=0
for a in "$@"; do
  case "$a" in
    --force) FORCE=1 ;;
    --check) CHECK=1 ;;
    *) echo "bilinmeyen argüman: $a" >&2; exit 2 ;;
  esac
done

mkdir -p "$DEST"

# ---- pinli sürümler ----
# Sürüm yükseltirken: URL'yi ve dosya adını birlikte değiştir, sonra --force çalıştır.
VIA_VERSION="5.12.3"

# name|url
ITEMS=(
  # Protocol çevirisi: Bedrock/Geyser eski-yeni Java sürümleri arasında köprü.
  # Klasik sunucularda olmazsa olmaz — istemci sürümü farklıysa kapıdan giremezsin.
  "ViaVersion-${VIA_VERSION}.jar|https://github.com/ViaVersion/ViaVersion/releases/download/${VIA_VERSION}/ViaVersion-${VIA_VERSION}.jar"
  "ViaBackwards-${VIA_VERSION}.jar|https://github.com/ViaVersion/ViaBackwards/releases/download/${VIA_VERSION}/ViaBackwards-${VIA_VERSION}.jar"

  # Geyser + Floodgate: Java sunucusuna Bedrock (telefon/konsol) oyuncusu bağlar.
  # "latest" bilinçli: Geyser kendi sürüm takibini yapıyor, eski kalırsa Bedrock
  # istemcileri bağlanamaz. Geyser'i sabitlemek burada riski artırır, azaltmaz.
  "Geyser-Spigot.jar|https://download.geysermc.org/v2/projects/geyser/versions/latest/builds/latest/downloads/spigot"
  "floodgate-spigot.jar|https://download.geysermc.org/v2/projects/floodgate/versions/latest/builds/latest/downloads/spigot"
)

# Opsiyonel eklentiler (LuckPerms, spark, CoreProtect, Chunky, sesli sohbet) burada
# DEĞİL: docker-compose.yml içindeki MODRINTH_PROJECTS satırından gelir. Sebep —
# Modrinth sürüm/hash çözümünü kendisi yapıyor, elle URL takibi kırılgan.
OPTIONAL=()

fetch() {
  local name="$1" url="$2" mandatory="$3"
  local out="$DEST/$name"

  if [[ "$CHECK" == "1" ]]; then
    local code
    code="$(curl -sIL -m 20 -o /dev/null -w '%{http_code}' "$url" || echo 000)"
    printf '%-28s %s  %s\n' "$name" "$code" "$url"
    return 0
  fi

  if [[ -f "$out" && "$FORCE" != "1" ]]; then
    echo "atlandı (var): $name"
    return 0
  fi

  echo "indiriliyor: $name"
  if ! curl -fsSL -m 300 --retry 3 --retry-delay 2 -o "$out.part" "$url"; then
    rm -f "$out.part"
    if [[ "$mandatory" == "1" ]]; then
      echo "HATA: zorunlu plugin indirilemedi: $name ($url)" >&2
      return 1
    fi
    echo "uyarı: opsiyonel plugin atlandı: $name" >&2
    return 0
  fi

  # jar sağlamlık kontrolü: ZIP imzası "PK\x03\x04"
  if [[ "$(head -c 2 "$out.part")" != "PK" ]]; then
    rm -f "$out.part"
    echo "HATA: indirilen dosya jar değil (HTML hata sayfası olabilir): $name" >&2
    [[ "$mandatory" == "1" ]] && return 1 || return 0
  fi
  mv "$out.part" "$out"
  echo "  ✓ $name ($(du -h "$out" | cut -f1))"
}

rc=0
for item in "${ITEMS[@]}"; do
  fetch "${item%%|*}" "${item#*|}" 1 || rc=1
done
for item in "${OPTIONAL[@]}"; do
  fetch "${item%%|*}" "${item#*|}" 0 || true
done

if [[ "$CHECK" == "1" ]]; then
  exit 0
fi

echo
echo "plugins/ hazır:"
ls -lh "$DEST" | tail -n +2 | awk '{print "  " $9 "  " $5}'
echo
echo "Sıradaki: docker compose up -d"
exit "$rc"
