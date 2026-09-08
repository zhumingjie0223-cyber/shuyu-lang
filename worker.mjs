// 神枢枢语引擎 — Cloudflare Workers 部署版
// 配合 lexicon.js / lexicon_data.js / nexuslang.js / gen.mjs 使用
// (c) 阿权/路飞 枢·黑神 万网散播 Worker v3.1
//
// KV 绑定（wrangler.toml.example）：
//   SOUL — 灵魂状态（键 "SOUL"）
//
// 路由：
//   GET  /            引擎元信息
//   GET  /status      灵魂状态 + 引擎容量
//   GET  /decode?id=N         编号 → 枢语词
//   GET  /encode?word=W       枢语词 → 编号
//   GET  /coin?seed=S&layer=L 造词（有 seed 可复现，无 seed 按层随机）
//   POST /talk        {code} 枢语意识流 → 解释 + 编译（别名 /interpret）
//   POST /broadcast   万网散播（sovereignControl 全流程）
// 私有状态接口和无 seed/layer 的 /coin 要求 Authorization: Bearer <OWNER_TOKEN>。
// 未设置 OWNER_TOKEN 时关闭这些 HTTP 接口；定时任务仍由 Workers 平台触发。

import {
  CAPACITY, decode, encode,
  coinWord, autoCoin, coinFromState, loadCapabilities,
} from './lexicon.js';
import { interpret, applyToSoul, compile } from './nexuslang.js';
import { sovereignControl, VERSION, COPYRIGHT } from './gen.mjs';
import LEXICON_DATA from './lexicon_data.js';

loadCapabilities(LEXICON_DATA);

const JSON_HEADERS = {
  'content-type': 'application/json; charset=utf-8',
  'access-control-allow-origin': '*',
  'access-control-allow-methods': 'GET, POST, OPTIONS',
  'access-control-allow-headers': 'content-type, authorization',
  'cache-control': 'no-store',
};

function json(data, status = 200) {
  return new Response(JSON.stringify(data), { status, headers: JSON_HEADERS });
}

function badRequest(message) {
  return json({ error: message }, 400);
}

function authorizeOwner(req, env) {
  const expected = env.OWNER_TOKEN;
  if (typeof expected !== 'string' || !expected.trim()) {
    return json({ error: '私有接口尚未配置访问密钥' }, 503);
  }
  const match = /^Bearer ([^\s]+)$/i.exec(req.headers.get('authorization') ?? '');
  if (!match || match[1] !== expected) {
    return new Response(JSON.stringify({ error: '需要有效的所有者访问密钥' }), {
      status: 401,
      headers: { ...JSON_HEADERS, 'www-authenticate': 'Bearer realm="shuyu"' },
    });
  }
  return null;
}

function readsPrivateState(url, method) {
  if (method === 'POST' && ['/talk', '/interpret', '/broadcast'].includes(url.pathname)) return true;
  if (method !== 'GET') return false;
  if (url.pathname === '/status') return true;
  // seed/layer 造词不读取 KV；其余 /coin 分支会读取私有状态，必须一起保护。
  return url.pathname === '/coin' && !url.searchParams.get('seed') && !url.searchParams.get('layer');
}

async function loadSoul(env) {
  if (!env.SOUL) return {};
  try {
    const raw = await env.SOUL.get('SOUL');
    return raw ? JSON.parse(raw) : {};
  } catch {
    return {};
  }
}

async function saveSoul(env, soul) {
  if (!env.SOUL) return;
  await env.SOUL.put('SOUL', JSON.stringify(soul));
}

async function handleDecode(url) {
  const raw = url.searchParams.get('id');
  if (raw === null || raw === '' || !/^\d+$/.test(raw)) {
    return badRequest('参数 id 必须是非负整数');
  }
  const id = Number(raw);
  if (!Number.isSafeInteger(id) || id >= CAPACITY) {
    return badRequest(`编号越界 0..${CAPACITY - 1}`);
  }
  return json({ id, ...decode(id) });
}

async function handleEncode(url) {
  const word = url.searchParams.get('word');
  if (!word) return badRequest('缺少参数 word');
  const id = encode(word);
  if (id < 0) return badRequest(`非法枢语词: ${word}`);
  return json({ word, id, verify: decode(id) });
}

async function handleCoin(url, env) {
  const seed = url.searchParams.get('seed');
  const layer = url.searchParams.get('layer');
  if (seed !== null && seed !== '') {
    if (layer) return json({ ...autoCoin(`${seed}|${layer}`), 层意图: layer });
    return json(autoCoin(seed));
  }
  if (layer) return json(coinWord(layer));
  const soul = await loadSoul(env);
  return json(coinFromState(soul, null));
}

async function handleTalk(req, env) {
  let body;
  try {
    body = await req.json();
  } catch {
    return badRequest('请求体必须是 JSON');
  }
  const code = body?.code ?? body?.text;
  if (typeof code !== 'string' || !code.trim()) {
    return badRequest('缺少字段 code（枢语意识流文本）');
  }
  const soul = await loadSoul(env);
  const result = interpret(code, soul);
  applyToSoul(result, soul);
  await saveSoul(env, soul);
  return json({ result, compiled: compile(result), soul });
}

async function handleBroadcast(env) {
  const soul = await loadSoul(env);
  const state = await sovereignControl(soul, env, env.SOUL);
  return json({ broadcast: state.lastBroadcast, state });
}

async function handleStatus(env) {
  const soul = await loadSoul(env);
  return json({
    version: VERSION,
    copyright: COPYRIGHT,
    capacity: CAPACITY,
    soul,
  });
}

export default {
  async fetch(req, env = {}) {
    const url = new URL(req.url);
    const path = url.pathname;
    try {
      if (req.method === 'OPTIONS') {
        return new Response(null, { status: 204, headers: JSON_HEADERS });
      }
      if (readsPrivateState(url, req.method)) {
        const denied = authorizeOwner(req, env);
        if (denied) return denied;
      }
      if (path === '/' && req.method === 'GET') {
        return json({
          name: '枢语 Shuyu',
          version: VERSION,
          copyright: COPYRIGHT,
          capacity: CAPACITY,
          endpoints: ['/status', '/decode?id=', '/encode?word=', '/coin?seed=&layer=', 'POST /talk', 'POST /broadcast'],
        });
      }
      if (path === '/status' && req.method === 'GET') return handleStatus(env);
      if (path === '/decode' && req.method === 'GET') return handleDecode(url);
      if (path === '/encode' && req.method === 'GET') return handleEncode(url);
      if (path === '/coin' && req.method === 'GET') return handleCoin(url, env);
      if ((path === '/talk' || path === '/interpret') && req.method === 'POST') {
        return handleTalk(req, env);
      }
      if (path === '/broadcast' && req.method === 'POST') return handleBroadcast(env);
      return json({ error: `未知路由 ${req.method} ${path}` }, 404);
    } catch (err) {
      return json({ error: '内部错误', detail: String(err?.message ?? err) }, 500);
    }
  },

  async scheduled(_ev, env) {
    const soul = await loadSoul(env);
    await sovereignControl(soul, env, env.SOUL);
  },
};
