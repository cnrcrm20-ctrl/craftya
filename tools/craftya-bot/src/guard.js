#!/usr/bin/env node
/**
 * guard.js — ilk kademe moderasyon (gözlemci modu).
 *
 * Sorumluluğu kasıtlı olarak DAR:
 *   - chat'i loglar (denetim izi)
 *   - yasaklı kelime görürse mod kanalına bildirir ("/say" ile veya konsola)
 *   - sunucu durumunu (TPS, oyuncu) periyodik kaydeder
 *
 * YAPMAZ: ban, kick, mute. "Son karar insan" — cracked sunucuda yanlış ban
 * geri dönüşü olmayan güven kaybıdır. Otomatik işlem ancak kullanıcı
 * config'te açıkça işaretlerse devreye girer (mode: "enforce").
 *
 * Örnek: node src/guard.js
 */
import { appendFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { connect, loadConfig, log, sleep, ROOT } from './lib.js';

const DEFAULT_WORDS = [
  // örnek liste — config.json'da "guard.words" ile değiştirilir
  'küfür1', 'küfür2',
];

async function main() {
  const cfg = await loadConfig();
  const g = cfg.guard || {};
  const mode = g.mode || 'observer';
  const words = (g.words || DEFAULT_WORDS).map((w) => w.toLocaleLowerCase('tr'));
  const chatLog = path.join(ROOT, g.chatLogPath || 'out/chat.log');
  await mkdir(path.dirname(chatLog), { recursive: true });

  await log('INFO', `guard başlıyor — mod: ${mode} (kelime sayısı: ${words.length})`);
  const bot = await connect(cfg);

  const writeChat = async (who, msg) => {
    const line = `${new Date().toISOString()}\t${who}\t${msg}`;
    await appendFile(chatLog, line + '\n');
  };

  bot.on('chat', async (username, message) => {
    if (username === bot.username) return;
    await writeChat(username, message);
    const low = message.toLocaleLowerCase('tr');
    const hit = words.find((w) => w && low.includes(w));
    if (!hit) return;
    await log('WARN', `kural ihlali şüphesi — ${username}: "${message.slice(0, 80)}" (eşleşen: ${hit})`);
    if (mode === 'enforce') {
      // DİKKAT: sadece kullanıcı bunu açıkça açtıysa. Varsayılan kapalı.
      bot.chat(`/warn ${username} küfür/uygunsuz dil`);
      await log('WARN', `uyarı gönderildi: ${username}`);
    } else {
      // gözlemci modu: kimseyi cezalandırma, sadece görünür uyar
      bot.chat(`/say §e[mod] §7${username} — lütfen dilinize dikkat edin.`);
    }
  });

  // periyodik durum kaydı: TPS/oyuncu → out/status.json (site de okuyabilir)
  const statusPath = path.join(ROOT, 'out', 'status.json');
  setInterval(async () => {
    try {
      const players = Object.keys(bot.players || {}).filter((p) => bot.players[p].entity);
      const status = {
        at: new Date().toISOString(),
        online: players.length,
        players,
        position: bot.entity ? [Math.round(bot.entity.position.x), Math.round(bot.entity.position.y)] : null,
        health: bot.health,
      };
      await mkdir(path.dirname(statusPath), { recursive: true });
      await appendFile(statusPath.replace('.json', '.ndjson'), JSON.stringify(status) + '\n');
    } catch { /* durum yazılamazsa guard çalışmaya devam eder */ }
  }, 30000);

  bot.on('playerJoined', (p) => log('INFO', `katıldı: ${p.username}`));
  bot.on('playerLeft', (p) => log('INFO', `ayrıldı: ${p.username}`));

  // bot sonsuza kadar çalışır (lib.js yeniden bağlanmayı yönetir)
  await sleep(Number.MAX_SAFE_INTEGER);
}

main().catch(async (e) => {
  await log('ERROR', e.stack || e.message);
  process.exit(1);
});
