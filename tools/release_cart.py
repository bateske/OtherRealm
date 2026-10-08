#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write a personal cartridge using the official CHGame format implementation."""
from pathlib import Path
import hashlib
import json
import sys

ROOT=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(ROOT/'CHGame'/'tools'))
from chcart import model,runtime,zipio
import chgpack
from build_pack import verify_pack

VERSION='0.5.0'


def license_files():
    paths={'LICENSE':'LICENSE','THIRD_PARTY.md':'THIRD_PARTY.md',
           'DEVICE-GPL3':'firmware/Otherrealm/LICENSE_GPL3.txt',
           'AUDIO-LICENSE':'firmware/Otherrealm/THIRD_PARTY_AUDIO_LICENSE.txt',
           'AUDIO-NOTICE':'firmware/Otherrealm/THIRD_PARTY_AUDIO_NOTICE.txt',
           'DISPLAY-LICENSE':'firmware/Otherrealm/THIRD_PARTY_CHGFX_LICENSE.txt',
           'FLASH-LICENSE':'firmware/Otherrealm/THIRD_PARTY_FLASH_LICENSE.txt'}
    result={name:(ROOT/path).read_bytes() for name,path in paths.items()}
    result['GAME-CONTENT.txt']=(
        'This personal cartridge contains Another World data converted from its owner\'s files.\n'
        'Another World / Out of This World is the creation and trademark of Eric Chahi.\n'
        'Original game content retains its original rights. It is not GPL or CC0.\n'
        'Do not redistribute this cartridge or its OTHERWRL.PAK file.\n'
        'The engine, source tools and original Other Realm logo are distributed separately.\n'
    ).encode()
    return result


def package(pack_path,binary,cover,output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    info=verify_pack(pack_path);data=Path(pack_path).read_bytes();firmware=Path(binary).read_bytes()
    game=model.Game(id='otherrealm',title='OTHER REALM',version=VERSION,author='bateske',
        description='Another World for CHGame. Personal conversion; original game required.',
        genre='Adventure',license='GPL-3.0-or-later AND LicenseRef-Original-Game',
        sourceUrl='https://github.com/bateske/Otherrealm',url='https://github.com/bateske/Otherrealm',
        binaries={'rev0':firmware},sd={'OTHERWRL.PAK':data},cart_image=cover,license_files=license_files(),
        buttons=[('D-pad','Move; Up jumps / swims'),('A','Run / shoot / action'),
                 ('B','Jump / menu back'),('START','Pause; hold 3 seconds to exit'),('SELECT','Pause')])
    cart=model.Cart(title='Other Realm',games=[game],version=VERSION,author='bateske',
        description=game.description,license=game.license,cover=cover,sourceUrl=game.sourceUrl,url=game.url)
    issues=model.validate(cart)
    if issues:raise ValueError('\n'.join(map(str,issues)))
    archive=zipio.to_bytes(cart);issues=[];decoded=zipio.load(archive,issues)
    if issues or zipio.to_bytes(decoded)!=archive:raise ValueError('Cartridge round-trip validation failed')
    chg=runtime.chg_file(decoded.games[0]);parsed=chgpack.parse(chg,target=chgpack.target_of('rev0'))
    if chg[512:512+parsed['payload_bytes']]!=chgpack.pad_image(firmware):raise ValueError('Firmware verification failed')
    (output/'Otherrealm.chgame').write_bytes(archive)
    (output/'Otherrealm.bin').write_bytes(firmware)
    (output/'cover.png').write_bytes(cover)
    (output/'sdcard'/'GAMES').mkdir(parents=True,exist_ok=True)
    (output/'sdcard'/'OTHERWRL.PAK').write_bytes(data)
    (output/'sdcard'/'GAMES'/'OTHERREA.CHG').write_bytes(chg)
    report=dict(version=VERSION,pack=info,checks=['official cart round trip','bootloader CHG parser','firmware identity'],
                files={p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted(output.rglob('*')) if p.is_file()})
    (output/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    (output/'START-HERE.txt').write_text(
        'OTHER REALM - PERSONAL GAME CARTRIDGE\n\n'
        'Otherrealm.chgame: import with CHGame cartridge tools.\n'
        'sdcard/: copy its contents to a FAT16/FAT32 card root. It adds just\n'
        'OTHERWRL.PAK and GAMES/OTHERREA.CHG, preserving existing menu files.\n'
        'Eject the card, insert it in CHGame, then choose OTHER REALM.\n'
        'Otherrealm.bin: optional direct USB upload with the board-package uploader.\n\n'
        'Up + A: swim out of the opening pool. D-pad: move. A: run/action/shoot.\n'
        'B: jump/back. Start: pause. Hold Start 3 seconds: system launcher.\n'
        'New Game starts the intro. Start > Skip Intro skips it. Progress autosaves.\n\n'
        'This folder contains your personal game data. Do not redistribute it.\n'
        'Full installation guide: https://github.com/bateske/Otherrealm\n',encoding='utf8')
    return report
