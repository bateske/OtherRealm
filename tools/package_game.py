#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Package the release firmware and original First Contact setup guide.

This does not connect to hardware. It rebuilds the original demo from its JSON,
never reads private/ or ADF files, and writes only to a project-local output folder.
Requires the requested CHGame checkout at CHGame/ and Python 3.10+ with Pillow.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CHGAME = ROOT / "CHGame"
sys.path.insert(0, str(CHGAME / "tools"))

try:
    from chcart import background, backup, fixtures, model, picture, runtime, zipio
    import chgpack
except ImportError as error:
    raise SystemExit("Clone https://github.com/bateske/CHGame into CHGame/ before packaging.") from error

from build_demo import build as build_demo
from build_pack import verify_pack


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def make_cover() -> bytes:
    """An original workstation/card cover, readable on the handheld launcher."""
    from PIL import Image, ImageDraw
    design = json.loads((ROOT / "demo/first-contact.json").read_text(encoding="utf8"))
    colors = ["#" + value for value in design["palette"]]
    image = Image.new("RGB", (128, 128), colors[0])
    draw = ImageDraw.Draw(image)
    draw.polygon([(0, 77), (22, 59), (53, 83), (79, 53), (127, 83), (127, 100), (0, 100)], fill=colors[1])
    draw.rectangle((12, 42, 78, 84), fill="#d7b36a")
    draw.rectangle((16, 46, 74, 79), fill="#101f31")
    draw.line((24, 56, 31, 61, 24, 66), fill="#83d1bd", width=2)
    draw.line((37, 68, 54, 68), fill="#83d1bd", width=2)
    draw.rectangle((41, 85, 49, 90), fill="#d7b36a")
    draw.rectangle((30, 91, 60, 93), fill="#d7b36a")
    draw.line((80, 67, 91, 67), fill="#f5e8c8", width=2)
    draw.polygon([(88, 63), (94, 67), (88, 71)], fill="#f5e8c8")
    draw.polygon([(100, 50), (112, 50), (120, 58), (120, 88), (100, 88)], fill="#83d1bd")
    for x in (103, 108, 113):
        draw.rectangle((x, 55, x + 2, 62), fill="#101f31")
    draw.rectangle((103, 69, 116, 80), fill="#101f31")
    draw.rectangle((0, 100, 127, 127), fill="#d7b36a")
    ink = (255, 244, 214)
    put = lambda x, y, color: draw.point((x, y), fill=color)
    background.text(put, 31, 10, "OTHER REALM", ink)
    background.text(put, 25, 23, "FIRST CONTACT", ink)
    background.text(put, 34, 110, "SETUP GUIDE", (16, 31, 49))
    out = io.BytesIO()
    image.save(out, "PNG", optimize=True)
    converted, _ = picture.convert(out.getvalue())
    if not picture.ready(converted):
        raise ValueError("generated cover violates CHGame picture rules")
    return converted


def package(binary: Path, output: Path, version: str, date: str, check_fixtures: bool) -> dict:
    output = output.resolve()
    # A package build must never target a mounted SD drive or the source root.
    if output == ROOT or ROOT not in output.parents:
        raise ValueError("output must be a dedicated directory inside this project")
    output.mkdir(parents=True, exist_ok=True)
    if check_fixtures:
        problems = fixtures.check(log=lambda value: None)
        if problems:
            raise ValueError(f"official CHGame conformance fixtures failed: {problems}")
    firmwares = {rev: (ROOT / f'build/device/{rev}/release/Otherrealm.ino.bin').read_bytes() for rev in ('rev0','rev1')}
    if binary is not None: firmwares['rev0'] = binary.read_bytes()
    firmware = firmwares['rev0']
    generated_pack = output / "generated" / "OTHERWRL.PAK"
    build_demo(ROOT / "demo/first-contact.json", generated_pack)
    pack_info = verify_pack(generated_pack)
    demo = generated_pack.read_bytes()
    cover = make_cover()
    (output / "cover.png").write_bytes(cover)
    licenses = {
        "LICENSE": (ROOT / "LICENSE").read_bytes(),
        "CHGAME-LICENSE": (CHGAME / "tools/LICENSE").read_bytes(),
        "CHGAME-NOTICE": (CHGAME / "tools/NOTICE").read_bytes(),
        "FLASH-LICENSE": (ROOT / "firmware/Otherrealm/THIRD_PARTY_FLASH_LICENSE.txt").read_bytes(),
        "DISPLAY-LICENSE": (ROOT / "firmware/Otherrealm/THIRD_PARTY_CHGFX_LICENSE.txt").read_bytes(),
        "DEVICE-GPL3": (ROOT / "firmware/Otherrealm/LICENSE_GPL3.txt").read_bytes(),
        "AUDIO-LICENSE": (ROOT / "firmware/Otherrealm/THIRD_PARTY_AUDIO_LICENSE.txt").read_bytes(),
        "AUDIO-NOTICE": (ROOT / "firmware/Otherrealm/THIRD_PARTY_AUDIO_NOTICE.txt").read_bytes(),
    }
    if (ROOT / "THIRD_PARTY.md").is_file():
        licenses["THIRD_PARTY.md"] = (ROOT / "THIRD_PARTY.md").read_bytes()
    licenses["ASSETS.txt"] = (
        "First Contact's authored JSON, prose, procedural art, generated guide assets, and\n"
        "the original cover geometry are dedicated to the public domain under CC0-1.0.\n"
        "The engine and authoring tools are GPL-2.0-or-later; see LICENSE.\n"
        "The combined device firmware uses GPL-3.0-or-later (DEVICE-GPL3),\n"
        "including CHGame's Apache-2.0 piezo sequencer (AUDIO-LICENSE/NOTICE).\n"
        "CHGame tooling and default menu assets come from the requested CHGame\n"
        "checkout; see CHGAME-LICENSE and CHGAME-NOTICE.\n"
        "No Another World disk images, extracted resources, or private packs are included.\n"
    ).encode("utf8")
    game = model.Game(
        id="otherrealm", title="OTHER REALM", version=version, author="bateske",
        description="First Contact: a playable setup guide for bringing your Another World copy to CHGame.",
        genre="Adventure", license="GPL-3.0-or-later", binaries=firmwares,
        sourceUrl="https://github.com/bateske/Otherrealm",url="https://github.com/bateske/Otherrealm",
        license_files=licenses, sd={"OTHERWRL.PAK": demo}, cart_image=cover,
        buttons=[("D-pad", "Move; Up jumps"), ("A", "Read station / next page"),
                 ("B", "Jump / menu back"), ("SELECT", "Menu"), ("START", "Pause / system menu")],
    )
    cart = model.Cart(title="Other Realm", games=[game], version=version,
                      author="Otherrealm", date=date, license="GPL-3.0-or-later", cover=cover,
                      description="Five-room setup adventure. Buy, find the Amiga disks, build, and copy to SD. Another World not included.")
    issues = model.validate(cart)
    if issues:
        raise ValueError("\n".join(map(str, issues)))
    cart_path = output / "Otherrealm.chgame"
    archive = zipio.write(cart, cart_path)
    roundtrip_issues = []
    loaded = zipio.load(archive, roundtrip_issues)
    if roundtrip_issues or zipio.to_bytes(loaded) != archive:
        raise ValueError("official .chgame round-trip verification failed")
    for revision, image in firmwares.items():
        prepared = runtime.prepare(loaded, revision)
        chg = next(value for name,value in prepared.items() if name.endswith('.CHG'))
        parsed = chgpack.parse(chg, target=chgpack.target_of(revision))
        if chg[512:512+parsed['payload_bytes']] != chgpack.pad_image(image):
            raise ValueError(f'{revision} firmware mismatch')
        restored, issues = backup.backup(prepared, title=cart.title, device=revision)
        if issues or runtime.prepare(restored, revision) != prepared:
            raise ValueError(f'{revision} backup mismatch')
        (output / f'Otherrealm-{revision}.bin').write_bytes(image)
        (output / f'Otherrealm-SD-{revision}.zip').write_bytes(backup.card_zip(prepared))
    card = runtime.prepare(loaded, "rev0")
    chg_paths = [name for name in card if name.endswith(".CHG")]
    if len(chg_paths) != 1 or card.get("OTHERWRL.PAK") != demo:
        raise ValueError("prepared card has unexpected firmware or data files")
    chg = card[chg_paths[0]]
    chg_info = chgpack.parse(chg, target=chgpack.target_of("rev0"))
    if chg[512:512 + chg_info["payload_bytes"]] != chgpack.pad_image(firmware):
        raise ValueError("CHG payload does not match release firmware")
    restored, backup_issues = backup.backup(card, title=cart.title, device="rev0")
    if backup_issues or runtime.prepare(restored, "rev0") != card:
        raise ValueError("official prepared-card backup round-trip verification failed")
    # The official helper produced each byte; write only its listed files and
    # preserve unrelated existing files instead of its recursive-delete wrapper.
    for name, content in card.items():
        path = output / "sdcard" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    (output / "Otherrealm-SD.zip").write_bytes(backup.card_zip(card))
    files, manifest = zipio.read(archive)
    report = {
        "format": "CHGame schemaVersion 1 / prepared card v2", "devices": list(firmwares),
        "cart": {"file": cart_path.name, "bytes": len(archive), "sha256": sha256(archive)},
        "firmware": {"file": "Otherrealm-rev0.bin",
                     "bytes": len(firmware), "sha256": sha256(firmware)},
        "binaries": {rev: {"bytes": len(data), "sha256": sha256(data)} for rev,data in firmwares.items()},
        "demo": pack_info,
        "checks": {"official_fixtures": "passed" if check_fixtures else "not requested",
                   "cart_roundtrip": "byte-identical", "card_backup_roundtrip": "byte-identical",
                   "bootloader_chg_parser": "passed", "warnings": []},
        "archive_entries": sorted(files), "manifest": manifest,
        "card_files": [{"path": name, "bytes": len(content), "sha256": sha256(content)}
                       for name, content in sorted(card.items())],
    }
    (output / "package-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
    (output / "README.txt").write_text(
        "OTHER REALM / FIRST CONTACT SETUP GUIDE\n\n"
        "Otherrealm.chgame is a CHGame Rev0 + Rev1 cartridge: firmware,\n"
        "original guide, metadata, cover, and licenses. Import it with CHGame tools.\n"
        "Otherrealm-SD-rev0.zip / Otherrealm-SD-rev1.zip match each board.\n"
        "Otherrealm-SD.zip and sdcard/ are the Rev0 prepared single-game\n"
        "card layout. Copy their contents to a FAT16/FAT32 card's root, then select\n"
        "OTHER REALM in its bootloader menu. Import the cart into an existing\n"
        "multi-game collection to preserve that collection's menu entries.\n\n"
        "Left/Right: move; Up/B: jump; A: read / next page; START/SELECT: menu.\n"
        "Menu: Up/Down select, A confirm, B back. Progress autosaves at milestones.\n"
        "Read each station to open the path: arrival, terminal, archive, workshop,\n"
        "card dock. Walk back to reread. Playing is only a rehearsal: it cannot\n"
        "purchase, download, convert, or install files. Complete those steps on a PC.\n\n"
        "Buy/install Another World - 20th Anniversary Edition on Steam (app 233550).\n"
        "Manage > Browse local files opens its installation. Bonus/rom Amiga holds\n"
        "the two supported ADF disks. Open Otherrealm-Patcher.exe, choose the Steam\n"
        "game folder and a new empty output folder, then Build my cartridge.\n"
        "After all five steps pass, choose sdcard-rev0 or sdcard-rev1 for your board.\n"
        "Copy its CONTENTS to your\n"
        "card root: OTHERWRL.PAK and GAMES/OTHERREA.CHG. Safely eject, insert into\n"
        "CHGame, and choose OTHER REALM. Import the generated Otherrealm.chgame with\n"
        "CHGame tools when adding it to an existing multi-game collection.\n\n"
        "This public bundle contains only the original First Contact guide.\n"
        "Another World data is supplied by you and stays in your personal output.\n"
        "package-report.json records firmware/data hashes and official validation.\n",
        encoding="utf8",
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=ROOT / "build/dist")
    parser.add_argument("--version", default="0.5.1")
    parser.add_argument("--date", default="2026-11-24")
    parser.add_argument("--check-fixtures", action="store_true", help="also check all official CHGame conformance fixtures")
    args = parser.parse_args()
    try:
        result = package(args.binary.resolve() if args.binary else None, args.output, args.version, args.date, args.check_fixtures)
    except (OSError, ValueError, model.CartError, chgpack.ChgError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"Built {args.output / result['cart']['file']}: {result['cart']['bytes']:,} bytes")
    print(f"Firmware: {result['firmware']['bytes']:,} bytes; original demo: {result['demo']['bytes']:,} bytes")
    print("Official cart/card round trips and bootloader parser: PASS; warnings: none")
    print(f"SHA256 {result['cart']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
