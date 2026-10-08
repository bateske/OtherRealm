#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Collect actual installed dependency notices beside the standalone patcher."""
import importlib.metadata as metadata
from pathlib import Path
import shutil
import sys

ROOT=Path(__file__).resolve().parents[1]


def bundle(destination):
    target=Path(destination)/'licenses';target.mkdir(parents=True,exist_ok=True)
    def copy(source,name):
        source=Path(source)
        if not source.is_file():raise FileNotFoundError(f'Required notice missing: {source}')
        shutil.copyfile(source,target/name)
    copy(ROOT/'LICENSE','ENGINE-GPL2.txt')
    copy(ROOT/'firmware/Otherrealm/LICENSE_GPL3.txt','DEVICE-AND-PACKAGER-GPL3.txt')
    copy(ROOT/'THIRD_PARTY.md','THIRD_PARTY.md')
    copy(ROOT/'CHGame/tools/LICENSE','CHGAME-TOOLS-LICENSE.txt')
    copy(ROOT/'CHGame/tools/NOTICE','CHGAME-TOOLS-NOTICE.txt')
    copy(ROOT/'CHGame/LICENSE','CHGAME-PLATFORM-LICENSE.txt')
    copy(Path(sys.base_prefix)/'LICENSE.txt','PYTHON-LICENSE.txt')
    for distribution,pattern in [('pillow','licenses/LICENSE'),('pyinstaller','COPYING.txt')]:
        dist=metadata.distribution(distribution)
        matches=[p for p in dist.files if str(p).endswith(pattern)]
        if not matches:raise FileNotFoundError(f'{distribution} license not found')
        for i,p in enumerate(matches):copy(dist.locate_file(p),f'{distribution.upper()}-{i}-LICENSE.txt')
    import ziglang
    zig=Path(ziglang.__file__).parent
    for rel,name in [('LICENSE','ZIG'),('lib/libcxx/LICENSE.TXT','LIBCXX'),
                     ('lib/libcxxabi/LICENSE.TXT','LIBCXXABI'),('lib/libunwind/LICENSE.TXT','LIBUNWIND'),
                     ('lib/libc/mingw/COPYING','MINGW')]:copy(zig/rel,name+'-LICENSE.txt')
    # Tk is collected by PyInstaller; Tcl's license also lives in the Python tree.
    for label,rel in [('TK','_internal/_tk_data/license.terms'),('TCL','_internal/_tcl_data/license.terms')]:
        source=Path(destination)/rel
        if not source.is_file():
            candidates=list((Path(sys.base_prefix)/'tcl').glob(('tcl*' if label=='TCL' else 'tk*')+'/license.terms'))
            if not candidates and label=='TCL':candidates=list((ROOT/'tools/licenses').glob('TCL-8.6.12.txt'))
            if not candidates:raise FileNotFoundError(label+' license missing')
            source=candidates[0]
        copy(source,label+'-LICENSE.txt')
    (target/'README.txt').write_text(
        'Other Realm Windows patcher — dependency notices\n\n'
        'Engine source: https://github.com/bateske/Otherrealm\n'
        'CHGame source revision: 6f51ea690aff5c3ef893502dbdcbfbd170ab832c\n'
        'https://github.com/bateske/CHGame/tree/6f51ea690aff5c3ef893502dbdcbfbd170ab832c\n'
        'Source and build instructions are provided alongside this binary release.\n'
        'PyInstaller includes its bootloader exception; Pillow includes its dependency notices.\n'
        'Zig is used at build time; its C++ and MinGW runtimes are linked into the renderer.\n'
        'No commercial Another World game resources are included in this app.\n',encoding='utf8')
    print(f'Collected {len(list(target.iterdir()))} license files')


if __name__=='__main__':bundle(sys.argv[1])
