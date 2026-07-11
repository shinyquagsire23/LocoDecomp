#!/usr/bin/env bash
# First full compile-and-LINK smoke test: link every byte-match TU obj
# (build/<TU>.obj for each src/<TU>.cpp) with the VC++ 5.0 SP3 linker into
# build/Loco-linked.exe, plus smoke scaffolding from link/ (clearly separated
# from the byte-match product), and print a triage summary. Re-runnable.
#
# Usage: tools/link_check.sh [--run]
#   --run   also smoke-run the result under wine (timeout-guarded)
#
# What it does:
#   1. Compiles link/stubs.cpp (hand-written smoke stubs: CRT _malloc/_free
#      renames, the generic array helpers, WinMain entry glue -> LocoWinMain).
#   2. Builds stub import libraries for DPLAYX.dll / MSVFW32.dll (DirectX SDK
#      / VfW libs the VC5 retail LIB dir doesn't ship) from link/*.def.
#   3. Link pass 1 without generated stubs; harvests every LNK2001 into
#      link/gen_syms.txt. If pass 1 dies with LINK's "Internal error" (the
#      VC5 5.10 undecorate crash on unresolved symbols whose signature embeds
#      a pointer-to-member-function type -- first seen on
#      ?ArrayConstructWithIteratorMaybe@@YGPAXPAXII0P8DSoundChannel@@AEXXZ@Z),
#      the pmf-signature undefined symbols are scraped straight out of the
#      objs with llvm-readobj and added to the stub list instead.
#   4. link/gen_stubs.py emits link/gen_stubs.obj defining all harvested
#      symbols (untranscribed TUs, declared-only virtuals, VtblProbe
#      byproducts, cross-TU globals) as aliases of one `ret`/one zero dword.
#   5. Final link (+/map) -> build/Loco-linked.exe; prints triage summary and
#      a section/import comparison against loco/Loco.exe (never touched).
#
# Scaffolding files (all under link/, none are byte-match sources):
#   stubs.cpp  dplayx.def  msvfw32.def  gen_stubs.py  gen_syms_extra.txt
# Generated artifacts (safe to delete): link/stubs.obj link/init_globals.obj link/gen_stubs.obj
#   link/*.lib link/*.exp link/gen_syms.txt link/Loco-linked.map
#   build/Loco-linked.exe
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LINK=toolchain/bin/link
CL=toolchain/bin/cl
CVTRES=toolchain/vc50/BIN/CVTRES.EXE   # SP3's BIN has no CVTRES; RTM's does
LLVM_READOBJ=/opt/homebrew/opt/llvm/bin/llvm-readobj
LLVM_OBJDUMP=/opt/homebrew/opt/llvm/bin/llvm-objdump
CFLAGS="/nologo /c /MT /W3 /O2 /Gy /GX /D WIN32 /D NDEBUG /D _WINDOWS"
LIBS="kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib comdlg32.lib ole32.lib version.lib ddraw.lib dsound.lib winmm.lib"
OUT=build/Loco-linked.exe

# 0. Obj set: exactly one build/*.obj per src/*.cpp (build/ also contains
#    probe/scaffold objs that are NOT part of the app image).
objs=()
missing=0
for f in src/*.cpp; do
  b=$(basename "$f" .cpp)
  if [[ -f "build/$b.obj" ]]; then objs+=("build/$b.obj"); else
    echo "MISSING OBJ for src/$b.cpp"; missing=1; fi
done
[[ $missing -eq 0 ]] || exit 1
echo "== ${#objs[@]} TU objs"

# 1-2. Scaffolding
$CL $CFLAGS /Folink/stubs.obj link/stubs.cpp >/dev/null 2>&1 || { echo "stubs.cpp compile FAILED"; exit 1; }
$CL $CFLAGS /Folink/init_globals.obj link/init_globals.cpp >/dev/null 2>&1 || { echo "stubs.cpp compile FAILED"; exit 1; }
$LINK /lib /nologo /machine:ix86 /def:link/dplayx.def /out:link/dplayx.lib >/dev/null 2>&1
$LINK /lib /nologo /machine:ix86 /def:link/msvfw32.def /out:link/msvfw32.lib >/dev/null 2>&1

# 2b. Resources.  src/ can produce no .rsrc (no .rc source, and VC5 ships no RC.EXE),
#     so re-emit the original's resource directory as a .RES and CVTRES it into an obj.
#     Not decoration: TileKind_GetOrLoadDescriptor turns every tile id into a LoadStringA
#     against RT_STRING, so an exe with no string table loads no art whatsoever.
RESOBJ=""
if tools/extract_res.py -o link/Loco.res >/dev/null 2>&1; then
  if WINEDEBUG=-all wine $CVTRES /machine:ix86 /out:link/Loco_res.obj link/Loco.res >/dev/null 2>&1 \
     && [[ -f link/Loco_res.obj ]]; then
    RESOBJ=link/Loco_res.obj
    echo "== resources: $(wc -c < link/Loco.res) B .res -> $RESOBJ"
  else
    echo "== resources: CVTRES failed -- linking WITHOUT .rsrc (game will load no art)"
  fi
else
  echo "== resources: no loco/Loco.exe to copy .rsrc from -- linking WITHOUT .rsrc"
fi

link_now() { # $1=extra log tag, rest handled via globals
  $LINK /nologo /subsystem:windows /base:0x400000 /opt:noref /map:link/Loco-linked.map \
    /out:$OUT "${objs[@]}" link/stubs.obj link/init_globals.obj ${GENOBJ:-} ${RESOBJ:-} \
    link/dplayx.lib link/msvfw32.lib $LIBS 2>&1
}

harvest() { # stdin = link log, stdout = unique undefined symbol names
  grep -oE 'unresolved external symbol .*' \
    | sed -E -e 's/^unresolved external symbol //' \
             -e 's/^.*\(([^()]*)\)[^()]*$/\1/' \
             -e 's/ .*$//' \
    | tr -d '\r' | sort -u
}

# 3. Pass 1: harvest unresolved symbols
log1=$(link_now pass1)
if printf '%s' "$log1" | grep -q "Internal error"; then
  echo "== pass1 hit VC5 LINK undecorate crash; scraping pmf-signature undefined symbols from objs"
  : > /tmp/loco_pmf.txt
  for o in "${objs[@]}"; do
    $LLVM_READOBJ --symbols "$o" 2>/dev/null | awk '/Name: /{n=$2} /IMAGE_SYM_UNDEFINED/{print n}'
  done | sort -u | grep -E 'P8[A-Za-z0-9_]+@@' > /tmp/loco_pmf.txt || true
  cat /tmp/loco_pmf.txt
  printf '%s\n' "$log1" | harvest > /tmp/loco_undef.txt || true
  cat /tmp/loco_undef.txt /tmp/loco_pmf.txt | sort -u > /tmp/loco_undef_all.txt
else
  printf '%s\n' "$log1" | harvest > /tmp/loco_undef_all.txt
fi

# Symbols with real hand-written stubs must not be gen-stubbed.
grep -vxE '__malloc|__free|_ArrayDestructWithIteratorMaybe' /tmp/loco_undef_all.txt > link/gen_syms.txt || true
cat link/gen_syms_extra.txt >> link/gen_syms.txt
sort -u link/gen_syms.txt -o link/gen_syms.txt

# 4. Generated stubs + final link
python3 link/gen_stubs.py link/gen_syms.txt link/gen_stubs.obj || exit 1
GENOBJ=link/gen_stubs.obj
log2=$(link_now pass2)
# /OPT:NOREF above is deliberate. The default /OPT:REF used to discard Ddraw_Init -- the only
# transcribed body that IMPORTS from DDRAW.dll (DirectDrawCreate; every other DirectDraw use in
# the repo is a COM vtable call, which needs no import) -- because in this smoke link almost
# every path from the entry point still runs through a generated stub, so nothing "reachable"
# called it. The exe then shipped with no DDRAW.dll import at all and could not have brought up
# graphics no matter what. Keeping every COMDAT also makes the section table comparable to the
# original's and stops the stub set from silently changing what is in the image.
printf '%s\n' "$log2" | grep -E "error|warning|Internal" | grep . && exit 1
[[ -f $OUT ]] || { echo "LINK FAILED (no output)"; exit 1; }

# 5. Triage summary
echo
echo "== LINK OK: $OUT ($(wc -c < $OUT) bytes)"
n=$(wc -l < link/gen_syms.txt | tr -d ' ')
vt=$(grep -c '^??_7' link/gen_syms.txt || true)
virt=$(grep -c '@@UAE\|@@U.?E' link/gen_syms.txt || true)
vslot=$(grep -cE '^\?_v[0-9]+@' link/gen_syms.txt || true)
mangled=$(grep -c '^?' link/gen_syms.txt || true)
data=$(grep -c '^_g\|^_DAT_\|^?DAT_\|^?g_' link/gen_syms.txt || true)
cat <<EOF
== Generated stub symbols: $n total (untranscribed TUs / declared-only bodies)
   - mangled C++ (methods, virtuals, globals): $mangled
       of which vtable symbols (??_7):        $vt
       of which virtual methods (@@U..):      $virt
       of which _vNN placeholder vft slots:   $vslot
   - data/globals-ish:                        $data
   - remainder: plain-C app fns in untranscribed TUs
== Sections (ours vs original):
EOF
{ $LLVM_OBJDUMP -h $OUT | grep -E "Idx|\.text|\.rdata|\.data|\.rsrc"; echo "  --- loco/Loco.exe (sacred original, read-only compare)";
  $LLVM_OBJDUMP -h loco/Loco.exe | grep -E "Idx|\.text|\.rdata|\.data|\.rsrc"; } | sed 's/^/   /'
echo "== Imports (ours vs original):"
{ $LLVM_OBJDUMP -p $OUT | grep "DLL Name" | awk '{print "   +", $3}';
  $LLVM_OBJDUMP -p loco/Loco.exe | grep "DLL Name" | awk '{print "   o", $3}'; } | sort | uniq -c

if [[ "${1:-}" == "--run" ]]; then
  echo "== wine smoke run (25s timeout)"
  WINEDEBUG=-all timeout 25 wine "$OUT"; rc=$?
  echo "   wine exit code: $rc (expected: dies early jumping through a"
  echo "   generated stub -- most app bodies are stubs at this stage)"
fi
