#!/usr/bin/env node
/**
 * verify.js — plan gerçekten uygulandı mı?
 *
 * scan.json (mevcut durum) ile plan.json (istenen) karşılaştırılır.
 * Blok karşılaştırması için yeniden tarama yapılır → yalan söylemez.
 *
 * Örnek:
 *   node src/verify.js                       son taramayı plana göre kontrol et
 *   node src/verify.js --rescan              önce yeniden tara, sonra kontrol et
 */
import { readFile } from 'node:fs/promises';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import path from 'node:path';
import { loadConfig, log, ROOT } from './lib.js';

const run = promisify(execFile);
const RESCAN = process.argv.includes('--rescan');

async function main() {
  const cfg = await loadConfig();
  const planPath = path.join(ROOT, cfg.apply?.planPath ?? 'out/plan.json');
  const scanPath = path.join(ROOT, cfg.scan?.savePath ?? 'out/scan.json');

  if (RESCAN) {
    await log('INFO', 'yeniden taranıyor...');
    await run('node', ['src/scan.js'], { cwd: path.join(ROOT, '.') });
  }

  let plan, scan;
  try {
    plan = JSON.parse(await readFile(planPath, 'utf8'));
    scan = JSON.parse(await readFile(scanPath, 'utf8'));
  } catch (e) {
    throw new Error(`dosya okunamadı: ${e.message}`);
  }

  // dünyada ne var: koordinat → blok
  const world = new Map();
  for (const [x, y, z, name] of scan.blocks || []) world.set(`${x},${y},${z}`, name);

  // planda ne olmalı
  const expected = [];
  for (const op of plan.ops || []) {
    if (op.op === 'block') expected.push({ pos: op.pos, want: op.block.replace(/^minecraft:/, ''), kind: 'blok' });
    if (op.op === 'npc') expected.push({ pos: op.pos, want: null, kind: 'npc', name: op.name });
  }

  const npcs = (scan.entities || []).filter((e) => ['player', 'villager', 'zombie', 'armor_stand'].includes(e.kind));

  let okCount = 0;
  const missing = [];

  for (const exp of expected) {
    if (exp.kind === 'blok') {
      const got = world.get(exp.pos.join(','));
      // tarama seyrek (step>1) → yakın komşulukta da kabul
      const near = got === exp.want || [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]]
        .some((d) => world.get([exp.pos[0] + d[0], exp.pos[1] + d[1], exp.pos[2] + d[2]].join(',')) === exp.want);
      if (near) okCount++;
      else missing.push({ ...exp, got: got ?? '(hava/boşluk)' });
    } else {
      const found = npcs.some((n) => {
        const d = Math.hypot(n.pos[0] - exp.pos[0], n.pos[1] - exp.pos[1], n.pos[2] - exp.pos[2]);
        return d <= 6;
      });
      if (found) okCount++;
      else missing.push({ ...exp, got: '(yakında NPC yok)' });
    }
  }

  const total = expected.length;
  const pct = total ? Math.round((okCount / total) * 100) : 100;

  console.log('');
  console.log('DOĞRULAMA');
  console.log(`  beklenen öğe : ${total}`);
  console.log(`  doğru        : ${okCount} (%${pct})`);
  console.log(`  eksik        : ${missing.length}`);
  if (missing.length) {
    console.log('');
    console.log('  eksikler (ilk 15):');
    for (const m of missing.slice(0, 15)) {
      console.log(`    ${m.kind.padEnd(4)} @ ${m.pos.join(',').padEnd(14)} beklenen=${m.want ?? m.name ?? '-'}  bulunan=${m.got}`);
    }
    if (missing.length > 15) console.log(`    ... ve ${missing.length - 15} tane daha`);
  }
  console.log('');
  console.log('  dünya özeti:');
  console.log(`    entity: ${(scan.entities || []).length}, tabela: ${(scan.signs || []).length}`);
  console.log(`    blok tipi: ${(scan.topBlocks || []).length} çeşit`);

  await log('INFO', `doğrulama: ${okCount}/${total} (%${pct})`);
  return missing.length === 0 ? 0 : 2;
}

main()
  .then((c) => process.exit(c ?? 0))
  .catch(async (e) => {
    await log('ERROR', e.stack || e.message);
    process.exit(1);
  });
