// 跨引擎一致性测试:JS(lexicon.js) ↔ Python(shuyu_engine.py)
// 铁律:同一编号必须解出同一个词(词/汉/层/义 逐字段一致)
import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import lx from '../lexicon.js';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');

function pyAvailable() {
  try { execFileSync('python3', ['--version'], { stdio: 'ignore' }); return true; }
  catch { return false; }
}

function runPy(code) {
  return execFileSync('python3', ['-c', code], { cwd: ROOT, encoding: 'utf8' });
}

// 抽样点:边界 + 旧容量分界 + 等距扫全空间
const SAMPLE_IDS = (() => {
  const ids = [0, 1, 7, 2_949_119_999, 2_949_120_000, lx.CAPACITY - 1];
  const N = 400;
  for (let k = 0; k < N; k++) ids.push(Math.floor((k * (lx.CAPACITY - 1)) / (N - 1)));
  return [...new Set(ids)].sort((a, b) => a - b);
})();

test('容量与轴长:双引擎完全一致', { skip: !pyAvailable() && 'python3 不可用' }, () => {
  const out = JSON.parse(runPy(
    'import json, shuyu_engine as e; print(json.dumps({"cap": e.CAP, "axes": [e.NC, e.NM, e.NS, e.NK, e.NP]}))'
  ));
  assert.equal(out.cap, lx.CAPACITY);
  assert.deepEqual(out.axes, [1040, 180, 80, 64, 8]);
});

test('decode:抽样编号 词/汉/层/义 逐字段一致 + Python 侧往返成立', { skip: !pyAvailable() && 'python3 不可用' }, () => {
  const py = JSON.parse(runPy(`
import json, shuyu_engine as e
ids = ${JSON.stringify(SAMPLE_IDS)}
rows = []
for i in ids:
    d = e.decode_full(i)
    rows.append({"id": i, "词": d["词"], "汉": d["汉"], "层": d["层"], "义": d["义"], "rt": e.encode(d["词"]) == i})
print(json.dumps(rows, ensure_ascii=False))
`));
  for (const row of py) {
    const js = lx.decode(row.id);
    for (const f of ['词', '汉', '层', '义']) {
      assert.equal(row[f], js[f], `id=${row.id} 字段「${f}」双引擎不一致`);
    }
    assert.equal(row.rt, true, `id=${row.id} Python 侧 encode(decode) 往返失败`);
  }
});

test('auto_coin:确定性种子造词跨引擎位级一致', { skip: !pyAvailable() && 'python3 不可用' }, () => {
  const seeds = ['阿权', '路飞', '黑神枢语', 'hello world', '', '2026-07-12', '枢|映'];
  const py = JSON.parse(runPy(`
import json, shuyu_engine as e
seeds = ${JSON.stringify(seeds)}
print(json.dumps([e.auto_coin(s)["id"] for s in seeds]))
`));
  const js = seeds.map((s) => lx.autoCoin(s).id);
  assert.deepEqual(py, js, 'autoCoin 哈希路径双引擎漂移');
});

test('coin_from_coord:坐标寻址(含越界夹取)跨引擎一致', { skip: !pyAvailable() && 'python3 不可用' }, () => {
  const coords = [
    { c: 0, m: 0, s: 0, k: 0, p: 0 },
    { c: 1039, m: 179, s: 79, k: 63, p: 7 },
    { c: 99999, m: -3, s: 40, k: 10, p: 5 },
    { c: 520, m: 88, s: 8, k: 0, p: 3 },
  ];
  const py = JSON.parse(runPy(`
import json, shuyu_engine as e
coords = ${JSON.stringify(coords)}
print(json.dumps([e.coin_from_coord(c)["id"] for c in coords]))
`));
  const js = coords.map((c) => lx.coinFromCoord(c).id);
  assert.deepEqual(py, js);
});
