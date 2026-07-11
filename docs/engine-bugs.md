# Original engine bugs

Verified-against-disasm defects in the shipped `Loco.exe` that our byte-exact source must
faithfully reproduce. Each reproduction site in `src/` is marked with a `// sic:` comment. Do
NOT "fix" these — matching them exactly is the point.

- **`TimeOfDay_IsDateInWindowMaybe` (`0x412670`, transcribed v522, `src/TimeOfDayMaybe.cpp:35`)
  can never fail a wrapping date window.** The non-wrap arm tests
  `nOpenDay <= nNowDay && nNowDay <= nCloseDay`, but the wrap arm (open day > close day) is
  spelled `nOpenDay >= nNowDay || nNowDay >= nCloseDay` — which is *always* true whenever the
  window wraps (if `nNowDay < nCloseDay < nOpenDay` then `nOpenDay >= nNowDay`; otherwise
  `nNowDay >= nCloseDay`), so a wrapping date window never closes. Compare the sibling
  `TimeOfDay_IsDateTimeInWindowMaybe` (`0x412790`), whose minute arm uses the correct inverted
  form. Reproduction site: the wrap-arm return in `TimeOfDay_IsDateInWindowMaybe`, marked
  `// sic:` there.

- **`WindowBase::Create` (`0x425b70`, transcribed v170, `src/WindowBase.cpp`) ignores its own
  `dwStyleMaybe`/`dwExStyleMaybe` parameters.** `ret 0x2c` proves the function takes 11 real
  stack params (Ghidra's auto-analysis only inferred 9), and every real caller passes distinct
  values for the trailing pair (`CreateFullscreenPopupWnd` passes `0x81000000`/`0`) — but
  the function body hardcodes its own `CreateWindowExA` style (`0x87000000`) instead of reading
  either parameter anywhere (confirmed via full raw disasm: no reference to either param's
  stack slot exists past the prologue). Same "dead but real argument" shape as
  `ShowFatalErrorMessageBoxMaybe`'s own `nErrorCodeMaybe` (see below) — a genuine,
  reproducible defect, not decompiler noise (caller-side stack cleanup proves the arguments are
  real). Reproduction site: the two unused parameters in `WindowBase::Create`'s own signature,
  marked `// sic:` there.
- **`ShowFatalErrorMessageBoxMaybe` (`0x463600`, transcribed this session, `src/WindowBase.cpp`)
  never reads its own `nErrorCodeMaybe` parameter.** Both known call sites push a real,
  distinct per-site constant with caller-side `add esp,4` cleanup after the `call`
  (`WindowBase::AcquireWorkSurfaceDCMaybe` passes `0x48`; an untranscribed sibling at
  `0x414be9` passes `0x49`) — textbook cdecl 1-int-param shape, not a decompiler
  hallucination — but the callee's own raw disasm never reads the value past its own locals:
  it builds a fixed resource-id-0x14a message and shows it via `MessageBoxA` regardless of
  which error code the caller thought it was reporting. Reproduction site: the unused
  parameter in `ShowFatalErrorMessageBoxMaybe`'s own signature, marked `// sic:` there.
- **`WindowBase::CommitScreenUpdate` (`0x426b90`, first-draft transcribed v175,
  `src/WindowBase.cpp`) can blit an uninitialized dirty rect on the first cursor redraw.**
  When `pUpdateRectMaybe != NULL` but `rectLastCursorDrawMaybe.right == 0` (no previous cursor
  frame to union against yet), the function's 2nd `UnionRect` call — which fills the scratch
  `rectUnion2` — never executes, yet the final `Ddraw_BltUpdateRect` call unconditionally reads
  `&rectUnion2` as its dirty rect. Ground-truthed via raw disasm: the `lea`/stack-offset that
  computes the dirty-rect argument at the final call site is byte-identical whether or not the
  2nd `UnionRect` call actually ran earlier in the same function — there is no runtime guard.
  Reproduction site: `rectUnion2`'s declaration in `CommitScreenUpdate`, marked `// sic:`
  there.

- **`GNetManager::DPlay_InitConnection` (`0x45f390`, transcribed v281, `src/GameNet.cpp`) leaks
  the DirectPlay COM object on 2 of its 4 late failure paths.** Ground-truthed via raw disasm:
  the whole function only calls `DPlay_TeardownConnection` (`0x45fc30`) TWICE — once at
  function entry (the `if (pDirectPlay4Maybe != 0)` reset) and once in a tail shared by the
  `CreateCompoundAddress`-failed and `InitializeConnection`-failed paths. The two intermediate
  "Direct Play final Create failed"/"Direct Play final Query Interface failed" fallback
  handlers (reached only via the rare `CoCreateInstance`-then-`DirectPlayCreate`-fallback path)
  report the error and `return 0;` WITHOUT calling teardown, leaving `pDirectPlay4Maybe` and/or
  `pTempDPlayIfaceMaybe` set to a live-but-abandoned COM pointer — a genuine reference leak on
  a rare fallback path, not decompiler noise. Reproduction sites: the two `// sic:` comments in
  `DPlay_InitConnection`'s fallback-`DirectPlayCreate`/fallback-`QueryInterface` failure blocks.

The rest are catalogued from disasm/decompile reads only, for when Phase 4 TU work reaches
them:

- **REFUTED 2026-07-16 (v135): the "`EditCardWnd::TeardownBuiltUiMaybe` releases
  `pPreviewCanvasBMaybe` through a wrong vtable slot" claim below (v134) was built on a
  mis-attributed sibling function, not a real engine bug.** The v134 write-up used
  `EditCardWnd_BuildPreviewCanvasBMaybe` (`0x4216f0`) as proof the field held a real
  `LocoBitmap*` — but that function's `this` had been mistyped: its ONLY caller
  (`SplashWnd::FUN_00421500`, itself already correctly namespaced) unambiguously passes a
  `SplashWnd*`, and Ghidra's own independently-built `SplashWnd` struct already names that same
  offset `pDrawTargetMaybe`. Retyped `0x4216f0`'s `this` to `SplashWnd*` and renamed it
  `SplashWnd::BuildDrawTargetCompositeMaybe` — it has nothing to do with `EditCardWnd`.
  Cross-checking `EditCardWnd`'s OWN field at that offset instead (via its real writer,
  `FUN_00417f20`, the "build" counterpart to `TeardownBuiltUiMaybe` sharing the same
  `bNeedsCleanupMaybe` gate in the opposite state): it's populated from
  `TileKind::TileKind_GetOrLoadDescriptorMaybe`, whose real return type is a `BigObj*`
  (`docs/subsystems.md`'s `BigObj` entry) — a class confirmed to derive from `Obj0x4779e0` <-
  `CursorDescMaybe`. `TeardownBuiltUiMaybe`'s vtable-slot-2 call is simply the INHERITED,
  un-overridden `CursorDescMaybe::ReleaseRefMaybe` (ground-truthed by reading `BigObj`'s own
  vtable, `0x478358`, slot by slot: slot 2 resolves to `0x4257f0` =
  `CursorDescMaybe::ReleaseRefMaybe`) — a completely ordinary, correct virtual call, not a bug.
  `src/EditCardWnd.cpp` updated to call `pPreviewIconDescMaybe->ReleaseRefMaybe()` directly (the
  field retyped `CursorDescMaybe*`, renamed from `pPreviewCanvasBMaybe`; its paired
  `nSelectedItemMaybe` sibling field similarly renamed `pPreviewIconRealizedMaybe`, since it
  holds `GetOrLoadFrameBitmapMaybe`'s return value, not a selected-item index) — the
  `LocoBitmapVtblSlot2ProbeMaybe` hack is gone, still confirmed EXACT byte match (362/362
  bytes) with the simpler, correct source. Original (now-refuted) claim struck through below
  for history; don't reproduce a "sic:" workaround for this call.
  ~~`EditCardWnd::TeardownBuiltUiMaybe` (`0x4180a0`) releases `pPreviewCanvasBMaybe` through a
  vtable slot that doesn't belong to its own class~~ — see the correction above; the field is a
  `BigObj*`/`CursorDescMaybe*`, not a `LocoBitmap*`, and slot 2 is its real `ReleaseRefMaybe`.

- **`TutorialWnd::OnExit` (`0x450ae0`) double-releases a `DSoundChannel*`.** Releases
  `this+0x158` unconditionally, then — if `DAT_004fd3bc != 0` — releases it again, with no
  null-out in between. Found 2026-07-11 reading the function for renaming; not yet traced to
  confirm real-world impact (may be masked by the pointer being stale/reused, or may be a
  real double-free depending on `DSoundChannel::~DSoundChannel`'s guard behavior).
- **`MailWnd::OnButtonMouseUp` (`0x42de70`) leaves button 6 visually stuck down.** The
  mouse-up handler for button 6 only releases the button's pressed sprite state when a
  computed live-session count is nonzero; when the count is zero it `break`s out early
  without releasing, leaving the button rendered in its pressed state until some other event
  resets it. Found 2026-07-11 reading the function for renaming; not yet reproduced live.
- **REFUTED 2026-07-13 (v60): the "`CarNetObj_Ctor` clobbers decal slot 127" claim below was
  an artifact of `CarNetStateMaybe` being mismodeled 12 bytes too large (see the
  `CarNetStateMaybe` correction in `docs/subsystems.md`), not a real engine bug.** With the
  corrected 924-byte (`0x39c`) embedded struct, `decalSlotsMaybe` really ends at
  `CarNetObj+0x88+0x396` = `+0x41e`, well BEFORE the `+0x428`/`+0x42c` writes below — those
  offsets land in genuine `CarNetObj`-level fields instead (a ctor-param pointer at `+0x428`,
  not yet named, and what raw disasm shows is really `pNameAMaybe` at `+0x42c`, not the
  `carSubIdMaybe` echo originally guessed — the existing Ghidra `CarNetObj` struct's
  `pNameAMaybe`/`pNameBMaybe` at `+0x430`/`+0x434` are consequently likely off by 4 bytes too
  and need their own re-verification pass). Original (now-refuted) claim kept below struck
  through for history; do not treat slot 127 as corrupted.
  ~~`CarNetObj_Ctor` (`0x40d500`) permanently clobbers decal slot 127 on every car.~~ The
  embedded `CarNetStateMaybe` sub-object at `CarNetObj+0x88` runs its own ctor
  (`CarNetStateMaybe_CarNetStateMaybe`, `0x442850`) first, zero-initializing the full
  128-entry `decalSlotsMaybe` array. Right after, `CarNetObj_Ctor` unconditionally writes a
  4-byte `mov DWORD PTR [esi+0x428],edi` and (conditionally) `mov DWORD PTR [esi+0x42c],eax`
  — under the OLD (wrong, 936-byte) `CarNetStateMaybe` size these appeared to land inside
  `decalSlotsMaybe[127]`; under the corrected 924-byte size they don't. Found 2026-07-11 while
  modeling `CarNetObj`/`PeerTrainNode` as full Ghidra structs; refuted 2026-07-13 once the
  underlying struct-size bug was found and fixed.
- **`EditCardWnd::HandleLButtonDownMaybe`'s DDraw "dissolve" transition passes a garbage
  dirty-rect to `CommitRectUpdateMaybe` on every animation frame (2026-07-18, first-draft
  transcription, `src/EditCardWnd.cpp`).** Inside the iris-reveal loop (triggered on the
  "commit card and exit edit mode" path when the card already has a name), the `RECT` argument
  passed to `CommitRectUpdateMaybe` each frame is built from 4 unrelated leftover
  stack values -- `{left=0, top=(int)this->pPointCursorRectMaybe, right=(int)
  this->pPointCursorDescMaybe, bottom=0}` -- i.e. two raw pointer VALUES reinterpreted as pixel
  coordinates, not a real rect. These are stale reads of the args built for an earlier,
  unrelated vtable-slot-0xc call a few statements above; the raw disasm confirms the same stack
  slots are simply re-read, not recomputed. Likely harmless in practice (the real screen content
  was already correctly blitted by the two `Blt` calls immediately above in the same loop
  iteration, so this call may just be a redundant/no-op dirty-mark) -- `CommitScreenUpdate`'s own
  body neither validates nor clamps a non-NULL rect argument, so garbage does flow through to
  `Ddraw_BltUpdateRect`, consistent with "sic" rather than "impossible". Reproduced verbatim as
  `garbageRectSic` in source, not fixed.
- **`CarNetStateMaybe::AddDecalMaybe`'s 6th/7th call args (from `HandleLButtonDownMaybe`) read
  unrelated bytes out of the placed decal's thumbnail `LocoBitmap` (2026-07-18, first-draft
  transcription).** The call site computes `pThumb = paDecalThumbCacheMaybe[slot]` then passes
  `*(byte*)(pThumb+8)` and `*(byte*)(pThumb+0xc)` as the trailing two params -- per
  `src/LocoBitmap.h`'s real field layout these are the LOW BYTE of `LocoBitmap::height` (a
  4-byte `int` at `+8`, so only its bottom 8 bits survive) and `LocoBitmap::bOwnsPalette` (a
  real byte field at `+0xc`, but semantically an unrelated "does this bitmap own its palette"
  flag, not a decal placement dimension). Whether this is a genuine placement-size bug (the
  decal's real width, stored at `LocoBitmap::width`/`+4`, is never read at all) or intentional
  is unconfirmed -- reproduced verbatim via `*(unsigned char *)&pThumb->height` and
  `pThumb->bOwnsPalette`, not fixed.
- **`EditCardWnd::AnimateDecalPickerScrollMaybe` (`0x418e20`, transcribed v220,
  `src/EditCardWnd.cpp`) reads its own "should the old page slide out" flag uninitialized on
  one path.** The flag is written via `setne` from the page-toggle's own "was this buffer ever
  populated" byte on BOTH toggle branches, so it always starts as `(bOldReadyMaybe != 0)` --
  but a THIRD write, `cVar8 = 1`, only happens inside the `if (bOldReadyMaybe == 0)` block (the
  one-shot `RestoreOverlapBlt` prep). Since `bOldReadyMaybe == 0` is exactly the case where the
  flag would otherwise read `0`, the two writes are exhaustive in practice (the flag is always
  effectively `1` by the time the wipe loop reads it) -- but the raw disasm shows the ORIGINAL
  compiler kept a genuine runtime `test`/`je` at the loop's read site rather than folding it to
  a constant, which only happens if the true source variable is left uninitialized on the
  `bOldReadyMaybe != 0` path (relying on stack-slot reuse with the `setne` result to coincide)
  rather than being explicitly assigned there. Reproduced by leaving `bDrawOldSlideMaybe`
  genuinely uninitialized on that path (a real, if relatively benign, UB read) instead of the
  more obviously-correct `= (bOldReadyMaybe != 0)` assignment, which our compiler DOES fold to
  a constant and eliminates the branch entirely -- not fixed. See `docs/PARKED.md` for the
  function's own remaining byte-match residual.
- **`EditCardWnd::BuildPreviewCanvasAMaybe` (`0x416460`, transcribed v307,
  `src/EditCardWnd.cpp`) blits 3 of its 4 source icons for nothing.** Builds a lazily-allocated
  1280x1024 `pHelpBitmapCache` from 4 `TileKind` icon frames (ids `0x3caa`/`0x3cc4`/`0x3cc5`/
  `0x3cc6`), each composited via `LocoBitmap::BlitOntoBitmap` — but every one of the 4 calls
  passes the SAME full-canvas rect `{0,0,0x500,0x400}` as both `destRect` and `srcRect` (no
  per-icon offset, no read of the loaded icon's own width/height), ground-truthed
  instruction-for-instruction against the raw disasm for all 4 blocks. Each blit therefore
  fully overwrites the entire canvas, so only the LAST icon (`0x3cc6`) is ever visible in the
  finished `pHelpBitmapCache` — the first 3 `TileKind` loads + blits + releases are pure dead
  work with no observable effect. Reproduced verbatim (all 4 blocks transcribed as-is), not
  "optimized" down to a single call — see `docs/PARKED.md` for the function's own remaining
  byte-match residual.
- **`ScreenSaver::Tick` (`0x448120`, transcribed v366, `src/ScreenSaver.cpp`) re-randomizes only
  3 of the 4 peer-train slots.** In screen-saver/attract mode the tick sweeps
  `g_PeerTrainSlotQueue.aSlots` every 0x800 frames and re-picks each parked train's selected car
  from its own endpoint pair. The loop's trip count is 3 (`mov edi,3` / `dec edi; jne`, an
  unambiguous 3 iterations over a 4-entry array), while every other sweep over the same array in
  the binary runs 0..3 — the two `SetSoundStateMaybe` loops in the frame driver at `0x45c3c0`
  (`iVar2 = 4`), `PeerTrainSlotQueueMaybe::ResetAllFields`, and
  `NetSessionEventQueue`'s layout writer. The train parked in slot 3 therefore never shuffles its
  rolling stock while the screen saver runs. Reproduced as `for (i = 0; i < 3; i++)` with a
  `// sic:` marker, not fixed. (The function is an EXACT match with the 3, so the count is not a
  transcription guess.)
- **`EditCardWnd::RefreshClientClipRect` (`0x417180`, rewritten v371, `src/EditCardWnd.cpp`)
  lays out the 5th decal-kind tab against the 3rd tab's bottom edge, not the 4th's.** The six
  decal-kind tabs are laid out as a left-to-right chain: each takes `rect.bottom` from the
  PREVIOUS tab's `rect.bottom` and `rect.left` from the previous tab's `rect.left + 0x3d`. Tab
  5 breaks the chain on the bottom edge only — `0x417d3e` loads `[esi+0x310]`
  (`pDecalKindBtn3`) for the bottom while `0x417d51` correctly loads `[esi+0x314]`
  (`pDecalKindBtn4`) for the left, an unambiguous copy-paste slip in the original source. It is
  invisible at runtime because every tab bitmap shares the same height, so tab 3's and tab 4's
  bottoms are always equal. Reproduced verbatim with a `// sic:` marker, not fixed.
- **`MailWnd::OnButtonMouseUp` (`0x42de70`, transcribed v381, `src/MailWnd.cpp`) can leave the bin
  button visually stuck down.** The handler repaints a button's released sprite when its command
  id comes in; for command 6 (`paButtons[4]`, the bin) it first restores the backdrop under the
  button, then recomputes the viewed category's live card count and only draws the released
  sprite `if (nCount > 0)`. Deleting the LAST card in the category takes that count to zero on
  the very release that caused the deletion, so the `else` path falls out of the switch having
  erased the button but never redrawn it — the bin stays in its pressed state until something
  else repaints the area. Every other command id in the same switch repaints unconditionally.
  Reproduced with a `// sic:` marker, not fixed. (The function is not yet byte-exact, but the
  guard itself is unambiguous in the raw disasm at `0x42dfde`: `test ax,ax; jbe <epilogue>`.)
- **`MailWnd::PromptForAttachmentSavePathMaybe` (`0x42eea0`, transcribed v384, `src/MailWnd.cpp`)
  builds the save dialog's file-type filter and then never installs it.** The function carefully
  derives a filter string from the attachment's own name — `char szFilter[0x104] = "*.";` followed
  by either `strcat(szFilter, szDefExt)` (when the source name has an extension) or
  `strcat(szFilter, "*")` (when it does not) — and then hands `GetSaveFileNameA` the EMPTY string
  literal instead: the only write to `OPENFILENAME+0x0c` (`lpstrFilter`) is
  `mov [esp+0x2c],0x4851d0`, the pooled `""`. `szFilter`'s stack slot is written by the two
  strcat/init blocks and read by nothing, and `lpstrCustomFilter` (`+0x10`) stays zeroed from the
  `memset`. The dialog therefore shows no file-type list at all, and the whole filter derivation
  is dead work. Reproduced verbatim with `// sic:` markers, not fixed. (The function is an EXACT
  match with the dropped filter, so this is not a transcription guess — a version that passed
  `szFilter` could not byte-match.)

## `DecorActorBase::~DecorActorBase` (0x433be0) leaks a building's occupancy count

`src/DecorActor.cpp`. When an ambient actor is destroyed it unhooks itself from the building
that spawned it: scan that building's 5-entry `apSpawnedActorMaybe` array for `this`, clear the
matching slot, and decrement `bSpawnedActorCountMaybe`.

The "not found" path — all 5 slots scanned without a hit — nulls the actor's own
`pOwnerObjMaybe` and returns **without decrementing the building's count**. Any desync between
the slot array and the counter is therefore permanent, and each occurrence permanently consumes
one of that building's 5 spawn slots as far as the counter is concerned.

Reproduced faithfully (`// sic:` at the early return), not fixed.
- **`RoadVehicleActor::ChooseNextStepTileMaybe` (`0x453790`, transcribed v392,
  `src/RoadVehicleActor.cpp`) makes TWO `CalcSqDist` calls whose results it never uses.** Both
  are the remains of a test that `WalkerActor::ChooseNextStepTileMaybe` — the function this one
  was plainly copy-and-edited from — still performs:
  - in the "standing on the destination tile" case, the walker has
    `if (CalcSqDist(x, y, ptDest) < 0x901) { step = dest; goto out; }`; the road version keeps
    the call and drops the test, so it always falls through to the `(-1,-1)` stop. In play that
    means a vehicle can never leave the road network for the last short hop to its actual
    destination — it stops on the road and re-decides;
  - in the off-network case, the walker computes the destination distance in order to compare it
    against the network-entry distance (`if (nDestSqDist < nEntrySqDist) head for the
    destination instead`); the road version computes it and then unconditionally heads for the
    entry tile.

  Both are verified against raw disasm, not inferred from the decompiler: at `0x453d1d` the
  `call 0x45c7a0` is followed immediately by `add esp,0x10` and then `or eax,0xffffffff`, which
  overwrites the return value before any use. A dead comparison would have left the `cmp`
  behind; a dead call would have been eliminated only if the compiler knew `CalcSqDist` were
  pure, which it does not. Reproduction sites: the two bare `CalcSqDist(...);` statements in
  `RoadVehicleActor::ChooseNextStepTileMaybe`, both marked `// sic:`.
- **`TutorialWnd::OnMouseMove` (`0x4517b0`, transcribed v397, `src/TutorialWnd.cpp`) is missing
  three `break`s: its four hover-cursor switch arms fall through.** The handler asks
  `HitTestControl` which control the pointer is over, then decides between the popup's two
  preloaded cursors. Each arm is meant to be "over nav button N, and button N is enabled ⇒ show
  the hover cursor, otherwise the resting cursor":

  ```
  case 1: if (bErrObj1Loaded == 1) { hover; return 0; }   // no break
  case 2: if (bErrObj2Loaded == 1) { hover; return 0; }   // no break
  case 3: if (bErrObj3Loaded == 1) { hover; return 0; }   // no break
  case 4: if (bErrObjsLoaded  == 1) { hover; return 0; }
          break;
  ```

  With the fall-through, a pointer over a DISABLED button 1 does not fall back to the resting
  cursor — it goes on to test button 2's enabled byte, then button 3's, then the shared
  `bErrObjsLoaded`. So hovering a greyed-out "prev" button still shows the hover cursor whenever
  any LATER control happens to be enabled, and the effect is asymmetric: button 4's arm is the
  only one that can reach the resting cursor, and codes 5–8 never enter the chain at all.

  This is a genuine source-level defect, not a decompilation artifact: it is visible directly in
  the jump table at `0x451864`, whose four entries (`0x451803`/`0x45180b`/`0x451813`/`0x45181b`)
  are the four consecutive `cmp`/`je` pairs, each falling into the next. The sibling
  `OnLButtonDown` (`0x451540`), which switches on the same hit codes, has no fall-through
  anywhere — so this is not a house style.

  Reproduced faithfully (`// sic:` on the function and on each fall-through), not fixed.

- ~~**`DecorObjMgrMaybe::TestRectAgainstAllActorsMaybe` (`0x435020`) throws away its entire
  result.**~~ **RETRACTED 2026-07-26 (v415) — this was never a bug, only a decompiler artifact.**
  The claim came from Ghidra's decompile of the function, which has no prototype set and takes its
  `RECT` BY VALUE; Ghidra mis-framed the stack, aliased the `nResult` slot at `[esp+0x10]` onto a
  local `RECT`, and so rendered the tail as a bare `return 0`. The raw disassembly stores 7 at
  `0x4350f1` and 8 at `0x4351cf` and returns that slot at `0x4351e7`, exactly like its caller
  `TestActorCollisionMaybe` — and the function is now transcribed and EXACT in `src/DecorActor.cpp`
  from the disassembly, which is the proof. Kept as a retraction rather than deleted because the
  general lesson is worth having: on a by-value-struct function with no prototype, Ghidra's
  decompile can invent a plausible-looking dead-code bug out of nothing.

- **`DecorObjMgrMaybe::ResolveClickMaybe` (`0x434c50`, EXACT, `src/DecorActor.cpp`) guards only ONE
  of its two registry walks against empty slots.** The category-7 (walker) pass tests
  `pActor != 0 && pActor->bReady` before dispatching; the category-8 (road vehicle) pass, which is
  otherwise the same code, tests neither and calls the actor's own virtual `Contains` straight
  through whatever `GetAt(i)` returned. A registry-8 slot that is occupied-but-null therefore
  dispatches through a NULL vptr. Reproduced with a `// sic:` marker, not fixed. (The two passes
  disagree in the other direction too: only the vehicle pass additionally refuses to start a drag
  while `PlacementCursorMaybe::bSnapLockMaybe` is held. That one is asymmetric but not a defect.)

- **`DecorObjMgrMaybe::DeregisterEntryMaybe` (`0x434b60`, EXACT, `src/DecorActor.cpp`) mutates the
  registry OUTSIDE the lock that exists to protect it.** It resolves the category, then runs the
  full remove (`IndexOfMaybe` + `RemoveAtShiftingTail`, which memmoves the tail down and
  decrements the count) and only THEN takes `lockAMaybe`/`lockBMaybe`; the critical section covers
  just the live-count decrement and the `delete`. Since the same registries are walked from the
  idle worker thread, the array shift races. Reproduced with a `// sic:` marker, not fixed.

- **`NameAnchorMaybe::TryMatchSocketMaybe` (`0x40b880`, PARKED, `src/NameAnchorMaybe.cpp`)
  dereferences the neighbouring tile before it null-checks it.** The guard is written
  `if ((unsigned char)TileKind_GetCategory(pNeighbour->pKindDesc->resourceId) != 3 ||
  pNeighbour == 0)` — the category lookup loads `pNeighbour->pKindDesc` and then `->resourceId`
  off it, and only the SECOND operand of the `||` asks whether `pNeighbour` was null at all.
  The compiled code has the same order (`mov edx,[edi+0x40]` at `0x40b924`, `test edi,edi` at
  `0x40b93b`), so this is the source's own operand order, not a scheduling artifact. In practice
  it is masked: `WorldBoardMaybe::GetPlaneASlotGridPosMaybe` has already been asked about the
  same cell one instruction earlier and answered a non-negative origin, which only happens when
  that cell is occupied — so the paired `GetPlaneASlotMaybe` call cannot return null unless the
  two disagree. Reproduced in source order with a `// sic:` marker, not fixed.

- **`AnimDescRefObj0x477488::BlitOverlayFrameMaybe` (`0x405fd0`, PARKED, `src/WidgetBase.cpp`)
  computes its whole source rectangle, then throws it away.** The function opens with a near
  verbatim copy of `BlitAnimFrameMaybe`'s body: intersect the dirty rect, split on the subframe's
  `Unk0x16Maybe` selector, and `SetRect` a source rectangle — the mirrored variant in one arm, the
  plain one in the other. It then bumps the anim frame by +1 through vtable slot 8 (which re-points
  `rectViewport` at the next frame's slice) and calls `SetRect` on **the same local** with the
  PLAIN formula, unconditionally, before blitting. The first result is never read. Two consequences:
  the mirrored-source case is dead for the overlay layer (though its `0x20` blit flag still rides
  along, so the flag and the geometry disagree), and the pre-bump computation is pure waste.
  This reads as a copy-paste leftover rather than a deliberate recompute. ⚠ Ghidra's decompiler
  hides it, rendering the two calls as writes to different locals; the raw disasm has both
  `lea`-ing `entry_esp-0x10`, and the stack model is pinned independently by the `flag` parameter
  test at `0x406127` landing exactly on `entry_esp+0x14`. Reproduced with a `// sic:` marker,
  not fixed.

- **`HasOpaquePixelInWorkSurfaceRect` (`0x42c9f0`, PARKED, `src/LocoBitmap.cpp`) answers a
  question about the wrong surface, and then keeps scanning after it already knows the answer.**
  Two independent defects in one 284-byte function:
  1. **It ignores the bitmap it was asked about.** `LocoBitmap::HasOpaquePixelInRect` (`0x42c950`)
     is a member, and on a CONVERTED (DirectDraw-surface) bitmap it forwards to this function —
     which is a free `__stdcall`, takes only the RECT, and never receives or reads `this`. What it
     actually scans is the process-wide work surface `g_pDDrawWorkSurface`. So the converted path
     reports whether anything is opaque in that rectangle of the *shared back buffer*, not of the
     caller's bitmap. Confirmed from the raw disasm: the function clobbers ECX with a literal
     (`mov ecx,0x1f`, the `rep stos` count) in its very first block, so the incoming `this` in ECX
     is destroyed before it could ever be used.
  2. **The row advance is wrong after an early exit.** When the inner column loop finds an opaque
     pixel it sets the result flag and `break`s — but only out of the INNER loop. The outer loop
     then applies the fixed row stride `p += nStride - nCols`, which is only correct if the cursor
     walked the whole row. Having broken out early, it did not, so `p` is left short by the
     remaining columns and drifts further off every subsequent row, reading progressively further
     outside the intended rectangle (and eventually past the locked surface span) for no purpose —
     the return value can no longer change. A `return` in place of the `break` would have been both
     correct and faster; the sibling `HasOpaquePixelInRect` does exactly that.
  Both reproduced with `// sic:` markers, not fixed.

- **`PopupWndBase::LoadCursorSlots` (`0x414130`, EXACT, `src/PopupWndBase.cpp`) writes 32 bits
  through each of two 16-bit scratch locals.** After creating the process-wide 256x256 cursor
  composite surface it calls `DDraw_QuerySurfaceDims(surface, &w, &h)` — whose two out-parameters
  are `unsigned int *` — with the addresses of two `unsigned short` locals. Each store therefore
  runs 2 bytes past its variable. The frame pins this independently: the function's whole 0x80 of
  locals is the 124-byte `DDSURFACEDESC` plus exactly 4 more bytes, and the two pointers passed
  are 2 bytes apart (`esp+0xa` for width, `esp+0x8` for height), so the pair cannot be anything
  wider. Harmless in practice, which is presumably why it survived: both results are dead (nothing
  ever reads them back), and the 2 bytes each write overruns land in the descriptor's `dwSize`
  field, which the very next call (`Ddraw_QuerySurfacePixelFormat`) refills from scratch.
  Reproduced with a `// sic:` marker and explicit `(unsigned int *)` casts, not fixed.

- **`GameNetThreadState::NetResource_RequestMissingAppearances` (`0x438e40`, EXACT,
  `src/GameNet.cpp`) sends 2 bytes of uninitialized heap in every clip-art request.** Each
  missing appearance is requested with a 6-byte `ClipartRequestWireMsg` built by hand on a raw
  `operator new(6)`: the code writes the opcode as a 16-bit store at +0x0 and the two selector
  bytes at +0x4/+0x5, and never touches +0x2/+0x3. Those 2 bytes ship over DirectPlay as whatever
  the allocator last left there. Same idiom (and same defect) as `FileBlockWireMsg`'s outbound
  build in `NetFile_PumpPendingTransferSend` and the "leaving" 0x3fd message in
  `GameNet_TeardownAndFlushQueues`, so it is a house style rather than a one-off slip. Harmless to
  gameplay — the receiving 0x3ed handler never reads the gap — but it is an information leak
  across the network. Reproduced (the struct declares `wPad` and nothing assigns it), not fixed.

- **The 0x3ed clip-art REQUEST and the 0x3ee clip-art PUSH disagree about byte order.** The
  request built by `NetResource_RequestMissingAppearances` puts the index byte at +0x4 and the
  category/desc byte at +0x5; the `ClipartFileWireMsg` reply consumed by `GameNet_DispatchMessage`
  reads them the other way round (desc at +0x4, index at +0x5). Both are verified against raw
  disasm at their own call sites — the request's own `PostBag_BuildClipartFilePath(bDescByte,
  nIndex, ...)` argument order pins which byte is which. Modeled faithfully as two structs with
  opposite field order rather than reconciled.

- **`WorldBoardPartial::CheckInsertSeqPerimeterMaybe` (`0x456d90`, EFFECTIVE,
  `src/WorldBoardMaybe.cpp`) visits the bottom-left corner of the ring TWICE, and so consumes one
  more InsertSeq entry than the ring has tiles.** The function walks the one-tile border around a
  placed object's footprint as four loops sharing one index into the kind descriptor's InsertSeq
  array, handing off between loops by undoing the previous loop's overshoot and stepping the other
  axis: `x--; y++;` between the top row and the right column, `y--; x--;` between the right column
  and the bottom row — but only `x++` between the bottom row and the left column, with no matching
  `y--`. The bottom-row walk already runs one column past the footprint (down to
  `start.wPosX - 1`), so the left-column walk restarts on the very tile it just finished. A
  `cols x rows` footprint therefore consumes `2*cols + 2*rows + 5` entries for a ring of
  `2*cols + 2*rows + 4` tiles, and every entry from the bottom-left corner onward is checked
  against the wrong tile — the left column is compared against the sequence shifted by one. Since
  the `.dat` files that author these sequences were tuned against the shipped executable, the data
  presumably absorbs the shift. Reproduced with a `// sic:` marker, not fixed.

- **`WorldBoardPartial::IsTrackGraphNodeAMaybe` (`0x4573e0`, EXACT, `src/WorldBoardMaybe.cpp`)
  reuses its four-entry branch array as the diagonal-neighbour scratch, so its last two checks
  inspect the wrong tiles.** The function fills `apBranch[0..3]` with the four ORTHOGONAL
  neighbours that survived the link-cost gate, counts them, and then — for graph selector 7 on a
  full 4-way crossing only — OVERWRITES all four entries with the four DIAGONAL neighbours in
  order to discount the ones whose category is 0xc. The final step, which forces a node whenever
  a neighbour's kind id is 0xc50 (slots 0 and 2) or 0xc52 (slots 1 and 3), then reads whatever is
  left in the array: the diagonals on every path that took the selector-7 branch, the orthogonal
  neighbours on every other path. The two cases cannot both be intended, since the slot->kind-id
  pairing is positional and the diagonals are in a different order. The compiler's own
  out-of-line block at 0x457695 (which reloads `apBranch[2]`/`apBranch[3]` into esi/ebx before
  rejoining the shared tail) is what proves the reuse is in the original source rather than a
  misreading of the decompiler. `IsTrackGraphNodeBMaybe` (0x4576b0) has no diagonal step and so
  is unaffected. Reproduced with a `// sic:` marker, not fixed.

- **`Ddraw::DDraw_CreateSurfaceFromFile` (`0x401000`, EXACT, `src/DDrawSurface.cpp`) retries a
  failed video-memory surface creation with the SAME video-memory caps it was just refused.** The
  function picks `ddsCaps.dwCaps` from its `bVideoMemory` argument —
  `DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY` (0x4040) when it is 1, and
  `DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY` (0x840) otherwise — then calls
  `IDirectDraw2::CreateSurface`. On failure it enters a retry block guarded on `bVideoMemory == 1`
  (i.e. only the video-memory request gets a second chance, which is the shape of a
  "fall back to system memory" path), logs the HRESULT through `Ddraw_HResultToString`, re-stamps
  `ddsCaps.dwCaps`... with the literal 0x4040 again, and re-issues the identical call. The second
  attempt therefore asks the driver for exactly what it just declined, and the only way out of the
  block is the `OutputDebugStringA("DDINIT - failed to create surface")` bail. Almost certainly a
  typo for the 0x840 the non-video path already uses one branch above. Pinned byte-wise: the store
  at 0x4010fd is `c7 84 24 94 00 00 00 40 40 00 00`, and 0x4040 is the same immediate the
  `bVideoMemory == 1` arm of the ternary at 0x4010ad..0x4010cb computes. Reproduced with a `// sic:`
  marker, not fixed.

- **`PeerTrainSlotQueueMaybe::CheckDerailCollisionMaybe` (`0x44e3f0`, PARTIAL,
  `src/PeerTrainSlotQueueMaybe.cpp`) guards its per-car loop with a test that can never be true.**
  The OUTER, per-slot guard filters candidate trains with
  `dwModeAMaybe != 2 && dwModeAMaybe != 3 && dwModeBMaybe != 2` — the three mid-manoeuvre states
  in which a train must not be considered for a derail. The INNER, per-car guard at the top of the
  same function's car loop repeats those same three tests with the senses flipped and joined the
  same way: `dwModeAMaybe == 2 && dwModeAMaybe == 3 && dwModeBMaybe == 2`, which asks a single
  `int` field to hold both 2 and 3 at once. It is never true, so it never skips a car, and the
  per-car loop is effectively unguarded — the outer test is the only thing that actually filters.
  The intent was surely `||` (the De Morgan dual of the outer `&&` chain). Pinned byte-wise at
  0x44e4a2..0x44e4b1: one CSE'd `mov ecx,[eax+0x60]`, then `cmp ecx,edx`/`jne` and `cmp ecx,3`/
  `jne` both branching to the loop body at 0x44e4b7, with only the third test's `je` reaching the
  loop increment — i.e. the compiler emitted all three compares and the never-taken branch rather
  than folding the block away. Ghidra's own decompiler does fold it ("Removing unreachable block
  (ram,0x0044e4ae)"). ⚠ cl 5.0 folds it too whenever both reads are written as the same source
  expression, which is why the transcription deliberately spells one of them `(*ppOther)->` and
  the other `pOther->`; see the function's own comment. Reproduced with a `// sic:` marker, not
  fixed.
- **The timed-event jitter guards test their special cases on the branch that excludes them —
  with the constants swapped between the two functions (v494, `src/ScriptEventLoader.cpp`).**
  `ProcessTimedEventsMaybe`'s re-arm (0x41ff20) tests `period == 0` inside the `period >= 1`
  arm (dead) and `period == 2` inside the `period < 1` arm (dead), while the parser that seeds
  the same field (0x41fbe0) tests `period == 1` inside the `period < 0` arm (dead) and
  `period == -1` inside the `period >= 0` arm (dead). Both reads are provably unreachable from
  the disasm itself (the dominating guard constants exclude them), so the four branches can
  never have executed — the 0/2 vs 1/-1 pairing looks like a copy-paste between the two
  functions with the signs flipped. The guards survive into the bytes because cl 5.0 does no
  arithmetic-range reasoning (CODEGEN #63). Reproduction sites: the four dead guards in
  `ProcessTimedEventsMaybe` and `ParseRandomizedRecordMaybe`, marked `// sic:` there.
- **`LoadEasterEggsMaybe` (0x41f7e0, `src/ScriptEventLoader.cpp`) builds a local IniFile
  pointed at ee.ini and never reads from it.** The function constructs an `IniFile("LOCO.INI")`,
  overwrites its path with `"<prefix>ee.ini"` via the same strcpy-through-sprintf quirk the two
  script loaders use — then does every `[EasterEggs]` read through `g_pIniFile` (LOCO.INI)
  instead. So the unlock list lives in LOCO.INI, and the local object is dead work beyond its
  SEH frame. Reproduction site: the unused `ini` local, marked `// sic:` there.
- **The ScriptEventLoader ctor (0x41f480) zeroes only two of its three pointer/count fields.**
  It clears `+0x8` (fixed list head) and `+0x10` (egg count) but never touches `+0xc` (timed
  list head) — correct only because the singleton is a BSS static. Reproduction site: the
  missing zeroing noted in the ctor's comment.
- **Both ee.ini record parsers memset the node AFTER constructing it** (0x41fb20/0x41fbe0).
  The `memset(pRec, 0, sizeof(...))` runs after the two embedded `TimeOfDayMaybe` ctors have
  stored their vtables, flattening them — harmless only because nothing ever dispatches on a
  record and the shared node dtor re-stores both vptrs on the way out. Reproduction site: the
  memset comment in each parser.
- **`MenuNodeObj0x477568::~MenuNodeObj0x477568` (0x40d040, `src/MenuNode.cpp`) destroys its base
  subobject TWICE.** The dtor body opens with an explicit `this->RectFlagObj0x477820::
  ~RectFlagObj0x477820();` and the compiler then emits its own base-subobject destruction on the
  way out, so `~RectFlagObj0x477820` runs twice on the same `this` — visible in raw disasm as
  `call 0x436a00` at 0x40d06b (EH state 0, i.e. the first body statement) and again at 0x40d08e
  (immediately after the EH-state −1 store, i.e. the compiler's own). Two alternative
  explanations were ruled out before calling it a bug: an ICF fold of the ctor onto the dtor
  (the ctor is a distinct function at 0x4369d0, which is what `MenuNodeObj`'s own ctor calls),
  and an unmodeled intermediate base class (the vtable at 0x477568 inherits exactly
  `RectFlagObj0x477820`'s slots 1–5, so the derivation is direct). Harmless in practice, which
  is why it shipped: `~RectFlagObj0x477820` is idempotent — it restamps the vptr and re-clears
  `bValid`, and owns no resources. Reproduction site: the explicit base-dtor call, marked
  `// sic:` there. (The function is an EXACT match on the first compile *with* the redundant
  call, so this is not a transcription guess — the second call is in the bytes.)
