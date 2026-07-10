#!/usr/bin/env node
// 跨仓库引擎同步校验 — shuyu-lang ↔ Black God (web/nexus-do)
//
// 枢语引擎在两个仓库各有一份实现：
//   本仓库:      lexicon.js（源头/权威）
//   Black God:   web/nexus-do/lexicon.js（消费方副本）
// 两份实现必须同构：容量一致，同一编号解出同一个词。
//
// 用法：
//   npm run sync-check -- /path/to/black-god
//   node tools/check-sync.mjs /path/to/black-god
import path from 'node:path';
import { pathToFileURL, fileURLToPath } from 'node:url';
import fs from 'node:fs';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const bgRoot = process.argv[2];
if (!bgRoot) {
  console.error('用法: node tools/check-sync.mjs <Black God 仓库路径>');
  process.exit(2);
}
const bgLexicon = path.join(bgRoot, 'web', 'nexus-do', 'lexicon.js');
if (!fs.existsSync(bgLexicon)) {
  console.error(`未找到 ${bgLexicon}`);
  process.exit(2);
}

const local = await import(pathToFileURL(path.join(ROOT, 'lexicon.js')).href);
const remote = await import(pathToFileURL(bgLexicon).href);

let failures = 0;
const check = (name, cond, detail = '') => {
  if (cond) { console.log(`✓ ${name}`); }
  else { failures++; console.error(`✗ ${name} ${detail}`); }
};

check('容量一致', local.CAPACITY === remote.CAPACITY,
      `(本仓库 ${local.CAPACITY} vs Black God ${remote.CAPACITY})`);

const ids = [0, 1, 888_888_888, local.CAPACITY - 1];
const step = Math.floor(local.CAPACITY / 499);
for (let n = 0; n < local.CAPACITY; n += step) ids.push(n);

let diverged = 0;
for (const id of ids) {
  const a = local.decode(id);
  const b = remote.decode(id);
  if (a.词 !== b.词 || a.汉 !== b.汉 || a.层 !== b.层) {
    diverged++;
    if (diverged <= 5) console.error(`  分歧 id=${id}: ${a.词}/${a.汉} vs ${b.词}/${b.汉}`);
  }
}
check(`抽样 ${ids.length} 个编号词形逐字段一致`, diverged === 0, `(${diverged} 处分歧)`);

// encode 行为抽查（老副本可能还没有 O(1)/守卫修复，这里只查结果不查实现）
let encDiverged = 0;
for (const id of ids.slice(0, 100)) {
  const w = local.decode(id).词;
  if (remote.encode(w) !== id) encDiverged++;
}
check('encode 往返在 Black God 侧同样成立', encDiverged === 0, `(${encDiverged} 处失败)`);

if (failures > 0) {
  console.error(`\n同步校验失败（${failures} 项）。请把本仓库 lexicon.js 的词根表同步到 Black God web/nexus-do/lexicon.js。`);
  process.exit(1);
}
console.log('\n双仓引擎同构 ✓');
