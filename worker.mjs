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
  'access-control-allow-headers': 'content-type',
};

const MAX_TALK_CHARS = 8_000;
const MAX_TALK_LINES = 200;
const MODEL_TIMEOUT_MS = 8_000;

function json(data, status = 200) {
  return new Response(JSON.stringify(data), { status, headers: JSON_HEADERS });
}

function badRequest(message) {
  return json({ error: message }, 400);
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

function countLines(text) {
  if (!text) return 0;
  return text.split('\n').length;
}

async function applyResultAndSaveSoul(env, result) {
  const latest = await loadSoul(env);
  const next = applyToSoul(result, latest);
  const rev = Number.isInteger(latest._rev) ? latest._rev : 0;
  next._rev = rev + 1;
  await saveSoul(env, next);
  return next;
}

async function callAnthropic(prompt, maxTokens, env) {
  const payload = {
    model: env.ANTHROPIC_MODEL || 'claude-3-5-haiku-latest',
    max_tokens: Number.isInteger(maxTokens) ? maxTokens : 200,
    messages: [{ role: 'user', content: prompt }],
  };
  const req = fetch('https://api.anthropic.com/v1/messages', {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      'x-api-key': env.ANTHROPIC_KEY,
      'anthropic-version': '2023-06-01',
    },
    body: JSON.stringify(payload),
  });
  const timeout = new Promise((_, reject) => setTimeout(() => reject(new Error('模型请求超时')), MODEL_TIMEOUT_MS));
  const res = await Promise.race([req, timeout]);
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`模型请求失败(${res.status}): ${text.slice(0, 200)}`);
  }
  const data = await res.json();
  const text = Array.isArray(data?.content)
    ? data.content.filter(c => c?.type === 'text' && typeof c?.text === 'string').map(c => c.text).join('\n').trim()
    : '';
  return {
    ok: true,
    model: data?.model || payload.model,
    text,
    usage: {
      input_tokens: data?.usage?.input_tokens ?? null,
      output_tokens: data?.usage?.output_tokens ?? null,
    },
  };
}

async function maybeCallBrain(compiled, env) {
  if (!compiled?.brainCall) return { enabled: false, skipped: true, reason: 'no_brain_call' };
  if (env.MODEL_ENABLED !== '1') return { enabled: false, skipped: true, reason: 'model_disabled' };
  if (!env.ANTHROPIC_KEY) return { enabled: false, skipped: true, reason: 'missing_anthropic_key' };
  try {
    const out = await callAnthropic(compiled.brainCall.prompt, compiled.brainCall.maxTokens, env);
    return { enabled: true, skipped: false, ...out };
  } catch (err) {
    return { enabled: true, skipped: false, ok: false, error: String(err?.message ?? err) };
  }
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
  if (code.length > MAX_TALK_CHARS) {
    return badRequest(`code 过长，最多 ${MAX_TALK_CHARS} 字符`);
  }
  if (countLines(code) > MAX_TALK_LINES) {
    return badRequest(`code 行数过多，最多 ${MAX_TALK_LINES} 行`);
  }
  const soulBefore = await loadSoul(env);
  const result = interpret(code, soulBefore);
  const compiled = compile(result);
  const model = await maybeCallBrain(compiled, env);
  const soul = await applyResultAndSaveSoul(env, result);
  return json({ result, compiled, model, soul });
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
    packageVersion: '4.0.0',
    copyright: COPYRIGHT,
    capacity: CAPACITY,
    soul,
  });
}

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    const path = url.pathname;
    try {
      if (req.method === 'OPTIONS') {
        return new Response(null, { status: 204, headers: JSON_HEADERS });
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
