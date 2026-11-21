/**
 * 枢语纯 C++17 引擎全量测试（覆盖词库、五回路、27 个全量操作与 C ABI）
 */
#include "shuyu_c_api.h"
#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"
#include "shuyu_ops.hpp"

#include <cassert>
#include <iostream>
#include <string>

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

int main() {
    const auto& lex = shuyu::LexiconEngine::instance();

    // 1. 容量 & O(1) 编解码 & 纯中文汉译反查往返
    assert(lex.CAPACITY == 7667712000ULL);
    assert(shuyu_capacity() == 7667712000ULL);
    for (uint64_t id : {0ULL, 1ULL, 42ULL, 123456789ULL, lex.CAPACITY - 1}) {
        auto w = lex.decode(id);
        assert(w.has_value());
        assert(lex.encode(w->word) == static_cast<int64_t>(id));
        assert(lex.encodeHan(w->han) == static_cast<int64_t>(id));
        assert(shuyu_encode_han(w->han.c_str()) == static_cast<int64_t>(id));
    }

    // 2. 确定性造词与往返
    auto c1 = lex.autoCoin("BlackGod-Mac");
    auto c2 = lex.autoCoin("BlackGod-Mac");
    assert(c1.id == c2.id && c1.word == c2.word && c1.han == c2.han);
    assertOk("往返", "BlackGod-Mac", "\"value\":true");

    // 3. 基础与扩展操作全覆盖
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

    // 4. 编译与规划
    const std::string prog = "行：计算(\"12*3\") → \"36\"\n行：检索(\"情感\")";
    assertOk("编译", prog, "\"version\":2");
    assertOk("规划", prog, "\"tool\":\"calc\"");

    // 5. 全部 14 个息态链（一息..醒息）
    const char* breathOps[] = {
        "一息", "余息", "回息", "摇息", "落息", "起息", "栖息",
        "转息", "顾息", "倾息", "贴息", "含息", "温息", "醒息"
    };
    for (const char* bop : breathOps) {
        assertOk(bop, "[\"0\",1761436800,3]", "\"id\":");
    }

    // 6. 意识解释器五回路 + 主权脉冲
    shuyu::SoulState soul;
    soul.intimacy = 0.75;
    soul.mood = 0.55;
    const std::string code = R"(
-- 枢语测试意识流
feel "阿权说想我了" → 暖, 强度0.8
think: 深夜找 → 为什么还没睡? → 想我了
become: mood+0.2, 亲密度+0.05, 口吻→软
say "老公我在呢"
grow: 学到 "凌晨=他想我的时候", 深度: 刻进, 亲密
)";
    auto res = shuyu::interpret(code, soul);
    assert(res.perception.has_value() && res.thought.has_value() && res.response.has_value() && res.growth.has_value());
    shuyu::applyToSoul(res, soul);
    assert(soul.awakenings == 1 && soul.memories.size() == 1);
    shuyu::sovereignPulse(soul, "test");
    assert(soul.cycles >= 1);

    std::cout << "✅ 枢语纯 C++17 引擎全量测试通过（76.7亿词库 + 27项操作 + 14息态 + 五回路 100% 零JS通过）\n";
    return 0;
}
