#!/usr/bin/env python3
"""Read-only inventory of WWE 2K15 game files and known container signatures."""

from __future__ import annotations

import argparse
import struct
from collections import defaultdict
from pathlib import Path


SIGNATURES = {
    b"XEX2": "Xbox XEX2 executable/module",
    b"EPAC": "EPAC archive",
    b"PACH": "PACH table/container",
    b"KB2i": "Bink 2 video",
    b"AKPK": "WWE audio bank (AKPK)",
    b"BDES": "XZP/BDES data",
    b"xttf": "XTT font/data",
    b"\x0f\xf5\x12\xed": "XCompress/LZX block stream",
}


def classify(header: bytes, suffix: str) -> str:
    for signature, label in SIGNATURES.items():
        if header.startswith(signature):
            return label
    if suffix.lower() == ".pac" and header.startswith(b"PACH"):
        return "PAC/PACH container"
    if suffix.lower() == ".arc":
        return "ARC table (inspect tags to identify variant)"
    if suffix.lower() == ".def":
        return "definition/list file"
    if suffix.lower() == ".lex" and header.startswith(b"XEX2"):
        return "XEX2 executable module"
    return "unidentified"


def epac_header(header: bytes, file_size: int) -> str | None:
    if len(header) < 16 or header[:4] != b"EPAC":
        return None
    list_size, data_size, version = struct.unpack_from("<III", header, 4)
    list_end = 0x800 + list_size
    data_start = 0x4000
    valid = list_end <= data_start and data_start + data_size <= file_size
    return (
        f"list_size=0x{list_size:X}, data_size=0x{data_size:X}, "
        f"field_0C=0x{version:X}, list_at=0x800, data_at=0x4000, "
        f"bounds={'plausible' if valid else 'do not fit file'}"
    )


def pach_header(header: bytes, file_size: int) -> str | None:
    if len(header) < 8 or header[:4] != b"PACH":
        return None
    count = struct.unpack_from("<I", header, 4)[0]
    table_end = 8 + count * 12
    valid = count <= 1_000_000 and table_end <= file_size
    return f"count={count}, table_end=0x{table_end:X}, bounds={'plausible' if valid else 'do not fit file'}"


def read_pach_entries(path: Path, header: bytes, file_size: int) -> tuple[str, list[tuple[int, int, int]]] | None:
    details = pach_header(header, file_size)
    if details is None:
        return None
    count = struct.unpack_from("<I", header, 4)[0]
    table_end = 8 + count * 12
    if count > 1_000_000 or table_end > file_size:
        return details, []
    entries: list[tuple[int, int, int]] = []
    with path.open("rb") as stream:
        stream.seek(8)
        for _ in range(min(count, 5)):
            raw = stream.read(12)
            if len(raw) != 12:
                break
            entries.append(struct.unpack("<III", raw))
    return details, entries


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_root", type=Path, help="Root folder of an extracted game dump")
    parser.add_argument("--samples", type=int, default=3, help="Sample files per extension (default: 3)")
    args = parser.parse_args()

    root = args.game_root.expanduser().resolve()
    if not root.is_dir():
        parser.error(f"not a directory: {root}")

    totals: dict[str, list[int]] = defaultdict(lambda: [0, 0])
    samples: dict[str, list[Path]] = defaultdict(list)
    files = sorted(path for path in root.rglob("*") if path.is_file())
    for path in files:
        suffix = path.suffix.lower() or "<no extension>"
        totals[suffix][0] += 1
        totals[suffix][1] += path.stat().st_size
        with path.open("rb") as stream:
            header = stream.read(32)
        sample_key = f"{suffix} / {classify(header, path.suffix)}"
        if len(samples[sample_key]) < max(args.samples, 0):
            samples[sample_key].append(path)

    print(f"Root: {root}")
    print(f"Files: {len(files)}")
    print(f"Bytes: {sum(values[1] for values in totals.values())}")
    print("\nBy extension:")
    for suffix, (count, size) in sorted(totals.items(), key=lambda item: (-item[1][1], item[0])):
        print(f"  {suffix:16} {count:5} files  {size:12} bytes")

    print("\nHeader samples by detected type:")
    for sample_key in sorted(samples):
        for path in samples[sample_key]:
            with path.open("rb") as stream:
                header = stream.read(32)
            signature = header[:4]
            label = classify(header, path.suffix)
            hex_bytes = " ".join(f"{value:02X}" for value in header[:16])
            print(f"  {path.relative_to(root)} | {path.stat().st_size} bytes | {label} | {hex_bytes}")
            details = epac_header(header, path.stat().st_size)
            if details:
                print(f"    EPAC header candidate: {details}")
            pach = read_pach_entries(path, header, path.stat().st_size)
            if pach:
                pach_details, entries = pach
                print(f"    PACH table candidate: {pach_details}")
                for index, (file_number, relative_offset, size) in enumerate(entries):
                    print(
                        f"      [{index}] id={file_number} relative_offset=0x{relative_offset:X} "
                        f"size=0x{size:X}"
                    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
