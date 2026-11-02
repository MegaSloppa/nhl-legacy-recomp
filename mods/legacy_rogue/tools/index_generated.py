#!/usr/bin/env python3
"""Build a compact whole-program index of generated/default/*.cpp for offline static analysis.

One row per recompiled function. The output is small enough to upload to an agent that
cannot read the ~400 MB generated code (gzip + base64 text parts, so chat uploads accept it).

  python mods/legacy_rogue/tools/index_generated.py --generated generated/default --out index_out
  python mods/legacy_rogue/tools/index_generated.py --generated generated/default --out index_out \
      --image path/to/default.decompressed.exe        (adds the text of referenced strings)

Output (in --out):
  func_index.tsv.gz       func  file  lines  n_direct  n_indirect  callees  refs
                            callees = direct sub_X calls (comma list, deduped)
                            refs    = image-range constant addresses the function builds or
                                      loads/stores through (lis/addi/ori + displacement),
                                      deduped, hex without 0x, capped at --max-refs
  strings.tsv.gz          va  text   (only with --image; only strings some function references)
  index.partNN.b64.txt    base64 of index_bundle.zip (the files above), split for upload

Address reconstruction is the same linear per-register tracking as find_tweak_consumers.py:
it ignores control flow, so refs are CANDIDATES (good for search, not proof).
"""
import argparse, base64, glob, gzip, io, re, sys, zipfile
from pathlib import Path

IMAGE_LO, IMAGE_HI = 0x82000000, 0x83EA0000
FUNC_RE = re.compile(r"DEFINE_REX_FUNC\((sub_[0-9A-Fa-f]{8})\)")
CALL_RE = re.compile(r"\b(sub_[0-9A-Fa-f]{8})\(ctx, base\)")
SET_RE = re.compile(r"ctx\.r(\d+)\.[su]64 = (-?\d+);")
ADD_RE = re.compile(r"ctx\.r(\d+)\.s64 = ctx\.r(\d+)\.s64 \+ (-?\d+);")
OR_RE = re.compile(r"ctx\.r(\d+)\.u64 = ctx\.r(\d+)\.u64 \| (\d+);")
MEM_RE = re.compile(r"ctx\.r(\d+)\.u(?:32|64) \+ (-?\d+)")
ASSIGN_RE = re.compile(r"ctx\.r(\d+)\.[su](?:32|64) = ")


def in_image(v):
    return IMAGE_LO <= v < IMAGE_HI


def index_file(path, max_refs):
    fname = Path(path).name
    cur, lines, n_ind = None, 0, 0
    callees, refs, seen, reg = set(), [], set(), {}

    def add_ref(v):
        if in_image(v) and v not in seen:
            seen.add(v)
            if len(refs) < max_refs:
                refs.append(v)

    def row():
        return (cur, fname, lines, len(callees), n_ind, ",".join(sorted(callees)),
                ",".join(f"{v:08X}" for v in sorted(refs)))

    for line in open(path, "r", encoding="utf-8", errors="ignore"):
        m = FUNC_RE.search(line)
        if m:
            if cur:
                yield row()
            cur, lines, n_ind = m.group(1), 0, 0
            callees, refs, seen, reg = set(), [], set(), {}
            continue
        if cur is None:
            continue
        lines += 1
        if line.lstrip().startswith("//"):
            continue
        for c in CALL_RE.findall(line):
            if c != cur:
                callees.add(c)
        if "REX_CALL_INDIRECT_FUNC" in line:
            n_ind += 1
        for mm in MEM_RE.finditer(line):
            r = int(mm.group(1))
            if r in reg:
                add_ref((reg[r] + int(mm.group(2))) & 0xFFFFFFFF)
        m = ADD_RE.search(line)
        if m:
            d, sr, imm = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if sr in reg:
                reg[d] = (reg[sr] + imm) & 0xFFFFFFFF
                add_ref(reg[d])
            else:
                reg.pop(d, None)
            continue
        m = OR_RE.search(line)
        if m:
            d, sr, imm = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if sr in reg:
                reg[d] = (reg[sr] | imm) & 0xFFFFFFFF
                add_ref(reg[d])
            else:
                reg.pop(d, None)
            continue
        m = SET_RE.search(line)
        if m:
            v = int(m.group(2)) & 0xFFFFFFFF
            reg[int(m.group(1))] = v
            add_ref(v)
            continue
        m = ASSIGN_RE.search(line)
        if m:  # any other write kills the tracked constant
            reg.pop(int(m.group(1)), None)
    if cur:
        yield row()


def read_cstring(data, va):
    off = va - IMAGE_LO
    if off < 0 or off >= len(data):
        return None
    end = data.find(b"\0", off, off + 512)
    if end - off < 4:
        return None
    raw = data[off:end]
    if any((b < 0x20 or b > 0x7E) and b not in (9, 10, 13) for b in raw):
        return None
    t = raw.decode("ascii")
    return t.replace("\t", " ").replace("\r", " ").replace("\n", " ")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--generated", default="generated/default")
    ap.add_argument("--out", default="index_out")
    ap.add_argument("--image", default=None, help="flat decompressed image (base 0x82000000)")
    ap.add_argument("--max-refs", type=int, default=96)
    ap.add_argument("--part-mb", type=float, default=14.0, help="size of each base64 part in MB")
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(glob.glob(str(Path(a.generated) / "*.cpp")))
    if not files:
        sys.exit(f"no .cpp files in {a.generated}")
    all_refs, n = set(), 0
    buf = io.StringIO()
    buf.write("func\tfile\tlines\tn_direct\tn_indirect\tcallees\trefs\n")
    for i, path in enumerate(files, 1):
        for r in index_file(path, a.max_refs):
            buf.write("\t".join(str(x) for x in r) + "\n")
            if r[6]:
                all_refs.update(int(x, 16) for x in r[6].split(","))
            n += 1
        print(f"[{i}/{len(files)}] {Path(path).name}: {n} functions so far", file=sys.stderr)
    blobs = {"func_index.tsv.gz": gzip.compress(buf.getvalue().encode(), 9)}
    if a.image:
        data = Path(a.image).read_bytes()
        sb, ns = io.StringIO(), 0
        sb.write("va\ttext\n")
        for va in sorted(all_refs):
            t = read_cstring(data, va)
            if t:
                sb.write(f"{va:08X}\t{t}\n")
                ns += 1
        blobs["strings.tsv.gz"] = gzip.compress(sb.getvalue().encode(), 9)
        print(f"strings referenced: {ns}", file=sys.stderr)
    zb = io.BytesIO()
    with zipfile.ZipFile(zb, "w", zipfile.ZIP_STORED) as z:
        for k, v in blobs.items():
            (out / k).write_bytes(v)
            z.writestr(k, v)
    b64 = base64.b64encode(zb.getvalue())
    step = int(a.part_mb * 1024 * 1024)
    for old in out.glob("index.part*.b64.txt"):
        old.unlink()
    parts = [b64[i:i + step] for i in range(0, len(b64), step)] or [b""]
    for k, p in enumerate(parts, 1):
        (out / f"index.part{k:02d}.b64.txt").write_bytes(p)
    print(f"{n} functions from {len(files)} files -> {out} ({len(parts)} upload part(s), "
          f"{len(b64) / 1e6:.1f} MB base64). Upload every index.partNN.b64.txt.")


if __name__ == "__main__":
    main()
