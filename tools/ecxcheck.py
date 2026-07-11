#!/usr/bin/env python3
"""Usage: tools/ecxcheck.py <addr> [<addr> ...]     (hex, with or without 0x)

For each given function address, examine every direct-call site in Loco.exe and decide
whether the caller establishes ECX for it.  A function our source models as a free
__stdcall/__cdecl function whose call sites all establish ecx is really a __thiscall member
whose body happens never to read `this` (the PostBagCacheBundle class -- see src/PostBag.h).

Method: disassemble a window ending at the CALL, walk backwards to the previous call/branch
(the basic-block tail), and report the last instruction that WRITES ecx.  "elided" = no write
in the tail, which is inconclusive: `this` may still be live in ecx from the prologue.
"""
import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_OPT_SYNTAX_INTEL

import os
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = open(os.path.join(_ROOT, "loco/Loco.exe"), "rb").read()
TEXT_VA, TEXT_RAW, TEXT_LEN = 0x401000, 0x400, 0x78000
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.syntax = CS_OPT_SYNTAX_INTEL
md.detail = True


def call_sites(target):
    out = []
    for i in range(TEXT_RAW, TEXT_RAW + TEXT_LEN):
        if EXE[i] != 0xE8:
            continue
        rel = struct.unpack_from("<i", EXE, i + 1)[0]
        va = TEXT_VA + (i - TEXT_RAW)
        if va + 5 + rel == target:
            out.append(va)
    return out


def tail_ecx(call_va, window=64):
    """Disassemble [call_va-window, call_va) aligned by trying each start offset until the
    decode lands exactly on call_va; return the last ecx-writing insn in the block tail."""
    # largest window first: a long decode that lands exactly on the call is far more likely
    # to be the real instruction stream than a short accidentally-aligned one (a short window
    # can start AFTER the ecx load and wrongly report "elided").
    for back in range(window, 7, -1):
        start = call_va - back
        code = EXE[start - TEXT_VA + TEXT_RAW:call_va - TEXT_VA + TEXT_RAW]
        insns = list(md.disasm(code, start))
        if not insns or insns[-1].address + insns[-1].size != call_va:
            continue
        if len(insns) < 3:
            continue
        last = None
        for ins in insns:
            if ins.mnemonic in ("call", "jmp") or ins.mnemonic.startswith("j"):
                last = None            # new basic block / ecx clobbered by a call
                continue
            regs_read, regs_written = ins.regs_access()
            names = {md.reg_name(r) for r in regs_written}
            if "ecx" in names or "cx" in names or "cl" in names:
                last = f"{ins.mnemonic} {ins.op_str}"
        return last
    return "?undecodable"


for arg in sys.argv[1:]:
    target = int(arg, 16)
    sites = call_sites(target)
    rows = [(s, tail_ecx(s)) for s in sites]
    n = sum(1 for _, d in rows if d and d != "?undecodable")
    if not rows:
        verdict = "no direct call sites"
    elif n == len(rows):
        verdict = "THISCALL (every site establishes ecx)"
    elif n == 0:
        verdict = "free (no site establishes ecx)"
    else:
        verdict = f"MIXED ({n}/{len(rows)}) -- elided sites may still have `this` live"
    print(f"{target:#08x}  {len(sites)} site(s)  ->  {verdict}")
    for s, d in rows:
        print(f"      {s:#08x}  {d or '(elided -- no ecx write in block tail)'}")
