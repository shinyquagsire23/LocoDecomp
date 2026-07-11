#!/usr/bin/env python3
"""Find RUNS of adjacent untranscribed functions -- the "never-opened .obj" sweep.

The premise (v487/v488): the cheapest bytes left in the binary are not in the parked
residuals, they are in the .obj nobody has opened. Because the linker keeps an .obj's
contributions together, a maximal run of ADJACENT functions with no `// FUNCTION: LOCO`
marker in src/ is almost always one of two things:

  * a whole translation unit nobody has touched (v487's src/DDrawSurface.cpp, 5 functions
    at 0x401000 -- the FIRST .obj in .text, and 100% untranscribed for 487 sessions), or
  * the HEAD or an INTERIOR HOLE of a TU we already own, which is even cheaper because the
    home file, its header and its structs already exist (v488's 0x4061e0..0x4068c4, the six
    functions immediately BEFORE src/AppWindow.cpp's existing 0x4068d0 start).

Telling those apart is what the `prev`/`next` owner columns are for: if either neighbour is
already owned by a src/ file, the run is that file's -- there is no home-TU question to
settle, which is the expensive part of adopting a genuinely new TU.

Sizes come from toolchain/test/app_funcs.txt, which records Ghidra's CODE extent per
function -- so they EXCLUDE any trailing jump table. Treat the byte totals as a ranking
signal, not as the `--len` you feed asmscore.py (see CLAUDE.md on deriving the COMDAT
extent from the next function's start address).

Usage:
    tools/objsweep.py              # top 15 runs by total bytes
    tools/objsweep.py -n 40        # top 40
    tools/objsweep.py --min-funcs 3
    tools/objsweep.py --all        # every run, including single functions
"""
import argparse
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Past this address it is all statically-linked CRT/library internals that will never get a
# marker -- the same cut progress.py's app-region view uses.
APP_END = 0x463800


def load_extents():
    path = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")
    if not os.path.exists(path):
        sys.exit("missing %s -- run the Ghidra app-region sweep first" % path)
    table = {}
    for line in open(path):
        parts = line.split()
        if len(parts) == 2:
            table[int(parts[0], 16)] = int(parts[1])
    return table


def load_owners():
    """address -> basename of the src/ .cpp whose marker claims it."""
    owners = {}
    for cpp in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True):
        text = open(cpp).read()
        for m in re.finditer(r"FUNCTION:\s*LOCO\s+0x0*([0-9a-fA-F]+)", text):
            owners[int(m.group(1), 16)] = os.path.basename(cpp)
    return owners


def find_runs(addrs, owners):
    runs, cur = [], []
    for a in addrs:
        if a in owners:
            if cur:
                runs.append(cur)
                cur = []
        else:
            cur.append(a)
    if cur:
        runs.append(cur)
    return runs


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-n", "--top", type=int, default=15, help="how many runs to print")
    ap.add_argument("--min-funcs", type=int, default=2,
                    help="skip runs shorter than this (default 2; lone functions are usually "
                         "compiler-generated thunks, not unopened .objs)")
    ap.add_argument("--all", action="store_true", help="print every run, no minimum")
    ap.add_argument("--members", action="store_true",
                    help="list each run's individual functions and sizes")
    args = ap.parse_args()

    table = load_extents()
    owners = load_owners()
    addrs = sorted(a for a in table if a < APP_END)
    index = {a: i for i, a in enumerate(addrs)}

    runs = find_runs(addrs, owners)
    if not args.all:
        runs = [r for r in runs if len(r) >= args.min_funcs]
    runs.sort(key=lambda r: -sum(table[a] for a in r))

    unmarked = sum(1 for a in addrs if a not in owners)
    print("%d app-region functions, %d unmarked, %d runs" % (len(addrs), unmarked, len(runs)))
    print("%-24s %4s %7s   %s" % ("run", "n", "bytes", "prev owner / next owner"))
    print("-" * 92)

    for r in runs[:args.top]:
        i, j = index[r[0]], index[r[-1]]
        prev = addrs[i - 1] if i > 0 else None
        nxt = addrs[j + 1] if j + 1 < len(addrs) else None
        span = "%#x..%#x" % (r[0], r[-1] + table[r[-1]])
        neighbours = "%s / %s" % (owners.get(prev, "-"), owners.get(nxt, "-"))
        print("%-24s %4d %7d   %s" % (span, len(r), sum(table[a] for a in r), neighbours))
        if args.members:
            for a in r:
                print("        %#x %6d" % (a, table[a]))


if __name__ == "__main__":
    main()
