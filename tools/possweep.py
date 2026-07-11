#!/usr/bin/env python3
"""Sweep a function's POSITION within its TU and report the best-scoring slot.

Why this exists (v349): VC++ 5.0 carries optimizer state forward through a translation
unit, so the register allocation a function gets depends on WHICH FUNCTIONS WERE COMPILED
BEFORE IT in the same .cpp (Yoda #7). That had always been used as an explanation after
parking a residual; it is also an ACTIVE lever. Moving a function's block to a different
slot in its own .cpp -- not touching one character of its body -- took three functions in
src/PeerTrainNode.cpp from a 19-register-rename residual to BYTE-EXACT, including one that
v348 had parked as "intrinsic, retry never".

    tools/possweep.py src/PeerTrainNode.cpp 0x40e520 --len 204

Reads the TU, finds every `// FUNCTION: LOCO 0x...` block, and for each candidate slot
re-inserts the target block there and re-scores it with asmscore.py. The file is restored
on exit (including on Ctrl-C / exception). Prints one line per position, best last.

TRIAGE FIRST -- the lever acts on register ASSIGNMENT, so it can only help a residual whose
asmscore `reg_pen` is > 0. A `reg_pen=0` residual is an instruction-selection or structural
gap and scores identically at every position (confirmed on 0x44cb10 / 0x44d740 / 0x40df80).
`reg_pen > 0` is necessary but not sufficient (0x40e340 reg_pen=8 and 0x44d630 reg_pen=1 are
also flat) -- a FLAT SWEEP IS THE REAL PROOF that a residual is intrinsic, which is the other
half of this tool's value: it turns "probably intrinsic" into a measured fact worth writing
into a docs/PARKED.md row.

CAVEATS
  * Run only ONE sweep at a time. Two concurrent sweeps rewriting the same .cpp interleave
    their writes and silently corrupt each other's captured baseline (hit for real in v349).
  * Some slots won't compile (a block moved above a struct it depends on); those are
    reported as SKIP(compile) and are not a tool bug.
  * After ACCEPTING a move, always re-run the full `tools/cc.sh` on the TU: the position that
    fixes one function can perturb its neighbours. Then record the dependency in the
    function's own comment ("keep this block here") so a later tidy-up doesn't undo it.
  * `--len` should be the TRUE original body span from Ghidra's own `Body: START - END`
    (round up a byte or two -- that span can clip a trailing multi-byte instruction). Passing
    a stale candidate-side length silently truncates the comparison window; see CLAUDE.md.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARKER = re.compile(r"//\s*FUNCTION:\s*LOCO\s*(0x[0-9a-fA-F]+)")
SCORE = re.compile(r"Score\(total=(\d+)")


def blocks(text):
    """[(addr, start_offset, end_offset)] for each // FUNCTION: LOCO block, in file order.

    A block runs from the start of the contiguous `//` comment run above its marker through
    the first column-0 `}` after it, plus any trailing blank lines -- i.e. exactly the unit a
    human would cut and paste to move the function.
    """
    lines = text.split("\n")
    offs, o = [], 0
    for line in lines:
        offs.append(o)
        o += len(line) + 1
    out = []
    for i, line in enumerate(lines):
        m = MARKER.match(line.strip())
        if not m:
            continue
        s = i
        while s > 0 and lines[s - 1].startswith("//"):
            s -= 1
        e = i
        while e < len(lines) and lines[e] != "}":
            e += 1
        e += 1
        while e < len(lines) and lines[e] == "":
            e += 1
        out.append((m.group(1).lower(), offs[s], offs[e] if e < len(lines) else len(text)))
    return out


def move(text, addr, before):
    """Return `text` with block `addr` relocated just above block `before` ('END' = last)."""
    spans = {a: (s, e) for a, s, e in blocks(text)}
    s, e = spans[addr]
    blk, rest = text[s:e], text[:s] + text[e:]
    if before == "END":
        return rest + blk
    t = {a: s2 for a, s2, _ in blocks(rest)}[before]
    return rest[:t] + blk + rest[t:]


def score(path, addr, length):
    """Compile `path` and return (total, detail-line) for `addr`; total None if it failed."""
    r = subprocess.run(
        [os.path.join(ROOT, "tools", "asmscore.py"), path, addr, "--len", str(length)],
        capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if "Score" in line:
            m = SCORE.search(line)
            return (int(m.group(1)) if m else None), line.strip()
    return None, "SKIP(compile): " + (r.stdout + r.stderr).strip().replace("\n", " ")[:160]


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    path, addr = sys.argv[1], sys.argv[2].lower()
    length = int(sys.argv[sys.argv.index("--len") + 1], 0) if "--len" in sys.argv else None
    if length is None:
        sys.exit("--len <true original body size from Ghidra> is required; see the docstring.")

    orig = open(path).read()
    addrs = [a for a, _, _ in blocks(orig)]
    if addr not in addrs:
        sys.exit("no `// FUNCTION: LOCO %s` block in %s\nfound: %s" % (addr, path, addrs))

    results = []
    try:
        base_total, base_line = score(path, addr, length)
        print("baseline (as-is)   -> %s" % base_line, flush=True)
        for pos in addrs + ["END"]:
            if pos == addr:
                continue
            try:
                variant = move(orig, addr, pos)
            except KeyError:
                continue
            open(path, "w").write(variant)
            total, line = score(path, addr, length)
            print("  before %-10s -> %s" % (pos, line), flush=True)
            if total is not None:
                results.append((total, pos))
    finally:
        open(path, "w").write(orig)
        print("\n(%s restored)" % path, flush=True)

    if not results:
        return
    best_total, best_pos = min(results)
    spread = max(r[0] for r in results) - best_total
    if base_total is not None and best_total >= base_total:
        print("FLAT: no position beats the current one (spread %d) -- if reg_pen>0 this is "
              "real evidence the residual is INTRINSIC; say so in the PARKED.md row." % spread)
    else:
        print("BEST: move it before %s (total %d, was %s). Re-run tools/cc.sh on the whole TU "
              "before accepting -- neighbours can shift." % (best_pos, best_total, base_total))


if __name__ == "__main__":
    main()
