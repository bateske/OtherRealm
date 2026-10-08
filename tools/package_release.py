#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble audited public release assets; never read private game inputs."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    destination=ROOT/'build/public-release';destination.mkdir(parents=True,exist_ok=True)
    app=ROOT/'build/release/Otherrealm-Patcher'
    for path in (app/'Otherrealm-Patcher.exe',app/'licenses/README.txt',app/'Upload-Cartridge.ps1'):
        if not path.is_file():raise FileNotFoundError(f'Build the standalone patcher first: {path}')
    subprocess.run([sys.executable,str(ROOT/'tools/package_source.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/package_game.py'),'--version','0.5.0',
                    '--output',str(ROOT/'build/public-demo'),'--check-fixtures'],cwd=ROOT,check=True)
    with zipfile.ZipFile(destination/'Otherrealm-Patcher-Windows.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(app.rglob('*')):
            if not path.is_file():continue
            if path.is_symlink():raise ValueError('Symlink in application bundle')
            if path.suffix.lower() in ('.adf','.pak','.chg','.chgame','.orgf'):
                raise ValueError(f'Unexpected game data in patcher: {path}')
            archive.write(path,'Otherrealm-Patcher/'+path.relative_to(app).as_posix())
    shutil.copyfile(ROOT/'build/dist/Otherrealm-source.zip',destination/'Otherrealm-source.zip')
    shutil.copyfile(ROOT/'build/public-demo/Otherrealm.chgame',destination/'Signal-Grove.chgame')
    names=['Otherrealm-Patcher-Windows.zip','Otherrealm-source.zip','Signal-Grove.chgame']
    for name in names:
        with zipfile.ZipFile(destination/name) as archive:
            bad=archive.testzip()
            if bad:raise ValueError(f'{name}: failed CRC for {bad}')
            for member in archive.namelist():
                p=Path(member)
                if p.suffix.lower() in ('.adf','.orgf') or 'private' in p.parts or 'reference' in p.parts:
                    raise ValueError(f'Private input in release: {name}/{member}')
                if p.suffix.lower()=='.pak':
                    # Both public archives may contain only the exact original demo pack.
                    if archive.read(member)!=(ROOT/'demo/sd/OTHERWRL.PAK').read_bytes():
                        raise ValueError(f'Non-demo game pack in public release: {name}')
    hashes={name:hashlib.sha256((destination/name).read_bytes()).hexdigest() for name in names}
    (destination/'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name,digest in hashes.items()),encoding='ascii')
    print(json.dumps({name:dict(bytes=(destination/name).stat().st_size,sha256=digest) for name,digest in hashes.items()},indent=2))


if __name__=='__main__':main()
