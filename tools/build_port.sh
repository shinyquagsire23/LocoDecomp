#!/usr/bin/env bash
# Build the PORT configuration: every src/ TU plus port/PortMode.cpp, compiled
# with -D LOCO_PORT, linked into build/Loco-port.exe.
#
# The port build is NOT a byte-match product and never will be -- LOCO_PORT
# changes emitted code by design. Its whole point is that with the macro OFF the
# token stream is identical, so tools/progress.py's numbers cannot move; run
# tools/progress.py after touching any #ifdef LOCO_PORT block to prove that.
#
# See port/README.md for what the port actually does.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CL=toolchain/bin/cl
OUTDIR=build/port
# ⚠ NOT /I src. The filesystem is case-insensitive, so `/I src` makes the SDK's
# `#include <ddraw.h>` resolve to this repo's own src/Ddraw.h and every TU that
# touches DirectDraw fails to compile. src/ TUs reach port/PortMode.h via /I port;
# PortMode.cpp reaches src/LocoBitmap.h by relative path.
# ⚠ /D _USER32_ is load-bearing, not boilerplate. It makes WINUSER.H declare the user32 API
# WITHOUT __declspec(dllimport), so a call compiles to `call _GetCursorPos@4` instead of
# `call [__imp__GetCursorPos@4]` -- and the linker then resolves that from port/PortWinShim.cpp,
# which DEFINES a dozen of those entry points to translate between the engine's virtual screen
# and the real one (objects are searched before import libraries, and USER32.LIB carries both
# spellings, so the undecorated one simply comes from us). Drop this and every call site
# silently binds straight to user32 again, with no diagnostic anywhere.
CFLAGS="/nologo /c /MT /W3 /O2 /Gy /GX /D WIN32 /D NDEBUG /D _WINDOWS /D LOCO_PORT /D _USER32_ /I port"

mkdir -p "$OUTDIR"

# ── Compile: ONE cl invocation for every TU ────────────────────────────────────
# Same trick tools/progress.py's compile_all() uses. A wine/CL.EXE cold start costs
# ~0.8s, so 70 serial invocations spent ~55s on process startup alone; cl accepts
# any number of sources and /Fo then names an output DIRECTORY (trailing backslash
# required, Z:-mapped exactly like toolchain/bin/cl's own winp() conversion).
#
# cl does NOT stop at the first bad TU (it exits 2 but keeps compiling the rest), so
# per-file success is judged by the .obj EXISTING afterwards -- stale objs are removed
# first so yesterday's build can never stand in for a TU that failed today.
#
# link/stubs.cpp and link/init_globals.cpp ride along in the same batch (they are port
# scaffolding, not TUs, so they land in link/ under the _port names the link step wants;
# cl can only write one /Fo directory per run, so they are compiled into $OUTDIR and
# moved). Their objs are named explicitly at link time, never globbed.
#
# Only OUT-OF-DATE TUs are handed to cl. The dependency test is deliberately coarse --
# a TU rebuilds if its own .cpp is newer than its .obj, and ANY header edit anywhere
# rebuilds everything -- because this build has no depfile machinery (VC5 predates
# /showIncludes) and a missed rebuild is a debugging catastrophe next to a few wasted
# seconds. LOCO_PORT_REBUILD=1 forces a full rebuild.
srcs=(); dests=()
for f in src/*.cpp port/*.cpp; do
  srcs+=("$f"); dests+=("$OUTDIR/$(basename "$f" .cpp).obj")
done
srcs+=(link/stubs.cpp);        dests+=(link/stubs_port.obj)
srcs+=(link/init_globals.cpp); dests+=(link/init_globals_port.obj)

# Newest input every TU implicitly depends on: any header, plus this script (it owns
# CFLAGS) and the compiler wrapper.
newest_dep=$(ls -t src/*.h port/*.h "$0" "$CL" 2>/dev/null | head -1)
todo=()
for i in "${!srcs[@]}"; do
  d="${dests[$i]}"
  if [[ -n "${LOCO_PORT_REBUILD:-}" || ! -f "$d" || "${srcs[$i]}" -nt "$d" || "$newest_dep" -nt "$d" ]]; then
    rm -f "$d"          # never let a stale obj stand in for a TU that fails today
    todo+=("${srcs[$i]}")
  fi
done

if [[ ${#todo[@]} -gt 0 ]]; then
  # Z:-mapped /Fo directory, mirroring toolchain/bin/cl's winp().
  FO="/FoZ:$(cd "$OUTDIR" && pwd | sed 's#/#\\#g')\\"
  $CL $CFLAGS "$FO" "${todo[@]}" > "$OUTDIR/build.log" 2>&1
  # cl writes every obj into the one /Fo directory; move the scaffolding pair out to the
  # link/ names the link step wants, which also keeps $OUTDIR TU-only for the glob below.
  [[ -f "$OUTDIR/stubs.obj" ]] && mv "$OUTDIR/stubs.obj" link/stubs_port.obj
  [[ -f "$OUTDIR/init_globals.obj" ]] && mv "$OUTDIR/init_globals.obj" link/init_globals_port.obj
fi

fail=0
for i in "${!srcs[@]}"; do
  if [[ ! -f "${dests[$i]}" ]]; then
    echo "COMPILE FAILED: ${srcs[$i]}"
    # cl echoes each source's basename before its diagnostics; print that TU's slice.
    awk -v f="$(basename "${srcs[$i]}")" '
      $0 == f {on=1; next} /^[A-Za-z0-9_]+\.cpp\r?$/ {on=0} on' "$OUTDIR/build.log" | head -12
    fail=1
  fi
done
[[ $fail -eq 0 ]] || { echo "== port build: compile errors above (full log: $OUTDIR/build.log)"; exit 1; }
echo "== port build: ${#srcs[@]} objs clean (${#todo[@]} recompiled)"

# Only real TU objs are in $OUTDIR, so the link list is a plain glob.
objs=("$OUTDIR"/*.obj)

# ── Link with the same scaffolding link_check.sh uses, against the port objs ───
LINK=toolchain/bin/link
CVTRES=toolchain/vc50/BIN/CVTRES.EXE
LIBS="kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib comdlg32.lib ole32.lib version.lib ddraw.lib dsound.lib winmm.lib"
OUT=build/Loco-port.exe

# The .res only depends on loco/Loco.exe, which is never rebuilt -- skip the extract +
# CVTRES round trip (one more wine start) once the obj exists.
[[ -f link/Loco_res.obj ]] ||
  { tools/extract_res.py -o link/Loco.res >/dev/null 2>&1 &&
    WINEDEBUG=-all wine $CVTRES /machine:ix86 /out:link/Loco_res.obj link/Loco.res >/dev/null 2>&1; }
RESOBJ=""; [[ -f link/Loco_res.obj ]] && RESOBJ=link/Loco_res.obj

# ⚠ /debug makes LINK 5.10 link INCREMENTALLY against build/Loco-port.ilk, and an
# incremental link reuses the PREVIOUS symbol RESOLUTION. That is fine while objs only
# change bodies, but it is silently wrong the moment a scaffolding obj starts DEFINING a
# symbol that used to come from a library: link/stubs.cpp gaining its own operator new
# (??2@YAPAXI@Z) relinked "OK" four times in a row while link/Loco-port.map still showed
# LIBCMT:new.obj and the exe still called LIBCMT's. Nothing warns -- the link prints
# nothing at all. So drop the .ilk whenever a scaffolding obj is newer than it, which is
# exactly when a resolution can have moved, and pay for one full link.
for scaffold in link/stubs_port.obj link/init_globals_port.obj link/gen_stubs_port.obj; do
  [[ -f $scaffold && $scaffold -nt build/Loco-port.ilk ]] && rm -f build/Loco-port.ilk
done

link_port() {
  $LINK /nologo /subsystem:windows /base:0x400000 /opt:noref /map:link/Loco-port.map \
    /debug /debugtype:cv /pdb:link/Loco-port.pdb \
    /out:$OUT "${objs[@]}" link/stubs_port.obj link/init_globals_port.obj ${GENOBJ:-} ${RESOBJ:-} \
    link/dplayx.lib link/msvfw32.lib $LIBS 2>&1
}
harvest() { # stdin = link log, stdout = the gen_syms list
  grep -oE 'unresolved external symbol .*' \
    | sed -E -e 's/^unresolved external symbol //' -e 's/^.*\(([^()]*)\)[^()]*$/\1/' -e 's/ .*$//' \
    | tr -d '\r' | sort -u | grep -vxE '__malloc|__free|_ArrayDestructWithIteratorMaybe'
}

# Fast path: the stub set only changes when a symbol is transcribed, added or renamed,
# which is a minority of builds. Try the previous gen_stubs_port.obj first -- if it still
# covers exactly the right symbol set the link succeeds outright and we are done in ONE
# link pass instead of two. Anything else (a newly unresolved symbol, or a duplicate
# because a TU now really defines what used to be stubbed) falls back to the full
# harvest-and-regenerate below, so a stale obj can never survive into the output.
GENOBJ=""
if [[ -f link/gen_stubs_port.obj ]]; then
  GENOBJ=link/gen_stubs_port.obj
  log=$(link_port)
  if ! printf '%s\n' "$log" | grep -qE "error|Internal"; then
    [[ -f $OUT ]] || { echo "PORT LINK FAILED"; exit 1; }
    echo "== PORT LINK OK (cached stubs): $OUT ($(wc -c < $OUT) bytes)"
    exit 0
  fi
  GENOBJ=""
fi

log1=$(link_port)
printf '%s\n' "$log1" | harvest > link/gen_syms_port.txt
cat link/gen_syms_extra.txt >> link/gen_syms_port.txt
sort -u link/gen_syms_port.txt -o link/gen_syms_port.txt
python3 link/gen_stubs.py link/gen_syms_port.txt link/gen_stubs_port.obj || exit 1
GENOBJ=link/gen_stubs_port.obj
log2=$(link_port)
printf '%s\n' "$log2" | grep -E "error|Internal" | grep . && exit 1
[[ -f $OUT ]] || { echo "PORT LINK FAILED"; exit 1; }
echo "== PORT LINK OK: $OUT ($(wc -c < $OUT) bytes)"
