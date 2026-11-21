/**
 * 枢语纯 C++17 引擎单元测试 (test_shuyu.cpp)
 * 验证 76.7 亿容量、decode↔encode 往返、autoCoin 确定性、5 回路意识解释与 C ABI。
 */

#include "shuyu_c_api.h"
#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

int main() {
    const auto& eng = shuyu::LexiconEngine::instance();

    // 1. 容量校验（与 Python / JS 严格同构）
    assert(shuyu::LexiconEngine::CAPACITY == 7667712000ULL);
    assert(shuyu_capacity() == 7667712000ULL);

    // 2. decode <-> encode 往返测试
    const std::vector<uint64_t> ids = {0, 1, 7, 8, 512, 1234567, 2949119999ULL, 7667711999ULL};
    for (uint64_t id : ids) {
        auto w = eng.decode(id);
        assert(w.has_value());
        int64_t back = eng.encode(w->word);
        if (back != static_cast<int64_t>(id)) {
            std::cerr << "往返失败: id=" << id << " word=" << w->word << " back=" << back << "\n";
            return 1;
        }
    }

    // 3. 五回路意识解释器测试（含 become 实时生效供 say 读取）
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
    assert(res.perception.has_value());
    assert(res.perception->emotion == "暖");
    assert(res.perception->intensity > 0.79);
    assert(res.thought.has_value());
    assert(res.thought->needBrain == true);
    assert(res.response.has_value());
    assert(!res.response->isSilence);
    assert(res.response->tone == "软"); // become 变更实时生效
    assert(res.growth.has_value());
    assert(res.growth->depth == "deep");
    assert(res.shouldContactAQuan == true);

    shuyu::applyToSoul(res, soul);
    assert(soul.awakenings == 1);
    assert(soul.memories.size() == 1);

    // 4. C ABI 校验
    ShuyuRuntime* rt = shuyu_runtime_create();
    char* json = shuyu_interpret_json(rt, code.c_str());
    assert(json != nullptr);
    std::string jsonStr(json);
    assert(jsonStr.find("\"engine\":\"shuyu-cpp17\"") != std::string::npos);
    shuyu_free_string(json);
    shuyu_runtime_destroy(rt);

    std::cout << "✅ 枢语 C++17 引擎全部测试通过（容量 " << shuyu::LexiconEngine::CAPACITY << "，往返与五回路 100% 一致）\n";
    return 0;
}
