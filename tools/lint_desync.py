#!/usr/bin/env python3
"""lint_desync.py -- find BYTE-INVISIBLE SYMBOL DESYNCS between translation units.

The defect class (found and mined in v554): one image function or global is
referenced by two or more TUs under DIFFERENT MANGLED SYMBOLS, because the TUs
disagree about its LINKAGE (`extern "C"` vs C++), its CALLING CONVENTION
(`__cdecl` vs `__stdcall`), or one of its PARAMETER/POINTER TYPES. One address
cannot have two symbols, so at least one of those TUs is emitting a call or a
reference that nothing in the project defines.

Why nothing else here catches it:
  * verify.py / match.py MASK relocations, so the wrong call is byte-identical
    to the right one -- both sides still MATCH;
  * we never link, so the undefined symbol never surfaces (link_check.sh is
    lenient about undefined externs by design -- most callees are untranscribed);
  * lint_alias.py compares NAMES, and every spelling here shares the SAME name.
    It is blind to a pure linkage/convention/type disagreement;
  * lint_ghidra_sync.py checks each declaration against Ghidra independently,
    and each one can individually agree;
  * the compiler is happy: every declaration is well-formed in its own TU.

The oracle is the object files themselves. Group every symbol in build/*.obj by
the underlying source identifier its mangling encodes; any identifier carrying
more than one distinct symbol is a disagreement. A group is REPORTED only when
at least one of its spellings is never DEFINED anywhere -- that spelling is the
one that goes nowhere.

A SECOND defect class, found in v563 and structurally invisible to the grouping
above: one METHOD reached through two different CLASS spellings. The identifier a
C++ method mangles to carries its scope, so `?Load@ViewStruct@@...` and
`?Load@RealClass@@...` reduce to DIFFERENT identifiers ("ViewStruct::Load" vs
"RealClass::Load") and never land in the same group -- even though exactly one
address is meant, and the view's spelling is defined nowhere.

That is the TU-LOCAL VIEW STRUCT pattern, and it is the project's most common
way to reach a class whose real definition is not in a header. It is correct
while the callee is untranscribed and becomes a live defect the moment a real
definition lands elsewhere: nothing links, so nothing complains, and in the PORT
build link/gen_stubs.py quietly supplies the view's spelling as
`xor eax,eax; ret N`. Every call into a fully transcribed body then returns 0.
In v563 that was nine methods of one singleton, and the zero came back as "the
world failed to load" -- see docs/CODEGEN.md #184.

The rule used here deliberately does NOT flag every method name defined under one
class and undefined under another (classes legitimately share method names like
Init or MarkDirty). It fires only when the undefined spelling's class is already
KNOWN TO BE A VIEW: some global in the project is declared with two or more
different class types, and that class is one of them. Those classes are, by
construction, alternative models of ONE object.

Severity ordering, most actionable first:
  DEFINED   -- some obj defines one spelling, so that spelling is GROUND TRUTH
               and every other one is provably wrong. Fix the dissenters.
  VIEW      -- a method undefined under a known view-struct class whose bare name
               IS defined under another class. Same ground truth, reached across
               a scope rather than a signature.
  INHERIT   -- an undefined VIRTUAL whose class defines nothing at all anywhere,
               while the identical SIGNATURE is defined under some real class.
               That is a class re-declaring a virtual it only inherits: the
               declaration hides the base member, every qualified call mangles
               under the wrong class, and the port gets a do-nothing stub.
               Closes the third known blind spot in the VIEW test, which can only
               see classes it can prove model one object (some global declared
               under >= 2 class types) and is therefore blind to a class that
               appears only as a BASE. Found by fixing
               AnimDescRefHotspotPartial::RepositionWithHotspot, where ten calls
               that should have reached 0x405c00 positioned nothing.
  MAJORITY  -- no definition exists yet (untranscribed callee), so the majority
               spelling is a strong convention but not proof. Still a real
               disagreement: one address, one true type.

Not every finding is free to fix -- a linkage change can move codegen through
the documented declaration dial (v554: fixing DDraw_CreateSurfaceFromFile in
src/LocoBitmap.cpp costs LocoBitmap::Fill its 124 B exact). ALWAYS measure with
a full tools/progress.py per-file table diff, bisect one declaration at a time
when a batch regresses, and park what you cannot afford with a `// TODO: idiom`
note carrying the measurement.

INFORMATIONAL: exits 0 always (like lint_names.py / lint_alias.py), so it is not
wired into verify.py/cc.sh. `--strict` exits 1 when any DEFINED, VIEW or INHERIT
finding survives. A NAME argument filters to one identifier.

WHICH BUILD YOU ARE MEASURING MATTERS, and for INHERIT it is the whole question
(v569). The default oracle is build/*.obj -- the MATCH build -- where a whole
class of these declarations is body-less ON PURPOSE: the original keeps an
out-of-line call at the site, and a visible body is exactly what would let /O2
inline it away. `EffectCollectionCtorViewMaybe::ReserveMaybe` is the type case.
Those rows are correct findings about the match build and they can NEVER be
retired there; what gets fixed is the PORT build, with a `#ifdef LOCO_PORT`
forwarder. So a fixed row does not disappear from the default run, and three
sessions in a row can "discover" the same already-fixed defect.

Pass `--objdir build/port` (run tools/build_port.sh first) to ask the question
that actually has an answer: does the LINKED build still have a hole here? A row
present under build/ but absent under build/port is already forwarded.

⚠ But do NOT read a clean `--objdir build/port` run as "the port has no holes".
The INHERIT test fires only on a class that defines NOTHING AT ALL anywhere, so
forwarding ONE method of a class makes it go blind to that class's REMAINING
stubs. Measured v569: build/port reports 0 INHERIT while
`PlacedObjCollectionMaybe` still has four undefined slots (3/9/10/13), invisible
only because its other slots are forwarded in the same block. The AUTHORITATIVE
list of what the port still stubs is link/gen_syms_port.txt -- that is generated
from the link itself and has no heuristic in it. Use this flag to confirm a fix
landed, never to certify a class clean.

Usage:
    tools/lint_desync.py                 # summary table (MATCH build)
    tools/lint_desync.py --objdir build/port   # what the PORT link still lacks
    tools/lint_desync.py --list          # every finding, full detail
    tools/lint_desync.py Ddraw_HResultToString
    tools/lint_desync.py --strict
"""
import collections
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDIR = os.path.join(ROOT, "build")

# llvm-nm reads this toolchain's COFF fine; GNU nm trips on the weak-external
# storage class VC5 emits for a declared-but-empty dtor (see CLAUDE.md).
NM_CANDIDATES = [
    "/opt/homebrew/opt/llvm/bin/llvm-nm",
    "/usr/local/opt/llvm/bin/llvm-nm",
    "llvm-nm",
]

DEFINED_CLASSES = set("TtDdBbRr")


def find_nm():
    for c in NM_CANDIDATES:
        try:
            subprocess.run([c, "--version"], capture_output=True)
            return c
        except (OSError, subprocess.SubprocessError):
            continue
    return None


def base_name(sym):
    """Reduce a mangled symbol to the source identifier it encodes.

    MSVC C++:  ?name@Scope@@<types>   -> "Scope::name"
    MSVC C:    _name  or  _name@N     -> "name"   (the @N is stdcall's arg-byte count)
    """
    if sym.startswith("??"):
        return None                      # operators, ctors/dtors, vftables: not our target
    if sym.startswith("?"):
        head = sym[1:].split("@@")[0]
        parts = [p for p in head.split("@") if p]
        if not parts:
            return None
        return "::".join(reversed(parts))  # mangling lists scopes innermost-first
    m = re.match(r"^_(.+?)(@\d+)?$", sym)
    if m:
        return m.group(1)
    return None


# ?g_name@@3UClass@@A / ?g_name@@3VClass@@A -- the declared TYPE of a global.
RE_DATA_TYPE = re.compile(r"^\?[^@]+@@3[UV]([^@]+)@@A$")
# ?Method@Class@@Q... -- an ordinary (non-virtual-table, non-operator) member.
RE_METHOD = re.compile(r"^\?([^@]+)@([^@]+)@@[A-Z]")


def view_class_names(table):
    """Classes that are a VIEW of some object: the project declares one global
    under two or more different class types, so those types model one object."""
    views = set()
    for base, spellings in table.items():
        types = set()
        for sym in spellings:
            m = RE_DATA_TYPE.match(sym)
            if m:
                types.add(m.group(1))
        if len(types) >= 2:
            views |= types
    return views


def view_findings(table, views):
    """-> [(bare_method, undefined_sym, class, defined_sym, defined_objs, ref_objs)]"""
    defined = {}   # bare method name -> [(sym, class, objs)]
    undef = []     # (bare, sym, class, ref_objs)
    for spellings in table.values():
        for sym, rec in spellings.items():
            m = RE_METHOD.match(sym)
            if not m:
                continue
            bare, cls = m.group(1), m.group(2)
            if rec["def"]:
                defined.setdefault(bare, []).append((sym, cls, sorted(set(rec["def"]))))
            elif cls in views:
                undef.append((bare, sym, cls, sorted(set(rec["ref"]))))
    out = []
    for bare, sym, cls, refs in sorted(undef):
        for dsym, dcls, dobjs in defined.get(bare, []):
            if dcls != cls:
                out.append((bare, sym, cls, dsym, dobjs, refs))
    return out


# ?Method@Class@@<access><rest> where <access> is a VIRTUAL member code
# (U public, E private, M protected). The <rest> is the full signature mangling.
RE_VMETHOD = re.compile(r"^\?([^@]+)@([^@]+)@@([UEM].*)$")


def inherit_findings(table):
    """Undefined VIRTUAL methods that are really the base class's, spelled under a derived
    class -> [(bare, undefined_sym, class, defined_sym, defined_objs, ref_objs)].

    The blind spot this closes: view_findings() only considers classes it can PROVE model one
    object, which it does by finding one global declared under two class types. A class that
    only ever appears as a BASE in an inheritance chain -- never as a global's type -- is
    invisible to that test, so a redundant re-declaration of an inherited virtual on a partial/
    derived class sails through. `AnimDescRefHotspotPartial::RepositionWithHotspot` did exactly
    that: it hid the real `AnimDescRefObj0x477488::RepositionWithHotspot` (0x405c00), every
    qualified call mangled under the deriving class, and in the port all ten of them became a
    do-nothing stub -- effects were spawned and never positioned.

    Two conditions, and BOTH are needed -- the first alone reports 64 findings on this repo and
    every one of the extra 63 is legitimate:

      1. IDENTICAL SIGNATURE, not just an identical bare name. Unrelated classes share method
         names constantly (Init, MarkDirty), so the name alone is far too noisy.
      2. The undefined spelling's class DEFINES NOTHING ANYWHERE in the build. This is what
         separates the defect from the ordinary shape it otherwise looks exactly like: a real
         base class (`DecorActorBase`, `Obj0x477758Base`, `WidgetBaseObj0x4784c8`) whose own
         body for a slot is merely untranscribed while derived classes override it. Those
         classes all define plenty of OTHER methods, so they are real, modeled classes and
         their declared-only slot is ordinary backlog. A class that defines nothing at all is a
         pure re-declaration shell, and a virtual it names belongs to somebody else."""
    defined, undef, definers = {}, [], set()
    for spellings in table.values():
        for sym, rec in spellings.items():
            m = RE_METHOD.match(sym)
            if m and rec["def"]:
                definers.add(m.group(2))
            m = RE_VMETHOD.match(sym)
            if not m:
                continue
            key = (m.group(1), m.group(3))       # (bare name, full signature)
            if rec["def"]:
                defined.setdefault(key, []).append((sym, m.group(2), sorted(set(rec["def"]))))
            else:
                undef.append((key, sym, m.group(2), sorted(set(rec["ref"]))))
    out = []
    for key, sym, cls, refs in sorted(undef):
        if cls in definers:
            continue                             # a real class with a declared-only slot
        # One row per UNDEFINED symbol, not per defined twin: a slot the whole family
        # overrides otherwise reports the same defect a dozen times over.
        twins = [(d, o) for d, c, o in defined.get(key, []) if c != cls]
        if twins:
            out.append((key[0], sym, cls, twins[0][0], twins[0][1], refs, len(twins) - 1))
    return out


def collect(nm):
    """-> {base: {symbol: {"def": [objs], "ref": [objs]}}}"""
    table = collections.defaultdict(
        lambda: collections.defaultdict(lambda: {"def": [], "ref": []}))
    objs = sorted(glob.glob(os.path.join(OBJDIR, "*.obj")))
    for obj in objs:
        out = subprocess.run([nm, obj], capture_output=True, text=True).stdout
        name = os.path.basename(obj)
        for line in out.splitlines():
            parts = line.split()
            if not parts:
                continue
            if parts[0] == "U" and len(parts) >= 2:
                sym, kind = parts[1], "ref"
            elif len(parts) >= 3 and parts[1] in DEFINED_CLASSES:
                sym, kind = parts[2], "def"
            else:
                continue
            b = base_name(sym)
            if b:
                table[b][sym][kind].append(name)
    return table, len(objs)


def findings(table):
    out = []
    for base in sorted(table):
        spellings = table[base]
        if len(spellings) < 2:
            continue
        undefined = [s for s in spellings if not spellings[s]["def"]]
        if not undefined:
            continue                     # every spelling is defined: not our defect
        defined = [s for s in spellings if spellings[s]["def"]]
        kind = "DEFINED" if defined else "MAJORITY"
        out.append((kind, base, spellings))
    out.sort(key=lambda f: (f[0] != "DEFINED", f[1]))
    return out


def refcount(spellings, sym):
    return len(spellings[sym]["ref"])


def show(kind, base, spellings, verbose):
    print("%-8s %s" % (kind, base))
    ordered = sorted(spellings, key=lambda s: (not spellings[s]["def"], -refcount(spellings, s)))
    for sym in ordered:
        d, r = spellings[sym]["def"], spellings[sym]["ref"]
        tag = "DEFINED in " + ", ".join(sorted(set(d))) if d else "undefined"
        print("    %s" % sym)
        print("        %s%s" % (tag, "" if d else "  <-- goes nowhere"))
        if r:
            objs = sorted(set(r))
            shown = objs if verbose else objs[:6]
            more = "" if len(shown) == len(objs) else ", +%d more" % (len(objs) - len(shown))
            print("        referenced by %d TU(s): %s%s" % (len(objs), ", ".join(shown), more))
    print()


def main():
    global OBJDIR
    args = [a for a in sys.argv[1:]]
    strict = "--strict" in args
    verbose = "--list" in args
    for f in ("--strict", "--list"):
        if f in args:
            args.remove(f)
    if "--objdir" in args:
        i = args.index("--objdir")
        if i + 1 >= len(args):
            print("lint_desync: --objdir needs a directory")
            return 2
        OBJDIR = os.path.join(ROOT, args[i + 1])
        del args[i:i + 2]
    only = args[0] if args else None

    nm = find_nm()
    if nm is None:
        print("lint_desync: no llvm-nm found; skipping (informational lint)")
        return 0
    if not os.path.isdir(OBJDIR) or not glob.glob(os.path.join(OBJDIR, "*.obj")):
        print("lint_desync: no *.obj in %s -- run tools/progress.py (or tools/build_port.sh) first; skipping" % OBJDIR)
        return 0

    table, nobj = collect(nm)
    found = findings(table)
    views = view_findings(table, view_class_names(table))
    seen = set((v[1] for v in views))            # don't report a symbol under both classes
    inherits = [f for f in inherit_findings(table) if f[1] not in seen]
    if only:
        found = [f for f in found if f[1] == only or only in f[1]]
        views = [v for v in views if only in v[0] or only in v[2]]
        inherits = [v for v in inherits if only in v[0] or only in v[2]]
        if not found and not views and not inherits:
            print("lint_desync: no desync for '%s'" % only)
            return 0

    for kind, base, spellings in found:
        show(kind, base, spellings, verbose or bool(only))

    for bare, sym, cls, dsym, dobjs, refs in views:
        print("%-8s %s" % ("VIEW", bare))
        print("    %s" % sym)
        print("        undefined  <-- goes nowhere (view struct '%s')" % cls)
        if refs:
            shown = refs if (verbose or only) else refs[:6]
            more = "" if len(shown) == len(refs) else ", +%d more" % (len(refs) - len(shown))
            print("        referenced by %d TU(s): %s%s" % (len(refs), ", ".join(shown), more))
        print("    %s" % dsym)
        print("        DEFINED in %s  <-- the real body" % ", ".join(dobjs))
        print()

    for bare, sym, cls, dsym, dobjs, refs, nmore in inherits:
        print("%-8s %s" % ("INHERIT", bare))
        print("    %s" % sym)
        print("        undefined  <-- goes nowhere ('%s' re-declares an inherited virtual)" % cls)
        if refs:
            shown = refs if (verbose or only) else refs[:6]
            more = "" if len(shown) == len(refs) else ", +%d more" % (len(refs) - len(shown))
            print("        referenced by %d TU(s): %s%s" % (len(refs), ", ".join(shown), more))
        print("    %s" % dsym)
        print("        DEFINED in %s  <-- same signature under a real class%s"
              % (", ".join(dobjs), "" if not nmore else " (+%d other override(s))" % nmore))
        print()

    ndef = sum(1 for k, _, _ in found if k == "DEFINED")
    print("%d obj(s) scanned: %d desynced identifier(s) -- %d with a DEFINED spelling "
          "(provably wrong dissenters), %d convention-only; %d view-struct method(s) "
          "reaching a defined body under another class; %d inherited-virtual re-declaration(s)"
          % (nobj, len(found), ndef, len(found) - ndef, len(views), len(inherits)))
    if not verbose and not only and (found or views or inherits):
        print("  (--list for every referencing TU)")
    return 1 if (strict and (ndef or views or inherits)) else 0


if __name__ == "__main__":
    sys.exit(main())
