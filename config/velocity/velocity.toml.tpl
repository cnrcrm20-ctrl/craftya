# Üretildi: scripts/render.py
# Velocity — ön kapı. Oyuncu sadece buraya bağlanır, oda geçişi buradan yönetilir.
config-version = "2.7"

bind = "0.0.0.0:${PORT_VELOCITY}"
# MOTD gradyanlı: scripts/gradient.py üretir (MiniMessage → hex)
# >>> GRADIENT:BEGIN proxy.motd
motd = "<gradient:#7ef29d:#49c6ff>CRAFTYA</gradient><dark_gray> | <gray>sunucu dunyasi"
# <<< GRADIENT:END proxy.motd
show-max-players = ${MAX_PLAYERS}
online-mode = false
force-key-authentication = false
prevent-client-proxy-connections = false
player-info-forwarding-mode = "modern"
forwarding-secret-file = "forwarding.secret"
announce-forge = false
kick-existing-players = false
ping-passthrough = "DISABLED"
sample-players-in-ping = true

[servers]
__SERVERS_BLOCK__
# NOT: "auth" (limbo) bilinçli olarak try listesinde DEĞİL — oyuncu oraya
# sadece LibreLogin karar verdiğinde düşer, normal girişte lobiye gider.
try = ["lobby"]

[forced-hosts]
# "mc.craftya.dev" = ["lobby"]

[advanced]
compression-threshold = 256
compression-level = -1
login-ratelimit = 3000
connection-timeout = 5000
read-timeout = 30000
haproxy-protocol = false
tcp-fast-open = true
bungee-plugin-message-channel = true
show-ping-requests = false
failover-on-unexpected-server-disconnect = true
announce-proxy-commands = true
log-command-executions = true
log-player-connections = true
accepts-transfers = true

[query]
enabled = false
port = 25565
map = "Craftya"
show-plugins = false
