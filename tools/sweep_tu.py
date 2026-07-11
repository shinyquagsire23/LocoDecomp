#!/usr/bin/env python3
"""Find un-transcribed work by walking app_funcs.txt against src/'s `// FUNCTION:` markers.

Two complementary views, both needed -- each is blind exactly where the other sees (v436):

  (b) RUNS of >=3 consecutive unclaimed functions whose neighbours on either side belong to
      two DIFFERENT files.  That is what an unclaimed WHOLE TU looks like, and it is invisible
      to any per-file window because it belongs to neither neighbour.  Found
      src/LoadingScreen.cpp and src/NetSetupWnd.cpp (v436) and src/ApplSetupWnd.cpp (v439).

  (a) ADJACENCY: an unclaimed function whose IMMEDIATELY adjacent markers on BOTH sides belong
      to the same file -- a leftover inside an already-claimed TU.  Note this deliberately does
      NOT use each file's marker min..max span: for a base class whose methods are scattered
      (src/WindowBase.cpp spans 0x402520-0x463600) that window swallows 96 KB of pure noise.

Before transcribing a candidate run, dump the class's VTABLE -- it names functions by slot and
often reveals further blocks of the same TU that this script reports separately.

Usage: tools/sweep_tu.py        (no arguments; reads toolchain/test/app_funcs.txt + src/*.cpp)
"""
import os, re, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

funcs = []
for line in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
    a, n = line.split()
    funcs.append((int(a, 16), int(n)))
funcs.sort()

MARK = re.compile(r"//\s*FUNCTION:\s*LOCO\s+0x([0-9a-fA-F]+)")
claim = {}
for f in glob.glob(os.path.join(ROOT, "src/*.cpp")):
    base = os.path.basename(f)
    for line in open(f, errors="ignore"):
        m = MARK.search(line)
        if m:
            claim[int(m.group(1), 16)] = base

# App region only. Above ~0x463800 is the static CRT (src/CrtLeaves.cpp's 0x46a000+ window,
# 122 funcs / 24 KB), which is not app code and would dominate every listing.
APP_HI = 0x463800


def owner(a):
    return claim.get(a)


print("=== (b) RUNS of >=3 consecutive UNCLAIMED funcs bracketed by different files ===")
i = 0
runs = []
while i < len(funcs):
    if owner(funcs[i][0]) is None and funcs[i][0] < APP_HI:
        j = i
        while j < len(funcs) and owner(funcs[j][0]) is None and funcs[j][0] < APP_HI:
            j += 1
        # neighbours
        prev = funcs[i - 1][0] if i > 0 else None
        nxt = funcs[j][0] if j < len(funcs) else None
        po = owner(prev) if prev is not None else "<start>"
        no = owner(nxt) if nxt is not None else "<end>"
        cnt = j - i
        size = sum(n for _, n in funcs[i:j])
        runs.append((size, cnt, funcs[i][0], funcs[j - 1][0], po, no))
        i = j
    else:
        i += 1

for size, cnt, lo, hi, po, no in sorted(runs, reverse=True):
    if cnt < 3:
        continue
    tag = "*** NEW TU CANDIDATE" if po != no else "    (in-TU leftover run)"
    print("%s  %5d B  %3d funcs  0x%06x..0x%06x   prev=%s next=%s"
          % (tag, size, cnt, lo, hi, po, no))

print()
print("=== (a) ADJACENCY form: unclaimed funcs whose BOTH neighbours are the SAME file ===")
byfile = {}
for k in range(len(funcs)):
    a, n = funcs[k]
    if owner(a) is not None or a >= APP_HI:
        continue
    p = next((funcs[x][0] for x in range(k - 1, -1, -1) if owner(funcs[x][0])), None)
    q = next((funcs[x][0] for x in range(k + 1, len(funcs)) if owner(funcs[x][0])), None)
    if p and q and owner(p) == owner(q):
        byfile.setdefault(owner(p), []).append((a, n))
for f, lst in sorted(byfile.items(), key=lambda kv: -sum(n for _, n in kv[1])):
    print("  %-34s %6d B  %3d funcs   %s"
          % (f, sum(n for _, n in lst), len(lst),
             " ".join("0x%06x" % a for a, _ in lst[:8])))
