import test from 'node:test';
import assert from 'node:assert/strict';
import worker from '../worker.mjs';

const talk = () => new Request('https://shuyu.example/talk', {
  method: 'POST',
  body: JSON.stringify({ code: 'feel "他说想我" → 暖\nbecome: mood+0.2' }),
});

async function assertInternal(res) {
  assert.equal(res.status, 500);
  assert.equal(res.headers.get('access-control-allow-origin'), '*');
  assert.deepEqual(await res.json(), { error: '内部错误' });
}

test('状态读取失败返回脱敏错误且不覆盖数据', async () => {
  let writes = 0;
  const env = { SOUL: {
    async get() { throw new Error('不可回传的测试密钥'); },
    async put() { writes++; },
  } };
  await assertInternal(await worker.fetch(talk(), env));
  assert.equal(writes, 0);
});

for (const raw of ['', '{损坏', 'null', '[]', '1', 'true', '"文本"']) {
  test(`非法持久化状态 ${JSON.stringify(raw)} 不得被覆盖`, async () => {
    let writes = 0;
    const env = { SOUL: {
      async get() { return raw; },
      async put() { writes++; },
    } };
    await assertInternal(await worker.fetch(talk(), env));
    assert.equal(writes, 0);
  });
}

test('持久化失败由路由捕获，不返回成功或异常正文', async () => {
  let writes = 0;
  const env = { SOUL: {
    async get() { return '{}'; },
    async put() { writes++; throw new Error('不可回传的存储连接信息'); },
  } };
  await assertInternal(await worker.fetch(talk(), env));
  assert.equal(writes, 1);
});

for (const [path, method] of [['/status', 'GET'], ['/coin', 'GET'], ['/broadcast', 'POST']]) {
  test(`${method} ${path} 捕获状态读取异常`, async () => {
    let writes = 0;
    const env = { SOUL: {
      async get() { throw new Error('读取失败'); },
      async put() { writes++; },
    } };
    await assertInternal(await worker.fetch(new Request(`https://shuyu.example${path}`, { method }), env));
    assert.equal(writes, 0);
  });
}

test('定时任务读取失败向运行时报告失败且不写入', async () => {
  let writes = 0;
  const env = { SOUL: {
    async get() { throw new Error('读取失败'); },
    async put() { writes++; },
  } };
  await assert.rejects(worker.scheduled({}, env), /读取失败/);
  assert.equal(writes, 0);
});

test('缺失状态键仍可以正常初始化', async () => {
  let saved;
  const env = { SOUL: {
    async get() { return null; },
    async put(key, value) { saved = { key, value }; },
  } };
  const res = await worker.fetch(talk(), env);
  assert.equal(res.status, 200);
  assert.equal(saved.key, 'SOUL');
  assert.deepEqual(JSON.parse(saved.value), (await res.json()).soul);
});

test('未绑定存储保留原有可用行为', async () => {
  assert.equal((await worker.fetch(talk(), {})).status, 200);
});
