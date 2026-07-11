#!/usr/bin/env python3
"""Rank the port build's generated DATA stubs by how wrong they are.

`link/gen_stubs.py` lays every unresolved data symbol inside a MIRROR of the original
image's 0x477000..0x501000 span -- which fixes the relative spacing between neighbouring
globals, but the mirror is a `.bss` section, so every byte of it reads as ZERO. That is
correct for a symbol that genuinely lives in the original's BSS tail, and silently,
invisibly WRONG for one that lives in initialised .rdata/.data: the port gets a zeroed
copy of a table the engine depends on, and nothing -- not the compiler, not the linker,
not any lint, not `stub_calls.log` (data stubs execute no code, so they cannot report
themselves) -- can see it.

That failure mode cost this project ~14 sessions of the port not rendering. `DAT_0047f108`
is the packed dirty-bitmap's 8-byte mask table (0x80 >> i). Zeroed, `bitmap[n>>3] |= 0`
marked nothing and `0 & bitmap[n>>3]` found nothing, so the board coalesced zero dirty
rects and blitted nothing, forever, while every gate upstream of it read as healthy.

This is the data-side analogue of sorting `loco/stub_calls.log` by count: it ranks the
remaining data stubs by the number of NON-ZERO bytes the original image actually holds at
that address. A stub with a non-zero footprint is a live defect; one whose span is all
zeros in the original is a genuine BSS global and the mirror is already right.

Usage:
    tools/datastubs.py                 # ranked table, worst first
    tools/datastubs.py --all           # include the provably-harmless zero ones
    tools/datastubs.py --syms F        # read a different gen_syms list
    tools/datastubs.py --hex NAME      # dump one symbol's original bytes
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "link"))

import gen_stubs  # noqa: E402  -- reuse its name->address derivation verbatim

EXE = os.path.join(ROOT, "loco", "Loco.exe")
DEFAULT_SYMS = os.path.join(ROOT, "link", "gen_syms_port.txt")

# How far past a symbol to look when the next symbol's address is unknown or far away.
# Wide enough to catch a real table, narrow enough not to charge a symbol for its
# neighbours' content.
MAX_SPAN = 64


def load_sections(data):
    """[(name, vaddr, vsize, rawoff, rawsize)] from the PE section table."""
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    tab = pe + 24 + opt
    out = []
    for i in range(nsec):
        e = tab + 40 * i
        name = data[e:e + 8].rstrip(b"\0").decode("ascii", "replace")
        vsize, vaddr, rawsize, rawoff = struct.unpack_from("<IIII", data, e + 8)
        out.append((name, base + vaddr, vsize, rawoff, rawsize))
    return out


def read_va(sections, data, va, n):
    """n bytes at virtual address va, or None if va is not in a mapped section.

    A span running past a section's RAW size is BSS tail and reads as zero, which is
    exactly what the mirror already gives -- so pad rather than truncate.
    """
    for _name, vaddr, vsize, rawoff, rawsize in sections:
        if vaddr <= va < vaddr + vsize:
            off = va - vaddr
            avail = max(0, min(n, rawsize - off))
            return data[rawoff + off:rawoff + off + avail] + b"\0" * (n - avail)
    return None


def section_of(sections, va):
    for name, vaddr, vsize, _rawoff, _rawsize in sections:
        if vaddr <= va < vaddr + vsize:
            return name
    return "?"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--syms", default=DEFAULT_SYMS, help="gen_syms list to read")
    ap.add_argument("--all", action="store_true", help="include all-zero (harmless) stubs")
    ap.add_argument("--hex", metavar="NAME", help="dump one symbol's original bytes")
    args = ap.parse_args()

    if not os.path.exists(EXE):
        sys.exit("datastubs: %s not found" % EXE)
    if not os.path.exists(args.syms):
        sys.exit("datastubs: %s not found -- run tools/build_port.sh first" % args.syms)

    data = open(EXE, "rb").read()
    sections = load_sections(data)
    table = gen_stubs.load_addr_table()

    syms = [s.strip() for s in open(args.syms) if s.strip()]
    rows = []          # (nonzero, addr, sym, span, section, preview)
    unplaced = []
    for sym in syms:
        if gen_stubs.is_code(sym):
            continue
        addr = gen_stubs.data_addr(sym, table)
        if addr is None:
            unplaced.append(sym)
            continue
        rows.append([0, addr, sym, 0, "", b""])

    # A symbol's span ends at its next placed neighbour: charging it for the bytes past
    # that would blame it for its neighbour's content.
    rows.sort(key=lambda r: r[1])
    for i, r in enumerate(rows):
        nxt = rows[i + 1][1] if i + 1 < len(rows) else r[1] + MAX_SPAN
        span = max(1, min(MAX_SPAN, nxt - r[1]))
        raw = read_va(sections, data, r[1], span) or b""
        r[0] = sum(1 for b in raw if b)
        r[3] = span
        r[4] = section_of(sections, r[1])
        r[5] = raw

    if args.hex:
        for r in rows:
            if args.hex in r[2]:
                print("%s  0x%06x  %s  span=%d" % (r[2], r[1], r[4], r[3]))
                print("  " + " ".join("%02x" % b for b in r[5]))
        return 0

    live = [r for r in rows if r[0]]
    show = rows if args.all else live
    show.sort(key=lambda r: (-r[0], r[1]))

    print("  data stubs: %d placed, %d with NON-ZERO original content, %d unplaced"
          % (len(rows), len(live), len(unplaced)))
    print("  a NON-ZERO row is a live defect: the port reads zeros where the game has data")
    print()
    print("  %-4s %-8s %-7s %-5s %s" % ("nz", "addr", "section", "span", "symbol"))
    for r in show:
        print("  %-4d %08x %-7s %-5d %s" % (r[0], r[1], r[4], r[3], r[2]))
        if r[0]:
            print("       %s" % " ".join("%02x" % b for b in r[5][:24]))
    if unplaced:
        print()
        print("  unplaced (no known address, mirror gives them an overflow slot):")
        for s in unplaced:
            print("    %s" % s)
    return 0


if __name__ == "__main__":
    sys.exit(main())
