#!/usr/bin/env python3
"""TUN-8: which recompiled functions touch a tweak object (or its game-style table)?

TUN-5 (2026-10-29) wrote the real value (object N+20 and the heap copy) of
gSlapShotPowerMultiplier, gMomentumPhysicsImpulse and gHitTune_GlobalScale at x0.2..x10,
in a match and before one: no visible effect. So either nothing reads those objects during
play (they are editor mirrors, read once at boot, or overridden by other data), or the game
reads a different location. This scan answers "who references the object at all".

Object addresses come from the TUN-4/TUN-5 probe logs (OBSERVED, docs/re-findings.md).
Generated code builds addresses as DECIMAL lis/addi/ori pairs and accesses memory as
`ctx.rN.u32 + disp`; we track per-register constants like find_franchise_code.py and also
resolve `rN + disp` inside load/store expressions, then match against each object's span.

  python mods/legacy_rogue/tools/find_tweak_consumers.py --generated generated/default \
      --out tweak_consumers.txt

TUN-9 catalog mode (every tweak object, from the panel's "Dump tweak catalog" button):

  python mods/legacy_rogue/tools/find_tweak_consumers.py --generated generated/default \
      --catalog out/build/win-amd64-vk-ffx/rogue_tweak_catalog.txt --out tweak_readers.txt

writes one row per tunable: how many functions LOAD its live value (N+32, CONFIRMED by TUN-5b)
and which. Tunables with readers are the ones a perk can drive by writing N+32.

Output per object: functions that reference it, the offset from the name pointer N
(+8 = type tag, +20 = value, +24/+28/+32 = min/max/default, +36 = heap-copy pointer),
and up to 6 source lines per function so loads vs stores and f32 vs u32 are visible.
"""
import argparse, glob, re
from collections import defaultdict
from pathlib import Path

# name -> address N of the name pointer inside the image object (object base = N - 8)
OBJECTS = {
    "gSlapShotPowerMultiplier": 0x83C8430C,
    "gSlapShotPowerScale": 0x83C7EC50,
    "gSlapShotPowerSlider": 0x83C82A18,
    "gMomentumPhysicsImpulse": 0x83CD9F78,
    "gHitTune_GlobalScale": 0x83CDEE78,
    "gBeAPro_LineEnergyBoost": 0x83C77F38,
    "gTweakByGameStyleGameSpeedLevel": 0x83BD68F8,
    "gTweakByGameStyleSlapShotPowerLevel": 0x83BD6B30,
    "gTweakByGameStyleWristShotAccuracyLevel": 0x83BD6B00,
    # positive controls (rendering; likely read by real code)
    "gLODShader": 0x83D1F7A8,
    "gEnableLynxBinaryFileLoad": 0x83D207FC,
}
# static per-game-style value tables (tag 13 objects, N+24 points here), 3 x u32
TABLES = {
    "table:gTweakByGameStyleGameSpeedLevel": 0x839DEC74,
    "table:gTweakByGameStyleSlapShotPowerLevel": 0x839DEDB8,
    "table:gTweakByGameStyleWristShotAccuracyLevel": 0x839DED70,
}
SPAN_BEFORE, SPAN_AFTER = 8, 48

FUNC_RE = re.compile(r"DEFINE_REX_FUNC\((sub_[0-9A-Fa-f]{8})\)")
SET_RE = re.compile(r"ctx\.r(\d+)\.[su]64 = (-?\d+);")
ADD_RE = re.compile(r"ctx\.r(\d+)\.s64 = ctx\.r(\d+)\.s64 \+ (-?\d+);")
OR_RE = re.compile(r"ctx\.r(\d+)\.u64 = ctx\.r(\d+)\.u64 \| (\d+);")
MEM_RE = re.compile(r"ctx\.r(\d+)\.u(?:32|64) \+ (-?\d+)")


def build_ranges():
    rs = []
    for name, n in OBJECTS.items():
        rs.append((n - SPAN_BEFORE, n + SPAN_AFTER, name, n))
    for name, t in TABLES.items():
        rs.append((t, t + 12, name, t))
    return rs


def classify(addr, ranges):
    for lo, hi, name, anchor in ranges:
        if lo <= addr < hi:
            return name, addr - anchor
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--generated", required=True, help="generated/default directory")
    ap.add_argument("--out", default=None)
    ap.add_argument("--catalog", default=None, help="rogue_tweak_catalog.txt (TUN-9 mode, tab-separated)")
    a = ap.parse_args()
    if a.catalog:
        return catalog_mode(a)
    ranges = build_ranges()
    lo_all = min(r[0] for r in ranges)
    hi_all = max(r[1] for r in ranges)
    # (name) -> {(func, file) -> set(offsets)} and sample lines
    hits = defaultdict(lambda: defaultdict(set))
    samples = defaultdict(list)
    files = sorted(glob.glob(str(Path(a.generated) / "*.cpp")))

    def record(addr, cur, fname, line):
        if not (lo_all <= addr < hi_all):
            return
        c = classify(addr, ranges)
        if not c:
            return
        name, off = c
        hits[name][(cur, fname)].add(off)
        key = (name, cur)
        if len(samples[key]) < 6:
            samples[key].append(f"{off:+d}: {line.strip()[:160]}")

    for path in files:
        fname = Path(path).name
        cur, reg = None, {}
        for line in open(path, "r", encoding="utf-8", errors="ignore"):
            m = FUNC_RE.search(line)
            if m:
                cur, reg = m.group(1), {}
                continue
            for mm in MEM_RE.finditer(line):
                r, disp = int(mm.group(1)), int(mm.group(2))
                if r in reg:
                    record((reg[r] + disp) & 0xFFFFFFFF, cur, fname, line)
            m = ADD_RE.search(line)
            if m:
                d, s, imm = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if s in reg:
                    reg[d] = (reg[s] + imm) & 0xFFFFFFFF
                    record(reg[d], cur, fname, line)
                else:
                    reg.pop(d, None)
                continue
            m = OR_RE.search(line)
            if m:
                d, s, imm = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if s in reg:
                    reg[d] = (reg[s] | imm) & 0xFFFFFFFF
                    record(reg[d], cur, fname, line)
                continue
            m = SET_RE.search(line)
            if m:
                v = int(m.group(2)) & 0xFFFFFFFF
                reg[int(m.group(1))] = v
                record(v, cur, fname, line)

    out = ["# TUN-8 tweak object consumer scan",
           f"# {len(files)} generated files scanned",
           "# offsets are from the name pointer N: +8 tag, +20 value, +24 min, +28 max,",
           "# +32 default, +36 heap-copy pointer; -8 type class. Tables: offset from table start.", ""]
    for name in list(OBJECTS) + list(TABLES):
        fns = hits.get(name, {})
        out.append(f"== {name}: {len(fns)} function(s)")
        for (fn, fname), offs in sorted(fns.items())[:40]:
            out.append(f"   {fn} ({fname}) offsets {sorted(offs)}")
            for s in samples[(name, fn)]:
                out.append(f"        {s}")
        out.append("")
    text = "\n".join(out) + "\n"
    if a.out:
        Path(a.out).write_text(text, "utf-8")
        print(f"wrote {a.out}")
    else:
        print(text)


LOAD_RE = re.compile(r"REX_LOAD_(?:U8|U16|U32|U64|F32|F64)?\w*\(ctx\.r(\d+)\.u(?:32|64)(?: \+ (-?\d+))?\)")


def catalog_mode(a):
    import bisect
    rows = []
    for line in open(a.catalog, "r", encoding="utf-8", errors="ignore"):
        if line.startswith("#") or line.startswith("name\t"):
            continue
        p = line.rstrip("\n").split("\t")
        if len(p) < 9:
            continue
        rows.append(p)
    starts = sorted((int(p[2], 16) - 8, i) for i, p in enumerate(rows))
    keys = [s for s, _ in starts]
    readers = defaultdict(set)   # row -> {(func,file)} loading N+32..N+35
    other = defaultdict(set)     # row -> {(func,file)} any other reference
    lo_all, hi_all = keys[0], keys[-1] + 48

    def hit(addr, cur, fname, is_load):
        if not (lo_all <= addr < hi_all):
            return
        k = bisect.bisect_right(keys, addr) - 1
        if k < 0:
            return
        base, i = starts[k]
        off = addr - (base + 8)
        if addr >= base + 48:
            return
        if is_load and 32 <= off < 36:
            readers[i].add((cur, fname))
        else:
            other[i].add((cur, fname))

    files = sorted(glob.glob(str(Path(a.generated) / "*.cpp")))
    for path in files:
        fname = Path(path).name
        cur, reg = None, {}
        for line in open(path, "r", encoding="utf-8", errors="ignore"):
            m = FUNC_RE.search(line)
            if m:
                cur, reg = m.group(1), {}
                continue
            for mm in LOAD_RE.finditer(line):
                r = int(mm.group(1))
                if r in reg:
                    hit((reg[r] + int(mm.group(2) or 0)) & 0xFFFFFFFF, cur, fname, True)
            m = ADD_RE.search(line)
            if m:
                d, s, imm = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if s in reg:
                    reg[d] = (reg[s] + imm) & 0xFFFFFFFF
                    hit(reg[d], cur, fname, False)
                else:
                    reg.pop(d, None)
                continue
            m = OR_RE.search(line)
            if m:
                d, s, imm = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if s in reg:
                    reg[d] = (reg[s] | imm) & 0xFFFFFFFF
                continue
            m = SET_RE.search(line)
            if m:
                reg[int(m.group(1))] = int(m.group(2)) & 0xFFFFFFFF
    out = ["name\ttag\tvalue\tmin\tmax\treaders\tother_refs\treader_functions"]
    order = sorted(range(len(rows)), key=lambda i: (-len(readers[i]), rows[i][0]))
    for i in order:
        p = rows[i]
        fns = ",".join(sorted(f"{fn}@{fl.replace('nhllegacy_recomp.', '').replace('.cpp', '')}"
                              for fn, fl in readers[i])[:12])
        out.append(f"{p[0]}\t{p[1]}\t{p[4]}\t{p[6]}\t{p[7]}\t{len(readers[i])}\t{len(other[i])}\t{fns}")
    live = sum(1 for i in range(len(rows)) if readers[i])
    out.append(f"# {len(rows)} tunables, {live} with a static reader of N+32, {len(files)} files")
    text = "\n".join(out) + "\n"
    if a.out:
        Path(a.out).write_text(text, "utf-8")
        print(f"wrote {a.out}: {live} of {len(rows)} tunables have a static reader")
    else:
        print(text)


if __name__ == "__main__":
    main()
