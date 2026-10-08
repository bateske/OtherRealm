"""Synthetic corruption/format tests; optional integration with locally supplied ADFs."""
import hashlib
from contextlib import contextmanager
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

import build_pack as pack


@contextmanager
def test_directory():
    root = (Path(__file__).resolve().parent.parent / "private" / "test-temp").resolve()
    root.mkdir(parents=True, exist_ok=True)
    temporary = tempfile.TemporaryDirectory(dir=root)
    # Check the recursive cleanup target stays inside this task's scratch folder.
    Path(temporary.name).resolve().relative_to(root)
    try:
        yield temporary.name
    finally:
        temporary.cleanup()


def literal_stream(value: bytes) -> bytes:
    # A short literal packet fitting in the initial 31-bit bit register.
    assert 1 <= len(value) <= 3
    bits = [0, 0] + [((len(value) - 1) >> i) & 1 for i in (2, 1, 0)]
    for byte in reversed(value):
        bits.extend((byte >> i) & 1 for i in range(7, -1, -1))
    word = (1 << len(bits)) | sum(bit << i for i, bit in enumerate(bits))
    return struct.pack(">III", word, word, len(value))


def put32(block, offset, value):
    struct.pack_into(">I", block, offset, value)


def checked(block):
    put32(block, 20, -sum(struct.unpack(">128I", block)) & 0xFFFFFFFF)
    return block


def ofs_fixture() -> bytes:
    image = bytearray(20 * 512)
    image[:4] = b"DOS\0"
    put32(image, 8, 10)
    root = bytearray(512)
    for offset, value in ((0, 2), (12, 72), (24, 11), (508, 1)):
        put32(root, offset, value)
    image[10*512:11*512] = checked(root)
    header = bytearray(512)
    for offset, value in ((0, 2), (4, 11), (16, 12), (324, 3), (508, 0xFFFFFFFD)):
        put32(header, offset, value)
    header[432:439] = b"\x06bank01"
    image[11*512:12*512] = checked(header)
    data = bytearray(512)
    for offset, value in ((0, 8), (4, 11), (8, 1), (12, 3)):
        put32(data, offset, value)
    data[24:27] = b"abc"
    image[12*512:13*512] = checked(data)
    return bytes(image)


class CompressionTests(unittest.TestCase):
    def test_literal_byte_order(self):
        for value in (b"A", b"AZ", b"ABC", b"\0\xff\x80"):
            self.assertEqual(pack.bytekiller_unpack(literal_stream(value), len(value)), value)

    def test_crc_rejected(self):
        data = bytearray(literal_stream(b"ABC"))
        data[7] ^= 1
        with self.assertRaisesRegex(pack.DataError, "checksum"):
            pack.bytekiller_unpack(data, 3)

    def test_bad_size_rejected(self):
        with self.assertRaisesRegex(pack.DataError, "size"):
            pack.bytekiller_unpack(literal_stream(b"ABC"), 4)

    def test_truncated_stream_rejected(self):
        with self.assertRaises(pack.DataError):
            pack.bytekiller_unpack(bytes(8), 3)

    def test_invalid_backreference_rejected(self):
        # 01 selects two-byte reference; zero offset is not decoded data.
        word = (1 << 10) | 2
        with self.assertRaisesRegex(pack.DataError, "reference"):
            pack.bytekiller_unpack(struct.pack(">III", word, word, 2), 2)


class DiskTests(unittest.TestCase):
    def read(self, data):
        with test_directory() as temporary:
            path = Path(temporary) / "test.adf"
            path.write_bytes(data)
            return pack.OfsDisk(path).files()

    def test_linked_ofs_file(self):
        self.assertEqual(self.read(ofs_fixture()), {"bank01": b"abc"})

    def test_nonbootable_root_pointer_falls_back(self):
        image = bytearray(ofs_fixture())
        put32(image, 8, 0x44534F00)
        self.assertEqual(self.read(image), {"bank01": b"abc"})

    def test_data_corruption_rejected(self):
        image = bytearray(ofs_fixture())
        image[12*512 + 24] ^= 1
        with self.assertRaisesRegex(pack.DataError, "checksum"):
            self.read(image)

    def test_link_cycle_rejected(self):
        image = bytearray(ofs_fixture())
        block = bytearray(image[12*512:13*512])
        put32(block, 16, 12)
        put32(block, 20, 0)
        image[12*512:13*512] = checked(block)
        with self.assertRaisesRegex(pack.DataError, "cycle"):
            self.read(image)


class BitmapTests(unittest.TestCase):
    def test_planes_and_nibble_order(self):
        # A whole image tiled 0..15 horizontally checks all plane bits.
        image = bytearray(32000)
        for plane, pair in enumerate((b"\x55\x55", b"\x33\x33", b"\x0f\x0f", b"\x00\xff")):
            image[plane*8000:(plane+1)*8000] = pair * 4000
        self.assertEqual(pack.convert_bitmap(image, 320), bytes.fromhex("0123456789abcdef") * 4000)

    def test_target_dimensions(self):
        self.assertEqual(pack.convert_bitmap(bytes([255])*32000, 104), bytes([255])*3380)


class PackTests(unittest.TestCase):
    def fixture(self):
        data = bytearray(1024)
        pack.HEADER.pack_into(data, 0, b"ORW1", 1, 32, 1, 32, 512, 1024, 1, 0)
        pack.ENTRY.pack_into(data, 32, 512, 3, zlib.crc32(b"abc"), 4, 1)
        data[512:515] = b"abc"
        return data

    def verify(self, data):
        with test_directory() as temporary:
            path = Path(temporary) / "pack.bin"
            path.write_bytes(data)
            return pack.verify_pack(path)

    def test_valid_pack(self):
        self.assertEqual(self.verify(self.fixture())["present"], 1)

    def test_pack_crc_corruption(self):
        data = self.fixture()
        data[514] ^= 1
        with self.assertRaisesRegex(pack.DataError, "CRC32"):
            self.verify(data)

    def test_unaligned_resource(self):
        data = self.fixture()
        struct.pack_into("<I", data, 32, 513)
        with self.assertRaisesRegex(pack.DataError, "bounds"):
            self.verify(data)

    def test_truncated_pack(self):
        with self.assertRaisesRegex(pack.DataError, "length"):
            self.verify(self.fixture()[:-1])

    def test_inconsistent_bitmap_flags(self):
        data = self.fixture()
        struct.pack_into("<H", data, 46, 3)
        with self.assertRaisesRegex(pack.DataError, "bitmap"):
            self.verify(data)

    def scene_fixture(self):
        data = bytearray(2560)
        pack.HEADER.pack_into(data, 0, b"ORW1", 1, 32, 4, 32, 512, 2560, 5, 0)
        for index, (kind, content) in enumerate(((3, bytes(32)), (4, b"\x11"), (5, b"shape"),
                                               (7, struct.pack("<5H", 16001, 0, 1, 2, 0)))):
            offset = (index + 1) * 512
            pack.ENTRY.pack_into(data, 32 + 16 * index, offset, len(content), zlib.crc32(content), kind, 1)
            data[offset:offset + len(content)] = content
        return data

    def test_vector_only_scene_map(self):
        self.assertEqual(self.verify(self.scene_fixture())["present"], 4)

    def test_scene_reference_rejected(self):
        data = self.scene_fixture()
        struct.pack_into("<H", data, 2048 + 4, 99)
        struct.pack_into("<I", data, 32 + 3 * 16 + 8, zlib.crc32(data[2048:2058]))
        with self.assertRaisesRegex(pack.DataError, "outside directory"):
            self.verify(data)

    def test_scene_resource_type_rejected(self):
        data = self.scene_fixture()
        struct.pack_into("<H", data, 32 + 16 + 12, 0)
        with self.assertRaisesRegex(pack.DataError, "incompatible resource"):
            self.verify(data)


class PrivateIntegrationTests(unittest.TestCase):
    def test_supplied_images_roundtrip_and_target(self):
        root = Path(__file__).resolve().parent.parent
        images = [root / f"AnotherWorld_Disk{disk}_nologo_noprotec.adf" for disk in "AB"]
        if not all(image.exists() for image in images):
            self.skipTest("private ADF images are not present")
        with test_directory() as temporary:
            directory = Path(temporary)
            raw_path, small_path = directory / "raw.pak", directory / "small.pak"
            raw = pack.build(images, raw_path)
            small = pack.build(images, small_path, 104)
            self.assertEqual(raw["pack"]["present"], 138)
            self.assertEqual(raw["directory_offset"], 0x5EC2)
            self.assertEqual((small["pack"]["width"], small["pack"]["height"]), (104, 65))
            raw_bytes, small_bytes = raw_path.read_bytes(), small_path.read_bytes()
            for a, b in zip(raw["resources"], small["resources"]):
                original = raw_bytes[a["offset"]:a["offset"]+a["size"]]
                target = small_bytes[b["offset"]:b["offset"]+b["size"]]
                self.assertEqual(target, pack.convert_bitmap(original, 104) if original and a["type"] == 2 else original)
            self.assertEqual(hashlib.sha256(raw_bytes).hexdigest(), "eab6d39a7d0339a071542ffb849bc24db3bcee26319641cff99aeba7c0d125d0")


if __name__ == "__main__":
    unittest.main()
