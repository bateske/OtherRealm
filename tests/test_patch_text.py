# SPDX-License-Identifier: GPL-2.0-or-later
"""Patcher safety tests. Proprietary-data integration tests skip without assets."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import patch_text as patcher
from build_pack import DataError, verify_pack


class TextPatchTests(unittest.TestCase):
    def test_original_string_table_alignment_and_empty_strings(self):
        required = {1, 2, 0x1a, 0x35, 0x13d, 0x182, 0x185, 0x190, 0x265}
        values = sorted(set(range(1, 130)) | required)
        data = bytearray(b"fake")
        for identity in values:
            data.extend(struct.pack(">H", identity))
            data.extend((b"" if identity == 5 else b"TEST\rLINE") + b"\0\0")
            if len(data) & 1:
                data.append(0)
        data.extend(b"\xff\xff\xff\xff")
        strings, offset = patcher.extract_strings(bytes(data))
        self.assertEqual(offset, 4)
        self.assertEqual(strings[1], "TEST\nLINE")
        self.assertEqual(strings[5], "")
        self.assertEqual(set(strings), set(values))

    def test_reject_false_string_table(self):
        with self.assertRaises(DataError):
            patcher.extract_strings(b"\0\1not a complete table\0\0\xff\xff")

    def test_instruction_boundaries_ignore_opcode_looking_operands(self):
        code = bytes((0, 0x12, 0x12, 0x12, 0x12, 0, 1, 0, 0, 6, 0x11, 0))
        self.assertEqual([(pc, op) for pc, op, _ in patcher.instructions(code)],
                         [(0, 0), (4, 0x12), (10, 0x11)])

    def test_shape_lengths_all_forms(self):
        for opcode in range(0x40, 256):
            if opcode & 0x80:
                length = 4
            else:
                length = 5 + (not opcode & 0x30) + (not opcode & 0x0c) + ((opcode & 3) in (1, 2))
            stream = bytes((opcode,)) + bytes(length - 1) + b"\x11"
            self.assertEqual(list(patcher.instructions(stream))[-1][0], length)

    def test_branch_into_operand_rejected(self):
        with self.assertRaisesRegex(DataError, "non-instruction"):
            patcher.branch_targets(b"\x07\x00\x01\x11")

    def test_extensions_have_original_draw_string_length(self):
        code = b"\x1b\x00\x02\x00\x00\x06" + patcher.event(1) + b"\x11"
        self.assertEqual([pc for pc, _, _ in patcher.instructions(code)], [0, 6, 12])

    def test_text_fit_is_checked(self):
        with self.assertRaisesRegex(DataError, "does not fit"):
            patcher.text_resource("TOO LONG", 8, 6)
        with self.assertRaises(DataError):
            patcher.text_resource("A\nB", 4, 6)
        self.assertEqual(patcher.text_resource("A", 4, 6), b"TX\x01\x01\x04\x06\x02\0A\0")

    def test_formatting_preserves_words(self):
        self.assertEqual(patcher.wrapped("  Tests... with\n\n  spaces ", 25), "TESTS WITH\nSPACES")
        self.assertEqual(patcher.clean("A  N A L Y S I S"), "ANALYSIS")
        self.assertEqual(patcher.clean("Copyright }"), "COPYRIGHT (C)")

    def test_native_and_identity_resource_validation(self):
        scratch = ROOT / "private/test-temp"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            file = Path(directory) / "test.pak"
            resources = [patcher.Resource(patcher.text_resource("TEST", 16, 6), 8, 1),
                         patcher.Resource(b"ORID\x01\0\0\0", 9, 1)]
            file.write_bytes(patcher.encode_pack(resources, 11, 104 | (65 << 16)))
            self.assertEqual(verify_pack(file)["resources"], 2)
            resources[0].data = b"TX\x01\x01\x04\x06\x02\0A\0B\0"
            file.write_bytes(patcher.encode_pack(resources, 11, 104 | (65 << 16)))
            with self.assertRaisesRegex(DataError, "native text"):
                verify_pack(file)

    def test_already_patched_is_byte_identical(self):
        scratch = ROOT / "private/test-temp"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            source, output = Path(directory) / "in.pak", Path(directory) / "out.pak"
            resources = [patcher.Resource(patcher.text_resource("TEST", 16, 6), 8, 1),
                         patcher.Resource(b"ORID\x01\0\0\0", 9, 1)]
            source.write_bytes(patcher.encode_pack(resources, 11, 104 | (65 << 16)))
            result = patcher.patch(source, output)
            self.assertTrue(result["already_adapted"])
            self.assertEqual(result["original_pack_crc32"], 1)
            self.assertEqual(source.read_bytes(), output.read_bytes())

    def test_duplicate_identity_rejected(self):
        scratch = ROOT / "private/test-temp"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            source = Path(directory) / "in.pak"
            identity = patcher.Resource(b"ORID\0\0\0\0", 9, 1)
            source.write_bytes(patcher.encode_pack([identity, identity], 11, 104 | (65 << 16)))
            with self.assertRaisesRegex(DataError, "unique"):
                patcher.patch(source, Path(directory) / "out.pak")

    @unittest.skipUnless((ROOT / "private/sd/OTHERWRL.PAK").exists() and
                         (ROOT / "private/banks/another").exists(), "private Amiga assets unavailable")
    def test_real_pack_all_sites_control_flow_idempotence(self):
        scratch = ROOT / "private/test-temp"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            source, output = ROOT / "private/sd/OTHERWRL.PAK", Path(directory) / "out.pak"
            report = patcher.patch(source, output, (ROOT / "private/banks/another").read_bytes())
            self.assertEqual(len(report["sites"]), 147)
            self.assertEqual(len(report["vector_patches"]), 7)
            _, _, _, before = patcher.read_pack(source)
            _, flags, _, after = patcher.read_pack(output)
            self.assertEqual(flags, 11)
            for resource in patcher.CODE_HASHES:
                if resource == 126:
                    self.assertEqual(after[resource].data, patcher.event(2) + b"\x06\x07\x00\x06")
                    continue
                old, new = patcher.branch_targets(before[resource].data), patcher.branch_targets(after[resource].data)
                self.assertTrue(all(new[pc] == destination for pc, destination in old.items()))
                self.assertFalse(any(op == 0x12 for _, op, _ in patcher.instructions(after[resource].data)))
            for site in report["sites"]:
                if "box" in site:
                    self.assertLessEqual(site["position"][0] + site["box"][0], 104)
                    self.assertLessEqual(site["position"][1] + site["box"][1], 65)
            for resource in range(146):
                if resource not in patcher.CODE_HASHES and resource not in (71, 83):
                    self.assertEqual(after[resource], before[resource])
            again = Path(directory) / "again.pak"
            patcher.patch(output, again)
            self.assertEqual(output.read_bytes(), again.read_bytes())


if __name__ == "__main__":
    unittest.main()
