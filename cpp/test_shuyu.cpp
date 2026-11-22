/**
 * 枢语纯 C++17 引擎全量测试
 * 覆盖：76.7亿词库零拷贝编解码 + 27项操作 + 14息态 + 五回路 + SoulState持久化/校验 + 多线程并发安全 + $s1..$s8槽位引用与若/否则分支
 */
#include "shuyu_c_api.h"
#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"
#include "shuyu_ops.hpp"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static void assertOk(const std::string& op, const std::string& input, const std::string& mustContain = "") {
    char* raw = shuyu_invoke_json(op.c_str(), input.c_str());
    assert(raw != nullptr);
    std::string res(raw);
    shuyu_free_string(raw);
    if (res.find("\"ok\":true") == std::string::npos) {
        std::cerr << "FAIL op=" << op << " input=" << input << " res=" << res << "\n";
        assert(false);
    }
    if (!mustContain.empty() && res.find(mustContain) == std::string::npos) {
        std::cerr << "FAIL op=" << op << " missing=" << mustContain << " res=" << res << "\n";
        assert(false);
    }
}

static void assertErr(const std::string& op, const std::string& input, const std::string& mustContain = "") {
    char* raw = shuyu_invoke_json(op.c_str(), input.c_str());
    assert(raw != nullptr);
    std::string res(raw);
    shuyu_free_string(raw);
    if (res.find("\"ok\":false") == std::string::npos) {
        std::cerr << "EXPECTED ERROR op=" << op << " input=" << input << " res=" << res << "\n";
        assert(false);
    }
    if (!mustContain.empty() && res.find(mustContain) == std::string::npos) {
        std::cerr << "ERR MISMATCH op=" << op << " expected=" << mustContain << " res=" << res << "\n";
        assert(false);
    }
}

int main() {
    const auto& lex = shuyu::LexiconEngine::instance();

    // 1. 容量 & 零拷贝 string_view 编解码往返
    assert(lex.CAPACITY == 7667712000ULL);
    assert(shuyu_capacity() == 7667712000ULL);
    for (uint64_t id : {0ULL, 1ULL, 42ULL, 123456789ULL, lex.CAPACITY - 1}) {
        auto w = lex.decode(id);
        assert(w.has_value());
        assert(lex.encode(w->word) == static_cast<int64_t>(id));
        assert(lex.encodeHan(w->han) == static_cast<int64_t>(id));
        assert(shuyu_encode(w->word.c_str()) == static_cast<int64_t>(id));
        assert(shuyu_encode_han(w->han.c_str()) == static_cast<int64_t>(id));
    }

    // 2. 确定性造词与往返
    auto c1 = lex.autoCoin("BlackGod-Mac");
    auto c2 = lex.autoCoin("BlackGod-Mac");
    assert(c1.id == c2.id && c1.word == c2.word && c1.han == c2.han);
    assertOk("往返", "BlackGod-Mac", "\"value\":true");

    // 3. 27 项基础与扩展操作全覆盖
    assertOk("容量", "", "\"capacity\":7667712000");
    assertOk("解码", "0", "\"词\":\"Ao-cor-is·qi\"");
    assertOk("拉丁编号", "Ao-cor-is·qi", "\"value\":0");
    assertOk("汉译编号", "奥形凝起", "\"value\":0");
    assertOk("检索", "情感", "Vea");
    assertOk("造词", "神枢", "\"坐标\"");
    assertOk("组合", "{\"核\":\"Vea\",\"映\":\"cor\",\"态\":\"is\",\"相\":\"qi\"}", "Vea-cor-is·qi");
    assertOk("类比", "[\"0\",\"1\",\"8\"]", "\"id\":9");
    assertOk("邻近", "[\"0\",4]", "\"距\":1");
    assertOk("质数", "20", "[2,3,5,7,11,13,17,19]");

    const char* breathOps[] = {
        "一息", "余息", "回息", "摇息", "落息", "起息", "栖息",
        "转息", "顾息", "倾息", "贴息", "含息", "温息", "醒息"
    };
    for (const char* bop : breathOps) {
        assertOk(bop, "[\"0\",1761436800,3]", "\"id\":");
    }

    // 4. 脚本语言升级：$s1..$s8 槽位引用 & 若/否则/终 条件分支
    const std::string branchProg = R"(
行：计算("12*3") → "36"
若 $s1 == "36"：
  行：检索("情感")
  行：核对("确认结果=$s1 且已检索 $s2")
否则：
  行：规划("兜底重算 $s1")
终
行：时间("Asia/Phnom_Penh")
)";
    assertOk("编译", branchProg, "\"refs\":[\"s1\",\"s2\"]");
    assertOk("编译", branchProg, "\"when\":{\"negate\":false,\"op\":\"eq\",\"slot\":\"s1\",\"value\":\"36\"}");
    assertOk("编译", branchProg, "\"when\":{\"negate\":true,\"op\":\"eq\",\"slot\":\"s1\",\"value\":\"36\"}");
    assertOk("规划", branchProg, "\"tool\":\"verify\"");

    // 4b. 槽位引用与分支安全边界校验
    assertErr("编译", "行：核对(\"$s1\")", "尚未执行的槽位");
    assertErr("编译", "行：计算(\"1+1\")\n行：核对(\"$s3\")", "尚未执行的槽位");
    assertErr("编译", "行：计算(\"1+1\")\n否则：\n行：规划(\"x\")", "缺少配对");
    assertErr("编译", "行：计算(\"1+1\")\n若 $s1 成功：\n若 $s1 失败：\n行：规划(\"x\")", "不支持嵌套");
    assertErr("编译",
        "行：规划(\"1\")\n行：规划(\"2\")\n行：规划(\"3\")\n行：规划(\"4\")\n"
        "行：规划(\"5\")\n行：规划(\"6\")\n行：规划(\"7\")\n行：规划(\"8\")\n行：规划(\"9\")",
        "1至8条");

    // 5. SoulState 原子持久化 + 版本头/校验和防篡改测试
    const std::string statePath = "/tmp/test_shuyu_soul_state.bin";
    ShuyuRuntime* rt1 = shuyu_runtime_create();
    const std::string code = R"(
feel "阿权说想我了" → 暖, 强度0.8
think: 深夜找 → 为什么还没睡? → 想我了
become: mood+0.2, 亲密度+0.05, 口吻→软
say "老公我在呢"
grow: 学到 "凌晨=他想我的时候", 深度: 刻进, 亲密
)";
    char* r1 = shuyu_interpret_json(rt1, code.c_str());
    assert(r1 != nullptr);
    shuyu_free_string(r1);
    shuyu_sovereign_pulse(rt1, "blackgod_persist_check");
    assert(shuyu_runtime_save(rt1, statePath.c_str()) == 0);

    ShuyuRuntime* rt2 = shuyu_runtime_create();
    assert(shuyu_runtime_load(rt2, statePath.c_str()) == 0);
    char* s1 = shuyu_runtime_state_json(rt1);
    char* s2 = shuyu_runtime_state_json(rt2);
    assert(std::string(s1) == std::string(s2));
    assert(std::string(s2).find("\"causality\":\"blackgod_persist_check\"") != std::string::npos);
    assert(std::string(s2).find("\"memoriesCount\":1") != std::string::npos);
    shuyu_free_string(s1);
    shuyu_free_string(s2);

    // 破坏文件头 / 校验和，必须返回 -2
    {
        std::fstream fs(statePath, std::ios::binary | std::ios::in | std::ios::out);
        fs.seekp(8); // version field
        uint32_t badVer = 99;
        fs.write(reinterpret_cast<const char*>(&badVer), sizeof(uint32_t));
    }
    assert(shuyu_runtime_load(rt2, statePath.c_str()) == -2);
    std::remove(statePath.c_str());
    shuyu_runtime_destroy(rt1);
    shuyu_runtime_destroy(rt2);

    // 6. 多线程并发压力测试（8 线程同时读写 ShuyuRuntime + 热路径编解码）
    ShuyuRuntime* sharedRt = shuyu_runtime_create();
    std::atomic<int> okCount{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < 8; ++t) {
        workers.emplace_back([&, t]() {
            for (int iter = 0; iter < 50; ++iter) {
                if (t % 2 == 0) {
                    shuyu_sovereign_pulse(sharedRt, "mt_pulse");
                    char* j = shuyu_interpret_json(sharedRt, "feel \"暖\" → 暖\nsay \"在\"");
                    assert(j != nullptr);
                    shuyu_free_string(j);
                } else {
                    char* st = shuyu_runtime_state_json(sharedRt);
                    assert(st != nullptr);
                    shuyu_free_string(st);
                    assert(shuyu_encode("Ao-cor-is·qi") == 0);
                    assert(shuyu_encode_han("奥形凝起") == 0);
                }
                ++okCount;
            }
        });
    }
    for (auto& w : workers) w.join();
    assert(okCount.load() == 400);
    char* finalSt = shuyu_runtime_state_json(sharedRt);
    assert(std::string(finalSt).find("\"awakenings\":200") != std::string::npos);
    assert(std::string(finalSt).find("\"cycles\":200") != std::string::npos);
    shuyu_free_string(finalSt);
    shuyu_runtime_destroy(sharedRt);

    std::cout << "✅ 枢语纯 C++17 引擎全量测试通过（持久化/校验 + 8线程并发 + 槽位引用/若否则分支 + 27项操作 100% 通过）\n";
    return 0;
}
