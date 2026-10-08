"""Import GPL text/font tables from the cited, locally cloned RAW interpreter."""
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/'reference/fabother/src/staticres.cpp').read_text()
def table(name,decl):
    match=re.search(r'const [^\n]*Video::'+name+r'\[\] = \{(.*?)\n\};',src,re.S)
    if not match: raise RuntimeError('Missing upstream table '+name)
    return decl+' = {'+match[1]+'\n};\n'
header='''// SPDX-License-Identifier: GPL-2.0-or-later
// Tables from RAW, copyright (C) 2004 Gregory Montoir.
// Imported from fabiensanglard/Another-World-Bytecode-Interpreter.
#pragma once
#include <stdint.h>
namespace otherrealm {
struct TextEntry { uint16_t id; const char *text; };
'''
header+=table('_font','static const uint8_t awFont[]')
header+=table('_stringsTableEng','static const TextEntry awStrings[]')
header=header.replace('{ END_OF_STRING_DICTIONARY, "" }','{ 0xFFFF, 0 }')
header+='}\n'
(root/'engine/aw_text.h').write_text(header)
