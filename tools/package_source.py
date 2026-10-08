#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Bundle the engine, patcher, tests and original demo using explicit allowlists.

Never traverses private/, references, disk images or generated private packs.
Run after rebuilding the public demo and browser runtime.
"""
from pathlib import Path
import hashlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    files = [ROOT / name for name in ("README.md", "BUILDING.md", "LICENSE", "THIRD_PARTY.md", "CMakeLists.txt", ".gitignore", ".gitattributes")]
    for folder, patterns in {
        "engine": ("*.cpp", "*.h"),
        "firmware/Otherrealm": ("*.cpp", "*.h", "*.ino", "*.txt"),
        "tools": ("*.py", "*.ps1", "*.md", "*.cpp", "requirements-release.txt"),
        "tools/licenses": ("*.txt",),
        "tests": ("*.cpp", "*.mjs", "*.py"),
        "tests/replays": ("*.csv",),
        "docs": ("*.md", "banner.png", "cart.png", "cart-menu-preview.png", "gameplay.gif", "gameplay.json"),
        ".github/workflows": ("*.yml",),
        "demo": ("*.json", "*.md", "*.txt", "title.bin"),
        "assets": ("title.png",),
        "web": ("*.html", "*.cpp", "otherrealm.js", "otherrealm.wasm"),
    }.items():
        for pattern in patterns:
            files.extend((ROOT / folder).glob(pattern))
    # Only the authored CC0 demo is distributed. It is never copied from SD.
    files.append(ROOT / "demo/sd/OTHERWRL.PAK")
    output = ROOT / "build/dist/Otherrealm-source.zip"
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for source in sorted(set(files)):
            if source.is_symlink() or not source.is_file():
                raise ValueError(f"Expected regular source file: {source}")
            archive.write(source, "Otherrealm/" + source.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(output) as archive:
        if archive.testzip():
            raise ValueError("Source ZIP verification failed")
    print(f"Built {output}: {output.stat().st_size:,} bytes, {len(set(files))} files")
    print(f"SHA256 {hashlib.sha256(output.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
