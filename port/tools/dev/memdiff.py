#!/usr/bin/env python3
"""Compare two memdump.exe snapshots (same guest range) and list the changed big-endian words.

  python memdiff.py a.bin b.bin [--max 400] [--small] [--noise c.bin]

--small keeps only words where both values are small (< 256): option flags/indexes, not pointers or floats.
Output: guest address, old, new. Take A and B with the game idle on the same screen, only the option changed.
A third snapshot (same screen, nothing touched) lets you drop noise: pass it with --noise c.bin and any word
that differs between a.bin and c.bin is ignored.
Works block by block (4 KB) so 100 MB snapshots stay cheap: equal blocks are skipped without decoding.
"""
import argparse
import struct

BLOCK = 4096


def load(path):
    """-> {guest address of a record: bytes}"""
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


def block_at(mem, addr, size):
    """Bytes of [addr, addr+size) if one record covers it, else None."""
    for ga, blob in mem.items():
        if ga <= addr and addr + size <= ga + len(blob):
            return blob[addr - ga:addr - ga + size]
    return None


def diff_words(x, y, base):
    out = []
    n = len(x) // 4
    vx = struct.unpack(">%dI" % n, x[:n * 4])
    vy = struct.unpack(">%dI" % n, y[:n * 4])
    for i in range(n):
        if vx[i] != vy[i]:
            out.append((base + i * 4, vx[i], vy[i]))
    return out


def changed(a, b):
    """{addr: (va, vb)} for every word that differs, over the records both snapshots share."""
    res = {}
    for ga, blob in a.items():
        other = b.get(ga)
        if other is None or len(other) != len(blob):
            continue
        if blob == other:
            continue
        for off in range(0, len(blob), BLOCK):
            x, y = blob[off:off + BLOCK], other[off:off + BLOCK]
            if x != y:
                for addr, va, vb in diff_words(x, y, ga + off):
                    res[addr] = (va, vb)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--noise")
    ap.add_argument("--max", type=int, default=400)
    ap.add_argument("--small", action="store_true")
    args = ap.parse_args()
    a, b = load(args.a), load(args.b)
    diff = changed(a, b)
    if args.noise:
        noise = changed(a, load(args.noise))
        diff = {k: v for k, v in diff.items() if k not in noise}
    rows = sorted((k, v[0], v[1]) for k, v in diff.items()
                  if not args.small or (v[0] < 256 and v[1] < 256))
    print("%d changed word(s)" % len(rows))
    for k, va, vb in rows[:args.max]:
        print("%08X  %d -> %d  (0x%X -> 0x%X)" % (k, va, vb, va, vb))


if __name__ == "__main__":
    main()
