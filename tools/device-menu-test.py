"""Exercise the known, silent Otherrealm debug firmware's menu and save journal.

Run once normally, relaunch the debug firmware, then run with --resume to
verify that CONTINUE restores the flash checkpoint without another write.
Requires pyserial and Pillow. Never run against unknown firmware.
"""
import argparse
import json
import struct
import time
import zlib
from pathlib import Path

import serial
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--out", type=Path, default=Path("build/device-test/menu"))
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
        def receive(prefix, seconds=10):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                line = port.readline().decode("ascii", errors="replace").strip()
                if line.startswith("FAULT "):
                    raise RuntimeError(line)
                if line.startswith(prefix):
                    return line
            raise TimeoutError(f"No {prefix!r} response")

        def send(line, prefix="OK"):
            port.write((line + "\n").encode("ascii"))
            return receive(prefix) if prefix else None

        def press(mask):
            send(f"K{mask:x}")
            time.sleep(0.06)
            send("K0")
            time.sleep(0.06)

        def profile():
            return dict(field.split("=", 1) for field in send("P", "PERF ").split()[1:])

        def binary(command, prefix):
            header = send(command, prefix).split()
            count = int(header[-1])
            payload = bytearray()
            end = time.monotonic() + 10
            while len(payload) < count and time.monotonic() < end:
                payload.extend(port.read(count - len(payload)))
            assert len(payload) == count, (len(payload), count)
            return bytes(payload)

        def ui(name):
            pixels = binary("U", "UI ")
            rgb = bytearray()
            for (color,) in struct.iter_unpack("<H", pixels):
                rgb.extend(((color >> 11) * 255 // 31, ((color >> 5) & 63) * 255 // 63, (color & 31) * 255 // 31))
            Image.frombytes("RGB", (128, 128), bytes(rgb)).resize((512, 512), Image.Resampling.NEAREST).save(args.out / name)

        hello = send("?", "OTHERREALM ")
        send("L1")
        send("K0")
        if args.resume:
            boot = profile()
            assert boot["menu"] == "1" and boot["saved"] == "1" and boot["writes"] == "0", boot
            ui("continue.png")
            press(1)  # CONTINUE
            assert profile()["part"] == "16002"
            send("N1", "OK ")
            assert profile()["writes"] == "0", "Restoring an unchanged checkpoint rewrote flash"
        else:
            boot = None
            send("G16002")
            send("N2", "OK ")
            writes = int(profile()["writes"])
            send("G16003")
            send("N1", "OK ")
            assert int(profile()["writes"]) == writes + 1, "New chapter did not save"
            send("G16002")
            send("N1", "OK ")
            assert int(profile()["writes"]) == writes + 2, "Journal did not rotate back"
        initial = profile()
        assert initial["saved"] == "1" and initial["save_failed"] == "0" and initial["fault"] == "none", initial
        before = binary("S", "FRAME ")
        press(64)  # START opens the system menu.
        paused = profile()
        assert paused["menu"] == "1", paused
        ui("pause.png")
        time.sleep(0.3)
        assert binary("S", "FRAME ") == before, "Menu changed a game drawing page"
        assert profile()["frames"] == paused["frames"], "Game advanced while paused"
        press(64)  # START resumes.
        assert profile()["menu"] == "0"
        press(64)  # A second START proves the released edge was tracked.
        assert profile()["menu"] == "1"
        press(8)   # RETRY SAVE
        press(1)
        send("N1", "OK ")
        retried = profile()
        assert retried["part"] == "16002" and retried["writes"] == initial["writes"], retried
        press(64)
        press(8)
        press(8)   # NEW GAME opens confirmation; it cannot immediately destroy a save.
        press(1)
        ui("confirm-new.png")
        assert profile()["writes"] == initial["writes"]
        press(2)   # B cancels and preserves the checkpoint.
        final = profile()
        assert final["menu"] == "1" and final["saved"] == "1" and final["save_failed"] == "0", final
        pages = binary("F", "FLASH ")
        records = []
        for slot in range(2):
            page = pages[slot * 256:(slot + 1) * 256]
            magic, identity, sequence, size, version, _ = struct.unpack_from("<IIIHBB", page)
            crc, commit = struct.unpack_from("<II", page, 248)
            if magic == 0x3257524F and version == 1 and commit == 0x51A7C0DE and crc == zlib.crc32(page[:248]):
                records.append(dict(slot=slot, identity=identity, sequence=sequence, size=size, part=struct.unpack_from("<H", page, 22)[0]))
        assert records and any(record["part"] == 16002 for record in records), records
        report = dict(hello=hello, mode="resume" if args.resume else "create", boot=boot, initial=initial, paused=paused, retried=retried, final=final, records=records)
        (args.out / "profile.json").write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2))
        print("Menu, paused pages, retry, confirmation and flash-record checks passed")


if __name__ == "__main__":
    main()
