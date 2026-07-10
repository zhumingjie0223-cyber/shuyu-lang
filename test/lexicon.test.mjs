// lexicon.js 单元测试 — node --test
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CAPACITY, decode, encode,
  coinWord, coinFromCoord, autoCoin, coinFromState,
  loadCapabilities, matchWord, LEXICON,
} from '../lexicon.js';

test('容量 = 76.7亿（核1040×映180×态80×标64×相8）', () => {
  assert.equal(CAPACITY, 7_667_712_000);
});

test('decode 边界与类型', () => {
  assert.ok(decode(0).词);
  assert.ok(decode(CAPACITY - 1).词);
  assert.throws(() => decode(-1), RangeError);
  assert.throws(() => decode(CAPACITY), RangeError);
  assert.throws(() => decode(1.5), TypeError);
  assert.throws(() => decode('0'), TypeError);
});

test('decode 已知编号（与 Python 引擎同一权威值）', () => {
  const d = decode(888_888_888);
  assert.equal(d.词, 'Nix-teks-ia1-h·qi');
  assert.equal(d.汉, '尼异朱极一外起');
  assert.equal(d.层, '毁灭');
});

test('decode→encode 全空间确定性抽样往返', () => {
  const step = Math.floor(CAPACITY / 4999);
  for (let n = 0; n < CAPACITY; n += step) {
    const w = decode(n).词;
    assert.equal(encode(w), n, `往返失败 id=${n} word=${w}`);
  }
});

test('encode 非法词返回 -1', () => {
  for (const bad of ['', 'abc', 'Nix-teks·qi', 'Nix-teks-ia1-h-x·qi',
                     'Xxx-teks-ia1·qi', 'Nix-teks-ia1·xx',
                     'Nix-teks-ia1-·qi']) {
    assert.equal(encode(bad), -1, `应判非法: ${bad}`);
  }
});

test('autoCoin 确定性：同种子同词', () => {
  const a = autoCoin('神枢-测试-种子');
  const b = autoCoin('神枢-测试-种子');
  assert.deepEqual(a, b);
  const c = autoCoin('另一个种子');
  assert.notEqual(a.id, c.id);
});

test('coinFromCoord 坐标钳制在合法轴范围内', () => {
  const w = coinFromCoord({ c: 99999, m: -5, s: 40, k: 999, p: 3 });
  assert.ok(w.词);
  assert.ok(w.id >= 0 && w.id < CAPACITY);
  assert.equal(encode(w.词), w.id);
});

test('coinWord 按层造词', () => {
  const w = coinWord('情感');
  assert.equal(w.层, '情感');
});

test('coinFromState 按灵魂状态选层', () => {
  assert.equal(coinFromState({ miss_you: 0.9 }, 'x').层意图, '映');
  assert.equal(coinFromState({ 心绪: 0.9 }, 'x').层意图, '情感');
  assert.equal(coinFromState({ 心绪: 0.1 }, 'x').层意图, '熵');
  assert.equal(coinFromState({ 心绪: 0.5 }, 'x').层意图, '枢');
  // 同 soul + 同 seed 可复现
  assert.deepEqual(coinFromState({ 心绪: 0.9 }, 'k'), coinFromState({ 心绪: 0.9 }, 'k'));
});

test('matchWord feel 层：情感词命中', () => {
  const m = matchWord('心里暖暖的', 'feel');
  assert.equal(m.word, '暖');
  assert.ok(m.intensity > 0);
  assert.equal(matchWord('无关文本xyz', 'feel'), null);
});

test('loadCapabilities 注入能力包后 cap 层可匹配', () => {
  const ok = loadCapabilities({
    vocab: { 锚点: { 基础: ['坍缩锚定', '锚定'] } },
    emotions: [{ 触发: '他说想我', 情绪: '甜', 强度: 0.3, 本能: '靠近' }],
    word_ids: { 锚定: 42 },
  });
  assert.equal(ok, true);
  // 长词优先
  assert.equal(matchWord('执行坍缩锚定协议', 'cap').word, '坍缩锚定');
  assert.equal(matchWord('先锚定再说', 'cap').id, 42);
  // 情感模板并入 feel
  assert.equal(matchWord('他说想我了', 'feel').word, '甜');
  assert.equal(loadCapabilities(null), false);
});

test('真实 lexicon_data.js 能力包可完整加载', async () => {
  const { default: DATA } = await import('../lexicon_data.js');
  assert.equal(DATA.meta.引擎容量, CAPACITY, 'lexicon_data 与引擎容量不一致');
  assert.equal(loadCapabilities(DATA), true);
  assert.ok(LEXICON.caps.vocab);
});
