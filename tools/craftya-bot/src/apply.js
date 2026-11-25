#!/usr/bin/env node
/**
 * apply.js — plan dosyasını uygula.
 *
 * plan.json biçimi (sadece 4 komut türü — keyfi komut YOK, güvenlik):
 *   { "ops": [
 *       { "op": "cmd",   "cmd": "fill 0 64 0 10 64 10 stone" },
 *       { "op": "block", "pos": [0,65,0], "block": "oak_planks" },
 *       { "op": "npc",   "pos": [0,65,0], "name": "Hayatta Kalma", "cmd": "server survival" },
 *       { "op": "wait",  "ms": 1000 }
 *   ] }
 *
 * Neden keyfi komut yok: bot `op` değil, ama yine de beyaz liste şart.
 * Sadece şu komut köklerine izin var: fill, setblock, summon, tp, gamemode, time,
 * weather, gamerule, npc, hologram, bw, parkour, save-all.
 *
 * Örnek:
 *   node src/apply.js --dry-run       ne yapılacak listele
 *   node src/apply.js                 uygula
 *   node src/apply.js --verify        uygula + sonucu kontrol et
 */
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { connect, loadConfig, log, sleep, ROOT } from './lib.js';

const args = process.argv.slice(2);
const DRY = args.includes('--dry-run');
const VERIFY = args.includes('--verify');

const ALLOWED = new Set([
  'fill', 'setblock', 'summon', 'tp', 'gamemode', 'time', 'weather',
  'gamerule', 'npc', 'hologram', 'holograms', 'bw', 'parkour', 'save-all',
  'say', 'title', 'setworldspawn',
]);

function assertAllowed(cmd) {
  const root = String(cmd).trim().split(/\s+/)[0].replace(/^\//, '').toLowerCase();
  if (!ALLOWED.has(root)) throw new Error(`izin verilmeyen komut: "${root}" (beyaz liste dışı)`);
  return '/' + String(cmd).trim().replace(/^\//, '');
}

async function main() {
  const cfg = await loadConfig();
  const planPath = path.join(ROOT, cfg.apply?.planPath ?? 'out/plan.json');
  let plan;
  try {
    plan = JSON.parse(await readFile(planPath, 'utf8'));
  } catch (e) {
    throw new Error(`plan okunamadı (${planPath}): ${e.message}`);
  }
  const ops = plan.ops || [];
  await log('INFO', `plan: ${ops.length} işlem${DRY ? ' (dry-run)' : ''}`);

  // 1) önce hepsini doğrula — yarısı çalışıp yarısı patlamasın
  for (const [i, op] of ops.entries()) {
    if (op.op === 'cmd') assertAllowed(op.cmd);
    else if (op.op === 'block') {
      if (!Array.isArray(op.pos) || op.pos.length !== 3) throw new Error(`op[${i}]: pos 3 elemanlı olmalı`);
      if (!op.block) throw new Error(`op[${i}]: block eksik`);
    } else if (op.op === 'npc') {
      if (!Array.isArray(op.pos) || op.pos.length !== 3) throw new Error(`op[${i}]: pos 3 elemanlı olmalı`);
    } else if (op.op !== 'wait') {
      throw new Error(`op[${i}]: bilinmeyen tür "${op.op}"`);
    }
  }
  await log('INFO', 'plan doğrulandı');

  if (DRY) {
    for (const [i, op] of ops.entries()) {
      const desc =
        op.op === 'cmd' ? `/ ${op.cmd}`
        : op.op === 'block' ? `blok ${op.block} @ ${op.pos.join(',')}`
        : op.op === 'npc' ? `NPC "${op.name}" @ ${op.pos.join(',')}`
        : `bekle ${op.ms}ms`;
      console.log(`  ${String(i).padStart(3)}  ${desc}`);
    }
    console.log('');
    console.log(`  toplam ${ops.length} işlem — uygulamak için --dry-run olmadan çalıştır`);
    return 0;
  }

  // 2) bağlan ve uygula
  const bot = await connect(cfg);
  await sleep(2000);

  let done = 0;
  for (const [i, op] of ops.entries()) {
    try {
      if (op.op === 'cmd') {
        bot.chat(assertAllowed(op.cmd));
      } else if (op.op === 'block') {
        bot.chat(`/setblock ${op.pos.join(' ')} ${op.block}`);
      } else if (op.op === 'npc') {
        // Citizens komut zinciri: NPC oluştur, isim ver, sağ tık komutunu ata
        bot.chat(`/npc create ${op.name}`);
        await sleep(400);
        bot.chat(`/npc move ${op.pos.join(' ')}`);
        await sleep(300);
        if (op.cmd) {
          bot.chat(`/npc command add ${op.cmd}`);
          await sleep(300);
        }
      } else if (op.op === 'wait') {
        await sleep(op.ms ?? 500);
      }
      done++;
      if (done % 10 === 0) await log('INFO', `${done}/${ops.length} işlem`);
      await sleep(op.op === 'wait' ? 0 : 220); // komut spam'i yememek için
    } catch (e) {
      await log('ERROR', `op[${i}] başarısız: ${e.message}`);
      bot.quit();
      return 1;
    }
  }

  await log('INFO', `uygulandı: ${done} işlem`);
  if (VERIFY) {
    bot.chat('/save-all flush');
    await sleep(2000);
    await log('INFO', 'dünya kaydedildi — doğrulamak için: npm run verify');
  }
  bot.quit();
  return 0;
}

main()
  .then((c) => process.exit(c ?? 0))
  .catch(async (e) => {
    await log('ERROR', e.stack || e.message);
    process.exit(1);
  });
