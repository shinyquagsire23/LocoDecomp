#!/usr/bin/env python3
"""lint_unlinked.py -- find UNLINKED CALLS, keyed by ADDRESS rather than by name.

THE DEFECT
----------
One original function is transcribed in some TU under name N, while another TU
declares the SAME ADDRESS under a different name M and calls M.  The emitted call
targets a symbol that no TU defines.  Nothing in the normal loop can see it:

  * verify.py / match.py MASK relocations, so the wrong call is byte-identical to
    the right one and both sides still MATCH;
  * the match build never links, so the undefined symbol never surfaces;
  * lint_ghidra_sync.py accepts both a Ghidra FQN and its "::"->"_" flattening, so
    two spellings can each agree with Ghidra while disagreeing with each other;
  * lint_alias.py and lint_desync.py compare NAMES.  That is the blind spot this
    tool exists for: the dominant producer of this defect is the TU-local METHODS-
    ONLY VIEW STRUCT, where the method name is IDENTICAL in both TUs and only the
    enclosing class differs -- which is exactly what changes the mangled symbol.
    v577 found five such defects in one session (UIResources::TickStationClock-
    ChimeMaybe, WorldBoardPartial::Ddraw_RecenterViewportOffsetMaybe, DecorObjMgr-
    Maybe::ActivateEligibleEntriesMaybe, CarNetObj::ResolveBothAnchorsToPointMaybe,
    LocoBitmap::CreateAndFill); lint_alias.py reported 0 the whole time.

Until v577 the only oracle was the PORT's runtime stub log, which only sees a defect
once that code path actually executes.  This tool finds them statically.

THE ORACLE
----------
The ORIGINAL IMAGE, not any name.  For a function whose marker address is A, a
CALL at byte offset k inside our compiled COMDAT sits at A+k-1 in the original, and
the original's own rel32 displacement there names the TRUE callee address T.  So:

    call to an UNDEFINED symbol at offset k, and the original's target T
    already carries a `// FUNCTION: LOCO 0xT` marker in src/
        =>  a live unlinked call, NOT an untranscribed function.

Because it reads the target out of the binary, it is immune to every naming
question -- it catches a callee renamed beyond recognition (v576's
BuildToolButton_ResetAndCloseToolMenu vs the member ResetAndCloseToolMenuMaybe,
which no name-keyed matcher can pair) exactly as easily as a view-struct twin.

It needs no Ghidra: just build/*.obj (run tools/progress.py first) and loco/Loco.exe.

SAFETY
------
Offset k only maps to A+k-1 while our bytes agree with the original's up to that
point.  That is GUARANTEED inside a byte-EXACT function and merely likely otherwise,
so findings come in two tiers and the distinction is load-bearing:

  CONFIRMED   -- the containing function is byte-exact (relocation-masked).  The
                 offset is exact, so the decoded target is the real callee.
  UNCONFIRMED -- the containing function is not exact.  A 0xE8 happened to sit at
                 the same offset in both images, but our code may have drifted, so
                 the two 0xE8s need not be the same call.  Real defects do show up
                 here -- most non-exact functions are only a few instructions off --
                 but so do coincidences.  The tell is a decoded target that makes no
                 sense for the symbol: the first run of this tool "resolved"
                 `??2@YAPAXI@Z` (operator new, a CRT symbol at 0x465ce0) to
                 0x44be50, a constructor.  VERIFY EACH ONE against the disassembly
                 before acting on it.

Both tiers additionally require 0xE8 at k-1 in OUR bytes and at A+k-1 in the
original's; sites failing that are SKIPPED and never reported.

Only symbols the port actually has to stub are considered (link/gen_syms_port.txt
when it exists), which keeps the CRT and the import libraries out of the report --
they are undefined in build/*.obj but are supplied by real libraries at link time.

Exit code is always 0 (a burn-down tracker like lint_names.py/lint_alias.py) unless
--strict is passed, which exits 1 when any DEFECT is found.

  tools/lint_unlinked.py            per-defect report
  tools/lint_unlinked.py --stubs    also list untranscribed callees WITH ADDRESSES
                                    (gen_stubs cannot derive these from the mangled
                                    name; this is the worklist of what to transcribe)
  tools/lint_unlinked.py --strict   exit 1 on any defect
"""
import glob
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match  # noqa: E402  -- reuse coff_functions/pair_by_name and the .text mapping

ROOT = match.ROOT
MARKER = re.compile(r'//\s*FUNCTION:\s*LOCO\s+0x(4[0-9a-f]{5})')
REL_REL32 = 20  # IMAGE_REL_I386_REL32


def coff_detail(objpath):
    """(funcs, defined, undefined) for one .obj.

    funcs: [(name, code, reloc_offsets, [(offset, symname, type)])] -- the first three
    fields are exactly match.coff_functions' tuple so match.pair_by_name accepts it.
    """
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
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, flags = struct.unpack_from(
            "<IIIIIIHHI", d, off + 8)
        secs.append((rawptr, rawsz, relptr, nrel))

    def symname(rec):
        if rec[:4] == b"\0\0\0\0":
            o = struct.unpack_from("<I", rec, 4)[0]
            return d[strtab + o:d.index(b"\0", strtab + o)].decode("latin1")
        return rec.rstrip(b"\0").decode("latin1")

    # Pass 1: the whole symbol table, indexed as the relocations index it.
    syms, defined, undefined = [], set(), set()
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        nm = symname(rec)
        syms.append((nm, secn, typ))
        for _ in range(naux):
            syms.append((None, 0, 0))
        if secn > 0:
            defined.add(nm)
        elif secn == 0 and val == 0 and nm:
            undefined.add(nm)
        i += 1 + naux

    funcs = []
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        if typ == 0x20 and secn > 0:
            rawptr, rawsz, relptr, nrel = secs[secn - 1]
            code = d[rawptr:rawptr + rawsz]
            offs, detail = [], []
            for r in range(nrel):
                ro, rsym, rtyp = struct.unpack_from("<IIH", d, relptr + r * 10)
                offs.append(ro)
                nm = syms[rsym][0] if rsym < len(syms) else None
                detail.append((ro, nm, rtyp))
            funcs.append((symname(rec), code, offs, detail))
        i += 1 + naux
    return funcs, defined, undefined


def load_markers():
    """0xaddr -> (path, definition line) for every `// FUNCTION: LOCO` marker in src/."""
    out = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
        for i, line in enumerate(lines):
            m = MARKER.search(line)
            if not m:
                continue
            # the first following line that looks like a definition, skipping comments
            for j in range(i + 1, min(i + 40, len(lines))):
                s = lines[j].strip()
                if not s or s.startswith("//") or s.startswith("/*") or s.startswith("*"):
                    continue
                if MARKER.search(lines[j]):
                    break
                out.setdefault(int(m.group(1), 16), (os.path.basename(path), s))
                break
    return out


def main():
    want_stubs = "--stubs" in sys.argv
    strict = "--strict" in sys.argv

    objs = sorted(glob.glob(os.path.join(ROOT, "build", "*.obj")))
    if not objs:
        print("lint_unlinked: no build/*.obj -- run tools/progress.py first")
        return 0

    exe = os.path.join(ROOT, "loco", "Loco.exe")
    if not os.path.exists(exe):
        print("lint_unlinked: loco/Loco.exe missing (gitignored) -- cannot read call targets")
        return 0
    image = open(exe, "rb").read()

    def orig_at(va, n):
        off = va - match.TEXT_VA + match.TEXT_RAW
        return image[off:off + n]

    # A symbol is a stub iff SOME obj needs it and NO obj defines it.
    per_obj, all_defined, all_undefined = {}, set(), set()
    for obj in objs:
        funcs, defined, undefined = coff_detail(obj)
        per_obj[obj] = funcs
        all_defined |= defined
        all_undefined |= undefined
    stubs = all_undefined - all_defined
    # Keep the CRT and the import libraries out: they are undefined in build/*.obj but
    # come from real libraries at link time.  gen_syms_port.txt is precisely the set the
    # port has to synthesize, i.e. the set with no real provider anywhere.
    for cand in ("gen_syms_port.txt", "gen_syms.txt"):
        path = os.path.join(ROOT, "link", cand)
        if os.path.exists(path):
            listed = {l.strip() for l in open(path) if l.strip()
                      and not l.startswith("#") and not l.startswith("__imp__")}
            stubs &= listed
            break

    markers = load_markers()
    defects, untranscribed, skipped = [], {}, 0

    for obj in objs:
        src = os.path.join(ROOT, "src", os.path.basename(obj)[:-4] + ".cpp")
        if not os.path.exists(src):
            continue
        text = open(src, encoding="utf-8", errors="replace").read()
        funcs = per_obj[obj]
        three = [(n, c, o) for n, c, o, _d in funcs]
        detail_by_name = {n: dd for n, _c, _o, dd in funcs}
        code_by_name = {n: c for n, c, _o, _d in funcs}

        for addr, name, _code, relocs in match.pair_by_name(text, three):
            code = code_by_name.get(name, b"")
            # Is this COMDAT byte-exact against the original?  Only then is a relocation
            # offset guaranteed to name the same instruction in both images.
            n = match.trim_pad(code)
            orig = orig_at(addr, n)
            exact = (len(orig) == n and
                     match.mask(code, relocs, n) == match.mask(orig, relocs, n))
            for off, sym, rtyp in detail_by_name.get(name, []):
                if rtyp != REL_REL32 or not sym or sym not in stubs:
                    continue
                # CONFIRM the offset really is a call in BOTH images before decoding.
                if off < 1 or off + 4 > len(code) or code[off - 1] != 0xE8:
                    skipped += 1
                    continue
                ob = orig_at(addr + off - 1, 5)
                if len(ob) != 5 or ob[0] != 0xE8:
                    skipped += 1
                    continue
                disp = struct.unpack_from("<i", ob, 1)[0]
                target = addr + off + 4 + disp
                if target in markers:
                    defects.append((not exact, addr + off - 1, name, sym, target,
                                    markers[target]))
                elif exact:
                    untranscribed.setdefault(sym, set()).add(target)

    print("=" * 78)
    print("lint_unlinked: UNLINKED CALLS (an address transcribed under one name, called")
    print("               under another -- the emitted call reaches no definition)")
    print("=" * 78)
    if not defects:
        print("  none")
    tier = None
    for unconf, site, caller, sym, target, (mfile, mdef) in sorted(defects):
        if unconf != tier:
            tier = unconf
            print("\n--- UNCONFIRMED (containing function is not byte-exact; the offset may"
                  "\n    have drifted, so VERIFY against the disassembly before acting) ---"
                  if unconf else
                  "\n--- CONFIRMED (containing function is byte-exact) ---")
        print(f"\n  call at {site:#08x}  in {caller}")
        print(f"    emits    -> {sym}   (NO definition anywhere in build/)")
        print(f"    original -> {target:#08x}, which IS defined in src/{mfile}:")
        print(f"                  {mdef[:100]}")

    if want_stubs:
        print()
        print("=" * 78)
        print("lint_unlinked: UNTRANSCRIBED CALLEES, with the addresses gen_stubs cannot")
        print("               derive from a mangled name (this is the transcription worklist)")
        print("=" * 78)
        for sym in sorted(untranscribed):
            addrs = " ".join(f"{a:#08x}" for a in sorted(untranscribed[sym]))
            print(f"  {addrs}  {sym}")

    print()
    nconf = sum(1 for d in defects if not d[0])
    print(f"  {nconf} CONFIRMED + {len(defects) - nconf} UNCONFIRMED unlinked call site(s); "
          f"{len(untranscribed)} untranscribed callee symbol(s) resolved to an address; "
          f"{skipped} call site(s) skipped (our bytes and the original's disagree there, "
          f"so the offset could not be confirmed)")
    return 1 if (strict and defects) else 0


if __name__ == "__main__":
    sys.exit(main())
