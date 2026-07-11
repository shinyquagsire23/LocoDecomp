#!/usr/bin/env python3
"""Verify byte-matching of a compiled module against Loco.exe.

Unlike match.py's positional best-fit, this pairs each `// FUNCTION: LOCO 0xADDR`
marker to the COMDAT for the SAME source function (match.pair_by_name), which
best-fit mis-assigns when two COMDATs become byte-identical after relocation
masking (e.g. two tiny `mov eax,imm; ret` stubs). It also drops compiler-generated
COMDATs that carry no `// FUNCTION` marker at all — dynamic-initializer thunks for
file-scope objects with constructors (MSVC's `_$E<n>`/`$E<n>` symbols) — which
would otherwise desync the positional pairing in match.py.

Ported from Yodecomp/tools/verify.py. Yoda's version additionally filtered out
MFC base-class library COMDATs (CObject/CWnd/... methods pulled from NAFXCW.LIB);
Loco has **no MFC** (CLAUDE.md: plain Win32, no NAFXCW import), so that filter
(LIB_OWNERS / owner_of) does not apply here and has been dropped rather than
carried over dead.

Usage:
    # 1. compile the module's .obj (VC5 flags are a Phase-0 HYPOTHESIS — confirm/replace):
    toolchain/bin/cl /nologo /c /MT /W3 /O2 /Gy /D WIN32 /D NDEBUG /D _WINDOWS \\
        src/Foo.cpp     # run from src/ ; produces build/Foo.obj (see tools/cc.sh)
    # 2. verify:
    python3 tools/verify.py src/Foo.cpp
Add -v to dump the first differing offsets (orig vs ours byte) for each non-exact function.
"""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match  # reuse coff_functions / trim_pad / mask / pair_by_name
import lint_idiom  # non-idiomatic-C++ gate (see _lint below)

TEXT_VA, TEXT_RAW = 0x401000, 0x400
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _lint(src, cap=5):
    """Lint the TU + the local headers it #includes for non-idiomatic C++
    (raw offset access = an unmodeled struct field). Ported from
    another project's _lint; flat-src/ adaptation: the TU's header
    set is its `#include "..."` list, not a per-TU directory glob. Returns the
    number of UNTAGGED findings (they fail the run)."""
    import pathlib
    p = pathlib.Path(src)
    files = [p]
    for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', p.read_text(), re.M):
        h = (p.parent / inc).resolve()
        if h.exists() and h not in files:
            files.append(h)
    rows = []  # (path, lineno, classes, raw, tagged)
    for f in files:
        rows += [(f,) + r for r in lint_idiom.scan_file(f)]
    dup_findings, _ = lint_idiom.collect_dups()  # class E needs the repo-wide view
    for f, drows in dup_findings.items():
        if f.resolve() in [x.resolve() for x in files]:
            rows += [(f, lineno, ["E"], raw, tagged)
                     for lineno, raw, tagged in drows]
    if not rows:
        return 0
    untagged = [r for r in rows if not r[4]]
    print(f"  ⚠ NON-IDIOMATIC C++: {len(rows)} findings "
          f"({len(untagged)} untagged) — raw `*(T*)((char*)p + 0xNN)` = an"
          " unmodeled struct field (model it; codegen is identical); [E] ="
          " duplicate class definition (extend the canonical model, don't"
          " re-declare a local copy — sizeof-drift hazard); [F] = raw"
          " vtable-slot call `(*(VFn**)p)[N](p)` = an unmodeled virtual method.")
    shown = (untagged + [r for r in rows if r[4]])[:cap]
    for f, lineno, classes, raw, tagged in shown:
        mark = "known" if tagged else "NEW"
        rel = os.path.relpath(f, ROOT)
        print(f"      {rel}:{lineno}: [{','.join(classes)},{mark}] {raw.strip()[:90]}")
    if len(rows) > cap:
        print(f"      … and {len(rows) - cap} more:"
              f" tools/lint_idiom.py {os.path.relpath(src, ROOT)}")
    if untagged:
        print("      ➜ untagged findings FAIL the run: model the struct field"
              " now, or tag known debt `// TODO: idiom` /"
              " a reviewed deliberate keep `// idiom-exempt`.")
    return len(untagged)


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    src = sys.argv[1]
    verbose = "-v" in sys.argv
    exe = os.path.join(ROOT, "loco/Loco.exe")
    obj = os.path.join(ROOT, "build", os.path.splitext(os.path.basename(src))[0] + ".obj")
    if not os.path.exists(obj):
        print("obj not found — compile first (see this file's header):", obj); return 2

    text = open(src).read()
    exe_bytes = open(exe, "rb").read()
    # drop CRT dynamic-init thunks (_$E123 etc. from a file-scope object with a ctor/dtor): they
    # byte-match but carry no // FUNCTION marker, so best-fit/pair_by_name would mis-pair them.
    funcs = [f for f in match.coff_functions(obj)
             if not f[0].lstrip("?").startswith(("_$E", "$E"))]
    paired = match.pair_by_name(text, funcs)

    ok = exact_bytes = 0
    rows = []
    for va, name, code, relocs in paired:
        L = match.trim_pad(code)
        orig = exe_bytes[(va - TEXT_VA) + TEXT_RAW:][:L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        diffs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
        rows.append((va, name, L, diffs, orig, code))
    for va, name, L, diffs, orig, code in rows:
        status = "MATCH" if not diffs else f"DIFF({len(diffs)})"
        print(f"  {va:#08x}  {status:>9}  len={L:<4} {name}")
        ok += not diffs
        exact_bytes += L if not diffs else 0
        if verbose and diffs:
            for o in diffs[:8]:
                print(f"       @{o:#05x} orig={orig[o]:02x} ours={code[o]:02x}")
    print(f"\n{ok}/{len(rows)} exact   ({exact_bytes} bytes byte-identical)")
    print("  (non-exact = effective matches: reg-alloc / instruction-selection tie-breaks; see the")
    print("   in-source // EFFECTIVE MATCH annotations. Disasm cross-check: Ghidra @addr vs the .obj.)")
    untagged = _lint(src)
    return 0 if (ok == len(rows) and not untagged) else 1


if __name__ == "__main__":
    sys.exit(main())
