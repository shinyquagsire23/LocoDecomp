# Prior session pickups (archive)

Newest last. Each block is moved here verbatim from CLAUDE.md's `⏭ NEXT SESSION PICKUP` section
when a session bumps the pickup version, per the SESSION PROTOCOL in CLAUDE.md.

## v7 — 2026-07-11 — AVI-dispatcher gap closed

**Match count unchanged from v6 (39/48)** — not re-verified this session (no probe-file
changes); re-run `tools/cc.sh` on the 5 probes first thing next session per protocol.

**Closed this session's assigned pickup item #2** (defining the two AVI-dispatcher gaps):
both ranges were plain `create_function` misses, not jump tables — each undefined region
starts a few bytes of `0x90` padding after the previous function's `ret`, found by walking
forward with `objdump --start-address=... --stop-address=...` against `loco/Loco.exe` and
calling `create_function` (`POST /create_function?program=Loco.exe` body
`{"address":"0x..."}` — note `address` not `function_address`, and `rename_function`/
`set_plate_comment` want `old_name`/`address` resp., discovered by trial — add to the
CLAUDE.md write-recipes list below). 6 new functions defined, all one WndProc-family
cluster driving a splash/video-sequence state machine (`this+0xe8`=state):
`SplashVideoSubclassProc` (0x420b20), `Splash_OnKeyDownMaybe` (0x420bb0), `Splash_Redraw
ParentThunkMaybe` (0x420e90), **`Splash_DlgProc`** (0x420ec0 — the dispatcher, owns all 3
known video-launch sites: state 0→IgSpin.avi, state 1→ini-configured intro video),
**`Splash_SetStateMaybe`** (0x4208f0 — the state-transition fn), `Splash_OnPaintMaybe`
(0x421be0), `Splash_OnIdlePumpMaybe` (0x421eb0 — kicks off the 3rd video, legoSpin.avi,
when idle+state==0). Full disasm/decompile notes: `docs/subsystems.md`'s AVI/MCI section
(rewritten in place, "Gap CLOSED" callout with a per-function table). All renamed +
plate-commented in Ghidra, `save_program`'d.

**Free resolution of two other standing open items** (found while reading
`Splash_SetStateMaybe`'s body, not separately chased): (1) `DSound_GetOrCreateManager`'s
caller — confirmed `0x4208f0` state 7 w/ prior state 0, the actual first-sound-played
trigger; (2) the previously-unlinked `"%svideo\music.wav"` string — read+`PlaySoundA`'d in
that same function when leaving state 0 or 1.

**Not done this session** (explicitly deferred, bigger scope than the one pickup item
tackled): the splash controller's `this` struct itself is still unnamed/unmodeled (a
vtable-bearing object, fields run to at least `+0x220`, distinct from the `DAT_004aa4a0`
app singleton) — next natural step before this subsystem is source-transcribable.
`Splash_OnIdlePumpMaybe`'s own caller not traced (likely `WM_PAINT`/`WM_TIMER` in a sibling
proc). `Splash_OnKeyDownMaybe` (0x420bb0, 722B) and `Splash_OnPaintMaybe`'s `0x42b050`
calls read only shallowly — full logic not transcribed.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Model the splash controller struct** (name TBD — `SplashWnd`/`MenuWnd`-ish) now that
   its 6 methods are named: pin `sizeof` from its `operator new` call site (not found yet —
   trace who constructs it), define the struct in Ghidra, re-`this`-type all 6 methods.
3. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open, none touched this session):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`), and check whether the 5-member near-identical `"%s%s.dat"`-building
     loader family (`0x424bf0`/`0x41e6e0`/`0x436490`/`0x40e690`/`0x44b290`) shares a base
     class/vtable — good next TU-boundary evidence.
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
4. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## v9 — 2026-07-11 — all 8 singleton windows named

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any changes, per protocol.

**Closed this session's assigned pickup item** (name the 7 sibling `WindowBase` singletons
from `FUN_00406f90`): done, all 8. Full writeup: `docs/subsystems.md`'s "The 8 singleton
windows — all named" section. Highlights:
- **Key unlock**: the 8 ctors' `resourceId` param feeds `LoadStringA` for the `WNDCLASSA`
  class name — extracting the raw `RT_STRING` block (ids 496–511) from `Loco.exe`'s `.rsrc`
  gives every class's LITERAL ORIGINAL NAME directly, no behavioral guessing needed. This is
  now the standing first-move for any future unnamed `WindowBase`/`PopupWndBase` singleton.
  2 more class-name strings exist unclaimed in the same block (`NETSETUPCLASS`=502,
  `APPLSETUPCLASS`=505) — at least 2 more singletons of this family exist, constructed
  on-demand elsewhere, good next target.
- **Second unlock**: there are TWO base window classes, not one — `WindowBase`
  (`0x425870`/`0x425b70`, used by `SplashWnd`/`MailWnd`/`MapWnd`/`AlbumCardWnd`/
  `EditCardWnd`) and a new one, **`PopupWndBase`** (ctor `0x413ab0`, `Create` `0x413de0`,
  sizeof `0x118`, used by `MsgBoxWnd`/`TutorialWnd`/`CreditsWnd`) — confirmed independent
  (never calls `WindowBase::WindowBase`, sets its own vtable). `PopupWndBase` windows are
  DirectDraw-composited overlays (`Blt`'d directly onto the primary surface) rather than
  normal GDI child windows.
- **All 8 named**: `SplashWnd`(front-end splash/video), `MailWnd`(main menu hub, 8
  icon-buttons → app screen-states), `MapWnd`(layout/town picker), `MsgBoxWnd`(surprising —
  behaves like an interactive build/cursor-tool overlay despite the class name, flagged
  open), `AlbumCardWnd`(card album), `EditCardWnd`(multiplayer roster EDIT control +
  cached help bitmap), `TutorialWnd`(largest, 12KB — the central gameplay/world view,
  refuted the DirectPlay hypothesis), `CreditsWnd`(animated-image DDraw overlay, likely
  the LEGO/credits animation). Method: 6 parallel read-only research agents, one per
  class/pair, cross-checked against every previously-documented subsystem before Ghidra
  writes were applied by hand in the main thread (same protocol as the v6 Phase 3 fan-out).
- **Ghidra state**: `PopupWndBase` + 7 leaf structs created (sizes pinned from `operator
  new` call sites), all 8 ctors + 7 distinct `Create` fns `this`-typed/namespaced (`MapWnd`/
  `AlbumCardWnd` share one `Create`, `0x402520`, confirmed generic/no override), all 8
  globals renamed `g_p<Class>Wnd` + typed + plate-commented, bootstrap fn itself renamed
  `Bootstrap_ConstructSingletonWindows`. `save_program`'d.
- **New write-recipe confirmed**: `set_global` (POST, body keys `address`/`type_name`/
  `name`/`plate_comment`) is a single-transaction global rename+retype+comment — the plain
  `rename_data` endpoint rejects Hungarian-prefixed names (e.g. `g_pFoo`) unless the type is
  already a pointer, `set_global` sidesteps that by validating name+type together.

**Not done this session** (explicitly deferred): field-level struct modeling is partial —
`TutorialWnd`'s 200-entry item array's internal per-entry layout, `WindowBase`'s and
`PopupWndBase`'s remaining `Unk0xNN`/`field_0xNN` gaps, and most of each of the 7 classes'
non-ctor/Create methods (button handlers, refresh functions, etc. — named in
`docs/subsystems.md`'s per-class bullets but not yet renamed in Ghidra) are all open.
`MsgBoxWnd`'s class-name-vs-behavior contradiction is unresolved. `SplashWnd::OnIdlePump`'s
own caller still untraced.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Rename the highest-value non-ctor/Create methods** for the 7 newly-named classes (all
   addresses already in `docs/subsystems.md`'s per-class bullets) — e.g. `TutorialWnd`'s
   `FUN_0044f560` (the cross-subsystem notify hub, called from many unrelated places) and
   exit handler `0x450ae0`; `EditCardWnd`'s `0x416e00`/`0x41a0e0`; `MailWnd`'s
   `0x42e150`/`0x42de70`/`0x42f810`. This is `set_function_this_type`-style grinding, same
   as `SplashWnd` got in v8, now spread across 7 classes instead of 1.
3. **Chase the 2 unclaimed `WindowBase`-family class strings**: `NETSETUPCLASS` (502) and
   `APPLSETUPCLASS` (505) — find their construction sites (search for `LoadStringA`/
   `FUN_00447330` calls with those literal resourceIds, or `RegisterClassA` xrefs to their
   class-name strings once extracted).
4. **Resolve the `MsgBoxWnd` naming contradiction**: read its full method set to determine
   whether "message box" is a dev-vocabulary artifact (an old name that stuck) or whether
   the DDraw-cursor-tool-overlay behavior actually belongs to a DIFFERENT object this
   session conflated with it — re-verify the `DAT_00485258`/`0x436b20` pairing from scratch
   if the contradiction doesn't resolve cleanly.
5. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open, none touched this session):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`), and check whether the 5-member near-identical `"%s%s.dat"`-building
     loader family (`0x424bf0`/`0x41e6e0`/`0x436490`/`0x40e690`/`0x44b290`) shares a base
     class/vtable — good next TU-boundary evidence.
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
6. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## ⏭ NEXT SESSION PICKUP (v11 — 2026-07-11 — Enter/Esc toggle traced (corrects v9), 3 more WindowBase vtable gaps closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

**Closed pickup item 2** (trace the Enter/Esc-prompt toggle logic). Full writeup:
`docs/subsystems.md`'s "Fourth session" section. Highlights:
- **Corrects v9's "Enter surfaces NetSetupWnd, Esc surfaces ApplSetupWnd" reading — it's
  wrong.** Real mechanism, read from `SplashWnd::OnKeyDown` (`0x420bb0`) disasm directly
  (cross-checked against `objdump` since Ghidra's decompile prints the two branches out of
  address order): **Esc unconditionally quits the app** (dispatches app-state
  `FUN_00408130(0xa)` — DSound channel release + `WM_CLOSE` post — after a no-op/cancel
  call, it never shows `ApplSetupWnd`). **Enter** validates a player-name string from an
  edit control, then via `FUN_00422660` branches on flags in an untyped global config
  struct (`DAT_004fd3a8`) to `SplashWnd::SetState(2)` (show `NetSetupWnd`),
  `SetState(4/5)` (show `ApplSetupWnd` directly, skipping NetSetup), or — if already
  configured — skips setup UI and dispatches app-state `FUN_00408130(1)` (start the game).
  `SplashWnd::SetState` (0x4208f0) is the real Show/Hide toggle, confirmed via its own
  vtable slots `+4`(Hide)/`+8`(Show) on the two child window objects.
- **`FUN_00408130` (`0x408130`) identified as the top-level app-state dispatcher**
  (`DAT_004851f4` = current state), states 1–0xa covering start-game/MailWnd/AlbumCardWnd/
  EditCardWnd/TutorialWnd/MapWnd/quit — one level above each window's own internal `state`.
  Matches `TutorialWnd::NotifyOrLaunch`'s `FUN_00408130(8)` call from the 3rd session.
- **3 more `create_function` gaps closed** (same plain-analyzer-miss pattern as prior
  sessions), found chasing the Esc branch's vtable `+0x10` call on `WindowBase`'s own
  vtable (`0x477c30`, read via `read_memory`, not guessed): `0x425fd0` (66B, slot `+0xc`),
  `0x426020` (260B, slot `+0x10`), `0x426130` (3B, slot `+0x14`, bare `ret 0x4` no-op).
  This-typed into `WindowBase`, renamed (`RequestModeTransitionFromSourceMaybe`/
  `ScheduleModeTransitionMaybe`/`NoOpVirtualMaybe`), plate-commented, `save_program`'d.
  `ScheduleModeTransitionMaybe` is a **timer-driven mode/rect transition scheduler**
  (unrelated to Show/Hide) touching most of `WindowBase`'s remaining `Unk0xNN` fields in
  one shot (`+0x18/0x1c/0x20/0x24/0x2c/0x30/0x3c/0x50/0x54/0x58/0x5c` all now have confirmed
  read/write sites) — added new field `WindowBase::pSettledTargetMaybe` (`+0x70`, was an
  unnamed gap byte) from this. **Purpose still open**: what visually calls/uses this
  transition (DDraw clipper resize on Alt+Tab? a zoom animation?) isn't traced past the
  Esc-cancel call site.

**Not done this session** (explicitly deferred, still open, carried from v10): the
`InitErrorCode` sub-object semantics inside `NetSetupWnd`/`ApplSetupWnd` still not traced;
`DAT_004fd3a8`'s struct/flags (network/session config, read as gate conditions this session
at `+7`/`+0x18`/`+0x24`/`+0x28`) not named/typed; `FUN_00422660`'s edit-control validation
and the string globals it touches (`DAT_004aa4a8`/`DAT_004a9eec`) not traced;
`FUN_00408130`'s states 2/3/4 bodies not read; `BuildToolCursorWnd`'s `Unk0x120`/
`toolParam2Maybe` still unread; the 2 catalogued engine bugs (`docs/engine-bugs.md`) not
yet reproduced live or transcribed to `src/`.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Trace `ScheduleModeTransitionMaybe`'s real callers** beyond the Esc-cancel site — find
   where `RequestModeTransitionFromSourceMaybe`/`ScheduleModeTransitionMaybe` get called
   with a non-NULL target, and what `Ddraw_RebindWindowClipper`+`FUN_00426eb0` visually do,
   to finally resolve `WindowBase`'s remaining `Unk0xNN` fields with confident names.
3. **Read `Obj0x47851c::InitErrorCode`'s body** (already matched as a Phase-2 leaf,
   `0x454b50`) against the string-id ranges found in v10 (`0x417`-`0x421` for NetSetupWnd,
   `0x429`-`0x42f`/`0x43a`+9 for ApplSetupWnd) to pull the actual error/status message text.
4. **Type `DAT_004fd3a8`** (the network/session config struct gating Enter's NetSetup vs.
   ApplSetup vs. skip-to-game branch) and **read `FUN_00408130`'s states 2/3/4 bodies** to
   round out the app-state-machine picture.
5. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`), and check whether the 5-member near-identical `"%s%s.dat"`-building
     loader family (`0x424bf0`/`0x41e6e0`/`0x436490`/`0x40e690`/`0x44b290`) shares a base
     class/vtable — good next TU-boundary evidence.
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
6. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## ⏭ NEXT SESSION PICKUP (v12 — 2026-07-11 — `ScheduleModeTransitionMaybe` traced: it's a CURSOR-rect transition, not window-mode)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

**Closed pickup item 2** (trace `ScheduleModeTransitionMaybe`'s real callers, resolve what
`Ddraw_RebindWindowClipper`+`FUN_00426eb0` visually do). Full writeup: `docs/subsystems.md`'s
"Fifth session" section. Highlights:
- **Corrects v11's "mode/rect transition, purpose unknown" hypothesis — it's not a window
  fullscreen/windowed-mode system, it's a CUSTOM CURSOR rect-transition system.** Found the
  ONLY non-NULL-target call site in the whole binary (`SplashWnd::OnPaint`, state 7) by a
  raw `.text` scan for the virtual-call idiom `mov REG,[REG2]` + `call [REG+0xc]`/`[REG+0x10]`
  across ALL 6 general-purpose registers — an initial pass restricted to registers ending in
  `x` (eax/ebx/ecx/edx) silently missed every `esi`/`edi`-based hit, including the
  already-known `OnKeyDown` site; widen the register class FIRST next time this pattern comes
  up. `get_xrefs_to` and a full-program decompile-and-grep sweep (~1782 functions) both came
  back empty for this class of question — virtual calls never resolve to a name in Ghidra's
  decompiled C, so neither approach can find indirect-call callers; the raw-disasm idiom scan
  is the only thing that worked.
- **`WindowBase::InitCursorDescriptorsMaybe`** (renamed from `FUN_00425dc0`, called from
  `WindowBase::Create`/`0x425b70`) resolves 3 named CURSOR resources via the same
  lazy-registry mechanism used for window class names (`FUN_00446ea0`/`DAT_004855e8`):
  RT_STRING id `0x1400`=`"cursors\point"`, `0x1403`=`"cursors\anipoint"`,
  `0x1402`=`"cursors\eraser"` — extracted with a hand-rolled PE resource-directory parser
  (no `pefile` in this environment) since no Ghidra REST endpoint exposes `STRINGTABLE`
  content directly. Caches each cursor's descriptor pointer + a rect/size object (its own
  vtable+4 call) into 3 new `WindowBase` field pairs: `pPointCursorRectMaybe`/
  `pPointCursorDescMaybe` (`+0x60`/`+0x64`), `pAnipointCursorRectMaybe`/
  `pAnipointCursorDescMaybe` (`+0x68`/`+0x6c`, the latter previously a raw undefined-byte
  gap), and `pEraserCursorDescMaybe` (`+0x74`) pairing with the ALREADY-NAMED
  `pSettledTargetMaybe` (`+0x70`, v11) — now explained: it's filled with the ERASER cursor's
  rect, so eraser is likely the default/idle cursor and the "50ms once settled, else 120ms"
  timer logic reads as "redraw fast once back at idle." Same function also allocates the
  shared game-cursor DirectDraw surface (`DAT_004fd3cc`) right after — independent
  confirmation this is the cursor subsystem, same one `BuildToolCursorWnd`/
  `RedrawGhostCursor` touch (also `"cursors\..."`-named resources).
- `0x425fd0`/`0x426020` plate comments rewritten to describe the cursor mechanism (not
  window-mode); `SplashWnd::OnPaint`/`OnCreateComplete` (`0x421be0`/`0x4206b0`) commented
  with their exact call args. All `save_program`'d.

**Not done this session** (explicitly deferred, still open, carried from v11): the
`InitErrorCode` sub-object semantics inside `NetSetupWnd`/`ApplSetupWnd` still not traced;
`DAT_004fd3a8`'s struct/flags not named/typed; `FUN_00422660`'s edit-control validation and
the string globals it touches (`DAT_004aa4a8`/`DAT_004a9eec`) not traced; `FUN_00408130`'s
states 2/3/4 bodies not read; `BuildToolCursorWnd`'s `Unk0x120`/`toolParam2Maybe` still
unread; the 2 catalogued engine bugs (`docs/engine-bugs.md`) not yet reproduced live or
transcribed to `src/`.

**New from this session, carried forward:** `FUN_00446840`'s construction logic (builds the
cursor-descriptor object from the resolved path string — what its fields actually mean,
particularly why `RequestModeTransitionFromSourceMaybe` reads `+0x32`/`+0x34`/`+0x160` on it
while `InitCursorDescriptorsMaybe` reads `+0x14`/`+0x16`/`+0x160` on the SAME object type —
two different sub-fields, not yet modeled); the cursor-descriptor object's own vtable
(`+4`, called to produce the "rect" object — what it does); `Ddraw_RebindWindowClipper`+
`FUN_00426eb0` still just "redraw," not traced pixel-level; whether the other 5
`WindowBase`-derived singletons (`MailWnd`/`MapWnd`/`AlbumCardWnd`/`EditCardWnd`/
`NetSetupWnd`/`ApplSetupWnd`) ever trigger a real cursor transition of their own — this
session's idiom-scan cross-reference was keyed off `SplashWnd`'s known address range, not
exhaustive per-singleton.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Read `FUN_00446840`** (constructs the cursor-descriptor object from a resolved path
   string) to finally model that object's struct — resolves the `+0x14`/`+0x16`/`+0x160` vs
   `+0x32`/`+0x34`/`+0x160` field-meaning question above and lets `pXCursorDescMaybe`/
   `pXCursorRectMaybe` graduate from `Maybe` names.
3. **Read `Obj0x47851c::InitErrorCode`'s body** (already matched as a Phase-2 leaf,
   `0x454b50`) against the string-id ranges found in v10 (`0x417`-`0x421` for NetSetupWnd,
   `0x429`-`0x42f`/`0x43a`+9 for ApplSetupWnd) to pull the actual error/status message text.
4. **Type `DAT_004fd3a8`** (the network/session config struct gating Enter's NetSetup vs.
   ApplSetup vs. skip-to-game branch) and **read `FUN_00408130`'s states 2/3/4 bodies** to
   round out the app-state-machine picture.
5. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`), and check whether the 5-member near-identical `"%s%s.dat"`-building
     loader family (`0x424bf0`/`0x41e6e0`/`0x436490`/`0x40e690`/`0x44b290`) shares a base
     class/vtable — good next TU-boundary evidence.
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
6. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## ⏭ NEXT SESSION PICKUP (v13 — 2026-07-12 — `CursorDescMaybe` struct modeled from `FUN_00446840`)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

**Closed pickup item 2** (read `FUN_00446840`, model the cursor-descriptor object's struct).
Full writeup: `docs/subsystems.md`'s "Sixth session" section. Highlights:
- **`FUN_00446840` is a generic RT_STRING-id-keyed resource-object factory, not cursor-specific.**
  Dispatches on `bucket = id >> 10`; each bucket `operator new`s a differently-sized object via a
  bucket-specific constructor. Bucket 5 (ids `0x1400`-`0x17FF`) is the CURSOR range, but buckets
  0/1/14 construct the SAME `0x168`-byte class for other id ranges — it's a generic small-bitmap
  resource holder, cursor use is just its best-understood case so far.
- **Modeled and named that `0x168`-byte class `CursorDescMaybe`** (matches the existing
  `pXCursorDescMaybe` field-naming convention already on `WindowBase`). Read its ctor
  (`0x424af0`), real loader body (`0x424bf0`, now `CursorDescMaybe::LoadMaybe`), destructor
  (`0x424ba0`), and scalar-deleting-destructor (`0x424b40`) to pin the full field layout; built
  the struct via `StructureDataType`/`growStructure`/`replaceAtOffset` (per CLAUDE.md's documented
  recipe — `create_struct`'s REST endpoint ignores offsets). `set_function_this_type`'d all 4
  methods into the new namespace; both consumers (`CursorDescMaybe::LoadMaybe` itself and
  `WindowBase::RequestModeTransitionFromSourceMaybe`, retyped to take `CursorDescMaybe *`) now
  decompile with clean `this->field`/`pSizeSourceMaybe->field` access, no raw offsets.
- **Resolves the v11/v12 open question:** `+0x14`/`+0x16` (`nativeWidthMaybe`/`nativeHeightMaybe`)
  and `+0x32`/`+0x34` (`hotspotXMaybe`/`hotspotYMaybe`) are NOT a duplicate width/height pair —
  they're genuinely different sub-fields (native loaded bitmap size vs. a hotspot/anchor point),
  both sharing `+0x160` (`scaleMaybe`, default 1) as a common scale divisor. No contradiction,
  just two distinct fields on the same object.
- **Opens a bigger new mystery:** `CursorDescMaybe`'s own vtable (`PTR_FUN_00477c18`) contains
  `RequestModeTransitionFromSourceMaybe`/`ScheduleModeTransitionMaybe` at slots `+0x24`/`+0x28` —
  the SAME two pointers, same relative slots, verbatim in all 8 top-level singleton windows' own
  vtables (confirmed via `get_xrefs_to`: exactly 8 DATA refs each). Strongly suggests
  `CursorDescMaybe` derives from `WindowBase` and inherits those two methods unmodified — but its
  own low-offset fields (`resourceId` at `WindowBase`'s nominal `+4` "`hInstance`" slot, etc.)
  don't match `WindowBase`'s currently-named low fields, and `CursorDescMaybe`'s OWN slot `+0xc`
  is a totally different, unrelated method (`SetBitmapMaybe`, not
  `RequestModeTransitionFromSourceMaybe`) — so slot `+0xc` isn't uniformly the same virtual across
  every `WindowBase` subclass, only `+0x24`/`+0x28` are. Left `CursorDescMaybe` as a flat,
  independent struct rather than assert an unverified inheritance model in Ghidra.
- All changes `save_program`'d; probe match counts re-verified unaffected after the struct edits.

**Not done this session** (explicitly deferred, still open, carried from v12): the
`InitErrorCode` sub-object semantics inside `NetSetupWnd`/`ApplSetupWnd` still not traced;
`DAT_004fd3a8`'s struct/flags not named/typed; `FUN_00422660`'s edit-control validation and
the string globals it touches (`DAT_004aa4a8`/`DAT_004a9eec`) not traced; `FUN_00408130`'s
states 2/3/4 bodies not read; `BuildToolCursorWnd`'s `Unk0x120`/`toolParam2Maybe` still
unread; the 2 catalogued engine bugs (`docs/engine-bugs.md`) not yet reproduced live or
transcribed to `src/`; `Ddraw_RebindWindowClipper`+`FUN_00426eb0` still just "redraw," not
traced pixel-level; whether the other `WindowBase`-derived singletons ever trigger a real
cursor transition of their own.

**New from this session, carried forward:** the `CursorDescMaybe`/`WindowBase` shared-vtable-slot
mystery above (highest-value next step — see the concrete plan in item 2 below);
`CursorDescMaybe`'s own vtable slots `0`, `+4`, `+8`, `+0x14`-`+0x1c` (addresses `0x424b40`
[named, scalar-deleting-dtor], `0x425670`, `0x4257f0`, `0x4258f0`, `0x425990`, `0x4259c0`) not
individually read/named yet — only `+0xc` (`SetBitmapMaybe`) and `+0x10` (`LoadMaybe`, i.e. it's
its own vtable slot too) are understood; `FUN_00446840`'s other 4 constructor families
(`FUN_0041e570`/`0x630B`, `FUN_0044b190`/`0x63cB`, `FUN_00436400`/`0x178B`, `FUN_0040e600`/
`0x7acB`) and their real-loader bodies (`0x41e6e0`/`0x436490`/`0x40e690`/`0x44b290` per v12's
"5-member near-identical family" note — these ARE `CursorDescMaybe`'s siblings) not read.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Resolve the `CursorDescMaybe`/`WindowBase` vtable-sharing mystery.** Read
   `FUN_0041e570`/`FUN_0044b190`/`FUN_00436400`/`FUN_0040e600` (the other 4 bucket constructors
   in `FUN_00446840`) and check whether THEIR vtables also embed
   `RequestModeTransitionFromSourceMaybe`/`ScheduleModeTransitionMaybe` at `+0x24`/`+0x28`. If
   all of them do, that's strong evidence of a genuine deep `WindowBase` inheritance hierarchy
   (a "loadable resource object" base sitting between `WindowBase` and its 8 window subclasses)
   worth modeling properly in Ghidra; if only some do, the shared-slot theory needs rethinking.
   Also read `CursorDescMaybe`'s untouched vtable slots (`0`, `+4`, `+8`, `+0x14`..`+0x1c`) to see
   if any of THEM also collide with named `WindowBase` methods — more data points either way.
3. **Read `Obj0x47851c::InitErrorCode`'s body** (already matched as a Phase-2 leaf,
   `0x454b50`) against the string-id ranges found in v10 (`0x417`-`0x421` for NetSetupWnd,
   `0x429`-`0x42f`/`0x43a`+9 for ApplSetupWnd) to pull the actual error/status message text.
4. **Type `DAT_004fd3a8`** (the network/session config struct gating Enter's NetSetup vs.
   ApplSetup vs. skip-to-game branch) and **read `FUN_00408130`'s states 2/3/4 bodies** to
   round out the app-state-machine picture.
5. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`).
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
6. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut).

## v14 — 2026-07-12 — 4 sibling `CursorDescMaybe`-derived classes modeled; `+0x24`/`+0x28`
WindowBase-slot-sharing mystery resolved to "not uniform"

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

**Closed pickup item 2** (resolve the `CursorDescMaybe`/`WindowBase` vtable-sharing mystery).
Full writeup: `docs/subsystems.md`'s "Seventh session" section. Highlights:
- **Fanned out 4 parallel read-only agents**, one per `FUN_00446840` sibling bucket constructor
  (`FUN_0041e570`/`FUN_0044b190`/`FUN_00436400`/`FUN_0040e600`). **All 4 call
  `CursorDescMaybe::CursorDescMaybe(this, param_1, 0)` directly in place on `this` before
  overwriting their own vtable pointer** — the textbook MSVC base-ctor-then-derived-vtable-store
  pattern. This is unambiguous: `CursorDescMaybe` is a genuine, reused C++ base class (data-layout
  inheritance), now confirmed base of 5 total resource-object families reachable through
  `FUN_00446840`'s bucket dispatch.
- **Modeled all 4 as new Ghidra structs** (`Obj0x4779e0` 0x630B, `Obj0x478358` 0x63cB,
  `Obj0x478118` 0x178B, `Obj0x477610` 0x7acB — named by their vtable address per house style),
  each with a `base` field at offset 0 (parent struct, sizes confirmed against each family's
  `operator new(N)` call site). Discovered a 3rd inheritance level: `Obj0x478358` derives
  `Obj0x4779e0` (not `CursorDescMaybe` directly) — chain is `CursorDescMaybe → Obj0x4779e0 →
  Obj0x478358`. `Obj0x4779e0` also embeds two known `Obj0x477840` sub-objects at `+0x534`/`+0x548`.
  Moved+renamed each family's ctor (bare class name) and loader (`LoadMaybe`) into the new
  namespace via `set_function_this_type`; plate comments on each ctor capture the evidence.
- **The `+0x24`/`+0x28` sharing is NOT uniform across siblings** — only `Obj0x4779e0` (direct
  `CursorDescMaybe` child) inherits both slots byte-identical (plus `+0x04`/`+0x08`/`+0x2c`/`+0x30`,
  6 slots total). `Obj0x478358` explicitly OVERRIDES both (rect hit-test instead).
  `Obj0x478118`'s raw vtable read is inconclusive (a possible off-by-one-slot layout divergence,
  not reconciled with a structured re-read). `Obj0x477610`'s own vtable doesn't extend that far at
  all (ends ~`+0x14`/`+0x18`) — it simply never inherited those slots. Net: the original "derives
  WindowBase, all consumers inherit the two mode-transition methods" story only demonstrably holds
  for one sibling; the others diverge in different ways. Still consistent with a real common
  ancestor above `CursorDescMaybe` — just not something every id-range family that reuses
  `CursorDescMaybe`'s DATA layout also reuses past a certain point in the VIRTUAL layout.
- All changes `save_program`'d; probe match counts re-verified unaffected after the struct edits.

**Not done this session** (explicitly deferred, still open, carried from v13): the
`InitErrorCode` sub-object semantics inside `NetSetupWnd`/`ApplSetupWnd` still not traced;
`DAT_004fd3a8`'s struct/flags not named/typed; `FUN_00422660`'s edit-control validation and
the string globals it touches (`DAT_004aa4a8`/`DAT_004a9eec`) not traced; `FUN_00408130`'s
states 2/3/4 bodies not read; `BuildToolCursorWnd`'s `Unk0x120`/`toolParam2Maybe` still
unread; the 2 catalogued engine bugs (`docs/engine-bugs.md`) not yet reproduced live or
transcribed to `src/`; `Ddraw_RebindWindowClipper`+`FUN_00426eb0` still just "redraw," not
traced pixel-level; whether the other `WindowBase`-derived singletons ever trigger a real
cursor transition of their own.

**New from this session, carried forward:**
- `Obj0x478118`'s (`0x436400`) vtable-slot-alignment discrepancy at `+0x24`/`+0x28` — needs a
  structured re-read (e.g. via `get_xrefs_to` on `FUN_00414340`/`NoOpVirtualMaybe` scoped to that
  vtable, not a raw dword scan) to settle whether it's a genuine 1-slot layout divergence or a
  misread.
- `Obj0x477610`'s (`0x40e600`) `+0xc` vtable override target (`0x40e8d0`) has NO Ghidra function
  yet — needs `create_function` first (likely a plain analyzer gap per the established
  "Ghidra-missed function gaps" playbook) before it can be read/named.
- None of the 4 new classes' own TRAILING fields (past the inherited base) are byte-precise yet —
  only the base subobject (and, for `Obj0x4779e0`, its two `Obj0x477840` sub-objects) are
  struct-modeled; the rest is Ghidra's default padding. Lowest-priority of the open items — full
  field-mapping of 4 new classes is its own multi-session project, not implied by "resolve the
  mystery."
- `FUN_00454890` (the `Obj0x478358` override at `+0x24`) and `FUN_00414340` (the `Obj0x478118`
  override at `+0x24`, signature-compatible with `RequestModeTransitionFromSourceMaybe`) not
  read/named.
- `CursorDescMaybe`'s own untouched vtable slots `0`, `+4`, `+8`, `+0x14`-`+0x1c` (addresses
  `0x424b40` [named], `0x425670`, `0x4257f0`, `0x4258f0`, `0x425990`, `0x4259c0`) still not
  individually read/named — carried from v13, still open.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Settle `Obj0x478118`'s vtable-slot discrepancy** with a structured (not raw-dword) read —
   the smallest, most self-contained follow-up to this session's work.
3. **Read `Obj0x47851c::InitErrorCode`'s body** (already matched as a Phase-2 leaf,
   `0x454b50`) against the string-id ranges found in v10 (`0x417`-`0x421` for NetSetupWnd,
   `0x429`-`0x42f`/`0x43a`+9 for ApplSetupWnd) to pull the actual error/status message text.
4. **Type `DAT_004fd3a8`** (the network/session config struct gating Enter's NetSetup vs.
   ApplSetup vs. skip-to-game branch) and **read `FUN_00408130`'s states 2/3/4 bodies** to
   round out the app-state-machine picture.
5. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`).
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
6. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## ⏭ NEXT SESSION PICKUP (v15 — 2026-07-11 — `Obj0x478118` vtable mystery fully resolved (no
divergence); `InitErrorCode` corrected → `ResourceRefMaybe::InitResourceId`, real button/icon
resource names recovered for NetSetupWnd/ApplSetupWnd)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

**Closed pickup item 2** (settle `Obj0x478118`'s `+0x24`/`+0x28` vtable-slot discrepancy).
Full writeup: `docs/subsystems.md`'s "Eighth session" section. **No layout divergence exists —
the "mystery" was a value-vs-offset matching artifact.** Dumped all 5 family vtables
(`CursorDescMaybe`/`Obj0x478118`/`Obj0x4779e0`/`Obj0x478358`/`Obj0x477610`) structurally,
per-offset, resolving every dword through `FunctionManager.getFunctionAt` instead of searching
for a function's address anywhere in a table. Result: every slot `0x0`–`0x3c` in `Obj0x478118`
lines up 1:1 in POSITION with `CursorDescMaybe`'s base vtable — `+0x24` overrides
`RequestModeTransitionFromSourceMaybe` with its own `FUN_00414340`; `+0x28` overrides
`ScheduleModeTransitionMaybe` with `NoOpVirtualMaybe` (a REAL override to a no-op, correct
slot); `+0x2c` overrides with `PopupWndBase::Create`; `+0x38` reuses `FUN_004661a0` (same
function `CursorDescMaybe` itself uses, unmodified, at ITS OWN `+0x3c`); `+0x40` is a genuine
NEW virtual (`0x422ea0`) appended past `CursorDescMaybe`'s 16-slot/`0x3c`-ending base table. The
original false alarm: `NoOpVirtualMaybe`/`FUN_004661a0` each occur at TWO DIFFERENT offsets
within `CursorDescMaybe`'s own base vtable (code reuse across slots, common in this codebase —
see the growing "shared no-op stub" pattern), so matching by function-pointer VALUE instead of
by OFFSET gave an ambiguous, wrong answer. Plate comment on `0x436400` updated with the full
resolution; `docs/subsystems.md`'s comparison table and "Revised understanding" paragraph
updated to match. `save_program`'d.

**Substantially advanced pickup item 3** (was: "read `InitErrorCode`'s body... to pull the
actual error/status message text"). **The premise was wrong — corrected, not just answered.**
`Obj0x47851c::InitErrorCode` (`0x454b50`) has no `LoadStringA` call; it's not an error-code
object at all. Its single param is a raw `RT_STRING` resource-**name** id, stored verbatim.
Decompiled `NetSetupWnd::InitFields`/`ApplSetupWnd::InitFields` (`0x440fa0`/`0x408b20`) directly
to read the exact ids passed per call site (no assumption of a contiguous range), then
cross-checked against a fresh `RT_STRING` extraction (`pip install pefile` + a ~30-line script —
no existing extraction tool was in the repo) and `loco/rfh.txt`. Result: `NetSetupWnd`'s 7 ids
(`0x417`/`0x418`/`0x419`/`0x41a`/`0x41f`/`0x420`/`0x421`) are `startup\net_ipx`/`net_tcp`/
`net_Go`/`net_Exit`/`netInput`/`net_MATX`/`net_Tray` (a network protocol-picker screen: IPX vs.
TCP, Go/Exit); `ApplSetupWnd`'s ids (`0x429`/`0x42a`/`0x42b`/`0x42c`/`0x42f` + a 9-loop
`0x43a`+0..8) are `startup\apsearch`/`apGo`/`apoption`/`apExit`/`APmatrix`/`aplayer0`..
`aplayer8` (a 9-avatar player-select wizard). All exist as `.dat`+`.bmp` pairs in
`loco/rfh.txt`. The originally-assumed "`0x417`–`0x421`"/"`0x429`–`0x42f`" ranges from the 3rd
session were outer-bound estimates, not literally contiguous — 4 ids in the first range
(`0x41b`–`0x41e`) aren't used by either window and have no `RT_STRING` entry. Renamed
end-to-end (struct/namespace/ctor/field, 0 stragglers verified): `Obj0x47851c` →
`ResourceRefMaybe`, `InitErrorCode` → `InitResourceId`, field `nM_1c` → `resourceId`. Also
checked the class's own vtable: exactly ONE real slot (dtor `0x454b70`) — the next dword in
`.rdata` is NOT a second slot, it's the start of an unrelated adjacent vtable
(`PTR_FUN_00478520`, `create_function`'d at `0x454db0` to confirm). Plate comment on `0x454b50`
captures the full resolution. `nM_14`/`nM_18`/`nM_20` (an owned sub-object pointer released via
a virtual call in the dtor, presumably the lazily-loaded bitmap for `resourceId`, plus 2 more
unread fields) still open — lower priority, this was a well-supported but partial pass.

**Lightly scoped pickup item 4** (type `DAT_004fd3a8`) — NOT typed, scope-checked only. It has
60+ read/write xrefs spanning dozens of functions across the DirectPlay/network-setup layer
(`DPlay_PrepareInternetConnection`, `Config_SaveWindowAndCleanExit`, `SetState`,
`OnCreateComplete`, many `FUN_0040/41/42/43/44xxxx`) — confirmed too large for an incidental
pass, it's its own multi-session project like the scalar-dtor family. Confirmed it's a global
**pointer** (not embedded state): `FUN_00406ba0` (bootstrap) does
`DAT_004fd3a8 = FUN_00440c60(puVar3);` — `FUN_00440c60` is presumably its constructor/factory,
not yet traced. `FUN_00408130`'s states WERE read as a side effect (it's the top-level app-screen
state macht, dispatch on a global `DAT_004851f4` "current state" pointer, 10 states 0x1-0xa
covering Splash/Mail/AlbumCard/EditCard/Map/Tutorial-mirror/teardown transitions with DSound
ducking + `PostMessageA`-based quit) — but this switch does NOT reference `DAT_004fd3a8` at all,
so it doesn't inform the NetSetup/ApplSetup gating question the pickup was actually after; that
gating logic (the literal Enter-key branch) lives elsewhere and is still untraced.

**Not done this session** (explicitly deferred, still open, carried from v14): `FUN_00422660`'s
edit-control validation and the string globals it touches (`DAT_004aa4a8`/`DAT_004a9eec`) not
traced; `BuildToolCursorWnd`'s `Unk0x120`/`toolParam2Maybe` still unread; the 2 catalogued
engine bugs (`docs/engine-bugs.md`) not yet reproduced live or transcribed to `src/`;
`Ddraw_RebindWindowClipper`+`FUN_00426eb0` still just "redraw," not traced pixel-level; whether
the other `WindowBase`-derived singletons ever trigger a real cursor transition of their own.

**New from this session, carried forward:**
- `DAT_004fd3a8`'s actual literal Enter-key NetSetup-vs-ApplSetup-vs-skip-to-game gating branch
  not yet located — `FUN_00408130` (checked this session) is a red herring for this specific
  question, it's a different (top-level screen) state machine. Whatever function reads the
  Enter keypress and branches 3 ways is still unidentified; `FUN_00440c60` (`DAT_004fd3a8`'s
  apparent ctor/factory) is the natural next place to look, plus the callers of that bootstrap
  write site (`FUN_00406ba0`).
- `ResourceRefMaybe`'s `nM_14`/`nM_18`/`nM_20` fields unread — `nM_14` is an owned pointer
  released via `(**(code**)(*p+8))()` in the dtor (a virtual call, slot 2 of whatever `nM_14`
  points at); `nM_18`/`nM_20` purposes not even hypothesized yet.
- `0x454db0`'s new adjacent, still-unidentified class (`PTR_FUN_00478520`) — found by accident
  while checking `ResourceRefMaybe`'s vtable extent; nothing else about it traced (not even
  whether it's related to the resource-object family at all).
- None of the 4 `CursorDescMaybe`-sibling classes' own TRAILING fields (past the inherited base)
  are byte-precise yet — carried from v14, still lowest-priority/still open.
- `FUN_00454890` (the `Obj0x478358` override at `+0x24`) and `FUN_00414340` (the `Obj0x478118`
  override at `+0x24`) not read/named — carried from v14.
- `CursorDescMaybe`'s own untouched vtable slots `0`, `+4`, `+8`, `+0x14`-`+0x1c` (addresses
  `0x424b40` [named], `0x425670`, `0x4257f0`, `0x4258f0`, `0x425990`, `0x4259c0`) still not
  individually read/named — carried from v13/v14, still open.
- `Obj0x477610`'s (`0x40e600`) `+0xc` vtable override target (`0x40e8d0`) still has no Ghidra
  function — carried from v14, still open.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Find `DAT_004fd3a8`'s Enter-key gating branch** — the actual original ask behind pickup
   item 4. Start from `FUN_00440c60` (the apparent ctor/factory) and `FUN_00406ba0`'s bootstrap
   write site; do NOT re-derive from `FUN_00408130`, already confirmed unrelated this session.
   Full struct typing of `DAT_004fd3a8` is a separate, much bigger follow-on — don't conflate
   the two; the gating branch is the smaller, more self-contained win.
3. **Read `ResourceRefMaybe`'s `nM_14`/`nM_18`/`nM_20`** and find where the lazily-loaded
   bitmap/resource for `resourceId` actually gets loaded (not in the ctor) — likely another
   method reachable from `NetSetupWnd`/`ApplSetupWnd`'s `Create`/paint paths.
4. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`).
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
5. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## ⏭ NEXT SESSION PICKUP (v17 — 2026-07-11 — `bSkipSetupWizardMaybe`'s write site found
(`NetSetupWnd::OnLButtonUp`), `FUN_00440ea0` mis-hypothesis corrected, `ResourceRefMaybe`'s
`nM_14`/`nM_18`/`nM_20` load chain fully resolved — all 3 v16 pickup items closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

Full writeup: `docs/subsystems.md`'s "Tenth session" section. All three v16 pickup items closed
in one session:

**1. Found `bSkipSetupWizardMaybe`'s write site.** Ghidra had never created a function at
`0x422930` (the analyzer-gap pattern hit before) — `create_function` recovered a 1090-byte body
(`0x422930`–`0x422d71`) cleanly. Named **`NetSetupWnd::OnLButtonUp`**: a `WM_LBUTTONUP`-shaped
dispatcher that `PtInRect`-tests 6 button regions (now real `RECT` fields —
`rectEnterMaybe`/`rectCancelMaybe`/`rectPlayAloneMaybe`/`rectConnectOnlineMaybe`/
`rectToggleAMaybe`/`rectToggleBMaybe` at `+0x13c`/`+0x14c`/`+0xfc`/`+0x10c`/`+0x11c`/`+0x12c`).
`rectPlayAloneMaybe` sets `bSkipSetupWizardMaybe=1`; **`rectConnectOnlineMaybe` sets it to `0`**
— the write site the v16 pickup was hunting for (gated on `g_pNetSettingsMaybe->Unk0x10 != 0`,
a non-empty provider list). Plate comment added documenting the full 6-region branch table.

**2. `FUN_00440ea0` traced — v16's hypothesis was WRONG, corrected.** It's not a provider
resync; decompile shows a flat `WriteFile` of `0xac` bytes (`this+4..this+0xb0`) to
`NetSettings.dat` — the exact inverse of `LoadOrInitFromDisk`. Renamed
**`NetSettingsMaybe::SaveToDisk`**. The REAL provider-availability resync is a different,
previously-unnamed function, **`FUN_004419c0`** → renamed **`NetSetupWnd::RefreshProviderAvailability`**
(called once, from `OnCreateComplete`; walks `g_pNetSettingsMaybe->Unk0x10`'s linked list,
sets `bProviderId2/4AvailableMaybe`). `OnEnterCommitAndDispatch`'s plate comment corrected to
remove the wrong claim. ⚠ **Self-caught error, also fixed**: an intermediate writeup
mis-transcribed `DAT_004fd3ac` (a separate global 4 bytes after `g_pNetSettingsMaybe`, used by
an unrelated >2000-byte singleton via `FUN_0043d2b0`) as `g_pNetSettingsMaybe->Unk0xac` — these
are NOT the same thing; corrected in `docs/subsystems.md` before it propagated further (never
written to Ghidra, so no Ghidra-side damage).

**3. `ResourceRefMaybe`'s `nM_14`/`nM_18`/`nM_20` resolved.** Load chain:
`NetSetupWnd::OnFirstActivateMaybe` (renamed from `FUN_00441870`, one-shot gated on new field
`bResourcesLoadedMaybe` at `+0x1ac`) calls **`ResourceRefMaybe::LoadMaybe`** (renamed from
`FUN_00454bf0`) per member: `pCursorDescMaybe` (renamed `nM_14`, typed `CursorDescMaybe*`) comes
from a shared ~0x4000-slot lazy resource registry (`FUN_00446ea0`, keyed by `resourceId`,
`LoadStringA`+`FUN_00446840` dispatcher on a cache miss — already-documented `CursorDescMaybe`
family, confirmed via the loaded RT_STRING text e.g. `"startup\net_ipx"`); `nRealizedHandleMaybe`
(renamed `nM_18`) is that object's own vtbl+4 "realize" call's return value. `nM_20` stays
genuinely unused by this chain — ruled out, not resolved.

**Not done this session** (carried/new, all open): `FUN_004421d0` (tail call of
`OnFirstActivateMaybe`, unread); `FUN_00446ea0`'s locale-offset switch table (`DAT_004851f4`)
not fully mapped; the `DAT_004fd3ac`-rooted singleton `FUN_0043d2b0` operates on (likely
DirectPlay session/lobby manager, >2000 bytes) unidentified; a **new open question**:
`DPlay_PrepareInternetConnection` compares `rememberedProtocolPrimaryMaybe` against `1`/`2`/`3`
(selecting `Unk0x2c`/`Unk0x6c`/`Unk0xac`-based connection info), but
`OnEnterCommitAndDispatch`/`OnLButtonUp`/`RefreshProviderAvailability` all compare the SAME field
against `2`/`4` — same struct offset, two different value domains at different call sites, not
reconciled; this also bears on the still-open "does 4/2 mean TCP/IP vs IPX" question.
`0x454db0`'s adjacent unidentified class (`PTR_FUN_00478520`); none of the 4 `CursorDescMaybe`-
sibling classes' trailing fields byte-precise yet; `FUN_00454890`/`FUN_00414340` not read/named;
`CursorDescMaybe`'s own untouched vtable slots `0`/`+4`/`+8`/`+0x14`-`+0x1c` not individually
read/named; `Obj0x477610`'s `+0xc` override target (`0x40e8d0`) still has no Ghidra function;
`BuildToolCursorWnd`'s `Unk0x120`/`toolParam2Maybe` still unread; the 2 catalogued engine bugs
(`docs/engine-bugs.md`) not yet reproduced live or transcribed to `src/`;
`Ddraw_RebindWindowClipper`+`FUN_00426eb0` still just "redraw," not traced pixel-level.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Reconcile `rememberedProtocolPrimary/SecondaryMaybe`'s two value domains** (`1`/`2`/`3` in
   `DPlay_PrepareInternetConnection` vs `2`/`4` everywhere else) — see "Not done this session"
   above. Would also resolve the long-standing "TCP/IP vs IPX" naming question.
3. **Identify the `DAT_004fd3ac`-rooted singleton** `FUN_0043d2b0` operates on — a >2000-byte
   object touched from `NetSetupWnd::OnLButtonUp`'s commit paths, plausibly the DirectPlay
   session/lobby manager. Not yet even xref-swept.
4. **Trace `FUN_004421d0`** (tail call of `NetSetupWnd::OnFirstActivateMaybe`) and
   `FUN_00446ea0`'s locale-offset switch table (`DAT_004851f4`) for completeness of the
   resource-load chain closed this session.
5. **Continue Phase 3 fan-out** to the remaining unexplored territory each v6 agent flagged
   (all still genuinely open):
   - RF archive: cluster the unclustered `LoadResource` callers (`0x448a70`/`0x413660`/
     `0x438e40`).
   - DirectSound: `0x412c50`'s channel-count formula has an unresolved input (`piStack_10`,
     no visible assignment in the decompile) — needs a raw disasm read before trusting it.
     Also resolve the `SetCooperativeLevel` hwnd-vs-SEH-frame ambiguity the agent flagged.
   - DirectDraw: `0x401280` is the strongest Flip/Present candidate (7 touches of the
     primary-surface global) but unread; the `0x45b9b0`/`0x45ba19` mode-redetect twin of
     `Ddraw_Init` is unread; write-sites for `DAT_004aad46`/`DAT_004aad48` (tile-grid
     extent globals) not traced.
   - DirectPlay: the gameplay data-sync layer (custom message/packet structs sent via
     `IDirectPlay4::Send`) was NOT found in the 2-3 hop radius searched — likely reached
     from `0x460360`/`0x45fd80`/`0x466390` or the main game loop's message pump.
   - INI/config: no writer found yet for `RemoteRes` (`DAT_004a97a8`) outside `0x4068d0`
     itself — likely lives in the unexplored DirectPlay network layer above.
6. **Older open items, all still genuinely open** (carried forward, lower priority than
   the above now that Phase 2 isn't the driver): `CombineFlags0x445910` (parked v2, needs a
   fresh angle — bitfields? a different expression shape?); v3 parked pure-scheduling
   residuals `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980` (each tried one reorder variant, no improvement);
   `0x466126` (hand-shaped stub, no own prologue, reads caller's ebp directly — still
   uncharacterized, called from 60+ sites after `operator new`); the ~54 remaining
   scalar-deleting-destructor family members (need real base-dtor bodies modeled first —
   this is now squarely Phase 3/4 work, not a leaf shortcut); `0x40e680`'s shared
   base-dtor-body `0x424ba0` (touches offsets up to 0x162 across 3 owned sub-objects).

## ⏭ NEXT SESSION PICKUP (v19 — 2026-07-11 — `GameNet_DispatchMessage`'s thread chain found
[tick-driven, confirmed], `NetSettingsMaybe`'s protocol-domain enum resolved via raw DirectPlay
GUID decode [1=Modem/2=TCP-IP/3=Serial/4=IPX], 21-function opcode/handler cluster mapped —
all 5 v18 pickup items closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

Full writeup: `docs/subsystems.md`'s "Twelfth session" section. Method: found the thread entry
point directly via the documented analyzer-gap pattern (`objdump` past the previous function's
`ret`), then fanned out 3 parallel READ-ONLY research agents over the resulting handler cluster
(grouped by close/retry, packet-handler, and locale/handshake families), applying all 25
renames + 3 plate comments afterward in the main thread via two batched `run_script_inline`
calls. Worked well again for a cluster this size — same shape as v18.

**1. `GameNet_DispatchMessage`'s own caller found, full thread chain resolved.** `0x439240`
(an un-created analyzer gap right after `FUN_004391a0`'s `ret` — same class of miss as before)
is the network thread's tick loop, renamed `GameNetThread_TickLoop`. Confirmed **tick-driven,
not message-pump-driven**: bare `do{}while(true)` + `Sleep(g_pNetSettingsMaybe->Unk0xc)`, no
`PeekMessage`/`GetMessage` anywhere. Full spawn chain traced: `FUN_00422820` (bootstrap) →
`Thread_Start` (0x461790, a method on a generic `Obj0x479168` thread-wrapper class — only this
method renamed, not the whole class) → CRT `_beginthreadex`-style wrapper (`FUN_00468870`,
left unrenamed — likely reused outside GameNet) → `CreateThread` targeting
`Thread_TrampolineProc` (0x461890, ALSO a previously un-created analyzer gap) → calls
`(*(this+0x410))(this+0x414)` then `_endthreadex(0)`. Each tick:
`GameNet_DispatchMessage` (if connected) + drains a SEPARATE local command queue via
`GameNet_ProcessLocalCommand` (renamed from `FUN_00439550`, opcodes 0-0x19 — a distinct
dispatch mechanism from the network-opcode one, keep them mentally separate) + every 0x14
ticks (default) runs `TrainNet_AdvanceLocalTrainSteps`.

**2. `NetSettingsMaybe`'s 1-4 protocol-domain question resolved definitively** (open since
v11+). `GameNetThread_InitState` (renamed from `FUN_00438bc0`) populates
`g_pNetSettingsMaybe->Unk0x10` from `DPlay_ProbeAvailableProviders` (0x45eab0). Hand-decoded
the raw GUID bytes passed to each `DirectPlayCreate` call (PE section-header math + raw file
read — not a Ghidra call) against known DirectPlay 3 SP GUIDs: **1=Modem** (via
`DPlay_ProbeModem`), **2=TCP/IP** (`36E95EE0-8577-11CF-960C-0080C7534E82` = `DPSPGUID_TCPIP`,
exact match), **3=Serial** (probed after 4x chained `CreateFileA` COM-port checks),
**4=IPX** (`685BC400-9D2C-11CF-A9CD-00AA006886E3` = `DPSPGUID_IPX`, exact match). This IS
`rememberedProtocolPrimary/SecondaryMaybe`'s domain — strike "not 100% pinned" from the v18
note. Plate comment written on `DPlay_ProbeAvailableProviders`. Separately: the
`NetSettingsMaybe` ctor's `"0".."3"` digit loop is UNRELATED to `Unk0x10` — confirmed via raw
byte read (found literal `"COM"` string) that it's a `CreateFileA("COM0".."COM3")` port-existence
probe into `field_0x14[0..3]`, not part of the provider list.

**3. Opcode/handler cluster mapped, 21 functions renamed** (full list + one-line rationale in
`docs/subsystems.md`'s "Twelfth session" section — not re-copied here to keep this block short).
Headline corrections to prior hypotheses: `GameNet_UnpackRosterRecord` (0x4426d0, was thought
"bulk world-state array" — it's actually a 9-slot PLAYER/SESSION ROSTER snapshot, full
byte-offset table in its plate comment, the train/track-position guess is retired);
`NetFile_PumpPendingTransferSend` (0x439df0, was thought "heartbeat" — it's the OUTBOUND
file-transfer chunk sender); `TrainNet_AdvanceLocalTrainSteps` (0x43bb00, was thought "periodic
full-sync broadcast" — no `DPlay_SendMessage` call at all, it's the per-tick LOCAL train
movement simulator posting to a local queue). New, correctly-scoped finds:
`GameNet_ConnectOrJoinSession` (0x43c410, the full DPlay session bootstrap — bigger than
expected), `GameNet_BroadcastPlayerRoster`/`GameNet_ReceiveRosterSnapshot` (0x43ccc0/0x43ce10,
send/receive pair for a 0x390-byte-per-peer roster snapshot, opcode 0x3ec),
`GameNet_HandleTrainStateSync`/`GameNet_HandleTrainStateAck` (0x43b240/0x43b6d0, opcode
0x3f2/0x3f3 request/ack pair with an embedded 936-byte-stride per-car sub-record, NOT yet
individually field-mapped).

**Not done this session:** the 936-byte per-car sub-record inside `GameNet_HandleTrainStateSync`
not individually field-mapped (train position/heading/track-piece IDs still unknown at that
level — the natural next deep-dive); `GameNet_UnpackRosterRecord`'s dest offset 0x37-0x47 gap
unmapped; two file-transfer list roots (`this+0x28` new vs `+0x2c` in-progress on the tick-loop
state struct) not reconciled as inbound-vs-outbound or independent categories; `g_pNetManager`
still not modeled as a full Ghidra struct; `Obj0x479168` thread-wrapper class not renamed as a
whole (only its `Start` method); `FUN_00468870` CRT `_beginthreadex` wrapper not renamed
(needs a broader xref check first — likely reused outside GameNet); `DPlaySessionMgr`'s
remaining unread methods still carried from v17/v18 (`FUN_0043de30`/`FUN_0043eec0`/partially-read
`FUN_0043e690`); locale RT_STRING offset constants still not written to Ghidra; `0x454db0`'s
adjacent unidentified class (`PTR_FUN_00478520`); the 4 `CursorDescMaybe`-sibling classes'
trailing fields still not byte-precise; the 2 catalogued engine bugs still not reproduced live.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Field-map `GameNet_HandleTrainStateSync`'s embedded 936-byte per-car sub-record** — the
   natural continuation of this session's cluster mapping; this is where actual train
   position/heading/track-piece sync data lives (opcode 0x3f2, function at 0x43b240).
3. **Reconcile the two file-transfer list roots** (`this+0x28` vs `this+0x2c` on the
   `GameNetThread_TickLoop` state struct) — `GameNet_BeginFileTransfer` (0x439d00) writes
   `+0x28`, `GameNet_HandleFileTransferBlock` (0x43a140) reads/writes `+0x2c`; figure out if
   they're inbound-vs-outbound or two independent transfer categories.
4. **Model `g_pNetManager` as a full Ghidra struct** (currently only a plate-comment field
   list) — same treatment `DPlaySessionMgr` got in v18.
5. **Map `GameNet_UnpackRosterRecord`'s dest offset 0x37-0x47 gap** (17 unwritten bytes in the
   76-byte roster record — likely consumer/runtime-populated fields, not part of the wire copy).
6. **Older open items, all still genuinely open** (carried forward, lower priority): the items
   listed under "Not done this session" above (`DPlaySessionMgr`'s unread methods, locale
   RT_STRING constants, `0x454db0`'s class, `CursorDescMaybe` sibling fields, etc.);
   `CombineFlags0x445910` (parked v2, needs a fresh angle); v3 parked pure-scheduling residuals
   `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980`; `0x466126` (hand-shaped stub, still uncharacterized);
   the ~54 remaining scalar-deleting-destructor family members (need real base-dtor bodies
   modeled first — Phase 3/4 work); `0x40e680`'s shared base-dtor-body `0x424ba0`.

## ⏭ NEXT SESSION PICKUP (v20 — 2026-07-11 — `GameNet_HandleTrainStateSync`'s 936-byte
per-car sub-record field-mapped, `CarNetStateMaybe` struct created, `PeerTrainNode`/
`CarNetObj` class pair discovered and named — the v19 top-priority item closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

Full writeup: `docs/subsystems.md`'s "Thirteenth session" section (includes the complete
byte-offset table, not re-copied here to keep this block short). Method: read the target
function's decompile directly (no need for research agents this pass — a single function's
own unpack loop plus 2 helper-function decompiles was enough to derive the whole layout),
cross-checked the derived offsets 3 ways (self-consistency of the unpack loop, the
destination-copy helper using the same relative offsets, and the sub-object ctor's zero-init
loop), then wrote the Ghidra struct + plate comments + renames in the main thread.

**1. The 936-byte per-car record is now (almost) fully field-mapped.** Stride confirmed as
exactly 0xea dwords = 936 bytes, record starts at `param_1+0x2c-0x14`. New Ghidra struct
**`CarNetStateMaybe`** (0x3a8 bytes) holds the layout: `carTypeIdMaybe`/`carSubIdMaybe`
(dwords @0x00/0x04, doubling as ctor args for the per-car object), `hasDetailFlag` (byte
@0x08, gates everything below), `nameAMaybe`/`nameBMaybe` (21-byte buffers @0x1c/0x31,
likely name strings), `Unk0x46` (word, compared against the roster to trigger a
missing-appearance request), `subItemsMaybe` (20 dwords @0x4f, likely cargo/attachment ids),
and the big one — **`pathDataMaybe`** (768 bytes @0xa2 = 128 six-byte entries, almost
certainly per-segment track/waypoint data, but NOT yet individually decoded — see item 2
below). Only ~12 of 936 bytes remain unmapped gaps. Full table in `docs/subsystems.md`.

**2. Discovered the enclosing class pair, previously anonymous locals.** `PeerTrainNode`
(ctor renamed `FUN_0044be50`→`PeerTrainNode_Ctor`, 0x94-byte alloc): one per remote train,
keyed by `trainId`@+0x7a/`ownerByte`@+0x78, linked via a `+0x70` next-pointer (confirmed from
`GameNet_HandleTrainStateAck`'s scan), holds up to 3 car-slot pointers filled one-at-a-time
by `PeerTrainNode_AllocCarSlot` (was `FUN_0044c220`). `CarNetObj` (ctor renamed
`FUN_0040d500`→`CarNetObj_Ctor`, 0x450-byte alloc): the persistent per-car object, embeds a
`CarNetStateMaybe`-shaped sub-object at `+0x88` (own vtable via `FUN_00442850`, not renamed
— see item 4). `CarNetObj_ApplyNetState` (was `FUN_0040d770`) copies the wire-unpacked
scratch record into that `+0x88` sub-object — this is what proved the wire layout and the
in-memory layout are the same. `PeerTrainNode_UpdateSelectedCar` (was `FUN_0044d6c0`) also
renamed (switches which of 2 remembered car ids is "selected").

**3. Plate comments written** on `GameNet_HandleTrainStateSync`, `PeerTrainNode_Ctor`, and
`CarNetObj_Ctor` with the field tables (item 1/2 above, abbreviated in-Ghidra).
`CarNetStateMaybe` is registered in the Ghidra DTM but not yet applied as a variable/field
type anywhere — natural follow-on once `PeerTrainNode`/`CarNetObj` get full struct
definitions to hang the `+0x88` field on.

**Not done this session:** `pathDataMaybe`'s 128 six-byte entries not individually decoded
(this is where actual per-car position/heading/track-piece data almost certainly lives —
now scoped far more precisely than the old "936-byte sub-record" framing); `PeerTrainNode`/
`CarNetObj` not modeled as full Ghidra structs (only plate-comment field lists so far);
`FUN_00442850` (the `CarNetStateMaybe` sub-object's ctor) and its likely-but-unconfirmed
dtor pairing with the already-tracked `VtblDtorStub_0x442a00` not renamed — the cleanup-site
call passes `&local_3ac` (a byte-pointer local), which doesn't cleanly read as "this same
object's address," so treat the pairing as a hypothesis, not fact, until re-checked;
`GameNet_UnpackRosterRecord`'s dest offset 0x37-0x47 gap unmapped; two file-transfer list
roots (`this+0x28` new vs `+0x2c` in-progress) not reconciled as inbound-vs-outbound or
independent categories; `g_pNetManager` still not modeled as a full Ghidra struct;
`Obj0x479168` thread-wrapper class not renamed as a whole (only its `Start` method);
`FUN_00468870` CRT `_beginthreadex` wrapper not renamed (needs a broader xref check first —
likely reused outside GameNet); `DPlaySessionMgr`'s remaining unread methods still carried
from v17/v18 (`FUN_0043de30`/`FUN_0043eec0`/partially-read `FUN_0043e690`); locale RT_STRING
offset constants still not written to Ghidra; `0x454db0`'s adjacent unidentified class
(`PTR_FUN_00478520`); the 4 `CursorDescMaybe`-sibling classes' trailing fields still not
byte-precise; the 2 catalogued engine bugs still not reproduced live.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Decode `pathDataMaybe`'s 128 six-byte entries** (`CarNetStateMaybe`+0xa2,
   `GameNet_HandleTrainStateSync` at 0x43b240) — the natural continuation of this session's
   field-mapping work, and the most promising place to finally pin down train
   position/heading/track-piece sync data at the individual-field level.
3. **Model `PeerTrainNode` and `CarNetObj` as full Ghidra structs** (currently only
   plate-comment field lists) and apply `CarNetStateMaybe` as the type of `CarNetObj`+0x88 —
   same treatment `DPlaySessionMgr`/`CursorDescMaybe` got in earlier sessions.
4. **Resolve `FUN_00442850`/`VtblDtorStub_0x442a00`'s ctor/dtor pairing** for real (read the
   `GameNet_HandleTrainStateSync` SEH cleanup stack frame closely — is `&local_3ac` actually
   the same address as `&local_3a8`, i.e. a Ghidra stack-slot-overlap artifact, or a genuinely
   different local?) before renaming either function.
5. **Reconcile the two file-transfer list roots** (`this+0x28` vs `this+0x2c` on the
   `GameNetThread_TickLoop` state struct) — `GameNet_BeginFileTransfer` (0x439d00) writes
   `+0x28`, `GameNet_HandleFileTransferBlock` (0x43a140) reads/writes `+0x2c`; figure out if
   they're inbound-vs-outbound or two independent transfer categories.
6. **Older open items, all still genuinely open** (carried forward, lower priority): `g_pNetManager`
   as a full struct; `GameNet_UnpackRosterRecord`'s 0x37-0x47 gap; `DPlaySessionMgr`'s unread
   methods; locale RT_STRING constants; `0x454db0`'s class; `CursorDescMaybe` sibling fields;
   `CombineFlags0x445910` (parked v2, needs a fresh angle); v3 parked pure-scheduling residuals
   `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980`; `0x466126` (hand-shaped stub, still uncharacterized);
   the ~54 remaining scalar-deleting-destructor family members (need real base-dtor bodies
   modeled first — Phase 3/4 work); `0x40e680`'s shared base-dtor-body `0x424ba0`.

## v21 — 2026-07-11 — `pathDataMaybe` decoded and RETIRED as a
hypothesis: it's the train-car sticker/decal placement list (`decalSlotsMaybe`), not
track/waypoint data — the v20 top-priority item closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
first thing this session before any Ghidra changes, per protocol. (Ghidra-only session — no
`src/`/toolchain changes, so this wasn't re-verified at session end; it's unaffected.)

Full writeup: `docs/subsystems.md`'s "Fourteenth session" section. Method: found
`CarNetStateMaybe`'s own class has 3 more methods clustered right after its ctor
(0x442c90-0x442e99, found via an `x*3`-then-shift `lea` disasm sweep near the ctor's address
range, since a plain `imul`-by-6 sweep came up empty), decompiled all 3, then traced their 3
real call sites (found via `get_xrefs_to`) down to raw disasm to recover argument provenance
Ghidra couldn't name.

**The 13th session's "per-segment track/waypoint data" guess for `pathDataMaybe` is WRONG —
retired.** It's actually a **128-slot fixed-capacity FIFO of placed decal/sticker hit-rects**:

- **`CarNetStateMaybe_AddDecalMaybe`** (was `FUN_00442c90`): appends one record; scans for an
  empty slot (`placementSeqMaybe`==0), or evicts slot 0 (oldest) and reuses slot 127 if full.
- **`CarNetStateMaybe_HitTestDecalMaybe`** (was `FUN_00442d30`): real `PtInRect` test against
  each occupied slot's rect — confirmed via Ghidra IAT names `GetCursorPos`/`ScreenToClient`/
  `PtInRect` at its call sites; clears+compacts on hit (click-to-remove).
- **`CarNetStateMaybe_CompactDecalsMaybe`** (was `FUN_00442e00`): packs occupied slots down
  over emptied ones.

**Each 6-byte record** (new Ghidra struct `DecalSlotMaybe`, applied as `CarNetStateMaybe`'s
`decalSlotsMaybe : DecalSlotMaybe[128]`, replacing the flat `pathDataMaybe : byte[768]`):
`packedKindMaybe` (dirOrKind<<3 | subIndex-1), `placementSeqMaybe` (nonzero=occupied,
doubles as placement order), `xHalfMaybe`/`yHalfMaybe` (local coords ×½, relative to the
car-art origin), `widthMaybe`/`heightMaybe` (hit-rect size, looked up by the CALLER from a
per-kind descriptor table, not stored here). Confirmed 3 ways: the dword+word copy shape in
`CarNetObj_ApplyNetState`, the ctor's zero-init loop (only pre-zeros the first 2 fields —
exactly the occupancy-marking fields), and the 3 methods' own field arithmetic.

**Caller context nails the "decal" interpretation but is itself a new lead.** All 3 real
call sites sit in a large **UNANALYZED CODE GAP**: `get_function_by_address` returns nothing
for ANY address from 0x4198a4 through past 0x41c485, even though raw `objdump` shows unbroken
plausible code — Ghidra's own `FUN_00419680` cuts off at 0x4198a4 while execution obviously
continues, and a likely jump table around 0x41991d may be why the analyzer choked. Despite no
names, the raw disasm is unambiguous — `GetCursorPos`+`ScreenToClient`, subtract a car-art
origin, call `AddDecalMaybe`/`HitTestDecalMaybe` — a mouse-driven place/remove-decal UI, same
address neighborhood as `EditCardWnd`/`RebuildLocalPlayerCard` (12th session). Since this is
embedded in the opcode-0x3f2 train-state-sync record, **decal placements are networked to
peers** so everyone sees the same stickers on a given car.

**Ghidra state:** `DecalSlotMaybe` struct created; `CarNetStateMaybe.pathDataMaybe` replaced
in-place with `decalSlotsMaybe : DecalSlotMaybe[128]` (verified via readback, offset/length
unchanged); all 3 functions renamed with full plate comments.

**Not done this session:** the 0x4198a4-0x41c485+ code gap not recovered (sizeable, promising
— likely the car-decoration editor's window procedure AND would name the editor window class
itself, currently only known by field offsets `+0x184`/`+0x188`/`+0x1a0`/`+0x1a4`/`+0x2b1`/
`+0x2b2`/`+0x384`/`+0x2bc`); the per-kind decal descriptor table (`this+0x384`-indexed, width
@+8/height @+0xc per entry) not located/typed; `PeerTrainNode`/`CarNetObj` still not modeled
as full Ghidra structs (carried from 13th session, and `CarNetStateMaybe` could now be applied
to `CarNetObj`+0x88 once that struct exists); `FUN_00442850`/`VtblDtorStub_0x442a00` ctor/dtor
pairing still unconfirmed; `GameNet_UnpackRosterRecord`'s 0x37-0x47 gap still unmapped; the two
file-transfer list roots still not reconciled; `g_pNetManager` still not a full struct;
`Obj0x479168` thread-wrapper class not renamed as a whole; `FUN_00468870` `_beginthreadex`
wrapper not renamed; `DPlaySessionMgr`'s remaining unread methods still carried from v17/v18;
locale RT_STRING offset constants still not written to Ghidra; `0x454db0`'s adjacent
unidentified class; the 4 `CursorDescMaybe`-sibling classes' trailing fields still not
byte-precise; the 2 catalogued engine bugs still not reproduced live.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Recover the 0x4198a4-0x41c485+ code gap** — the car-decoration editor's window procedure.
   Follow the CLAUDE.md gap-recovery method (`objdump` to find where real code resumes,
   `create_function` there) but expect a jump table (~0x41991d) to complicate it — may need
   manual jump-table decoding first. Naming this class would also resolve the descriptor-table
   lookup (`this+0x384`-indexed, width/height per decal kind) and the editor's own field layout.
3. **Model `PeerTrainNode` and `CarNetObj` as full Ghidra structs** (currently only
   plate-comment field lists) and apply `CarNetStateMaybe` as the type of `CarNetObj`+0x88 —
   same treatment `DPlaySessionMgr`/`CursorDescMaybe` got in earlier sessions.
4. **Resolve `FUN_00442850`/`VtblDtorStub_0x442a00`'s ctor/dtor pairing** for real (read the
   `GameNet_HandleTrainStateSync` SEH cleanup stack frame closely — is `&local_3ac` actually
   the same address as `&local_3a8`, i.e. a Ghidra stack-slot-overlap artifact, or a genuinely
   different local?) before renaming either function.
5. **Reconcile the two file-transfer list roots** (`this+0x28` vs `this+0x2c` on the
   `GameNetThread_TickLoop` state struct) — `GameNet_BeginFileTransfer` (0x439d00) writes
   `+0x28`, `GameNet_HandleFileTransferBlock` (0x43a140) reads/writes `+0x2c`; figure out if
   they're inbound-vs-outbound or two independent transfer categories.
6. **Older open items, all still genuinely open** (carried forward, lower priority): `g_pNetManager`
   as a full struct; `GameNet_UnpackRosterRecord`'s 0x37-0x47 gap; `DPlaySessionMgr`'s unread
   methods; locale RT_STRING constants; `0x454db0`'s class; `CursorDescMaybe` sibling fields;
   `CombineFlags0x445910` (parked v2, needs a fresh angle); v3 parked pure-scheduling residuals
   `RelPosObj0x436a40::ComputeLocalPos`/`ResetObj0x44d870::ResetAllFields`/
   `PackHalfMinus1Hi/Lo0x41d920/0x41d980`; `0x466126` (hand-shaped stub, still uncharacterized);
   the ~54 remaining scalar-deleting-destructor family members (need real base-dtor bodies
   modeled first — Phase 3/4 work); `0x40e680`'s shared base-dtor-body `0x424ba0`.

## ⏭ NEXT SESSION PICKUP (v22 — 2026-07-11 — 0x4198a4-0x41ef9a gap closed (`EditCardWnd`
identified), `CarNetStateMaybe`/`CarNetObj`/`PeerTrainNode` fully struct-modeled, ctor/dtor
pairing + file-transfer queues resolved, new engine bug found — all 4 v21 top items closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
both before and after this session's changes (Ghidra-only, no `src/`/toolchain changes).

Full writeup: `docs/subsystems.md`'s "Fifteenth session" section. Method: 3 parallel agents
against the v21 pickup's top 3 items (gap recovery does mechanical `create_function` writes;
ctor/dtor pairing and file-transfer reconciliation are read-only — safe to run concurrently),
then main-thread struct modeling + a 4th item (struct definitions) done serially afterward
since it touches the same Ghidra data-type manager the gap-recovery writes didn't.

**1. 0x4198a4-0x41ef9a analyzer gap fully closed — it's `EditCardWnd`.** 7 functions manually
recovered via `objdump`+`create_function`, then Ghidra's own analyzer auto-recovered everything
else in between with no further intervention (confirmed merged into already-known territory —
hit `PackHalfMinus1Hi/Lo`, `LoadMaybe`, `Obj0x4779e0`'s ctor). Only one jump table (inside
`FUN_0041e1f0`, Ghidra bounded it correctly alone). 4 functions renamed/typed as
`EditCardWnd *`: `EditCardWnd_ImportDecalImageMaybe` (0x419b10, file-open handler for decal
images), `EditCardWnd_CenterFileDialogHookProcMaybe` (0x419fd0, its `OPENFILENAMEA` hook),
`EditCardWnd_HandleTimerMaybe` (0x41a8a0, `WM_TIMER` decal hit-test), and
`EditCardWnd_HandleLButtonDownMaybe` (0x41ac10, 7783-byte ~30-`PtInRect` mouse-click
dispatcher for the whole editor). 3 more (`0x41ab70`/`0x41ca80`/`0x41cdf0`/`0x41ce50`) plate-
commented but not confidently named yet — same family, lower confidence.

**2. `CarNetStateMaybe_CarNetStateMaybe`/`CarNetStateMaybe_dtor` ctor/dtor pairing confirmed
and renamed.** The `local_3ac`/`local_3a8` split in `GameNet_HandleTrainStateSync` was a pure
Ghidra decompiler variable-splitting artifact — same `[ESP+0x28]` address at all 3 call sites
(ESP provably constant). `FUN_00442850`→`CarNetStateMaybe_CarNetStateMaybe`, `0x442a00`→
`CarNetStateMaybe_dtor`, both `this`-typed as `CarNetStateMaybe *`.

**3. File-transfer `+0x28`/`+0x2c` lists resolved: outbound vs inbound, not independent
categories.** `GameNet_BeginFileTransfer` (0x439d00, `+0x28`) opens `GENERIC_READ` (sending
out); `GameNet_HandleFileTransferBlock` (0x43a140, `+0x2c`) opens `GENERIC_WRITE`/`CREATE_NEW`
plus has sequence-validation strings (receiving in). Two distinct opcodes (0x11/0x3fb vs
0x12/0x3fc). Plate comments added; `GameNetThread_TickLoop`'s own state struct still not
modeled as a real Ghidra type (future work).

**4. `CarNetObj` (0x450B) and `PeerTrainNode` (0x94B) modeled as full Ghidra structs**,
applied as `this` type of their ctors. Correction to 13th-session notes: `PeerTrainNode` has
**4** car-slot pointers (`pCarSlot0Maybe`-`pCarSlot3Maybe` @+0x10/14/18/1c), not 3.

**⚠ New discovery, not on any prior pickup: `CarNetObj_Ctor` permanently corrupts decal slot
127.** The embedded `CarNetStateMaybe` at `CarNetObj+0x88` spans `0x88`-`0x430` (confirmed via
its own 128-iteration zero loop — hard evidence). `CarNetObj_Ctor`'s `carTypeIdMaybe`/
`carSubIdMaybe` echo writes at `+0x428`/`+0x42c` (previously, WRONGLY, described as "outer
fields beyond the embedded sub-object") actually land INSIDE that range, on
`decalSlotsMaybe[127].yHalfMaybe`/`.heightMaybe` — verified via raw disasm
(`mov DWORD PTR [esi+0x428],edi`) against `DecalSlotMaybe`'s real Ghidra field order
(`packedKind`/`placementSeq`/`xHalf`/`width`/`yHalf`/`height` — note the non-obvious
x/width/y/height interleave). Every `CarNetObj` construction stomps slot 127. Catalogued in
`docs/engine-bugs.md`, not yet reproduced live.

**Ghidra state:** `CarNetObj`/`PeerTrainNode` structs created+applied; `CarNetStateMaybe`
ctor/dtor + 4 `EditCardWnd` methods renamed+typed; plate comments rewritten (corrected) on
`CarNetObj_Ctor`/`PeerTrainNode_Ctor`/both file-transfer functions. All readback-verified,
`save_program` called.

**Not done this session:** the per-kind decal descriptor table (`this+0x384`-indexed, width
@+8/height @+0xc) still not located/typed; the 3 remaining unnamed `EditCardWnd`-family
functions (`0x41ab70`/`0x41ca80`/`0x41cdf0`/`0x41ce50`) not renamed; `GameNetThread_TickLoop`'s
own state struct still not modeled; the new decal-slot-127 bug not reproduced live;
`GameNet_UnpackRosterRecord`'s 0x37-0x47 gap still unmapped; `g_pNetManager` still not a full
struct; `Obj0x479168` thread-wrapper class not renamed as a whole; `FUN_00468870`
`_beginthreadex` wrapper not renamed; `DPlaySessionMgr`'s remaining unread methods still
carried from v17/v18; locale RT_STRING offset constants still not written to Ghidra;
`0x454db0`'s adjacent unidentified class; the 4 `CursorDescMaybe`-sibling classes' trailing
fields still not byte-precise; all 3 catalogued engine bugs still not reproduced live.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Locate and type the per-kind decal descriptor table** (`this+0x384`-indexed on
   `EditCardWnd`, width@+8/height@+0xc per entry) — now that `EditCardWnd_HandleLButtonDownMaybe`
   and friends are named, tracing their `+0x384` accesses should be tractable.
3. **Name/type the 3 remaining `EditCardWnd`-family functions** (`0x41ab70`/`0x41ca80`/
   `0x41cdf0`/`0x41ce50`) — all plate-commented with their state-dispatch shape already,
   decompile with the now-applied `EditCardWnd *` this-type to make this easier.
4. **Model `GameNetThread_TickLoop`'s own state struct** so `outboundTransfersMaybe`
   (`+0x28`)/`inboundTransfersMaybe` (`+0x2c`) can be real typed fields instead of plate-comment
   only — same treatment `CarNetObj`/`PeerTrainNode` just got.
5. **Reproduce the decal-slot-127 corruption bug live** (or via a targeted disasm trace) to
   confirm real-world impact, alongside the 2 older catalogued-but-unreproduced bugs.
6. **Older open items, all still genuinely open** (carried forward, lower priority):
   `GameNet_UnpackRosterRecord`'s 0x37-0x47 gap; `g_pNetManager` as a full struct;
   `Obj0x479168` thread-wrapper class; `FUN_00468870` `_beginthreadex` wrapper;
   `DPlaySessionMgr`'s unread methods; locale RT_STRING constants; `0x454db0`'s class;
   `CursorDescMaybe` sibling fields; `CombineFlags0x445910` (parked v2, needs a fresh angle);
   v3 parked pure-scheduling residuals `RelPosObj0x436a40::ComputeLocalPos`/
   `ResetObj0x44d870::ResetAllFields`/`PackHalfMinus1Hi/Lo0x41d920/0x41d980`; `0x466126`
   (hand-shaped stub, still uncharacterized); the ~54 remaining scalar-deleting-destructor
   family members (need real base-dtor bodies modeled first — Phase 3/4 work); `0x40e680`'s
   shared base-dtor-body `0x424ba0`.

## ⏭ NEXT SESSION PICKUP (v23 — 2026-07-12 — per-kind decal thumbnail cache resolved, all 4
remaining `EditCardWnd` gap functions named via a new vtable-slot cross-reference technique,
`GameNetThreadStateMaybe`/`FileTransferNodeMaybe` fully struct-modeled — all 4 v22 top items closed)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
both before and after this session's changes (Ghidra-only, no `src/`/toolchain changes).

Full writeup: `docs/subsystems.md`'s "Sixteenth session" section. Worked solo (no agents) —
each item built directly on the previous one's findings, a poor fit for parallel dispatch.

**1. The `this+0x384`-indexed table is `EditCardWnd::paDecalThumbCacheMaybe`, a 64-slot
`LocoBitmap*` array at `+0x48c`** — NOT a bespoke decal-metadata struct as the v22 pickup
guessed, but the already-recovered `LocoBitmap` class itself (`+8 width`/`+0xc height` matched
exactly). Traced the array's base+count from the ctor-adjacent init helper's zero loop
(boundary-exact with the next field). Found and named the lazy-population mechanism: a global
clip-art bitmap cache, `ClipartBitmapCache_GetOrLoadMaybe` (0x4442b0), keyed by kind/subkind/
index bytes, LRU-evicted at 256 slots. 3 functions renamed+`this`-typed as `EditCardWnd *`:
`EditCardWnd_ResetAndBeginEditMaybe` (0x416b80, destroys+nulls the cache via virtual dtor),
`EditCardWnd_StepDecalPickerScrollMaybe` (0x419260, pages the picker and fills missing slots).

**2. All 4 remaining unnamed `EditCardWnd`-family functions named**, via a new reusable
technique: cross-check the SAME vtable slot number in a DIFFERENT, already-confirmed sibling
class to pin a handler's WM_* identity (see the new CLAUDE.md lesson bullet). Confirmed slot
0x38 = `WM_LBUTTONDOWN` class-wide via `NetSetupWnd`'s own (previously unnamed) slot-0x38
function sharing the identical shape. Named (all `this`-typed `EditCardWnd *`):
`EditCardWnd_HandleLButtonUpMaybe` (0x41ca80, slot 0x40), `EditCardWnd_HandleMouseMoveMaybe`
(0x41ab70, slot 0x48), `EditCardWnd_HandleSetCursorMaybe` (0x41ce50, slot 0x50, high
confidence — ~20-rect hover/default cursor picker), `EditCardWnd_HandleSetFocusMaybe`
(0x41cdf0, slot 0x60, lower confidence — redirects focus to `g_pTutorialWnd`).

**3. `GameNetThreadStateMaybe` (0x38 bytes, real alloc size from `FUN_00422820`) and
`FileTransferNodeMaybe` (0x1c bytes) modeled as full Ghidra structs**, closing the v22 item on
`outboundTransfersMaybe`/`inboundTransfersMaybe`. Key fields: `+0x00` own vtable
(`0x4781c4` — the state object is itself polymorphic, unexplored), `+0x0d
bStopRequestedMaybe`, `+0x14 pPeerListHeadMaybe`, `+0x20/+0x24` tick counter/interval,
`+0x28/+0x2c` the two transfer-queue heads (now real `FileTransferNodeMaybe*` fields). Applied
to `GameNetThread_TickLoop` (via `set_function_prototype` directly, NOT `set_function_this_type`
— avoids an unwanted namespace move on this `__cdecl` thread-proc-shaped function; new CLAUDE.md
lesson), `GameNet_BeginFileTransfer`, `GameNet_HandleFileTransferBlock`,
`GameNetThread_InitState`. All decompile clean post-application, no raw offset math left.

**Ghidra state:** `paDecalThumbCacheMaybe` field added to `EditCardWnd`;
`GameNetThreadStateMaybe`/`FileTransferNodeMaybe` structs created+applied; 7 functions
renamed+retyped this session (listed above); plate comments added/rewritten throughout. All
readback-verified, `save_program` called.

**Not done this session:** the decal-slot-127 corruption bug (and the other 2 catalogued
engine bugs) still not reproduced live; `GameNetThreadStateMaybe`'s own vtable (`0x4781c4`)
unexplored; its `unk04/08/10/18/1cMaybe`/`unk30/34Maybe` placeholder fields still untyped;
`GameNet_UnpackRosterRecord`'s 0x37-0x47 gap still unmapped; `g_pNetManager` still not a full
struct; `Obj0x479168` thread-wrapper class not renamed as a whole; `FUN_00468870`
`_beginthreadex` wrapper not renamed; `DPlaySessionMgr`'s remaining unread methods still
carried from v17/v18; locale RT_STRING offset constants still not written to Ghidra;
`0x454db0`'s adjacent unidentified class; the 4 `CursorDescMaybe`-sibling classes' trailing
fields still not byte-precise; all 3 catalogued engine bugs still not reproduced live; the
`GameNetThreadStateMaybe::` namespace landing on `GameNet_BeginFileTransfer`/
`GameNet_HandleFileTransferBlock` (a `set_function_this_type` side effect on functions that
read as free opcode handlers, not true methods) not reconsidered.

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Reproduce the decal-slot-127 corruption bug live** (or via a targeted disasm trace) to
   confirm real-world impact, alongside the 2 older catalogued-but-unreproduced bugs — now
   that `CarNetObj`/`CarNetStateMaybe`/`DecalSlotMaybe` are all fully struct-modeled, this
   should be tractable via a focused trace rather than needing the live game.
3. **Explore `GameNetThreadStateMaybe`'s own vtable at `0x4781c4`** — an unexpected discovery
   this session (the state object is polymorphic); its virtual methods are completely unread.
4. **Type the remaining `GameNetThreadStateMaybe` placeholder fields** (`unk04Maybe`/
   `unk08Maybe`/`unk10Maybe`/`unk18Maybe`/`unk1cMaybe`/`unk30Maybe`/`unk34Maybe`) — trace their
   read/write sites the same way `pPeerListHeadMaybe`/the transfer queues were resolved.
5. **Reconsider the `GameNetThreadStateMaybe::` namespace** on `GameNet_BeginFileTransfer`/
   `GameNet_HandleFileTransferBlock` — these read as free opcode-handler functions dispatched
   from `GameNet_DispatchMessage` by opcode, not true C++ methods of the state object; decide
   whether to keep the namespace (harmless per Ghidra, but potentially misleading) or split the
   this-type application from the naming/namespace once TU boundaries are clearer.
6. **Older open items, all still genuinely open** (carried forward, lower priority):
   `GameNet_UnpackRosterRecord`'s 0x37-0x47 gap; `g_pNetManager` as a full struct;
   `Obj0x479168` thread-wrapper class; `FUN_00468870` `_beginthreadex` wrapper;
   `DPlaySessionMgr`'s unread methods; locale RT_STRING constants; `0x454db0`'s class;
   `CursorDescMaybe` sibling fields; `CombineFlags0x445910` (parked v2, needs a fresh angle);
   v3 parked pure-scheduling residuals `RelPosObj0x436a40::ComputeLocalPos`/
   `ResetObj0x44d870::ResetAllFields`/`PackHalfMinus1Hi/Lo0x41d920/0x41d980`; `0x466126`
   (hand-shaped stub, still uncharacterized); the ~54 remaining scalar-deleting-destructor
   family members (need real base-dtor bodies modeled first — Phase 3/4 work); `0x40e680`'s
   shared base-dtor-body `0x424ba0`.
## ⏭ NEXT SESSION PICKUP (v24 — 2026-07-12 — user-proposed `operator new` xref
address-boxing technique validated via 7-agent parallel survey, 39 functions renamed across
~10 distinct classes/subsystems, iostream "dead RTTI" note corrected)

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
both before and after this session's changes (Ghidra-only, no `src/`/toolchain changes).

Full writeup: `docs/subsystems.md`'s "Seventeenth session" section (has the complete
per-cluster candidate list — this pickup only summarizes). Method: `0x465ce0` (`new_alloc`) is
confirmed `operator new`; pulled all 339 program-wide xrefs to it, found 99 still-unnamed
`FUN_XXXXXXXX` callers (ctor candidates), split into 7 address-range clusters, fanned out 7
parallel read-only `general-purpose` agents (one per cluster) to classify each as
CTOR/BULK-ALLOC/UTILITY and apply the user's **address-boxing** heuristic (functions between a
confirmed ctor and a confirmed sibling method are likely the same class/TU). All 7 returned
strong, well-evidenced findings; a human (main thread) triaged and applied the highest-
confidence subset. **The technique is confirmed high-value — reuse it going forward**, e.g. by
re-running it on any newly-discovered vtable's xrefs, or on `operator delete`'s xrefs for the
destructor-side equivalent.

**1. `EditCardWnd` extended** — `EditCardWnd_InitFieldsMaybe` (0x415a00, confirms the 16th
session's `paDecalThumbCacheMaybe` zero-loop), `EditCardWnd_DtorMaybe` (0x4166b0),
`EditCardWnd_BuildPreviewCanvasAMaybe`/`BMaybe` (0x416460/0x4216f0) — all `this`-typed.

**2. New, previously-undocumented network layout-sharing subsystem found**: 6 functions
(`LayoutNet_*`/`LayoutSet_*`, 0x43d250-0x43d820) implementing "share your custom track layout
with other players" — a per-slot 0x4c-stride array of named `.lay` bitmaps exchanged over
DPlay via opcodes 0x3f8/0x3f9/0x3fa/0x19/0x1b.

**3. `GameNet_SendTrainStateSyncMaybe`** (0x43ae20, send-side pair of the known
`GameNet_HandleTrainStateSync`) and **`GameNet_PackAndBroadcastRosterMaybe`** (0x440070,
likely pairs with the long-open `GameNet_UnpackRosterRecord` 0x37-0x47 gap) named.

**4. `GameNetManager_HandleQueuedEventMaybe`** (0x43f2b0) named — a large (~0x490-byte)
consumer-side dispatcher over ~18 event kinds; single most valuable un-mapped function
surfaced this session, body not yet read in detail.

**5. `BigObj_CreateAndInsertMaybe`/`RemoveMaybe`** (0x41dd80/0x41def0, world-object registry)
and **`EffectSpawner_SpawnAtPositionMaybe`/`SpawnSimpleMaybe`/`ClearBothListsMaybe`**
(0x423ab0/0x423c50/0x423d00, despawn-effect system triggered from `BigObj_RemoveMaybe`) named.

**6. `TrackGraph_BuildAdjacencyAMaybe`/`BMaybe`/`PropagateDistanceMaybe`**
(0x45ce40/0x45d1c0/0x45d6c0) — track/road topology graph builder + distance-propagation,
plausible source for the still-parked `RelPosObj0x436a40::ComputeLocalPos` residual.

**7. `CarNetStateMaybe` extended** with 2 more construction paths
(`CreateFromWireRecordMaybe`/`CreateFromFileMaybe`/`InitFromWireRecordMaybe`,
0x442fa0/0x444c70/0x4428e0) **and a new sibling class `CarNetStateAltMaybe`** discovered
(0x442a10/0x442ec0, own vtable `0x478268`, 12 bytes smaller — confirmed genuinely distinct via
direct vtable check, not a duplicate). No struct modeled yet for the sibling.

**8. iostream CRT classes identified** (`ifstream`/`istrstream`/`ofstream`/`filebuf`/
`streambuf`/`strstreambuf`, 10 functions renamed) — **corrects the CLAUDE.md "dead RTTI"
note**: the RTTI type_info strings are never referenced, but the classes ARE actively
constructed for real file I/O, just never through RTTI/`dynamic_cast`. This is statically-
linked iostream.lib CRT code, not app logic — low priority to hand-transcribe (comes free at
Phase 4 link time), renamed mainly so it's not re-investigated as an unknown app class.

**9. `WorldBoardMaybe` (tentative name) partially identified** — `InitFieldsMaybe`/
`DtorMaybe`/`UpdateDirtyTilesMaybe` (0x454cf0/0x454db0/0x456150), a genuinely huge singleton
(≥0x52514 bytes ≈ 336 KB), plausibly explaining the `.data` section's oversized BSS from the
CLAUDE.md binary fingerprint. Struct NOT modeled (too large/uncertain this session) — only 3
offsets plate-commented.

**Ghidra state:** 39 functions renamed (listed above); 4 `EditCardWnd` functions `this`-typed
(2 needed `set_function_prototype` fastcall→thiscall conversion first); 3 plate comments added
(iostream CRT correction, `WorldBoardMaybe` huge-struct caveat, `CarNetStateAltMaybe` sibling
discovery). All readback-verified, `save_program` called.

**Not done this session:** the ~85 remaining surveyed-but-unapplied candidates from the 7
clusters (full list in `docs/subsystems.md`'s Seventeenth session section) — notably: the
`Obj0x477798`-family embedding ctor/dtor/deleting-dtor triples at 0x41d250/0x4238c0 (high
priority, directly extends an already-flagged family); `g_pNetManager` as a full struct (now
has strong field evidence from cluster 5's shared-singleton analysis — `DAT_004fd3ac` is very
likely its instance pointer, fields at `+0x518`/`+0x54e`/`+0x7c4`/`+0x7cc`/`+0x7e0`/`+0x7e8`
already characterized); the `0x435700`/`0x435db0` message-clone sibling ctor pair (needs its
own class name); `LocoBitmap`'s remaining un-renamed methods (`0x42a980`/`0x42ab10`/`0x42af30`,
real ctor address now pinned to `0x42a110`); the editor "category icon" `ResourceRefMaybe`
subsystem (0x42ce10-0x431a10); `CarNetStateAltMaybe`'s struct; `WorldBoardMaybe`'s struct;
the decal-slot-127 corruption bug (and the other 2 catalogued engine bugs) still not
reproduced live; `GameNetThreadStateMaybe`'s own vtable (`0x4781c4`) still unexplored; its
placeholder fields still untyped; `Obj0x479168` thread-wrapper class not renamed as a whole;
`FUN_00468870` `_beginthreadex` wrapper not renamed; `DPlaySessionMgr`'s remaining unread
methods; locale RT_STRING constants; `CursorDescMaybe` sibling fields
(`0x424e00`/`0x425670`, high-confidence, not yet renamed this session).

1. **Reproduce first:** the 5 probe files should still print 9/9, 15/20, 4/8, 1/1, 10/10.
2. **Model `g_pNetManager` as a full struct** — cluster 5's survey did the hard field-discovery
   work already; this is now tractable in one focused pass rather than needing fresh RE.
3. **Struct-model the `Obj0x477798`-family embedding ctors** at 0x41d250/0x4238c0 — two
   independently-confirmed ctor/dtor/deleting-dtor triples ready to apply.
4. **Read `GameNetManager_HandleQueuedEventMaybe`'s (0x43f2b0) body in full** — the single
   highest-value unread function surfaced this session (~0x490 bytes, ~18-way switch).
5. **Re-run the `operator new` xref address-boxing technique on remaining candidates** — only
   ~14 of 99 candidate functions got Ghidra writes this session; the other ~85 are fully
   triaged and ready to apply from `docs/subsystems.md`'s per-cluster list. Also consider
   running the same technique against `operator delete`'s xrefs for the destructor-side view.
6. **Older open items, all still genuinely open** (carried forward, lower priority):
   `GameNet_UnpackRosterRecord`'s 0x37-0x47 gap; `Obj0x479168` thread-wrapper class;
   `FUN_00468870` `_beginthreadex` wrapper; `DPlaySessionMgr`'s unread methods; locale
   RT_STRING constants; `CombineFlags0x445910` (parked v2, needs a fresh angle); v3 parked
   pure-scheduling residuals `RelPosObj0x436a40::ComputeLocalPos`/
   `ResetObj0x44d870::ResetAllFields`/`PackHalfMinus1Hi/Lo0x41d920/0x41d980`; `0x466126`
   (hand-shaped stub, still uncharacterized); the ~54 remaining scalar-deleting-destructor
   family members (need real base-dtor bodies modeled first — Phase 3/4 work); `0x40e680`'s
   shared base-dtor-body `0x424ba0`.


## v26 — 2026-07-12 — closed out v25's punch list: `0x44fb10` reclassified,
`GNetManager.bSessionJoinedMaybe` session-joined writer resolved (synchronous, not async),
confirmed `GameNet_DispatchMessage`'s "caller gap" was already closed, operator-delete
address-boxing sweep names 6 more functions

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
both before and after this session's changes (Ghidra-only, no `src/`/toolchain changes).

All 4 of v25's numbered pickup items closed this session, each done directly in the main
thread (small, well-evidenced items — no read-only-agent fan-out needed this time):

**1. `0x44fb10` reclassified** into `ResourceRefCategoryTable_LoadCategoryFileMaybe` — its sole
caller `FUN_00450240` operates on the exact same `this`-struct as `ResourceRefCategoryTable_
InitMaybe` (fields `+0x118`/`+0x120`/`+0x128`/`+0x130`/`+0x148`/`+0x3068`/`+0x3074`/`+0x303c`
all match), confirming v25's research-agent flag. It loads a category record file
(RF-archive-first, file-fallback) then hands the stream to `FUN_0044fc80` (still unnamed —
parses `0x3c`-stride records into the table array), gated one-shot by `this+0x303c`.

**2. `GNetManager.bSessionJoinedMaybe` (`+0xd50`) writer found — and the async hypothesis was
wrong.** New technique (see the CLAUDE.md bullet added this session): grepped a full-`.text`
`objdump` dump for the literal `[reg+0xd50]` store opcode across all registers instead of
reasoning from the decompiled call graph. Found 4 total writer/clearer sites: the 2
already-known clears (`DPlayLobby_Init` on init, `DPlay_TeardownConnection` on teardown) plus 2
previously-unnamed immediate-`1` "success" writers, both called directly from the already-known
`DPlay_JoinOrHostSession` — renamed to `DPlay_HostNewSessionMaybe` (0x45fd80, calls
`IDirectPlay4::Open` vtbl+0x60 to create a session, error string "Failed to Open new
session...") and `DPlay_JoinExistingSessionMaybe` (0x460360, session-browser dialog + vtbl+0x34
enum / vtbl+0x60 open, error string "Failed to join session Direct Pl..."). **The field is set
SYNCHRONOUSLY right after `Open()` succeeds in both paths — not asynchronously from a DPSYS
receive handler as the last 2 sessions had hypothesized.** While tracing this, also found and
named `GameNetThread_ResetNetManagerMaybe` (0x4391a0, was `FUN_004391a0`) — the (re)creation
entry point for `g_pNetManager` itself (destroys any existing instance, allocates a fresh
0x160c-byte block, calls the ctor, sets `hWndParentMaybe`), called from `GameNetThread_
InitState` and `GameNet_ProcessLocalCommand`.

**3. `GameNet_DispatchMessage`'s "caller gap" (`0x439220`-`0x439263`) turned out to already be
closed** — current Ghidra state has `GameNetThread_ResetNetManagerMaybe` (`0x4391a0`-`0x43923e`)
and `GameNetThread_TickLoop` (`0x439240`-`0x4393ca`) fully, contiguously covering that range
already (likely an incidental fix during v25's mass renaming that never got noted). No
`create_function` needed. Bonus: the disasm read for this confirmed `GameNetThread_TickLoop`
gates its `GameNet_DispatchMessage` call on `g_pNetManager != 0 && g_pNetManager->+0xd50 != 0`
— i.e. "only dispatch net messages once the session is joined," independently confirming
`bSessionJoinedMaybe`'s semantics from item 2.

**4. `operator delete` xref address-boxing sweep run** (v25's item 5, "hasn't been done yet").
Confirmed `0x465cd0` (`FUN_00465cd0`, wraps the same `FUN_00466c70` free-helper `new_alloc`'s
sibling `0x465ce0` wraps for alloc) is `operator delete`. Its 99 xrefs are ~90% compiler-
generated SEH `Unwind@...` cleanup thunks (noise — these are the compiler freeing
partially-constructed objects on exception unwind, not source-authored call sites) plus a
handful of already-named callers. After filtering both, exactly 6 genuinely new, unnamed
functions remained, all named this session: `GameNetMsgQueue_EnqueueOrFreeNodeMaybe` (0x4393d0
— dual-purpose: append a message node to `g_pNetManager`'s linked queue at `DAT_004fd39c` under
lock `DAT_004fd394`, OR free it (virtual-delete if payload type is 0xe/0x10, else raw free)
when the queue is full/draining — ties into the v25 `NetMsgPayload7/8` family, worth a real
struct next time), `PostBag_InitCategoryFolderMaybe` (0x444d00 — builds a Sort-category
folder path from a switch over 8 category ids incl. the 9-language Easter set, ensures the dir,
rescans+recounts its `.crd` files), `PostBagFileCache_InsertRecordMaybe` (0x401690 — generic
grow-and-insert into a `0x18`-byte-stride dynamic array; `GetCountDiv24` at 0x401810,
independently found nearby, corroborates the 24-byte/0x18 stride), `PostBagFileCache_
SaveIndexFileMaybe` (0x401c90, was `thunk_FUN_00401c90` — writes an `AlbIndex.NNN` file,
paired with the already-known `PostBagFileCache_LoadIndexedFileMaybe`), `streambuf_SetBuffer`
(0x465730 — textbook MSVC iostream.lib `streambuf::setb(base,end,allocated)`; all 3 callers are
already-confirmed CRT streambuf-family functions, not app code — named without a `Maybe` suffix
to match the confidence level of its siblings `filebuf_Open`/`strstreambuf_GrowBuffer`), and
`ThumbnailBmp_CloseStreamsMaybe` (0x447fb0 — shared stream-cleanup helper called by both
`ThumbnailBmp_LoadMaybe` and `SaveMaybe`, frees the istrstream/ofstream/raw-buffer/`+0x1c4`
quartet).

**Ghidra state:** 9 functions renamed/retyped this session (`ResourceRefCategoryTable_
LoadCategoryFileMaybe`, `GameNetThread_ResetNetManagerMaybe`, `DPlay_HostNewSessionMaybe`,
`DPlay_JoinExistingSessionMaybe`, and the 6 `operator delete`-sweep names above); 4 plate
comments added documenting the reclassification and the two writer-resolution findings. No new
struct types this session (all fixes fit existing/no struct). All readback-verified,
`save_program` called after each batch.

**Not done this session** (all carried forward, none new): `GameNetMsgQueue_
EnqueueOrFreeNodeMaybe`'s node struct not modeled (fields `0`/`2`/`4`/`6`/`0x18`-next observed
but not typed — natural next step given it ties into the `NetMsgPayload` family);
`DPlaySessionMgrProviderSlot`'s 2 unexplained byte gaps; `GNetManager`'s 4 parallel list-heads'
semantic distinction; cluster 5's remaining thinly-evidenced producers (`0x43ee80`, `0x43efa0`,
`0x43f000`, `0x43f030`, `0x43f070`, `0x440610`); the `Obj0x477798` family's 8-sibling list not
yet cross-checked for a 9th/10th member elsewhere in the binary; `FUN_0044fc80` (the per-line
category-record parser `ResourceRefCategoryTable_LoadCategoryFileMaybe` hands its stream to)
still unnamed; older carried items unchanged (locale RT_STRING constants, `WorldBoardMaybe`'s
struct, `CarNetStateAltMaybe`'s struct, `Obj0x479168` thread-wrapper class, `FUN_00468870`
`_beginthreadex` wrapper, parked scheduling residuals, the ~54 remaining scalar-deleting-
destructor family members, `0x40e680`'s shared base-dtor body).

## v35 — 2026-07-12 — disambiguated `TilePlacedObjMaybe`'s 2 larger track variants, built proper `TrackTileObjMaybe`-family structs

**Match count reproduced clean, unchanged (39/48 = 9/9, 15/20, 4/8, 1/1, 10/10)** — verified
before this session's changes (Ghidra-only, no `src/`/toolchain changes this session).

**Resolved: the 2 larger `TilePlacedObjMaybe` track variants (`FUN_00412870`/`FUN_0044f210`),
carried forward from v34.** Decompiled both in full plus their shared base `FUN_0044ae80` and
`BigObj::BigObj_CreateAndInsertMaybe`'s dispatch to see the real hierarchy: category==3 (track
family) picks a leaf by `BigObj::IsType0x63aInSet`/`IsType0x63aInSet1234` (the coarse
`m_type0x63a` classifier, distinct from the full `dwKindIdMaybe`); neither-set falls through to
`FUN_0044ae80` directly as a LEAF (not just a helper). Built 3 structs, all readback-verified
(the decompiler re-derived every `this->field` from raw offsets after retyping):
- **`TrackTileObjMaybe`** (0x11c bytes; ctor `0x44ae80` → `TrackTileObjMaybe::InitFieldsMaybe`)
  — extends `TilePlacedObjMaybe`'s common fields with `+0x10c dwTrackStateMaybe` (0–8,
  `m_type0x63a`-driven classifier), `+0x110 dwTrackTickStateMaybe` (tri-state 0/1/2 at
  runtime, ctor also sets 3–5 for other track kinds), `+0x114`/`+0x118` (zeroed, unread).
- **`TrackTileObjTypeSet1234Maybe`** (0x128 bytes; ctor `0x44f210`) — `m_type0x63a` ∈
  `{1,2,3,4}`. Chains `TrackTileObjMaybe::InitFieldsMaybe`, adds `+0x11c
  dwOccupancyClaimMaybe` (0=free) and `+0x120 pOwningTrainMaybe` (ctor writes a small-int
  sentinel — `1` for the 4 global connector kind ids `0xc42/44/46/48`
  `ITEMKIND_TRACK_GLOBAL_{E,W,N,S}`, else `0` — overwritten with a real `PeerTrainNode*` at
  runtime) and `+0x124` (zeroed, unread).
- **`TrackTileObjTypeSetMaybe`** (0x12c bytes / 300B `new_alloc`; ctor `0x412870`) —
  `m_type0x63a` ∈ `{7,8,9,0xa}`. Same base-chain + `+0x11c`/`+0x120`/`+0x124` fields
  (independently re-initialized, NOT via a shared intermediate ctor — both leaf ctors call
  `TrackTileObjMaybe::InitFieldsMaybe` directly, never each other) plus one extra
  `+0x128 bUnk0x128Maybe` byte (zeroed, unread).
- Confirmed `+0x11c`/`+0x120`'s shared semantics via `PeerTrainNode_
  UpdatePlacementTickMaybe` (0x44c3a0): on train hand-off it writes `slot[0x47]=1` (occupancy
  claim) and `slot[0x48]=(int)param_1` (real owner pointer) into the tile's plane-A slot
  object — proving both fields sit at the same offsets across whichever concrete leaf class.
- Noted, NOT modeled: `FUN_004580a0` (the 0x10c generic base) itself chains an even deeper
  unmodeled base `FUN_00405790`/`FUN_004369d0` (BigObj-family root) — left un-renamed pending
  that; a good future target if the `BigObj`/`TilePlacedObjMaybe` family gets revisited.
Full writeup in `docs/subsystems.md`'s "Tile/scenery placement and world objects" section.

**Ghidra state:** 3 new structs (`TrackTileObjMaybe` 0x11cB, `TrackTileObjTypeSet1234Maybe`
0x128B, `TrackTileObjTypeSetMaybe` 0x12cB); 3 functions this-typed+renamed to
`InitFieldsMaybe` in their respective new namespaces (0x44ae80/0x44f210/0x412870). All
readback-verified, `save_program` called after the batch. `docs/subsystems.md` updated in
place (the carried-forward bullet marked RESOLVED with full writeup, no stale duplicate text).
