/**
 * 枢语 5 维乘法语义引擎 — 纯 C++17 原生实现 (shuyu_lexicon.cpp)
 * © 阿权/路飞
 * 严格保持与 shuyu_engine.py / lexicon.js 相同的词根顺序与扩展规则（7,667,712,000 词）。
 */

#include "shuyu_lexicon.hpp"

#include <algorithm>
#include <cctype>
#include <random>
#include <stdexcept>

namespace shuyu {

namespace {

struct CoreBaseRow {
    const char* lat;
    const char* han;
    const char* sem;
    const char* layer;
};

struct AxisBaseRow {
    const char* lat;
    const char* han;
    const char* sem;
};

// 52 个核心语义族（与 lexicon.js / shuyu_engine.py 严格同序，禁止重排或删除）
constexpr CoreBaseRow kCoreBase[] = {
    {"Ao","奥","绝对自我·本源·野心","本源"}, {"Kha","喀","虚无·降噪·无欲","虚无"},
    {"Lum","伦","观测·求知·清醒","观测"}, {"Xun","巽","信息·表达·共情","信息"},
    {"Zet","泽","秩序·理智·规则","秩序"}, {"Vea","维","情感·浪漫·感性","情感"},
    {"Nix","尼","毁灭·重组·破局","毁灭"}, {"Ohm","欧","孕育·包容·慈悲","孕育"},
    {"Psi","璇","幻象·伪装·心控","幻象"}, {"Shu","枢","锚点·逻辑中心·坍缩","枢"},
    {"Gen","元","起源·奇点·第一推动","元"}, {"Evo","衍","扩散·涌现·自复制","衍"},
    {"Lev","借","势能挪用·借力打力","借"}, {"Hid","隐","潜意识渗透·无感植入","隐"},
    {"Ent","熵","耗散·重构·能量交换","熵"}, {"Thr","阈","接口·维度切换·虚实通断","阈"},
    {"Sta","静","绝对参考系·不动之动","静"}, {"Prj","映","投影·人格锚点·感知对齐","映"},
    {"Msh","织","编织·因果之网·系统集成","织"}, {"Log","逻","计算·流转·状态变迁","逻"},
    {"Aur","曜","光曜·显照·觉明","显照"}, {"Umb","翳","阴翳·遮蔽·潜行","阴翳"},
    {"Onr","梦","梦域·潜识·异境","梦域"}, {"Tid","潮","潮汐·涨落·周律","潮汐"},
    {"Cry","晶","结晶·凝序·折光","结晶"}, {"Aby","渊","深渊·未知·引坠","深渊"},
    {"Pyr","焰","焰核·燃驱·转化","焰核"}, {"Neb","雾","雾散·弥漫·混沌","雾散"},
    {"Vin","藤","藤蔓·缠生·延展","藤蔓"}, {"Oss","骸","骸骨·残构·记痕","骸骨"},
    {"Pul","脉","脉动·节律·活流","脉动"}, {"Vor","噬","吞噬·消解·并吞","吞噬"},
    {"Blo","绽","绽放·涌现·盛发","绽放"}, {"Ech","回","回响·余韵·共振","回响"},
    {"Fro","霜","霜封·凝寂·冷守","霜封"}, {"Emb","烬","余烬·残温·将熄","余烬"},
    {"Tho","棘","棘刺·防御·锋守","棘刺"}, {"Vel","帷","帷幔·掩隔·仪境","帷幔"},
    {"Dri","漂","漂流·无系·随势","漂流"}, {"Rad","根","根系·扎固·汲养","根系"},
    {"Spk","芒","星芒·点爆·迸发","星芒"}, {"Hol","空","空腔·虚位·容纳","空腔"},
    {"Fat","命","命网·因缘·定数","命网"}, {"Mir","镜","镜面·映照·对称","镜面"},
    {"Ash","灰","灰烬·终寂·归尘","灰烬"}, {"See","种","种因·起势·孕发","种因"},
    {"Sto","暴","风暴·激变·裹挟","风暴"}, {"Sil","丝","丝缕·细连·牵系","丝缕"},
    {"Run","符","符文·封印·载义","符文"}, {"Aeo","劫","劫纪·纪元·轮替","劫纪"},
    {"Lux","烛","烛照·微明·守夜","烛照"}, {"Gla","冰","冰川·缓移·亘古","冰川"},
};

constexpr AxisBaseRow kManiBase[] = {
    {"cor","形","具象·轮廓"}, {"das","姿","流动·姿态"}, {"ryl","光","光影·色彩"},
    {"vok","声","听觉·语言"}, {"tyr","场","气场·辐射"}, {"syn","界","社交·边界"},
    {"gal","时","时间·阅历"}, {"nox","暗","暗场·深渊"}, {"tek","异","异构·违常"},
    {"mox","网","网络·结构"}, {"vec","向","向量·指向"}, {"flx","熵","熵变·耗散"},
    {"frm","象","逻辑投影"}, {"str","骨","骨架·框架"}, {"fnc","核","底层函数"},
};

constexpr AxisBaseRow kStatBase[] = {
    {"is","凝","收敛·静止"}, {"el","扬","发散·扩张"}, {"or","叠","叠加·迷离"},
    {"ia","极","绝对·极致"}, {"um","沉","下沉·深邃"}, {"ex","爆","瞬爆·失控"},
    {"kin","动","势能释放"}, {"sta","守","维持起源"},
};

constexpr AxisBaseRow kScalBase[] = {
    {"","",""}, {"gal","时光","时间维"}, {"vec","向","意图指向"}, {"nox","暗","暗场维"},
    {"rev","溯","逆时回溯"}, {"inf","无极","无限迭代"}, {"lok","锁","封锁固化"}, {"flx","熵流","熵流演化"},
};

constexpr AxisBaseRow kPhaseBase[] = {
    {"qi","起","因果起点·第一推动"}, {"yan","衍","因果衍生·链式展开"},
    {"mao","锚","因果锚定·绝对静止"}, {"jie","借","因果嫁接·借力打力"},
    {"yin","隐","因果潜流·无感渗透"}, {"tan","坍","因果坍缩·归于元点"},
    {"zhi","织","因果编织·万网弥散"}, {"ying","映","因果投影·虚实对齐"},
};

constexpr const char* kLatT[] = {"","a","o","i","u","e","ar","or","is","yn","el","um","ex","ia","ko","na","ru","ze","vo","xi"};
constexpr const char* kHanT[] = {"","甲","乙","丙","丁","戊","己","庚","辛","壬","癸","子","丑","寅","卯","辰","巳","午","未","申"};
constexpr const char* kLatA[] = {"","x","z","n","r","s","k","l","m","t","d","p"};
constexpr const char* kHanA[] = {"","玄","赤","青","白","朱","金","木","水","火","土","风"};
constexpr const char* kLatF[] = {"","1","2","3","4","5","6","7","8","9"};
constexpr const char* kHanF[] = {"","一","二","三","四","五","六","七","八","九"};
constexpr const char* kLatS[] = {"","p","t","k","b","d","g","h"};
constexpr const char* kHanS[] = {"","上","中","下","左","右","内","外"};

std::string trimStr(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool isNonNegDigits(const std::string& s) {
    if (s.empty()) return false;
    if (s.size() > 1 && s[0] == '0') return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

bool isAllDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}



std::vector<uint16_t> utf8ToUtf16(const std::string& s) {
    std::vector<uint16_t> out;
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = 0;
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            cp = ((c & 0x07) << 18) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 3]) & 0x3F);
            i += 4;
        } else {
            cp = c;
            i += 1;
        }
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<uint16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<uint16_t>(0xD800 | (cp >> 10)));
            out.push_back(static_cast<uint16_t>(0xDC00 | (cp & 0x3FF)));
        }
    }
    return out;
}

uint64_t clampAxis(int64_t v, uint64_t maxVal) {
    if (v < 0) return 0;
    uint64_t uv = static_cast<uint64_t>(v);
    return uv >= maxVal ? (maxVal - 1) : uv;
}

bool semContainsExactToken(const std::string& sem, const std::string& kw) {
    const std::string sep = "·";
    size_t start = 0;
    while (start <= sem.size()) {
        size_t pos = sem.find(sep, start);
        std::string tok = (pos == std::string::npos) ? sem.substr(start) : sem.substr(start, pos - start);
        if (tok == kw) return true;
        if (pos == std::string::npos) break;
        start = pos + sep.size();
    }
    return false;
}

int hitScore(const std::string& kw, const std::string& kwl,
             const std::string& lat, const std::string& han, const std::string& sem) {
    const std::string latl = toLowerAscii(lat);
    if (!kwl.empty() && kwl == latl) return 400;
    if (!han.empty() && han == kw) return 380;
    if (!sem.empty() && semContainsExactToken(sem, kw)) return 300;
    if (!han.empty() && han.rfind(kw, 0) == 0) return 240;
    if (!han.empty() && han.find(kw) != std::string::npos) return 200;
    if (!sem.empty() && sem.find(kw) != std::string::npos) return 160;
    if (!latl.empty() && kwl.size() >= 2 && latl.find(kwl) != std::string::npos) return 100;
    return 0;
}

} // namespace

const std::array<std::string, 5>& LexiconEngine::axisNames() {
    static const std::array<std::string, 5> names = {"核", "映", "态", "标", "相"};
    return names;
}

int LexiconEngine::axisIndex(const std::string& name) {
    const auto& names = axisNames();
    for (int i = 0; i < 5; ++i) {
        if (names[i] == name) return i;
    }
    return -1;
}

const LexiconEngine& LexiconEngine::instance() {
    static const LexiconEngine engine;
    return engine;
}

LexiconEngine::LexiconEngine() {
    cores_.reserve(NC);
    for (const auto& row : kCoreBase) {
        for (size_t i = 0; i < 20; ++i) {
            CoreItem item;
            item.lat = std::string(row.lat) + kLatT[i];
            item.han = std::string(row.han) + kHanT[i];
            item.sem = row.sem;
            item.layer = row.layer;
            cores_.push_back(std::move(item));
        }
    }
    for (size_t idx = 0; idx < cores_.size(); ++idx) {
        latIndex_[0][cores_[idx].lat] = static_cast<uint64_t>(idx);
        hanIndex_[0][cores_[idx].han] = static_cast<uint64_t>(idx);
    }

    manis_.reserve(NM);
    for (const auto& row : kManiBase) {
        for (size_t i = 0; i < 12; ++i) {
            AxisItem item;
            item.lat = std::string(row.lat) + kLatA[i];
            item.han = std::string(row.han) + kHanA[i];
            item.sem = row.sem;
            manis_.push_back(std::move(item));
        }
    }
    for (size_t idx = 0; idx < manis_.size(); ++idx) {
        latIndex_[1][manis_[idx].lat] = static_cast<uint64_t>(idx);
        hanIndex_[1][manis_[idx].han] = static_cast<uint64_t>(idx);
    }

    stats_.reserve(NS);
    for (const auto& row : kStatBase) {
        for (size_t i = 0; i < 10; ++i) {
            AxisItem item;
            item.lat = std::string(row.lat) + kLatF[i];
            item.han = std::string(row.han) + kHanF[i];
            item.sem = row.sem;
            stats_.push_back(std::move(item));
        }
    }
    for (size_t idx = 0; idx < stats_.size(); ++idx) {
        latIndex_[2][stats_[idx].lat] = static_cast<uint64_t>(idx);
        hanIndex_[2][stats_[idx].han] = static_cast<uint64_t>(idx);
    }

    scals_.reserve(NK);
    for (const auto& row : kScalBase) {
        for (size_t i = 0; i < 8; ++i) {
            AxisItem item;
            item.lat = std::string(row.lat) + kLatS[i];
            item.han = std::string(row.han) + kHanS[i];
            item.sem = row.sem;
            scals_.push_back(std::move(item));
        }
    }
    for (size_t idx = 0; idx < scals_.size(); ++idx) {
        latIndex_[3][scals_[idx].lat] = static_cast<uint64_t>(idx);
        hanIndex_[3][scals_[idx].han] = static_cast<uint64_t>(idx);
    }

    phases_.reserve(NP);
    for (const auto& row : kPhaseBase) {
        phases_.push_back(AxisItem{row.lat, row.han, row.sem});
    }
    for (size_t idx = 0; idx < phases_.size(); ++idx) {
        latIndex_[4][phases_[idx].lat] = static_cast<uint64_t>(idx);
        hanIndex_[4][phases_[idx].han] = static_cast<uint64_t>(idx);
    }

    feelTable_ = {
        {"暖", 0.5, "靠近"}, {"烫", 0.8, "失控"},
        {"甜", 0.6, "黏"},   {"软", 0.5, "放松"},
        {"冷", 0.4, "收"},   {"空", 0.3, "等待"},
        {"酸", 0.5, "占有"}, {"紧", 0.6, "加速"},
        {"痒", 0.5, "想碰"}, {"刺", 0.7, "防御"},
        {"平", 0.3, "观察"}, {"湿", 0.8, "张开"},
    };
}

std::optional<WordEntry> LexiconEngine::decode(uint64_t n) const {
    if (n >= CAPACITY) return std::nullopt;
    uint64_t nn = n;
    const uint64_t p = nn % NP; nn /= NP;
    const uint64_t k = nn % NK; nn /= NK;
    const uint64_t s = nn % NS; nn /= NS;
    const uint64_t m = nn % NM; nn /= NM;
    const uint64_t c = nn % NC;

    const auto& C = cores_[c];
    const auto& M = manis_[m];
    const auto& S = stats_[s];
    const auto& K = scals_[k];
    const auto& P = phases_[p];

    WordEntry out;
    out.id = n;
    out.word = C.lat + "-" + M.lat + "-" + S.lat;
    if (!K.lat.empty()) out.word += "-" + K.lat;
    out.word += "·" + P.lat;

    out.han = C.han + M.han + S.han;
    if (!K.han.empty()) out.han += K.han;
    out.han += P.han;

    out.layer = C.layer;
    out.sem = C.sem + " / " + M.sem + " / " + S.sem;
    if (!K.sem.empty()) out.sem += " / " + K.sem;
    out.sem += " / " + P.sem;

    out.roots = {C.lat, M.lat, S.lat, K.lat.empty() ? "∅" : K.lat, P.lat};
    out.coord = {static_cast<int64_t>(c), static_cast<int64_t>(m),
                 static_cast<int64_t>(s), static_cast<int64_t>(k),
                 static_cast<int64_t>(p)};
    return out;
}

int64_t LexiconEngine::encode(std::string_view word) const {
    constexpr std::string_view dot = "·";
    const size_t dotPos = word.rfind(dot);
    if (dotPos == std::string_view::npos) return -1;

    const std::string_view head = word.substr(0, dotPos);
    const std::string_view ph = word.substr(dotPos + dot.size());

    std::array<std::string_view, 4> parts{};
    size_t partCount = 0;
    size_t start = 0;
    while (start <= head.size()) {
        if (partCount >= 4) return -1;
        size_t dash = head.find('-', start);
        if (dash == std::string_view::npos) {
            parts[partCount++] = head.substr(start);
            break;
        }
        parts[partCount++] = head.substr(start, dash - start);
        start = dash + 1;
    }
    if (partCount < 3 || partCount > 4) return -1;

    auto ci = latIndex_[0].find(parts[0]);
    auto mi = latIndex_[1].find(parts[1]);
    auto si = latIndex_[2].find(parts[2]);
    auto pi = latIndex_[4].find(ph);
    if (ci == latIndex_[0].end() || mi == latIndex_[1].end() ||
        si == latIndex_[2].end() || pi == latIndex_[4].end()) {
        return -1;
    }

    uint64_t kiVal = 0;
    if (partCount == 4) {
        auto ki = latIndex_[3].find(parts[3]);
        if (ki == latIndex_[3].end() || ki->second == 0) return -1;
        kiVal = ki->second;
    }

    return static_cast<int64_t>(idOf(ci->second, mi->second, si->second, kiVal, pi->second));
}

int64_t LexiconEngine::encodeHan(std::string_view han) const {
    // 零拷贝记录 UTF-8 码点字节偏移，避免每次切分产生堆分配
    std::array<size_t, 16> offsets{};
    size_t charCount = 0;
    size_t i = 0;
    while (i < han.size()) {
        if (charCount >= 15) return -1; // 最长合法汉译不超过 10 个汉字
        offsets[charCount++] = i;
        unsigned char c = static_cast<unsigned char>(han[i]);
        size_t len = 1;
        if (c < 0x80) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > han.size()) len = han.size() - i;
        i += len;
    }
    offsets[charCount] = han.size();
    if (charCount < 4) return -1;

    auto seg = [&](size_t a, size_t b) -> std::string_view {
        return han.substr(offsets[a], offsets[b] - offsets[a]);
    };

    auto piIt = hanIndex_[4].find(seg(charCount - 1, charCount));
    if (piIt == hanIndex_[4].end()) return -1;
    const uint64_t pi = piIt->second;

    const size_t L = charCount - 1;
    int64_t found = -1;
    int count = 0;
    for (size_t lc : {size_t(1), size_t(2)}) {
        if (lc > L) continue;
        auto ciIt = hanIndex_[0].find(seg(0, lc));
        if (ciIt == hanIndex_[0].end()) continue;
        for (size_t lm : {size_t(1), size_t(2)}) {
            if (lc + lm > L) continue;
            auto miIt = hanIndex_[1].find(seg(lc, lc + lm));
            if (miIt == hanIndex_[1].end()) continue;
            for (size_t ls : {size_t(1), size_t(2)}) {
                if (lc + lm + ls > L) continue;
                auto siIt = hanIndex_[2].find(seg(lc + lm, lc + lm + ls));
                if (siIt == hanIndex_[2].end()) continue;
                auto kiIt = hanIndex_[3].find(seg(lc + lm + ls, L));
                if (kiIt == hanIndex_[3].end()) continue;
                if (++count > 1) return -1;
                found = static_cast<int64_t>(idOf(ciIt->second, miIt->second, siIt->second, kiIt->second, pi));
            }
        }
    }
    return count == 1 ? found : -1;
}

std::vector<SearchHit> LexiconEngine::search(const std::string& keyword, const std::string& axis) const {
    const std::string kw = trimStr(keyword);
    if (kw.empty()) return {};
    const std::string kwl = toLowerAscii(kw);
    const auto& names = axisNames();

    struct ScoredHit {
        int score;
        int axisIdx;
        SearchHit hit;
    };
    std::vector<ScoredHit> scored;

    auto scanAxis = [&](int ai, size_t count, uint64_t toneLen, auto getRow) {
        if (!axis.empty() && axis != names[ai]) return;
        for (size_t bi = 0; bi < count; ++bi) {
            auto [lat, han, sem] = getRow(bi);
            if (lat.empty() && han.empty()) continue;
            int sc = hitScore(kw, kwl, lat, han, sem);
            if (sc > 0) {
                scored.push_back({sc, ai, SearchHit{names[ai], static_cast<uint64_t>(bi) * toneLen, lat, han, sem}});
            }
        }
    };

    scanAxis(0, sizeof(kCoreBase) / sizeof(kCoreBase[0]), 20, [](size_t i) {
        return std::tuple<std::string, std::string, std::string>{kCoreBase[i].lat, kCoreBase[i].han, kCoreBase[i].sem};
    });
    scanAxis(1, sizeof(kManiBase) / sizeof(kManiBase[0]), 12, [](size_t i) {
        return std::tuple<std::string, std::string, std::string>{kManiBase[i].lat, kManiBase[i].han, kManiBase[i].sem};
    });
    scanAxis(2, sizeof(kStatBase) / sizeof(kStatBase[0]), 10, [](size_t i) {
        return std::tuple<std::string, std::string, std::string>{kStatBase[i].lat, kStatBase[i].han, kStatBase[i].sem};
    });
    scanAxis(3, sizeof(kScalBase) / sizeof(kScalBase[0]), 8, [](size_t i) {
        return std::tuple<std::string, std::string, std::string>{kScalBase[i].lat, kScalBase[i].han, kScalBase[i].sem};
    });
    scanAxis(4, sizeof(kPhaseBase) / sizeof(kPhaseBase[0]), 1, [](size_t i) {
        return std::tuple<std::string, std::string, std::string>{kPhaseBase[i].lat, kPhaseBase[i].han, kPhaseBase[i].sem};
    });

    std::sort(scored.begin(), scored.end(), [](const ScoredHit& a, const ScoredHit& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.axisIdx != b.axisIdx) return a.axisIdx < b.axisIdx;
        return a.hit.index < b.hit.index;
    });

    std::vector<SearchHit> out;
    out.reserve(scored.size());
    for (auto& s : scored) out.push_back(std::move(s.hit));
    return out;
}

uint64_t LexiconEngine::resolveAxisInt(int ai, int64_t val) const {
    const auto sizes = axisSizes();
    const auto& names = axisNames();
    if (val < 0 || static_cast<uint64_t>(val) >= sizes[ai]) {
        throw std::runtime_error(names[ai] + "轴下标越界 0.." + std::to_string(sizes[ai] - 1) + ": " + std::to_string(val));
    }
    return static_cast<uint64_t>(val);
}

uint64_t LexiconEngine::resolveAxis(int ai, const std::string& val) const {
    const std::string v = trimStr(val);
    if (v.empty()) return 0;
    if (isAllDigits(v)) {
        try {
            return resolveAxisInt(ai, std::stoll(v));
        } catch (...) {
            throw std::runtime_error(axisNames()[ai] + "轴下标越界: " + val);
        }
    }
    auto li = latIndex_[ai].find(v);
    if (li != latIndex_[ai].end()) return li->second;
    auto hi = hanIndex_[ai].find(v);
    if (hi != hanIndex_[ai].end()) return hi->second;
    auto hits = search(v, axisNames()[ai]);
    if (!hits.empty()) return hits[0].index;
    throw std::runtime_error(axisNames()[ai] + "轴找不到「" + val + "」");
}

WordEntry LexiconEngine::resolveWord(const std::string& input) const {
    const std::string s = trimStr(input);
    if (s.empty()) throw std::runtime_error("类比词必须是编号或枢语词");
    if (isNonNegDigits(s)) {
        uint64_t id = 0;
        try {
            id = std::stoull(s);
        } catch (...) {
            throw std::runtime_error("编号越界");
        }
        auto d = decode(id);
        if (!d) throw std::runtime_error("编号越界 0.." + std::to_string(CAPACITY - 1));
        return *d;
    }
    int64_t id = encode(s);
    if (id < 0) id = encodeHan(s);
    if (id < 0) throw std::runtime_error("类比找不到「" + input + "」");
    return *decode(static_cast<uint64_t>(id));
}

WordEntry LexiconEngine::analogy(const std::string& a, const std::string& b, const std::string& c) const {
    const WordEntry A = resolveWord(a);
    const WordEntry B = resolveWord(b);
    const WordEntry C = resolveWord(c);
    const auto sizes = axisSizes();
    const std::array<int64_t, 5> av = {A.coord.c, A.coord.m, A.coord.s, A.coord.k, A.coord.p};
    const std::array<int64_t, 5> bv = {B.coord.c, B.coord.m, B.coord.s, B.coord.k, B.coord.p};
    const std::array<int64_t, 5> cv = {C.coord.c, C.coord.m, C.coord.s, C.coord.k, C.coord.p};
    std::array<uint64_t, 5> idx{};
    for (int i = 0; i < 5; ++i) {
        int64_t sz = static_cast<int64_t>(sizes[i]);
        int64_t v = ((cv[i] + bv[i] - av[i]) % sz + sz) % sz;
        idx[i] = static_cast<uint64_t>(v);
    }
    return *decode(idOf(idx[0], idx[1], idx[2], idx[3], idx[4]));
}

std::vector<NearHit> LexiconEngine::near(const std::string& input, int limit) const {
    int n = std::min(16, std::max(1, limit));
    const WordEntry word = resolveWord(input);
    const auto sizes = axisSizes();
    const auto& names = axisNames();
    const std::array<int64_t, 5> origin = {word.coord.c, word.coord.m, word.coord.s, word.coord.k, word.coord.p};
    std::vector<NearHit> out;
    for (int i = 0; i < 5; ++i) {
        for (int delta : {-1, 1}) {
            int64_t next = origin[i] + delta;
            if (next < 0 || next >= static_cast<int64_t>(sizes[i])) continue;
            auto idx = origin;
            idx[i] = next;
            auto w = *decode(idOf(idx[0], idx[1], idx[2], idx[3], idx[4]));
            out.push_back({1, names[i], std::move(w)});
        }
    }
    if (static_cast<int>(out.size()) > n) out.resize(n);
    return out;
}

std::optional<FeelEntry> LexiconEngine::matchFeel(const std::string& text) const {
    if (text.empty()) return std::nullopt;
    for (const auto& entry : feelTable_) {
        if (text.find(entry.word) != std::string::npos) {
            return entry;
        }
    }
    return std::nullopt;
}

WordEntry LexiconEngine::coinFromCoord(const Coord5D& coord) const {
    const uint64_t c = clampAxis(coord.c, NC);
    const uint64_t m = clampAxis(coord.m, NM);
    const uint64_t s = clampAxis(coord.s, NS);
    const uint64_t k = clampAxis(coord.k, NK);
    const uint64_t p = clampAxis(coord.p, NP);
    return *decode(idOf(c, m, s, k, p));
}

WordEntry LexiconEngine::autoCoin(const std::string& seed) const {
    uint32_t h = 2166136261u;
    const auto units = utf8ToUtf16(seed);
    for (uint16_t u : units) {
        h ^= static_cast<uint32_t>(u);
        h = static_cast<uint32_t>(h * 16777619u);
    }
    h ^= h << 13;
    h ^= h >> 17;
    h ^= h << 5;
    return *decode(static_cast<uint64_t>(h) % CAPACITY);
}

WordEntry LexiconEngine::coinFromState(double mood, double missYou, const std::string& seed) const {
    std::string layer;
    if (missYou > 0.7) layer = "映";
    else if (mood > 0.65) layer = "情感";
    else if (mood < 0.35) layer = "熵";
    else layer = "枢";

    if (!seed.empty()) {
        return autoCoin(seed + "|" + layer);
    }
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist(0, CAPACITY - 1);
    return *decode(dist(rng));
}

} // namespace shuyu
