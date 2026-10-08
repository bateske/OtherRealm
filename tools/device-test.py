"""Drive a known Otherrealm debug build; collect timing and a framebuffer PNG.

Requires pyserial and Pillow. Never use this to probe unknown firmware; use
the CHGame uploader's bounded `info`/`probe` commands first.
"""
import argparse
import json
import time
from pathlib import Path

import serial
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--part", type=int, default=16002)
    parser.add_argument("--ticks", type=int, default=120)
    parser.add_argument("--buttons", default="0", help="hex: A=1 B=2 up=4 down=8 left=10 right=20")
    parser.add_argument("--continue-run", action="store_true", help="Continue current VM state instead of restarting its part")
    parser.add_argument("--keep-paused", action="store_true", help="Leave debug lockstep enabled for the next capture")
    parser.add_argument("--out", type=Path, default=Path("build/device-test"))
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + 10
    while True:
        try:
            connection = serial.Serial(args.port, 115200, timeout=0.2, write_timeout=1)
            break
        except serial.SerialException:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.2)
    with connection as port:
        def send(line):
            port.write((line + "\n").encode("ascii"))

        def receive(prefix, seconds=10):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                line = port.readline().decode("ascii", errors="replace").strip()
                if line.startswith("FAULT "):
                    raise RuntimeError(line)
                if line.startswith(prefix):
                    return line
            raise TimeoutError(f"No {prefix!r} response within {seconds}s")

        send("?")
        hello = receive("OTHERREALM ")
        send("L1")
        receive("OK")
        if not args.continue_run:
            send(f"G{args.part}")
            receive("OK")
        send(f"K{args.buttons}")
        receive("OK")
        send(f"N{args.ticks}")
        receive("OK ", max(10, args.ticks))
        send("P")
        report = receive("PERF ")
        send("S")
        header = receive("FRAME ").split()
        width, height, count = map(int, header[1:])
        payload = bytearray()
        end = time.monotonic() + 10
        while len(payload) < count and time.monotonic() < end:
            payload.extend(port.read(count - len(payload)))
        if len(payload) != count:
            raise TimeoutError("Incomplete framebuffer")
        packed, colors = payload[:-32], payload[-32:]
        palette = []
        for i in range(0, 32, 2):
            c = colors[i] | colors[i + 1] << 8
            palette.append(((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31))
        rgb = bytearray()
        for p in packed:
            rgb.extend(palette[p >> 4])
            rgb.extend(palette[p & 15])
        image = Image.frombytes("RGB", (width, height), bytes(rgb))
        image.resize((width * 4, height * 4), Image.Resampling.NEAREST).save(args.out / "frame.png")
        (args.out / "profile.json").write_text(json.dumps({"hello": hello, "report": report, "part": args.part, "ticks": args.ticks}, indent=2))
        send("K0")
        receive("OK")
        if not args.keep_paused:
            send("L0")
            receive("OK")
        print(report)
        print(args.out / "frame.png")


if __name__ == "__main__":
    main()
