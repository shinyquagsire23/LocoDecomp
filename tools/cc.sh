#!/usr/bin/env bash
# Compile one Loco.exe TU with the VC++ 5.0 toolchain into build/<TU>.obj, then
# byte-diff its `// FUNCTION: LOCO 0xADDR` markers against loco/Loco.exe.
#
# The single-command shape CLAUDE.md's "Work loop per function" expects: read the
# ORIGINAL disasm, transcribe idiomatically, `tools/cc.sh src/Foo.cpp`, look at the
# diff, repeat. Synthesized from toolchain/bin/cl (the wine wrapper, ported from
# Yodecomp/toolchain/bin/cl) + tools/verify.py's invocation pattern — Yodecomp has
# no standalone cc.sh of its own (its session loop is toolchain/bin/cl direct +
# tools/verify.py), so this follows the two-step compile-then-verify shape its
# CLAUDE.md documents (see toolchain/README.md's compile-a-TU recipe), combined
# into one script
#
# Usage: tools/cc.sh src/<TU>.cpp [-v]     (run from repo root; -v forwarded to verify.py)
#
# Toolchain: VC++ 5.0 (cl 11.00 / link 5.10) under wine, following Yodecomp's
# toolchain/bin/cl wrapper pattern. NOT YET SOURCED — toolchain/vc50/ is empty
# (see toolchain/README.md); this script is ready to use the moment it's populated.
# Override with VCDIR=/path to A/B-test a candidate build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CL="$ROOT/toolchain/bin/cl"

SRC="${1:?usage: tools/cc.sh src/<TU>.cpp [-v]}"
shift || true
TU="$(basename "${SRC%.cpp}")"
SRC_DIR="$(cd "$(dirname "$SRC")" && pwd)"
BUILD="$ROOT/build"
OBJ="$BUILD/$TU.obj"
mkdir -p "$BUILD"
rm -f "$OBJ"

# Flags: /O2 /Gy /MT, no MFC (no /D _MBCS, no NAFXCW linkage). /GX (C++ EH) is
# CONFIRMED ON (v82) — Phase 0's "/GX-" hypothesis was wrong: it was inferred from
# zero __CxxFrameHandler/_CxxThrowException IMPORTS, but /MT static-links the CRT,
# so a statically-linked _CxxFrameHandler would never show as an import regardless
# of whether /GX is on. Confirmed empirically: a local class-typed variable with a
# non-trivial dtor (no explicit throw/catch needed) reproduces
# DSound_InitDeviceAndChannelPool's exact SEH-frame prologue shape only under /GX;
# adding /GX regressed ZERO of the 39 phase2-probe + 10 DSoundChannel.cpp matches
# (re-verified byte-for-byte identical with and without). See docs/PARKED.md.
FLAGS="/nologo /c /MT /W3 ${LOCO_OPT:-/O2} /Gy /GX /D WIN32 /D NDEBUG /D _WINDOWS"

( cd "$SRC_DIR" && "$CL" $FLAGS "/Fo$OBJ" "$(basename "$SRC")" )

if [[ ! -f "$OBJ" ]]; then
  echo "compile failed — no $OBJ produced" >&2
  exit 1
fi

echo "built $OBJ ($(wc -c < "$OBJ") bytes)"
exec python3 "$ROOT/tools/verify.py" "$SRC" "$@"
