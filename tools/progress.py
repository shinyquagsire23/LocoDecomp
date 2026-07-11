#!/usr/bin/env python3
"""Completion dashboard: how much of Loco.exe's app region is byte-matched.

Compiles every src/**/*.cpp with the VC++ 5.0 toolchain, byte-matches each
annotated function against the original (relocations masked), and reports two
tiers against the total app-function bytes:

  EXACT    byte-identical (reloc-masked) — done.
  PARTIAL  transcribed (has a // FUNCTION marker + a compiled COMDAT) but not
           byte-exact — the C++ is written; only byte-matching polish remains.

Everything else still needs DECOMPILING. Functions carried by more than one TU
are deduped by address, an exact copy in any TU winning. Partial byte counts use
the compiled COMDAT's trimmed length as a proxy for the original function size
(close, but can drift a few bytes on length-shifted bodies — dashboard precision
only).

Ported from Yodecomp/tools/progress.py. Two things Yoda's version had that this
one drops: the `/D _MBCS` MFC auto-detection in compile_obj (Loco has no MFC —
plain Win32, see CLAUDE.md) and the verify.LIB_OWNERS COMDAT filter (same reason,
see tools/verify.py's docstring); the `_$E`/`$E` CRT dynamic-init-thunk filter
stays, since that's a generic MSVC C++ static-init artifact, not MFC-specific.

TOTAL_APP_BYTES / TOTAL_APP_FUNCS are TODO — Loco's app-code address range (the
CRT/library boundary) hasn't been surveyed yet (CLAUDE.md pickup step 5: "identify
the CRT boundary... find the app-code address range"). Until those are set, the
dashboard prints raw exact/partial counts without a percentage.

Usage:  tools/progress.py
"""
import os, sys, glob, subprocess, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match   # reuse coff_functions / trim_pad / mask / pair_by_name

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# App/CRT boundary (pickup step 5, closed 2026-07-18): swept every function's real Ghidra
# address+size (FunctionManager.getFunctions), then found the first statically-linked CRT/
# library symbol Ghidra had already name-matched. `ifstream_DefaultCtor` @ 0x463890 (iostream.lib)
# is the earliest unambiguous library hit -- everything below is real game code (confirmed:
# LocoWinMain @ 0x462e90, ShowFatalErrorMessageBox @ 0x463600, and a small DirectX/comdlg32
# import-thunk cluster @ 0x4637c0-0x4637fc immediately precede it, negligible bytes either way).
# CRT/library runs from there to .text's own end (0x4767ff): strncpy/strchr/strpbrk/strncat/
# strrchr/strstr (string.lib), operator new/delete (new_alloc @ 0x465ce0), __global_unwind2/
# __local_unwind2/__abnormal_termination/__NLG_Notify1 (EH runtime), WinMainCRTStartup
# (0x4689e0), _beginthreadex/_endthreadex, calloc, and a large tail of still-unnamed CRT
# internals. TOTAL_APP_END is a round address just past the import-thunk cluster, not
# byte-exact -- dashboard precision only, per this module's own docstring.
TOTAL_APP_END = 0x463800


CL = os.path.join(ROOT, "toolchain/bin/cl")
# /O2 /Gy /MT /GX — C++ EH confirmed ON (v82, see CLAUDE.md/docs/PARKED.md).
# $LOCO_OPT overrides the optimization level alone, for whole-repo flag experiments
# (v353: "/Ox" vs the locked "/O2" — see docs/PARKED.md's tail-merge writeup).
FLAGS = ("/nologo /c /MT /W3 %s /Gy /GX /D WIN32 /D NDEBUG /D _WINDOWS"
         % os.environ.get("LOCO_OPT", "/O2")).split()
EXE = open(os.path.join(ROOT, "loco/Loco.exe"), "rb").read()


def _true_extents():
    """addr -> COMDAT extent in the image, for every toolchain/test/app_funcs.txt entry.

    app_funcs.txt carries Ghidra's `Body:` span, which stops at the last INSTRUCTION and so
    EXCLUDES the trailing jump table the linker copies as part of the same COMDAT (the trap
    CLAUDE.md documents for `asmscore.py --len`).  Summed, that undercounts the app region by
    7293 bytes across 122 switch-heavy functions -- and it used to be the DENOMINATOR here
    while the numerator was our own compiled COMDAT lengths, which DO include the table.  Two
    different rulers on the two sides of one ratio is what produced the long-standing
    "100.47% transcribed" (v543); it was never rounding.

    The honest ruler, used on BOTH sides now: run to the next function's start, then trim
    trailing padding (0xCC/0x90).  Verified against the 822 already-EXACT functions -- where
    our COMDAT is byte-identical to the image and so is ground truth for the real extent --
    the two agree to within 20 bytes IN TOTAL.  Never shrink below Ghidra's own span.
    """
    path = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")
    if not os.path.exists(path):
        return None
    rows = []
    for ln in open(path):
        p = ln.split()
        if len(p) == 2:
            rows.append((int(p[0], 16), int(p[1])))
    rows.sort()
    ext = {}
    for i, (va, sz) in enumerate(rows):
        nxt = rows[i + 1][0] if i + 1 < len(rows) else va + sz
        end = max(nxt - va, sz)
        off = (va - match.TEXT_VA) + match.TEXT_RAW
        b = EXE[off:off + end]
        n = len(b)
        while n > 0 and b[n - 1] in (0xCC, 0x90):
            n -= 1
        ext[va] = max(n, sz)
    return ext


EXTENTS = _true_extents()
if EXTENTS is None:
    TOTAL_APP_BYTES, TOTAL_APP_FUNCS = None, None
else:
    _app = {va: sz for va, sz in EXTENTS.items() if va < TOTAL_APP_END}
    TOTAL_APP_BYTES, TOTAL_APP_FUNCS = sum(_app.values()), len(_app)


def compile_all(cpps):
    """Compile every TU in ONE wine/CL.EXE invocation (cl accepts multiple
    sources; /Fo then names the output DIRECTORY, trailing backslash required).
    Serial per-TU wine startups cost ~8s each (~5.5 min for 40 TUs); one batch
    is ~15s total. cl continues past a failed TU (exit 2, remaining files still
    compile), so per-file success is judged by the .obj existing afterward —
    stale objs are removed first so a failed TU can't pass on an old build."""
    build = os.path.join(ROOT, "build"); os.makedirs(build, exist_ok=True)
    objs = {}
    for cpp in cpps:
        obj = os.path.join(build, os.path.splitext(os.path.basename(cpp))[0] + ".obj")
        if os.path.exists(obj):
            os.remove(obj)
        objs[cpp] = obj
    # Z:-mapped /Fo dir, mirroring the winp() conversion in toolchain/bin/cl.
    fo = "/FoZ:" + build.replace("/", "\\") + "\\"
    env = dict(os.environ, WINEDEBUG="-all")
    subprocess.run([CL] + FLAGS + [fo] + cpps,
                   cwd=os.path.join(ROOT, "src"), env=env, capture_output=True)
    return {cpp: obj for cpp, obj in objs.items() if os.path.exists(obj)}


def main():
    # addr -> (exact, nbytes): dedupe functions carried by more than one TU
    # (exact beats partial; among equals keep the first seen).
    by_addr = {}
    rows = []
    cpps = sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True))
    built = compile_all(cpps)
    for cpp in cpps:
        obj = built.get(cpp)
        rel = os.path.relpath(cpp, ROOT)
        if not obj:
            rows.append((rel, "COMPILE FAILED", 0, 0)); continue
        text = open(cpp).read()
        # drop CRT dynamic-init thunks (_$E123 etc.): they byte-match but carry no
        # // FUNCTION marker, so best-fit pairing would mis-pair them.
        funcs = [f for f in match.coff_functions(obj)
                 if not f[0].lstrip("?").startswith(("_$E", "$E"))]
        # pair each marker to its SAME-named COMDAT (best-fit mis-assigns reloc-masked-identical
        # stubs — two tiny accessor stubs become byte-identical once their one imm reloc is masked).
        mb = mf = pf = 0
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            diffs = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            exact = diffs == 0 and len(orig) == L
            if exact:
                mb += (EXTENTS or {}).get(va, L); mf += 1
            else:
                pf += 1
            prev = by_addr.get(va)
            if prev is None or (exact and not prev[0]):
                by_addr[va] = (exact, L)
        rows.append((rel, "%d+%d/%d" % (mf, pf, len(funcs)), mb, len(funcs)))

    # Measure the numerator with the SAME ruler as the denominator: the image's own COMDAT
    # extent, not our candidate's compiled length (a PARTIAL candidate is routinely a few
    # bytes longer or shorter than the original, and summing that against a Ghidra-extent
    # denominator is what used to push the total past 100%).  Fall back to our length only
    # for an address app_funcs.txt doesn't list.  Scope to the app region for the same reason
    # the denominator is: a handful of CRT-region markers (3 funcs, 51 B) otherwise count in
    # the numerator alone.
    ext = EXTENTS or {}
    app = {va: v for va, v in by_addr.items() if va < TOTAL_APP_END}
    exact_bytes = sum(ext.get(va, L) for va, (e, L) in app.items() if e)
    exact_funcs = sum(1 for e, L in app.values() if e)
    partial_bytes = sum(ext.get(va, L) for va, (e, L) in app.items() if not e)
    partial_funcs = sum(1 for e, L in app.values() if not e)

    print("=" * 60)
    print(" LocoDecomp completion — app region")
    print("=" * 60)
    print("  %-26s %-12s %8s" % ("", "exact+part", "exact B"))
    for rel, note, mb, nf in rows:
        print("  %-26s %-12s %6d B" % (rel, note, mb))
    print("-" * 60)
    try:
        import lint_idiom
        lrows = [r for v in lint_idiom.collect().values() for r in v]
        untag = sum(1 for r in lrows if not r[3])
        print("  idiom debt %d non-idiomatic findings (%d untagged)  <- tools/lint_idiom.py; drive to 0"
              % (len(lrows), untag))
    except Exception as e:
        print("  idiom debt lint failed (%s)" % e)
    try:
        import lint_names
        print("  %s" % lint_names.dashboard_line(lint_names.collect()))
    except Exception as e:
        print("  naming debt lint failed (%s)" % e)
    print("  EXACT    %6d bytes  (%d funcs)  — byte-matched" % (exact_bytes, exact_funcs))
    print("  PARTIAL  %6d bytes  (%d funcs)  — transcribed, needs byte-matching"
          % (partial_bytes, partial_funcs))
    if TOTAL_APP_BYTES is None or TOTAL_APP_FUNCS is None:
        print("  TOTAL    <unset>  — TODO: run the Ghidra app-region sweep and set")
        print("           TOTAL_APP_BYTES/TOTAL_APP_FUNCS in this file (CLAUDE.md pickup step 5)")
    else:
        e_pct = 100.0 * exact_bytes / TOTAL_APP_BYTES
        p_pct = 100.0 * partial_bytes / TOTAL_APP_BYTES
        t_pct = e_pct + p_pct
        print("  TOTAL    %6d bytes  (%d funcs in app region)" % (TOTAL_APP_BYTES, TOTAL_APP_FUNCS))
        print("  >>> %.2f%% exact + %.2f%% partial = %.2f%% transcribed; "
              "%.2f%% left to decompile <<<" % (e_pct, p_pct, t_pct, 100.0 - t_pct))
    # Ghidra-extent basis (mirrors Yoda's v23 fix): the PARTIAL sum above uses OUR
    # COMDAT lengths (jump tables etc. included), which can overcount against a strict
    # Ghidra-measured denominator. Once toolchain/test/app_funcs.txt exists (a
    # `addr(hex) size(dec)` dump per function, generated via the same run_script_inline
    # recipe Yoda used), measure marker coverage against those TRUE extents.
    ext_path = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")
    if os.path.exists(ext_path):
        table = dict(EXTENTS or {})
        marked = set()
        for cpp in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True):
            for m in re.finditer(r"FUNCTION:\s*LOCO\s+0x0*([0-9a-fA-F]+)",
                                 open(cpp).read()):
                marked.add(int(m.group(1), 16))
        tot = sum(table.values())
        cov = sum(sz for va, sz in table.items() if va in marked)
        un = sorted((sz, va) for va, sz in table.items() if va not in marked)
        print("  --- image-extent basis (%d funcs, %d bytes incl. funclets/tables) ---"
              % (len(table), tot))
        print("  >>> marker coverage %.2f%% (%d bytes); largest unclaimed (whole binary):"
              % (100.0 * cov / tot, cov))
        for sz, va in un[-6:][::-1]:
            print("      %#x  %d bytes" % (va, sz))
        # App-region-only view of the same unclaimed list: past TOTAL_APP_END it's all
        # statically-linked CRT/library internals that will never get a // FUNCTION marker,
        # so they're noise for "what to decompile next" — filter them out here.
        app_table = {va: sz for va, sz in table.items() if va < TOTAL_APP_END}
        app_tot = sum(app_table.values())
        app_cov = sum(sz for va, sz in app_table.items() if va in marked)
        app_un = sorted((sz, va) for va, sz in app_table.items() if va not in marked)
        print("  >>> app-region-only coverage %.2f%% (%d bytes); largest unclaimed APP gaps:"
              % (100.0 * app_cov / app_tot, app_cov))
        for sz, va in app_un[-6:][::-1]:
            print("      %#x  %d bytes" % (va, sz))


if __name__ == "__main__":
    main()
