#!/usr/bin/env bash
# ============================================================================
#  Craftya — dünya senkronu (6 saatlik devir için)
# ============================================================================
#  GitHub Actions runner'ı 6 saatte bir değişiyor. Dünya bir yerde durmalı.
#  Karar: **GitHub artifact** (10 GB/dosya, 90 gün, ekstra hesap yok).
#  Drive/OAuth YOK — kullanıcı haklı: uğraştırır, ayrıca agent dışarıya yazamaz.
#
#  Alt komutlar:
#    save     tüm dünyaları RCON ile diske yaz (flush) + dünyayı tar'la
#    upload   tar'ı artifact dizinine koy (workflow artifacts/ → upload-artifact)
#    restore  artifact'ı indirip dünyaları yerine aç (bütünlük kontrolü ile)
#    status   ne var ne yok (boyut, tarih, hash)
#
#  Bütünlük: her tar için sha256 yazılır; restore sırasında doğrulanır.
#  Bozuk yedek sessizce geri yüklenmez → erken ve yüksek sesle hata.
# ============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="${CRAFTYA_DATA:-$HERE/runtime/data}"
ARCHIVE="${CRAFTYA_ARCHIVE:-$HERE/artifacts}"
ROOMS=(lobby survival skyblock bedwars duello parkur)
STAMP_FMT='%Y%m%d-%H%M'

R() { printf '\033[%sm%s\033[0m\n' "$1" "$2"; }
ok() { R 32 "  ✓ $*"; }
warn() { R 33 "  ! $*"; }
die() { R 31 "  ✗ $*" >&2; exit 1; }

rcon_port() {
  case "$1" in
    lobby) echo 25577 ;; survival) echo 25580 ;; skyblock) echo 25579 ;;
    bedwars) echo 25575 ;; duello) echo 25576 ;; parkur) echo 25578 ;;
  esac
}

# ---------------------------------------------------------------- save
cmd_save() {
  R 36 "dünyalar kaydediliyor"
  for r in "${ROOMS[@]}"; do
    if ! docker inspect -f '{{.State.Running}}' "hub-$r" 2>/dev/null | grep -q true; then
      warn "$r çalışmıyor, atlandı"; continue
    fi
    # save-off → flush → save-on: yedek sırasında yarım chunk yazılmasın
    for c in "save-off" "save-all flush"; do
      docker exec "hub-$r" rcon-cli --port "$(rcon_port "$r")" "$c" >/dev/null 2>&1 || warn "$r: /$c yanıt vermedi"
    done
    # flush'ın bitmesini bekle
    sleep 2
    docker exec "hub-$r" rcon-cli --port "$(rcon_port "$r")" "save-on" >/dev/null 2>&1 || true
    ok "$r kaydedildi"
  done
}

# ---------------------------------------------------------------- upload
cmd_upload() {
  local stamp tar sha
  stamp="$(date +"$STAMP_FMT")"
  mkdir -p "$ARCHIVE"
  tar="$ARCHIVE/craftya-world-$stamp.tar.gz"
  R 36 "arşivleniyor → $tar"

  [[ -d "$DATA" ]] || die "veri dizini yok: $DATA"
  # sadece dünya klasörleri: dünya + playerdata + advance
  local args=()
  for r in "${ROOMS[@]}"; do
    [[ -d "$DATA/$r" ]] || continue
    args+=(-C "$DATA/$r" "world" "world_nether" "world_the_end")
    # ikinci turda var olanları ekle (shell glob güvenliği)
  done
  # basit ve güvenilir yol: her odanın dizinini ayrı ayrı ekle
  local list=()
  for r in "${ROOMS[@]}"; do
    for w in world world_nether world_the_end; do
      [[ -d "$DATA/$r/$w" ]] && list+=("$r/$w")
    done
  done
  [[ ${#list[@]} -gt 0 ]] || die "hiç dünya bulunamadı ($DATA)"

  tar -czf "$tar" -C "$DATA" "${list[@]}"
  sha="$(sha256sum "$tar" | cut -d' ' -f1)"
  echo "$sha  $(basename "$tar")" > "$tar.sha256"
  ok "arşiv: $(du -h "$tar" | cut -f1) · sha256 ${sha:0:16}…"

  # eski arşivleri buda (varsayılan: 3 tut)
  local keep="${CRAFTYA_KEEP_ARCHIVES:-3}"
  mapfile -t olds < <(ls -1t "$ARCHIVE"/craftya-world-*.tar.gz 2>/dev/null | tail -n +$((keep + 1)) || true)
  for o in "${olds[@]:-}"; do
    [[ -n "$o" ]] || continue
    rm -f "$o" "$o.sha256"
    warn "eski arşiv silindi: $(basename "$o")"
  done

  # workflow için yol bilgisi
  [[ -n "${GITHUB_OUTPUT:-}" ]] && echo "archive=$tar" >> "$GITHUB_OUTPUT"
  echo "$tar"
}

# ---------------------------------------------------------------- restore
cmd_restore() {
  local tar="${1:-}"
  if [[ -z "$tar" ]]; then
    tar="$(ls -1t "$ARCHIVE"/craftya-world-*.tar.gz 2>/dev/null | head -1 || true)"
  fi
  [[ -n "$tar" && -f "$tar" ]] || die "arşiv bulunamadı (artifacts/ boş)"

  R 36 "bütünlük kontrolü: $(basename "$tar")"
  if [[ -f "$tar.sha256" ]]; then
    local want got
    want="$(cut -d' ' -f1 "$tar.sha256")"
    got="$(sha256sum "$tar" | cut -d' ' -f1)"
    [[ "$want" == "$got" ]] || die "sha256 uyuşmuyor! arşiv bozuk: $tar"
    ok "sha256 doğru"
  else
    warn "sha256 dosyası yok — bütünlük doğrulanamadı"
  fi

  R 36 "açılıyor → $DATA"
  mkdir -p "$DATA"
  tar -xzf "$tar" -C "$DATA"
  ok "dünyalar yerinde"
  for r in "${ROOMS[@]}"; do
    [[ -d "$DATA/$r/world" ]] && ok "$r dünyası hazır"
  done
}

# ---------------------------------------------------------------- status
cmd_status() {
  R 36 "arşivler ($ARCHIVE)"
  if ls -1 "$ARCHIVE"/craftya-world-*.tar.gz >/dev/null 2>&1; then
    ls -lht "$ARCHIVE"/craftya-world-*.tar.gz | awk '{printf "  %-45s %s\n", $9, $5}'
  else
    warn "henüz arşiv yok"
  fi
  echo
  R 36 "veri dizini ($DATA)"
  if [[ -d "$DATA" ]]; then
    du -sh "$DATA" 2>/dev/null | awk '{print "  toplam: " $1}'
    for r in "${ROOMS[@]}"; do
      [[ -d "$DATA/$r" ]] && printf '  %-10s %s\n' "$r" "$(du -sh "$DATA/$r" 2>/dev/null | cut -f1)"
    done
  else
    warn "veri dizini yok"
  fi
}

# ---------------------------------------------------------------- ana
case "${1:-status}" in
  save) shift; cmd_save "$@" ;;
  upload) shift; cmd_upload "$@" ;;
  restore) shift; cmd_restore "$@" ;;
  status) cmd_status ;;
  *) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
