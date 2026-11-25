// Manifest tutarlılık denetimi (CI + yerel).
// Neden ayrı dosya: YAML içine gömülü `node -e "..."` bloğunda ${p.slug} gibi
// ifadeleri bash kendi değişkeni sanıp bozuyordu ("bad substitution").
const fs = require('fs');
const m = require('../plugins/manifest.json');

const slugs = m.plugins.map((p) => p.slug);
const dup = slugs.filter((s, i) => slugs.indexOf(s) !== i);
if (dup.length) fail('tekrarlanan slug: ' + dup.join(', '));

// '_' ile başlayanlar şablon/ortak dizin (oda değil)
const roomDirs = fs
  .readdirSync('config/rooms', { withFileTypes: true })
  .filter((d) => d.isDirectory() && !d.name.startsWith('_'))
  .map((d) => d.name);
const rooms = new Set([...roomDirs, 'all', 'proxy']);

for (const p of m.plugins) {
  for (const r of p.rooms) {
    if (!rooms.has(r)) fail(`${p.slug}: olmayan oda '${r}'`);
  }
}

const g = m.plugins.find((p) => p.slug === 'geyser');
if (!g) fail('manifest: geyser yok');
if (JSON.stringify(g.rooms) !== '["proxy"]') fail('Geyser rooms=["proxy"] olmali (backend degil)');
if (g.platform !== 'velocity') fail('Geyser platform=velocity olmali');

const mandatory = m.plugins.filter((p) => p.mandatory).length;
console.log(
  `manifest ok: ${m.plugins.length} plugin (${mandatory} zorunlu), ${m.core.length} çekirdek, ` +
    `${roomDirs.length} oda`
);

function fail(msg) {
  console.error('✗ ' + msg);
  process.exit(1);
}
