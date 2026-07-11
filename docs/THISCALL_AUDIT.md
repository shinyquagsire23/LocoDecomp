# `__thiscall` audit — functions whose call sites pass `this` in ecx

Generated in **v362** by the lever-3 sweep (see `docs/CODEGEN.md`, "A dead `mov ecx` before a
call is a `this` pass"). This is a **work queue**, not a finding list: every row is a strong
lead that still needs its own call-site read before you act on it.

## Method (reproduce in ~2 min, no compiling)

1. `objdump -d -M intel loco/Loco.exe > full.asm`
2. For every `call <imm>` into the app region (`0x401000`–`0x463800`), check whether the
   IMMEDIATELY preceding instruction writes `ecx`. Tally per target.
3. Keep targets with ≥2 ecx-passing sites and an ecx fraction ≥80%.
4. Ask Ghidra for each target's name / calling convention / namespace; drop the ones already
   typed `__thiscall`.

Step 2 deliberately looks only at the immediately-preceding instruction. Widening the window
buys almost nothing and costs a lot of precision — an ecx write three instructions back is far
more often a `rep movs` counter or an unrelated load than a `this` setup.

## Why the residue is real signal

A callee that ignores `this` **cannot be identified from its own bytes** — `__thiscall` with N
stack args and `__stdcall` with N stack args emit the same `ret N`. Ghidra's analyzer therefore
types such a function `__stdcall`, and every transcription of its CALLERS then comes up one
`mov ecx` short. Only the call sites carry the evidence.

The two conventions in the table below behave differently:

- **`__stdcall`** — the body never reads ecx. These are the true lever-3 finds: Ghidra had no
  way to know, and the ecx pass is invisible in the callee. Highest value.
- **`__fastcall`** — the body DOES read ecx, and Ghidra inferred a first register parameter.
  VC5 does not emit `__fastcall` for ordinary app code, so these are almost certainly
  `__thiscall` members whose `this` is currently exposed as an explicit `param_1`. Retyping
  them is mostly a decompiler-output-quality win (typed `this->field` instead of `param_1[N]`),
  not usually a byte-match one. ⚠ When converting, do NOT repeat that pointer in the new
  prototype's explicit param list — see `docs/GHIDRA_RECIPES.md`.

## False positives to expect

An ecx-passing call site whose callee is ALREADY a member proves nothing; and a caller that
does NOT set up ecx is not counter-evidence, because MSVC omits the setup wherever `this` is
still live in ecx from the caller's own prologue. Confirmed false positives while building this
table: `0x404f60` and `0x41ac10` (both call `g_UIResources` members already modelled correctly
— their apparent gaps are alignment artifacts inside large diff regions), plus v361's own
`0x42b050` / `0x40d170`.

Because of the second rule, an ecx fraction well below 100% is normal and is NOT a reason to
skip a row: `0x41d5c0` was a genuine member at 1/2.

## Fixed in v362

| addr | class | result |
|---|---|---|
| `0x414ef0` | `PopupWndBase` | was `static` in src — **+3 exact funcs, +326 B** (closed `OnMouseMove` 0x414a80 and `OnPaint` 0x414ae0) |
| `0x41d5c0` | `NetSessionEventQueue` | was a free `__stdcall` in src; caller `PlaceEdgeLinksAndFlush` gained its missing instruction (both still parked) |
| `0x447930` | `UIResources` | Ghidra retyped (src already modelled it as a member) |
| `0x447a70` | `UIResources` | Ghidra retyped |
| `0x445170` | `PostBagCacheBundle` | Ghidra retyped; `src/PostBag.h`'s "a free function (no `this`/cache)" claim corrected |

`0x41d5c0` does not appear in the table below: it has only 2 call sites, so the ≥2-ecx-site
filter excluded it. It was found by the complementary scan (lever-3 gaps in the aligner output
of already-transcribed near-misses) — run BOTH sweeps, they surface different rows.

## Queue CLOSED in v364 — all 40 rows retyped `__thiscall` in Ghidra

Every row below was confirmed from its call sites and retyped (`save_program` done, all read
back). **No byte movement, exactly as this doc predicted** — see "Why no bytes moved" below.
The table is kept as the evidence record; treat it as history, not as work.

Three rows needed more than a convention flip:

| addr | what was special |
|---|---|
| `0x448970` | singleton `0x4a9910` identified as the **screensaver / attract-mode** controller (see below) |
| `0x434800` | ends `ret 0x4` — one stack param Ghidra had dropped entirely; restored as `bUnusedFlagMaybe` |
| `0x4316f0` | ends `ret 0x4` — same; restored as `nModeMaybe` |

`0x434800`'s stack arg is a **dead-but-real parameter**: the body never reads it, but its two
call sites push `0` and `1` and the `ret 0x4` proves the push is real. Same class as the VC5
`FUN_00463600` case in `docs/GHIDRA_RECIPES.md`. `0x409970` was skipped — it is a bare
`jmp 0x409980` thunk, i.e. an aliased entry to the row below it, not its own function.

### The `0x4a9910` singleton = screensaver / attract mode

Its six members (`0x448040` ctor, `0x4480c0`, `0x448120`, `0x448350`, `0x4484a0`, `0x448970`)
form one contiguous `0x448xxx` cluster — a TU. `0x4480c0` gates on the `-s` cmdline flag,
reads ini `[ScreenSaver] Sound`, and plays `\video\music.wav`; `0x448120` is an attract-mode
tick that re-randomizes each of the 4 peer train slots' cars every 0x800 ticks; `0x448350`
enters the demo session (skip-setup-wizard, tick sleep 0x32, `DPlaySessionMgr::SetMode(1)`).
`src/Main.cpp` still models it as `&DWORD_004a9910` handed to free functions — that is the
poison this sweep exists to remove, and it is the natural next cleanup.

**`0x4484a0` had NO Ghidra function at all** — the bytes were never disassembled, so an ecx-passing
call target was invisible to every xref/namespace sweep. Created in v364 (`DisassembleCommand`
then `CreateFunctionCmd`; a bare `CreateFunctionCmd` on undisassembled bytes silently yields a
**1-byte body** — disassemble FIRST). It is 612 bytes (`0x4484a0-0x4487ce`), `ret 0x10`, and reads
`[esp+8]` as a message code against `0x1c` (`WM_ACTIVATEAPP`): the screensaver's window
procedure, typed `int __thiscall (HWND, uint uMsg, uint wParam, int lParam)`.

### Sweep (3) — call targets with NO Ghidra function (new in v364)

`0x4484a0` prompted a general check, and it is worth re-running after any analysis change:
diff every direct `call <imm>` target in the app region against the set of Ghidra function
ENTRY POINTS (`getFunctionManager().getFunctions(true)`). Of 925 app-region call targets, **3
had no function**: `0x4484a0`, plus `0x45e210` (490 B, `__cdecl` 1 byte arg) and `0x45e400`
(132 B, void) — both in the shutdown/quit cluster reached from `LocoWinMain`'s neighbourhood.
All three were created. That is ~1234 bytes of app code that no xref, namespace, or
address-boxing sweep could previously see, and which `progress.py`'s Ghidra-extent basis was
silently undercounting.

Compare ENTRY POINTS, but confirm each hit with `get_function_by_address` before creating —
that endpoint reports the CONTAINING function, so it distinguishes "genuinely absent" from
"already inside a neighbour's body". Note the converse hole this sweep does NOT cover: an
address reached only by a JUMP (e.g. `0x462dc3`, a tail arm sitting just past a `ret 0x10`)
is not a call target and so never appears here.

### Why no bytes moved

`src/` already spells these callee-ignores-`this` cases as **`__fastcall` free functions taking
an explicit pointer** (`void __fastcall GameNet_PostAttemptJoin(DPlaySessionMgr *pMgr)`, mangled
`?...@@YIX...@Z`). MSVC passes a `__fastcall`'s single pointer param in ECX with no stack slot —
**byte-identical to `__thiscall` on both the caller and callee side**. So that spelling was
already correct; only Ghidra's view was wrong.

This is the key distinction from v362's `0x414ef0`: **`static` breaks the match, `__fastcall`
does not.** A `static` member emits no ecx setup at all, so its callers come up one instruction
short. A `__fastcall` free function emits exactly the `mov ecx` the original has. When you find
a lever-3 row, check which of the two spellings `src/` uses before expecting bytes.

### Evidence table

`ecx` = ecx-passing sites / total sites. `__stdcall` rows first, then by evidence strength.

| addr | ecx/sites | Ghidra conv | namespace | name | ecx value at first site |
|---|---|---|---|---|---|
| `0x456c60` | 2/2 | `__stdcall` | `WorldBoardMaybe` | `FUN_00456c60` | `ecx,esi` |
| `0x45d810` | 2/2 | `__stdcall` | `TrackGraph` | `FUN_0045d810` | `ecx,esi` |
| `0x45d980` | 2/2 | `__stdcall` | `TrackGraph` | `FUN_0045d980` | `ecx,ebp` |
| `0x448970` | 2/2 | `__stdcall` | `Global` | `FUN_00448970` | `ecx,0x4a9910` |
| `0x4255f0` | 26/26 | `__fastcall` | `CursorDesc` | `IsItemAvailableMaybe` | `ecx,edi` |
| `0x43efa0` | 13/16 | `__fastcall` | `GameNet` | `GameNet_ResetProvidersAndPostTeardown` | `ecx,DWORD PTR ds:0x4fd3ac` |
| `0x449410` | 11/13 | `__fastcall` | `Global` | `Lock0x449410` | `ecx,edi` |
| `0x409980` | 8/8 | `__fastcall` | `Global` | `FUN_00409980_DrawsText` | `ecx,esi` |
| `0x409360` | 6/6 | `__fastcall` | `Global` | `FUN_00409360_CopyRect_OffsetRect` | `ecx,esi` |
| `0x44d630` | 6/6 | `__fastcall` | `PeerTrainNodeTickPartial` | `CanReverseDirectionMaybe` | `ecx,ebp` |
| `0x409970` | 6/6 | `__fastcall` | `Global` | `FUN_00409980_DrawsText` | `ecx,DWORD PTR [ecx+0x220]` |
| `0x401810` | 5/5 | `__fastcall` | `PostBagFileCache` | `FUN_401810_GetCountDiv24` | `ecx,DWORD PTR ds:0x4fd3b4` |
| `0x454fa0` | 5/5 | `__fastcall` | `Ddraw` | `Ddraw_RecenterViewportOffsetMaybe` | `ecx,0x4aad08` |
| `0x43f070` | 5/5 | `__fastcall` | `GameNet` | `GameNet_PostResetConnection` | `ecx,DWORD PTR ds:0x4fd3ac` |
| `0x43f030` | 5/5 | `__fastcall` | `GameNet` | `GameNet_PostPrepareInternet` | `ecx,DWORD PTR ds:0x4fd3ac` |
| `0x410d20` | 5/5 | `__fastcall` | `PlacementCursorMaybe` | `FUN_00410d20` | `ecx,esi` |
| `0x4544a0` | 5/5 | `__fastcall` | `Global` | `FUN_004544a0` | `ecx,DWORD PTR [esi+0x210]` |
| `0x409770` | 4/4 | `__fastcall` | `Global` | `FUN_00409770_DrawsText` | `ecx,esi` |
| `0x40b880` | 4/5 | `__fastcall` | `BigObj` | `BigObj_TryMatchSocketPointMaybe` | `ecx,esi` |
| `0x422010` | 3/3 | `__fastcall` | `SplashWnd` | `FUN_00422010` | `ecx,esi` |
| `0x433160` | 3/3 | `__fastcall` | `Global` | `FUN_00433160` | `ecx,esi` |
| `0x441b40` | 3/3 | `__fastcall` | `NetSetupWnd` | `FUN_00441b40` | `ecx,esi` |
| `0x434800` | 2/2 | `__fastcall` | `DecorObjMgrMaybe` | `FUN_00434800` | `ecx,0x485448` |
| `0x42fdf0` | 2/2 | `__fastcall` | `Global` | `FUN_0042fdf0` | `ecx,DWORD PTR ds:0x4fd37c` |
| `0x430c20` | 2/2 | `__fastcall` | `Global` | `FUN_00430c20` | `ecx,DWORD PTR ds:0x4fd388` |
| `0x457320` | 2/2 | `__fastcall` | `WorldBoardMaybe` | `FUN_00457320` | `ecx,0x4aad08` |
| `0x43f000` | 2/2 | `__fastcall` | `GameNet` | `GameNet_PostAttemptJoin` | `ecx,DWORD PTR ds:0x4fd3ac` |
| `0x423d00` | 2/2 | `__fastcall` | `EffectSpawner` | `EffectSpawner_ClearBothListsMaybe` | `ecx,0x4fd220` |
| `0x423d70` | 2/2 | `__fastcall` | `Global` | `FUN_00423d70` | `ecx,0x4fd220` |
| `0x421ae0` | 2/2 | `__fastcall` | `SplashWnd` | `FUN_00421ae0` | `ecx,esi` |
| `0x422820` | 2/2 | `__fastcall` | `Global` | `FUN_00422820` | `ecx,esi` |
| `0x4532a0` | 2/2 | `__fastcall` | `Global` | `Profile_SavePlayerUserFile` | `ecx,DWORD PTR ds:0x4aa4a8` |
| `0x423560` | 2/2 | `__fastcall` | `Global` | `FUN_00423560` | `ecx,eax` |
| `0x440310` | 2/2 | `__fastcall` | `GameNet` | `GameNet_BroadcastSlotEnabled` | `ecx,DWORD PTR ds:0x4fd3ac` |
| `0x4316f0` | 2/2 | `__fastcall` | `Global` | `FUN_004316f0` | `ecx,esi` |
| `0x4370f0` | 2/2 | `__fastcall` | `BuildToolCursorWnd` | `FUN_004370f0` | `ecx,esi` |
| `0x41e120` | 2/2 | `__fastcall` | `Global` | `FUN_0041e120` | `ecx,0x4a9990` |
| `0x4530c0` | 2/2 | `__fastcall` | `Global` | `Config_LoadPlayerIdentityFileMaybe` | `ecx,ebp` |
| `0x45a400` | 2/2 | `__fastcall` | `WorldActionCursor` | `RefreshVariantMenuIconsMaybe` | `ecx,esi` |
| `0x45a480` | 2/2 | `__fastcall` | `WorldActionCursor` | `RefreshCategoryMenuIconsMaybe` | `ecx,esi` |
