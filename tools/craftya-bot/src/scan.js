#!/usr/bin/env node
/**
 * scan.js — dünyayı JSON'a dök.
 *
 * Ne toplar:
 *   - blok haritası (seyrek: sadece "havadan farklı" bloklar)
 *   - entity listesi (NPC'ler burada: tip, isim, konum)
 *   - tabela içerikleri
 *   - spawn noktası, oyuncu konumu, chunk sınırları
 *
 * Ne toplamaz: dokular, ışık, partikül (mineflayer görmez zaten).
 *
 * Örnek:  node src/scan.js --radius 96 --step 4
 */
import { writeFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { connect, loadConfig, log, shortName, prettyName, sleep, ROOT } from './lib.js';

const args = process.argv.slice(2);
const argOf = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i >= 0 && args[i + 1] ? args[i + 1] : dflt;
};

async function main() {
  const cfg = await loadConfig();
  const radius = Number(argOf('radius', cfg.scan?.radius ?? 96));
  const step = Number(argOf('step', cfg.scan?.step ?? 4));
  const minY = Number(argOf('minY', cfg.scan?.minY ?? -20));
  const maxY = Number(argOf('maxY', cfg.scan?.maxY ?? 120));

  await log('INFO', `scan başlıyor: radius=${radius} step=${step} Y=${minY}..${maxY}`);

  const bot = await connect(cfg);
  await sleep(2500); // chunk'ların gelmesini bekle

  const center = bot.entity.position.floored();

  // ---- 1) blok taraması (seyrek) -----------------------------------------
  const blocks = [];
  let scanned = 0;
  let nonAir = 0;
  const counts = {};      // blok tipi sayacı: haritanın karakterini anlamak için
  const signsFound = [];  // tabela: tarama sırasında toplanır

  for (let dx = -radius; dx <= radius; dx += step) {
    for (let dz = -radius; dz <= radius; dz += step) {
      // dikeyde daha ince: 1 blok atlama yeterli değil, 2'şer
      for (let y = minY; y <= maxY; y += 2) {
        const p = center.offset(dx, y - center.y, dz);
        const b = bot.blockAt(p);
        scanned++;
        if (!b) continue;
        if (b.name === 'air' || b.name === 'cave_air' || b.name === 'void_air') continue;
        nonAir++;
        const sn = shortName(b.name);
        counts[sn] = (counts[sn] || 0) + 1;
        blocks.push([p.x, p.y, p.z, sn]);
        if (sn.includes('sign')) {
          const lines = (b.signText || [])
            .map((l) => (l ? String(l).replace(/§./g, '').trim() : ''))
            .filter(Boolean);
          if (lines.length) signsFound.push({ pos: [p.x, p.y, p.z], lines });
        }
      }
      // ilerlemeyi göster (uzun tarama sessiz kalmasın)
      if ((dx + radius) % (step * 8) === 0) {
        await log('INFO', `tarama %${Math.round(((dx + radius) / (2 * radius)) * 100)} — ${blocks.length} blok`);
      }
    }
  }

  // ---- 2) entity'ler (NPC'ler burada) ------------------------------------
  const entities = Object.values(bot.entities || {})
    .filter((e) => e.id !== bot.entity.id)
    .map((e) => ({
      id: e.id,
      type: e.type,
      name: e.name || e.username || null,
      displayName: e.displayName ? String(e.displayName) : null,
      pos: [Math.round(e.position.x), Math.round(e.position.y), Math.round(e.position.z)],
      // NPC'ler çoğunlukla villager/zombie/player tipinde görünür (Citizens)
      kind: e.type === 'player' ? 'player' : e.type,
    }));

  // ---- 4) özet -----------------------------------------------------------
  const topBlocks = Object.entries(counts)
    .sort((a, b) => b[1] - a[1])
    .slice(0, 25)
    .map(([name, n]) => ({ name: prettyName(name), count: n, pct: +((n / nonAir) * 100).toFixed(1) }));

  const result = {
    meta: {
      scannedAt: new Date().toISOString(),
      host: `${cfg.host}:${cfg.port}`,
      center: [center.x, center.y, center.z],
      radius,
      step,
      yRange: [minY, maxY],
      scannedPoints: scanned,
      nonAirBlocks: nonAir,
      density: +((nonAir / scanned) * 100).toFixed(2),
    },
    blocks,
    entities,
    signs: signsFound,
    topBlocks,
    spawn: bot.spawnPoint
      ? [Math.round(bot.spawnPoint.x), Math.round(bot.spawnPoint.y), Math.round(bot.spawnPoint.z)]
      : null,
    // Dünya donmuş mu / boş mu, hızlı yorum için
    verdict: {
      empty: nonAir === 0,
      flatLand: (() => {
        const ys = blocks.map((b) => b[1]);
        if (ys.length < 20) return null;
        const min = Math.min(...ys);
        const max = Math.max(...ys);
        return max - min <= 3;
      })(),
      entitiesFound: entities.length,
      signsFound: signsFound.length,
    },
  };

  const outPath = path.join(ROOT, cfg.scan?.savePath ?? 'out/scan.json');
  await mkdir(path.dirname(outPath), { recursive: true });
  await writeFile(outPath, JSON.stringify(result, null, 1));

  await log('INFO', `bitti — ${blocks.length} blok, ${entities.length} entity, ${signsFound.length} tabela`);
  await log('INFO', `yazıldı: ${outPath}`);
  console.log('');
  console.log('ÖZET');
  console.log(`  taranan nokta : ${scanned}`);
  console.log(`  dolu blok     : ${nonAir} (${result.meta.density}%)`);
  console.log(`  entity (NPC?) : ${entities.length}`);
  console.log(`  tabela        : ${signsFound.length}`);
  console.log(`  boş dünya     : ${result.verdict.empty ? 'EVET (dünya üretilmemiş olabilir)' : 'hayır'}`);
  console.log(`  düz arazi     : ${result.verdict.flatLand === null ? '?' : result.verdict.flatLand ? 'evet' : 'hayır'}`);
  console.log('');
  console.log('  en çok bulunan bloklar:');
  for (const b of topBlocks.slice(0, 8)) console.log(`    ${b.name.padEnd(20)} ${b.pct}%`);

  bot.quit();
  return 0;

}

main()
  .then((code) => process.exit(code ?? 0))
  .catch(async (e) => {
    await log('ERROR', e.stack || e.message);
    process.exit(1);
  });
