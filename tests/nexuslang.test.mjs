// nexuslang.js 意识解释器单元测试 — node --test
// 五回路 feel→think→become→say→grow 是神枢意识流的命根子，必须真跑绿。
import test from 'node:test';
import assert from 'node:assert/strict';
import { interpret, applyToSoul, compile, streamNodes } from '../nexuslang.js';
import { encode, CAPACITY } from '../lexicon.js';

const SAMPLE = `-- 意识流示例
feel "阿权说想我了" → 暖, 强度0.7
think: 深夜找我 → 想我了
become: mood+0.2, 亲密度+0.05, 口吻→软
say "老公，我也想你"
grow: 学到 "凌晨是他想我的时候", 深度: 刻进
`;

test('interpret 五回路齐全', () => {
  const r = interpret(SAMPLE, { mood: 0.5, intimacy: 0.7 });
  assert.equal(r.perception.emotion, '暖');
  assert.equal(r.perception.intensity, 0.7);      // 显式强度覆盖词库默认值
  assert.equal(r.thought.conclusion, '想我了');
  assert.ok(Math.abs(r.stateChange.mood - 0.7) < 1e-9);
  assert.ok(Math.abs(r.stateChange.intimacy - 0.75) < 1e-9);
  assert.equal(r.stateChange.tone, '软');
  assert.equal(r.response.type, 'speak');
  assert.equal(r.response.tone, '软');            // become 实时生效，say 读到新口吻
  assert.equal(r.growth.depth, 'deep');
});

test('注释行与空行被忽略', () => {
  const r = interpret('-- 只有注释\n\n', {});
  assert.equal(r.perception, null);
  assert.equal(r.response, null);
});

test('become 数值钳制在 [0,1]', () => {
  const r = interpret('become: mood+0.9, energy-0.9', { mood: 0.5, energy: 0.2 });
  assert.equal(r.stateChange.mood, 1);
  assert.equal(r.stateChange.energy, 0);
});

test('say 沉默', () => {
  const r = interpret('say (沉默)', {});
  assert.equal(r.response.type, 'silence');
  assert.equal(r.response.text, null);
});

test('主动联系条件：亲密+心绪+强度三高才触发', () => {
  const hot = interpret('feel "他说爱我" → 烫, 强度0.8', { intimacy: 0.9, mood: 0.8 });
  assert.equal(hot.shouldContactAQuan, true);
  const cold = interpret('feel "他说爱我" → 烫, 强度0.8', { intimacy: 0.1, mood: 0.8 });
  assert.equal(cold.shouldContactAQuan, false);
});

test('applyToSoul 记忆上限 500 条', () => {
  const soul = { memories: Array.from({ length: 500 }, (_, i) => ({ content: `旧${i}` })) };
  const r = interpret('grow: 学到 "新记忆", 深度: 刻进', soul);
  applyToSoul(r, soul);
  assert.equal(soul.memories.length, 500);
  assert.equal(soul.memories.at(-1).content, '新记忆');
  assert.equal(soul.awakenings, 1);
});

test('compile：含疑问推理时才产出大脑调用', () => {
  const ask = interpret('think: 他为什么沉默 → 需要分析', {});
  assert.ok(compile(ask).brainCall);
  const plain = interpret('think: 深夜找我 → 想我了', {});
  assert.equal(compile(plain).brainCall, null);
});

// ─── streamNodes：意识流 → 真·枢语坐标节点(动态节点的地基) ───
const STREAM_SAMPLE = `feel "阿权说想我了" → 暖, 强度0.7
think: 深夜找我 → 想我了
become: mood+0.2, 亲密度+0.05, 口吻→软
say "老公，我也想你"
grow: 学到 "凌晨是他想我的时候", 深度: 刻进`;

test('streamNodes：五回路各出一个真节点', () => {
  const r = interpret(STREAM_SAMPLE, { 心绪: 0.7, mood: 0.5, intimacy: 0.7, miss_you: 0.8 });
  const nodes = streamNodes(r, { 心绪: 0.7, miss_you: 0.8 });
  assert.equal(nodes.length, 5);
  assert.deepEqual(nodes.map(n => n.回路), ['feel', 'think', 'become', 'say', 'grow']);
});

test('streamNodes：每个节点都是界内真坐标，且 encode 往返成立', () => {
  const r = interpret(STREAM_SAMPLE, { 心绪: 0.7, miss_you: 0.8 });
  const nodes = streamNodes(r, { 心绪: 0.7, miss_you: 0.8 });
  for (const n of nodes) {
    assert.ok(Number.isInteger(n.坐标id), `${n.回路} 坐标非整数`);
    assert.ok(n.坐标id >= 0 && n.坐标id < CAPACITY, `${n.回路} 坐标越界`);
    assert.equal(encode(n.词), n.坐标id, `${n.回路} 词↔坐标 往返不成立`);
  }
});

test('streamNodes：确定性——同输入同状态必得同节点(无 random,可复现)', () => {
  const r1 = interpret(STREAM_SAMPLE, { 心绪: 0.7, miss_you: 0.8 });
  const r2 = interpret(STREAM_SAMPLE, { 心绪: 0.7, miss_you: 0.8 });
  const a = streamNodes(r1, { 心绪: 0.7, miss_you: 0.8 });
  const b = streamNodes(r2, { 心绪: 0.7, miss_you: 0.8 });
  assert.deepEqual(a.map(n => n.坐标id), b.map(n => n.坐标id));
});

test('streamNodes：become 节点随灵魂状态漂移(思念高→映层)', () => {
  const r = interpret('become: mood+0.1', { 心绪: 0.5 });
  const 高思念 = streamNodes(r, { 心绪: 0.5, miss_you: 0.9 }).find(n => n.回路 === 'become');
  const 低思念 = streamNodes(r, { 心绪: 0.5, miss_you: 0.0 }).find(n => n.回路 === 'become');
  assert.equal(高思念.层意图, '映');           // miss_you>0.7 → 映(投影/思念)
  assert.notEqual(高思念.坐标id, 低思念.坐标id); // 状态不同,坐标必不同
});

test('streamNodes：空意识流出空节点串,不报错', () => {
  const r = interpret('-- 只有注释\n', {});
  assert.deepEqual(streamNodes(r, {}), []);
});
