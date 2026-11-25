#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""First Contact: a data-only, five-station installation adventure.

Uses the existing native text opcode so instructions remain legible at 104x65.
No game rules or prose are compiled into the firmware. The older Signal Grove
builder remains available as an authoring example and regression fixture.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import struct

from build_demo import Assembler, Shapes, FONT

ROOT = Path(__file__).resolve().parents[1]
GROUND = 171


def objects(shapes, items):
    result = []
    for item in items:
        kind = item['kind']
        if kind == 'rect': result.append(shapes.rectangle(item['color'], *item['box']))
        elif kind == 'strip': result.append(shapes.strip(item['color'], item['rows']))
        elif kind == 'tree': result.extend(shapes.tree(*item['position'], item.get('size', 1)))
        else: raise ValueError(f'Unsupported guide geometry: {kind}')
    return result


def create_shapes(scene):
    s = Shapes()
    ids = {'background': s.scene_group(objects(s, scene['objects']))}
    active = objects(s, scene.get('activated_objects', []))
    if active: ids['active'] = s.scene_group(active)
    # A coat, satchel and long stride read as a courier at handheld scale.
    for pose in range(2):
        parts = [s.rectangle(11, 12, 0, 9, 9), s.rectangle(1, 10, 0, 12, 3),
                 s.strip(10, [[10, 9, 22], [29, 6, 24]]),
                 s.rectangle(9, 7, 26, 18, 5), s.rectangle(11, 24, 15, 4, 13),
                 s.rectangle(8, 5, 19, 8, 12)]
        if pose:
            parts += [s.strip(3, [[30, 9, 15], [43, 1, 8]]),
                      s.strip(3, [[30, 17, 23], [43, 24, 31]])]
        else:
            parts += [s.rectangle(3, 10, 30, 5, 13), s.rectangle(3, 19, 30, 5, 13)]
        ids['walk' if pose else 'idle'] = s.group(parts, (16, 43))
    ids['marker'] = s.strip(10, [[0, 0, 12], [8, 6, 6]])[0]
    ids['crate'] = s.group([s.rectangle(9, 0, 0, 18, 13),
                            s.rectangle(11, 3, 2, 4, 8), s.rectangle(11, 11, 2, 4, 8)], (9, 7))
    ids['pack'] = s.group([s.rectangle(14, 0, 0, 17, 14),
                           s.rectangle(0, 3, 4, 11, 7)], (8, 7))
    ids['closed'] = s.rectangle(9, 0, 0, 6, 45)[0]
    return s, ids


def native(a, rid, x=2, y=2, color=11):
    a.emit(0x1b); a.word(rid); a.emit(x, y, color)


def create_script(index, scenes, ids, text):
    # Saved: 00 x,01 walk,02 room,03 stations completed,04 modal page
    # (0 closed),05 previous action,06 completion,07 modal kind (intro/station/end),
    # 08 y,09 velocity,0A time,0F initialized. E0 is the checkpoint milestone.
    a = Assembler(); scene = scenes[index]; target = scene.get('station_x', 215)
    a.compare(15, '!=', 0, 'enter')
    for var, value in [(0,40),(1,0),(3,0),(4,1),(5,0),(6,0),(7,0),
                       (8,GROUND),(9,0),(10,0),(15,1),(0xe0,1)]: a.mov(var,value)
    a.label('enter'); a.mov(2,index); a.mov(0xff,2); a.emit(11,0,0)
    a.select(0); a.fill(0,scene['sky']); a.draw(ids['background'],160,100)
    a.label('frame'); a.add(10,1)
    a.compare(4,'!=',0,'modal_input')
    a.compare(0xfc,'==',0,'vertical'); a.compare(0xfc,'==',1,'right')
    a.add(0,-4); a.jump('moved')
    a.label('right'); a.add(0,4)
    a.label('moved'); a.add(1,1)
    a.label('vertical'); a.compare(8,'<',GROUND,'gravity')
    a.compare(0xfb,'!=',-1,'gravity'); a.mov(9,-8)
    a.label('gravity'); a.add_var(8,9); a.add(9,1)
    a.compare(8,'<',GROUND,'bounds'); a.mov(8,GROUND); a.mov(9,0)
    a.label('bounds'); a.compare(0,'<',10,'exit_left')
    a.compare(3,'>',index,'open_path'); a.compare(0,'<=',280,'open_path'); a.mov(0,280)
    a.label('open_path'); a.compare(0,'>',310,'exit_right')
    a.compare(0xfa,'==',0,'render'); a.compare(5,'!=',0,'render')
    a.compare(0,'<',target-36,'render'); a.compare(0,'>',target+36,'render')
    a.compare(8,'<',GROUND,'render')
    a.mov(4,1); a.mov(7,1); a.add(0xe0,1); a.jump('render')
    a.label('modal_input')
    a.compare(0xfa,'==',0,'render'); a.compare(5,'!=',0,'render')
    a.add(4,1); a.add(0xe0,1)
    for kind,key in [(0,'intro'),(1,'pages'),(2,'ending')]:
        a.compare(7,'!=',kind,f'not_{key}')
        a.compare(4,'<=',len(text[key]),'render')
        a.mov(4,0)
        if kind==1:
            a.compare(3,'>',index,'render'); a.mov(3,index+1)
            a.emit(0x18,0,1,20,30,0) # Small acknowledgement from existing piezo API.
        if kind==2:
            a.mov(0,270) # Stay at dock, free to backtrack and reread every step.
        a.jump('render'); a.label(f'not_{key}')
    a.label('render'); a.copy(5,0xfa)
    a.emit(15,0,1); a.select(1)
    a.compare(3,'<=',index,'inactive')
    if 'active' in ids: a.draw(ids['active'],160,100)
    a.jump('draw_actor')
    a.label('inactive'); a.draw(ids['closed'],287,148)
    a.copy(11,10); a.emit(0x14,11,0,8)
    a.compare(11,'==',0,'draw_actor'); a.draw(ids['marker'],target,83)
    a.label('draw_actor'); a.copy(11,1); a.emit(0x14,11,0,4)
    a.compare(4,'!=',0,'idle'); a.compare(0xfc,'==',0,'idle'); a.compare(11,'==',0,'idle')
    a.draw_variables(ids['walk'],0,8); a.jump('cargo')
    a.label('idle'); a.draw_variables(ids['idle'],0,8)
    a.label('cargo'); a.compare(3,'<',3,'hud'); a.compare(3,'>=',5,'hud')
    a.copy(11,0); a.add(11,18); a.copy(12,8); a.add(12,-20)
    a.compare(3,'==',4,'carry_pack'); a.draw_variables(ids['crate'],11,12); a.jump('hud')
    a.label('carry_pack'); a.draw_variables(ids['pack'],11,12)
    a.label('hud'); native(a,text['heading'])
    a.compare(4,'!=',0,'modal_draw')
    a.compare(0,'<',target-36,'distant'); a.compare(0,'>',target+36,'distant')
    native(a,text['action'],y=58,color=10); a.jump('display')
    a.label('distant'); a.compare(3,'>',index,'next_hint')
    native(a,text['objective'],y=58,color=10); a.jump('display')
    a.label('next_hint'); native(a,text['done'],y=58,color=14); a.jump('display')
    a.label('modal_draw')
    for kind,key in [(0,'intro'),(1,'pages'),(2,'ending')]:
        a.compare(7,'!=',kind,f'skip_{key}')
        for page,rid in enumerate(text[key],1):
            a.compare(4,'!=',page,f'skip_{key}_{page}')
            native(a,rid,y=16); native(a,text['footers'][key][page-1],y=58,color=10)
            a.jump('display'); a.label(f'skip_{key}_{page}')
        a.label(f'skip_{key}')
    a.label('display'); a.emit(16,1,6); a.jump('frame')
    a.label('exit_left')
    if index:
        a.mov(0,300); a.mov(8,GROUND); a.mov(9,0); a.add(0xe0,1); a.transition(scenes[index-1]['id'])
    else: a.mov(0,10); a.jump('render')
    a.label('exit_right')
    if index<len(scenes)-1:
        a.mov(0,20); a.mov(8,GROUND); a.mov(9,0); a.add(0xe0,1); a.transition(scenes[index+1]['id'])
    else:
        a.mov(0,300); a.mov(6,1); a.mov(7,2); a.mov(4,1); a.add(0xe0,1)
        a.emit(0x1c,3,0,0,0,0); a.jump('render')
    return a.finish(), a.labels


def title_art(palette):
    """Small original pixel/vector-style backdrop; keeps the firmware logo clear."""
    pixels = bytearray(128*128)
    def rect(x,y,w,h,c):
        for py in range(max(0,y),min(128,y+h)):
            for px in range(max(0,x),min(128,x+w)): pixels[py*128+px]=c
    def label(text,y,c):
        start=(128-len(text)*4)//2
        for i,ch in enumerate(text):
            for row,bits in enumerate(FONT[ch]):
                for col in range(3):
                    if bits&(4>>col):rect(start+i*4+col,y+row,1,1,c)
    rect(0,68,128,60,1);rect(0,88,128,3,3)
    for x,y in [(8,54),(21,44),(110,49),(94,58),(44,52)]:rect(x,y,1,1,6)
    rect(78,50,38,38,2);rect(82,54,30,25,14);rect(85,57,24,19,0)
    rect(86,59,19,2,6);rect(86,64,12,2,6);rect(88,79,18,9,3)
    rect(29,62,8,8,11);rect(27,71,12,13,10);rect(28,84,4,8,3);rect(35,84,4,8,3)
    rect(14,96,100,16,0);label('SETUP GUIDE',99,11);label('FIRST CONTACT',106,10)
    colors=[]
    for color in palette:
        r,g,b=(int(color[i:i+2],16)>>4 for i in (0,2,4))
        colors.append((r*31//15)<<11|(g*63//15)<<5|(b*31//15))
    return struct.pack('<16H',*colors)+bytes((pixels[i]<<4)|pixels[i+1] for i in range(0,len(pixels),2))


def build(source: Path, output: Path):
    from patch_text import Resource, encode_pack, branch_targets
    from build_pack import verify_pack
    design=json.loads(source.read_text(encoding='utf8'))
    prose=json.loads((source.parent/'guide-text.json').read_text(encoding='utf8'))
    scenes=design['scenes']
    if len(scenes)!=5 or len(prose['stations'])!=5:raise ValueError('Guide needs five stations')
    if len(design['palette'])!=16:raise ValueError('Guide needs sixteen colors')
    resources=[Resource(b'',0,0) for _ in range(23+len(scenes)*3)]
    text_cache={}
    def text_resource(value,body=False):
        lines=value.split('\n')
        if len(lines)>(6 if body else 1) or any(len(line)>25 for line in lines):
            raise ValueError(f'Guide text exceeds handheld layout: {value!r}')
        payload=b'TX\x01\x01'+bytes([100,38 if body else 7,0,0])+value.encode('ascii')+b'\0'
        if payload not in text_cache:
            text_cache[payload]=len(resources);resources.append(Resource(payload,8,1))
        return text_cache[payload]
    palette=b''.join(struct.pack('>H',((int(c,16)>>20)&15)<<8|((int(c,16)>>12)&15)<<4|((int(c,16)>>4)&15)) for c in design['palette'])
    directory=bytearray();info=[]
    for index,scene in enumerate(scenes):
        station=prose['stations'][index]
        texts={key:text_resource(station[key]) for key in ('heading','objective','action')}
        texts['done']=text_resource('STEP READ - WALK RIGHT >' if index<4 else 'WALK RIGHT TO FINISH >')
        texts['footers']={}
        for key,pages in [('intro',prose['intro']),('pages',station['pages']),('ending',prose['ending'])]:
            texts[key]=[text_resource(page,True) for page in pages]
            texts['footers'][key]=[text_resource(f'A: NEXT  {i+1}/{len(pages)}' if i<len(pages)-1 else 'A: CLOSE / WALK') for i in range(len(pages))]
        vectors,ids=create_shapes(scene)
        bytecode,labels=create_script(index,scenes,ids,texts);branch_targets(bytecode)
        base=23+index*3
        resources[base:base+3]=[Resource(palette,3,1),Resource(bytecode,4,1),Resource(bytes(vectors.data),5,1)]
        directory.extend(struct.pack('<5H',scene['id'],base,base+1,base+2,0))
        info.append(dict(id=scene['id'],name=scene['name'],code_bytes=len(bytecode),shape_bytes=len(vectors.data),labels=labels,shapes=ids,text=texts))
    resources.append(Resource(bytes(directory),7,1))
    # Bind saves to the adventure, while allowing title artwork to be replaced
    # without losing progress (the same ORID contract as commercial packs).
    original=encode_pack(resources,5,0);identity=2166136261
    for i in range(len(resources)):
        for byte in original[32+i*16+8:32+i*16+16]:
            identity=((identity^byte)*16777619)&0xffffffff
    resources[-1:-1]=[Resource(b'ORID'+struct.pack('<I',identity),9,1),
                       Resource(title_art(design['palette']),10,1)]
    data=encode_pack(resources,5,0)
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(data);verify_pack(output)
    manifest=dict(title=design['title'],source=source.name,license='CC0-1.0 (original guide assets)',scenes=info,
                  bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
    return manifest
