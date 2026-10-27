#!/usr/bin/env python3
"""Find WWE 2K15's XDK D3D functions by instruction-sequence similarity with a port that names them.

Reads the generated code of both games (the PowerPC mnemonics the recompiler keeps as comments),
takes the functions named in the SvR 2010 port's svr_d3d_names.h, and ranks the 2K15 functions of
the D3D window (around the only VdSwap caller) by difflib similarity. Paths are the local
checkouts; nothing is written. Result of 2026-10-27: port/src/native/wwe2k15_d3d_names.h.
"""
import re, glob, json, difflib
def funcs(pattern, wanted=None, lo=None, hi=None):
    out = {}
    for f in glob.glob(pattern):
        text = open(f, encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\) \{\n(.*?)^\}', text, re.S | re.M):
            a = m.group(1)
            if wanted is not None and a not in wanted: continue
            if lo is not None and not (lo <= int(a, 16) < hi): continue
            out[a] = [l.split()[0] for l in re.findall(r'^\t// ([a-z][a-z0-9.+-]*(?: .*)?)$', m.group(2), re.M)]
    return out
names = {}
for line in open(r'C:/Dev/WWE-SVR2010-RECOMP/src/native/svr_d3d_names.h'):
    m = re.match(r'#define (D3D\w+|\w+)\s+sub_([0-9A-F]{8})', line)
    if m and not m.group(1).startswith('__imp'): names[m.group(2)] = m.group(1)
svr = funcs(r'C:/Dev/WWE-SVR2010-RECOMP/generated/default/*.cpp', wanted=set(names))
k15 = funcs(r'C:/Dev/wwe2k15-pc2/port/generated/default/wwe2k15_recomp.*.cpp', lo=0x82B9C000, hi=0x82BB8000)
print(len(svr), 'svr named,', len(k15), '2k15 window')
for a, ops in sorted(svr.items(), key=lambda x: names[x[0]]):
    scores = []
    for b, ops2 in k15.items():
        if abs(len(ops2) - len(ops)) > max(20, len(ops)): continue
        sm = difflib.SequenceMatcher(None, ops, ops2, autojunk=False)
        if sm.real_quick_ratio() < 0.5: continue
        scores.append((sm.ratio(), b, len(ops2)))
    scores.sort(reverse=True)
    print(f"{names[a]:42s} svr sub_{a} ({len(ops)} ins) -> " + ", ".join(f"sub_{b} {r:.2f} ({n})" for r, b, n in scores[:3]))
