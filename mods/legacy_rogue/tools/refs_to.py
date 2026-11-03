#!/usr/bin/env python3
"""Which functions reference these image addresses? Query the whole-program index made by
index_generated.py (func_index.tsv.gz, "refs" column) - no need to re-scan generated code.

  python mods/legacy_rogue/tools/refs_to.py index_out/func_index.tsv.gz 8206E01C 82082BB0 ...
  python mods/legacy_rogue/tools/refs_to.py index_out/func_index.tsv.gz --file vas.txt > refs.txt

Prints one line per (address, function): address, function, file, lines, n callees.
Addresses are hex without 0x (as in franchise_re_strings.txt). Also matches refs within +/- --slack
bytes (default 0) for tables that are addressed through a base.
"""
import argparse, gzip, sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("index")
    ap.add_argument("vas", nargs="*")
    ap.add_argument("--file")
    ap.add_argument("--slack", type=int, default=0)
    a = ap.parse_args()
    want = [v.strip().lower().replace("0x", "") for v in a.vas]
    if a.file:
        for line in open(a.file, encoding="utf-8", errors="ignore"):
            tok = line.split()
            if tok:
                want.append(tok[0].lower().replace("0x", ""))
    targets = {int(v, 16): v for v in want if v}
    hits = {t: [] for t in targets}
    with gzip.open(a.index, "rt", encoding="utf-8", errors="ignore") as f:
        for line in f:
            col = line.rstrip("\n").split("\t")
            if len(col) < 7 or col[0] == "func":
                continue
            refs = [int(r, 16) for r in col[6].split(",") if r]
            for r in refs:
                for t in targets:
                    if abs(r - t) <= a.slack:
                        hits[t].append((col[0], col[1], col[2], col[5].count(",") + (1 if col[5] else 0)))
    for t, v in targets.items():
        if not hits[t]:
            print(f"{t:08X}\t-\tno reference")
        for fn, file, lines, nc in hits[t]:
            print(f"{t:08X}\t{fn}\t{file}\tlines={lines}\tcallees={nc}")


if __name__ == "__main__":
    main()
