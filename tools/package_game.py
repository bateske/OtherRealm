#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Package the release firmware and original Signal Grove demo using CHGame's helpers.

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
    """An original geometric cover using Signal Grove's authored palette."""
    from PIL import Image, ImageDraw
    design = json.loads((ROOT / "demo/signal-grove.json").read_text(encoding="utf8"))
    colors = ["#" + value for value in design["palette"]]
    image = Image.new("RGB", (128, 128), colors[0])
    draw = ImageDraw.Draw(image)
    draw.polygon([(0, 73), (26, 41), (71, 90), (0, 99)], fill=colors[1])
    draw.polygon([(47, 87), (79, 52), (127, 96)], fill=colors[2])
    draw.rectangle((0, 91, 127, 127), fill=colors[4])
    draw.rectangle((0, 91, 127, 94), fill=colors[5])
    for x in (9, 24):
        draw.rectangle((x, 54, x + 3, 91), fill=colors[8])
        draw.polygon([(x + 1, 38), (x - 8, 67), (x + 11, 67)], fill=colors[5])
        draw.polygon([(x + 1, 53), (x - 10, 80), (x + 13, 80)], fill=colors[4])
    draw.rectangle((85, 47, 93, 93), fill=colors[2])
    draw.rectangle((111, 47, 119, 93), fill=colors[2])
    draw.rectangle((86, 43, 118, 50), fill=colors[3])
    draw.line((96, 53, 96, 88), fill=colors[14], width=2)
    draw.line((102, 53, 102, 88), fill=colors[14], width=2)
    draw.line((108, 53, 108, 88), fill=colors[14], width=2)
    draw.polygon([(52, 70), (63, 70), (70, 93), (45, 93)], fill=colors[8])
    draw.rectangle((48, 68, 67, 72), fill=colors[9])
    draw.polygon([(57, 48), (65, 58), (57, 67), (50, 58)], fill=colors[14])
    draw.line((57, 50, 57, 63), fill=colors[10], width=2)
    ink = (255, 244, 214)
    put = lambda x, y, color: draw.point((x, y), fill=color)
    background.text(put, 31, 10, "OTHER REALM", ink)
    background.text(put, 28, 23, "SIGNAL GROVE", ink)
    background.text(put, 16, 102, "VECTOR ADVENTURE", ink)
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
    firmware = binary.read_bytes()
    generated_pack = output / "generated" / "OTHERWRL.PAK"
    build_demo(ROOT / "demo/signal-grove.json", generated_pack)
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
        "Signal Grove's authored JSON, procedural art, generated demo assets, and\n"
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
        description="Signal Grove: a three-room vector adventure streamed from SD.",
        genre="Adventure", license="GPL-3.0-or-later", binaries={"rev0": firmware},
        sourceUrl="https://github.com/bateske/Otherrealm",url="https://github.com/bateske/Otherrealm",
        license_files=licenses, sd={"OTHERWRL.PAK": demo}, cart_image=cover,
        buttons=[("D-pad", "Move; Up jumps"), ("A", "Interact / collect / unlock"),
                 ("B", "Jump / menu back"), ("SELECT", "Menu"), ("START", "Pause / system menu")],
    )
    cart = model.Cart(title="Otherrealm: Signal Grove", games=[game], version=version,
                      author="Otherrealm", date=date, license="GPL-3.0-or-later", cover=cover,
                      description="Otherrealm vector engine with the original Signal Grove demo.")
    issues = model.validate(cart)
    if issues:
        raise ValueError("\n".join(map(str, issues)))
    cart_path = output / "Otherrealm.chgame"
    archive = zipio.write(cart, cart_path)
    roundtrip_issues = []
    loaded = zipio.load(archive, roundtrip_issues)
    if roundtrip_issues or zipio.to_bytes(loaded) != archive:
        raise ValueError("official .chgame round-trip verification failed")
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
        "format": "CHGame schemaVersion 1 / prepared card v2", "device": "rev0",
        "cart": {"file": cart_path.name, "bytes": len(archive), "sha256": sha256(archive)},
        "firmware": {"file": str(binary.relative_to(ROOT) if ROOT in binary.parents else binary),
                     "bytes": len(firmware), "sha256": sha256(firmware)},
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
        "OTHERREALM / SIGNAL GROVE\n\n"
        "Otherrealm.chgame is the standard CHGame Rev0 cart: release firmware,\n"
        "original demo, metadata, cover, and licenses. Import it with CHGame tools.\n"
        "Otherrealm-SD.zip and sdcard/ are the equivalent prepared single-game\n"
        "card layout. Copy their contents to a FAT16/FAT32 card's root, then select\n"
        "OTHERREALM in its bootloader menu. Import the cart into an existing\n"
        "multi-game collection to preserve that collection's menu entries.\n\n"
        "D-pad: move; Up/B: jump; A: interact; START/SELECT: system menu.\n"
        "Menu: Up/Down select, A confirm, B back. Progress autosaves at milestones.\n"
        "Collect the crystal in the shrine, take it right to unlock the gate,\n"
        "then walk through. Press A after victory to play again. Sound toggles in the menu.\n\n"
        "This bundle contains only the original Signal Grove demo. Personal\n"
        "Another World resources remain separate in private/.\n"
        "package-report.json records firmware/data hashes and official validation.\n",
        encoding="utf8",
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/device/release/Otherrealm.ino.bin")
    parser.add_argument("--output", type=Path, default=ROOT / "build/dist")
    parser.add_argument("--version", default="0.5.0")
    parser.add_argument("--date", default="2026-10-08")
    parser.add_argument("--check-fixtures", action="store_true", help="also check all official CHGame conformance fixtures")
    args = parser.parse_args()
    try:
        result = package(args.binary.resolve(), args.output, args.version, args.date, args.check_fixtures)
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
