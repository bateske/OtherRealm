"""Title assets preserve save identity and bytecode resource IDs."""
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from patch_title import attach
from patch_text import read_pack
from build_pack import DataError

class TitleTests(unittest.TestCase):
    def test_idempotent_and_game_resources_preserved(self):
        source=ROOT/'demo/sd/OTHERWRL.PAK'
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            out=Path(folder)/'test.pak'
            attach(source,ROOT/'demo/title.bin',out)
            self.assertEqual(source.read_bytes(),out.read_bytes())
            before=out.read_bytes();attach(out,ROOT/'demo/title.bin',out)
            self.assertEqual(before,out.read_bytes())
            _,_,_,resources=read_pack(out)
            self.assertEqual(resources[-1].kind,7)
            self.assertEqual(sum(r.kind==10 for r in resources),1)
            self.assertEqual(sum(r.kind==9 for r in resources),1)
            self.assertTrue(all(resources[i].kind==4 for i in (24,27,30)))

    def test_invalid_size_does_not_overwrite(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            out=Path(folder)/'test.pak';out.write_bytes(b'keep me')
            bad=Path(folder)/'bad.bin';bad.write_bytes(bytes(8192))
            with self.assertRaises(DataError):attach(ROOT/'demo/sd/OTHERWRL.PAK',bad,out)
            self.assertEqual(out.read_bytes(),b'keep me')

    def test_unlock_title_preserves_identity_and_scene_ids(self):
        source=ROOT/'demo/sd/OTHERWRL.PAK'
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            out=Path(folder)/'test.pak';alternate=Path(folder)/'alternate.bin'
            changed=bytearray((ROOT/'demo/title.bin').read_bytes());changed[100]^=1
            alternate.write_bytes(changed)
            _,_,_,before=read_pack(source)
            attach(source,ROOT/'demo/title.bin',out,alternate)
            data=out.read_bytes();attach(out,ROOT/'demo/title.bin',out,alternate)
            self.assertEqual(data,out.read_bytes())
            _,_,_,after=read_pack(out)
            self.assertEqual(after[-1].kind,7)
            self.assertEqual(next(r.data for r in after if r.kind==9),next(r.data for r in before if r.kind==9))
            self.assertEqual(next(r.data for r in after if r.kind==11),bytes(changed))
            self.assertEqual([r.data for r in before if r.kind==4],[r.data for r in after if r.kind==4])
            alternate.write_bytes(b'invalid')
            with self.assertRaises(DataError):attach(out,ROOT/'demo/title.bin',out,alternate)
            self.assertEqual(data,out.read_bytes())

if __name__=='__main__':unittest.main()
