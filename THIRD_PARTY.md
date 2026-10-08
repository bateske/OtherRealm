# Source attribution

- [Another-World-Bytecode-Interpreter](https://github.com/fabiensanglard/Another-World-Bytecode-Interpreter),
  revision `170b7d466a6ea6cc36a9e53506d5b858ed503a84`.
  Copyright (C) 2004 Gregory Montoir, with annotations by Fabien Sanglard.
  GPL-2.0-or-later. Compatibility opcode/page/polygon semantics informed the
  portable engine. The font and English string tables in `engine/aw_text.h`
  are imported from this source by `tools/vendor_text.py`. Root `LICENSE` is
  the upstream GPL version 2 text.
- [RawGL](https://github.com/cyxx/rawgl), revision
  `049e4ade49543a12414f68a7838a94ec0a6c149d`.
  Copyright (C) 2004–2005 Gregory Montoir. Consulted for Amiga-specific startup
  variables, original rendering semantics, resource formats, and silent music
  synchronization. The rewritten engine is distributed under GPL-2.0-or-later.
- [CHGame](https://github.com/bateske/CHGame), revision
  `6f51ea690aff5c3ef893502dbdcbfbd170ab832c`.
  The CHSd library is linked from the checkout. ST7735 setup in
  `firmware/Otherrealm/Display.cpp` is adapted from CHGfx, Copyright (c) 2026
  bateske, MIT; its full notice is preserved at
  `firmware/Otherrealm/THIRD_PARTY_CHGFX_LICENSE.txt`. The CHGame tools used to
  construct cartridge/SD files retain their own licenses and notices.
  The SRAM flash programming sequence in `firmware/Otherrealm/Persistence.cpp`
  is adapted from the CHGame bootloader's MIT-licensed `flash.c`; its notice is
  preserved in `firmware/Otherrealm/THIRD_PARTY_FLASH_LICENSE.txt`.

The [Another World Hebrew patcher](https://github.com/gmegidish/another-world-hebrew)
informed the approach of adapting original script resources instead of replacing
the game executable. No code or assets from that project are included here.

The local `reference/` and `CHGame/` checkouts are development dependencies and
are ignored by version control. They are not required by the portable engine
after the checked-in text tables have been generated.

The user-supplied Amiga ADF files, extracted banks/resources, and resulting
`private/` packs are proprietary game content, not part of the engine's license
or the redistributable package. `demo/signal-grove.json`, its procedural artwork,
and `demo/sd/OTHERWRL.PAK` were created for this project and are dedicated to the
public domain under [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/).

The Emscripten-generated browser runtime uses the installed Emscripten SDK and
its permissively licensed support libraries. `web/browser.cpp` is engine glue;
its C++ engine component retains GPL-2.0-or-later.

The menu uses the public-domain X11 **Misc Fixed 8x13 Bold** and **5x8 Regular**
bitmap fonts from the local PixelLogoLab `fonts/u8g2/8x13B.bdf` and `5x8.bdf`
collection. Both source files state: "Public domain font. Share and enjoy."
`tools/vendor_ui_fonts.py` extracts their unchanged ASCII 32–95 bitmap rows into
`engine/aw_ui_font.h`, which records the original file hashes. The fonts render
at one display pixel per bitmap pixel; neither font is enlarged or resampled.

The native title bitmap uses original angular polygon letterforms and a
black/gold palette, dedicated to CC0 in `tools/title_art.py`.
`demo/title.bin` is an original Signal Grove render, also CC0.

`firmware/Otherrealm/Audio.cpp` and `Audio.h` are copied unchanged from CHGame
`src/chgame/Audio.*` (Copyright 2026 bateske), under **Apache-2.0**. The full
license and notice are in `THIRD_PARTY_AUDIO_LICENSE.txt` and
`THIRD_PARTY_AUDIO_NOTICE.txt` beside those files. The combined device binary
is distributed under **GPL-3.0-or-later** (see `LICENSE_GPL3.txt` there); the
engine source remains available under its existing GPL-2.0-or-later grant.

The standalone release packager is GPL-3.0-or-later and bundles the official
CHGame cartridge tools under their Apache-2.0 license. Its distribution includes
CHGame's license and NOTICE, Python/Tcl/Tk notices, Pillow's license and bundled
dependency notices, the PyInstaller license with bootloader exception, and the
native renderer's LLVM/MinGW runtime notices. `tools/bundle_notices.py` collects
these from the actual dependencies used to build it. The build instructions and
complete engine/patcher sources are distributed with every binary release.

The README's promotional scenes and gameplay GIF depict the original game and
retain its rights. They are not part of the CC0 demo or reusable GPL game assets;
see [media provenance](docs/MEDIA.md).
