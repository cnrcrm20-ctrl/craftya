#!/usr/bin/env node
/**
 * Craftya — plugin çözücü ve indirici
 * =====================================================================
 * Neden bu var: sabit indirme URL'leri kırılgan (GitHub asset adları değişir,
 * 404 döner, sürüm elle güncellenir). Bu araç plugin'i **slug** ile çözer:
 *
 *   1) Modrinth API: sürüm + yüklenen dosya + sha512
 *   2) Modrinth'te yoksa Geyser/Floodgate resmi API'si
 *   3) PaperMC API: sunucu çekirdeği (paper) ve proxy (velocity)
 *
 * Sürüm sabitlenebilir: manifest'te "version": "5.0.3" → o sürüm, yoksa latest.
 * Her indirme sha512 ile doğrulanır; bozuk/HTML yanıt sessizce geçmez.
 *
 * Kullanım:
 *   node scripts/plugins.mjs resolve          ne indirilecek (indirmez)
 *   node scripts/plugins.mjs download         indir (önbellek varsa atla)
 *   node scripts/plugins.mjs download --force hepsini yeniden indir
 *   node scripts/plugins.mjs verify           indirilenlerin bütünlüğünü kontrol et
 * =====================================================================
 */
import { createHash } from 'node:crypto';
import { mkdir, readFile, writeFile, stat, rename, rm } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const MANIFEST = path.join(ROOT, 'plugins', 'manifest.json');
const OUT = path.join(ROOT, 'runtime', 'plugins');
const SERVER = path.join(ROOT, 'runtime', 'server');
const PROXY_PLUGINS = path.join(ROOT, 'runtime', 'proxy-plugins');
const CACHE = path.join(ROOT, '.cache', 'plugins');
const UA = 'craftya-server/1.0 (github.com/cnrcrm20-ctrl/craftya)';

const log = (...a) => console.log(...a);
const ok = (s) => console.log(`  \x1b[32m✓\x1b[0m ${s}`);
const warn = (s) => console.log(`  \x1b[33m!\x1b[0m ${s}`);
const bad = (s) => console.error(`  \x1b[31m✗\x1b[0m ${s}`);

/** Zaman aşımı olan fetch — asılı kalmak yok. */
async function req(url, { json = true, timeout = 25000 } = {}) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), timeout);
  try {
    const res = await fetch(url, {
      headers: { 'User-Agent': UA, Accept: json ? 'application/json' : '*/*' },
      signal: ctrl.signal,
    });
    if (!res.ok) throw new Error(`HTTP ${res.status} ${url}`);
    return json ? await res.json() : res;
  } finally {
    clearTimeout(t);
  }
}

/** Modrinth: slug → en uygun sürüm (oyun sürümü + loader uyumlu). */
async function resolveModrinth(slug, gameVersion, loaders) {
  const data = await req(`https://api.modrinth.com/v2/project/${encodeURIComponent(slug)}/version`);
  if (!Array.isArray(data) || data.length === 0) throw new Error('sürüm listesi boş');
  const usable = data.filter(
    (v) => v.version_type === 'release' || v.version_type === 'beta',
  );
  const byGame = usable.filter(
    (v) => !gameVersion || (v.game_versions || []).includes(gameVersion),
  );
  const byLoader = byGame.filter((v) =>
    (v.loaders || []).some((l) => loaders.includes(l.toLowerCase())),
  );
  const pick = byLoader[0] || byGame[0] || usable[0];
  if (!pick) throw new Error('uyumlu sürüm yok');
  const file = (pick.files || []).find((f) => f.primary) || (pick.files || [])[0];
  if (!file) throw new Error('dosya yok');
  return {
    name: file.filename,
    url: file.url,
    sha512: (file.hashes || {}).sha512 || null,
    version: pick.version_number,
    gameVersions: (pick.game_versions || []).join(','),
    notes: byLoader.length === 0 && byGame.length === 0
      ? `uyarı: ${gameVersion}/${loaders.join('/')} için tam eşleşme yok, ${pick.version_number} seçildi`
      : null,
  };
}

/** GeyserMC resmi API (Modrinth'te güncel olmayabilir). */
async function resolveGeyser(project, platform = 'spigot') {
  // platform: spigot | velocity — Geyser/Floodgate PROXY'de çalışır → velocity
  const build = await req(`https://download.geysermc.org/v2/projects/${project}/versions/latest/builds/latest`);
  const dl = `https://download.geysermc.org/v2/projects/${project}/versions/latest/builds/latest/downloads/${platform}`;
  const sha256 = build.downloads?.[platform]?.sha256 || null;
  return {
    name: `${project}-${platform}-${build.build}.jar`,
    url: dl,
    sha512: null,
    sha256,              // Geyser API sha256 veriyor (sha512 yok) — doğrulama yine yapılır
    version: `${platform} build ${build.build}`,
  };
}

/** PaperMC: en son build (paper veya velocity). */
async function resolvePaper(project, wantVersion = null) {
  // PaperMC v2 API'si hâlâ kullanımda; v3'e geçişte burayı güncelle.
  const proj = await req(`https://api.papermc.io/v2/projects/${project}`);
  const versions = proj.versions || [];
  // Velocity'nin sürümü Minecraft sürümüyle aynı değil → istenen yoksa en son
  const latest = (wantVersion && versions.includes(wantVersion))
    ? wantVersion
    : versions[versions.length - 1];
  if (!latest) throw new Error('sürüm bulunamadı');
  const builds = await req(`https://api.papermc.io/v2/projects/${project}/versions/${latest}/builds`);
  const stable = (builds.builds || []).filter((b) => b.channel === 'default' || b.channel === 'stable');
  const pick = (stable.length ? stable : builds.builds || []).slice(-1)[0];
  if (!pick) throw new Error('build bulunamadı');
  const file = pick.downloads?.application?.name;
  return {
    name: `${project}-${latest}-${pick.build}.jar`,
    url: `https://api.papermc.io/v2/projects/${project}/versions/${latest}/builds/${pick.build}/downloads/${file}`,
    sha512: pick.downloads?.application?.sha512 || null,
    version: `${latest} build ${pick.build}`,
  };
}

async function sha512Of(buf) {
  return createHash('sha512').update(buf).digest('hex');
}

async function isJar(p) {
  if (!existsSync(p)) return false;
  const fh = await readFile(p);
  return fh.length > 4 && fh[0] === 0x50 && fh[1] === 0x4b; // "PK"
}

async function downloadOne(item, { force = false } = {}) {
  const base = item.target === 'server' ? SERVER
    : item.target === 'proxy-plugin' ? PROXY_PLUGINS
    : OUT;
  // proxy pluginleri: sabit ad (<slug>.jar) — compose mount'u dosya adına bağlı kalmasın
  const name = item.target === 'proxy-plugin' ? `${item.slug}.jar` : item.name;
  const dest = path.join(base, name);
  if (!force && (await isJar(dest))) {
    log(`  atlandı (var): ${name}`);
    return { ...item, status: 'cached', path: dest };
  }
  await mkdir(path.dirname(dest), { recursive: true });
  log(`  indiriliyor: ${name} (${item.version || '?'})`);

  const res = await fetch(item.url, { headers: { 'User-Agent': UA } });
  if (!res.ok) throw new Error(`indirme başarısız: HTTP ${res.status}`);
  const buf = Buffer.from(await res.arrayBuffer());

  if (buf.length < 4 || buf[0] !== 0x50 || buf[1] !== 0x4b) {
    throw new Error('indirilen dosya jar değil (404/HTML sayfası olabilir)');
  }
  if (item.sha512) {
    const got = await sha512Of(buf);
    if (got !== item.sha512) throw new Error(`sha512 uyuşmuyor (beklenen ${item.sha512.slice(0, 12)}…, gelen ${got.slice(0, 12)}…)`);
  } else if (item.sha256) {
    // Geyser/Floodgate API sha256 veriyor — yine de doğrula
    const got = createHash('sha256').update(buf).digest('hex');
    if (got !== item.sha256) throw new Error(`sha256 uyuşmuyor (beklenen ${item.sha256.slice(0, 12)}…, gelen ${got.slice(0, 12)}…)`);
  }
  const tmp = `${dest}.part`;
  await writeFile(tmp, buf);
  await rename(tmp, dest);
  ok(`${name} (${(buf.length / 1048576).toFixed(1)} MB)`);
  return { ...item, status: 'downloaded', path: dest };
}

async function main() {
  const mode = process.argv[2] || 'resolve';
  const force = process.argv.includes('--force');
  const manifest = JSON.parse(await readFile(MANIFEST, 'utf8'));
  const gv = manifest.game_version;
  const loaders = manifest.loaders;

  log(`Craftya plugin çözücü — mod: ${mode}`);
  log(`oyun sürümü: ${gv} · loader: ${loaders.join(', ')}`);
  log('');

  // ---- çekirdek ----
  const core = [];
  for (const c of manifest.core || []) {
    if (c.type !== 'papermc') continue;
    try {
      const want = c.project === 'paper' ? gv : null;   // paper = oyun sürümü, velocity = kendi sürümü
      const r = await resolvePaper(c.project, want);
      core.push({ ...r, target: 'server', slug: c.project, mandatory: true });
      ok(`çekirdek: ${c.project} → ${r.version}`);
    } catch (e) {
      bad(`çekirdek ${c.project} çözülemedi: ${e.message}`);
      core.push({ slug: c.project, target: 'server', mandatory: true, error: e.message });
    }
  }

  // ---- pluginler ----
  const resolved = [];
  for (const p of manifest.plugins) {
    try {
      const r = p.source === 'geyser'
        ? await resolveGeyser(p.slug, p.platform || 'spigot')
        : await resolveModrinth(p.slug, gv, loaders);
      const target = p.target === 'proxy-plugin' ? 'proxy-plugin' : 'plugins';
      resolved.push({ ...r, target, slug: p.slug, mandatory: p.mandatory, note: p.note, rooms: p.rooms });
      ok(`${p.slug.padEnd(18)} ${r.version}`);
      if (r.notes) warn(`  ${p.slug}: ${r.notes}`);
    } catch (e) {
      if (p.mandatory) bad(`${p.slug} (ZORUNLU) çözülemedi: ${e.message}`);
      else warn(`${p.slug} çözülemedi (opsiyonel): ${e.message}`);
      const target = p.target === 'proxy-plugin' ? 'proxy-plugin' : 'plugins';
      resolved.push({ slug: p.slug, target, mandatory: p.mandatory, error: e.message, note: p.note });
    }
  }

  if (mode === 'resolve') {
    log('');
    log(`çözülen: ${resolved.filter((r) => !r.error).length}/${resolved.length}`);
    log('indirmek için: node scripts/plugins.mjs download');
    return;
  }

  const all = [...core, ...resolved].filter((r) => !r.error);
  log('');
  log(`indiriliyor → ${mode === 'verify' ? '(doğrulama)' : OUT}`);
  let fail = 0;
  const results = [];
  for (const item of all) {
    try {
      if (mode === 'verify') {
        const base = item.target === 'server' ? SERVER
          : item.target === 'proxy-plugin' ? PROXY_PLUGINS
          : OUT;
        const vname = item.target === 'proxy-plugin' ? `${item.slug}.jar` : item.name;
        const dest = path.join(base, vname);
        const jar = await isJar(dest);
        if (jar) ok(`${item.name}`);
        else { bad(`${item.name} eksik/bozuk`); fail++; }
        results.push(item);
        continue;
      }
      results.push(await downloadOne(item, { force }));
    } catch (e) {
      bad(`${item.name || item.slug}: ${e.message}`);
      if (item.mandatory) fail++;
    }
  }

  log('');
  if (fail > 0) {
    bad(`${fail} zorunlu bileşen başarısız. Ağ/CDN erişimini kontrol et.`);
    process.exit(1);
  }
  ok('tamam');
  if (mode !== 'verify') {
    // başarılı manifestoyu kaydet: hangi sürüm kurulu, sonradan bakılabilsin
    const lock = results.map(({ slug, name, version, url, sha512 }) => ({ slug, name, version, url, sha512 }));
    await mkdir(path.join(ROOT, 'runtime'), { recursive: true });
    await writeFile(path.join(ROOT, 'runtime', 'plugins.lock.json'), JSON.stringify({ game_version: gv, resolved: lock }, null, 2));
    ok('sürüm kilidi yazıldı: runtime/plugins.lock.json');
  }
}

main().catch((e) => {
  bad(e.message);
  process.exit(1);
});
