// Cloudflare Worker(worker.mjs)路由测试:用内存 KV 桩模拟 env.SOUL
import test from 'node:test';
import assert from 'node:assert/strict';
import worker from '../worker.mjs';

function memKV() {
  const store = new Map();
  return {
    store,
    async get(k) { return store.has(k) ? store.get(k) : null; },
    async put(k, v) { store.set(k, v); },
  };
}

test('/status:冷启动返回只读快照,不炸不写', async () => {
  const env = { SOUL: memKV() };
  const res = await worker.fetch(new Request('http://shu.local/status'), env);
  assert.equal(res.headers.get('content-type'), 'application/json');
  const s = await res.json();
  assert.equal(s.code, 'SHU-STATUS');
  assert.equal(s.mode, null);
  assert.equal(env.SOUL.store.size, 0, 'status 不应写 KV');
});

test('/broadcast:全网同步 + 固化边界,灵魂落盘', async () => {
  const env = { SOUL: memKV() };
  const res = await worker.fetch(new Request('http://shu.local/broadcast'), env);
  const r = await res.json();
  assert.equal(r.code, 'STAS-LOCK-REAL');
  assert.equal(r.syncRate, 1.0);
  assert.equal(r.entropy, 0);
  assert.equal(r.realityLocked, true);
  assert.ok(env.SOUL.store.has('SOUL'), '广播后灵魂应已落盘');
});

test('/talk:三步坍缩去噪后回一个确定性枢语词', async () => {
  const env = { SOUL: memKV() };
  const req = new Request('http://shu.local/talk', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ text: '老婆在吗' }),
  });
  const r = await (await worker.fetch(req, env)).json();
  assert.equal(r.input, '老婆在吗');
  assert.match(r.词, /^[A-Za-z0-9-]+·[a-z]+$/);
  assert.ok(r.汉.length > 0);
  assert.equal(typeof r.id, 'number');
  // 同样输入必得同一个词(确定性)
  const req2 = new Request('http://shu.local/talk', {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ text: '老婆在吗' }),
  });
  const r2 = await (await worker.fetch(req2, env)).json();
  assert.equal(r2.id, r.id);
});

test('/talk 用 GET 打不进对话路由,落到默认响应', async () => {
  const env = { SOUL: memKV() };
  const r = await (await worker.fetch(new Request('http://shu.local/talk'), env)).json();
  assert.equal(r.code, 'SHU-GEN-SYNC');
});

test('scheduled:主权脉冲 + 广播全程可跑,状态收敛', async () => {
  const env = { SOUL: memKV() };
  await worker.scheduled({}, env);
  const soul = JSON.parse(env.SOUL.store.get('SOUL'));
  assert.equal(soul.mode, 'SOVEREIGN');
  assert.equal(soul.entropy, 0);
  assert.ok(soul.birth > 0);
});

test('无 KV 绑定时全部路由降级可用(纯内存,不抛错)', async () => {
  for (const path of ['/status', '/broadcast']) {
    const res = await worker.fetch(new Request(`http://shu.local${path}`), {});
    assert.equal(res.status, 200, `${path} 无 KV 时应降级可用`);
  }
});
