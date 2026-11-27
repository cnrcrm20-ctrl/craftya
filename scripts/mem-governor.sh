#!/usr/bin/env bash
# ============================================================================
#  Craftya — bellek havuzu yöneticisi (governor)
# ============================================================================
#  Mantık: 5 GB ortak havuz. Her 30 sn'de kimin ne kadar hak ettiğine karar verir.
#
#  NEDEN jcmd: -Xmx açılışta sabitlenir, canlı değişmez. Ama Java 12+ şu anki
#  yumuşak sınırı VM.set_flag SoftMaxHeapSize ile ÇALIŞIRKEN değiştirmeye izin
#  verir. Restart yok, oyuncu düşmez. cgroup sınırı da docker update ile canlı
#  değişir (fiziksel emniyet).
#
#  KURALLAR (bkz. BRAIN.md §4.6):
#    1. Havuz toplamı POOL_GB'yi aşamaz
#    2. Dolu odanın payı asla tabanın altına inmez
#    3. Toplam RSS > LIMIT_GB ise: önce BOŞ odalar küçültülür
#    4. Sadece boş oda yeniden başlatılır (JVM yeniden açılmadan taban değişmez)
#    5. governor çökerse kimseye zarar gelmez — herkes tabanında kalır
#
#  Kullanım:
#     ./scripts/mem-governor.sh --once     tek tur (test için)
#     ./scripts/mem-governor.sh            döngü (30 sn)
#     ./scripts/mem-governor.sh --dry-run  sadece karar ver, uygulama
# ============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG="$HERE/logs/mem-governor.log"
INTERVAL="${GOVERNOR_INTERVAL:-30}"
LIMIT_GB="${GOVERNOR_LIMIT_GB:-13}"       # bu sınırı aşınca müdahale
POOL_GB="${GOVERNOR_POOL_GB:-5}"          # dağıtılabilir havuz
TOTAL_GB="${GOVERNOR_TOTAL_GB:-16}"

ROOMS=(lobby survival skyblock bedwars duello parkur)

# oda -> taban (MB) | oda -> izin (Xmx)
declare -A BASE=( [lobby]=384 [survival]=1536 [skyblock]=768 [bedwars]=768 [duello]=512 [parkur]=256 )
declare -A CAP=(  [lobby]=2048 [survival]=5120 [skyblock]=4096 [bedwars]=5120 [duello]=3072 [parkur]=1536 )

ONCE=0; DRY=0
for a in "$@"; do
  case "$a" in
    --once) ONCE=1 ;;
    --dry-run) DRY=1 ;;
    *) echo "bilinmeyen argüman: $a" >&2; exit 2 ;;
  esac
done

mkdir -p "$HERE/logs"
log() {
  local line
  line="[$(date '+%F %T')] $*"
  echo "$line" | tee -a "$LOG"
}

# ---- yardımcılar ----------------------------------------------------------
container_of() { echo "hub-$1"; }

# o odadaki oyuncu sayısı (RCON). Kapalıysa 0.
players_in() {
  local room="$1" port
  port=$(rcon_port_of "$room")
  local out
  out=$(timeout 5 docker exec "hub-$room" rcon-cli --port "$port" list 2>/dev/null || true)
  if [[ -z "$out" ]]; then echo 0; return; fi
  # "There are 3 of a max of 20 players online: a, b, c" → 3
  local n
  n=$(echo "$out" | grep -oE 'There are [0-9]+' | grep -oE '[0-9]+' | head -1)
  echo "${n:-0}"
}

rcon_port_of() {
  case "$1" in
    lobby) echo 25577 ;; survival) echo 25580 ;; skyblock) echo 25579 ;;
    bedwars) echo 25575 ;; duello) echo 25576 ;; parkur) echo 25578 ;;
  esac
}

# RSS MB
rss_mb() {
  docker stats --no-stream --format '{{.MemUsage}}' "hub-$1" 2>/dev/null \
    | awk '{print $1}' | sed 's/MiB//;s/GiB//' \
    | awk -v g=0 '{print $0}'
}

# RSS MB (güvenilir: cgroup dosyasından)
rss_mb_exact() {
  local cid
  cid=$(docker inspect -f '{{.Id}}' "hub-$1" 2>/dev/null) || { echo 0; return; }
  local path="/sys/fs/cgroup/system.slice/docker-${cid}.scope/memory.current"
  [[ -f "$path" ]] || path="/sys/fs/cgroup/docker/${cid}/memory.current"
  if [[ -f "$path" ]]; then
    awk '{printf "%d", $1/1048576}' "$path"
  else
    # fallback: docker stats
    local v u
    read -r v u <<< "$(docker stats --no-stream --format '{{.MemUsage}}' "hub-$1" 2>/dev/null | tr '/' ' ' | awk '{print $1, $2}')"
    case "$v" in
      *GiB) awk -v x="${v%GiB}" 'BEGIN{printf "%d", x*1024}' ;;
      *MiB) echo "${v%MiB}" ;;
      *) echo 0 ;;
    esac
  fi
}

running() { [[ "$(docker inspect -f '{{.State.Running}}' "hub-$1" 2>/dev/null)" == "true" ]]; }

# canlı SoftMax değiştir (restart YOK)
set_softmax() {
  local room="$1" mb="$2"
  if [[ "$DRY" == "1" ]]; then log "  [dry] $room SoftMax → ${mb}M"; return 0; fi
  # jcmd ile: java PID 1 (itzg imajında java ana süreç)
  if docker exec "hub-$room" jcmd 1 VM.set_flag SoftMaxHeapSize "${mb}M" >/dev/null 2>&1; then
    log "  ✓ $room SoftMax → ${mb}M (canlı)"
    return 0
  fi
  log "  ! $room SoftMax değişmedi (jcmd yok ya da sürüm desteklemiyor) — restart gerekir"
  return 1
}

# cgroup limiti (fiziksel emniyet)
set_cgroup() {
  local room="$1" mb="$2"
  if [[ "$DRY" == "1" ]]; then log "  [dry] $room cgroup → ${mb}M"; return 0; fi
  if docker update --memory "${mb}M" --memory-swap "${mb}M" "hub-$room" >/dev/null 2>&1; then
    log "  ✓ $room cgroup → ${mb}M"
  else
    log "  ✗ $room cgroup değişmedi"
  fi
}

# boş odayı yeni tabanla yeniden başlat (tek yol: JVM açılışta sabitler)
restart_room() {
  local room="$1" base="$2"
  if [[ "$DRY" == "1" ]]; then log "  [dry] $room yeniden başlatılacak (taban ${base}M)"; return 0; fi
  log "  ↻ $room yeniden başlatılıyor (yeni taban ${base}M)"
  GOVERNOR_SOFTMAX_OVERRIDE="${base}M" docker compose -f "$HERE/docker-compose.hub.yml" \
    up -d --no-deps --force-recreate "$room" >>"$LOG" 2>&1 || log "  ✗ $room restart başarısız"
}

# ---- tek tur --------------------------------------------------------------
tick() {
  local total=0
  declare -A players cur soft
  for r in "${ROOMS[@]}"; do
    if ! running "$r"; then players[$r]=0; cur[$r]=0; continue; fi
    players[$r]=$(players_in "$r")
    cur[$r]=$(rss_mb_exact "$r")
    total=$((total + cur[$r]))
  done

  local total_gb=$(( total / 1024 ))
  log "tur: toplam ${total}MB (${total_gb}GB / limit ${LIMIT_GB}GB) | " \
      "$(for r in "${ROOMS[@]}"; do printf '%s:%s/%sMB ' "$r" "${players[$r]}" "${cur[$r]}"; done)"

  # 1) sınır aşıldı mı? önce boş odaları küçült
  if (( total_gb >= LIMIT_GB )); then
    log "UYARI: bellek sınırı aşıldı (${total_gb}GB ≥ ${LIMIT_GB}GB) — boş odalar küçültülüyor"
    for r in "${ROOMS[@]}"; do
      (( players[$r] > 0 )) && continue
      running "$r" || continue
      local new_base=${BASE[$r]}
      if (( cur[$r] > new_base + 256 )); then
        set_softmax "$r" "$new_base"
        set_cgroup "$r" $(( new_base + 512 ))
      fi
    done
    return
  fi

  # 2) talep puanı: oyuncu ağırlıklı + boş odalar tabana iner
  #    (TPS ölçümü RCON gerektirir; yoksa oyuncu sayısı yeterli sinyal)
  local pool_left=$(( POOL_GB * 1024 ))
  declare -A want
  for r in "${ROOMS[@]}"; do
    if ! running "$r"; then want[$r]=0; continue; fi
    if (( players[$r] == 0 )); then
      want[$r]=0                              # boş → taban
      if (( cur[$r] > BASE[$r] + 384 )); then
        set_softmax "$r" "${BASE[$r]}"
        pool_left=$(( pool_left - BASE[$r] ))
      else
        pool_left=$(( pool_left - BASE[$r] ))
      fi
      continue
    fi
    # dolu oda: oyuncu başına 160MB, tavan Xmx ile sınırlı
    local w=$(( BASE[$r] + players[$r] * 160 ))
    (( w > CAP[$r] )) && w=${CAP[$r]}
    want[$r]=$w
  done

  # havuzu dolu odalara dağıt (sırayla, en çok oyunculudan)
  local -a full=()
  for r in "${ROOMS[@]}"; do (( players[$r] > 0 )) && full+=("$r"); done
  if (( ${#full[@]} > 0 )); then
    IFS=$'\n' full=($(for r in "${full[@]}"; do printf '%s %s\n' "${players[$r]}" "$r"; done | sort -rn | awk '{print $2}'))
    unset IFS
  fi
  for r in "${full[@]}"; do
    local give=${want[$r]}
    local need=$(( give - BASE[$r] ))
    if (( need > pool_left )); then
      give=$(( BASE[$r] + pool_left ))
      need=$pool_left
    fi
    (( need <= 0 )) && continue
    pool_left=$(( pool_left - need ))
    (( give > CAP[$r] )) && give=${CAP[$r]}
    log "  → $r: ${players[$r]} oyuncu, hedef ${give}M (havuzdan ${need}M)"
    set_softmax "$r" "$give"
    set_cgroup "$r" $(( give + 512 ))
  done
  log "  havuzda kalan: $(( pool_left / 1024 ))GB"
}

# ---- ana döngü ------------------------------------------------------------
log "governor başladı: aralık=${INTERVAL}s limit=${LIMIT_GB}GB havuz=${POOL_GB}GB toplam=${TOTAL_GB}GB"
if ! command -v docker >/dev/null 2>&1; then
  log "HATA: docker yok — governor çalışamaz (sunucu da çalışmaz)"
  exit 1
fi

if [[ "$ONCE" == "1" ]]; then
  tick
  exit 0
fi

while true; do
  tick || log "tur hatası (governor devam ediyor — fail-safe)"
  sleep "$INTERVAL"
done
