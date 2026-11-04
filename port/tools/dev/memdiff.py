#!/usr/bin/env python3
"""Compare two memdump.exe snapshots (same guest range) and list the changed big-endian words.

  python memdiff.py a.bin b.bin [--max 400] [--small]

--small keeps only words where both values are small (< 256): option flags/indexes, not pointers or floats.
Output: guest address, old, new. Take A and B with the game idle on the same screen, only the option changed.
A third snapshot (same screen, nothing touched) lets you drop noise: pass it with --noise c.bin and any word
that differs between a.bin and c.bin is ignored.
"""
import argparse
import struct


def load(path):
    mem = {}
    with open(path, "rb") as f:
        data = f.read()
    p = 0
    while p + 8 <= len(data):
        ga, sz = struct.unpack_from("<II", data, p)
        p += 8
        mem[ga] = data[p:p + sz]
        p += sz
    return mem


def words(mem):
    out = {}
    for ga, blob in mem.items():
        n = len(blob) // 4
        vals = struct.unpack(">%dI" % n, blob[:n * 4])
        for i, v in enumerate(vals):
            out[ga + i * 4] = v
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--noise")
    ap.add_argument("--max", type=int, default=400)
    ap.add_argument("--small", action="store_true")
    args = ap.parse_args()
    a, b = words(load(args.a)), words(load(args.b))
    noise = set()
    if args.noise:
        c = words(load(args.noise))
        noise = {k for k, v in c.items() if a.get(k) != v}
    rows = []
    for k, va in a.items():
        vb = b.get(k)
        if vb is None or va == vb or k in noise:
            continue
        if args.small and not (va < 256 and vb < 256):
            continue
        rows.append((k, va, vb))
    rows.sort()
    print("%d changed word(s)" % len(rows))
    for k, va, vb in rows[:args.max]:
        print("%08X  %d -> %d  (0x%X -> 0x%X)" % (k, va, vb, va, vb))


if __name__ == "__main__":
    main()
