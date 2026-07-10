# -*- coding: utf-8 -*-
"""shuyu_engine.py 单元测试（标准库 unittest，零依赖）。

运行：python3 -m unittest discover -s tests -v
"""
import json
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

import shuyu_engine as eng  # noqa: E402


class TestAxes(unittest.TestCase):
    def test_axis_sizes(self):
        self.assertEqual(eng.NC, 1040)   # 52核 × 20阶
        self.assertEqual(eng.NM, 180)    # 15映 × 12阶
        self.assertEqual(eng.NS, 80)     # 8态 × 10阶
        self.assertEqual(eng.NK, 64)     # 8标 × 8阶
        self.assertEqual(eng.NP, 8)

    def test_capacity(self):
        self.assertEqual(eng.CAP, 7_667_712_000)

    def test_latin_forms_unique_per_axis(self):
        """encode 的良定义性：每条轴上拉丁词形必须唯一。"""
        for name, axis in [("CORES", eng.CORES), ("MANIS", eng.MANIS),
                           ("STATS", eng.STATS), ("SCALS", eng.SCALS),
                           ("PHASES", eng.PHASES)]:
            lats = [x[0] for x in axis]
            self.assertEqual(len(lats), len(set(lats)), f"{name} 存在重复拉丁词形")

    def test_core_layers_complete(self):
        """52 个核心族每个都有非空层名。"""
        self.assertEqual(len(eng._CORE_BASE), 52)
        for row in eng._CORE_BASE:
            self.assertEqual(len(row), 4)
            self.assertTrue(row[3])


class TestDecode(unittest.TestCase):
    def test_boundaries(self):
        eng.decode(0)
        eng.decode(eng.CAP - 1)

    def test_out_of_range(self):
        with self.assertRaises(ValueError):
            eng.decode(-1)
        with self.assertRaises(ValueError):
            eng.decode(eng.CAP)

    def test_type_check(self):
        with self.assertRaises(TypeError):
            eng.decode("0")
        with self.assertRaises(TypeError):
            eng.decode(True)

    def test_word_shape(self):
        d = eng.decode(888_888_888)
        self.assertEqual(d["词"], "Nix-teks-ia1-h·qi")
        self.assertEqual(d["层"], "毁灭")
        self.assertEqual(len(d["根"]), 5)

    def test_han_pure_chinese(self):
        """设计铁律：汉译纯中文，不掺英文/数字/符号。"""
        for n in range(0, eng.CAP, eng.CAP // 997):
            han = eng.decode(n)["汉"]
            self.assertTrue(re.fullmatch(r"[一-鿿]+", han),
                            f"id={n} 汉译不纯: {han}")


class TestRoundtrip(unittest.TestCase):
    def test_decode_encode_roundtrip_sampled(self):
        """确定性跨全空间抽样：decode → encode 必须还原编号。"""
        step = eng.CAP // 4999
        for n in range(0, eng.CAP, step):
            word = eng.decode(n)["词"]
            self.assertEqual(eng.encode(word), n, f"往返失败 id={n} word={word}")

    def test_encode_invalid(self):
        for bad in ["", "abc", "Nix-teks·qi", "Nix-teks-ia1-h-x·qi",
                    "Xxx-teks-ia1·qi", "Nix-xxx-ia1·qi", "Nix-teks-ia1·xx",
                    "Nix-teks-ia1-·qi",  # 显式空标量不合法
                    None]:
            self.assertEqual(eng.encode(bad), -1, f"应判非法: {bad!r}")


class TestCLI(unittest.TestCase):
    def _run(self, *args):
        return subprocess.run([sys.executable, str(ROOT / "shuyu_engine.py"), *args],
                              capture_output=True, text=True, timeout=60)

    def test_count(self):
        r = self._run("--count")
        self.assertEqual(r.returncode, 0)
        self.assertIn("7,667,712,000", r.stdout)

    def test_id(self):
        r = self._run("--id", "888888888")
        self.assertEqual(r.returncode, 0)
        payload = json.loads(r.stdout.split("\n", 1)[1])
        self.assertEqual(payload["词"], "Nix-teks-ia1-h·qi")

    def test_word_roundtrip(self):
        r = self._run("--word", "Nix-teks-ia1-h·qi")
        self.assertEqual(r.returncode, 0)
        payload = json.loads(r.stdout.split("\n", 1)[1])
        self.assertEqual(payload["id"], 888888888)

    def test_bad_word_exit_code(self):
        r = self._run("--word", "不是枢语")
        self.assertEqual(r.returncode, 1)


if __name__ == "__main__":
    unittest.main()
