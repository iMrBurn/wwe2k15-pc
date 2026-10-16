#!/usr/bin/env python3
"""Turn ReXGlue's false-fatal branches into local gotos when their label exists."""

from __future__ import annotations

import re
from pathlib import Path


GENERATED = Path(__file__).resolve().parents[1] / "generated" / "default"
FUNCTION = re.compile(r"^DEFINE_REX_FUNC\([^\n]+\) \{", re.MULTILINE)
ERROR = re.compile(
    r'(?m)^(\s*)if \((.*?)\) REX_FATAL\('
    r'"Unresolved branch from 0x[0-9A-Fa-f]{8} to 0x([0-9A-Fa-f]{8})"\);$'
)


def patch_file(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    starts = list(FUNCTION.finditer(text))
    replacements: list[tuple[int, int, str, int]] = []
    for index, match in enumerate(starts):
        end = starts[index + 1].start() if index + 1 < len(starts) else len(text)
        body = text[match.start() : end]
        for error in ERROR.finditer(body):
            target = error.group(3).upper()
            label = re.compile(rf"(?m)^loc_{target}:$")
            if label.search(body):
                begin = match.start() + error.start()
                finish = match.start() + error.end()
                replacement = f"{error.group(1)}if ({error.group(2)}) goto loc_{target};"
                replacements.append((begin, finish, replacement, error.start()))

    for begin, finish, replacement, _ in reversed(replacements):
        text = text[:begin] + replacement + text[finish:]
    if replacements:
        path.write_text(text, encoding="utf-8", newline="\n")
    return len(replacements)


def main() -> None:
    patched = 0
    for source in sorted(GENERATED.glob("*.cpp")):
        patched += patch_file(source)
    print(f"Repaired {patched} local unresolved branch(es).")


if __name__ == "__main__":
    main()
