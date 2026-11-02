#!/usr/bin/env python3
"""Read values from the flat guest image dump (NHL_DUMP_IMAGE=1 -> guest_image_dump.bin, base 0x82000000).

Big-endian, like the guest. Output is small text, safe to paste or upload.

  python mods/legacy_rogue/tools/read_image.py guest_image_dump.bin words 0x83AA5B20 56
  python mods/legacy_rogue/tools/read_image.py guest_image_dump.bin floats 0x83ABF528 35
  python mods/legacy_rogue/tools/read_image.py guest_image_dump.bin str 0x82048220
  python mods/legacy_rogue/tools/read_image.py guest_image_dump.bin table 0x83AA5B20 14 16
      (rows of N bytes as words: one line per entry)

Words that point into the image are annotated with the C string there, if any.
Note: the dump is taken before the game runs, so .data holds initial values only
(heap pointers and lazily filled slots are 0).
"""
import struct, sys

BASE, SIZE = 0x82000000, 0x1EA0000


def cstr(d, va):
    o = va - BASE
    if not 0 <= o < len(d):
        return None
    e = d.find(b"\0", o, o + 200)
    raw = d[o:e] if e > o else b""
    if len(raw) >= 4 and all(32 <= b < 127 for b in raw):
        return raw.decode()
    return None


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    d = open(sys.argv[1], "rb").read()
    mode, va = sys.argv[2], int(sys.argv[3], 0)
    n = int(sys.argv[4], 0) if len(sys.argv) > 4 else 1
    o = va - BASE
    if mode == "str":
        print(f"{va:08X}  {cstr(d, va)!r}")
        return
    if mode == "table":
        row = int(sys.argv[5], 0) if len(sys.argv) > 5 else 16
        for i in range(n):
            ws = struct.unpack(">%dI" % (row // 4), d[o + i * row:o + (i + 1) * row])
            print(f"[{i:3d}] {va + i * row:08X}  " + " ".join(f"{w:08X}" for w in ws))
        return
    for i in range(n):
        a = va + 4 * i
        w = struct.unpack(">I", d[o + 4 * i:o + 4 * i + 4])[0]
        if mode == "floats":
            print(f"{a:08X}  {w:08X}  {struct.unpack('>f', struct.pack('>I', w))[0]:.6g}")
        else:
            s = cstr(d, w) if BASE <= w < BASE + SIZE else None
            print(f"{a:08X}  {w:08X}" + (f"  -> {s!r}" if s else ""))


if __name__ == "__main__":
    main()
