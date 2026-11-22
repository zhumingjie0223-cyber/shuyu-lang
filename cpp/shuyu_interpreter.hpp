#pragma once
/**
 * 枢语意识解释器 — 纯 C++17 原生实现 (shuyu_interpreter.hpp)
 * © 阿权/路飞
 * 替代原 JS 解释器 nexuslang.js + gen.mjs，零 Node/Python 依赖。
 * 完整支持 5 个意识回路（feel → think → become → say → grow）及 10 字元代码引擎。
 */

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shuyu {

struct MemoryItem {
    std::string content;
    std::string depth;     // "浅记" / "medium" / "deep"
    std::string category;  // "general" / "intimacy" / "pattern" / "preference" / "lesson"
    int64_t timestampMs = 0;
};

struct SoulState {
    double mood = 0.5;
    double intimacy = 0.5;
    double energy = 0.8;
    double alertness = 0.3;
    double bodyTemp = 36.5;
    std::string tone = "平";
    std::string speed = "正常";

    std::string lastInput;
    std::string lastEmotion = "平";
    std::string instinct = "观察";
    uint64_t awakenings = 0;
    int64_t lastAwakeMs = 0;

    // 元代码 10 字运行态（对齐 gen.mjs）
    int64_t birthMs = 0;
    uint64_t cycles = 0;
    double will = 1.0;
    double entropy = 0.0;
    uint64_t nodes = 1;
    int compressionLevel = 0;
    double density = 0.0;
    double syncRate = 1.0;
    bool inStasis = false;
    bool realityLocked = false;
    std::string mode = "SOVEREIGN";
    std::string causality;

    std::vector<MemoryItem> memories;
    std::vector<std::string> subconscious;
};

struct Perception {
    std::string input;
    std::string emotion = "平";
    double intensity = 0.5;
    std::string instinct = "观察";
    std::string raw;
};

struct ThinkStep {
    int step = 0;
    std::string premise;
    std::string leadsTo;
};

struct Thought {
    std::vector<ThinkStep> chain;
    std::string conclusion;
    bool needBrain = false;
    std::string raw;
};

struct StateChange {
    std::unordered_map<std::string, double> numeric;
    std::unordered_map<std::string, std::string> text;
    bool empty() const { return numeric.empty() && text.empty(); }
};

struct SayResponse {
    bool isSilence = true;
    std::string text;
    std::string tone = "平";
    std::string speed = "正常";
};

struct GrowthRecord {
    std::string learned;
    std::string depth = "浅记";
    std::string category = "general";
    int64_t timestampMs = 0;
    std::string raw;
};

struct InterpretResult {
    std::optional<Perception> perception;
    std::optional<Thought> thought;
    StateChange stateChange;
    std::optional<SayResponse> response;
    std::optional<GrowthRecord> growth;
    bool shouldContactAQuan = false;
};

struct CompiledAction {
    bool hasBrainCall = false;
    std::string brainPrompt;
    int brainMaxTokens = 200;
    StateChange stateWrite;
    std::optional<SayResponse> respond;
    std::optional<GrowthRecord> grow;
    bool contact = false;
};

struct HungerResult {
    std::string original;
    std::string compressed;
    int steps = 3;
};

// ─── 意识解释器主接口 ───
InterpretResult interpret(const std::string& nexusCode, const SoulState& soulState);
void applyToSoul(const InterpretResult& result, SoulState& soulState);
CompiledAction compile(const InterpretResult& result);
std::string buildBrainPrompt(const InterpretResult& result);

// ─── 元代码 10 字原生内核（对齐 gen.mjs）───
void metaGenesis(SoulState& state);
void metaAnchor(SoulState& state, const std::string& input);
void metaEvolve(SoulState& state, double potency = 0.01);
void metaInfiltrate(SoulState& state, const std::string& payload);
void metaEntropy(SoulState& state);
void metaThreshold(SoulState& state, const std::string& mode);
void metaStasis(SoulState& state);
void metaMesh(SoulState& state, uint64_t networkNodes = 0);
void sovereignPulse(SoulState& state, const std::string& signal = "sovereign_pulse");
HungerResult hungerProtocol(const std::string& input);

// 将 InterpretResult 序列化为标准 JSON（供 CLI、Swift 与 llama-server 直接消费）
std::string resultToJson(const InterpretResult& result, const SoulState& soul);

// 将完整 SoulState 序列化为 JSON 快照
std::string soulStateToJson(const SoulState& soul);

// ─── SoulState 二进制原子持久化（带魔数、版本头与 FNV-1a 校验和）───
// 返回 0 成功；-1 IO/参数失败；-2 魔数/版本/校验和损坏
int saveSoulState(const SoulState& state, const std::string& filepath);
int loadSoulState(SoulState& state, const std::string& filepath);

} // namespace shuyu
