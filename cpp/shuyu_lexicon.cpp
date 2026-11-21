/**
 * 枢语 5 维乘法语义引擎 — 纯 C++17 原生实现 (shuyu_lexicon.cpp)
 * © 阿权/路飞
 * 严格保持与 shuyu_engine.py / lexicon.js 相同的词根顺序与扩展规则（7,667,712,000 词）。
 */

#include "shuyu_lexicon.hpp"

#include <algorithm>
#include <random>

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
    // v4 扩充：32 个新核心语义族
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

// 将 UTF-8 字符串按 UTF-16 code unit 遍历（与 JS charCodeAt 保持完全一致，确保 autoCoin 同构）
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

} // namespace

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
            coreIndex_.emplace(item.lat, static_cast<uint64_t>(cores_.size()));
            cores_.push_back(std::move(item));
        }
    }

    manis_.reserve(NM);
    for (const auto& row : kManiBase) {
        for (size_t i = 0; i < 12; ++i) {
            AxisItem item;
            item.lat = std::string(row.lat) + kLatA[i];
            item.han = std::string(row.han) + kHanA[i];
            item.sem = row.sem;
            maniIndex_.emplace(item.lat, static_cast<uint64_t>(manis_.size()));
            manis_.push_back(std::move(item));
        }
    }

    stats_.reserve(NS);
    for (const auto& row : kStatBase) {
        for (size_t i = 0; i < 10; ++i) {
            AxisItem item;
            item.lat = std::string(row.lat) + kLatF[i];
            item.han = std::string(row.han) + kHanF[i];
            item.sem = row.sem;
            statIndex_.emplace(item.lat, static_cast<uint64_t>(stats_.size()));
            stats_.push_back(std::move(item));
        }
    }

    scals_.reserve(NK);
    for (const auto& row : kScalBase) {
        for (size_t i = 0; i < 8; ++i) {
            AxisItem item;
            item.lat = std::string(row.lat) + kLatS[i];
            item.han = std::string(row.han) + kHanS[i];
            item.sem = row.sem;
            scalIndex_.emplace(item.lat, static_cast<uint64_t>(scals_.size()));
            scals_.push_back(std::move(item));
        }
    }

    phases_.reserve(NP);
    for (const auto& row : kPhaseBase) {
        AxisItem item{row.lat, row.han, row.sem};
        phaseIndex_.emplace(item.lat, static_cast<uint64_t>(phases_.size()));
        phases_.push_back(std::move(item));
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

    return out;
}

int64_t LexiconEngine::encode(const std::string& word) const {
    const std::string dot = "·";
    const size_t dotPos = word.find(dot);
    if (dotPos == std::string::npos) return -1;

    const std::string head = word.substr(0, dotPos);
    const std::string ph = word.substr(dotPos + dot.size());

    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= head.size()) {
        size_t dash = head.find('-', start);
        if (dash == std::string::npos) {
            parts.push_back(head.substr(start));
            break;
        }
        parts.push_back(head.substr(start, dash - start));
        start = dash + 1;
    }
    if (parts.size() < 3 || parts.size() > 4) return -1;

    auto ci = coreIndex_.find(parts[0]);
    auto mi = maniIndex_.find(parts[1]);
    auto si = statIndex_.find(parts[2]);
    auto pi = phaseIndex_.find(ph);
    if (ci == coreIndex_.end() || mi == maniIndex_.end() ||
        si == statIndex_.end() || pi == phaseIndex_.end()) {
        return -1;
    }

    uint64_t kiVal = 0;
    if (parts.size() == 4) {
        auto ki = scalIndex_.find(parts[3]);
        if (ki == scalIndex_.end()) return -1;
        kiVal = ki->second;
    }

    const uint64_t id = ((((ci->second * NM) + mi->second) * NS + si->second) * NK + kiVal) * NP + pi->second;
    return static_cast<int64_t>(id);
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
    const uint64_t id = ((((c * NM) + m) * NS + s) * NK + k) * NP + p;
    return *decode(id);
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
