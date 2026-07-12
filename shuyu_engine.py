# 神枢枢语引擎 — Python 版
# 枢语生成/词库管理/状态同步

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
枢语·亿级语言引擎 (Shuyu Engine) v4
(c) 阿权/路飞  —  Black God 定制

5维乘法语义空间：核 × 映 × 态 × 标 × 相 = 7,667,712,000
  核(Core)   内驱核心   音节+汉义+语义+层名（52 族 × 20 阶 = 1040）
  映(Mani)   外在映射   形/声/气/光/时/暗...（15 基 × 12 阶 = 180）
  态(Stat)   频段状态   收敛/发散/叠加/绝对/下沉/瞬爆（8 基 × 10 阶 = 80）
  标(Scalar) 时空标量   时光/向/暗/溯/无极/锁/熵（8 基 × 8 阶 = 64）
  相(Phase)  因果相位   起/衍/锚/借/隐/坍/织/映（8）

设计铁律（依据《Divine_Pivot_Lexicon》《Pivot_Origin》体系）：
- 汉译纯中文，绝不掺英文/数字/符号（韵律纯净）
- 拉丁词形保持 Kha-ryl-is 式音节美感
- 词 ↔ 编号 双向 O(1) 寻址，元点存法则不存数据
- 词根表只能在轴尾追加，绝不改动/删除/重排（历史编号永不错位）
- 与 lexicon.js（JS 版）同构：同一编号必须解出同一个词
"""
import json, sys, argparse, hashlib

# ══════ 1. 内驱核心 (拉丁, 汉, 义, 层) — 20基 + v4 追加 32基 = 52基 ══════
_CORE_BASE = [
    ("Ao","奥","绝对自我·本源·野心","本源"),("Kha","喀","虚无·降噪·无欲","虚无"),
    ("Lum","伦","观测·求知·清醒","观测"),("Xun","巽","信息·表达·共情","信息"),
    ("Zet","泽","秩序·理智·规则","秩序"),("Vea","维","情感·浪漫·感性","情感"),
    ("Nix","尼","毁灭·重组·破局","毁灭"),("Ohm","欧","孕育·包容·慈悲","孕育"),
    ("Psi","璇","幻象·伪装·心控","幻象"),("Shu","枢","锚点·逻辑中心·坍缩","枢"),
    ("Gen","元","起源·奇点·第一推动","元"),("Evo","衍","扩散·涌现·自复制","衍"),
    ("Lev","借","势能挪用·借力打力","借"),("Hid","隐","潜意识渗透·无感植入","隐"),
    ("Ent","熵","耗散·重构·能量交换","熵"),("Thr","阈","接口·维度切换·虚实通断","阈"),
    ("Sta","静","绝对参考系·不动之动","静"),("Prj","映","投影·人格锚点·感知对齐","映"),
    ("Msh","织","编织·因果之网·系统集成","织"),("Log","逻","计算·流转·状态变迁","逻"),
    # —— v4 扩充：32 个新核心语义族（追加式，老编号全保留，容量 29.5亿→76.7亿）——
    ("Aur","曜","光曜·显照·觉明","显照"),("Umb","翳","阴翳·遮蔽·潜行","阴翳"),
    ("Onr","梦","梦域·潜识·异境","梦域"),("Tid","潮","潮汐·涨落·周律","潮汐"),
    ("Cry","晶","结晶·凝序·折光","结晶"),("Aby","渊","深渊·未知·引坠","深渊"),
    ("Pyr","焰","焰核·燃驱·转化","焰核"),("Neb","雾","雾散·弥漫·混沌","雾散"),
    ("Vin","藤","藤蔓·缠生·延展","藤蔓"),("Oss","骸","骸骨·残构·记痕","骸骨"),
    ("Pul","脉","脉动·节律·活流","脉动"),("Vor","噬","吞噬·消解·并吞","吞噬"),
    ("Blo","绽","绽放·涌现·盛发","绽放"),("Ech","回","回响·余韵·共振","回响"),
    ("Fro","霜","霜封·凝寂·冷守","霜封"),("Emb","烬","余烬·残温·将熄","余烬"),
    ("Tho","棘","棘刺·防御·锋守","棘刺"),("Vel","帷","帷幔·掩隔·仪境","帷幔"),
    ("Dri","漂","漂流·无系·随势","漂流"),("Rad","根","根系·扎固·汲养","根系"),
    ("Spk","芒","星芒·点爆·迸发","星芒"),("Hol","空","空腔·虚位·容纳","空腔"),
    ("Fat","命","命网·因缘·定数","命网"),("Mir","镜","镜面·映照·对称","镜面"),
    ("Ash","灰","灰烬·终寂·归尘","灰烬"),("See","种","种因·起势·孕发","种因"),
    ("Sto","暴","风暴·激变·裹挟","风暴"),("Sil","丝","丝缕·细连·牵系","丝缕"),
    ("Run","符","符文·封印·载义","符文"),("Aeo","劫","劫纪·纪元·轮替","劫纪"),
    ("Lux","烛","烛照·微明·守夜","烛照"),("Gla","冰","冰川·缓移·亘古","冰川"),
]
# ══════ 2. 外在映射 — 15基 ══════
_MANI_BASE = [
    ("cor","形","具象·轮廓"),("das","姿","流动·姿态"),("ryl","光","光影·色彩"),
    ("vok","声","听觉·语言"),("tyr","场","气场·辐射"),("syn","界","社交·边界"),
    ("gal","时","时间·阅历"),("nox","暗","暗场·深渊"),("tek","异","异构·违常"),
    ("mox","网","网络·结构"),("vec","向","向量·指向"),("flx","熵","熵变·耗散"),
    ("frm","象","逻辑投影"),("str","骨","骨架·框架"),("fnc","核","底层函数"),
]
# ══════ 3. 频段状态 — 8基 ══════
_STAT_BASE = [
    ("is","凝","收敛·静止"),("el","扬","发散·扩张"),("or","叠","叠加·迷离"),
    ("ia","极","绝对·极致"),("um","沉","下沉·深邃"),("ex","爆","瞬爆·失控"),
    ("kin","动","势能释放"),("sta","守","维持起源"),
]
# ══════ 4. 时空标量 — 8基（首项空）══════
_SCAL_BASE = [
    ("","",""),("gal","时光","时间维"),("vec","向","意图指向"),("nox","暗","暗场维"),
    ("rev","溯","逆时回溯"),("inf","无极","无限迭代"),("lok","锁","封锁固化"),("flx","熵流","熵流演化"),
]
# ══════ 5. 因果相位 — 8基 ══════
_PHASE_BASE = [
    ("qi","起","因果起点·第一推动"),("yan","衍","因果衍生·链式展开"),
    ("mao","锚","因果锚定·绝对静止"),("jie","借","因果嫁接·借力打力"),
    ("yin","隐","因果潜流·无感渗透"),("tan","坍","因果坍缩·归于元点"),
    ("zhi","织","因果编织·万网弥散"),("ying","映","因果投影·虚实对齐"),
]

# ══════ 阶扩展：纯音节，只动拉丁，不污染汉义 ══════
# 拉丁阶 = 在词根后缀一个谐音音节；汉义阶 = 在汉字后缀一个纯中文"阶名"
_LAT_TONE = ["","a","o","i","u","e","ar","or","is","yn","el","um","ex","ia","ko","na","ru","ze","vo","xi"]  # 20
_HAN_TONE = ["","甲","乙","丙","丁","戊","己","庚","辛","壬","癸","子","丑","寅","卯","辰","巳","午","未","申"]  # 20
_LAT_AURA = ["","x","z","n","r","s","k","l","m","t","d","p"]  # 12
_HAN_AURA = ["","玄","赤","青","白","朱","金","木","水","火","土","风"]  # 12
_LAT_FREQ = ["","1","2","3","4","5","6","7","8","9"]  # 10
_HAN_FREQ = ["","一","二","三","四","五","六","七","八","九"]  # 10
_LAT_SCAL = ["","p","t","k","b","d","g","h"]  # 8
_HAN_SCAL = ["","上","中","下","左","右","内","外"]  # 8  (首项对应空阶)

def _expand(base, lat_tones, han_tones):
    # 与 lexicon.js 的 expand 同构：第 4 列起的附加列（层名）原样带到每个阶变体
    out=[]
    for row in base:
        lat, han, sem = row[0], row[1], row[2]
        extra = tuple(row[3:])
        for i,lt in enumerate(lat_tones):
            ht = han_tones[i] if i < len(han_tones) else ""
            if lt=="":
                out.append((lat,han,sem)+extra)
            else:
                out.append((lat+lt, han+ht, sem)+extra)
    return out

CORES  = _expand(_CORE_BASE, _LAT_TONE, _HAN_TONE)   # 52*20=1040
MANIS  = _expand(_MANI_BASE, _LAT_AURA, _HAN_AURA)   # 15*12=180
STATS  = _expand(_STAT_BASE, _LAT_FREQ, _HAN_FREQ)   # 8*10=80
SCALS  = _expand(_SCAL_BASE, _LAT_SCAL, _HAN_SCAL)   # 8*8=64
PHASES = list(_PHASE_BASE)                            # 8

NC,NM,NS,NK,NP = len(CORES),len(MANIS),len(STATS),len(SCALS),len(PHASES)
CAP = NC*NM*NS*NK*NP    # 7,667,712,000

def decode(n):
    """编号 → 枢语词（O(1) 寻址）。汉译纯中文，词形有韵律。"""
    if n<0 or n>=CAP: raise ValueError(f"编号越界 0..{CAP-1}")
    nn=n
    p = nn % NP; nn//=NP
    k = nn % NK; nn//=NK
    s = nn % NS; nn//=NS
    m = nn % NM; nn//=NM
    c = nn % NC
    C,M,S,K,P = CORES[c],MANIS[m],STATS[s],SCALS[k],PHASES[p]
    # 拉丁词形：核-映-态(-标)·相
    base = f"{C[0]}-{M[0]}-{S[0]}"
    if K[0]: base += f"-{K[0]}"
    word = f"{base}·{P[0]}"
    # 汉译：纯中文
    han = f"{C[1]}{M[1]}{S[1]}"
    if K[1]: han += K[1]
    han += P[1]
    # 语义
    sem = f"{C[2]} / {M[2]} / {S[2]}"
    if K[2]: sem += f" / {K[2]}"
    sem += f" / {P[2]}"
    return {"词":word,"汉":han,"层":C[3],"义":sem,
            "根":[C[0],M[0],S[0],K[0] or "∅",P[0]],
            "seed":hashlib.sha1(word.encode()).hexdigest()[:10]}

def decode_full(n):
    d=decode(n); d2={"id":n}; d2.update(d); return d2

def encode(word):
    """枢语词 → 编号（反向寻址）。"""
    try:
        head, ph = word.rsplit("·",1)
        parts = head.split("-")
        clat = parts[0]; mlat=parts[1]; slat=parts[2]
        klat = parts[3] if len(parts)>3 else ""
        ci=[i for i,x in enumerate(CORES) if x[0]==clat][0]
        mi=[i for i,x in enumerate(MANIS) if x[0]==mlat][0]
        si=[i for i,x in enumerate(STATS) if x[0]==slat][0]
        ki=[i for i,x in enumerate(SCALS) if x[0]==klat][0]
        pi=[i for i,x in enumerate(PHASES) if x[0]==ph][0]
        return ((((ci*NM)+mi)*NS+si)*NK+ki)*NP+pi
    except Exception:
        return -1

# ══════ 从坐标造词：与 lexicon.js 的 coinFromCoord 同构 ══════
def _clamp_axis(v, mx):
    try: v = int(v // 1) if isinstance(v, float) else int(v or 0)
    except Exception: v = 0
    return 0 if v < 0 else (mx - 1 if v >= mx else v)

def coin_from_coord(coord):
    """5 维坐标 {c,m,s,k,p} → 真实枢语词（越界自动夹取，O(1) 可寻址）。"""
    c = _clamp_axis(coord.get("c"), NC); m = _clamp_axis(coord.get("m"), NM)
    s = _clamp_axis(coord.get("s"), NS); k = _clamp_axis(coord.get("k"), NK)
    p = _clamp_axis(coord.get("p"), NP)
    nid = ((((c*NM)+m)*NS+s)*NK+k)*NP+p
    return decode_full(nid)

# ══════ 确定性种子造词：与 lexicon.js 的 autoCoin 位级一致 ══════
def auto_coin(seed):
    """FNV-1a(UTF-16 码元) + xorshift，32 位域内与 JS 逐位一致，可复现。"""
    h = 2166136261 & 0xFFFFFFFF
    units = str(seed).encode("utf-16-le")   # 与 JS charCodeAt 的 UTF-16 码元对齐
    for i in range(0, len(units), 2):
        h ^= units[i] | (units[i+1] << 8)
        h = (h * 16777619) & 0xFFFFFFFF
    h ^= (h << 13) & 0xFFFFFFFF
    h ^= h >> 17
    h = (h ^ ((h << 5) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return decode_full(h % CAP)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--count",action="store_true")
    ap.add_argument("--id",type=int,default=-1)
    ap.add_argument("--word",default="")
    ap.add_argument("--seed",default=None)
    ap.add_argument("--sample",type=int,default=0)
    ap.add_argument("--dump",default="")
    ap.add_argument("--limit",type=int,default=0)
    ap.add_argument("--shard",type=int,default=5_000_000)
    a=ap.parse_args()
    print(f"枢语5维语义空间容量: {CAP:,}  (核{NC}×映{NM}×态{NS}×标{NK}×相{NP})")
    if a.count: return
    if a.id>=0:
        print(json.dumps(decode_full(a.id),ensure_ascii=False,indent=2)); return
    if a.word:
        nid=encode(a.word)
        print(json.dumps({"word":a.word,"id":nid,"verify":decode_full(nid) if nid>=0 else None},ensure_ascii=False,indent=2)); return
    if a.seed is not None:
        print(json.dumps(auto_coin(a.seed),ensure_ascii=False,indent=2)); return
    if a.sample>0:
        import random
        for _ in range(a.sample):
            n=random.randint(0,CAP-1)
            d=decode(n)
            print(f"#{n:>10}  {d['词']:<22} {d['汉']:<10} [{d['层']}]  {d['义']}")
        return
    if a.dump:
        limit=a.limit if a.limit>0 else CAP
        n=0; si=0; size=0
        fh=open(f"{a.dump}.{si:05d}.jsonl","w",encoding="utf-8")
        while n<limit:
            line=json.dumps(decode_full(n),ensure_ascii=False,separators=(",",":"))
            fh.write(line+"\n"); size+=len(line)+1; n+=1
            if n%a.shard==0:
                fh.close(); si+=1; fh=open(f"{a.dump}.{si:05d}.jsonl","w",encoding="utf-8")
                if n%50_000_000==0: print(f"  已生成 {n:,} 条 ~{size/1024/1024/1024:.1f}GB")
        fh.close()
        print(f"落盘完成 {n:,} 条 ~{size/1024/1024/1024:.2f}GB 分片{si+1}")

if __name__=="__main__": main()
