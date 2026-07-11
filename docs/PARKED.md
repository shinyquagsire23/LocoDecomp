# Parked functions — retry catalog

## ✅ v557 — RESOLVED in v557d: `Obj0x4779e0::ParseEntryExitMaybe` is EXACT again

### ✅ `Obj0x4779e0::ParseEntryExitMaybe` (0x41f0c0) — **RECOVERED, +489 B**, no longer parked

Parked below as a deliberate v557b spend; **un-parked in v557d at no cost**. The retry route the
old note proposed ("find a form of those `CursorDesc` declarations that does not rotate this TU")
was the wrong frame — the fix was to keep adding declarations rather than to find a cheaper one.
Typing the two +0x534/+0x548 embeds as real `TimeOfDayMaybe` sub-objects let `Obj0x4779e0`'s
ctor, dtor, `LoadMaybe` and all four parse methods move onto the real class, and once `BigObj`'s
ctor/dtor/`Load` followed, this function came back EXACT. The class also renamed with it
(`Obj0x4779e0ParsePartial::` → `Obj0x4779e0::`) because the TU-local view is gone.

Measured trail, which is the point worth keeping (see CODEGEN #168): ctor declaration = free and
recovered these 489 B; dtor declaration = lost them again; four parse declarations = no change;
`BigObj`'s three = recovered. **The dial is a threshold, not a per-declaration tariff.** Net for
the whole set: +1222 B / +5 EXACT funcs repo-wide.

The historical note is kept below for the reasoning, not as live debt.

### ~~⛔ `Obj0x4779e0ParsePartial::ParseEntryExitMaybe` (0x41f0c0, −489 B)~~ — HISTORICAL

EXACT as of `3a52a30`, now DIFF(19) at identical length. **The function did not change.**
`src/CursorDesc.h:95-105` predicted this exactly: declaring `CursorDesc(int, char *, int)` and
widening `Load` to its real 2-arg signature rotates this TU. Both were done, because without a
real `CursorDesc::CursorDesc` every one of the 15361 tile-kind descriptors was constructed by a
generated do-nothing stub, `TileKind_GetOrLoadDescriptor` returned the −1 poison, and
`SplashWnd::EnsureArtLoaded` dereferenced it — a hard null-pointer crash at
`loco-port+0x4b3f6`, which is what the game did before this change.

The ctor came back **+67 B EXACT on the first compile**, exactly as that note also predicted, so
the net is **−422 B**. Retry route is the same as the two below: find a form of those
`CursorDesc` declarations that does not rotate this TU.

## v566 — three EXACTs deliberately spent to make the board RENDER

All three are the same trade the v557/v563 sections above record: a call that resolved to a symbol
defined nowhere is a live defect, and correctness lands even when the fix moves a dial. See
docs/CODEGEN.md #189-#190. Net for the session: **−1286 B / −3 funcs**, against the port going from
"a single frozen loading-screen checksum for ~1850 frames" to a real world backdrop on screen.

| addr | function | cost | why it was spent | retry |
|---|---|---|---|---|
| 0x457ce0 | `WorldBoardPartial::FindNearestObjOfCategoryMaybe` | **951 B** (MATCH → DIFF(16)) | `#include "BuildToolButton.h"` in `src/WorldBoardMaybe.cpp`, needed to retire the `BuildToolButtonPaintView0x456700` view whose `BlitAllRegionsMaybe(RECT,int)` mangled to nothing and was the hottest stub in the run at **703 calls** (the toolbar skipped on every dirty rect of every frame). | ⭐ **CHEAP — one extra file-scope declaration anywhere in that TU brings it straight back to MATCH**, probed v566 with a throwaway `extern int`; two behave the same, so it is a THRESHOLD, not the parity v490 inferred. Left unspent only because no honest declaration was owed there yet. ⚠ Do NOT pay it by folding that TU's `Ddraw_BltUpdateRect` extern onto `DDrawSurface.h` — probed, lands on the wrong side. |
| 0x454fe0 | `WorldBoardMaybe_ResetAllTilesMaybe` | **211 B** (MATCH → DIFF(2)) | Same include. This is the same commutative-load coin flip the row further down documents, flipping for the eighth time. | Did NOT respond to the +1-declaration lever that recovers 0x457ce0. Retry at the next TU-context rotation, as at all prior flips. |
| 0x42aa90 | `LocoBitmap::Fill` | **124 B** (MATCH → DIFF(2)) | `src/LocoBitmap.cpp` spelled `DDraw_CreateSurfaceFromFile` `extern "C"`, emitting a call to `_DDraw_CreateSurfaceFromFile`, which nothing defines. v554 measured this exact price and PARKED it as deliberate wrong-symbol debt; **v566 re-measured it (unchanged, 124 B) and paid it anyway**, because the consequence turned out not to be cosmetic: in the port that spelling became a zero-filled DATA stub (CODEGEN #189), so the call jumped into BSS on the path every non-8bpp bitmap asset takes. Fixing it is what made the real world backdrop load instead of the flat default green. | The residual is v554's `imul` operand-order coin flip, already probed five ways (`height*width`, `width*height`, a cached local, a file-scope declaration, restoring the intermediate `pSurf` local). Source cannot reach it. |

## v557 — two EXACTs deliberately spent to make the smoke build boot

### ✅ `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe` (0x405c40) — **RECOVERED v563, row kept for the pricing history**

The 407 B came BACK, and the lever was the opposite of the one everyone expected. v563 landed
`AnimDescRefObj0x477488::DispatchAnimStateMaybe` (0x405a50, slot 14) into this same TU — the body
`src/WidgetBase.h` had held out of the tree since v479 precisely BECAUSE adding it was measured, three
separate ways, to cost 0x405c40 its 407 B. Measured on landing: **EXACT 173238 → 173645 B, +407 B /
+1 func**, `src/WidgetBase.cpp` 24+7/31 1971 B → 25+7/32 2378 B. 0x405c40 returned to EXACT and the
new 0x405a50 arrived as PARTIAL.

So the toll had not merely expired (the v560 `RectFlagObj0x477820` lesson) — it had INVERTED: paying
it refunded it. Two for two on "re-measure a withheld body before believing its price"; see
docs/CODEGEN.md #186. The body was also blocking the port's entire widget layer (#185): as a
declared-only virtual it was a stub, and `SetDescriptor` tests the `nSubFrame` the stub never
writes, so every widget's `SetDescriptor` returned 0.

`TutorialWnd::RestorePresenterBackdrop` (0x452b00, **−249 B**) is unaffected by this and still parked
below.

### ⛔ `TutorialWnd::RestorePresenterBackdrop` (0x452b00, **−249 B**) — original v557 note

Both were EXACT as of `3a52a30` and are DIFF now. **Neither function changed.** The cause is six
method declarations added to `src/UIResources.h` so that `UIResources`' own methods stop being
defined on TU-local views — `Init`, `TileKind_CreateDescriptor`, `TileKind_LoadDescriptorRange`,
`Locale_DetectLanguage`, `Shutdown`, `ReleaseAllCachedResources`. Both TUs include that header
(for `g_UIResources.m_hFont14` / the label font), and it is the documented declaration-count dial
its own v340/v356 notes price.

Why it was worth paying: a TU-local view satisfies the byte-match perfectly — `verify.py` compares
one COMDAT at a time and masks relocations — while leaving the symbol OTHER TUs actually call
undefined. `src/AppWindow.cpp` called `UIResources_Init`, which the link resolved to a generated
do-nothing stub returning 0; the bootstrap read that as "resources failed" and put up the fatal
MessageBox. Every tile-kind descriptor, the RF archive, the fonts and the WAV bank were never
loaded. Paying 656 bytes moved the smoke build from "three stub calls then a fatal dialog" to
"680 stub calls, deep inside resource loading".

**The cost is NOT linear, which matters for anyone retrying this.** The first THREE declarations
cost the whole −656 B / −2 funcs. The next THREE — added in the same session, same header, to
retire the second view — cost **exactly zero**. So the dial is a threshold/parity effect, not a
per-declaration price, and "this header is expensive" is the wrong lesson: measure the specific
set you need, and do not stop at a partial fix believing each further declaration compounds.

Retry route: find a form of the `UIResources` declarations that does not rotate these two TUs
(the v548 note below is the same class of problem) — NOT by reverting them, which would put the
build back to unbootable.

## v548 — the `WorldBoardPartial` virtual-dtor promotion (PRICED AND WITHHELD, −1311 B) + one new residual

### ⛔ `WorldBoardPartial`'s real `virtual ~WorldBoardPartial()` — priced at **−1311 B** for **+32 B**

The v547 pickup named this the next lever ("claims bytes AND is a candidate for the parity move
that recovers 0x452b00"). Both halves are TRUE and it is still a large net loss. Measured in full,
from a clean baseline, with a full `progress.py` per state.

The model change is the correct one: vtable 0x478520 has exactly ONE slot and it holds
`??_GWorldBoardPartial` (0x454db0, 32 B), so the class really is polymorphic and its `pad0x0[4]`
really is a vptr. Promoting the free `WorldBoardMaybe_InitFieldsMaybe(WorldBoardPartial*)` escape
hatch (which stored the vtable through a raw `extern void *g_vtable0x478520[]`) to the real member
ctor is what makes cl emit the vftable, and with it the `??_G` thunk. **0x454db0 was EXACT on the
first compile at 32 B, the ctor 0x454cf0 KEPT its 190-byte EXACT, and `FindNearestObjOfCategoryMaybe`
(0x457ce0) kept its 951.** The dtor must be INLINE (`{}` in the class) per CODEGEN #140 — the thunk
carries its own `mov [esi],0x478520`. The separate out-of-line `??1` at 0x454dd0 belongs to whichever
TU defines `g_worldBoard` (its atexit thunk 0x45c760 `mov ecx,0x4aad08; jmp 0x454dd0` is the only
caller), i.e. to the unmodeled static-object-initializer work, not to `src/WorldBoardMaybe.cpp`.

The cost is `src/WorldBoardMaybe.h`'s declaration-count dial, and the ctor + dtor pair cannot be
paid for. Canary totals over the 9 TUs that move (`LocoBitmap`, `RoadVehicleActor`, `ThumbnailBmp`,
`TilePlacedObj`, `TutorialWnd`, `WidgetBase`, `WorldBoardMaybe`, `DPlaySessionMgr`,
`PlacementCursorMaybe`), baseline = **31938**:

| state | canary total | note |
|---|---|---|
| baseline (D+0) | **31938** | current tree |
| D+1 (ctor+dtor, `EraseAlongDragSegmentMaybe` retired) | 29819 | the only retirable declaration on the header — everything else has callers |
| D+2 (ctor+dtor) | 30247 | = full `progress.py` **−1691 B** |
| D+3 (+`GetTopPlaneBSlotMaybe`) | **30627** | best reachable state, still **−1311 B** |
| D+4 (+`GetPlaneBTopSlotAtPixelMaybe`) | 30231 | |
| D+5 (+`Ddraw_RecenterViewportOffsetMaybe`) | 30231 | inert — the counter is not one-per-declaration |

⭐ **The dial is NOT polymorphism** — that was the obvious suspicion and it is REFUTED. A control
that added ONE ordinary non-virtual declaration to the baseline header, with no virtual dtor and no
ctor promotion anywhere, reproduced the entire −1160 B core loss exactly (RoadVehicleActor 1363→859,
WidgetBase 2259→1852, TutorialWnd 9495→9246). So the price is the declaration count alone.

⭐ **And a dummy declaration is NOT interchangeable with a real one here**, contrary to the
`src/AppWindow.h` folklore. A sweep with N throwaway `void ProbeDeclNN();` members over the
baseline header (`N=0..8`, six-TU totals) puts the same three victims at different rungs than one
real `TilePlacedObj *GetTopPlaneBSlotMaybe(short, short, short *)` declaration does — one real
declaration weighs roughly what 3–4 `void ()` declarations weigh. **N=0 is the maximum**; nothing
in 1..8 beats it:

| N | six-TU total | RoadVehicleActor | WidgetBase | TutorialWnd | TilePlacedObj | ThumbnailBmp | LocoBitmap |
|---|---|---|---|---|---|---|---|
| 0 | **16927** | 1363 | 2259 | 9495 | 2374 | 923 | 513 |
| 1 | 16520 | 1363 | 1852 | 9495 | 2374 | 923 | 513 |
| 2 | 16140 | 859 | 1852 | 9495 | 2374 | 923 | 637 |
| 3 | 15620 | 859 | 1852 | 9495 | 2374 | 403 | 637 |
| 4 | 15311 | 859 | 1852 | 9246 | 2314 | 403 | 637 |
| 5 | 15311 | 859 | 1852 | 9246 | 2314 | 403 | 637 |
| 6 | 15187 | 859 | 1852 | 9246 | 2314 | 403 | 513 |
| 7 | 15707 | 859 | 1852 | 9246 | 2314 | 923 | 513 |
| 8 | 15707 | 859 | 1852 | 9246 | 2314 | 923 | 513 |

(⚠ These are `cc.sh` per-TU totals, which differ slightly from `progress.py`'s for the same tree —
use them for DELTAS only.)

**Consequence for the PostBag lever (v547 item 4).** `TutorialWnd::RestorePresenterBackdrop`
(0x452b00, 249 B) is confirmed to be the `−249` toll in BOTH prices — it falls at
`src/WorldBoardMaybe.h` N≥4 and it falls under the `??_GPostBagFileCache` in-class dtor. So a
WorldBoard-dtor + PostBag-dtor pair would pay that toll only once, making the combined figure
−1311 + 195 = **−1116 B**. Still a loss; the pairing does not rescue either lever. **Do not re-run
either without a NEW knob** — the WorldBoardMaybe.h dial itself is now swept and exhausted.

### 0x446030 `TileKind_GetCategory` — EFFECTIVE, DIFF(16), 21/22 B, insns 7/8

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x446030 | `TileKind_GetCategory` (free `__cdecl`, src/UIResources.cpp) | 22 B | compiled **21 B**, DIFF(16); asmscore `--len 22` total 24336, align=24 reg_pen=3 identity_miss=2 byte_diff=16, insns **7/8** | Newly transcribed v548 (it sits directly in front of this TU's 0x446050, and four other TUs already declared it extern). The branchless mask is CONFIRMED as the original's shape (`cmp al,0x10 / sbb al,al / and eax,ecx` = `value & -(value < 0x10)`) and the parameter-reassignment spelling is what reproduces it — a plain byte local with a ternary compiles to a BRANCH instead (DIFF(21), 28 B) and is refuted. Whole residual is one narrowing choice: the original keeps the projection as a BYTE in the parameter's own stack slot (`mov [esp+4],al`) and reloads the DWORD to AND against; this build truncates in a register (`and ecx,0xff`) and never spills, dropping one instruction and flipping which register carries the mask. Eight spellings probed, none reproduces the spill — see the full list in the in-source autopsy. The `unsigned char` return type is well-supported (the AND leaves junk in the upper three bytes; every caller re-narrows with `(unsigned char)`) but changing it changes the mangled name, so it means touching four consumer TUs' extern declarations and their now-redundant casts — not worth spending on 22 bytes until the spill is understood. |

### ⛔ Confirmed NOT claimable: the ICF `jmp` thunks

`0x401680` (`PostBagFileCache_SaveIndexFileMaybe` → `jmp 0x401c90`) and `0x409970`
(`DrawAvatarGrid` → `jmp 0x409980`) are 5-byte linker COMDAT-folding thunks, the same family as
the `??_G*` pairs CLAUDE.md already lists as intentionally left. Our toolchain emits the full body
for any source function we write, never a 5-byte jump, so no source spelling can byte-match them.
Do not re-survey them off the unclaimed list.

## v532 — two previously-EXACT bystanders knocked off by the claim-queue landings

Both are REGRESSIONS, not new transcriptions: each was byte-identical at the v531 baseline and was
flipped by declarations/definitions added elsewhere this session. Both are instruction-count-exact
pure register coin-flips, i.e. the documented intrinsic VC5 `/Og` class, and both were cheaper to
pay than the 354 B they bought. Neither body was touched.

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x458310 | `TilePlacedObj::GetFrontRowTilePosMaybe` (src/TilePlacedObj.cpp) | 60 B | DIFF(18); asmscore `--len 60` total 12565, align=12 reg_pen=5 identity_miss=5 byte_diff=15, insns **14/14** | The `sub edx,eax` / `sub eax,edx` coin flip this header family has paid repeatedly since v442 — the ENTIRE residual is that one operand swap plus the stack-slot reload it drags one instruction later. Flipped by the `~PeerTrainNodePartial()` declaration added to `src/PeerTrainNode.h` (a header this TU includes but whose new member it neither calls nor mentions). PROBED: a throwaway extra declaration on that header restored it while `src/TilePlacedObj.cpp` was otherwise unchanged, so it IS a declaration-count dial — but once this session's two real definitions (0x44f2c0, 0x44f340) landed in this TU, the real declaration that replaced the dummy (`SetModeBMaybe`) no longer recovered it. So the dial spans this TU's own definitions AND the shared header together, exactly as v490 describes, and there is no single knob left that pays it back. Retry whenever `src/TilePlacedObj.cpp` next changes shape. |
| 0x40e130 | `CarNetObjAnchorPartial::StartFrameSoundMaybe` (src/PeerTrainNode.cpp) | 34 B | DIFF(4); asmscore `--len 34` total 224, align=0 reg_pen=2 identity_miss=2 byte_diff=4, insns **12/12** | Two adjacent loads trade registers and order: the original does `mov edx,[ecx+0x40]` (pKindDesc) then `mov eax,[ecx+0x28]` (nSubFrame), this build the reverse. Everything else in the 12 instructions is identical. Flipped by the `~PeerTrainNodePartial()` landing in this same TU. PROBED and INERT: hoisting `BigObj *pKind = this->pKindDesc;` into its own local to force the read order compiles BIT-IDENTICAL — cl canonicalizes the address computation before scheduling, so the source cannot express the difference. Not sensitive to `src/PeerTrainNode.h`'s declaration count either (unchanged at N=0, 1 and 2 extra declarations). No source-side lever known. |

## v531 — one register coin-flip in the TrackTileObj leaf

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44b130 | `TrackTileObj::ReleaseChannelAndDispatch` (via TU-local `TrackTileObjChannelView0x44b130`, src/TilePlacedObj.cpp) | 96 B COMDAT (0x44b190, the next function, is the bound; `app_funcs.txt`'s 96 agrees) | compiled **98 B**, DIFF(51); asmscore `--len 96` total 6224, align=6 reg_pen=2 identity_miss=2 byte_diff=4, insns **27/26** | CONTENT-COMPLETE — every call, operand, branch target and all three epilogues agree. The ENTIRE residual is ONE instruction: the `dwTrackState == 1` arm keeps arg in edx in the original (`push edx` / `mov [ecx+0x110],edx`) and routes it through eax here (`mov eax,edx` / `push eax` / `mov [ecx+0x110],eax`); the resulting shift is the whole of DIFF(51). Structure CONFIRMED, not assumed, on two points that both look like tidy-up opportunities and are not: (1) the original calls the base at 0x44b159, 0x44b170 AND 0x44b188 — three real call sites, so the two early arms genuinely return rather than falling into the shared tail, and merging them would be wrong; (2) the switch lever that took the sibling 0x44b0b0 from DIFF(89) to EXACT does NOT apply — the gate block lowers as `test edx,edx / jne` + `cmp edx,1 / jne`, cl's if-CHAIN form rather than the `sub`/`dec` ladder of a dense switch, and that block already matches byte-for-byte. Also measured inert: passing `arg` vs a literal `0` on the provably-zero path (byte-identical, so the original's own spelling is undecidable there). Retry if the VC5 register coin-flip class ever becomes source-steerable. |

## v527 — two FINISHED EXACT bodies withheld on the src/WidgetBase.cpp dial (0x405c40's 407 B)

Both bodies below **compile EXACT on the first try** and are correct. Neither is a codegen park:
each is withheld because landing it costs `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe`
(0x405c40, 407 B) its EXACT, dropping it to the same DIFF(314) at 405 B that v526 recorded for the
`#include "EffectSpawner.h"` probe. Measured one at a time from a clean baseline, everything else
held fixed.

| addr | name | size | net if landed | why withheld |
|---|---|---|---|---|
| ~~0x436ab0~~ | ~~`RectFlagObj0x477820::MarkDirty`~~ ✅ **LANDED v560, EXACT, AT NO COST** — the −407 B toll had already expired (0x405c40 lost its EXACT to something else since v540); re-measuring cost one compile. Row kept for the pricing history only. | 41 B | **actual: +41 B** (predicted −366 B) | Body is a one-liner: `g_worldBoard.MarkRectDirty(rect)`, the by-value RECT copy. Already DECLARED (`src/WidgetBase.h:36`), so no header change is involved — this is a bare DEFINITION that is nevertheless not free, which is the finding. Not an external-symbol effect either: `g_worldBoard` is already referenced twice in this TU by 0x454890. |
| 0x454630 | `WidgetBaseObj0x4784c8::ClearOwned` (src/WidgetBase.cpp) | 75 B | **−332 B** (+75 itself, −407 for 0x405c40) | Also already declared (`src/WidgetBase.h:298`). ⭐ Here the DEFINITION IS FREE and the **`#include "EffectSpawner.h"` it requires is what costs the 407 B** — isolated by compiling the include with the body removed, which reproduces the full DIFF(314) on its own. The body needs `DAT_004fd220.EffectSpawner_RemoveHandle` and there is no way to reach a member call without the class definition. Land it when the include becomes free, or if EffectSpawner's handle API ever gets a narrow header. |

⭐ **The result worth carrying: v526's "a definition is nearly free, a declaration is what costs"
is a WEAK PRIOR, not a rule — and this TU shows both halves failing and holding in the same
session.** Three definitions were added to `src/WidgetBase.cpp` here, all EXACT on the first
compile, all into declarations that already existed:

| definition added | cost to 0x405c40 |
|---|---|
| `WidgetBaseObj0x4784c8::SetDescriptor` (0x454680, 67 B) | **free** — landed |
| `RectFlagObj0x477820::MarkDirty` (0x436ab0, 41 B) | −407 B — withheld |
| `WidgetBaseObj0x4784c8::ClearOwned` (0x454630, 75 B) | −407 B, and it is the INCLUDE not the body — withheld |

It is also not a count parity: `SetDescriptor` alone (+1 definition) keeps 0x405c40 EXACT, while
`MarkDirty` alone (+1 definition) breaks it. So which body you add matters, not how many. The
practical rule: **land declared-only bodies one at a time and run a full `progress.py` around each**
— the cheap ones really are free, and the expensive ones are only identifiable by measuring.

## v526 — one scheduling park, and one finished EXACT body withheld on a measured price

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x456d10 | `WorldBoardDirtyTileView0x456150::FUN_00456d10` (src/WorldBoardMaybe.cpp) | 127 B (no jump table; `app_funcs.txt`'s 127 IS the COMDAT extent here) | compiled **127 B**, DIFF(36); asmscore `--len 127` total 25115, insns **49/49**, align 24 / reg_pen 10 / byte_diff 25 | CONTENT-COMPLETE — identical instruction COUNT and identical instructions; the sole disagreement is where the surviving-node branch schedules its `pNode = pNode->pNext` load. The original emits the four-dword rect copy first and the advance last; this compile emits the advance first, and the `mov eax,esi` in the delete branch slides by the same amount. Both forms keep two copies of the node pointer live (ecx as the store base, esi for the advance), so this is register/instruction SCHEDULING, not a shape difference. Four probes, none of which moved it: (1) hoisting `pPrev = pNode` above the copy — no change; (2) field-by-field rect stores instead of the struct assignment — **WORSE** (125 B, DIFF(68)), which positively confirms the struct assignment is the original's form; (3) advancing through `pPrev->pNext` so two distinct pointer variables straddle the store, betting on conservative aliasing — CSE'd straight back; (4) hoisting the RECT local out of the loop the way the matched 0x456c60 sibling does — no change. Retry only if the VC5 /Og load-scheduling class cracks generally. |

**Withheld, not parked on codegen: 0x45ca20 `RFIndex::~RFIndex` (123 B) is FINISHED and compiles
EXACT on the first try** — the body is trivially correct (fclose the .RFD handle, free the parsed
index list node by node, free the .RFD path copy) and it is the missing counterpart to the already
matched `RFIndex::RFIndex` at 0x45ca10. It cannot be landed because a destructor must be declared
in its class, `RFIndex` lives in `src/DSoundChannel.h`, and that header reaches 20+ TUs. **Measured
twice from a clean baseline, not assumed:**

| DSoundChannel.h declaration count | net EXACT vs. baseline | who moves |
|---|---|---|
| +1 (`~RFIndex()` alone) | **−1169 B** | UIResources +123, LocoBitmap **+124**, Obj0x4779e0 −489, ThumbnailBmp −520, WidgetBase −407 |
| +2 (`~RFIndex()` + hoisting `Rf_HuffmanDecompress` out of src/UIResources.cpp) | **−487 B** | UIResources +123, WidgetBase −407, TilePlacedObj −203 |

⭐ **The finding worth carrying: this header's dial is NOT a single parity bit shared by its
includers.** At +1 three TUs regress and one IMPROVES; at +2 those three recover and a fourth
(TilePlacedObj) breaks, while WidgetBase's `AdvanceAnimFrameMaybe` (0x405c40, 407 B — the same
function 0x4545a0/0x454630 are parked behind) is broken at BOTH counts. So no single shared-header
count satisfies every includer, and the v490 per-TU file-local lever is the only remaining route.
A free −1 was searched for and does not exist: `g_pInstallPathPrefix` is declared exactly once (the
other two apparent sites are comment prose), and `IsOpen`/`LoadResource`/`g_RFIndex`/`_free` are all
live. Land it when either the per-TU levers for WidgetBase.cpp and TilePlacedObj.cpp are found, or
`RFIndex` moves to a narrow header of its own (the RF archive is its own subsystem and arguably
should not live in the DirectSound channel header at all — that split is the real fix).

## v525 — src/Obj0x477798Family.cpp (one twin-selection park; the two v524 placement parks re-priced)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4361e0 | `RoadVehicleRegistryMaybe::CompareEntriesMaybe` (src/Obj0x477798Family.cpp) | 156 B COMDAT (138 B of code + a 2-byte pad + the 4-entry jump table; `app_funcs.txt`'s 138 is the CODE extent and truncates the compare window) | compiled **152 B**, DIFF(76); asmscore `--len 156` total 94355, insns **58/59** | Transcribed v525 as the category-8 copy of the comparator whose category-7 twin (0x435c00) src/PlacedObjRegistryMaybe.cpp already owns EXACT. CONTENT-COMPLETE — the residual is EXACTLY the operand-evaluation-order difference that DEFINES the twin pair: in both half-word arms the original loads pOther (`[esp+0x14]`) before pObj and materializes the unsigned arm through a `mov ecx,edx` / `mov eax,esi` shuffle (the one extra instruction), where this compile loads pObj first and subtracts in place. So this TU emits 0x435c00's twin. This is the documented v457 class — **which twin a given text produces is decided by the TU, not by the source** (the identical text gave 0x435c00's order in src/PlacedObjRegistryMaybe.cpp and 0x4361e0's in src/DecorActor.cpp). NOT chased deliberately: the only source edits that flip the load order are temps spelling out "read pOther first", and the whole premise of the pair is that the two .objs hold IDENTICAL source — writing the copies differently would encode a compiler artifact as a source difference. Claimed anyway because this IS its .obj (0x436040/0x436140/0x436280/0x4362b0 all live in this file) and the claim is what documents the finding. Retry only if the twin-selection mechanism itself becomes understood. |

**The two v524 placement parks below are UNCHANGED in substance, but v525 re-priced the question
they sit on and the answer moved.** v524 framed both as "a declaration-count dial forbids this";
v525 measured an EIGHT-declaration block into `src/Ddraw.h` across six includers at **zero** cost
(CODEGEN #86). So the dial is a per-header property, and the standing advice is now: **measure the
specific header before assuming a park is required.** Neither park's own measurement is invalidated
— `LocoBitmap.h` and `src/WidgetBase.cpp` were each measured directly — but the SEARCH for a
cheaper home is now the first move, not the last.

- **0x42c330 `LocoBitmap::CopyRectRawColorKey`** still waits on a genuine SECOND real
  `LocoBitmap.h` declaration (N=4 is +361 B net, N=3 is −1110 B). v525 looked for one and did not
  find it: the obvious candidate, a canonical home for `DAT_004ff110` shared between 0x45c8a0 and
  0x45c970, does not work because `src/AppWindow.cpp` (0x45c8a0's current home) does not include
  `LocoBitmap.h` and making it do so is a far larger change than the 149 B is worth. Still parked
  on the same landing condition.
- **0x4545a0 `~WidgetBaseObj0x4784c8` + 0x454630 `ClearOwned`** still cost `AdvanceAnimFrameMaybe`
  (0x405c40, 407 B) its EXACT via `#include "EffectSpawner.h"`, net −188 B. Unchanged.


## v524 — src/LocoBitmap.cpp (one EXACT-but-unlandable body; no residual parks)

**Nothing is parked on CODEGEN this session.** The one park is a *placement* park, same species as
v523's 0x45c970: the body is finished and compiles EXACT, and only the shared header's
declaration-count dial keeps it out of the tree.

**0x42c330 `LocoBitmap::CopyRectRawColorKey` (149 B) — transcribed, compiles EXACT, NOT landed.**
The color-keyed twin of `CopyOverlapRaw` (0x42c3d0) and its immediate `.text` neighbour: identical
rect walk, identical dead-`destRect.right/bottom` convention, identical "source read via a plain
int offset re-added to the member each iteration, dest cached as a real pointer" split (Yoda
lesson #19). The only difference is the per-pixel guard — a source index of 0 is the transparent
key and leaves the destination byte alone, while both cursors still advance. It is NOT one of
`RestoreOverlapBlt`'s 11 dispatch siblings: its only caller is the station-clock chime tick
(0x447400, 4 call sites), which hands it another `LocoBitmap`'s own `pPixels`/`width` as
`pDestPixels`/`destWidth` — so it is the raw-8bpp bitmap-onto-bitmap compositor. `ret 0x28`
confirms the 10-dword `PixelCopyBlit` parameter convention.

⚠ **Why it is not landed — measured 2026-07-31 from a clean baseline, full `progress.py` each
time** (a per-TU `cc.sh` shows only the win and none of the cost):

| `LocoBitmap.h` decl count | EXACT total | vs. baseline | victims |
|---|---|---|---|
| N=2 (baseline, HEAD) | 163699 B | — | — |
| N=3 (this declaration alone) | 162589 B | **−1110 B** | WorldBoardMaybe.cpp 4208→3257 (the 0x457ce0 canary), ThumbnailBmp.cpp 923→403, TilePlacedObj.cpp 1091→1031 |
| N=4 (this + a dummy) | 164060 B | **+361 B** | only TilePlacedObj.cpp 1091→1031 |

So the declaration costs −1531 B of collateral against its own +421 B of gain (the +421 is 149 +
148 + a third sibling in the TU that flips with it). N=4 recovers nearly all of it — but
`LocoBitmap.h`'s own standing rule (written when N went 1→2) forbids padding with a fake
declaration: *"a real declaration is currency, a fake one is not."* **Landing condition: the next
time a SECOND real declaration is genuinely needed in `LocoBitmap.h`, land it together with this
one and re-measure both.** The dial is documented as neither a parity bit nor a clean period (see
src/TilePlacedObj.h), so N=4's near-freeness must be re-derived, not assumed.

The body, ready to paste back into src/LocoBitmap.cpp immediately above `CopyOverlapRaw` (the
`copyW`-before-`copyH` order is load-bearing — see CODEGEN #81):

```cpp
// FUNCTION: LOCO 0x42c330
void LocoBitmap::CopyRectRawColorKey(RECT destRect, unsigned char *pDestPixels, int destWidth,
                                            RECT srcRect) {
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned int copyH = (srcRect.bottom - srcRect.top) & 0xffff;
    unsigned char *pDst = (unsigned char *)(destWidth * destRect.top + (int)pDestPixels + destRect.left);
    int srcOffset = srcRect.top * width + srcRect.left;
    for (unsigned short wRow = 0; wRow < copyH; wRow++) {
        for (unsigned short wCol = 0; wCol < copyW; wCol++) {
            unsigned char index = pPixels[srcOffset];
            if (index != 0) {
                *pDst = index;
            }
            pDst++;
            srcOffset++;
        }
        pDst += destWidth - copyW;
        srcOffset += width - copyW;
    }
}
```

**UNPARKED this session — 0x42c3d0 `LocoBitmap::CopyOverlapRaw` is now EXACT** (it was carried as
an in-source `// EFFECTIVE MATCH` autopsy rather than a row here, so there is no row to delete;
that comment is rewritten in place). It had been parked for many sessions at DIFF(18)/148 B with
an identical 54/54 instruction count. The fix was one line: declare `copyW` BEFORE `copyH`. See
CODEGEN #81 — and note the fix needed NO header change, which is the only reason this session
banks it.

**0x4545a0 `WidgetBaseObj0x4784c8::~WidgetBaseObj0x4784c8` (144 B) + 0x454630
`WidgetBaseObj0x4784c8::ClearOwned` (75 B) — BOTH transcribed, BOTH compile EXACT together, NOT
landed.** Same species of park as 0x42c330 above: finished, verified, and blocked purely on a
declaration-count price in src/WidgetBase.cpp.

ClearOwned is slot 15: `delete pMenuListHead` (slot-0 scalar-deleting-dtor call, arg 1; the node
cascades down pNext), then `DAT_004fd220.EffectSpawner_RemoveHandle(pEffectSpawner)`, then
`SetDescriptor(0, -1, 0)` — slot 6 (+0x18) = 0x454680. ⚠ Ghidra renders that last call with TWO
arguments; the raw disasm pushes THREE. The dtor's entire body is `ClearOwned();`.

⭐ **The interesting part is how the dtor is modeled, and it is a reusable technique — see
CODEGEN #82.** 0x4545a0 and 0x454630 have byte-identical teardown bodies, which reads like two
copies of one source body. It is not: it is one `ClearOwned()` call that VC5 inlines. The tell is
a single instruction. Spelling the teardown out longhand inside the dtor compiles to 38
instructions against the original's 39, because there VC5 knows the dynamic type exactly and
DEVIRTUALIZES the SetDescriptor call to a direct `call`, where the original keeps
`mov edx,[esi]; call [edx+0x18]`. Inside ClearOwned — an ordinary member — the dynamic type is not
pinned, so the virtual dispatch survives, and inlining preserves it. Requires BOTH levers: the
definition must be marked **out-of-class `inline`** and must sit **before the dtor** (without
`inline` VC5 emits a real call and the dtor is 79 B / DIFF(29) instead of 144 B EXACT). Being
`virtual` it still gets its own out-of-line COMDAT for the vtable slot, so both addresses match at
once. Same out-of-class-inline shape as v518's ProbeComPort, used to FORCE an inline rather than
suppress one.

⚠ **Why not landed — measured 2026-07-31, full `progress.py`, bisected:** the bodies need
`#include "EffectSpawner.h"` in src/WidgetBase.cpp (for `DAT_004fd220`), and that include ALONE —
verified with the two bodies `#if 0`'d out — knocks `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe`
(0x405c40, **407 B**) out of EXACT to DIFF(314)/405 B. Against the pair's +219 B that is **net
−188 B**. Include POSITION is irrelevant (tested first and last in the list). No other TU moves —
the collateral is entirely inside this TU. The documented escape hatch (a TU-local partial view of
the `DAT_004fd220` singleton, the shape src/AnimEffectObj.cpp already uses for this very global) is
deliberately NOT taken: per-TU singleton views are exactly what this project has ruled out.
**Landing condition: whenever src/WidgetBase.cpp next needs `EffectSpawner.h` for its own reasons,
or the TU's dial rotates 0x405c40 anyway — land both bodies then, and re-measure.**

**RE-PRICED v526 — unchanged, still blocked.** v526 landed a new definition in this very TU
(0x405870 `~AnimDescRefObj0x477488`, +141 B EXACT) which the v525 pickup predicted would rotate the
TU's dial and thereby settle this park one way or the other. It did neither: 0x405c40 kept its full
407 B across the new definition, and re-running the isolated probe (adding `#include
"EffectSpawner.h"` alone, with the new dtor in place) still knocks 0x405c40 to DIFF(314)/405 B.
So the include's cost is INDEPENDENT of this TU's definition count — it is the include itself, not
a parity the definition could flip. Price stands at +219 B for the pair against −407 B, net −188 B.
The landing condition is unchanged, but one route is now closed: adding more definitions here will
not buy it.

## v523 — src/CursorDesc.cpp (plus one NOT-transcribed gap, and the VtblProbe re-price)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44bdb0 | `BigObj::IsAtMatchingBoardEdgeMaybe` (transcribed through the TU-local `BigObjBoardEdgeView0x44bdb0`, src/CursorDesc.cpp) | 151 B | compiled **137 B**, DIFF(127), insns 50/49 — full autopsy in-source | Transcribed v523 (the v522 pickup's fresh-gap tier). CONTENT-COMPLETE and structurally paired arm for arm — every asmscore dump row across all four edge arms is an `r` register-rename, in the same order. The WHOLE residual is a SPILL decision: the original allocates two stack bytes (`sub esp,8`) and keeps BOTH locals in memory — `nNorthEdgeRow` at [esp+0x14], reloaded as a dword and masked (`mov ebp,[esp+0x14]; and ebp,0xff`), and `bResult` at [esp+0x13], live in `al` across the arms and reloaded after the east arm clobbers eax — burning a fifth callee-saved register (ebp as a general register under /Oy) to do it. This compile enregisters `nNorthEdgeRow` in dl/bl and keeps `bResult` purely in its stack slot: one more instruction, 14 fewer bytes. Lever baked in (do not undo): the single `\|\|`-chain funnel + accumulator — four separate `if (...) return 1;` statements emit four separate epilogues instead of the original's one shared `mov al,1` tail (151 B but DIFF(137), insns 61/49). Probes refuted (do not re-run): explicit int-width casts on the north arm's compare (no-ops after promotion); `char` vs `unsigned char` return/accumulator type; declaring `bResult` before `nNorthEdgeRow`. All three byte-identical at 137 B / DIFF(127). Retry only on the spill-decision class or a TU-context rotation. |

**Also parked this session, NOT transcribed — 0x45c970 `LocoBitmap_ReleaseThumbPalSingletonMaybe`
(153 B), blocked on a placement decision, not on codegen.** The body is fully understood and
trivial: six COM singletons (0x4ff0fc, 0x4ff100, 0x4ff104, 0x4ff108, 0x4ff10c, 0x4ff0f8 — in
THAT source order, note the lowest address comes sixth) each `if (p) { p->Release(); p = NULL; }`
through `IUnknown::Release` (vtable +0x8), then `if (p) { delete p; p = NULL; }` on
`DAT_004ff110` (a `LocoBitmap *`, slot-0 scalar-deleting-dtor call with arg 1).
⚠ **Engine finding: this function is the ONLY xref of any kind to all six COM globals — nothing
in .text ever WRITES them, so all six arms are dead at runtime.** Typed `IUnknown *` is all the
image proves about them. The blocker is where the six externs live: its declaration's home
(`Ddraw.h`) reaches 6 TUs including the position-sensitive WidgetBase.cpp/UIResources.cpp;
`Ddraw.cpp` has no `LocoBitmap.h` (needed for the `delete`); and its sibling 0x45c8a0's TU
(src/AppWindow.cpp) would take six file-local `extern` decls, i.e. +6 tagged class-I idiom
findings. Do it as part of a deliberate Ddraw.h/AppWindow.cpp declaration-ownership pass — the
same pass that folds the eight consumer TUs' file-local DDraw singleton copies Ddraw.h already
documents as pre-existing debt.

## v522 — src/Main.cpp + src/WindowBase.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x463430 | `FUN_00463430_ExitRelatedMaybe` (Main-cluster clean-exit dispatcher; called from `Main::AppWndProc`'s quit paths) | 181 B | compiled **180 B**, byte_diff 25, insns 53/53 structurally paired | Transcribed v522 (the v521 pickup's third fresh-gap tier). Content-complete: the screen-state 1/10 hide-and-reshow arm vs the state-10 park, then the shared tail (0x4634f0 WM_CLOSE broadcast, `AppWindow::SaveWindowAndCleanExit`, DestroyWindow + null hwndOwner, `ThreadWrapper::PollAndResume(&g_worldLoadThread)`, KillTimer, PostQuitMessage(0), clear DAT_00485444). Levers baked in (do not undo): `switch (g_nScreenState) { case 1: case 10: }` (reproduces the dec/sub/jz chain; an `\|\|` if-form emits cmp/cmp), and a TU-local `AppWindow *pAppWnd = g_pApp` for the head region ONLY (the tail must keep reading `g_pApp` directly). The WHOLE residual is the documented #R1 class: the original spends the 5-byte `a1` encoding on the `g_nScreenState` load and keeps `g_pApp` in ecx; cl 5.0 reverses it (also explains the 1-byte length gap). Probes refuted (do not re-run): if-form vs switch, both/neither local, declaration order, local rename, switch operand direct-global vs local. Retry only on a TU-context rotation. |
| 0x425ac0 | `WindowBase::MapPointBetweenRects` (free `__cdecl` rect-to-rect point rescaler; caller `MapWnd::DrawPeerTrainDotsMaybe` 0x431b30) | 173 B | compiled **173 B**, byte_diff 12, insns 71/66 align-inclusive | Transcribed v522 (the v521 pickup's fifth fresh-gap tier). Content-complete: subtract src origin, per-axis fixed-point ratio `(dstExtent*1000)/srcExtent` applied as `(v*ratio)/1000`, add dst origin. Levers baked in (do not undo): named ratio locals (`int nRatioX/nRatioY` computed BEFORE the origin subtracts) — Ghidra's literal statement order compiles to 167 B / DIFF(152); the hoisted-locals form reproduces the both-ratios-up-front structure and full 173 B length. The WHOLE residual is TWO register coin-flip clusters: (1) the `pDstRect->left` temp rides edx in the original vs ebp here; (2) inside the ratioY computation cl swapped srcH/dstH load order (numerator-first in the original; the X axis right above compiles numerator-first in BOTH). Probes refuted (do not re-run): compound vs plain assignment (byte-identical), interleaving the `*px` subtract between the ratios (much worse, 170 B), split decl/assignment, swapped decl order, hoisted `nDstW` temp. Retry only on the reg-allocation coin-flip class or a TU-context rotation. |

## v520 — src/BuildToolCursorWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x437ea0 | `BuildToolCursorWnd::OnClose(HWND, UINT, WPARAM, LPARAM)` (vtable slot +0x7c, WM_CLOSE) | 202 B gap; true COMDAT extent **240 B** (switch jump table to 0x437f90) | cc.sh DIFF(133) at default len; asmscore `--len 240` total 99142 (align=98 — masked jump-table reloc noise, byte_diff=52), insns **72/76** | Transcribed v520 (the v519 pickup's first fresh-gap tier). Identity ground-truthed by the class vtable dword at 0x4781ac and its fall-through call target 0x414b80 = `PopupWndBase::OnClose`. Content-complete: the `IsNetShuttingDownMaybe()` guard swallowing WM_CLOSE (same guard shape as `CreditsWnd::OnClose`), the `NotifyToolModeChanged` commit tail with the toolMode-keyed PostMessageA switch (case 8 declared first per the source-order rule), the vtable+4 `OnExit()` and both flag clears all pair. The WHOLE residual is ONE known class: the **/Og cross-jump-granularity residual already parked on `NotifyToolModeChanged` (0x438890) in this same file** — the original keeps each switch arm's own `g_pApp`/`[reg+8]`/`push 0x401`/`push hwnd` group and cross-jumps only the bare `call [PostMessageA]`; this compile suffix-merges everything from the `g_pApp` load onward (costing the case-8 arm's 4-insn group, hence 72 vs 76). Probes refuted (do not re-run): per-case `HWND hwnd` local in case-8 only, in the case-list only, in both — all three compiled byte-identical (exactly 0x438890's own three-probe refutation). Source-spelling-proof. Third site of this parked class in the TU (0x438890, here, same tail inlined in 0x4370f0/0x437f90) — a class-level crack pays off multiple rows at once. Retry only on that /Og suffix-merge class. |

## v519 — src/DecorActor.cpp + src/DPlaySessionMgr.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4345f0 | `DecorObjMgrMaybe::~DecorObjMgrMaybe` | 160 B | compiled **160 B**, DIFF(21), asmscore total 20144 (align=20 reg_pen=1 identity_miss=1 byte_diff=34), insns **46/46** | Transcribed v519 (the v518 pickup's first fresh-gap tier — the 0x434500 twin). Fell out of v518's ctor modeling exactly as the pickup hoped: the singleton definition already in this TU + the new in-class `~PlacedObjCollectionMaybe` (src/DecorObjMgrMaybe.h) inlining into a body that is only the manager-vtable re-stamp; members destruct in reverse declaration order (reg8, reg7, lockB, lockA) producing the original's three /GX states and both LockableMaybe dtor calls for free. The derived registry half declares NO dtor — its implicit one's vptr store dead-store-eliminates against the base's, which is why the original shows only the two BASE vtable stores. Content-complete: every instruction pairs. The WHOLE residual is TWO /Og scheduling tie-breaks: (a) both registry teardowns emit the compiler's vptr re-store BEFORE the body's first field zero where the original schedules nCount first, (b) the category-8 teardown's state-2 store vs array-null CMP order. Probes refuted (do not re-run): teardown as a user dtor on the DERIVED class (+36 B, DIFF 135 — second /GX state per member, derived vptr NOT eliminated); swapped nCount/nCapacity order (DIFF 23); dropping the pArray-hoist local (byte-identical, DIFF 21 either way). Retry only on the dtor-vptr-store scheduling class or a TU-context rotation. |
| 0x43f7b0 | `GameNet_TeardownAllSessionState` (free `__fastcall`; Ghidra `GameNet::TeardownAllSessionState`) | 204 B | compiled **204 B**, DIFF(4), asmscore total 12004 (align=12 reg_pen=0 identity_miss=0 byte_diff=4), insns **68/68** | Transcribed v519 (the v518 pickup's second fresh-gap tier). Content-complete first compile: the bracketing `GameNet_ResetProvidersAndPostTeardown` posts, the queueSourceId/searchProviderId zero pair (head load hoisted BEFORE them, the original's order), both queued-node list drains with the post-delete head RELOAD idiom, the 9-slot result-chain + layout-blob cleanup (hoisted slot pointer anchoring esi at pLayoutData, down-counter — NOT ResetProviders' direct-subscript form), and the ResetProviders(0)/SetMode(0) tail all pair instruction-for-instruction. The WHOLE residual is ONE two-instruction scheduling swap at the SetMode(0) call: original `push 0` BEFORE `mov ecx,edi`, this compile ecx-load first (the ResetProviders(0) call one statement earlier, identical shape, matched the original's order). Same push/load coin-flip class as v516's parked 0x440070 tail. Retry only if that scheduling class cracks. |
| 0x4287b0 | `WidgetPickerObj0x477cc8::HitTestNodeSecondary` (was `::TestMenuCommand`; re-declared v552 as the real vtable slot-17 override, byte-neutral — see the in-source note) | 448 B | compiled **460 B**, DIFF(286) — was 448 B/DIFF(368) pre-v519; asmscore total 440220 (align=440 reg_pen=0 identity_miss=0 byte_diff=220), insns **149/142** | v519 RETRY with CODEGEN #18m (the v518 pickup item; the shape that cracked sibling 0x4289a0): entry-initialized `unsigned char bResult = 0`, every in-switch exit as `return bResult`, `bResult = 1` on the pass paths. The old EBX-persistence residual is GONE (reg_pen → 0) and case 0x2c09 now pairs exactly (`mov bl,1` hoisted before GetLabelText + the `mov al,bl` tail). The REMAINING residual is structural: the original funnels every switch zero-exit through ONE shared `mov al,bl` epilogue (plus one `xor bl,bl` on the default/out-of-range path) where this compile proves bl==0 per path and materializes per-case `xor al,al` epilogues, and the shared wState tail closes `mov al,1` where the original has `mov bl,1; mov al,bl`. Measured and REJECTED (do not re-run): full #18m funnel (all cases `bResult = X; break;` to ONE `return bResult`, wState block duplicated per passed case — did NOT cross-jump; 484 B, DIFF 386); `goto pass;/goto done;` funnel (484 B, DIFF 392). Earlier refuted list stands (bResult assigned-only-at-use, && chains, goto sans accumulator). Retry only if the shared-epilogue-funnel class becomes source-steerable. |

## v518 — src/GameNet.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x438bc0 | `GameNetThreadState::GameNetThreadState` (Ghidra ex-`GameNetThread_InitState`) | 210 B | compiled **211 B**, DIFF(53), insns **77/77** | Transcribed v518 (the v517 pickup's first fresh-gap tier). Content-complete: the field-seed block (original's store order, incl. pOutbound/pInbound NULLed LAST), the screensaver-flag gate, the ResetNetManager + provider-list cons-copy (type-temp in edx, `a1`-form head store) and the 4-iteration ProbeComPort loop all pair instruction-for-instruction. The WHOLE residual is ONE register-allocation rotation inside the COM-port loop: original digit->al / buffer-arg->ecx / per-iteration g_pNetSettings reload->edx (`add al,0x30` = `04 30` short form), this build cl/edx/ecx (`add cl,0x30` = 3 B -> the +1 B) plus the byte store's SIB index/base swap. Levers baked in (do not undo): (a) the cons-loop node init spells `pCopy->nProviderType = pNode->nProviderType;` BEFORE `pCopy->pNext = ...` with NO temp — the only shape that lands the load in edx and the stores in the original's order; (b) the COM loop keeps an explicit `bool bPortExists` local (direct store moves inc-bl before the call and compensates the byte store to +0x13); (c) `cDigit` declared BEFORE `i` (the original's xor order); (d) the buffer is a function-scope `char[12]` (the original's frame is 0xc for a 2-byte string). Probes refuted (no byte change): named digit local; unsigned counter/index; loop-scope buffer. **The `#pragma inline_depth(0)`/`(8)` bracket is load-bearing**: without it VC5's /Ob1 inlines ProbeComPort at this site (266 B) where the original makes a real `call` to the out-of-line COMDAT 0x45ee60 — which this TU now emits and byte-matches (93 B EXACT). inline_depth(0) is the ONLY lever found that suppresses one call site's expansion: `auto_inline(off)`, argument spelling, and placing the body after the ctor in the TU all failed (VC5 inlines with whole-TU visibility — the body consequently lives mid-TU as an out-of-class `inline` between the two consumers, declared-only in GNetManager.h). Retry only on the al/cl temp-rotation class. |

## v517 — src/PeerTrainSlotQueueMaybe.cpp + src/DecorActor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x434720 | `DecorObjMgrMaybe::TickCategory7And8Maybe` | 212 B | compiled **212 B**, DIFF(75), insns **93/93** | Transcribed v517 (the v516 pickup's third fresh-gap tier). Content-complete: the 300-tick throttle clear, both pre-incrementing registry walks (each entry ticked with its successor; the last gets the past-the-end GetAt's NULL; category-8 gated on bValid == 1), and the `>= 2` lockBMaybe/SortAllMaybe tail (TickCategory7OnlyMaybe's exact shape) all pair instruction-for-instruction. The WHOLE residual is ONE register-role ripple: the original keeps `this` in EBP (EBX = loop counter), this build keeps it in EBX (counter in EDI) — every differing byte is that one assignment propagating through the two walks and the prologue's push/spill order. Levers baked in (do not undo): the pre-incrementing while (a `for (i = 1; i <= Count(); i++)` starts the counter at 1, not the original's EBX=0 + top-of-body INC), and Count() spelled as the loop condition so it is re-called every iteration. Probe refuted (WORSE, DIFF 171): hoisting the `i = 0` declaration above pActor's. Retry only on the EBX/EBP role coin-flip class. |
| 0x44dad0 | `PeerTrainSlotQueueMaybe::ReleaseSlotsOverlappingRectMaybe` | 214 B | compiled **213 B**, DIFF(126), insns **77/78** | Transcribed v517 (the v516 pickup's second fresh-gap tier). Content-complete: the 4-slot walk, the `(dwModeBMaybe != 2 \|\| pTile->dwTrackState == 4)` gate, the short-circuit OR overlap fold across the 4 car slots (IntersectRect against `pTile->rect`), and the unboard-8/detach/release tail all pair instruction-for-instruction. Levers that DID pay (baked in, do not undo): (a) outer walk is a `ppSlot++/n--` do-while with every use re-reading `*ppSlot` (an `aSlots[s]` for-loop caches the node in EBX and spills the walk — DIFF(185)); (b) the car index is UNSIGNED int (signed flips the backedge `jc`→`jl`); (c) the passenger loop hoists its base through an `int *pPassenger` local (plain indexing re-loads the node per store after LeaveTrainMaybe's call — DIFF(179)). The WHOLE residual is ONE instruction-selection coin-flip cluster rippling from the car fetch: original unfuses (`mov eax,[ebx]; add eax,esi; mov edx,[eax]` — pCar in EDX), this build folds (`mov eax,[ebx+esi]` — pCar in EAX), and every downstream register choice follows (push edx/eax for `&car->rect`, lea eax/edx for `&rcOverlap`, ecx/eax for the Detach node, dl/cl swap in the Release tail), plus one MOV+ADD vs LEA pick for the passenger base. Same VC5 /Og selection coin-flip family as v329's LEA scheduling-swap; possibly TU-phase carry from this TU's eight earlier partial siblings. Probes refuted (byte-identical both): pCar declared outside the loop; `&apPassengerMaybe[0]` for the decay. Header note: adding THIS declaration to PeerTrainSlotQueueMaybe.h did NOT flip WorldBoardMaybe.cpp's 0x457ce0 (the v492 dial's +1 rung) — measured full progress.py, both dial rows unchanged; the only cost was +26 B of partial-length churn in WorldBoardMaybe.cpp's partial rows. Retry only if the fused/unfused selection class cracks or the TU's partial siblings match. |

## v516 — src/DPlaySessionMgr.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x440070 | `GameNet_BroadcastRosterSnapshot` (Ghidra ex-`GameNet_PackAndBroadcastRosterMaybe`) | 218 B | compiled **218 B**, DIFF(6), asmscore total 12002 (align=12 reg_pen=0 identity_miss=0 byte_diff=2), insns **67/67** | Transcribed v516 (the v515 pickup's third fresh-gap tier). Content-complete: the 0x228 RosterSnapshotWireMsg alloc, the header stores (opcode 0x3f1, field_0x8 dword, the two grid-dim low bytes), the 9-iteration GameNet_PackRosterRecord loop (pSlot/pRec pointer pair, ebp down-counter), the NetMsgQueueNode build + EnqueueOrFreeNode, and the IsWindowVisible/DrawAvatarGrid/CommitScreenUpdate UI tail all pair instruction-for-instruction. The WHOLE residual is ONE scheduling coin-flip in the CommitScreenUpdate tail: the original pushes all three zero args BEFORE loading the receiver (`mov ecx,[ecx+0x220]`), this compile interleaves that load between the 2nd and 3rd `push esi` (identical instruction set, one push two bytes earlier). The sibling GameNet_RemoveSourcePeerAndReconcile (0x43f880) shows the SAME flip at its byte-identical original tail, so the tail's source spelling is confirmed shared and the flip is the TU-consistent /Og tie-break (Yoda #29/#30 family). Lever that DID pay (baked in, do not undo): declaring `pSlot`/`n` AFTER the four header stores pins the original's one-register this/pSlot aliasing (field reads via `[esi-0x510]` after `add esi,0x518`) — declaring them first costs a second register and 9 B (DIFF 139 → 6). Probes refuted (do not re-run): typed constants `((HDC)0/(RECT*)0)` — byte-identical; `!= 0` on the IsWindowVisible gate — byte-identical; `BOOL bVisible` local — WORSE (DIFF 8). Retry only if the push/load scheduling class cracks. |

## v515 — src/CreditsWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x40f890 | `CreditsWnd::OnDrawContent` (PopupWndBase vtable slot 0x1c override; Ghidra `FUN_0040f890`) | 228 B | compiled **203 B**, DIFF(174), asmscore total 119948-140329 (align=118-138 reg_pen=16-20 identity_miss=23-26 byte_diff=88-99), insns **64/60** | Transcribed v515 (the v514 pickup's first fresh-gap tier). Content-complete: the bResourcesLoaded gate, the one-shot bAnimationStartedFlag set with the real post-set re-read, the RestoreOverlapBlt of pFrameBitmap over rectWindow, and the BlitFadeCanvas + virtual SetCursorDesc(cursorNormal) + CommitScreenUpdate present tail all pair instruction-for-instruction. The WHOLE residual is ONE class: the original MATERIALIZES the {0,0,w,h} src rect as a real stack local (4 source-order field stores — right=EAX, bottom=EDX, left=EDI(0), top=EBX(0), two separate zero regs) with a fused offset-order field copy into the outgoing by-value arg slot; every plain-local spelling folds straight into the arg slot under /Og (203 B, 4 insns short). Probes refuted (do not re-run): aggregate `{0,0,w,h}`; bare 4-field assignment in left/top/right/bottom AND right/bottom/left/top order; `{0,0,0,0}` + right/bottom-only assignment; pointer-deref `*(&srcRect)` (folds); RECT-returning helper (plain static = NOT inlined, real sret call, 225 B; `__inline` = folds to 203 B); whole-struct copy `srcRect = rectWindow` + collapse-to-origin (folds, 188 B). Retry only if the by-value-local materialization class ever becomes source-steerable. |

## v514 — src/EffectSpawner.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4238c0 | `BigObjTrackingSetsMaybe::CtorMaybe` (EffectSpawner ctor, via TU-local `EffectSpawnerCtorViewMaybe`) | 242 B | compiled **240 B**, DIFF(54), asmscore total 18238 (align=18 reg_pen=2 identity_miss=2 byte_diff=18), insns **76/77** | Transcribed v514 (the v513 pickup's first fresh-gap tier). Content-complete: the SEH prologue with all three unwind states, the three embedded registry sub-object ctors in order (+0x4 candidate 0x477bd0/0x477b78, +0x1c/+0x34 placed/ghost 0x477b40/0x477ae8 — two out-of-line `call 0x435d10` reserves, ghost's reserve inlined as `new(0x190)` + `rep stosd` + neg/sbb/and collapse), the own-vtable 0x477ad0 stamp, and the virtual slot-19 `SetSortParamsAndSortMaybe(0xc, -4)` on the ghost set. Levers that DID pay (baked in, do not undo): the live count m_0c is zeroed at the BASE ctor's tail — the `mov [esi+0xc],0` sits between the reserve and the derived vtable stamp, which is where a base-ctor-body store lands (the Obj0x477758 pair carries m_0c on the derived half instead); and only the ghost site inlines the reserve, modeled by splitting the ghost member onto its own bodied-ReserveMaybe view class (candidate/placed declared-only). The WHOLE residual is TWO /Og tie-breaks: (1) the ghost collapse's `cmp eax,ebx` — the original tests the collapsed m_count against the resident zero register between the `and eax,0x64` and the store, this build branches on the AND's own flags (the same source text keeps its `test eax,eax` in the out-of-line 0x435d10 body in BOTH builds, so the fold is an inline-context coin flip); (2) the own-vtable + EH-state-2 stores schedule — original interleaves both into the ghost call's arg pushes (ghost vtbl in EDX), this build stamps before the call setup (EAX). Probes refuted (do not re-run): named-local collapse (the #20c lever — folds anyway, the value comes from neg/sbb/and with live flags, not a call), store-below-branch reorder (DIFF(53), store order goes wrong), `0 == m_count` (byte-identical), reference-form ghost call (byte-identical). Retry if the inline flag-fold / ctor-tail scheduling classes crack. |

## v513 — src/PeerTrainSlotQueueMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44d8a0 | `PeerTrainSlotQueueMaybe::ReleaseOrForwardMatchingSlotMaybe` | 269 B | compiled **269 B**, DIFF(5), insns 103/103 | Structure and register allocation byte-identical, including the three cross-jumped `*ppSlot=0; nActiveCount--; return 1` tail copies. The whole residual is ONE scheduling tie-break in the three-predecessor shared tail: original emits `mov al,1` right after `dec word [edi+4]`, ours sinks it between `pop ebp` and `pop ebx` (plus the two branch displacements that shift with it). Already-paid levers, do NOT undo: the search loop keeps its scan variable loop-local and re-fetches the winning slot (`pNode = aSlots[i]`) after the loop — that is what reproduces the original's `and ecx,0xff; mov esi,[edi+ecx*4+8]` rematerialization (218 → 5); and the loop is `while(true){...break; i++; if(i<4)continue; return 0;}` — a `for`/`do-while` + post-loop `if(i>=4)return 0` leaves an unfolded second `cmp cl,4`, and `for(;;){...if(i>=4)return 0;}` rotates the return out-of-line. Refuted: `else if` chain vs nested `else { if }` for the forward/delete branch (byte-identical). |

## v552 — src/WorldBoardMaybe.cpp (the duplicate-model fold's one toll)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x454fe0 | `WorldBoardMaybe_ResetAllTilesMaybe` (free `__fastcall`) | 211 B | compiled **211 B**, **DIFF(2)** — byte-for-byte the v522 residual, not a new one | Flipped OFF (again) by folding this TU's FIVE TU-local views of `SelectedObjWidgetMaybe` / `WorldActionCursor` onto the canonical classes. That fold is the point: `SelectedObjWidgetView0x456150` was a real duplicate DEFINITION (`pad0x0[0x88]`/`bActive`/`pad`/`animRectMaybe`) of a class that already existed, which is the one debt this project does not carry. **211 B is the fold's ENTIRE measured price** — no other function in this TU and no other TU in the repo moved, and in particular 0x457ce0's knife-edged 951 B SURVIVED, which is exactly what v551 feared and why the fold had been deferred. The residual itself is the documented commutative-load coin flip: the memset count `(wRows * wCols) / 8 + 1` loads wRows (+0x40) before wCols (+0x3e) in the original and the other way round here, and cl canonicalizes operand order so no source spelling can reach it. Do NOT re-autopsy; retry only on the next TU-context rotation, as at all seven prior flips of this same function. ⚠ **That old "REFUTED, do NOT re-run" warning about folding the sibling `DecorObjMgrPaintView0x456700` onto the real `DecorObjMgrMaybe` is ITSELF stale and is retracted:** v564 re-ran it and it was FREE (0x457ce0 kept all 951 B). Third confirmation of CODEGEN #186 — re-measure a priced-and-withheld toll before believing it. See the v566 section above for what DID finally cost 0x457ce0 its 951 B. |

## v512 — src/TilePlacedObj.cpp + src/WorldBoardMaybe.cpp (the 0x4589b0 bundle's dial tolls)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4588b0 | `TilePlacedObj::SpawnSeqRecordEffectMaybe` | 143 B | **v552: flipped OFF again (EIGHTH flip) — DIFF(79) at 141 B, byte-for-byte the sixth flip's residual.** Trigger: the `SelectedObjWidgetMaybe` blit pair (0x42d280/0x42d3a0) becoming real member declarations on `src/WorldActionCursor.h`, which unparked 0x42d280 for **+278 B** (CODEGEN #149) against this 143. Measured both ways — ONE of the two declarations costs the identical 143 B, so the step is flat and the pair was taken together. Do NOT re-autopsy; every probe was already refuted at the sixth flip. Historical: **RESOLVED v518 — EXACT again (seventh flip, back ON).** The v518 `DecorObjMgrMaybe.h` ctor/registry-model edits (0x434500 work) rotated this TU's /Og state back onto the good side; source untouched, as at every prior flip. Same session flipped RoadVehicleActor.cpp's 0x454050 back ON too. Historical residual: compiled **141 B**, DIFF(79) | Sixth EXACT→DIFF flip, paid as half of the v512 ctor-bundle toll. Bisected: EITHER of the bundle's two header edits alone flips it (the WorldActionCursor.h ctor+slot-16 decls, OR the WidgetBase.h ctor default args). Same residual kind as every prior flip: the original uses FOUR callee-saved registers (ebp = the default arm's rect.top scratch only), ours three — a pure /Og register-budget coin flip, no content difference. Probes refuted v512 (see the in-source comment): hoisted nRecX/nRecY (DIFF 97), hoisted nOriginX/nOriginY (DIFF 108), `long lSpace` (DIFF 74), #23 line-count dial ×5 (inert). Retry only on TU-context rotation, as always. |
| 0x457ce0 | `WorldBoardPartial::FindNearestObjOfCategoryMaybe` | 951 B | **RESOLVED v522 — MATCH (951 B EXACT) again.** The 0x44e020 landing (PeerTrainSlotQueueMaybe.h +1 declaration) rotated the dial the other way and recovered it; kept healthy through v522's 0x456c60 landing too. Historical residual: compiled **951 B**, DIFF(16), insns 327/327-paired | Paid as the other half of the v512 toll (the CODEGEN #78 measurement, now spent). Bisected: the WidgetBase.h ctor DEFAULT-ARGS edit alone flips it (the WorldActionCursor.h decls are not involved). Whole residual is the v375 zero-register-residency class + the commutative-LEA swap: original zeroes ECX as the ring counter (`xor ecx,ecx` / `cmp [esp+0x24],cx` / `movsx ebx,cx` / `inc ecx`), ours EAX, and the origin loads swap order (`lea esi,[ebx+eax]` vs `[eax+ebx]`). Probes refuted v512: 1-3 spare struct decls / 1-2 free decls / forward-class+free-decl at TU top (all inert — a SIGNATURE-shape dial touch, #8, not count), `short ring; for (;…)` and `++ring` (inert). Retry only if the zero-register-residency class cracks. |

## v511 — src/Obj0x477798Family.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x424790 | `EffectPlacedRegistryMaybe::SetAtMaybe` (derived table 0x477ae8 slot 10) | 129 B | compiled **129 B**, DIFF(1), insns all paired | Transcribed v511 with the rest of the effect-registry slot-9/10 set (its three siblings 0x424170/0x4246f0/0x424290 all landed EXACT from the same source text). The WHOLE residual is the ONE commutative-SIB byte the Walker/RoadVehicle derived twins 0x435a10/0x4360b0 already carry: the closing `return pArrayMaybe[nIndex];` encodes as `mov eax,[ecx+edi]` where the original picks the interchangeable `mov eax,[edi+ecx]` — no source spelling names the ModRM base (see the autopsy on 0x4124b0 in src/Obj0x477798Family.cpp). Its candidate-set twin 0x424290 escapes it the way base-side 0x4123a0 does, which pins the flip to the extra guard's register pressure. Retry only if that class cracks. |

## v510 — src/LocoBitmap.cpp + src/WorldBoardMaybe.cpp + src/EditCardWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x417040 | `EditCardWnd::OnKeyDown` (WindowBase vtable slot 0x54 override; Ghidra `FUN_00417040`) | 305 B | compiled **301 B**, DIFF(176), asmscore total 71272 (align=68 reg_pen=29 identity_miss=30 byte_diff=72), insns **99/101** | Transcribed v510 (the refreshed gap list's first fresh lead after the two v509-pickup targets). Content-complete: both mode gates, the wParam 0xd/0x1b test, the DefWindowProcA tail, the srcRect local→outgoing copy, the pExitBtn re-reads into DrawFrame/CommitRectUpdate, and the Sleep/virtual EndActiveSession/SetScreenState tail all pair. The WHOLE residual is /Og scheduling inside the exit block: the original reserves the outgoing srcRect area (`sub esp,0x10`) BEFORE computing the offset fields and loads the four rect fields in memory order (eax,ecx,edx,edi), this build computes two fields first and loads left,right,top,bottom into ecx,edi,edx,eax; plus the EndActiveSession vtable-pointer edx-vs-eax coin flip. Levers that DID pay (baked in): (a) a named `destRect` local (pRes->rect field copies, register-promoted) is what reproduces the original's MEMORY-RESIDENT srcRect — without it cl forwards the computed fields straight to the outgoing area (279 B, insns 97/101, total 157647: shorter code, much worse structure); (b) DrawFrame/CommitRectUpdate as direct `pExitBtn->` uses (the original re-reads [esi+0x2c4] per statement). Probes refuted (do not re-run): full direct `pExitBtn->` everywhere (DIFF 230/236); pRes for all three calls (DIFF 162 but insns 97/101, total 157647); the `&srcRect` pointer spelling (byte-identical); destRect/srcRect statement-order swaps (306 B total 77304, resp. byte-identical). Retry if the outgoing-area-reservation scheduling becomes steerable. |
| 0x454e60 | `WorldBoardPartial::Ddraw_InitTileGridExtent` (Ghidra `Ddraw::Ddraw_InitTileGridExtent`) | 315 B | compiled **315 B**, DIFF(4), insns all paired | Transcribed v510 (the v509 pickup's second fresh APP-gap lead). EVERY instruction pairs — the clamp ladder (with the shared `=0x400/=0x300` cross-jumped tail), the full field re-derivation pass in the original's exact store order (rcViewport origin, half extents before the w/h copies, scroll zeroes, client-rect midpoints, w/16 and h/16), the dirty-bitmap delete/realloc and the inline `rep stosd/stosb` 0xff fill. The WHOLE residual is ONE commutative-load scheduling coin-flip, twice: the original loads wCols (+0x3e) before wRows (+0x40) in both `wCols * wRows` movsx pairs (alloc size, memset count), this build loads them the other way round. Probe refuted (byte-identical, do not re-run): the `wRows * wCols` operand-order flip — cl canonicalizes the commutative imul's load schedule, same class as 0x405ab0's add-accumulator coin-flip (v509). Retry if that class cracks. |
| 0x42cb10 | `LocoBitmap::IsPixelTransparentAtMaybe` | 321 B | compiled **319 B**, DIFF(116), asmscore total 61711 (align=60 reg_pen=15 identity_miss=15 byte_diff=61), insns **100/100** | Transcribed v510 (the v509 pickup's first fresh APP-gap lead; verified NOT inside the parked blit family — it sits past 0x42c9f0, after both HasOpaquePixel siblings). The bounds checks (with the `cmp reg,mem; jg` polarity), the converted fall-through layout, the entire Lock block, both load-from-local epilogues and the 8bpp tail pair instruction-for-instruction. The WHOLE residual is the 16bpp pixel-test cluster, the same intrinsic scheduling class as 0x42c9f0's baked-in note: the original keeps the pixel in dx (with a dead `lea eax,[ecx+2*eax]` remat artifact right after the load), the red result in eax, and loads g_nBBitMask INTO a register (`mov ecx,[g_nBBitMask]; and ecx,edx`) where cl here folds it (`and eax,[g_nBBitMask]` — that fold is the 2-byte shortfall, the exact twin of 0x42c9f0's "2 missing instructions" fold); and cl promotes bTransparent to bl across the Unlock call (xor bl,bl / mov bl,[local] select) where the original keeps it memory-resident at all three sites. Levers that DID pay (baked in, do not undo): (a) bounds spelled `x > r.right` / `y > r.bottom` (the original's `jg` form; `r.right < x` flips the cmp operands); (b) converted path as the FALL-THROUGH with the 8bpp scan trailing (OPPOSITE of HasOpaquePixelInRect's baked-in lever — measured). Probes refuted (ALL byte-identical, do not re-run): a pPixel pointer local; an nStride local; the `!(red==0x1f && blue==0x1f)` parenthesization; pixel declared before the desc. Side finding: Ghidra's decompile of this function silently DROPS the whole pixel-load/mask-test block and shows the converted return as a byte of the desc pointer — the "original bug" the header comment used to record was Ghidra's blind spot, now corrected in src/LocoBitmap.h. Retry if the mask-fold/reg-rename cluster class cracks. |

## v509 — src/WidgetBase.cpp + src/WorldBoardMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x455960 | `WorldBoardMaybe::EraseAlongDragSegmentMaybe` (via TU-local `WorldBoardClickView0x455d60`) | 323 B | compiled **321 B**, DIFF(203), asmscore total 58258 (align=58 reg_pen=2 identity_miss=3 byte_diff=28), insns **107/107** | Transcribed v509 (progress.py's fresh APP-gap lead). The integer prologue, the full local slot map, the ENTIRE loop body (unit-step fadds, both __ftol `x < 0 ? -1 : x >> 4` clamps with the `or edi,-1` idiom, the GetTopPlaneBSlotMaybe + FlushQueuedBigObjMaybe calls, `dist -= 1.0` bottom test) and the epilogue pair instruction-for-instruction. The WHOLE residual is the FP-stack scheduling in the setup block: cl 11.00 spills the parked x/y doubles before the squares' `faddp` and carries one fewer live FP-stack entry through the two `fdiv`s than the original, shifting every fxch index (original: sum first, fx spilled pre-fsqrt, fy spilled after the divisions; ours: both spilled pre-sum). Levers kept: `double dist;` declared uninitialized first; fdx/fdy as raw `(double)` conversions divided in place after the sqrt; tile coords as `short aTileXY[2]` (forces the word stores + dword re-reads); dx declared before dy. Probes refuted (byte-identical): fy declared after the dist assignment OR after both divisions. Refuted with regression: swapped sqrt sum order (DIFF(206), 319 B). Retry if cl's FP-stack scheduling around parked doubles ever becomes source-steerable. |
| 0x405ab0 | `AnimDescRefObj0x477488::EnsureSoundPlayingMaybe` (via TU-local `AnimDescRefSoundView0x405ab0` — the v349 header-parity note in src/WidgetBase.h still stands) | 329 B | compiled **329 B**, DIFF(10), asmscore total 1110 (align=0 reg_pen=10 identity_miss=10 byte_diff=10), insns **115/115** | Transcribed v509 (the third fresh APP-gap lead, 0x405ab0/329 B). Content-complete: the g_pDSoundManager/id gates, the id-change release + SoundBank_LookupEntryById re-lookup (bLoaded != 1 rejection), the 0xffffffff-sentinel id republish, both AcquireChannelForSound arms (bLoop = no-retrigger-delay), the positive-delay `rand() % delay + 1` arming, the negative-delay `delay + rand() % (2 - delay)` jitter (with the `delay != 2` guard), and the live-channel ResumeOrRestart arm all pair instruction-for-instruction. The WHOLE residual is TWO instances of the commutative-add accumulator coin-flip: the original folds `g_dwGameTick + nDelay` into the tick register (`mov eax,[tick]; add eax,edx; store eax`), this build folds it into the nDelay register (`add edx,eax; store edx`), in both arms. Levers that DID matter (kept): existing-channel arm written FIRST (`if (pDSoundChannel != NULL)` — the original's fall-through; inverted order swaps the block layout); the temp split `nDelay = rand() % delay + 1; dwSoundResumeTick = g_dwGameTick + nDelay;` reproduces the original's `inc edx` where the fused expression emits `lea ecx,[edx+eax+1]`; field re-reads (`pFrameEntry->nSoundRetriggerDelay`) in both modulo operands drop the callee-saved ebx the local-cached form spends. Probes refuted: operand-order flips (both directions, both arms — cl commutes regardless), parenthesization, `g_dwGameTick + nDelay + 1` (re-folds to the lea, DIFF(55)), local decl-order swaps. Retry if the add-accumulator coin-flip class cracks. |

## v508 — src/DSound.cpp + src/WorldActionCursor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x45b3a0 | `WorldActionCursor::OnKeyDownMaybe` (slot 16 override; retyped onto the class v512, view retired) | 339 B | compiled **337 B**, DIFF(6), asmscore total 12005 (align=12 reg_pen=0 identity_miss=0 byte_diff=5), insns **112/112** | Transcribed v508 (progress.py's third fresh APP-gap lead). Content-complete: the base-chain call, the VK_UP/VK_DOWN switch (both visible+idle guards, SetNodeState(2)+wSelIndexMaybe=6), the HandleTextEditKey default, the SelectedObjWidgetMaybe fallback, and all three label-commit arms (mode 6 lead-car SetNameImpl, mode 7 decor SetNameImpl + category-7 resort + page refresh, default decor SetNameImpl) pair instruction-for-instruction. The WHOLE residual is ONE dead store the original keeps and this build eliminates: the fallback epilogue's `mov bl,al` (bHandled's register home) before the pops — 2 bytes plus the 3 shifted rows. Levers that DID matter (kept in-source): both dispatches are `switch` with default-first/descending case order (layout oracle); the label-commit block is a POSITIVE `if (bHandled)` guard with the fallback as trailing plain-statement tail (the negative early-exit form inlines the fallback before the mode switch, DIFF(144)); per-exit switch duplication for cross-jumping does NOT fire (472 B, refuted). Probes refuted (byte-identical): three fallback spellings incl. a second `bool` local — dead-store elimination is cl's coin-flip, same class as 0x40c3d0. Side-product landed: `UiIconListItem::HandleTextEditKey`'s return type corrected uchar→bool (the un-normalized `mov bl,al` after its call pins it; both prior callers insensitive). The honest slot-16 override declaration (measured at -143 B on the parity dial) was PAID v512 as part of the 0x4589b0 ctor bundle — the TU-local view is retired and the body is the real member now. Retry if the redundant-move-elimination coin-flip class cracks. |
| 0x45b7e0 | `DSound_GetOrCreateManager` (Ghidra `DSound::DSound_GetOrCreateManager`) | 347 B | compiled **340 B**, DIFF(188), asmscore total 92597 (align=92 reg_pen=5 identity_miss=5 byte_diff=47), insns 105/100 | Transcribed v508 (the v507 pickup's second fresh lead), CONTENT-COMPLETE and branch-exact: both early-bail gates, the `new DSound`/EH-state sequence, the InitDeviceAndChannelPool(0x10, hwndOwner) failure delete, the SetListenerPosition viewport pair, the ini-present fall-through with the 75/75/78 defaults trailing `else`, and the High-passed-twice ApplyIniVolumeDefaults call all pair instruction-for-instruction. The ENTIRE residual is ONE instance of the v375 zero-register-residency class, polarity "ours hoists, original doesn't" (same as 0x434100, reversed from 0x4349d0): the original spells all four NULL gates `test reg,reg` and both zero stores (EH-state-0 + the post-delete singleton clear) as immediates; this build hoists `xor esi,esi` in the prologue and spends it on all six (7 bytes shorter), which also parks VolumeLow in esi where the original uses edi (the one knock-on rename). Levers that DID matter (kept): the ini-present arm as fall-through with defaults in the trailing `else` (the original's `je` outlines the defaults), Low/Med/High read in that order. Probes refuted (all byte-identical): the `nLow`/`nMed` sibling declaration-order swap, routing the new-expression through a named `DSound *pNewManager` local. Retry only if the v375 class cracks. |

## v507 — src/GameNet.cpp (GameNetThreadState virtual dtor)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x438cc0 | `GameNetThreadState::~GameNetThreadState` (vtable 0x4781c4) | 371 B | compiled 367 B, DIFF(113), insns **136/137** | Transcribed v507, CONTENT-COMPLETE — prologue, thread-stop gate, GNetManager teardown, all three peer-train-list drains, the lock/unlock pair, and both file-transfer-list drains are instruction-identical. The whole residual sits in the send-queue drain and is ONE coupled /Og register cascade, keyed off the loop bottom: the original caches pNode in eax and stores the new head through the `a3` short form (`mov eax,edi; cmp edi,ebx; mov [head],eax; jne` vs this build's `cmp edi,ebx; mov eax,edi; mov [head],edi; jne` — same three statements, coin-flip). That one flip explains everything downstream: with eax == head invariant at the original's loop top, its payload-null-check path skips the head reload (`je` straight to `mov [eax+8],ebx`) and the post-delete head read resists CSE (two `a1` reloads kept — ours CSEs to one, the missing 137th insn), and the type-0xe virtual delete dispatches `mov edx,[ecx]; call [edx]` vs our `mov eax,[ecx]; call [eax]`. Levers that DID matter (kept, all in the in-source autopsy): `!= 0xe` with operator delete as fall-through; `delete (NetMsgPayloadObjMaybe *)pNode->pPayload` through the node (keeps delete's null check unfolded — the pPayload-local spelling folds it); a fresh `pNode = g_pNetMsgSendQueueHead;` read before the payload-null store (lands the store base in eax). Refuted (byte-identical): hoisted pPayload/pNext decls, named payload local, chained loop bottom. Retry axis: any source shape that makes the loop-bottom head store land in eax (`a3`) should cascade the rest closed. |

## v506 — src/BuildToolButton.cpp (WidgetTagObj0x478378 slot 10)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44ec50 | `WidgetTagObj0x478378::AdvanceAnimFrameMaybe` (slot 10 override) | 140 B | compiled 143 B, DIFF(75), insns **56/56** | Transcribed v506, CONTENT-COMPLETE. ONE residual class: the original reads the localized point back through ComputeLocalPos's returned hidden-buffer pointer (`mov ebx,[eax]` / `mov ebp,[eax+4]`, with the `test edi,edi` loop guard scheduled BETWEEN the two loads) where this build addresses the same buffer frame-relative (`mov ebx,[esp+0x10]` / `mov ebp,[esp+0x14]`, both ahead of the test). The by-value POINT return makes the two addressings alias exactly; which one cl picks is /Og scheduling state, not source shape. Probes refuted: x/y declaration order (byte-identical score), direct ptLocal.x/.y reads in the loop (worse — per-iteration stack reloads, DIFF(89)). Levers that DID matter (kept): named `int x`/`int y` locals to force the ebx/ebp caching, and hoisting the pMenuListHead read into a pre-call local (matches the original's pre-call `mov edi,[esi+0xd0]`). |

## v505 — src/WorldActionCursor.cpp (SelectedObjWidgetMaybe method run)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x42d1a0 | `SelectedObjWidgetMaybe::AdvanceAnimFrameMaybe` (slot 10 override, ex-`TickFollowSelectionMaybe`) | 209 B | compiled 205 B, DIFF(98), asmscore total 24018 (align=24), insns **71/71** | Transcribed v505, CONTENT-COMPLETE. Three stacked /Og lottery items: (a) the original loads +0x194 into ecx ahead of the second center compare, this build memory-compares it; (b) the `bMoved` materialization is full-width `xor eax,eax`/`mov eax,1` there vs byte-width `xor al,al`/`mov al,1` here (same register, pure instruction-selection tie — `int` and `char` spellings of the local both score WORSE, refuted; cached-locals spelling refuted); (c) the epilogue splits into two rets here vs one shared pop-tail there. |

## v500 — src/EffectSpawner.cpp (new TU)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x423ab0 | `EffectSpawner::EffectSpawner_SpawnAtPositionMaybe` | 402 B | compiled **371 B**, DIFF(177), asmscore total 123822 (align=122 reg_pen=16 identity_miss=16 byte_diff=62), insns 116/126 | Transcribed v500 (the v499 pickup's next-gap lead), CONTENT-COMPLETE — every gate instruction-aligned: the minFlyingFps fild/fcomp/`test ah,0x41` frame-rate gate with the 0x3861 override, the MaxInstances check (zero-extended word count vs dword cap), the must/cant-have pair (must-have descriptor loaded unconditionally BEFORE its id==-1 test), the new/EH-state sequence, the `bValid == 1` split to the two embedded collections' slot-0x34 Add, and the scalar-deleting-dtor delete. Residual is ONE /Og cascade: the original never materializes a zero register (`test eax,eax` + `cmp word,0` at the cant-have gate, immediate EH-state-0 store, ESI keeps pDesc until the ctor result) while this build hoists `xor esi,esi` there and spends it three times — which also tail-merges the delete path into the early-bail `xor eax,eax` epilogue the original keeps as a separate fourth epilogue. The v375 zero-register-residency + v326/v328 /Og block-layout classes. Probes refuted (all byte-identical): `!= 0` vs `> 0` on the count, positive-bail vs nested gate, redundant `if (pEffect != NULL)` around the delete. |

## v499 — src/EditCardWnd.cpp + src/BuildToolButton.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x41a8a0 | `EditCardWnd::OnTimerDefaultMaybe` (slot 0x30, WM_TIMER) | 410 B | compiled **410 B**, DIFF(39), asmscore total 14674 (align=12 reg_pen=24 identity_miss=24 byte_diff=34), insns **117/117** | Transcribed v499, CONTENT-COMPLETE. Levers landed: the dispatch is a `switch` on wParam (only the switch yields the original's reverse-order case-body layout — 0x53 first, 0x4d, 0x44 last); miss/hit arms spelled `if (bRemoved != 0) { hit } else { = 200 }` (the original's `je` outlines the =200 store after the hit body); cooldown gate is `<= 0` (`test eax,eax; jg`), not `< 1` (`cmp eax,1; jge`). Residuals: ONE push-scheduling coin-flip at the RemoveDecalAtPoint call (original computes xRel, yRel, then pushes y/x; ours pushes yRel before computing xRel — named locals and a POINT temp both compile byte-identically, refuted) plus pervasive eax/ecx/edx permutations across the KillTimer/SetTimer/DefWindowProcA pushes — the documented symmetric-register-swap intrinsic class. |
| 0x44ac20 | `BuildToolButton::HandleMenuCommandMaybe` (slot 20) | 420 B | compiled **460 B** COMDAT, DIFF(126), asmscore total 132726 (reg_pen=6 identity_miss=8 byte_diff=46), insns 152/149 | (row also filed under the v393/v394 `src/BuildToolButton.cpp` section below) Transcribed v499, CONTENT-COMPLETE — the whole body is instruction-aligned except ONE residual: the candidate's 0x240d gate emits `xor eax,eax` before `setne al` where the original reuses the switch-index EAX with no pre-clear. Byte-for-byte the SAME xor-pre-clear-before-setcc sub-case 0x4497a0's gate carries (FIVE spellings refuted there), so it parks with that class. VC5 lays switch bodies out in source order — the original's non-numeric case layout (0x2407/0x2408/0x240b/0x240e/0x240d/0x240c/0x240f) pinned the source's case order. |

## v497 — src/WorldBoardMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x457080 | `WorldBoardPartial::CaptureBoardToBitmap` | 597 B | compiled **632 B**, DIFF(276), insns 188/194 | Newly transcribed this session (the v496 pickup's ⭐⭐ top target). Instruction-for-instruction identical from the prologue through both switch dispatches and all four footprint-coordinate arms — the entire residual is VC5 cross-jumping TWO identical tail pairs the original keeps duplicated: case 0xc24's `[cmp ecx,ebp; jne; mov byte [ebx],5; jmp]` into case 0xc1e's copy, and case 0xc22's `[test ecx,ecx; jz; mov byte [ebx],5; jmp]` into case 0xc20's, each insertion of a 5-byte jmp deleting a 10-byte block and shifting everything downstream 20 B. The original's four copies are byte-identical apart from branch WIDTH (near in the early cases, short in the late ones) — our compile is simply the BETTER-optimized one, i.e. the v348 0x40e520 class ("cross-jumping is downstream of register allocation"; a candidate can lose by being better optimized), except here the copies do not even differ by register, so there is no allocator asymmetry to chase either. Every source axis that reproduces the shape is already in place: the guard's third disjunct folded into the `||` (that is what makes the C-term successes jump to the SHARED write5 like the original's), inner cases in descending source order 0xc24/0xc22/0xc20/0xc1e (layout oracle), outer order 3,2,4,13,12, `pOut++, col++` increment order, `unsigned short` loop vars, `bool` return (`mov al,1` epilogue). Probes all INERT — see the in-source autopsy; retry only if the cross-jump-with-jump-insertion class ever cracks, or re-probe after future `src/WorldBoardMaybe.h` dial work (it is the same header). |

## v475 — the CarNetObj promotion (0x430 → full 0x450) TU-state rotation — ✅ FULLY REPAID

Kept as a worked example, with no live rows left. `src/CarNetObj.h`'s class was promoted from the
0x430-byte `CarNetObjPartial` to the full 0x450-byte `CarNetObj`, absorbing the anchor/mode tail
that `src/PeerTrainNode.cpp`'s `CarNetObjAnchorPartial` and `src/NameAnchorMaybe.cpp`'s
`CarNetObjModePartial` each used to declare separately — retiring a genuinely duplicated struct
layout (the standing "never duplicate a struct definition across TUs" rule). Price at the time:
**−294 B / −2 EXACT**, both REGRESSIONS of functions whose own source was never touched
(`NameAnchorMaybe::ExtendAcrossTileBoundaryMaybe` 0x40cb10, `CarNetObjAnchorPartial::
StartFrameSoundMaybe` 0x40e130).

⭐ **v477 got BOTH back, for free, in one session and without touching either function.** Two
further `src/CarNetObj.h` edits — merging `RetagKind` into the real slot-15
`SetCarTypeAndCategory`, then adding `virtual ~CarNetObj()` + `RepositionForHeadingMaybe` +
`ResolveBothAnchorsToPointMaybe` — rotated the same `/Og` state back the other way. 0x40e130
flipped on the first of those, 0x40cb10 on the second.

⭐ **The transferable rule: a regression whose diff is entirely `r` rows (register renames at
identical instruction count) and whose source was never edited is a LOAN, not a loss.** Do not
pay it down by noise-fitting the source — v476 found that re-declaring a parameter as an empty
`struct X : CarNetObj {}` restores 0x40cb10 exactly, and correctly rejected it as encoding
nothing true (docs/CODEGEN.md #8f). Keep the honest source, park the row, and keep doing real
work in the same header; the coin comes back. Both of these did.

## v474 — src/CarNetObj.cpp (new TU) + the src/CarNetObj.h TU-state rotation

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x40d770 | `CarNetObj::CarNetObj_ApplyNetState` | 275 B | compiled **276 B**, **insns 92/91**, total **52130**, align 52, reg_pen 1, identity_miss 1, byte_diff 20 | Newly transcribed this session in a new TU (`src/CarNetObj.cpp`), and it was the #1 pickup row. Block-for-block and instruction-for-instruction identical, INCLUDING the whole inlined `CarNetState::operator=` expansion — the entire residual is ONE spurious `xor ecx,ecx` in the prologue. cl materialises a zero REGISTER and then spends it on all three guard compares plus the latch clear (`cmp dl,cl` / `cmp edx,ecx` ×2 / `mov byte ptr [eax+0x424], cl`) where the original uses `test cl,cl` / `test edx,edx` ×2 / `mov byte ptr [eax+0x424], 0`. That is the zero-register RESIDENCY class (`docs/CODEGEN.md`, v375): no `if (p)` / `if (p != NULL)` respelling reaches it, and cl's heuristic actually misfires — the hoist costs 2 bytes and saves only 1 on the store. ⭐ **The block LAYOUT, by contrast, was fully source-steerable and is the session's transferable lesson — do not undo it:** the shared tail must be **written out TWICE**, once per early exit, so cl cross-jumps the copies back together. Measured, three compiles: `if (!flag) { copy } else if (p) { return 0 }` + one shared tail = 222314 / 287 B (outer polarity inverted, copy block on the wrong side); flipped to `if (flag) { if (p) return 0; } else if (p) { copy }` + one shared tail = 128395 / 287 B (right polarity, but cl emits the copy BEFORE the tail while the original has the tail first); the duplicated-tail form = 52130 / 276 B, exact layout. **Measured and INERT — do NOT re-run:** `unsigned char`+`0`/`1` instead of `bool`+`false`/`true` for `bStateAppliedMaybe` (byte-identical). Retry only if the zero-register residency class cracks. |
| 0x40e520 | `CarNetObjAnchorPartial::CheckCarClearedDepotMaybe` | 220 B | compiled **196 B**, DIFF(124) | ⚠ **REGRESSION, not a new transcription — this was EXACT from v349 to v473.** No source edit caused it: `src/CarNetObj.h` grew the real `CarNetState stateMaybe` member at +0x88 (every offset unchanged, `sizeof` unchanged), which pulls `CarNetState.h`/`PostBag.h` into this TU and moves the `/Og` state this function's register allocation rides on. cl is cross-jumping all four settle tails into one again — the exact v348 symptom, whose *cause* v349 correctly identified as TU position rather than source shape. **Two position probes run, BOTH failed:** hoisting the pair above `LayoutCarAnchorsMaybe` does not compile at all (`PeerTrainNodeTickPartial` is not yet defined there), and swapping it one slot later past `UpdateCarPlacementTickMaybe` leaves the TU at the same 13/26. Accepted because the same header change paid **+768 B / +3 EXACT** in `src/NameAnchorMaybe.cpp` (0x40b740, 0x40cb10, 0x40cc90 — all previously near-misses at DIFF 2/111/13), for a net **+328 B**. ⭐ It is a POSITION coin-flip, so **re-probe it after any future `src/CarNetObj.h` work** rather than treating it as settled. **v475 did exactly that re-probe** — the 0x450-byte promotion above rotated this TU again and neither this function nor 0x40e440 moved (both still DIFF at the same numbers), so the coin has now survived two independent `CarNetObj.h` rotations — this is the same "parked coin un-flips on unrelated header work" class that recovered 0x4553e0 in v473. |
| 0x40e440 | `CarNetObjAnchorPartial::CheckCarEnteredDepotMaybe` | 220 B | compiled 220 B, DIFF(15) | ⚠ Same regression and same cause as 0x40e520 above, milder half: the length is still correct and only ~15 bytes of the EAX↔EDX scratch-temp swap in cases 7/9/10 are back. Same two failed position probes, same recovery lead. |

## v472 — src/UIResources.cpp (the /Ob1 inline-vs-COMDAT contradiction)

⭐ **v471's 0x4553e0 row is DELETED — `WorldBoardPartial::TestOrClearFootprintMaybe` is EXACT
again (572 B), recovered for free.** Its source was never touched in either direction. The
declaration that flipped it back is `UIResources::SoundBank_PreloadWavRange` landing on
`src/UIResources.h` (below) — a header this TU consumes and whose new method it never names.
That is now the THIRD recorded instance of an unrelated header edit flipping this coin back
(after `TilePlacedObj`'s 0x458310 / 0x4588b0), and it confirms v471's own advice to retry these
opportunistically rather than autopsy them.

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x447930 | `UIResources::PlayUiSound` | 153 B | DIFF(34) at a compiled 58 B | Both this and 0x4479d0 below inline `UIResources::SoundBank_LookupEntryById` (0x4472b0) in the original — their copies index `m_apSoundBankEntries` absolutely off `g_UIResources` and load `ecx = 0x4855e8` explicitly for the `SoundBank_PreloadWavRange` call, which is exactly what inlining a call whose `this` is the constant `&g_UIResources` produces. ⛔ **This is a TOOLCHAIN contradiction, not a source shape — do not re-autopsy it as one.** Measured both ways: with the helper left non-inline (what is committed) VC5 `/O2` implies **`/Ob1`**, which expands ONLY functions marked `inline`, so these emit a `call` and compile to 58/53 B; with the helper marked `inline`/`__inline` both callers compile to **exactly** 153/149 B (DIFF(97)/DIFF(28) of ordinary residual) but cl then emits **no out-of-line COMDAT for the helper at all** — and 0x4472b0 has ~30 real callers in other TUs, so that copy must exist. Its marker also loses its COMDAT and mis-pairs, silently corrupting every later score in the file. 127 B of EXACT was taken over 302 B that would leave the helper unclaimed and the pairing broken. Retry only if a source spelling is found that inlines at these two sites while still emitting the external copy. |
| 0x4479d0 | `UIResources::PlaySoundAtScreenPos` | 149 B | DIFF(37) at a compiled 53 B | Same cause as 0x447930 above; same park. Differs from it only in taking the position/category from parameters and in omitting PlayUiSound's redundant `pEntry != NULL` re-check (not an engine bug — `AcquireChannelForSound` at 0x413210 tests its own descriptor argument and bails). |

## v470 — src/GameNet.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x43c160 | `GameNetThreadState::TrainNet_TryBoardEdgeHandoffMaybe` | 685 B | compiled **681 B**, **insns 255/257**, total **70133**, align 70, reg_pen 1, identity_miss 1, byte_diff 23 | Newly transcribed this session (was declared-only, and the #1 declared-only row by size with an existing home TU). Content-complete and structurally pinned: all four edge tests in order (bottom/top/left/right → headings 0xb4/0/0x10e/0x5a), both branch polarities, the bottom-edge-only `bDirty` guard, the four reflected `wLocalHeading` values and the shared stall-checkpoint tail all pair one-for-one. **The whole residual is ONE peephole coin-flip repeated three times, and the ORIGINAL is internally inconsistent about it:** `ProviderSlotAt`'s inlined `i >= 0` guard becomes a bare `js` on our side everywhere, while the original emits `js` only for the TOP edge (index computed with `sub`) — which matches us exactly — and the long `cmp <idx>, <zero-reg>; jl` for the other three (`add` / `inc` / `dec`). Same compiler, same inline helper, same shape; cl simply declines the flag-reuse peephole after those three opcodes. The 2-instruction deficit IS those three `cmp`s, and the single reg_pen is the knock-on operand-order coin-flip on the commutative `add` (the original parks `bOwnerByteB` in EAX and the row width in EDX; cl the other way round). ⚠ The left/right edges have **no operand order to flip at all** — `dec eax`/`inc eax` are byte-identical on both sides and only the following test differs, which is what rules out any source-level lever. **Measured and INERT, one compile each — do NOT re-run:** the `if (i >= 0) { return &aProviderSlots[i]; } return 0;` spelling of `ProviderSlotAt` (byte-identical); swapping the commutative add's operands (byte-identical, cl normalizes); `nSlot = ...; nSlot += ...;` (byte-identical); block-scoping `nSlot`/`pSlot` per branch instead of one function-scope pair (byte-identical). **Measured and WORSE:** inverting `ProviderSlotAt` to `i < 0 ? 0 : &aProviderSlots[i]` (DIFF 260 → 282). Retry only if the flag-reuse-peephole class ever cracks. |

## v469 — src/GameNet.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x43aa00 | `GameNetThreadState::AttemptJoinOrHostSession` | 514 B | compiled **512 B**, **insns 144/144**, total **12340**, align **10**, reg_pen 21, identity_miss 21, byte_diff 30 | Newly transcribed this session (was declared-only, and the #2 declared-only row by size). The instruction SEQUENCE pairs one-for-one across the whole body — every diff row is an `r`. The residual is a single register coin-flip and its knock-on: the original keeps `g_pNetManager` in **ESI** across the `\|\|` guard and `g_pLocalPlayerIdentity` in **ECX** through the host-branch `wsprintfA` block, where cl picks EDX and EAX — a 3-cycle permutation (eax→edx→ecx). ⚠ **The 2-byte length gap is NOT missing body:** `mov eax,[mem32]` is the 5-byte `A1` accumulator encoding while `mov ecx,[mem32]` is the 6-byte `8B 0D` form, and that global load happens TWICE in the host path (0x4e, 0x75) — 2 registers × 1 byte. Same family as the `test al,al` (original) vs `cmp al,bl` (ours) selection at 0x30, which is 2 bytes either way. **THREE levers ARE baked in, do not undo:** (1) `bool bTransportLive = g_pNetManager->pDirectPlay4 != 0;` as a named local — the documented CODEGEN `bool <name> = <ptr> != NULL;` intermediate, which is what produces the original's `xor eax,eax; cmp; setne al; test al,al` instead of a bare `cmp/je` (156093 → 138405; all byte-sized types tie, `int` loses the setne entirely); (2) the two early bail-outs combined into ONE `\|\|` guard — worth 172518 → 52359 by collapsing two duplicated inline epilogues into the original's single shared tail, and splitting them back apart now scores 132499, so the `\|\|` is load-bearing, not cosmetic; (3) the transport-dead bail-out written as its OWN source copy of the type-5 block rather than a `goto` to the shared one (52359 → 12340, insns 143 → 144/144) — the original's compiler tail-merged two source copies into one block entered by `push 0x1c; jmp`, which a single shared block cannot reproduce. **Measured and INERT — do NOT re-run:** a local for `pApplSetupWnd`; hoisting `bTransportLive`'s declaration to the top beside the buffers; `unsigned char`/`char` for it (identical); the label-inside-the-`if`-body layout (172518). Retry only if the register-coin-flip class cracks. |

## v464 — src/PopupWndBase.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|

## v463 — src/BuildToolButton.cpp (the WidgetTagObj0x478378 vtable cluster)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44ece0 | `WidgetTagObj0x478378::LayoutMenuIconGridMaybe` | 152 B | compiled **155 B**, **insns 61/61**, total **12229**, align 12, reg_pen 2, identity_miss 2, byte_diff 9 | Newly transcribed this session. THREE diff rows, all one register choice. The original has a spare register at the SetRect and keeps `pIconDesc` in EDX, zero-extending `wShadowBitmapHeight` into a pre-cleared EAX (`xor eax,eax; mov ax,[edx+0x2a]` — the documented unsigned-short widening form, which independently confirms the field's declared type). cl reuses EAX for `pIconDesc` and masks afterwards (`and eax,0xffff`). Same instruction count, one different register. **Measured and REFUTED — do NOT re-run:** caching `pLastHitNode` in a local, and caching `pIconDesc` in its own local, are both BYTE-IDENTICAL to the kept form; declaring `nCell` `int` instead of `unsigned short` is strictly WORSE (DIFF 121 / 150 B — it drops the widening entirely, which is what pins the field unsigned). The kept form re-reads `pLastHitNode` for the `Draw()` rather than holding it, matching the original's two separate loads. Retry if the spare-register class ever cracks. |
| 0x44f190 | `WidgetTagObj0x478378::OnKeyDownMaybe` | 122 B | compiled **134 B**, **insns 44/39**, total **50801**, align 50, reg_pen 7, identity_miss 7, byte_diff 31 | Newly transcribed this session. Content-complete — both accelerators, both guards, the repeat-arm value 3 and the fallback all verified against raw disasm. The five extra instructions are ONE allocator decision plus its knock-on: the original keeps `bHandled` in AL for the whole body (it is never live across a call there — each arm assigns it AFTER its `SetNodeState` calls, and the default path has no call at all), needing only two callee-saved registers and comparing the node state against an IMMEDIATE. cl gives `bHandled` its own callee-saved EBX (`push ebx` + `mov bl,al` + `mov al,bl`) and then reuses that register to hold the constant 1, turning both `cmp word ptr [ecx+0x48], 1` into `cmp word ptr [ecx+0x48], bx` and adding a `mov ebx,1` per arm. **Measured and INERT:** hoisting `bHandled = true` to the top of each case is BYTE-IDENTICAL. **REFUTED ON EVIDENCE despite a better score:** rewriting the switch as `if (nKey == VK_LEFT) … else if (nKey == VK_RIGHT)` scores 132 B / DIFF(94), but emits `cmp edi,0x25` where the original emits the `mov ecx,edi; sub ecx,0x25; je; sub ecx,2; jne` subtract-chain only a switch produces — structural evidence beats the byte count. |

## v461 — src/EditCardWnd.cpp + src/LocoBitmap.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x419260 | `EditCardWnd::StepDecalPickerScroll` | 754 B | compiled **771 B**, **insns 241/239**, total **140961**, align 136, reg_pen 44, identity_miss 44, byte_diff 121 | Newly transcribed this session (was declared-only, and the joint-largest unclaimed APP gap). Block layout, branch polarity and the entire control-flow skeleton pair instruction-for-instruction; every residual descends from ONE callee-saved-register tie-break in the FORWARD half. Five values are live across the `ClipartBitmapCache_GetOrLoad` call — `this` / the element-address induction variable / `nIdx` / `nSlot` / `nWidth` — against four callee-saved registers: the original spends EBP on `nSlot` and demotes `nWidth` to ECX plus a spill through `[esp+0x14]` (the slot `nOldScrollB` already owns); cl here spends EBP on `nWidth` and homes `nSlot` in memory. The 4 extra instructions ARE that spill code, and the downstream `cmp r,ebp`-vs-`test r,r` and `xor r,r`-before-`setg` rows are knock-on effects of EBP no longer holding a live zero/slot. **Measured and INERT — do NOT re-run:** (1) scoping `nSlot`/`nWidth`/the array cursor per-branch instead of per-function (VC5's allocator is live-range based, not scope based); (2) walking a `LocoBitmap **ppThumb` cursor instead of subscripting `paDecalThumbCache[nSlot]` — **BYTE-IDENTICAL both ways, tested twice**, so the pointer-vs-index question is unanswerable from the bytes and the subscript form is kept as the simpler one; (3) reusing `nOldScrollB` itself as the forward width accumulator to lengthen its live range; (4) initializing `nSlot` at its declaration (`int nSlot = 0;`) rather than inside the random arm, on the theory that the original shares one `xor ebp,ebp` between `nRandWalk`'s and `nSlot`'s zeroing — strictly WORSE, 140961 → 156978. **Three levers DID pay and are baked in, do not undo:** the `field_0x2b5` seed must be spelled `if (nOldScrollB <= 0) { =0 } else { =1 }` (the original `jg`s to the `=1` arm, i.e. the ZERO case is written first) — worth 196998 → 186995; dropping a redundant `nRandWalk = 0` from the random arm — 186995 → 180991; hoisting `field_0x2b4 = 1` above the `nOldScrollB` read — 180991 → 172977; and CODEGEN.md item-(5)'s `? true : false` lever on BOTH `field_0x2b5` result assignments, which drops the two `xor r,r`-before-`setg` widenings — 162975 → 140961. Retry if the callee-saved-register coin-flip class ever cracks. |
| 0x42c950 | `LocoBitmap::HasOpaquePixelInRect` | 147 B | compiled **142 B**, **insns 65/67**, total **146584**, align 144, reg_pen 23, identity_miss 21, byte_diff 74 | Newly transcribed this session (was declared-only). The ENTIRE scan — both loops, both zero-trip guards, the `mov edx,ebx; sub edx,eax; add ecx,edx` row-stride advance and all three exit blocks — is byte-identical to the original. The whole residual is one register coin flip in the PROLOGUE: `this` and the scan pointer `p` have overlapping live ranges (the address sum starts at the `imul` before the last `this` field read at `[r+0x18]`), and the original resolves it by keeping `this` in its incoming ECX and paying a `mov ecx,edx` once the address is final, where cl here pays a `mov edx,ecx` at ENTRY instead. That entry instruction is also what stops cl from sinking the four callee-saved pushes past the `bConverted` branch the way the original does — the original's converted path saves and restores NOTHING — and that is where both the 5-byte length shortfall and the epilogue tail-merge difference come from. **Measured and REFUTED — do NOT re-run:** writing the loop bounds inline (`y < r.bottom - r.top`) instead of as the `nRows`/`nCols` locals is much WORSE, 190289, which independently pins the named locals as the original's own shape. **One lever IS baked in, do not undo:** the raw-8bpp scan must be the FALL-THROUGH with the converted forward as the trailing tail (`if (bConverted == 0) { … } return twin(r);`); the natural-reading `if (bConverted != 0) return twin(r);` early-exit form emits the converted block first and scores 254870. |

## v456 — src/NameAnchorMaybe.cpp + src/WidgetBase.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x405e60 | `AnimDescRefObj0x477488::BlitAnimFrameMaybe` | 354 B code (368 B COMDAT incl. nop padding) | compiled **354 B (byte-exact code length)**, **insns 130/130**, total **672**, align **0**, reg_pen 6, identity_miss 6, byte_diff 12 | SIX instructions, one contiguous cluster, every diff row an `r`. Computing `nRight`, the original loads `rectClip.right` before `rectViewport.left` and this compile loads them the other way round, cascading into an EBP/EDX rename across the two hoisted loads that follow. Arithmetic order, branch sense, both SetRect arms and the entire blit tail pair instruction-for-instruction. **One lever IS baked in, do not undo:** `flags` must be mutated IN PLACE, not copied into a `blitFlags` local — the original keeps the PARAMETER's own value in the long-lived EDI across all three ORs, and the local makes cl park `nBlitFlags` there instead (DIFF 27 → 14, total 898 → 672). **Measured, do NOT re-run:** (1) spelling the first OR `nBlitFlags \| flags` is IDENTICAL — cl canonicalizes the commutative operand order, so it cannot steer register choice; (2) the swap-two-sibling-locals lever (declaring `nRight` before `nLeft`) is much WORSE, DIFF(224) at 349 B — which independently pins the original's declaration order. Retry together with 0x405fd0. |
| 0x405fd0 | `AnimDescRefObj0x477488::BlitOverlayFrameMaybe` | 466 B code (480 B COMDAT incl. nop padding) | compiled **466 B (byte-exact code length)**, **insns 177/177**, total **1447**, align **0**, reg_pen 13, identity_miss 13, byte_diff 17 | THE SAME residual class as its twin 0x405e60 above, in the same branch and on the same pair of expressions — the mirrored arm's nLeft/nRight loads get a different EDX/ECX/EDI assignment. Two functions landing on the identical cluster from independently written source is good evidence the class is intrinsic scheduling, not a source-shape error. Everything else pairs instruction-for-instruction, including both 16-bit-wrapping `SetAnimFrame((unsigned short)(nAnimValueCache ± 1), 0)` calls and the two-by-value-RECT blit tail. ⚠ **Do NOT transcribe this one from Ghidra** — its decompilation invents `unaff_EBX`/`unaff_ESI` and reads both by-value RECT arguments 8 bytes low, which makes the two SetRect calls look like writes to different locals; they target the same `rectSrc`. The hand-derived stack model is pinned by the `flag` parameter test at 0x406127 resolving to entry_esp+0x14. The dead first SetRect that falls out of that is a real original bug, recorded in docs/engine-bugs.md and reproduced `// sic:`. `flags`-mutated-in-place is baked in here too. |

## v454 — src/WidgetPicker.cpp + src/NameAnchorMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x427580 | `WidgetPickerObj0x477cc8::InitMenuIconsMaybe` | 580 B (COMDAT, incl. the 11-entry/44-byte jump table) | compiled **568 B**, **insns 190/185**, total **275216**, align 272, reg_pen 28, identity_miss 28, byte_diff 136 | Content-complete and verified instruction-by-instruction: both descriptor ids, the `pMenuListHead` first-call guard, all three entry NULL stores, the 20-iteration counted loop, all 9 switch arms and the 6-slot sub-loop. Case bodies are written in the original's own PHYSICAL order (0x2c02/03/05/0c/04/09/07/08/default), NOT the ascending order the jump table lists. ONE residual, a single allocator decision with a systematic knock-on: the original assigns EBX to the sub-loop's `nDy` and lets its zero constant die at `mov edi,eax` (EDI is zero only for the two SetDescriptor calls, then becomes `pDesc`), so all 17 in-loop zero arguments encode as 2-byte `push 0` immediates; this compile promotes 0 to EBX for the WHOLE function and spills `nDy` — so every one of those sites becomes a 1-byte `push ebx`. That is the entire 12-byte length shortfall, plus one extra `xor ebx,ebx` to re-establish the register after the sub-loop clobbers it, plus the `test edi,edi` → `cmp edi,ebx` pair v360's zero-reg lesson predicts. **Measured and INERT — do NOT re-run:** swapping the sub-loop's two same-type sibling locals (`nDy`/`nSlots`) in declaration order — the documented swap-two-sibling-locals lever scores an IDENTICAL 275216, because the contest is between the CONSTANT and `nDy`, not between the two locals. **One lever IS baked in, do not undo:** the two descriptor guards must be a single combined `&&` over an `unsigned char bOk` local with a trailing `else { bOk = 0; }`, not two `return 0;` early exits — the original funnels both guards into ONE shared failure epilogue (`je 0x206` twice) and materializes the result through a stack byte, where the early-return form makes cl duplicate the whole epilogue at each guard (DIFF 402 → 309, total 409041 → 275216). Same VC5 cross-jump/tail-merge family as the twin `BuildToolButton::InitMenuIconsMaybe`'s residual #1, resolved in the OPPOSITE direction: there the original duplicates and we shared; here the original shares and we duplicated. |
| 0x40c3d0 | `NameAnchorMaybe::TryBeginCouplingWaitMaybe` | 133 B | compiled **133 B (byte-exact length)**, **insns 53/53**, total **10002**, align 10, reg_pen **0**, identity_miss **0**, byte_diff **2** | ONE instruction, and it costs nothing in length: at the early-return epilogue the original spells the zero `mov al,bl` — reusing the result variable, which cl already keeps in BL and already uses two instructions earlier for the `wUnk0x36 = 0` store (`mov [edi+0x36],bx`) — where this compile emits `xor al,al`. Both encodings are 2 bytes. **Measured and INERT — do NOT re-run:** spelling the early return `return bWaiting;` instead of `return 0;` scores an IDENTICAL 10002; cl constant-propagates the variable (provably still 0 on that path) and folds it back to `xor al,al` either way, so there is no source-level way to ask for the register read. |
| 0x40cc90 | `NameAnchorMaybe::CheckAnchorCrossedTileBoundMaybe` | 196 B | compiled 196 B, **insns 63/56** (see caveat), total **53902**, align 52, reg_pen 17, identity_miss 17, byte_diff 32 | ⚠ **A REGRESSION, and its source was NOT touched — it was EXACT through v453.** Bisected to the arrival of its own near-twin `ExtendAcrossTileBoundaryMaybe` (0x40cb10) in this TU (Yoda #23 TU-context sensitivity). The two run the same four kind-7..0xa footprint-bound comparisons and only one gets the register assignment it wants: with 0x40cb10 present, this function's whole bound computation swaps EAX↔EDX against the original, and all 13 real diff rows are `r`. ⚠ The `63/56` headline is NOT a 7-instruction content gap — everything from offset 0xb4 on is this function's own jump table decoding as masked-relocation noise; the real content diff is zero extra instructions. **The trade was measured and is NET POSITIVE, so it stands: +260 B (0x40cb10 EXACT) against −196 B here = +64 B.** Two recovery probes measured, do NOT re-run: (1) moving 0x40cb10 to the END of the TU instead of its address-order slot is WORSE — 0x40cb10 itself drops to DIFF(111) and this one stays DIFF(13); (2) flipping the commutative operand order of the two-term bound sums (`pos + cols` → `cols + pos`) is INERT, cl canonicalizes the add. Adding 0x40c3d0 to the TU is separately confirmed NEUTRAL for both. Retry if 0x40cb10's own residual class ever cracks. ⚠ **The "+64 B" arithmetic above has been STALE twice, in both directions.** v456 knocked 0x40cb10 out too (so briefly BOTH twins were parked and the TU's gain came from 0x40c460 alone); v477 got 0x40cb10 back for good — its row is now deleted — so the +64 B trade is live again as originally measured. This function stayed at DIFF(13) through every one of those rotations, which is itself the finding: **0x40cc90 is the twin that does NOT respond to TU-state rotation**, so stop re-probing it on header work and retry only if the register-coin-flip class cracks outright. |

## v438 — src/DPlaySessionMgr.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x440a80 | `DPlaySessionMgr::LoadOutboxCardsIntoTrain` | 476 B | compiled **473 B**, **insns 154/155**, total **19016**, align 18, reg_pen 9, identity_miss 9, byte_diff 26 | Exactly ONE instruction short, and every other difference is its knock-on. The original RELOADS `pCar->pKindDesc` for the second half of the car's `(pKindDesc == 0 ? -1 : pKindDesc->resourceId) == 0x1870 \|\| … == 0x1871` test (`mov esi,[esi+0x40]` at 0x440ae2); cl 11.00 common-subexpression-eliminates it and keeps the first load alive in eax, which then forces the first ternary to compute into ecx rather than destroying eax — and both ternaries' registers rotate from there. Nothing stores between the two reads, so there is no source-level aliasing lever to break the CSE. **Two shapes measured — do NOT re-run:** this `\|\|` form (19016) and splitting it into `if (…) { flag = 1; } else if (…) { flag = 1; }`, which is WORSE (467 B, total 33k — cl sinks the whole second test past the flag store). Two levers WERE found and are baked in, do not undo: the ternary must be spelled NULL-first (`pKindDesc == 0 ? -1 : …`, so the `-1` is the fall-through — the `!= 0 ? … : -1` polarity inverts the branch and costs ~95k), and the `nEmptyCars--` must be an explicit FIRST statement of the loop tail rather than a `for`-increment (the `for` form sinks it below the `pFile->pNext` read). |

## v439 — src/ApplSetupWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x408f70 | `ApplSetupWnd::RefreshClientClipRect` | 771 B | compiled **657 B**, **insns 179/207**, total **427399**, align 414, reg_pen 118, identity_miss 117, byte_diff 429 | The page's whole layout pass — nine rects. Call parity is 3/3 and every rect, constant and store is present and arithmetically identical to the original's; the 28-instruction gap is entirely cl RE-USING values the original RELOADS. The original re-reads each descriptor's `nativeWidth`/`nativeHeight` and each just-written rect field out of memory (12 `xor r,r; mov r16,[…]` zero-extend pairs against this compile's 10) and materializes each button's four values into stack slots `[esp+0x10..0x1c]` before a 4-load/4-store copy into the ResourceRef. **Three shapes measured against `--len 771` — do NOT re-run:** page-relative constants + direct field writes (444752, 187/207, 707 B), chained constants + one shared scratch `RECT` per button (481503, 180/207, 657 B), chained constants + direct field writes — this one (427399, 179/207, 657 B). ⚠ The scratch-RECT shape is the one the original's stack traffic literally *shows*, and it is the WORST of the three: cl folds the copy away again and then has one more live value to schedule, so those stack slots are cl's own spill decision, NOT a source-level local. **One lever IS baked in, do not undo:** every constant must be chained off the PREVIOUS rect (`grid.right = grid.left + 0x1ee`, `textArea.left = grid.left - 2`, `exit.top = list.bottom + 0x6b`, …), not re-derived as the page-relative absolute it works out to — the original's own `lea`/`add` operands are the chained values, and flattening them costs 17k. |
| 0x4094b0 | `ApplSetupWnd::DrawProviderList` | 690 B | compiled **695 B**, **insns 215/216**, total **99168**, align 96, reg_pen 27, identity_miss 37, byte_diff 98 | One register-allocation coin-flip with a pure length knock-on. The original parks `this` in edi and keeps the CSE'd `&rectListMaybe` in ebx across the whole rect-adjust block, so its four field loads encode as 2-3-byte `[ebx+N]`; this compile parks `this` in esi, spills the pointer after one use, and re-materializes the other three as 6-byte `[esi+0x1e4]`-style absolutes — which is the entire 5-byte overrun. **Three shapes measured, all identical at 279 — do NOT re-run:** field order left/top/right/bottom vs the emitted store order right/left/top/bottom (cl canonicalizes both), and `rectRow.bottom > rectListMaybe.bottom` vs the operands reversed. **One lever IS baked in, do not undo:** the per-row `SetTextColor` must be written out in BOTH arms of the `nRows == nUnk0xf4` test rather than as a ternary on the colour (worth 32 points and 3 bytes) — the original pushes a different constant inside each arm at 0x409675/0x40967c over one shared `push esi; call` tail. |
| 0x409980 | `ApplSetupWnd::DrawAvatarGrid` | 725 B | compiled **735 B**, **insns 220/219**, total **292184**, align 284, reg_pen 71, identity_miss 80, byte_diff 284 | Induction-variable allocation across the nested row/column loops: the original strength-reduces the slot index into one register and derives the 0x4c `aProviderSlots` byte stride with an `lea r,[i+i*8]` / `lea r,[i+r*2]` / `shl r,2` chain (0x409abd), where this compile keeps the index and the byte offset in two separate stack slots and re-derives each. Every call, constant and store is present and in the original's own order. **Four shapes measured — do NOT re-run:** `providerId == 0` first vs `!= 0` first (431 vs 435; cl canonicalizes the arm order either way), indexed `aPlayerAvatars[nRow * 3 + nCol]` vs an explicit two-level pointer walk (431 vs 432), and the opening rect-build order — the original's own interleaving (cell left/top/bottom, whole-rect copy, cell right) at 431 vs the tidy rect-copy-first form at **594**, which is v438's "read the store order" lesson paying out again as a 163-point penalty for a plausible-looking reordering. |

## v440 — src/ApplSetupWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x40ae20 | `ApplSetupWnd::OnKeyDown` | 640 B (COMDAT, incl. the 20-byte switch tables) | compiled **628 B**, **insns 212/213**, total **88365**, align 88, reg_pen 3, identity_miss 3, byte_diff 35 | Exactly ONE instruction differs in the whole 213-instruction function. Where the `VK_UP` and `VK_DOWN` arms merge into the shared upper-bound test, the original materializes the row count into a register first (`mov eax,[esi+0x104]; cmp edi,eax` at 0x40afac/0x40afb2) while cl folds it into the compare (`cmp edi,[esi+0x104]`). BOTH arms already hold the count in eax on their own incoming edge — `VK_DOWN` from its own `nIndex >= nListRowCountMaybe` test at 0x40af85, `VK_UP` from its reset at 0x40af7d — so this is purely which edge cl chose to rematerialize on at a two-predecessor merge, a CSE-availability tie-break with no source-level lever. **Six shapes measured and FOUR are byte-identical — do NOT re-run:** combined `\|\|` post-switch guard (this one), two separate `if`s, a `VK_UP`-local `< 0` guard plus a shared upper bound, and per-arm `SetState(6)` in the `VK_RETURN` tail. **Two shapes are strictly worse and pinned the rest of the function, do not undo them:** `nListRowCountMaybe <= nIndex` operand order costs 10k (98367), and `VK_UP` before `VK_DOWN` in source costs 72k (160512) — the latter is what proves the original's source order is DOWN-then-UP, even though the jump table lists the cases by ascending key code. **Three levers ARE baked in:** the select-and-repaint tail must be shared (writing it out per arm costs 200 B — cl does not cross-jump two blocks that long); the `bUseSecondaryRememberedChoice` test in that tail must be spelled `== 0` so the PRIMARY arm is the fall-through (the inverted polarity costs 70k); and the `DrawProviderList` call must be written out in BOTH arms rather than as a ternary on the list pointer (the original pushes a different register in each arm over one shared `mov ecx,esi; call` tail — worth 7k and the whole `reg_pen`). |

## v440 — src/ThumbnailBmp.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x447b60 | `ThumbnailBmp::~ThumbnailBmp` (`??_G`) | 36 B | compiled **30 B**, DIFF(19) — a thunk, not the body | The original INLINES the dtor's two statements into the scalar-deleting thunk (`mov [esi],vtbl; call CloseStreams`) where ours CALLS `??1ThumbnailBmp` — the shape an **in-class** dtor definition produces, exactly like the `??_GNetSettings` row above. **Measured and it DOES close: moving `~ThumbnailBmp() { ThumbnailBmp_CloseStreams(); }` into the class body in `src/ThumbnailBmp.h` makes 0x447b60 EXACT at 36 B and `src/ThumbnailBmp.cpp` 8/8.** Parked because the in-class form makes every consumer TU emit its own ThumbnailBmp dtor COMDAT, and that reshuffle costs `src/DPlaySessionMgr.cpp` its `ApplSetupWnd::SendSelectRequestMaybe` (0x40ac50) match — 345 B EXACT → DIFF(4) at 324 B. **Net +69 B in-class vs +389 B out-of-line, so the out-of-line form stays.** Note the two forms also disagree about who OWNS 0x447b90: out-of-line, this TU emits the standalone `??1` (11 B, EXACT); in-class, 0x447b90 belongs to whichever TU owns `SavedFileEntryMaybe::~SavedFileEntryMaybe` (0x429820). Retry once DPlaySessionMgr.cpp is split. |

| 0x447e30 | `ThumbnailBmp::ThumbnailBmp_Save` | 288 B | compiled **446 B**, **insns 132/76**, total **388539**, align 388, reg_pen 3, byte_diff 209 | The whole 56-instruction gap is ONE construct: the `LocoBitmap capture` local's destructor. cl INLINES it at both exits (the `cmp al,1` / free-palette / free-pixels / virtual-release blocks, twice) because `src/LocoBitmap.h` defines `~LocoBitmap()` IN-CLASS; the original emits a plain out-of-line `lea ecx,[esp+8]; call LocoBitmap::~LocoBitmap` at each exit (0x447f01, 0x447f26). Everything else lines up instruction-for-instruction: the opening `ThumbnailBmp_CloseStreams()`, the `new ofstream(path, ios::out\|ios::binary\|ios::trunc, filebuf::openprot)` (mode 0x92, prot 0x1a4), the `rdstate() & badbit` guard, `g_worldBoard.CaptureBoardToBitmap(&capture, 0)` and both `write` calls. **Not probed further on purpose:** the only known lever is moving LocoBitmap's dtor out of line, and that header is included by ~20 TUs whose matches depend on the inline form — a repo-wide before/after experiment for a dedicated session, not a local fix. Retry alongside that experiment. |

## v437 — src/NetSetupWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x440d00 | `NetSettings::LoadOrInitFromDisk` | 406 B | compiled **406 B (byte-exact length)**, **insns 131/131**, total **450**, align **0**, reg_pen 4, identity_miss 4, byte_diff 10 | FOUR instructions, and all four are one fact twice over: the two copies of the `pDetectedProviderList = pSavedProviderList` restore tail read their local from `[esp+0x10]` where the original reads `[esp+0x14]` (and so land in eax/ecx where the original uses edx). The original's frame is `dwRead`@+0x10, `pSavedProviderList`@+0x14, `dwWritten`@+0x18; cl 11.00 puts the ONE non-address-taken (merely spilled) local BELOW both address-taken DWORDs. **Six shapes measured, all scoring an identical 450 — do NOT re-run:** saved-local declared first / between / last / after the buffers; buffers hoisted above the scalars; the saved local retyped `DPlayProviderNode *` → plain `DWORD` with casts; an extra unused pointer local inserted ahead of it. Declaration order, declaration form and local type are ALL inert — the address-taken/spilled split is the allocator's own. **One lever found and BAKED IN — do not undo:** the stamp test must be spelled `if (magicMaybe != 0x6a) { …defaults… } else { bValidSaveLoadedMaybe = 1; }` (inverted), which puts the one-store arm last, right before the shared restore tail, exactly as the original lays it out; the natural `== 0x6a` polarity duplicates the whole tail into that arm and scores DIFF(206). |
| 0x440cc0 | `NetSettings::~NetSettings` (`??_G`) | 59 B | **RESOLVED v449 — EXACT at 59 B.** | The image has NO standalone `??1NetSettings`: the body is FOLDED into the compiler-generated `??_GNetSettings` scalar deleting destructor, which is what an **in-class** definition produces (the shape `DSound::~DSound` uses in `src/DSound.h`). The body now lives in-class in `src/GameNetMsgQueue.h`; `src/NetSetupWnd.cpp` keeps a hint-only `??_G` marker because that is the TU the COMDAT lands in. The stated blocker — `#include "GNetManager.h"` costing `ApplSetupWnd::SelectGridCellFromPointMaybe` (0x40aba0) its 166 B — had ALREADY been spent by v448 for an unrelated reason (`LockableMaybe.h`), and a knife-edge cannot fall twice, so the trade came in as a clean **+59 B with zero collateral across all 20 consumers**. One extra step the old row did not predict: `src/GNetManager.h` used `GUID` without including `<windows.h>`, which only showed up once it was pulled in ahead of `<windows.h>` by `src/DPlaySessionMgr.cpp` — the header now includes `<windows.h>` itself, a no-op for every consumer that already had it. |

## v435 — src/LoadingScreen.cpp + src/NetSetupWnd.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x45de40 | `App_LoadWorldThreadProcMaybe` | 591 B | compiled **572 B**, **insns 167/175**, total 48020, align 48, reg_pen **0**, identity_miss **0**, byte_diff 20 | Every instruction pairs up; the entire 8-instruction gap is FOUR redundant `test al,al; jne <same target>` pairs the original keeps after each of its four `char`-returning bring-up calls and our cl 11.00 folds away. (The fifth check escapes it because its callee returns a real `bool`.) Documented fold-side optimizer-STRENGTH class. **Refuted probe, do NOT re-run:** writing the redundancy out literally as a nested `if (!bOk) { if (!bOk) { … } }` — cl folds THAT too, byte-for-byte identically (572 B either way). **Two levers found and BAKED IN — do not undo:** (1) the wait loop must go through two `inline unsigned char` screen-state predicates (`IsAppStateOneMaybe`/`IsNetShuttingDownMaybe`), else cl emits a plain `cmp/je` where the original has the `sete`-materialized branch; (2) the `"~curr"` checkpoint fallback must be written out in BOTH the screen-saver and the single-player arm — VC5 cross-jumps the two copies itself, and hoisting it to one shared post-`if` statement (which reads more natural) puts the screen-saver arm's own `g_bCleanExit` test in the wrong place. |
| 0x440c60 | `NetSettings::NetSettings` | 85 B | compiled **85 B (byte-exact length)**, **insns 25/25**, total 28144, align 28, reg_pen 1, identity_miss 1, byte_diff 34 | Every field store present and correct; the residual is which register each of the three shared constants lands in. The original loads all three up front (2→ecx, 0→eax, 4→edx); ours takes 2→eax and re-derives 0 later, and the displacement bytes follow from it. **Two source orderings measured:** the original's own EMITTED store order (total 36374) and plain member-DECLARATION order (28144, committed). Neither closes it. Same intrinsic constant-register-assignment class as the other ctor tie-breaks. |
| 0x440ea0 | `NetSettings::SaveToDisk` | 116 B | compiled **114 B**, **insns 37/38**, total 6002, align 6, byte_diff **2** | ONE instruction: a dead `test eax,eax` the original emits on `WriteFile`'s return, between the `CloseHandle` argument push and the call, with no branch consuming it. Same fold-side class as 0x45de40's four byte tests — a normalization of a result nothing reads, which our cl drops. |

## v434 — src/GameNet.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x45eab0 | `GNetManager::DPlay_ProbeAvailableProviders` | 933 B | **total 13119**, align 12, reg_pen 7, identity_miss 38, **byte_diff 39**, **insns 286/286**, compiled **933 B (byte-exact length)** | Transcribed from scratch this session; every instruction pairs up and the ONLY `-`/`+` rows in the whole `--dump` are the position of one `push edi`. The entire residual is a single 2-way callee-saved register coin flip: the original allocates `this`→edi and the CSE'd `&pTempDPlayIface`→esi, this compile allocates them the other way round (`&pDirectPlay4`→ebp and the hoisted CreateFileA import thunk→ebx agree in both). **Two source levers found and BAKED IN — do not undo:** (1) the COM-port helper's `char szPortName[5] = "COMn"` must be an ARRAY INITIALIZER, not `strcpy(szPortName, "COMn")` — VC5's `/Oi` strcpy intrinsic emits the generic runtime `repne scasb` + `rep movsd` shape even for a constant literal, while an initializer copies the 5-byte template out of .data as one dword + one byte (933 → 906 B, insns 327 → 283); (2) that helper must funnel its result through an explicit `bPortExists` local assigned BEFORE the `CloseHandle` call, since the original keeps the value live across it (`mov byte ptr [esp+0x17],1` … `mov al,[esp+0x13]`) where a trailing `return true` compiles to a post-call `mov al,1` (DIFF 565 → 39). **Refuted probes, do NOT re-run:** consolidating the four `DPlayProviderNode *` locals into one reused `pNode`; hoisting `GUID guidSp` to function scope; swapping `ProbeComPort`'s own `bPortExists`/`hPort` declaration order (the documented swap-two-sibling-locals lever has nothing to grip — this function has no same-type sibling local pair); IPX-arm success-first branch polarity (WORSE, 86 — failure-first is correct and is baked in); spelling the four probe calls `g_pNetManager->ProbeComPort(...)` as the this-ignoring-thiscall family does (byte-identical, the dead global load is eliminated, so that spelling is unobservable here). Same intrinsic reg-coin-flip class as `NameAnchorMaybe::TryMatchSocketMaybe` (0x40b880) and `TilePlacedObj::SpawnSeqRecordEffectMaybe` (0x4588b0) — TU context is the only thing that has ever moved one of these. |

## v433 — src/TilePlacedObj.cpp (a REGRESSION, not a new transcription)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| ~~0x4588b0~~ | `TilePlacedObj::SpawnSeqRecordEffectMaybe` | 143 B | **RESOLVED v471 — EXACT again (third flip, back ON).** `#include <stdio.h>` reaching `src/DSoundChannel.h` for `RFIndex`'s `FILE *` handle rotated this TU's /Og state back onto the good side; source untouched, as at every previous flip. The same edit flipped this TU's 0x458310 OFF (−60 B), for +83 B net here. Row kept as the record of why no source lever exists. Historical residual: DIFF 79, compiled 141 B — callee-saved-register COUNT shift (original pushes four: ebp/esi/edi/ebx; this compile pushes three), so `this` and the record pointer land in different registers and the whole diff is a rename cascade over otherwise-identical instructions | ⚠ **Was EXACT at v432 and regressed this session; its source was NOT touched.** Cause isolated by bisection to adding ONE non-virtual member DECLARATION (`UiIconListItem::HandleTextEditKey`) to `src/MenuNode.h` — a class this TU never names and a function it never calls. Confirmed both ways: removing the declaration restores EXACT, and moving it to three different positions in the `UiIconListItem` body changes nothing (it is the declaration's EXISTENCE, not its placement). The declaration is faithful to the real class and is required by `src/WidgetPicker.cpp`'s new `OnKeyDownMaybe`, which is worth 999 EXACT bytes against this one's 143 — kept deliberately. This function has now flipped in BOTH directions purely from unrelated shared-header edits (it went exact for free at v411 when `src/WorldBoardMaybe.h` modelled its tile grid properly). **Do NOT re-autopsy it as a source-shape problem** — it is a /Og register-budget coin flip and TU context is the only thing that has ever moved it. |

## v432 — src/NameAnchorMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x40b880 | `NameAnchorMaybe::TryMatchSocketMaybe` | 836 B | **total 244937** (first transcription 616788), align 234, reg_pen 95, identity_miss 118, byte_diff 257, **insns 243/250**, compiled 804 B | Structurally complete; verified block-by-block against the raw disasm over the whole 0x40b880-0x40bbc3 span. **Two source-shape levers found and BAKED IN, do not undo:** (1) ⭐ the four `return pNeighbour;` failure exits SHARE one physical epilogue in the original (`jne 0x40bbaf`), and getting that requires the last socket block to be a NESTED `if (wSocketCountExt != 0) { if (point matches) { …; return 0; } }` FALLING OUT to a single trailing `return pNeighbour;` — a flat `if (…) return pNeighbour;` ladder makes VC5 emit an inline 6-instruction epilogue copy at each one (616788 → 361530, surplus insns 27 → 8); the two `m_type0x63a == 0xb` chain-end guards then have to be a single `if (A \|\| B)` for the same reason (361530 → 257497). The original still keeps TWO local duplicates of that epilogue (0x40bace, 0x40bb73 — both the fall-through of an inverted `je`) beside the shared one, the documented "goto-sharing is LOCAL/trace-driven" VC5 class, and this transcription now reproduces the same 3-copy count. (2) `nY` is declared before `nX`, and `nOriginY` before `nOriginX` (257497 → 244937) — declaration order is this compiler's tie-break for which value wins a callee-saved register, same lever as `WalkerActor::UpdateFacingFromStepDeltaMaybe`. **Refuted probes, do NOT re-run:** spelling `pKind->pSocketTable`/`pKind->wSocketCountExt` out at every use instead of via `pS`/`nExt` locals, to stop VC5 CSE-ing loads the original re-issues (270846); splitting `TileGridPos origin = f(…)` into a declaration plus an assignment to reproduce the original's redundant copy-back of the hidden-return temp (265395); swapping nOriginX/nOriginY back after the nX/nY swap (248609); swapping nTileX/nTileY (byte-neutral). **Residual is a register-budget cascade, not a control-flow gap:** the original pins `this` in esi and the neighbour in edi (this compile has them the other way round) and keeps `nOriginX` live in ebp for the whole body, paying by SPILLING nTileX and by re-loading `pSocketTable`/`wSocketCountExt` from memory at 5 sites this compile CSEs into registers — which is why ours is 7 instructions SHORTER than the original, not longer. Same class as this TU's other three parks. |

## v430 — src/TrackGraph.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x45ce40 | `TrackGraph::BuildAdjacencyAMaybe` | 869 B | **total 112983** (was 209359), align 112, reg_pen 8, insns 279/273, compiled 880 B | v431 spent the cross-TU session v430 asked for. **All five levers below are LANDED and load-bearing — do not undo them.** (1) `src/Obj0x477798Family.h`'s base ctor now CALLS `ReserveMaybe` instead of spelling the inlined from-empty alloc, and (2) `Obj0x477758::Add` moved IN-CLASS so callers inline it — both confirmed against 0x45ce60 / 0x45cecf, and the pair is **byte-neutral repo-wide** (EXACT 104503/437 before and after), so v430's "+1767" estimate was necessary-but-not-sufficient rather than wrong. (3) `TrackGraphEdge` lost its constructor: both `new TrackGraphEdge` sites run `Unk0x4 = 0` unconditionally off operator new's register, where ANY ctor forces VC5's mandatory `test eax,eax / je` null guard. (4) The two `if (nNodeCount != 0)` guards are `> 0` — the original branches `jbe` off a live zero register, not `je`. (5) **GetAt/RemoveAt go through a base REFERENCE**, because VC5 devirtualizes a call written on the object itself (`nodeTiles.GetAt(i)` → a direct `call ?GetAt@Obj0x477758Base`) while the original re-reads the vtable at all three sites; worth 149234 → 112983 alone. `Add` must NOT go through it (routing it there costs 277969, insns 260/273 — the body stops inlining). **THREE residual classes left, all compiler-side:** the `cmp eax,ebx` vs `test eax,eax` zero-reg tie-break inside the inlined `Add` (same class as 0x456150 / 0x462e90); VC5 declining to build the original's separate `[esp+0x1c]` countdown trip counter for the 4-iteration direction loop (it emits `cmp eax,4 / jl` where the original emits `dec eax / jne` beside the same `dir` spill); and one `and eax,0xff`. **Refuted probes, do NOT re-run:** widening `GetOppositePathMaybe`'s parameter to `int` — the original really does range-check a raw dword with no mask, but it makes VC5 abandon the inner loop's POINTER strength-reduction in BOTH twins and rotates ebx/ebp globally (169236 → 289823), so the byte parameter is worth its one masked instruction; `unsigned int dir` (identical 112983 — VC5 canonicalizes it); a base POINTER instead of the reference (identical 112983 — either spelling works; the reference is kept as the cleaner one). |
| 0x45d1c0 | `TrackGraph::BuildAdjacencyBMaybe` | 898 B | **total 189923** (was 249957), align 188, reg_pen 16, **insns 285/281**, compiled 908 B | Same five levers, same three residual classes, same refuted probes — see the row above. The twins differ only in which of the tile's two graph slots they walk. |
| 0x45d6c0 | `TrackGraph::PropagateDistanceMaybe` | 322 B | DIFF(294), insns **120/118** | EFFECTIVE-adjacent; two stacked known classes, nothing source-shaped left. (a) The `xor eax,eax / cmp / sete al / test al,al` prologue on the `g_nScreenState == 3` guard — the same sete-prologue class already autopsied on 0x456150 (v334) and 0x462e90 (v335). (b) The original keeps `nNode` as ONE index used both byte-scaled (`mov byte ptr [ecx+esi],1`) and dword-scaled (`lea eax,[edx+esi*4]`), where VC5 here strength-reduces it to a precomputed `nNode * 4` held in a stack slot. Neither is reachable from the source text. |
| 0x45dbc0 | `TrackGraph::SolveRouteMaybe` | 443 B | DIFF(373), insns 158/164, **reg_pen 2, identity_miss 2** | EFFECTIVE. Every instruction is present in the same register; the entire difference is BLOCK LAYOUT — the original leaves both early-exit epilogues inline at the top of the function, VC5 sinks them to the bottom and cross-jumps them together. Same VC5 cold-path-sinking class as 0x44a250 / 0x44ce10. **PROBED AND REFUTED (v430):** writing the body as the `if` branch with the unreachable case falling out the bottom, exactly as the original reads, compiles to **byte-identical** code — so the control-flow spelling is not a lever here. The source keeps the original's shape anyway, for faithfulness. |
| 0x45dd80 | `TrackGraph::GetStepDirectionMaybe` | 90 B | DIFF(35), insns **43/43**, compiled length **90 = the original's exactly** | EFFECTIVE. Identical structure and length; what is left is register-assignment coin-flips (the original threads the pair through edi/esi where VC5 picks esi/edx/ebx, and pops in the other order). **Levers already found and KEPT:** the "pair was swapped" flag must be a real if/else setting it in BOTH arms, not a one-armed `if` over a pre-initialised flag (that alone took it from DIFF(87) to DIFF(35) — the original materializes `mov dl,1` / `xor dl,dl` with a `jmp` merge); and the `& 3` on the return must be a compound `bPair &= 3; return bPair;` rather than an expression or a cast, which is what keeps it 8-bit (`and al,3`) and closed the last length gap. Do not re-run those. |
| 0x45dde0 | `TrackGraph::SetStepDirectionMaybe` | 89 B | DIFF(72), insns **38/40**, compiled 83 B | EFFECTIVE. Same swap-flag lever as its read twin (kept). One instruction-selection tie-break left: on the `bDir == 0xff` path the original tail-merges the store into the shared `or al,cl; mov [esi],al` — having proved `bPair | 0xff == 0xff` — where VC5 emits the shorter direct `mov [ecx],dl`, and it re-materializes the `nFrom != nTo` compare twice where VC5 CSEs it once. **Refuted probe:** reaching the pair byte through a cached `unsigned char *` instead of re-subscripting the array (the array form is right and is what is kept). |

## v427 — src/WorldActionCursor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x459da0 | `WorldActionCursor::TickAndTutorialCheckMaybe` | 1010 B | insns **307/307**, align 44, reg_pen 9, identity_miss 9, byte_diff 38, total **45028** (1009 B compiled) | Content-complete: the first **679 of 1010 bytes** — everything through stage 1 (hover icon), stage 2 (variant re-roll), stage 3 (drag), the menu-node dispatch loop, the mode-7 subframe re-seat and the whole mode-3 block — is byte-identical, and the instruction COUNT matches exactly. The entire residual is ONE register-allocation coin-flip confined to the mode-6 block, where three locals (`bCoupled`, `nReversed`, `wSelectedCarId`) compete for two callee-saved registers: esi/edi are already `pTrain`/`this` and ebp takes `nReversed` in BOTH builds, so exactly one of `bCoupled` and `wSelectedCarId` must live on the stack. The original gives ebx to `wSelectedCarId` (`mov bx,[esi+0x58]` once, then two 16-bit `cmp bx,[esi+0x24]`) and spills `bCoupled` to `[esp+0x13]` (one store + two reloads); this compile gives ebx to `bCoupled` (`sete bl` / `test bl,bl`) and spills `wSelectedCarId`. Our xor+store+reload trio exactly replaces the original's store+reload+reload, which is what keeps the counts equal and makes it a tie-break rather than a shape difference. **Refuted probes, one compile each — do NOT re-grind:** declaring `bCoupled` first vs. last (identical 45028 both ways; source order does not drive this allocator); `char` instead of `bool` for it (identical 45028); dropping the named local so all three tests re-read `pTrain->dwSoundStateMaybe` (strictly WORSE, 108718 — VC5 does NOT CSE a member read across the intervening `SetNodeState` calls, it reloads from `[esi+0x5c]`, so the original's `sete`-materialized byte in a stack slot is positive proof of a real named local rather than a compiler temp). **Levers that got it from 125528 to here, both kept and both real source facts:** `wSelectedCarId` must be `unsigned short` (not `short`) so it matches the two members it is compared against and VC5 can emit a 16-bit `cmp` instead of promoting both sides to int; and the third icon-state block is written negated (`if (!bCoupled) { …2… } else { …1… }`) because the original lays the not-coupled arm out first and branches `jne` to the coupled one. |

## v426 — src/AppWindow.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4068d0 | `AppWindow_LoadConfigDirectories` | 712 B | insns **242/242**, align **0**, reg_pen **0**, identity_miss **0**, byte_diff 21, total **21**, compiled length **712 = the original's exactly** | The closest non-EXACT function in the repo. Every instruction matches 1:1, in order, in the same registers; the entire residual is ONE stack slot that failed to overlay. The original parks the compiler temp for `new IniFile(...)`'s raw allocation in the dead `DWORD dwType` slot it passed to `RegQueryValueExA` ~200 bytes earlier (frame +0x14) — which is exactly why Ghidra reports the two as one variable, `IniFile *local_a3c`, also used as the `lpType` out-param. This compile gives the temp its own dword, so the frame is `sub esp,0xa38` instead of `0xa34` and all 21 differing bytes are the uniform +4 displacement shifts that follow (0x18→0x14, 0x2c→0x30, 0x40→0x44, 0x1c→0x20, …). Nothing else differs at all. **Refuted probes, do NOT re-grind:** `dwType` at function scope rather than block scope (identical 21); swapping the `dwType`/`dwSize` declaration order (identical 21 — VC5 picks the slot order itself regardless); moving `dwSize` into the query block beside `dwType` (strictly WORSE, 72). The declaration order of the four function-scope locals is already correct and IS load-bearing — do not disturb it. Levers that got it here, all kept: repeating the registry-path string literal instead of a named `static const char *const` (the named form compiles to a memory load); a `goto` to the shared self-heal label instead of a reused status variable; a short-lived named local per `Reg*` status (drives `cmp eax,ebx` rather than `test eax,eax`); hoisting the `strlen` above `RegCreateKeyExA` and incrementing it in place; `unsigned` for the install-prefix length (the `> 2` test is `jbe`, not `jle`); and modelling `sizeof(IniFile)` as its real 0x10c. |

## v423 — src/AppWindow.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| ~~0x408350~~ | `AppWindow_StartGame` | 641 B | **RESOLVED v428 — EXACT** | Was blocked purely by src/AppWindow.h's MEASURED DIAL. v428 spent the 1332-byte `MailWnd::RefreshClientClipRect` knife-edge for an unrelated reason (src/EditCardWnd.h had to grow to hold `EditCardWnd::OnRButtonDown`), after which the dial was free. `DrainQueuedMouseInput` was promoted to a real `void AppWindow::DrainQueuedMouseInput(char)` and all five call sites respelled `g_pApp->DrainQueuedMouseInput(1)` — exactly v423's recipe, and it produced exactly v423's predicted 641-byte EXACT. ⚠ The other half of v423's price did NOT recur: `src/WorldBoardMaybe.cpp`'s `FindNearestObjOfCategoryMaybe` (0x457ce0) was predicted to cost 951 B and did not move at all. A stale price is a hypothesis, not a measurement. |
| 0x42f8b0 | `MailWnd::RefreshClientClipRect` (src/MailWnd.cpp) | 1332 B | (unchanged from the v422 row below) | Two more source-shape probes REFUTED in v423, run with the dial deliberately flipped (a `DrainQueuedMouseInput` member added to src/AppWindow.h) so the target was DIFF(150)/1334 B, testing whether a different spelling of the `rectFlagMaybe` block could survive BOTH declaration counts. (a) Hoisting `pRect = &rectFlagMaybe;` and routing the `CopyRect`/`.right`/`OffsetRect` uses through it — WORSE, DIFF(155)/1335 B. (b) Writing the sum as `rectFlagMaybe.left + pDesc->nativeWidth` instead of `pDesc->nativeWidth + rectFlagMaybe.left` — BIT-IDENTICAL to no probe at all (DIFF(150)/1334 B); VC5 canonicalizes commutative integer operand order before register allocation, so operand order is NOT a lever here. Do NOT re-grind either. Correction to the v422 row: the field zero-extended into edx at that site is `nativeWidth` (`[ecx+0x14]`), not `nativeHeight`; `nativeHeight` (`[ecx+0x16]`) is the second, already-agreeing load six instructions later. The conclusion stands — the divergence is a whole-function register-allocation decision, not a local expression shape, so probes that only reshape this one block cannot reach it. | **v428 UPDATE — the dial has now been SPENT and this function is parked in the perturbed state (1334 B / DIFF(150), total 34134, insns 397/396).** Four MORE probes refuted, all run against the perturbed build: (c) moving the added declaration's position within src/EditCardWnd.h — identical DIFF(150); (d) making the added declaration NON-virtual — identical DIFF(150), so the dial counts DECLARATIONS, not vtable slots; (e) adding a SECOND declaration — byte-identical to +1, independently confirming v423's saturation finding from the other direction; (f) hoisting `pDesc = pDesc0xNNN;` above its `CopyRect` in both member-rect blocks — much WORSE (34134 → 88264). ⚠ ALSO: the trigger set is WIDER than previously documented — it is not just src/AppWindow.h and src/DPlaySessionMgr.h. **src/EditCardWnd.h does it too**, proved with a neutral `void ZZProbeUnrelated();` dummy. Assume EVERY header on src/MailWnd.cpp's include list is part of the dial. Cracking this register knife-edge is now worth a straight +1332 with nothing to give back.

## v422 — src/AppWindow.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x406ed0 | `AppWindow_CreateMainWindow` | 192 B | insns **64/64**, align 86, reg_pen 16, identity_miss 15, byte_diff 70, total **87820** (193 B compiled) | Content-complete — the instruction SEQUENCE is identical and the whole residual is ONE register choice. VC5 hoists three of `CreateWindowExA`'s four non-immediate arguments into registers before the pushes begin and defers the fourth to the register recycled by the hInstance push; the original defers nWidth and gives eax to the `dwExStyle` select, this compile defers the select and gives eax to nWidth. eax is what buys the original its 192nd byte: the select's mask step encodes as the 2-byte `and al, 0xf8` there and as the 3-byte `and ecx, -8` here, and the 15 identity_miss rows are that one choice cascading. Probed and bit-identical (do NOT re-grind): hoisting the select into its own `DWORD dwExStyle` statement before the call; writing it `== 1 ? WS_EX_TOPMOST : 0` instead of `!= 1 ? 0 : WS_EX_TOPMOST`. Not caused by the `g_screenSaver.bScreenSaverMode` member spelling either — src/ScreenSaver.h documents that a cross-TU member access is exactly what compiles to the absolute load the original shows. **v448 UPDATE — the member form IS now available and IS what is checked in.** It is spelled `char AppWindow::CreateMainWindow()` and scores DIFF(60) / total 51473 (vs DIFF(72) as the free `__fastcall` hatch), so the numbers in this row's residual column are the OLD free-function ones. It still does not close — the residual is unchanged in kind (the CreateWindowExA argument-evaluation order and the eax-vs-ecx select). It earns its declaration slot for a second reason: it is the THIRD declaration on src/AppWindow.h, and that is the parity position at which src/WorldBoardMaybe.cpp's 0x457ce0 stays EXACT (see 0x42f8b0). |
| 0x42f8b0 | `MailWnd::RefreshClientClipRect` (src/MailWnd.cpp) | 1332 B | insns **397/396**, align 32, reg_pen 19, identity_miss 19, byte_diff 44, total **34134** (1334 B compiled) | NOT parked for its own source's sake — it byte-matches exactly as written, and this row records the DIAL that takes the match away so a future session does not blame MailWnd.cpp. Adding ANY declaration to src/AppWindow.h or src/DPlaySessionMgr.h — member function or free function, either header independently, at any position, under any name, and regardless of how many — pushes it to 1334 B / DIFF(150), always identically. Bisected one declaration at a time in v422 with everything else held fixed; a dummy `char AaaZzz();` does it as surely as a real one. The autopsy is one register: at the `rectFlagMaybe` block the original zero-extends `pDesc0x664->nativeHeight` straight into edx (`xor edx,edx / mov dx,[ecx+0x14]`) and keeps eax for its `lea eax,[esi+0x66c]`, while the perturbed build routes the value through eax and spends an extra `mov edx,eax`; the other 18 identity_miss rows are that cascading. Consequence, applied in v422: `CreateMainWindow` (0x406ed0) and `ConstructSingletonWindows` (0x406f90) are modeled as free `__fastcall` functions taking `AppWindow *` even though both genuinely ARE `__thiscall` members, and `GameNet_TeardownAllSessionState` (0x43f7b0) is declared in src/AppWindow.cpp rather than in src/DPlaySessionMgr.h where it belongs. Promote all three once this register knife-edge is understood. Also measured: one added `AppWindow` member moved src/WorldBoardMaybe.cpp's `FindNearestObjOfCategoryMaybe` (0x457ce0) off its 951-byte match and a second and third moved it back, so the member-count effect is NOT monotone. ⭐⭐ **v448 CONFIRMED AND ACTED ON: that non-monotone note was the key, and it is a PARITY CYCLE.** Three declarations were added to src/AppWindow.h (`LoadConfigDirectories`, `InitSubsystemsAndWindows`, `CreateMainWindow`). At +2 declarations 0x457ce0 falls (-951 B, residual `insns 327/327, align=8, reg_pen=13` — a pure zero-register ecx-vs-eax coin flip); at +3 it comes back with ZERO collateral across all 19 other consumers, and the net is **+723 B / +1 func** because 0x406ba0 goes EXACT. So two of the three functions this row says to "promote once the knife-edge is understood" are now promoted WITHOUT understanding it — the parity was enough. ⚠ Only `ConstructSingletonWindows` (0x406f90) is left, and it should NOT be promoted casually: it already matches free at 2052 B so it can only lose, and being a FOURTH declaration it would move the parity and likely take 0x457ce0 with it. 0x42f8b0 itself is unaffected — it was already spent in v428 and did not move again here. See docs/CODEGEN.md's v422 bullet and src/MailWnd.h's MEASURED DIAL note. |

## v421 — src/AppWindow.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x407d20 | `AppWindow_ApplyDisplayModeMaybe` | 1034 B code / 1040 B COMDAT | insns **306/305**, align 220, reg_pen 88, identity_miss 87, byte_diff 263, total **229933** | Structurally complete, down from 303490 on the first transcription. ONE extra instruction: a second `xor` in the prologue. The original materializes exactly one zero register (ebx = `hWndInsertAfter`), reuses it for the dword `dwScrollX/dwScrollY = 0` stores and the NULL/FALSE pushes, and still spells every BYTE-sized zero as an immediate; this compile has one register more to spare (the original also pins GetWindowLongA's import slot in edi for all three call sites, where ours caches it for only the last two), so VC5 builds a SECOND dedicated zero and folds it into the byte ops — one instruction cheaper per site, one `xor` dearer overall, and the resulting register renaming plus the 2-byte length delta is the entire `align=220`. Four source shapes WERE load-bearing and are applied: `HWND hWndInsertAfter = NULL;` at function scope (311861 if declared in the arm that uses it); the frame-adjust arithmetic split into two `+=` statements rather than one expression (worth 14k); the second `SetWindowPos` spelling its anchor as `g_rectAppWindowBounds.left/.top` rather than the just-assigned `rect.left/.top`; and the screen-saver test written `!= 1` with the `SWP_NOZORDER` arm FIRST, which is the layout order the original's `je` implies. Probed and bit-identical (do NOT re-grind): hoisting `dwStyle` to function scope seeded by the discarded GetWindowLongA; `!bFullscreen` vs `== 0`; `rect` declared after `hWndInsertAfter`; hoisting `uFlags`; passing `hWndInsertAfter` instead of `NULL` at the windowed SetWindowPos; `0` instead of `NULL`. Probed and WORSE: `BOOL bClamped` (241942), an `int` clamp tally (247940). See docs/CODEGEN.md's v421 bullets. |

## v420 — src/WorldBoardMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x455d60 | `WorldBoardClickView0x455d60::ResolveWorldClickMaybe` | 949 B code / 988 B COMDAT | insns **348/343**, align 336, reg_pen 17, identity_miss 17, byte_diff 158, total **338028** | Structurally complete and instruction-aligned, down from 1475673 on the first transcription; four source levers found and kept (switch not if/else; `case 4:` written before `case 3:`; a `break` rather than an early `return bConsumed` in the drag arm; the two selection calls spelled `a() == 0 && b() == 0` with the failure arm first). Two gaps left, both probed and refuted. (a) The original computes case 4's col/row BEFORE the switch dispatch, which only a FUNCTION-scope pair reproduces — and at function scope VC5 spills `col` across case 3's own recompute, growing the frame from `push ecx` to `sub esp,8` (416476, i.e. 78k WORSE). Reassigning a shared pair in case 3 instead of declaring a second one scores bit-identically, so there is no third spelling; the whole `align=336` is that one block's position cascading into every jump displacement. (b) The drag-threshold temporaries: the original allocates ONE register (ebp) for both `dwLastBulldozeXMaybe` and `dwLastBulldozeYMaybe` and keeps `x` in the volatile ecx, so it must reload both; this compile holds them in preserved registers and needs neither reload — the documented symmetric-register-swap class. See docs/CODEGEN.md #18j/#18k/#18l/#18m. |

## v415 — src/DecorActor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x434100 | `DecorActorBase::OnArriveAtDestinationMaybe` | 335 B | 265 (insns 103/104) | ONE root cause, the documented zero-register residency class — this time with the polarity REVERSED from 0x4349d0's: *our* compile allocates the dedicated zero (`xor ecx,ecx` in the prologue) and the ORIGINAL does not. The original spends an immediate or a fresh 8-bit zero at all nine of its zero uses (`mov al,[mem]`/`test al,al` for the three byte-flag tests, `mov dword ptr [esi+0xa4],0`, `mov byte ptr [esi+0x24],0` ×2, `xor al,al` for the early return); ours spends `cl`. That also burns ecx as a scratch, which is the whole reason both `ArePositionsEqualMaybe` sites sink their y-coordinate loads below the x compare (the original has four free registers and hoists all four loads above it) and the tail's two `nPrevDest` stores come out reversed. Two source shapes WERE load-bearing and are applied: `case 0: break;` (without a fourth label VC5 abandons the jump table for a `dec eax`/`je` chain — 252664 → 214739), and the re-decide line DUPLICATED into each case rather than sitting after the switch behind a `goto` (the single-copy `goto` form takes the jump table away again — 390122; VC5 cross-jumps the three copies back together on its own). Probed and inert: `!bReady`/`!bThrottleMaybe` instead of `== false`, and `unsigned char` instead of `bool` on ArePositionsEqualMaybe. |
| 0x433ec0 | `DecorActorBase::FindFootprintTileOfCategoryMaybe` | 353 B | 4 (insns 144/144) | Scheduling coin-flip, and the smallest residual class there is: align=0, byte_diff=4, every instruction/operand/branch/register identical. The original hoists `mov ebx,[esp+0x18]` (x) then `mov esi,[esp+0x1c]` (y) into the prologue; this compile emits the same two loads into the same two registers in the opposite order. Probed and inert: caching `pKindDesc` in a local for the third probe vs. reading the member twice. Both of the shapes that DID matter are already applied and documented in-source (the two-return `GetTileCategoryMaybe` wrapper, worth the `movzx cx,cl` vs `and ecx,0xff` widening; and its POSITIVE null test, worth a 12-instruction block-layout gap). |
| 0x433dc0 | `DecorActorBase::ComputeStepTargetMaybe` | 256 B | 42 (insns 96/96) | Register coin-flip, and about as close as a non-match gets: EVERY instruction sits at the SAME byte offset as the original's and every operand agrees except that the original parks `this` in edi and `nTargetX` in esi while ours does the reverse (nTargetY is ebp in both). The two scheduling rows in the stop path fall straight out of that — holding `this` in the register that dies at the end of the block lets our compile reload hotspotPosY into esi one instruction earlier, where the original still needs edi and borrows edx. Nothing in the source names a callee-saved register. The `IsNoTargetMaybe` inline predicate IS load-bearing (a bare `&&` in the `if` folds the bool materialization away, 61439 → 31285); probed and score-neutral: `unsigned char` instead of `bool` on it. Probed and much worse (DIFF 189): hoisting `pt.x = hotspotPosX` above the three zero stores. |

## v414 — src/DecorActor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4349d0 | `DecorObjMgrMaybe::SpawnActorForKindMaybe` | 395 B | 245 (insns 133/129) | ONE root cause, the documented zero-register residency class: align=58, reg_pen=0, identity_miss=0, every instruction/operand/branch/register otherwise identical. The original keeps `pActor`'s 0 resident in esi and SPENDS it — `cmp eax,esi` at the pCantHave and both `operator new` null checks (ours `test eax,eax`, same 2 bytes), `mov [esp+0x18],esi` for case 7's EH state (ours the 8-byte `mov …,0`), and no re-zeroing at all on the failed-`new` path (ours adds `jmp`/`xor esi,esi` twice = 4 instructions / 8 bytes). Probed and REFUTED: hoisting the `DecorActorBase *pActor = 0;` declaration to the top of the function (119748 — costs esi its pKind tenancy), and the implicit-bool null tests `if (pCantHave)` / `if (pActor)` (bit-for-bit inert, as docs/CODEGEN.md predicts). A separate typed local per arm (`WalkerActor *pWalker = new …; … pActor = pWalker;`) is much worse (96820 — spills pActor to the stack). Three shapes WERE load-bearing and are applied: the must-have guard as ONE `if` with `||` (both failure legs then cross-jump into the pCantHave `return 0`), `> 0` rather than `!= 0` on the cant-have live-count test (that is the `jbe`), and each arm's validity test written NEGATED with the delete first. Nothing left to turn short of the zero-register class itself cracking. |

## v413 — src/DecorActor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x435580 | `DecorObjMgrMaybe::ApplySeqRecordToActorsMaybe` | 299 B | 4 (insns 102/102) | Pure register-naming tie-break, the smallest residual class there is: align=0, byte_diff=4, every instruction/operand/branch identical. The original sets up the `PtInRect` call as `mov ecx,[esi+0xc]` / `lea edx,[esp+0x18]`; this compile swaps the two registers. Probed, both no-ops: `*(POINT *)&pActor->rect.left` instead of `&pActor->rect`, and hoisting the point into its own named `POINT` local. The one part of that call site that IS steerable is already applied — the original's `test al,al` needs the result in a byte-typed local (`unsigned char bInRect`), not a bare `if (PtInRect(...))`, which is worth 1 byte. Nothing left to turn. |

## v410 — src/TilePlacedObj.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x458310 | `TilePlacedObj::GetFrontRowTilePosMaybe` | 60 B | **v471: flipped OFF again (fifth flip) — DIFF(13) at 60 B** | The repo's most declaration-set-sensitive match. Flipped OFF in v446 by `PlacedObjRegistryMaybe`'s slot-15 parameter list; back ON for free in v448 when `InitTrainCouplingMenuIconsMaybe` + `GetOrCreateIconItemMaybe` were promoted onto `src/WorldActionCursor.h`; **OFF again in v471** when `#include <stdio.h>` reached `src/DSoundChannel.h` for `RFIndex`'s `FILE *` handle. No source change to this function at any flip; the bare `sub edx,eax` vs `sub eax,edx` coin flip simply lands on one side or the other. The same v471 edit flipped this TU's 0x4588b0 ON (+143 B), so `src/TilePlacedObj.cpp` came out **+83 B net** and the repo **+754 B / +4 funcs**. ⚠ Expect it to flip again: treat 60 B as the standing price/prize of ANY declaration work reaching src/TilePlacedObj.cpp, and do not spend probes on the function itself.
| 0x458350 | `TilePlacedObj::GetEntryExitPointMaybe` | 102 B | 5 (insns 40/40) | Pure constant-materialization width. Registers and instructions agree everywhere; the original writes `mov ebx,1` / `xor ebx,ebx` (32-bit) then `test bl,bl` (8-bit), ours the matching-width `mov bl,1` / `xor bl,bl`. The byte test pins the local at 1 byte, so this is not a type error: `int` is far worse (50893, insns 38/40, whole-function re-allocation) and `char` / `unsigned char` / `bool` are byte-identical to each other. |
| 0x458430 | `TilePlacedObj::SpawnOwnedActorMaybe` | 968 B | 495 (686 B, insns 222/312) | Content-complete. Dominant gap is a MISSING INLINE, not a source shape: the original inlines `GetFootprintRectMaybe` (dead `pRect != 0` test on a stack address at 0x458486) and VC5's `/O2` implies `/Ob1`, so ours emits a `call`. Marking the definition `inline` inlines it (704 → 778 B) but suppresses the out-of-line COMDAT and costs 0x4583c0's own 99-byte EXACT. `/O2 /Ob2` gives both locally but is refuted repo-wide (87077 B / 382 funcs vs /O2's 89321 / 395). Likely reading: the original declared it `inline` and 0x4583c0's COMDAT came from another TU — chasing that would move the marker off this file. **v411** fixed a real semantic error worth 71 B (the `left > right - nativeWidth` arm's divisor is `left - (right - nativeWidth) + 1` with base `right - nativeWidth`, not `-nRangeX`; the X pick is a symmetric uniform-point-in-[min,max]) and REFUTED two shape probes: spelling the tail-duplicated X block twice makes it WORSE (532, 727 B — cl cross-jumps the copies back together, unlike 0x458820's guard, because these copies converge on a shared call rather than each ending in `return`), and writing the spans the way the post-`rand()` blocks recompute them is worse again (548, 746 B — without named locals cl merges all four `call rand` sites into one where the original keeps five). The original's memory-heavy shape is register-pressure driven. |


> # ⛔ v358 — EVERY NUMBER IN THIS FILE PREDATING v358 WAS MEASURED WITH THE WRONG COMPILER.
>
> `Loco.exe` was built with **VC++ 5.0 Service Pack 3**, not RTM, and `toolchain/bin/cl` now
> defaults to `toolchain/vc50sp3`. (SP3's `link.exe` stamps 5.10.7303 = the PE header's
> `linker 5.10`; RTM's `c2` emits the wrong 3-byte code-alignment filler as a whole-image
> constant. Full write-up: `toolchain/README.md`, summary in `docs/CODEGEN.md`.)
>
> **Do not trust a pre-v358 row's diff count, and trust its "not source-steerable" verdict even
> less.** The repo-wide switch gained 9 byte-matches with zero regressions and collapsed
> residuals that had been parked for dozens of sessions: 0x458c90 986 → 5 raw diffs, 0x447400
> 863 → 12, 0x4216f0 823 → 48, 0x459180 819 → 70. **Re-score before retrying, and re-score
> before quoting.** Two v358 retries of rows that explicitly recorded "tried, zero effect"
> succeeded on the spot (0x425b70 and 0x458c90, both now EXACT and deleted from this file) —
> a toolchain-constant residual is indistinguishable from an allocator coin-flip from the
> inside, so those verdicts were honest but unfounded.
>
> Cheapest way to re-triage: `tools/progress.py`, then `tools/cc.sh <TU> -v` on anything with a
> small DIFF. As of v359 the near-miss tail is unusually rich — 0x41a360 is **1 byte** from
> exact and 0x440390 is 2. (0x416e00 and 0x42aa90 were in this list and are now EXACT.)

> **⭐ v356 — the `sete`-materialization class is SOLVED; several rows below are stale.**
> Any row here that blames "the 0x4393d0 sete-materialization class" or says "retry once the
> sete class cracks" should be RE-SCORED before being trusted. The original's
> `xor r,r; cmp g,K; sete rl; test rl,rl; jcc` comes from a **byte-returning inline predicate**
> (`inline unsigned char IsInGameModeMaybe() { return DAT_004851f4 == 3; }`), not from an
> inline `g == K` test — see docs/CODEGEN.md. Already re-scored and updated in v356:
> `GameNetMsgQueue::EnqueueOrFreeNode` 0x4393d0 (total 12113, **byte_diff 3**, insns 86/86 —
> was a whole-function cascade reading as DIFF(192); **now EXACT as of v359 — its last 3 bytes
> were a TWO-cursor tail walk that had to become a ONE-cursor walk, see docs/CODEGEN.md**), `Main::LocoWinMain` 0x462e90
> (183550 -> **104284**), `WorldBoardMaybe::UpdateDirtyTiles` 0x456150 (509762 -> **424323**),
> `WorldActionCursor::SelectDecorObjAndDispatchModeMaybe` 0x459180 (513668 -> **479365**).
> **v357 closed the audit**: the 74-site inventory was re-derived and every `sete` site inside an
> already-transcribed `src/` function is now handled (see `docs/CODEGEN.md`). Two more functions
> were written WITH the predicate from the start -- `WorldBoardPartial::MarkRectDirty` (0x455840)
> went byte-EXACT on the first compile, and `DPlaySessionMgr::GameNetMsgQueue_
> EnqueueOrProcessLocalNode` (0x43f140, the sibling the 0x4393d0 autopsy always named) landed
> instruction-for-instruction aligned. Rows below that still blame the class do so only as ONE of
> several stacked residuals in functions whose sete site is already routed through a predicate;
> their headline numbers are current.
> ⚠ The lever is local but its effect is global: at `WorldBoardMaybe::FUN_00456700`'s own
> sete site (0x45689a) the predicate reproduces the gate yet nets the function WORSE
> (478854 -> 488419), so it is deliberately left inline there. Always score the whole function.



- `Main::AppWndProc` (0x4618c0, v368, first transcription -- the app's window procedure and the
  largest function in the app region). `asmscore.py --len 0x15d0` (5584 = the span to
  LocoWinMain; the five jump tables at 0x462df8..0x462e83 are part of the COMDAT and Ghidra's
  `Body` span stops before them, so a `Body`-derived length under-reports the window here):
  total 1975015, align 1928, reg_pen 412, identity_miss 412, byte_diff 1695, **insns 1739/1740**
  -- one instruction OVER, so there is no missing body. Structure verified block-by-block
  against the raw disasm. Two intrinsic residual classes, full autopsy in the src/Main.cpp
  body comment: (1) REGISTER RESIDENCY -- the original dedicates ebx to `g_pApp` and re-reads
  `hWnd` from `[esp+0x36c]` at all 26 `DefWindowProcA` sites, ours does the reverse; this alone
  is ~74 of the 112 original-only dump rows. (2) CROSS-JUMPING -- the original tail-merges five
  more identical `...; return 0;` tails (most visibly the `PostMessageA(hwndOwner, WM_CLOSE)`
  block shared between WM_CHAR's `'Q'` and WM_KEYDOWN's VK_ESCAPE), worth ~65 of our 111 extra
  rows. Landed levers (kept): four new byte-predicates for app states 0/1/4/10 (the v356 class,
  which is what the doubled `(state==0||1||2)` test is made of), and the WM_USER+1 jump-table
  switch reordered to the original's body order, case **8 first** (2135102 -> 1975015). Refuted
  (do not retry): reversing the entry guard to `g_pApp->hwndOwner != hWnd` so g_pApp is
  evaluated first (1-byte regression, no allocation change); swapping WM_CHAR before WM_KEYDOWN
  (exactly byte-neutral -- the outer switch's compare tree, not source order, fixes body order
  for a tree-lowered switch; only the jump-TABLE switches follow source order).

- `ScreenSaver::FilterMessage` (0x4484a0, v366, first transcription; `asmscore.py --len 0x350`
  total 1830285, align=1828 reg_pen=11 identity_miss=11 byte_diff=1075, insns 332/274 -- cc.sh
  DIFF(841), len 1062 vs 848). ⚠ **Every one of those numbers overstates the residual: our CODE
  is 792 bytes to the original's 814, i.e. 22 bytes SHORTER, and the four inlined dismiss blocks
  line up instruction-for-instruction.** The entire 214-byte gap is ONE switch jump-table shape
  decision -- the new "switch CLUSTER ABSORPTION" class, see `docs/CODEGEN.md`: the original trees
  {0x1c},{0x20},{0x100},{0x104-0x106},{0x112} and tables only the dense 0x200..0x207 cluster
  (8 dword entries), ours absorbs WM_SYSCOMMAND (0x112) into that cluster and tables
  0x112..0x207 as a 24-byte dword table plus 246 bytes of byte indices, moving the top-level tree
  pivot from 0x1c to 0x20 with it. `insns 332/274` is mostly the aligner decoding those table
  bytes as `add eax,0x5050505`, not real code. Probed without effect / worse (do not retry):
  WM_ACTIVATEAPP guard polarity (no-op, VC5 normalizes it); explicit `default: return 0;` inside
  the switch vs a trailing one after it (no-op); `case WM_SYSCOMMAND` moved last (841 -> 845);
  four extra no-op `return 0` cases densifying the 0x202..0x206 gap (841 -> 846, len 1070).
  Case-group source order is NOT the lever -- the case-body EMISSION order already matches the
  original exactly. Next axis to try: the SHAPE of the absorbed 0x112 arm itself (e.g. hoist its
  inner `wParam & 0xfff0` switch into a non-inline helper so the arm becomes a bare call, which
  may drop it out of the cluster).

- `ScreenSaver::EnterDemoSession` (0x448350, v366, first transcription; `asmscore.py --len 0x40`
  total 226, align=0 reg_pen=2 identity_miss=2 byte_diff=6, **insns 13/13**). EFFECTIVE. The ONLY
  disagreement is which register holds the SECOND `g_pNetSettings` load: the original re-uses eax
  (where its own first load went), we take ecx -- which the very next instruction then overwrites
  with `g_pDPlaySessionMgr`, so ours is strictly the less consistent choice. Instruction
  sequence, scheduling and the tail `jmp DSound_GetOrCreateManager` are all identical. Pure
  allocator coin-flip; retry only if that class ever cracks.

- `DPlaySessionMgr::GameNetMsgQueue_EnqueueOrProcessLocalNode` (0x43f140, v357, first
  transcription; `asmscore.py --len 358` total 153871, align=150 reg_pen=32 identity_miss=56
  byte_diff=111, insns 136/129). Body is instruction-for-instruction aligned with the original.
  The ENTIRE residual is one ebx<->ebp symmetric-register-swap (Yoda #29/#30): the original holds
  the zero constant in ebx and pNode in ebp, this compile does the reverse -- and because ebx is
  the first-pushed callee-saved register, the original SHRINK-WRAPS (pushing only ebx/esi before
  the shutdown-vs-enqueue branch, edi/ebp inside the shutdown arm), so its two enqueue epilogues
  pop 2 registers where ours pop 4. That accounts for the 4-byte length excess as well. Probed
  without effect: C-style declarations hoisted to the top, an explicit if/else instead of the
  early return, and moving the definition from the end of the TU to its address-order slot (that
  last one also costs an unrelated EXACT elsewhere in DPlaySessionMgr.cpp -- do not repeat it).
  ⭐ Three source levers DID land and are documented in `docs/CODEGEN.md`: case-body source
  ORDER (345831 -> 177833), re-reading `pNode->pPayload` inside the case bodies to reproduce the
  original's redundant null guards, and advancing the list cursor at the TOP of the walk loop.
  Retry only if the symmetric-register-swap class ever cracks.

- `UIResources::TileKind_GetOrLoadDescriptor` (0x446ea0, v357, first transcription;
  `asmscore.py --len 520` total 156287, reg_pen=2 identity_miss=2 byte_diff=67, insns 162/154).
  ONE instruction from exact: the original materializes the range helper's `nLastId` parameter
  into EDX before the clamp (`mov edx,eax ; cmp edx,0x4000 ; mov [esp+0x18],edx`) where VC5 here
  coalesces it with `nFirstId` and keeps everything in EAX (`cmp eax,0x4000 ; mov [esp+0x18],eax`).
  That is 3 bytes shorter, so every downstream jump displacement shifts by 3 -- which is the whole
  of the raw DIFF(359); the first real disagreement is at +0x4c and there is not a second one.
  Probed without effect: an if/else clamp (strictly worse, 176312); hoisting the loop into the
  inline `TileKind_LoadDescriptorRange` helper vs. writing it flat compiles BYTE-IDENTICALLY.
  Note its twin `TileKind_GetOrLoadDescriptorNoAlias` (0x4470b0) is byte-EXACT through that same
  helper, so the helper and the 264-byte buffer are both proven correct -- this is purely a
  register-residency coin flip on the one clamp the twin does not have.

- `WidgetPickerObj0x477cc8::ReloadBackdropPreview` (0x428400, v308, first transcription,
  `asmscore.py --len 336` total 100933, align=100 reg_pen=8 identity_miss=8 byte_diff=53,
  insns 96/97) — `src/WidgetPicker.cpp`. 3rd and last of the `LocoBitmap::BlitOntoBitmap`
  caller family (the other two, `SplashWnd::BuildDrawTargetCompositeMaybe` 0x4216f0 and
  `EditCardWnd::BuildPreviewCanvasAMaybe` 0x416460, are both EXACT as of v375 and no longer
  listed here) — this one decompiles cleanly (already this-typed from an
  earlier session), so no raw-disasm-only transcription was needed. Every structural (`S`) row
  in `asmscore.py --dump` aligns; the whole residual is (1) a persistent "0" register the
  original materializes once in its prologue and reuses for every null-check/literal-0 call
  arg (this compile uses `test reg,reg` + literal `push 0` instead, no register dedicated to
  0), and (2) the familiar symmetric register-swap (esi/edi) on the derived `ThumbnailBmp*`
  through the thumbnail branch. Tried and confirmed NO EFFECT: computing the
  `ThumbnailBmp_IsLoaded()` call target inline vs. via a named `pThumb` local (byte-identical
  either way). Two real (non-tie-break) levers WERE needed to get here, both closed: the
  `nCategory != 5` branch must be the fall-through body (not `== 5`, Yoda #15/branch-order
  family — a ~213000-point miss when backwards), and `ThumbnailBmp::wWidth`/`wHeight`
  (declared `short`, per `DPlaySessionMgr::LayoutSet_LoadSlotBitmap`'s own already-EXACT
  read) need an explicit `(unsigned short)` cast at THIS call site to force the zero-extension
  the raw disasm shows, while `g_WorldBoard.wCols`/`wRows` need the opposite `(short)` cast
  (sign-extends here). No untried lever found within budget.

- `GNetManager::DPlay_SendMessage` (0x460d40, v294, first transcription, `asmscore.py --len 330`
  total 165257, align=162 reg_pen=28 byte_diff=157, insns 105/109) — `src/GameNet.cpp`. Down
  from an initial DIFF total 233400 via 2 real fixes: (1) `dwNumMsgs`/`nResult` locals declared
  at the very TOP of the function (before the `pDirectPlay4==0` early return) — raw disasm shows
  both stack slots written unconditionally in the true prologue; (2) the Send-vs-SendEx `if`
  tests `sessionDescUpdate.dwFlags != 0` (SendEx first/fall-through) not `== 0` — the original's
  physical block order is SendEx-inline, Send-at-jump-target, opposite of a naive reading.
  Residual: mostly the intrinsic register-role-swap class (Yoda #7/#29/#30 — `this`/
  `dpidLocalPlayer`/etc. picked different scratch registers throughout the two big vtable
  calls). Two unexplained stack-slot anomalies remain even after the prologue-hoist fix: the
  candidate still constant-folds `nResult`'s return to a literal `mov eax,1` (original loads it
  from the stack slot), and drops the defensive `dwNumMsgs=0` init entirely (providably dead —
  the only read is right after the address-taking call). Declaration-order swap between the two
  locals had zero effect (tried both orders, identical score). Retry idea if revisited: try
  forcing register pressure some other way (e.g. an extra dummy cross-call-live local) to see if
  it reproduces the stack residency, or accept as an intrinsic register-pressure tie-break from
  the two heavy 9-arg/6-arg vtable calls.

- `WidgetBaseObj0x4784c8::HitTestAndLocalizeSecondaryMaybe`/`HitTestAndLocalizeMaybe` (0x4549e0/
  0x454a60, v361, `asmscore.py --len 123` total 31589, align=30 reg_pen=13 byte_diff=39, insns
  55/56 -- was total 69937/byte_diff 67 until v361 made `ComputeLocalPos` return its POINT BY
  VALUE, retiring the transcribed `int local[2]` return temporary) — `src/WidgetBase.cpp`, first transcription of either function (both were previously
  named+this-typed but not in `src/`). Structural twins: own `Contains` (slot 2) gates a walk of
  `pMenuListHead` (skipping `pLastHitNode`), dispatching each node through THIS widget's own
  vtable slot 18/17 (+0x48/+0x44, part of the base's documented "17-20 abstract
  PureVirtualAbortMaybe placeholder" family) — at the time via a throwaway `WidgetBaseVtblProbe`
  (since retired 2026-07-21: slots 17/18 are now real `virtual HitTestNodeSecondary`/
  `HitTestNode` methods on `WidgetBaseObj0x4784c8`, called as ordinary `this->HitTestNodeSecondary
  (...)`/`this->HitTestNode(...)`) — passing the
  node pointer as an explicit arg (confirmed via raw disasm — NOT the node's own vtable). Down
  from an initial DIFF(128)/139B via 2 real levers: (1) branch-order (guard-clause `if
  (Contains()==0) return 0;` instead of wrapping the body in `if (Contains()){...}` — the
  original's Contains-false path is the fall-through, matching the guard-clause form); (2) NOT
  caching ComputeLocalPos's 2nd out-value (`ly`) into a named local — re-reading `local[1]` at
  its one use site matches the original's own asymmetric register/stack residency for the pair
  (`lx` stays in a register all loop, `ly` is spilled/reloaded each iteration in the ORIGINAL
  too — Yoda lesson #13). Residual: the original re-reads `this->vftableMaybe` fresh before EACH
  of its 2 virtual calls (Contains, then the loop's per-node dispatch); our compile CSEs that
  read into one shared register since `ComputeLocalPos` is defined earlier in the same TU and
  provably doesn't touch the vtable pointer, letting the optimizer prove both reads are the same
  address — this steals the register slot the original reserves for `bAny`, forcing it to a
  stack byte. Tried and rejected: `bool`/`int` `bAny` (no effect on the CSE), `register` hint (no
  effect, MSVC ignores it under /O2), separate `bHit` local vs. inlined OR-expression (minor
  improvement, doesn't touch the root cause). Retry idea if revisited: find a source shape that
  defeats the cross-call CSE of the vtable read without an unfaithful extra call layer — e.g.
  check whether declaring `ComputeLocalPos` only (not defining it) ABOVE these two functions in
  this TU changes the optimizer's provenance analysis (would need moving/duplicating its
  definition, untested this session).
  **v361 residual, newly understood:** the original reads BOTH components of the returned
  POINT through the returned pointer (`mov ebp,[eax]` / `mov edx,[eax+4]; mov [esp+0x14],edx`)
  and is ONE instruction longer than ours (56 vs 55); our `POINT pt = ComputeLocalPos(x,y);`
  gets MSVC's return-slot optimization (the hidden arg IS `&pt`, so reads are `[esp+N]`).
  Probed and REFUTED: separate declaration + `pt = ComputeLocalPos(...)` assignment (emits a
  real 8-byte copy, len 119 -> 127, DIFF 72 -> 116) and `const POINT &pt = ComputeLocalPos(...)`
  (folded away, byte-identical to the init form). Some third spelling suppresses the
  return-slot optimization without adding a copy; nothing tried reaches it.

- `GNetManager::DPlay_InitConnection` (0x45f390, v281, `asmscore.py --len 2076` total 1426710,
  align=1416 reg_pen=87 byte_diff=1140, insns 631/586) — `src/GameNet.cpp`. Down from v280's
  2173791 (byte_diff 1691) via 4 real structural fixes this session, all confirmed against raw
  `objdump` of the ORIGINAL (not just the decompile): (1) both protocol-dispatch `switch`
  statements (`GUID` selection, `elems[]` building) needed an explicit `case 4:` (IPX) instead
  of `default:`, letting the compiler FUSE the earlier bounds-check into the switch's own jump
  table — the original's `dec eax; cmp eax,3; ja fail; jmp [eax*4+table]` is one instruction
  sequence, not a separate `if` bounds check + switch; (2) a genuine correctness bug: the
  original calls `DPlay_TeardownConnection` only TWICE in the whole function (entry reset +
  one tail shared by `CreateCompoundAddress`-failed/`InitializeConnection`-failed), but the
  transcription had added it to 2 MORE handlers (fallback `DirectPlayCreate`/`QueryInterface`
  failed) that the original leaves as a bare `report; return 0;` — a real COM-object leak on
  those paths, now `// sic:`'d and cross-referenced in `docs/engine-bugs.md`; fixed by `goto`
  to a shared `teardown_fail:` label for the 2 real teardown sites only; (3) the Serial case's
  `nPort < 1 || nPort > 4` check compiles unsigned (`jb`/`jbe`) in the original, needing
  `unsigned int nPort` not `int`; (4) EACH of the 4 protocol cases sets `elems[0].guidDataType`/
  `dwDataSize` independently (confirmed via raw disasm: all 4 case bodies redundantly reload the
  same GUID from `0x4790b8-c4`) rather than once before the switch — not DRY, but faithful.
  Remaining residual is now dominated by classes already established elsewhere in this codebase
  as intrinsic/not source-steerable: symmetric register-role swaps (edi/ebx, eax/ecx — Yoda
  #29/#30 family), the `xor;cmp;setne;test` condition-materialization idiom (CLAUDE.md's
  `GameNet_EnqueueOrFreeNodeMaybe` precedent), and — the largest remaining chunk — several
  report+teardown/report-only exit blocks physically positioned at the very END of the compiled
  function (via a trailing `jmp`) instead of inline near their first use like the original
  (same "block layout is trace-driven, mostly not source-steerable" class already parked on
  `DPlay_JoinOrHostSession`, Yoda #15). Retry ideas if revisited: try moving the shared
  `teardown_fail:` label's TEXTUAL position earlier in the function (right after the
  `CreateCompoundAddress` check, before `Release()`/`CoCreateInstance`) to see if that steers
  the compiler's block placement — untested this session; double-check the Modem case's
  vtbl+0x48 (`GetConnectionCapsMaybe`)/`EnumAddress` argument shapes against
  `0x45f5f2`-`0x45f6d4`'s raw disasm (partially spot-checked this session, still the
  least-confident part — no `<dplay.h>` in this project). Durable non-match wins banked:
  `nProtocol`'s setter identified, all SP/AID GUIDs pinned, 7 new `GNetManager` fields,
  full per-protocol control-flow mapped, a real reference-leak engine bug found — see
  `src/GNetManager.h`'s own comment and `docs/subsystems.md`'s Connection-establishment section.

- `GNetManager::DPlay_HostNewSession` (0x45fd80, v290 confirmed unchanged from earlier analysis,
  `asmscore.py --len 419` total 233569, align=232 reg_pen=13 byte_diff=129, insns 132/134) —
  `src/GameNet.cpp`. Same 2 intrinsic classes as every other function in this DirectPlay
  connect/host/join cluster: (1) symmetric register-role swap (ebp/ebx/esi/edi permute
  differently vs. the original across most of the function); (2) the `hrLastResult==0x88770118 ->
  return 0x88770100` mask: original derives it via `xor al,al` on the already-compared register
  (reusing the just-loaded 0x88770118, whose low byte cleared == 0x88770100) while any source
  form (`return 0x88770100;` literal OR `return hrLastResult & 0xffffff00;`) compiles to a fresh
  `mov eax,0x88770100` immediate — TESTED BOTH forms this session, byte-identical output either
  way (score unchanged, 233569), confirming the compiler already constant-folds regardless of
  source shape; not source-steerable. Kept the plain literal (simpler, matches likely original
  intent). Not re-litigated further; same triage-budgeted family as `DPlay_InitConnection`/
  `DPlay_FindSession`/`DPlay_JoinOrHostSession`/`DPlay_JoinExistingSession` in this file.

- `GNetManager::DPlay_JoinExistingSession` (0x460360, v290 confirmed unchanged from earlier
  analysis, `asmscore.py --len 699` total 249576, align=248 reg_pen=13 byte_diff=146, insns
  222/233) — `src/GameNet.cpp`. Same goto-tail-sharing-is-local-not-global class already
  documented on `DPlay_FindSession` (3-vs-1 epilogues) and `DPlay_JoinOrHostSession` (5-vs-1):
  this function's shared `report_error:` tail (reached from 3 `goto` sites: 2 retry-loop
  bailouts + the explicit DPERR_NOSESSIONS check) compiles to up to 3 separate physical copies
  in the candidate rather than 1 shared block. Confirms (this session, across 4 independently-
  checked functions in this same connect/host/join cluster) that this residual class is a
  stable, well-understood wall — not worth further per-function re-litigation without a
  genuinely new lever; see `DPlay_InitConnection`'s own row below for the most complex instance.

- `DPlay_JoinSessionDlgProcMaybe` (0x4611b0, v279, `asmscore.py --len 1104` total 561017,
  align=550 reg_pen=95 byte_diff=427, insns 343/340) — `src/GameNet.cpp`. Large DLGPROC (session-
  browser listbox dialog); 3 real structural fixes landed this session (see the function's own
  EFFECTIVE-match comment): switch-not-if-ladder for both the outer WM_INITDIALOG/WM_COMMAND and
  inner id-dispatch (decrement-chain codegen); LOWORD-switch as the primary fall-through vs.
  HIWORD==2 as the `else` (jump-away-when-2 branch polarity); a `GNetManager *pNet =
  g_pNetManager;` local SCOPED to just each of the 2 duplicated drain+retry-loop regions (a
  function-wide local regressed elsewhere by pinning a register across the whole function).
  Residual: the id==1/notification==2 cases' identical "LB_GETCOUNT then conditionally
  LB_GETCURSEL" prologue gets one extra physically-duplicated copy (confirmed source-order-
  independent — swapping the case 1/0x7d01 source order was a no-op) plus the usual `cmp
  reg,ebx`-vs-`test reg,reg` zero-check tie-breaks (Yoda #29/#30 family). Not re-litigated
  further this session; a fresh angle (if any) would need to target why 2 byte-identical
  "get selection" prologues get partially merged/interleaved here but stay fully separate in
  the original.

- `GameNetThreadState::GameNet_BroadcastPlayerRoster` (0x43ccc0, v272, `asmscore.py --len
  327` total 202252, align=200 reg_pen=18 byte_diff=152, insns 96/104) — `src/GameNet.cpp`.
  Two intrinsic residual classes, ~30 min triage confirmed unmovable: (1) a full 3-way
  register-role rotation (original keeps pTrain/loop-index/detail-count in edi/esi/ebp; every
  recompile permutes them differently) — the symmetric-register-swap class (Yoda #29/#30)
  extended to 3 roles; (2) the second loop's record-slot address: the original recomputes
  `idx*0x390` via a fresh multiply-decomposition every iteration, while every source form tried
  (direct `pMsg->records[idx]` subscript, a bumped output pointer) gets strength-reduced by /O2
  into a `+= 0x390` accumulator — same induction-variable-elimination class as the already-parked
  `ApplyProviderSnapshot` (below), just on the write side. Direct subscripting scored best
  overall (202252) despite a slightly higher raw byte_diff than the pointer-bump variant
  (204472) and is what's kept. A dead `cmp word ptr [esp+N],3` with no consuming branch also
  appears in the original right before the header writes — never reproduced, likely the
  redundant-check-kept-by-original fold class; not chased further.

- `GameNetThreadState::GameNet_HandleTrainStateSync` (0x43b240, v271, `asmscore.py --len
  1162` byte_diff 56, insns 337/338) — `src/GameNet.cpp`. Real fixes landed this session (see the
  function's own EFFECTIVE-match comment for detail): hoisting the record-loop counter above the
  `dwReversed` store; re-reading the AllocNextAttId echo from `pReq[3]` at each use instead
  of caching it in a local (Yoda #19 aliasing); reclassifying `PostBag_AllocNextAttId` to a
  `PostBagCacheBundle::` member (this-in-ecx-but-never-read class); the active-list unlink loop
  needed a plain `while` with `pPrev != 0` as the fall-through arm; the two
  `(unsigned char)g_pDPlaySessionMgr->selectedProviderIndex` narrowing stores each needed an
  intermediate `unsigned int` local declared right before the store (byte-spill-width lesson).
  Residual: (a) a handful of `this`-register-role swaps in the roster-provider-slot scan setup
  (Yoda #29/#30, intrinsic); (b) the appearance-request success/failure branch's fall-through
  polarity — both `if(pFound==0)`/`if(pFound!=0)` source orders were tried (387 vs 533 total DIFF
  bytes), neither reproduces the original's own choice — matches the documented "genuinely not
  source-steerable" guard-clause counter-example (PostBag_SaveCardToCategoryMaybe); (c) a
  redundant `pCur != 0` recheck immediately before the scalar-deleting-dtor call, provably dead in
  both binaries — kept by the original, folded by our compiler, the intrinsic fold-vs-keep class.

- `GameNetThreadState::GameNet_ReceiveRosterSnapshot` (0x43ce10, v270, `asmscore.py --len
  647` total 154042, align=150 reg_pen=33 byte_diff=142, insns 198/199) — `src/GameNet.cpp`. The
  one real structural fix landed this session: the tail if/else's fall-through side must be
  `if (list != 0) broadcast(); else {teardown...}`, NOT the more natural-reading `if (list==0)
  {teardown...} else broadcast()` (semantically identical, opposite fall-through polarity) —
  this alone dropped the score 200050→154042 and closed every structural (`-`/`+`) gap in the
  whole 645-byte function, same branch-fall-through-polarity family as
  PostBag_ScanCategoryCrdFilesMaybe/AlbumCardWnd::DrawOrEraseCardSlot (CLAUDE.md). What remains is a
  single pervasive residual: `this` lands in esi throughout the recompile vs edi in the
  original (dragging a matching push-order/spill shuffle with it) — the well-documented
  intrinsic symmetric-register-swap class (Yoda #29/#30), confirmed dozens of times elsewhere
  in this file; not source-steerable. Two minor loop-internal scheduling diffs not chased
  further (both plausible but untested retry ideas): the car-record do-while's first-iteration
  skips a stack-slot reload the back-edge does (classic loop-rotation "first iter already has
  the value in a register" shape — Yoda #13 family); and the `AllocCarSlot(0x1871,4,1)` arg
  pushes interleave differently with the strcpy tail-copy's `and ecx,3`/`rep movs byte` setup.

- `GameNetThreadState::DPlay_BuildOtherSessionsList` (0x43a8b0, v270, `asmscore.py
  --len 321` total 48244→ (after polarity fix) byte_diff 24, align=48 reg_pen=2, insns 110/111)
  — `src/GameNet.cpp`. Two small residuals, both tried-and-failed for a source-level fix: (a)
  the strcmp-match ("filter out this session") case's own `pFound = pFound->pNext` advance is a
  DUPLICATE physical copy in the original (`mov ebx,[ebx]; jmp <loop top>`) vs. a single
  TAIL-MERGED copy shared with the no-match case's own advance here — an explicit `goto`-
  labelled duplicate had no effect (self-contained block-layout/cross-jump choice, Yoda
  #15/#18); (b) the `IniFile::ReadString` call's `this` (g_pIniFile) load lands a few bytes
  earlier here than in the original (which computes the output-buffer address into ecx FIRST,
  then reuses ecx for `this` only after that address is pushed) — an explicit `IniFile
  *pIni` local had no effect. Note for retry elsewhere: the `bool bHasDirectPlayMaybe = ...;`
  local WITH an inverted `if (!bHasDirectPlayMaybe)` polarity (false-arm-as-fall-through) DID
  fully reproduce the `xor/mov/cmp/setne/test` boolean-materialization prologue for this same
  `g_pNetManager->pDirectPlay4` field — `DPlay_UiConnectHandler` below tried the bool local
  alone (without the inverted polarity) and it did NOT reproduce; worth retrying there with the
  inverted-polarity lever.

- `GameNetThreadState::DPlay_UiConnectHandler` (0x43c860, v269, `asmscore.py --len 880`
  total 53813, align=50 reg_pen=34 byte_diff=73, insns 271/270) — `src/GameNet.cpp`. The URL
  char-class validation branch tree (a 3-level nested if/else on the scanned char's value) now
  byte-matches EXACTLY after picking each if/else pair's polarity so the flatter/smaller
  sub-tree is the fall-through side and the sub-tree that recurses into another comparison is
  the out-of-line jump target (both the outer `c<=0x3b`/`c>0x3b` split and inner
  `c<=0x5c`/`c>0x5c` split needed this — see CLAUDE.md's "if/else branch order" lever). Also
  needed an explicit `bool bHasDirectPlayMaybe = ...;` local (not an inline `if (ptr != 0)`) to
  reproduce the boolean-materialization dance (`xor/mov/test/setne/test`) around
  `g_pNetManager->pDirectPlay4`. Remaining residual: a consistent 3-way register rotation
  (edx/ecx/eax) spanning nearly the whole function, rooted at that same materialization site —
  matches the documented intrinsic prologue-materialization/symmetric-register-swap class
  (Yoda #29/#30, `GameNetMsgQueue::EnqueueOrFreeNode` 0x4393d0) and a tiny tail-end
  independent-constant-init reorder (`xor eax,eax`/`or ecx,0xffffffff` before a `strcpy`'s
  `repne scasb` scan). Not re-probed beyond the triage budget; no new lever tried.

- `SplashWnd::RedrawSettingRectsMaybe` (0x422010, v375, first transcription, `asmscore.py
  --len 1068` total **204010**, align=202 reg_pen=17 identity_miss=17 byte_diff=140, insns
  **345/347**) — `src/SplashWnd.cpp`. Repaints the four setting rects from NetSettings.
  **CALL PARITY EXACT** (8x 0x422570, 7x `LocoBitmap::RestoreOverlapBlt`, 1x 0x422440, 1x
  CopyRect, 1x OffsetRect) and block layout fully aligned. Getting there needed two BRANCH
  POLARITY corrections worth 244675 points between them — the original writes the flag-SET arm
  as the fallthrough for both radio pairs (`if (bSkipSetupWizardMaybe != 0) {checked} else
  {unchecked}` and `if (bUseSecondaryRememberedChoice != 0) {checked} else {unchecked}`), while
  the second, outer skip test guarding the remembered-choice pair is the opposite way round
  (`== 0`). Residual is one systematic zero-register tie-break repeated in 5 of the 6 art-stamp
  blocks: both compiles hold a zero in EDX for the `artRect.left/top = 0` stores, but the
  original still writes `test ebp,ebp` for the bitmap null check where this compile CSEs the
  zero in and writes `cmp ebp,edx`. **The 6th block — the one where VC5 keeps the zero in ESI
  instead — uses `test ebp,ebp` in BOTH**, which is the proof this is register residency and
  not source shape. Same class as the v334/v335 zero-reg residuals. Tried, no effect:
  `if (pArt)` vs `if (pArt != NULL)` (204010 to the point). Tried, strictly worse: RECT store
  order left/top/right/bottom instead of right/bottom/left/top (262600).

- `SplashWnd::OnActivate` (0x421be0, v376, first transcription, `asmscore.py --len 711`
  total **489479**, align=478 reg_pen=101 identity_miss=101 byte_diff=369, insns **226/244**) —
  `src/SplashWnd.cpp`. vtable slot 0x20, the "screen just became active, draw yourself" hook
  (same slot `AlbumCardWnd::OnActivate` overrides; NOT WM_PAINT). Structure fully verified against
  the raw disasm, CALL PARITY exact. The 18-instruction deficit is TWO instances of one MSVC
  PHASE-ORDERING difference, neither source-steerable:
  (1) The original emits the `CommitScreenUpdate; return` tail TWICE — once for `state == 0`, once
  for the `neither 0 nor 7` arm — and the two copies are NOT byte-identical, so they cannot merge:
  the default copy holds `hwndSelf` in eax and needs no `mov ecx,esi` (ecx still holds `this` from
  entry), the state==0 copy holds it in ecx and must therefore reload `this`. That is register
  allocation running AFTER the duplication; our cl 11.00 cross-jumps the two tails BEFORE
  allocation and emits one shared copy reached by a single `jne`. **v376's duplicated-tail lever
  (the one that took `OnEnterCommitAndDispatch` exact) does NOT apply — the source here already
  spells both tails out as separate statements.** Tried, no effect (489479 to the point):
  rewriting the three-way dispatch as `switch (state) { case 0: …; case 7: … }`.
  (2) The original keeps four dead `artRect` stores in BOTH label blocks (frame 0x24 vs ours 0x14
  — a whole second 16-byte RECT slot) and rebuilds the by-value argument from the same registers
  a few instructions later. Tried, no effect: hoisting `srcRect`/`artRect`/`screenRect` to function
  scope (v375's lever). No `lea` for the slot anywhere, so not the address-taken-local class.

- `SplashWnd::OnKeyDown` (0x420bb0, v376, first transcription, `asmscore.py --len 722`
  total **495575**, align=486 reg_pen=84 identity_miss=84 byte_diff=335, insns **237/243**) —
  `src/SplashWnd.cpp`. The keyboard twin of `OnLButtonDown`'s two button arms. Structure verified,
  CALL PARITY exact. The 6-instruction deficit is purely instance (2) above — the dead `artRect`
  local, frame 0x20 (two RECTs) vs ours. Tried and **STRICTLY WORSE (525704)**: flattening the key
  dispatch to `if (Enter) … else if (Esc) … else return DefWindowProcA(…)`; the nested
  `else { if (wParam != VK_ESCAPE) return DefWindowProcA(…); … }` form is what reproduces the
  original's block layout (both compares consecutive, DefWindowProcA inline as the fallthrough).

- `SplashWnd::SetState` (0x4208f0, v379, first transcription, `asmscore.py --len 0x225` total
  **24014**, align=24 reg_pen=**0** identity_miss=**0** byte_diff=14, insns **159/159**) —
  `src/SplashWnd.cpp`. The front-end sequencer. Every instruction lines up one-for-one and the
  register allocation agrees EVERYWHERE — the whole residual is one hoisted `push`. In the
  state-6 arm the original lifts the constant `1` above the `bSkipSetupWizardMaybe` test, so the
  single pushed constant serves `DPlaySessionMgr::SetMode` on the taken path and
  `ThreadWrapper::SetPriority` on the skipped one (the taken path then pushes its own second `1`
  before falling into SetPriority). Ours pushes each call's argument at that call. There is no
  source expression left to vary: both arguments really are the literal 1, and the two calls are
  already adjacent and in the right order. Probed and inert: spelling the guard `!= 0`.
  Two source shapes WERE worth real bytes here and are already fixed, so do not undo them: cases
  5 and 4 are written out as separate arms (cl 11.00 cross-jumps them back together itself, which
  is what the original does), and the state-3 arm duplicates its `BeginModalCapture()` call into
  BOTH branches — writing it once after the if/else makes cl compute the 4-or-5 store branchlessly
  (`setne`/`add ecx,4`) where the original branches, costing 66 instructions' worth of divergence.

- `SplashWnd::OnIdlePump` (0x421eb0, v376, first transcription, `asmscore.py --len 349`
  total **887**, align=0 reg_pen=8 identity_miss=8 byte_diff=7, insns **101/101**) —
  `src/SplashWnd.cpp`. Compiled length is EXACT and every instruction lines up one-for-one. The
  entire residual is a register rotation across the last three Win32 calls of the hwndVideo block:
  the original lets the `pVideoPlayer` load die into its own register (`mov eax,[esi+0x210]` then
  `mov eax,[eax+4]`) while this compile keeps the base pointer alive and picks a fresh register for
  the field (`mov ecx,[eax+4]`), which then rotates eax/ecx/edx through the `SetFocus` and
  `EnableWindow` reloads. No source expression left to vary — the member really is re-read from
  `this->pVideoPlayer` at each of the three call sites, which rules out a cached local. Same
  intrinsic allocator coin-flip class as the project's other documented tie-breaks.

- `SplashWnd::DrawArtOverBackdrop` (0x422440, v375, first transcription, `asmscore.py
  --len 304` total **26453**, align=24 reg_pen=22 identity_miss=22 byte_diff=33, insns
  **99/99**) — `src/SplashWnd.cpp`. Restores the composited backdrop under a setting rect, then
  stamps one frame of the setting's art over it. Every instruction lines up one-for-one; load
  order, test order and RECT store order all already agree (the store order was worth 12 bytes
  and is fixed). The whole residual is one register coin-flip plus its knock-on: the original
  parks `pDesc` in EDI and `nFrameIndex` in ECX, this compile picks the opposite pair, and
  downstream the original reloads `artRect.right` from its stack slot to build the by-value
  argument copy while this one keeps it in EAX. **Tried and confirmed NO EFFECT: hoisting
  `srcRect`/`artRect` to function scope** — the exact lever that turned its TU-sibling 0x4216f0
  exact in this same session — score unchanged at 26453 to the point, so this is definitively
  NOT the stack-slot-reuse class, it is the plain register coin-flip class.

- `SplashWnd::OnLButtonDown` (0x422930; v374: `asmscore.py --len 1090` total **162759**,
  byte_diff 139, **insns 381/378**) — the front-end click dispatcher, first transcription,
  `src/SplashWnd.cpp`. Structure verified against the raw disasm; **call parity 38 vs the
  original's 36**, and both extras are explained. Two independent residual classes:
  (1) **a VC5 dead-store difference.** The `artRect` local's four stores at `[esp+0x20..0x2c]`
  are genuinely dead in the ORIGINAL (written in both button arms, never read — the by-value
  argument copy is built separately from the same registers) and VC5 keeps them; our compile
  forwards the registers into the argument and drops the local, costing 4 instructions per arm.
  ⚠ Do NOT re-derive "it is read" from the `[esp+0x2c]` reads at 0x4229d6/0x422b26 — those sit
  at a push depth 0x14 deeper and address the CopyRect local at `[esp+0x10]` (the absolute-K
  aliasing trap). **Tried, zero effect: hoisting `RECT artRect` to function scope so one local
  serves both arms (162759 → 162759, exactly unchanged).** This is NOT the address-taken-local
  class of v371/v372 — there is no `lea` for the slot anywhere in the original.
  (2) **a cross-jump merge-point tie-break.** The play-alone arm's tail IS duplicated in the
  source — call parity proves it, `0x422010`/`RedrawSettingRectsMaybe` appears TWICE — and MSVC
  cross-jumps the common suffix. The original merges from `mov ecx,esi; call CommitScreenUpdate`
  onward, leaving only the arm's own `hwndSelf` load + 4 pushes duplicated; our compile merges
  later, from `PlayUiSound` onward, leaving an extra CommitScreenUpdate +
  RequestModeTransitionFromSource pair (exactly the +2 calls). Cause is one level up: the
  original loads pPointCursorDesc/pPointCursorRect/vtable into eax/ecx/edx in BOTH copies so the
  blocks are byte-identical and merge, ours picks eax/ecx/edx in one and ecx/edx/eax in the
  other, blocking it. **Tried, made it worse: a single shared tail** (one source-level
  `RedrawSettingRectsMaybe`) — total 138084 / insns 363/378, but it merges MORE than the
  original and contradicts call parity, so it is the wrong source shape despite the better
  score. A future retry should attack (1) first; fixing it may cascade into (2)'s allocation.

- `EditCardWnd::OnLButtonDown` (0x41ac10; v372: `asmscore.py --len 7792` total **6164892**,
  byte_diff 4232, **insns 2351/2451** — was total 6576410 / byte_diff 4710 / insns 2259/2451)
  — **v372 found and fixed a real content gap that every prior session mis-read as layout
  noise: our source SPLIT the original's single address-taken `POINT` local into two.** The
  original homes the click point at `[esp+0x10]`/`[esp+0x14]` and reloads it **119 times**,
  because one far-away site (`ClientToScreen(hwndSelf, &pt)` / `SetCursorPos`) takes its
  address; we had used a separate `ptScreen` there, so VC5 enregistered `pt` and dropped ~150
  `mov`s. Reusing the one local recovered **92 instructions from a one-line change**. See
  `docs/CODEGEN.md`'s v372 bullets for the diagnostic (stack-slot READ pressure, not `lea`
  counts). Content is now confirmed complete: call parity is 248 vs 243 with every callee
  accounted for. The REMAINING residual is the block-layout class described below.
  (Historical, still accurate for what is left — v226, then total
  6625018, byte_diff 4768, insns 2260/2450) — the ~600-line mouse-click dispatcher,
  `src/EditCardWnd.cpp`. Full writeup in `docs/subsystems.md`'s `HandleLButtonDownMaybe`
  section (search "v226"). v225 hypothesized 3 early-exit sites needed converting to an
  explicit `goto commitScreenAndReturn0;` shared tail; v226 REFUTED this by tracing all 3
  sites address-by-address — `src/` already matches the original byte-for-byte at each site,
  and each already ends in a plain `hwndTarget = hwndSelf;` with no `return`, i.e. already
  models the exact control-flow convergence a `goto` would add. The REAL residual (mapped by
  raw `asmscore.py --dump` offset, not eyeballed) is a large-scale BLOCK-LAYOUT/scheduling
  difference: ~47% of the function's `-`/`+` diff lines are one whole logical block (the same
  content, confirmed field-for-field) that the original keeps inline at each branch's own
  position but our recompile relocates ~0x1300 bytes later, near the shared tail. Yoda #15
  family ("block layout mostly NOT source-steerable") — v221 already ruled out a
  declaration-reorder probe on this same function with zero effect. **Do NOT re-attempt the
  goto-tail conversion — confirmed inert.** Needs real per-block register-allocation/layout
  iteration in a dedicated session, or a fundamentally new lever.

- `CursorDesc::ParseTokenFieldMaybe` (0x424e00, v224, `asmscore.py --len 0x7ef` total
  202754, insns 664/655) — the shared ee.ini-style token-field parser, `src/CursorDesc.cpp`.
  Full writeup in `docs/subsystems.md`'s `CursorDesc` section. v222 left this at
  265123/658. **v223 closed the documented "eof()-check CSE" cluster's ROOT CAUSE (265123→
  202754): the two post-loop skip-blocks were transcribed as `while (!pStream->eof() && cond)`
  (combined `&&` condition), which let /O2 treat both loops' inlined `istream::eof()` expansions
  as one shareable computation.** Raw disasm of the ORIGINAL at 0x425404-0x4254c5 showed each
  loop's own top-of-loop check is ONLY an `eof()` test (no `cond` check at all there) with the
  `cond` check deferred to the loop's own BACK EDGE after `getline()` — i.e. the real shape is
  `while (!pStream->eof()) { getline(...); if (cond) break; }`, not a combined-condition
  `while`. Rewriting both loops this way (this session's fix) reproduces each loop's own
  distinct inlined `eof()` expansion (confirmed via `objdump` cross-check — loop 1 uses
  `edx`/`dl` with `add eax,esi` AFTER the byte load, loop 2 uses `ecx`/`cl` with
  `lea eax,[ecx+esi]` BEFORE it, matching the original's own two genuinely-different register
  compositions) and closed most of the residual. Tried both a `break`-only and a
  `continue`-then-`break` phrasing of the same shape — byte-IDENTICAL compiles, no additional
  lever there. Remaining 202754 residual, NOT further reduced this session (2 clusters, still
  fits the "needs real per-block register-allocation iteration" bucket from v222, not a quick
  lever):
  1. **A residual duplicate-`eof()`-check artifact specifically at the loop-1-exit→loop-2-entry
     JUNCTION** — our compile still emits a small (~12-instruction) EXTRA re-check/duplicate
     inlined `eof()` expansion right at the point loop 1's back-edge falls through toward loop
     2 (`asmscore.py --dump` offsets 0x663-0x672 and 0x6d4-0x6e2), which the original doesn't
     have (it just falls straight into loop 2's own single top-of-loop check). This is likely
     the compiler being able to prove path-sensitive facts across the two loops' shared exit
     that the original toolchain's own allocator didn't/couldn't exploit at this call site —
     genuinely register-allocator/cross-jump-geometry territory (Yoda #7/#15/#18 family), not
     source-shape-steerable by anything tried so far (both break/continue phrasings, above,
     compiled identically). insns 664 vs 655 (9 extra) traces almost entirely to this one
     cluster. **Root cause pinned via raw disasm this session (v224):** the ORIGINAL doesn't
     avoid the duplicate check via a smarter loop2 entry — it does the OPPOSITE: it reuses the
     SAME shared-tail code (`0x425463: cmp cl,bl; jne 0x4254c5`) for BOTH loop1's normal
     exit-because-slash-found (a fallthrough, where the compare is provably-redundant/always
     false-taken) AND loop1's eof-exit (where the compare is a genuine, needed recheck of the
     last-read byte) — i.e. the original's compiler chose to CROSS-JUMP/tail-merge these two
     semantically-different exit reasons into one physical block, accepting one redundant
     compare on the fallthrough path to save code size, rather than specializing each path (what
     our compile does instead, at the cost of the extra ~12-instruction block). **Two more
     levers tried and RULED OUT this session (both zero/negative effect, confirming this is not
     source-shape-steerable):** (a) Yoda-lesson-#9-style loop rotation (`if (!eof()) { do
     {...} while (!eof()); }` for both loops instead of the plain `while`) — ZERO effect,
     byte-identical compile to the unrotated form (score unchanged 202754/664). (b) Explicitly
     funneling loop1's slash-exit and the "started with slash" case through one shared `goto`
     label (matching the original's own control-flow merge point) instead of two independent
     `if` guards — made it WORSE (202754→251120; insns dropped to 659 but align/reg_pen/
     byte_diff all grew), so the two-independent-`if` phrasing already in `src/` remains the
     best found. Do not re-attempt either without a genuinely new idea; this is now confirmed
     via 4 independent negative probes (2 from v222/v223, 2 from v224) to be intrinsic
     cross-jump/tail-merge tie-break territory.
  2. `strlen(field_0x48Maybe)`/`strcpy`/`strcat`'s own internal-scan scheduling (2 rewrite
     attempts from an even earlier session — reusing the strlen result via indexed writes
     instead of `strcat`, and hoisting the `while` guard — both scored WORSE and were reverted,
     see git history). This session's `objdump` read of the tail (0x4254c5-0x425542) shows most
     of its own diff lines are `S`-shifted (same content, different address — a downstream
     cascade of cluster 1's 9-instruction excess), with only a few genuine `r` register-choice
     diffs remaining; likely shrinks further on its own once cluster 1 closes, not an
     independent bug worth attacking separately.
  Also confirmed (cheap, isolated, not pursued further): a small ~10-byte-shifted `mov edi,ebx`
  diff at offset 0x14b/0x155, inside the "Name" branch's 3 repeated `strlen()` calls trimming
  trailing CR/LF (source lines ~42-50) — isolated single-instruction shift, not obviously
  connected to either cluster above, left unaddressed (low priority, tiny). Two unconfirmed
  "purpose not yet resolved" details, low priority: the exact meaning of the post-loop "skip to
  a `/`-prefixed line, then skip the whole following `/`-prefixed block" double-loop, and the
  literal 2-byte `"ut"` suffix appended to the shadow-bitmap base name before loading.
  `field_0x48Maybe`'s declared size (`char[0x20]`) and `szFullPath`'s declared size (263,
  frame-size-derived) are both still placeholders, not byte-confirmed.

- `EditCardWnd::AnimateDecalPickerScrollMaybe` (0x418e20, v220, `asmscore.py --len 0x435` total
  623745, insns 295/324) — the decal-picker page-flip wipe animation, the last real callee gap
  in `HandleLButtonDownMaybe`'s own tree (now content-complete). Full autopsy in
  `docs/subsystems.md`'s `EditCardWnd` section (v220 bullet). Two real structural bugs already
  fixed this session (both `unaff_EBX`/`unaff_EDI` root-caused as decompiler stack-slot
  mistracking, not hidden params; a missed one-shot `RestoreOverlapBlt` prep block + its
  "always slide old content" gate flag, reproduced via a deliberately-uninitialized local per
  `docs/engine-bugs.md`'s new entry). Remaining residual: dense register/stack-slot pressure in
  the pre-loop `RestoreOverlapBlt`-setup block (same family as `RedrawDecalThumbnailGridMaybe`'s
  own still-parked residual) plus an apparent loop-invariant hoist of the
  `RequestModeTransitionFromSourceMaybe` vtable+0xc fetch our compile performs that the original
  doesn't. **v221: the flagged retry idea (swap the `EditCardWndVtblProbe`-cast idiom at this one
  call site) was tried and RULED OUT** — hoisting `EditCardWndVtblProbe *pThis = (EditCardWndVtblProbe
  *)this;` once before the loop and calling `pThis->RequestModeTransitionMaybe(...)` inside it
  produced a byte-for-byte IDENTICAL compile (score 629748, byte_diff 528, insns 295/323,
  unchanged) — the compiler already treats the repeated inline cast identically, this lever is a
  dead end, don't retry it. No new lever identified this session; needs real per-block
  register-allocation iteration, not a quick idiom swap. **(2026-07-21: `EditCardWndVtblProbe`
  itself was subsequently retired repo-wide — this call site, now `EditCardWnd::
  AnimateDecalPickerPageWipe`, calls the real `WindowBase::RequestModeTransitionFromSource`
  virtual directly as `this->RequestModeTransitionFromSource(...)`; the v221 finding that the
  probe-cast lever is a dead end for this residual still stands, just via the real virtual call
  instead of the retired probe.)** `src/EditCardWnd.cpp`.

- `EditCardWnd::RedrawIdentityPreviewMaybe` (0x4189a0, v216, `asmscore.py --len 227` total
  92508, byte_diff 88, insns 70/74) — redraws the identity-preview strip (restores the
  button-row background over `rectUnk0x1a0`, offset up 0xb px, then conditionally redraws the
  loaded card's thumbnail). Content-complete first draft, one of `HandleLButtonDownMaybe`'s
  own not-yet-transcribed callees (now transcribed). Residual is confined entirely to the
  `destRect`/`srcRect` field-computation block: the original reads all 4 fields of
  `rectUnk0x1a0` through ONE cached base pointer (`lea eax,[esi+0x1a0]; mov ecx,eax`), computes
  `srcRect`'s 4 fields FIRST (spilling `nDestRight`/`nDestBottom` to fixed stack slots
  mid-computation), builds `srcRect` into an `esp`-relative buffer pinned via a dedicated `ebp`
  pointer, THEN reserves a SECOND stack buffer for `destRect` and reloads the two right/bottom
  values back out of their spill slots (rather than keeping them live in registers) to fill it
  — a dense, multi-register spill/reload choreography where the stack-slot offsets keep
  drifting relative to intervening `push`/`sub esp` instructions. Tried an explicit
  `RECT *pDest = &rectUnk0x1a0;` cached-pointer local (matching the original's own `ecx`-base
  read pattern): ZERO byte effect (identical total=92508 before and after), confirming the
  compiler already treats the 4 field reads equivalently regardless of this surface syntax —
  not a viable lever. Same class as the documented register-swap/spill-slot-reuse intrinsic
  residuals (Yoda #29/#30 family) until a fundamentally new angle turns up (e.g. a live
  register dump of the true original, per `RedrawCustomCursor`'s own precedent). `src/EditCardWnd.cpp`.

- `EditCardWnd::SelectDecalSlot` (0x41a360, v217, re-scored v359 `asmscore.py --len 254`
  byte_diff 1/254, insns 83/83, EFFECTIVE MATCH) — decal-picker thumbnail click handler. Sole
  residual: the SIB base/index roles of `mov al,[<this>+<idx>+0x370]`. The ORIGINAL is
  `[esi+edx*1+0x370]` (base=esi/this) and OURS is base=edx — v217's autopsy had these two sides
  printed backwards. **No longer "not source-steerable": v359 SOLVED this class** (see
  docs/CODEGEN.md "SIB base/index role") and used it to close the sibling 0x416e00. What is
  left here is the `this`-relative sub-case, where the slot goes to whichever operand is an
  ARITHMETIC result — the original masks its subscript yet still gets base=this. ~12 spellings
  of that mask probed and refuted (listed in the in-source autopsy); do not re-probe those.
  `src/EditCardWnd.cpp`.

- `EditCardWnd::RedrawRosterListMaybe` (0x419680, v217, `asmscore.py --len 570` total 399951,
  byte_diff 291, insns 172/182 -- mine 10 SHORTER, a real structural gap) — redraws the
  multiplayer roster scrollback list. Content-complete first draft, one of
  `HandleLButtonDownMaybe`'s own not-yet-transcribed callees (now transcribed). Known gap: the
  original computes its "fresh pass" flag (`field_0x174==0`) with a bare `cmp;jne` at its FIRST
  use, never materializing it to a stack byte until the reset branch; my transcription
  materializes `bFirstPass` up front (needed again inside the loop), adding ~3 extra
  comparison-adjacent instructions. Untested retry: re-derive the loop's own "first pass" test
  separately at its point of use instead of reusing one cached local (Yoda lesson #19's
  aliasing/no-early-caching family). `src/EditCardWnd.cpp`.

- `EditCardWnd::SyncDecalPickerButtonHighlights` (0x419560, v218, `asmscore.py --len 278`
  total 60707, byte_diff 27/278, EFFECTIVE MATCH) — full decal-picker chrome highlight sync
  (un-highlights 6 fixed buttons, conditionally highlights the network button + hit-test toggle,
  syncs all 16 category buttons). Needed the boolean call-args as explicit `if/else` branches
  (literal `push 0`/`push 1`) rather than a computed `cond ? 1 : 0` expression — the original's
  own per-branch pushes, not a stored/setcc'd boolean. Also needed the category loop's `i`
  declared plain `int`, not `unsigned int` (the original's own bound check is signed `jl`).
  Remaining residual is a pure `ebx`/`edi` register swap on the loop's pointer/index pair (Yoda
  #29/#30 family) — do not re-grind without a genuinely new lever. `src/EditCardWnd.cpp`.

- `EditCardWnd::CycleDecalKindHighlight` (0x41a460, v361, `asmscore.py --len 495` total
  12113, byte_diff 3/495, insns 161/161, EFFECTIVE MATCH) — decal-KIND hit-test cycler. Was
  byte_diff 27 until v361 fixed two real source facts: the coordinate pair is a **`POINT`
  by-value parameter** (not two `LONG`s used to fill a local — see `AlbumCardWnd::HitTestUiElement`
  and docs/CODEGEN.md), and the timer read must NOT be hoisted into a local before the
  `nEditMode == 9` test (`hDecalHitTestTimer` read directly in both arms; VC5 still CSEs it into
  the common predecessor, but scheduled AFTER the `cmp`, so `nEditMode` dies there and the timer
  load reuses `eax` — hoisting pinned the load before the `cmp` and forced a second register,
  29 → 15 → 5 bytes). Also needs `byOldKind` declared `unsigned int` (not `unsigned char`) for
  the original's zero-extend-to-dword spill shape. Residual is 3 bytes in the SECOND of the two
  duplicated `KillTimer` arms only: the original pushes the timer then reloads `hwndSelf` into
  the freed `eax` (`push eax; mov eax,[esi+8]; push eax`), ours loads `hwndSelf` into `ecx`
  first. The FIRST arm emits the original's exact pattern, so this is a per-arm scheduling
  coin-flip, not a source-shape gap. Probed and REFUTED: an `int nMode = nEditMode` local ahead
  of the timer read (folded away, byte-identical). `src/EditCardWnd.cpp`.

- `EditCardWnd::AdjustIdentityColorChannel` (0x418450, v218, `asmscore.py --len 803` total
  151402, byte_diff 82/803) — RGB channel +/- stepper. **Confirmed needs a real `switch(param_1)`**
  — an `if/else-if` chain on the same content scored dramatically worse (341894 vs 151402, tried
  both ways at the correct length). Remaining residual: the original shares ONE hoisted
  `edi=0xff` (and separately `edi=1`) register across all 3 case bodies' own comparisons — a
  decrement-chain dispatch shape (`sub eax,0; je; dec eax; je; dec eax` — NOT a jump table, no
  intervening code clobbers the register between cases) that keeps the constant alive across
  case boundaries. Tried hoisting `int nMax = 0xff;` before the `switch` and referencing it in
  all 3 cases' comparisons: ZERO effect (constant-propagated back to the literal either way,
  score unchanged) — not a viable lever via a plain local. Needs a genuinely new angle (possibly
  a live register dump of the real original, per `RedrawCustomCursor`'s own precedent) or accept
  as intrinsic. `src/EditCardWnd.cpp`.

- `EditCardWnd::RedrawDecalThumbnailGridMaybe` (0x418a90, v219, `asmscore.py --len 897` total
  415147, insns 261/269 — mine 8 SHORTER) — decal-picker thumbnail grid redraw, one of
  `HandleLButtonDownMaybe`'s own not-yet-transcribed callees (now transcribed), 897 bytes, the
  largest single function transcribed this session. Content-complete first draft, not byte-
  matched. Tried restructuring the 5-tier vertical-anchor `if`/`else if` chain to build
  `destRect`/`srcRect` fields directly inside each branch (mirroring what looked like per-branch
  duplicated setup code in the raw disasm) instead of computing shared `destTop`/`srcTop` locals
  then building the RECTs once afterward: scored WORSE (455541 vs 415147, insns 271 vs 269),
  reverted — the shared-variable form is the better baseline. Remaining residual looks like real
  register-allocation/stack-slot pressure (the prologue caches `this` into a second register
  early — `mov ebp,ecx` immediately after the initial param load — similar to the documented
  "ebp reused as second this-holding register" idiom on other large functions) rather than one
  identifiable structural gap; a future session should budget real per-block iteration rather
  than expecting a quick single-lever win. `src/EditCardWnd.cpp`.

- `NetSessionEventQueue::BigObj_CreateAndInsert` (0x41dd80, v208, `match.py` DIFF
  4/368, EFFECTIVE MATCH — was DIFF 30/371 v207, 192/249 under the old manual-alloc form)  —
  registry create/insert dispatcher for the BigObj-family world-object classes. **v208 root-
  caused the v207 residual**: the `[ebp+0x18]`/`this[1].vtbl`-shaped access is a genuine
  `NetSessionEventQueue` MEMBER (`dwReadyBigObjCount`, struct grown 0x18→0x1c bytes in
  Ghidra), not the separately-Ghidra-labeled adjacent global `g_dwReadyBigObjCountMaybe` a prior
  session modeled it as — ebp-relative addressing in the original disasm proves this (a plain
  global-by-name reference always compiles to absolute addressing under this toolchain,
  regardless of memory adjacency). Fixing this alone dropped DIFF 30→4. Residual: the 4 leaf
  `new` expressions' `/GX` alloc-protection EH state numbers (compiler-internal 0/1/2/3 tags,
  one per protected `new`) come out cyclically shifted by one vs the original. Tried flipping
  the outer `if(category==3)`/`else` polarity to reorder the lexical `new`-encounter order
  (matching the apparent state-number order) — this INSTEAD wrecked the whole function
  (264/372) because it also flipped which branch is the fall-through block (`TilePlacedObj`
  must stay the `else`/fall-through arm, matching the original's own block layout) — confirms
  the state numbering isn't simply lexical-order-driven and isn't independently source-
  steerable without breaking a separately-fixed, higher-value structural match. Same class as
  the documented register-swap/stack-slot-reuse intrinsic residuals (Yoda #29/#30 family) — do
  NOT re-grind. `src/NetSessionEventQueue.cpp`.
- `NetSessionEventQueue::BigObj_RemoveMaybe` (0x41def0, v209, `match.py` DIFF 260/518, true
  len 515 — unchanged from v208) — registry remove/destroy counterpart: unqueues from
  `pEvents`, then a paired-tile hand-off (global-track-link tiles ↔ points/switch tiles
  each search their 4 board-adjacent neighbors for the other shape and flush the match's queued
  BigObj), then an optional despawn-effect spawn + self-delete. **v208 fixed 3 issues**: (1) the
  same `dwReadyBigObjCount` real-member fix as the create side above; (2) the
  `pEvents` search loop was `if (Count()!=0) { do {...} while(i<nCount); }` — the
  documented do-while-peeled-by-a-distinct-outer-guard family — rewritten as a plain
  `while (i < pEvents.Count())` with `i` declared `unsigned int` (Count()'s real
  comparison form in the original disasm is unsigned; expect the resulting signed/unsigned
  compiler warning, it's intentional and matches asmscore's own jbe/jb tells); (3) the 4-field
  neighbor reset (`Unk0xc4/c8/cc/d0Maybe`, previously 4 separate `= 0;` stores) needed
  `memset(&pNeighbor->Unk0xc4Maybe, 0, 4*sizeof(unsigned int))` to reproduce the original's
  single shared-base-pointer (`lea`+offset-relative stores) shape — a bare cached pointer LOCAL
  to the same address had ZERO effect (tried, reverted) before memset's own codegen closed it.
  Also fixed a real (byte-match-neutral) bug found while duplicating the despawn-effect call per
  `DAT_00485234` branch to stop `/O2` folding the value-identical arms away: the literal
  mobility-flag argument had the 0/1 backwards vs the original's real fall-through/jump-taken
  split (now `DAT_00485234!=1` → flag 1, `==1` → flag 0, confirmed via raw disasm). **v209**:
  widened `dir` from `int` to `unsigned int` in both neighbor-search loops (the same lever that
  fixed the `pEvents` loop — original's own back-edge test at 0x41dfcc/0x41e056 is unsigned
  `jb`) — semantically correct (now matches the real comparison form) but score UNCHANGED
  (56249→56249): the deeper residual isn't signedness, it's the branch SHAPE. The original
  compiles each loop as short-backward-`jb`-continue + near-forward-unconditional-`jmp`-exit
  (the canonical do-while shape, loop entered by fallthrough in both binaries alike); our
  candidate compiles the inverse — near-forward-conditional-`jae`-exit + short-backward-
  unconditional-`jmp`-continue (1 byte heavier per loop, the source of the whole +3-byte length
  delta vs the true 515). Checked whether the shared tail (0x41e07d) is reached by a 3rd path
  that might explain the merge choice — it's reached by 4 paths total (2 early-skip guards + 2
  loop exits), all already correctly modeled via the single `tail:` label; no new lever found.
  Content-complete, still NOT byte-matched — remaining residual (asmscore score ~56k, was ~278k
  pre-v208) is this branch-shape tie-break on both neighbor-search loops (2 independent prior
  experiments already ruled out: for-loop conversion, explicit `goto tail;` unification on the
  2nd loop — treat as an intrinsic block-layout/trace-scheduling residual, Yoda lesson #15's
  family) plus one instruction-scheduling/register-swap tie-break in the effect-spawn branch
  pair. Own residual, don't re-grind without a genuinely new idea. `src/NetSessionEventQueue.cpp`.
- `TilePlacedObj::TilePlacedObj` (0x4580a0, v210, `asmscore.py --len 413` total
  325768, byte_diff 238, insns 113/119 — was v207 byte_diff 237/total 327537) — still
  content-complete, NOT byte-matched, but **v210 fixed 2 real transcription bugs found via raw
  disasm cross-check** (independent of the score, which barely moved — these are correctness
  fixes, not tie-break attempts): (1) the field-zeroing block right after the `bSpawnOffset`
  variance logic had `Unk0xccMaybe = 0; Unk0xd0Maybe = 0;` where the original actually writes
  `Unk0xbcMaybe = 0;` (a field that already existed in `src/TilePlacedObj.h` at `+0xbc` but was
  never assigned by this ctor) — `Unk0xcc`/`Unk0xd0` belong only to the SECOND zero-block later
  in the function (confirmed: disasm at 0x45817b/0x458181 writes `[esi+0xbc]`/`[esi+0xb8]`, not
  `0xcc`/`0xd0`, in this position). (2) `unsigned char cVariance = pDesc->bSpawnVariance;`
  was a caching local that doesn't exist in the original: the two comparisons (`==0`, dead `<1`)
  share ONE `al` load (adjacent, no intervening call — ordinary CSE), but each branch's
  `rand() % variance` expression RE-READS `pDesc->bSpawnVariance` fresh from memory via a
  newly-loaded `pDesc` pointer, not the already-loaded `al` (Yoda lesson #19's aliasing family —
  a real per-statement reload once a `call rand()` intervenes, no local). Fixed by using
  `pDesc->bSpawnVariance` directly at all 3 use sites instead of a cached local. **v211
  closed most of the remaining residual: `asmscore.py --len 413` total 325768→232999, byte_diff
  238→119, insns 113/119→119/119 (own exact insn count now).** 3 more real fixes: (1) removed
  the `BigObj *pDesc = pKindDesc;` ctor-scope caching local ENTIRELY (v210 had only fixed the 2
  recheck branches, not the whole ctor) — raw disasm confirmed 5 separate fresh reloads of
  `dword ptr [this+0x40]` across the function, the original never caches this pointer in a
  register at all; (2) inverted the outer `bSpawnVariance == 0` guard to
  `if (!=0) {...} else { =0; }` — the original places the zero-case out-of-line at the function's
  tail (CLAUDE.md's branch-order lever, previously only confirmed on 2-way if/else, now confirmed
  generalizing to a 3-way if/else-if/else chain too); (3)
  `memset(&Unk0xc4Maybe, 0, 4*sizeof(unsigned int));` /
  `memset(&Unk0xd4Maybe, 0, 4*sizeof(unsigned int));` replacing 8 individual `= 0;` stores — same
  `BigObj_RemoveMaybe`-documented memset-shaped-bulk-zero lever, closing the single largest
  remaining chunk. **Remaining residual is now ENTIRELY the 2 `rand()` calls' own register
  allocation**: the original keeps the loaded `bSpawnVariance` byte alive across the
  `rand()` call in a callee-saved register at both call sites (no stack round-trip); our
  candidate spills to a stack slot at one of them. Tried `int nRand = rand();` explicit
  sequencing (mixed/inconclusive result, reverted) — looks like the Yoda #29/#30 intrinsic
  register-allocation tie-break family but not conclusively proven not-source-steerable. Own
  residual, PARKED. **1 further idea tried and RULED OUT this session:** a shared
  `unsigned char bOffset;` local across all 3 branches + one final `bSpawnOffset = bOffset;`
  store (Yoda #3's "result-through-variable" family, on the theory that the original's repeated
  `mov byte ptr [esi+0x8c], dl` stores in 2 of the 3 branches implied a common local) — WORSENED
  the score (232999→249122, byte_diff 119→132, insns 119/119→118/119) — reverted. Direct
  per-branch `bSpawnOffset = ...;` stores (the current state) is confirmed better; don't
  re-try the shared-local shape without a new angle.
- `TrackTileObj::TrackTileObj` (0x44ae80, v210, `match.py` DIFF 322/426, `asmscore.py
  --len 426` total 136784, byte_diff 124, insns 109/112 — was v207 byte_diff 127/total 136787)
  — content-complete, NOT byte-matched. **v210 found a real content bug**: the
  `type in {0xe,0xf,0x10,0x11}` arm only had ONE `dwTrackTickState = 4;` write, but the
  original disasm (0x44afd5 AND 0x44afe9) writes it TWICE — once before `dwTrackState = 7;`
  and once again right after, immediately before the `ReleaseChannelAndDispatch(1)` call —
  a genuine redundant duplicate-assignment in the original (transcribed faithfully per the
  project's "reproduce, don't fix" convention; small, real, disasm-confirmed improvement, byte_
  diff 127→124). **2 further hypotheses tried and RULED OUT this session**: (a) swapping which
  is read first, `pDesc->m_type0x63a` (the dead `==0xb` recheck) vs `pDesc->Unk0x1e` (`dir`) —
  the original reads the type byte BEFORE `dir` but our shared-local structure reads `dir`
  first; restructuring so each branch computes its OWN `dir` (hoping the compiler would CSE the
  identical expression above the branch, matching the read-before-branch position) instead made
  the compiler DUPLICATE the read into each branch body, WORSENING the score (136784→147011,
  byte_diff 124→131) — reverted. (b) the dominant remaining residual is that the original caches
  the literal `4` (used at `nTypeTag`'s init AND all `dwTrackTickState = 4` sites) in
  `ebp`, kept alive across the WHOLE function (3 pushed callee-saved regs: `ebp`/`esi`/`edi`) —
  our candidate never allocates `ebp` at all (only 2 pushes: `esi`/`edi`), re-materializing the
  immediate `4` at each site instead; this cascades into a systematic +4 stack-slot-offset shift
  for every `[esp+N]` reference downstream. This looks like the SAME class as the documented
  register-swap/stack-slot-reuse intrinsic residuals (Yoda #29/#30 family). **v211 settled
  this: TU-position-dependence is RULED OUT.** Compiled this function ALONE as the first (only)
  function in a minimal probe TU (just `#include "TilePlacedObj.h"` + this one ctor body, no
  preceding functions at all) — `match.py` DIFF and `asmscore.py --len 426` both came back
  byte-for-byte IDENTICAL to the in-context compile (total=136784, byte_diff=124, insns
  109/112, zero change). This closes the last open lever from v210's writeup: the `ebp`-caches-
  literal-`4` register allocation is a genuinely intrinsic /O2 tie-break, not sensitive to
  what's textually before it in the TU. Confirmed EFFECTIVE — do NOT re-grind without a
  fundamentally new idea (e.g. a different local-declaration-order permutation, per Yoda #5).
  `TrackDepotTileObj::TrackDepotTileObj` (0x412870) reached EXACT MATCH
  in v207 once its own field-write order was corrected to ascending-offset (+0x11c/+0x120/
  +0x124/+0x128/+0x10c, matching the original). `src/TilePlacedObj.cpp`.
- `TrackConnectorTileObj::TrackConnectorTileObj` (0x44f210, v209, `match.py` DIFF
  3/136, EFFECTIVE MATCH — was DIFF 8/136 v207, mis-diagnosed then as "pure trailing-padding
  noise"). **v209 found the v207 byte_diff=8 reading was masking a real field-mapping bug, not
  padding noise**: the ctor's byte PATTERN was always right (a field zeroed unconditionally
  before the `switch`, a 1-or-0 sentinel written inside the switch, a 3rd field zeroed
  unconditionally in both arms — raw disasm at 0x44f240/0x44f24f+0x44f259/0x44f266+0x44f26c),
  but `src/TilePlacedObj.h`'s field NAMES had drifted out of sync with which offset is which
  (an interim fix during this same session briefly had them swapped the OTHER way before a
  3-way cross-check settled it — see below). **Confirmed true mapping** (Ghidra's own live
  struct had this right all along, `src/TilePlacedObj.h` was the thing out of sync): `+0x11c
  dwOccupancyClaim` (0=free, zeroed by this ctor; `PeerTrainNode::
  UpdatePlacementTickMaybe`'s own disasm — `mov ebx,1` then `mov [edi+0x11c],ebx` — writes 1 on
  train hand-off), `+0x120 pOwningTrain` (a real `void*`; this ctor presets a `(void*)1`
  sentinel for the 4 `ITEMKIND_TRACK_GLOBAL_{E,W,N,S}` kind ids else 0, later overwritten with
  the real owner via `UpdatePlacementTickMaybe`'s `mov [edi+0x120],esi`; independently
  cross-checked against `NetSessionEventQueue.cpp`'s own `QueuedBigObjItemPartial::
  pOwningTrain` partial-view field at the same +0x120 and its `== (void*)1` sentinel
  check), `+0x124 dwUnk0x124Maybe` (genuinely unmodeled, zeroed by this ctor). Fixing the field
  NAMES (byte pattern was already right) dropped DIFF 8→3. **The true residual (3 bytes) IS
  pure NOP-fill padding** between the `ret` and the 4-byte-aligned jump table (ours emits
  `90 8b ff`, the original emits `8d 49 00` — both are 3-byte no-ops filling the same gap; all
  real code is byte-identical up to that offset, insns 42/43 is exactly this one padding-
  instruction-count delta). Genuinely not source-steerable — don't re-grind. `src/TilePlacedObj.cpp`.

- `LocoBitmap::CreateAndFill` (0x42a850, v203, `asmscore.py --len 291` total 56274, byte_diff 54,
  insns 102/100) — fresh-allocate+fill dispatcher. Two already-documented intrinsic residual
  classes recur together here: (1) the DDBLTFX-field-init-vs-vtable-load ordering tie-break
  (identical to `Resize`'s own EFFECTIVE MATCH note in this file — tried caching `pSurface` in
  an explicit `pSurf` local first, per that precedent; zero change, confirmed byte-identical
  compile), and (2) the `bSuccess`-reload-vs-register-cache tie-break already documented on
  `AllocSurface` (the original stores `bSuccess`/`local_65` to its stack slot but sometimes
  keeps the freshly-returned value resident in a register instead of reloading it before the
  next use). No new lever found within budget. Don't re-grind without a genuinely new idea.

- `BuildToolCursorWnd::DrawIconFrame` (0x437900, v188, DIFF(157), len=188, candidate
  188 bytes) — the shared single-icon-slot blit helper. Original pushes only 2 callee-saved
  registers (esi=this, edi... actually esi=pRect/edi=this per the raw disasm); candidate needs
  a 3rd (ebp) to hold `pBitmap` live across the body instead of reloading it from its stack
  slot right before the `RestoreOverlapBlt` call, like the original does. Confirmed intrinsic
  via 3 independent, all byte-identical (same 2466/2483-byte .obj) source-shape probes: (1) a
  single `RECT` local built and passed directly by value vs. (2) two locals (`localRect` then
  field-copied into `srcRect`, matching Ghidra's own decompile of the true original) vs. (3) an
  explicit `RECT destRect = *pRect;` local instead of dereferencing `*pRect` inline at the call
  site; also tried `void*` vs. the real `CursorDesc*` type for the dead
  `pDescUnused` param — no effect either. Same register-allocation-tie-break family as
  Yoda lessons #7/#29/#30. Don't re-grind without a genuinely new lever.

- `BuildToolCursorWnd::DrawAllIconSlots` (0x4379c0, v189, `asmscore.py --len 813` total 458315,
  byte_diff 485, insns 229/234) — draws all 4 icon slots (A/B/C/D). Same intrinsic
  register-allocation/scheduling family as `DrawIconFrame` above. Confirmed via 2 tested
  levers: (1) hoisting each slot's `pBitmapX`/`nFrameIndexX` into a local declared right after
  the `Unk0x120` re-check (matching the original's own unconditional-early-load shape, visible
  in raw disasm before the `if` even branches) — improved the score (493→485 byte_diff,
  223→229/234 insns) but did NOT close it, since C++ local-declaration order doesn't force the
  optimizer to materialize the load at that exact point (data-flow scheduling, not
  source-position-driven); (2) slot A's own `destRect`/field-order variants (right,bottom-first
  vs left,top-first; with/without a separate `destRect` local vs. passing `rectWindow`
  directly) — right,bottom-first + explicit `destRect` copy scored best (458943) of several
  tried, but still non-exact. Remaining diff is dominated by (a) the decompiler's own
  "`pcVar8` cached `OutputDebugStringA` IAT pointer" artifact (unmodelable — plain direct calls
  already match `DrawIconFrame`'s own precedent) and (b) a `test reg,reg` vs `cmp
  reg,zeroreg` tie-break on each slot's `nFrameIndex != 0` check, where the candidate
  opportunistically reuses an already-zeroed register from the adjacent RECT zero-fill instead
  of a fresh `test`. Don't re-grind without a genuinely new lever.

- `BuildToolCursorWnd::StartSlotDAnimation` (0x438280, v190, `asmscore.py --len 0x2f8` total 501421,
  insns 211/205, byte_diff 381) — (re)starts slot D's animation in one of 5 states. Same
  register-allocation/scheduling family as `DrawIconFrame`/`DrawAllIconSlots` above (an
  identical decompiler-side `OutputDebugStringA` IAT-pointer-cache artifact plus assorted
  tie-breaks). Tested lever: fixed the top guard's comparison shape to match the field's real
  signed-`short` type (`nAnimState == (unsigned short)nState` instead of an unsigned-cast
  comparison) after retyping the Ghidra field from `unsigned short` to `short` (confirmed via 2
  independent sibling functions both naturally sign-extending it) — real, needed fix, but
  didn't close the residual alone. Don't re-grind without a genuinely new lever.

- `BuildToolCursorWnd::AdvanceSlotDAnimation` (0x438590, v190, `asmscore.py --len 0x2e1` total 214784,
  insns 226/214, byte_diff 184) — per-tick frame ADVANCE for slot D's animation, called every
  WM_TIMER tick by `OnAnimTimer`. Same register-allocation/scheduling family as the rest of
  this cluster. Improved by the same `nAnimState` signed-`short` retype as `StartSlotDAnimation`
  above (score 224796→214784). Don't re-grind without a genuinely new lever.

- `BuildToolCursorWnd::NotifyToolModeChanged` (0x438890, v190, `asmscore.py --len 0x88` total 148396,
  insns 50/38, byte_diff 66) — state-change side effect (sound cue + PostMessageA + vtable+4
  call + Unk0x120 reset). Root residual: the compiler CSEs/hoists the shared
  `g_pApp->hwndOwner` load across BOTH mutually-exclusive switch-case bodies (case 8 and
  the shared case-list body), collapsing what the original keeps as two independent 4-push
  sequences (cross-jump-merged only at the literal `call` instruction, Yoda lesson #18's
  family) into one shared push sequence. Tested and RULED OUT: (1) a shared `goto postMsg;`
  tail — same score as (2) two fully independent `PostMessageA(...)` statements — same score as
  (3) inlining `g_pApp->hwndOwner`/the byte cast directly as call arguments instead of
  through locals. All 3 variants compiled byte-identical, meaning this is a compiler-level PRE
  (partial redundancy elimination) decision, not a surface source-shape lever. One separately
  confirmed REAL fix: case 8's body must be declared FIRST in the switch (source declaration
  order determines case-body layout order in `.text`, matching the original's case-8-first
  block order) — this alone dropped the score from 200530→154964. Don't re-grind the CSE
  residual without a genuinely new lever (e.g. investigating whether an intervening volatile-ish
  read or differently-shaped case body defeats the compiler's redundancy analysis).

- `BuildToolCursorWnd::OnAnimTimer` (0x438940, v190, `asmscore.py --len 0x184` total 230339,
  insns 116/109, byte_diff 179) — the WM_TIMER handler driving the whole 4-slot animation
  cluster. Same register-allocation family as `DrawIconFrame` (needs an extra callee-saved
  register, ebp, to hold a value live across the `RestoreOverlapBlt` call that the original
  reloads from a struct field instead) plus a jump-table/dispatch-order shape difference in the
  Unk0x120 gate at function entry. Not deeply re-probed this session (discovered fresh, along
  with `NotifyToolModeChanged`, as a Ghidra analyzer gap-fill mid-session — see CLAUDE.md pickup v190).
  Worth a fresh, focused probe pass before re-parking further.

- `PopupWndBase::SetCursorDesc` (0x414340, v191, `asmscore.py --len 152` total 40233,
  insns 52/53, byte_diff 13, candidate len=150) — the (confirmed-shared, vtable slot 0xc) active
  cursor/icon-descriptor setter for all 3 PopupWndBase-derived singletons. Content-complete:
  the key-change guard, both cached-redraw-rect resets, and the conditional redraw chain all
  transcribed. Residual: a provably-dead redundant recheck (`if (nOldKey==nKey) goto setRects;`,
  unreachable-false given nothing between it and the outer identical check can change either
  value) that VC5 eliminates into a bare `jmp` in this recompile but the original keeps as a
  live `cmp;je` — tried both a cached local (`int nOldKey = ...;`) and fresh repeated `this->`
  field reads for the two comparisons, neither stopped the elimination. Unlike the
  `PostBagFileCache::LoadIndexedFileMaybe` precedent (where switching the surviving check from
  an equality to an unsigned range compare fixed an analogous issue), that lever doesn't apply
  here — both the original's surviving instruction and this recompile already independently
  agree on the `je`/equality FORM, so there's no comparison-shape mismatch to fix. The second
  residual this row used to list — "one unrelated dead `mov ecx,esi` before the argument-less
  `FUN_00414ef0()` call (that callee never reads `this`/esi)" — was neither dead nor unrelated
  and is FIXED in v362: it is the `this` pass to `PopupWndBase_RebindClipperToActiveScreen`,
  which was mis-declared `static` on exactly the inverted reading of that evidence (lever 3).
  Dropping `static` closed `OnMouseMove` (0x414a80) and `OnPaint` (0x414ae0) outright and
  supplied the missing instruction here; only the dead-recheck elimination is left.
  Don't re-grind without a genuinely new lever.

- `DSound::AcquireChannelForSoundMaybe` (0x413210, v89, DIFF 515/831 bytes, down from v81's
  706/811; `asmscore.py --len 0x320` total 591813, down from 683202 — align 588/byte_diff
  293/reg_pen 32/identity_miss 32, down from 676/382/62/62) — the DSound manager's
  channel-selection/steal dispatcher, semantics fully confirmed correct, still not
  byte-matching. v81's diagnosis (single shared `nChosen` index across all 4 search loops,
  `unsigned char nActiveCount`) stands and is unchanged. **v89 found two more real, principled
  fixes** applying the same techniques that closed `DSound_InitDeviceAndChannelPool` (v88):
  (1) raw objdump showed the original's `or edi,0xffffffff` (`nChosen = -1`) sitting
  IMMEDIATELY after the prologue pushes, before even the first search loop runs — our compile
  put the equivalent store much later (right before the `nActiveCount`/cooldown early-return
  check). Moving `int nChosen = -1;`'s DECLARATION to before `int nSoundId = ...;` (i.e.
  right after the null-check guard clause, but still after it — NOT above it, see below)
  pulled the store forward and closed a large chunk of the align/byte_diff gap in one step.
  (2) The cooldown check `pDesc->nNextAllowedTickMaybe > (int)g_dwGameTick` compiled with
  the load order (field first, global second) reversed vs. the original (global first, field
  second), flipping the jcc polarity (`jl` vs `jg` on an equivalent condition) — Yoda #6's
  "CMP operand order not always source-steerable" turned out to BE steerable here by writing
  the comparison in the original's apparent evaluation order:
  `(int)g_dwGameTick < pDesc->nNextAllowedTickMaybe`. **Tried and REVERTED:** moving
  `nChosen`'s declaration one step further, to ABOVE the null-check guard clause (matching the
  original's absolute-earliest placement) — this shrank the composite `align` score further
  but triggered a register-reassignment cascade (ebx/edi swapped through most of the function's
  back half) that made the ACTUAL byte diff worse (515→526 via `cc.sh`, the ground-truth
  metric) despite a lower composite `asmscore` total; reverted. **Lesson: when a source
  transform lowers the composite asmscore total but raises `reg_pen`/`identity_miss` sharply,
  cross-check `cc.sh`'s raw byte-diff before keeping it** — composite align improvements can
  be outweighed by a cascading register-swap regression that the weighted score under-counts.
  Remaining residual: loop A's `nSoundId` local sits in a STACK SLOT in the original
  (`[esp+0x14]`, reloaded every iteration) but our compile keeps it in a register — a
  whole-function register-pressure/spill difference (Yoda #13/#19); plus several repeated
  `add ecx, dword ptr [esi+0x98]` (channel-stride) sequences whose register choice differs
  per-use, matching the symmetric-register-swap intrinsic class (Yoda #29/#30). **v90 tried
  the queued "force extra register pressure" idea directly:** dropped the `nSoundId` local
  entirely and read `pDesc->nSoundId` inline in the loop-A comparison (hypothesis: an extra
  memory dereference per iteration might mimic the original's stack-slot-reload traffic).
  Result: regressed sharply, 515→718 bytes via `cc.sh` — reverted immediately, confirmed back
  at 515/831. The original's field-reload pattern (both `this->pChannels` and
  `this->nChannelCount`, not just `nSoundId`, are reloaded from `this` every loop-A iteration
  per the raw disasm — see `41x282`/`41x250` in the 0x413210 dump) looks like a genuine
  whole-function spill decision the allocator makes holistically, not something a single local
  variable's shape steers. **Closing this residual out as accepted/intrinsic** (Yoda
  #13/#19/#29/#30 family) after 4 sessions (v80/v81/v89/v90) of real, varied attempts — no
  further probing planned absent a new concrete hypothesis. `src/DSound.cpp`.
- `DSound::DSound_InitDeviceAndChannelPool` (0x412c50, v88, `asmscore.py --len 0x288` total
  score 254700, down from v87's 297417 — align 254/byte_diff 150/reg_pen 5/identity_miss 5,
  down from 296/197/9/32, **insns now 198/198, exact instruction-count alignment**) — v83
  found+fixed three real bugs and closed the SEH-scaffolding root cause; v85 closed the SEH
  prologue's remaining internal-ordering puzzle; v86 found two more real transcription bugs
  (branch-order polarity, `DSBUFFERDESC` zero-init idiom); v87 found and fixed three more real
  bugs in the channel-pool-header alloc/construct block; **v88 found and fixed the REAL
  RETURN-TYPE BUG, closing the last remaining structural gap:**
  12. **The function's real return type is `unsigned char` (a byte-truncated BOOL), not
      `unsigned int`/`HRESULT` — every one of its 4 return points sets only AL, never clears
      or sets the rest of EAX.** All 3 failure paths in the original disasm are the bare 2-byte
      `xor al,al` (rel 0x75, 0x90, 0xc9 — identical instruction, identical bytes, at ALL three
      `!= DS_OK` checks including the SetCooperativeLevel one) and the success path is
      `mov al,0x1` (rel 0x271) — none of the four ever touch bits 8-31 of EAX. The prior
      transcription (`unsigned int` return, `return hr;` / `return hr & 0xffffff00;`) was
      wrong: with a `uint`/`HRESULT`-width return, `/O2` MUST compute/preserve the full 32-bit
      value, so it always emits a wider instruction than a bare AL-clear (this exact residual
      pattern is CLAUDE.md's own documented "bool-return register width" lesson — `unsigned
      char` + `if (cond) return 1; return 0;` shape, apply by default whenever the original
      ends in bare `xor al,al`/`mov al,1` with no EAX-wide clear — somehow missed on this
      function despite being a known lesson). Ghidra's OWN decompile was actively misleading
      here: under the (wrong) `uint` prototype it rendered the 2 early failure points as
      `return uVar5 & 0xffffff00;` and the SetCooperativeLevel one as `uVar5 = uVar5 &
      0xffffff00;` — this is NOT a real 32-bit AND in the source, it's Ghidra algebraically
      describing the necessary consequence of an 8-bit-only write (`xor al,al`) against its
      current uint-typed view of the register (the true value only ever needs 1 byte; the
      `& 0xffffff00` framing evaporates once the prototype is corrected — confirmed by
      retyping in Ghidra: the decompile now shows clean `return '\0';`/`return '\x01';` at all
      4 points). Fixed: `src/DSound.h`/`src/DSound.cpp`'s return type to `unsigned char`, all
      3 failure `return hr;`/`return hr & 0xffffff00;` statements to `return 0;` (kept the
      unchanged `return 1;` success statement). Confirmed via Ghidra `set_function_prototype`
      retype + readback + `save_program`. Result: score 297417→254700, reg_pen 9→5,
      identity_miss 32→5 (biggest single-item drop of the whole function's history),
      byte_diff 197→150, **insns 195/198→198/198 (the instruction-COUNT mismatch that had
      persisted since v83 is now fully closed)** — zero regressions across all 39
      phase2-probe + 10 DSoundChannel.cpp + other 14 DSound.cpp matches (reverified). Tried
      caching the two ceiling constants (`0x14`/`0x28`) in explicit locals to chase the
      remaining ceiling-block scheduling residual (see "Remaining residual (v88)" below): zero
      effect, reverted —
      confirms that residual is pure `/O2` scheduling, not source-steerable.
  Earlier v87 fixes, all in the channel-pool-header alloc/construct block
  (`unsigned int *pHeader = ::operator new(...)` through `ArrayConstructWithIteratorMaybe`):
  9. **`HeaderGuard`'s ctor/dtor needed REAL (inline) bodies, not declared-without-bodies
     — CORRECTING v83's conclusion below.** Direct disasm of the compiled `.obj`
     (`objdump -d -r`, matching each `call`'s COFF relocation symbol name — `e8`'s masked
     target bytes alone don't disambiguate) showed our compile emitting THREE calls in this
     block (`HeaderGuard::HeaderGuard`, `ArrayConstructWithIteratorMaybe`,
     `HeaderGuard::~HeaderGuard`) where the ORIGINAL has only ONE
     (`ArrayConstructWithIteratorMaybe` — confirmed via the same relocation-symbol technique
     on `loco/Loco.exe`'s own call targets at `412e06`). v83's declared-without-bodies recipe
     (needed to stop an EARLIER, simplified scratch PROBE from optimizing away the whole guard
     including the SEH scaffolding) turned out to be the wrong prescription for the REAL
     function: giving both methods real inline bodies — ctor `: p(pp) {}` (trivial, provably
     inlinable) and dtor `{ if (p) ::operator delete(p); }` (a real conditional, NOT the
     unconditional-delete shape the funclet itself uses) — let `/O2` see that `guard.p = 0;`
     unconditionally precedes every normal-path scope-exit, proving the dtor call is always a
     no-op on the non-exceptional path and eliding it entirely, while the SEH unwind funclet
     (which fires on an exception BEFORE `guard.p = 0` runs, when `p` is still the real
     pointer) still needs the real cleanup logic and can't be proven away — so the compiler
     correctly KEEPS the `mov eax,fs:0`-style prologue/funclet AND drops the two spurious
     normal-path calls. Confirmed via direct disasm + `save_program`-free Ghidra readback: SEH
     prologue byte-for-byte unchanged (still present), ctor+dtor calls both gone from the
     normal path, score 445556→391390 (ctor fix alone; insns 203→200) then 391390→357602 (dtor
     fix; insns →197). **Do not use `HeaderGuard`'s old declared-without-bodies form as a
     template for a future similar guard** — try real bodies with a null-check dtor FIRST,
     only fall back to bodyless if the whole scaffolding provably vanishes on the real
     function (not just an isolated probe).
  10. **The `pHeader == NULL` / `!= NULL` branch polarity was inverted** — same
      "positive-condition-with-true-path-written-first" bug class as v86's item 7
      (`bSelectBestDevice`). Original's `je <null-case>` falls through into the
      `ArrayConstructWithIteratorMaybe` path and jumps away only for the null case — i.e.
      compiled from `if (pHeader != NULL) { ...construct...; } else { pNewChannels = NULL; }`,
      not `if (pHeader == NULL) {NULL} else {construct}`. Fixed by flipping the condition and
      swapping the bodies (semantics unchanged). Score 357602→297417 combined with fix 11 below
      (not separately measured — found and applied together in one pass).
  11. **`nChannelCount` needed an explicit cached local (`int nCount = nChannelCount;`) taken
      BEFORE the `operator new` call, then reused for both `*pHeader = nCount;` and the
      `ArrayConstructWithIteratorMaybe` count argument — reading `this->nChannelCount` fresh
      from memory at each use (the previous transcription) doesn't reproduce this.** Original
      loads `nChannelCount` into a register ONCE (well before the `operator new` call, for the
      alloc-size computation `nChannelCount*sizeof(DSoundChannel)+4`) into a CALLEE-SAVED
      register (edi) that naturally survives the call, then reuses that same register for both
      post-call reads — zero reloads. Our previous transcription re-read `this->nChannelCount`
      from memory for the alloc-size computation into a CALLER-SAVED register (eax), which the
      `operator new` call clobbers, forcing two separate memory reloads afterward that the
      original doesn't have. Caching the value in an explicit local before the call let the
      register allocator naturally pick a callee-saved register and carry it through,
      reproducing the original's single-load-reuse shape exactly (this specific 3-instruction
      island of the diff disappeared entirely). reg_pen dropped 29→9 in the same pass — this
      fix, not just the branch-polarity one, was the main driver.
  Combined v87 result: **score 445556→297417 (a third off), reg_pen 28→9, identity_miss 46→32,
  byte_diff 306→197, insns 203/198→195/198** — all with **zero regressions** across the other
  38 already-matched functions in this file and all 39 phase2-probe + 10 DSoundChannel.cpp
  matches (reverified). v86's earlier two-fix summary, kept for context:
  7. **The `bSelectBestDevice`/`else` branch order was inverted.** The real binary's
     `test bl,bl; je <false-branch>` places the TRUE (`bSelectBestDevice`) path as the
     fall-through and the FALSE path as the jump target — i.e. it's compiled from
     `if (bSelectBestDevice) { DirectSoundEnumerateA(...); DirectSoundCreate(&deviceGuid,...);
     } else { DirectSoundCreate(NULL,...); memset(&caps,...); ...; }`, a POSITIVE condition
     with the enumerate-path written first — not `if (!bSelectBestDevice) {A} else {B}` (which
     this toolchain compiles with the negated-condition branch, A, as fall-through instead).
     Purely a branch-order/condition-polarity fix, semantics unchanged. Fixed by flipping the
     `if`/`else` bodies and condition in `src/DSound.cpp`.
  8. **`DSBUFFERDESC dsbdesc`'s 5 fields are zero-filled first, then 2 overwritten — not 5
     individual field assignments.** Direct disasm shows a shared zero register (`xor edx,edx`)
     stored into all 5 field slots, THEN `dwSize`/`dwFlags` get overwritten with the real
     values — the same `memset(&x,0,sizeof(x)); x.field=...;` idiom this function already uses
     for `caps` just above. The previous transcription wrote 5 separate field assignments
     (`dsbdesc.dwSize=...; dsbdesc.dwFlags=...; dsbdesc.dwBufferBytes=0; ...`), which doesn't
     reproduce the zero-then-overwrite shape. Fixed by replacing with
     `memset(&dsbdesc, 0, sizeof(dsbdesc)); dsbdesc.dwSize=...; dsbdesc.dwFlags=...;` (matching
     `caps`'s style) — confirmed via objdump the memset compiles to the same 5-zero-store
     shape as the original, not a real function call.
  Both confirmed via direct objdump of `build/DSound.obj`, not just `asmscore.py`'s alignment
  view. **Remaining residual (v88, RE-CONFIRMED intrinsic, insns now 198/198 exact):** around the
  `CreateSoundBuffer` call, the original defers loading `pDirectSound`'s vtable pointer
  (`mov eax,[ebp]`) until immediately before `call [eax+0xc]` (the standard "push all args
  including `this`, then load vtable, then call" COM shape) — our compile loads the vtable
  pointer early, right after dereferencing `pDirectSound`, before the `dsbdesc` field stores
  and argument pushes. Tried (a) assigning the call result through an explicit `hr =` local
  first (kept — marginally improves the score, with zero regressions elsewhere, but does NOT
  move the vtable load) and (b) nothing else source-shape-wise moved it either. **This is the
  exact same
  residual class as `DSound_PickBestDeviceCallback`'s GetCaps call** (see its own entry below,
  parked since v80) — two independent sites, same toolchain behavior (a vtable-pointer load
  scheduled at its first-available point rather than deferred to point-of-use), confirmed via
  a v86 re-test on the PickBestDeviceCallback site (explicit `HRESULT hrCaps =` temp: zero
  effect). Promoted to a "Recurring residual classes" entry below — stop re-deriving this
  per-function, it's the scheduler's, not the source's. Plus the already-cataloged
  register-swap/zero-reuse tie-break families below. **The ceiling-constant-init block
  (nCeilingLow/MedMaster/Effective, offsets ~0x1f3-0x213) RE-TRIAGED v88 per the standing
  pickup item: source statement order already matches the original exactly (Low,Med,Low,Med
  interleaved by field, matching the original's store order) — the residual is purely `/O2`
  choosing to group the two Master-then-Effective stores by CONSTANT VALUE (0x28 pair then
  0x14 pair) instead of the original's by-FIELD-PAIR order, with the `nChannelCount` reload
  scheduled differently between them. Tried caching both constants (`0x14`/`0x28`) in explicit
  named locals ahead of all 4 assignments (matching the original's "load both constants before
  any store" shape): zero effect on the score, confirming this is pure `/O2` instruction
  scheduling, not source-steerable — same family as Yoda lesson #15/#7. Do not re-grind.**
  v85's original three-fix summary, kept for context:
  5. **The `WAVEFORMATEX wfx` local's real declaration site is immediately before
     `pPrimaryBuffer->SetFormat(&wfx)`, not at the top of the function.** The original defers
     all 7 field-init stores (`mov word/dword ptr [esp+N],K`) until right before that call
     (`412d69`-`412d95`); the previous transcription declared+initialized `wfx` as the
     function's first statements, so `/O2` hoisted all 7 stores to function entry instead —
     this was the actual root cause of the SEH prologue's wrong internal instruction order
     (too much unrelated stack-store traffic scheduled before `mov fs:0,esp`), not an
     intrinsic scheduler quirk as v84 suspected. Fixed by moving the whole `WAVEFORMATEX`
     declaration+init block into the `if (...CreateSoundBuffer(...) == DS_OK)` body,
     immediately before the `SetFormat` call.
  6. **`bSelectBestDevice` is declared BEFORE the `DSound_Teardown` call, not after.** The
     original's `mov bl,1` initializer executes before the `call DSound_Teardown` (safe since
     `ebx` is callee-saved under this calling convention) — this only reproduces when the
     source itself places the declaration first; a later declaration keeps the assignment
     scheduled after the call (source-position-sensitive, per Yoda lesson #7's family). Fixed
     by reordering the declaration above the `DSound_Teardown(...)` call.
  With both fixes, the SEH prologue now matches byte-for-byte AND instruction-for-instruction:
  `mov eax,fs:0; push -1; push <thunk>; push eax; mov eax,[g_pApp]; mov fs:0,esp; sub
  esp,0x28` — confirmed via direct disasm of the compiled `.obj`. **v84's item (c) is CLOSED:
  it was source-order-steerable all along, not intrinsic** — the isolated scratch probe got
  the real order right because it had no unrelated hoistable stack traffic to reorder around,
  same underlying cause as fix 5 above. Tried `char bSelectBestDevice` (to chase the real
  function's `bl`-width register choice for that variable): made things WORSE — spilled to a
  stack slot (`[esp+0x13]`) and broke the prologue reorder again (align regressed from 584 to
  628); reverted to `BOOL`. That register-WIDTH choice (byte vs full 32-bit) remains open, low
  priority — see remaining residual below.

  **v83's three original bugs + SEH-scaffolding root cause** (unchanged, kept for context):
  1. **`ArrayConstructWithIteratorMaybe` (`FUN_004671e0`) takes 5 args, not 4.** Raw disasm at
     the call site (`412df3`-`412e06`) pushes FIVE values before the call: dtor thunk
     (`0x40eca0`), ctor thunk (`0x40ec30`), count, size, array ptr. The callee's own `ret 0x14`
     (20 bytes = 5 dwords) confirms it. The missing 5th arg is the DESTRUCTOR callback
     (`&DSoundChannel::Release`, same address `ArrayDestructWithIteratorMaybe` already uses) —
     used internally by the helper to roll back already-constructed elements if a later
     element's ctor throws. Fixed: added the 5th param to the declaration and call site.
  2. **`ArrayConstructWithIteratorMaybe` is `__stdcall`, not `__cdecl`.** The callee's `ret 0x14`
     is a callee-cleanup return; our default-`__cdecl` declaration was emitting a spurious
     caller-side `add esp,0x14` after the call that the original doesn't have. Fixed by
     declaring it `void *__stdcall ArrayConstructWithIteratorMaybe(...)`.
  3. **`DSound_Teardown`'s argument was wrong.** Ghidra's own decompile shows
     `DSound_Teardown(this,*(undefined4 *)(DAT_004aa4a0 + 8))` — the App singleton's `+8` HWND
     field, NOT this function's own `hwndOwner` parameter (which is used later, for
     `SetCooperativeLevel`, instead). `src/DSound.cpp` was calling `DSound_Teardown(hwndOwner)`
     — a real, un-applied fix from a previous session's finding. Fixed: added
     `extern "C" char *g_pApp;` (`DAT_004aa4a0`) and changed the call to
     `DSound_Teardown(*(HWND *)(g_pApp + 8))`.
  4. **SEH-scaffolding root cause, now fully closed.** Confirmed via the FuncInfo table at
     `0x47a900` (raw dwords: magic `0x19930520`, `maxState=1`, `pUnwindMap=0x47a920`,
     `nTryBlocks=0`) that this is pure RAII automatic-unwind (no try/catch — `nTryBlocks=0`
     rules that out), with exactly ONE unwind state whose action is the funclet at `0x4752a0`:
     `mov eax,[ebp+4]; push eax; call operator_delete` (unconditional, no null check — safe
     since `operator delete(NULL)` is a no-op). This is the RAII guard hypothesized in v82,
     freeing the channel-pool header block (`pHeader`) if an exception unwinds during
     construction, before ownership transfers to `pChannels`. v82's attempt to reproduce this
     failed because of a SEPARATE, previously-unknown compiler behavior, isolated via a
     minimal scratch probe (not committed): **the automatic-unwind scaffolding is only emitted
     around a call the compiler considers capable of throwing a C++ exception — and a call to
     an `extern "C"`-linkage function is treated as non-throwing, suppressing the scaffolding
     entirely, even with an otherwise-correct class-typed RAII guard in scope.** Confirmed by
     probe: identical guard + call, only the callee's linkage (`extern "C"` vs plain C++)
     toggled — `extern "C"` produces a flat function body with duplicated inline dtor calls per
     exit path (no `fs:0` frame at all); real C++ linkage produces the exact `mov eax,fs:0;
     push -1; push <thunk>; push eax; mov fs:0,esp` shape plus a shared cleanup funclet in a
     `.text$x`-style section. Separately, a guard whose ctor/dtor bodies are given INLINE
     definitions gets fully optimized away by `/O2` (it can constant-propagate the dtor's
     pointer argument to a provable `0` and dead-code-eliminate the whole guard, ctor+dtor+
     scaffolding included) — the ctor/dtor must be declared WITHOUT bodies (never linked;
     `cc.sh` only compiles to `.obj`, never links, so this is safe) to force the compiler to
     treat them as real opaque calls it can't optimize away. **Fix applied:**
     `ArrayConstructWithIteratorMaybe` moved out of the `extern "C" { }` block (real C++
     linkage now), and a `HeaderGuard { void *p; HeaderGuard(void*); ~HeaderGuard(); }`
     (declared-only, no bodies) added, scoped in a nested block spanning from the `pHeader`
     alloc through the `ArrayConstructWithIteratorMaybe` call, disarmed (`guard.p = 0`) right
     before the block closes — matching the real function's state-variable toggle (`0` right
     after the `pHeader` alloc, `-1` right after the array-construct call returns). This
     recovered the EXACT real prologue shape (confirmed via direct disasm of the compiled
     `.obj`) and grew the match's instruction-count alignment from wildly misaligned to
     199/198 (near 1:1) instructions once diffed against the real function's TRUE byte extent
     (`0x412c50`–`0x412ed8` = 648 bytes, not the previously-assumed 591 — always pass
     `asmscore.py --len 0x288` for THIS function, the plain compare defaults to the
     candidate's own length and silently spills into the next function's bytes on any
     length mismatch).
     **⚠ SUPERSEDED (v87): the "declared-without-bodies" half of this fix was WRONG for the
     real function** — it kept the SEH scaffolding (good) but also left two spurious ctor/dtor
     CALL instructions in the normal path that the original doesn't have (bad — see item 9
     above). The correct recipe is REAL inline bodies (trivial ctor, null-checked dtor), which
     still keeps the scaffolding (because `/O2` can't prove the EXCEPTION-path dtor call is a
     no-op, only the normal-path one) while also eliding the two normal-path calls. The
     "inline bodies eliminate the whole guard" finding above was real but was an artifact of
     v82's ISOLATED SCRATCH PROBE (likely missing this function's genuine runtime-unknown
     branch, so the whole guard's liveness was fully provable const in that simplified probe);
     it does not hold once real branchy control flow is present. If retrofitting a similar
     guard elsewhere, start with real bodies + a null-check dtor, not declared-without-bodies.
  **Remaining residual** (v85, ~420/665 bytes): the function is now structurally correct
  (SEH shape AND internal order, call signature, calling convention, argument all fixed) but
  still shows (a) a handful of pervasive symmetric register-swap diffs (Yoda lesson #29/#30's
  family — the guard local's presence shifted which physical register the allocator picks for
  `pNewChannels`/the array-base pointer, rippling through ~20 later instructions that all
  reference it; likely intrinsic, not source-steerable) and (b) `cmp reg,ebp` vs `test reg,reg`
  instruction-selection diffs where the original always re-tests a pointer against zero
  explicitly even when a zero-holding register is already live (ours opportunistically reuses
  the zero register instead) — same family as Yoda lesson #6, likely intrinsic. (c) [CLOSED
  v85, see above] the SEH prologue's internal order now matches exactly. (d) NEW, v85:
  `bSelectBestDevice`'s register WIDTH — the original keeps it in `bl` (byte register) for its
  entire lifetime (`mov bl,1` / `mov bl,al` / `test bl,bl`), ours uses a full 32-bit register
  (`edi` after the reorder) despite the `BOOL` (int) declared type; tried `char` to chase the
  byte width, made it worse (stack spill, broke the prologue reorder), reverted — likely
  intrinsic (a value-only-ever-compared-to-zero narrowing decision independent of declared
  type), low priority. `src/DSound.cpp`.
- **Channel-pool array alloc/construct/destruct mystery — RESOLVED for the ctor, REFUTED for
  new[]/delete[] (v82).** The pool is built via a hand-rolled "array with 4-byte count header"
  idiom (`new_alloc(count*sizeof(DSoundChannel)+4)`, header stores count, a helper
  `FUN_004671e0` iterates calling a per-element ctor-shaped callback) and torn down via a
  mirror-image helper `FUN_00467280` passed `DSoundChannel::Release` as its callback. v81
  hypothesized this was the compiler's OWN `new[]`/`delete[]` vector-construct/destruct
  machinery (implying a real default ctor + dtor) and left it untried; v82 tried it and got a
  MIXED result:
  - **Ctor confirmed real, now modeled and MATCHED.** `DSoundChannel::DSoundChannel()`
    (0x40ec30) is a genuine constructor — added `DSoundChannel() { <Init()'s 11 field-init
    statements, duplicated inline, NOT a call to Init()> }` to `src/DSoundChannel.cpp`/`.h`,
    field order matching Init()'s EXECUTION order but starting from `pExternalHandle`/`bLoop`
    (not `nCategory` first, unlike Init()'s own source order — the two functions' statement
    order genuinely differs). This unparked 0x40ec30 outright (MATCH, len=49) — confirms the
    real source has a tiny standalone ctor whose body independently repeats Init()'s field
    writes (this is NOT delegating construction via a call to `Init()`; a `DSoundChannel() {
    Init(); }` version compiles to a real 12-byte out-of-line CALL, which does NOT match — the
    original inlines all 11 writes directly, len=49). `DSoundChannel.cpp` net +1 match (9/14 →
    10/15), +49 bytes byte-identical (269→318).
  - **new[]/delete[] hypothesis REFUTED for the call sites in `DSound.cpp` — do not retry.**
    Switching `DSound_InitDeviceAndChannelPool`'s alloc to plain `new DSoundChannel[nChannelCount]`
    made NO measurable difference to that function's residual (still ~494-499/582-591) because
    our compiler expands a runtvalue-count `new T[n]` as a raw INLINE loop (push/call the ctor
    per iteration, no shared runtime helper), never as a call to a generic ctor+dtor-taking
    helper function — but the ORIGINAL explicitly pushes BOTH 0x40ec30 (ctor) and 0x40eca0 (dtor
    address) as literal data arguments to a named helper (`FUN_004671e0`), which is the shape of
    a HAND-WRITTEN generic array utility, not compiler-synthesized `new[]` codegen. Tried adding
    a real `~DSoundChannel() { Release(); }` too (to make `delete[]` symmetric) — it compiles to
    a distinct 5-byte `jmp Release` TAIL-CALL THUNK at its own separate address, not literally
    Release()'s own address, so there is no hidden real destructor sharing 0x40eca0 either; the
    original source just passes `&DSoundChannel::Release` directly as the helper's callback
    argument (valid C++, same address, no synthesized dtor needed). Switching `DSound_Teardown`
    to `delete[] pChannels` made things WORSE (106/200 → 128/213 — confirmed regression, reverted)
    since the compiler's own vector-delete loop shape doesn't match the original's explicit
    helper-call shape at all. **Conclusion: keep the existing
    `ArrayConstructWithIteratorMaybe`/`ArrayDestructWithIteratorMaybe`/
    `DSoundChannel_ConstructThunkMaybe` opaque-extern model in `src/DSound.cpp` exactly as-is —
    it was already the correct shape, don't swap to real `new[]`/`delete[]` again.** The
    now-modeled real ctor (`DSoundChannel::DSoundChannel()`) lives only in
    `src/DSoundChannel.cpp`, independent of what `DSound.cpp` opaquely references for its call
    site's relocation (verify.py masks relocations, so the two don't need to be the same C++
    symbol to both byte-match). Teardown's explicit `Release()`-then-helper-`Release` double-pass
    remains CORRECT (confirmed real, not a modeling bug, per v81). `src/DSound.cpp`,
    `src/DSoundChannel.h`, `src/DSoundChannel.cpp`.

- `CreditsWnd::Show` (0x40f2a0, v199/v200, 72/72 insns structurally matching, byte_diff
  51/285) — slot-8 override. Residual is a pure register-allocation/instruction-scheduling
  tie-break around the `CreateSurface` call-arg setup and the RGB565-branch
  `DecodeFrameMaybe(nFrameIndex)` call (same S/r-only-diff class as Yoda lesson #29/#30's
  documented register-swap residuals): tried 2 field-store-order permutations for the
  `DDSurfaceDescPadded0x7c` scratch (matching the raw disasm's own dwSize/ddsCaps/dwWidth/
  dwHeight/dwFlags order, then a dwFlags-before-dwWidth/dwHeight variant) and removing the
  intermediate `hr` local in favor of a direct `if (CreateSurface(...) == 0)` — all three
  compiled byte-identical, ruling out simple source-shape levers. Don't re-grind without a
  genuinely new lever. `src/CreditsWnd.cpp`.

- `CreditsWnd::InitPreviewCanvasLazy` (0x40f6a0, v201, 50/50 insns structurally matching,
  byte_diff 12/188) — lazy resource init. Sole residual: the `pTileDesc` field store
  (`mov [esi+0x134],eax`) lands 2 instructions later than the original (after the vtable-slot-1
  deref for the following `GetOrLoadFrameBitmapMaybe(0,0)` call, instead of immediately after
  the `TileKind_GetOrLoadDescriptorMaybe` call that produced it) — pure store-scheduling, same
  S-class tie-break as Yoda lesson #29/#30. 3 source-shape variants tried, all byte-identical:
  the plain 2-statement form, splitting into an explicit `CursorDesc *pDesc` local assigned
  to the field first, and a nested `pFrameBitmap = (pTileDesc = ...)->GetOrLoad...()`
  single-statement form. Don't re-grind without a genuinely new lever. `src/CreditsWnd.cpp`.

- `CreditsWnd::BlitFadeCanvas` (0x410280, v202, 217/211 insns structurally matching,
  byte_diff 105/643) — fade-blend blit dispatcher. Needed 2 real fixes first: the documented
  do-while-peel lesson (`if (n>0) { do{}while(); }` → plain `while(n>0){}`), and a genuine
  `SetRect()` call for the two 0xd8x0xc4 scratch rects (an aggregate `{0,0,0xd8,0xc4}` literal
  silently constant-folds the call away, but the original really calls it — confirmed via a
  cached imported-function-pointer `call ebx` in the raw disasm). Sole remaining residual: the
  per-iteration fade-loop srcRect's 2 constant (0) fields get hoisted OUT of the loop by our
  compile (a provably loop-invariant store into a stable stack slot) while the original
  recomputes all 4 fields fresh every iteration. 4 source-shape variants tried, all
  byte-identical: aggregate literal vs. field-by-field assignment, 2 field-write orders
  (left/top-then-right/bottom vs. reversed), and with/without a named `destRect` local passed to
  `RestoreOverlapBlt`. Confirms an intrinsic VC5 store-hoisting choice, not source-steerable.
  Don't re-grind without a genuinely new lever. `src/CreditsWnd.cpp`.

- `CreditsWnd::BuildResourcePath` (0x40fe50, v204, 333/312 insns structurally matching,
  byte_diff 192/1004 — use `--len 1004`, the real Ghidra body size, not the candidate's own
  compiled length) — RF-archive-then-loose-file credits resource path/text loader. Needed 2 real
  fixes: `szLine`'s true size is 263 bytes, not the 0x1000 passed as `getline`'s own max-length
  cap (the original's total stack frame, 0x27c, has no room for a 4096-byte buffer — a genuine
  overflow-prone engine bug, transcribed faithfully with a `// sic:` comment, not "fixed"); an
  explicit `bool bHaveArchive = g_RFIndex.pFile != NULL;` intermediate matched the original's own
  `setne`/`test` bool materialization for the outer RF-archive guard (closed a small standalone
  residual). Sole remaining residual (5 occurrences, ~18 extra insns): every `istream::getline()`
  call inlines its own internal `lock()`+`get()`+`unlock()` sequence (the inline body in ios.h) —
  the original inlines the leading `lock()` the same way we do, but keeps the TRAILING `unlock()`
  as a real out-of-line call to a shared, `/Gy`-folded `ios::unlock()` body elsewhere in the exe,
  while our compile inlines `unlock()` too at every site. No lever found for this asymmetry
  itself — likely an MSVC inline-budget heuristic tied to cumulative inlined-code size within the
  function, not source-steerable (can't force `__declspec(noinline)`, rejected by this
  toolchain). Don't re-grind without a genuinely new lever. `src/CreditsWnd.cpp`.

- `CreditsWnd::DecodeFrameMaybe` (0x40f980, v205, 405/402 insns structurally matching, byte_diff
  148/1214 — use `--len 1214`, the real Ghidra body span) — the largest CreditsWnd method: scans
  `szResourcePathBuf` for the nFrameIndex-th `*`-delimited entry (throwing/catching an
  out-of-range condition via the shared `throw <int>;` machinery), parses an optional `<NNN>`
  TileKind-id tag via `atoi`, blits the resolved TileKind frame, then vertically-centers and
  `DrawTextA`'s the remaining caption via `pAnimSurface->GetDC`/`ReleaseDC`. Needed 3 real
  content fixes found only by cross-checking raw disasm against the decompile: (1) a completely
  missing `g_UIResources.PlayUiSound(0x5597);` call at function entry — Ghidra's own
  decompile showed it, but an earlier read of only the printed pseudocode summary skipped it;
  (2) a genuine dead-store engine bug — `ddck.dwColorSpaceHighValue` set but never read before
  `SetColorKey`, needing an explicit `ddck.dwColorSpaceHighValue = wColorKey;` statement to
  reproduce (a plain intermediate local got CSE'd away by this toolchain, unlike the original —
  writing to the real struct FIELD, not a temp, was what forced the second store); (3) the
  `pOwnedObj2`-null-check branch polarity needed inverting (`if (pOwnedObj2 != NULL) {
  <real work> } else { nTileKindId = 0; }`, success as fall-through) to match the original's
  own fall-through direction, per the established branch-order-is-sometimes-real lever. Also
  confirms 2 new decompiler-artifact classes for future sessions: Ghidra's own `extraout_ECX` in
  a function whose `this` is saved via `mov ebx,ecx` (not the literal `push ecx` slot, which gets
  overwritten by SEH-frame bookkeeping one instruction later) should be read as `this`, held in
  whatever register the real disasm shows; and a decompiled `local[0] = pcVar[1];`-shaped loop
  that never re-reads its own prior writes is likely a plain register-only scan Ghidra mis-
  rendered as a spurious memory write (confirmed via raw disasm: no store instruction at all).
  Sole remaining residual: a ~28-byte (0x1c) stack-offset drift starting at the `DDBLTFX fx`
  local (ours at `[ebp-0xac]`, original at `[ebp-0x90]`) that cascades through the rest of the
  function's local layout (the 3 big scratch buffers land ~0x1c further out at every use) —
  2 declaration-order variants tried (`ddck`-before-`fx` vs `fx`-before-`ddck`), the former
  scored better and is kept; no further lever found this session. Don't re-grind without a
  genuinely new lever. `src/CreditsWnd.cpp`.

## Recurring residual classes

- `WindowBase::CommitScreenUpdate` (0x426b90, ex-`FUN_00426b90`, v175: TRANSCRIBED, PARKED
  at DIFF 628/751 bytes, true body 789 bytes; v176: one real fix applied — asmscore
  678581->658466; v177: one MORE real structural fix applied — a genuinely missing branch, not a
  tie-break — asmscore 658466->522289, still PARKED, see below) -- the shared cursor-aware
  "commit a screen update" helper; see
  `docs/subsystems.md`'s "WindowBase" section for the full behavioral writeup. Same
  decompiler/register-allocation-maze class as the sibling `RedrawCustomCursor` above (both
  reuse the same cursor-rect clamp algorithm against `rectClipBounds`). Structurally/
  semantically verified via an exhaustive raw-disasm trace (delegated to a research agent first,
  then independently re-verified instruction-by-instruction by hand against the compiled
  candidate's own register/stack provenance -- every call argument and field derivation
  cross-checked directly from `objdump`, not trusted from Ghidra's own decompile, which
  mis-attributes fields in the `RestoreOverlapBlt` call's `destRect`/`srcRect` construction the
  same way it did on `RedrawCustomCursor` -- `unaff_EBX`/`unaff_EBP` here resolve to a plain
  `rectCursor` copy for `destRect` and the usual clamp-delta-derived `srcRect`, confirmed via
  independent stack-offset tracing).
  **v175 lever found and applied**: `nClampTopMaybe`/`nClampLeftMaybe` (`local_70`/
  the `[esp+0x14]` slot) are zero-initialized UNCONDITIONALLY at function entry in the original
  -- before even the `hdcToRelease` check, let alone the cursor-aware branch that actually
  reads them -- same "trivial-constant local hoisted above an intervening call" class documented
  for `DSound_InitDeviceAndChannelPool`. Moving their declaration from inside the cursor-aware
  `else`-branch to the top of the function (matching the original's own store order: top-clamp
  delta first, left-clamp delta second) dropped the asmscore total from 704209 to 678581 but
  did not close the gap.
  **v176 lever found and applied**: the `nCursorFrameCount` frame-offset branch was written
  as `if (nCursorFrameCount < 2) { nFrameOffX = 0; } else { ...multiply...; }` — Ghidra's
  own decompile normalized form — but the raw disasm at `0x426cbf` (`cmp edi,1; jle <zero-path>`,
  zero-path placed OUT OF LINE as a forward-jump target, multiply as fall-through) is the literal
  codegen for the INVERTED source shape `if (nCursorFrameCount > 1) { ...multiply...; }
  else { nFrameOffX = 0; }` — same Yoda-lesson-#2 "comparisons are emitted literally" class,
  combined with the documented branch-order/fall-through lever (CLAUDE.md's `if`/`else`
  polarity bullet). Fixing this dropped asmscore total 678581->658466 (byte_diff 481->476) — a
  real, confirmed, but small improvement; did not close the gap.
  **v176 declaration-order levers tried, BOTH exhausted (zero effect)**: (1) hoisted
  `rectBltDest`+`srcRect` to the top of the cursor-aware branch (before `ptCursor`/`GetCursorPos`)
  to raise register pressure earlier — compiled BYTE-IDENTICAL to the un-hoisted form; (2)
  separately hoisted `rectUnion`+`rectUnion2` to the same position — also BYTE-IDENTICAL, zero
  effect. Both reverted; kept the natural declare-near-first-use order (cleaner source, no
  compiled-code cost either way).
  **Dominant remaining residual (unchanged from v175)**: this function has 3 semantically-
  identical `Ddraw_RebindWindowClipper(g_pApp->hwndOwner); return;` exit points (the
  `bSkip!=0` early-out, the failed-both-`IntersectRect`s bail, and the function's own
  trailing statement) -- the ORIGINAL keeps all 3 as separate, non-merged physical copies
  (confirmed via raw disasm: each uses a DIFFERENT register pair -- eax/ecx, eax/ecx with
  different scheduling, edx/eax -- meaning they're not byte-identical post-allocation and so
  aren't `/O2` cross-jump-merge candidates), while this candidate's lower overall register
  pressure lets the allocator produce byte-identical copies at 2+ of the 3 sites, which the
  backend then merges into one shared tail (visible as a `jne`/`je` to a FAR address near the
  end of the function instead of a short local jump past a small inline block). Also converted
  2 ternary pointer-selections (`pRect`/`pDirtyRect`) to explicit if/else matching the original's
  literal per-branch stores -- no measurable effect on its own, kept for fidelity regardless.
  **v177 lever found and applied (real structural fix, not a tie-break)**: the original
  re-reads `pCursorBitmap` (`this+0x14`) a SECOND time, right after the `GetCursorPos` call,
  and branches on it AGAIN to pick `nHeight`/`nWidth` (0/0 if NULL) — even though it was already
  proven non-NULL by the outer `if (pCursorBitmap == NULL || bSuppressCursorRedraw)`
  guard earlier in the function. This redundant check is NOT eliminated by `/O2` because
  `GetCursorPos` is an opaque external call sitting between the two reads — the compiler can't
  prove `this->pCursorBitmap` is unchanged across it, so the check survives as a genuine
  compiled branch (a new variant of the "provably-dead redundant re-check" class: this one
  needs an intervening opaque CALL to survive, rather than needing an unsigned RANGE compare —
  see `PostBagFileCache::LoadIndexedFile`'s own variant of the same family in CLAUDE.md).
  Confirmed via raw disasm: `mov eax,[esi+0x14]` re-executes right after `GetCursorPos`, then
  `cmp eax,ebx; je` branches to a zero-both-locals arm, structurally identical to the outer
  guard's own check. Writing this explicitly as `if (pCursorBitmap != NULL) { nHeight =
  nCursorHeight; nWidth = nCursorWidth; } else { nHeight = 0; nWidth = 0; }` (replacing
  the previously-unconditional `int nWidth = nCursorWidth; int nHeight =
  nCursorHeight;`) dropped asmscore total 658466->522289 (byte_diff 476->369) — a
  substantial, confirmed improvement (~22% reduction), NOT a full close. **Metric gotcha
  discovered along the way**: `cc.sh`'s own naive `DIFF()` count went UP (628->640) from this fix
  purely because match.py's comparison window is sized to the CANDIDATE's own compiled length
  (751->762 bytes, still short of the true 789) — always judge this function via
  `asmscore.py src/WindowBase.cpp 0x426b90 --len 789 --dump`, never `cc.sh`'s raw `DIFF()`, which
  is actively misleading whenever the candidate is still shorter than the true body (same trap
  CLAUDE.md already documents for `asmscore.py` itself when `--len` is omitted — turns out
  `match.py`/`cc.sh` has the identical, undocumented-until-now blind spot).
  **v177 follow-up hoist tried, made it WORSE (reverted)**: moved the `srcRect` field
  computation earlier (right after `nFrameOffX`, before the `rectUnion`/`pUpdateRect`
  block) to match what looked like an early combined `nFrameOffX`+`srcRect` computation cluster
  in the original disasm right after the v177 fix's own branch — asmscore total got WORSE
  (522289->526835, byte_diff 369->425). Reverted; kept `srcRect` declared at its natural
  near-first-use position right before the `RestoreOverlapBlt` call.
  **v177 also corrected a prior claim**: cross-checked `RedrawCustomCursor`'s own raw disasm
  at the equivalent point (right after ITS `GetCursorPos` call) — it loads `nCursorWidth`/
  `nCursorHeight` UNCONDITIONALLY, no second branch, because unlike `CommitScreenUpdate`
  it has no antecedent `pCursorBitmap==NULL` early-exit before this point. So the v177 fix's
  double-check pattern is specific to `CommitScreenUpdate`, NOT shared with
  `RedrawCustomCursor` as prior pickups assumed — only the 4-way `rectClipBounds`
  clamp-if block itself (right/bottom/top/left) is genuinely shared between the two functions,
  not the surrounding setup code. A future joint live-register-dump session should scope itself
  to just that 4-way clamp block, not the whole function bodies.
  Next session: the missing-branch class of bug is now closed for this function (verified by an
  exhaustive re-trace of every remaining "-" (orig-only) instruction cluster in the
  `asmscore --dump` output — the survivors are the already-documented 3-exit tail-merge +
  general register-allocation cascade, not more missing logic). Don't keep probing isolated
  declaration-order tweaks in this function alone — the shared 4-way clamp block still needs a
  live register dump / minimal-TU reproduction (scoped narrowly this time, per the correction
  above) to make further progress. `src/WindowBase.cpp`.
  **v178: ran the minimal-TU probe (compiled the function completely alone, just this method +
  `WindowBase.h` + the minimal externs it needs, no other WindowBase.cpp functions in the TU)
  to test the TU-position-sensitivity hypothesis directly rather than assuming it.** Result:
  `asmscore.py --len 789` on the standalone probe scored EXACTLY 522289 total / byte_diff 369 —
  byte-IDENTICAL to the full-`WindowBase.cpp` compile. This function's residual is confirmed
  **100% intrinsic to its own code shape, NOT TU-position-sensitive** (rules out Yoda lesson #7
  for this specific function, a genuinely new and directly-tested conclusion, not just an
  inference). Same test run on the sibling `RedrawCustomCursor` (isolated single-method
  probe vs. full-TU compile): also byte-IDENTICAL, 753768/658 both ways — same conclusion for
  the sibling too. Since both functions' residuals are proven position-independent and are
  dominated by documented block-layout/cross-jump-merge decisions (Yoda #15, "mostly NOT
  source-steerable"), a real "live register dump" of the running game would add nothing further
  here — the values at every point are already known exactly, both from static disasm tracing
  (done across v138-v177) and would be from a dynamic trace too; what's undetermined is a
  compiler-backend block-placement tie-break, which no register value can resolve. **Do not
  chase a live debugger/emulator session for this residual class specifically** — that lever is
  for cases where a register's VALUE is ambiguous (e.g. Ghidra's SSA-tracking breakdown that
  produced `unaff_EBX`-shaped locals, already resolved for both functions in earlier sessions),
  not for a block-layout/tail-merge disagreement. Both functions remain PARKED at their v177
  scores (522289/369 and 753768/658 respectively); no further lever identified within the
  standard triage budget. Any future session picking this back up should try genuinely
  different, differently-SHAPED source restructurings of the 3-exit tail specifically (not
  declaration-order/hoisting variants, all of which are now exhausted for both functions), or
  treat as `// EFFECTIVE` and stop re-grinding. `src/WindowBase.cpp`.

- `DPlaySessionMgr::LayoutNet_ReplyWithStoredLayout` (0x43d520, v236, `asmscore.py --len
  253` total ~57709, cc.sh DIFF 42, insns 86/85) — the opcode-0x3f9 layout-bitmap reply
  producer, `src/DPlaySessionMgr.cpp`. Structure is byte-aligned through the payload alloc +
  opcode store; the entire residual is a single symmetric ax↔cx allocator swap on the two
  `short` cols/rows loads (original cols→cx, rows→ax with the product landing in edx; ours lands
  it in ecx), which cascades into one extra pixel-count register shuffle and reschedules the
  constant `bReliable=1` store earlier. Confirmed unsteerable from source within the triage
  budget: multiply operand order (rows*cols vs cols*rows — compiler canonicalizes, zero effect),
  named-local vs inline field reads, and cols/rows read-order were all tried; each only
  relocates the swap to a different register pair, never fixes it. Intrinsic register-swap class
  (Yoda #29/#30). REFUTED lever (v237): transcribing the real `.obj` predecessor
  `LayoutNet_SendCurrentLayoutBitmap` (0x43d350) and placing it immediately before Reply in
  source did NOT change Reply's swap (still DIFF 42) — incoming allocator state from the .obj
  predecessor is not the cause here. Do NOT re-grind the source-shape variants above.

- `DPlaySessionMgr::LayoutNet_SendCurrentLayoutBitmap` (0x43d350, v237, `asmscore.py --len
  457` total ~68168, cc.sh DIFF ~227, insns 140/142) — the sibling opcode-0x3f9 producer that
  snapshots the live board (`WorldBoardMaybe::CaptureBoardToBitmap` into a `new LocoBitmap`), caches a
  private copy in the local slot, and sends it, `src/DPlaySessionMgr.cpp`. Structure is fully
  faithful; the two-insn gap is /O2 SPECULATIVELY hoisting the `pMsg->dwColsRows` load above
  the `selectedProviderIndex >= 0` branch and spilling it across (a hoist-or-not tie-break my
  compile resolves the other way, not a source difference). Dominant residual is the SAME
  intrinsic ax/cx cols/rows register swap as Reply (0x43d520), cascading into the pSelected-reload
  register, pixel-count shuffle, `bReliable=1` store scheduling, and the first slot-field read's
  addressing mode (anchored `[edi+0x44]` vs index-math `[edi+edx*4+0x55c]`). Send is the FIRST
  LayoutNet function in the .obj so it has no in-source predecessor to reproduce its incoming
  allocator state. Levers tried (all inert or only relocating the swap): multiply operand order,
  colsRows local vs inline read, anchored old-pointer read. Intrinsic register-swap class
  (Yoda #29/#30).

- `GameNetThreadState::GameNet_BeginFileTransfer` (0x439d00, v256, `asmscore.py --len 233`
  total ~142186, cc.sh DIFF 150, insns 82/82) — opcode-0x11 handler: open a requested local clipart
  `.att` file and append a FileTransferNode to the outbound-transfer list, `src/GameNet.cpp`.
  Structure is byte-identical (82/82 insns, same CFG, branch orders already inverted to match); the
  sole residual is an intrinsic callee-saved 3-cycle permutation for {this, pHead, zeroConst} =
  {ebx, ebp, edi} (original) vs {ebp, edi, ebx} (mine). Its one visible downstream tell: the two
  byte-field zero stores (`bFlag0x10`/`bFlag0x14`) emit an immediate `mov byte[..],0` in the original
  vs `mov byte[..],bl` here — a pure consequence of the original's zero landing in edi (no
  8-bit sub-register) forcing the immediate. Confirmed intrinsic: identical score in a minimal
  isolated TU (not TU-position-sensitive) and unmoved by swapping the pHead/pNode declaration order.
  Yoda #29/#30. Do NOT re-grind without a genuinely new register-steering idea.

- `GameNet_DrainPendingTrainQueue` (0x43e010, v238, `asmscore.py --len 447` total ~120734,
  cc.sh DIFF ~312, insns 150/151) — the pending-train-queue consumer (rate-gated pop of the first
  node whose cooldown byte is 0; joined-mode owner-slot check releases via virtual dtor, else
  placement + local-player arrival-chime scan), `src/DPlaySessionMgr.cpp`. Two intrinsic /O2
  residuals, both source-unsteerable within budget (full autopsy in the function's own
  `// EFFECTIVE MATCH` header): (1) the find-first-matching-node loop is PEELED/rotated — /O2
  duplicates the cooldown load (peeled iter-0 copy + rotated bottom copy) where the original keeps
  one shared loop-top load; reproduced identically by do-while+goto, while, and for(;;)+break, and
  it cascades into the node==0 exit routing (direct-to-epilogue vs through the after-merge's
  redundant re-test) and a symmetric ecx/edx/eax swap in the unlink block. (2) The
  `aProviderSlots[owner].providerId == 0` check: the original materializes the slot address via
  `lea` behind a DEAD `test/jl -> null` signed-index guard (MSVC's non-pow2 `&member_array[i]`
  lowering) then derefs; my compile proves the unsigned-char index non-negative and folds to a
  direct load. Levers tried and inert: `&arr[i]`/pointer-cache/`(int)`-cast for the guard;
  do-while/while/for+goto, head/prev hoist, and fresh-field-reread for the peel. Steerable wins
  already applied: early head/prev hoist (fixed the head-read timing + prev register), 16-bit
  `Unk0x3a` push (callee short param, Yoda #14), i-before-ppSlot loop-local order.

- `DPlaySessionMgr::AttemptQueuedTrainPlacement` (0x43e1d0, v239, `asmscore.py --len 264`
  total 56456, cc.sh DIFF 134, insns 79/86) — resolves a popped pending-train node's board
  edge/quadrant from its heading and hands it to the slot registry, `src/DPlaySessionMgr.cpp`.
  Structure fully matches (state `switch` gate, all 4 NetSessionEventQueue edge-helper calls
  with the ecx=singleton load, shared `PeerTrainSlotQueueMaybe::ClaimSlotForTrain` call, esi=dir/edi=
  pNode register allocation). Residuals are the intrinsic reg-alloc / instruction-selection family
  (Yoda #29/#30): (1) per-edge-call scratch-register scheduling (original leaves the compute-result
  read in ecx/edx and interleaves `mov esi,<quadrant>` before / `mov ecx,<singleton>` after the
  push; my compile picks eax + the opposite interleave — source-inert ordering); (2) the
  connecting-mode branch pushes its own literal-0 quadrant+coord then joins only at `push pNode`,
  while my compile shares those pushes (dir=0 there feeds the same `push esi`) — COUPLED to the
  dir-as-quadrant reuse that wins the register allocation, so accepted; (3) `if (place()==0) return
  0; return 1;` compiles to a branchless `setne al` here vs. the original's `test; jne; mov al,1`
  branch (condition inversion + result-in-local both tried, neither reproduced the branch). Key
  steerable modeling that landed the structure is in-source: state `switch` not `&&` chain, `int`
  dir (no re-mask), dir reused as quadrant, edge helpers return Pair16 by value through an explicit
  `tmp` field-wise-copied into `coord` (stops connecting-mode call specialization + reproduces the
  tmp->coord word materialization).

- Roster placement-result group (v242, rewritten v360, `src/DPlaySessionMgr.cpp`) — the host-only
  (connectionMode==2) per-provider-slot linked list of `GameNetRosterResultNode`
  (id/x/y/ownerA/slotKey/next, 0x14 bytes), filed onto `aProviderSlots[bOwnerB].pResultsChainHead`.
  **The "dead signed-index guard" blamed here for 100+ sessions was NOT intrinsic** — it is the body
  of the implicitly inline `DPlaySessionMgr::ProviderSlotAt(int i) { return i >= 0 ? &aProviderSlots[i]
  : 0; }` accessor (see docs/CODEGEN.md for the full lever, including why the ternary must be spelled
  `i >= 0 ? ptr : 0` and why the same accessor shows up branchless as `setl; dec; and` inside loops).
  The v242 "confirmed unreproducible" verdict was testing the wrong axis (the SUBSCRIPT) and its
  SetMode counter-example was simply a site that doesn't call the accessor.
  - `FindTrainPlacementResult` (0x440750) — **EXACT as of v360** (196 B). Routing all three phases
    through `ProviderSlotAt` plus the original's own null-check pattern (checked on the first two
    lookups, dereferenced straight through on the third) closed it outright.
  - `SetTrainPlacementResult` (0x440610) — score 149957 -> **24012**, insns 115/113, reg_pen=0,
    identity_miss=0. Three levers landed: the accessor; the unlink's `prev != 0` arm written FIRST
    (so the prev==0 arm goes out of line); and a NULL check wrapping the whole create path. Sole
    residual: this compile widens the `oldSlotKey` byte to int THROUGH MEMORY
    (`mov [esp+0x24],cl` / `mov ecx,[esp+0x24]`) where the original masks in register
    (`mov cl,[eax+0xd]` … `and ecx,0xff`). Probed and refuted: `& 0xff` on the already-byte local
    (no-op, byte-identical); declaring it `unsigned int` (far worse, 88140 — turns the original's
    byte `cmp cl,bl` into a dword compare).
  - `RemoveTrainPlacementResult` (0x4404c0) — cc.sh DIFF 275 -> **174**, insns 134/135. The accessor
    supplied the guards and aligned the instruction counts. Remaining is register allocation: the
    original keeps the SLOT pointer in esi and reaches the chain head as `[esi+0x38]`, while this
    compile strength-reduces to the FIELD address (`lea esi,[eax+0x38]`), and the original spills the
    masked index to a stack slot.
- `GameNet_BroadcastRosterTick` (0x43ded0, v243, cc.sh DIFF ~248, insns 92/98, `src/DPlaySessionMgr.cpp`)
  — the msg-0x3f6 roster/placement-tick broadcast (fires every retryCountMaybe ticks): packs each
  `GameNetRosterResultNode` in the selected slot's chain into an 8-byte `RosterTickRecord` and posts the
  list unreliably. Structure fully faithful (record-packing loop, header stores, enqueue tail all align).
  Two intrinsic residuals: (1) the peer-slot find-first scan is a two-exit mid-exit loop (match test at
  top, advance+bound at bottom) — /O2 declines to duplicate the exhausted-return epilogue inline the way
  the original does (original: fall through to a 6-insn copy of the epilogue; mine: tail-merge that return
  into the shared far epilogue + `jmp` back to loop top), shifting every downstream offset by a constant;
  same block-layout class as the sibling drain loop 0x43e010 (Yoda #15/#18). (2) A 1-insn register
  tie-break on the tick: original computes `++tick` in ecx and copies to eax for the idiv dividend, mine
  computes it in eax directly (no copy) — symmetric-swap class (Yoda #29/#30). Levers already applied
  (banked, not residual): caching `count` in a local, hoisting `selIdx` to a top-of-function local for the
  scan, the `unsigned int idx` dword-load temp for the bProviderIndex byte store, and computing payloadLen
  before the enqueue-node alloc.

- `DPlaySessionMgr::ReconcileCarHandoff` (0x4408b0, v243, cc.sh DIFF ~218, insns 156/153,
  `src/DPlaySessionMgr.cpp`) — reconcile a train's car composition when it changes hands: bank each
  locally-owned car's card to the PostBag + clear its net-state, then sync every car's 0x1870<->0x1871
  hand-off socket state, then push a UI-mode change. Structure fully faithful — the inlined strcmp,
  all three car-slot passes, and the final UI-mode block byte-align. Sole residual: pass 2 (the
  find-first-with-break "any car still has detail" scan) — /O2 peels its first iteration's pointer load
  in my compile (`mov ecx,[node+0x14]` + `lea edi` + `ja exit` shell) vs the original's clean
  `lea edi; do{ mov ecx,[edi]; call; jne found } while(jbe)`. The v240 `for`-with-break rotation fix
  works on the loop in isolation but, as the 2nd of 4 car-slot loops sharing pNode in ebp, it spills
  pNode to the stack and desyncs the whole allocation (reg_pen 1->28, far worse); the `if + do-while`
  form is kept. Two-exit mid-exit peel class (Yoda #15; cf. drain 0x43e010). Levers already applied:
  the loop entry guards written `if (i <= count)` (reuses the counter register: `cmp count,i-reg; jb`)
  rather than `if (count != 0)`, and `unsigned int` counters for the `jbe` compares.

- **`DPlaySessionMgr::GameNetManager_HandleQueuedEvent` (0x43f2b0, v246, DIFF ~250)** — the ~18-case
  inbound-event jump-table dispatcher; all 156 case bodies structurally present. Two intrinsic residuals:
  (1) a symmetric `this` register swap — the original keeps `this` in ebx and the case-2 zero-constant in
  ebp; this compile flips them (this=ebp, zero=ebx), a pervasive ~40-instruction ebp-vs-ebx ModRM byte diff
  of identical length (Yoda #29/#30). (2) case 0x18's mid-exit find-first scan: /O2 rotates the loop and
  tail-merges its null-exit `return` into the default epilogue (reordering the found-block + cases 0x1a/0x1c
  after it); the original keeps a dedicated inline null-exit epilogue. for/while/do-while forms compile
  byte-identical — same class as `GameNet_DrainPendingTrainQueue` (0x43e010). Levers already applied:
  case-5 `switch(connectionMode)` (subtract-chain + far ==0 block), and NOT caching case-0x16's slot index in
  a local (re-read so it stays scratch, freeing a callee-saved reg — caching it widened the frame 4 bytes and
  worsened the swap cascade).
- **`GameNet_RemoveSourcePeerAndReconcile` (0x43f880, v248, DIFF 66, insns 60/60, `src/DPlaySessionMgr.cpp`)** —
  event-0xc leaf handler (removes the queueSourceId peer from all provider slots, then reconciles by
  connectionMode). Fully structural: the slot-clear loop, queueSourceId reset, and the compare-chain
  `switch(connectionMode)` dispatch byte-align exactly (prefix through offset 0x52 identical). Sole residual is a
  symmetric eax↔ecx register rotation across every `g_pSplashWnd` scratch load in the case bodies — the original
  reuses the just-freed eax (return reg of the preceding call / the switch-key reg), this compile grabs ecx
  instead (Yoda #29/#30). Confirmed inert: case reorder (v245 compare-chain layout is source-order-inert) and the
  faithful no-cache re-load shape (caching pApplSetupWnd would break the structural match anyway).

- **`DPlaySessionMgr::RemovePeerTrainsAndSlot` (0x43f940, v249, DIFF 432, insns 169/165, `src/DPlaySessionMgr.cpp`)** —
  event-0xb leaf handler (a peer identified by destPlayerId departed: release its board train slots, purge its
  pending-train-queue nodes + placement-result nodes, clear+reload its provider slot, repaint). The entire clear
  block, `LayoutSet_LoadSlotBitmap` call, result-chain drain, and the IsWindowVisible repaint tail byte-align
  EXACTLY — all diff is concentrated in the two provider-slot find-loops + the two list-walk unlinks. Three
  documented-intrinsic residuals: (1) loop-1's match/"found" store is DEFERRED by /O2 to a cold block after the
  `ret` (this compile keeps it inline) — trace-driven block layout (Yoda #15), which shifts every later address and
  inflates the raw byte_diff even though the instructions align. (2) both find-loops PEEL here (slot[0] load hoisted)
  while the original keeps the match test at the loop top — the original redundantly RE-READS destPlayerId from its
  stack slot every iteration, a /O2 quirk on a value param that isn't reproducible from source (a param compiled to a
  live register can't be forced back to a per-iteration memory reload). (3) a pervasive counter/pointer register swap
  (Yoda #29/#30). Levers that LANDED the structure (299590→161973 asmscore): two-variable-split of the loop counter
  from the result (freeing ebp for the whole-function 0-constant, so the result spills to a stack home like the
  original), the do-while-with-`goto` shape for BOTH find-loops, and single-variable (advance-in-place) list walks
  instead of a two-variable `while((cur=next))` form (killed 3 `mov ecx,eax` copies, reg_pen 38→17).

- **`DPlaySessionMgr::ReleaseOwnerTrainsAndBroadcast` (0x43fb50, v250, DIFF 175, asmscore total 36013,
  insns 87/83, `src/DPlaySessionMgr.cpp`)** — event-0x1a leaf handler (the departing owner id rides in the event
  node's `pPayload` slot reused as an int: release its board train slots, drop its one pending-train-queue node,
  then `GameNet_BroadcastLocalOrigin`-broadcast+remove every placement-result node it owns on the selected
  provider slot, repaint). The ReleaseSlotsForOwner call, unlink/delete block, result-chain broadcast loop, and
  IsWindowVisible repaint tail align EXACTLY. Sole structural residual: the pending-train find-first scan — /O2
  PEELS/rotates this two-exit field-read loop (duplicating the match test at the bottom rather than looping back
  to a single top test) identically for the do-while, `goto`, and plain-`while` forms (all three compile
  byte-identical at 36013) — the intrinsic drain-0x43e010 mid-exit rotation class (Yoda #15). The peel shifts every
  later instruction's offset (the align cascade that inflates the raw byte_diff), plus one extra `xor edx` that
  materializes the bOwnerB zero-extend the original folds into the compare's `and edx,0xff` (a register-scheduling
  tie-break under the peel's register pressure). Lever that LANDED the structure (46908→36013): swapping the unlink
  branch order to `if (pPrev != 0) pPrev->pNext = ...; else head = ...;` (the original's `test eax,eax; je` puts the
  `pPrev != 0` case on the fall-through — reg_pen 8→0, identity_miss 8→0).

- **`DPlaySessionMgr::ApplyProviderSnapshot` (0x43fc50, v251, DIFF 352, asmscore total 300962, insns 152/151,
  `src/DPlaySessionMgr.cpp`)** — event-9 leaf handler: reconcile all 9 provider slots against the peer's incoming
  snapshot (`pMsg->pPayload` is an array of 9 `DPlaySessionMgrProviderSlot` records), broadcast our own selected
  slot's enable/disable transitions, refresh/reload bitmaps for emptied or changed slots, reply with our stored
  layout if it diverged, then repaint the application-setup window. The whole post-loop tail (the
  `LayoutNet_PostSimpleOpcode(0)`/`LayoutNet_ReplyWithStoredLayout`/`FUN_0040a4a0` calls and the
  `IsWindowVisible` repaint block) byte-ALIGNS exactly — every diff row there is a bare register swap. Sole
  structural residual: the 9-slot reconcile loop's induction-variable representation. The original re-reads
  `pMsg->pPayload` inline at each of the 3 source accesses (proving it is NOT source-cached) yet derives the
  source's `i*0x4c` scaled offset from the DESTINATION induction pointer `esi` — spilling the invariant
  `-this-0x518` (`mov ebx,0xfffffae8; sub ebx,edi`) and adding `esi` per iteration — instead of maintaining a
  separate `i*0x4c` stride IV. The faithful inline form (`((DPlaySessionMgrProviderSlot*)pMsg->pPayload)[i]`)
  re-reads correctly but the compiler maintains its own `i*0x4c` IV (one extra `add eax,0x4c`, 152 vs 151 insns),
  cascading a single register reassignment through the loop body's ~30 `mov`/`cmp` rows (the whole byte_diff).
  Intrinsic /O2 IV-elimination tie-break — direct `arr[i]` subscripting was WORSE (312948, 156 insns, extra IVs)
  and caching `Slot *pSrcArray = pMsg->pPayload` DROPPED the original's faithful re-reads (305965, 147 insns), so
  the inline form is both closest and the only one matching the original's memory-access pattern.

- **`GameNetThreadState::GameNet_HandleSelfStateRotate` (0x43a4b0, v255, DIFF 134, asmscore total 74372,
  insns 87/88, `src/GameNet.cpp`)** — inbound "rotate self train 180deg" handler: post a type-0x17 notify, then
  (if the message targets the local player's own provider slot) find the matching active train, unlink it, flip
  its heading a half turn (0<->180deg, 90<->270deg) via a 4-case switch, re-stamp its secondary owner byte, and
  post it as a type-0x11 notify. Sole structural residual: /O2 PEELS the find-first search loop. The original is
  a clean 2-exit mid-exit loop (match-test at top with a forward `je` to the match body, null-check at bottom
  `jne` back to the top-test, no-match `return` kept INLINE as its own `pop x4; ret 8` at 0x525). Every recompile
  instead peels a duplicated match test to the loop bottom and tail-merges the no-match return into the shared
  function-end epilogue (`je 0x107`) -- shifting all downstream offsets ~4 bytes (the whole align=74 cascade) and
  swapping two registers in the match body. Confirmed intrinsic: `while` / `for(;;){if(match)break;...}` /
  `do{if(match)goto;...}while(non-null)` all compile byte-identically to the peeled form -- same class as
  `GameNet_DrainPendingTrainQueue` (0x43e010). Levers already applied: the v236 byte-narrowing fix (an
  `unsigned int nSelfSlot` temp read before `pTrain->bOwnerByteB = (uchar)nSelfSlot`, so the
  `selectedProviderIndex` read is a full dword `mov eax,[eax+0x7d0]` not a byte load -- 78374->74372).

- **`GameNetThreadState::TrainNet_HandleMoveRequest` (0x43ad00, v257, DIFF 237, asmscore total 156727,
  insns 108/116, `src/GameNet.cpp`)** — local "move my train one board slot in heading D" command handler: if
  not host (connectionMode != 2) or the train is flagged release-outright (+0x4 == 1), delete it and clear the
  payload; else map the heading (0/90/180/270deg) to the neighbouring provider slot from our own selected slot,
  and either hand it off to the peer owning that slot (4-arg GameNet_SendTrainStateSyncMaybe, trailing literal 0)
  + park-on-awaiting-ack-tail-and-drain on failure, or defer to TrainNet_HandleEmptySlotHandoffMaybe for an empty slot. Identical CFG,
  correct arg counts, correct control flow. Sole residual is a whole-function register-residency tie-break: the
  original SPILLS `this` to a reused parameter stack slot (`push ecx` / `mov [esp+0xc],ecx`, reloaded at every
  call site) and keeps the manager pointer in callee-saved edi; my allocation keeps `this` in callee-saved ebx
  and the manager in scratch ecx (equivalent, in fact 8 insns shorter -- the missing `push ecx`/4x `pop ecx`
  scaffolding). Confirmed intrinsic: the original leaves ebp entirely FREE yet still chooses the stack spill, so
  it is not register pressure; reordering the mgr-cache declaration only worsened it (156727->173753). The swap
  cascades a register-byte difference through every this/mgr reference (hence the large raw byte_diff) plus a lone
  `ja` vs `jne` (unsigned !=0 lowering). Levers already applied that mattered: caching the manager pointer in a
  local (a single `mov edi,[0x4fd3ac]` load vs per-access reloads, 257879->156727) and flipping the empty-slot
  guard to `if (providerId == 0) { FUN; return; }` so the peer path is laid out-of-line (matches the original's
  `ja`-to-peer layout). Yoda #29/#30.

- `DPlaySessionMgr::TrainNet_PostMoveRequestForNode` (0x43eec0, v260, DIFF(128), `asmscore.py
  --len 212` total 10892, insns 68/68) — `src/DPlaySessionMgr.cpp`. A this-ignoring thiscall member
  that picks a hand-off heading from (fromSlot,toSlot), probes it plus the 0/270/90 fallbacks for
  provider-slot room, and posts a type-0x10 move-request node. Structurally IDENTICAL (68/68 insns);
  the sole residual is a symmetric-register-swap (Yoda #29/#30): /O2 colors the two arg loads
  `toSlot->eax / fromSlot->ecx` where the original picks `toSlot->ecx / fromSlot->eax`, cascading into
  the `cmp` operand order and the branchless-select `setle` vs `setg` polarity on the `fromSlot<=toSlot`
  ternary. Confirmed unsteerable — comparison direction (`fromSlot<=toSlot` vs `toSlot>=fromSlot`) and
  branch-nesting order do not move the first-load register color (both probed, neither flips it).


- `GameNetThread_TickLoop` (0x439240, v262, cc.sh DIFF(204) but `asmscore.py --len 394` byte_diff 22,
  insns 125/129) — `src/GameNet.cpp`. The GameNet worker thread's infinite do-while tick loop
  (dispatch inbound / drain local-command queue / advance trains or age hand-off timers + Sleep;
  reentrant-dispatch path flushes+disposes the send queue and exits). Structure 100% correct and ALL
  register ROLES match the original: ebx=0-const, edi=this, esi=scratch-node, and /O2 correctly HOISTS
  the Sleep IAT slot into callee-saved ebp (`call ebp`). The entire residual is THREE redundant pointer
  null-checks that a dominating branch already proved non-null, which the original binary KEEPS
  (`cmp reg,ebx; je`) but our cl 11.00 FOLDS via redundant-branch elimination: (a) the inner-drain
  loop's `if (pNode == 0)` recheck after `delete pNode`; (b) the pending-train `while (pTrain != 0)`
  rotation guard, redundant with its enclosing `if (pTrain != 0 && ...)`; (c) the dispose-case
  `delete (NetMsgPayloadObjMaybe*)pPayload`'s own null-check, redundant with the enclosing
  `if (pPayload != 0)`. Confirmed unsteerable — `delete pNode` vs `operator delete(pNode)`, a split
  vs `||` inner tail, and `for` vs `while` rotation ALL fold identically. The type-value register
  (edx vs eax) and the `mov al,[edi+0xd]` materialization of bDispatchBusy are downstream artifacts of
  fold (a). Same redundant-check-elimination-strength difference class as the peel lessons, inverted
  (fold removes a check vs peel adds one). Yoda #29/#30.

- `GameNetThreadState::GameNet_ConnectOrJoinSession` (0x43c410, v274, `asmscore.py --len
  1082` byte_diff 90, insns 326/325) — `src/GameNet.cpp`. The session-connect-or-join bootstrap
  (opcode 0xe handler): append/drain the pending-train list, create `g_pNetManager`, teardown +
  init + `DPlay_JoinOrHostSession` with one retry, and on total failure post a type-0x1c notify
  plus register a placeholder solo player card. Structurally 100% correct — every branch, loop
  shape, and field access matches; the residual is 3 small intrinsic scheduling artifacts: (a)
  the `g_pNetManager` creation section's `local_4=-1` (/GX alloc-protection reset) lands one
  field-init statement earlier than the original among 3 back-to-back writes (same class as the
  `DSound_InitDeviceAndChannelPool` SEH-placement lessons — invisible compiler bookkeeping, not
  source-steerable via statement reordering, tried); (b) the already-joined "broadcast roster,
  return" branch: a `goto`-to-end-label restructure (matching the original's out-of-line tail
  placement) had no effect — the compiler just inlines the 1-call target at the goto site
  instead of jumping there, since the target is trivially small (Yoda #15, block layout is
  trace-driven); (c) `AddDecalMaybe`'s two literal args (0x63/0x94) route through intermediate
  registers in the original vs. direct immediate pushes here, a minor register-scheduling
  tie-break. Confirmed via `asmscore.py --dump`: all 3 are isolated 2-6 instruction pockets: the
  rest of the 325-insn function aligns exactly.

- `GNetManager::DPlay_FindSession` (0x45f090, v275, `asmscore.py --len 531` byte_diff 97, insns
  178/177) — `src/GameNet.cpp`. Re-enumerates DirectPlay sessions via `IDirectPlay4::
  EnumSessions`. Structurally 100% correct (every branch, loop, and call site aligned; calling
  convention confirmed correct via a first attempt that used plain `__thiscall` probe methods and
  silently dropped the COM `this` push — fixed by making `IDirectPlay4VtblProbe`'s methods
  `__stdcall`, matching STDMETHODCALLTYPE). Two intrinsic residual classes: (a) the password-
  truncate branch's saved byte (`src[0x80]`, restored after the copy) lands in ECX here vs. EBX
  in the original — ECX collides with `strcpy`'s own `rep movs` count register, forcing a
  spill/reload pair around the call; tried hoisting the local's declaration to function-top
  (SEH-placement-order lever), zero effect. This single register choice cascades into a broad
  register-swap ripple through most of the rest of the function (dozens of `r`-marked reg-only
  diffs), inflating `align`/`reg_pen` far more than the 2 genuinely extra spill instructions
  alone would suggest — likely the Yoda #29/#30 short-lived-value tie-break family. (b) the
  original keeps 3 separate physical epilogues (initial-failure tail, loop-exit-success tail,
  bJoinAttempted-early-exit tail) while this compiles the initial-failure and loop-exit-success
  tails (both a bare `return pListHead1;`) into ONE shared tail; tried both a nested-if-
  wrapping-the-loop shape and an early-return-then-unconditional-loop shape (logically identical,
  differ only in whether the loop is lexically inside an `if`) — byte-identical output either
  way, confirming a self-contained block-layout/cross-jump choice (Yoda #15/#18), not source-
  steerable. Confirmed via `asmscore.py --dump` + a direct disasm read of the compiled COMDAT.

- `GNetManager::DPlay_ReceiveAndDispatch` (0x4606d0, v293, `asmscore.py --len 1648` total
  733520, align=728 reg_pen=46 byte_diff=460, insns 605/603) — `src/GameNet.cpp`. Down from
  v292's 793556/1289134-original via the block-order fix already landed (see the in-source plate
  comment). Residual is PURELY the physical placement order of 2 cold blocks: original emits
  `[switch][switch-tail][ack-check][hrLastResult!=0 handler][generic-message][switch's
  pBuiltNode!=0 wrapper-return]`, ours emits `[switch][switch-tail][hrLastResult!=0
  handler][ack-check][generic-message][wrapper-return]` — i.e. the ack-check and hrLastResult
  handler blocks are swapped, everything else (including both blocks' own internal content)
  byte-identical. Confirmed via a full linear capstone disasm of the compiled COMDAT
  (`tools/match.py`'s `coff_functions`/`trim_pad`, dumped instruction-by-instruction) cross-read
  against the ORIGINAL's raw `objdump`, not just `asmscore.py --dump`'s LCS-realigned view.
  Three restructurings tried this session, none net-positive: (1) wrapping the whole
  pDpidFrom/switch/ack/generic body in `if (hrLastResult == 0) {...} else { <handler moved to
  the very end> }` DID move the handler block to the end (total dropped to 519398) but as a side
  effect made the compiler TAIL-MERGE the unrelated top-of-function `if (pDirectPlay4==0) return
  0;` early-return with the loop's own return-0 paths — regressing a previously byte-EXACT
  6-instruction region (its own dedicated `xor eax,eax;pop...;ret` epilogue) into a shared/absent
  block. Rejected: net token trade is not worth breaking an exact region. (2) Replacing the
  structured `if (hrLastResult != 0) {...}` with `goto recv_error;` to a label placed at the very
  end of the loop body (after the generic-message return, requiring the generic branch's
  `pWrapper` local to be un-initialized-at-declaration per the documented VC5 for-scope/goto
  `C2362` gotcha) compiled BYTE-IDENTICAL to the un-refactored baseline — zero effect, confirming
  this specific goto-vs-structured-if rewrite doesn't touch block ordering. (3) Converting the
  two sibling top-level `if (hrLastResult != 0) {...}` / `if (*pDpidFrom != 0) {...} else
  {switch}` statements into one `if (hrLastResult != 0) {...} else if (*pDpidFrom != 0) {...}
  else {switch}` else-if chain (same textual position, no relocation) — also byte-identical, zero
  effect. This function has 6+ distinct cold blocks (the largest cold-block count of any parked
  residual in this codebase) and the swap doesn't correlate with any simple encounter-order
  theory tested by hand (neither a LIFO-stack nor FIFO-queue model of "cold blocks queued as
  their branch instruction is scanned in the hot path" predicts "switch first, then ack, then
  hrLastResult" for the original, nor any variant tried). Matches the Yoda #15 "block layout is
  trace-driven and mostly NOT source-steerable" class at a scale that makes further hand-probing
  low-ROI without a decompiler-driven trace. Retry idea if revisited: instrument/trace MSVC's own
  actual block-scheduling pass (not available in this toolchain) or try restructuring the SWITCH
  itself (e.g. reordering which case is written first) in case it changes which cold blocks are
  discovered/queued during the switch's own nested resolution — untested this session.

- `ApplSetupWndPartial::OnLButtonDown` (0x40a4e0, v300, `asmscore.py --len 1339`, DIFF 804/1339)
  — the ApplSetupWnd WM_LBUTTONDOWN dispatch (5 ResourceRef button rects + an easter-egg sound
  rect + LIST-mode/GRID-mode provider-list click regions), fully transcribed, structurally
  complete: every branch, field access, and call verified 1:1 against the raw disasm (closed 3
  sibling functions to EXACT in the same session — `AbortToDisconnectedStateMaybe`/
  `SelectGridCellFromPointMaybe`/`ApplySecondaryListSelectionMaybe`, see `src/DPlaySessionMgr.cpp`
  — proving the shared struct layout/field names are correct). The residual: once the first
  virtual call (`ScheduleModeTransitionMaybe`, vtbl+0x10) is reached, register allocation
  diverges from the original's — the original keeps lParam's x/y pinned in edi/ebp for the
  WHOLE function and issues a direct `call [reg+0x10]` at each vtable dispatch; this compile
  spills the (x, y) point to a stack-resident local and materializes the vtable slot's target
  into a register (`mov edi,[edx+0x10]; call edi`) before calling. Tried: a `POINT pt` local
  (current) vs separate `int x, y` locals (AlbumCardWnd::OnLButtonDown's own precedent shape);
  the wrapper-method call form (`ScheduleModeTransitionMaybe(...)`) vs a direct
  `((ApplSetupWndVtblProbe*)this)->ScheduleModeTransition(...)` cast at each site (`ApplSetupWndVtblProbe`
  was since retired repo-wide 2026-07-21 — `ScheduleModeTransition` is now a real `virtual` on
  `ApplSetupWndPartial`/`SplashWndPartial`, and the still-used `ScheduleModeTransitionMaybe(...)`
  wrapper itself just forwards to `this->ScheduleModeTransition(...)`) — the latter
  pair compiled to IDENTICAL bytes (same length, same diff), proving the wrapper indirection
  itself isn't the cause. Same class as the already-parked `AlbumCardWnd::OnLButtonDown`
  (DIFF 954/1324) and `EditCardWnd::HandleLButtonDownMaybe` (0x41ac10) — this project's 3rd
  large multi-region WM_LBUTTONDOWN dispatcher to hit this same residual ceiling, reinforcing
  that the class as a whole resists full byte-match under this toolchain rather than any one
  function having a fixable bug. Retry idea if revisited: instrument the exact point register
  (edi/ebp) allocation decision — try declaring x/y as the FIRST two locals before any other
  statement (matching the original's own very-first `mov ebp,[esp+0x20]` extraction), or try
  building each PtInRect call's POINT inline from raw lParam bit-ops at every call site instead
  of a single cached local (per Yoda's "per-statement reload = no caching local" aliasing
  lesson) — untested this session.

- `ApplSetupWndPartial::SendSelectRequestMaybe` (0x40ac50, v301, `asmscore.py --dump`,
  DIFF 4/324, insns 99/99, align=0) — EFFECTIVE MATCH, symmetric ecx/edx register-role swap
  (Yoda #29/#30) across the tail 4-instruction sequence (`destPlayerId` read from
  `g_pGameNetThreadState->nQueueSourceId`, the `bReliable`/`destPlayerId` stores, and the
  `g_pDPlaySessionMgr->bLayoutSyncPingSent = 0` clear): the original reuses ONE register (ecx)
  as both the source pointer AND the destination for the loaded dword (`mov ecx,[ecx+0x10]`),
  while this compile keeps them in separate registers (`mov edx,[ecx+0x10]`) — cascading into a
  full ecx/edx role swap for the rest of the tail. Tried: an `int destId = ...;` temp declared
  immediately before the `bReliable`/`destPlayerId` stores (matches the original's own
  read-before-store statement order, closest — DIFF 4, kept) vs. a cached
  `GameNetMsgQueue *pQueue = g_pGameNetThreadState;` pointer local (scored worse, DIFF 14).
  Same class as the already-documented symmetric-swap residuals; not worth further probing at
  this budget.

- `ApplSetupWndPartial::LoadIndexFileMaybe` (0x409e70, v301, `src/DPlaySessionMgr.cpp`,
  DIFF 548/699) — PARKED, structurally complete and semantically verified 1:1 against the raw
  disasm. Same still-open toolchain-level residual class already parked on
  `Wav_ParseAndLoad`/`Wav_ReadOrFindChunk` (`src/Wav.cpp`) after 4+ dedicated sessions there:
  constructing a real `istream`-hierarchy class (`istrstream`/`ifstream`) via `new` under this
  toolchain compiles to a genuine EBP-framed, multi-entry SEH scope table in the ORIGINAL (its
  own unique unwind thunk at 0x475096, with distinct entries tail-calling the shared
  scalar-deleting-dtor stub at 0x436a00 for base-subobject cleanup), while cl 11.00 here
  compiles the equivalent source to the simpler ESP-relative single-state-variable automatic
  scaffolding (the "declared-only ctor" precedent's shape, which works fine for POD/no-base
  classes like `ProviderListNode` but not for real multi-base iostream classes). Confirmed
  inert this session: local declaration order/position (top-of-function vs. point-of-use),
  unifying vs. splitting the istrstream/ifstream pointers into one `istream*`, goto-based
  single-exit vs. nested-loop `return`. Untested: an explicit `try { throw ...; } catch (...)
  { throw; }` re-throw shim around the two throw sites — `Wav_ParseAndLoad`'s own v92 fix
  needed a REAL user try/catch to materialize its 3rd SEH state, but `LoadIndexFileMaybe` has
  no local catch funclet (exceptions genuinely propagate to the caller here), so it's unclear
  whether an artificial catch-and-rethrow would even be faithful, let alone fix the shape —
  worth one probe in a future session focused on iostream-construction SEH shapes generally
  (would also potentially unlock `Wav_ParseAndLoad`'s own remaining residual if solved).

- `DPlaySessionMgr::LayoutSet_InitFromConfigFileMaybe` (0x43d820, v302, `src/DPlaySessionMgr.cpp`,
  DIFF 724/992) — PARKED, structurally complete and verified against the raw disasm (stream
  fallback, header/line parsing, 32-byte truncation clamp, count/cols/rows validation cascade —
  transcribed as Ghidra's own nested-if-after-switch shape to mirror the jump-table fallthrough
  literally — and the `catch (...)` -> shared-cleanup tail all confirmed). Same open
  toolchain-level SEH-scaffolding class as `LoadIndexFileMaybe`/`Wav_ParseAndLoad`: `new
  istrstream(...)`/`new ifstream(...)` under this toolchain doesn't reproduce the original's
  richer multi-entry SEH scope table. This is the 3rd sibling to hit the identical shape,
  closing the `SplashWndMaybe.h`/`DPlaySessionMgr` LayoutSet declared-only cluster entirely —
  all remaining members are now either EXACT or PARKED on this one well-understood residual.
  Unlike `LoadIndexFileMaybe`, this function DOES have a real local `catch (...)`
  (`Catch@0043dba7`) that sets a flag (reusing the dead `pConfigLine` stack slot's high 3 bytes,
  `[ebp+0xb]`) then falls into the shared cleanup — confirmed this doesn't change the underlying
  SEH-shape gap (the `--dump` diff is dominated by the same missing register-tracking
  instructions around the RF/ifstream construction, not anything catch-related). Still untested
  project-wide: whether ANY source shape reproduces the original's richer SEH scope table for a
  real (non-POD-base) iostream `new` — treat as one unsolved question shared by all 3 parked
  sites, not 3 separate problems.

- `MenuNodeObj0x477568::SetNodeState` (0x40d170, `src/MenuNode.cpp`, `asmscore.py --len 276`:
  total 164644, align=160 reg_pen=41 byte_diff=94, insns 99/91) — PARKED. Modes 1-3 all
  dispatch through the SAME vtable slot (6, `SetFrameAndNotify`); the compiler CSEs the vtable
  pointer load across all 3 call sites (loads `*this` once, reuses it) while the original
  reloads `*this` fresh at each of the 3 sites. Tried and confirmed IDENTICAL score: caching
  `pIconDesc->nButtonFrameCount` in a local, replacing `break` with early `return` per case
  (matching the original's own independent-leaf-per-case block shape), and both together.
  The `bValid == true` literal comparison (vs. a bare `if (bValid)`) WAS a real fix (dropped
  total from 190658 to 164644) -- keep that half. Remainder reads as an intrinsic cross-case
  CSE, extending the Yoda #29/#30 symmetric-register-swap family to a shared-vtable-slot
  dispatch across sibling switch cases. Untested retry idea: `goto`-based case bodies with the
  vtable call wrapped in a helper the optimizer can't see through (would need a non-inlined
  boundary, awkward for a single-TU function).

- `MenuNodeObj0x477568::Draw` (0x40d340, `src/MenuNode.cpp`, `asmscore.py --len 290`: total
  97554, align=94 reg_pen=30 byte_diff=84, insns 97/96) — PARKED. The original loads
  `this->rect`'s 4 fields via ONE `lea`-computed address held in a single register (edx),
  dereferenced 4 times (`[edx]`/`[edx+4]`/`[edx+8]`/`[edx+0xc]`), then keeps all 4 field
  values purely in registers (eax/ebx/ecx/ebp) through the entire carousel-adjust branch,
  only spilling to the local RECT's real stack slot at the post-branch join point (right
  before `pOwner` is re-read fresh for the viewport-check fields). 5 source forms tried for
  the initial load, all scored worse than or equal to the winning form: plain struct-copy
  assignment (`RECT localRect = rect;` — the kept form, 116 total diff), field-by-field
  assignment (217), an explicit `RECT *pRect = &rect;` pointer (compiler folds the redundant
  address computation away identically to a bare field read — refutes the address-of/aliasing
  hypothesis for this toolchain), an explicit `CopyRect(&localRect, &rect)` call (243), and
  hoisting the 4 rect fields to standalone `int` locals read via `this->` (229 — reproduces
  the original's register-only residency THROUGH the carousel branch but loses the
  single-address-then-4-derefs load shape at the very top, netting an overall WORSE
  structural diff than the plain struct copy). Every later register-role swap (eax<->edx
  throughout the viewport check) and 3 downstream `cmp`/`jcc` operand-order flips cascade
  from this one unresolved root. Untested retry idea: none identified — the "cache the field
  address once, defer the stack spill past an unrelated branch" idiom didn't reproduce from
  any tried source shape; may need a genuinely different angle (e.g. a helper function
  boundary) if revisited.

- ~~`??_GThreadWrapper` scalar deleting dtor~~ **CLOSED v495**: the retry condition
  (`FUN_0045c3c0` transcribed) was met by `src/FrameDriver.cpp`; `~ThreadWrapper` moved
  in-class (src/ThreadWrapper.h) and `??_G` is EXACT at 65 B. The planned "let that TU emit
  0x461690" half did NOT survive contact with reality: VC5 /O2 inlines the in-class body into
  the compiler-generated $E static-dtor thunk, so no TU emits `??1ThreadWrapper` any more and
  the 0x461690 marker was dropped (net +20 B exact: +65 for `??_G`, -45 for 0x461690). The
  original kept a standalone ??1 because its frame-driver TU evidently saw a declaration-only
  view of the class. Full trade note in `src/ThreadWrapper.cpp`.

- `??_GScopedTimestampMaybe` scalar deleting dtor (0x41f4b0, v333, first transcription,
  DIFF(22)/30B) — `src/ScopedTimestampMaybe.cpp`. Same inline-into-`??_G` class as
  `??_GThreadWrapper` above: the original inlines the trivial dtor body (vtable store inside
  the `test al,1` flag branch, then the delete); VC5 emits a CALL to the out-of-line `??1`
  (0x41f4d0, itself EXACT) instead. Moving the dtor in-class drops BOTH COMDATs in this TU
  (vtable slot 0 is `??_G` itself and nothing else forces emission), orphaning the 0x41f4d0
  marker; the original's standalone `??1` survives because the orphan block's TU
  (0x45c655..0x45c675) calls it directly. Retry only if that orphan block's owner is ever
  transcribed.

- `GameNetThreadState::TrainNet_AdvanceLocalTrainSteps` (0x43bb00, v323, first transcription,
  `asmscore.py --len 1632` byte_diff 180/1622, insns 496/500, align 110) — `src/GameNet.cpp`.
  The per-tick LOCAL train movement stepper (heading switch, board-edge handoff, stall-detect
  reroute, type-0x15/0x11 notifies + opcode-0x3f6 broadcast). Structure fully faithful — every
  instruction exists in both binaries in the same order. Residual: (1) the documented-
  unreproducible dead signed-index guard on `&aProviderSlots[bOwnerByteB]` (same intrinsic
  class as DrainPendingTrainQueue/0x43e010 and the TrainPlacementResult trio — our cl folds
  the guard for an unsigned-char index; the fused `[eax+edx*4+0x55c]` pLayoutData load is
  downstream of it); (2) register tie-breaks inside the heading switch's 4 case bodies
  (symmetric eax/ecx grid-pointer/stride role swap, Yoda #29/#30; spill-victim choice in
  cases 0x5a/0x10e). Probed without movement: nCols/nY/nX declaration orders (3 variants),
  pGrid hoisted vs per-case (per-case required for the original's reloads), idx-first vs
  pGrid-first per case, explicit switch-pivot local. No untried lever found.

- `WorldActionCursor::TestMenuCommandMaybe` (0x45a880, v323, first transcription,
  `asmscore.py --len 444` byte_diff 83, insns 151/149, align 142, reg_pen 10) and
  `WorldActionCursor::HandleMenuCommandMaybe` (0x45aa50, v323, first transcription,
  `asmscore.py --len 2235` byte_diff 766, insns 750/754, align 1576, reg_pen 68) —
  `src/WorldActionCursor.cpp` (new TU). The slot-17/20 menu-command test/execute pair.
  Structure faithful end-to-end: both jump tables with exact case→body maps, the chain
  dispatch, all case bodies/call targets/arg orders, both shared goto tails, the
  0x380e/0x3810 cross-case shared-label CFG. Residual is ONE systemic /Og global-allocation
  divergence: the original keeps constant 1 in EBX across the whole function (`mov ebx,1` +
  ~45 bl/bx-form uses) while our compile never hoists it (immediates, EBP unused), cascading
  every register role + cross-jump outcomes (our /O2 merges identical tails the original
  keeps as copies). PROVEN TU-context-sensitive (Yoda #23): prepending 2 dummy functions to
  the TU measurably shifts codegen; our cl DOES produce the ebx=1 form elsewhere with
  different region boundaries. The original TU held ~10 earlier WorldActionCursor methods.
  Retry when those siblings (esp. the ctor and RefreshTrainCouplingMenuMaybe/0x4597e0) are
  transcribed into the TU ahead of it. Probes that did not move it: `nOne` source local
  (constant-folded), +8 artificial 1-uses, 5 mini-replica TUs, if/else-if vs switch, label
  restructuring.

- `WorldActionCursor::SelectDecorObjAndDispatchModeMaybe` (0x459180, v337, first
  transcription, `asmscore.py --len 1388` byte_diff 371, insns 418/405, align 468,
  reg_pen 173) — `src/WorldActionCursor.cpp` (end of TU, per the v336 in-TU placement
  lesson). The decor-object select/deselect entry point behind the 0x3802/0x3866/0x3867
  menu commands. Structure faithful end-to-end: mode-3 prologue fixup, 4-deep &&-guard,
  the 11-entry mode jump table (modes 2..0xc) with exact case→body map (case declaration
  order 7/8/2+4/6/0xc/3 recovered from .text), the mode-8 icon do-while, all 5 inlined
  center-anim blocks (inline helper `CenterModeAnimOverWidgetMaybe`), the shared 6/0xc
  block (fallthrough + goto), the neg/sbb/and bActive ternary, both return tails. Residual
  is THREE stacked documented /Og coin-flip classes: (1) the 0x4393d0 sete-materialization
  class on `(DAT_004851f4 == 3)` mid-&&-chain (shared with the v334/v335 parks; a
  `bool bInGame` probe materialized the sete but hoisted it ahead of the null guards —
  reverted); (2) an /Og vtable-value-CSE coin-flip — ours hoists `mov ebp,[esi+0x3a0]`
  per case and calls [ebp+0x18]/[ebp+0xc], the original caches only the ADDRESS in edi and
  reloads the vtable per call, cascading the `mov ecx,edi`/`add esp,8`/arg-sum scheduling
  of all 5 center-anim tails (~half the dump rows); (3) 16-bit partial-reg coin flips
  (cx/ax vs dx, cl vs al). Levers that landed: unsigned `> 0` count guard (jbe), clamp
  operand order (`base > nIndex`), if/else (not ternary) for the categoryByte mode byte
  (the `xor r16,r16; mov r16l,al` zero-extend), the inline center-anim helper, TU-local
  probes/views for the vtbl+0x40 category-mgr slot / UIResources clock tick /
  SelectedObjWidgetMaybe bActive. A minimal-TU probe (function + decls alone, parked
  siblings removed) reproduces the IDENTICAL score — residual is INTRINSIC, not TU-context
  (unlike the 0x45a880/0x45aa50 pair above; same finding as v336's 0x43e900). Retry only
  if the 0x4393d0 sete class or the vtable-CSE coin-flip class cracks.

- `WorldActionCursor::RefreshTrainCouplingMenuMaybe` (0x4597e0, v338, first
  transcription, `asmscore.py --len 1375` byte_diff 165, insns 390/394, align 192,
  reg_pen 58) — `src/WorldActionCursor.cpp` (end of TU). The train-coupling submenu
  refresh (was the largest unclaimed APP gap). Structure faithful end-to-end: the
  toggle-dispatched SetDescriptor(0x3800/0x3801) pair, the mode6/mode3 bool pair, the
  SetLabelText source select (mode-6 locomotive `pSelectedDecorObjMaybe[8].rect.top` →
  `carSlots[0]->szCategoryName` vs the decor object's own), the attach/spawn item's
  3/2-vs-3/1 state machine (incl. the mode 8/0xc/4/2 + `[1].nTypeTag` byte-1 chain), the
  candidate-visibility trio, the 8-icon do-while, both couple-choice branches (shared
  0x8c/0xb8 and 0xa6/0x33 / 0xec/0x33 reposition coords, `IsSlotCountOutOfRangeMaybe` /
  `dwModeBMaybe == 2` state demotions, shared 4-Draw tail), the variant prev/next
  do-while, the 0x88-stride bReady walk, and the mode-3 per-car subframe loop
  (`carSlots[0..3]`, -1 on null `pKindDesc`, `(resourceId - 0x1804)/2` and
  `(resourceId - 0x1866)/2 + 1`) all match the original's CFG and instruction selection —
  the whole 0x459cd3..0x459d26 region is byte-faithful. Residual is FOUR documented /Og
  coin-flip classes: (1) cross-jump/tail-merge — the original keeps TWO full SetDescriptor
  call sequences (separate vtable reloads) and sinks the SetLabelText arg's add+push into
  each select branch; our cl merges both (probe: ptr-select and char*-select source forms
  compile identically); (2) EAX-wide vs AL-wide bool materialization at 4 sites (original
  `mov eax,1`/`xor eax,eax`, ours `mov al,1`/`xor al,al`; if/else vs &&-expression forms
  compile identically → intrinsic); (3) the 0x4393d0 sete class variant — original `sete`
  direct to `[esp+0x13]` for bMode3, ours `sete cl` + byte store; (4) the v337
  vtable-value-CSE class at the RepositionWithHotspot tail (original reloads `mov
  edx,[esi]`, ours reuses the prologue's spilled vtable). Plus reg coin-flips (edi/ecx
  pTrain residency, ebp/edi constant-3, variant-loop eax/cl/edx swaps, the original's
  pTrain re-fetch for the `dwModeBMaybe` test, `mov ecx,4` scheduling). Levers that
  landed: per-branch SetNodeState calls (cross-jump FIRES, reproducing the original's
  push-per-branch/shared-call shape — proof our cl can produce the form, strengthening
  the "single-call source merged" reading of class (1)); car-loop `pAnim =
  &animArrayMaybe[1]` init with the non-null path first. Header fields added:
  animMaybe7 (+0x4b0), pDetachMenuItemMaybe/pAttachOrSpawnMenuItemMaybe (+0x564/+0x568),
  the 4 pCoupleChoice* buttons (+0x574..+0x580), pIconStateTargetA/B/CMaybe
  (+0x5a4..+0x5ac) — all Ghidra-synced names. Retry only if the 0x4393d0 sete class or
  the v337 vtable-value-CSE class cracks.

- `WorldBoardMaybe::FUN_00456700` (0x456700, v339, first transcription,
  `asmscore.py --len 1364` byte_diff 302, insns 407/424, align 310, reg_pen 59;
  **cc.sh DIFF 1058 → 1088 / len 1299 → 1309 in v573**, when the `col = colMin` re-seed was
  moved back INSIDE the row loop where the original's back edge puts it — see CODEGEN #206.
  ⚠ That +30 is NOT a regression to chase: on a residual this size the masked score carries no
  information about correctness, and the pre-v573 spelling was a real defect that disabled
  incremental repaint for every row but the first of each dirty rect) —
  `src/WorldBoardMaybe.cpp` (end of TU, append-only so the parked UpdateDirtyTiles and
  EXACT ResetFlag0x3c bodies don't rotate). The dirty-rect paint routine called per
  flushed rect by UpdateDirtyTiles (was the largest unclaimed APP gap, 1364B): per tile
  in the rect's tile span, if the tile's dirty bit is set it walks the cell's plane-B
  object slots (bPlaneBCountMaybe clamped to >= 2 passes), virtual-draws each slotted
  object (slots +0x2c/+0x30, the latter gated on the kind-desc raw record's
  bDoubleSpeedFlag), and -- in app-state 3 only -- runs the pass-0/1 overlay helpers
  (PeerTrainSlotQueueMaybe::FUN_0044e2e0, DecorObjMgrMaybe::FUN_004348a0, the
  category-3 swap of their order, BigObjTrackingSets' FUN_00423e80), then invalidates
  the tile rect on all six overlay widgets. Tail: if the rect intersects the selected
  object's bounds the selection widget is redrawn directly (unlock/relock of the work
  surface around it), and the placement cursor is invalidated on its own/hover-ghost
  bounds. Structure faithful end-to-end (see the in-source autopsy). Residual is THREE
  stacked documented intrinsic /Og coin-flip classes: (1) the 0x4393d0
  sete-materialization class on `DAT_004851f4 == 3` (shared with the v334/v335/v337/
  v338 parks); (2) induction-var residency (the v334 (3) class) on the row/col/colIdx/
  slot-index induction set; (3) the count-pointer init formation coin-flip
  ((idx+2)<<6 vs lea-fold). Levers that landed: int temps feeding the ternary tile
  clamps, `switch (pass)`, SHORT iPlaneMaybe params on FUN_0044e2e0/FUN_004348a0
  (movsx ax + WORD spills), cell-pointer walk (pCell/pCellCol). New TU-local views
  (no shared-header changes): WorldBoardPaintView0x456700 (aTileCellsMaybe +0x44),
  BoardTileCellView0x456700 (aPlaneBSlotsMaybe +0x20[7], bPlaneBCountMaybe +0x3c —
  Ghidra BoardTileCellMaybe names), TilePlacedObjPaintView0x456700 (padded-vtable probe
  slots 11/12 + nSubFrameMaybe +0x28/pKindDesc +0x40), TilePlacedObjKindDescView0x456700
  (resourceId +0x4, pRawBufferMaybe +0x20, stride-0x18 records, bDoubleSpeedFlag +0x17),
  and methods-only callee views for PeerTrainSlotQueue/BigObjTrackingSets/BuildToolButton/
  WorldActionCursor/DecorObjMgr/SelectedObjWidget/PlacementCursor. Retry only if the
  0x4393d0 sete class or the induction-var residency class cracks.

- `UIResources::TickStationClockChimeMaybe` (0x447400 — ⭐ **RE-SCORED v359 UNDER SP3, and the
  whole diagnosis below is STALE: it is now DIFF(12), len 1316 == 1316, align=0 reg_pen=0
  identity_miss=0, insns 445/445.** Both /Og classes this row blames (the v337 vtable-value-CSE
  frame growth and the symmetric esi/edi swap) were RTM toolchain artifacts and are simply gone.
  The entire residual is ONE swapped pair of spill slots — the original puts pClockDesc at
  [esp+0x10] and pFrame at [esp+0x14], we do the reverse. Re-probed under SP3 and INERT:
  declaration order of the two pointers, and per-block scoping of pFrame. Untried: anything that
  changes their relative spill weight / live-range structure. Original v340 text follows.)
  (v340, first transcription,
  `asmscore.py --len 1316` total 211654, align=194 reg_pen=152 identity_miss=211
  byte_diff=344, insns 448/445) -- `src/UIResources.cpp` (end of TU, append-only so the
  EXACT SetKindSlotPtrMaybe and parked InitPartial bodies don't rotate). The station-clock
  chime tick (callers pass `g_dwGameTick` as nSeconds plus an unread 2nd stack param --
  `ret 0x8`): computes the 12-step minute index `(((nSeconds/60)%60)/5+1)%12` and hour
  index `(nSeconds/3600 + (nSeconds%3600 >= 1800 ? 2 : 1))%12`; on minute steps 0/3/6/9
  it stores the step into the registry's +0x28 last-chime-step field and plays the
  full-hour (DAT_004a64c8, wav 0x53ab) / quarter-hour (DAT_004a6480, wav 0x5399) chime
  SoundBankEntry via DSound::AcquireChannelForSound at screen center (category 4), each
  lazily preloaded via SoundBank_PreloadWavRangeMaybe with the (SoundBankEntry *)-1
  load-failure sentinel + errno=ENOENT (FUN_00467fd0 == the CRT `_errno` accessor). Then
  blits the 4 clock-hand layers (kind descriptors 0x3dad/0x3dae at dest offset
  (0xf,0x18), 0x3db0/0x3db1 at (0x1f,0x2a), the latter pair gated on descriptor 0x843's
  pOwnedObjA as the first pair is gated on 0x842's) into the clock descriptor's own
  pOwnedObjA bitmap via the untranscribed raw color-key blit LocoBitmap::FUN_0042c330
  (0x42c330, same RECT-by-value idiom as the PixelCopyBlit family), each source rect
  offset by index*nativeWidth across the 12-frame strip (minute index on 0x3dad/0x3db0,
  hour index on 0x3dae/0x3db1), finishing with WorldBoardMaybe::MarkRectDirty of
  rcViewport. Structure faithful end-to-end (see the in-source autopsy). Residual is TWO
  stacked documented intrinsic /Og coin-flip classes: (1) the v337 vtable-value-CSE
  class -- this compile caches each sprite descriptor's vtable pointer in a stack slot
  across the GetOrLoadFrameBitmap/ReleaseRef pair (the original reloads `[pDesc]` per
  call); the extra cache slot grows the frame 0x30 vs 0x2c, cascading a 4-byte shift into
  every stack reference (most of the byte mass); (2) the symmetric esi/edi register-role
  swap (Yoda #29/#30, confirmed not source-steerable on VC5): original keeps nMinuteStep
  in edi + pDesc in esi, this compile the opposite. Levers that landed (kept): calling
  SoundBank_PreloadWavRangeMaybe on the g_UIResources global (the original's
  `mov ecx,0x4855e8`, not `this`), reusing the nSeconds parameter for the hour-step index
  (the original overwrites param_1's stack slot), inverting the half-hour branch
  (`>= 1800 ? +2 : +1`) for the original's jl polarity. Confirmed NO EFFECT: distinct
  per-sprite locals (byte-identical DIFF). The function is a member of the TU-local
  UIResourcesView0x447400 (with TileKind_GetOrLoadDescriptor/
  SoundBank_PreloadWavRangeMaybe callee decls): ANY new method declaration in the shared
  UIResources.h rotates DPlaySessionMgr.cpp's /Og TU state and breaks
  SelectGridCellFromPointMaybe's EXACT (v340 bisect -- the +0x28 field addition alone is
  rotation-free and stays in the shared header; the view produces byte-identical codegen).
  Retry only if the v337 vtable-value-CSE class or the symmetric-swap class cracks.

- `NameAnchorMaybe::AdvanceAlongTrackMaybe` (0x40bbd0, v325, first transcription,
  `asmscore.py --len 1928` total 1919542, align=1906 reg_pen=115 identity_miss=115
  byte_diff=892, insns 682/655) — `src/NameAnchorMaybe.cpp`. The train lead-anchor's
  per-tick socket-step + matched-tile dispatch (the largest APP gap claimed in the v324
  pickup). Structure verified block-by-block against the raw disasm: the entry extend-phase
  dispatch, the NoMatch socket-fail path (byte-identical to the original over its whole
  ~0x9a span), the dwSoundStateMaybe==1 switch (points/level-crossing/ApplyLeadTileTickStateMaybe arms),
  the phase-chain socket step, both inline viewport/tile extend copies, the state-4
  claim-release tail, and the matched-tile state dispatch (8/2/7/3/default) itself.
  Residual: (1) an 8-byte guard block (`test matched,matched; je ContinueTick`) the
  original doesn't have — the original thread-eliminates the second `if (matched == 0)`
  test and places the matched-tile handling at the FUNCTION'S TAIL (jump-threaded straight
  from the dispatch), while every source shape we wrote makes VC5 either keep the guard
  inline or rotate the matched-handling up next to the dispatch; (2) downstream /Og noise
  (CanReverse-fail park block out-lined vs inline-duplicated, bRet store-forwarding in the
  ViewportExtend cases). 12+ structure variants tried incl. VC5 probe-compiles mapping its
  placement rules (plain goto / if-else-goto / double-if / ladder / labels): the original
  [dispatch, NoMatch, main path, extend, returns, matched-at-tail] layout was not
  reproducible. Same "block layout is trace-driven, mostly not source-steerable" class as
  the DPlay_JoinOrHostSession entry above. Retry if the TU-context /Og layout class ever
  cracks (see the 0x45a880/0x45aa50 park note directly above — the v323 twin has the same
  uncracked family).

- `NameAnchorMaybe::AdvanceCarAnchorMaybe` (0x40c580, v343, first transcription,
  `asmscore.py --len 1207` total 1506886, align=1494 reg_pen=110 identity_miss=119
  byte_diff=696, insns 525/429) — `src/NameAnchorMaybe.cpp`. The per-car sibling of
  AdvanceAlongTrackMaybe directly above (one of a CarNetObj's own 2 track anchors; no
  points-crossing/coupling-wait sub-calls). Structure verified block-by-block against the
  raw disasm (objdump, full 0x40c580-0x40ca37 span): the phase==2 entry fast-path, the main
  socket-match dispatch (dwModeA/dwModeB mode-echo writes onto the CAR not the train), the
  bound-check-then-step tail, and all 4 distinct extend-phase blocks (ViewportExtend4 at
  0x40c987 shared from 2 call sites, a SEPARATE non-sharing physical duplicate of the same
  shape at 0x40c7e8, and TileBoundaryCheck/SharedExit at 0x40ca1b/0x40ca2f shared from 2
  more call sites) are all individually confirmed. Two real findings drove the biggest score
  improvements (asmscore total 1748468 -> 1506886 across 2 rewrite passes): (1) the original
  has a SEPARATE full push/pop epilogue at nearly every distinct return point rather than
  sharing one via fallthrough -- the source must place an explicit `return` at each decision
  point, not consolidate into one shared tail; (2) tail-merging between the 4 extend-phase
  blocks is real but NON-uniform — some call sites share an existing block's epilogue via a
  genuine jump, a functionally-identical sibling site compiles its own separate copy instead,
  and within one shared block the ==4 sub-case shares while the ==5 sub-case doesn't. Same
  "block layout is trace-driven, mostly not source-steerable" VC5 class as
  AdvanceAlongTrackMaybe above (see docs/CODEGEN.md's goto-sharing entry). Residual after
  modeling all of the above: a register-allocation-tier mismatch, not a control-flow gap —
  the compiled prologue pushes 5 registers up front (`ecx,ebx,ebp,esi,edi`) where the
  original stages 1 then 3 more after the null check, consistent with this function's own
  register-pressure budget landing differently. Tried and no effect: dropping the
  one-time-use `matched` local for an inline condition. Retry only if the shared TU-context
  /Og register-budget class ever cracks (see AdvanceAlongTrackMaybe's own park note above).

- `CarNetObjAnchorPartial::UpdateCarPlacementTickMaybe` (0x40d940, v344, first transcription,
  `asmscore.py --len 576` total 166166, align=160 reg_pen=55 identity_miss=55 byte_diff=116,
  insns 182/185) — `src/PeerTrainNode.cpp`. The per-car counterpart of
  `PeerTrainNode_UpdatePlacementTickMaybe`'s own tick (called once per car slot from that
  function's main do-while): a "both anchors share one claimed tile" settle check (defers to
  SettleClaimedSocketMaybe or a simple bound check), else advances both of the car's own anchors
  (`NameAnchorMaybe::AdvanceCarAnchorMaybe`) and runs the heading/mode-follow-up sequence
  (ComputeHeadingAngleMaybe/RepositionForHeadingMaybe/CompleteViewportExtendMaybe/CheckCarLeftViewportMaybe/AdvanceModeBStateMaybe),
  then independently releases a stale global-connector occupancy claim on the train's far tile.
  Structure verified statement-by-statement against the raw disasm/decompile over the full
  0x40d940-0x40db80 span, including decoding 3 raw-offset Ghidra artifacts back to real fields
  (`pTVar9[1].base.base.vftableMaybe` == `TrackTileObj::dwTrackState` at +0x10c, a stale
  TilePlacedObj-sized `[1]`-index over the true TrackTileObj type; `pTVar1[1].base.base.
  rectRightMaybe` == `TrackConnectorTileObj::dwOccupancyClaim` at +0x11c, same mechanism;
  `(pTVar->base).base.rectBottomMaybe` == plain `pTVar->rect.bottom`) via a live Ghidra struct
  dump (RectFlagObj0x477820/AnimDescRefObj0x477488/TilePlacedObj/TrackTileObj field offsets).
  Very close instruction count (182 vs 185) and almost every mismatch is a register-only ("r")
  or equivalent-shape ("S") diff, not a missing/extra block. Root cause: the compiled prologue
  pushes ONE MORE register than the original (`ebx,ebp,esi,edi` here vs the original's
  `ebx,esi,edi`), cascading into a stack-offset shift for the rest of the body. Same "TU-context
  /Og register-budget" class already parked and unresolved for this function's own callee-side
  siblings `NameAnchorMaybe::AdvanceCarAnchorMaybe` (0x40c580, v343) and `AdvanceAlongTrackMaybe`
  (0x40bbd0, v325, 12+ variants). Tried: moving `bHandled`'s zero-init to the very first
  statement (genuine fix, matched the original's own statement order and closed one real
  instruction-ordering mismatch, total 178170 -> 166166); hoisting every local to
  declare-then-assign at the top (no effect, identical score). Retry only if the shared
  TU-context /Og register-budget class ever cracks generally (see the two parks above).

- `CarNetObjAnchorPartial::SettleClaimedSocketMaybe` (0x40dc20, ex-`FUN_0040dc20`, v345, first
  transcription, `asmscore.py --len 853` total 154534, align=148 reg_pen=58 identity_miss=58
  byte_diff=154, insns 267/265) — `src/PeerTrainNode.cpp`. The claimed-tile socket-settling
  handler `UpdateCarPlacementTickMaybe` defers to when both of a car's anchors sit on one
  claimed (dwTrackState==5) tile of kind m_type0x63a==5: settles both anchors' positions
  straight from the tile's socket table and returns unhandled when the train-direction-picked
  anchor already sits at its own socket-index boundary; otherwise steps both anchors' socket
  indices by one, re-settles positions (the lead anchor's own X/Y come from an index picked
  0xb sockets off, not its own just-adjusted index), dirty-marks the car's rect, and finally
  repositions the car (vtbl slot 3, `RepositionWithHotspot`) around a per-heading footprint
  hotspot offset. Structure verified statement-by-statement against the raw disasm/decompile
  over the full 0x40dc20-0x40df75 span; along the way promoted Ghidra's own already-named
  `Obj0x4779e0::aFootprintOccupancyMaskMaybe` (819-byte per-heading footprint/hotspot table,
  still only byte-array-typed) into `src/CursorDesc.h` as a same-size/same-offset padding
  split (confirmed safe: only `src/Obj0x4779e0.cpp`'s own `Obj0x4779e0ParsePartial` consumed
  the old `pad0x16a`/`pad0x16e` names directly, fixed up in the same commit; a full
  `tools/progress.py` re-run confirmed zero regressions elsewhere). Close instruction count
  (267 vs 265); almost every mismatch is register-choice noise (edi/ebp swapped throughout)
  plus two intrinsic scheduling differences: (1) the prologue loads the incoming `pTrain` param
  from a different stack slot than the original (same "compiled prologue pushes a different
  register set" tell as this function's own callers), (2) one `this->pKindDesc` reload is
  hoisted earlier in the original at each of the 3 `RepositionWithHotspot` call sites, deferred
  here. Tried and confirmed NO EFFECT: an explicit if/else instead of a ternary for the
  direction-picked anchor local; inlining the `RepositionWithHotspot` x/y args directly instead
  of through `int` locals. Same shared TU-context /Og register-budget class already parked for
  this function's own siblings/callers in this TU (`AdvanceCarAnchorMaybe` 0x40c580 v343,
  `UpdateCarPlacementTickMaybe` 0x40d940 v344, `AdvanceAlongTrackMaybe` 0x40bbd0 v325) — retry
  together if that class ever cracks.

- `CarNetObjAnchorPartial::ComputeHeadingAngleMaybe` (0x40df80, ex-`CarNetObj::
  ComputeHeadingAngleMaybe`, v346, first transcription, `asmscore.py --len 324` total 24016,
  align=24 reg_pen=0 identity_miss=0 byte_diff=16, insns 103/103) — `src/PeerTrainNode.cpp`.
  Recomputes `wHeadingMaybe` from the current A/B anchor pixel positions: single-quadrant
  `fpatan(dy_abs/dx_abs)` converted radians->degrees, quadrant-corrected off the SIGNED anchor
  deltas, two axis-aligned overrides (dy==0 forces 0/180, dx==0 forces 270/90 — these win over
  the fpatan-derived value), quantized into a 0-127 "compass" unit and clamped 0x80->0. Two real
  findings this session: (1) **Ghidra's own decompile of this function silently drops an FPU
  `fmul` scale constant** (the final degrees->128ths multiply) — same "never trust decompiled
  FPU pseudocode on an x87-heavy leaf" trap CLAUDE.md documents, caught only by re-deriving the
  whole control-flow graph from the raw disasm/`.rdata` double constants by hand (0x4775d0=0.0,
  0x4775d8=57.29577950560105=180/pi, 0x4775e0/e8/f0/f8/477600=180/-180/360/270/90,
  0x477608=0.35555...=128/360). (2) **A genuine dead stack parameter**: the caller (`push eax`
  before `call 0x40df80` at 0x40da81, where eax=`this->rect.left`) plus this function's own
  `ret 0x4` prove a real 4-byte stack arg exists in the signature, but the body never reads it
  anywhere — Ghidra's own decompile silently drops it too (same "dead-but-real parameter" tell
  as `BuildToolCursorWnd.h`'s `pDescUnused`/`PostBag.h`'s `nUnused`); modeled as `int nUnused`,
  caller updated to pass `this->rect.left` explicitly. Fixing this ALSO improved
  `UpdateCarPlacementTickMaybe`'s own score (166166->159721, insns now matching 185/185 exactly)
  since the call site itself was previously wrong-shaped. Residual after both fixes: an
  FPU-stack-residency scheduling tie-break, the x87 analog of the file's well-documented
  GP-register-budget class — the original keeps the constant `0.0` (the `theta=0.0` default)
  resident on the FPU stack from function entry through every branch, reloading it lazily where
  needed; every source shape tried (if/else assignment, restructured else-if chain matching the
  original's own flag-reuse block layout, a ternary initializer) still has the compiler
  materialize a FRESH `0.0` load at the point of use instead of carrying the entry-time push
  through — the else-if restructure (mirroring the original's own redundant-cmp block layout,
  see `SettleClaimedSocketMaybe`'s `ApplyOffset` goto precedent) was a real, large improvement
  (58026->28017), and the ternary on top of it a smaller further one (28017->24016, insns
  103/103 exact) but neither closes the entry-fld-0.0 gap. Only 4 diff lines survive out of 103
  instructions. Retry only if a general fix for VC5's FPU-stack-residency-across-branches
  scheduling is ever found (a new class, not yet cracked for any sibling in this file).

- `CarNetObjAnchorPartial::RepositionForHeadingMaybe` (0x40d8e0, ex-`CarNetObj::
  RepositionForHeadingMaybe`, v346) — `src/PeerTrainNode.cpp`. **EXACT MATCH.** Reads
  `wHeadingMaybe` back as a `*4`-strided index into the same per-heading hotspot table
  `SettleClaimedSocketMaybe` reads (`this->pKindDesc->aFootprintOccupancyMaskMaybe`,
  `+0x168..+0x367` relative to the descriptor), offsets `pNameAMaybe`'s anchor by the looked-up
  (dx,dy), sizes the rect off `nativeWidth`/`nativeHeight`, calls the inherited vtable slot 3
  (`RepositionWithHotspot`). First-draft transcription (hoisting `heading`/`pKindDesc` into
  named locals and computing `x`/`y` as separate int locals before the field writes) scored
  89877 (align=88) — rewriting to read `this->wHeadingMaybe`/`this->pKindDesc` directly inline
  at each use (matching `SettleClaimedSocketMaybe`'s own established style, no cached locals for
  either) and writing through `this->rect.left`/`this->rect.top` fields instead of separate `x`/
  `y` ints dropped it straight to a clean byte-identical match (`get_function_by_address`'s own
  `Body` span clipped the trailing single-byte `ret` — the usual "round up a byte or two"
  correction, true length 91 not 90). A clean instance of the general "prefer direct field
  access over locally-cached copies" lesson for this file's register-pressure-sensitive
  functions.

- `CarNetObjAnchorPartial::CompleteViewportExtendMaybe` (0x40e340, ex-`CarNetObj::CompleteViewportExtendMaybe`, v347, first
  transcription, `asmscore.py --len 254` total 98806, align=98 reg_pen=7 identity_miss=7
  byte_diff=36, insns 90/85) — `src/PeerTrainNode.cpp`. The car's own dwModeAMaybe==4
  follow-up: resets/dispatches `SetReadyStateMaybe(1)` (a newly-identified real virtual, vtable
  slot 9 -- see below) when both anchors are idle and dwModeAMaybe was set, re-derives the far
  end of the whole train (same forward/reversed pFarAnchor pick as
  `UpdateCarPlacementTickMaybe`'s own tail), and releases a stale plane-A occupancy claim.
  Sibling `CarNetObjAnchorPartial::CheckCarLeftViewportMaybe` (0x40e2a0, viewport-extend-phase-2 settle) and
  `AdvanceModeBStateMaybe` (0x40db90, dwModeBMaybe follow-up, dispatches to the still-untranscribed
  `CheckCarClearedDepotMaybe`) BOTH went EXACT the same session with a plain natural-shape rewrite -- see
  below for why 0x40e340 didn't. **Real bug caught along the way**: both 0x40e340 and 0x40e2a0
  were first declared `void` (an earlier-session guess); raw disasm proved they actually return
  `unsigned char` (0x40e2a0 always 0; 0x40e340 either 0 or 1) via an explicit `xor al,al`/
  `mov al,1` at the shared exits -- the usual CONCAT31 "caller discards it, but it's real" tell
  already documented elsewhere in this TU. Fixing the return type alone flipped 0x40e2a0 straight
  to EXACT once combined with one operand-order swap (writing `this->rect.top >
  g_worldBoard.dwViewportHeightMaybe` instead of the reverse to match the original's own
  top-loaded-first case-3 comparison). **Also identified vtable slot 9** (`+0x24`,
  `AnimDescRefObj0x477488::_v09`, previously an unnamed placeholder) via a live Ghidra vtable
  dump of `CarNetObj_Vtbl`+`FUN_004061b0`'s own decompile: sets `bReady`, dispatches `MarkDirty`
  (slot 1), then resumes/pauses the car's `DSoundChannel` to match -- promoted to a real named
  virtual `SetReadyStateMaybe(char)` on `AnimDescRefObj0x477488` (`src/WidgetBase.h`), not
  overridden by `CarNetObj` (confirmed by the vtable dump), so both callers here reach it through
  the real inheritance chain, not a raw vtable-slot cast. Residual on 0x40e340 itself, after the
  return-type fix: the prologue (0x40e340-0x40e37f) and the ENTIRE far-anchor re-derive block
  (0x40e37f-0x40e3f8, including the original's own register-level code-sharing trick -- one
  physical `cmp ecx,1`/`jne` at 0x40e3ba reached both as the reversed branch's real entry AND as
  a forward-branch fall-through bail, reusing one register-held copy of `pTrain->dwReversed` for
  both purposes) reproduce byte-for-byte from a plain nested if/else-if with NO gotos -- VC5's
  own optimizer found the same sharing unprompted, no source-shape effort needed. Two remaining
  residual classes: (1) reg_pen=7/identity_miss=7, EAX-vs-ECX register-choice swaps for the
  reversed/forward `pFarAnchor` locals -- the same "TU-context /Og register-budget" class already
  parked for this file's siblings (`AdvanceCarAnchorMaybe`/`UpdateCarPlacementTickMaybe`/
  `SettleClaimedSocketMaybe`). (2) The real gap: the ORIGINAL tail-merges EVERY `return 0` exit
  (including the very first reset-check's own early-out) into ONE shared physical epilogue at
  the function's end, reached via a single far `0f 85` `jne` all the way from the earliest
  check; the candidate instead compiles that FIRST early-out into its own separate, physically
  duplicated small epilogue (a near `74`/`75` jump to a LOCAL copy of
  `xor al,al; pop edi; pop esi; ret 4`), shifting every later byte offset and cascading into the
  reported diff even though the actual instructions are otherwise identical. Tried and
  confirmed WORSE, not better: routing all 3 early-outs through one `goto Fail; ... Fail: return
  0;` label (98806 -> forced an extra redundant re-check block instead of merging the epilogues,
  265 total len); flattening to a single `bAbort` accumulator with one tail return (283 total
  len, worse still -- VC5 evaluated both branches unconditionally instead of skipping one). The
  plain triple-`return 0;` shape (kept in `src/`) is the best of the 3 variants tried. Retry only
  if a general fix for VC5's own epilogue tail-merging heuristic is ever found -- a DIFFERENT
  residual class from the plain register-budget one, not yet cracked for any function in this
  file.

- ~~`CarNetObjAnchorPartial::CheckCarClearedDepotMaybe` (0x40e520)~~ — **RESOLVED v349, row kept
  briefly as a cautionary example; delete on the next PARKED.md pass.** v348 parked this as the
  first "intrinsic, candidate better-optimized than the original" residual in this file, on the
  strength of a minimal-TU probe that reproduced the full-TU score exactly. The symptom autopsy
  was accurate (a pure EAX<->EDX scratch-temp swap in cases 7/9/10, which let the candidate
  cross-jump all four settle tails into one where the original keeps two) and every source-shape
  probe it ran really is byte-neutral — but the CAUSE was not the function. Sweeping the block's
  POSITION in `src/PeerTrainNode.cpp` took reg_pen from 19 to 0: hoisted above
  `UpdateCarPlacementTickMaybe`, the byte-for-byte SAME source text is an EXACT match. The
  minimal-TU probe is therefore not a valid intrinsic/TU-context classifier (a standalone probe
  carries its own optimizer state, which coincides with SOME position in the real TU — here, a
  bad one). See `docs/CODEGEN.md`'s position-sweep bullet; sweep position before parking any
  register-rename residual.

- `BuildToolButton::DispatchMenuItemClickMaybe` (0x44a250, v326, first transcription,
  `asmscore.py --len 1760` total 912615, align=902 reg_pen=92 identity_miss=97
  byte_diff=445, insns 634/553) — `src/BuildToolButton.cpp`. The toolbar menu-item click
  handler (vtable slot 17 override): early-out gate (null/bVisible/own-slot-2 Contains),
  then a switch over the node icon's resourceId 0x2403..0x240e. Structure verified
  block-by-block against the raw disasm: prologue + gate + dispatch + case 0x2406 are
  instruction-aligned for the first ~0xbd bytes, and every case body matches
  instruction-for-instruction modulo the residuals. Residual: (1) the 0x2405 case's
  viewport tail — the original keeps TWO branchy copies of the computation (its own at
  0x44a398 plus the 0x2404/0x2403-shared one at 0x44a50d) with a shared
  true-store/final tail (0x44a53c/0x44a543); every all-branchy source form got
  /Og-merged 3-ways into one copy here, so the landed source keeps 0x2405 as a plain
  if/else (compiles to setge/setcc) while the 0x2404/0x2403 pair merges branchy as in
  the original; (2) the deselect tail (SetNodeState(1)+bSuppressRectBMaybe=0+
  ActivateNodeMaybe(,0)) — the original shares ONE copy at 0x44a56e past the final
  tail, ours merges the three inline copies into one too but places it inside case
  0x2403's body (source-level goto labels placed the block correctly but made the total
  strictly worse — tail-merge magnet effect); (3) push-scheduling interleaves in the
  0x2409/0x240a ActivateTab tails and pervasive vtable-pointer register-cache swaps
  ([esi+0x178] into ebp vs per-use edx reload). 12 variants tried (shared goto labels
  1.56M, full goto structure 2.04M, plain duplication 1.01M, arm swaps 1.01M,
  arms-duplicated tails 0.968M, goto/inline hybrids 1.08M/1.15M, temp-local neutral,
  landed mixed form 0.913M). Same VC5 /Og trace-driven block-layout class as the
  0x40bbd0 entry above. Landed with it: BuildToolButton's full 0x74c struct model
  (WidgetBaseObj0x4784c8 base + iconBMaybe/+0xe0 + hit rect/+0x168 + regionAMaybe/+0x178
  + regionBMaybe/+0x260 + tail fields/+0x740..0x748) and the new
  WidgetTagObj0x478378 class in src/BuildToolButton.h (regionAMaybe's type, slot-21
  ActivateNodeMaybe). Retry if
  the TU-context /Og layout class ever cracks.

- `PeerTrainNodeLayoutPartial::LayoutCarAnchorsMaybe` (0x44ce10, v327, first transcription,
  `asmscore.py --len 1631` total 324574, align=316 reg_pen=73 identity_miss=105
  byte_diff=224, insns 514/476 incl. jump-table decode noise) — `src/PeerTrainNode.cpp`
  (new TU). Computes every car slot's `NameAnchorMaybe` tile/socket/heading plus the lead
  `pNameMaybe`'s world-pixel anchor after a car attach/reversal (see docs/subsystems.md's
  PeerTrainNode entry). Structure verified block-by-block against the raw disasm: loop-1
  if/else-if `m_type0x63a` chain (NOT a switch — the original has no jump table there) with
  its two shared anchor-init tails, socket-table anchor math, both jump-table switch loops.
  Key wins: source case order drives VC5's case-block stream order (original emits
  2/8, 4/0xa, 1/7, 3/9 — source cases reordered to match, 762k→599k score); the `pA=0`
  initializer + declaration placement before the null check reproduces the up-front
  push/xor-EDI prologue; dropping the reversed loop's `pCar` local reproduces the
  per-statement `carSlots[i]` reloads (`MOV EBX,[EAX]` per store). Residual is the VC5 /Og
  scheduling/reg-tie-break class: else-arm LEA interleaves in the +0x26 cases, a one-block
  EBX detour for the 18*i term in forward 1/7, load-order swaps in two Y-case tails,
  `nStepOut=0` init placement. ~10 variants tried (324k floor). Looks TU-context-bound —
  retry when more of the PeerTrainNode cluster (0x44c3a0 next) is transcribed into this TU.
  **v328 update:** 0x44c3a0 landed in the TU (parked alongside, below) and shifted this
  function's floor UP to 358265 (align=350 reg_pen=70 identity_miss=102 byte_diff=245) —
  TU context moved it, but the wrong direction; retry both together once the /Og class cracks.

- `PeerTrainNodeTickPartial::PeerTrainNode_UpdatePlacementTickMaybe` (0x44c3a0, v328,
  first transcription, `asmscore.py --len 1520` total 694239, align=688 reg_pen=53
  identity_miss=53 byte_diff=409, insns 536/514) — `src/PeerTrainNode.cpp`. The train's
  per-tick placement driver (see the function's plate comment + docs/subsystems.md's
  PeerTrainNode entry). Structure verified block-by-block against the raw disasm. Key wins:
  BOTH `dwTrackTickState` tri-state dispatches are SWITCH statements (the sub/dec/dec
  cascade is the tell; a ==-chain compiles to test/cmp/cmp) — same lesson for the closing
  0xc5c..0xc60 kind test (sub-chain switch, not ==-chain); the `bMoved` flag lives in AL
  only when `bMoved=true` is a per-case statement (a declaration init pinned it in BL); the
  byte-counter car loops (INC BL + `[esp+0x18]` spill/AND 0xff reload) come from a separate
  `unsigned char` counter + `int` index variable do-while, NOT a `for` over `unsigned char`
  (which compiles to a plain int loop). Residual is the VC5 /Og trace-layout +
  const-allocation class: (1) case-1 + the shared `SetSoundStateMaybe(2)` + `bMoved=true`
  blocks of the first switch out-of-line after the main RET in the original, inline here
  (identical relative block order — the 0x40bbd0 park class); (2) const-1 materialization
  timing (orig EBX=1 at the modeA check + EDI=1 in the SetCarsReady(1) block; ours EBX=1
  hoisted to the loop preheader); (3) EffectSpawner arg-prep scheduling; (4) do-while tail
  rotation (jc-back vs jae-exit/jmp-back); (5) scattered reg tie-breaks. ~8 structural
  variants tried (694k floor). Parked alongside 0x44ce10 — same /Og class, same TU context;
  retry together if the class ever cracks.

- `PeerTrainNodeTickPartial::ReverseDirectionMaybe` (0x44cb10, v329, first transcription,
  `asmscore.py --len 714` total 150058, align=150 reg_pen=0 identity_miss=0 byte_diff=58,
  insns 246/223) — `src/PeerTrainNode.cpp`. Commits a validated direction reversal
  (dwModeAMaybe 1<->4 DEC/SUB-cascade switch, SETZ dwReversed toggle, per-car dwModeAMaybe
  range nest + dwModeBMaybe jump-table switch with case blocks in the original's 2,1,5,4
  memory order, nOccupantRefCount DEC/INC move to the new lead tile, CopyTailFrom + up-to-12
  AdvanceAlongTrackMaybe re-walk in both directions, closing dwUnk0x1c switch with case
  blocks in 1/2,0,4/5 memory order and a break-to-shared-return tail). Structure fully
  verified block-by-block. Sole residual is a 9-byte scheduling swap at +0x51: the original
  emits the car-loop entry guard (`CMP word [wCarSlotCount],BX; JB exit`) BEFORE the
  `LEA EDI,[carSlots]` pointer init; this build schedules the LEA between the CMP and the JB
  (same near-jump encodings). Tried and confirmed NO EFFECT: declaration order of
  i/ppCar/bCont, declaring i inside the for; carSlots[i] indexing instead of the pointer walk
  is much worse (rotates the whole prologue). Same one-line /Og scheduling tie-break class as
  SetSoundStateMaybe's residual below.

- `PeerTrainNodeTickPartial::CanReverseDirectionMaybe` (0x44d630, v329, first transcription,
  `asmscore.py --len 139` total 12115, align=12 reg_pen=1 identity_miss=1 byte_diff=5,
  insns 58/58) — `src/PeerTrainNode.cpp`. Reversal gate: refuses while
  dwSoundStateMaybe==4/bUnk0x5a, then compares the current lead tile's kind vs the
  would-be-new-lead tile's kind (both must be track families and NOT differ).
  Structure fully verified. Sole residual is one register-allocation tie-break at +0x15: the
  original tests dwReversed in EAX (reserving EDX for the arm-1 movzx index); this build
  tests it in EDX and movzx's into EAX. Same instruction count and blocks — /Og allocation
  coin-flip.

- `AnimEffectObj0x477a90::AnimEffectObj0x477a90` (0x422ec0, v329, first transcription,
  `asmscore.py --len 1510` total 381480, align=374 reg_pen=66 identity_miss=66
  byte_diff=220, insns 530/503) — `src/AnimEffectObj.cpp`. The class's ctor (base-chain
  AnimDescRefObj0x477488, toupper'ed placement-mode char switch
  'C'/'D'/'P'/'R'/'S'/'U'/'W' of randomized viewport-edge-relative placement draws via
  class-qualified RepositionWithHotspot, optional EffectSpawner companion spawn).
  Structure fully verified block-by-block (jump-table switch with case blocks in the
  original's C/R/P/S/U/D/W source order; per-arm duplicated calls with /Og
  suffix-merging reproducing both shared call sites incl. the SINGLE param_4==-1
  fixup shared by 'P'/'S' -- matched via `goto FixTargetX` into 'S's tail; the
  divisor-zero-guarded inclusive-range draw macro at all 14 sites incl. the
  self-cancelling +hs-hs pairs). Residual is the VC5 /Og allocation/scheduling
  tie-break class: the case-'R' y-half register cluster (nYLimit/nY in EBP vs hsY in
  EDX in the original -- guard reassociation, lo-fallback MOV block, per-arm je/jne
  polarity -- vs this build's shared-EBX fold), 'P' entry/else desc-load placement,
  per-arm dwViewportWidth load scheduling, 'U' entry scheduling (its identical-source
  'D' twin matches), effect-tail EBX load order, LEA-vs-ADD/CDQ placement. Retry if
  the TU-context /Og block-layout/allocation class ever cracks (same family as the
  0x40bbd0/0x44a250/0x44ce10/0x44c3a0 rows).

- `Ddraw_HResultToString` (0x45bbc0, v330, first transcription, `asmscore.py --len 1493`
  total 2536766, align=2536 reg_pen=0 identity_miss=0 byte_diff=766, insns 786/435) —
  `src/Ddraw.cpp`. SDK-sample-derived HRESULT -> English string mapper: one sparse
  129-case switch returning string literals. Case SET byte-verified identical to the
  original (its 61 sweep `cmp eax,imm` values + 4 jump tables' 68 non-default entries
  union to exactly these 129); the first 251 instructions — through the
  0x8876017c-family table — byte-match. Residual is ONE switch-lowering choice in the
  0x88760208..0x88760319 region: the original keeps a pairwise sweep + 3 direct dword
  jump tables (spans 29/32/13) there, while this c2 always merges the whole region into
  a single byte-index-compressed two-level table (`xor ecx,ecx; mov cl,[eax+tbl]; jmp
  [ecx*4+tbl2]`). Refuted levers: source case order (ascending / string-VA-descending /
  shuffled — clustering sorts, order-independent), VC5 SP3 c1/c1xx/c2 extracted from the
  archive.org vs97sp3 cabs (byte-identical .obj — SP3 does NOT change this lowering, so
  it is not a fix for the RTM->SP3 toolchain skew noted in toolchain/README.md), VC4.2
  c2, flags /O1 /O2 /Os /Ot /Od /Og- /G3 /G4 /G5 /G6 /GB, C vs C++ frontend. Retry if a
  compiler/flag combination that suppresses the two-level-table merge ever shows up, or
  if evidence surfaces that the region was a different source construct (no plausible
  one found: interleaved sweep+table+sweep positions rule out chained nested switches,
  and if-else chains don't produce the bisecting `jg` sweep).

- `Obj0x4779e0::LoadMaybe` (0x41e6e0, v332, first transcription, `asmscore.py --len 777`
  total 85235, align=80 reg_pen=47 identity_miss=46 byte_diff=75, insns 208/213) —
  `src/Obj0x4779e0.cpp`. The BigObj-family base's full .dat loader (vtable 0x4779e0 slot
  +0x10): defaults sweep (memset(0xff) for the -1 short tables, 3 seq-record clears,
  SetRectEmpty), then RF-archive-first (istrstream over LoadResource) with loose ifstream
  (ios::nocreate) fallback, virtual+CursorDesc::ParseTokenField dual parse, hotspot seed.
  Whole-function structure verified. Residual 3 tie classes: (1) the hotspot guard's
  11-byte branchy bool materialization (`mov eax,1/jmp/xor eax,eax/cmp al,bl`) — this
  compile folds it into two direct jne branches no matter the guard phrasing (9 forms
  tried: plain &&, bool/int temp init, ternary ?1:0, no-init + branch assignment, & 1,
  !x&&!y, two-bool-temps, != false; ALL fold — unlike v322's ThreadWrapper flags, nothing
  is live across the fold point to pin the materialization); (2) the istrstream `delete`'s
  vbase-load shape (orig `mov edx,[vtbl+4]; mov eax,[edx+edi]; lea ecx,[edx+edi]` vs this
  compile's `mov ecx,[vtbl+4]; add ecx,edi; mov edx,[ecx]`, -2B — same-source instruction
  selection inside the compiler's own vtordisp emission, not source-steerable); (3) pervasive
  eax/ecx/edx reg-role ties + 2 push/load scheduling swaps in the archive/fallback blocks
  (global /Og allocation coin-flip class). Retry if the VC5 &&-fold-suppression class ever
  cracks — (1) is the only residual with real byte mass (11B) and it blocks ~15 reg-tie
  re-rolls downstream of it.

- `WorldBoardMaybe::UpdateDirtyTiles` (0x456150, v334, first transcription,
  `asmscore.py --len 1448` total 437834, align=416 reg_pen=192 identity_miss=217
  byte_diff=464, insns 465/471) — `src/WorldBoardMaybe.cpp`. The per-frame dirty-tile
  flush: walks the packed 1-bit-per-tile dirty bitmap (pDirtyBitmapMaybe/+0x52484, mask
  table DAT_0047f108), coalesces each row's dirty runs into an accumulated RECT, appends
  finished runs to a linked list of 0x14-byte {RECT, pNext} nodes, back-buffer-blits each
  rect (LocoBitmap::RestoreOverlapBlt via the CursorDesc singleton's +0x10 bitmap), appends
  the placement-cursor + hover-ghost rects, coalesces (FUN_00456c60/FUN_00456d10), then per
  rect: locks the shared work surface if needed (bSurfaceLockGuard), paints
  (FUN_00456700 by-value RECT), unlocks, Ddraw_BltUpdateRect's, frees; finally memsets the
  whole dirty bitmap ((wRows*wCols)/8+1) and clears the train-slot sort scratch. Whole-
  function structure verified against the raw disasm. Residual is FOUR stacked documented
  intrinsic /Og coin-flip classes (see the in-source autopsy): (1) the leading
  `(DAT_004851f4 == 3 || == 4)` prologue sete-materialization (the 0x4393d0 class, not
  source-steerable) — ours is 6 insns shorter, cascading every downstream displacement;
  (2) `this`->edi vs ebp swap; (3) inner-loop induction-variable allocation (original
  stacks `col`, keeps xPix in ebp, frame 0x44; ours keeps `col` in ebp, frame 0x40, shifting
  every param-slot reference by 4); (4) the zero-register class (orig dedicates ebp=0 for
  null-cmp/push/imm-store ties). Real levers that DID close (kept in the transcription):
  if/else direct-store assignments on the scroll-path span temps (imm-store -1 arms),
  textually duplicated clamp ternaries (the original recomputes for the store), the
  `((x <= 0) - 1) & x` branchless clamp-to-zero, inverted `if (!bFirst) UnionRect else
  copy` layout, dirty-bit test as `(mask & bitmap) != 0` with extend-first layout, and
  g_worldBoard (not this->) for the in-loop wCols/pDirtyBitmap reads. Retry only if the
  prologue sete-materialization class ever cracks — (1) blocks every downstream byte.

- `Main::LocoWinMain` (0x462e90, v335, first transcription, `asmscore.py --len 1440`
  total 183550, align=178 reg_pen=47 identity_miss=68 byte_diff=170, insns 441/451) —
  `src/Main.cpp` (new TU). The game's WinMain: CreateDialogParamA splash (centered via
  SM_CXFULLSCREEN/SM_CYFULLSCREEN), `new AppWindow(hInstance)` singleton construction
  (g_pApp/DAT_004aa4a0, /GX alloc-protection SEH frame), LoadConfigDirectories +
  Locale_DetectLanguage, ParseCommandLine, startup sound (FUN_004480c0),
  LoadWindowAndBalancing, FUN_00406680_ChecksHighResModeRelated, single-instance
  FindWindowA/SetForegroundWindow, FUN_00406ba0 "Run", FUN_00408130_BigSwitchStatement,
  then the double message pump (GetMessage loop while app-state==2, PeekMessage loop with
  the 15-iteration PM_NOREMOVE/GetMessage input-drain budget), GameNet_DrainEventQueue,
  the FPS counter (DAT_00481914 threshold growing to 100, _time sampling into
  DAT_00481170), and the clean-shutdown tail (SaveWindowAndCleanExit on the
  FUN_00406ba0-failure path, scalar-deleting-dtor deletes, CoUninitialize). Every
  failure path returns via msg.wParam (-2/-1/0/-3/1/FUN_00406ba0's WPARAM). Whole-function
  structure verified against the raw disasm. Residual is FOUR stacked documented intrinsic
  /Og coin-flip classes (see the in-source autopsy): (1) the 0x4393d0 prologue
  sete-materialization class on `(DAT_004851f4 == 2)` and `(DAT_004851f4 == 3)` (not
  source-steerable; cascades downstream displacements, hence the raw DIFF); (2) the
  zero-register class (orig dedicates ebp=0, then ebx=0 in the game loop; ours edi=0);
  (3) slot-vs-register residency on nRunResult (orig stacks it at frame+4, the dead
  new-expression temp slot; declaration-order and int-vs-WPARAM probes NO EFFECT); (4)
  symmetric lea-register swaps on the &msg leas (#29/#30 class). Levers that DID close
  (kept): splitting the AppWindow view into a no-virtuals CtorModel (derived, for the
  new-expression) + a standalone methods-only view with a virtual dtor (a derived view
  introducing the vtable pushes the base subobject back 4 bytes and breaks every field
  offset -- documented in src/Main.cpp), the two-call-site FUN_00408130 tail-merge form,
  `if (nPumpBudget < 1)` (NOT `> 0`) for the pump-budget branch layout, `nFrameCount >=
  DAT_00481914` operand order, and int-widened bConfigOk (and eax,0xff). Retry only if
  the 0x4393d0 sete-materialization class cracks.

- `DPlaySessionMgr::LoadOrCreateEasterCard` (0x43e900, v336, first transcription,
  `asmscore.py --len 1405` total 295713, align=294 reg_pen=14 identity_miss=14
  byte_diff=173, insns 461/456; cc.sh DIFF(1139), ours == orig 1405B) —
  `src/DPlaySessionMgr.cpp`. The PostBag Easter-card loader/creator keyed by a car's
  nameA: uniquifies the name against g_pPostBagCache's 16x13 aEasterNames table
  (decimal-index alias via _itoa on collision), builds the
  PostBag\Easter\<Lang>\<name>.crd/.rsp paths, then per retry (up to 0x14): reads each
  file (0x8000-byte buffer), parses the first-line count (buf[2]=0 truncation + atoi),
  picks a random line (rand()/(0x7fff/count), scan from offset 4), strips CRs, strcat's
  the .crd-side line (a card file name) onto the truncated directory path +
  CarNetState_CreateFromFile, expands the .rsp-side line (a "//"->'/', "/n"->CRLF,
  "/?"->local-player-name escape template) into the card's szDescription (0x50 cap,
  retry while it overflows), and finishes with the (newly-identified, v336) CarNetState
  method AssignStampSlotVariantMaybe(result,1,-1) on the +0x94/+0x95 stamp pair.
  Whole-function structure verified against the raw disasm (frame 0x8f98 and every
  local offset byte-identical; a minimal-TU probe reproduces the IDENTICAL score, so
  the residual is intrinsic, not TU context). Residual is FOUR stacked documented
  intrinsic /Og coin-flip classes (see the in-source autopsy): (1) the zero-register
  class in the prologue (orig dedicates ebx=0 for the nRead/bDone/nTry init stores;
  ours eax — the v334/v335 class); (2) slot-vs-register residency on the bCollided
  flag (orig stacks it at frame+0x12; ours keeps it in bl — the v335 (3) class;
  declaration-order, bool-vs-char, assignment-order and != 0 probes ALL NO EFFECT),
  cascading into pCarName spilled-to-memory and pResult in edx where orig keeps both
  in ebx (~half the dump's rows); (3) the loop-entry-guard rotation class on all THREE
  do-while loops with a top break (rewriting the outer loop as an equivalent while()
  produced BYTE-IDENTICAL output — proof the rotation is compiler normalization, not
  source shape; the v329 class); (4) small reassociation residue on the path-surgery
  index (#29/#30 family). Levers that DID close (kept): szExpanded declared [0x320]
  (the frame pins the real declared size; 0x50 is only the logical cap — an unused-pad
  array is dead-code-eliminated by VC5 /O2, confirmed by probe), dual pSrc/pNext
  pointers in the escape loop, for(;;)+two-breaks pinning of the newline-scan bodies,
  unsigned i for the name-table loop (jb not jl), DWORD nRead without (int) casts,
  TU-local view structs reproducing the dead ecx=g_pPostBagCache loads before the
  __stdcall BuildEasterCardPath/CreateFromFile calls and the ecx=pResult load before
  AssignStampSlotVariantMaybe, and strcpy-then-bDone order in the final branch (keeps
  the two strcpy expansions unmerged). Retry only if the slot-vs-register residency
  class or the loop-entry-guard rotation class cracks — (2)+(3) are the bulk of the
  residual.

- ~~`WorldActionCursor::InitTrainCouplingMenuIconsMaybe` (0x458c90)~~ — **RESOLVED v358, now
  EXACT (1257 B).** The SP3 toolchain switch took it 986 -> 5 raw diffs, and the last 5 were
  the documented `test al,al` vs `test eax,eax` width tell: `WidgetBase::SetDescriptor` is
  `virtual unsigned char`, not `virtual unsigned`. Fixing the DECLARATION (not casting at the
  call sites) closed it, with zero effect anywhere else in the repo. See docs/CODEGEN.md.

- `WidgetBaseObj0x4784c8::GetOrCreateMenuIconItemMaybe` (0x4546d0, first transcription,
  `asmscore.py --len 0x146` total 30471, align=30 reg_pen=4 identity_miss=4 byte_diff=31,
  insns 105/104; cc.sh DIFF(172), ours 334B vs orig 326B) — `src/WidgetBase.cpp` (defined
  on the TU-local `WidgetBaseView0x4546d0`, not `WidgetBase.h` itself — adding a new method
  to `WidgetBaseObj0x4784c8`'s own class body would rotate WidgetBase.h's many OTHER TU
  consumers, the same cross-TU hazard already documented for UIResources.h/
  WorldBoardMaybe.h/WorldActionCursor.h). **Renamed/retyped this session — its old Ghidra
  name/typing (`TutorialWnd::ResourceRefCategoryTable_GetOrCreateIconItemMaybe`) was
  refuted**: ground-truthed via raw disasm that the field it reads/writes at `this`+0xd0 is
  exactly `WidgetBaseObj0x4784c8::pMenuListHead`, and ALL confirmed callers
  (`BuildToolButton::InitMenuIconsMaybe`, `WidgetPickerObj0x477cc8::FUN_00427580`,
  `WorldActionCursor::InitTrainCouplingMenuIconsMaybe`) are `WidgetBaseObj0x4784c8`
  descendants — zero real `TutorialWnd` call site exists; `MenuNodeObj0x477568`/
  `UiIconListItem`'s own ctors already independently typed their owner param as
  `WidgetBaseObj0x4784c8 *pOwner`, corroborating the fix. Builds (or appends) a menu-icon
  node onto the owner's own menu list, tracked via a private global tail cursor
  (`g_pMenuIconListTailMaybe`/`DAT_00485270`, confirmed via xref sweep to have ZERO other
  readers/writers anywhere in the binary) that lets repeated calls append in O(1) while the
  owner's own `pMenuListHead` field is written exactly once, by the first call. Residual is
  a single VC5 store-scheduling tie-break: the "first item" branch writes 2 fields
  (`pMenuListHead`, then `g_pMenuIconListTailMaybe`) from the same register; the original
  reaches ONE shared 2-store tail block from all 3 sub-paths (both ctor branches plus the
  allocation-failure fallthrough), while our compile duplicates whichever store comes FIRST
  in source into each of the 3 sub-paths and only shares the SECOND via jump — tried both
  write orders (each duplicates whichever comes first) and both an intermediate local vs.
  reading back through the global directly (the decompile's own literal shape); all
  converged on the same best score. Same family as 0x4597e0's own documented "cross-jump/
  tail-merge class". Retry only if that class ever cracks.

---

## v350 — TU-position sweep results (`tools/possweep.py`), and what they mean

v349 introduced the TU-position lever and got three EXACT matches from it in
`src/PeerTrainNode.cpp`. v350 ran the sweep across **five other TUs** to test how general it
is. **Every single one was FLAT — spread 0, byte-identical score at every legal position:**

| addr | function | TU | profile at baseline |
|---|---|---|---|
| 0x41a360 | `EditCardWnd::SelectDecalSlot` | EditCardWnd.cpp (63 slots) | align=0 reg_pen=2 byte_diff=1 |
| 0x43f880 | `GameNet_RemoveSourcePeerAndReconcile` | DPlaySessionMgr.cpp (21 legal slots) | align=12 reg_pen=14, insns 60/60 |
| 0x447400 | `TickStationClockChimeMaybe` | UIResources.cpp | **v359 SP3 re-score: align=0 reg_pen=0, insns 445/445, byte_diff 12** (one swapped spill-slot pair; the v340 numbers align=194 reg_pen=152 insns 445/448 were RTM artifacts) |
| 0x437900 | `BuildToolCursorWnd::DrawIconFrame` | BuildToolCursorWnd.cpp | align=76 reg_pen=30, insns 60/62 |
| 0x40d340 | `MenuNodeObj0x477568::Draw` | MenuNode.cpp | align=88 reg_pen=30, insns 97/97 |
| 0x40d170 | `MenuNodeObj0x477568::SetNodeState` | MenuNode.cpp | align=160 reg_pen=41, insns 99/91 |
| 0x40cd60 | `NameAnchorMaybe::ApplyDirectionReversalMaybe` | NameAnchorMaybe.cpp | align=464 reg_pen=36, insns 228/224 |

**Conclusion: the position lever is REAL but NARROW.** All three v349 wins were in ONE TU, on
small closely-related functions. Seven functions across six other TUs did not move by a single
byte. Treat the sweep as a cheap last-resort probe (~8s/slot), not a general lever — and note
its other half of value is exactly what happened here: it converts "probably intrinsic" into
measured fact for all seven rows above.

**`reg_pen > 0` is a much weaker filter than v349 assumed.** 180 of the repo's 219 non-exact
functions have `reg_pen > 0`. Two sharper discriminators came out of this sweep:
* **Where the register differences SIT matters more than how many there are.** 0x41a360 has
  `align=0 reg_pen=2 byte_diff=1` — as close to matching as a function gets — but its whole
  residual is ONE instruction's SIB base/index order (`[esi+edx+0x370]` vs `[edx+esi+0x370]`,
  a single byte). That is an operand-ORDER artifact of the source expression, not an
  allocation choice, so no amount of repositioning can touch it. v349's winners had reg_pen
  spread across ~30% of their instructions.
* **A big `align` cost means a structural/source-shape gap, and the sweep cannot fix those
  either.** Fix the source shape first (see 0x40cd60's own note above: three source-shape
  findings took it from 604568 to 468196 while the sweep did nothing).

⚠ Any pre-v350 park whose numbers came from `tools/asmscore.py` on a **free function** (no
`Class::` on its definition line) was scored against the WRONG COMDAT — see the tool fix in
commit `f67e7c1`. 0x43f880 above is the confirmed example: it had been reported as
`insns 169/60, total 822119` when its true score is `60/60, total 13566`. Re-score before
trusting any such row.

## v352/v353 — the PostBag easter sub-switch (shape SOLVED; flag suspicion REFUTED)

For 12 sessions this cluster carried a residual described as "the original emits a flat 0-9
(10-entry) jump table; ours normalizes to 1-9 with `dec eax`", parked as intrinsic across ~12
sites. **The shape half of that is now solved, and the diagnosis was wrong.**

**The real source shape.** The inner switch's two `"\Easter\Eng"` arms are SEPARATE arms, and
`default:` is written FIRST:

```c
switch (g_nEasterLocaleId) {
default: pszCategory = "\\Easter\\Eng"; break;   // written first -> its block is emitted first
case 1: ... case 2: ... case 4: ... case 9: ...  // no case 0, no case 3 in between
case 0:
case 3:  pszCategory = "\\Easter\\Eng"; break;   // written LAST, a distinct arm
}
```

Because `case 0` is a real label the case set's minimum is 0, so MSVC emits the flat 10-entry
table (`cmp eax,9; ja`) with no `dec`. The old source folded `case 0:`/`case 3:`/`default:` into
ONE arm, which drops 0 from the case set, makes the minimum 1, and forces `dec eax; cmp eax,8`.
Restructuring removed the `dec` at all 7 sites in `src/EditCardWnd.cpp` and roughly halved DIFF
across the cluster, with no regressions anywhere:

| addr | function | DIFF before | DIFF after |
|---|---|---|---|
| 0x445400 | `PostBag_BuildAttFilePath` | 77 | 48 |
| 0x445510 | `PostBag_BuildDatFilePath` | 77 | 48 |
| 0x445930 | `PostBag_BuildCrdPath` | 69 | 40 |
| 0x445000 | `PostBagCacheBundle::DeleteCardById` | 133 | 98 |
| 0x4451a0 | `PostBag_DeleteAttachmentFiles` | 194 | 154 |
| 0x445620 | `PostBag_BuildEasterCardPath` | 93 | 93 |
| 0x445700 | `PostBag_BuildClipartFilePath` | MATCH | MATCH |

Ground-truthed against the raw tables, not decompile text: inner-table entries 0 and 3 and the
out-of-range `ja` ALL target the last block, and the original carries a genuinely UNREACHABLE
`mov eax,"\Easter\Eng"; jmp` block at the first arm position that nothing in either table
points at.

**⚠ THE REMAINING RESIDUAL IS TOOLCHAIN-LEVEL, NOT SOURCE-LEVEL.** (v352 read this as evidence
against the Phase-0 flag set; v353 below refutes that.) Both Eng arms assign the SAME string
literal, so VC5-as-we-invoke-it tail-merges the two identical basic blocks into one, losing
exactly one 7-byte block. The original build did NOT merge them: it kept both and pointed every
reference at the second, orphaning the first.

PROOF (reproducible in one compile): make the two arms reference textually DIFFERENT literals.
Relocations are masked by the compare, so only block STRUCTURE is being tested — and
**0x445400, 0x445510 and 0x445930 all go byte-EXACT at the original's exact length**, with
0x445000 dropping to DIFF(48). We deliberately do NOT keep that variant: it would match only by
feeding the oracle a string the game does not contain.

### v353 — mechanism PINNED, and the Phase-0 suspicion is REFUTED. Lead closed.

The mechanism is compiler string pooling after all — v352's probe simply asked the wrong
question (it counted string COPIES in the `.obj`, which is not what pooling decides here).

**`/Gf` is the whole story, and `/O2` is exactly `/Ox` + `/Gf`.** Measured repo-wide, not
inferred from the doc text: `/Ox /Gf` and `/Ox /GF` each reproduce the `/O2` baseline
*to the byte* (38464 B / 235 funcs), and `/Ox` alone is the only variant that moves.
With pooling ON the two `"\Easter\Eng"` literals become ONE symbol, so the two arms' blocks
are literally identical and the optimizer merges them. With pooling OFF they are two distinct
symbols, the blocks differ by their relocation target, and no merge happens.

`tools/progress.py` now honours `$LOCO_OPT` to make this kind of whole-repo flag experiment a
one-liner: `LOCO_OPT=/Ox tools/progress.py`.

| `$LOCO_OPT` | EXACT | funcs |
|---|---|---|
| `/O2` (locked) | 38464 B | 235 |
| `/Ox /Gf` | 38464 B | 235 |
| `/Ox /GF` | 38464 B | 235 |
| `/O2 /GF`, `/O2 /Gs`, `/O2 /Op` | 38464 B | 235 |
| `/Ox` (pooling off) | **35063 B** | **233** |
| `/O2 /Ob2` | 37176 B | 225 |

**Pooling is load-bearing and the locked flags are right.** `/Ox` gains 2 functions in
`src/EditCardWnd.cpp` (0x445400/0x445510/0x445930 go byte-EXACT, 0x445000 98→48, 0x4451a0
154→85) but loses more than it gains everywhere else — only 4 TUs move at all:
`src/CursorDesc.cpp` 6+1→5+2 (**−2006 B**, one large function), `src/WidgetPicker.cpp` 5+9→3+11
(−953 B), `src/DPlaySessionMgr.cpp` 39+25→38+26 (−166 B), `src/EditCardWnd.cpp` 24+39→26+37
(−276 B). Net −3401 B / −2 funcs.

**Per-file flag overrides are ruled out too, decisively.** 0x445930 (wants pooling OFF) and
0x445a40 `PostBag_ReadDatFile` (wants pooling ON — DIFF 16 under `/O2`, DIFF 112 under `/Ox`)
are IMMEDIATELY ADJACENT in `.text`: 0x445930's COMDAT runs to 0x445a3f and 0x445a40 starts at
the next byte. Same `.obj`, therefore same flags, therefore no flag assignment satisfies both.

**And the `/Ox` "match" is byte-coincidental, not semantic — this is the part that closes the
lead.** Read the `/Ox` `.obj`'s own relocations: all 10 inner blocks are LIVE (jump-table
entries cover 9 of them, the `ja` covers the 10th). The ORIGINAL's first block is DEAD — zero
references anywhere in the image, verified by scanning `Loco.exe` for the dword 0x00445973.
So `/Ox` and v352's different-literals variant are the SAME trick (defeat pooling ⇒ no merge ⇒
10 live blocks) and neither reproduces what the original actually did, which is *merge and then
leave the orphan behind*. The bytes agree only because every block is `mov eax,<masked reloc>;
jmp <common tail>` and the relocations are masked.

cl 11.00.7022 (VC5 RTM, matching the image's link 5.10) never leaves that orphan under any
`/O`, `/G` or `/Ob` combination tried. The residual is a compiler-BUILD difference (a VC5
service-pack optimizer revision) or an internal pass we cannot reach from the command line —
not a flag we are getting wrong. **Phase 0 stands. Do not re-open this on the flag axis.**

## v352 — ⚠ `--len` must be the COMDAT extent, NOT the code extent

`toolchain/test/app_funcs.txt` and Ghidra's `Body:` span both give a function's CODE extent,
which EXCLUDES the trailing jump table that is part of the same COMDAT. For switch-heavy
functions that silently truncates `asmscore.py`'s compare window and manufactures a fake
"content is missing" reading: scoring 0x445400 with `--len 193` (app_funcs) instead of its true
`--len 0x110` (272 = code 200 + table 72) reported `insns 88/51`, which reads as ~37
instructions of missing body, when the true score is `insns 88/88` — content-complete. Derive
`--len` from the NEXT function's start address when the function contains a switch.

## v353 — `PeerTrainNode_AllocCarSlot` (0x44c220)

- `PeerTrainNodePartial::PeerTrainNode_AllocCarSlot` (0x44c220, v353, `asmscore.py --len 0xe8`
  total 22016, align=22 reg_pen=0 identity_miss=0 byte_diff=16, insns 69/68) — new transcription,
  content-complete. Appends one freshly-constructed car to the train (refuses once 3 slots are
  used or the slot one past the end is occupied; bumps the count, `new`s a 0x450-byte CarNetObj
  under `/GX`, stamps the car's owner back-pointer, and rolls the whole thing back — `delete` +
  slot clear + count decrement — if the ctor left `bValid` clear). Every block is structurally
  identical, including the EH-state stores, the count-bump-before-construct order and the
  vtable-slot-0 scalar-deleting-dtor call.
  Sole residual is WHERE the constant 1 is materialized: the original hoists `mov eax,1` above
  the slot reload so AL can be the memory-compare operand (`cmp byte ptr [ecx+0x18], al`); this
  build materializes it late and spends one extra instruction loading the byte first
  (`mov dl,[ecx+0x18]; mov eax,1; cmp dl,al`) — exactly the 69/68 gap.
  Probes (one compile each): `!= false` instead of `== true` → byte_diff 19 but total 42679
  (structurally further away — load+test+separate `mov al,1`); an explicit `unsigned char
  bAdded = 1;` hoisted above the null check and returned → BIT-IDENTICAL (constant-propagated
  back); a cached `pCar` local instead of re-reading `carSlots[wCarSlotCount]` → WORSE, total
  34020. VC5 /Og constant-materialization scheduling class. `src/PeerTrainNode.cpp`.
  **The `pCar`-local probe is the reusable lesson**: caching the pointer lets value-numbering
  fold away the null re-test MSVC's `delete` lowering emits, and the original DOES emit it.
  When a `delete` site is missing its redundant `test reg,reg`, stop caching the operand in a
  local and re-read it from memory at the delete.

## v354 — `PeerTrainNodePartial::PeerTrainNodePartial` (0x44be50)

- `PeerTrainNodePartial::PeerTrainNodePartial` (0x44be50, v354, `asmscore.py --len 598`
  total 51696, align=50 reg_pen=15 identity_miss=15 byte_diff=46, insns 184/187) — new
  transcription, content-complete (candidate len 598 == the original's own body extent). The
  train node's constructor: zero the node, give it a lead `NameAnchorMaybe` and a car 0 of kind
  `nKindId`, then — only for a locally originated train (`bPeerOwned == 0`) — mint a train id,
  park the anchors just off the top of the board and file the placement with the session roster.
  Every block is structurally identical: the `/GX` EH-state ladder across both protected `new`s
  (-1 → 0 → -1 → 1 → -1), BOTH car-slot clears (the original really does clear the 4 slots twice,
  at 0x44bec4 and 0x44bf00), the `rep stos` of the 8-dword `aUnk0x38Maybe` block, the
  re-read-per-use `carSlots[wCarSlotCount]` shape (the same shape as `PeerTrainNode_AllocCarSlot`,
  and the reason the `delete`'s redundant null re-test survives), and the whole
  locally-originated-train tail.
  Three residuals, all register-allocation/scheduling tie-breaks:
  (1) the scheduler emits `xor ebx,ebx` one slot later than the original — a pure swap with the
      adjacent `mov [esi+4],ecx`;
  (2) the original spends one extra `mov eax,ecx` copying the freshly-incremented train id out of
      ECX so ECX is free for the following thiscall's `this`; this build already has the value in
      EAX and needs no copy;
  (3) `SetTrainPlacementResult`'s byte arguments — the original widens `bOwnerByteA` to a dword
      through a dead incoming-parameter stack slot (`mov [esp+0x34],al; mov eax,[esp+0x34]`),
      this build pushes the register directly. Both are the SAME don't-care byte→dword widening;
      which one you get is allocator-driven.
  Source-shape levers that DID land (one compile each, DIFF 261 → 174 before the type fix):
  `this->wSelectedCarId = this->wSelectedCarIdAMaybe;` — reading the member BACK rather than
  re-reading the descriptor field, which is what produces the original's `mov cx,dx`
  store-forwarding copy (DIFF 233 → 174); an `int` local for `selectedProviderIndex` so one dword
  load feeds two byte stores instead of two byte loads; and
  `int n = mgr->nNextTrainId + 1; mgr->nNextTrainId = n;` instead of `++mgr->nNextTrainId` used as
  a value — the `++` form made MSVC re-read BOTH the global pointer and the field after the store.
  A fourth lever was a genuine TYPE fix, not a park: the placement-result family's `bOwnerB` is an
  `unsigned char`, not an `unsigned int` (total 73592 → 51696) — see the note in
  `src/DPlaySessionMgr.h`. It cost no exact match anywhere and also improved
  `HandleQueuedPlacementEvent` (321 → 268) and `HandleQueuedTrainPlacement` (208 → 113).

### `UIResources::TileKind_CreateDescriptor` — 0x446840 (v355) — EFFECTIVE

`asmscore.py --len 0x47c`: total 305655, align=304 reg_pen=14 identity_miss=13 byte_diff=125,
**insns 357/341**. Candidate COMDAT 1164 B vs the original's 1148 B (1088 B of code + a 60-byte
jump table) — the 16-byte excess is exactly the three residual expansions, so the function is
content-complete, not a partial draft. ⚠ `--len` here MUST be `0x47c`, derived from the next
function's start (0x446cc0); Ghidra's `Body:` span stops at the last instruction and would clip
the jump table, fabricating a "missing body" reading.

The per-kind descriptor factory on the `UIResources` registry: picks a descriptor class from
`TileKind_GetCategory(kindId)` (and, inside the paired categories, from whether the id is even or
odd), constructs it from the kind's RT_STRING definition text, interns it in
`m_apKindDescriptors[kindId]`, then requires it to report itself loaded — else `delete`s it and
poisons the slot with -1.

**Everything except three inlined `TileKind_GetCategory` expansions is BYTE-IDENTICAL**: the
prologue and its `push ecx` result-local slot, the entry guard, all 14 `new` arms with their
correct `operator new` sizes, the `/GX` EH-state ladder (states 0..0xd), the jump-table dispatch,
the cross-jumped shared `m_apKindDescriptors[kindId] = pNew` tail the 12 plain arms share, both
special-cased store-then-flag arms (categories 5 and 14), the `delete` tail with its surviving
null test, and the epilogue. Everything the `--dump` reports past offset 0x440 is masked
jump-table relocation noise, not a real disagreement.

Levers that landed (one compile each): the `unsigned char bResult = 1;` result local instead of
literal `return 1;`/`return 0;` (DIFF 918 → 838, and it restored the whole prologue including the
frame-slot displacements); the accept/reject guard chain and the `delete` written against the
member array expression rather than a cached local (restores `delete`'s null test); and the
`/GX` EH-state ladder read as a source-case-order oracle, which got the non-numeric case ordering
right on the first compile. All three are now bullets in `docs/CODEGEN.md`.

Residual: the `sbb`-mask clamp class — see `docs/CODEGEN.md`'s "UNSOLVED CLASS" bullet and the
in-source autopsy on `TileKind_GetCategoryInline` in `src/UIResources.cpp`. Eight source shapes
probed against the standalone copy at 0x446030; none reproduces the byte spill before the
compare. **Do not re-grind this on the register axis** — it is one source-shape question, shared
with 0x446030's own long-standing park, and cracking it closes both at once.

- `UIResources::SoundBank_PreloadWavRange` (0x446cc0, v356, first transcription) — EFFECTIVE.
  `asmscore --len 0x1e0`: total 149517, align=148 reg_pen=13 identity_miss=13 byte_diff=87,
  insns 140/139; candidate 472 B vs the original's 480 B (437 code + a 40-byte jump table).
  Structure confirmed end-to-end against the raw disasm: the 0x6060 clamp, the rotated
  `for` over the id range, the shutdown gate (byte-predicate, matches), the 100..500 locale
  remap switch with all 8 language arms + the shared default/out-of-band arm, both
  `LoadStringA(GetModuleHandleA(NULL), ...)` blocks with the retry-on-raw-id path, the `new`
  with its /GX EH-state bracket, and every one of the three `-1` poison stores. Residual is
  ONE four-instruction cluster: the original CSEs the constant -1 into ESI (`or esi,0xffffffff`)
  and reuses that register for BOTH its compiler-generated EH-state restore and the source's
  slot poison, and emits the array store before the EH restore rather than after. The EH-state
  slot is compiler-owned, so that CSE is not directly expressible in source. Probed without
  effect: naming the poison as a `SoundBankEntry *` local initialized to -1, and folding the
  array store into the `new` full-expression. **Two source shapes here ARE load-bearing and must
  not be "simplified": the byte-returning `IsShuttingDownMaybe()` predicate, and the `int nSlot`
  index local (see docs/CODEGEN.md's induction-variable bullet — inlining it costs 65k).**

- `PostBagFileCache::FindFirstLoadableCardAtOrAfterIndex` (0x401c10, v361, `asmscore.py
  --len 0x74` total 26248, byte_diff 28, insns 38/39) — v361 corrected the MODEL: the two
  `mov ecx,[g_pPostBagCache]` loads parked since v168 as "unrelated, unused global reloads" are
  the two callees' `this`. `PostBag_BuildCrdPath` (0x445930) and `CarNetState_CreateFromFile`
  (0x444c70) are both `__thiscall` `PostBagCacheBundle` members that never read `this`; every call
  site of each loads ecx from the singleton (see `src/PostBag.h`). Modelling them as members took
  insns 36/39 → 38/39. (This row is NEW — the in-source note had said "see docs/PARKED.md" since
  v168 but no row was ever added.)
  Sole remaining residual is ONE instruction: the original materializes the record address
  (`lea eax,[esi+ecx+0x14]` … `mov eax,[eax]`, with the first ecx load sandwiched between) where
  this compile fuses it into one SIB load, and so pushes the literal `0` argument one slot later.
  Probed and REFUTED: a `PostBagAlbumIndexRecord *pRecord = &pRecords[i];` pointer local (emits
  `add eax,esi` + `mov edx,[eax+0x14]`, total 36700, WORSE) and an implicitly-inline
  `RecordAt(int)` accessor returning `&pRecords[i]` (still fuses the load — it only shuffles
  registers, total 22344). So this is NOT the v360 inline-accessor class despite the superficially
  matching "materialized lea vs fused load" symptom. `src/EditCardWnd.cpp`.

## 0x420ec0 `SplashWnd::OnUnhandledMessageMaybe` — cross-jumped duplicate tail (v378)

PARTIAL, total 165222, insns 199/219 under `--len 818`. Content-complete; the entire
20-instruction / 65-byte residual is ONE cross-jump. The boot-video sequence's two steps end with
textually identical tails (`SetWindowLongA` / `ShowWindow` / `SetFocus` / `EnableWindow` /
`break`), which cl 11.00 merges — case 0's test becomes `jne` into case 1's copy. The original
keeps both copies inline.

**This is v377's duplicated-tail class at its documented HARD LIMIT, and the second confirmed
instance of it.** Ran v377's own diagnostic: the two original copies are NOT
byte-identical-apart-from-displacement — they differ in register allocation *and* scheduling
(copy 1 `mov [esi+0x218],eax; mov eax,[esi+0x210]` … eax/ecx/edx; copy 2
`mov edx,[esi+0x210]; mov [esi+0x218],eax` … edx/eax/ecx). The original duplicated BEFORE
register allocation; cl 11.00 cross-jumps BEFORE it. Two textually identical source blocks cannot
be told apart, so no source shape reaches this. **Do NOT re-probe.**

Probed and REFUTED: writing case 0's failure as an explicit `goto` into case 1 — the only way C
can express a CONDITIONAL fallthrough — scores 454764 (much worse), because it also reorders the
surrounding blocks. Remaining diff beyond the merge is ordinary reg-alloc noise (reg_pen 28).
`src/SplashWnd.cpp`.


---

## `MailWnd::OnSetFocus` (0x42ff20) — v383, EFFECTIVE

DIFF(15), **insns 32/32**, `align=12 reg_pen=5 identity_miss=5 byte_diff=10`. Structurally
identical; the whole residual sits in the four-argument tail forward to `DefWindowProcStub`
(0x422ea0). The original schedules it across TWO registers, interleaving loads and pushes
(load lParam→edx, load wParam→eax, `push edx`, load msg→edx, `push eax`, load hwnd→eax, …);
cl hoists THREE loads (ecx/edx/eax) before the first push. Same operands, same call, same count
— a register/scheduling coin-flip of the documented class.

Probed without effect: routing the result through a named `LRESULT` local.
Probed and **REFUTED**: calling `DefWindowProcA` directly instead of the 0x422ea0 stub
(DIFF 15 → 20). That refutation is itself the useful result — it confirms the direct call to the
shared stub is the right model here, which the disasm independently shows (a plain 4-argument
`__stdcall` call with no `this` in ECX, so NOT a base-qualified `WindowBase::OnSetFocus(...)`).
`src/MailWnd.cpp`.

## `MapWnd::OnMouseMove` (0x432540) — v388, EFFECTIVE

DIFF 36 → **14**, **insns 156/156**, `align=0 reg_pen=14 identity_miss=14 byte_diff=14`.

**What the 22 bytes of the v387 residual actually were, and how they closed:** the hover-CHANGED
path `return`s from inside its own `if` rather than falling through to the function's shared
transition tail. Proof in the original: the `je` at 0x43262c (`nSlot == nHoverSlotMaybe`) jumps
clear over a full inlined copy of `RequestModeTransitionFromSource(...)` and lands on the SHARED
tail at 0x4326a4 — the two tails are only distinguishable because the branch-private copy has
somewhere to sink `nHoverSlotMaybe = nSlot`, which the scheduler drops at 0x432655, between
`mov ecx,esi` and the `call`. Writing the call once after the `if` merges both paths onto one
tail, and the store is then stranded ahead of the whole call setup. This is a THIRD instance of
the "original duplicates where the candidate merges" class, and the cheapest one to spot: look at
where the equality branch LANDS, not at how many tails exist.

**Residual (parked):** the two LAST-emitted transition tails, plus the one-instruction `pt.y`
shuttle at 0x134, allocate one register step around the eax→ecx→edx cycle (orig `+0x64`→eax,
`+0x60`→ecx, vtable→edx; mine ecx, edx, eax). Every instruction, operand and block boundary
agrees. The FIRST tail — the branch-private one — already allocates exactly like the original
from the same source line, which is what rules out a source-shape cause: one call expression
cannot be written two ways in one place. Per-block allocator tie-break. `src/MapWnd.cpp`.

## `MapWnd::OnKeyDown` (0x430ef0) — v388, EFFECTIVE (order SOLVED, encoding parked)

DIFF **27**, insns 72/72, `align=16 reg_pen=0 identity_miss=0 byte_diff=23`. Unchanged in size
from v387, but the open question is now closed: **the original's source order for `srcRect` is
`right, left, top, bottom`.**

`srcRect` is address-taken (`OffsetRect(&srcRect, …)`), so it has a real stack home and its
stores follow source order verbatim — the disasm IS the answer, it just has to be resolved
properly. Base = E+8, pinned twice (the `lea eax,[esp+0x10]` at 0x430f42, and independently by
the four reload displacements at 0x430f61..0x430f7f that push the rect by value). Against that
base: 0x430f3d→`+8` right, 0x430f47→`+0` left, 0x430f4f→`+4` top, 0x430f57→`+0xc` bottom. Note
two of the four stores share the literal displacement `[esp+0x14]` while meaning different
fields — three pushes land between them — so reading the order off the raw displacements gives
the wrong permutation.

**Why the source below is written right/bottom/left/top anyway.** Three encodings measured:

| source order | DIFF | total | what's wrong |
|---|---|---|---|
| right, bottom, left, top (incumbent) | 27 | 16023 | store order wrong; registers right |
| right, left, top, bottom (TRUE order) | 27 | 36020 | stores right; height load sinks below them |
| `int nHeight` local + true order | 40 | 31720 | rect block byte-EXACT; one-step rotation downstream |

The original hoists BOTH 16-bit loads to the top (0x430f33/0x430f39, which kills the descriptor
pointer early) while still storing `bottom` last. cl only hoists the height load when the value
is routed through a named local; doing so makes the entire rect construction byte-identical but
displaces the whole cost into a one-step `eax→ecx→edx` rotation across the two by-value RECT
argument copies — **the same rotation class as `OnMouseMove`'s parked residual in this same TU**,
which is the real target for a future session. Also measured and REFUTED: inlining the `pIcon`
local into the call (68/72 insns, DIFF 173) — that local is load-bearing, it is what keeps the
bitmap in EDI early. `src/MapWnd.cpp`.

## `DecorActorBase::~DecorActorBase` — 0x433be0, DIFF 68 (v390)

`src/DecorActor.cpp`. Structurally identical, insns 26/25 (candidate/original). The whole
residual is one extra callee-saved register: our candidate hoists
`pOwner->apSpawnedActorMaybe` into `ebx` — paying a `push ebx`/`pop ebx` pair — and peels the
occupant-scan loop's first compare, while the original keeps the entire walk in `edx`/`ecx` and
rotates the loop the other way. Downstream of that it also emits `dec byte ptr [eax+0x8e]`
where the original does the load/dec/store triple `mov dl,[eax+0x8e]; dec dl; mov [eax+0x8e],dl`.

Measured and REFUTED (both left the score at exactly DIFF 68):

| probe | result |
|---|---|
| `x = x - 1` instead of `x--` for the occupant-count decrement | no change |
| seed the walk pointer from `pOwnerObjMaybe` instead of the cached `pOwner` local | no change |

Register-allocation / loop-rotation tie-break class. The sibling in the same TU,
`TickIdleDecayMaybe` (0x433c50), IS byte-exact, so this is not a TU-position problem.

## v391 — `src/WalkerActor.cpp`, the nine non-exact slots

The whole class transcribed in one session; 5/14 byte-exact. Every residual below is
CONTENT-COMPLETE (instruction counts within a few of the original, and every branch
cross-checked against raw disasm), so these are register-allocation / block-layout
tie-breaks, not missing body — with the two exceptions called out in the notes.

Three source-shape levers were found and applied while grinding these; they are what took the
TU from its first-compile state to here, and they are documented in `docs/CODEGEN.md`:
the (x,y) pair-equality inline predicate pair, `dec eax; je` == `switch`, and the
"declare locals at the top so every early exit can `goto` one shared return" shape.

| addr | function | profile at baseline | note |
|---|---|---|---|
| 0x433220 | `WalkerActor::FollowLeaderStepMaybe` | align=28 reg_pen=21, insns 123/124 | closest of the nine. The original materialises the shared `-1` into a register (`or ecx,-1`) and compares memory against it twice; our candidate instead loads the first member and reuses THAT register as the `-1` for the second compare. Same predicate, different constant-CSE choice. |
| 0x4327b0 | `WalkerActor::TickMaybe` | align=54 reg_pen=18, insns 122/127 | 250120 → 56021 across the session (the `switch` lever, then the non-virtual `DecorActorBase::HeadForObjectMaybe` call). 5 instructions short; the residual is in the switch's own case-block layout. |
| 0x432fd0 | `WalkerActor::AdvanceMovementMaybe` | align=70 reg_pen=42, insns 127/128 | 671641 → 74690 once the on-foot branch was put FIRST (it is the fall-through) and the `CanStandAtMaybe`/`TryEnterTrackTileMaybe` pair correctly NESTED. Probed and measured WORSE (74690 → 119062): using `pt.x`/`pt.y` directly instead of caching them into two int locals. |
| 0x433530 | `WalkerActor::BoardTrainMaybe` | align=130 reg_pen=22, insns 116/128 | 12 instructions short — the only residual here with real body still missing. Suspect the pre-plan `do/while` walk's exit conditions: the original's loop has three separate breaks and our rendering may be folding two. |
| 0x432940 | `WalkerActor::SetDestinationTileMaybe` | align=174 reg_pen=21, insns 125/125 | instruction count EXACT; pure scheduling/allocation. |
| 0x433860 | `WalkerActor::CanStandAtMaybe` | align=176 reg_pen=16, insns 154/157 | 470178 → 177858. Fixed en route: two duplicated epilogues (top-declared locals + one shared `done:` return), the signed-`int` category widening, and the `> 4`-first branch order. |
| 0x4336a0 | `WalkerActor::LeaveTrainMaybe` | align=164 reg_pen=24, insns 138/149 | 11 short. 214789 → 166756 once the 0x13 drop-off jitter was DERIVED rather than guessed: `lea ebp,[eax+ecx*1-0x5]` with `eax = ±1` makes it `(stepY - hotspotY) - 5 + (rand()%2*15 ? -1 : 1)`, not the `(±1)*15 - 4` first transcribed from the decompile — and the dead `*15` inside the test is real, see the `// sic:` in the body. The remaining 11 instructions are unaccounted for; the fallback path (`pScheduleAwayObjMaybe`) is the least-verified part left. |
| 0x433370 | `WalkerActor::TryEnterTrackTileMaybe` | align=266 reg_pen=47 identity_miss=68, insns 161/167 | the high identity_miss says several call targets/constants still land differently; the two `rand()%2 ? -1 : 1` sign expressions are the likeliest culprits. |
| 0x432ae0 | `WalkerActor::ChooseNextStepTileMaybe` | align=1088 reg_pen=98, insns 343/361 | the big one (1264 B). 18 instructions short. Structurally verified branch-by-branch against the decompile, but its three-way case split plus two nested `goto` tails leave a lot of block-layout freedom; the best next lever is probably to settle whether the "on a graph node" and "between nodes" arms share a tail in the original. |

## v392 — `src/RoadVehicleActor.cpp`, the three non-exact slots

The whole class transcribed in one session; **6/9 byte-exact** (ctor, dtor,
`ApplyLaneOffsetMaybe`, `AdvanceMovementMaybe`, `UpdateFacingFromStepDeltaMaybe`,
`CanStandAtMaybe`). All three residuals below are CONTENT-COMPLETE — instruction counts are
within 6 of the original and every branch was cross-checked against raw disasm — so these are
register-allocation / block-layout tie-breaks, not missing body.

Five source-shape levers were found while grinding these and are what closed the six exact
matches; all are written up in `docs/CODEGEN.md`: the top-declared-and-ASSIGNED struct return
buffer, "two ints that won't swap slots are one struct", the byte-typed ternary zero arm, the
16-bit category local, and MSVC 5's four-cases-before-a-jump-table threshold.

| addr | function | profile at baseline | note |
|---|---|---|---|
| 0x453450 | `RoadVehicleActor::TickMaybe` | align=84 reg_pen=21, insns 93/97 | closest of the three, and untouched by any of this session's levers (they all landed elsewhere). The original burns one more callee-saved register (`push ebx` + `push esi`, ours `push esi` + `push edi`) and consequently RE-READS `pRoad->hotspotPosX` for the `SetDestinationTileMaybe` call instead of reusing the value the preceding pair-compare already loaded. Same constant-CSE family as v391's 0x433220. |
| 0x453570 | `RoadVehicleActor::SetDestinationTileMaybe` | align=186 reg_pen=24, insns 174/180 | 385512 → 188763. Fixed en route: the two-separate-ternaries + 16-bit `nCategory` extraction, and inverting the `nGraphNodeIdMaybe != -1` branch so the "already a graph node" case is the fall-through. What is left is one stack slot: the original keeps `nCategory` in memory at `esp+0x10` and reloads it on the `pDestTile == 0` path (`push ecx`, one local dword), ours keeps it in a register and spills something else instead (`sub esp,8`, two local dwords). Also unresolved: the original loads BOTH halves of `ptDestMaybe` into registers for the pair-compare where ours uses a memory operand for the second. |
| 0x453790 | `RoadVehicleActor::ChooseNextStepTileMaybe` | align=730 reg_pen=119, insns 444/449 | the big one (1499 B, and the largest single function in the app region). 1100931 → 743664. Fixed en route: `nDirIn`/`nDirOut` as BYTE locals rather than a `uint`+`uchar` pair, the unsigned `> 0x900` entry-distance test, and the declaration order that puts `nCategory`/`nKindId` ahead of the two direction bytes. 5 instructions short. The remaining cost is concentrated in the cost-argmin loop's induction variable — the original strength-reduces to a walking pointer (`lea ebp,[edi+0xfc]`; `mov ecx,[ebp]`), ours to a negative base (`mov edx,0xffffff08; sub edx,edi`) — which is the same documented induction-anchor class as `docs/CODEGEN.md`'s struct-pointer-walk lever, and the best next thing to try. |

### v395 — `src/BuildToolButton.cpp` (`InitMenuIconsMaybe`, and a v394 regression)

| addr | function | profile at baseline | note |
|---|---|---|---|
| 0x449600 | `BuildToolButton::InitMenuIconsMaybe` | align=112 reg_pen=0 identity_miss=0 byte_diff=31, insns 152/140, total 112031 | 413 B, DIFF(311) at TU position, 295 in isolation. Transcribed v395 and CONTENT-COMPLETE — every descriptor id, the 20-iteration counted loop over TileKind 0x2400..0x2413, both cached-node cases (0x2406 → `pAutoCurveConnectMenuItemMaybe`, 0x240c → `pMenuItem0x240cCachedMaybe`), the spawner teardown/respawn pair and all eight closed-state stores verified instruction-by-instruction against the raw disasm. TWO residuals, both intrinsic. (1) TEN of the twelve extra instructions are the VC5 **cross-jump/tail-merge** class: the original funnels the first two guards into ONE shared `xor al,al; pop*4; ret` block at +0x13c (`je 0x13c` twice) while keeping guard 3's own `return 0` as a separate al-already-zero epilogue; ours duplicates the first two inline. BOTH plausible source shapes were measured — writing guards 1-2 as NESTED ifs over a single trailing `return 0` (which is literally what `je <far shared xor block>` means, and what Ghidra renders) DOES reproduce those two jumps exactly, but cl then merges guard 3 AND the final guard into that same far block too (total 164371, insns 137/140), strictly worse than the early-return form kept (112031). cl's merge threshold, not a source-shape bug. (2) The other two are the `setne`-widening sub-case: original `test al,al; setne bl` vs ours `mov bl,al; neg bl; sbb ebx,ebx; neg ebx`; `bool x = f();` (implicit C4800), `bool x = f() != 0;` and `unsigned char x = f() != 0;` are all BYTE-IDENTICAL, exactly as v394's sub-case (3) predicts. Do not re-grind either. |
| 0x449dc0 | `BuildToolButton::RepositionWithHotspot` | — | **REGRESSED 119 → 572 (len 753 → 746) in v395, at zero cost in exact bytes** (it was already non-matching). Cause BISECTED to a single line: `src/BuildToolButton.cpp`'s new `#include "NetSessionEventQueue.h"`, which `OnPressReleaseMaybe` needs for its `SaveBoardLayout` call. Ruled OUT by measurement: the WidgetBase.h `GetOrCreateMenuIconItemMaybe` declaration (HEAD's .cpp against the new header still gives 119/753), the other two new includes (`UIResources.h`, `ScreenSaver.h` — both individually neutral), and function ORDER within the TU (moving `InitMenuIconsMaybe` past `RepositionWithHotspot` changed nothing). If a future session wants this one exact, the lever is to make `NetSessionEventQueue.h` lighter (it drags in `Pair16.h` plus the two inline-bodied `Obj0x477758Base`/`Obj0x477758` record types) rather than anything in this function's own source. The v393/v394 shape lessons below all still stand. |

### v393/v394 — `src/BuildToolButton.cpp`

| addr | function | profile at baseline | note |
|---|---|---|---|
| 0x449dc0 | `BuildToolButton::RepositionWithHotspot` | align=74 reg_pen=29, insns 238/237, total 77267 | CONTENT-COMPLETE, and as of v394 the compiled length is EXACT (753/753). 634 → 342 (v393) → **119** (v394). Three source-shape levers, all load-bearing: (1) every clamp comparison written in the original's own operand order (`nX > nLimit`, not `nLimit < nX` — MSVC emits `cmp` operands in source order, so the reversed form flips jg/jl throughout); (2) the first `bSuppressRectBMaybe` test as an explicit `== true` (the original shares the constant 1 in eax with the `nButtonStateMaybe != 1` compare above it: `mov eax,1` / `cmp [esi+0xad],al`); (3) **v394 — the SECOND `bSuppressRectBMaybe` test is a PLAIN `if (bSuppressRectBMaybe)` whose FALL-THROUGH arm is the `pKindDesc->nativeWidth + nX` one** (`mov al,[esi+0xad]; test al,al; je <other arm>` at 0x449ef5). v393 had it as `== false` with the arms the other way round, which is the same predicate but the opposite branch layout; swapping the arms alone was worth 252 → 119 and closed the last length gap. Residual is purely the clamp chain's working-copy register assignment: the original loads `x` straight into edi at 0x0012 and compares with edi throughout, where ours loads it into edx in the prologue (`mov edx,[esp+0xc]` at 0x0003), copies to edi, and then RE-READS the parameter slot for each `cmp` because copy-propagation proves `nX == x` on those paths; the three clamp arms also disagree on whether `pKindDesc->nativeWidth` or the viewport width gets CSE'd into a register first. Tried and REJECTED: flipping the hit-area rebuild to test `!= 0` first (342 → 448); declaring `nY` before `nX` (no change). |
| 0x4497a0 | `BuildToolButton::AdvanceAnimFrameMaybe` | align=6 reg_pen=2, insns 353/352, total 6228 | 1113 B, DIFF(345) — the per-frame tick, transcribed v394. CONTENT-COMPLETE: the ONLY disagreement in the whole function is three bytes at the tutorial gate. The original is `mov eax,[0x4fd3ac]; cmp [eax+0x7c4],2; setne al; test al,al; je` — reusing EAX for both the pointer and the byte, with NO zero-extension. Ours emits `mov ecx,[g]; xor eax,eax; cmp [ecx+0x7c4],2; setne al; test al,al; je`. The `setne`+`test` pair itself IS steerable and is worth having: a plain `if (g_pDPlaySessionMgr->connectionMode != 2)` drops both instructions (350/352 insns, total 12005 — strictly worse), so `docs/CODEGEN.md`'s byte-returning-inline-predicate lever is correct here. What is NOT steerable is the widening: FIVE spellings compile byte-identically to each other (`unsigned char` inline predicate, `bool` inline predicate, an `unsigned char` predicate taking the manager pointer as a parameter, an `unsigned char` LOCAL assigned from the comparison, and an explicit `(unsigned char)` cast in the `if`). This is a NEW sub-case of the sete family: the documented v356 originals all CONTAIN the `xor r,r`, and this is the first original found WITHOUT one. Two levers landed while transcribing and are load-bearing: `nAnimValueCache` is UNSIGNED (`fild qword` off a zeroed high dword, DIFF 883 → 345) and `bDraggingMaybe` is spelled `== true` at its first two test sites but PLAIN at the third. |
| 0x449d00 | `BuildToolButton::ContainsAnyRegionMaybe` | align=88 reg_pen=3, insns 51/47 | 115 B, DIFF(61). The original normalizes each sub-region's `char` hit result to 0/1 in AL (`test al,al` / `setne al`) and outlines the `bHit = true` arm past the `ret` as a shared `mov al,1; jmp` tail; ours normalizes into EAX (`neg al` / `sbb eax,eax` / `neg eax`) and materializes all three arms inline. Four local/return type combinations tried — `bool` local with and without `!= 0`, `char` local, `int` local, plus a `bool` return type — and the first three are BYTE-IDENTICAL to each other, so the local's type is not the lever here; `int` was strictly worse (DIFF 64). |

(For 0x44ac20 `BuildToolButton::HandleMenuCommandMaybe` — parked v499 — see the v499 section at the top of this file; its residual is the same xor-pre-clear sub-case as 0x4497a0's gate above.)

## `TutorialWnd::RefreshClientRect` (0x450d60) — v398, EFFECTIVE (3 bytes, one CSE coin-flip)

`asmscore.py src/TutorialWnd.cpp 0x450d60 --len 1054`: **insns 351/352, align 6, reg_pen 9,
identity_miss 9, byte_diff 8, total 6998**; `cc.sh` reports DIFF(437) at len 1051 vs the
original's 1054.

Content-complete. All nine `pErrObj*` layout blocks, the desktop re-centering, and the whole
tail are byte-identical; the diff is six rows in one place — the `pErrObj5` realized-ness guard:

| offset | original | ours |
|---|---|---|
| 0x24a | `mov eax, [esi+0x134]` (pErrObj5 → EAX) | `mov ecx, [esi+0x134]` (→ ECX) |
| 0x250 | `mov ecx, [eax+0x14]` (pCursorDesc → ECX) | `mov eax, [ecx+0x14]` (→ EAX) |
| 0x25b | `mov ecx, [eax+0x18]` (reuses ECX, killing the descriptor) | `mov edx, [ecx+0x18]` |
| 0x266 | `mov eax, [eax+0x14]` — **re-loads the descriptor** | *(absent — reuses EAX)* |

The original burns ONE register (ECX) for both guard temps and re-loads `pCursorDesc` for the
body; cl instead CSEs the guard's load, which forces a third register (EDX) for
`nRealizedHandle` and back-propagates EAX onto the descriptor. From the `SetRect` at 0x266
onward the two are byte-identical, so the 3-byte gap is exactly that one absent re-load.

**Probed and REJECTED (all one compile each):**
- Negating the condition and swapping the arms (`!a || !b` + empty-first vs `a && b` +
  empty-last): **fully inert** — byte-for-byte identical output. cl canonicalizes the
  short-circuit chain, so arm order is not a lever here.
- Hoisting the descriptor into a then-block local (`CursorDesc *pDesc = pErrObj5->pCursorDesc;`):
  also fully inert. The CSE happens regardless of source spelling.
- Writing the two empty arms out as a duplicated `if / else if` pair, per the v397 #18i
  "write the duplication out to GET a cross-jump" lever: **strictly worse** (1059 B, DIFF 449,
  insns 355/352). cl emits the first empty arm inline and does NOT cross-jump it into the
  second. Useful negative result: #18i is a switch-tail lever, it does not generalize to
  if/else-if arms.

Verdict: intrinsic CSE/register-allocation coin flip, same family as the OnExit row below.
Do not re-grind.

## `TutorialWnd::OnExit` (0x450ae0) — v396, EFFECTIVE (2 bytes, register coin-flip)

`asmscore.py src/TutorialWnd.cpp 0x450ae0 --len 0x1c0`: **insns 112/112, align 0, reg_pen 4,
byte_diff 12, total 452**; `cc.sh` reports DIFF(41) at len 435 vs the original's 433.

Content-complete and structurally identical — every instruction pairs up and the block layout
is aligned to the byte, including the `dec eax; je / dec eax; je / dec eax; je` ladder with the
default arm inline. The entire residual is one intrinsic register tie-break appearing twice, in
the two out-of-line switch arms that reload the target singleton after their `SetCaptureMode`
call:

| offset | original | ours |
|---|---|---|
| 0x16b (case 3, EditCardWnd) | `mov eax, ds:0x4fd380` (A1, 5 B) | `mov edx, ds:0x4fd380` (6 B) |
| 0x1a3 (case 1, MailWnd)     | `mov eax, ds:0x4fd37c` (A1, 5 B) | `mov ecx, ds:0x4fd37c` (6 B) |

…plus the dependent register rename on each arm's following store. The 2-byte length gap is
exactly these two 5-vs-6-byte encodings (EAX has the short absolute-load form, EDX/ECX do not).

**Why this is a coin-flip and not a source lever:** the case-2 arm is written in the identical
shape (`g_pAlbumCardWnd->SetCaptureMode(0);` then a field store) and cl DOES pick EAX there, so
the same source spelling produces both outcomes within one function.

**Probed and REJECTED:**
- if/else-if chain instead of `switch` — much worse (DIFF 117, align 308, insns still 112/112).
  It relocates the default arm out of the fall-through slot the original's `dec eax` ladder puts
  it in, and re-orders all three out-of-line arms. This is a useful negative result in its own
  right: the `dec eax; je ×3 + inline default` shape IS the `switch` shape, so the switch is
  confirmed correct and no future session should re-try the chain.

Verdict: same class as the Yoda #29/#30 register-swap family. Do not re-grind.

## `TutorialWnd::ResourceRefCategoryTable_LoadCategoryFile` (0x44fb10) — v396, EFFECTIVE

`asmscore.py src/TutorialWnd.cpp 0x44fb10 --len 0x170`: **insns 112/112, align 60,
byte_diff 38, total 61358**; `cc.sh` DIFF(121) at len 369 vs the original's 367.

Content-complete. Three real source levers were found and applied, taking it
**168870 -> 61358 in three compiles**; all three are reusable and are written up in
`docs/CODEGEN.md`:

1. **Local declaration ORDER is load-bearing when one of the locals is a non-POD.**
   `char szPath[0x105] = "";` must be declared BEFORE `ifstream fileStream;` — the original
   inits the path buffer (pooled-empty-literal byte copy + `rep stosd` tail zero) and only
   then runs the stream constructor. `char bLoaded = 0;` also has to precede the ifstream.
2. **The explicit `bool bHaveArchive = g_RFIndex.pFile != NULL;` intermediate**, the same lever
   `CreditsWnd::BuildResourcePath` needed for the identical RF-archive guard: it supplies the
   original's `xor r,r; cmp; setne rl; test rl,rl` materialization that a plain `if` does not.
3. **Do NOT write the redundant `bLoaded = 0;` inside the archive branch.** Ghidra prints one
   there, but it is provably dead (bLoaded is still 0 from its own initializer) and writing it
   costs a real extra instruction: 113/112 with it, 112/112 without.

**Residual (not source-steerable):** cl picks EDX/EAX for the `bHaveArchive` materialization
where the original picks ECX/EDX, and that choice cascades through the `LoadResource` argument
setup and the `delete pRfStream` virtual-base adjustor (`lea ecx,[eax+esi]` vs `add ecx,esi`);
the inline `repnz scasb` strlen block is additionally scheduled a few instructions earlier.
This is the documented non-steerable `setcc`-widening class — see `docs/CODEGEN.md`'s bullet
(4) under the byte-predicate inventory. Do not re-grind without a genuinely new idea.

---

## v399 — `src/TutorialWnd.cpp` tail (four new parks)

All four are structurally complete transcriptions; each is autopsied in-source at its
definition, with the refuted probes listed there. None should be re-ground without a genuinely
new idea.

| addr | name | residual | class |
|---|---|---|---|
| 0x44f560 | `TutorialWnd::NotifyOrLaunch` | 221 B / 486 vs 494, insns **138/143**, align 98, byte_diff 38, total **98038** | Two stacked intrinsic classes, both already documented. (1) The strstr predicate widens the wrong way: original `test eax,eax; setne al`, cl gives `neg eax; sbb eax,eax; neg eax` -- CODEGEN's sete-materialized-predicate caveat (4). The `unsigned char` inline predicate still EARNS ITS KEEP (it supplies the `test al,al` pair a plain `if (strstr(...) != NULL)` lacks, worth ~80 align). (2) cl cross-jumps this arm's `return 0` into the early-exit guard's epilogue where the original keeps two copies -- and it only can because ITS two copies differ, interleaving `xor al,al` between the pops at 0x44f73f but not at 0x44f642. Pure scheduling; CODEGEN #18i's hard limit. Refuted (v401): predicate taking the strstr RESULT as a pointer parameter (byte-identical, 98038); hoisting the whole if/else chain inside the first buffer scope so the predicate feeds the branch with no intervening local (WORSE, 98056, and does not restore the `setne`). Everything else -- the 0x414 frame, both scratch-buffer pairs' slots, every block -- matches. |
| 0x452b00 | `TutorialWnd::RestorePresenterBackdrop` | 43 B / 249, insns **83/83** (was 32 B pre-v401; the TU gained four `#include`s and three functions, which re-rolled the tie-break without changing the length -- still content-complete) | load-hoist schedule tie-break. The original hoists all FIVE field loads of the two `rectWindow.right/bottom` assignments ahead of either store and keeps both results live in ecx/edx; cl loads three, stores `.right`, then reloads. Refuted: two `int` temporaries (folded away, byte-identical), guard read hoisted above the assignments (32 → 38 B). |
| 0x452c00 | `TutorialWnd::DrawPresenterFrame` | 34 B / 203, insns **68/68** | zero-register residency (documented v375 class, no source lever). cl parks 0 in ebx and spends it on all six zero uses; the original materializes each as an immediate. Refuted: aggregate-initialised `srcRect` vs. four field stores, and early-return vs. wrapping `if` — both byte-identical. |
| 0x452570 | `TutorialWnd::DrawItemTitle` | 108 B / 314, insns **110/110** | block placement only. The original emits the bail-out restore block immediately after the guard and jumps FORWARD over it into the draw body; cl sinks it past the body. The 8 extra bytes are all wider jump encodings. Refuted, all strictly worse: explicit if/else with a restore per arm (331 B), positive `&&` guard with the body as the then-arm (331 B), nested `if (loaded) { if (id == 0) bail; … }` (365 B). |
| 0x452230 | `TutorialWnd::DrawDescriptionPage` | 259 B / 823, insns **264/292** (candidate 28 SHORT) | cl cross-jumps the three bail/tail restore-and-present copies into one shared block; the original emits each in full with its own epilogue. Lands on CODEGEN #18i's documented hard limit — the original's copies are not byte-identical to each other (tail 1 stages `oldFont` through ecx, tail 2 through eax), so its non-merge is a phase-ordering artifact of the original build. Refuted: factoring the sequence into a private inline member helper called at all four sites (cl inlines it fully, then cross-jumps anyway — byte-for-byte the same 772-byte output). |

### ✅ RESOLVED v412 — `MailWnd::RefreshClientClipRect` (0x42f8b0, 1332 B) is EXACT again

*Was parked v399–v411 as "newly EXPOSED, not newly broken".* The original diagnosis — that this
function is sensitive to the declaration-count/shape of the shared headers `src/MailWnd.cpp`
includes, and that no legitimate `TutorialWnd` method count recovers it — was CORRECT about the
mechanism and WRONG about the conclusion that "the too-small model was masking a real residual".
The dial is the whole INCLUDED DECLARATION SET, not `TutorialWnd`'s share of it, and the excess
declarations were in `src/PostBag.h`, not `TutorialWnd.h`.

It went back to EXACT with no change to `src/MailWnd.cpp` at all, purely from **deleting nine
duplicate declarations** that `src/PostBag.h`/`src/CarNetState.h` carried: five functions were
declared BOTH as `PostBagCacheBundle` members and as free `__stdcall` externs (the deliberate
"dual model" v362 introduced), and the parameter types of three more disagreed between the two
spellings. Retiring the free externs — after proving all of those functions really are
`__thiscall` members, see the v412 ecx sweep in `docs/CODEGEN.md` — shrank the declaration set
past the threshold. **Lesson: a "declaration-count dial" residual is a signal to go looking for
redundant declarations in the shared headers, not only to accept the model as-is.**

## v402 — `src/PlacementCursorMaybe.cpp` (new TU) and the shared-header fold it forced

| addr | name | residual | class |
|---|---|---|---|
| 0x410510 | `PlacementCursorMaybe::PlacementCursorMaybe` | 178 B / 333 vs 335, insns **94/95**, align 6, reg_pen 0, identity_miss 0, byte_diff 2, total **6002** | ONE instruction apart. The original carries a redundant `cmp eax, ebx` at +0x7f (ebx = its live zero register) between the `and eax,0xa` that computes `m_ptr ? 10 : 0` and the store into `m_count`; cl reuses the `and`'s own flags for the following `jne`. Every other byte agrees, including the whole `neg/sbb/and` materialization. The compare lives inside `Obj0x477758Base`'s ctor, INLINED from the shared `src/Obj0x477798Family.h`, whose spelling is pinned by its other consumer (`NetSessionEventQueue.cpp`, 249 B either way) — not this TU's to choose. Refuted, one compile each, both leaving the score at exactly 6002: hoisting the ternary into a local (`int nReserved = …; m_count = nReserved; if (nReserved == 0)`), and spelling the condition explicitly (`m_ptr != 0 ? … : …`). Zero-register-liveness tie-break driven by the OUTER ctor's register state. |

## v403 — `src/PlacementCursorMaybe.cpp`, second sitting

| addr | name | residual | class |
|---|---|---|---|
| 0x410840 | `PlacementCursorMaybe::AdvanceAnimFrameMaybe` (the tick, slot 10) | 169 B / 466 vs 468, insns **134/134**, align 70, reg_pen 16, identity_miss 16, byte_diff 52, total **71812** | The same 134 instructions in the same order EVERYWHERE except the two `ClampToGridBoundsMaybe` call sequences, which differ only in which half of the packed position cl computes first. The original evaluates the arguments right-to-left (`shr` for y into the loaded register, `and` for x into the copy, hidden return-buffer `lea` LAST at `[esp+0x14]` i.e. after both pushes); cl here evaluates left-to-right (retbuf `lea` first, `and` into the loaded register, `shr` into the copy) while pushing in the identical order. Everything after each call site is those two instructions displaced — that displacement is the whole of `align=70`, there is no second disagreement in the body. FIVE probes refuted, one compile each: `LOWORD`/`HIWORD` on the member (**81386**, strictly worse — the macro's `(WORD)` cast makes cl re-load the low half from memory instead of masking the register copy, a real extra disagreement, now a CODEGEN bullet); `HIWORD` for y with a plain mask for x (71812, so the ordering is driven by the LOW half's spelling); hoisting the packed member into an `unsigned int` local (71812); that local plus `LOWORD`/`HIWORD` (71812); and the pre-`unsigned` baseline, which was worse because it emitted `sar`. **That last probe found a real bug and is kept** — all five `packed*` members are `unsigned int` now. What remains is a register/scheduling coin-flip at a struct-return call site. |
| 0x411ae0 | `PlacementCursorMaybe::SelectCursorTypeTilePlacementMaybe` | 20 B / 364, insns **112/112**, align **0**, reg_pen 18, identity_miss 20, byte_diff 20, total **2020** | Every instruction present, in order, identical opcodes and displacements. The ENTIRE residual is which scratch register the five hit-test call sites load their `(x, y)` argument pair into: the original cycles `ecx/edx, eax/ecx, edx/eax, ecx/edx, eax/ecx` and cl runs the same `eax->ecx->edx` cycle exactly ONE STEP AHEAD. So the original had one more temp allocated before the first call than this spelling produces, and every later site inherits the offset. Three probes refuted, one compile each: splitting the second condition's `||` into two `if`s with a duplicated body (much WORSE, align 118 — the `||` short-circuit shape is confirmed correct); rewriting the whole chain as `if`/`else if`/`else` instead of early returns (codegen-IDENTICAL, the two shapes are indistinguishable here); routing the first hit test's result through a named `char` local before the `||`. Known intrinsic class (the one-step `eax->ecx->edx` rotation, v386). |
| 0x411000 | `PlacementCursorMaybe::CommitPendingCoupleMaybe` | 357 B / 558 vs 560, insns **157/157**, align 32, reg_pen 32, identity_miss 32, byte_diff 62, total **35582** | Same 157 instructions in the same order, TWO disagreements, both already-diagnosed classes. (1) The `ClampToGridBoundsMaybe` call sequence, byte-for-byte 0x410840's residual above — two more probes refuted here, both landing on exactly 35582: hoisting ONLY the y argument into its own named `int` local (cl folds it), and spelling the unpack as `% 0x10000` / `/ 0x10000` (a different AST that emits the identical `and`/`shr` pair). Seven refuted probes across the two sites now; treat the class as closed unless it cracks. (2) The one-step `eax->ecx->edx` rotation across the SEVEN `(x, y)` argument pairs the hit-test chain reloads — the same class as 0x411ae0 above, and here it is DOWNSTREAM of (1), since the two Clamp spellings leave a different number of scratch registers allocated by the time the first hit test is reached. |
| 0x411230 | `PlacementCursorMaybe::CommitPendingRotateMaybe` | 282 B / 368 vs 357, insns **114/111**, align 80, reg_pen 41, identity_miss 41, byte_diff 94, total **84604** | Every branch, call and argument agrees, including the redundant `cmp edx,ecx` at +0x85 that pins the source as a chain of independent `else if`s over the same condition pair. Two register classes, both already-documented, neither this function's own spelling to fix. (1) The `ClampToGridBoundsMaybe` ARGUMENT-EVALUATION ORDER -- original emits `shr eax,0x10` before `and ecx,0xffff`, this the other way round: byte-for-byte 0x410840/0x411000's residual, the class closed after SEVEN refuted probes across those two sites, so deliberately NOT re-probed here. (2) A CSE coin-flip cascading off it -- the whole +3 instruction delta is prologue/epilogue (this saves ebx/edi as well as esi, the original only esi): cl keeps `pt.x`/`pt.y` live in the two extra callee-saved registers and CSEs them against the later `resolvedPosBXMaybe`/`resolvedPosBYMaybe` reads, where the original spills the POINT temp and re-reads both members at each of the four call sites. Same coin-flip on `wRMBSeqMaybe`: the original re-reads `word ptr [eax+0x52e]` at all THREE uses, this loads it once into ax. Nothing in the source caches either value, so there is no local to delete. ONE probe landed and is kept: dropping a `BigObj *pKind = pKindDesc;` local for plain `pKindDesc->` reads, 85279 -> 84604. |
| 0x410d20 | `PlacementCursorMaybe::RefreshFootprintHighlightMaybe` | 156 B / 736 vs 736, insns **230/230**, align 10, reg_pen 7, identity_miss 7, byte_diff 17, total **10787** | Same 230 instructions in the same order; every block, branch, call, argument and constant agrees. The whole residual is 7 instructions choosing a different register for the same value, at two sites: (a) the `ContainsAnyRegionMaybe` argument pair (original loads `lastResolvedPosY`/`X` into ecx/edx, this into eax/ecx, after which ecx is reloaded with the `this` literal either way), and (b) the `packedMousePosMaybe` copy in the snap-lock arm (eax vs edx, same hoist-the-load-before-the-bool-store order). This is the SAME closed argument-evaluation-register class as 0x410840/0x411000/0x411230 above -- seven refuted probes there, deliberately not re-probed. One probe run here, swapping the `nTileY + (short)y` operand order: identical 10787. Four diagnosed source-shape corrections got it from 414430 to 10787 and are recorded in-source: if/else ARM ORDER; both retract loops are real `for` loops (`test/jbe` zero-trip guard, not `test/je`); `aFootprintOccupancyMaskMaybe` is a 3-D `[13][9][7]` array, not a flat block walked with hand-rolled row pointers; and `Obj0x477758::Count()` returns UNSIGNED (`jbe`, not `jle`). |

## v408 — the `Obj0x477758` collection tail and `src/TilePlacedObj.cpp`

| addr | name | residual | class |
|---|---|---|---|
| 0x4124b0 | `Obj0x477758::SetAtMaybe` (slot 10, derived override) | 1 B / 129, insns **47/47**, align **0**, reg_pen 2, identity_miss 2, byte_diff 1 | ONE byte. Every block, branch, call, constant and register assignment agrees; the sole disagreement is the SIB operand order of the closing `return m_ptr[idx];` — the original encodes `mov eax,[edi+ecx*1]` (ModRM base = the scaled index), this encodes `mov eax,[ecx+edi*1]` (base = m_ptr). The BASE class's copy of that identical statement, 40 bytes up the same file at 0x4123a0, encodes it the original's way and is EXACT, so the flip is downstream of the extra `idx > m_0c` guard's register pressure (the derived spills through ebx and its `pop ebx` lands between the store and the reload), not of anything spellable in the statement. The two operands are commutative in the addressing mode and the source cannot name which becomes the base. Do not grind. |
| 0x423560 | `AnimEffectObj0x477a90::TickMaybe` (the effect per-frame step) | DIFF(639) / 702 B compiled vs a true COMDAT extent of **736**, insns **254/245** | PARTIAL but content-complete. ⚠ Score it with `--len 736` (0x423560..0x423840) -- Ghidra's `Body` span stops at 0x423811 and excludes the two trailing switch tables. CALL PARITY exact on 3 of 4 targets (RepositionWithHotspot 5/5, ReleaseChannelAndDispatch 4/4, IsReclaimable 1/1); every block, constant, field and branch sense agrees. Two stacked VC5 /Og classes: (1) CROSS-JUMPING -- the source spells `AdvanceAnimFrameMaybe()` at 14 sites and the original emits 10, having tail-merged four identical `{ Advance(); break; }` blocks (both `nSubFrame == 0` arms land on the shared 0x423788); this compile merges none, which is essentially the whole instruction excess. Same class as v378's cross-jumped duplicate tail. (2) ZERO-REGISTER + a spilled flag -- the original materialises 0 in ebx and compares against it throughout while keeping bExpired in the stack byte its `push ecx` prologue reserves; this compile keeps it in bl and uses `test reg,reg`. Same class as 0x456150 / 0x462e90. The one lever that DID land is baked in: `bUnk0x94Maybe` is UNSIGNED (a signed char emits `movsx` where the original zero-extends). |
| 0x435a10 | `WalkerRegistryMaybe::SetAtMaybe` (slot 10, derived override, T = DecorActorBase) | 1 B / 130, insns **47/47**, align **0**, reg_pen 2, identity_miss 2, byte_diff 1 | The SAME residual as 0x4124b0 directly above, reached from the same source text over a different element type: the closing `return pArrayMaybe[nIndex];` encodes `mov eax,[ecx+edi]` where the original picks the commutative `mov eax,[edi+ecx]`. Do not grind. |
| 0x4360b0 | `RoadVehicleRegistryMaybe::SetAtMaybe` (slot 10, derived override, T = DecorActorBase) | 1 B / 130, insns **47/47**, align **0**, reg_pen 2, identity_miss 2, byte_diff 1 | The category-8 twin of 0x435a10, byte-for-byte the same one-byte residual. THREE instantiations of this one source now land on it and only the base-side 0x4123a0 escapes it, which pins the flip to the extra `idx > <live count>` guard's register pressure rather than to anything spellable in the statement. Do not grind. |

Also parked from this session, both in `src/TilePlacedObj.cpp` and both autopsied in-source:

| addr | name | residual | class |
|---|---|---|---|
| 0x458310 | `TilePlacedObj::GetFrontRowTilePosMaybe` | 59 B / 63 vs 60, insns **15/14** | Expression REASSOCIATION below the source level. The original evaluates `(rows - ySteps)` and only then adds `pos.wPosY` (`sub edx,eax` … `add dx,[ecx+0x8a]`); cl turns the same source into `(pos.wPosY + rows) - ySteps` and needs one more move. `a - b + c` already parses as `(a - b) + c`, so parentheses are a no-op. Refuted with an identical 40258 each time: an explicit `(unsigned short)` cast around the difference, and hoisting the difference into its own named `unsigned short` local. |
| 0x458350 | `TilePlacedObj::GetEntryExitPointMaybe` | 42 B / 99 vs 102, insns **40/40**, align **0**, reg_pen **0**, byte_diff 5 | FIVE bytes, all in how one bool temp is materialized. The original writes it full-width (`mov ebx,1` / `xor ebx,ebx`) then narrows to test it (`test bl,bl`); this emits the byte-width `mov bl,1` / `xor bl,bl` pair with the same `test bl,bl`. Declaring the temp `int` is REFUTED and strictly worse — it widens the test to `test ebx,ebx` too and takes the residual from 42 to 92. |

⚠ **Both `src/TilePlacedObj.cpp` rows are currently OUT OF THE TREE.** Their two member
declarations on the shared `TilePlacedObj.h` cost `src/PeerTrainNode.cpp` a 220-byte exact match
(the declaration-set lottery, see `docs/CODEGEN.md`), which is a net loss while they are both
still residuals. The transcriptions are recorded verbatim in the v408 pickup — land them again
alongside enough other TilePlacedObj members that the one declaration-set change pays for itself.

## v419 — `src/WorldBoardMaybe.cpp`

| addr | name | residual | class |
|---|---|---|---|
| 0x455ab0 | `WorldBoardPartial::FlushQueuedBigObjMaybe` | 695 B / 686, insns **216/215**, align 220, reg_pen 125, identity_miss 146, byte_diff 264, total **234224** | Register-role allocation, not body. Nearly every `--dump` row is `r` (same mnemonic, different register). One real lever found and kept: `BoardTileCellMaybe`'s two plane counts are SIGNED `char` (433375 → 234224) — that is what restores the original's 8-bit `dec bl / test bl,bl / jge` slot loop and its `cmp al,bl` count test in place of a spilled two-counter loop with a 32-bit `and ecx,0xff` compare. Three gaps left, all refuted as source-steerable: (a) the dirty-bit store read-modify-writes through a register in the original where this compile folds to `or byte ptr [edx],al` — and PlaceObject's own stamp of the SAME bitmap emits the folded form in the original, so the two sites genuinely differ; (b) loop 1's inner-loop entry, where this compile hoists `(int)nCol` out of the row loop and needs one extra `jmp` — the original does exactly that hoist in loop 2 and NOT in loop 1, so no single source shape yields both; (c) `this` in edx (original) vs ebp (here), the symmetric-register-swap class, which is what makes the frame one dword bigger. Refuted probes: every declaration-order permutation of the six prologue locals; `row + col * BOARD_TILE_ROWS`; `nCell`/`slot`/`i` hoisted to function scope; the dirty-bit OR spelled longhand or routed through a temp (all exactly neutral); and assigning loop 2's biased start into the row LOOP VARIABLE rather than into `nRow` (256349, worse, despite being literally which stack slot the original writes). |

| 0x4550c0 | `WorldBoardPartial::PlaceObject` | 832 B / 791, insns **250/240**, align 276, reg_pen 89, byte_diff 283, total **286183** | Carried from v418 at 274182; rescored UP in v419 by a deliberate CORRECTNESS fix, not a regression. Its plane-B dirty-bit index was transposed (`wCols * nCol + nScanRow`) until v419 — the original's `imul eax,edi / add eax,ecx` has edi = the scan ROW and ecx = the running COLUMN, the same `wCols * row + col` MarkRectDirty writes and both readers consult. Both spellings are one `imul` plus one `add`, so only the register roles differ and asmscore charges it as alignment noise. Underlying residual is unchanged and still the v418 one: register pressure in the prologue (the original keeps `this` in esi and the running column in ebp where this compile spills both). Do NOT re-try hoisting `nRow + nRowBias` into a shared local for the two legality calls — it closes the count to 246/240 and costs 108k of alignment. |

## v429 — `src/AppWindow.cpp`

| addr | name | residual | class |
|---|---|---|---|
| ~~0x406ba0~~ | `AppWindow::InitSubsystemsAndWindows` | 723 B | **RESOLVED v448 — EXACT** | Was NOT a scheduling residual at all. The whole DIFF(8) came from the function being spelled as the `int __fastcall AppWindow_InitSubsystemsAndWindows(AppWindow *pApp)` escape hatch instead of the `__thiscall` member Ghidra reads it as. Re-spelling it `int AppWindow::InitSubsystemsAndWindows()` — identical body, `pApp->` becoming `this->` — takes it to a full 723-byte EXACT with no other change. The eight refuted source probes recorded here were all probing the wrong thing; the `__fastcall(Class *)` hatch is argument-passing-equivalent at the CALL SITE only, and VC5 still schedules the two differently. Cost: one declaration on src/AppWindow.h, paid as part of that header's parity move (see 0x42f8b0). ⭐ Generalize: any parked residual on a function modeled with the `__fastcall(Class *)` hatch deserves one re-spelling probe before it is called intrinsic.

## v442 — `src/ThumbnailBmp.cpp`

| addr | name | residual | class |
|---|---|---|---|
| 0x447ba0 | `ThumbnailBmp::ThumbnailBmp_Load` | 520 B / 520 (exact length), DIFF **2**, insns **150/150**, align **0**, reg_pen **0**, identity_miss **0**, byte_diff 2 | COMMUTATIVE-OPERAND canonicalization, locked by an EH edge. The two bytes are the displacements at 0x447d05/0x447d0c — which of `wWidth`(+0xb2)/`wHeight`(+0xb4) feeds the `imul` DESTINATION register. The original loads wHeight into the dest; cl here always picks the lower offset. Nine source shapes give the identical two bytes: `w*h`, `h*w`, `n=h; n*=w;`, `n=w; n*=h;`, a function-scope `unsigned int nPixels;` with either order, `int nPixels`, and an `unsigned short` temp for either operand. Repeating the size expression at all three use sites instead of a local is strictly worse (DIFF 127 at 564 B — cl recomputes instead of CSEing), which confirms the explicit local. Root cause isolated with a standalone probe: in a plain `/GX` try-block cl DOES honour the source order for `(unsigned short)a * (unsigned short)b`, but adding a `throw` on an earlier check INSIDE the same try (here the header read's `gcount()` test) makes it canonicalize by ascending field offset and ignore source order entirely. |

## v442 — the `??_G` scalar-deleting-dtor cluster (three classes, all measured, all rejected)

Every one of these is the same residual: the original's `??_G` thunk carries the destructor's
body INLINED, which is what an IN-CLASS destructor produces, while an out-of-line `~T()` in the
`.cpp` makes the thunk `call ??1T`. In all three cases the in-class form DOES close the thunk and
loses more elsewhere, because it also makes every CONSUMER TU inline instead of call.

> ⛔ **v449: THE REPO-WIDE SWEEP HAS NOW BEEN RUN, AND IT REFUTES THE "MAY WELL BE NET-POSITIVE
> TAKEN TOGETHER" HYPOTHESIS THESE ROWS ALL DEFERRED TO. DO NOT RE-RUN IT.** See
> *v449 — the in-class-dtor / shared-victim cluster sweep* at the bottom of this file for the
> full measurement. Short version: the cluster's TOTAL available gain is **+99 B** against a
> **fixed 249 B** cost that all five levers pay, so no subset can tip positive. The 249 B is
> a single SATURATING flip, not an additive or parity-cyclic one — each lever produces the
> identical `DIFF(32)` on the victim independently.

| addr | name | residual | measurement |
|---|---|---|---|
| 0x452d30 | `??_GIniFile` | 30 B / 32, DIFF **22** | In-class makes it EXACT at 32 B and `src/IniFile.cpp` 6/6 at 249 B, but DELETES this TU's `??1` COMDAT outright and costs `src/TutorialWnd.cpp`'s `RestorePresenterBackdrop` (0x452b00) 249 B. Repo-wide 119121/495 out-of-line vs 118897/494 in-class. |
| 0x454b70 | `??_GResourceRef` | 30 B / 65, DIFF **19** | The ONLY one of the three whose real dtor genuinely IS in-class — the image has no out-of-line `??1ResourceRef` anywhere. Two levers, both negative: writing the dtor in-class alone gets 36 B / DIFF(20) and costs TutorialWnd 0x452b00 (-249 B) for nothing; ALSO making `ReleaseRealized` in-class closes the thunk (EXACT 65 B, file 4/4 at 333 B) but regresses SEVEN TUs as its ~35 call sites start inlining — AlbumCardWnd -163, ApplSetupWnd -676, EditCardWnd -362, MailWnd -743, MapWnd -494, NetSetupWnd -478, TutorialWnd -249 (repo-wide 119164/499 vs 116275/489). |
| 0x452fa0 | `??_GLocalPlayerIdentity` | 30 B / 32, DIFF **22** | Added v443. Same shape and same victim as `??_GIniFile` above: the image has no out-of-line `??1LocalPlayerIdentity` anywhere, so the real dtor IS in-class, and writing `virtual ~LocalPlayerIdentity() {}` in `src/LocalPlayerIdentity.h` DOES make it EXACT at 32 B — but it costs `src/TutorialWnd.cpp`'s `RestorePresenterBackdrop` (0x452b00) its full 249 B, a genuine 83-instruction reg-alloc reshuffle at identical length (confirmed NOT a marker-pairing artifact: `pair_by_name` resolves 0x452b00 correctly by name in both builds, and `--dump` shows real register/scheduling divergence). Repo-wide 120516/503 out-of-line vs 120299/503 in-class, i.e. net **-217 B**. |
| 0x4493c0 | `??_GLockableMaybe` | 30 B / 41, DIFF **24** | NOT probed — strictly more exposed than the other two (heap-allocated `g_pGameNetMsgQueueLock` plus two EMBEDDED members in `DecorObjMgrMaybe`), so every TU destroying one of those would inline it. Include it in the repo-wide sweep, not on its own. |

## v442 — `src/LocalPlayerIdentity.cpp` (new TU)

| addr | name | residual | class |
|---|---|---|---|
| 0x4532a0 | `Profile_SavePlayerUserFile` | 115 B / 115 (exact length), DIFF **3** | Pure instruction SCHEDULING: the original emits `push esi` (CloseHandle's argument) BEFORE the dead `test eax,eax` on WriteFile's result; this compile emits the test first. Same class as 0x406ba0 (v429). The branch on WriteFile's result IS load-bearing and must not be simplified away — without one, cl drops the `test` and the function comes out 2 bytes SHORT at DIFF(12). Four no-branch spellings refuted (discarded `BOOL` local, discarded `bool` local, empty `if (...) {}`, empty `if (!...) {}` — cl dead-codes the flag set in all four); three BRANCHING spellings are byte-identical to each other (early-return on failure, `if (!ok) close; else close;`, `?:` of two CloseHandle calls), i.e. the arms cross-jump and only the dead test survives. |
| 0x453320 | `AllocNextPostSeqIdString` | 166 B / 166 (exact length), DIFF **3** | Identical residual to 0x4532a0 — same write-then-close tail, same scheduling swap. |


## v443 — measured-and-rejected header changes (NOT residuals; the function itself matches)

| addr | name | status | measurement |
|---|---|---|---|
| 0x424af0 | `CursorDesc::CursorDesc` | 67 B, **EXACT on the first compile** — but reverted | The cost is not in the function, it is in DECLARING it. `src/Obj0x4779e0.cpp` carries a standing warning that ANY parameterized method decl added to `src/CursorDesc.h` rotates dependent TUs; a v443 bisect confirmed that covers a CONSTRUCTOR declaration too, and covers the sibling change independently. Adding `CursorDesc(int, char *, int)` ALONE, and widening `virtual void Load()` to its real two-argument signature ALONE, EACH turn `Obj0x4779e0ParsePartial::ParseEntryExitMaybe` (0x41f0c0) from EXACT into DIFF(19) at identical length, **-489 B**. Net **-422 B**, so the ctor stays parked and `Load` stays deliberately mis-declared as no-arg (nothing in `src/` calls it, so the wrong arity is free today). The measurement is written into `src/CursorDesc.h` at the `Load` declaration. Retry when that TU-rotation class cracks. |
| 0x45b500 | `Ddraw::Ddraw_Init` | 728 B, **TRANSCRIBED v530 — EFFECTIVE, DIFF(78), insns 225/225** | ⚠ The "toolchain-blocked" verdict this row carried from v443 to v529 was WRONG, and cost six sessions. No DX5 SDK header is needed: every field the body touches (`dwSize`, `dwFlags`, `dwHeight`, `dwWidth`, `ddsCaps.dwCaps` at +0x68) sits at an IDENTICAL offset in `DDSURFACEDESC` and `DDSURFACEDESC2`, only `sizeof` differs (108 vs 124) — and `DDSurfaceDescPadded0x7c` in `src/LocoBitmap.h`, which the repo has shared across TUs since v200, already supplies exactly that. Every vtable slot used (SetCooperativeLevel +0x50, CreateSurface +0x18, CreateClipper +0x10, GetSurfaceDesc +0x58, GetClipper +0x3c, SetClipper +0x70, SetColorKey +0x74) is at the same offset in IDirectDraw2/IDirectDrawSurface as in the 4-suffixed interfaces, which `src/Ddraw.h` had already documented. The ONE genuinely missing symbol was the IID, now a `DEFINE_GUID` in `src/Ddraw.h` read out of the image bytes at 0x4785e8. Landed with ZERO collateral (full `progress.py` diff: only this TU's row moved). Residual is one register coin-flip — see the EFFECTIVE MATCH autopsy above the body in `src/Ddraw.cpp` for the five refuted probes. Retry only if the eax-vs-ecx allocation class cracks. |


## v444 — src/BuildToolCursorWnd.cpp + src/NameAnchorMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x436d60 | `BuildToolCursorWnd::RedrawGhostCursor` | 345 B (COMDAT extent 0x160) | compiled **339 B**, **insns 99/99**, total **82826**, align 82, reg_pen 7, identity_miss 7, byte_diff 56 | Content-complete, and everything from the `CopyRect` onward — the spawn / union / intersect / `MarkRectDirty` / `UpdateDirtyTiles` tail, about two thirds of the body — is byte-identical bar one `lea`'s register choice. The whole residual sits in the ~40-byte coordinate block. Both arms of the `g_bBoardScrollFlag` branch read `this->Unk0xdc`, and cl 11.00 hoists that load ABOVE the branch as a partial-redundancy elimination; with the value already live in edi it then folds the scroll offset with `add`/`sub` on edi, where the original reloads per arm and folds scroll + 0x32 into a single `lea edi,[eax+ecx+0x32]`. **Three shapes measured — do NOT re-run:** reassociating `+ 0x32` to last (identical, DIFF 185), inverting the `g_bBoardScrollFlag` guard so the simple arm is written first (WORSE, DIFF 190), and the two branch-polarity fixes that ARE baked in and must not be undone — writing the scroll arm first and the `pDirtyRectHandle != NULL` arm first took it from 215010 to 82826 and closed the structural half of the diff. |
| 0x40b500 | `NameAnchorMaybe::NameAnchorMaybe` | 66 B (exact length) | **insns 18/18**, total **22343**, align 22, reg_pen 3, identity_miss 3, byte_diff 13 | Every store, its order and the branch polarity are right. The original materializes -1 and 0 into edx/ecx FIRST and only then loads the `bPeerOwned` byte (mid-body, at 0x40b50d); this compile loads the parameter into dl at offset 0 and folds -1 and 0 through a single reused ecx. Same intrinsic ctor constant-materialization/scheduling class as `BuildToolCursorWnd`'s own ctor (v443). **One shape measured:** duplicating the six common stores into BOTH arms of the `if`, so the parameter's first use moves to the top — byte-identical DIFF(17), no effect. |

## v444 — measured-and-rejected header changes (NOT residuals; the function itself matches)

| addr | name | status | measurement |
|---|---|---|---|
| 0x401650 | `??_GPostBagFileCache` | 36 B, **EXACT on the first compile** — but reverted | **Fourth member of the in-class-dtor cluster** (`??_GIniFile`, `??_GResourceRef`, `??_GLocalPlayerIdentity` above), and the fourth with the same victim. The image has no out-of-line `??1PostBagFileCache`, so the real dtor IS in-class, and `virtual ~PostBagFileCache() { SaveIndexFile(); }` in `src/PostBag.h` makes it EXACT at 36 B — but it costs `src/TutorialWnd.cpp`'s `RestorePresenterBackdrop` (0x452b00) its full 249 B, MATCH → DIFF at unchanged length. TutorialWnd.cpp reaches `PostBag.h` through `AlbumCardWnd.h`. Repo-wide **122091/517 in-class vs 122055/516 out-of-line**, i.e. +36 B here and −249 B there for a net **−213 B**. Bisected by reverting this one declaration alone. The measurement is written into `src/PostBag.h` at the declaration. **Every member of this cluster so far has traded a small local win for a larger loss in a TU downstream of the header — stop probing them one at a time and do the sweep repo-wide, together, or not at all.** |
| 0x40b550 | `NameAnchorMaybe::~NameAnchorMaybe` | 70 B, NOT transcribed — **header-dependency blocked, not a residual** | Body fully read (`if (pTileMaybe != NULL && !IsNetShuttingDownMaybe()) { pTileMaybe->nOccupantRefCount--; pTileMaybe = NULL; }` then the delete-flag tail). Its class's only dtor COMDAT is the scalar deleting form, so it must be written IN-CLASS — which needs `TrackTileObj` COMPLETE in `src/NameAnchorMaybe.h`, i.e. `#include "TilePlacedObj.h"` there, dragging `WidgetBase.h` + the position-sensitive `CursorDesc.h` into `src/WalkerActor.cpp` and `src/PeerTrainNode.cpp`. Not attempted for 70 B. Same blocker shape as 0x4431f0 below. |
| 0x4431f0 | `PostBagCacheBundle::~PostBagCacheBundle` | 109 B, NOT transcribed — **header-dependency blocked, not a residual** | Body fully read (release `pCachedBadgeTileDesc` via vtable slot 2 and null it plus `pRealizedBadgeFrame`; pointer-induction walk of the 256 `paBitmapSlots` `delete`-ing and nulling each; `PostBag_ClearWorkingFolders()`; delete-flag tail). Must be in-class for the same single-COMDAT reason, which needs BOTH `CursorDesc` and `LocoBitmap` complete in `src/PostBag.h` — a header with no includes at all today, pulled in by 8 files, SIX of which see neither (AppWindow.cpp, GameNet.cpp, ScreenSaver.cpp, LocalPlayerIdentity.cpp, AlbumCardWnd.h, CarNetState.h). Not attempted for 109 B. |

## v445 — src/NameAnchorMaybe.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x40b740 | `NameAnchorMaybe::NameAnchorMaybe_ResolveTileAnchorMaybe` | 312 B (exact length) | **insns 113/113**, total **442**, align 0, reg_pen 4, identity_miss 4, byte_diff **2** | Two bytes. Every instruction, register and operand agrees except the scale-1 SIB base/index ORDER on the function's last two loads: the original encodes `movsx ecx, [eax + ecx]` / `movsx eax, [eax + ecx + 2]` (the materialized byte offset as base, `pSocketTable` as index) where this compile encodes the same two registers the other way round. The loop's own `[ebx + ebp*4]` table access two blocks earlier matches EXACTLY, so this is not a modelling error about the socket table — it is the coin flip that only appears once VC5 materializes the doubled index into its own register, which it does here because the index is used twice and the `* 2` therefore comes out of the addressing mode. **Three shapes measured — do NOT re-run:** hoisting the doubled index into an `int nSocketOffset` local (DIFF(43), strictly worse — it also suppresses the `pSocketTable` reload the original issues twice), swapping the two addition operands so the tile base comes first (byte-identical DIFF(2)), and writing the index as `2 * n` instead of `n * 2` (byte-identical DIFF(2)). |

## v445 — measured-and-rejected header change — ✅ RESOLVED AND LANDED IN v486

**All three rows below are CLOSED.** The declaration they were all blocked on
(`virtual void RepositionWithHotspot(int, int);` on `AnimDescRefObj0x477488`, `src/WidgetBase.h`)
is now checked in; 0x405c00 (61 B) and 0x454820 (98 B) are EXACT, and the byte-invisible
wrong-call-target defect in `PlacementCursorMaybe::SetTypeMaybe` is fixed. Net **+302 B** with
**zero victims** — not the −852 B v449 measured, and the lever itself did not change.

What changed is the PARITY it is priced at. `WorldBoardMaybe::FindNearestObjOfCategoryMaybe`
(0x457ce0, 951 B) answers to the COMBINED declaration count of several shared headers, and v486
was holding a spare true-but-optional declaration in `src/Obj0x477798Family.h` (`_v15`, the
16th vtable slot). Dropping that one in the same commit as adding this one holds the parity, and
then every recorded victim survives — 0x457ce0 holds, 0x452b00 holds, and 0x458310 does not
merely survive but GAINS 143 B. ⭐ **Generalise it: a parked "net-negative header change" is only
net-negative AT A GIVEN PARITY. Before trusting a row like these, check whether another header
can absorb the parity change — a true declaration you are free to write or not write is a
currency, and this repo has several.** The rows are kept below for their measurements and for
the verified bodies; the *conclusions* in them are superseded.

The old header, for greppability: measured-and-rejected header change (NOT residuals; all three
functions match)

| addr | name | status | measurement |
|---|---|---|---|
| 0x405c00 | `AnimDescRefObj0x477488::RepositionWithHotspot` | 61 B, **EXACT on the first compile** — but reverted | **Fifth instance of the v442-v444 header cluster, and the fifth to hit the SAME victim, `TutorialWnd::RestorePresenterBackdrop` (0x452b00).** The class does not declare this slot-3 override today; adding `virtual void RepositionWithHotspot(int x, int y);` to `AnimDescRefObj0x477488` in `src/WidgetBase.h` lets both this body and 0x454820's land, worth **+224 B** together with 0x436a60 — but it costs **0x452b00 its full 249 B** (MATCH → DIFF(32) at unchanged length) *and* `TilePlacedObj::GetFrontRowTilePosMaybe` (0x458310) its **60 B** (MATCH → DIFF(18)), for a net **−85 B** repo-wide (123110/521 without vs 123025/522 with). Declaration POSITION was tested both ways (immediately after the dtor, and as the last member of the class) — identical result, so it is the declaration's existence, not its ordering. 0x458310's own residual is a bare 14/14-instruction `sub edx,eax` vs `sub eax,edx` register coin flip with no source lever. **Do not re-probe alone — fold into the single repo-wide header sweep the v444 row calls for.** The verified body is recorded at the declaration in `src/WidgetBase.h`; it is `RectFlagObj0x477820::RepositionWithHotspot(x, y);` then `hotspotPosX = pKindDesc->hotspotX + x; hotspotPosY = pKindDesc->hotspotY + y;` then `if (pDSoundChannel != NULL) pDSoundChannel->SetPosition(x, y);`. |
| 0x454820 | `WidgetBaseObj0x4784c8::RepositionWithHotspot` | 98 B, **EXACT on the first compile** — but reverted | Blocked by the SAME declaration as 0x405c00 above, not by anything of its own: it is already declared on its own class, but its body's first statement is the base-qualified call `AnimDescRefObj0x477488::RepositionWithHotspot(x, y)`, which without that declaration binds to the INHERITED root member and would emit a call to 0x436a60 where the original calls 0x405c00. Byte-identical either way (verify.py masks relocations), so landing it under the wrong binding would be a silent lie in the source — hence held back with its sibling rather than shipped. Verified body: the base call, then `SetRect(&rectCMaybe, rect.left + 0x32, rect.bottom, rect.right + 0x32, rect.bottom + 0x31);` `SetRect(&rectBMaybe, rect.right, rect.top + 0x32, rect.right + 0x32, rect.bottom);` `MarkDirty();`. |
| — | `PlacementCursorMaybe::SetTypeMaybe` (0x411fb0) call sites | **pre-existing wrong-symbol binding, byte-invisible** | Both `AnimDescRefObj0x477488::RepositionWithHotspot(...)` calls in that function currently resolve to the inherited `RectFlagObj0x477820` member and emit calls to **0x436a60**, where the original calls **0x405c00** (raw disasm, 0x412019 and 0x41204f). The function byte-matches regardless because relocations are masked, and neither `lint_idiom.py`, `lint_calls.py` nor `lint_ghidra_sync.py` can see it. Fixed by the same declaration — land it with the sweep. |

## v449 — the in-class-dtor / shared-victim cluster sweep (RUN, MEASURED, REFUTED — do not re-run)

> ⚠ **PARTIALLY SUPERSEDED BY v486.** The fifth lever in this table — the `src/WidgetBase.h`
> `RepositionWithHotspot` declaration, priced here at −852 B — has since been landed for
> **+302 B with zero victims**, by paying its parity cost with a spare declaration in another
> header rather than with 0x457ce0's 951 B. See the v445 section above. The measurements below
> are still correct AT THE PARITY THEY WERE TAKEN AT; the "do not re-run" verdict now applies
> only to the four in-class-dtor levers.
>
> ⚠ **The four in-class-dtor levers WERE re-probed at the new parity in v486, and they still
> lose.** `virtual ~IniFile() {}` alone still takes `TutorialWnd::RestorePresenterBackdrop`
> (0x452b00) from MATCH to its full −249 B, with 0x457ce0 unaffected. So the two canaries have
> DIFFERENT mechanics and must be priced separately: **0x457ce0 cycles on parity** (and is
> therefore buyable), **0x452b00 saturates** (and is not). Finding 2 below stands exactly as
> written. Do not re-probe this cluster again without a NEW lever on 0x452b00 itself.

Five separate parked wins had been deferred, one per session from v442 to v445, each with the
same recorded reasoning: "it costs `TutorialWnd::RestorePresenterBackdrop` (0x452b00) its 249 B,
but 0x452b00 can only be lost ONCE, so fold this into the planned single repo-wide sweep and the
cluster may well be net-positive taken together." v449 ran that sweep. **It is net −1008 B.**

All five levers applied at once, against v449's post-`??_GNetSettings` baseline of
**124656 B / 536 funcs**: **123648 B / 537 funcs**. Every row of the per-file table, pasted:

| TU | lever | delta |
|---|---|---|
| `src/EditCardWnd.cpp` | `virtual ~PostBagFileCache() { SaveIndexFile(); }` in `src/PostBag.h` | **+36** (0x401650 EXACT) |
| `src/IniFile.cpp` | `virtual ~IniFile() {}` in `src/IniFile.h` | **+25** (0x452d30 EXACT at 32 B; 7→6 funcs — see below) |
| `src/LocalPlayerIdentity.cpp` | `virtual ~LocalPlayerIdentity() {}` in `src/LocalPlayerIdentity.h` | **+32** (0x452fa0 EXACT) |
| `src/WidgetBase.cpp` | `virtual void RepositionWithHotspot(int,int);` on `AnimDescRefObj0x477488` | **+159** (0x405c00 61 B + 0x454820 98 B, both EXACT on the first compile, exactly as recorded) |
| `src/TutorialWnd.cpp` | — | **−249** (0x452b00) |
| `src/TilePlacedObj.cpp` | — | **−60** (0x458310) |
| `src/WorldBoardMaybe.cpp` | — | **−951** (0x457ce0) |

Three findings, in order of how much they change the picture:

1. **⭐ There is a THIRD victim, never recorded by any of the five rows: `WorldBoardMaybe::
   FindNearestObjOfCategoryMaybe` (0x457ce0), 951 B — bigger than every gain in the cluster
   combined.** Bisected to a single cause by reverting `src/WidgetBase.h`/`.cpp` alone: 0x457ce0
   goes straight back to MATCH, so it is the `RepositionWithHotspot` DECLARATION and nothing
   else. Its residual at the bad position is DIFF(16). This is the same 951-byte function
   v448 identified as the `src/AppWindow.h` parity-cycle canary — it is evidently sensitive to
   the declaration count of MORE than one shared header, which is why nobody had priced it here.
   ⇒ **The `WidgetBase.h` lever is −852 B on its own and is not close to viable.** Its two
   verified bodies (0x405c00, 0x454820) stay held back, and the pre-existing byte-invisible
   wrong-call-target defect at `PlacementCursorMaybe::SetTypeMaybe`'s two call sites stays open.
2. **The 249 B is a SATURATING flip, not additive and not a parity cycle.** Each of the three
   in-class dtors was applied alone and each produced the *identical* `DIFF(32)` on 0x452b00.
   So the "paid once" premise the rows were built on is TRUE — and it does not help, because the
   whole cluster's gain is only +93 B (+99 with `??_GResourceRef`'s +6 lever-1 form) against it.
   ⇒ **No subset of this cluster is net-positive. The hypothesis is closed, not deferred.**
   Note also that the ONE thing that would rescue it — recovering 0x452b00 — needs to happen
   in the lever-ON configuration, and see 3.
3. **A real new lever on 0x452b00 was found, and it gets to within 4 bytes but not to zero.**
   Swapping the two SIBLING `RECT` LOCAL DECLARATIONS (`RECT rectWindow; RECT rectDest;` rather
   than the checked-in `RECT rectDest; SetRect(...); RECT rectWindow;`) takes it from
   **DIFF(32) → DIFF(4)** with `align=0 reg_pen=0 identity_miss=0 insns 83/83`. At that point
   the schedule, the registers and both RECTs' stack slots all agree exactly; the entire residual
   is 4 displacement bytes expressing one binary choice — which of two independent loads goes to
   `eax` first. The original puts the `rectDest` fields in `eax` and starts each accumulator from
   the `rectWindow` field; ours does the reverse.
   - ⚠ **And it is CONFIGURATION-DEPENDENT, which is the real lesson: the swapped order scores
     DIFF(30) with the levers OFF, where the checked-in order is EXACT.** The declaration order
     here is compensating for TU-level codegen state, not recovering a truer source order — so
     the checked-in form is the correct one and was restored. Do not "fix" it.
   - Refuted expression probes, all measured in the lever-ON config, **do not re-run**: two `int`
     temporaries with the stores sunk (32); the guard read interleaved between the two
     assignments (38); swapped addends `rectDest.right + rectWindow.left - rectDest.left` (30);
     the parenthesized width/height form `left + (right - left)` (4 — folded, identical to the
     plain form); `rectDest`-first association `right - left + left` (30); accumulators seeded
     from `rectWindow` then `+=` (29); swapped assignment-statement order, `.bottom` before
     `.right` (8). Other declaration/call permutations: `(rectDest, rectWindow)` decl order (32,
     i.e. no better than checked-in); `rectWindow` first with `GetWindowRect` before `SetRect`
     (35); `(rectDest, rectWindow)` with `GetWindowRect` first (63).

One incidental mechanic worth keeping, since it cost a `COMPILE FAILED` to learn: with
`~IniFile` in-class, **nothing in the repo needs the dtor out of line any more, so no TU emits a
`??1IniFile` COMDAT at all** and 0x452d50's marker has nothing to compare against — the file goes
7 funcs → 6 even as its byte count rises. A lever that folds a dtor inward can therefore RETIRE a
neighbouring marker, and the func count is the only place that shows up.

## v450 — src/WorldActionCursor.cpp

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x459720 | `WorldActionCursor::ClampRectIntoViewMaybe` | 180 B | compiled **180 B (byte-exact length)**, **insns 73/73**, total 24246, align 24, reg_pen 2, identity_miss 2, byte_diff 26 | EFFECTIVE. Every instruction pairs up; the only `-`/`+` rows in the whole `--dump` are WHERE the `g_rectAppClientBounds.right`/`.bottom` load is scheduled inside each of the two else-branches. The original slots it in BEFORE the `sub eax,<nativeWidth>`, reusing the now-dead `pKindDesc` register (`mov edx,[esi+0x40]; xor ebx,ebx; mov bx,[edx+0x14]; mov edx,ds:0x485228; sub eax,ebx; add eax,edx`); this compile emits it after the sub, in ebx. A scheduler/allocator coin flip with no source lever. **Two levers ARE baked in, do not undo:** (1) the `int nLeft` / `int nScrollX` temps — they are what puts the rect read ahead of the scroll-global read; without them the two loads swap and the score is DIFF(17); (2) the max-bound expression must live in an `else`-branch TEMP. **Refuted probe, do NOT re-run:** writing that expression inline in an `else if` and again in the call argument (the natural no-temp reading) breaks the two `RepositionWithHotspot` calls' cross-jump merge outright — 212 B, DIFF(112). |
| 0x45a400 | `WorldActionCursor::RefreshVariantMenuIconsMaybe` | 125 B | compiled **125 B (byte-exact length)**, **insns 45/45**, total 10113, align 10, reg_pen 1, identity_miss 1, byte_diff 3 | EFFECTIVE, and about as close as a non-match gets: 3 bytes. The original loads `pSelectedDecorObjMaybe` into a scratch register and LEAs the variant-list base out of it (`mov eax,[ebp+0x538]; lea edi,[eax+0x90]`); this compile loads straight into the induction register and adds (`mov edi,[ebp+0x538]; add edi,0x90`). **Three source shapes measured, all give the IDENTICAL 3-byte residual — do NOT re-run:** an indexed `while` loop, the same loop through an explicit `AnimDescRefObj0x477488 *pSel` temp, and the induction-pointer `do/while` that is checked in. **One lever IS baked in, do not undo:** the byte counter `i` must be DECLARED FIRST, ahead of both induction pointers — with a pointer declared first the counter's `xor bl,bl` sinks below both `lea`s and the score is DIFF(19). |

⚠ **Both were transcribed from scratch this session and BOTH landed at the original's exact byte
length on the first compile** — they are structurally complete, not first drafts. Their sibling
`RefreshCategoryMenuIconsMaybe` (0x45a480, 126 B) is EXACT.

⭐ **The header dial moved, and the price is now known.** Modelling
`SelectedObjWidgetMaybe::animDescMaybe` (+0xe4) in `src/WorldActionCursor.h` — which
0x459720 needs, since it tests the cursor against that sub-object's rect at 0x48538c — costs
`TilePlacedObj::GetFrontRowTilePosMaybe` (0x458310) its 60 B, flipping the same
`sub edx,eax`/`sub eax,edx` coin v442–v448 kept paying to this header family back to the bad
side. That is the 60 B v448 had just won back. **Net for the session's three functions is still
+66 B** (+126 exact from 0x45a480, −60 from 0x458310) plus 305 B newly PARTIAL, so it was taken.
`src/TilePlacedObj.cpp` is the ONLY one of this header's eight consumers that moves.
**Refuted probe, do NOT re-run:** declaring the member AFTER the class's two method declarations
instead of directly after `pSelectedObjMaybe` changes nothing — 0x458310 stays DIFF(18) at 60 B.
So `src/WorldActionCursor.h`'s dial is no longer "proven benign" for MEMBER additions; it is
benign for the METHOD declarations v448 measured, and knife-edged on 0x458310 for the rest.

## v450 — the declared-only harvest (three TUs)

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x435d10 | `Obj0x477758Base::ReserveMaybe` | 158 B | compiled **163 B**, **insns 67/67**, total 73694, align 72, reg_pen 15, identity_miss 15, byte_diff 44 | PARTIAL / EFFECTIVE-adjacent. Every instruction, call, constant and branch target is present and in the original's order; the whole residual is ONE root cause plus its register-rename cascade — VC5 ROTATES the trailing-NULL trim loop, peeling the first `m_ptr[i-1]` load above the loop and reloading at the bottom, where the original keeps the load at the loop TOP and branches back to it. **THREE loop spellings measured and ALL THREE compile byte-identically — do NOT re-run:** the `do { if (p[i-1] != 0) break; i--; } while (i > nCapacity);` form that maps 1:1 onto the original's basic blocks (checked in), the top-tested `while (p[i-1] == 0) { i--; if (i <= nCapacity) break; }`, and the compound `while (p[i-1] == 0 && --i > nCapacity) {}`. **Two levers ARE baked in:** both zero guards must be `nWanted > 0`, not `!= 0` (unsigned ⇒ the original branches `jbe` off a live zero register, not `je` — the same lever `TrackGraph::BuildAdjacencyAMaybe` needed), and the trim guard must read `m_count > nCapacity` in that operand order. One separate cosmetic residual: the original's final `m_ptr = 0` reuses the register already holding the collapsed zero where this compile stores an immediate. |

⭐ **What the harvest itself proved, and it is the reusable lesson: a DECLARED-ONLY row is the
cheapest work in the repo precisely because it needs NO header change, hence no rotation risk.**
Six functions transcribed across `src/DecorActor.cpp`, `src/PeerTrainSlotQueueMaybe.cpp` and
`src/Obj0x477798Family.cpp`; **four were EXACT** (three of them on the first or second compile)
and the two above are byte-exact in length. Not one consumer TU moved. Contrast the same
session's three `WorldActionCursor.h` rows, where the single MEMBER declaration one of them
needed cost an unrelated TU 60 B. **Prefer declared-only rows whose body needs nothing new on a
shared header, and check that before picking the row, not after.**

⚠ **One trap, and it cost a `COMPILE FAILED` caught only by the per-file TABLE: renaming a
method OFF its `FUN_<addr>` placeholder is a HEADER-WIDE rename.** `FUN_00434800` →
`MarkAllEntriesDirtyMaybe` on `DecorObjMgrMaybe.h` broke `src/AppWindow.cpp`, which calls it
twice and which this session never opened. `grep -rn '<oldname>' src/` before believing any
rename is done — the recipe in `docs/GHIDRA_RECIPES.md` already says so.

## v451 — VideoPlayer (new TU) + WindowBase's four declared-only rows

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x426020 | `WindowBase::ScheduleModeTransition` | 260 B | compiled **260 B (byte-exact length)**, **insns 98/98**, total 222, align **0**, reg_pen 2, identity_miss 2, **byte_diff 2** | EFFECTIVE, and the second-closest non-match in the repo after 0x44e830. Exactly ONE instruction differs, by register only: in the non-strip arm the original reads the height with `mov eax,[ecx+0xc]` where this compile reuses the now-dead pTarget register, `mov ecx,[ecx+0xc]`. Both stores land in `[esi+0x1c]`. **Three levers ARE baked in, do not undo:** (1) the two leading tests are INDEPENDENT `if`s, not an if/else — the original emits a third, redundant `cmp eax,ecx` after the null check, which is exactly what re-testing the same condition as a separate statement produces, and an if/else CSEs it away (total 54695 → 222 on this change alone); (2) `bFullRedraw` is a real `char` local seeded to 1 and cleared inside the `bResetCounters` block, NOT `RedrawCustomCursor(bResetCounters == 0)` at the call site — the latter compiles to the sete-materialization idiom instead of the original's plain byte store; (3) `SetTimer` is called in BOTH arms of the eraser test, and VC5 cross-jumps the call itself, leaving only the two `push <period>` immediates duplicated — hoisting it into one call after the branch forces the period through a register (`mov eax,0x32 / push eax`). |
| 0x454380 | `VideoPlayer::OpenAndPlay` | 274 B | compiled **254 B**, **insns 97/96**, total 140089, align 138, reg_pen 18, identity_miss 18, byte_diff 109 | EFFECTIVE. Structure, call order and stack layout agree instruction for instruction; the whole residual is one register-allocation decision and its ripple. The original spills `dwNotify` to its own stack slot (esp+0xc) and keeps NO dedicated zero register — it makes zeros with a short-lived `xor eax,eax` for the three `MCI_PLAY_PARMS` stores and uses literal `push 0` / `mov [esi+4],0` everywhere else. This compile keeps `dwNotify` in ebp and hoists one `xor edi,edi` for the whole body, so every literal zero becomes `push edi` and the null test becomes `cmp eax,edi`. That is the documented zero-register-residency class. **Refuted probes, do NOT re-run:** `LOWORD()` instead of `& 0xffff` (worse — the original does a 32-bit load then `and eax,0xffff`, `LOWORD` emits a 16-bit `mov bp,` load, so the mask spelling is the one to keep), and swapping the `dwNotify`/`mciPlay` declaration order (byte-identical, total 140089 either way). |

| 0x449100 | `UiIconListItem::HandleTextEditKey` | 136 B | compiled **138 B**, **insns 59/57**, total 36783, align 36, reg_pen 7, identity_miss 7, **byte_diff 13** | EFFECTIVE. Structure, both `strlen` intrinsics, every constant and both call sites agree; the residual is a PROLOGUE SHRINK-WRAP difference. The original pushes only esi, runs the `bTextRedrawEnabled` gate, and pops esi and returns on the not-editable path — pushing edi/ebx only afterwards for the body proper — while this compile pushes all three callee-saved registers up front and pops all three on the early out. Two instructions, plus one register coin-flip on the append store (`mov eax,[esi+0x60]` vs `mov edx,...`). **Two levers ARE baked in:** the rejected-edit paths must be EARLY RETURNS each carrying their own copy of the sound call — writing the accepted path as `if (ok) { …; Draw(); }` and falling through to one shared sound call leaves TWO `Draw` call sites, because VC5 cross-jumps the three identical sound tails but NOT the two `Draw` calls (insns 62/57, total 69236) — and the length guard must read `nLen >= nTextLen` in that operand order (the original is `cmp ecx,[esi+0x5c] / jae`; `nTextLen > nLen` swaps it to `cmp [esi+0x5c],ecx / jbe`). |

### ~~0x435c00 — structurally unreachable, not a residual~~ ⛔ **REFUTED AND CLOSED (v457)**

**This row's verdict was wrong; 0x435c00 is now MATCHED, EXACT, 152 B, and the marker lives in
the new `src/PlacedObjRegistryMaybe.cpp`.** The claim below — that only one of the two twins is
reachable while one `PlacedObjRegistryMaybe` models both instantiations, and that recovering the
other needs the struct split into two derived classes — rested on the assumption that the SOURCE
(`a - b`'s operand order) decides which twin comes out. It does not: **the TU does.** The
identical text gives 0x4361e0's order (156 B) inside `src/DecorActor.cpp` and 0x435c00's order
(152 B) in a small TU of its own. Nothing about the source changed to land it, and no struct
split was needed. 0x4361e0 is now the unclaimed copy instead — a 4 B net trade, taken because it
also puts the definition in the right .obj (0x435aa0 and 0x435c00 are ADJACENT in `.text`; the
pair is one original translation unit, and 0x4361e0 belongs to a different one).

Kept for the reasoning lesson: an "unreachable, needs a big structural change" verdict on a
duplicated-COMDAT twin should be re-tested from a DIFFERENT TU before it is believed. The
original text follows.

> `PlacedObjRegistryMaybe::CompareEntriesMaybe` exists TWICE in the image, at 0x435c00 and
> 0x4361e0: identical source compiled separately for the two registry instantiations, which is why
> the linker never folded the two COMDATs. They differ only in the operand evaluation order of the
> two half-word arms (0x435c00 loads pObj before pOther, 0x4361e0 the reverse). Plain `a - b` gives
> the 0x4361e0 order, so that is the twin the marker claims — EXACT, 156 B. **0x435c00 cannot be
> claimed while one `PlacedObjRegistryMaybe` models both instantiations**: one declaration, one
> definition, one COMDAT. Recovering its 134 B means splitting the struct into the two real derived
> classes (vtables 0x478018 over 0x478070, and 0x477f88 over 0x477fe0), which is a shared-header
> change and should be priced as one.

## v457 — the two DecorActorBase virtuals

| addr | name | orig | asmscore | verdict |
|---|---|---|---|---|
| 0x434260 | `DecorActorBase::HeadForObjectMaybe` | ~327 B | compiled **315 B**, **insns 123/125**, total 36355, align 36, reg_pen 3, byte_diff 25 | EFFECTIVE. ONE cluster, exactly the two missing instructions: in the idle path the original RE-READS `ptStepMaybe.x/.y` out of the object to build `ComputeStepTargetMaybe`'s argument list, where this compile forwards the `or eax,0xffffffff` it just stored into both members. Visible cause in the schedule: this build hoists the `bReady` load ABOVE the two -1 stores (0x10c vs the original's 0x11b), which is what lets store-to-load forwarding fire. Everything else — all three early-out arms, both `RAND_RANGE_MAYBE` pairs including their lo/hi swaps, and the duplicated slot-16 call tail VC5 emits for the y arm's zero-span guard — pairs instruction-for-instruction. Same no-caching family as Yoda lesson #19, but the SCHEDULER decides here, not the source shape. |
| 0x4343f0 | `DecorActorBase::PickReachableDestNodeMaybe` | 165 B | compiled **165 B** (exact length), **insns 64/64**, total 25464, align 24, reg_pen 13, byte_diff 34 | EFFECTIVE. **Two levers ARE baked in, do not undo:** (1) the reachability test is a SWITCH on 0x80/0xff, not `bDir != 0x80 && bDir != 0xff` — the original's `and eax,0xff; sub eax,0x80; je; sub eax,0x7f; je` is VC5's sequential-compare switch lowering with the second immediate as the DELTA (0x80 + 0x7f = 0xff), where the two-compare form gives `cmp al,0x80`/`cmp al,0xff` (DIFF 67 → 26); (2) the direction must land in a named `unsigned char` LOCAL before the switch, which is what produces the original's byte-store/dword-reload round trip through the stack slot (148 B → 165 B). Residual is one cluster: building `CalcSqDist`'s argument list, the original loads `papNode` BEFORE `ptDestMaybe.y` and finishes the `papNode[i]->pTile` chain between the two destination pushes. **Measured and REJECTED — do NOT re-run:** inlining the `pTile` local into the call expression is IDENTICAL, DIFF(26) either way (cl CSEs the repeated chain to the same schedule). |

⭐ **v451's own reusable lesson, extending v450's:** the "no header change" test is necessary but
not sufficient — also check whether the class the row belongs to is MODELED. `EffectSpawner`'s
four declared rows looked like the same cheap work and are not: `src/EffectSpawner.h` says the
object's layout is unread and declares no data members, so every one of those bodies needs the
struct first. `VideoPlayer` was the opposite and the best pick of the session — a complete
four-function class with no TU at all, so a brand-new `.cpp` could not rotate anything.

## v452 — the two no-TU descriptor classes (both fully EXACT; this section is the NEGATIVE result)

`src/Obj0x478118.cpp` (4/4 EXACT, 1250 B) and `src/CarKindDesc.cpp` (5/5 EXACT, 1219 B) landed
with no parked rows of their own. What is worth recording is what did NOT transfer.

### ⛔ REJECTED — the multi-condition predicate lever on `Obj0x4779e0::LoadMaybe` (0x41e6e0)

That function's parked tie class #1 ("the hotspot guard's 11-byte branchy bool materialization …
9 guard phrasings tried, ALL fold") is the SAME construct that took `Obj0x478118::LoadMaybe`
(0x436490) from DIFF(51) to EXACT — an inline `unsigned char` predicate wrapping the whole
short-circuit `&&` (see docs/CODEGEN.md; none of the 9 phrasings was a predicate, which is why
the class read as unsteerable). Retried on 0x41e6e0 by adding
`unsigned char IsHotspotUnsetMaybe() const { return hotspotX == 0 && hotspotY == 0; }` to the
TU-local `Obj0x4779e0ParsePartial` view. **Measured: −489 B, reverted.**

| | baseline | with predicate |
|---|---|---|
| `src/Obj0x4779e0.cpp` | 7/8 exact, **2942 B** | 6/8 exact, **2453 B** |
| 0x41e6e0 `LoadMaybe` | DIFF(218), len 762 | DIFF(**234**), len 775 |
| 0x41f0c0 `ParseEntryExitMaybe` | **MATCH** 489 B | DIFF(19) |

Two separate reasons, both worth remembering: the predicate DOES inject the 13 missing bytes
(762 → 775, the right shape) but the function's other two tie classes then score WORSE, so a lone
lever on a function with live residuals elsewhere can rotate more than it fixes; and adding an
inline member to a **TU-LOCAL VIEW STRUCT** rotates that TU's `/Og` state just like a shared-header
change does — `src/CursorDesc.h`'s note has always said parameterized decls THERE cost 0x41f0c0
its match, and this proves the view struct is no safer. Do not re-run either half.

### Measured, accepted shared-header price — `src/CarKindDesc.h`'s method block

Declaring BOTH `ParseTokenField` and `LoadMaybe` on `CarKindDesc` costs `src/PeerTrainNode.cpp`
its 0x40e520 (`CarNetObjAnchorPartial::CheckCarClearedDepotMaybe`) EXACT match — 220 B, MATCH →
DIFF(124) at 196 vs 220 B. Fully bisected, and the result is NON-MONOTONIC: `{dtor}` alone,
`{ParseTokenField, LoadHeadingOffsetTablesMaybe}` and `{LoadHeadingOffsetTablesMaybe, LoadMaybe}`
are each FREE; it is the `ParseTokenField` + `LoadMaybe` PAIR that trips it, with or without the
third. All three declaration orders of the trio were probed — identical. The data-model
corrections in the same edit (`short[400]` tables replacing `short[320]`+pad, and the
`unsigned short` car-id pair) are free. Kept, because the TU it unlocks is +1219 B: net +999 B.

## v459 — the CarNetState / CarNetStateAlt cluster (one residual left out of twelve)

The TU went 3/4 → 11/12 exact (513 B → 1674 B) once `nameA`/`nameB` were modeled as the member
classes `CardNameA`/`CardNameB` — see `docs/CODEGEN.md`'s vptr-store bullet for the general rule.
Everything else in the class is EXACT. What is left:

### 0x442c90 `CarNetState::AddDecal` — EFFECTIVE, DIFF(7), insns 55/55 at the exact 159 B

The entire residual is the two instructions before the `packedKind` store: the original loads
`nKind` into `cl` and `nSubkind` into `dl` (`dec cl; shl dl,3`); this compiles the pair the other
way round (`shl cl,3; dec dl`). Both agree on which STACK SLOT each parameter is (`nSubkind` at
`[esp+0x10]`, `nKind` at `[esp+0x14]`) — only the register roles are swapped. Documented
symmetric-register-swap class (Yoda #29/#30).

**⚠ It is INDUCED, not intrinsic.** This function was EXACT for many sessions and only flipped
when `CardNameA`/`CardNameB` landed in `src/CarNetState.h` and rotated the TU. That is the price
paid for +609 B of constructors, so it is accepted, not a regression to chase.

Probed and REFUTED (all three compile to byte-identical output — cl canonicalizes `|` before
register allocation, so no surface spelling reaches this):
- operand order: `(nKind - 1) | (nSubkind << 3)` instead of `(nSubkind << 3) | (nKind - 1)`
- hoisting the decrement: `unsigned char byKind = nKind - 1;` then `byKind | (nSubkind << 3)`

### ⛔ Also refuted this session — the default ctor's pre-CardName shape

Before the member classes existed, all three ctors sat at DIFF(9)/DIFF(20)/DIFF(10) with the vptr
store 3 stores too early. Chaining the clears (`nameA[0] = nameA[20] = nameB[0] = 0`) does NOT
move the vptr and additionally REVERSES the three stores (right-to-left evaluation emits +0x25,
+0x24, +0x10). Strictly worse; the real fix was the layout, not the statement.

### Accepted shared-header price — `src/CarNetState.h`'s CardNameA/CardNameB

Costs `src/TutorialWnd.cpp` its `RestorePresenterBackdrop` (0x452b00), **−249 B** — the same
victim function, at the same price, that `src/LocalPlayerIdentity.h`'s in-class-dtor lever hits,
and which v449 bisected as a SATURATING flip rather than an additive one. Kept: +642 B in
`src/CarNetState.cpp` against it, net **+393 B**.

### `HasOpaquePixelInWorkSurfaceRect` (0x42c9f0, `src/LocoBitmap.cpp`) — callee-saved coin flip

282 B vs 284, insns 89/91, total 99073. Prologue + lock block (0x00-0x4e) and unlock block +
epilogue (0xe9-end) BYTE-IDENTICAL; every instruction of both loops corresponds 1:1. The original
spends EBP on `nCols` and so has no register free — it hoists only `g_nRedShiftPos` (which must
live in CL anyway) and re-loads both colour masks from memory each iteration, keeping the pixel
alive in ESI. cl puts `nCols` in EDI, freeing EBP for a hoisted `g_nRBitMask`, which lets the blue
test fold its load into the operand and destroy the dead pixel — that fold is one of the 2 missing
instructions; the other is the original's `test eax,eax` before the zero-trip `jbe` (it schedules
two flag-clobbering ops between the mask and the branch; cl reuses the mask's flags).

Baked in: the red channel's `(short)` cast — NOT `unsigned short` — is what yields the original's
16-bit `cmp ax,0x1f`. REFUTED (do not re-run): operand order across both masks, in all four
spellings, is byte-identical (cl canonicalizes the commutative AND); `*p` twice instead of a
`pixel` local (91/91 insns but 102868); hoisting the `pixel` declaration, and declaring the loop
counters outside the loops (both byte-identical); named per-channel locals before the `if`
(127762); declaring `p` ahead of nRows/nCols (127333). `int nRedShift = g_nRedShiftPos;` scores
better (97353) but is REJECTED on evidence: a source local pins the load BEFORE the zero-trip
guard, and the original's sits AFTER it in the preheader — i.e. compiler LICM, not a source local.

### `NetSessionEventQueue::PickRandomBigObjByCategory` (0x41e1f0) — callee-saved allocation choice

884 B vs 888, insns 348/350, total 192053 — first-draft transcription of what was the LARGEST
unclaimed APP gap. All FIVE switch arms are structurally correct: the complete diff over 350
instructions is 61 rows and every one is a register-assignment consequence, with no structural
disagreement anywhere. The original spends EBP on the per-arm nTarget/nSeen and therefore re-reads
`category` from its incoming stack slot (`mov ecx,[esp+0x18]`) at the one site needing it; cl
parks `category` in EBP for the whole function instead. The 2-instruction shortfall is exactly
that reload plus one of the original's two `xor edi,edi` loop-counter zeroings (cl reaches those
with an already-zero register), against one extra instruction of mine in the case -1 arm, where
the original folds the &pEvents and vtable loads into `lea ecx,[esi+4]; mov eax,[ecx]`.

⚠ Ghidra's decompiler is badly wrong here and is NOT a usable model: it mis-reads the `push ecx`
local-slot reservation, so the result slot at [esp+0x10] never appears (every arm reads
`return 0` where the real epilogue is `mov eax,[esp+0x10]`), the `category` parameter surfaces as
a bogus `unaff_retaddr`, and two blocks are dropped as unreachable. Frame re-derived by hand.

Baked in: `nPlacedObjectCount` retyped `int` -> `unsigned int` (the case -1 arm's `rand() %
nPlacedObjectCount` is the codebase's only discriminating site: unsigned `div` vs signed `idiv`);
measured repo-wide as byte-neutral. REFUTED (do not re-run): hoisting the loop locals to function
scope VC5-old-for-scope style is byte-identical — the allocator is live-range based, not scope
based.

### `WorldBoardPartial::CheckInsertSeqPerimeterMaybe` (0x456d90) — prologue scheduler tie-break

752 B, DIFF 27, insns **241/241 — an identical instruction multiset**, total 44131. Everything
from COMDAT offset 0x67 to the end (all four perimeter walks plus the out-of-line cold block) is
byte-identical; the entire residual is six rows of prologue scheduling. The original interleaves
the two descriptor byte loads before either `pObj->pos` add (`movzx cols, movzx rows, load temp,
add posX, add posY`); cl pairs each load with its own add instead, which frees pKind's register
ahead of pObj's and lands the `start.wPosY` reload in edi where the original uses esi — the single
reg_pen, and the `dec ecx` placement follows from the same choice.

A minimal-TU probe (the function plus `WorldBoardMaybe.h`/`TilePlacedObj.h` alone) reproduces
DIFF(27) to the byte, so this is INTRINSIC scheduling, not /Og TU-state carry: repositioning it
inside `src/WorldBoardMaybe.cpp` cannot move it.

REFUTED (do not re-run): swapping the extents' commutative operands to
`pObj->pos.wPosX + pKind->bBitmapOccupancyColsMaybe - 1` is BYTE-IDENTICAL (cl canonicalizes);
moving `start = pObj->GetFrontRowTilePosMaybe()` below the two extent computations is DIFF 542
(the call is genuinely first in the source — hoisting the extents above it spills both across it);
declaring x/y ahead of xEnd/yEnd is DIFF 108.

Baked in and load-bearing: `char bOk = 1;` declared AHEAD of `BigObj *pKind` (DIFF 56 -> 31 — it
is what puts pKind, not bOk, in the dead incoming-argument slot at [esp+0x38]); `unsigned int i`
declared AFTER x/y (31 -> 27); `TileGridPos start;` declared and assigned as two statements rather
than initialized (DIFF 666 -> 56 — the original materializes the NRV temp at [esp+0x24] and copies
it to `start` at [esp+0x28], which only an assignment produces); the single `||` condition
`(pOcc == NULL && lWant != 0) || (pOcc != NULL && lWant != pOcc->pKindDesc->resourceId)` rather
than nested ifs (472994 -> 308029 — the redundant second `test eax,eax` in the original is the
re-evaluation of the second term's null test after the first term's short-circuit); and
`TileGridPos`'s members flipped to signed `short` (308029 -> 87078, see src/TilePlacedObj.h).

### `WorldBoardPartial::GetNeighborObject` — 0x4579d0, `src/WorldBoardMaybe.cpp`, DIFF 52 / 392

EFFECTIVE, `insns 146/145`. The COMDAT is 392 bytes: 376 of code, a 3-byte alignment nop and a
16-byte jump table, so `--len` has to come from the next function's start (0x457b60) — Ghidra's
`Body:` span stops at the last INSTRUCTION and would truncate the compare window by the whole
table (the trap CLAUDE.md records for switch-heavy functions).

Everything outside four rows is byte-identical. Those four are one instruction of register
allocation around the SECOND entry/exit point: the original still has `pt` occupying [esp+0x10]
when it materializes `ptFar`, so it gives `ptFar` its own retbuf at [esp+0x18] and reads it back
through the returned pointer (`mov ecx,[eax+4]`); we let cl reuse `pt`'s slots for `ptFar` and
reach them off esp.

Baked in and load-bearing: the two entry guards written as ONE `||` rather than two `if`s
(212002 -> 91964, and the length went 412 -> 396 — two separate `return NULL;` statements each got
their own inlined epilogue, where the short-circuit sends both arms to the single shared exit the
original has); and `pt` ASSIGNED while `ptFar` is INITIALIZED (249974 -> 68911), lever #34 read in
both directions inside one function.

Probes run and REJECTED (do not re-run): hoisting the `CalcSqDist` result into a named local, and
moving the `POINT pt;` declaration to the top of the function — both BYTE-IDENTICAL.

## v482 — BuildToolCursorWnd's message-box confirmation pair, and the TrackGraph twins

| addr | name | state | residual |
|---|---|---|---|
| 0x45ce40 | `TrackGraph::BuildAdjacencyAMaybe` | DIFF(317), insns 283/274 | was DIFF(384); lever #36 closed the loop shape |
| 0x45d1c0 | `TrackGraph::BuildAdjacencyBMaybe` | DIFF(316) | was DIFF(388); same fix, twins agree again |
| 0x437f90 | `BuildToolCursorWnd::OnLButtonDown` | DIFF(254), insns 229/235 | new this session |
| 0x437180 | `BuildToolCursorWnd::OnKeyDown` | DIFF(344), insns 340/344 | new this session (`--len 0x370`, NOT Ghidra's 700) |
| 0x4370f0 | `BuildToolCursorWnd::PostToolModeChangeMaybe` | DIFF(69), insns 45/47 | new this session |

**The BuildAdjacency twins** are down to three individually-diagnosed instructions: one
`and eax,0xff` that is the documented price of the byte-wide direction-helper parameter (refuted
twice, v431 and v482 — see `docs/CODEGEN.md` #36's confirmation note), one `cmp eax,ebx` vs
`test eax,eax` zero-register tie-break, and one scheduling swap. The old autopsy above 0x45ce40
blaming two scratch-collection expansions and promising a +1767 `Obj0x477798Family.h` lever was
STALE — v431 already spent that lever; the note is rewritten in place.

**`PostToolModeChangeMaybe` (0x4370f0)** is the same intrinsic as its already-parked twin
`NotifyToolModeChanged` (0x438890): the original materializes `g_pApp->hwndOwner` separately in
each of the two mutually-exclusive case arms, and cl CSEs the load across them no matter how the
arms are spelled. Declaring the case list before case 8 (rather than after, as 0x438890 needs)
was worth 71 → 69 and nothing more.

**`OnLButtonDown` / `OnKeyDown`** are content-complete; their dumps are almost entirely
register-rename rows, the same class already parked across this TU's `DrawIconFrame` /
`DrawAllIconSlots` family (cl wants one more callee-saved register to hold a value across
`RestoreOverlapBlt` where the original reloads it from a struct field). Levers that DID land, in
order of value: the POINT's `pt.x` BEFORE `pt.y` (515 → 358 — the opposite order from
`TutorialWnd::HitTestControl`, so the rule is "assignment order is register order", not "y first");
a `RECT *pRect` pointer local reused for all four field reads and the blit destination instead of
member access off `this` (539 → 515, and it brought the compiled length to exactly the original's
714); and caching `pBitmapC` in one local used by BOTH blits rather than re-reading it for the
second (358 → 254).

## v485 — the registry family base/derived split

| addr | name | residual | note |
|---|---|---|---|
| 0x424820 | `PlacedObjRegistryMaybe::FindIndexMaybe` | DIFF(70), insns 79/79 | new this session |
| ~~0x454050~~ | `RoadVehicleActor::CanStandAtMaybe` | **RESOLVED v518 — EXACT again.** The v518 DecorObjMgrMaybe.h ctor/registry edits turned the same header dial back ON; source untouched. Historical: DIFF(13), insns 190/191 | **REGRESSION**, header dial |
| 0x452b00 | `TutorialWnd::RestorePresenterBackdrop` | DIFF(32), 249 B | **REGRESSION**, header dial |

**`FindIndexMaybe` (0x424820)** is content-complete at an equal 79/79 instructions with every
call, constant, block and branch target in the original's order. The whole residual is one
register coin-flip: the original parks `nHi` in `ebx` and `pObj` in `ebp`, this compile swaps
them, and because `ebp` is then the busier of the two the binary half spills `pObj` and reloads
it at each of its three uses where the original hoists it once. Nothing in the source names which
callee-saved register a parameter lands in. One lever DID land and is baked in: the identity test
must read `pObj == pArrayMaybe[i]` in that operand order (reversed, VC5 emits `cmp mem, reg`
where the original has `cmp reg, mem`) — the same commutative-compare lever `ReserveMaybe`'s trim
guard needed.

**The two REGRESSIONS are one dial, measured and bisected** (see `docs/CODEGEN.md`'s
parameter-budget bullet). `src/Obj0x477798Family.h` reaches a wide fan-out through
`PlacementCursorMaybe.h`, and giving three of its declared-only placeholder slots their real
PARAMETER flips a register-allocation coin in two unrelated TUs. Bisected in four steps and the
result is worth recording precisely:

> Renaming `_v02`…`_v12` to their real names — FREE. Changing their RETURN TYPES (`void *` →
> `void` / `TilePlacedObj *` / `int`) — FREE. Adding ONE parameter — costs both matches, and
> which of the three declarations gets it makes no difference. Adding two or three costs exactly
> the same, so it is a THRESHOLD, not a parity bit, and it does not undo.

Adding declarations partly compensates, per TU and non-monotonically: `src/WorldBoardMaybe.cpp`'s
951 B `FindNearestObjOfCategoryMaybe` came back at +2 trailing declarations and stayed back at
+3 and +4, and `src/WalkerActor.cpp`'s 112 B came back once slot 21 was declared. RoadVehicle's
504 B did NOT come back at any declaration count tried (1, 2, 3, 4) — its trigger is the
parameter itself. Taken deliberately: the session claimed 864 B of new EXACT in the family
against these 753 B, and the dial is expected to flip back as the model converges.

⭐ **0x452b00 IS THE ALREADY-CATALOGUED SERIAL VICTIM, and that changes the accounting in this
repo's favour.** `TutorialWnd::RestorePresenterBackdrop` is the same 249 B that the v442–v444
in-class-dtor cluster (`??_GIniFile`, `??_GResourceRef`, `??_GLocalPlayerIdentity`,
`??_GPostBagFileCache`) and the `AnimDescRefObj0x477488::RepositionWithHotspot` row further up
this file each independently measured as THEIR price, and which this file already answers with
"0x452b00 can only be lost ONCE, so fold this into the planned single repo-wide sweep". It has
now been lost. **Every one of those five parked improvements is therefore cheaper than its
recorded price from here on** — four of them were EXACT on the first compile and were reverted
solely to protect this function. Whoever runs that sweep next should re-measure them all against
the CURRENT baseline before assuming any of the recorded net figures still hold; several of them
should now be straight gains. The v485 loss is not a sixth instance of the cluster, it is the
event the cluster was waiting for.

## v488 — the AppWindow .obj head (0x4061e0..0x4068c4)

Four of the six functions in this run went EXACT. The two below are transcribed, at exactly the
original's length, with the whole body accounted for — only intrinsic allocator/scheduler
tie-breaks left. Full autopsies are in `src/AppWindow.cpp` beside each definition.

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x406480 | `AppWindow::LoadWindowAndBalancing` | 500 | DIFF(2), insns 130/130, align=0, reg_pen=4 | The two extent-clamp `lea`s pick the other SIB base/index (`[edx+ecx-0xa]` vs the original's `[ecx+edx*1-0xa]`). The v329 LEA coin-flip class. Refuted probes: parenthesising as `left + (width - 10)`; writing the sum reversed as `width - 10 + left`. Neither moved a byte — see CODEGEN's "the `lea` SIB base/index order is not driven by source operand order". |
| 0x4062e0 | `AppWindow::ReadOwnFileVersion` | 416 | DIFF(121), insns 146/148, align=16, byte_diff=17 | Two instructions plus three SIB coin-flips. The original keeps a provably-redundant `test ebp,ebp; je` before `operator delete` that VC5 folds under every spelling tried (explicit `if (p)` guard, plain `delete p` inside the guard, hoisted out to a sibling, and `new BYTE[n]`/`delete[]` — the array operators ICF-fold onto the scalar ones, so that spelling is byte-invisible). The declaration order of `lpValue`/`pInfo` IS already the better of the two and is load-bearing (swapping costs 4 bytes). |

> ⚠⚠ **v490 UPDATE — 0x457ce0 is EXACT again, and the paragraph below is superseded as a
> PREDICTION while remaining accurate as a MEASUREMENT.** v490 did not touch `src/AppWindow.h` at
> all. It deleted a TU-LOCAL VIEW STRUCT from `src/WorldBoardMaybe.cpp` itself —
> `PeerTrainSlotQueuePaintView0x456700`, a one-method alias for 0x44e2e0 that became a live
> `lint_alias.py` finding the moment that address got a real definition — and 0x457ce0's full
> 951 B came straight back, with zero collateral anywhere else in the repo.
>
> The lesson is that this function's dial is **not** `src/AppWindow.h`'s declaration count
> specifically. It answers to a declaration/definition count that spans several headers AND its
> own TU's file-local declarations, exactly as the v445 section further up already suspected. v488
> was right that no `src/AppWindow.h` count buys it back; it was wrong to conclude the dial was
> therefore SPENT, because a knob in a completely different file was still free. **Before pricing
> any future change against this function, re-measure — and note that removing a file-local
> declaration is one of the levers.**

**As measured in v488: `src/WorldBoardMaybe.cpp`'s `FindNearestObjOfCategoryMaybe`
(0x457ce0, 951 B) was SPENT and a live row.** It is the `src/AppWindow.h`
declaration-dial victim v423/v448 documented. v488 re-measured the dial from a clean baseline and
**refuted the parity model**: +1, +3 and +4 added declarations all knock 0x457ce0 out, so there is
no declaration count that buys it back and v448's "add a third and it returns" recipe does not
generalise. The +1 measurement also refutes CLAUDE.md's general "free-function declarations do not
move it" rule for this header — the single declaration was a plain `__cdecl` free function and it
cost the full 951 B alone. Its residual is unchanged from v448: DIFF(16), insns 327/327, align=8,
reg_pen=13 — ONE zero-register rotation (the original keeps the ring counter / zero in `ecx`, this
compile picks `eax`) cascading into a `movsx` scheduling swap and a `lea` operand swap. Refuted
probe: reversing the top-edge loop bound to `ring + ptOrigin.x` changes nothing, because the swap
FOLLOWS the register rotation rather than driving it. Because the dial is spent, further
declarations on `src/AppWindow.h` are now free with respect to this function.

## v490 — the PeerTrainSlotQueue interior run

Three of the nine unmarked functions interior to `src/PeerTrainSlotQueueMaybe.cpp` were
transcribed. All three are content-complete; the rows below are what is left.

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44e160 | `PeerTrainSlotQueueMaybe::IsTileOccupiedByAnyTrainMaybe` | 160 | DIFF(68), insns **62/62**, align=56, reg_pen=5 | Structure is exact. ONE allocation coin-flip and its cascade: the original keeps `bFound` in AL (so `return bFound` needs no move) and SPILLS the loop counter to `[esp+0x10]`; this compile keeps `i` in EAX and spills `bFound` to `[esp+0x13]`, costing an extra `xor eax,eax` and two `mov al,cl`. Same zero-register class as 0x457ce0. The exit shape IS steerable and is already paid: `bFound = 1; break;` instead of `return 1;` took it 92725 → 56626. Refuted (all three BYTE-IDENTICAL, do not re-probe): `i < 4 && !bFound`, a leading `if (bFound) break;`, and both together. Also refuted: hoisting `i`'s declaration above `bFound`. |
| 0x44e200 | `PeerTrainSlotQueueMaybe::CollectAndSortVisibleCarsMaybe` | 208 | DIFF(175), insns **75/77**, align=120, reg_pen=28 | Every basic block and test is present in the original's order; register assignment differs throughout. The original RELOADS `this` from `[esp+0x14]` at the top of each outer iteration and jumps over that reload on the first pass; this compile keeps `this` live in a register. Refuted (BYTE-IDENTICAL): the early-return guard vs Ghidra's wrapping `if (nActiveCount != 0) { ... }`. |
| 0x44e2e0 | `PeerTrainSlotQueueMaybe::DrawVisibleCarsInTileMaybe` | 272 | DIFF(48), insns **88/89**, align=58, reg_pen=2 | Two levers already paid, do NOT undo: re-reading the car as `(*ppCar)->` rather than caching it in a loop-local (130107 → 78267, insns 96 → 88), and retyping `CarNetObj::HitTestTileMaybe`'s dead 4th parameter `short` → `unsigned char` (78267 → 58257). What is left is one stack-slot assignment coin-flip — the original puts the counter at `[esp+0x10]` and the walk pointer at `[esp+0x14]`, this compile swaps them — plus the single instruction its loop tail implies (`inc; mov; dec; cmp; jl` on the pre-increment value vs `inc; mov; cmp; jle`). Refuted, and MUCH worse (58257 → 119062): spelling the outer loop `do { ... } while (i++ < 16)` to reproduce that tail; it drags the null check up into the prologue. Hoisting the counter's declaration does not move the slots either. |

⭐ **A caller pinned a callee's parameter type.** `CarNetObj::HitTestTileMaybe`'s 4th argument is
dead in the callee body, so the callee can never pin it — its own codegen is identical for every
4-byte-slot type. 0x44e2e0 pins it from the OUTSIDE: it passes the owning train's `bUnk0x2c` as a
bare `mov dl, byte ptr [eax+0x2c]` with the register's upper bytes left as garbage, which is what
a byte argument looks like; declared `short` it was a `movzx` at each of the two sites. Worth
remembering as a general move — for a dead parameter, the call sites are the only evidence there
is.

## v491 — PeerTrainSlotQueue interior run, part 2

Four more of the interior functions were transcribed. Three are EXACT and carry no row; only
`ClaimSlotForTrain` is parked.

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44df40 | `PeerTrainSlotQueueMaybe::ClaimSlotForTrain` | 224 | DIFF(175), insns **81/78**, align=126, reg_pen=4 | Every block is the original's. Three levers already paid, do NOT undo (220747 → 126497 together): (1) the whole body wrapped in `if (nActiveCount < 4)` with ONE trailing `return bClaimed`, which makes the flag a real phi and pins it in BL — the early-return spelling constant-folds every exit to a literal and drops EBX entirely; (2) the `connectionMode` dispatch as a `switch`, not an if/else-if chain — the original's `dec eax; je / dec eax; je` is cl's dense-switch form and is what tail-merges the two `PickRandomBigObjByCategory` calls onto one shared `mov ecx,0x4a9990; call`; (3) the arrival-tile null chain as one short-circuit condition rather than a nested re-test. What is left is ONE loop-rotation choice: this compile PEELS the `i == 0` slot test (`mov edx,[ecx+8]`, index folded away) and runs a bottom-tested loop from the increment; the original keeps the test at the loop top with the backedge returning to it. That sinks the roster-full exit to the end of the function, where it materializes 0 as `xor al,al` instead of reusing the already-zeroed BL. Refuted (BYTE-IDENTICAL, do not re-probe): `while (aSlots[i] != 0) { i++; if (i >= 4) ... }` vs `while (...) { if (++i >= 4) ... }`; and both spellings of the arrival-tile condition (`p == 0 && (p = Pick(1)) == 0` with the arms one way, `p != 0 || ... != 0` with them the other) — so the if/else ARM ORDER is not source-steerable here. Refuted and WORSE (245 B, adds a fourth epilogue): `for (;;) { if (aSlots[i] == 0) break; ... }`, and giving the `EnqueueClaimWaiterMaybe` arm an explicit `return`. |

⭐ **The SUBSCRIPT-vs-POINTER-WALK lever landed two EXACT matches this session** (0x44c310 and
0x44e630) and is now the first thing to try on any residual whose diff is register-role or
`lea`-placement noise inside an array walk. See `docs/CODEGEN.md`.

## v492 — the last two big PeerTrainSlotQueue interior functions

Both of the session's transcriptions are parked; neither is close enough to call EFFECTIVE, but
both are structurally the original's and the residual is diagnosed in each case.

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x44dc10 | `PeerTrainSlotQueueMaybe::SpawnOrAssignRandomTrain` | 648 | DIFF(544) at 697/656 B, insns **228/213**, reg_pen=**2**, identity_miss=2, byte_diff=89 | Register allocation is essentially the original's — a penalty of 2 across a 213-instruction body — and the ENTIRE 15-instruction excess is one block cl declined to cross-jump. The original reaches its `nActiveCount++/nOwnedCountMaybe++/ClaimDecorObjMaybe` tail (0x44de56) from THREE places (two jumps out of the random-consist arm, a fallthrough out of the selection arm) and emits it once; this compile emits the two source copies separately. Same VC5 /Og trace-layout class as the 0x40bbd0 family. Three levers paid, do NOT undo (226579 → 144309): (1) the tail stays DUPLICATED in both arms; (2) `switch (rand() % 3 + 2)` with cases **2/3/4**, not `switch (rand() % 3)` with 0/1/2 — the switch value IS the car category it then hands to `AllocCarSlot`, and the original's otherwise inexplicable `add edx,2` right before the switch's own `sub edx,2` normalization is the tell (the 0/1/2 spelling gives `sub edx,ebx`); (3) the random car count is UNSIGNED (`jbe` against 0; a signed `int` gives `jle`). Refuted and WORSE — do not re-probe: hoisting the tail out of both arms behind a shared `if (aSlots[i] != 0)` (170870, reg_pen 2 → 7), and early `return aSlots[i]` for the null/destroy paths instead of nesting (226579 — it DOES share the claim tail, but cl then stops merging the three copies of the two-instruction `aSlots[i]` exit load, which costs more than it saves). |
| 0x44e3f0 | `PeerTrainSlotQueueMaybe::CheckDerailCollisionMaybe` | 561 | DIFF(456) at 517/561 B, insns **164/171**, align=268, reg_pen=54 | Structure is the original's throughout; the residual is one whole-body register-allocation choice. The original keeps `pNode` in EBX and `pLead` in EBP for the entire body and SPILLS the two rect edges (`nLeft` → [esp+0x1c], `nTop` → [esp+0x24]) plus two of the four `rectViewport` dwords; this compile keeps the rect values in registers and reloads `pNode`/`ppOther` from the frame instead, which is why it comes out 7 instructions SHORTER rather than longer. Levers paid (346055 → 274454): the rect tests spelled FIELD-FIRST (`pCar->rect.left < X`, `pCar->rect.right > X`), `nActiveCount >= 2` rather than `> 1`, explicit `nLeft`/`nTop` locals, and the dead-guard spelling below. |

⚠ **A DEAD BRANCH IN THE ORIGINAL CAN BE UNREACHABLE FROM SOURCE.** 0x44e3f0's per-car guard is
`dwModeAMaybe == 2 && dwModeAMaybe == 3 && dwModeBMaybe == 2` — an engine bug
(`docs/engine-bugs.md`), never true, but the original EMITS it in full. cl 5.0 FOLDS it away
whenever both reads are written as the SAME source expression: measured for all four
same-expression spellings — plain `&&`, three nested `if`s, a cached `int` local, and
all-`(*ppOther)->` — every one scores an identical 324077 with the block gone. Only two DIFFERENT
expressions naming the same field survive the fold, so the transcription deliberately spells one
read `(*ppOther)->` and the other `pOther->`. That leaves one residual instruction (the original
CSEs its two reads into one load; the mixed pair compiles to two memory compares) and is the best
of every spelling probed. ⭐ Generalises: when a provably-dead comparison chain survives into the
original's bytes, the source cannot have been the obvious spelling of it.


### ~~⛔ `PeerTrainSlotQueueMaybe::DrainPendingSlotsMaybe` (0x44e020, 311 B)~~ — **LANDED v522.** The body below was pasted verbatim into `src/PeerTrainSlotQueueMaybe.cpp` with a TU-local `PeerTrainNodeDrainTickPartial` view instead of the `PeerTrainNodeTickPartial` header move (that move is REFUTED for the current tree — it alone flips `src/TilePlacedObj.cpp`'s 0x4588b0 EXACT → DIFF(79), 143 B). With only the header declaration added, the landing was clean AND recovered `src/WorldBoardMaybe.cpp`'s 0x457ce0 to MATCH (951 B EXACT — it was DIFF(16) at baseline; the dial now runs the other way, so the v492 curve below is historical). Landed at exactly the documented score: PARTIAL DIFF(222), 295/311 B, insns 97/104. The section below is kept for the residual documentation and the ⭐ canary-baseline rule.

### ⛔ `PeerTrainSlotQueueMaybe::DrainPendingSlotsMaybe` (0x44e020, 311 B) — FINISHED, MEASURED, AND HELD OUT OF THE REPO BY ONE DECLARATION

The body at the end of this section is **complete and verified**, not a sketch: **score 76348,
insns 97/104, DIFF(222) at 295/311 bytes**. The residual is two things and nothing else — (1) the
documented `sete` prologue class (the original renders `g_nScreenState == 3 || g_nScreenState == 9`
as `xor ecx,ecx; cmp eax,3; sete cl; test cl,cl; jne` and again for 9, six instructions where cl
here emits a plain `cmp`/`je` pair; same class as 0x456150 and 0x462e90) and (2) one
`mov ebx,eax`, after which every remaining row is a straight EDI<->EBX rename. Remove the `sete`
block and the two bodies are the same code.

The one source lever, worth 130715 -> 76348 alone, is the OPPOSITE of this TU's usual #51 idiom:
the deferred-move arm must **cache** its re-read of the slot in a local and walk THAT, not repeat
`(*ppNode)->` at each use. The original issues exactly one `mov eax,[edi]` at the top of that arm
then a `lea esi,[eax+0x38]` pointer walk; repeating the dereference makes cl abandon the pointer
walk for a byte-offset index (`mov ecx,[ecx+edi]` / `cmp edi,0x58`) and costs the register the
original's spill-and-reload of `this` needs. The FIRST passenger loop still reads through the
cached `pNode` from before the branch -- the two loops genuinely disagree about which pointer they
walk, and that asymmetry is the original's.

**Why it is not in `src/`.** Declaring it on `src/PeerTrainSlotQueueMaybe.h` costs
`src/WorldBoardMaybe.cpp`'s 0x457ce0 its full 951-byte EXACT -- a net loss against the 311 bytes
it adds. That header's declaration count is a live dial for that function; the curve, measured
from a healthy baseline, in ordinary member declarations added on top of
`CheckDerailCollisionMaybe`:

| extra decls | +0 | +1 | +2 | +3 | +4 | +5 | +6 |
|---|---|---|---|---|---|---|---|
| 0x457ce0 | healthy | **DIFF** | **DIFF** | **DIFF** | healthy | healthy | **DIFF** |

So it needs **three more honest ordinary-method declarations** on that header to reach the `+4`
rung, and only two exist today (`TeardownAllSlotsMaybe` 0x44d9b0 and
`ReleaseSlotsOverlappingRectMaybe` 0x44dad0). ⚠ **A CONSTRUCTOR declaration does NOT count** --
the 0x44d800 ctor plus those two lands on `+2`, not `+3` (measured). The only members left are
the destructor pair (0x44d860 / 0x44d830), and declaring them properly would force the class off
its `void *pVtbl` field model onto a real vtable. Consequence: `TeardownAllSlotsMaybe` also stays
on `src/AppWindow.cpp`'s `__fastcall` escape hatch as parked `// TODO: idiom` debt -- folding it
onto the class is correct and costs 951 B on its own.

Two things measured along the way that are NOT the blocker, so nobody re-derives them:
  * moving `PeerTrainNodeTickPartial` out of `src/PeerTrainNode.cpp` into `src/NameAnchorMaybe.h`
    (8 declarations, reaching `src/WorldBoardMaybe.cpp` through `TilePlacedObj.h`) is **completely
    byte-neutral** -- all five TUs that see it hold at baseline. That move is the other half of
    landing this function and it is free;
  * `TrackConnectorTileObj::PumpClaimWaitQueueMaybe` added to `src/TilePlacedObj.h` as a currency
    does NOT recover 0x457ce0 and BREAKS `src/PlacedObjRegistryMaybe.cpp` (329 -> 177 B). Refuted.

⚠⚠ **THE MISATTRIBUTION THAT NEARLY BURIED THIS -- read it before pricing any dial move.** The
struct move above was made FIRST and was the obvious suspect when 0x457ce0 broke. It was innocent.
The cost came from the header declaration, and the trail was muddied further by a small
separately-motivated cleanup (folding `TeardownAllSlotsMaybe` onto the class) that was itself
"measured byte-neutral" -- except that measurement was taken while 0x457ce0 was ALREADY broken, so
"still 2786 B" read as "no effect" when it was really a second, independent 951-byte cause.
⭐ **The rule: a dial measurement is only meaningful against a baseline where the canary is
HEALTHY.** A "no change" reading on an already-broken canary is not weak evidence of innocence --
it is no evidence at all. Re-measure from a clean tree, or bisect by reverting each candidate
individually, before attributing a loss.

**To land it:** move `PeerTrainNodeTickPartial` verbatim from `src/PeerTrainNode.cpp` into
`src/NameAnchorMaybe.h` (immediately before `struct PeerTrainNodeCarAdvancePartial;`), add
`#include "GameNetMsgQueue.h"` and the member declaration to `src/PeerTrainSlotQueueMaybe.h`, and
paste the body below into `src/PeerTrainSlotQueueMaybe.cpp` -- then get that header to the `+4`
rung, or re-measure the curve (it is a property of the whole compilation, not of one header, so
it moves).

```cpp
// FUNCTION: LOCO 0x44e020
// The roster's per-tick pump. Gated on the world actually being on screen (g_nScreenState 3 or
// 9), it walks the four slots and, for each occupied one, derail-checks it, ticks it, and then
// takes one of two retirement paths:
//   * dwSoundStateMaybe == 3 -- the train has finished whatever it was doing: put every passenger
//     back on the ground, unhook it from the board, and release the queue slot (the `1` flag
//     tells ReleaseOrForwardMatchingSlotMaybe this is a local retirement, not a peer's);
//   * nDeferredMoveStateMaybe == 1 -- the tick just decided this train belongs to another peer's
//     board: stamp the state to 2 (dispatched) so the decision is not retaken, snapshot the tile
//     it is standing on BEFORE detaching (detaching drops pNameMaybe's tile), unboard the
//     passengers only if the node is also flagged for discard, then detach, clear the slot,
//     decrement the roster and post the move.
// The two are mutually exclusive and the second is the `else` of the first.
//
// ⚠ The two 8-entry passenger loops are FreeQueuedTrainCarSlots' body written out inline TWICE
// rather than called, and the two copies are NOT identical: the sound-state-3 arm walks the
// CACHED `pNode`, the deferred-move arm walks a RE-READ `(*ppNode)`. That asymmetry is the
// original's -- it re-reads the slot between the two arms -- and is reproduced as found.
//
// Takes NO parameter: the original ends in a plain `ret`, not `ret 0x4` (Ghidra infers a spurious
// `param_1`), the same way CollectAndSortVisibleCarsMaybe does.
//
// PARTIAL -- DIFF(222) at 295/311 bytes, insns 97/104, align=74 reg_pen=20. The residual is TWO
// things and nothing else: (1) the documented `sete` prologue class -- the original compiles
// `g_nScreenState == 3 || g_nScreenState == 9` as `xor ecx,ecx; cmp eax,3; sete cl; test cl,cl;
// jne` + the same again for 9, six instructions where this compile emits a plain `je`/`cmp` pair
// (same class as 0x456150 and 0x462e90, docs/PARKED.md; not source-steerable); (2) one
// `mov ebx,eax` the original uses to re-materialize the node pointer into a second register,
// after which every remaining row is a straight EDI<->EBX rename. Take the `sete` block out and
// the two bodies are the same code.
//
// The one source lever that mattered is worth 130715 -> 76348 on its own, and is the OPPOSITE of
// this TU's usual #51 idiom: the deferred-move arm must CACHE its re-read of the slot in a local
// (`pReread`) and walk THAT, not repeat `(*ppNode)->` at each use. The original issues exactly one
// `mov eax,[edi]` at the top of that arm and then a `lea esi,[eax+0x38]` pointer walk; repeating
// the dereference makes cl abandon the pointer walk for a byte-offset index (`mov ecx,[ecx+edi]`
// / `cmp edi,0x58`) and costs the register that the original's spill-and-reload of `this` needs.
// Note the FIRST passenger loop still reads through the cached `pNode` from before the branch --
// the two loops genuinely disagree about which pointer they walk, see above.
void PeerTrainSlotQueueMaybe::DrainPendingSlotsMaybe()
{
    if (nActiveCount != 0 && (g_nScreenState == 3 || g_nScreenState == 9)) {
        PeerTrainNodePartial **ppNode = aSlots;
        int nSlots = 4;
        do {
            if (*ppNode != 0) {
                CheckDerailCollisionMaybe(*ppNode);
                ((PeerTrainNodeTickPartial *)*ppNode)->PeerTrainNode_UpdatePlacementTickMaybe();
                PeerTrainNodePartial *pNode = *ppNode;
                if (pNode->dwSoundStateMaybe == 3) {
                    for (int i = 0; i < 8; i++) {
                        if (pNode->apPassengerMaybe[i] != 0) {
                            ((WalkerActor *)pNode->apPassengerMaybe[i])->LeaveTrainMaybe();
                            pNode->apPassengerMaybe[i] = 0;
                        }
                    }
                    DetachFromBoardMaybe(*ppNode);
                    ReleaseOrForwardMatchingSlotMaybe((*ppNode)->wTrainId, (*ppNode)->bOwnerByteA,
                                                      1);
                } else if (pNode->nDeferredMoveStateMaybe == 1) {
                    TileGridPos pos = pNode->pNameMaybe->pTileMaybe->pos;
                    pNode->nDeferredMoveStateMaybe = 2;
                    PeerTrainNodePartial *pReread = *ppNode;
                    if (pReread->nDiscardFlag != 0) {
                        for (int j = 0; j < 8; j++) {
                            if (pReread->apPassengerMaybe[j] != 0) {
                                ((WalkerActor *)pReread->apPassengerMaybe[j])->LeaveTrainMaybe();
                                pReread->apPassengerMaybe[j] = 0;
                            }
                        }
                    }
                    DetachFromBoardMaybe(*ppNode);
                    *ppNode = 0;
                    nActiveCount--;
                    g_pDPlaySessionMgr->RequestTrainMoveOrReleaseNode(pos.wPosX, pos.wPosY, pNode);
                }
            }
            ppNode++;
            nSlots--;
        } while (nSlots != 0);
    }
}
```

## v493 — the WorldActionCursor / SelectedObjWidgetMaybe vtable-slot run

Three functions landed from `src/WorldActionCursor.cpp`, all three found by DUMPING THE CLASS
VTABLE rather than by reading bodies (see CODEGEN #60): each had zero or one code xref and read
as an anonymous gap, and one dump each identified it as an override of a slot the base already
declares. Zero collateral for the whole run — the full `progress.py` per-file table moved only
this TU, and EXACT never left 147366 B / 639 funcs.

| addr | name | slot | size | result |
|---|---|---|---|---|
| 0x45a500 | `WorldActionCursor::RepositionWithHotspot` | 3 | 564 | PARTIAL, DIFF(440), insns 165/171 |
| 0x45a740 | `WorldActionCursor::TryHandleClickMaybe` | 4 | 313 | PARTIAL, DIFF(141), **insns 113/113** |
| 0x42d440 | `SelectedObjWidgetMaybe::RepositionWithHotspot` | 3 | 548 | PARTIAL, DIFF(277), **insns 186/186** |

### 0x45a740 — EFFECTIVE, pure EBP↔EDI coin flip
113/113 instructions, `align=0`, total 1222. Every one of the 11 differing rows is the same
register pair: the original keeps the click's `x` in EBP and `&animMaybe6` in EDI, the candidate
has them the other way round. The 1-byte length difference is just `[ebp]` needing a disp8 where
`[edi]` does not. Nothing source-level distinguishes these; closable only if the general
register-coin-flip class ever cracks.

### 0x42d440 — EFFECTIVE, two coin-flip classes
186/186 instructions, `align=10`, total 11554, 550 vs 548 bytes. (a) An EAX↔EBP swap in the
`nSpan` computation: both descriptor pointers load in the original's own order, only which width
lands in which register differs. PROBED — `(animW >> 1) + ownW` and a cached `int nOwnWidth` temp
both score an identical 11554, because cl normalizes the commutative add, so the operand order in
the source is not the lever. (b) `add ecx,ebp` vs `lea edx,[ebp+ecx]` for `rcBoard.left + nSpan`,
which is the whole +2 bytes: the original clobbers the reloaded rect field as a dead temp, the
candidate builds a fresh one — the same distinction CODEGEN #58 documents for `add edi,0x83`.

### 0x45a500 — PARTIAL, one 5-instruction block genuinely parked
Six instructions short of the original. Five of them are one block: a live
`cmp word ptr [esi+0x398],6` (`nModeMaybe == 6`) at 0x45a597, with the `jne` at 0x45a5ba
straddling two pushes of the SAME stack slot in different registers (`lea eax,[esp+0x18]` vs
`lea ecx,[esp+0x18]`) ahead of a shared `push 0x478538; call CenterRectInRect`. Since arguments
push right-to-left the branch sits on the SECOND argument — the inner rect — and there is exactly
one call, one rect copy (shared, ahead of the branch) and one reposition after it.

FOUR source shapes measured, none reproduces it:
- plain `CenterModeAnimOverWidgetMaybe()` (no branch) — 166/171, total 167337
- ternary `nModeMaybe == 6 ? &rc : (RECT *)&rc.left` — 165/171, total 159765
- an if/else assigning the same two expressions to a `RECT *prc` — identical 159765
- two full inline expansions of the helper in an if/else — DUPLICATES, +105 bytes

The first three are the same answer: cl 5.0 folds any two expressions naming one
compile-time-known stack address, so no same-slot spelling can survive (CODEGEN #59). The fourth
independently proves cl does not cross-jump here. ⇒ Reproducing this needs a SECOND REAL RECT
that merely shares the slot, and nothing in the body supplies one yet. Left as the plain helper
call (simplest source, same score as the best probe) until that object is identified.

The remaining 1 instruction is the CODEGEN #58 case, now understood but not fully paid: the
original's `add edi,0x83` / `add edi,0x38` into `nX`'s own register vs the candidate's
`lea eax,[edi+0x83]`. Widening the ternary to `b ? nX + 0x83 : nX + 0x38` gets the BRANCH back
(the constants-only spelling went branchless via `neg/sbb/and 0x4b`) but not the clobber.

## v494 — the ScriptEventLoader .obj run (12 EXACT, 3 parked)

The whole 0x41f4e0..0x4202b0 .obj landed in the new `src/ScriptEventLoader.cpp` (2074 B exact,
zero collateral -- the full progress.py per-file table moved only this one row). Twelve of
fifteen functions are EXACT: ctor 0x41f480, teardown trio 0x41f4e0/0x41f540/0x41f590, loaders
0x41f5e0/0x41f6e0/0x41f7e0, parsers 0x41fb20/0x41fbe0, eggs 0x41f8e0/0x41f970, expiry helper
0x4202b0. The three below are parked.

### 0x41fd00 `ScriptEventLoader::TickWorldIdleMaybe` — PARTIAL, DIFF(251), 193/201 insns
The world idle tick (533 B). Every walk, guard, message id, Sleep cadence and the count/12
clamp reproduces; the residual has ONE root cause: the original homes the PostMessageA IAT in
edi and the loop counters in ebx/esi, the candidate homes the IAT in ebx and the counters in
edi, and the swap cascades into the block-3 random-pick loop (counter `i` spills to
[esp+0x10], +4 insns) and the loop-invariant GetItem vptr getting hoisted into the freed esi
(+2 insns, original reloads `mov edx,[0x4a9994]` per iteration). PROBED and inert: one shared
counter variable vs per-loop variables, top-of-function declaration and zero-init of the
counter (the v255 pin lever), a separate countdown variable vs reusing `i`, `if (count != 0)`
vs `if (count > 0)` (the `> 0` spelling DID fix a jbe/je), and the field-each-time spelling of
the tick (much worse — cl re-reads it). The ebx↔edi assignment itself did not move; same
family as 0x45a740's EBP↔EDI coin flip but with a real instruction cascade, so it is NOT
marked EFFECTIVE — closable only if the register-preference class ever cracks. Also present:
the two stack locals' slot order is swapped ([esp+0x10] tick/mod10), cosmetic.

### 0x41ff20 `ScriptEventLoader::ProcessTimedEventsMaybe` — PARTIAL, DIFF(133), 92/88 insns
The [TimeEvents] fire-and-rearm pump. Won back from a full accumulator fold (see CODEGEN #62):
the loop must be the fire-inside-do-while + single `return bFiredMaybe` at a JOIN shape, the
dead period guards must be written as `>= 1`-dominated `== 0` and computed-expression
`2 - period != 0` (CODEGEN #63), and `nPeriod` must be named from the field per use (keeps it
caller-saved, forcing the original's post-`rand()` reloads). Remaining residual: (a) the
loop-exhausted edge jumps to the shared tail (`je` + `jmp`) instead of falling into an inline
duplicated epilogue as the original's does; (b) the case-0 arm's constant reuse — original
materializes the 1 in edx for the `+ 1` add and copies dl to the accumulator, candidate emits
`mov bl,1` + `inc`; (c) ebp↔edi swap of the tick pointer. (a) and (b) were each probed three
ways (early/late/embedded assignments, store-before/after-flag orderings) with zero movement;
parked at byte_diff 56.

### 0x420000 `ScriptEventLoader::ProcessInsertSeqSpawnsMaybe` — PARTIAL, DIFF(251), 195/195 insns
The InsertSeq spawn pump (677 B), the session's biggest single win after the structure landed
(shared `TileGridPos pos` across both call sites, the two-call if/else for the 0x3010-0x301b
arg5 chain, the inverted `!= 0x53`/`!= 0x57` mode nesting, per-arm x/y assignments -- each of
those moved the score by an order of magnitude, 288919 -> 13796). The residual is 36 bytes of
pure register coin-flips (edi↔ebp for pObj, the pPlaced result living in eax vs copied to
edi, scheduling of the mobility-word load) plus one duplicated restore-tail epilogue the
original shares. Nothing source-visible remains on the autopsy table; EFFECTIVE-class.

### 0x455670 `WorldBoardMaybe::GetTopPlaneBSlotMaybe` — EFFECTIVE, DIFF(79), 48/47 insns
The topmost occupied slot at a tile, backward-scanning the cell's slot array. Two structural
levers landed it from 40901 to 16893 and both are load-bearing (see the in-source autopsy): the
scan is a `while`, NOT `if (count >= 0) { do … while }` — the count-negative case must reach the
same `return pResult` as the loop, not the bounds-failure `return 0` — and inside the hit branch
`pResult = pObj` precedes `iSlot = -1`. What is left is ONE register coin-flip: the original
parks `y` in `si` and `x * BOARD_TILE_ROWS` in `edi`, needing a 3-address `lea esi,[edi+edx]`;
this compile parks them the other way round and folds the sum into a 2-byte `add esi,edx`. That
single byte shifts every subsequent instruction, which is the whole of the DIFF. Six source
shapes probed and refuted, including splitting the multiply into its own named local — the
obvious way to keep the product alive for a `lea`, and bit-for-bit inert. Same class as
0x45a740's EBP↔EDI flip; closable only if the register-preference class ever cracks.

### 0x4557c0 `WorldBoardMaybe::GetPlaneASlotGridPosMaybe` — EFFECTIVE, DIFF(38), 45/45 insns
The origin tile of the plane-A object in a given slot, returned by value under the hidden-return-
pointer convention. Every instruction, operand, branch target and both epilogues agree; the
residual is register allocation only — the original parks `y` in `ax` and the cell accumulator in
`esi` and loads `y` only AFTER computing `x * BOARD_TILE_ROWS`, this build parks `y` in `si` and
the accumulator in `edi` and loads `y` first, which also moves the `push edi` two instructions
earlier and swaps the pair in the hidden-return copy. The nested guard IS load-bearing (the outer
`if (pObj != 0)`'s else-arm is the (-1, -1) return, so its `je` targets the failure epilogue
directly) and must not be flattened with `&&`. Four shapes probed and refuted: a named `int nCell`
temp, `pos.wPosY` before `pos.wPosX`, declaring `pObj` ahead of `pos`, initializing `pObj` at its
declaration. The `BoardTileCellMaybe *pCell` lever that made its sibling 0x4556f0 exact does not
apply — the slot index is folded into the same scaled index as the cell, so there is no separate
cell address to name. Same coin-flip class as 0x455670.

### 0x44d800 `PeerTrainSlotQueueMaybe::PeerTrainSlotQueueMaybe` (ctor) — EFFECTIVE, DIFF(18), 16/16 insns
The singleton's constructor: the vtable store plus ResetAllFields' (0x44d870) body verbatim.
reg_pen=0, identity_miss=0 — every instruction, operand and register agrees. Only two are placed
differently, and both are the same rep-stosd setup schedule 0x44d870 is already parked on: the
original interleaves `lea edi,[edx+0x18]` between the +0xc and +0x10 stores and puts
`xor eax,eax` ahead of the `push edi`, this build groups the LEA with the `mov ecx,0x10`. The
address calc depends on nothing in the store run, so an /O2 scheduler places it freely (Yoda
lesson #15's class). Refuted: `memset(…, sizeof(…))` for the 16-entry loop, and hoisting the
`for`'s induction variable — both bit-for-bit inert, the loop is recognized either way. Closable
only with 0x44d870. ⚠ Declaring the ctor on the shared header was MEASURED tree-wide as free,
confirming that header's own note that a constructor declaration does not count toward its
0x457ce0 dial (ordinary member declarations do).

## v530 — src/Ddraw.cpp + src/WidgetBase.cpp

### 0x45b500 `Ddraw::Ddraw_Init` — EFFECTIVE, DIFF(78) of 728 B, 225/225 insns
Also see this function's row in the table above, which the v443–v529 "toolchain-blocked" verdict
was recorded in and which v530 corrected. Structure, length, operands and stack layout all agree;
the residual is ONE register coin-flip and the store scheduling that follows from it. The original
puts the red channel mask in `eax` and the 0x3def/0x7bef guard constant in `ecx`, so its two `mov
word ptr` guard stores schedule BETWEEN the R/G and B mask stores; this build swaps the two
registers and the guard stores float to the front. It is a genuine tie in the cost model: the
original spends an extra byte on `66 89 0d` (cx → moffs) to keep `eax` free for the two 5-byte
`a3` short-form R/B stores, we spend it the other way for the two 6-byte `66 a3` guard stores —
identical total size either way. Five shapes probed, all landing on exactly total=20487: guard
local typed `int` vs `unsigned short`; a named `dwRBitMask` local feeding both the compare and the
store; declaring `wGuardMask` first vs last; `DAT_00485284 = g_wChannelBleedGuardMask` (re-read, to
shorten the local's live range); and all three source orderings of the R/G/B/guard stores. That
last one is the informative negative: the scheduler emits identical code for guard-first,
mask-first and interleaved, so store order is DRIVEN BY the register assignment, not the reverse —
there is no source-side lever left. Four structural levers WERE found and are load-bearing (they
took it 461 → 231 → 78); they are written up above the body in `src/Ddraw.cpp`.

### 0x4545a0 `WidgetBaseObj0x4784c8::~WidgetBaseObj0x4784c8` — NOT LANDED, net-negative
Transcribed and DIFF(7) on the first compile at the correct 144 B, then reverted. The body is not
the problem and needs no further work; its one dependency is. It calls
`DAT_004fd220.EffectSpawner_RemoveHandle`, and bringing `class EffectSpawner` into
`src/WidgetBase.cpp` costs `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe` (0x405c40) its whole
407-byte EXACT — net −263 B. The full bisect (which sharpens the TU-dial rule, see CODEGEN) lives
at the dtor's declaration in `src/WidgetBase.h`, and the full matrix is CODEGEN #98: the DEFINITION is free
and the `#include` pays the entire cost; free-function declarations, a duplicate guard-swallowed
`#include` and the already-present `<stdio.h>`/`<math.h>`/`<time.h>` are all byte-neutral; but a
genuinely NEW file costs the full −407 B **even when that file is completely EMPTY** (`<assert.h>`
does it too), as does any new TYPE declared inline in the `.cpp` (class, POD struct, or even a bare
`typedef`) and any added member declaration on an already-included project header. Two new headers
cost exactly the same as one, so it is a THRESHOLD, not a parity bit a second declaration could pay
back. A TU-local one-method view is therefore NOT an escape either — it is equally a new type.
Retry when 0x405c40's own class cracks, or free of charge the moment that TU needs `class
EffectSpawner`, or any other new header, for anything else.

## v533 — src/WidgetPicker.cpp

### 0x428a80 `WidgetPickerObj0x477cc8::HandleMenuCommandMaybe` — EFFECTIVE, DIFF(6) of 98 B, 38/38 insns
Vtable slot 20. Every instruction, block, branch, constant and epilogue is the original's, in the
original's order — including the fact that there are TWO `xor al,al` epilogues rather than one
(the early `return 0` inside the category-1 arm is load-bearing; an `else if` chain would share a
single trailing `TickAdvanceFrame` call, and the original emits two). `align=0`.

The entire residual is a three-site `eax`↔`edx` coin flip on the vtable-pointer loads. The original
threads EAX through `mov eax,[ecx]` → `call [eax+0x54]` → `mov eax,[esi]` → `call [eax+0x1c]`,
reusing the register across the pair; this build alternates the other way, starting from EDX, so
the same four loads come out `eax/edx`-swapped at three of them. Same class as v531's 0x458310 and
the wider "register coin-flip" family — no size or scheduling difference, just the allocator's
tie-break.

Probe REFUTED: hoisting `nCategory` into a named `short` local to force the single load compiles
BIT-IDENTICAL. cl already loads the member once on its own (the original's `mov ax,[ecx+0x49c]`
feeds both the `== 1` test and the 2/3/4/5 chain), so the local has nothing to give — there is no
source-side lever here, the same negative result v530 reached on 0x45b500's store ordering.

Landed anyway rather than withheld: it cost nothing (per-file table diff clean tree-wide), and
getting there required a real model fix that stands on its own — `HandleTabSwitchMenuNode` and
`HandleSavegameMenuNode` were declared NON-virtual while genuinely being vtable slots 21 and 22.
They are now `virtual`. That matters beyond this function: `call [vtbl+0x54]` carries the slot index
as an instruction DISPLACEMENT, not a relocation, so `verify.py`'s masking cannot hide a wrong slot
number the way it hides a wrong call target.

---

## v536 — 0x424b40 `~CursorDesc` (92 B): TRANSCRIBED AND VERIFIED EXACT, WITHHELD at −2792 B

Body is byte-EXACT (92 B, 0 diffs, first compile) as the `??_G` scalar-deleting dtor with the
dtor body inlined into it. It is not in the tree because it cannot be paid for. The two owning-
pointer `delete`s dispatch through LocoBitmap's vtable (slot 0, arg 1), so `src/CursorDesc.h` needs
LocoBitmap COMPLETE — against the forward declaration cl silently emits a plain `operator delete`
instead and the match is gone. Measured: `#include "LocoBitmap.h"` there is **−1956 B** on its own
(Obj0x4779e0.cpp −1940, NameAnchorMaybe.cpp −456, PeerTrainNode.cpp +440), and the dtor body on top
of it a further **−836 B**. Only ONE of this header's 25 consumers (CreditsWnd.h) already includes
LocoBitmap.h ahead of it, so the usual "a guarded include is byte-neutral by construction" argument
does not apply here.

Deferring BOTH the include and the body to the very bottom of the header (an out-of-line
`inline CursorDesc::~CursorDesc()`) was measured too and is byte-for-byte the SAME −2792 B: the cost
is LocoBitmap.h's declaration CONTENT, not its position. The verified body is recorded in
`src/CursorDesc.h` on the dtor declaration. Unblocked by: anything that makes LocoBitmap.h cheap to
include widely, or a way to spell a virtual-dispatching `delete` against an incomplete type.

## v536 — 0x405620 / 0x41cdf0 `AlbumCardWnd::OnSetFocus` / `EditCardWnd::OnSetFocus` (92/91 B)

Both are the tutorial-modal focus bounce (`PostMessageA(tutorial, WM_SETFOCUS, 0, 0)` +
`SetWindowPos(..., SWP_NOMOVE|SWP_NOSIZE)`, else fall through to the shared DefWindowProcStub).
32/32 instructions, correct length, same operands, same call. The entire residual is the
6-instruction argument setup for the tail stub call: the original rotates just TWO scratch
registers (edx/eax), reloading each parameter from its esp-relative slot as the pushes shift esp;
cl gives a three-register (ecx/edx/eax) load-all-up-front schedule. Byte-identical above the join,
so a pure allocator coin-flip.

Same class as the pre-existing `MailWnd::OnSetFocus` (0x42fe80) row — identical construct, identical
symptom — where routing the result through a named `LRESULT` local and calling `DefWindowProcA`
directly were already probed and refuted. Probed additionally here without effect: an explicit
`else` around the fall-through.

## v536 — 0x405a50 `AnimDescRefObj0x477488::DispatchAnimStateMaybe` (96 B): body verified, WITHHELD at −407 B

insns 39/40, byte_diff 7 at the true 96-byte length. The only residual is that the original saves
`edi` in the prologue unconditionally (and so restores it on the `bValid` early-exit path — that
`pop edi` is the missing instruction), while cl sinks the push past the early exit because `edi`
(the pFrameEntry pointer) is only live inside the range-checked block.

Withheld because merely ADDING the body to `src/WidgetBase.cpp` costs the sibling
`AdvanceAnimFrameMaybe` (0x405c40) its full **407-byte** EXACT — v479's "a new definition moves the
same parity bit a declaration does". MEASURED three ways: as a plain member definition, through a
TU-local view struct, and with the signature change isolated (that last one alone is FREE — 0x405c40
stays MATCH, so the `unsigned int`→`int` retype landed and stands on its own). Compensating parity
currencies tried, all worse: declaring `EnsureSoundPlayingMaybe` on the class (and retiring its
view) buys back the 407 but costs WorldBoardMaybe's 0x457ce0 −951 and RoadVehicleActor −504;
spending `src/Obj0x477798Family.h`'s `_v15` slot costs −504 and leaves the −951.

One real source lever WAS found and is recorded with the body: caching `pFrameEntry->nStartFrame` in
a local rather than reading the member twice (the original loads it once and feeds both the
nAnimValueCache store and the SetAnimFrame push from that register) — DIFF 92 → 88, 99 B → 95 B.
Verified body is in `src/WidgetBase.h` on the DispatchAnimStateMaybe declaration.

## v537 — 0x4344a0 `DecorActorBase::SetCategoryIfPrintable` (82 B): EXACT on the first compile, WITHHELD at −1524 B

Vtable SLOT 13 (+0x34), this class's override of `AnimDescRefObj0x477488::SetCategoryIfPrintable`
(0x405e20). Three statements — chain the base, force the manager's category-7-only tick when this
actor's kind descriptor is category 7, and arm the population throttle when the new name is the
literal `"PARTY"`. It is the out-of-line copy of the block this class's own ctor (0x433a20) already
inlines. Verified body is in `src/DecorActor.h` on the ⛔ note; `MATCH len=82` first compile, and
the 407-byte ctor beside it kept its own EXACT.

Withheld because the DECLARATION it needs on `DecorActorBase` costs, bisected to the declaration
alone: `WorldBoardMaybe::FindNearestObjOfCategoryMaybe` (0x457ce0) −951, `WorldBoardMaybe_
ResetAllTilesMaybe` −211, `src/RoadVehicleActor.cpp` −504, against +82 here and +60 in
`src/TilePlacedObj.cpp`. Net −1524.

Reading it also settled that `src/WidgetBase.h` models ONE function as TWO (a declared-only
`_v13()` placeholder for slot 13 plus a separate non-virtual `SetCategoryIfPrintable` for
0x405e20) — CLAUDE.md's "vtable slot and named method at the same address" hazard, second sighting.
UNIFYING them is separately net-negative at −1005 B, and three compensating currencies were
measured and refuted: `src/Obj0x477798Family.h`'s `_v15` (restores its own TU +152, costs
RoadVehicleActor −504, leaves the −951), `EnsureSoundPlayingMaybe` (−504/−152/−407, still no
0x457ce0), and the class's genuinely-missing slot 15 `SpawnOwnedActorMaybe`/0x4062a0 (needs a
`class DecorActorBase;` forward declaration in `src/WidgetBase.h`, itself a −6174 B lever across
nine TUs; a `void *` return is not an escape — it makes TilePlacedObj's override a return-type
mismatch, an error rather than a covariance). Do not re-run any of the three.

## v537 — 0x454630 `WidgetBaseObj0x4784c8::ClearOwned` (75 B): body read, WITHHELD on the shared EffectSpawner toll

Three statements (delete pMenuListHead, release pEffectSpawner through
`DAT_004fd220.EffectSpawner_RemoveHandle`, then `SetDescriptor(0,-1,0)`); verified body recorded on
the declaration in `src/WidgetBase.h`. Blocked by the SAME `EffectSpawner.h`-in-`src/WidgetBase.cpp`
dependency that already blocks the 0x4545a0 destructor — a second member of that family, not a new
problem. The toll is one-time and shared: −407 B (0x405c40's exact) against at most +75 B of new
EXACT here, plus 0x4545a0 (144 B, DIFF(7)) and 0x405a50 (96 B, byte_diff 7) arriving as PARTIAL.
Still net-negative, so all three stay withheld together and land as one batch if 0x405c40's class
ever cracks.

## v540 — `RectFlagObj0x477820`'s four remaining bodies (151 B): ALL EXACT on the first compile, WITHHELD at −256 B

> ### ✅ SUPERSEDED v560 — THREE OF THE FOUR ARE LANDED, AND THE TOLL DID NOT EXIST ANY MORE.
>
> The ctor `0x4369d0` (46 B), `MarkDirty` `0x436ab0` (41 B) and `TryInvokeCallbackA` `0x436ae0`
> (32 B) are in `src/WidgetBase.cpp` and all three are EXACT. Measured repo-wide on landing:
> **EXACT 173119 → 173238 B, +119 B / +3 funcs**, and the per-file table moved in exactly one
> place. `AdvanceAnimFrameMaybe` (0x405c40) had ALREADY lost its EXACT to something else in the
> ~20 sessions since v540, so the 407 B toll this whole section was withheld against no longer
> existed to pay. Everything below was correct when measured; what expired was the assumption
> that the victim was still alive. See CODEGEN #175.
>
> `TryInvokeCallbackB` (`0x436b00`, 32 B) is still out, for an unrelated reason: its root
> declaration is the wrong `void()`, and correcting it entangles `WidgetBaseObj0x4784c8`'s real
> slot-5 override at 0x454a60 (currently modeled as the ordinary member `HitTestAndLocalizeMaybe`).
> That is a model edit, not a pricing one. Its body is byte-for-byte `TryInvokeCallbackA`'s
> against +0x20 instead of +0x1c.
>
> One transcription detail worth keeping: `TryInvokeCallbackA` needs its guard spelled `!= 0` with
> the CALL as the fallthrough arm and `return 0` last — the original's `test eax,eax / je` jumps
> AWAY to `xor al,al`. The inverted spelling (early `return 0`) costs 23 bytes.


`0x4369d0` ctor (46 B), `0x436ab0` MarkDirty (41 B), `0x436ae0` TryInvokeCallbackA (32 B) and
`0x436b00` TryInvokeCallbackB (32 B) — the last four members of the widget root base, whose other
four bodies already live in `src/WidgetBase.cpp`. Every one of them byte-matched on the FIRST
compile. They are withheld because landing ANY of them costs
`AnimDescRefObj0x477488::AdvanceAnimFrameMaybe` (0x405c40) its whole 407 B EXACT.

⭐ **This joins the existing 0x405c40 toll-gate family.** The v536 (0x405a50, 96 B), v537
(0x454630, 75 B) and 0x4545a0 (144 B) rows are all withheld against the SAME one-time −407 B.

> ### ⚠ CORRECTED v542 — the "NET POSITIVE (+59 B)" claim below was an ARITHMETIC ERROR. Do not act on it.
>
> This section originally read: *"Adding these four makes the batch worth **466 B of new EXACT
> content** against a single 407 B toll — i.e. the bundle is now NET POSITIVE (+59 B)"*, and the
> v541 pickup promoted that to its headline recommendation. **It is wrong.** 466 is
> 144+96+75+151 — the sum of all four rows' FUNCTION SIZES, counted as though each would land
> EXACT. Two of them do not, as this very file's v537 section states: **0x4545a0 lands DIFF(7)
> and 0x405a50 lands byte_diff 7 — both PARTIAL.** The real arithmetic is:
>
> | member | size | lands as |
> |---|---|---|
> | 0x454630 `ClearOwned` | 75 B | EXACT |
> | 0x4369d0 / 0x436ab0 / 0x436ae0 / 0x436b00 | 151 B | EXACT |
> | 0x4545a0 `~WidgetBaseObj0x4784c8` | 144 B | **PARTIAL** (DIFF 7) |
> | 0x405a50 `DispatchAnimStateMaybe` | 96 B | **PARTIAL** (byte_diff 7) |
>
> New EXACT content = **226 B**, not 466. Against the −407 B toll the bundle is **−181 B**, and
> every member remains individually net-negative as before. ⇒ **The bundle stays withheld.** It
> did NOT flip sign by growing; it never flipped. Re-price it only if the toll itself moves or
> if 0x405c40's residual class cracks.
>
> Process lesson (v542): a carried-forward headline number that flipped sign is exactly the kind
> that should be re-derived before a session is spent on it. Doing the sum took two minutes.

⇒ If 0x405c40's residual class ever cracks, land all of them together.

Measurements (v540, each one compile apart): the ctor ALONE breaks 0x405c40; MarkDirty ALONE
breaks it; all four together break it identically; and the header-only model fix (the callback
typedef + TryInvokeCallbackB's signature correction) breaks it with NO body added at all. A spare
declaration added to `src/WidgetBase.h` changes nothing — the DIFF is byte-identical with and
without it — so unlike v486's `RepositionWithHotspot` there is no parity currency to spend here.
0x405c40's own residual is content-complete and intrinsic: 144/144 instructions, the entire diff
one eax↔edx base/index coin-flip on `pKindDesc->paFrameEntries + nSubFrame` propagating through
four later loads. Two source-shape probes (`&paFrameEntries[nSubFrame]`; caching `pKindDesc` in a
local) give a BYTE-IDENTICAL DIFF(314). Full writeup, plus the two real model findings the work
produced, is on the class in `src/WidgetBase.h`.

## v540 — 0x423d20 `EffectSpawner::EffectSpawner_RemoveHandle` (68 B) — EFFECTIVE, one peeled loop test

LANDED as PARTIAL in `src/EffectSpawner.cpp` (74 B compiled, insns 37/35, DIFF 42). Every call,
operand, exit and epilogue is the original's; the whole excess is ONE peeled copy of the
`GetEntryMaybe(i) == pHandle` compare that VC5 emits when it rotates this search loop, plus the
`pHandle` parameter load sliding from before the loop to inside it. The original enters its loop
at the top with the slot count tested once ahead of it, keeps a separate loop-exhausted epilogue
at 0x423d50, and lets the found-arm fall into the shared one at 0x423d5e.

Three source shapes tried, all one compile apart, all landing on the same peel: (1) `while
(GetEntry(i) != pHandle) { i++; if (GetSlotCount() <= i) return; }`; (2) the `do { … } while (i <
GetSlotCount())` form kept in the source, which reads closest to the original's control flow;
(3) the compound bottom-test `if (++i >= GetSlotCount())` guard that DOES suppress this exact peel
on 0x44cb10's forward re-walk in `src/PeerTrainNode.cpp`. It does not suppress it here. Same
family as that function's own parked residual.

## v542 — 0x4243c0 `EffectCandidateRegistryMaybe::CompareEntriesMaybe` (152 B): EXACT lost to a header-content twin flip

| addr | name | size | residual | why parked |
|---|---|---|---|---|
| 0x4243c0 | `EffectCandidateRegistryMaybe::CompareEntriesMaybe` (src/Obj0x477798Family.cpp) | true COMDAT **152 B** (134 B of code + the 4-entry jump table; the unclaimed-gap list's 134 is the CODE extent and truncates the window) | compiled **156 B**, DIFF(80) | Was EXACT from v525 until v542. Its own source did not change. v542 added `AnimEffectObj0x477a90`'s three slot-7/9/10 override DECLARATIONS to `src/AnimEffectObj.h`, which this TU includes, and that alone flipped this site from the 152-byte pObj-before-pOther twin onto the 156-byte reversed twin. Accepted deliberately in exchange for the three overrides (101 B, all EXACT on the first compile) plus the vtable correction they carry — see the ⭐ note below. Net **−51 B**. |

⭐ **This sharpens the v457 twin-selection rule in two ways, and both are new.**

1. **Twin selection is NOT a fixed property of the TU.** v457 concluded "which twin a given text
   produces is decided by the TU, not by the source", and every later note treated that as a
   constant of the .obj. It is not — it is perturbable by the CONTENT of an unrelated INCLUDED
   HEADER. Three method declarations on a class this comparator never mentions moved it.
2. **The two twin sites in ONE TU are decided INDEPENDENTLY.** `src/Obj0x477798Family.cpp` now
   emits the 156-byte form at 0x4243c0 and the 152-byte form at 0x4361e0 — i.e. each site holds
   exactly the twin the OTHER one wants, an exact inversion. If a lever were ever found that
   swaps the two, BOTH would match and the pair would be worth **+156 B** over the v541 baseline.
   That is now the highest-value known open question in this file.

Levers probed and REFUTED (v542, each one compile apart):
- A spare non-virtual declaration added to `AnimEffectObj0x477a90` — the DIFF is byte-identical
  with and without it. So this is a **THRESHOLD, not a parity bit**; a second declaration cannot
  buy it back. Same shape as v540's finding on `src/WidgetBase.h`.
- A dummy `static` definition + file-scope initializer inserted immediately AHEAD of 0x4361e0 —
  no effect on either site. So the form is not decided by locally-preceding definition state.
- Landing a SUBSET of the three overrides is strictly worse, not better: the threshold fires on
  any one of them, so one override costs the same 152 B as all three (+37 vs +101).

NOT chased further, on the v525 reasoning which still stands: the only source edits known to flip
the load order are temps spelling out "read pOther first", and writing the two copies of what is
genuinely ONE original source differently would encode a compiler artifact as a source difference.

**Why the trade was taken.** The three overrides are EARNED matches — correct source, correct
signatures, EXACT on the first compile — and they fix real wrong linked content: before v542 the
emitted `??_7AnimEffectObj0x477a90@@6B@` had slots 7/9/10 pointing at the BASE implementations
(0x405a20/0x4061b0/0x405c40) where the image's vtable 0x477a90 holds 0x423840/0x423890/0x423870.
The COMDAT now agrees with the image at **15/15** slots, up from 12/15. 0x4243c0's own EXACT, by
contrast, was never earned by source correctness — v457 established it was a TU coin-flip — so the
trade is unearned luck for earned correctness plus three functions off the unclaimed list.

---

## v543 — `tools/vtable_audit.py`: the CODEGEN #129 sweep, and the worklist it produced

The v542 pickup's #1 item was "run the CODEGEN #129 vtable audit across other classes". It is now
a tool rather than a manual `llvm-objdump` read: **`tools/vtable_audit.py`** parses every
`??_7Class@@6B@` COMDAT out of `build/*.obj`, resolves each slot's relocation target to an image
VA through the `// FUNCTION:` marker set, locates the class's real vtable by scanning `.rdata`
for the dword array that best agrees with the resolved slots, and diffs. **94 classes; 52
vtables fully agree; 3 hard MISMATCH; 39 not located** (almost all 1–2-slot dtor-only tables and
TU-local view structs, which have nothing to anchor a scan on).

Three reports, in descending confidence:

**A. MISMATCH — we emit a KNOWN address where the image holds a DIFFERENT known address.**
- `WorldActionCursor` slot 2 (vt 0x478548): image **0x459d60**, we emit 0x436a10
  (`RectFlagObj0x477820::Contains`). 0x459d60 is `ContainsIncludingIconMaybe`, already
  transcribed and EXACT — it IS the slot-2 override, modelled non-virtual. ⚠ Its own header note
  ("called class-qualified so this leaf's own slot-2 override is deliberately bypassed") is
  **wrong about which function the override is**: the qualified call is this function avoiding
  recursion into ITSELF. Blocked on a return-type conflict — the base declares
  `virtual char Contains(int,int)` and 0x459d60 is documented (v446) to need `int`, and C++ will
  not let an override differ only in return type. One of the two models is wrong; resolving that
  is the real work.
- `AlbumCardWnd` slot 32 (vt 0x4773f0): image **0x419a10**, we emit 0x426a90
  (`WindowBase::OnClose`). 0x419a10 is transcribed in `src/EditCardWnd.cpp`. Likely one ICF-folded
  body installed in two classes' tables, so only one marker can ever claim it.
- `CreditsWnd` slots 21/22 — see C.

**B. MISSING OVERRIDE DECLARATIONS — ~40 of them**, found by comparing the leaf's image slot
against the image slot of the class we inherit that slot FROM (comparing against "is the address
claimed?" alone does NOT work: an untranscribed base body is unclaimed too). Largest, all already
transcribed under descriptive non-virtual names: 0x44a250 (1746 B, `BuildToolButton` slot 17),
0x459da0 (1010 B, `WorldActionCursor` slot 10), 0x4379c0 (813 B, `BuildToolCursorWnd` slot 7),
0x45a880 (451 B, `WorldActionCursor` slot 17), 0x4287b0 (437 B, `WidgetPickerObj0x477cc8`
slot 17), 0x438940 (389 B, `BuildToolCursorWnd` slot 11), 0x44a0c0 (387 B, `BuildToolButton`
slot 4), 0x45a740 (313 B, `WorldActionCursor` slot 4), 0x42d770 (302 B,
`SelectedObjWidgetMaybe` slot 20). Several are DELIBERATE non-virtual models with the reasoning
recorded in-header (e.g. `src/WorldActionCursor.h:332` for 0x45a740: modelled as an ordinary
member because its only call site is a DIRECT call). Those notes are still correct about the
call site and still leave the vtable wrong — that tension is the finding, not a mistake to
silently "fix".

**C. `PopupWndBase` IS MODELLED ~15 SLOTS SHORT — a real structural gap, fully derivable.**
Our `PopupWndBase` emits 21 slots; its image vtable (0x477898) runs to at least 36. Slots 0–20
align correctly; past that our model just stops, and `CreditsWnd`'s `OnClose`/`OnKillFocus` get
mis-slotted into 21/22. The image's `PopupWndBase` table is structurally PARALLEL to
`WindowBase`'s (0x477c30), shifted by exactly one slot — WB OnMouseMove 20 ↔ PB 19, OnMouseActivate
23 ↔ 22, OnSize 26 ↔ 25, OnPaint 27 ↔ 26, OnSetCursor 28 ↔ 27, OnEraseBkgnd 30 ↔ 29, OnDestroy
31 ↔ 30, OnClose 32 ↔ 31 — so the two are siblings over a shared message-handler block, NOT one
deriving from the other (our `class PopupWndBase {` standalone declaration is right about that
much). Image `PopupWndBase` slots 21–35: 21 stub, 22 `WindowBase::OnMouseActivate` (0x426950),
23–24 stub, 25 `PopupWndBase::OnSize` (0x414ac0), 26 `PopupWndBase::OnPaint` (0x414ae0),
27 `WindowBase::OnSetCursor` (0x426a60), 28 stub, 29 `WindowBase::OnEraseBkgnd` (0x426ac0),
30 `WindowBase::OnDestroy` (0x426ad0), 31 `PopupWndBase::OnClose` (0x414b80), 32–35 stub
(`WindowBase_DefWindowProcStub` 0x422ea0 throughout). `CreditsWnd` overrides slot 24
(`OnKillFocus`, 0x40f820) and slot 31 (`OnClose`, 0x40f760).

**D. PURE VIRTUALS — see CODEGEN #130.** `WidgetBaseObj0x4784c8` slots 17–20 are `__purecall`
(0x467e90) in the image; we declare four ordinary virtuals. PROBED AND REVERTED: adding `= 0`
alone costs **12 COMPILE FAILED TUs** until all five leaves declare the full set (leaf slot
17/18/19/20 addresses: `BuildToolButton` 0x44a250/0x44ef00/0x44ef00/0x44ac20;
`SelectedObjWidgetMaybe` 0x42d6b0/0x44ef00/0x42d760/0x42d770; `WidgetPickerObj0x477cc8`
0x4287b0/0x44ef00/0x4289a0/0x428a80; `WidgetTagObj0x478378` 0x44ed80/0x44ef00/0x44ef10/0x44ef70;
`WorldActionCursor` 0x45a880/0x44ef00/0x42d760/0x45aa50). Slot 18 is 0x44ef00 (`xor al,al;
ret 0xc`, 5 B) for ALL FIVE and slot 19 is 0x42d760 (`mov al,1; ret 0xc`, 5 B) for two — ICF-folded
identical stubs, so most of these bodies can never carry their own marker. ~20 slot decisions
plus ~8 tiny new bodies, on a header included nearly everywhere: a session of its own, not a
drive-by.

## v544 — `PopupWndBase` extended to its full 37-slot vtable; 0x438890's signature corrected

Worked the v543 pickup's item-2 headline (`PopupWndBase` modelled ~15 slots short). Landed, plus
the derived-class fallout it exposed. **Net −38 B**, and that number is the whole story of the
session's cost — see the parity note below.

**What was wrong.** `class PopupWndBase` emitted 21 vtable slots; the image's table (0x477898)
runs 0..36. Slots 0–20 agreed, which is why `tools/vtable_audit.py` had been scoring the class
`OK` — it can only compare the slots we emit. The damage was one level down: `CreditsWnd`'s
`OnClose`/`OnKillFocus` and `BuildToolCursorWnd`'s `OnClose`, having no base slot to override,
were appended as NEW virtuals and landed at slots 21/22 instead of 31/24. See CODEGEN #132.

**Ground truth used** (all re-derived this session from `loco/Loco.exe` directly, not taken from
v543's write-up): the table ends at slot 36 — a NULL dword sits at 0x47792c, the same shape
`WindowBase` (0x477c30) and `CreditsWnd` (0x477680) show, and a NULL cannot be a slot. The
`PopupWndBase` and `WindowBase` tables are structurally PARALLEL with a one-slot shift and hold
the SAME address at every shared slot, which pins ~15 slot identities without reading a body.
The slot NAMES were already correct in `PopupWndBaseVtblProbe`, ground-truthed long ago from
`PopupWndBase_RouteMessage`'s own switch; the class simply adopted them.

**Also fixed: 0x438890 was modelled with the wrong SIGNATURE, not just the wrong name.** It was
`void BuildToolCursorWnd::NotifyToolModeChanged()` — a name invented from behaviour, with no
call sites to contradict it. The audit put it in slot 24, and the image ends it
`xor eax,eax; pop esi; ret 0x10`: four stack arguments and an LRESULT return, which a `void ()`
signature cannot emit. It is `BuildToolCursorWnd::OnKillFocus`. Renamed and retyped in `src/`
and in Ghidra (`save_program` OK). Byte-neutral repo-wide, but the previous model could not
produce the original's epilogue at all. See CODEGEN #133.

Five more genuine overrides recovered as declared-only, all byte-neutral: `TutorialWnd` slot 31
`OnClose` (0x40f760, ICF twin of `CreditsWnd`'s) and slot 20 `OnKeyDown` (0x426950);
`BuildToolCursorWnd` slot 36 `OnWindowPosChanging` (0x426ac0) and slot 15 `OnRButtonDown`
(0x451520); `CreditsWnd` slot 15 `OnRButtonDown` (same folded body).

**Audit delta: 52 → 53 vtables fully agree, 3 → 2 MISMATCH.** The two left are the pre-existing
blocked pair — `WorldActionCursor` slot 2 (return-type conflict) and `AlbumCardWnd` slot 32.

### The −38 B, and two levers PRICED AND WITHHELD (do not re-run without a new idea)

The extension is net +9 declarations on a header three TUs include (one transitively, via
`CreditsWnd.h` → `src/WorldBoardMaybe.cpp`). It moved exactly two functions, simultaneously and
in OPPOSITE directions: `src/TutorialWnd.cpp` 0x452b00 `RestorePresenterBackdrop` MATCH →
DIFF(30) (−249 B) and `src/WorldBoardMaybe.cpp` 0x454fe0 `WorldBoardMaybe_ResetAllTilesMaybe`
DIFF → MATCH (+211 B). Nothing else in the repo moved; zero `COMPILE FAILED`.

- **Paying the debt back in `src/TutorialWnd.h` — REFUTED.** That is the one header
  `TutorialWnd.cpp` sees and `WorldBoardMaybe.cpp` does not, so a declaration there should have
  flipped one side only. One added declaration is INERT: byte-identical measurement. CLAUDE.md's
  "pay by ADDING a real declaration" is a per-header weak prior, not a steerable mechanism.
- **0x452b00's own documented RECT-declaration-order lever — REFUTED IN THIS CONFIGURATION.**
  Swapping to `RECT rectWindow; RECT rectDest;` scores WORSE here (byte_diff 26 → 28,
  align 28 → 24 but reg_pen/identity_miss 5 → 9). Residual is 83/83 instructions — a pure
  scheduling/register tie-break (the original computes into `ecx` and interleaves the
  `g_worldBoard.bSurfaceLockGuard` read between the `add` and the `sub`; we compute into `eax`
  and sink the spill). 0x452b00 is this repo's known shared victim of header-declaration
  changes: five earlier levers each knocked it MATCH → DIFF(32) independently, and v449's
  repo-wide attempt to chase the class lost 1008 B. Left as EFFECTIVE.
- **Deleting the now-redundant `PopupWndBaseVtblProbe` — MEASURED AND REVERTED, costs 211 B.**
  Every slot it declares is now a real virtual on the class, so as a TYPE it is pure redundancy,
  and retargeting `PopupWndBase_RouteMessage`'s 17 dispatches from `pProbe->` to `pWnd->` is
  byte-IDENTICAL for its own TU. But it takes 0x454fe0's 211-byte exact straight back out again.
  Retained, with the measurement recorded at both the class and the probe. `src/WindowBase.h`
  keeps its own probe on exactly this footing despite declaring all 37 slots itself. CODEGEN #134.

### Still open in this family

- **`BuildToolCursorWnd` slot 7 (0x4379c0, 813 B) and slot 11 (0x438940, 389 B)** remain on the
  missing-override list — both are large transcribed bodies under descriptive non-virtual names,
  so promoting them is a real edit, not a declaration. Unchanged from v543.
- The v543 pure-virtual item (`WidgetBaseObj0x4784c8` slots 17–20, CODEGEN #130) is untouched and
  still costs 12 COMPILE FAILED TUs if started piecemeal.

## v545 — vtable-model corrections off `tools/vtable_audit.py` report B

Four rows CLEARED, three of them free and one worth +249 B; one family re-refuted.

### Landed (rows deleted from the audit)

- **0x4379c0 `BuildToolCursorWnd::OnDrawContent` (slot 7)** — was `void DrawAllIconSlots()`, a
  behaviour-derived name with ZERO arguments against the image's `ret 0x4`. Renamed and given the
  `PAINTSTRUCT *`; epilogue `ret` → `ret 0x4` (+2 B on the compiled length, 801 → 803). Its
  residual is unchanged (documented reg-alloc/scheduling class, DIFF 647 → 648). Repo byte-neutral.
- **0x438940 `BuildToolCursorWnd::OnTimerDefault` (slot 11)** — arity was already right
  (`ret 0x10`), only `virtual` was missing. Pure modelling, repo byte-neutral.
- **`AlbumCardWnd` slots 0x2c (0x40b4c0) and 0x80 (0x419a10)** — declared-only; bodies stay in
  `src/ApplSetupWnd.cpp` / `src/EditCardWnd.cpp` as the surviving ICF COMDATs. Fixed the audit's
  one remaining hard MISMATCH and paid **+249 B** (returned `src/TutorialWnd.cpp`'s 0x452b00,
  the exact v544 spent). CODEGEN #136.
- **0x4323c0 slot 0x40 on `ApplSetupWnd` / `NetSetupWnd` / `SplashWnd`** — declared-only, body in
  `src/MapWnd.cpp`. Byte-neutral. `src/NetSetupWnd.h` had withheld the line on the mistaken
  grounds that the address "is NOT a NetSetupWnd member".
- **0x459da0 `WorldActionCursor::AdvanceAnimFrameMaybe` (slot 10, 1010 B)** — was the non-virtual
  `TickAndTutorialCheckMaybe`. Byte-neutral because its one call site dispatches on the concrete
  global `g_worldActionCursor`, which the standard dispatches statically; caller 0x45c3c0 still
  EXACT. CODEGEN #137.

### Re-refuted, do NOT re-run

- **Retyping `WidgetBaseObj0x4784c8`'s slot-19 placeholder + deleting the three TU-local structs
  it spawned: −1862 B.** v506 priced the retype alone at −1094 B; bundling the deletions makes it
  ~768 B worse, not better. Victims: 0x457ce0 −951, 0x405c40 −407, `RoadVehicleActor` −504,
  `PlacedObjRegistryMaybe` −152, `Obj0x477798Family` +152. Idiom debt would drop 14 → 10. The
  slot's true signature (`char(node, int, int)`) is now recorded at the declaration for free.
  CODEGEN #138.

### Still open in this family

- The `RectFlagObj0x477820` slot 4/5 rows (`TryInvokeCallbackA`/`B`, 8 audit rows) are blocked on
  the same header model fix the v540 block prices — changing `TryInvokeCallbackB`'s signature is
  part of the withheld edit that costs 0x405c40 its 407 B.
- The `AnimDescRefObj0x477488` slot 11/13 rows (0x4343b0 ×7, 0x4344a0 ×3) are the v539 −1345 B
  dial; the bodies live on TU-local views in `src/DecorActor.cpp`.
- The slot-17 rows (0x44a250, 0x45a880, 0x4287b0, 0x42d6b0, 0x44a0c0) are deliberate non-virtual
  models with reasoning recorded in-header — each is a real edit, and several have signature
  mismatches against the base that C++ cannot express as an override.
- The v543 pure-virtual item (`WidgetBaseObj0x4784c8` slots 17–20, CODEGEN #130) is untouched and
  still costs 12 COMPILE FAILED TUs if started piecemeal.

## v546 — the `??_G` destructor seam

| Addr | Class | Status | Note |
|---|---|---|---|
| 0x4428c0 | `??_GCarNetState` | PARTIAL, DIFF(22) at 30/32 B | Intrinsic. The inline-dtor fix makes it exact but is **−177 B** repo-wide; the image proves the out-of-line dtor is the right model (0x442a00 has six callers + three EH unwind funclets). PRICED AND WITHHELD — full autopsy at the marker in `src/CarNetState.cpp`. CODEGEN #140. |
| 0x412640 | `??_GTimeOfDayMaybe` | PARTIAL, DIFF at 30/32 B | Same intrinsic residual, same reason: 0x412660 has ten out-of-line callers. Claiming it retired a fabricated `Method0` placeholder virtual — CODEGEN #141. |
| 0x401650 | `??_GPostBagFileCache` | UNCLAIMED, **re-priced** | See below. |

### 0x401650 — re-priced v546, still negative but by 54 B, not 213

v444/v449 rejected the in-class `virtual ~PostBagFileCache() { SaveIndexFile(); }` at +36 B here
against a fixed −249 B on `src/TutorialWnd.cpp`'s 0x452b00. Re-measured this session because v545
recovered 0x452b00 through an unrelated lever, which proves the parity it answers to has moved:

| | v449 | v546 |
|---|---|---|
| 0x401650 itself | +36 B | +36 B |
| `CarNetState::AddDecal` (0x442c90) | — | **+159 B** (new; PostBag.h is upstream via CarNetState.h) |
| `src/TutorialWnd.cpp` 0x452b00 | −249 B | −249 B |
| **net** | **−213 B** | **−54 B** |

Two spellings measured and byte-identical (in-class body; declaration plus an out-of-class
`inline` definition) — both give exactly 171839 B, so the dtor's spelling is not the variable.
**One more parity move that recovers 0x452b00 makes this lever net +195 B.** That, not another
dtor spelling, is what to hunt. `src/WorldBoardMaybe.h`'s undeclared `virtual ~WorldBoardMaybe()`
(vtable 0x478520 slot 0 = `??_G` 0x454db0, dtor 0x454dd0, one xref from the static-dtor thunk
0x45c765) is a real candidate, but `WorldBoardPartial` still models its vptr as `pad0x0[4]` — the
same shape as the carried UIResources item, so it is its own piece of work.

### Confirmed blocked, with the reason now recorded

The four `Obj0x477xxx` `??_G`-shaped thunks (0x424a70, 0x424ad0, 0x436360, 0x4363c0) and
`PeerTrainNodePartial`'s 0x44c0b0 are NOT claimable as compiler-generated: those classes store
their vtable through a manual `vtbl`/`pVtbl` FIELD rather than a real vptr, so cl emits no `??_G`
for them at all. Claiming them requires promoting each class to a genuine polymorphic model first.

## v549 — the `UIResources` ctor/dtor cluster: all three EXACT, all three withheld

The v548 pickup's top unblocked target (0x445f70 ctor 74 B, 0x445fc0 `??_G` 30 B, 0x445fe0 `??1`
80 B = 184 B). **All three matched on the first compile** once the model was made honest, and all
three are withheld: the declarations they need cost more than they buy, at every rung swept.

### The transcriptions — correct, verified, recorded so a later session only re-types them

`src/UIResources.h`: delete `unsigned char pad0x0[4]` (the compiler-inserted vptr occupies exactly
those 4 bytes, so no field offset changes) and add four declarations —

```cpp
    UIResources();
    virtual ~UIResources();
    unsigned char Shutdown();            // 0x446340, moved off UIResourcesTuLocalView
    void ReleaseAllCachedResources();    // 0x4467e0, moved off UIResourcesTuLocalView
```

`src/DSoundChannel.h`, on `struct RFIndex`, add `~RFIndex();  // 0x45ca20`. Required: without it
RFIndex is trivially destructible and `~UIResources` emits neither the member-destructor call on
`this+0x18` nor the `/GX` frame the original's SEH prologue is.

`src/UIResources.cpp`:

```cpp
// FUNCTION: LOCO 0x445f70
UIResources::UIResources() {
    int i;

    memset(m_apKindDescriptors, 0, sizeof(m_apKindDescriptors));
    for (i = 0; i < 0x4000; i++)
        m_pKindSlotPtrsMaybe[i] = &m_apKindDescriptors[i];
    m_hFont12 = m_hFont14 = m_hFont16 = m_hFont24 = NULL;
}

// FUNCTION: LOCO 0x445fe0
UIResources::~UIResources() {
    Shutdown();
}

// FUNCTION: LOCO 0x445fc0 (??_GUIResources scalar deleting dtor -- compiler-generated)
```

Two source shapes are load-bearing and neither is guessable from the decompile (both generalised
into CODEGEN #147): the clear must be a `memset`, not a `for` loop (the loop form rotates the
`rep stos` setup by 12 bytes), and the font clears must be ONE CHAINED assignment, not four
statements (chaining associates right-to-left, so the stores come out descending +0x10, +0xc,
+0x8, +0x4 — which is the original's order). Only four of the five fonts are cleared; `m_hFont20`
is the game's own omission, harmless because `Init` (0x446050) assigns all five.

The seed loop stops at `0x4000`, one short of the clear's `0x4001` — the off-by-one tail slot both
arrays carry is left NULL, never self-seeded. `UIResources`' vtable is 0x478270, 5 slots; slot 0 is
the `??_G`, slots 1–4 (0x447b60 / 0x4489d0 / 0x448c90 / 0x448fe0) are unread and deliberately left
undeclared — an undeclared slot costs nothing because the emitted vftable is DATA, which
`verify.py` never compares.

### The price, measured three ways from a clean baseline

7-TU probe set (`UIResources`, `LocoBitmap`, `Obj0x4779e0`, `ThumbnailBmp`, `TilePlacedObj`,
`TutorialWnd`, `WidgetBase` — the TUs a full `progress.py` showed moving):

| configuration | in-TU | net |
|---|---|---|
| `UIResources();` alone (1 decl; ctor does NOT match — no vptr without the virtual dtor) | +0 | **+20** |
| full cluster, `src/UIResources.h` only (4 decls; dtor stays PARTIAL) | +104 | **−592** |
| full cluster + `~RFIndex()` (confirmed by full `progress.py`: −1524, 172014 → 170490) | +184 | **−1540** |

Collateral of the full change, from the full per-file table: `Obj0x4779e0` −489, `ThumbnailBmp`
−484, `WidgetBase` −407, `TutorialWnd` −249 (the familiar 0x452b00 toll), `TilePlacedObj` −203,
`LocoBitmap` **+124**.

### ⛔ The `src/DSoundChannel.h` dial is SWEPT — do not re-run it

`~RFIndex()` alone is −1581 of the −1540, i.e. essentially the whole toll; the four
`src/UIResources.h` declarations are only about −143. A plain `void ProbeDecl();` on the same
struct is −876, so the destructor costs roughly 2× an ordinary declaration (CODEGEN #148). Sweep
over (destructor × N dummy declarations), same probe set:

| N dummies | with `~RFIndex()` | without |
|---|---|---|
| 0 | −1581 | **+20 (baseline, the maximum)** |
| 1 | −1092 | −876 |
| 2 | −1216 | −752 |
| 3 | −839 | −1272 |
| 4 | −839 | −1581 |

No rung recovers it. Best reachable with the destructor in place is −839, against a +184 prize.

### What would unblock it

Nothing on this header. The prize is fixed at 184 B and the cheapest rung that can hold the
destructor is −839, so it needs a **new currency on some OTHER header** — the v486 `_v15` pattern,
where a freely-removable real declaration elsewhere absorbs the parity. None was available in
v549. Re-price only if some unrelated session frees one up.
