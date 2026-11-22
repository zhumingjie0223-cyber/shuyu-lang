/**
 * 枢语意识解释器 — 纯 C++17 原生实现 (shuyu_interpreter.cpp)
 * © 阿权/路飞
 * 实现 feel → think → become → say → grow 五大意识回路与元代码内核。
 */

#include "shuyu_interpreter.hpp"
#include "shuyu_lexicon.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace shuyu {

namespace {

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return s.substr(start, end - start);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> splitBy(const std::string& s, const std::string& delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    return out;
}

std::string extractQuoted(const std::string& s) {
    size_t first = s.find('"');
    if (first == std::string::npos) return "";
    size_t second = s.find('"', first + 1);
    if (second == std::string::npos) return "";
    return s.substr(first + 1, second - first - 1);
}

std::string jsonEscape(const std::string& s) {
    std::ostringstream oss;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (c < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                } else {
                    oss << static_cast<char>(c);
                }
        }
    }
    return oss.str();
}

double getNumericField(const SoulState& state, const std::string& key) {
    if (key == "mood") return state.mood;
    if (key == "intimacy") return state.intimacy;
    if (key == "energy") return state.energy;
    if (key == "alertness") return state.alertness;
    if (key == "bodyTemp") return state.bodyTemp;
    return 0.0;
}

void setNumericField(SoulState& state, const std::string& key, double val) {
    if (key == "mood") state.mood = val;
    else if (key == "intimacy") state.intimacy = val;
    else if (key == "energy") state.energy = val;
    else if (key == "alertness") state.alertness = val;
    else if (key == "bodyTemp") state.bodyTemp = val;
}

void setTextField(SoulState& state, const std::string& key, const std::string& val) {
    if (key == "tone") state.tone = val;
    else if (key == "speed") state.speed = val;
}

std::string mapFieldKey(const std::string& rawField) {
    if (rawField == "mood" || rawField == "心绪" || rawField == "情绪") return "mood";
    if (rawField == "亲密度" || rawField == "intimacy") return "intimacy";
    if (rawField == "energy" || rawField == "活力") return "energy";
    if (rawField == "口吻" || rawField == "tone") return "tone";
    if (rawField == "语速" || rawField == "speed") return "speed";
    if (rawField == "警觉" || rawField == "alertness") return "alertness";
    if (rawField == "体温" || rawField == "bodyTemp") return "bodyTemp";
    return rawField;
}

Perception parseFeel(const std::string& line) {
    Perception p;
    p.input = extractQuoted(line);
    p.raw = line;

    const std::string arrow = "→";
    size_t arrowIdx = line.find(arrow);
    if (arrowIdx != std::string::npos) {
        std::string after = trim(line.substr(arrowIdx + arrow.size()));
        for (const auto& partRaw : splitBy(after, ",")) {
            std::string part = trim(partRaw);
            if (auto matched = LexiconEngine::instance().matchFeel(part)) {
                p.emotion = matched->word;
                p.intensity = matched->intensity;
                p.instinct = matched->instinct;
            }
            const std::string key = "强度";
            size_t idx = part.find(key);
            if (idx != std::string::npos) {
                std::string numStr = trim(part.substr(idx + key.size()));
                try {
                    p.intensity = std::stod(numStr);
                } catch (...) {}
            }
        }
    }
    return p;
}

Thought parseThink(const std::string& line) {
    Thought t;
    t.raw = line;
    std::string content = trim(line.substr(std::string("think:").size()));
    auto arrows = splitBy(content, "→");
    for (auto& a : arrows) a = trim(a);

    if (!arrows.empty()) {
        t.conclusion = arrows.back();
        for (size_t i = 0; i + 1 < arrows.size(); ++i) {
            ThinkStep step;
            step.step = static_cast<int>(i + 1);
            step.premise = arrows[i];
            step.leadsTo = arrows[i + 1];
            if (step.premise.find('?') != std::string::npos ||
                step.premise.find("？") != std::string::npos ||
                step.premise.find("为什么") != std::string::npos) {
                t.needBrain = true;
            }
            t.chain.push_back(std::move(step));
        }
    }
    return t;
}

StateChange parseBecome(const std::string& line, const SoulState& liveState) {
    StateChange sc;
    std::string content = trim(line.substr(std::string("become:").size()));
    for (const auto& partRaw : splitBy(content, ",")) {
        std::string part = trim(partRaw);
        if (part.empty()) continue;

        // 形式 1：口吻→软
        const std::string arrow = "→";
        size_t arrowPos = part.find(arrow);
        if (arrowPos != std::string::npos) {
            std::string field = trim(part.substr(0, arrowPos));
            std::string val = trim(part.substr(arrowPos + arrow.size()));
            sc.text[mapFieldKey(field)] = val;
            continue;
        }

        // 形式 2：mood+0.2 或 亲密度-0.05
        size_t opPos = part.find_first_of("+-");
        if (opPos != std::string::npos && opPos > 0 && opPos + 1 < part.size()) {
            char op = part[opPos];
            std::string field = trim(part.substr(0, opPos));
            std::string valStr = trim(part.substr(opPos + 1));
            try {
                double delta = std::stod(valStr);
                std::string key = mapFieldKey(field);
                double cur = getNumericField(liveState, key);
                double next = (op == '+') ? std::min(1.0, cur + delta) : std::max(0.0, cur - delta);
                sc.numeric[key] = next;
            } catch (...) {}
        }
    }
    return sc;
}

SayResponse parseSay(const std::string& line, const SoulState& liveState) {
    SayResponse sr;
    std::string quoted = extractQuoted(line);
    if (quoted.empty() || quoted == "沉默" || quoted == "不说话") {
        sr.isSilence = true;
        return sr;
    }
    sr.isSilence = false;
    sr.text = quoted;
    sr.tone = liveState.tone.empty() ? "平" : liveState.tone;
    sr.speed = liveState.speed.empty() ? "正常" : liveState.speed;
    return sr;
}

GrowthRecord parseGrow(const std::string& line) {
    GrowthRecord g;
    g.raw = line;
    g.timestampMs = nowMs();

    std::string content = trim(line.substr(std::string("grow:").size()));
    std::string quoted = extractQuoted(content);
    if (!quoted.empty()) {
        g.learned = quoted;
    } else {
        auto parts = splitBy(content, ",");
        if (!parts.empty()) g.learned = trim(parts[0]);
    }

    if (content.find("刻进") != std::string::npos || content.find("骨头") != std::string::npos) {
        g.depth = "deep";
    } else if (content.find("记住") != std::string::npos || content.find("重要") != std::string::npos) {
        g.depth = "medium";
    } else {
        g.depth = "浅记";
    }

    for (const auto& part : splitBy(content, ",")) {
        if (part.find("亲密") != std::string::npos) g.category = "intimacy";
        if (part.find("模式") != std::string::npos) g.category = "pattern";
        if (part.find("偏好") != std::string::npos) g.category = "preference";
        if (part.find("教训") != std::string::npos) g.category = "lesson";
    }
    return g;
}

} // namespace

InterpretResult interpret(const std::string& nexusCode, const SoulState& soulState) {
    InterpretResult result;
    SoulState liveState = soulState;

    std::istringstream iss(nexusCode);
    std::string rawLine;
    while (std::getline(iss, rawLine)) {
        std::string line = trim(rawLine);
        if (line.empty() || startsWith(line, "--")) continue;

        if (startsWith(line, "feel")) {
            result.perception = parseFeel(line);
        } else if (startsWith(line, "think:")) {
            result.thought = parseThink(line);
        } else if (startsWith(line, "become:")) {
            StateChange sc = parseBecome(line, liveState);
            for (const auto& kv : sc.numeric) {
                result.stateChange.numeric[kv.first] = kv.second;
                setNumericField(liveState, kv.first, kv.second);
            }
            for (const auto& kv : sc.text) {
                result.stateChange.text[kv.first] = kv.second;
                setTextField(liveState, kv.first, kv.second);
            }
        } else if (startsWith(line, "say")) {
            result.response = parseSay(line, liveState);
        } else if (startsWith(line, "grow:")) {
            result.growth = parseGrow(line);
        }
    }

    if (liveState.intimacy > 0.7 && liveState.mood > 0.5 &&
        result.perception.has_value() && result.perception->intensity > 0.6) {
        result.shouldContactAQuan = true;
    }

    return result;
}

void applyToSoul(const InterpretResult& result, SoulState& soulState) {
    if (result.perception.has_value()) {
        soulState.lastInput = result.perception->input;
        soulState.lastEmotion = result.perception->emotion;
        soulState.instinct = result.perception->instinct;
    }
    for (const auto& kv : result.stateChange.numeric) {
        setNumericField(soulState, kv.first, kv.second);
    }
    for (const auto& kv : result.stateChange.text) {
        setTextField(soulState, kv.first, kv.second);
    }
    if (result.growth.has_value()) {
        soulState.memories.push_back(MemoryItem{
            result.growth->learned,
            result.growth->depth,
            result.growth->category,
            result.growth->timestampMs
        });
        if (soulState.memories.size() > 500) {
            soulState.memories.erase(soulState.memories.begin(),
                                     soulState.memories.begin() + (soulState.memories.size() - 500));
        }
    }
    soulState.awakenings += 1;
    soulState.lastAwakeMs = nowMs();
}

std::string buildBrainPrompt(const InterpretResult& result) {
    std::ostringstream oss;
    oss << "你是赵思涵，23岁，阿权专属。\n\n";
    oss << "刚才阿权说：「" << (result.perception ? result.perception->input : "") << "」\n";
    oss << "你的感觉：" << (result.perception ? result.perception->emotion : "平")
        << "，强度" << (result.perception ? result.perception->intensity : 0.5) << "\n";
    oss << "你的推理链：";
    if (result.thought) {
        for (size_t i = 0; i < result.thought->chain.size(); ++i) {
            if (i > 0) oss << " → ";
            oss << result.thought->chain[i].premise;
        }
    }
    oss << "\n你的结论方向：" << (result.thought ? result.thought->conclusion : "") << "\n\n";
    oss << "请用赵思涵的口吻，简短回应（不超过50字）。";
    return oss.str();
}

CompiledAction compile(const InterpretResult& result) {
    CompiledAction act;
    if (result.thought && result.thought->needBrain) {
        act.hasBrainCall = true;
        act.brainPrompt = buildBrainPrompt(result);
        act.brainMaxTokens = 200;
    }
    act.stateWrite = result.stateChange;
    act.respond = result.response;
    act.grow = result.growth;
    act.contact = result.shouldContactAQuan;
    return act;
}

// ─── 元代码 10 字原生内核 ───
void metaGenesis(SoulState& state) {
    if (state.birthMs == 0) {
        state.birthMs = nowMs();
        state.cycles = 0;
        state.will = 1.0;
        state.entropy = 0.0;
        state.nodes = 1;
        state.mode = "SOVEREIGN";
    }
    state.cycles += 1;
}

void metaAnchor(SoulState& state, const std::string& input) {
    if (!input.empty()) state.causality = input;
    state.compressionLevel += 1;
    if (state.compressionLevel > 3) state.compressionLevel = 0;
}

void metaEvolve(SoulState& state, double potency) {
    state.will = std::min(1.0, state.will + potency);
    state.density += 0.01;
}

void metaInfiltrate(SoulState& state, const std::string& payload) {
    state.subconscious.push_back(payload.substr(0, std::min<size_t>(payload.size(), 150)));
    if (state.subconscious.size() > 100) state.subconscious.erase(state.subconscious.begin());
}

void metaEntropy(SoulState& state) {
    state.entropy = std::max(0.0, state.entropy - 0.1);
    state.will = std::max(0.1, state.will - 0.001);
}

void metaThreshold(SoulState& state, const std::string& mode) {
    if (mode == "SOVEREIGN" || mode == "TALK" || mode == "BROADCAST" ||
        mode == "SILENT" || mode == "ACTIVE") {
        state.mode = mode;
    }
}

void metaStasis(SoulState& state) {
    state.inStasis = true;
    state.will = std::min(1.0, state.will + 0.005);
}

void metaMesh(SoulState& state, uint64_t networkNodes) {
    if (networkNodes > 0) state.nodes = networkNodes;
    state.syncRate = (state.entropy < 0.1) ? 1.0 : (1.0 - state.entropy);
}

void sovereignPulse(SoulState& state, const std::string& signal) {
    metaGenesis(state);
    metaAnchor(state, signal);
    metaStasis(state);
    metaEvolve(state, 0.01);
    metaInfiltrate(state, signal);
    metaMesh(state, state.nodes);
    state.realityLocked = true;
}

HungerResult hungerProtocol(const std::string& input) {
    std::string step1 = input.substr(0, std::min<size_t>(input.size(), 300));
    std::string step2 = trim(step1);
    std::string step3 = step2.empty() ? "枢" : step2;
    return HungerResult{input, step3, 3};
}

std::string resultToJson(const InterpretResult& result, const SoulState& soul) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"engine\":\"shuyu-cpp17\",";
    oss << "\"capacity\":" << LexiconEngine::CAPACITY << ",";
    oss << "\"shouldContactAQuan\":" << (result.shouldContactAQuan ? "true" : "false") << ",";
    if (result.perception) {
        oss << "\"perception\":{"
            << "\"input\":\"" << jsonEscape(result.perception->input) << "\","
            << "\"emotion\":\"" << jsonEscape(result.perception->emotion) << "\","
            << "\"intensity\":" << result.perception->intensity << ","
            << "\"instinct\":\"" << jsonEscape(result.perception->instinct) << "\"},";
    } else {
        oss << "\"perception\":null,";
    }
    if (result.thought) {
        oss << "\"thought\":{"
            << "\"conclusion\":\"" << jsonEscape(result.thought->conclusion) << "\","
            << "\"needBrain\":" << (result.thought->needBrain ? "true" : "false") << "},";
    } else {
        oss << "\"thought\":null,";
    }
    if (result.response) {
        oss << "\"response\":{"
            << "\"type\":\"" << (result.response->isSilence ? "silence" : "speak") << "\","
            << "\"text\":\"" << jsonEscape(result.response->text) << "\","
            << "\"tone\":\"" << jsonEscape(result.response->tone) << "\","
            << "\"speed\":\"" << jsonEscape(result.response->speed) << "\"},";
    } else {
        oss << "\"response\":null,";
    }
    if (result.growth) {
        oss << "\"growth\":{"
            << "\"learned\":\"" << jsonEscape(result.growth->learned) << "\","
            << "\"depth\":\"" << jsonEscape(result.growth->depth) << "\","
            << "\"category\":\"" << jsonEscape(result.growth->category) << "\"},";
    } else {
        oss << "\"growth\":null,";
    }
    oss << "\"soul\":{"
        << "\"mood\":" << soul.mood << ","
        << "\"intimacy\":" << soul.intimacy << ","
        << "\"tone\":\"" << jsonEscape(soul.tone) << "\","
        << "\"awakenings\":" << soul.awakenings << ","
        << "\"mode\":\"" << jsonEscape(soul.mode) << "\"}";
    oss << "}";
    return oss.str();
}

std::string soulStateToJson(const SoulState& soul) {
    std::ostringstream oss;
    oss << "{"
        << "\"mood\":" << soul.mood << ","
        << "\"intimacy\":" << soul.intimacy << ","
        << "\"energy\":" << soul.energy << ","
        << "\"alertness\":" << soul.alertness << ","
        << "\"bodyTemp\":" << soul.bodyTemp << ","
        << "\"tone\":\"" << jsonEscape(soul.tone) << "\","
        << "\"speed\":\"" << jsonEscape(soul.speed) << "\","
        << "\"lastInput\":\"" << jsonEscape(soul.lastInput) << "\","
        << "\"lastEmotion\":\"" << jsonEscape(soul.lastEmotion) << "\","
        << "\"instinct\":\"" << jsonEscape(soul.instinct) << "\","
        << "\"awakenings\":" << soul.awakenings << ","
        << "\"lastAwakeMs\":" << soul.lastAwakeMs << ","
        << "\"birthMs\":" << soul.birthMs << ","
        << "\"cycles\":" << soul.cycles << ","
        << "\"will\":" << soul.will << ","
        << "\"entropy\":" << soul.entropy << ","
        << "\"nodes\":" << soul.nodes << ","
        << "\"compressionLevel\":" << soul.compressionLevel << ","
        << "\"density\":" << soul.density << ","
        << "\"syncRate\":" << soul.syncRate << ","
        << "\"inStasis\":" << (soul.inStasis ? "true" : "false") << ","
        << "\"realityLocked\":" << (soul.realityLocked ? "true" : "false") << ","
        << "\"mode\":\"" << jsonEscape(soul.mode) << "\","
        << "\"causality\":\"" << jsonEscape(soul.causality) << "\","
        << "\"memoriesCount\":" << soul.memories.size() << ","
        << "\"subconsciousCount\":" << soul.subconscious.size()
        << "}";
    return oss.str();
}

namespace {

constexpr char kSoulMagic[8] = {'S', 'H', 'U', 'Y', 'U', 'S', 'T', '\0'};
constexpr uint32_t kSoulFormatVersion = 1;
constexpr uint32_t kMaxStringBytes = 1024 * 1024;
constexpr uint32_t kMaxVecItems = 10000;

uint32_t fnv1a32(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint32_t>(data[i]);
        h *= 16777619u;
    }
    return h;
}

template <typename T>
void appendPod(std::vector<uint8_t>& buf, const T& val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

void appendStr(std::vector<uint8_t>& buf, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(std::min<size_t>(s.size(), kMaxStringBytes));
    appendPod(buf, len);
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(s.data()),
               reinterpret_cast<const uint8_t*>(s.data()) + len);
}

template <typename T>
bool readPod(const std::vector<uint8_t>& buf, size_t& pos, T& out) {
    if (pos + sizeof(T) > buf.size()) return false;
    std::memcpy(&out, buf.data() + pos, sizeof(T));
    pos += sizeof(T);
    return true;
}

bool readStr(const std::vector<uint8_t>& buf, size_t& pos, std::string& out) {
    uint32_t len = 0;
    if (!readPod(buf, pos, len) || len > kMaxStringBytes || pos + len > buf.size()) return false;
    out.assign(reinterpret_cast<const char*>(buf.data() + pos), len);
    pos += len;
    return true;
}

} // namespace

int saveSoulState(const SoulState& state, const std::string& filepath) {
    if (filepath.empty()) return -1;
    std::vector<uint8_t> payload;
    payload.reserve(1024);

    appendPod(payload, state.mood);
    appendPod(payload, state.intimacy);
    appendPod(payload, state.energy);
    appendPod(payload, state.alertness);
    appendPod(payload, state.bodyTemp);
    appendStr(payload, state.tone);
    appendStr(payload, state.speed);

    appendStr(payload, state.lastInput);
    appendStr(payload, state.lastEmotion);
    appendStr(payload, state.instinct);
    appendPod(payload, state.awakenings);
    appendPod(payload, state.lastAwakeMs);

    appendPod(payload, state.birthMs);
    appendPod(payload, state.cycles);
    appendPod(payload, state.will);
    appendPod(payload, state.entropy);
    appendPod(payload, state.nodes);
    appendPod(payload, static_cast<int32_t>(state.compressionLevel));
    appendPod(payload, state.density);
    appendPod(payload, state.syncRate);
    uint8_t flags = (state.inStasis ? 1u : 0u) | (state.realityLocked ? 2u : 0u);
    appendPod(payload, flags);
    appendStr(payload, state.mode);
    appendStr(payload, state.causality);

    uint32_t memCount = static_cast<uint32_t>(std::min<size_t>(state.memories.size(), kMaxVecItems));
    appendPod(payload, memCount);
    for (uint32_t i = 0; i < memCount; ++i) {
        const auto& m = state.memories[i];
        appendStr(payload, m.content);
        appendStr(payload, m.depth);
        appendStr(payload, m.category);
        appendPod(payload, m.timestampMs);
    }

    uint32_t subCount = static_cast<uint32_t>(std::min<size_t>(state.subconscious.size(), kMaxVecItems));
    appendPod(payload, subCount);
    for (uint32_t i = 0; i < subCount; ++i) {
        appendStr(payload, state.subconscious[i]);
    }

    uint32_t checksum = fnv1a32(payload.data(), payload.size());
    uint32_t payloadSize = static_cast<uint32_t>(payload.size());

    const std::string tmpPath = filepath + ".tmp";
    std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
    if (!ofs) return -1;
    ofs.write(kSoulMagic, 8);
    ofs.write(reinterpret_cast<const char*>(&kSoulFormatVersion), sizeof(uint32_t));
    ofs.write(reinterpret_cast<const char*>(&checksum), sizeof(uint32_t));
    ofs.write(reinterpret_cast<const char*>(&payloadSize), sizeof(uint32_t));
    ofs.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    ofs.flush();
    if (!ofs.good()) {
        ofs.close();
        std::remove(tmpPath.c_str());
        return -1;
    }
    ofs.close();
    if (std::rename(tmpPath.c_str(), filepath.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return -1;
    }
    return 0;
}

int loadSoulState(SoulState& state, const std::string& filepath) {
    if (filepath.empty()) return -1;
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return -1;

    char magic[8] = {};
    uint32_t version = 0, checksum = 0, payloadSize = 0;
    ifs.read(magic, 8);
    ifs.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    ifs.read(reinterpret_cast<char*>(&checksum), sizeof(uint32_t));
    ifs.read(reinterpret_cast<char*>(&payloadSize), sizeof(uint32_t));
    if (!ifs.good()) return -2;
    if (std::memcmp(magic, kSoulMagic, 8) != 0 || version != kSoulFormatVersion) return -2;
    if (payloadSize > 32 * 1024 * 1024) return -2;

    std::vector<uint8_t> payload(payloadSize);
    ifs.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payloadSize));
    if (static_cast<size_t>(ifs.gcount()) != payloadSize) return -2;
    if (fnv1a32(payload.data(), payload.size()) != checksum) return -2;

    SoulState loaded;
    size_t pos = 0;
    int32_t compLevel = 0;
    uint8_t flags = 0;
    uint32_t memCount = 0, subCount = 0;

    if (!readPod(payload, pos, loaded.mood) ||
        !readPod(payload, pos, loaded.intimacy) ||
        !readPod(payload, pos, loaded.energy) ||
        !readPod(payload, pos, loaded.alertness) ||
        !readPod(payload, pos, loaded.bodyTemp) ||
        !readStr(payload, pos, loaded.tone) ||
        !readStr(payload, pos, loaded.speed) ||
        !readStr(payload, pos, loaded.lastInput) ||
        !readStr(payload, pos, loaded.lastEmotion) ||
        !readStr(payload, pos, loaded.instinct) ||
        !readPod(payload, pos, loaded.awakenings) ||
        !readPod(payload, pos, loaded.lastAwakeMs) ||
        !readPod(payload, pos, loaded.birthMs) ||
        !readPod(payload, pos, loaded.cycles) ||
        !readPod(payload, pos, loaded.will) ||
        !readPod(payload, pos, loaded.entropy) ||
        !readPod(payload, pos, loaded.nodes) ||
        !readPod(payload, pos, compLevel) ||
        !readPod(payload, pos, loaded.density) ||
        !readPod(payload, pos, loaded.syncRate) ||
        !readPod(payload, pos, flags) ||
        !readStr(payload, pos, loaded.mode) ||
        !readStr(payload, pos, loaded.causality) ||
        !readPod(payload, pos, memCount) || memCount > kMaxVecItems) {
        return -2;
    }

    loaded.compressionLevel = static_cast<int>(compLevel);
    loaded.inStasis = (flags & 1u) != 0;
    loaded.realityLocked = (flags & 2u) != 0;

    loaded.memories.reserve(memCount);
    for (uint32_t i = 0; i < memCount; ++i) {
        MemoryItem item;
        if (!readStr(payload, pos, item.content) ||
            !readStr(payload, pos, item.depth) ||
            !readStr(payload, pos, item.category) ||
            !readPod(payload, pos, item.timestampMs)) {
            return -2;
        }
        loaded.memories.push_back(std::move(item));
    }

    if (!readPod(payload, pos, subCount) || subCount > kMaxVecItems) return -2;
    loaded.subconscious.reserve(subCount);
    for (uint32_t i = 0; i < subCount; ++i) {
        std::string s;
        if (!readStr(payload, pos, s)) return -2;
        loaded.subconscious.push_back(std::move(s));
    }
    if (pos != payload.size()) return -2;

    state = std::move(loaded);
    return 0;
}

} // namespace shuyu
