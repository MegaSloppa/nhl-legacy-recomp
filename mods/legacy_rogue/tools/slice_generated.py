#!/usr/bin/env python3
"""Slice the recompiled code (generated/default/*.cpp, ~400 MB) into small, uploadable pieces.

  python mods/legacy_rogue/tools/slice_generated.py func sub_828CC300 > out.txt
      full body of one function
  python mods/legacy_rogue/tools/slice_generated.py callers sub_83411E90
      every function that calls it directly (+ file); indirect callers are not visible
  python mods/legacy_rogue/tools/slice_generated.py callees sub_828CC300
      direct callees of a function and its number of indirect calls
  python mods/legacy_rogue/tools/slice_generated.py offset 640 [--float] [--context 6]
      every load / store at displacement +640 from any register (e.g. struct fields), with
      a few lines of context; --float keeps only REX_LOAD_U32 into temp (lfs pattern)

All commands take --generated DIR (default generated/default) and --max N (default 200).
Output is plain text, small enough to upload as .txt.
"""
import argparse, glob, re, sys
from pathlib import Path

FUNC_RE = re.compile(r"DEFINE_REX_FUNC\((sub_[0-9A-Fa-f]{8})\)")


def iter_funcs(gen):
    for path in sorted(glob.glob(str(Path(gen) / "*.cpp"))):
        name, buf = None, []
        for line in open(path, "r", encoding="utf-8", errors="ignore"):
            m = FUNC_RE.search(line)
            if m:
                if name:
                    yield name, Path(path).name, buf
                name, buf = m.group(1), [line]
            elif name:
                buf.append(line)
        if name:
            yield name, Path(path).name, buf


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["func", "callers", "callees", "offset"])
    ap.add_argument("arg")
    ap.add_argument("--generated", default="generated/default")
    ap.add_argument("--max", type=int, default=200)
    ap.add_argument("--float", action="store_true")
    ap.add_argument("--context", type=int, default=6)
    a = ap.parse_args()
    out, n = [], 0
    if a.cmd == "func":
        for name, f, buf in iter_funcs(a.generated):
            if name == a.arg:
                out.append(f"// {f}\n")
                out += buf
                break
    elif a.cmd == "callers":
        pat = a.arg + "(ctx, base)"
        for name, f, buf in iter_funcs(a.generated):
            if name != a.arg and any(pat in l for l in buf):
                out.append(f"{name}\t{f}\n")
                n += 1
                if n >= a.max:
                    break
    elif a.cmd == "callees":
        for name, f, buf in iter_funcs(a.generated):
            if name == a.arg:
                body = "".join(buf)
                calls = sorted(set(re.findall(r"\b(sub_[0-9A-F]{8})\(ctx, base\)", body)))
                out.append(f"{name} ({f}): {len(calls)} direct callees, "
                           f"{body.count('REX_CALL_INDIRECT_FUNC')} indirect calls\n")
                out += [c + "\n" for c in calls]
                break
    else:
        off = int(a.arg, 0)
        if a.float:
            pat = re.compile(r"temp\.u32 = REX_LOAD_U32\(ctx\.r\d+\.u32 \+ %d\)" % off)
        else:
            pat = re.compile(r"REX_(?:LOAD|STORE)_\w+\(ctx\.r\d+\.u32 \+ %d[,)]" % off)
        for name, f, buf in iter_funcs(a.generated):
            for i, l in enumerate(buf):
                if pat.search(l):
                    lo, hi = max(0, i - a.context), min(len(buf), i + a.context + 1)
                    out.append(f"=== {name} ({f}) line {i}\n")
                    out += [x.strip() + "\n" for x in buf[lo:hi] if not x.strip().startswith("//")]
                    n += 1
                    break
            if n >= a.max:
                break
    sys.stdout.write("".join(out) if out else "no matches\n")


if __name__ == "__main__":
    main()
