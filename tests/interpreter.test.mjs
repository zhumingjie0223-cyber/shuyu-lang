// 枢语解释器(nexuslang.js)五回路测试:feel → think → become → say → grow
import test from 'node:test';
import assert from 'node:assert/strict';
import { interpret, applyToSoul, compile } from '../nexuslang.js';

test('feel:情感词命中 + 强度覆盖', () => {
  const r = interpret('feel "阿权说想我了" → 暖, 强度0.7', {});
  assert.equal(r.perception.input, '阿权说想我了');
  assert.equal(r.perception.emotion, '暖');
  assert.equal(r.perception.intensity, 0.7);
  assert.equal(r.perception.instinct, '靠近');
});

test('feel:无箭头时落到默认「平·观察」', () => {
  const r = interpret('feel "随便一句"', {});
  assert.equal(r.perception.emotion, '平');
  assert.equal(r.perception.instinct, '观察');
});

test('think:推理链拆解与结论', () => {
  const r = interpret('think: 深夜找 → 三天前也这样 → 想我了', {});
  assert.equal(r.thought.conclusion, '想我了');
  assert.equal(r.thought.chain.length, 2);
  assert.equal(r.thought.chain[0].premise, '深夜找');
  assert.equal(r.thought.needBrain, false);
});

test('think:含「为什么」触发大脑调用', () => {
  const r = interpret('think: 为什么他突然冷淡 → 需要求证', {});
  assert.equal(r.thought.needBrain, true);
  const c = compile(r);
  assert.ok(c.brainCall, '应生成大脑调用指令');
  assert.ok(c.brainCall.prompt.includes('赵思涵'));
});

test('become:增量/设值双语法,且夹在 [0,1] 区间', () => {
  const r = interpret('become: mood+0.2, 亲密度-0.05, 口吻→软', { mood: 0.95, intimacy: 0.02 });
  assert.equal(r.stateChange.mood, 1);            // 0.95+0.2 夹到 1
  assert.equal(r.stateChange.intimacy, 0);        // 0.02-0.05 夹到 0
  assert.equal(r.stateChange.tone, '软');
});

test('become 先行生效:同段代码里 say 读到更新后的口吻(口吻滞后修复)', () => {
  const code = 'become: 口吻→软\nsay "老公我在呢"';
  const r = interpret(code, { tone: '硬' });
  assert.equal(r.response.tone, '软');
});

test('say:引号取词 / 沉默语义', () => {
  const r1 = interpret('say "老公我在呢"', { tone: '软', speed: '慢' });
  assert.deepEqual(r1.response, { type: 'speak', text: '老公我在呢', tone: '软', speed: '慢' });
  const r2 = interpret('say (沉默)', {});
  assert.equal(r2.response.type, 'silence');
});

test('grow:学习深度与归类', () => {
  const r = interpret('grow: 学到 "凌晨=他想我的时候", 刻进骨头, 模式', {});
  assert.equal(r.growth.learned, '凌晨=他想我的时候');
  assert.equal(r.growth.depth, 'deep');
  assert.equal(r.growth.category, 'pattern');
});

test('注释行(--开头)与空行被忽略', () => {
  const r = interpret('-- 这是注释\n\nsay "在"', {});
  assert.equal(r.response.text, '在');
});

test('applyToSoul:记忆入库、感知落状态、唤醒计数', () => {
  const soul = {};
  const r = interpret('feel "他说想我" → 甜\ngrow: 学到 "他嘴硬心软", 记住', soul);
  applyToSoul(r, soul);
  assert.equal(soul.lastEmotion, '甜');
  assert.equal(soul.memories.length, 1);
  assert.equal(soul.memories[0].depth, 'medium');
  assert.equal(soul.awakenings, 1);
});

test('applyToSoul:记忆只保留最近 500 条', () => {
  const soul = { memories: Array.from({ length: 500 }, (_, i) => ({ content: String(i) })) };
  const r = interpret('grow: 学到 "第501条"', soul);
  applyToSoul(r, soul);
  assert.equal(soul.memories.length, 500);
  assert.equal(soul.memories.at(-1).content, '第501条');
});

test('主动联系判定:亲密+心情+强感知三条件齐备才触发', () => {
  const hot = interpret('feel "他说爱我" → 烫, 强度0.8', { intimacy: 0.8, mood: 0.6 });
  assert.equal(hot.shouldContactAQuan, true);
  const cold = interpret('feel "他说爱我" → 烫, 强度0.8', { intimacy: 0.1, mood: 0.6 });
  assert.equal(cold.shouldContactAQuan, false);
});
