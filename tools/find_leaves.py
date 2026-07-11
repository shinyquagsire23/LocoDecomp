#!/usr/bin/env python3
"""
find_leaves.py — list good byte-match target functions in Loco.exe.

Default: small, branch-free (no call/jmp-to-elsewhere) leaf functions — the
ideal early Phase 2 wins (context-insensitive, MATCH == raw byte-identical;
see CLAUDE.md "Phase 2 — First byte-match").

Adapted from another project--
this version reads its function address/size list from a plain two-column
`addr(hex, no 0x) size(dec)` text dump instead (one line per function), the same
format Yodecomp's `toolchain/test/app_funcs.txt` uses — generated from Ghidra
(e.g. via a `run_script_inline` walk of `getFunctionManager()` per CLAUDE.md
pickup step 5), not from the binary itself. `--class`/demangling-based filtering
is dropped for the same reason (nothing to demangle pre-decomp); disassembly-based
filtering (size, branch-freedom) is architecture-generic and kept, just re-targeted
at x86 capstone groups instead of the ARM64 BRANCH mnemonic set.

⚠ toolchain/test/app_funcs.txt does not exist yet for Loco — this script is
ready to use the moment that dump is generated; until then it errors cleanly on
the default --table path.

Usage (from repo root):
  tools/find_leaves.py                        # default leaf sweep
  tools/find_leaves.py --max 40 --limit 60    # tune size / count
  tools/find_leaves.py --allow-calls          # include funcs with call/jmp
  tools/find_leaves.py --addr 0x401010        # dump one function's disasm

Prints: addr  size  [mnemonics]
"""
import argparse
import os
import sys

import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "loco", "Loco.exe")
TABLE = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")
TEXT_VA, TEXT_RAW = 0x401000, 0x400   # Loco.exe .text: VA 0x401000 -> file 0x400 (see tools/match.py)

BRANCH_GROUPS = {capstone.CS_GRP_CALL, capstone.CS_GRP_JUMP}


def load_table(path):
    """addr -> size, from a Ghidra-exported `addr(hex) size(dec)` dump."""
    out = {}
    for ln in open(path):
        p = ln.split()
        if len(p) == 2:
            out[int(p[0], 16)] = int(p[1])
    return out


def read_func(exe_bytes, addr, size):
    off = (addr - TEXT_VA) + TEXT_RAW
    return exe_bytes[off:off + size]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--table", default=TABLE,
                     help="Ghidra addr/size dump (default toolchain/test/app_funcs.txt)")
    ap.add_argument("--exe", default=EXE)
    ap.add_argument("--min", type=int, default=4)
    ap.add_argument("--max", type=int, default=40)
    ap.add_argument("--limit", type=int, default=40)
    ap.add_argument("--allow-calls", action="store_true",
                     help="include functions that call/jmp elsewhere (non-leaf)")
    ap.add_argument("--addr", default=None, help="dump one function at this address and exit")
    a = ap.parse_args()

    if not os.path.exists(a.table):
        print(f"no function table at {a.table} — export one from Ghidra first "
              f"(addr(hex) size(dec) per line, see this file's docstring)")
        return 2
    table = load_table(a.table)
    exe_bytes = open(a.exe, "rb").read()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    if a.addr:
        addr = int(a.addr, 16)
        size = table.get(addr)
        if size is None:
            print(f"0x{addr:x} not in {a.table}")
            return 1
        code = read_func(exe_bytes, addr, size)
        print(f"0x{addr:x}  {size}B")
        for i in md.disasm(code, addr):
            print(f"    0x{i.address:x}: {i.mnemonic:8s} {i.op_str}")
        return 0

    out = []
    for addr, size in table.items():
        if not (a.min <= size <= a.max):
            continue
        code = read_func(exe_bytes, addr, size)
        ins = list(md.disasm(code, addr))
        if not ins or sum(i.size for i in ins) != len(code):
            continue   # partial/garbage decode (mid-function data, misaligned table entry) -> skip
        mns = [i.mnemonic for i in ins]
        if not a.allow_calls and any(g in BRANCH_GROUPS for i in ins for g in i.groups):
            continue
        out.append((size, addr, mns))
    out.sort()
    for size, addr, mns in out[:a.limit]:
        print(f"0x{addr:06x}  {size:2d}B  [{' '.join(mns)}]")
    print(f"--- {len(out)} candidates (showing {min(a.limit, len(out))}) ---")


if __name__ == "__main__":
    main()
