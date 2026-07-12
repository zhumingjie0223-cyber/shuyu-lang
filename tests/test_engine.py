# 枢语引擎不变量测试(Python 版)
# 覆盖:容量公式 / 边界 / 双向寻址往返 / 词根表防篡改护栏 / 确定性造词
import hashlib
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import shuyu_engine as e  # noqa: E402


def sha(obj):
    # 与 JS 侧 JSON.stringify 对齐:紧凑分隔符、保留非 ASCII 原文
    return hashlib.sha256(
        json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


# 词根表防篡改护栏黄金指纹(与 tests/engine.test.mjs 同源同值)
GOLDEN_LEGACY20 = "3cd2c7b3845dbbd02886c71f9e4f4e5bb46e5e512f3c2098f47092074b63a30b"
GOLDEN_MANI = "a46d2a06a44f74e2f38eead710f3e3d2cddacc9403c90b323b36512eab9d149d"
GOLDEN_STAT = "f5122db790765b8e698374ab5a81f6e8b10ed954ef358dc1a09a44e743f32f38"
GOLDEN_SCAL = "3599bd52df338217739b1c0570909c329727c7572571badf01c08ddabda95069"
GOLDEN_PHASE = "08dba96e797c924eff89fdae3ce4dd285ce85701b30f6e09bd19af6c1458a69e"


def rows(table):
    # 元组 → 列表,对齐 JS 数组的 JSON 形态
    return [list(r) for r in table]


class TestCapacity(unittest.TestCase):
    def test_axes(self):
        self.assertEqual((e.NC, e.NM, e.NS, e.NK, e.NP), (1040, 180, 80, 64, 8))

    def test_capacity(self):
        self.assertEqual(e.CAP, 1040 * 180 * 80 * 64 * 8)
        self.assertEqual(e.CAP, 7_667_712_000)


class TestRootGuard(unittest.TestCase):
    """铁律:词根表只能在轴尾追加。指纹失配 = 有人动了历史词根,必须回滚。"""

    def test_legacy_core_fingerprint(self):
        self.assertEqual(sha(rows(e._CORE_BASE[:20])), GOLDEN_LEGACY20, "老 20 族核心词根被改动!")
        self.assertEqual(len(e._CORE_BASE), 52)

    def test_other_axes_fingerprints(self):
        self.assertEqual(sha(rows(e._MANI_BASE)), GOLDEN_MANI, "映轴词根被改动!")
        self.assertEqual(sha(rows(e._STAT_BASE)), GOLDEN_STAT, "态轴词根被改动!")
        self.assertEqual(sha(rows(e._SCAL_BASE)), GOLDEN_SCAL, "标轴词根被改动!")
        self.assertEqual(sha(rows(e._PHASE_BASE)), GOLDEN_PHASE, "相轴词根被改动!")

    def test_expanded_latin_unique(self):
        for name, table in [("CORES", e.CORES), ("MANIS", e.MANIS),
                            ("STATS", e.STATS), ("SCALS", e.SCALS), ("PHASES", e.PHASES)]:
            lats = [r[0] for r in table]
            self.assertEqual(len(set(lats)), len(lats), f"{name} 拉丁词形重复,反向寻址会撞名")


class TestAddressing(unittest.TestCase):
    def test_boundaries(self):
        self.assertEqual(e.decode(0)["词"], "Ao-cor-is·qi")
        self.assertEqual(e.decode(0)["汉"], "奥形凝起")
        self.assertEqual(e.decode(e.CAP - 1)["词"], "Glaxi-fncp-sta9-flxh·ying")
        with self.assertRaises(ValueError):
            e.decode(-1)
        with self.assertRaises(ValueError):
            e.decode(e.CAP)

    def test_v4_anchor(self):
        """旧容量边界 2,949,120,000 恰是 v4 首词——追加式扩容不错位历史编号。"""
        w = e.decode(2_949_120_000)
        self.assertEqual(w["词"], "Aur-cor-is·qi")
        self.assertEqual(w["层"], "显照")
        self.assertEqual(e.decode(2_949_119_999)["层"], "逻")

    def test_roundtrip_sampled(self):
        n = 5001
        for k in range(n):
            i = k * (e.CAP - 1) // (n - 1)
            w = e.decode(i)
            self.assertEqual(e.encode(w["词"]), i, f"往返失败 id={i} 词={w['词']}")

    def test_encode_invalid(self):
        for bad in ["不是枢语", "Ao-cor-is", "Xx-cor-is·qi", ""]:
            self.assertEqual(e.encode(bad), -1)


class TestCoin(unittest.TestCase):
    def test_auto_coin_deterministic(self):
        self.assertEqual(e.auto_coin("阿权")["id"], e.auto_coin("阿权")["id"])
        # 黄金值:与 JS 版 autoCoin('阿权') 位级一致
        self.assertEqual(e.auto_coin("阿权")["id"], 3834309906)

    def test_coin_from_coord_clamp(self):
        self.assertEqual(e.coin_from_coord({"c": 0, "m": 0, "s": 0, "k": 0, "p": 0})["id"], 0)
        clamped = e.coin_from_coord({"c": 99999, "m": -5, "s": 79.9, "k": 63, "p": 7})
        expect = e.coin_from_coord({"c": 1039, "m": 0, "s": 79, "k": 63, "p": 7})
        self.assertEqual(clamped["id"], expect["id"])


class TestPurity(unittest.TestCase):
    def test_han_pure_chinese(self):
        """汉译纯中文:抽样 2000 点,汉字段不得混入英文/数字/符号。"""
        n = 2000
        for k in range(n):
            i = k * (e.CAP - 1) // (n - 1)
            han = e.decode(i)["汉"]
            for ch in han:
                self.assertTrue("一" <= ch <= "鿿", f"id={i} 汉译混入非中文字符: {han!r}")


if __name__ == "__main__":
    unittest.main()
