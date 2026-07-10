// 跨引擎一致性测试：shuyu_engine.py ↔ lexicon.js 必须同构
// （Black God 的 web/nexus-do/lexicon.js 与本仓库 lexicon.js 同源，
//   本测试守住"同一编号在任何实现里解出同一个词"的铁律）
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { CAPACITY, decode } from '../lexicon.js';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));

function pythonDecodeAll(ids) {
  const script = [
    'import json, sys',
    'sys.path.insert(0, sys.argv[1])',
    'import shuyu_engine as e',
    'ids = json.loads(sys.argv[2])',
    'print(json.dumps({"cap": e.CAP, "words": [e.decode_full(i) for i in ids]}, ensure_ascii=False))',
  ].join('\n');
  const r = spawnSync('python3', ['-c', script, ROOT, JSON.stringify(ids)],
                      { encoding: 'utf-8', timeout: 60_000 });
  if (r.status !== 0) throw new Error(`python3 失败: ${r.stderr}`);
  return JSON.parse(r.stdout);
}

const hasPython = spawnSync('python3', ['--version']).status === 0;

test('Python ↔ JS 双引擎同构（容量 + 抽样词形逐字段一致）', { skip: !hasPython && 'python3 不可用' }, () => {
  const ids = [0, 1, 7, 888_888_888, 2_949_119_999, CAPACITY - 1];
  const step = Math.floor(CAPACITY / 199);
  for (let n = 0; n < CAPACITY; n += step) ids.push(n);

  const py = pythonDecodeAll(ids);
  assert.equal(py.cap, CAPACITY, 'Python/JS 容量不一致');
  for (const pw of py.words) {
    const jw = decode(pw.id);
    assert.equal(jw.词, pw.词, `词形分歧 id=${pw.id}`);
    assert.equal(jw.汉, pw.汉, `汉译分歧 id=${pw.id}`);
    assert.equal(jw.层, pw.层, `层名分歧 id=${pw.id}`);
  }
});
