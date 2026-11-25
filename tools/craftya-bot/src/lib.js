/**
 * craftya-bot ortak katman: config, bağlantı, auth, log.
 * Her komut (scan/apply/verify/guard) buradan geçer → tek yerde düzelir.
 */
import { readFile, mkdir, appendFile } from 'node:fs/promises';
import path from 'node:path';
import process from 'node:process';

export const ROOT = process.cwd();

/** Ortak log: hem konsol hem dosya (denetim izi). */
export async function log(level, msg) {
  const line = `[${new Date().toISOString()}] ${level.padEnd(5)} ${msg}`;
  if (level === 'ERROR') console.error(line);
  else console.log(line);
  try {
    await mkdir(path.join(ROOT, 'out'), { recursive: true });
    await appendFile(path.join(ROOT, 'out', 'bot.log'), line + '\n');
  } catch { /* log yazamazsa çalışmayı durdurma */ }
}

export async function loadConfig() {
  const p = path.join(ROOT, 'config.json');
  let raw;
  try {
    raw = await readFile(p, 'utf8');
  } catch {
    throw new Error(`config.json yok. "cp config.example.json config.json" ile oluştur.`);
  }
  const cfg = JSON.parse(raw);
  for (const key of ['host', 'port', 'username']) {
    if (cfg[key] === undefined || cfg[key] === '') throw new Error(`config.json: "${key}" eksik`);
  }
  return cfg;
}

/**
 * Sunucuya bağlan ve gerekli hazırlığı yap (login/register).
 * Proxy'yi ATLAR: velocity forwarding'i olan bir sunucuya bot bağlanamaz,
 * bu yüzden doğrudan odanın portuna gidilir.
 */
export async function connect(cfg, { onReady } = {}) {
  let mineflayer;
  try {
    mineflayer = (await import('mineflayer')).default;
  } catch {
    throw new Error('mineflayer kurulu değil: cd tools/craftya-bot && npm install');
  }

  const create = () =>
    mineflayer.createBot({
      host: cfg.host,
      port: cfg.port,
      username: cfg.username,
      auth: 'offline',
      version: cfg.version === 'auto' ? false : cfg.version,
      hideErrors: false,
    });

  return new Promise((resolve, reject) => {
    let attempt = 0;
    let settled = false;

    const launch = () => {
      attempt += 1;
      const bot = create();
      const timeout = setTimeout(() => {
        bot.quit();
        log('WARN', 'bağlantı zaman aşımı');
      }, 45000);

      bot.once('spawn', async () => {
        clearTimeout(timeout);
        await handleAuth(bot, cfg);
        await log('INFO', `bağlandı: ${cfg.host}:${cfg.port} (${bot.username})`);
        if (onReady) await onReady(bot);
        if (!settled) { settled = true; resolve(bot); }
      });

      bot.once('error', (err) => {
        clearTimeout(timeout);
        const m = String(err?.message || err);
        // sürüm uyuşmazlığı / kick mesajları en sık hata — anlaşılır yaz
        if (/version/i.test(m)) log('ERROR', `sürüm uyuşmazlığı: ${m} (config: "version": "auto" dene)`);
        else if (/ECONNREFUSED/i.test(m)) log('ERROR', `bağlantı reddedildi: ${cfg.host}:${cfg.port} açık mı?`);
        else log('ERROR', m);
      });

      bot.once('end', async (reason) => {
        clearTimeout(timeout);
        await log('WARN', `bağlantı kapandı: ${reason}`);
        if (!cfg.reconnect?.enabled) {
          if (!settled) { settled = true; reject(new Error(`bağlantı kapandı: ${reason}`)); }
          return;
        }
        const delay = Math.min(
          cfg.reconnect.maxDelayMs ?? 60000,
          (cfg.reconnect.minDelayMs ?? 3000) * Math.pow(1.8, Math.min(attempt, 6)),
        );
        await log('INFO', `${Math.round(delay / 1000)} sn sonra yeniden denenecek`);
        setTimeout(launch, delay);
      });

      bot.on('kicked', (reason) => log('WARN', `kicklendi: ${JSON.stringify(reason)}`));
    };

    launch();
  });
}

/** AuthMe: ilk girişte /register, sonrakilerde /login. Sessizce yutma, logla. */
export async function handleAuth(bot, cfg) {
  if (!cfg.auth?.enabled) return;
  const pw = cfg.auth.password;
  if (!pw || pw.startsWith('BOT-PAROLASI')) {
    await log('WARN', 'auth açık ama şifre ayarlanmamış — /login atlanıyor');
    return;
  }
  // AuthMe giriş ekranında hareketi engeller; iki komutu da dene, hangisi tutarsa.
  await sleep(1200);
  bot.chat(`/login ${pw}`);
  await sleep(1500);
  bot.chat(`/register ${pw} ${pw}`); // zaten kayıtlıysa AuthMe bunu yok sayar
  await sleep(1500);
  await log('INFO', 'auth komutları gönderildi (/login + /register)');
}

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** Blok adını "minecraft:oak_planks" → "oak_planks" sadeleştir. */
export const shortName = (n) => String(n || '').replace(/^minecraft:/, '');

/** Paket adı → insan dili ("minecraft:oak_planks" → "Oak Planks"). */
export function prettyName(n) {
  return shortName(n)
    .split('_')
    .map((w) => (w ? w[0].toUpperCase() + w.slice(1) : w))
    .join(' ');
}
