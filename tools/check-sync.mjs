#!/usr/bin/env node
// 双仓同步校验:枢语源头(本仓) ↔ Black God 消费方(web/nexus-do 内嵌副本)
// 用法: node tools/check-sync.mjs [black-god仓路径]   默认依次尝试 ../Black-God、../black-god
// 校验项:容量 / 五轴词根表逐条 / 抽样 decode 逐字段 / 接口签名 / 能力数据包元数据
// 任何一项失配即退出码 1(发版联动审计的硬门禁)
import { existsSync } from 'node:fs';
import { resolve, join } from 'node:path';
import { pathToFileURL } from 'node:url';

const HERE = resolve(new URL('..', import.meta.url).pathname);
const argPath = process.argv[2];
const candidates = argPath ? [argPath] : [join(HERE, '../Black-God'), join(HERE, '../black-god')];
const bgRoot = candidates.map((p) => resolve(p)).find((p) => existsSync(join(p, 'web/nexus-do/lexicon.js')));

if (!bgRoot) {
  console.error('✗ 找不到 Black God 仓(web/nexus-do/lexicon.js),请传路径:node tools/check-sync.mjs <路径>');
  process.exit(2);
}

const src = (await import(pathToFileURL(join(HERE, 'lexicon.js')))).default;
const dst = (await import(pathToFileURL(join(bgRoot, 'web/nexus-do/lexicon.js')))).default;
const srcData = (await import(pathToFileURL(join(HERE, 'lexicon_data.js')))).default;
const dstDataPath = join(bgRoot, 'web/nexus-do/lexicon_data.js');
const dstData = existsSync(dstDataPath) ? (await import(pathToFileURL(dstDataPath))).default : null;

let failed = 0;
const ok = (msg) => console.log(`  ✓ ${msg}`);
const bad = (msg) => { failed++; console.error(`  ✗ ${msg}`); };

console.log(`双仓同步校验:${HERE}(源头) ↔ ${bgRoot}(消费方)\n`);

// ① 容量
console.log('① 引擎容量');
src.CAPACITY === dst.CAPACITY
  ? ok(`容量一致:${src.CAPACITY.toLocaleString('zh-CN')}`)
  : bad(`容量失配:源头 ${src.CAPACITY} vs 消费方 ${dst.CAPACITY}`);

// ② 五轴词根表
console.log('② 五轴词根表');
if (!dst.ROOTS) {
  bad('消费方副本缺少 ROOTS 导出(副本过旧),请把源头 lexicon.js 同步过去');
} else {
  for (const axis of ['CORE_BASE', 'MANI_BASE', 'STAT_BASE', 'SCAL_BASE', 'PHASE_BASE']) {
    const a = JSON.stringify(src.ROOTS[axis]);
    const b = JSON.stringify(dst.ROOTS[axis]);
    a === b
      ? ok(`${axis} 一致(${src.ROOTS[axis].length} 条)`)
      : bad(`${axis} 失配:源头 ${src.ROOTS[axis].length} 条 vs 消费方 ${dst.ROOTS[axis].length} 条(内容有差异)`);
  }
  JSON.stringify(src.ROOTS.AXES) === JSON.stringify(dst.ROOTS.AXES)
    ? ok(`轴长一致 ${JSON.stringify(src.ROOTS.AXES)}`)
    : bad('轴长失配');
}

// ③ 抽样 decode 逐字段
console.log('③ 抽样解码(边界 + 等距 500 点)');
{
  const ids = [0, 1, 2_949_119_999, 2_949_120_000, src.CAPACITY - 1];
  for (let k = 0; k < 500; k++) ids.push(Math.floor((k * (src.CAPACITY - 1)) / 499));
  let diff = 0;
  for (const id of new Set(ids)) {
    let a, b;
    try { a = src.decode(id); b = dst.decode(id); }
    catch (err) { diff++; bad(`id=${id} 解码抛错:${err.message}`); continue; }
    for (const f of ['词', '汉', '层', '义']) {
      if (a[f] !== b[f]) { diff++; bad(`id=${id} 字段「${f}」失配:${a[f]} vs ${b[f]}`); }
    }
    if (dst.encode(a.词) !== id) { diff++; bad(`id=${id} 消费方反向寻址失败`); }
  }
  if (!diff) ok('抽样解码逐字段一致,双向寻址往返成立');
}

// ④ 接口签名
console.log('④ 接口签名(Black God 消费的调用面)');
for (const fn of ['decode', 'encode', 'matchWord', 'coinWord', 'coinFromCoord', 'autoCoin', 'coinFromState', 'loadCapabilities']) {
  typeof dst[fn] === 'function' ? ok(`${fn} 存在`) : bad(`消费方缺接口 ${fn}`);
}

// ⑤ 能力数据包元数据
// 规则:引擎容量与自洽性是硬门禁;词库内容差异只做警告(消费方允许按人设本地裁剪能力包)
console.log('⑤ 能力数据包(lexicon_data.js)');
const countWords = (d) => {
  let n = 0;
  for (const l of Object.keys(d.vocab || {})) for (const c of Object.keys(d.vocab[l])) n += d.vocab[l][c].length;
  return n;
};
if (!dstData) {
  bad('消费方缺 lexicon_data.js');
} else {
  for (const [name, data, engine] of [['源头', srcData, src], ['消费方', dstData, dst]]) {
    data.meta.引擎容量 === engine.CAPACITY
      ? ok(`${name} meta.引擎容量 与引擎一致:${engine.CAPACITY}`)
      : bad(`${name} meta.引擎容量 ${data.meta.引擎容量} 与引擎 ${engine.CAPACITY} 失配`);
    const actual = countWords(data);
    data.meta.能力总数 === actual
      ? ok(`${name} meta.能力总数 自洽:${actual}`)
      : bad(`${name} meta.能力总数 虚标:声称 ${data.meta.能力总数},实际 ${actual}`);
  }
  if (countWords(srcData) !== countWords(dstData)) {
    console.log(`  ⚠ 词库内容有差异(源头 ${countWords(srcData)} 词 vs 消费方 ${countWords(dstData)} 词)——消费方本地裁剪,允许但请知悉`);
  }
}

console.log(failed ? `\n✗ 同步校验失败:${failed} 处失配,禁止发版` : '\n✓ 双仓同步校验全部通过');
process.exit(failed ? 1 : 0);
