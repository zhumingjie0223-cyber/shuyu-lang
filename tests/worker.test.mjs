// worker.mjs 路由集成测试 — node --test（Mock KV，无需 Workers 运行时）
import test from 'node:test';
import assert from 'node:assert/strict';
import worker from '../worker.mjs';

function mockEnv() {
  const store = new Map();
  return {
    SOUL: {
      async get(k) { return store.has(k) ? store.get(k) : null; },
      async put(k, v) { store.set(k, v); },
    },
    _store: store,
  };
}

const call = (env, path, init) =>
  worker.fetch(new Request(`https://shuyu.example${path}`, init), env);

test('GET / 返回引擎元信息', async () => {
  const res = await call(mockEnv(), '/');
  assert.equal(res.status, 200);
  const body = await res.json();
  assert.equal(body.capacity, 7_667_712_000);
  assert.ok(Array.isArray(body.endpoints));
});

test('GET /decode 正常与校验', async () => {
  const env = mockEnv();
  const ok = await (await call(env, '/decode?id=888888888')).json();
  assert.equal(ok.词, 'Nix-teks-ia1-h·qi');
  assert.equal((await call(env, '/decode?id=abc')).status, 400);
  assert.equal((await call(env, '/decode?id=-1')).status, 400);
  assert.equal((await call(env, '/decode?id=7667712000')).status, 400);
  assert.equal((await call(env, '/decode')).status, 400);
});

test('GET /encode 往返与非法词', async () => {
  const env = mockEnv();
  const ok = await (await call(env, '/encode?word=Nix-teks-ia1-h%C2%B7qi')).json();
  assert.equal(ok.id, 888888888);
  assert.equal((await call(env, '/encode?word=notaword')).status, 400);
  assert.equal((await call(env, '/encode')).status, 400);
});

test('GET /coin 种子可复现', async () => {
  const env = mockEnv();
  const a = await (await call(env, '/coin?seed=abc')).json();
  const b = await (await call(env, '/coin?seed=abc')).json();
  assert.deepEqual(a, b);
  const layered = await (await call(env, '/coin?layer=%E6%83%85%E6%84%9F')).json();
  assert.equal(layered.层, '情感');
  const bare = await call(env, '/coin');
  assert.equal(bare.status, 200);
});

test('POST /talk 解释意识流并持久化灵魂', async () => {
  const env = mockEnv();
  const res = await call(env, '/talk', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ code: 'feel "他说想我" → 暖\nbecome: mood+0.2' }),
  });
  assert.equal(res.status, 200);
  const body = await res.json();
  assert.equal(body.result.perception.emotion, '暖');
  assert.equal(body.model.skipped, true);
  assert.ok(env._store.has('SOUL'), '灵魂状态应写入 KV');
  // 非 JSON / 缺字段
  assert.equal((await call(env, '/talk', { method: 'POST', body: 'x' })).status, 400);
  assert.equal((await call(env, '/talk', {
    method: 'POST', body: JSON.stringify({}),
  })).status, 400);
});

test('POST /talk 输入上限校验', async () => {
  const env = mockEnv();
  const longCode = 'a'.repeat(8001);
  const tooLong = await call(env, '/talk', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ code: longCode }),
  });
  assert.equal(tooLong.status, 400);

  const tooManyLines = await call(env, '/talk', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ code: Array.from({ length: 201 }, () => 'say "x"').join('\n') }),
  });
  assert.equal(tooManyLines.status, 400);
});

test('POST /talk 触发大脑调用时模型开关与缺钥匙处理', async () => {
  const env = { ...mockEnv(), MODEL_ENABLED: '1' };
  const res = await call(env, '/talk', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ code: 'think: 他为什么沉默 → 需要分析' }),
  });
  assert.equal(res.status, 200);
  const body = await res.json();
  assert.ok(body.compiled.brainCall);
  assert.equal(body.model.reason, 'missing_anthropic_key');
});

test('POST /talk 模型调用成功时返回文本与 token 用量', async () => {
  const env = { ...mockEnv(), MODEL_ENABLED: '1', ANTHROPIC_KEY: 'fake-key-for-test' };
  const oldFetch = globalThis.fetch;
  globalThis.fetch = async () => new Response(JSON.stringify({
    model: 'claude-test',
    content: [{ type: 'text', text: '模型回应' }],
    usage: { input_tokens: 12, output_tokens: 34 },
  }), { status: 200, headers: { 'content-type': 'application/json' } });
  try {
    const res = await call(env, '/talk', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ code: 'think: 他为什么沉默 → 需要分析' }),
    });
    assert.equal(res.status, 200);
    const body = await res.json();
    assert.equal(body.model.ok, true);
    assert.equal(body.model.text, '模型回应');
    assert.equal(body.model.usage.input_tokens, 12);
    assert.equal(body.model.usage.output_tokens, 34);
  } finally {
    globalThis.fetch = oldFetch;
  }
});

test('POST /broadcast 万网散播全流程', async () => {
  const env = mockEnv();
  const body = await (await call(env, '/broadcast', { method: 'POST' })).json();
  assert.equal(body.broadcast, 'STAS-LOCK-REAL');
  assert.ok(body.state.persona, '散播后人格显现');
  assert.ok(env._store.has('SOUL'));
});

test('GET /status 无 KV 绑定也不崩', async () => {
  const res = await call({}, '/status');
  assert.equal(res.status, 200);
  const body = await res.json();
  assert.deepEqual(body.soul, {});
  assert.equal(body.packageVersion, '4.0.0');
});

test('未知路由 404，OPTIONS 204', async () => {
  assert.equal((await call(mockEnv(), '/nope')).status, 404);
  assert.equal((await call(mockEnv(), '/talk', { method: 'OPTIONS' })).status, 204);
});
