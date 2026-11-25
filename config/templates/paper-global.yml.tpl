# Üretildi: scripts/render.py
_version: 30
chunk-loading-basic:
  player-max-chunk-send-rate: 75.0
  target-player-chunk-send-rate: 100.0
chunk-system:
  worker-threads: -1
  io-threads: -1
collisions:
  enable-player-collisions: true
logging:
  deobfuscate-stacktraces: true
  log-player-ip-addresses: true
messages:
  kick:
    authentication-servers-down: "§cKimlik sunuculari kapali, birazdan tekrar dene."
    connection-throttle: "§cCok hizli baglaniyorsun, 5 saniye bekle."
    flying-player: "§cUcma tespit edildi."
  no-permission: "§cBunun icin yetkin yok."
misc:
  max-joins-per-tick: 5
  region-file-cache-size: 256
anti-xray:
  enabled: ${ANTI_XRAY:-true}
  engine-mode: 2
  max-block-height: 64
  update-radius: 2
  hidden-blocks:
    - copper_ore
    - deepslate_copper_ore
    - gold_ore
    - deepslate_gold_ore
    - iron_ore
    - deepslate_iron_ore
    - coal_ore
    - deepslate_coal_ore
    - lapis_ore
    - deepslate_lapis_ore
    - diamond_ore
    - deepslate_diamond_ore
    - redstone_ore
    - deepslate_redstone_ore
    - emerald_ore
    - deepslate_emerald_ore
    - chest
    - spawner
  replacement-blocks:
    - stone
    - deepslate
    - andesite
    - granite
    - diorite
    - tuff
