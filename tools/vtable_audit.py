#!/usr/bin/env python3
"""Audit every emitted `??_7Class@@6B@` vtable COMDAT against the ORIGINAL image's vtable.

CODEGEN #129: a derived class whose overrides are simply not DECLARED still compiles, still
byte-matches every function in its TU, and passes every lint -- but the vtable COMDAT it emits
silently inherits the BASE's function addresses in those slots.  Nothing else in this repo can
see that: we never link the real image, and a slot is data, not code, so no `// FUNCTION:`
marker covers it.

Method
------
1. Parse each `build/*.obj` (COFF) for sections whose COMDAT symbol is `??_7...@@6B@`, and read
   that section's relocations in offset order -- one per slot, slot order, 1:1.  Only .obj files
   a CURRENT `src/*.cpp` still produces are audited: `build/` is gitignored scratch and keeps
   artifacts from deleted probe TUs and from renamed sources (`build/Obj0x477610.obj` outlived
   the v482 rename to CarKindDesc.cpp by 68 sessions), which otherwise show up as phantom
   classes and inflate the not-located count with dead vtables.
2. Resolve each slot's mangled symbol to an image VA via the `// FUNCTION: LOCO 0x...` marker
   set in `src/`.  Compiler-generated thunks (`??_E`) and declared-only placeholder slots
   (`?_vNN@`) stay unresolved; they are reported as `?` and excluded from scoring.
3. Locate the class's vtable in the image by scanning `.rdata` for the dword array that best
   agrees with the resolved slots (a class named `...0x477a90` also gets that address as a
   hint, but the scan is what decides).
4. Print a per-slot diff.  A slot where the image holds an address we never claimed, while we
   emit some OTHER known function, is the #129 bug: an unclaimed override.

Usage:
    tools/vtable_audit.py                 # audit every class, summary + defects
    tools/vtable_audit.py --all           # also print fully-correct classes' slot tables
    tools/vtable_audit.py AnimEffectObj   # one class / TU (substring match)
"""
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
SRC = os.path.join(ROOT, "src")
EXE = os.path.join(ROOT, "loco", "Loco.exe")

TEXT_LO, TEXT_HI = 0x401000, 0x477000
APP_HI = 0x463800  # above this is statically-linked CRT, not app code
# `push 25 (_RT_PUREVIRT/R6025); call _amsg_exit` -- the CRT's __purecall.  A vtable slot holding
# it is a PURE VIRTUAL (`= 0`) in the original source; anything else there is a real body.
PURECALL = 0x467e90


# --------------------------------------------------------------------------- COFF
def coff_vtables(objpath):
    """-> [(vtable_symbol_name, [reloc_target_symbol_name, ...] in slot order)]"""
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
            e = d.index(b"\0", strtab + o)
            return d[strtab + o:e].decode("latin1")
        return rec.rstrip(b"\0").decode("latin1")

    # symbol index -> name (needed to resolve relocation targets)
    names = {}
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        naux = rec[17]
        names[i] = symname(rec)
        i += 1 + naux

    out = []
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        nm = names[i]
        if nm.startswith("??_7") and nm.endswith("@@6B@") and secn > 0:
            rawptr, rawsz, relptr, nrel = secs[secn - 1]
            slots = {}
            for r in range(nrel):
                voff, symidx, rtyp = struct.unpack_from("<IIH", d, relptr + r * 10)
                slots[voff] = names.get(symidx, "?")
            # section may hold the vtable at a nonzero offset (val); slots are 4 bytes apart
            ordered = [slots[o] for o in sorted(slots) if o >= val]
            out.append((nm, ordered))
        i += 1 + naux
    return out


# --------------------------------------------------------------------------- marker map
MARKER_RX = re.compile(r"^\s*//\s*FUNCTION:\s*LOCO\s+0x([0-9a-fA-F]+)")
DEF_NAME_RX = re.compile(r"([A-Za-z_~][\w:~]*)\s*\(")


def marker_map():
    """-> ({'Class::Method': va, 'FreeFn': va}, {every marker va})"""
    m = {}
    allva = set()
    for fn in sorted(os.listdir(SRC)):
        if not fn.endswith(".cpp"):
            continue
        lines = open(os.path.join(SRC, fn), encoding="latin1").read().splitlines()
        for i, line in enumerate(lines):
            mm = MARKER_RX.match(line)
            if not mm:
                continue
            va = int(mm.group(1), 16)
            allva.add(va)
            for j in range(i + 1, min(i + 12, len(lines))):
                s = lines[j].strip()
                if not s or s.startswith("//"):
                    continue
                d = DEF_NAME_RX.search(s)
                if d:
                    m[d.group(1)] = va
                break
    return m, allva


# --------------------------------------------------------------------------- demangle (partial)
def demangle(sym):
    """MSVC mangled -> 'Class::Method' / 'FreeFn', or None when we can't/shouldn't resolve."""
    if sym.startswith("??_E") or sym.startswith("??_G"):
        return None  # compiler-generated deleting-dtor thunk: no marker of its own
    if sym.startswith("??1"):
        body = sym[3:]
        cls = body.split("@@")[0]
        return "%s::~%s" % (cls, cls)
    if sym.startswith("?"):
        body = sym[1:]
        parts = body.split("@@")[0].split("@")
        if len(parts) >= 2:
            return "%s::%s" % (parts[1], parts[0])
        if len(parts) == 1:
            return parts[0]
    if sym.startswith("_"):
        return sym[1:]
    return None


# --------------------------------------------------------------------------- image
def load_image():
    d = open(EXE, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    sh = pe + 24 + optsz
    secs = []
    for i in range(nsec):
        off = sh + i * 40
        name = d[off:off + 8].rstrip(b"\0").decode("latin1")
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, off + 8)
        secs.append((name, base + va, rawptr, min(rawsz, vsz or rawsz)))
    return d, base, secs


def rdata_dwords(d, secs):
    """-> (start_va, [dword, ...]) over .rdata (where MSVC puts vtables)."""
    for name, va, rawptr, sz in secs:
        if name == ".rdata":
            n = sz // 4
            return va, list(struct.unpack_from("<%dI" % n, d, rawptr))
    raise SystemExit("no .rdata")


def find_vtable(rva, dws, resolved, hint):
    """resolved: {slot_index: va}. -> (best_va, score) scanning every dword-aligned start."""
    if not resolved:
        return (hint, 0) if hint else (None, 0)
    best, best_score = None, 0
    nslots = max(resolved) + 1
    for k in range(0, len(dws) - nslots + 1):
        s = 0
        for idx, va in resolved.items():
            if dws[k + idx] == va:
                s += 1
        if s > best_score:
            best, best_score = rva + k * 4, s
        elif s == best_score and best is not None and hint and rva + k * 4 == hint:
            best = hint
    return best, best_score


# --------------------------------------------------------------------------- main
def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_all = "--all" in sys.argv
    filt = args[0].lower() if args else None

    mm, claimed = marker_map()
    d, base, secs = load_image()
    rva, dws = rdata_dwords(d, secs)

    # build/ is gitignored scratch and accumulates STALE .obj files -- one-off probe TUs and
    # artifacts left behind when a src/ file is RENAMED (build/Obj0x477610.obj survived the
    # v482 rename to CarKindDesc.cpp for 68 sessions).  Auditing those reports classes that no
    # longer exist in the project and inflates the "not located" count with dead vtables, which
    # is exactly the kind of not-true-but-consistent model the audit exists to catch.  Only
    # audit an .obj that a CURRENT src/*.cpp still produces.
    live = {f[:-4] for f in os.listdir(SRC) if f.endswith(".cpp")}
    classes = []
    n_stale = 0
    for fn in sorted(os.listdir(BUILD)):
        if not fn.endswith(".obj"):
            continue
        if fn[:-4] not in live:
            n_stale += 1
            continue
        try:
            vts = coff_vtables(os.path.join(BUILD, fn))
        except Exception as e:
            print("  ! %s: %s" % (fn, e))
            continue
        for sym, slots in vts:
            cls = sym[4:-5].rstrip("@")
            if filt and filt not in cls.lower() and filt not in fn.lower():
                continue
            classes.append((fn, cls, slots))

    # function sizes, for pricing the worklist
    sizes = {}
    fpath = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")
    if os.path.exists(fpath):
        for line in open(fpath, encoding="latin1"):
            p = line.split()
            if len(p) >= 2:
                try:
                    sizes[int(p[0], 16)] = int(p[1])
                except ValueError:
                    pass

    n_ok = n_defect = n_unlocated = n_unlocated_anchored = 0
    defects = []
    unclaimed_slots = []
    located = {}   # cls -> (vt, orig slot list) — needed to tell inherited from overridden
    pending = []
    for fn, cls, slots in sorted(classes, key=lambda x: (x[1])):
        mine = []
        for s in slots:
            nm = demangle(s)
            mine.append(mm.get(nm) if nm else None)
        resolved = {i: v for i, v in enumerate(mine) if v}
        hint = None
        h = re.search(r"0x([0-9a-fA-F]{6})", cls)
        if h:
            hint = int(h.group(1), 16)
        vt, score = find_vtable(rva, dws, resolved, hint)
        if vt is None or score == 0:
            n_unlocated += 1
            # Two very different situations, kept apart because only one is alarming:
            #   0 resolved  -- every slot is a declared-only `?_vNN` placeholder, so there is
            #                  no anchor to scan with AND nothing to check even if we found
            #                  the table.  Expected for TU-local ctor views and for family
            #                  base classes that each derivation stamps with its OWN table.
            #   >0 resolved -- we DO know some slot addresses and still cannot find a table
            #                  that agrees.  That means the class model is wrong.
            if resolved:
                n_unlocated_anchored += 1
            # A 1-2 slot table is almost always a dtor-only class or a TU-local view struct:
            # there is nothing to anchor a scan on, and nothing to check.  Noise unless asked.
            if show_all or len(slots) > 2 or resolved:
                print("  ?? %-40s %-26s  vtable NOT LOCATED (%d slots, %d resolved)%s"
                      % (cls, fn, len(slots), len(resolved),
                         "  <- HAS ANCHORS, MODEL SUSPECT" if resolved else ""))
            continue
        orig = [dws[(vt - rva) // 4 + i] for i in range(len(slots))]
        located[cls] = (vt, orig)
        pending.append((fn, cls, vt, slots, mine, orig))
        bad = [i for i in range(len(slots)) if mine[i] is not None and mine[i] != orig[i]]
        if bad:
            n_defect += 1
            defects.append((cls, fn, vt, slots, mine, orig, bad, len(resolved)))
        else:
            n_ok += 1
            if show_all:
                print("  OK %-46s %-26s vt=0x%06x  %d/%d slots verified"
                      % (cls, fn, vt, len(resolved), len(slots)))

    for cls, fn, vt, slots, mine, orig, bad, nres in defects:
        print("\n== MISMATCH  %s  (%s)  image vtable 0x%06x  [%d/%d slots resolved]"
              % (cls, fn, vt, nres, len(slots)))
        for i in bad:
            un = "UNCLAIMED" if (orig[i] not in claimed and
                                TEXT_LO <= orig[i] < APP_HI) else ""
            print("   slot %2d  image=0x%06x %-10s  ours=0x%06x  %s"
                  % (i, orig[i], un, mine[i], slots[i]))

    # ---- report B: slots where the leaf's IMAGE vtable disagrees with the image vtable of the
    # class we inherit the slot FROM.  That is the #129 signature -- the leaf really overrides,
    # we never declared it, so our COMDAT quietly carries the base's address.  Comparing against
    # the BASE'S OWN image table is what separates a real override from ordinary inheritance of
    # a slot whose body simply isn't transcribed yet (the base's address is unclaimed either
    # way, so "is it claimed?" alone is not a usable test).
    for fn, cls, vt, slots, mine, orig in pending:
        for i in range(len(slots)):
            nm = demangle(slots[i]) or ""
            owner = nm.split("::")[0] if "::" in nm else cls
            if owner == cls or owner not in located:
                continue
            bvt, borig = located[owner]
            if i >= len(borig) or borig[i] == orig[i]:
                continue          # genuinely inherited -- our model is right
            if not (TEXT_LO <= orig[i] < APP_HI):
                continue
            unclaimed_slots.append((cls, fn, vt, i, orig[i], slots[i],
                                    sizes.get(orig[i], 0), orig[i] in claimed))

    # ---- report C: slots the image fills with __purecall.  Those are `= 0` in the original;
    # we declare them as ordinary virtuals, so our COMDAT names a declared-only stub instead.
    pure = []
    for fn, cls, vt, slots, mine, orig in pending:
        for i in range(len(slots)):
            if orig[i] == PURECALL:
                pure.append((cls, fn, vt, i, slots[i]))
    if pure:
        print("\n== PURE VIRTUAL slots (image holds __purecall; we declare an ordinary virtual)")
        seen = set()
        for cls, fn, vt, i, sym in pure:
            if (cls, i) in seen:
                continue
            seen.add((cls, i))
            print("   %-28s slot %2d  vt=0x%06x  we emit %s" % (cls, i, vt, sym))

    if unclaimed_slots:
        print("\n== MISSING OVERRIDE DECLARATIONS (leaf's image slot != base's image slot)")
        seen = set()
        for cls, fn, vt, i, va, sym, size, isclaimed in sorted(
                unclaimed_slots, key=lambda r: -r[6]):
            if (cls, i, va) in seen:
                continue
            seen.add((cls, i, va))
            print("   0x%06x %5s B  %s  %-28s slot %2d  overrides %s"
                  % (va, size if size else "?",
                     "transcribed" if isclaimed else "UNCLAIMED  ", cls, i, sym))

    print("\n%d classes: %d vtables fully agree, %d MISMATCH, %d not located; "
          "%d unclaimed slot targets"
          % (n_ok + n_defect + n_unlocated, n_ok, n_defect, n_unlocated,
             len(unclaimed_slots)))
    if n_stale:
        print("   (%d stale build/*.obj with no current src/*.cpp skipped)" % n_stale)
    if n_unlocated:
        print("   (of the %d not located, %d have NO resolved slot -- declared-only `_vNN`"
              " placeholders, nothing to anchor or check; %d HAVE anchors and are real"
              " model defects)" % (n_unlocated, n_unlocated - n_unlocated_anchored,
                                   n_unlocated_anchored))


if __name__ == "__main__":
    main()
