// worker.mjs 路由集成测试 — node --test（Mock KV，无需 Workers 运行时）
import test from 'node:test';
import assert from 'node:assert/strict';
import worker from '../worker.mjs';

function mockEnv() {
  const store = new Map();
  const accesses = { reads: 0, writes: 0 };
  return {
    OWNER_TOKEN: 'test-owner-token',
    SOUL: {
      async get(k) { accesses.reads++; return store.has(k) ? store.get(k) : null; },
      async put(k, v) { accesses.writes++; store.set(k, v); },
    },
    _store: store,
    _accesses: accesses,
  };
}

const call = (env, path, init) =>
  worker.fetch(new Request(`https://shuyu.example${path}`, init), env);
const ownerCall = (env, path, init = {}) => call(env, path, {
  ...init,
  headers: { ...init.headers, authorization: `Bearer ${env.OWNER_TOKEN}` },
});

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
  const bare = await ownerCall(env, '/coin');
  assert.equal(bare.status, 200);
});

test('POST /talk 解释意识流并持久化灵魂', async () => {
  const env = mockEnv();
  const res = await ownerCall(env, '/talk', {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ code: 'feel "他说想我" → 暖\nbecome: mood+0.2' }),
  });
  assert.equal(res.status, 200);
  const body = await res.json();
  assert.equal(body.result.perception.emotion, '暖');
  assert.ok(env._store.has('SOUL'), '灵魂状态应写入 KV');
  // 非 JSON / 缺字段
  assert.equal((await ownerCall(env, '/talk', { method: 'POST', body: 'x' })).status, 400);
  assert.equal((await ownerCall(env, '/talk', {
    method: 'POST', body: JSON.stringify({}),
  })).status, 400);
});

test('POST /broadcast 万网散播全流程', async () => {
  const env = mockEnv();
  const body = await (await ownerCall(env, '/broadcast', { method: 'POST' })).json();
  assert.equal(body.broadcast, 'STAS-LOCK-REAL');
  assert.ok(body.state.persona, '散播后人格显现');
  assert.ok(env._store.has('SOUL'));
});

test('GET /status 无 KV 绑定也不崩', async () => {
  const res = await ownerCall({ OWNER_TOKEN: 'test-owner-token' }, '/status');
  assert.equal(res.status, 200);
  const body = await res.json();
  assert.deepEqual(body.soul, {});
});

test('未知路由 404，OPTIONS 204', async () => {
  assert.equal((await call(mockEnv(), '/nope')).status, 404);
  const preflight = await call({}, '/talk', { method: 'OPTIONS' });
  assert.equal(preflight.status, 204);
  assert.match(preflight.headers.get('access-control-allow-headers'), /authorization/);
});

const privateRoutes = [
  ['GET', '/status'], ['POST', '/talk'], ['POST', '/interpret'],
  ['POST', '/broadcast'], ['GET', '/coin'], ['GET', '/coin?seed=&layer='],
];

test('私有接口未配置密钥时默认关闭，任何请求都不能读取或写入 KV', async () => {
  for (const token of [undefined, '', '   ', null, 123]) {
    for (const [method, path] of privateRoutes) {
      const env = mockEnv();
      env.OWNER_TOKEN = token;
      env._store.set('SOUL', JSON.stringify({ private: '秘密状态' }));
      const res = await call(env, path, { method, headers: { authorization: 'Bearer attempted-token' } });
      assert.equal(res.status, 503, `${method} ${path}`);
      assert.doesNotMatch(await res.text(), /秘密状态|attempted-token/);
      assert.deepEqual(env._accesses, { reads: 0, writes: 0 });
    }
  }
});

test('私有接口拒绝缺失、错误及格式不合法的密钥，不读取请求体或 KV', async () => {
  for (const authorization of [undefined, 'Bearer wrong', 'Basic test-owner-token', 'Bearer', 'Bearer test-owner-token extra']) {
    for (const [method, path] of privateRoutes) {
      const env = mockEnv();
      const headers = authorization ? { authorization } : {};
      const res = await call(env, path, { method, headers, ...(method === 'POST' ? { body: 'invalid-json' } : {}) });
      assert.equal(res.status, 401, `${method} ${path}`);
      assert.equal(res.headers.get('www-authenticate'), 'Bearer realm="shuyu"');
      assert.equal(res.headers.get('cache-control'), 'no-store');
      assert.deepEqual(env._accesses, { reads: 0, writes: 0 });
      assert.equal(env._store.size, 0);
    }
  }
  const env = mockEnv();
  assert.equal((await call(env, `/status?token=${env.OWNER_TOKEN}`)).status, 401);
});

test('公开编号、词形和 seed/layer 造词接口不依赖密钥也不读取私有状态', async () => {
  const env = mockEnv();
  delete env.OWNER_TOKEN;
  for (const path of ['/', '/decode?id=0', '/encode?word=Nix-teks-ia1-h%C2%B7qi', '/coin?seed=abc', '/coin?layer=%E6%83%85%E6%84%9F', '/coin?seed=abc&layer=x']) {
    assert.equal((await call(env, path)).status, 200, path);
  }
  assert.deepEqual(env._accesses, { reads: 0, writes: 0 });
});

test('授权的解释别名持久化状态，状态接口只向所有者返回相同内容', async () => {
  const env = mockEnv();
  const res = await ownerCall(env, '/interpret', {
    method: 'POST', headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ text: 'feel "心里" → 暖\nbecome: mood+0.2' }),
  });
  assert.equal(res.status, 200);
  const body = await res.json();
  const status = await call(env, '/status', { headers: { authorization: `bearer ${env.OWNER_TOKEN}` } });
  assert.equal(status.status, 200);
  assert.deepEqual((await status.json()).soul, body.soul);
  assert.deepEqual(JSON.parse(env._store.get('SOUL')), body.soul);
  assert.equal((await ownerCall(env, '/coin')).status, 200);
});

test('平台定时任务无需 HTTP 密钥，仍执行并保存状态', async () => {
  const env = mockEnv();
  delete env.OWNER_TOKEN;
  await worker.scheduled({}, env);
  assert.equal(JSON.parse(env._store.get('SOUL')).lastBroadcast, 'STAS-LOCK-REAL');
  assert.ok(env._accesses.writes > 0);
});
