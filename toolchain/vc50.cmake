# toolchain/vc50.cmake — CMake toolchain file for the wine-wrapped Microsoft Visual C++ 5.0
# (cl 11.00 / link 5.10) build used by the LEGO Loco decompilation.
#
#   cmake -B build-cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/vc50.cmake
#   cmake --build build-cmake
#
# Ported from Yodecomp/toolchain/vc42.cmake (cl 10.20 -> cl 11.00); same rationale, same
# custom-command shape — only the compiler generation and the VC tree directory name changed
# (vc42 -> vc50). NOT YET WIRED to an actual CMakeLists.txt (no src/ TUs exist yet to build) —
# this file only declares the target + hands wrapper paths to whatever build description uses it.
#
# WHY custom-command based (read this before "fixing" it to use CMake's MSVC ruleset):
#   cl 11.00 (_MSC_VER 1100) long predates CMake's MSVC auto-detection, and the compiler is a
#   32-bit Windows PE run under wine through the thin wrappers in toolchain/bin/{cl,link}. Making
#   CMake's built-in MSVC compile/link rules drive that (compiler-id probe, /showIncludes
#   dependency scanning, modern link-flag injection, mt.exe manifest embedding) is fragile and
#   would risk perturbing the byte-exact anchor. So any project using this file should be declared
#   LANGUAGES NONE with add_custom_command()s that invoke the SAME wrappers, with the SAME
#   invocation shape, as tools/cc.sh — the proven byte-match build recipe.

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# Repo root = parent of this toolchain/ directory.
get_filename_component(LOCO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# The wine wrappers (they set INCLUDE/LIB from the VC tree and translate paths for wine).
set(LOCO_CL   "${LOCO_ROOT}/toolchain/bin/cl"   CACHE FILEPATH "VC++ 5.0 cl.exe wine wrapper")
set(LOCO_LINK "${LOCO_ROOT}/toolchain/bin/link" CACHE FILEPATH "VC++ 5.0 link.exe wine wrapper")

# The VC++ 5.0 tree the wrappers use (NOT YET POPULATED — see toolchain/README.md). The wrappers
# honor a VCDIR env override at build time (export VCDIR=... before `cmake --build`) for
# A/B-testing an alternate compiler; this cache entry is informational / for future toolchain
# files that want to point elsewhere.
set(LOCO_VC_DIR "${LOCO_ROOT}/toolchain/vc50" CACHE PATH "VC++ 5.0 install tree (BIN/INCLUDE/LIB)")

# Executables produced by this target are Windows PEs.
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
