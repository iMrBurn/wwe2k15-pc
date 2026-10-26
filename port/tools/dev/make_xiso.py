"""Builds an Xbox 360 game-partition disc image (XDVDFS, "XISO") from an extracted game folder.

Test helper for the disc image support: python make_xiso.py <game folder> <out.iso> [--skip name ...]
The layout is what the SDK's DiscImageDevice reads: volume descriptor in sector 32
("MICROSOFT*XBOX*MEDIA", root directory sector and size), directory tables of entries
{left u16, right u16, sector u32, size u32, attributes u8, name length u8, name} 4-byte aligned,
each table a binary search tree over the case-insensitive names, files sector-aligned.
"""
import os
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"


def entry_size(name):
    return (14 + len(name) + 3) & ~3


class Node:
    def __init__(self, path, name):
        self.path, self.name = path, name
        self.is_dir = os.path.isdir(path)
        self.size = 0 if self.is_dir else os.path.getsize(path)
        self.children = []
        self.sector = 0


def scan(path, skip):
    node = Node(path, os.path.basename(path).encode("ascii"))
    if node.is_dir:
        for n in sorted(os.listdir(path), key=str.lower):
            if n.lower() in skip:
                continue
            node.children.append(scan(os.path.join(path, n), skip))
    return node


def table_bytes(children):
    """Directory table: a balanced binary search tree over the sorted names. The tree's root is
    written first (ordinal 0), so 0 can mean "no subtree" for every other link."""
    left, right = [None] * len(children), [None] * len(children)

    def build(lo, hi):
        if lo >= hi:
            return None
        mid = (lo + hi) // 2
        left[mid], right[mid] = build(lo, mid), build(mid + 1, hi)
        return mid

    root = build(0, len(children))
    order = [root] + [i for i in range(len(children)) if i != root]
    ordinal, pos = {}, 0
    for i in order:
        ordinal[i] = pos // 4
        pos += entry_size(children[i].name)
    out = bytearray()
    for i in order:
        c = children[i]
        l = ordinal[left[i]] if left[i] is not None else 0
        r = ordinal[right[i]] if right[i] is not None else 0
        rec = struct.pack("<HHIIBB", l, r, c.sector, c.size if not c.is_dir else c.table_size,
                          0x10 if c.is_dir else 0x80, len(c.name)) + c.name
        rec += b"\xff" * (entry_size(c.name) - len(rec))
        out += rec
    return bytes(out)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    skip = {s.lower() for s in sys.argv[3:] if s != "--skip"}
    root = scan(src, skip)

    # Assign sectors: header (sectors 0..33), then directory tables, then file data.
    next_sector = 34
    dirs = []

    def walk(n):
        if n.is_dir:
            dirs.append(n)
            for c in n.children:
                walk(c)

    walk(root)
    for d in dirs:
        d.table_size = sum(entry_size(c.name) for c in d.children) or 0
        d.sector = next_sector if d.children else 0
        next_sector += (d.table_size + SECTOR - 1) // SECTOR

    files = []

    def walk_files(n):
        for c in n.children:
            if c.is_dir:
                walk_files(c)
            else:
                files.append(c)

    walk_files(root)
    for f in files:
        f.sector = next_sector if f.size else 0
        next_sector += (f.size + SECTOR - 1) // SECTOR

    with open(dst, "wb") as out:
        out.truncate(next_sector * SECTOR)
        vd = MAGIC + struct.pack("<II", root.sector, root.table_size) + b"\0" * 8
        vd += b"\0" * (SECTOR - 20 - len(vd)) + MAGIC
        out.seek(32 * SECTOR)
        out.write(vd)
        for d in dirs:
            if d.children:
                out.seek(d.sector * SECTOR)
                out.write(table_bytes(d.children))
        done = 0
        for f in files:
            if not f.size:
                continue
            out.seek(f.sector * SECTOR)
            with open(f.path, "rb") as inp:
                while True:
                    chunk = inp.read(16 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
            done += f.size
        print(f"{len(files)} files, {len(dirs)} folders, {done / 2**30:.2f} GiB -> {dst}")


if __name__ == "__main__":
    main()
