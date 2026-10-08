#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build Signal Grove from original JSON/vector/script assets; no game disks.

The tiny Assembler and Shapes classes are reusable authoring primitives for
new SD games. All game rules below compile into ordinary cooperative bytecode.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]


class Assembler:
    """Big-endian bytecode with labels, variable operands and checked fixups."""
    def __init__(self):
        self.data = bytearray()
        self.labels: dict[str, int] = {}
        self.fixups: list[tuple[int, str]] = []

    def emit(self, *values: int):
        self.data.extend(values)

    def word(self, value: int):
        self.data.extend(struct.pack(">H", value & 0xffff))

    def label(self, name: str):
        if name in self.labels:
            raise ValueError(f"duplicate label: {name}")
        self.labels[name] = len(self.data)

    def target(self, label: str):
        self.fixups.append((len(self.data), label))
        self.word(0)

    def mov(self, var: int, value: int):
        self.emit(0, var); self.word(value)

    def copy(self, destination: int, source: int):
        self.emit(1, destination, source)

    def add(self, var: int, value: int):
        self.emit(3, var); self.word(value)

    def add_var(self, destination: int, source: int):
        self.emit(2, destination, source)

    def jump(self, label: str):
        self.emit(7); self.target(label)

    def compare(self, var: int, relation: str, value: int, label: str):
        condition = {"==": 0, "!=": 1, ">": 2, ">=": 3, "<": 4, "<=": 5}[relation]
        self.emit(10, 0x40 | condition, var); self.word(value); self.target(label)

    def fill(self, page: int, color: int):
        self.emit(14, page, color)

    def select(self, page: int):
        self.emit(13, page)

    def draw(self, offset: int, x: int, y: int):
        self.emit(0x40); self.word(offset // 2); self.word(x); self.word(y)

    def draw_variables(self, offset: int, x_var: int, y_var: int):
        self.emit(0x54); self.word(offset // 2); self.emit(x_var, y_var)

    def transition(self, scene: int):
        self.emit(25); self.word(scene); self.emit(6)
        # The host switches scenes after this tick. Ending this task also makes
        # a forgotten scene switch detectable without an infinite jump loop.
        self.emit(17)

    def finish(self) -> bytes:
        for offset, label in self.fixups:
            if label not in self.labels:
                raise ValueError(f"undefined label: {label}")
            struct.pack_into(">H", self.data, offset, self.labels[label])
        if len(self.data) >= 65534:
            raise ValueError("split scripts larger than 64 KiB into scenes")
        return bytes(self.data)


# Original, deliberately small 3x5 geometric lettering. Every lit run becomes
# a filled vector rectangle; no game fonts, bitmaps, or proprietary text data.
FONT = {
    "A": [2,5,7,5,5], "B": [6,5,6,5,6], "C": [3,4,4,4,3],
    "D": [6,5,5,5,6], "E": [7,4,6,4,7], "F": [7,4,6,4,4],
    "G": [3,4,5,5,3], "H": [5,5,7,5,5], "I": [7,2,2,2,7],
    "J": [1,1,1,5,2], "K": [5,5,6,5,5], "L": [4,4,4,4,7],
    "M": [5,7,7,5,5], "N": [5,7,7,7,5], "O": [2,5,5,5,2],
    "P": [6,5,6,4,4], "Q": [2,5,5,3,1], "R": [6,5,6,5,5],
    "S": [3,4,2,1,6], "T": [7,2,2,2,2], "U": [5,5,5,5,7],
    "V": [5,5,5,5,2], "W": [5,5,7,7,5], "X": [5,5,2,5,5],
    "Y": [5,5,2,2,2], "Z": [7,1,2,4,7], "!": [2,2,2,0,2],
    " ": [0,0,0,0,0], "-": [0,0,7,0,0], ":": [0,2,0,2,0],
}


class Shapes:
    """Compact colored monotonic polygons and hierarchical vector groups."""
    def __init__(self):
        self.data = bytearray()
        self.cache: dict[bytes, int] = {}

    def intern(self, data: bytes) -> int:
        if data in self.cache:
            return self.cache[data]
        if len(self.data) & 1:
            self.data.append(0)
        offset = len(self.data)
        if offset + len(data) > 65536:
            raise ValueError("split vector resources larger than 64 KiB into scenes")
        self.data.extend(data)
        self.cache[data] = offset
        return offset

    def strip(self, color: int, rows: list[list[int]]) -> tuple[int, int, int]:
        if not 2 <= len(rows) <= 32 or any(rows[i][0] > rows[i+1][0] for i in range(len(rows)-1)):
            raise ValueError("polygons need 2..32 ascending [y,left,right] rows")
        left = min(row[1] for row in rows); right = max(row[2] for row in rows)
        top, bottom = rows[0][0], rows[-1][0]
        width, height = right-left, bottom-top
        if not (0 <= color <= 15 and 0 <= width <= 255 and 0 <= height <= 255):
            raise ValueError("polygon exceeds encoded bounds")
        vertices = [(r[2]-left,r[0]-top) for r in rows] + [(r[1]-left,r[0]-top) for r in reversed(rows)]
        data = bytes([0xc0 | color, width, height, len(vertices)]) + bytes(v for xy in vertices for v in xy)
        return self.intern(data), left+width//2, top+height//2

    def rectangle(self, color: int, x: int, y: int, w: int, h: int) -> tuple[int, int, int]:
        return self.strip(color, [[y,x,x+w],[y+h,x,x+w]])

    def group(self, children: list[tuple[int, int, int]], origin=(0,0)) -> int:
        if not 1 <= len(children) <= 256:
            raise ValueError("groups need 1..256 children")
        data = bytearray([2, origin[0], origin[1], len(children)-1])
        for offset,x,y in children:
            if not (0 <= x <= 255 and 0 <= y <= 255):
                raise ValueError(f"child position out of range: {x},{y}")
            data.extend(struct.pack(">HBB", offset//2,x,y))
        return self.intern(bytes(data))

    def scene_group(self, children: list[tuple[int, int, int]]) -> int:
        # Position children wider than the format's 8-bit coordinates by adding
        # one nested group. Runtime needs neither a bitmap nor transformed copy.
        left = [(o,x,y) for o,x,y in children if x <= 255]
        right = [(o,x-160,y) for o,x,y in children if x > 255]
        if right:
            left.append((self.group(right),160,0))
        return self.group(left, (160,100))

    def text(self, text: str, color: int = 11, cell: int = 3) -> int:
        letters = []
        for index,ch in enumerate(text.upper()):
            if ch not in FONT:
                raise ValueError(f"unsupported lettering character {ch!r}")
            runs = []
            for y,bits in enumerate(FONT[ch]):
                x = 0
                while x < 3:
                    if not (bits & (4>>x)):
                        x += 1; continue
                    end = x+1
                    while end < 3 and bits & (4>>end): end += 1
                    # Polygon scanline endpoints are inclusive. Stop at the
                    # final lit column, so neighboring letters retain a gap
                    # even on the 104-pixel-wide embedded framebuffer.
                    runs.append(self.rectangle(color,x*cell,y*cell,(end-x-1)*cell,cell))
                    x = end
            if runs:
                letters.append((self.group(runs),index*4*cell,0))
        return self.group(letters)

    def tree(self, x: int, y: int, scale=1) -> list[tuple[int,int,int]]:
        return [self.rectangle(8,x-4*scale,y-52*scale,8*scale,52*scale),
                self.strip(5,[[y-82*scale,x,x],[y-37*scale,x-23*scale,x+23*scale]]),
                self.strip(6,[[y-64*scale,x,x],[y-24*scale,x-29*scale,x+29*scale]])]

    def character(self, pose: int) -> int:
        # Two walking poses, authored as grouped quadrilateral limbs. The
        # group's anchor is the character's feet, with a 32x42 local canvas.
        parts = [self.rectangle(11,10,1,12,10), self.rectangle(14,8,12,16,17),
                 self.rectangle(3,7,26,18,6), self.rectangle(11,3,14,5,14),
                 self.rectangle(11,24,14,5,14)]
        if pose:
            parts += [self.strip(10,[[31,9,15],[42,3,10]]), self.strip(10,[[31,18,24],[42,23,30]])]
        else:
            parts += [self.rectangle(10,9,31,6,11), self.rectangle(10,19,31,6,11)]
        return self.group(parts, (16,42))


def create_shapes(scene: dict, title: str) -> tuple[Shapes, dict[str,int]]:
    shapes = Shapes(); children = []
    for item in scene["objects"]:
        kind = item["kind"]
        if kind == "rect": children.append(shapes.rectangle(item["color"], *item["box"]))
        elif kind == "strip": children.append(shapes.strip(item["color"],item["rows"]))
        elif kind == "tree": children.extend(shapes.tree(*item["position"],item.get("size",1)))
        elif kind == "text": children.append((shapes.text(item["text"],item["color"]),*item["position"]))
        else: raise ValueError(f"unknown object kind {kind!r}")
    children += [(shapes.text(title,11),70,9), (shapes.text(scene["name"],3), (320-len(scene["name"])*12)//2,31)]
    ids = {"background": shapes.scene_group(children), "idle": shapes.character(0), "walk": shapes.character(1)}
    ids["crystal"] = shapes.strip(14,[[0,7,7],[7,0,14],[18,7,7]])[0]
    ids["crystal_glow"] = shapes.strip(11,[[0,9,9],[9,0,18],[22,9,9]])[0]
    ids["gate"] = shapes.rectangle(15,0,0,48,90)[0]
    ids["gate_bar"] = shapes.rectangle(14,0,0,4,86)[0]
    ids["open_gate"] = shapes.group([shapes.rectangle(14,0,0,5,91),shapes.rectangle(14,50,0,5,91)],(27,45))
    ids["need_key"] = shapes.text("FIND THE CRYSTAL",12)
    ids["have_key"] = shapes.text("SIGNAL FOUND",14)
    ids["victory_panel"] = shapes.rectangle(0,0,0,248,70)[0]
    ids["win"] = shapes.text("THE GROVE AWAKENS",7)
    ids["restart"] = shapes.text("ACTION TO RESTART",11)
    return shapes,ids


def create_script(index: int, scenes: list[dict], shapes: dict[str,int]) -> tuple[bytes,dict]:
    # v0=x, v1=walking timer, v2=room, v3=crystal, v4=gate, v5=last action,
    # v6=victory, v7=animation temp, v8=y, v9=jump velocity, v15=initialized.
    a = Assembler(); room = scenes[index]
    a.compare(15,"!=",0,"enter")
    for var,value in [(0,42),(1,0),(3,0),(4,0),(5,0),(6,0),(8,155),(9,0),(10,0),(15,1),(0xe0,1)]: a.mov(var,value)
    a.label("enter"); a.mov(2,index); a.mov(0xff,2); a.emit(11,0,0)
    a.select(0); a.fill(0,room["sky"]); a.draw(shapes["background"],160,100)
    a.label("frame")
    a.add(10,1)
    a.compare(6,"==",1,"victory")
    a.compare(0xfc,"==",0,"vertical")
    a.compare(0xfc,"==",1,"move_right")
    a.add(0,-4); a.jump("moved")
    a.label("move_right"); a.add(0,4)
    a.label("moved"); a.add(1,1)
    a.label("vertical")
    a.compare(8,"<",155,"gravity")
    a.compare(0xfb,"!=",-1,"gravity")
    a.mov(9,-8)
    a.label("gravity"); a.add_var(8,9); a.add(9,1)
    a.compare(8,"<",155,"bounds")
    a.mov(8,155); a.mov(9,0)
    a.label("bounds")
    a.compare(0,"<",10,"exit_left")
    a.compare(0,">",310,"exit_right")
    if index == 2:
        a.compare(4,"==",1,"interact")
        a.compare(0,"<=",230,"interact")
        a.mov(0,230)
    a.label("interact")
    a.compare(0xfa,"==",0,"after_action")
    a.compare(5,"!=",0,"after_action")
    if index == 1:
        a.compare(0,"<",124,"after_action"); a.compare(0,">",196,"after_action")
        a.compare(3,"==",1,"after_action");a.mov(3,1);a.add(0xe0,1)
    elif index == 2:
        a.compare(0,"<",182,"after_action"); a.compare(3,"==",0,"after_action")
        a.compare(4,"==",1,"after_action");a.mov(4,1);a.add(0xe0,1)
    a.label("after_action"); a.copy(5,0xfa)
    a.emit(15,0,1); a.select(1)
    if index == 1:
        a.compare(3,"==",1,"crystal_done")
        a.copy(7,10); a.emit(0x14,7,0,8)
        a.compare(7,"==",0,"small_crystal")
        a.draw(shapes["crystal_glow"],160,110); a.jump("crystal_done")
        a.label("small_crystal"); a.draw(shapes["crystal"],160,110)
        a.label("crystal_done")
    elif index == 2:
        a.compare(4,"==",1,"gate_open")
        a.draw(shapes["gate"],263,109)
        for x in [248,263,278]: a.draw(shapes["gate_bar"],x,109)
        a.compare(3,"!=",0,"gate_ready")
        a.draw(shapes["need_key"],66,69); a.jump("gate_done")
        a.label("gate_ready"); a.draw(shapes["have_key"],88,69); a.jump("gate_done")
        a.label("gate_open"); a.draw(shapes["open_gate"],263,109)
        a.label("gate_done")
    a.compare(3,"==",0,"no_key_hud"); a.draw(shapes["crystal"],304,24)
    a.label("no_key_hud")
    a.copy(7,1); a.emit(0x14,7,0,4)
    a.compare(0xfc,"==",0,"idle"); a.compare(7,"==",0,"idle")
    a.draw_variables(shapes["walk"],0,8); a.jump("display")
    a.label("idle"); a.draw_variables(shapes["idle"],0,8)
    a.label("display"); a.emit(16,1,6); a.jump("frame")
    a.label("exit_left")
    if index:
        a.mov(0,300);a.add(0xe0,1);a.transition(scenes[index-1]["id"])
    else:
        a.mov(0,10); a.jump("interact")
    a.label("exit_right")
    if index < len(scenes)-1:
        a.mov(0,20);a.add(0xe0,1);a.transition(scenes[index+1]["id"])
    else:
        a.mov(6,1);a.add(0xe0,1);a.emit(0x1c,3,0,0,0,0);a.jump("victory")
    a.label("victory")
    a.emit(15,0,1); a.select(1)
    a.draw(shapes["open_gate"],263,109)
    a.draw(shapes["victory_panel"],160,101)
    a.draw(shapes["win"],58,80); a.draw(shapes["restart"],58,108)
    a.emit(16,1)
    a.compare(0xfa,"==",0,"win_wait")
    a.compare(5,"!=",0,"win_wait")
    a.mov(15,0); a.transition(scenes[0]["id"])
    a.label("win_wait"); a.copy(5,0xfa); a.emit(6); a.jump("victory")
    return a.finish(),a.labels


def build(source: Path, output: Path) -> dict:
    design = json.loads(source.read_text(encoding="utf8"))
    scenes = design["scenes"]
    if len(scenes) != 3:
        raise ValueError("Signal Grove rules use exactly three rooms; edit create_script for a new game")
    if len(design["palette"]) != 16:
        raise ValueError("the 4-bit renderer needs exactly 16 palette colors")
    palette = b"".join(struct.pack(">H", ((int(c,16)>>20)&15)<<8 | ((int(c,16)>>12)&15)<<4 | ((int(c,16)>>4)&15)) for c in design["palette"])
    resources: dict[int,tuple[int,bytes]] = {}; directory = bytearray(); info = []
    for index,scene in enumerate(scenes):
        vectors,ids = create_shapes(scene,design["title"])
        bytecode,labels = create_script(index,scenes,ids)
        base = 23+index*3
        resources[base] = 3,palette
        resources[base+1] = 4,bytecode
        resources[base+2] = 5,bytes(vectors.data)
        directory.extend(struct.pack("<HHHHH",scene["id"],base,base+1,base+2,0))
        info.append({"id":scene["id"],"name":scene["name"],"code_bytes":len(bytecode),"shape_bytes":len(vectors.data),"labels":labels,"shapes":ids})
    directory_id = 23+len(scenes)*3
    resources[directory_id] = 7,bytes(directory)
    count = directory_id+1
    align = lambda n: (n+511)&~511
    data = bytearray(align(32+count*16)); payload_start = len(data)
    for rid,(kind,content) in sorted(resources.items()):
        offset = len(data)
        struct.pack_into("<IIIHH",data,32+rid*16,offset,len(content),zlib.crc32(content),kind,1)
        data.extend(content); data.extend(bytes(align(len(data))-len(data)))
    # Amiga-compatible vector/palette encoding + optional scene map (bit 2).
    # No bitmap dimensions: vectors are independent of the device raster size.
    struct.pack_into("<4sHHIIIIII",data,0,b"ORW1",1,32,count,32,payload_start,len(data),5,0)
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(data)
    from patch_title import attach
    attach(output, ROOT/"demo/title.bin", output)
    data = output.read_bytes()
    manifest = {"title":design["title"],"source":str(source.name),"license":"CC0-1.0 (original game assets)","scenes":info,"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()}
    output.with_suffix(".json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf8")
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,default=ROOT/"demo/signal-grove.json")
    parser.add_argument("--output",type=Path,default=ROOT/"demo/sd/OTHERWRL.PAK")
    args=parser.parse_args(); result=build(args.source,args.output)
    print(f"Built {result['title']}: {len(result['scenes'])} scenes, {result['bytes']} bytes, SHA256 {result['sha256']}")


if __name__ == "__main__": main()
