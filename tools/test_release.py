# SPDX-License-Identifier: GPL-3.0-or-later
"""Public-data tests of the release boundary: validation, extraction, no clobbers."""
import io
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch as mock_patch
from PIL import Image
import patch_game
import release_cart
from build_pack import DataError

ROOT=Path(__file__).resolve().parents[1]


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        scratch=ROOT/'private/test-temp';scratch.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=scratch);self.root=Path(self.temp.name).resolve()
        self.root.relative_to(scratch.resolve())
    def tearDown(self):self.temp.cleanup()
    def cartridge(self):
        firmware=self.root/'input.bin';firmware.write_bytes(bytes(range(256))*8)
        image=io.BytesIO();Image.new('RGB',(128,128),(0,0,0)).save(image,format='PNG')
        output=self.root/'personal'
        release_cart.package(ROOT/'demo/sd/OTHERWRL.PAK',firmware,image.getvalue(),output)
        return output,firmware
    def test_roundtrip_and_extract_preserves_firmware_and_pack(self):
        output,firmware=self.cartridge();extracted=self.root/'unpacked'
        patch_game.extract_cartridge(output/'Otherrealm.chgame',extracted)
        self.assertEqual((extracted/'Otherrealm.bin').read_bytes(),firmware.read_bytes())
        self.assertEqual((extracted/'sdcard/OTHERWRL.PAK').read_bytes(),(ROOT/'demo/sd/OTHERWRL.PAK').read_bytes())
        self.assertEqual((extracted/'sdcard/GAMES/OTHERREA.CHG').read_bytes(),(output/'sdcard/GAMES/OTHERREA.CHG').read_bytes())
    def test_existing_output_is_preserved(self):
        output=self.root/'existing';output.mkdir();sentinel=output/'keep.txt';sentinel.write_text('keep')
        with self.assertRaisesRegex(DataError,'not empty'):patch_game.convert(self.root,output)
        self.assertEqual(sentinel.read_text(),'keep')
    def test_no_input_writes_no_output(self):
        with self.assertRaisesRegex(DataError,'Choose the folder'):patch_game.convert(None,self.root/'out')
        self.assertFalse((self.root/'out').exists())
    def test_unsupported_steam_folder_writes_no_output(self):
        source=self.root/'game';(source/'DAT').mkdir(parents=True)
        with self.assertRaisesRegex(DataError,'Steam'):patch_game.convert(source,self.root/'out')
        self.assertFalse((self.root/'out').exists())
    def test_failed_conversion_cleans_staging_and_keeps_input(self):
        image=self.root/'DiskA.adf';image.write_bytes(b'bad disk')
        firmware=self.root/'firmware.bin';firmware.write_bytes(b'1234')
        with mock_patch.object(patch_game,'helper_command',return_value=['unused']):
            with self.assertRaises((DataError,ValueError,OSError)):
                patch_game.convert(self.root,self.root/'out',binary=firmware)
        self.assertEqual(image.read_bytes(),b'bad disk')
        self.assertFalse((self.root/'out').exists())
        self.assertFalse(list(self.root.glob('otherrealm-*')))
    def test_extract_refuses_existing_directory(self):
        output,_=self.cartridge();destination=self.root/'saved';destination.mkdir()
        (destination/'keep').write_text('keep')
        with self.assertRaisesRegex(DataError,'empty'):patch_game.extract_cartridge(output/'Otherrealm.chgame',destination)
        self.assertEqual(list(p.name for p in destination.iterdir()),['keep'])
    def test_corrupted_cart_rejected_before_extraction(self):
        output,_=self.cartridge();source=output/'Otherrealm.chgame'
        # Delete the manifest-referenced firmware while retaining a valid ZIP.
        with zipfile.ZipFile(source) as original:
            items=[(n,original.read(n)) for n in original.namelist() if not n.endswith('rev0.bin')]
        damaged=self.root/'damaged.chgame'
        with zipfile.ZipFile(damaged,'w') as z:
            for name,data in items:z.writestr(name,data)
        with self.assertRaises((DataError,ValueError)):patch_game.extract_cartridge(damaged,self.root/'unpacked')
        self.assertFalse((self.root/'unpacked').exists())


if __name__=='__main__':unittest.main()
