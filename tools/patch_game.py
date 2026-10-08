#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a personal, validated Other Realm .chgame from original game files."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parents[1]))
sys.path.insert(0,str(ROOT/'CHGame'/'tools'))
from build_pack import build,DataError,OfsDisk
from patch_text import patch
from patch_title import attach
import release_art
import release_cart
release_art.ROOT=ROOT


def helper_command(explicit=None):
    if explicit:return [str(Path(explicit).resolve())]
    for name in ('capture_title.exe','capture_title'):
        for parent in (ROOT/'helpers',ROOT/'build'):
            if (parent/name).is_file():return [str(parent/name)]
    if (ROOT/'build/capture_title.js').is_file() and shutil.which('node'):
        return [shutil.which('node'),str(ROOT/'build/capture_title.js')]
    raise DataError('Title renderer missing. Use the release patcher, or build capture_title with tools/build-host.ps1.')


def firmware_path(explicit=None):
    paths=[Path(explicit)] if explicit else [ROOT/'firmware/Otherrealm.bin',ROOT/'build/device/release/Otherrealm.ino.bin']
    for p in paths:
        if p.is_file():return p
    raise DataError('Firmware missing. Use the release patcher, or run tools/build-device.ps1.')


def convert(source,output,adfs=None,binary=None,helper=None,log=print):
    output=Path(output).expanduser().resolve()
    if output==Path(output.anchor) or output==ROOT:raise DataError('Choose a dedicated output folder, not a drive or source root.')
    if output.exists() and any(output.iterdir()):raise DataError('Output folder is not empty. Choose a new folder to preserve existing files.')
    images=[Path(p).expanduser().resolve() for p in (adfs or [])]
    if not images:
        if not source:raise DataError('Choose the folder containing your original Amiga ADF pair.')
        folder=Path(source).expanduser().resolve()
        if not folder.is_dir():raise DataError('Original game folder does not exist.')
        images=sorted(p for p in folder.iterdir() if p.is_file() and p.suffix.lower()=='.adf')
        if not images:
            raise DataError('No supported Amiga ADF files found. The Steam 20th Anniversary importer is being validated separately.')
    binary=firmware_path(binary);renderer=helper_command(helper)
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='otherrealm-',dir=output.parent) as temporary:
        stage=Path(temporary);work=stage/'work';work.mkdir();ready=stage/'ready';ready.mkdir()
        log('1/5  Validating and extracting your original game resources...')
        build(images,work/'original.pak',104)
        executable=None
        for p in images:
            executable=OfsDisk(p).files().get('another',executable)
        log('2/5  Adapting text, menu entry points and checkpoint screens...')
        patched=patch(work/'original.pak',work/'adapted.pak',executable,ready/'patch-report.json')
        log('3/5  Rendering both native title scenes from your own polygons...')
        build(images,work/'wide.pak',208)
        for scene in ('shoreline','greeting'):
            result=subprocess.run(renderer+[str(work/'wide.pak'),str(work/(scene+'.bin')),scene],
                capture_output=True,text=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            if result.returncode:raise DataError(result.stderr.strip() or 'Title capture failed')
        attach(work/'adapted.pak',work/'shoreline.bin',work/'final.pak',work/'greeting.bin')
        log('4/5  Composing cover art and packaging the .chgame cartridge...')
        report=release_cart.package(work/'final.pak',binary,release_art.cover(work/'final.pak'),ready)
        report['source_files']=[dict(name=p.name,sha256=release_cart.hashlib.sha256(p.read_bytes()).hexdigest()) for p in images]
        (ready/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
        log('5/5  Cartridge and firmware verified. Saving your personal output...')
        # Commit the complete output only after every operation has passed.
        if output.exists():output.rmdir()  # checked empty above; failure leaves it intact
        ready.rename(output)
    log(f'Ready: {output / "Otherrealm.chgame"}')
    return report


def extract_cartridge(cartridge,output):
    """Validate a personal cartridge before exposing its firmware for the uploader."""
    from chcart import zipio,runtime,model
    output=Path(output).resolve()
    if output.exists() and any(output.iterdir()):raise DataError('Extraction output must be empty.')
    issues=[]
    try:cart=zipio.load(Path(cartridge).read_bytes(),issues)
    except model.CartError as error:raise DataError(str(error)) from error
    if issues or len(cart.games)!=1 or cart.games[0].id!='otherrealm':raise DataError('Expected one valid Otherrealm cartridge.')
    game=cart.games[0]
    if set(game.sd)!={'OTHERWRL.PAK'}:raise DataError('Unexpected SD files in cartridge.')
    output.mkdir(parents=True,exist_ok=True)
    (output/'Otherrealm.bin').write_bytes(runtime.flash_image(game))
    (output/'sdcard'/'GAMES').mkdir(parents=True,exist_ok=True)
    (output/'sdcard'/'OTHERWRL.PAK').write_bytes(game.sd['OTHERWRL.PAK'])
    (output/'sdcard'/'GAMES'/'OTHERREA.CHG').write_bytes(runtime.chg_file(game))


def gui():
    import queue
    import threading
    import tkinter as tk
    from tkinter import filedialog,messagebox,ttk
    root=tk.Tk();root.title('Other Realm - game cartridge builder');root.geometry('740x510');root.minsize(640,470)
    root.configure(bg='#111b2b');style=ttk.Style();style.theme_use('clam')
    style.configure('TFrame',background='#111b2b');style.configure('TLabel',background='#111b2b',foreground='#eee5d1')
    style.configure('TButton',padding=8);style.configure('Title.TLabel',font=('Segoe UI',24,'bold'),foreground='#d5b66b')
    panel=ttk.Frame(root,padding=24);panel.pack(fill='both',expand=True)
    ttk.Label(panel,text='OTHER REALM',style='Title.TLabel').pack(anchor='w')
    ttk.Label(panel,text='Your Amiga ADF pair. One handheld cartridge.',font=('Segoe UI',11)).pack(anchor='w',pady=(0,20))
    source=tk.StringVar();destination=tk.StringVar(value=str(Path.home()/'Desktop'/'Otherrealm-personal'))
    def field(label,variable,choose):
        ttk.Label(panel,text=label).pack(anchor='w');row=ttk.Frame(panel);row.pack(fill='x',pady=(4,12))
        ttk.Entry(row,textvariable=variable).pack(side='left',fill='x',expand=True)
        ttk.Button(row,text='Browse...',command=choose).pack(side='right',padx=(8,0))
    field('Original Amiga game folder (Steam import is not supported yet)',source,lambda:source.set(filedialog.askdirectory() or source.get()))
    field('New output folder',destination,lambda:destination.set(filedialog.askdirectory() or destination.get()))
    ttk.Label(panel,text='Creates the cartridge, cover, SD files and a verification report.\nYour source game files are never changed.',wraplength=640).pack(anchor='w',pady=(0,12))
    messages=queue.Queue();log=tk.Text(panel,height=7,bg='#0b1220',fg='#c4d5e9',relief='flat',font=('Consolas',10),state='disabled')
    log.pack(fill='both',expand=True,pady=(8,12))
    def work(source_path,destination_path):
        try:convert(source_path,destination_path,log=lambda s:messages.put(('log',s)));messages.put(('done','Your cartridge is ready. Open START-HERE.txt in the output folder.'))
        except Exception as e:messages.put(('error',str(e)))
    def start():
        button.configure(state='disabled');threading.Thread(target=work,args=(source.get(),destination.get()),daemon=True).start()
    button=ttk.Button(panel,text='Build my cartridge',command=start);button.pack(anchor='e')
    def poll():
        while not messages.empty():
            kind,text=messages.get();log.configure(state='normal');log.insert('end',text+'\n');log.see('end');log.configure(state='disabled')
            if kind!='log':
                button.configure(state='normal')
                (messagebox.showinfo if kind=='done' else messagebox.showerror)('Other Realm',text)
        root.after(100,poll)
    poll();root.mainloop()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,help='original game installation or ADF folder')
    p.add_argument('--adf',nargs='+',type=Path,help='original Amiga disk images')
    p.add_argument('--output',type=Path,default=Path('Otherrealm-personal'))
    p.add_argument('--firmware',type=Path);p.add_argument('--renderer',type=Path)
    p.add_argument('--gui',action='store_true')
    p.add_argument('--extract-cart',type=Path,help='validate a .chgame and extract its firmware / SD files')
    a=p.parse_args()
    if a.gui or not a.source and not a.adf and not a.extract_cart:return gui()
    try:
        if a.extract_cart:extract_cartridge(a.extract_cart,a.output)
        else:convert(a.source,a.output,a.adf,a.firmware,a.renderer)
    except (OSError,ValueError) as e:p.exit(1,f'Error: {e}\n')
    return 0


if __name__=='__main__':raise SystemExit(main())
