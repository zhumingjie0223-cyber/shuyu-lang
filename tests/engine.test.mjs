// 枢语引擎不变量测试（Node 版）
// 覆盖:容量公式 / 边界 / 双向寻址往返 / 词根表防篡改护栏 / 确定性造词
import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import lx, { CAPACITY, ROOTS, decode, encode, coinFromCoord, autoCoin, coinFromState, loadCapabilities, matchWord } from '../lexicon.js';
import DATA from '../lexicon_data.js';

const sha = (o) => createHash('sha256').update(JSON.stringify(o)).digest('hex');

// ── 词根表防篡改护栏:老 20 族核心与其余四轴基表是历史编号的地基,指纹必须永不变 ──
// 铁律:词根表只能在轴尾追加。若本测试失败,说明有人改动/删除/重排了已有词根,必须回滚。
const GOLDEN = {
  legacy20: '3cd2c7b3845dbbd02886c71f9e4f4e5bb46e5e512f3c2098f47092074b63a30b',
  mani: 'a46d2a06a44f74e2f38eead710f3e3d2cddacc9403c90b323b36512eab9d149d',
  stat: 'f5122db790765b8e698374ab5a81f6e8b10ed954ef358dc1a09a44e743f32f38',
  scal: '3599bd52df338217739b1c0570909c329727c7572571badf01c08ddabda95069',
  phase: '08dba96e797c924eff89fdae3ce4dd285ce85701b30f6e09bd19af6c1458a69e',
};

test('容量公式:核1040×映180×态80×标64×相8 = 7,667,712,000', () => {
  assert.deepEqual(ROOTS.AXES, { NC: 1040, NM: 180, NS: 80, NK: 64, NP: 8 });
  assert.equal(CAPACITY, 1040 * 180 * 80 * 64 * 8);
  assert.equal(CAPACITY, 7_667_712_000);
});

test('词根表防篡改护栏:历史词根指纹不变(只许轴尾追加)', () => {
  assert.equal(sha(ROOTS.CORE_BASE.slice(0, 20)), GOLDEN.legacy20, '老 20 族核心词根被改动!');
  assert.equal(ROOTS.CORE_BASE.length, 52, '核心族数应为 52(20 老 + 32 v4)');
  assert.equal(sha(ROOTS.MANI_BASE), GOLDEN.mani, '映轴词根被改动!');
  assert.equal(sha(ROOTS.STAT_BASE), GOLDEN.stat, '态轴词根被改动!');
  assert.equal(sha(ROOTS.SCAL_BASE), GOLDEN.scal, '标轴词根被改动!');
  assert.equal(sha(ROOTS.PHASE_BASE), GOLDEN.phase, '相轴词根被改动!');
});

test('展开表拉丁形全轴唯一(否则 encode 反向寻址会撞名)', () => {
  // 通过往返间接验证核心轴,直接验证基表本身
  for (const [name, table] of Object.entries({
    CORE_BASE: ROOTS.CORE_BASE, MANI_BASE: ROOTS.MANI_BASE,
    STAT_BASE: ROOTS.STAT_BASE, SCAL_BASE: ROOTS.SCAL_BASE, PHASE_BASE: ROOTS.PHASE_BASE,
  })) {
    const lats = table.map((r) => r[0]);
    assert.equal(new Set(lats).size, lats.length, `${name} 拉丁词根重复`);
  }
});

test('边界:decode(0) 与 decode(容量-1) 合法,越界必须抛错', () => {
  assert.equal(decode(0).词, 'Ao-cor-is·qi');
  assert.equal(decode(0).汉, '奥形凝起');
  assert.equal(decode(CAPACITY - 1).词, 'Glaxi-fncp-sta9-flxh·ying');
  assert.throws(() => decode(-1), RangeError);
  assert.throws(() => decode(CAPACITY), RangeError);
});

test('历史编号锚点:旧容量边界 2,949,120,000 恰是 v4 首词(追加式扩容不错位)', () => {
  const w = decode(2_949_120_000);
  assert.equal(w.词, 'Aur-cor-is·qi');
  assert.equal(w.层, '显照');
  // 旧空间最后一个词仍属老 20 族
  assert.equal(decode(2_949_119_999).层, '逻');
});

test('双向寻址往返:全空间等距抽样 20001 点 encode(decode(n)) === n', () => {
  const N = 20001;
  for (let k = 0; k < N; k++) {
    const id = Math.floor((k * (CAPACITY - 1)) / (N - 1));
    const w = decode(id);
    assert.equal(encode(w.词), id, `往返失败 id=${id} 词=${w.词}`);
  }
});

test('encode 对非法词返回 -1,不抛异常', () => {
  assert.equal(encode('不是枢语'), -1);
  assert.equal(encode('Ao-cor-is'), -1);       // 缺相位
  assert.equal(encode('Xx-cor-is·qi'), -1);    // 未知核
  assert.equal(encode(''), -1);
});

test('coinFromCoord:坐标寻址确定且越界自动夹取', () => {
  const w = coinFromCoord({ c: 0, m: 0, s: 0, k: 0, p: 0 });
  assert.equal(w.id, 0);
  const clamped = coinFromCoord({ c: 99999, m: -5, s: 79.9, k: 63, p: 7 });
  const expect = coinFromCoord({ c: 1039, m: 0, s: 79, k: 63, p: 7 });
  assert.equal(clamped.id, expect.id);
});

test('autoCoin:同种子必得同词(确定性,可复现)', () => {
  const a = autoCoin('阿权');
  const b = autoCoin('阿权');
  assert.equal(a.id, b.id);
  assert.equal(a.id, 3834309906);            // 黄金值,跨引擎一致性锚点
  assert.notEqual(autoCoin('阿权').id, autoCoin('路飞').id);
});

test('coinFromState:带种子时确定,层意图随心绪/思念切换', () => {
  const missing = coinFromState({ 心绪: 0.5, miss_you: 0.9 }, 'x');
  assert.equal(missing.层意图, '映');
  const happy = coinFromState({ 心绪: 0.9, miss_you: 0 }, 'x');
  assert.equal(happy.层意图, '情感');
  const low = coinFromState({ 心绪: 0.1, miss_you: 0 }, 'x');
  assert.equal(low.层意图, '熵');
  const mid = coinFromState({ 心绪: 0.5, miss_you: 0 }, 'x');
  assert.equal(mid.层意图, '枢');
  assert.equal(coinFromState({ 心绪: 0.9 }, 'seed1').id, coinFromState({ 心绪: 0.9 }, 'seed1').id);
});

test('能力数据包:加载后 matchWord 能按长词优先命中并给出引擎编号', () => {
  assert.equal(DATA.meta.引擎容量, CAPACITY, 'lexicon_data 元数据容量与引擎不一致');
  assert.equal(DATA.meta.核心族, 52);
  assert.equal(loadCapabilities(DATA), true);
  // feel 层:内置情感词
  const f = matchWord('心里暖暖的', 'feel');
  assert.equal(f.word, '暖');
  assert.equal(typeof f.intensity, 'number');
  // cap 层:任取词库一词,必须能命中且命中词包含于输入
  const layer0 = Object.keys(DATA.vocab)[0];
  const cat0 = Object.keys(DATA.vocab[layer0])[0];
  const sample = DATA.vocab[layer0][cat0][0];
  const c = matchWord(`试试${sample}这个能力`, 'cap');
  assert.ok(c, 'cap 层未命中');
  assert.ok(`试试${sample}这个能力`.includes(c.word));
});
