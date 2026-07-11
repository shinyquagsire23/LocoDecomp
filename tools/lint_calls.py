#!/usr/bin/env python3
"""Call-relocation sweep: verify WHICH function each call in an EXACT-matched TU resolves to.

`tools/match.py` MASKS relocations before comparing bytes, so a byte-EXACT function proves
nothing about the identity of the functions it CALLS -- two different callees produce the same
masked bytes.  Source that names the wrong overload, the wrong calling convention, or the wrong
class therefore byte-matches silently.  (Found for real in v411: an `istream::operator>>(float&)`
that was really `operator>>(short&)`, wrong for eight sessions; and again in v412, below.)

Method: for each `// FUNCTION: LOCO 0x...` marker paired to its COMDAT, walk our .obj's call
relocations; at each offset whose preceding byte is 0xE8, decode the ORIGINAL's rel32 at the
same offset to get the address the ORIGINAL calls, and record `original target -> the mangled
symbol OUR source emitted`.  Only EXACT functions are trusted by default (offsets align).

Two reports:
  * CONFLICTS -- one original address reached under >1 distinct mangled symbol anywhere in the
    repo.  The linker maps a symbol to exactly one address, so this always means at least one
    spelling is wrong -- differing return type, calling convention, or owning class.  Some are
    known-deliberate duplicate views (see docs/subsystems.md); read each one.
  * istream::operator>> overload audit -- the callee addresses below were pinned by
    disassembling each one (v412); a symbol whose parameter letter is not in the allowed set
    for the address it lands on is a WRONG OVERLOAD, i.e. a struct field of the wrong type.

See also `tools/ecxcheck.py`, which answers the complementary question a CONFLICT often raises:
is this really a free function, or a __thiscall member whose body never reads `this`?

Usage: tools/lint_calls.py [--all]     (--all also scans non-exact functions -- offsets can
                                        desync there, so treat those rows as leads only)
Requires the build/*.obj set to be current: run tools/progress.py first.
"""
import os, re, struct, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match as M

EXE = open(os.path.join(ROOT, "loco/Loco.exe"), "rb").read()


def coff_functions_rel(objpath):
    """Like match.coff_functions but relocs are (offset, symname, type)."""
    d = open(objpath, "rb").read()
    nsec = struct.unpack_from("<H", d, 2)[0]
    symoff = struct.unpack_from("<I", d, 8)[0]
    nsym = struct.unpack_from("<I", d, 12)[0]
    opt = struct.unpack_from("<H", d, 16)[0]
    sh = 20 + opt
    strtab = symoff + nsym * 18
    secs = []
    for i in range(nsec):
        off = sh + i * 40
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, flags = struct.unpack_from("<IIIIIIHHI", d, off + 8)
        secs.append((rawptr, rawsz, relptr, nrel))

    def symname_at(idx):
        rec = d[symoff + idx * 18:symoff + idx * 18 + 18]
        if rec[:4] == b"\0\0\0\0":
            o = struct.unpack_from("<I", rec, 4)[0]
            e = d.index(b"\0", strtab + o)
            return d[strtab + o:e].decode("latin1")
        return rec[:8].rstrip(b"\0").decode("latin1")

    funcs = []
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        if typ == 0x20 and secn > 0:
            rawptr, rawsz, relptr, nrel = secs[secn - 1]
            code = d[rawptr:rawptr + rawsz]
            rels = []
            for r in range(nrel):
                ro, rsym, rtyp = struct.unpack_from("<IIH", d, relptr + r * 10)
                rels.append((ro, symname_at(rsym), rtyp))
            funcs.append((symname_at(i), code, rels))
        i += 1 + naux
    return funcs


def main():
    report_all = "--all" in sys.argv
    # target_addr -> {symbol -> [(src, funcaddr)]}
    mapping = collections.defaultdict(lambda: collections.defaultdict(list))
    nexact = 0
    for src in sorted(os.listdir(os.path.join(ROOT, "src"))):
        if not src.endswith(".cpp"):
            continue
        obj = os.path.join(ROOT, "build", src[:-4] + ".obj")
        if not os.path.exists(obj):
            continue
        text = open(os.path.join(ROOT, "src", src)).read()
        funcs = coff_functions_rel(obj)
        # match.pair_by_name wants (name, code, reloc_offsets)
        plain = [(n, c, [r[0] for r in rels]) for n, c, rels in funcs]
        relmap = {n: rels for n, c, rels in funcs}
        for addr, name, code, reloff in M.pair_by_name(text, plain):
            L = M.trim_pad(code)
            foff = (addr - M.TEXT_VA) + M.TEXT_RAW
            orig = EXE[foff:foff + L]
            if len(orig) != L:
                continue
            exact = M.mask(code, reloff, L) == M.mask(orig, reloff, L)
            if exact:
                nexact += 1
            elif not report_all:
                continue
            for ro, rsym, rtyp in relmap[name]:
                if ro < 1 or ro + 4 > L:
                    continue
                if code[ro - 1] != 0xE8:      # direct near call only
                    continue
                if orig[ro - 1] != 0xE8:
                    continue                   # offsets desynced (non-exact fn)
                rel = struct.unpack_from("<i", orig, ro)[0]
                target = addr + ro + 4 + rel
                mapping[target][rsym].append((src, addr, exact))
    print(f"scanned {nexact} exact functions\n")
    conflicts = []
    for target in sorted(mapping):
        syms = mapping[target]
        if len(syms) > 1:
            conflicts.append(target)
    print("=== CONFLICTS: one original callee, >1 distinct mangled symbol in our source ===")
    if not conflicts:
        print("  (none)")
    for target in conflicts:
        print(f"  {target:#08x}:")
        for sym, sites in sorted(mapping[target].items()):
            where = ", ".join(f"{s}@{a:#x}{'' if e else ' (nonexact)'}" for s, a, e in sites[:6])
            print(f"      {sym}   <- {where}")
    # ---- istream::operator>> overload audit -------------------------------------------
    # Ground truth from disassembling each callee (v412):
    #   0x4646c0  strtol  -> stores DWORD           : signed 32-bit   (int& / long&)
    #   0x464f70  strtoul -> stores DWORD           : unsigned 32-bit (unsigned int& / unsigned long&)
    #   0x464bc0  strtol  -> clamp +-0x7fff, WORD   : short&
    #   0x464750  strtoul -> clamp 0xffff, WORD     : unsigned short&
    #   0x4649f0  token loop, BYTE stores           : char*
    #   0x464ef0  single sbumpc, BYTE store         : char&
    ISTREAM = {
        0x4646c0: (set("HJ"), "signed 32-bit (int&/long&)"),
        0x464f70: (set("IK"), "UNSIGNED 32-bit (unsigned int&/unsigned long&)"),
        0x464bc0: (set("F"),  "short&"),
        0x464750: (set("G"),  "unsigned short&"),
        0x464ef0: (set("D"),  "char&"),
    }
    print("\n=== istream::operator>> overload audit ===")
    bad = 0
    for target, (allowed, desc) in sorted(ISTREAM.items()):
        for sym, sites in sorted(mapping.get(target, {}).items()):
            m = re.match(r"\?\?5istream@@QAEAAV0@AA(\w)@Z$", sym)
            letter = m.group(1) if m else None
            ok = letter in allowed if letter else False
            tag = "  ok " if ok else "**WRONG**"
            if not ok:
                bad += 1
            where = ", ".join(f"{s}@{a:#x}" for s, a, e in sites)
            print(f"  {tag} {target:#08x} [{desc}]  ours={sym}")
            print(f"          {len(sites)} site(s): {where}")
    # any operator>> symbol we emit that lands on a target NOT in the table
    print("\n=== operator>>/<< calls to targets outside the audited set ===")
    for target in sorted(mapping):
        if target in ISTREAM:
            continue
        for sym, sites in sorted(mapping[target].items()):
            if "??5" in sym or "??6" in sym:
                where = ", ".join(f"{s}@{a:#x}" for s, a, e in sites)
                print(f"  {target:#08x}  {sym}  ({len(sites)}): {where}")
    print(f"\n{bad} wrong-overload finding(s)")


main()
