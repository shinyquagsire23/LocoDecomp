# Toolchain — Visual C++ 5.0 under CrossOver wine (macOS / Apple Silicon)

Goal: run the original **Microsoft Visual C++ 5.0** `cl.exe` (11.00) / `link.exe` (5.10) under
wine so we can compile matching C++ and byte-compare against `loco/Loco.exe`. See `../CLAUDE.md`
for the full phased plan; this file is the concrete setup.

Ported from `~/workspace/Yodecomp/toolchain/README.md` (VC++ 4.2, cl 10.20/link 3.10) — same wine
harness, same wrapper shape, one compiler generation newer.

**Status (2026-07-10): stood up and smoke-tested end to end** — compile, link, and run under wine
all confirmed working (`toolchain/test/hello.c` → `add()` linked against a `main()` → ran under
`wine`, exit code 5 = `add(2,3)`). `CL.EXE` reports **cl 11.00**, `LINK.EXE` reports **5.00.7022** —
matches the Phase 0 fingerprint's "cl 11.00 era" prediction from the PE header exactly.

## Host status
Loco.exe's fingerprint (CLAUDE.md): PE32 x86, linker 5.10 => Visual C++ 5.0 (cl 11.00 era), link
timestamp 1998-10-06, image base 0x400000 (no rebasing needed — PE vaddrs == Ghidra addrs 1:1),
static CRT (`/MT`-style), **no MFC** (plain Win32; imports are KERNEL32/USER32/GDI32/ADVAPI32/
SHELL32/comdlg32/ole32/VERSION + DDRAW/DSOUND/DPLAYX/MSVFW32/WINMM). Confirm the wine/CrossOver
setup the same way Yodecomp did (its README's "Host status" section) before assuming it transfers
unmodified — worth re-verifying the 32-bit subsystem path on whatever machine actually stands this
up, since that verification hasn't happened for this project yet.

## Acquire VC++ 5.0 (done — source + exact steps, for reproducing on another machine)
VC++ 5.0 is abandonware. **Source: WinWorld's standalone "Microsoft Visual C++ 5.0 Professional
(ISO)"** (https://winworldpc.com/product/visual-c/5x, 254 MB download) — much more direct than
hunting through the 3-disc Visual Studio 97 bundle (its Professional-edition discs 1–3 on
archive.org do NOT contain VC++ standalone; disc 1 = VB5/VFP5/ActiveX/IE3, disc 2 = VJ++/Visual
InterDev/NT QFE, disc 3 = MSDN — VC++ is oddly absent from that particular archive.org upload set).

Steps that worked:
1. Download WinWorld's ISO link — it's actually a **7z archive** despite the `.iso`-looking page,
   containing `Microsoft Visual CPP 5.0 Professional (ISO)/VCPP-5.00.iso` (654 MB). Extract with
   `7z x`.
2. Mount on macOS: `hdiutil attach -readonly VCPP-5.00.iso` → volume `VC50PROCD1`.
3. The compiler lives at `DEVSTUDIO/VC/` on the disc (**not** the top level) —
   `cp -R DEVSTUDIO/VC/{BIN,INCLUDE,LIB,MFC} toolchain/vc50/`.
4. **`CL.EXE`/`LINK.EXE` need two DLLs that live OUTSIDE `DEVSTUDIO/VC/BIN`**, in the shared IDE
   folder — copy these into `toolchain/vc50/BIN/` too or they'll fail to load under wine
   (`err:module:import_dll ... not found`, status `c0000135`):
   - `DEVSTUDIO/SHAREDIDE/BIN/MSPDB50.DLL` (needed by `CL.EXE`)
   - `DEVSTUDIO/SHAREDIDE/BIN/MSDIS100.DLL` (needed by `LINK.EXE`), which itself needs
     `DEVSTUDIO/VC/REDIST/MSVCP50.DLL` and `DEVSTUDIO/VC/REDIST/MSVCRT.DLL` — copy those in too.
5. `toolchain/vc50/` (BIN+INCLUDE+LIB+MFC, no CRT/SAMPLES/HELP/etc) lands at ~127 MB.

`.gitignore` already excludes `/toolchain/vc5*/` — the copyrighted tree is never committed.
Path must contain **no spaces**.

## ⭐ THE TOOLCHAIN IS SP3, NOT RTM — settled v358, and it is the DEFAULT

`toolchain/bin/cl` and `toolchain/bin/link` now default to **`toolchain/vc50sp3`**. Both trees
must be present: `vc50sp3/` carries only the SP3 `c1`/`c1xx`/`c2`/`cvpack`/`link` (plus a copy
of RTM's `CL.EXE` driver, which the SP does not ship) and symlinks RTM's `INCLUDE`/`LIB`.
A/B the other way with `VCDIR=$PWD/toolchain/vc50 tools/cc.sh …`.

**Two independent proofs, both closed in v358:**

1. **`vc50sp3/BIN/link.exe` stamps `5.10.7303` — exactly the `linker 5.10` in `Loco.exe`'s PE
   header**, where RTM's stamps `5.00.7022`. The long-standing "linker-version lead" below is
   therefore CLOSED: the SP that bumps link to 5.10 is `vs97sp3`, and the game shipped built
   with it.
2. **RTM's `c2` emits the wrong 3-byte code-alignment filler, as a whole-image constant.**
   Where the shipped `.text` pads 3 bytes it always uses the single `npad 3` instruction
   `8d 49 00` (`lea ecx,[ecx+0]`) — **35 occurrences, and `90 8b ff` appears ZERO times**.
   RTM's c2 always emits `90 8b ff` (`nop` + `mov edi,edi`, i.e. npad 1 then npad 2) —
   **17 occurrences across our objs, and `8d 49 00` ZERO times.** SP3's c2 emits `8d 49 00`.
   This alone silently capped every function whose body length left a 3-byte gap before an
   interior jump table (found while byte-matching `UIResources::Init`/0x446050, whose ONLY
   residual under RTM was those 3 filler bytes).

**Repo-wide A/B, RTM vs SP3, identical sources (v358): 9 functions GAINED, 0 lost**
(0x403e80, 0x404770, 0x40f6a0, 0x412fb0, 0x416460, 0x417f20, 0x426140, 0x446050, 0x44f210;
EXACT 40007 B/242 funcs → 45929 B/249). SP3 also collapses several parked residuals —
0x458c90 986→5 raw diffs, 0x447400 863→12, 0x4216f0 823→48 — so **every pre-v358 `// EFFECTIVE`
autopsy and `docs/PARKED.md` row is suspect and must be re-measured before being trusted.**
28 more functions improved, 8 regressed slightly, none from EXACT.

**v330's earlier SP3 A/B was not wrong, just too narrow:** it tested one symptom
(`src/Ddraw.cpp`'s sparse-switch cluster-merge lowering) and correctly found SP3 byte-identical
there. Score a toolchain hypothesis repo-wide, the same way v353 scored the flag hypothesis.

## Use it — VALIDATED 2026-07-10
`bin/cl` and `bin/link` are thin wrappers that set `INCLUDE`/`LIB` and dispatch through wine —
ported verbatim from Yodecomp's `bin/cl`/`bin/link` (same wine-wrapper mechanism), with the MFC
`INCLUDE`/`LIB` additions dropped (no MFC here) and the default VC tree pointed at `vc50/` instead
of `vc42/`. Smoke test (confirmed working):
```
toolchain/bin/cl /nologo /c /O2 toolchain/test/hello.c && echo OK
```
Full compile+link+run round trip confirmed too: linked `hello.obj` (has `add()`) against a tiny
`main(){ return add(2,3); }`, ran the resulting PE under `wine` — **exit code 5**. `CL.EXE` reports
**cl 11.00**; `LINK.EXE` reports **Microsoft (R) 32-Bit Incremental Linker Version 5.00.7022**.

Single-TU compile + byte-diff loop (the day-to-day workflow — see `tools/cc.sh`):
```
tools/cc.sh src/Foo.cpp
```

**Flag set: HYPOTHESIS, not yet locked** (CLAUDE.md Phase 0). Starting point:
`/nologo /MT /W3 /O2 /Gy /D WIN32 /D NDEBUG /D _WINDOWS`, link `/INCREMENTAL:NO`. `/Gy` (function-
level linking / one-COMDAT-per-function) is listed explicitly here because it is what makes
per-function matching possible at all (Yoda lesson #1 — default-on for C++ on cl 10.20; confirm
it's still default-on, or needs to be explicit, on cl 11.00). No `/GX` (exceptions) yet — CLAUDE.md's
working hypothesis is that Loco's C portions have exceptions OFF; confirm via `__CxxFrameHandler`/
`_EH_prolog` xrefs (Phase 0 tail) before assuming this holds for every TU. No `/D _MBCS` — that
flag exists to compile `afxwin.h`; Loco has no MFC so it's never needed.

## ⚠️ Wrapper invocation (gotcha, inherited from Yodecomp)
`bin/cl` and `bin/link` are **bash scripts** that internally run `wine .../CL.EXE|LINK.EXE`.
Invoke them **directly** — `toolchain/bin/cl <args>` — NOT `wine toolchain/bin/cl` (that hands the
shell script to wine as a PE => `err:process:exec_process ... not supported on this system`, no
`.obj`, and you may then read a *stale* `.obj` from a prior build and think it worked). The wrappers
already handle wine, `INCLUDE`/`LIB`, and `/`→`Z:\` path conversion for existing-file args.

## Linker-version caveat (endgame only, likely applies here too)
Yodecomp found its `LINK.EXE` stamps a linker-version field that doesn't match the shipped exe's
(4.20 built vs 3.10 shipped) — irrelevant to per-function matching (the field is masked like the
timestamp) but relevant to a byte-identical whole-EXE endgame. CLAUDE.md already pins Loco's linker
at exactly 5.10 from the PE header, so if our `LINK.EXE` stamps something else, expect the same
"fine for now, source the exact linker before the whole-image puzzle" situation.

## Whole-image link smoke test (v521 — `tools/link_check.sh`)
`tools/link_check.sh` links the app objs (66 TU objs under `build/` — **exclude the 33
probe/scaffold objs**: `_szprobe`, `probe*`, `sc_*`, `*_test_includes_only`) plus
`link/stubs.obj` + `link/gen_stubs.obj` and explicit import libs into `build/Loco-linked.exe`
(`/subsystem:windows /base:0x400000`). It links and starts under wine: CRT static init → stub
`WinMain` → real `LocoWinMain`, then dies on the first generated stub (expected — the stub
count, printed each run, is itself a transcription-progress metric). Explicit import libs are
required (our objs carry no `/defaultlib` for Win32 DLLs); DPLAYX and MSVFW32 have no VC5
retail import lib, so `link/*.def` builds stub ones by ordinal/name.

⭐ **VC5 LINK 5.10 undecorate crash (root-caused v521):** LINK crashes with `Internal error
during ReadSymbolTable` (AV at 6A2DA25A) instead of printing LNK2001 whenever an unresolved
external's mangled signature embeds a **pointer-to-member-function type** (`P8<class>@@<cv>`).
Flag-independent (`/GX-`, `/O1`, `/Ob0` all crash). Any future missing body with a pmf
parameter will wedge the linker rather than error — `tools/link_check.sh` detects the crash
and scrapes such symbols straight out of the objs; current instance:
`?ArrayConstructWithIteratorMaybe@@YGPAXPAXII0P8DSoundChannel@@AEXXZ@Z` (hand-stubbed in
`link/stubs.cpp`).
