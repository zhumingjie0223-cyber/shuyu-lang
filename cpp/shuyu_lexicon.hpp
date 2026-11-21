#pragma once
/**
 * 枢语 5 维乘法语义引擎 — 纯 C++17 原生头文件 (shuyu_lexicon.hpp)
 * © 阿权/路飞
 * 与 shuyu_engine.py、lexicon.js 保持严格同构：
 * 核(1040) × 映(180) × 态(80) × 标(64) × 相(8) = 7,667,712,000
 * 零外部依赖，支持 O(1) 编解码、纯中文汉译反查、语义检索、按义组合、类比、邻近、自主造词。
 */

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shuyu {

struct Coord5D {
    int64_t c = 0;  // 核 (0..1039)
    int64_t m = 0;  // 映 (0..179)
    int64_t s = 0;  // 态 (0..79)
    int64_t k = 0;  // 标 (0..63)
    int64_t p = 0;  // 相 (0..7)
};

struct WordEntry {
    uint64_t id = 0;
    std::string word;   // 拉丁编码词，如 "Ao-cor-is·qi"
    std::string han;    // 纯中文汉译，如 "奥形凝起"
    std::string layer;  // 所属语义层，如 "本源"
    std::string sem;    // 完整五维语义解释
    std::array<std::string, 5> roots; // [核, 映, 态, 标或∅, 相]
    Coord5D coord;
};

struct FeelEntry {
    std::string word;
    double intensity = 0.5;
    std::string instinct = "观察";
};

struct SearchHit {
    std::string axis;
    uint64_t index = 0;
    std::string lat;
    std::string han;
    std::string sem;
};

struct NearHit {
    int dist = 1;
    std::string axis;
    WordEntry word;
};

class LexiconEngine {
public:
    static constexpr uint64_t NC = 1040; // 52 核族 × 20 阶
    static constexpr uint64_t NM = 180;  // 15 映族 × 12 阶
    static constexpr uint64_t NS = 80;   // 8 态族 × 10 阶
    static constexpr uint64_t NK = 64;   // 8 标族 × 8 阶
    static constexpr uint64_t NP = 8;    // 8 因果相
    static constexpr uint64_t CAPACITY = NC * NM * NS * NK * NP; // 7,667,712,000

    static const LexiconEngine& instance();

    static uint64_t idOf(uint64_t c, uint64_t m, uint64_t s, uint64_t k, uint64_t p) {
        return ((((c * NM) + m) * NS + s) * NK + k) * NP + p;
    }

    static const std::array<std::string, 5>& axisNames();
    static std::array<uint64_t, 5> axisSizes() { return {NC, NM, NS, NK, NP}; }
    static int axisIndex(const std::string& name);

    // 编号 → 枢语词（O(1) 寻址）
    std::optional<WordEntry> decode(uint64_t n) const;

    // 拉丁枢语词 → 编号（严格双射，找不到返回 -1）
    int64_t encode(const std::string& word) const;

    // 纯中文汉译 → 编号（回溯切分，解不出或不唯一返回 -1）
    int64_t encodeHan(const std::string& han) const;

    // 语义检索（基表级，返回按得分排序的命中列表）
    std::vector<SearchHit> search(const std::string& keyword, const std::string& axis = "") const;

    // 解析单轴输入（整数 / 拉丁根 / 汉译 / 语义关键词）
    uint64_t resolveAxis(int ai, const std::string& val) const;
    uint64_t resolveAxisInt(int ai, int64_t val) const;

    // 解析编号 / 拉丁词 / 汉译词 → WordEntry（失败抛 std::runtime_error）
    WordEntry resolveWord(const std::string& input) const;

    // 五维类比造词：A:B :: C:?
    WordEntry analogy(const std::string& a, const std::string& b, const std::string& c) const;

    // 五维 L1=1 不环绕邻近词
    std::vector<NearHit> near(const std::string& input, int limit = 8) const;

    // 情感词匹配
    std::optional<FeelEntry> matchFeel(const std::string& text) const;

    // 5 维坐标直接造词
    WordEntry coinFromCoord(const Coord5D& coord) const;

    // 确定性种子造词（与 lexicon.js autoCoin FNV-1a 完全同构）
    WordEntry autoCoin(const std::string& seed) const;

    // 按当前情绪/想念状态自主造词
    WordEntry coinFromState(double mood, double missYou, const std::string& seed = "") const;

private:
    struct CoreItem {
        std::string lat;
        std::string han;
        std::string sem;
        std::string layer;
    };
    struct AxisItem {
        std::string lat;
        std::string han;
        std::string sem;
    };

    LexiconEngine();

    std::vector<CoreItem> cores_;
    std::vector<AxisItem> manis_;
    std::vector<AxisItem> stats_;
    std::vector<AxisItem> scals_;
    std::vector<AxisItem> phases_;

    std::array<std::unordered_map<std::string, uint64_t>, 5> latIndex_;
    std::array<std::unordered_map<std::string, uint64_t>, 5> hanIndex_;

    std::vector<FeelEntry> feelTable_;
};

} // namespace shuyu
