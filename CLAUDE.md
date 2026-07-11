# LocoDecomp — decompilation cheat sheet

Goal: byte-match decompile **LEGO Loco** (`loco/Loco.exe` — Intelligent Games / LEGO Media, 1998)
into real, buildable C++ source. Methodology inherited from two sibling projects:
- **`~/workspace/Yodecomp`** — the mature MSVC-x86-PE byte-match project. Same arch/object
  format, so its process AND codegen lessons transfer — but Yoda is cl 10.20 (MSVC 4.2) and Loco
  is **VC++ 5.0 (link 5.10)**: treat every 4.2 lesson as a strong prior to RE-VERIFY on 5.0.
  Yoda's MFC lessons do NOT apply (Loco has no MFC).

Claude is permitted to edit this file with notes for later Claudes.

## Established facts (this project)

Everything under `loco/` is **gitignored** (original game binary + assets); `docs/`, `src/`,
`tools/` are tracked.

| Item | Where | Notes |
|---|---|---|
| Game binary | `loco/Loco.exe` | THE byte-match target |
| Resource archive | `loco/resource.RFH` (index) + `resource.RFD` (57 MB blob) | custom "RF" format, optional per-file Huffman compression |
| Extractor | `loco/rf-extract.py` | ⚠ **Python 2**; `loco/rfh.txt` = full listing (`name size flags`, flags&1 = compressed) |
| Extracted assets | `loco/extract/`, `loco/art-res/` | BMPs, `.dat` sprite data, `ee.ini`, zooms/, misc/ |
| Subsystem/class/global reference | `docs/subsystems.md` | per-subsystem, kept current — read the relevant section before touching a subsystem |

**`Loco.exe` fingerprint** (from the PE header):
- PE32 x86, **linker 5.10 ⇒ Visual C++ 5.0** (cl 11.00 era), link timestamp 1998-10-06.
- Image base 0x400000, entry 0x4689e0 (`WinMainCRTStartup`). Sections: `.text` 0x401000
  (~481 KB), `.rdata` 0x477000, `.data` 0x47e000 (vsize ≫ raw — big BSS), `.rsrc` 0x501000.
- **No MFC** — plain Win32. Imports: KERNEL32/USER32/GDI32/ADVAPI32/SHELL32/comdlg32/ole32/
  VERSION + **DDRAW, DSOUND, DPLAYX**, **MSVFW32**, WINMM. **Static CRT** (`/MT`-style).
- **RTTI is dead LIBRARY RTTI, not app RTTI**: the only `.?AV` type-descriptor strings
  (0x481920–0x481a38) are the statically-linked iostream.lib set. **No free symbol names anywhere** 
  — naming is a Yoda-style grind from strings/imports/xrefs.
- `/GX` (C++ EH) is ON, not off.**
- Multiplayer subsystem confirmed ("LEGO International Train Server", `loco.legomedia.com`).
  Registry key `SOFTWARE\Intelligent Games\LEGO LOCO\Path`; config `lego.ini`.

## Decompiling — Ghidra access

A ghidra-mcp instance (bethington/ghidra-mcp) is at `http://localhost:8089`, plus the richer
`mcp__ghidra__*` tools. **`Loco.exe` is ALREADY IMPORTED and analyzed** (2018+ functions,
image base 0x400000 — PE vaddrs match Ghidra addrs 1:1).

**⚠ CRITICAL: MANY programs are open. `program=Loco.exe` is MANDATORY on EVERY request —
reads AND writes.** Omitting it silently reads/corrupts a DIFFERENT game. `switch_program`
does not persist (stateless HTTP). On writes, `program=` must be a **QUERY parameter even for
POST** (body-only silently falls back to the active program). Always read back after a write.
Example: `http://localhost:8089/decompile_function?program=Loco.exe&address=0x4689e0`.

**Write recipes/gotchas (battle-tested):**
See `docs/GHIDRA_RECIPES.md`

## Naming conventions

No symbols survive — recover names from strings, imports, resource-file references (the RF
archive filenames are a semantic goldmine: `roads\half-vwint.dat` etc.), and behavior.

⭐ **The single richest naming oracle in this binary is the .dat/.ini KEYWORD PARSERS — mine
them before inferring a field's purpose from behaviour.** Loco is data-driven, so the authors'
own vocabulary ships inside the image as the token strings of `_stricmp` dispatch chains, and
each token binds to exactly one field. `Obj0x4779e0::ParseTokenField` (0x41e9f0) alone named 18
fields in v553. The token's OPERAND ORDER is part of the evidence, and often the decisive part
(`"bitmap_occupancy"` reads `cols` then `rows`; `"FreeToRoam"` reads a RECT left/top/right/bottom).
Rank candidate TUs with `grep -c '_stricmp\|_strnicmp' src/*.cpp`. Caveats, all real: a token
names a field even when nothing READS it back (certain NAME, unknown USE — say which in the
comment); fields the token leaves unnamed keep their `Maybe`, never promoted by analogy with
neighbours; and where a token-written field contradicts one consumer's reading of it, the WRITER
wins and the odd consumer is documented, not judged. See docs/CODEGEN.md #153.

**Function naming = C++ `Namespace::Method`** for `__thiscall` code. ⚠ **The Ghidra namespace
MUST equal a same-named Structure** — that's how auto-`this` gets typed; no matching struct ⇒
`this` degrades to `void*` — or, worse, to Ghidra's auto 1-byte PlaceHolder Class Structure,
which prints as a healthy `Foo *this` while every field access still decompiles as raw offset
math. **`tools/lint_ghidra_types.py` enforces this rule** (INFORMATIONAL; `--strict` fails) —
run it after ANY class rename, since a rename is really TWO renames (namespace + struct) plus
the ctor's own name, and half-applying it is invisible to every other lint.
`set_function_this_type X*` types AND moves the function into
namespace `X` in one act. Plain-C code gets subsystem-prefix names (`Track_`, `Net_`, `Rf_`).

**Free-function subsystem groups ALSO get a real (plain) Ghidra namespace** — keep the full
`Subsystem_Method` flat name (doc prose greps for it) and set the parent namespace, whenever
naming a function that shares a prefix with ≥1 existing sibling. Recipe (`run_script_inline`;
no REST endpoint): look up the prefix in `SymbolTable.getGlobalSymbols(name)` filtered to
NAMESPACE/CLASS first (reuse, don't duplicate — including real classes); else
`SymbolTable.createNameSpace(globalNamespace, prefix, USER_DEFINED)`; then
`Function.setParentNamespace(ns)` per member — touches neither calling convention nor
`this`-typing, safe to batch across dozens of functions.

**Namespace gap-fill = address-boxing between ADJACENT same-class members.** Sort a
namespace's members by address; any still-`FUN_`/`thunk_FUN_` function in `Global` sitting in
the byte range strictly between two ADJACENT members (not the namespace's min-max span) is a
same-TU candidate — move it in WITHOUT renaming (`Namespace::FUN_<addr>` = a valid "boxed but
unread" tier). **Cap the gap at `0x1000` bytes** and log skipped larger gaps for manual
review — uncapped, a class's outermost members can span tens of KB of interleaved
other-class functions. Membership is a hypothesis, cheaply reverted when the body is read.

**Variables: loose-Hungarian** — `p`=pointer (named after the pointee: `pTile`, not roles),
`pa`=pointer-to-array, `b`=bool, `n`=count/int, `w`=word.

**Uncertainty ladder** (grep-able, upgrade over time): `FUN_<addr>` (unread) / `Unk0x44`
(field, size known, purpose not) → `…Related` (touches a known subsystem) → `…Maybe`
(behavior clear, purpose hypothesized) → certain name. Read the body first — never
confidently-wrong names. **`tools/lint_names.py` catalogs the below-certain rungs as a
burn-down tracker** (INFORMATIONAL — never fails a build, unlike the idiom lint): it counts
DISTINCT `src/` names per severity class — `U` (bare `FUN_`/`DAT_`, wholly unresolved) > `FM`
(function w/ Maybe/Related) > `GM` (`g_`/extern global) > `MM` (struct/class member) > `TM`
(struct/class type name, an added 5th class so a type isn't miscounted as the global/ctor that
merely names it) > `UF` (an `Unk0xNN`/`UnkNN`-placeholder struct/class field DECLARATION —
lowest priority, ported from REDACTEDDecomp's `lint_idiom.py` class H and adapted to Loco's own
`Unk0xNN` convention instead of REDACTED's `m_Unk*`; a `->Unk0xb0` USE elsewhere isn't itself a
new site — the idiom lint's class C already flags Unk-field uses that do raw offset math on
top). Comments/string-literals are stripped, so only real code identifiers count; one name =
one finding regardless of call-site count. `progress.py` prints the repo-wide `naming debt`
line beside `idiom debt`; `tools/lint_names.py --list` dumps every name, a PATH arg scopes to
one file/dir.

**⚠ Promoting a struct MEMBER's `...Maybe`-suffixed name to a bare name that collides with an
already-existing LOCAL/PARAMETER identifier in the same function silently shadows the member —
turning a real member read/write into a self-reference, and it compiles cleanly.** 
A bulk naming-promotion sweep (stripping `Maybe` once a member's behavior
is certain) hit this 3 times in one batch: `LocoBitmap *pBitmapA = pBitmapAMaybe;` (a local
caching a member) became `LocoBitmap *pBitmapA = pBitmapA;` once the member was renamed to the
same bare name the local already used — UB (reads the not-yet-initialized local, not the
member) — and `width = widthParam;`/`hIconMaybe = hIcon;` (member = local-derived value) became
`width = width;`/`hIcon = hIcon;` once the member's name collided with an existing parameter/
local — a no-op that stops writing the member at all. None of these raise a compiler diagnostic
(shadowing a member with a local/param is ordinary, legal C++) and structurally the source still
*looks* like a real assignment. The only reliable catch: since a pure identifier rename can
never change generated code, **diff the compiled COMDAT bytes per function name before vs. after
any bulk rename** (mask relocations, compare trimmed code — see `tools/match.py`'s
`coff_functions`/`trim_pad`/`mask`) — every function whose bytes differ is a real regression to
fix, not a tie-break to park. Fix by renaming the LOCAL/PARAMETER (never the member — its
promoted name is the correct one) to something distinct, or by adding an explicit `this->`
prefix at the write site if the local's own name is otherwise fine. Run this byte-diff audit as
routine verification after ANY session that does a struct-member (not just function/global)
naming sweep — `lint_ghidra_sync.py`/`lint_idiom.py` do NOT catch this class of bug, and neither
does a plain compile.

**Struct discipline:** define structs in Ghidra FIRST so the decompiler emits `this->field` —
then transcription is copy-paste. Pin `sizeof` from the allocation site (`operator new(N)` /
`malloc(N)`), not observed access extent. One canonical definition: Ghidra DB + `src/`
headers, synced in the same commit. Non-idiomatic decompiled output (raw casts, magic
offsets) = a type is still missing; model it, don't transcribe mess.

**A hand-written `padN[SIZE]` field's NAME is not proof of its real struct offset — only the
SUM OF PRECEDING MEMBERS' actual sizes is.** Always make sure to reference the Ghidra structure
when altering the size and members of a structure, and for allocated classes, a new() instantiation 
as a ground-truth sizeof() oracle in order to be sure that the struct size doesn't accidentally grow
incorrectly.

**Never duplicate a struct definition across TUs — not even a partial view, not even to
unblock one byte-match.** The original source code was developed by humans, which means that
every struct and class is defined once and only once. Defining multiple partial views of a class
or struct requires extra cleanup and reconciliation later--get it right the first time, and keep
Ghidra and the project synchronized so the decompiler can do the heavy lifting.

**⚠ A VTABLE SLOT AND A NAMED METHOD AT THE SAME ADDRESS ARE ONE FUNCTION — never model them as
two.** The `.rdata` vtable dword IS the ground truth for what a slot is; read it before inventing
a name for the slot. The failure mode (found v477, `CarNetObj`) is a slow one: a slot whose body
is unread gets a placeholder virtual (`RetagKind`), a caller then needs to reach it, so an inline
wrapper with the caller's own vocabulary (`SetCarTypeAndCategory`) is added beside it — and the
class now carries two names, two signatures and one real function. It byte-matches the whole
time (an inline wrapper around a virtual emits the same `call [reg+off]` as the direct virtual
call), so nothing in the build, and no lint, ever objects. It only surfaces when someone
transcribes the body and finds it already has a name. **Prophylactic: whenever you add a
declared-only virtual, dump the class's vtable and check whether Ghidra already names that
address** — one `run_script_inline` reading `getInt(vtblAddr + slot*4)` and resolving the
function. Same family as the `pad` and duplicate-struct rules above: a model that is merely
consistent with the bytes is not the same as a model that is true.

**Headers must be SELF-CONTAINED — `tools/lint_selfcontained.py` is the oracle.** A header that
names a type it never declares compiles fine as long as every TU reaching it happens to include
that type's provider first; that is luck about include order, not a property of the header, and
it detonates later in a TU that never touched either file (v474: `src/PostBag.h` turned four
unrelated TUs into `COMPILE FAILED` at once). The tool compiles a one-line TU per `src/*.h` with
`cc.sh`'s own flags. INFORMATIONAL (exit 0) like `lint_names.py`/`lint_alias.py`; `--strict`
fails; a PATH/name argument checks one header. Deliberately NOT wired into `verify.py`/`cc.sh` —
a full sweep is ~60 serialized wine invocations. Run it after adding a header or giving one a new
member type. **The fix — one guarded `#include` at the top of the header — is byte-neutral BY
CONSTRUCTION while the repo compiles** (every consumer already includes the provider ahead of it,
so the provider's guard makes the line expand to nothing); confirm with a full `progress.py`
per-file table diff anyway.

## Decompilation strategy (phased plan)

Match functions **individually first** (address-anchored, reccmp-style); byte-identical
whole-image is a deterministic endgame puzzle, deferred.

### ⭐ Prior art — the trail is fully blazed, USE IT
- **`~/workspace/Yodecomp/tools/`** — the x86 PE/COFF harness to port nearly verbatim:
  `match.py`/`verify.py` (reloc-masked per-marker COMDAT compare), `progress.py` (name-keyed
  dashboard), `asmscore.py` (`--dump` = side-by-side), `bugscan.py`, `vtcheck.py`,
  `link_exe.sh`.
- **`~/workspace/Yodecomp/toolchain/`** — wine-wrapped cl pattern. ⚠ Parallel wine cl
  deadlocks the wineserver — serialize (Yoda's `JOB_POOL wine=1`).
- **⚠ Pre-v350 `asmscore.py` numbers for FREE functions are WRONG — re-score before trusting
  them.** Its CLI used to derive the marker→COMDAT pairing itself (scan forward for
  `Class::method(`, else global-min-total over every COMDAT). A free function has no `Class::`
  on its definition line, so the scan ran PAST it and scored whatever method came next in the
  file. `0x43f880` (`GameNet_RemoveSourcePeerAndReconcile`) reported `insns 169/60, total
  822119` against a true `60/60, total 13566` — and a `possweep.py` run over it then reported a
  spurious BEST, because the one slot with no following `Class::method(` is the only one where
  the fallback picked the right COMDAT. Fixed in v350: `asmscore` now uses `match.pair_by_name`,
  the same derivation `progress.py`/`verify.py` use. Any `docs/PARKED.md` row or in-source
  autopsy whose numbers came from asmscore on a free function is suspect.
- **⚠ `tools/asmscore.py`'s `--dump`/score DEFAULTS TO THE CANDIDATE'S OWN COMPILED LENGTH
  when `--len` isn't passed** — it silently truncates the comparison window to whatever the
  candidate happens to compile to, rather than flagging that the candidate might be
  SHORTER than the real original. On any function suspected of being an incomplete/first-draft
  transcription (as opposed to one already believed structurally complete), always pass
  `--len <N>` with the TRUE Ghidra body size (`get_function_by_address`'s own `Body: START -
  END` span, not a hand-computed guess) before trusting the diff or score — confirmed
  (v142, `EditCardWnd::RefreshClientClipRectMaybe`/0x417180) that omitting `--len` hid a
  genuine ~205-instruction gap (938 real insns vs. 733 compiled), which read as ordinary
  register-allocation noise until the true length was checked. **A second, sneakier version of
  the same trap (v218): `tools/cc.sh`'s own per-function `len=` column ALSO reports the
  CANDIDATE's current compiled length, not the true original** — feeding a stale DIFF line's
  `len=` back into `asmscore.py --len` (instead of a fresh `get_function_by_address` lookup) is
  circular and silently re-truncates the window to whatever your own last edit happened to
  compile to, hiding real structural gaps behind what looks like ordinary register noise. Only
  a `MATCH` line's `len=` is trustworthy (that's a real byte-identical comparison over the true
  length). **A THIRD version of the trap (v352), which fabricates the OPPOSITE error — a fake
  "the body is missing" reading on a function that is actually content-complete: `--len` wants
  the COMDAT extent, and BOTH obvious sources give you the CODE extent instead.**
  `toolchain/test/app_funcs.txt` and Ghidra's `Body:` span both stop at the last INSTRUCTION,
  excluding the trailing JUMP TABLE that the linker copies as part of the same COMDAT. On a
  switch-heavy function that truncates the compare window: 0x445400 (`PostBag_BuildAttFilePath`)
  scored `insns 88/51` under app_funcs' `--len 193` — which reads as ~37 instructions of missing
  body — versus its true `insns 88/88` under `--len 0x110` (272 = 200 code + 72 table). Derive
  `--len` from the NEXT function's start address whenever the function contains a switch. Related:
  most of the `align` cost on such a function is masked jump-table noise (the candidate's table
  entries are relocations, decoding as `add byte ptr [eax], al`, while the original's are real
  addresses), so judge it by the dump rows ABOVE the table boundary, never the headline score —
  0x445400's `align=242` concealed a real code disagreement of exactly THREE instructions.
  `get_function_by_address`'s `Body` span itself needs a small correction too: it can
  clip a trailing multi-byte instruction (e.g. a 3-byte `ret 0xNN`) right at the boundary —
  round up a byte or two and re-check rather than trusting the raw subtraction verbatim.
- **⚠ `asmscore.py`'s headline `insns A/B` is `MINE/ORIGINAL` — candidate FIRST** (the print is
  literally `self.n_mine, self.n_orig`). This reads backwards from the `--dump` column order
  (left=original) directly beneath it, and getting it wrong inverts the single most important
  question about a residual: whether you are MISSING body or emitting EXTRA. v378 read
  `insns 199/219` as "20 instructions too many" and went looking for something to delete, when
  the candidate was 20 instructions SHORT (a cross-jumped duplicate tail). The 0x445400 example
  below is the same orientation: `88/51` is a full 88-instruction candidate against an
  ORIGINAL window truncated to 51 by a wrong `--len`. When in doubt, re-derive it — a candidate
  that is also SHORTER in bytes than the original almost always has FEWER instructions too.
- **⚠ `tools/asmscore.py --dump`'s two columns are LEFT=ORIGINAL, RIGHT=CANDIDATE (mine) —
  easy to misread backwards, confirmed a real mid-session mistake (v215).** Per
  `dump_diff`'s own print (`"%s | %s" % (orig_side, mine_side)`): a `-` mark means the
  instruction is the ORIGINAL's own content missing from the candidate (right blank); a `+`
  mark means the CANDIDATE has extra content the original lacks (left blank); `r`/`S` rows
  print orig on the left, mine on the right too. Getting this backwards inverts every
  conclusion about which side needs to gain vs. lose an instruction — always sanity-check a
  suspicious `-`/`+` pattern against a raw `objdump -d -M intel --start-address=<addr>
  --stop-address=<addr+N> loco/Loco.exe` read of the ORIGINAL before trusting a large-scale
  dump interpretation (the `.obj` candidate side needs `objdump -b coff-i386`, though this
  toolchain's own COFF output can trip binutils' storage-class parser — `asmscore.py` uses
  capstone internally instead, so prefer trusting its own printed mnemonic/operand text over
  re-disassembling the `.obj` by hand).
- **isledecomp / reccmp** (github.com/isledecomp) — the `// FUNCTION:` marker methodology,
  x86 PE/PDB-native. **decomp.me** hosts VC++ 5.0 for single-function experiments.

### Phase 0 — Pin the compiler ✅ DONE
VC++ 5.0, flag hypothesis `/O2 /Gy /MT` (no `/GX`). Closed empirically via Phase 2's 39/48
exact matches across wildly different code shapes, every residual individually diagnosed as
an intrinsic allocator/scheduler tie-break or a source-shape issue — no wrong-flags symptoms
(those would be systemic, not isolated). Strong-but-not-airtight: revisit only if a future TU
stubbornly refuses to match structurally. **Re-verified empirically in v353 and CLOSED**: v352's
PostBag tail-merge suspicion was tested repo-wide with `LOCO_OPT=<flags> tools/progress.py` and
refuted — `/O2` == `/Ox /Gf` == `/Ox /GF` to the byte (38464 B / 235 funcs), `/Gf` string
pooling is load-bearing (`/Ox` alone costs 3401 B / 2 funcs), and per-file overrides are ruled
out by COMDAT adjacency. Score any future flag hypothesis repo-wide before believing a single
TU's local opinion.

### Phase 1 — Matching toolchain ✅ DONE
wine + cl 11.00 per Yodecomp's pattern; `cc.sh`/`verify.py`/`find_leaves.py` ported and
working; marker format `// FUNCTION: LOCO 0x<addr>` adopted.

### Phase 2 — First byte-match ✅ DONE (proof-of-pipeline) — PAUSED, not being scaled
39/48 exact across `src/phase2_probe.cpp`–`probe5.cpp` (9/9, 15/20, 4/8, 1/1, 10/10),
proving the toolchain end-to-end. Deliberately stopped grinding marginal leaves — poor ROI vs
Phase 3. Parked residuals: `docs/PARKED.md`; match inventory: `docs/subsystems.md` appendix.
Pick these back up opportunistically or when Phase 4 needs them.

### Phase 3 — Map compile units & document ⬅ CURRENT PHASE
No source map exists — reverse the TU skeleton Yoda-style: contiguity in `.text` address
order within an `.obj`, shared string pools/static helpers, subsystem clustering (`/Gy` gives
per-function COMDATs but the linker still groups an .obj's contributions). Document structs &
signatures in Ghidra, mirror into `src/` headers, and keep `docs/subsystems.md` current.
Proven methods:
- **Subsystem fan-out:** parallel READ-ONLY research agents, one per subsystem entry point
  (agents never write to Ghidra — avoids concurrent-write races on the shared instance; the
  main thread applies renames/structs afterward). Far cheaper than an address-order sweep.
- **`operator new` xref sweep:** `get_xrefs_to` on `0x465ce0` (`operator new`) surfaces every
  ctor candidate at once (ctor = new → vtable store at 0 → field inits → return `this`);
  split into address clusters, one read-only agent each. Works on `operator delete`
  (`0x465cd0`) for the dtor side too, but ~90% of those xrefs are SEH `Unwind@` cleanup
  thunks — filter first.
- **Address-boxing** (see Naming conventions) to sweep gap functions into namespaces.
- Cross-check a "newly discovered" singleton's field offsets against already-named structs
  before assuming it's new — address-boxing can't distinguish "new object" from "known
  object reached via a different call path".

### Phase 4 — Scale matching
TU-by-TU: write idiomatic C++ per compile unit, compile with the locked toolchain,
byte-match, track % with `progress.py`.

## ⭐ KEY VC++ x86 byte-matching lessons (inherited from Yodecomp, proven on MSVC 4.2)
See `docs/CODEGEN.md`

## Standing rules (compiler-agnostic)

- **The unit of completion is the TRANSLATION UNIT, not the function.** Codegen state flows
  forward through a TU (#7; measure 5.0's carry early). Plan TU-by-TU; leaves match piecemeal.
- **Structs before transcription — push the struct into Ghidra IMMEDIATELY.** Once a struct
  named exactly like the namespace exists, offsets are the tool's job, not yours.
- **Matched C++ must be REAL, idiomatic source** — a human wrote this game. The byte-match is
  the correctness oracle: rewrite idiomatic, recompile, confirm. If an idiomatic form breaks
  the match, keep the faithful form with `// TODO: idiom`.
- **`tools/lint_idiom.py` ENFORCES the idiomatic-source rule**
  Raw offset access = a still-unmodeled struct field. Classes: A `(char*)p + off`
  (including the scaled form `(char*)p + idx * 0x18` — an index expression in front of
  the magic number is the same raw byte-offset math),
  B `*(T*)(p + off)`, C `Unk`-ptr-field `+ off`, D access through a `pad`-named field
  (declaring `char pad0x8[8];` is fine; READING/WRITING through it means the field is real),
  E **duplicate class definition** (a class defined with a body outside its home file when
  ≥2 definitions exist repo-wide — the per-TU "local shortcut model" = the shared-struct
  `sizeof`-drift regression hazard; a deliberate canonical site in a sibling's header gets
  `// idiom-exempt: canonical`), F raw vtable-slot dispatch `(*(VFn**)p)[N](p)` (an
  unmodeled virtual method) — INCLUDING any raw cast of `this`, `((T*)this)->Method(...)`:
  the "probe struct" evasion where the slot math hides inside the probe type's layout
  instead of at the call site (same fix: declare the virtual on the class's real vtable
  and call it by name), G function-local `typedef` in a `.cpp` (hoist to the header),
  H a `memcpy`/`memset`/`memmove` call whose size argument has no `sizeof` (a bare byte-count
  magic number is exactly as fragile as a raw struct offset, I `extern "C"` in a `.cpp` (file-local
  extern decls dodge the shared subsystem header, so two TUs can declare the SAME symbol
  under different names/types and silently desync from each other and the Ghidra DB — move
  the decls to the shared header, where `extern "C"` itself is fine and often load-bearing
  under `/GX`; the class is about WHERE the decl lives, not the linkage). **⚠ A/B/C findings can NEVER be
  `// idiom-exempt`'d** — `+ 0x30`-style pointer arithmetic always
  means an unmodeled field, so `idiom-exempt` on an A/B/C line is IGNORED by the tool (the
  finding still fires); the only outs are a real fix (`&this->field`, or — if the offset can't
  cleanly map to one field yet — its own exactly-positioned `Unk0xNN`/`UnkNN` placeholder field
  referenced directly) or, failing that, `// TODO: idiom` backlog tagging. `offsetof(...)` is
  reserved for ONE narrow case — naming a class-H `memcpy`/`memset` size argument when the
  copied region starts at a named field but doesn't align with one field's own address (e.g.
  `memset((char*)this + offsetof(T, tailField), 0, sizeof(T) - offsetof(T, tailField))`) — it
  is NOT a general substitute for `&this->field` when a plain named-field reference already
  gives the address you need.
  **Severity A–E, H–I > F–G**: A–E and H–I are NON-LOCAL hazards or correctness smells (a wrong
  hand-computed offset/size, drifted duplicate-model `sizeof`, unsized `memcpy`/`memset`,
  or a desynced per-TU extern decl silently regresses OTHER TUs or masks a wrong-size bug);
  F–G are self-contained (the
  site's own byte-match verifies the slot) — clean up A–E and H–I first. Modes: default =
  per-file table; a PATH argument lists that file/dir's findings (`tools/lint_idiom.py
  src/LocoBitmap.cpp` — the per-instance view for cleanup work); `--only EF` filters by
  class; `--doc` regenerates `docs/IDIOM_CLEANUP.md` (do so after cleanup work); `--tag`
  appends `// TODO: idiom` to untagged lines. **`verify.py` — the tail of every
  `tools/cc.sh` run — auto-lints the TU + its local `#include "..."` set** (output capped
  at 5 findings) and **UNTAGGED findings FAIL the run** — model the field instead, or tag
  known debt `// TODO: idiom` / a reviewed deliberate keep `// idiom-exempt <why>`.
  `progress.py` prints the repo-wide `idiom debt` line; it must never go UP in a session,
  and cleanup sessions drive it to 0.
- **`tools/lint_ghidra_sync.py` cross-checks src/ names against the LIVE Ghidra DB.** Every
  `// FUNCTION: LOCO 0x<addr>` marker's definition name and every declaration carrying a
  `// FUN_00xxxxxx`/`// DAT_00xxxxxx` address comment must match Ghidra's fully-qualified
  name at that address (accepted variants: exact FQN, last `::` component — free functions
  live in a plain subsystem namespace like `Wav::Wav_ParseAndLoad` — and `::`→`_`; a raw
  struct-instance global keeping its auto `DAT_` name in Ghidra counts as an in-sync ALIAS,
  that's the documented `set_global` Hungarian wall). Statuses MISMATCH/MISSING/PARSEFAIL
  exit 1 when untagged; `// TODO: sync` on the definition/declaration line = parked debt
  (`--tag` bulk-tags; the frozen phase2 probe TUs' deliberately probe-local names are parked
  this way). One batched `run_script_inline` call resolves all addresses; offline Ghidra ⇒
  warn + exit 0, so it is deliberately NOT wired into `verify.py`/`cc.sh`; `--strict`
  hard-fails the offline case instead (exit 2) — that's what the pre-commit hook uses.
  Also run it after any Ghidra rename sweep (cheaper to fix drift while the rename is fresh).
- **`tools/lint_alias.py` finds BYTE-INVISIBLE WRONG CALL TARGETS** — an address transcribed
  under name N but declared and CALLED from another TU under a different name M, so the emitted
  call targets a symbol that exists nowhere. Nothing else here can see it: `verify.py`/`match.py`
  mask relocations (the wrong call is byte-identical to the right one), we never link, and
  `lint_ghidra_sync.py` accepts both a Ghidra FQN and its `::`→`_` flattening, so two spellings
  can each agree with Ghidra while disagreeing with each other. INFORMATIONAL (exit 0 always,
  like `lint_names.py`); the marker set is its oracle. The main producer is the TU-local
  methods-only VIEW STRUCT pattern — correct while a callee is untranscribed, a live defect the
  moment a real definition lands elsewhere. Fix = delete the alias, include the real header;
  that is a header change, so measure with a full `progress.py` per-file table diff. See
  `docs/CODEGEN.md`.
- **`tools/lint_desync.py` finds the SAME-NAME sibling of that defect, which `lint_alias.py` is
  structurally blind to** — two TUs spelling ONE symbol differently because they disagree about
  its LINKAGE (`extern "C"` vs C++), CALLING CONVENTION, or a PARAMETER/POINTER TYPE. `lint_alias`
  compares NAMES and every spelling here shares the same name; each declaration is well-formed in
  its own TU, so the compiler is happy and every other lint is green. It groups every
  `build/*.obj` symbol by the identifier its mangling encodes and reports any identifier with ≥2
  spellings where one is never DEFINED. INFORMATIONAL (exit 0); `--strict` fails on the DEFINED
  class (some obj defines a spelling ⇒ dissenters are provably wrong; with no definition yet the
  majority is only a convention). Needs `build/*.obj` — run `progress.py` first. ⚠ **Fixes are not
  automatically free: a wrong convention on a callee WITH arguments is real emitted code (v554:
  +197 B), but a linkage change can also move the declaration dial either way (v554: +714 B in
  one TU, −124 B in another). Measure every fix, bisect one declaration at a time when a batch
  regresses, and park what you cannot afford with the measurement in the note.** For the
  `extern "C"` variety the free fix is to INCLUDE THE SHARED HEADER that already declares it —
  `extern "C"` is illegal at block scope (C2598), and hoisting it to file scope just becomes a
  class I idiom finding. See `docs/CODEGEN.md` #155–#156.
- **Every commit is gated by the tracked pre-commit hook `tools/githooks/pre-commit`**
  (installed via LOCAL git config — `git config core.hooksPath tools/githooks`, re-run once
  per fresh clone; verify with `git config core.hooksPath`). It hard-fails the commit on
  (1) UNTAGGED `lint_idiom.py` findings and (2) UNTAGGED `lint_ghidra_sync.py --strict`
  findings — including Ghidra simply not running, so the sync check can't be skipped by
  accident; `lint_names.py` is printed as info only. Linting errors are NEVER ignored:
  fix them or tag genuinely parked debt (`// TODO: idiom` / `// TODO: sync`) so the
  parked-ness itself is visible and greppable. `git commit --no-verify` is for genuine
  emergencies only, needs the user's explicit OK, and the reason goes in the commit message.
- **A BODY THAT BYTE-MATCHED ONCE GETS LANDED, EVEN IF IT COSTS A SIBLING ITS EXACT** (user
  directive, v560). Do NOT hide a transcription behind `#ifdef LOCO_PORT`, and do not withhold it,
  merely because landing it regresses some other function's byte-match: that is a DIAL issue to be
  sorted once transcription is complete, not a reason to keep working code out of the tree. Land it
  and note in a comment the last commit at which it byte-matched, so a later session can re-derive
  the price. `#ifdef LOCO_PORT` stays correct for genuine port SCAFFOLDING — code with no original
  counterpart at all (a forwarder standing in for one address declared under two C++ spellings, a
  present/diagnostic hook) — because that code must never reach the match build.
  ⭐ **Corollary, and it pays immediately: a priced-and-withheld toll is a measurement with an
  EXPIRY DATE.** Re-measure before believing it, and especially before spending a session on the
  lever meant to unlock it. `RectFlagObj0x477820`'s three bodies sat withheld from v540 at a
  measured −256 B; landing them in v560 measured **+119 B / +3 funcs with no loss anywhere**,
  because the 407 B victim had already lost its EXACT to something else ~20 sessions earlier. Both
  measurements were correct when taken — what expired was the assumption that the victim was still
  alive. Re-running it cost one compile. See `docs/CODEGEN.md` #175.

- **`// EFFECTIVE` for allocator/scheduler tie-breaks:** identical structure, only register
  names/scheduling differ, cheap probes don't move it ⇒ one-line autopsy and move on. Track
  in `docs/PARKED.md` (add a row when parking, delete when matched).
- **Triage budget:** ~30 min of one-compile-at-a-time shape probes on a stubborn residual,
  then park. A minimal-TU probe (the function + header alone) splits intrinsic vs
  TU-position-sensitive residuals.
- **Agents for read-only RE sweeps** (naming/xref/structure surveys parallelize well); keep
  matching iterations in the main thread (serial compile-and-look loops). **A delegated
  exhaustive-trace agent's structural conclusion is a strong lead, not ground truth on a
  register-allocation maze** (v140, `RedrawCustomCursorMaybe`): a research agent correctly
  found that a whole local was missing, but its own guessed formula for one field was subtly
  wrong (conflated two reload sites into one combined expression) — only re-deriving the FINAL
  memory layout by hand, byte-offset by byte-offset, for the actual by-value struct arguments
  being built caught it. Re-verify a subagent's register/stack-slot conclusions against raw
  hex bytes yourself before committing them to `src/`, especially on a function dense enough to
  need delegation in the first place.
- **`docs/subsystems.md` is a per-subsystem REFERENCE, not a log.** Merge new findings into
  the right section, correct refuted claims in place, never append per-session narrative.
  Chronology lives in git log + `docs/PICKUPS_PRIOR.md`.
- **Original engine bugs are reproduced, not fixed** — document in `docs/engine-bugs.md`,
  mark reproduction sites `// sic:`.
- **⚠ A Ghidra auto-label MANGLES non-alphanumeric characters in a string to `_`, so copying a
  string literal out of the decompiler's label instead of the BINARY's bytes silently changes
  it.** `s__curr_0047e2a0` is the label for the string `"~curr"`, not `"_curr"` — and
  `src/Main.cpp` carried `"_curr"` at two call sites for many sessions (found and fixed v395;
  `src/WidgetPicker.cpp` had the correct `"~curr"` at its own two sites all along, so the two
  TUs silently disagreed about the same checkpoint filename). Neither the compiler nor any lint
  can catch this — a wrong literal is still a valid literal, and under `/Gf` it just pools as a
  second string. ALWAYS read a string literal out of the image itself (locate its VA in the
  section table, dump the raw bytes) rather than trusting the `s_`-prefixed label; the label is
  a display name, not the data.
- **⚠ Resolve a Win32 manifest constant NUMERICALLY, never by which name "obviously" fits the
  context.** Ghidra renders a `MAKEINTRESOURCE`-style argument faithfully as its ordinal
  (`&DAT_00007f01`); the failure mode is entirely on the reading side — v470 transcribed
  `IDC_ARROW` into an EDIT-control subclass proc because an arrow is what a text box "obviously"
  puts back, when 0x7f01 is `IDC_IBEAM` and `IDC_ARROW` is 0x7f00. The result compiles, reads
  correctly, and is off by exactly one byte. Same family as the `s_`-label trap above: the tool
  told the truth and the plausible reading overrode it.
- **⚠ After ANY header edit — including a one-word one — run a FULL `tools/progress.py`, not just
  `tools/cc.sh` on the TU you were working on.** A header change can break a TU you never opened,
  and the per-file compile will not show it. v414's one-word `class LocoBitmap;` →
  `struct LocoBitmap;` fix in `src/CursorDesc.h` silently turned three unrelated TUs into
  `COMPILE FAILED` (−9125 B / −26 exact funcs) because VC5 decides a type's DEFAULT ACCESS from the
  LAST-SEEN `class`/`struct` tag — see `docs/CODEGEN.md`'s last bullet. `progress.py`'s per-file
  table prints `COMPILE FAILED` explicitly; the headline EXACT number just quietly drops, so read
  the TABLE, not only the total. Same reasoning as the "never `git stash` around a baseline
  measurement" rule below: the measurement you skipped is the one that was wrong.
  ⭐ **And ADDING A FUNCTION BODY to a `.cpp` moves the same bit a header declaration does —
  measure a new definition as carefully as a new declaration** (v479, `src/WorldBoardMaybe.cpp`:
  one new 181-byte body cost a SIBLING in the same TU its 951-byte EXACT, at an identical
  instruction count with a pure `eax`↔`ecx` rename; the new definition's POSITION in the file was
  irrelevant). Where a header carries a documented declaration-count sensitivity, treat it as a
  PARITY BIT spanning that header's MEMBER declarations AND its TU's definitions together — and
  pay by ADDING a real declaration rather than retiring one, which is free and documents one more
  method. ⚠ **"Free-function declarations do not move it" is REFUTED for `src/AppWindow.h`** (v488, measured
  from a clean baseline): a single added `__cdecl` free-function declaration cost
  `src/WorldBoardMaybe.cpp`'s 0x457ce0 its full 951-byte exact, on its own. Treat the rule as a
  weak prior per header, not a licence to skip the measurement.
  ⭐ **And the dial is NOT a property of one header at all — FILE-LOCAL declarations count toward
  the same parity** (v490). v488 measured that no `src/AppWindow.h` declaration count recovers
  0x457ce0 and concluded it was SPENT; v490 got all 951 B back without touching that header, by
  deleting a one-method TU-LOCAL VIEW STRUCT from `src/WorldBoardMaybe.cpp` itself. So a "spent"
  verdict only ever covers the knobs actually tried. **Corollary: fixing a `lint_alias.py` finding
  is a BYTE LEVER, not just hygiene** — retiring an alias retires a declaration, and here the two
  arrived together because a view struct is the main producer of both. ⚠ Never retire a declaration without first
  checking it has no callers (`grep -rl` across `src/*.cpp`): a stale "declared-only, safe to
  delete" note in a header ages badly, and deleting a since-called declaration turns other TUs
  into `COMPILE FAILED`.
- **⚠ macOS `sed` is BSD sed: `\b` is a GNU-only word boundary and silently matches NOTHING** —
  `sed -i '' 's/\bOldName\b/New/g'` exits 0, reports nothing, and changes no bytes (hit for real
  in v350's `FUN_0044bcd0` rename). Since this project does bulk identifier renames constantly,
  ALWAYS grep for the new name afterwards to confirm the substitution actually landed; a clean
  exit code is not evidence. Use a plain pattern when the identifier is already unique (the
  `FUN_`/`DAT_` forms always are), or `[[:<:]]`/`[[:>:]]` when a boundary is genuinely needed.
- **⚠ Do NOT use `git stash` / `git stash pop` to get a baseline measurement while you also
  hand-edit the same files — the pop silently produces a HYBRID.** `progress.py`/`cc.sh` read the
  working tree, so stashing to measure HEAD is tempting; but if you `cp` a scratch copy over a
  tracked file before popping, git applies the stashed DIFF on top of whatever is there and the
  result is neither version (hit for real in v378: `src/DPlaySessionMgr.cpp` came back with a
  stray probe `#include` and the real one missing, while `git status` showed a perfectly ordinary
  ` M`). Nothing warns you, and the next compile measures fiction. Instead copy the file to the
  SCRATCHPAD, `git checkout --` the tracked path, measure, then restore from the scratchpad copy
  — or measure HEAD in a separate worktree.
- **⚠ The Bash tool's shell is zsh, and zsh does NOT word-split unquoted parameter expansions**
  (unlike bash/sh). `for x in "4202f0 176" ...; do set -- $x; helper $1 $2; done` silently passes
  ONE argument (`"4202f0 176"`) and `$2` is empty — the helper then builds nonsense and the loop
  exits 0-ish with garbage output (hit for real in v375 while dumping disasm ranges). Same
  family as the BSD-`sed` `\b` trap above: **write scratch helpers that take structured input in
  Python, not as shell loops over space-separated strings.**
- **Tools:** GNU binutils at `/opt/homebrew/opt/binutils/bin/`. Scratch work goes in the
  session scratchpad, not the repo. Ghidra `save_program`: use the HTTP endpoint
  `curl 'localhost:8089/save_program?program=Loco.exe'` — an inline
  `getCurrentProgram().save(getMonitor())` does NOT compile under the
  `run_script_inline` bridge (v324). When GNU objdump refuses a `.obj` ("file format is
  ambiguous"/storage-class-105 weak-external `??_E` from a declared-but-empty dtor),
  `/opt/homebrew/opt/llvm/bin/llvm-objdump -d -M intel` is a drop-in replacement (v329,
  src/AnimEffectObj.cpp).

## SESSION PROTOCOL (the phasing loop — follow every session)

1. **Orient:** read the ⏭ NEXT SESSION PICKUP block below. Reproduce its recorded match count
   BEFORE changing anything — if it doesn't reproduce, a header/toolchain input drifted;
   bisect first.
2. **Ghidra check:** confirm `Loco.exe` is open (`curl -s localhost:8089/list_open_programs`);
   `program=Loco.exe` on every call; read back writes.
3. **Work loop per function:** read the ORIGINAL disasm first (dump to a tmp file); transcribe
   idiomatically; compile fresh; diff at the function's address. Structural diff ⇒ hunt the
   source construct — **grep `docs/CODEGEN.md` for the symptom first**; most residuals are a 
   documented pattern. Pure tie-break with identical structure ⇒ `// EFFECTIVE` + autopsy +
   `docs/PARKED.md` row. Every `cc.sh` run ends with the idiom lint — an UNTAGGED non-idiomatic 
   finding fails the run even when all functions match: model the struct field
4. **Triage a stubborn residual (cheap probes, ~30 min max, then park):** flip
   operand/decl/guard shapes one compile at a time; check CALL PARITY (⚠ there is no
   `tools/bltally.py` — the protocol named one for many sessions but it has never existed, same
   as `tools/modules.py`; tally calls by hand: `grep -c call` the raw disasm vs. your own
   source's call sites, matching on YOUR names not Ghidra's) and check the
   epilogue (return type) before any autopsy; park with the probe results in the note **and
   add/update the function's row in `docs/PARKED.md`** (delete the row when it later matches).
   In general, prefer to decompile new functions rather than grind existing residuals until 95%
   transcription progress.
5. **Session end (all of these):**
   - **Bump the pickup version.** Write the NEW `⏭ NEXT SESSION PICKUP (vN+1)` block and move the
     just-completed pickup verbatim to **`docs/PICKUPS_PRIOR.md`** (append, newest last) — that
     is the LIVE archive every session since v30 has used. ⚠ A SECOND, STALE `PICKUPS_PRIOR.md`
     exists at the REPO ROOT: a v11-v24 fragment, abandoned long ago and NOT the archive
     (corrected v348, which is when the long-standing "check `git ls-files | grep PICKUPS` if a
     second ever appears" note finally got checked). Do not append to the root file; leave it
     alone pending a user decision on deleting it. CLAUDE.md carries exactly one pickup block,
     never a chronology. **Per-TU numbers come from `tools/progress.py`'s own per-file table,
     pasted — never hand-counted.** (There is no `tools/modules.py`; the protocol referenced one
     for many sessions but it has never existed in `tools/`.)
   - **Distill lessons as discrete bullets into `docs/CODEGEN.md`** (edit/dedup an existing bullet
     if it overlaps; group by its section headers). Only PROCESS-level rules belong in this file.
     Do not accrete per-session "vN did X" milestone paragraphs anywhere but the pickup archive.
   - Update `docs/MODULES.md` / struct docs if changed; sync new struct fields/renames to Ghidra
     **only if it's ACTIVE** (else list them PENDING in the pickup); `save_program`.
   - **A commit at the end of every session is REQUIRED** — descriptive message, clean tree.
     Intermediate commits mid-session are encouraged (checkpoint after each milestone).
6. **Agents:** use them for read-only RE sweeps (naming/xref/structure surveys — they parallelize
   well); keep matching iterations in the main thread (serial compile-and-look loops).

## TODO: These got added to the session protocol erroneously, relocate to appropriate sections
4. **Sweep usage note:** `tools/sweep.py <lib>`; never rename an app-region hit (< ~0x463800)
   from the sweep line alone (0x44f510 lesson); byte-identical twins disambiguate by actual
   call targets.
5. **Ambiguous pairs left:** `__getdcwd` vs `__wgetdcwd` stubs, `__loaddll`/`__seterrormode`
   thunks, `??_G*` scalar-deleting-dtor thunks (ICF-folded, intentionally left).
6. **`LocoBitmap.cpp`'s blit family is STILL PARKED** (v312-319 conclusion stands).
7. **DirectPlay connect/host/join residual cluster is STILL CLOSED** (v290).
8. **`DPlay_ReceiveAndDispatch` (0x4606d0) is STILL PARKED** (v293).
9. **DAT_ globals cluster (19 names left)** -- low ROI.
10. **EFFECTIVE/PARTIAL residuals elsewhere** (docs/PARKED.md + in-source comments, intrinsic --
    low ROI): the v294-v322 list (0x43f2b0, 0x43fc50, 0x43e010, 0x43f940, 0x439d00, 0x43a4b0,
    0x43ad00, 0x43eec0, 0x4393d0, 0x439240, 0x43a140, 0x4396c0, 0x43c860, 0x43a8b0, 0x43ce10,
    0x43b240, 0x43ccc0, 0x43ae20, 0x43c410, 0x4549e0/0x454a60, 0x4606d0, 0x460d40, 0x40a4e0,
    0x409e70/0x43d820, 0x40d170, 0x40d340, 0x4216f0, 0x416460, 0x428400) plus v323's 0x43bb00 (dead-index-guard class,
    closable only if that intrinsic class ever cracks) and 0x45a880/0x45aa50 (TU-context /Og
    class -- closable, see item 2b) and the carried probe parks (0x45ca10 UIResources.cpp,
    0x470c30/0x46c480 CrtLeaves.cpp, 0x445910 NetResource.cpp, 0x45cdf0 WorldBoardMaybe.cpp,
    0x44d870 PeerTrainSlotQueueMaybe.cpp), plus v325's 0x40bbd0 and v326's 0x44a250 (VC5 /Og
    block-layout class, see item 2h) and v327's 0x44ce10 and v328's 0x44c3a0 (VC5 /Og
    trace-layout/const-allocation class, see item 2h), plus v329's 0x44cb10/0x44d630
    (loop-entry-guard/LEA scheduling-swap + reg coin-flip classes, see item 2h) and 0x422ec0
    (AnimEffectObj0x477a90 ctor, /Og case-'R' register-cluster class), plus v330's 0x45bbc0
    (Ddraw_HResultToString, VC5 two-level-table cluster-merge class), plus v334's 0x456150
    (UpdateDirtyTiles, stacked sete-prologue/this-swap/induction-var/zero-reg classes,
    see the v334 block), plus v335's 0x462e90 (LocoWinMain, stacked
    sete/zero-reg/residency/lea-swap classes, see the v335 block), plus the
    `LocoBitmap.cpp` blit family (item 6).
## ⏭ NEXT SESSION PICKUP (v578 — 2026-08-01 — **the unlinked-call defect is now findable
statically, and there are 18 CONFIRMED ones left. The next session is the view-struct audit**):

### ⬅ OBJECTIVE (user-directed): AUDIT EVERY TU-LOCAL VIEW STRUCT AND FIX THEM OUTRIGHT

The user's instruction, verbatim: *"we should audit all of the views and just fix them outright,
even if the exact bytematch regresses."* That is this session's whole job, and
**`tools/lint_unlinked.py` (new in v577) hands you the worklist** — no discovery phase needed.

**The 18 CONFIRMED unlinked call sites** (`tools/lint_unlinked.py`, run `progress.py` first).
13 distinct defects, heavily concentrated in the TRAIN subsystem, which is very likely why train
gameplay is still unstable:

| emitted (undefined) spelling | real body |
|---|---|
| `ResetAllTiles@WorldBoardPartial` | 0x454fe0, src/WorldBoardMaybe.cpp |
| `GetPlaneASlotMaybe@WorldBoardSlotPartial` | 0x455620, src/WorldBoardMaybe.cpp |
| `MarkRectDirty@WorldBoardSlotPartial` | 0x455840, src/WorldBoardMaybe.cpp |
| `CanReverseDirectionMaybe@PeerTrainNodeTrackPartial` | 0x44d630, src/PeerTrainNode.cpp |
| `TryTransitionModeMaybe@PeerTrainNodeTrackPartial` | 0x44d5e0, src/PeerTrainNode.cpp |
| `ReverseDirectionGuardedMaybe@PeerTrainNodePartial` | 0x44caf0, src/PeerTrainNode.cpp |
| `FreeQueuedTrainCarSlots@PeerTrainSlotQueueEdgePartial` | 0x44e800, src/PeerTrainSlotQueueMaybe.cpp |
| `CheckAnchorCrossedViewportMaybe@NameAnchorReversePartial` | 0x40cc20, src/NameAnchorMaybe.cpp |
| `CheckAnchorCrossedTileBoundMaybe@NameAnchorReversePartial` | 0x40cc90, src/NameAnchorMaybe.cpp |
| `ComputeTop/Bottom/Left/RightEdgePlacement@NetSessionEventQueueEdge` | 0x41d980/0x41d950/0x41d920/0x41d8f0, src/NetSessionEventQueue.cpp |
| `IsAtMatchingBoardEdgeMaybe@BigObjEdgeView0x4553e0` | 0x44bdb0, src/CursorDesc.cpp (itself still a view!) |
| `PostBag_PackDecalKind@PostBagCacheBundle` | 0x445910, src/NetResource.cpp |

Plus **8 UNCONFIRMED** rows the tool prints separately — those sit in non-byte-exact functions
where the offset may have drifted, so **verify each against the disassembly before acting**
(CODEGEN #217 explains the tier and shows the one false positive that motivated it).

**Method that worked five times in v577** (CODEGEN #218): the fix is almost always to delete the
view and use the REAL shared class — and before believing any "⛔ load-bearing / do NOT re-run"
comment defending a view, **re-read its PREMISE, not just its price**. Every one v577 opened had
decayed: the header already declared the method, the TU already had the `#include`, or the named
victim had already lost its EXACT to something else. Per CODEGEN #219 and the user's directive,
when the correct model does cost bytes, **land it and record the number in a comment**.

**Then re-run the port and read `loco/stub_calls.log`** — still the only oracle for a defect
`lint_unlinked` cannot reach (vtable-slot stubs are DIR32 in `.rdata`, not REL32 calls; the tool
does not decode those yet — a worthwhile extension).

### ⚠ TWO USER-REPORTED CRASHES TO CONFIRM (both reported late in v577, NOT yet re-tested)

1. **Closing the toolbox inconsistently crashes, specifically after placing objects.** Still open.
   v576 fixed a close-path alias blind and v577 did not touch it.
2. **Saving the map crashes.** ⭐ **BELIEVED FIXED IN v577 — ask the user to re-test first.** Root
   cause was identified from the tracked `backtrace.txt`/`backtrace2.txt`: both faults are
   `movb $5,(%ebx)` with EBX a small integer (0x1a8, 0xdf), which is one of the nine `*pOut = 5;`
   writes in `WorldBoardPartial::CaptureBoardToBitmap` (port 0x481e70). `pOut = pBitmap->pPixels`
   was garbage because `CreateAndFill` resolved to a gen_stubs stub that returns 0 and never
   allocates — the `LocoBitmapCaptureView0x457080` defect, retired in v577.

### What v577 did

0. **EXACT 174672 B / 833 funcs** (session start 175190 / 835 — **−518 B / −2 funcs, deliberately**,
   see CODEGEN #219), PARTIAL 217157 / 386, idiom debt 6 (0 untagged), naming debt 892, zero
   `COMPILE FAILED`, `lint_alias` 0, `lint_ghidra_sync` 1452 sites / 0 UNTAGGED (ALIAS=24, OK=1362,
   TODO=66), `LINK OK`, `PORT LINK OK`. **Runtime stub frontier 5 rows → 2**; stub symbols
   **248 → 237** (code stubs 99 → 88).
1. ⭐ **The train-depot crash the user reported is FIXED**, and it was two stacked defects, both
   the v576 shape: `AnimDescRefObj0x477488::_v13()` was a declared-only placeholder for vtable
   slot 13 whose real body (0x405e20 `SetCategoryIfPrintable`, already EXACT) was modelled
   SEPARATELY as a non-virtual on the same class — CLAUDE.md's own "a vtable slot and a named
   method at the same address are ONE function" hazard, caught for the second time. The stub in
   the slot is `void*(void)`: it popped 0 where every call site pushes 4, so
   `PeerTrainSlotQueueMaybe::SpawnOrAssignRandomTrain`'s slot-13 dispatch unbalanced the stack.
   Unifying them was priced −1005 B (⛔ "do NOT re-run") and actually measured **+270 B / +1 func**.
   Second: `CarNetObjAnchorPartial` re-declared `ResolveBothAnchorsToPointMaybe`, hiding the
   inherited `CarNetObj` one that `src/CarNetObj.h` had deliberately placed on the shared class
   for exactly this reason — PeerTrainNode's ctor therefore seated neither anchor. Free.
2. ⭐ **`tools/lint_unlinked.py`** — the address-keyed sweep v577's pickup asked for. CODEGEN #217.
3. **Five view structs retired**, all redundant AND live defects (CODEGEN #218): the three
   `TickStationClockChimeMaybe` spellings + `UIResourcesView0x447400` (free),
   `Ddraw_RecenterViewportOffsetMaybe`'s three (−484 B, landed), `ActivateEligibleEntriesMaybe`'s
   two (−152 B, landed), `LocoBitmapCaptureView0x457080` (**+951 B** — retiring it refunded
   0x457ce0 the exact the Ddraw fold had cost).
4. **Three more stale ⛔ prices cashed** (the pattern is now four sessions old): v340's "ANY new
   method declaration in UIResources.h rotates DPlaySessionMgr.cpp" — free; the `_v13` −1005 B;
   the LocoBitmap.h-include price whose premise (that the TU lacked the include) was false.

### Still open

- **`??1CursorDesc@@UAE@XZ`** is the last non-view stub row, priced −2792 B on 2026-07-31. Per the
  v560 directive AND CODEGEN #219 it should be **re-measured and then landed**, not argued about.
- `GetPlaneASlotGridPosMaybe` (0x4557c0) is a CONFIRMED-adjacent view alias v577 left alone: the
  clean fix wants `TileGridPos` (currently in `src/TilePlacedObj.h`) visible from
  `src/WorldBoardMaybe.h`, i.e. a small header restructure rather than a one-line move.
- `tools/datastubs.py` still reports **17 live rows**, 8 zero-read VTABLES; `g_vtable0x478520` is
  the cheapest (one slot, 0x454db0) but needs `WorldBoardPartial` to get a real virtual dtor at
  +0x0 where the header models `pad`. `src/PeerTrainSlotQueueMaybe.h` still has no include guard.
  `src/EffectSpawner.h`'s migration note: ONE TU-local view left, `BuildToolCursorWnd.cpp`'s
  `WorldDirtyRectCollectionMaybe`. ⛔ **TITLEBAR WORK IS DONE** (v575). All other standing rules
  from v528–v576 stand untouched.

**Reproduce first:** `tools/progress.py` → **174672 exact / 833 funcs**, PARTIAL 217157/386, idiom
debt 6 (0 untagged), naming debt 892, **zero `COMPILE FAILED`**. Then `tools/link_check.sh` →
**LINK OK**; `tools/build_port.sh` → **PORT LINK OK** (`gen_stubs_port`: 237 symbols, 88 code /
149 data, 0 unplaced). Then `lint_alias.py` → **0**, `lint_ghidra_sync.py` → 1452 / 0 UNTAGGED,
`tools/lint_unlinked.py` → **18 CONFIRMED + 8 UNCONFIRMED**, `tools/datastubs.py` → 17 live.
Confirm `git config core.hooksPath` = `tools/githooks` and that both `toolchain/vc50sp3/` and
`toolchain/vc50/` exist.

**Run the port to the WORLD, open the toolbox, pick a tool, place an object:**
```
LOCO_PORT_CLICK="120:skip;300:enter;700:78,38;1000:30,45;1300:500,400;1600:560,430" \
  LOCO_PORT_DUMP=1500 /opt/homebrew/bin/timeout 90 ./run_diag.sh
```
Expect the `TOOLBAR: kind ...` block with `avail=1`/non-NULL `shadow=`, `pMenuListHead` NON-zero,
`icon: node=… hit=1`, then world clicks committing. ⚠ **`LOCO_PORT_CLICK` has NO key-press
vocabulary** — only `x,y` and registered hit-rect names (`skip`/`enter`/`esc` are SplashWnd rects,
not global keys), so an Esc-driven close CANNOT be scripted and **neither of the two open crashes
above is reproducible without the user**. ⚠ Traces go to `loco/port_trace.log`, **NOT stdout**;
stub hits land in `loco/stub_calls.log` (read it after EVERY run). Frames land in
`loco/port_frame%04u.bmp`. ⚠ `run_diag.sh` has **no timeout** — wrap it in
`/opt/homebrew/bin/timeout` (exit 124 is the expected healthy outcome) and `pkill -f
Loco-port.exe; pkill -f wineserver` FIRST and AFTER. ⚠ `tools/build_port.sh` caches stubs —
**`rm -f link/gen_stubs_port.obj` after ANY mangled-name change**. ⚠ `screencapture` is blocked by
macOS permissions here, so host-side screenshots need the user.
⚠ The Bash tool's cwd PERSISTS between calls, and its shell is **zsh, which does NOT word-split**
unquoted expansions and **aborts a `set -e` script on a glob that matches nothing** (use
`find -delete`, not `rm -f loco/port_frame*.bmp`). CODEGEN #170.
