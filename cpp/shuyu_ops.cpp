/**
 * 枢语全量扩展操作引擎 — 纯 C++17 原生实现 (shuyu_ops.cpp)
 * © 阿权/路飞
 * 100% 替代 shuyu.js / JavaScriptCore，零外部依赖。
 */

#include "shuyu_ops.hpp"
#include "shuyu_lexicon.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace shuyu {

namespace {

constexpr int64_t kPulseMaxAt = 4102444800LL; // 2100-01-01 UTC
constexpr int kTaskVersion = 2;
constexpr size_t kTaskMaxActions = 8;

// ─── 轻量级零依赖 JSON 值与解析器 ───

struct JsonVal {
    enum class Type { Null, Bool, Int, Double, String, Array, Object };
    Type type = Type::Null;
    bool bVal = false;
    int64_t iVal = 0;
    double dVal = 0.0;
    std::string sVal;
    std::vector<JsonVal> aVal;
    std::map<std::string, JsonVal> oVal;

    static JsonVal makeNull() { return JsonVal{}; }
    static JsonVal makeBool(bool b) { JsonVal v; v.type = Type::Bool; v.bVal = b; return v; }
    static JsonVal makeInt(int64_t n) { JsonVal v; v.type = Type::Int; v.iVal = n; return v; }
    static JsonVal makeString(std::string s) { JsonVal v; v.type = Type::String; v.sVal = std::move(s); return v; }
    static JsonVal makeArray(std::vector<JsonVal> a = {}) { JsonVal v; v.type = Type::Array; v.aVal = std::move(a); return v; }
    static JsonVal makeObject() { JsonVal v; v.type = Type::Object; return v; }
};

std::string escapeJson(const std::string& s) {
    std::ostringstream oss;
    for (unsigned char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    oss << buf;
                } else {
                    oss << static_cast<char>(c);
                }
        }
    }
    return oss.str();
}

std::string serializeJson(const JsonVal& v) {
    switch (v.type) {
        case JsonVal::Type::Null: return "null";
        case JsonVal::Type::Bool: return v.bVal ? "true" : "false";
        case JsonVal::Type::Int: return std::to_string(v.iVal);
        case JsonVal::Type::Double: {
            std::ostringstream oss;
            oss << v.dVal;
            return oss.str();
        }
        case JsonVal::Type::String: return "\"" + escapeJson(v.sVal) + "\"";
        case JsonVal::Type::Array: {
            std::string out = "[";
            for (size_t i = 0; i < v.aVal.size(); ++i) {
                if (i > 0) out += ",";
                out += serializeJson(v.aVal[i]);
            }
            out += "]";
            return out;
        }
        case JsonVal::Type::Object: {
            std::string out = "{";
            bool first = true;
            for (const auto& kv : v.oVal) {
                if (!first) out += ",";
                first = false;
                out += "\"" + escapeJson(kv.first) + "\":" + serializeJson(kv.second);
            }
            out += "}";
            return out;
        }
    }
    return "null";
}

class JsonParser {
public:
    explicit JsonParser(const std::string& src) : s_(src) {}

    JsonVal parse() {
        skipWs();
        JsonVal v = parseValue();
        skipWs();
        if (pos_ != s_.size()) throw std::runtime_error("JSON 存在尾随非法字符");
        return v;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    void skipWs() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    JsonVal parseValue() {
        skipWs();
        if (pos_ >= s_.size()) throw std::runtime_error("JSON 意外结束");
        char c = s_[pos_];
        if (c == '"') return JsonVal::makeString(parseString());
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == 't' && s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return JsonVal::makeBool(true); }
        if (c == 'f' && s_.compare(pos_, 5, "false") == 0) { pos_ += 5; return JsonVal::makeBool(false); }
        if (c == 'n' && s_.compare(pos_, 4, "null") == 0) { pos_ += 4; return JsonVal::makeNull(); }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error("JSON 语法错误");
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    uint32_t parseHex4() {
        if (pos_ + 4 > s_.size()) throw std::runtime_error("JSON \\u 转义不完整");
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char h = s_[pos_++];
            val <<= 4;
            if (h >= '0' && h <= '9') val |= static_cast<uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') val |= static_cast<uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') val |= static_cast<uint32_t>(h - 'A' + 10);
            else throw std::runtime_error("JSON \\u 十六进制非法");
        }
        return val;
    }

    std::string parseString() {
        if (s_[pos_] != '"') throw std::runtime_error("期望字符串");
        ++pos_;
        std::string out;
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= s_.size()) throw std::runtime_error("JSON 转义不完整");
                char e = s_[pos_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = parseHex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 2 <= s_.size() &&
                            s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint32_t lo = parseHex4();
                            cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: throw std::runtime_error("JSON 未知转义");
                }
            } else {
                out.push_back(c);
            }
        }
        throw std::runtime_error("JSON 字符串未闭合");
    }

    JsonVal parseNumber() {
        size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        bool isFloat = false;
        while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        if (pos_ < s_.size() && s_[pos_] == '.') {
            isFloat = true;
            ++pos_;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            isFloat = true;
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        std::string sub = s_.substr(start, pos_ - start);
        if (!isFloat) {
            try {
                return JsonVal::makeInt(std::stoll(sub));
            } catch (...) {}
        }
        JsonVal v;
        v.type = JsonVal::Type::Double;
        v.dVal = std::stod(sub);
        return v;
    }

    JsonVal parseArray() {
        ++pos_; // '['
        JsonVal arr = JsonVal::makeArray();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return arr; }
        while (true) {
            arr.aVal.push_back(parseValue());
            skipWs();
            if (pos_ >= s_.size()) throw std::runtime_error("JSON 数组未闭合");
            if (s_[pos_] == ']') { ++pos_; return arr; }
            if (s_[pos_] != ',') throw std::runtime_error("JSON 数组缺少逗号");
            ++pos_;
        }
    }

    JsonVal parseObject() {
        ++pos_; // '{'
        JsonVal obj = JsonVal::makeObject();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return obj; }
        while (true) {
            skipWs();
            std::string key = parseString();
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != ':') throw std::runtime_error("JSON 对象缺少冒号");
            ++pos_;
            obj.oVal[key] = parseValue();
            skipWs();
            if (pos_ >= s_.size()) throw std::runtime_error("JSON 对象未闭合");
            if (s_[pos_] == '}') { ++pos_; return obj; }
            if (s_[pos_] != ',') throw std::runtime_error("JSON 对象缺少逗号");
            ++pos_;
        }
    }
};

JsonVal parseJson(const std::string& s) {
    return JsonParser(s).parse();
}

std::string trimStr(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool isNonNegIntStr(const std::string& s) {
    if (s.empty()) return false;
    if (s.size() > 1 && s[0] == '0') return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

std::string jsonValToWordInput(const JsonVal& v) {
    if (v.type == JsonVal::Type::Int) return std::to_string(v.iVal);
    if (v.type == JsonVal::Type::String) return v.sVal;
    if (v.type == JsonVal::Type::Null) return "0";
    throw std::runtime_error("类比词必须是编号或枢语词");
}

int64_t jsonValToStrictInt(const JsonVal& v, const std::string& errMsg) {
    if (v.type == JsonVal::Type::Int) return v.iVal;
    if (v.type == JsonVal::Type::String) {
        std::string t = trimStr(v.sVal);
        if (t.empty()) throw std::runtime_error(errMsg);
        size_t idx = 0;
        long long n = 0;
        try { n = std::stoll(t, &idx); } catch (...) { throw std::runtime_error(errMsg); }
        if (idx != t.size()) throw std::runtime_error(errMsg);
        return static_cast<int64_t>(n);
    }
    throw std::runtime_error(errMsg);
}

// ─── WordEntry / Coord5D 序列化（与 shuyu.js 字段 100% 一致）───

JsonVal coordToJson(const Coord5D& c) {
    JsonVal o = JsonVal::makeObject();
    o.oVal["c"] = JsonVal::makeInt(c.c);
    o.oVal["m"] = JsonVal::makeInt(c.m);
    o.oVal["s"] = JsonVal::makeInt(c.s);
    o.oVal["k"] = JsonVal::makeInt(c.k);
    o.oVal["p"] = JsonVal::makeInt(c.p);
    return o;
}

JsonVal wordToJson(const WordEntry& w) {
    JsonVal o = JsonVal::makeObject();
    o.oVal["id"] = JsonVal::makeInt(static_cast<int64_t>(w.id));
    o.oVal["词"] = JsonVal::makeString(w.word);
    o.oVal["汉"] = JsonVal::makeString(w.han);
    o.oVal["层"] = JsonVal::makeString(w.layer);
    o.oVal["义"] = JsonVal::makeString(w.sem);
    JsonVal roots = JsonVal::makeArray();
    for (const auto& r : w.roots) roots.aVal.push_back(JsonVal::makeString(r));
    o.oVal["根"] = std::move(roots);
    o.oVal["坐标"] = coordToJson(w.coord);
    return o;
}

JsonVal seedBrief(const WordEntry& w) {
    JsonVal o = JsonVal::makeObject();
    o.oVal["id"] = JsonVal::makeInt(static_cast<int64_t>(w.id));
    o.oVal["词"] = JsonVal::makeString(w.word);
    o.oVal["汉"] = JsonVal::makeString(w.han);
    return o;
}

// ─── 14 个息态链（一息 → 醒息）100% 等价实现 ───

std::string pulsePhase(int hour) {
    if (hour >= 5 && hour <= 7) return "晨";
    if (hour >= 8 && hour <= 16) return "昼";
    if (hour >= 17 && hour <= 19) return "昏";
    return "夜";
}

struct Pose {
    std::string phase;
    int hour = 0;
    int minute = 0;
    std::string axis;
    int dir = 1;
    bool moved = false;
    WordEntry word;
};

JsonVal poseToJson(const Pose& p) {
    JsonVal o = JsonVal::makeObject();
    o.oVal["息"] = JsonVal::makeString(p.phase);
    o.oVal["时"] = JsonVal::makeInt(p.hour);
    o.oVal["分"] = JsonVal::makeInt(p.minute);
    o.oVal["轴"] = JsonVal::makeString(p.axis);
    o.oVal["向"] = JsonVal::makeInt(p.dir);
    o.oVal["动"] = JsonVal::makeBool(p.moved);
    o.oVal["id"] = JsonVal::makeInt(static_cast<int64_t>(p.word.id));
    o.oVal["词"] = JsonVal::makeString(p.word.word);
    o.oVal["汉"] = JsonVal::makeString(p.word.han);
    o.oVal["义"] = JsonVal::makeString(p.word.sem);
    o.oVal["坐标"] = coordToJson(p.word.coord);
    return o;
}

void assignWordFields(JsonVal& obj, const WordEntry& w) {
    obj.oVal["id"] = JsonVal::makeInt(static_cast<int64_t>(w.id));
    obj.oVal["词"] = JsonVal::makeString(w.word);
    obj.oVal["汉"] = JsonVal::makeString(w.han);
    obj.oVal["义"] = JsonVal::makeString(w.sem);
    obj.oVal["坐标"] = coordToJson(w.coord);
}

std::array<int64_t, 5> coordArr(const Coord5D& c) {
    return {c.c, c.m, c.s, c.k, c.p};
}

Pose computePulse(const std::string& seedStr, int64_t t, WordEntry& outSeed) {
    if (t < 0 || t > kPulseMaxAt) {
        throw std::runtime_error("一息时刻必须是0至4102444800的Unix秒");
    }
    const auto& lex = LexiconEngine::instance();
    outSeed = lex.resolveWord(seedStr.empty() ? "0" : seedStr);
    int64_t day = ((t % 86400) + 86400) % 86400;
    int hour = static_cast<int>(day / 3600);
    int minute = static_cast<int>((day % 3600) / 60);
    const auto sizes = LexiconEngine::axisSizes();
    const auto& names = LexiconEngine::axisNames();
    auto next = coordArr(outSeed.coord);
    int axis = hour % 5;
    int dir = minute < 30 ? 1 : -1;
    int64_t cand = next[axis] + dir;
    bool moved = false;
    if (cand >= 0 && cand < static_cast<int64_t>(sizes[axis])) {
        next[axis] = cand;
        moved = true;
    }
    WordEntry w = *lex.decode(LexiconEngine::idOf(next[0], next[1], next[2], next[3], next[4]));
    return Pose{pulsePhase(hour), hour, minute, names[axis], dir, moved, std::move(w)};
}

struct TrailArgs {
    std::string seed = "0";
    int64_t at = 0;
    int n = 3;
};

TrailArgs parseBreathArgs(const std::string& opName, const std::string& input, bool allowN) {
    TrailArgs args;
    std::string trimmed = trimStr(input);
    if (trimmed.empty()) throw std::runtime_error(opName + "需要时刻");
    if (trimmed[0] == '[') {
        JsonVal arr = parseJson(trimmed);
        if (arr.type != JsonVal::Type::Array || arr.aVal.empty()) {
            throw std::runtime_error(opName + "需要时刻");
        }
        if (arr.aVal.size() == 1) {
            args.at = jsonValToStrictInt(arr.aVal[0], "一息时刻必须是0至4102444800的Unix秒");
        } else if (arr.aVal.size() == 2) {
            args.seed = jsonValToWordInput(arr.aVal[0]);
            args.at = jsonValToStrictInt(arr.aVal[1], "一息时刻必须是0至4102444800的Unix秒");
        } else {
            args.seed = jsonValToWordInput(arr.aVal[0]);
            args.at = jsonValToStrictInt(arr.aVal[1], "一息时刻必须是0至4102444800的Unix秒");
            if (allowN && arr.aVal[2].type != JsonVal::Type::Null) {
                args.n = static_cast<int>(jsonValToStrictInt(arr.aVal[2], "余息步数必须是2至4"));
            }
        }
    } else if (trimmed[0] == '{') {
        JsonVal obj = parseJson(trimmed);
        if (obj.type != JsonVal::Type::Object) throw std::runtime_error(opName + "参数非法");
        if (obj.oVal.count("seed")) args.seed = jsonValToWordInput(obj.oVal["seed"]);
        else if (obj.oVal.count("种")) args.seed = jsonValToWordInput(obj.oVal["种"]);
        if (obj.oVal.count("at")) args.at = jsonValToStrictInt(obj.oVal["at"], "一息时刻必须是0至4102444800的Unix秒");
        else if (obj.oVal.count("时")) args.at = jsonValToStrictInt(obj.oVal["时"], "一息时刻必须是0至4102444800的Unix秒");
        else throw std::runtime_error(opName + "需要时刻");
        if (allowN && obj.oVal.count("n") && obj.oVal["n"].type != JsonVal::Type::Null) {
            args.n = static_cast<int>(jsonValToStrictInt(obj.oVal["n"], "余息步数必须是2至4"));
        }
    } else {
        args.at = jsonValToStrictInt(JsonVal::makeString(trimmed), "一息时刻必须是0至4102444800的Unix秒");
    }
    if (allowN && (args.n < 2 || args.n > 4)) {
        throw std::runtime_error("余息步数必须是2至4");
    }
    return args;
}

// 统一实现 一息..醒息 的链式演算
JsonVal evalBreathChain(const std::string& op, const std::string& input) {
    const auto& lex = LexiconEngine::instance();
    const auto sizes = LexiconEngine::axisSizes();

    if (op == "一息") {
        TrailArgs a = parseBreathArgs("一息", input, false);
        WordEntry seedWord;
        Pose p = computePulse(a.seed, a.at, seedWord);
        JsonVal out = poseToJson(p);
        out.oVal["种"] = seedBrief(seedWord);
        return out;
    }

    TrailArgs a = parseBreathArgs(op, input, true);
    WordEntry seedWord;
    Pose origin = computePulse(a.seed, a.at, seedWord);
    std::vector<Pose> steps = {origin};
    uint64_t currentId = origin.word.id;
    for (int i = 1; i < a.n; ++i) {
        int64_t stepAt = a.at + static_cast<int64_t>(i) * 60;
        if (stepAt > kPulseMaxAt) break;
        WordEntry dummy;
        Pose nextP = computePulse(std::to_string(currentId), stepAt, dummy);
        steps.push_back(nextP);
        currentId = nextP.word.id;
    }

    JsonVal stepsJson = JsonVal::makeArray();
    for (const auto& s : steps) stepsJson.aVal.push_back(poseToJson(s));
    const Pose& last = steps.back();

    if (op == "余息") {
        JsonVal out = JsonVal::makeObject();
        out.oVal["息"] = JsonVal::makeString(steps[0].phase);
        out.oVal["种"] = seedBrief(seedWord);
        out.oVal["步"] = JsonVal::makeInt(static_cast<int64_t>(steps.size()));
        out.oVal["迹"] = stepsJson;
        assignWordFields(out, last.word);
        return out;
    }

    // 回息
    int lastAxis = LexiconEngine::axisIndex(last.axis);
    auto rebound = coordArr(last.word.coord);
    int back = -last.dir;
    bool echoed = false;
    if (rebound[lastAxis] + back >= 0 && rebound[lastAxis] + back < static_cast<int64_t>(sizes[lastAxis])) {
        rebound[lastAxis] += back;
        echoed = true;
    }
    WordEntry echoWord = *lex.decode(LexiconEngine::idOf(rebound[0], rebound[1], rebound[2], rebound[3], rebound[4]));
    Pose restPose{last.phase, last.hour, last.minute, last.axis, back, echoed, echoWord};

    if (op == "回息") {
        JsonVal out = JsonVal::makeObject();
        out.oVal["息"] = JsonVal::makeString(last.phase);
        out.oVal["种"] = seedBrief(seedWord);
        out.oVal["步"] = JsonVal::makeInt(static_cast<int64_t>(steps.size()));
        out.oVal["迹"] = stepsJson;
        out.oVal["回"] = JsonVal::makeBool(echoed);
        out.oVal["轴"] = JsonVal::makeString(last.axis);
        out.oVal["向"] = JsonVal::makeInt(back);
        assignWordFields(out, echoWord);
        return out;
    }

    if (op == "摇息") {
        int side = static_cast<int>((a.at / 60) % 2);
        bool swaying = (echoWord.id != last.word.id);
        const Pose& pose = (swaying && side == 1) ? last : restPose;
        const Pose& other = (swaying && side == 1) ? restPose : (swaying ? last : restPose);
        JsonVal out = JsonVal::makeObject();
        out.oVal["息"] = JsonVal::makeString(last.phase);
        out.oVal["种"] = seedBrief(seedWord);
        out.oVal["步"] = JsonVal::makeInt(static_cast<int64_t>(steps.size()));
        out.oVal["迹"] = stepsJson;
        out.oVal["回"] = JsonVal::makeBool(echoed);
        out.oVal["轴"] = JsonVal::makeString(pose.axis);
        out.oVal["向"] = JsonVal::makeInt(pose.dir);
        out.oVal["摇"] = JsonVal::makeBool(swaying);
        out.oVal["侧"] = JsonVal::makeInt(swaying ? side : 0);
        out.oVal["摆"] = poseToJson(other);
        assignWordFields(out, pose.word);
        return out;
    }

    // 落息
    int landPhase = static_cast<int>((a.at / 60) % 4);
    bool canLand = (echoWord.id != last.word.id);
    bool airborne = canLand && (landPhase == 0);
    bool landed = !airborne;
    int landSide = airborne ? 1 : 0;
    const Pose& landPose = airborne ? last : restPose;
    const Pose& landOther = airborne ? restPose : last;

    JsonVal out = JsonVal::makeObject();
    out.oVal["息"] = JsonVal::makeString(last.phase);
    out.oVal["种"] = seedBrief(seedWord);
    out.oVal["步"] = JsonVal::makeInt(static_cast<int64_t>(steps.size()));
    out.oVal["迹"] = stepsJson;
    out.oVal["回"] = JsonVal::makeBool(echoed);
    out.oVal["落"] = JsonVal::makeBool(landed);
    out.oVal["侧"] = JsonVal::makeInt(landSide);
    out.oVal["着"] = poseToJson(landOther);

    if (op == "落息") {
        out.oVal["轴"] = JsonVal::makeString(landPose.axis);
        out.oVal["向"] = JsonVal::makeInt(landPose.dir);
        assignWordFields(out, landPose.word);
        return out;
    }

    // 起息
    auto stirNeighbors = lex.near(std::to_string(landPose.word.id), 16);
    bool rising = landed && !stirNeighbors.empty();
    Pose sitPose{last.phase, last.hour, last.minute, landPose.axis, landPose.dir, landed, landPose.word};
    Pose curPose = sitPose;
    bool didStir = false;
    if (rising) {
        const auto& pick = stirNeighbors[static_cast<size_t>((a.at / 480) % stirNeighbors.size())];
        int ax = LexiconEngine::axisIndex(pick.axis);
        auto c1 = coordArr(pick.word.coord);
        auto c0 = coordArr(landPose.word.coord);
        int dir = c1[ax] > c0[ax] ? 1 : -1;
        curPose = Pose{last.phase, last.hour, last.minute, pick.axis, dir, true, pick.word};
        didStir = true;
    }
    out.oVal["起"] = JsonVal::makeBool(didStir);
    out.oVal["由"] = poseToJson(sitPose);
    if (op == "起息") {
        out.oVal["轴"] = JsonVal::makeString(curPose.axis);
        out.oVal["向"] = JsonVal::makeInt(curPose.dir);
        assignWordFields(out, curPose.word);
        return out;
    }

    // 栖息
    Pose risePose{last.phase, last.hour, last.minute, curPose.axis, curPose.dir, didStir, curPose.word};
    bool didPerch = false;
    curPose = risePose;
    if (didStir) {
        didPerch = true;
        int ax = LexiconEngine::axisIndex(risePose.axis);
        auto coord = coordArr(risePose.word.coord);
        int64_t nxt = coord[ax] + risePose.dir;
        if (nxt >= 0 && nxt < static_cast<int64_t>(sizes[ax])) {
            coord[ax] = nxt;
            WordEntry nw = *lex.decode(LexiconEngine::idOf(coord[0], coord[1], coord[2], coord[3], coord[4]));
            curPose = Pose{last.phase, last.hour, last.minute, risePose.axis, risePose.dir, true, std::move(nw)};
        }
    }
    out.oVal["起处"] = poseToJson(risePose);
    out.oVal["栖"] = JsonVal::makeBool(didPerch);
    if (op == "栖息") {
        out.oVal["轴"] = JsonVal::makeString(curPose.axis);
        out.oVal["向"] = JsonVal::makeInt(curPose.dir);
        assignWordFields(out, curPose.word);
        return out;
    }

    // 转息
    Pose perchPose{last.phase, last.hour, last.minute, curPose.axis, curPose.dir, didPerch, curPose.word};
    auto allN = lex.near(std::to_string(perchPose.word.id), 16);
    std::vector<NearHit> sideN;
    for (const auto& nh : allN) {
        if (nh.axis != perchPose.axis) sideN.push_back(nh);
    }
    bool didTurn = didPerch && !sideN.empty();
    curPose = perchPose;
    if (didTurn) {
        const auto& pick = sideN[static_cast<size_t>((a.at / 960) % sideN.size())];
        int ax = LexiconEngine::axisIndex(pick.axis);
        auto c1 = coordArr(pick.word.coord);
        auto c0 = coordArr(perchPose.word.coord);
        int dir = c1[ax] > c0[ax] ? 1 : -1;
        curPose = Pose{last.phase, last.hour, last.minute, pick.axis, dir, true, pick.word};
    }
    out.oVal["栖处"] = poseToJson(perchPose);
    out.oVal["转"] = JsonVal::makeBool(didTurn);
    if (op == "转息") {
        out.oVal["轴"] = JsonVal::makeString(curPose.axis);
        out.oVal["向"] = JsonVal::makeInt(curPose.dir);
        assignWordFields(out, curPose.word);
        return out;
    }

    // 顾息 → 倾息 → 贴息 → 含息 → 温息 → 醒息（完全同构的倍增时间窗链）
    struct StageSpec {
        const char* opName;
        const char* prevPosKey;
        const char* flagKey;
        int64_t windowSec;
    };
    static constexpr StageSpec kStages[] = {
        {"顾息", "转处", "顾", 1920},
        {"倾息", "顾处", "倾", 3840},
        {"贴息", "倾处", "贴", 7680},
        {"含息", "贴处", "含", 15360},
        {"温息", "含处", "温", 30720},
        {"醒息", "温处", "醒", 61440},
    };

    bool prevFlag = didTurn;
    for (const auto& st : kStages) {
        Pose prevPose{last.phase, last.hour, last.minute, curPose.axis, curPose.dir, prevFlag, curPose.word};
        bool curFlag = false;
        curPose = prevPose;
        if (prevFlag && ((a.at / st.windowSec) % 2 == 0)) {
            curFlag = true;
            int ax = LexiconEngine::axisIndex(prevPose.axis);
            auto coord = coordArr(prevPose.word.coord);
            int64_t nxt = coord[ax] + prevPose.dir;
            if (nxt >= 0 && nxt < static_cast<int64_t>(sizes[ax])) {
                coord[ax] = nxt;
                WordEntry nw = *lex.decode(LexiconEngine::idOf(coord[0], coord[1], coord[2], coord[3], coord[4]));
                curPose = Pose{last.phase, last.hour, last.minute, prevPose.axis, prevPose.dir, true, std::move(nw)};
            }
        }
        out.oVal[st.prevPosKey] = poseToJson(prevPose);
        out.oVal[st.flagKey] = JsonVal::makeBool(curFlag);
        prevFlag = curFlag;
        if (op == st.opName) {
            out.oVal["轴"] = JsonVal::makeString(curPose.axis);
            out.oVal["向"] = JsonVal::makeInt(curPose.dir);
            assignWordFields(out, curPose.word);
            return out;
        }
    }

    throw std::runtime_error("未知息态操作");
}

// ─── compileTask / describePlan / primes（100% 对齐 shuyu.js）───

struct CmdSpec {
    std::string tool;
    std::vector<std::string> keys;
    std::string presetOp;
};

const std::map<std::string, CmdSpec>& taskMapping() {
    static const std::map<std::string, CmdSpec> m = {
        {"计算", {"calc", {"expression"}, ""}}, {"calc", {"calc", {"expression"}, ""}},
        {"枢语", {"shuyu", {"operation", "input"}, ""}}, {"shuyu", {"shuyu", {"operation", "input"}, ""}},
        {"执行", {"shell_execute", {"command"}, ""}}, {"shell", {"shell_execute", {"command"}, ""}},
        {"规划", {"plan", {"title"}, ""}}, {"plan", {"plan", {"title"}, ""}},
        {"核对", {"verify", {"criterion"}, ""}}, {"verify", {"verify", {"criterion"}, ""}},
        {"检索", {"shuyu", {"input"}, "检索"}}, {"search", {"shuyu", {"input"}, "检索"}},
        {"邻近", {"shuyu", {"input"}, "邻近"}}, {"near", {"shuyu", {"input"}, "邻近"}},
        {"时间", {"clock", {"timezone"}, ""}}, {"clock", {"clock", {"timezone"}, ""}},
        {"一息", {"shuyu", {"input"}, "一息"}}, {"pulse", {"shuyu", {"input"}, "一息"}},
        {"余息", {"shuyu", {"input"}, "余息"}}, {"trail", {"shuyu", {"input"}, "余息"}},
        {"回息", {"shuyu", {"input"}, "回息"}}, {"echo", {"shuyu", {"input"}, "回息"}},
        {"摇息", {"shuyu", {"input"}, "摇息"}}, {"sway", {"shuyu", {"input"}, "摇息"}},
        {"落息", {"shuyu", {"input"}, "落息"}}, {"land", {"shuyu", {"input"}, "落息"}},
        {"起息", {"shuyu", {"input"}, "起息"}}, {"stir", {"shuyu", {"input"}, "起息"}},
        {"栖息", {"shuyu", {"input"}, "栖息"}}, {"perch", {"shuyu", {"input"}, "栖息"}},
        {"转息", {"shuyu", {"input"}, "转息"}}, {"turn", {"shuyu", {"input"}, "转息"}},
        {"顾息", {"shuyu", {"input"}, "顾息"}}, {"gaze", {"shuyu", {"input"}, "顾息"}},
        {"倾息", {"shuyu", {"input"}, "倾息"}}, {"incline", {"shuyu", {"input"}, "倾息"}},
        {"贴息", {"shuyu", {"input"}, "贴息"}}, {"nestle", {"shuyu", {"input"}, "贴息"}},
        {"含息", {"shuyu", {"input"}, "含息"}}, {"hold", {"shuyu", {"input"}, "含息"}},
        {"温息", {"shuyu", {"input"}, "温息"}}, {"warm", {"shuyu", {"input"}, "温息"}},
        {"醒息", {"shuyu", {"input"}, "醒息"}}, {"rouse", {"shuyu", {"input"}, "醒息"}},
    };
    return m;
}

// 提取并校验 $s1..$s8 槽位引用（只能引用当前步之前已定义的槽位）
std::vector<std::string> extractAndValidateSlotRefs(const std::string& s, size_t currentStepIndex, size_t lineNum) {
    std::vector<std::string> refs;
    for (size_t i = 0; i + 2 < s.size(); ++i) {
        if (s[i] == '$' && s[i + 1] == 's' && std::isdigit(static_cast<unsigned char>(s[i + 2]))) {
            size_t j = i + 2;
            while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
            std::string numStr = s.substr(i + 2, j - (i + 2));
            int slotNum = 0;
            try { slotNum = std::stoi(numStr); } catch (...) { slotNum = 0; }
            if (slotNum < 1 || slotNum > static_cast<int>(kTaskMaxActions)) {
                throw std::runtime_error("第" + std::to_string(lineNum) + "行槽位引用越界 $s" + numStr);
            }
            if (static_cast<size_t>(slotNum) > currentStepIndex) {
                throw std::runtime_error("第" + std::to_string(lineNum) + "行引用了尚未执行的槽位 $s" + numStr);
            }
            std::string slotId = "s" + std::to_string(slotNum);
            if (std::find(refs.begin(), refs.end(), slotId) == refs.end()) {
                refs.push_back(std::move(slotId));
            }
            i = j - 1;
        }
    }
    return refs;
}

struct BranchCondition {
    std::string slot;   // "s1".."s8"
    std::string op;     // "eq" | "neq" | "contains" | "ok" | "fail"
    std::string value;
    bool negate = false;
};

BranchCondition parseBranchCondition(const std::string& exprRaw, size_t executedCount, size_t lineNum) {
    std::string expr = trimStr(exprRaw);
    // 去掉末尾冒号
    if (expr.size() >= 3 && expr.compare(expr.size() - 3, 3, "：") == 0) {
        expr = trimStr(expr.substr(0, expr.size() - 3));
    } else if (!expr.empty() && expr.back() == ':') {
        expr = trimStr(expr.substr(0, expr.size() - 1));
    }
    if (expr.size() < 3 || expr[0] != '$' || expr[1] != 's' || !std::isdigit(static_cast<unsigned char>(expr[2]))) {
        throw std::runtime_error("第" + std::to_string(lineNum) + "行条件分支必须以 $s1..$s8 槽位开头");
    }
    size_t j = 2;
    while (j < expr.size() && std::isdigit(static_cast<unsigned char>(expr[j]))) ++j;
    int slotNum = 0;
    try { slotNum = std::stoi(expr.substr(2, j - 2)); } catch (...) { slotNum = 0; }
    if (slotNum < 1 || slotNum > static_cast<int>(kTaskMaxActions) || static_cast<size_t>(slotNum) > executedCount) {
        throw std::runtime_error("第" + std::to_string(lineNum) + "行条件引用了无效或未执行的槽位 $s" + expr.substr(2, j - 2));
    }

    BranchCondition cond;
    cond.slot = "s" + std::to_string(slotNum);
    std::string rest = trimStr(expr.substr(j));

    if (rest == "成功" || rest == "ok") {
        cond.op = "ok";
        return cond;
    }
    if (rest == "失败" || rest == "fail") {
        cond.op = "fail";
        return cond;
    }

    auto parseQuotedVal = [&](const std::string& rhs) -> std::string {
        JsonVal v;
        try { v = parseJson(trimStr(rhs)); }
        catch (...) { throw std::runtime_error("第" + std::to_string(lineNum) + "行条件比较值必须为JSON字符串"); }
        if (v.type != JsonVal::Type::String || v.sVal.size() > 4096) {
            throw std::runtime_error("第" + std::to_string(lineNum) + "行条件比较值必须为有限长度字符串");
        }
        return v.sVal;
    };

    if (rest.rfind("==", 0) == 0) {
        cond.op = "eq";
        cond.value = parseQuotedVal(rest.substr(2));
        return cond;
    }
    if (rest.rfind("!=", 0) == 0) {
        cond.op = "neq";
        cond.value = parseQuotedVal(rest.substr(2));
        return cond;
    }
    if (rest.rfind("含", 0) == 0) {
        cond.op = "contains";
        cond.value = parseQuotedVal(rest.substr(std::string("含").size()));
        return cond;
    }
    if (rest.rfind("contains", 0) == 0 && (rest.size() == 8 || std::isspace(static_cast<unsigned char>(rest[8])))) {
        cond.op = "contains";
        cond.value = parseQuotedVal(rest.substr(8));
        return cond;
    }
    throw std::runtime_error("第" + std::to_string(lineNum) + "行不支持的条件运算符");
}

JsonVal conditionToJson(const BranchCondition& c) {
    JsonVal o = JsonVal::makeObject();
    o.oVal["slot"] = JsonVal::makeString(c.slot);
    o.oVal["op"] = JsonVal::makeString(c.op);
    if (!c.value.empty() || c.op == "eq" || c.op == "neq" || c.op == "contains") {
        o.oVal["value"] = JsonVal::makeString(c.value);
    }
    o.oVal["negate"] = JsonVal::makeBool(c.negate);
    return o;
}

JsonVal compileTask(const std::string& source) {
    if (source.size() > 8192) throw std::runtime_error("枢语程序为空或超过8192字符");
    std::vector<std::string> lines;
    std::istringstream iss(source);
    std::string rawLine;
    while (std::getline(iss, rawLine)) {
        std::string t = trimStr(rawLine);
        if (!t.empty() && t.rfind("--", 0) != 0) lines.push_back(std::move(t));
    }
    if (lines.empty()) {
        throw std::runtime_error("每个枢语程序需要1至8条执行语句");
    }

    const auto& mp = taskMapping();
    JsonVal actions = JsonVal::makeArray();

    bool inBranch = false;
    bool inElse = false;
    BranchCondition activeCond;

    for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const std::string& line = lines[lineIdx];
        const size_t lineNum = lineIdx + 1;

        // 1) 块结束：终 / 结束 / endif
        if (line == "终" || line == "结束" || line == "endif") {
            if (!inBranch) throw std::runtime_error("第" + std::to_string(lineNum) + "行出现孤立的结束标记");
            inBranch = false;
            inElse = false;
            continue;
        }

        // 2) 否则分支：否则： / 否则: / else:
        if (line == "否则：" || line == "否则:" || line == "否则" || line == "else:" || line == "else") {
            if (!inBranch || inElse) throw std::runtime_error("第" + std::to_string(lineNum) + "行「否则」缺少配对的「若」");
            inElse = true;
            activeCond.negate = true;
            continue;
        }

        // 3) 若条件头：若 <expr>： / if <expr>:
        if (line.rfind("若 ", 0) == 0 || line.rfind("若$", 0) == 0 || line.rfind("if ", 0) == 0) {
            if (inBranch) throw std::runtime_error("第" + std::to_string(lineNum) + "行不支持嵌套「若」分支，请先以「终」结束");
            std::string body = (line.rfind("若", 0) == 0) ? line.substr(std::string("若").size()) : line.substr(2);
            activeCond = parseBranchCondition(body, actions.aVal.size(), lineNum);
            inBranch = true;
            inElse = false;
            continue;
        }

        // 4) 普通执行语句（保持 1..8 条总执行语句安全边界）
        if (actions.aVal.size() >= kTaskMaxActions) {
            throw std::runtime_error("每个枢语程序需要1至8条执行语句");
        }

        std::string rest;
        if (line.rfind("行：", 0) == 0) rest = line.substr(std::string("行：").size());
        else if (line.rfind("行:", 0) == 0) rest = line.substr(std::string("行:").size());
        else if (line.rfind("do:", 0) == 0) rest = line.substr(3);
        else throw std::runtime_error("第" + std::to_string(lineNum) + "行不是已支持的执行语句");

        rest = trimStr(rest);
        size_t openParen = rest.find('(');
        if (openParen == std::string::npos || openParen == 0) {
            throw std::runtime_error("第" + std::to_string(lineNum) + "行不是已支持的执行语句");
        }
        std::string cmd = rest.substr(0, openParen);
        auto it = mp.find(cmd);
        if (it == mp.end()) {
            throw std::runtime_error("第" + std::to_string(lineNum) + "行不是已支持的执行语句");
        }

        bool quoted = false, escaped = false;
        size_t closeParen = std::string::npos;
        for (size_t i = openParen + 1; i < rest.size(); ++i) {
            char c = rest[i];
            if (escaped) { escaped = false; continue; }
            if (quoted && c == '\\') { escaped = true; continue; }
            if (c == '"') { quoted = !quoted; continue; }
            if (!quoted && c == ')') { closeParen = i; break; }
        }
        if (closeParen == std::string::npos || quoted) {
            throw std::runtime_error("第" + std::to_string(lineNum) + "行括号或字符串未闭合");
        }

        JsonVal args;
        try {
            args = parseJson("[" + rest.substr(openParen + 1, closeParen - openParen - 1) + "]");
        } catch (...) {
            throw std::runtime_error("第" + std::to_string(lineNum) + "行参数必须为JSON字符串");
        }
        const CmdSpec& spec = it->second;
        if (args.type != JsonVal::Type::Array || args.aVal.size() != spec.keys.size()) {
            throw std::runtime_error("第" + std::to_string(lineNum) + "行参数数量或类型错误");
        }

        std::vector<std::string> allRefs;
        for (const auto& a : args.aVal) {
            if (a.type != JsonVal::Type::String || trimStr(a.sVal).empty() || a.sVal.size() > 4096) {
                throw std::runtime_error("第" + std::to_string(lineNum) + "行参数数量或类型错误");
            }
            auto r = extractAndValidateSlotRefs(a.sVal, actions.aVal.size(), lineNum);
            for (const auto& sid : r) {
                if (std::find(allRefs.begin(), allRefs.end(), sid) == allRefs.end()) {
                    allRefs.push_back(sid);
                }
            }
        }

        std::string suffix = trimStr(rest.substr(closeParen + 1));
        JsonVal expected = JsonVal::makeNull();
        if (!suffix.empty()) {
            const std::string arrow = "→";
            if (suffix.rfind(arrow, 0) != 0) {
                throw std::runtime_error("第" + std::to_string(lineNum) + "行末尾无效");
            }
            std::string expStr = trimStr(suffix.substr(arrow.size()));
            try {
                expected = parseJson(expStr);
            } catch (...) {
                throw std::runtime_error("验收结果必须为JSON字符串");
            }
            if (expected.type != JsonVal::Type::String || expected.sVal.size() > 4096) {
                throw std::runtime_error("验收结果必须为有限长度字符串");
            }
        }

        JsonVal params = JsonVal::makeObject();
        if (!spec.presetOp.empty()) params.oVal["operation"] = JsonVal::makeString(spec.presetOp);
        for (size_t k = 0; k < spec.keys.size(); ++k) {
            params.oVal[spec.keys[k]] = args.aVal[k];
        }
        if (spec.tool == "shell_execute") params.oVal["timeout"] = JsonVal::makeString("10");

        JsonVal act = JsonVal::makeObject();
        act.oVal["id"] = JsonVal::makeString("s" + std::to_string(actions.aVal.size() + 1));
        act.oVal["tool"] = JsonVal::makeString(spec.tool);
        act.oVal["arguments"] = std::move(params);
        act.oVal["expected"] = std::move(expected);
        if (!allRefs.empty()) {
            JsonVal refsArr = JsonVal::makeArray();
            for (const auto& sid : allRefs) refsArr.aVal.push_back(JsonVal::makeString(sid));
            act.oVal["refs"] = std::move(refsArr);
        }
        if (inBranch) {
            act.oVal["when"] = conditionToJson(activeCond);
        }
        actions.aVal.push_back(std::move(act));
    }

    if (actions.aVal.empty() || actions.aVal.size() > kTaskMaxActions) {
        throw std::runtime_error("每个枢语程序需要1至8条执行语句");
    }

    JsonVal prog = JsonVal::makeObject();
    prog.oVal["version"] = JsonVal::makeInt(kTaskVersion);
    prog.oVal["actions"] = std::move(actions);
    return prog;
}

JsonVal describePlan(const JsonVal& prog) {
    JsonVal out = JsonVal::makeArray();
    for (const auto& act : prog.oVal.at("actions").aVal) {
        const auto& args = act.oVal.at("arguments").oVal;
        std::string title = act.oVal.at("tool").sVal;
        for (const char* k : {"title", "criterion", "expression", "command", "input", "timezone"}) {
            auto it = args.find(k);
            if (it != args.end() && it->second.type == JsonVal::Type::String && !it->second.sVal.empty()) {
                title = it->second.sVal;
                break;
            }
        }
        JsonVal row = JsonVal::makeObject();
        row.oVal["id"] = act.oVal.at("id");
        row.oVal["tool"] = act.oVal.at("tool");
        row.oVal["title"] = JsonVal::makeString(title);
        out.aVal.push_back(std::move(row));
    }
    return out;
}

JsonVal primesOp(const std::string& input) {
    if (input.empty()) throw std::runtime_error("质数范围必须为0至10000整数");
    for (char c : input) if (c < '0' || c > '9') throw std::runtime_error("质数范围必须为0至10000整数");
    int n = 0;
    try { n = std::stoi(input); } catch (...) { throw std::runtime_error("质数范围必须为0至10000整数"); }
    if (n < 0 || n > 10000) throw std::runtime_error("质数范围必须为0至10000整数");
    JsonVal out = JsonVal::makeArray();
    if (n < 2) return out;
    std::vector<uint8_t> marked(static_cast<size_t>(n + 1), 0);
    for (int i = 2; i <= n; ++i) {
        if (marked[i]) continue;
        out.aVal.push_back(JsonVal::makeInt(i));
        for (int j = i * i; j <= n; j += i) marked[j] = 1;
    }
    return out;
}

JsonVal dispatchOp(const std::string& operation, const std::string& input) {
    const auto& lex = LexiconEngine::instance();

    if (operation == "容量") {
        JsonVal axes = JsonVal::makeObject();
        axes.oVal["核"] = JsonVal::makeInt(LexiconEngine::NC);
        axes.oVal["映"] = JsonVal::makeInt(LexiconEngine::NM);
        axes.oVal["态"] = JsonVal::makeInt(LexiconEngine::NS);
        axes.oVal["标"] = JsonVal::makeInt(LexiconEngine::NK);
        axes.oVal["相"] = JsonVal::makeInt(LexiconEngine::NP);
        JsonVal o = JsonVal::makeObject();
        o.oVal["version"] = JsonVal::makeString("4.3");
        o.oVal["capacity"] = JsonVal::makeInt(static_cast<int64_t>(LexiconEngine::CAPACITY));
        o.oVal["axes"] = std::move(axes);
        return o;
    }
    if (operation == "解码") {
        if (!isNonNegIntStr(input)) throw std::runtime_error("编号必须是非负整数");
        uint64_t n = 0;
        try { n = std::stoull(input); } catch (...) { throw std::runtime_error("编号越界"); }
        auto d = lex.decode(n);
        if (!d) throw std::runtime_error("编号越界 0.." + std::to_string(LexiconEngine::CAPACITY - 1));
        return wordToJson(*d);
    }
    if (operation == "拉丁编号") {
        int64_t n = lex.encode(input);
        if (n < 0) throw std::runtime_error("无效枢语词");
        return JsonVal::makeInt(n);
    }
    if (operation == "汉译编号") {
        int64_t n = lex.encodeHan(input);
        if (n < 0) throw std::runtime_error("无效枢语汉译");
        return JsonVal::makeInt(n);
    }
    if (operation == "检索") {
        auto hits = lex.search(input);
        JsonVal arr = JsonVal::makeArray();
        size_t lim = std::min<size_t>(20, hits.size());
        for (size_t i = 0; i < lim; ++i) {
            JsonVal h = JsonVal::makeObject();
            h.oVal["轴"] = JsonVal::makeString(hits[i].axis);
            h.oVal["下标"] = JsonVal::makeInt(static_cast<int64_t>(hits[i].index));
            h.oVal["拉丁"] = JsonVal::makeString(hits[i].lat);
            h.oVal["汉"] = JsonVal::makeString(hits[i].han);
            h.oVal["义"] = JsonVal::makeString(hits[i].sem);
            arr.aVal.push_back(std::move(h));
        }
        return arr;
    }
    if (operation == "造词") {
        return wordToJson(lex.autoCoin(input));
    }
    if (operation == "组合") {
        JsonVal spec = parseJson(input);
        if (spec.type != JsonVal::Type::Object) throw std::runtime_error("compose 需要对象");
        std::array<uint64_t, 5> idx = {0, 0, 0, 0, 0};
        for (const auto& kv : spec.oVal) {
            std::string k = kv.first;
            if (k == "c") k = "核";
            else if (k == "m") k = "映";
            else if (k == "s") k = "态";
            else if (k == "k") k = "标";
            else if (k == "p") k = "相";
            int ai = LexiconEngine::axisIndex(k);
            if (ai < 0) throw std::runtime_error("未知轴「" + kv.first + "」");
            const auto& val = kv.second;
            if (val.type == JsonVal::Type::Null) idx[ai] = 0;
            else if (val.type == JsonVal::Type::Bool) throw std::runtime_error(k + "轴不接受布尔值");
            else if (val.type == JsonVal::Type::Int) idx[ai] = lex.resolveAxisInt(ai, val.iVal);
            else if (val.type == JsonVal::Type::String) idx[ai] = lex.resolveAxis(ai, val.sVal);
            else throw std::runtime_error(k + "轴入参类型非法");
        }
        return wordToJson(*lex.decode(LexiconEngine::idOf(idx[0], idx[1], idx[2], idx[3], idx[4])));
    }
    if (operation == "类比") {
        JsonVal parts = parseJson(input);
        if (parts.type != JsonVal::Type::Array || parts.aVal.size() != 3) {
            throw std::runtime_error("类比需要3个词");
        }
        return wordToJson(lex.analogy(
            jsonValToWordInput(parts.aVal[0]),
            jsonValToWordInput(parts.aVal[1]),
            jsonValToWordInput(parts.aVal[2])
        ));
    }
    if (operation == "邻近") {
        std::string target = input;
        int limit = 8;
        std::string trimmed = trimStr(input);
        if (!trimmed.empty() && trimmed[0] == '[') {
            JsonVal parts = parseJson(trimmed);
            if (parts.type != JsonVal::Type::Array || parts.aVal.empty()) {
                throw std::runtime_error("邻近需要词或编号");
            }
            target = jsonValToWordInput(parts.aVal[0]);
            if (parts.aVal.size() >= 2 && parts.aVal[1].type == JsonVal::Type::Int) {
                limit = static_cast<int>(parts.aVal[1].iVal);
            }
        }
        auto hits = lex.near(target, limit);
        JsonVal arr = JsonVal::makeArray();
        for (const auto& nh : hits) {
            JsonVal o = JsonVal::makeObject();
            o.oVal["距"] = JsonVal::makeInt(nh.dist);
            o.oVal["轴"] = JsonVal::makeString(nh.axis);
            assignWordFields(o, nh.word);
            arr.aVal.push_back(std::move(o));
        }
        return arr;
    }
    if (operation == "编译") return compileTask(input);
    if (operation == "规划") return describePlan(compileTask(input));
    if (operation == "往返") {
        WordEntry w = lex.autoCoin(input);
        bool ok = (lex.encode(w.word) == static_cast<int64_t>(w.id)) &&
                  (lex.encodeHan(w.han) == static_cast<int64_t>(w.id));
        return JsonVal::makeBool(ok);
    }
    if (operation == "质数") return primesOp(input);

    // 14 个息态操作
    static const std::array<std::string, 14> kBreathOps = {
        "一息", "余息", "回息", "摇息", "落息", "起息", "栖息",
        "转息", "顾息", "倾息", "贴息", "含息", "温息", "醒息"
    };
    for (const auto& bop : kBreathOps) {
        if (operation == bop) return evalBreathChain(operation, input);
    }

    throw std::runtime_error("未知枢语操作");
}

} // namespace

std::string invokeOperationJson(const std::string& operation, const std::string& input) {
    try {
        JsonVal val = dispatchOp(operation, input);
        JsonVal resp = JsonVal::makeObject();
        resp.oVal["ok"] = JsonVal::makeBool(true);
        resp.oVal["value"] = std::move(val);
        return serializeJson(resp);
    } catch (const std::exception& e) {
        JsonVal resp = JsonVal::makeObject();
        resp.oVal["ok"] = JsonVal::makeBool(false);
        resp.oVal["error"] = JsonVal::makeString(e.what());
        return serializeJson(resp);
    }
}

} // namespace shuyu
