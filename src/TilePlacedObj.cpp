// Real constructors for the TilePlacedObj family (see TilePlacedObj.h for the class
// hierarchy writeup). Each was previously modeled as an "InitFieldsMaybe" ordinary member
// called after a manual `::operator new` (src/NetSessionEventQueue.cpp's own
// BigObj_CreateAndInsert, before this session) -- retired in favor of real `new
// ClassName(kindId)` expressions now that the full base-class chain is modeled, per that
// function's own parked SEH-scaffolding-shape diagnosis (CLAUDE.md).
//
// First-draft transcriptions -- content-complete (every field offset/branch confirmed against
// Ghidra's own decompile, cross-checked against the raw disasm for the 2 redundant-recheck
// branches below) but NOT YET byte-matched; own multi-session arc, see docs/PARKED.md.
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "BigObjSeqRecordMaybe.h"
#include "DecorActor.h"
#include "DecorObjMgrMaybe.h"
#include "EffectSpawner.h"
#include "UIResources.h"
#include "TimeOfDayMaybe.h"
#include "TilePlacedObj.h"
#include "WorldActionCursor.h"
#include "WorldBoardMaybe.h"
#include "ScreenSaver.h"      // g_screenSaver.bScreenSaverMode -- the 0x4a9918 claim gate
#include "NameAnchorMaybe.h" // PeerTrainNodeTrackPartial -- TryClaimForTrainMaybe's train view
#include "PeerTrainSlotQueueMaybe.h" // g_PeerTrainSlotQueue -- ~TrackTileObj's one callee

extern unsigned int g_dwGameTick; // DAT_004a99b4

// FUNCTION: LOCO 0x4580a0
// The `bSpawnVariance == 0` / `< 1` pair below is a genuine 2-branch structure in the
// original, not a simplification: raw disasm at 0x4580f8-0x458167 shows an unsigned `cmp
// al,bl; jbe` (variance <= 0) test, THEN a separate unsigned `cmp al,1; jb` (variance < 1)
// test whose target Ghidra's own decompiler flags as unreachable ("Removing unreachable
// block (ram,0x00458131)") -- a provably-dead redundant recheck that only survives VC5's
// optimizer as a RANGE compare, not an equality compare against the same constant (CLAUDE.md's
// `LoadIndexedFileMaybe` precedent). The dead branch computes `rand() % (2 - variance) +
// variance` (mirroring the live branch's `rand() % variance + 1` with the roles of the modulus
// and the added constant swapped) -- transcribed faithfully as unreachable-but-present source,
// not simplified away, so a future byte-match pass starts from the right shape.
// v210: `pDesc->bSpawnVariance` is read FRESH from memory at each use (no `cVariance`
// caching local) -- confirmed via raw disasm: the two comparisons share one `al` load (adjacent,
// no intervening call), but each branch's `rand() % variance` expression re-loads the field from
// memory via a fresh pDesc pointer read, not the already-loaded `al` (Yoda lesson #19's
// aliasing family: a real per-statement reload, no local, once a call to `rand()` intervenes).
// v211: 3 more real fixes, closing asmscore total 325768->232999 / byte_diff 238->119 / insns
// 113/119->119/119 (own EXACT insn count now, residual is purely register-allocation): (1) the
// SAME aliasing lever extended repo-wide -- dropped the `BigObj *pDesc = pKindDesc;` ctor-scope
// caching local entirely (not just the 2 recheck branches) after confirming via raw disasm that
// the original re-reads `dword ptr [this+0x40]` (pKindDesc) FRESH at 5 separate use sites across
// the whole function, never once caching it in a register; (2) inverted the outer
// `bSpawnVariance == 0` guard's polarity to `if (!=0) {...} else { =0; }` -- the original
// places the zero-case OUT OF LINE at the function's tail (a `jbe`-taken far jump), with the
// nonzero path as the fall-through/common case; our prior `if (==0){} else if...else` shape had
// this backwards (zero-case inlined, nonzero pushed out), CLAUDE.md's documented branch-order
// lever (`AlbumCardWnd::DrawOrEraseCardSlot` precedent) generalizing to a 3-way if/else-if/else chain,
// not just a 2-way if/else; (3) `memset(apNeighbourTileMaybe, 0, sizeof(...));` /
// `memset(anNeighbourCostMaybe, 0, sizeof(...));` replacing 8 individual `= 0;` field stores
// -- the original zeroes each 4-dword group via ONE shared-base `lea`+4 offset-relative stores,
// the same `BigObj_Remove`-documented memset-shaped-bulk-zero lever (CLAUDE.md), closing the
// single largest remaining chunk of this residual. Remaining residual (asmscore --dump) is
// entirely confined to the 2 `rand()` calls' register-allocation: the original keeps the loaded
// `bSpawnVariance` byte alive across the `rand()` call in a callee-saved register (no stack
// round-trip) at both call sites, while our candidate spills to a stack slot at one of them --
// tried explicit `int nRand = rand();` sequencing (no consistent improvement) and reordering the
// dead/live branch bodies; looks like the same intrinsic register-allocation tie-break class as
// Yoda #29/#30, not conclusively proven not-source-steerable yet. Own residual, PARKED.
TilePlacedObj::TilePlacedObj(unsigned int kindId) : AnimDescRefObj0x477488(kindId, -1, 0, 0) {
    nTypeTag = 3;
    pos.wPosX = 0;
    pos.wPosY = 0;

    if (pKindDesc != 0) {
        if (pKindDesc->bSpawnVariance != 0) {
            if (pKindDesc->bSpawnVariance < 1) {
                // sic: unreachable given bSpawnVariance > 0 above, see this function's own header comment.
                bOwnedActorSlotsMaybe = (unsigned char)((rand() % (2 - pKindDesc->bSpawnVariance)) + pKindDesc->bSpawnVariance);
            } else {
                bOwnedActorSlotsMaybe = (unsigned char)((rand() % pKindDesc->bSpawnVariance) + 1);
            }
        } else {
            bOwnedActorSlotsMaybe = 0;
        }
        bLiveOwnedActorsMaybe = 0;
        bSpawnedActorCountMaybe = 0;
        dwPlacementArgB = 0;
        dwSeqGoalCooldownUntilMaybe = 0;
        memset(apOwnedActorMaybe, 0, sizeof(apOwnedActorMaybe));
        memset(apSpawnedActorMaybe, 0, sizeof(apSpawnedActorMaybe));

        bSaveableFlag = 1;
        if (pKindDesc->szCategoryName[0] != 0) {
            SetCategoryIfPrintable(pKindDesc->szCategoryName);
        }
        if (0 <= pKindDesc->paFrameEntries[nSubFrame].nCooldownTicks) {
            nAnimCooldownUntil = (rand() % 0x3d) + g_dwGameTick;
        }

        // Both navigation-graph slots start empty with a -1 node id. The original zeroes slot
        // A's two quads as two separate 4-dword groups but slot B's as a single 8-dword run,
        // which is why the two slots are written out asymmetrically here rather than in a loop.
        memset(apNeighbourTileAMaybe, 0,
               sizeof(apNeighbourTileAMaybe));
        memset(anNeighbourCostAMaybe, 0,
               sizeof(anNeighbourCostAMaybe));
        nGraphNodeIdAMaybe = -1;
        memset(apNeighbourTileBMaybe, 0,
               sizeof(apNeighbourTileBMaybe) +
                   sizeof(anNeighbourCostBMaybe));
        nGraphNodeIdBMaybe = -1;
    }
}

// FUNCTION: LOCO 0x4583c0
unsigned char TilePlacedObj::GetFootprintRectMaybe(RECT *pRect) {
    // Unlike this class's own constructor -- which re-reads pKindDesc FRESH at all five of its
    // use sites, the documented aliasing shape -- this one caches it in a local: the original
    // loads `[this+0x40]` exactly once, hoisted ABOVE the pRect null test, and keeps
    // `&pDesc->rcFreeToRoamMaybe` live in one register across both the IsRectEmpty call and the
    // copy that follows.
    BigObj *pDesc = pKindDesc;
    if (pRect != 0 && pDesc != 0) {
        SetRectEmpty(pRect);
        // The descriptor's footprint box is ONE RECT (see src/CursorDesc.h); the casts are
        // only because that header deliberately has no <windows.h> and so cannot name the type.
        // It really is a whole-struct ASSIGNMENT and not four field copies -- spelling out the
        // four assignments instead costs 11 bytes and breaks the match.
        if (!IsRectEmpty((RECT *)&pDesc->lFreeToRoamLeft)) {
            *pRect = *(RECT *)&pDesc->lFreeToRoamLeft;
            OffsetRect(pRect, rect.left, rect.top);
            return 1;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x4125e0 (??_GTilePlacedObj scalar deleting dtor -- compiler-generated)
// FUNCTION: LOCO 0x458270
// Vtable slot 0, reached through the `??_G` scalar-deleting-dtor thunk at 0x4125e0. The /GX
// SEH scaffolding (__ehhandler 0x4764a8, the -1 state store before the base-dtor call) is the
// compiler's own, emitted because the base subobject needs unwinding -- there is nothing in the
// source that spells it.
TilePlacedObj::~TilePlacedObj() {
    int i;
    // The five registry entries this object owns are handed back to the manager and cleared...
    for (i = 0; i < 5; i++) {
        if (apOwnedActorMaybe[i] != 0) {
            DecorObjMgrMaybe_00485448.DeregisterEntryMaybe(apOwnedActorMaybe[i], 1);
            apOwnedActorMaybe[i] = 0;
        }
    }
    // ...while the five employees merely back-referencing it are cut loose in place: their own
    // owner pointer is dropped and their next-decision deadline reset so they re-target at once.
    for (i = 0; i < 5; i++) {
        if (apSpawnedActorMaybe[i] != 0) {
            apSpawnedActorMaybe[i]->pOwnerObjMaybe = 0;
            apSpawnedActorMaybe[i]->dwNextDecisionTickMaybe = 0;
        }
    }
}

// FUNCTION: LOCO 0x458310
// EFFECTIVE MATCH -- 22 bytes, insns 15/14. TU-STATE SENSITIVE, and that is the whole finding:
// with this same source and one fewer function ahead of it in the TU (before ~TilePlacedObj was
// landed) it scored 12014 with reg_pen 0 / identity_miss 0 / insns 14/14 -- a single scheduling
// slot apart from EXACT, the hidden return-buffer load (`mov eax,[esp+8]`) sitting BETWEEN the
// `sub` and the `add dx,[this+0x8a]` in the original and after it in ours. Landing the dtor
// ahead of it flipped cl into reassociating the sum to `(posY - steps) + rows` instead of the
// original's `(rows - steps) + posY`, which is legal because the destination is 16-bit and every
// spelling truncates the same. Probed at that new TU position, all worse or equal: `posY +
// (unsigned short)(rows - steps)` (22243, identical), an `int nRowDelta` temp (62932, insns
// 19/14 -- it also loses the `movzx`es), and the plain `rows - steps + posY` (40258, which
// reassociates the other way to `(rows + posY) - steps`). See docs/PARKED.md.
//
// ⚠ THIS FUNCTION IS THE REPO'S MOST DECLARATION-SET-SENSITIVE MATCH, and it has now flipped
// EXACT -> DIFF three times (v442-v445's five rejected header levers, then v446's). It went
// EXACT again for v445 and back to DIFF(18) in v446 when `PlacedObjRegistryMaybe`'s slot-15
// placeholder in src/DecorObjMgrMaybe.h gained its real two-argument parameter list. Bisected
// in v446 to the PARAMETER LIST alone: renaming `_v15` costs nothing, and widening the return
// type from `void *` to `void` costs nothing, but going from `()` to `(int, int)` on that one
// never-called virtual is what flips it. At the v446 TU state the residual is a bare 14/14
// instruction `sub edx,eax` vs `sub eax,edx` register coin flip at the same 60 bytes, and two
// further probes were spent and rejected: writing the sum `posY + (unsigned short)(rows -
// steps)` is byte-identical to the current spelling, and dropping the `pDesc` local to index
// `pKindDesc` twice is strictly worse (DIFF(59) at 63 bytes). Do NOT re-grind: the flip is
// upstream of anything spellable here. It is worth 60 bytes, which is the price any future
// header work on that class should expect to pay -- v446 paid it for +268 bytes elsewhere.
// v448 flipped it back ON for free, and v471 flipped it OFF again (fifth flip, DIFF(13) at 60 B)
// when `#include <stdio.h>` reached src/DSoundChannel.h so RFIndex could hold a real FILE*. That
// same edit flipped this TU's 0x4588b0 ON (+143), so TilePlacedObj.cpp came out +83 B net, and
// the repo came out +754 B / +4 funcs. The 60 B remains the standing toll, exactly as predicted.
TileGridPos TilePlacedObj::GetFrontRowTilePosMaybe() {
    BigObj *pDesc = pKindDesc;
    TileGridPos posFront;
    posFront.wPosX = pos.wPosX;
    posFront.wPosY = (unsigned short)(pDesc->bBitmapOccupancyRows -
                                      pDesc->bFootprintYSteps) + pos.wPosY;
    return posFront;
}

// FUNCTION: LOCO 0x458350
// EFFECTIVE MATCH -- 5 bytes, insns 40/40, reg_pen 0, identity_miss 0. The whole function agrees
// down to the register names; the only disagreement is the WIDTH of the two constant
// materializations that build bUnsetMaybe -- the original writes `mov ebx,1` / `xor ebx,ebx`
// (32-bit) and then tests `test bl,bl` (8-bit), ours writes the matching-width `mov bl,1` /
// `xor bl,bl`. The byte-wide test pins the variable's storage as 1 byte, so this is a pure
// constant-materialization tie-break, not a type error: `int` was probed and is far worse
// (total 50893 -- it re-allocates every register in the function and drops to insns 38/40), and
// `char`/`unsigned char`/`bool` are all byte-identical to each other at 8005. See docs/PARKED.md.
POINT TilePlacedObj::GetEntryExitPointMaybe(int nIndex) {
    POINT pt;
    pt.x = -1;
    pt.y = -1;
    if (pKindDesc == 0) {
        return pt;
    }
    bool bUnsetMaybe = pKindDesc->aEntryExitMaybe[nIndex * 2] == -1 &&
                       pKindDesc->aEntryExitMaybe[nIndex * 2 + 1] == -1;
    if (!bUnsetMaybe) {
        pt.x = rect.left + pKindDesc->aEntryExitMaybe[nIndex * 2];
        pt.y = pKindDesc->aEntryExitMaybe[nIndex * 2 + 1] + rect.top;
    }
    return pt;
}

// FUNCTION: LOCO 0x458800
unsigned char TilePlacedObj::ResetToBaseSubFrameMaybe() {
    ReleaseChannelAndDispatch(nSubFrame);
    return 0;
}

// FUNCTION: LOCO 0x458810
unsigned char TilePlacedObj::OnPlacedObjEventMaybe() {
    return 0;
}

// FUNCTION: LOCO 0x458820
// The object half of "a seq/visits goal in pRec was just met" -- see
// src/BigObjSeqRecordMaybe.h for the record. lTileIdA0xcMaybe is a three-way selector: -1
// means the record is inactive (the parser writes -1 for any id whose tile category isn't
// 2/4/0xc/0xd, and Load() seeds it to -1), 0 means "don't retarget, just poke the anim"
// (slot 7, ReleaseChannelAndDispatch), any other id retargets the object's descriptor
// (slot 6, SetDescriptor).
//
// The UI feedback afterwards is suppressed in two cases: something else is already selected,
// or the world widget is sitting in mode 3. The original duplicates that whole guard into
// both arms of the retarget branch -- ordinary VC5 tail duplication of a small block, not two
// source copies.
void TilePlacedObj::ApplySeqRecordChangeMaybe(BigObjSeqRecordMaybe *pRec) {
    if (pRec->lTileIdA0xcMaybe == -1) {
        return;
    }
    if (pRec->lTileIdA0xcMaybe != 0) {
        SetDescriptor(pRec->lTileIdA0xcMaybe, pRec->wSubFrameAMaybe, 0);
        if (SelectedObjWidgetMaybe_004852a0.bActive) {
            return;
        }
        if (g_worldActionCursor.bActive == 1 &&
            g_worldActionCursor.nModeMaybe == 3) {
            return;
        }
    } else {
        ReleaseChannelAndDispatch(pRec->wSubFrameAMaybe);
        if (SelectedObjWidgetMaybe_004852a0.bActive) {
            return;
        }
        if (g_worldActionCursor.bActive == 1 &&
            g_worldActionCursor.nModeMaybe == 3) {
            return;
        }
    }
    g_UIResources.PlayUiSound(0x571e);
    g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(this);
}

// FUNCTION: LOCO 0x4588b0
// ⭐ EXACT again as of v471 (third flip, back ON) -- source untouched once more. The trigger this
// time was `#include <stdio.h>` arriving in src/DSoundChannel.h so RFIndex could model its .RFD
// handle as a real FILE*; the same edit flipped this TU's 0x458310 OFF (-60) for a net +83 here.
// The historical autopsy below is kept because it is the record of WHY no source lever exists.
// v512: flipped OFF again (sixth flip, DIFF(79) at 141 B vs 143) by the 0x4589b0 ctor bundle --
// bisected to EITHER of its two header edits alone (the WorldActionCursor.h ctor+slot-16 decls
// OR the WidgetBase.h ctor default args; each independently flips it, removing both restores
// EXACT). Re-win probes refuted this session, all no-ops or worse: hoisted `nRecX/nRecY` locals
// (DIFF 97), hoisted `nOriginX/nOriginY` (DIFF 108), a `long lSpace` local (DIFF 74), and the
// #23 line-count dial at five positions/counts (all byte-identical DIFF(79)). The residual is
// unchanged in kind: the original's FOUR callee-saved registers vs our three, ebp spent only as
// the default arm's rect.top scratch -- a pure /Og register-budget coin flip.
// v552: flipped OFF again (EIGHTH flip), DIFF(79) at 141 B -- byte-for-byte the SAME residual
// as the sixth flip, so nothing here was re-autopsied and nothing should be. Trigger: the blit
// pair (0x42d280/0x42d3a0) becoming real member declarations on src/WorldActionCursor.h, which
// bought 0x42d280 its 278 B EXACT (CODEGEN #149) for this 143. Measured: ONE of the two
// declarations costs the same 143 B as both, so the pair was taken together.
// EXACT again as of v518 (seventh flip, back ON): the DecorObjMgrMaybe.h ctor/registry-model
// edits for 0x434500 rotated this TU's /Og state back onto the good side -- source untouched,
// as at every prior flip. The EFFECTIVE autopsy below is kept as the case study.
// EFFECTIVE MATCH (DIFF 79) -- and a fully-diagnosed case study in how little it takes to flip
// this function. Its v411 autopsy blamed two intrinsic tie classes (a partial-redundancy coin
// flip on `pRec->lEffectXMaybe` and a callee-saved-register count shift). It then went
// byte-identical FOR FREE when src/WorldBoardMaybe.h stopped spelling the board tile grid as one
// opaque `pad` array and modelled it as a real `BoardTileCellMaybe aTileCellsMaybe[...]` member --
// a TU nothing here touched. On 2026-07-26 it flipped straight back, and the cause was isolated by
// bisection to something even smaller and even more remote: adding ONE non-virtual member
// DECLARATION (`UiIconListItem::HandleTextEditKey`) to src/MenuNode.h, a class this TU never names
// and a function it never calls. Confirmed by removing the declaration (exact returns) and by
// moving it to three different positions in the class body (no effect -- it is the declaration's
// existence, not its placement). The declaration is faithful to the original class and is needed
// by src/WidgetPicker.cpp, so it stays; this residual is the price.
// The residual itself is the callee-saved-register count: the original uses FOUR (ebp/esi/edi/ebx)
// and this compile uses three, so `this` and the record pointer land in different registers and
// the diff is a pure rename cascade over otherwise identical instructions.
// ⚠ Do NOT re-autopsy this as a source-shape problem. Two independent, unrelated shared-header
// edits have now moved it in opposite directions without either one touching its source; it is a
// /Og register-budget coin flip, and the only thing that has ever changed it is TU context.
// ⚠ The switch IS still load-bearing -- an if / else-if / else chain over the same three cases
// compiles the bodies INLINE in source order (DIFF 83) instead of the original's
// jump-to-out-of-line layout. Do not "simplify" it back.
//
// The effect half of the same reward (see ApplySeqRecordChangeMaybe above). lSpaceCharMaybe
// names the coordinate space the record's stored point is expressed in -- 'S' scroll-relative,
// 'W' absolute world, anything else relative to this object's own rect origin -- and doubles
// as the spawner's direction char for 'U'/'D', every other space spawning as 'W'.
void TilePlacedObj::SpawnSeqRecordEffectMaybe(BigObjSeqRecordMaybe *pRec) {
    int nEffectX;
    int nEffectY;

    if (pRec->lEffectKindIdMaybe <= 0) {
        return;
    }
    switch (pRec->lSpaceCharMaybe) {
    case 'S':
        nEffectX = pRec->lEffectXMaybe + g_worldBoard.dwScrollX;
        nEffectY = pRec->lEffectYMaybe + g_worldBoard.dwScrollY;
        break;
    case 'W':
        nEffectX = pRec->lEffectXMaybe;
        nEffectY = pRec->lEffectYMaybe;
        break;
    default:
        nEffectX = pRec->lEffectXMaybe + rect.left;
        nEffectY = pRec->lEffectYMaybe + rect.top;
        break;
    }
    if (pRec->lSpaceCharMaybe == 'U' || pRec->lSpaceCharMaybe == 'D') {
        DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(pRec->lEffectKindIdMaybe,
                                                        pRec->wEffectMobilityMaybe,
                                                        (char)pRec->lSpaceCharMaybe, nEffectX,
                                                        nEffectY, 1);
    } else {
        DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(pRec->lEffectKindIdMaybe,
                                                        pRec->wEffectMobilityMaybe, 'W', nEffectX,
                                                        nEffectY, 1);
    }
}

// FUNCTION: LOCO 0x458940
void TilePlacedObj::UpdateOpeningHoursFrameMaybe() {
    if (bValid == 1) {
        tm *pNow = localtime((time_t *)&g_dwGameTick);
        BigObj *pDesc = pKindDesc;
        if (pDesc->wClosedFS != -1) {
            // The two "shifts" records are raw longs in src/CursorDesc.h (that header
            // deliberately does not pull in TimeOfDayMaybe.h -- see its own note), hence the casts.
            unsigned char bOpen = TimeOfDay_IsTimeInWindowMaybe(
                pNow, &pDesc->shiftOpen, &pDesc->shiftClose);
            if (bOpen) {
                if (nSubFrame == pDesc->wClosedFS) {
                    ReleaseChannelAndDispatch((short)pDesc->wActiveFrameSetIndex);
                }
            } else if (nSubFrame != pDesc->wClosedFS) {
                ReleaseChannelAndDispatch(pDesc->wClosedFS);
            }
        }
    }
}

// 0x446030, extern -- see src/UIResources.cpp. Declared TU-locally the same way
// src/Obj0x4779e0.cpp and src/Main.cpp already do; folding all three onto a shared header is a
// clean follow-up (it is a declaration-set lottery ticket, so it wants measuring on its own).
extern unsigned int __cdecl TileKind_GetCategory(unsigned int kindId); // 0x446030

// FUNCTION: LOCO 0x458430
// Vtable slot 15, overriding AnimDescRefObj0x477488's own 0x4062a0. Content-complete against the
// raw disasm but NOT byte-matched (DIFF 495, 686 B against the original's 968, insns 222/312).
//
// ⚠ The largest single piece of that gap is NOT a source-shape problem: the original INLINES
// GetFootprintRectMaybe here (its dead `pRect != 0` test on a stack address, at 0x458486, is the
// giveaway) while our build emits a `call`, because VC5's /O2 implies /Ob1 -- only
// explicitly-`inline` functions get inlined. Two probes, both measured:
//   * marking the definition `inline` DOES inline it here (704 -> 778 B) but then cl stops
//     emitting the out-of-line COMDAT at all, costing that function's own 99-byte EXACT. Net
//     loss, reverted.
//   * `/O2 /Ob2` gives BOTH -- the inline here AND a byte-exact standalone 0x4583c0 -- but is
//     refuted REPO-WIDE: `LOCO_OPT="/O2 /Ob2" tools/progress.py` scores 87077 B / 382 funcs
//     against /O2's 89321 / 395, losing matches across DPlaySessionMgr, EditCardWnd, GameNet,
//     MenuNode, PeerTrainNode, PopupWndBase, TutorialWnd and WidgetBase (only
//     ScopedTimestampMaybe and ThreadWrapper gain). Phase 0's /O2 stands.
// The consistent reading is that the ORIGINAL declared GetFootprintRectMaybe `inline` and the
// standalone 0x4583c0 is some OTHER TU's COMDAT copy that the linker happened to keep -- which
// would move that marker off this file. Not chased; see docs/PARKED.md.
//
// Reads as "hire one more employee": find the first free slot in apOwnedActorMaybe, resolve the
// kind (a random pick from the descriptor's own aPossibleMinifigs roster when the caller
// passes none), then pick a standing spot -- a uniformly random point inside this object's
// footprint rect that leaves room for the actor's whole sprite, or this object's own hotspot
// position when it has no footprint -- and ask the manager to build and register the actor
// there. A newly built actor that cannot actually stand where it was put is immediately
// deregistered and the slot cleared again.
//
// The three `nSpan == 0` guards below are the same provably-dead redundant range recheck this
// TU's constructor already carries (see its own header comment): inside a branch that has just
// established the span is >= 1, it can never be zero, but VC5's range compare emits the test
// anyway. Transcribed faithfully rather than simplified away.
//
// v411 corrected a real semantic error here and dropped DIFF 566 -> 495 (704 -> 686 B): the
// `left > right - nativeWidth` arm's divisor is `left - (right - nativeWidth) + 1`, NOT the
// `-nRangeX` the first draft carried, and its base is `right - nativeWidth`. Read together with
// the opposite arm, the X pick is a clean symmetric "uniform point in [min,max] inclusive"
// (0x45861d..0x458657 and its twin 0x458696..0x4586c9). The Y pick is deliberately NOT symmetric:
// its `top > bottom - nativeHeight` arm just yields `top`, no rand (0x458594's `jg`).
//
// The two structural gaps that remain, each probed once in v411 and both REJECTED -- do not
// re-grind either without a new idea:
//   * the original TAIL-DUPLICATES the whole X block into both arms of the Y branch (copies at
//     0x4585da and 0x45865e, identical but for whether the Y value comes from the shared stack
//     slot [esp+0x10] or straight from rcFootprint.top at [esp+0x18]), ~112 B. Spelling the X
//     block twice in the source does NOT reproduce it -- cl cross-jumps the copies back together
//     and the score gets WORSE (DIFF 532, 727 B).
//   * writing each span the way the original's post-rand() blocks recompute it (`bottom - top -
//     nativeHeight + 1`, reassociated, no named local) is worse again (DIFF 548, 746 B): without
//     the named locals cl merges all four `call rand` blocks into one shared site, where the
//     original keeps five. The original's memory-heavy shape (rcFootprint reloaded from the stack
//     in every block, nSpawnX/nSpawnY living in [esp+0x10]) is register-pressure driven, not
//     source driven.
DecorActorBase *TilePlacedObj::SpawnOwnedActorMaybe(int kindId) {
    RECT rcFootprint;
    DecorActorBase *pSpawned = 0;
    int nSpawnX;
    int nSpawnY;

    if (bValid != 1) {
        return 0;
    }
    if (bOwnedActorSlotsMaybe == 0) {
        return 0;
    }
    if ((unsigned int)(DecorObjMgrMaybe_00485448.nActiveCategory7Maybe +
                       DecorObjMgrMaybe_00485448.nActiveCategory8Maybe) < 100) {
        GetFootprintRectMaybe(&rcFootprint);

        int nSlot = 0;
        for (nSlot = 0; nSlot < bOwnedActorSlotsMaybe; nSlot++) {
            if (apOwnedActorMaybe[nSlot] == 0) {
                break;
            }
        }
        if (kindId <= 0) {
            kindId = pKindDesc->aPossibleMinifigs[rand() % 5];
        }
        // The kind table really holds BigObj descriptors -- the same type pKindDesc has -- but
        // UIResources::TileKind_GetOrLoadDescriptor is still declared as returning the
        // CursorDesc base, so this reads bBitmapOccupancyCols (an Obj0x4779e0 field)
        // through a downcast. Widening that shared declaration is a clean follow-up; it is a
        // declaration lottery ticket, so it wants measuring on its own.
        BigObj *pKind = (BigObj *)g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
        if (nSlot < bOwnedActorSlotsMaybe && pKind != 0) {
            if (pKind->categoryByte == 7 &&
                pKind->nLiveInstanceCountMaybe < pKind->bBitmapOccupancyCols) {
                DecorObjMgrMaybe_00485448.lockAMaybe.Lock();
                if (IsRectEmpty(&rcFootprint)) {
                    nSpawnY = hotspotPosY - pKind->hotspotY;
                    nSpawnX = hotspotPosX - pKind->hotspotX;
                } else {
                    int nMaxY = rcFootprint.bottom - pKind->nativeHeight;
                    if (rcFootprint.top > nMaxY) {
                        nSpawnY = rcFootprint.top;
                    } else {
                        int nSpanY = nMaxY - rcFootprint.top + 1;
                        if (nSpanY == 0) {
                            // sic: unreachable given top <= nMaxY above.
                            nSpawnY = rcFootprint.top;
                        } else {
                            nSpawnY = rcFootprint.top + rand() % nSpanY;
                        }
                    }
                    int nMaxX = rcFootprint.right - pKind->nativeWidth;
                    if (rcFootprint.left > nMaxX) {
                        int nSpanX = rcFootprint.left - nMaxX + 1;
                        if (nSpanX == 0) {
                            // sic: unreachable given left > nMaxX above.
                            nSpawnX = nMaxX;
                        } else {
                            nSpawnX = nMaxX + rand() % nSpanX;
                        }
                    } else {
                        int nSpanX = nMaxX - rcFootprint.left + 1;
                        if (nSpanX == 0) {
                            // sic: unreachable given left <= nMaxX above.
                            nSpawnX = rcFootprint.left;
                        } else {
                            nSpawnX = rcFootprint.left + rand() % nSpanX;
                        }
                    }
                }
                apOwnedActorMaybe[nSlot] =
                    DecorObjMgrMaybe_00485448.SpawnActorForKindMaybe(kindId, this, nSpawnX,
                                                                    nSpawnY);
                if (apOwnedActorMaybe[nSlot] != 0) {
                    if (apOwnedActorMaybe[nSlot]->CanStandAtMaybe(hotspotPosX, hotspotPosY) == 0) {
                        DecorObjMgrMaybe_00485448.DeregisterEntryMaybe(apOwnedActorMaybe[nSlot], 0);
                        apOwnedActorMaybe[nSlot] = 0;
                    } else {
                        apOwnedActorMaybe[nSlot]->ActivateMaybe();
                        bLiveOwnedActorsMaybe++;
                    }
                }
                pSpawned = apOwnedActorMaybe[nSlot];
                DecorObjMgrMaybe_00485448.lockAMaybe.Unlock();
                return pSpawned;
            }
            if ((char)TileKind_GetCategory(kindId) == 8 &&
                pKind->nLiveInstanceCountMaybe < pKind->bBitmapOccupancyCols) {
                DecorObjMgrMaybe_00485448.lockBMaybe.Lock();
                apOwnedActorMaybe[nSlot] = DecorObjMgrMaybe_00485448.SpawnActorForKindMaybe(
                    kindId, this, hotspotPosX, hotspotPosY);
                if (apOwnedActorMaybe[nSlot] != 0) {
                    if (apOwnedActorMaybe[nSlot]->CanStandAtMaybe(hotspotPosX, hotspotPosY) == 0) {
                        DecorObjMgrMaybe_00485448.DeregisterEntryMaybe(apOwnedActorMaybe[nSlot], 0);
                        apOwnedActorMaybe[nSlot] = 0;
                    } else {
                        bLiveOwnedActorsMaybe++;
                    }
                }
                DecorObjMgrMaybe_00485448.lockBMaybe.Unlock();
                pSpawned = apOwnedActorMaybe[nSlot];
            }
        }
    }
    return pSpawned;
}

// FUNCTION: LOCO 0x44ae80
// dwTrackState's full m_type0x63a -> state enum is ground-truthed in
// docs/subsystems.md's TilePlacedObj entry (v77, via BigObj::
// BigObj_ParseSocketTableAndTypeKeywordMaybe's .dat keyword parser).
TrackTileObj::TrackTileObj(unsigned int kindId) : TilePlacedObj(kindId) {
    BigObj *pDesc = pKindDesc;
    nTypeTag = 4;
    dwTrackState = 0;
    dwTrackTickState = 3;

    if (pDesc->m_type0x63a == 0xc) {
        dwTrackState = 1;
        unsigned int dir = (unsigned short)pDesc->wActiveFrameSetIndex;
        if (pDesc->m_type0x63a == 0xb) {
            // sic: unreachable (the outer branch already tests m_type0x63a == 0xc)
            if (dir == 0) {
                dwTrackTickState = 5;
                ReleaseChannelAndDispatch(0);
            } else {
                if (dir == 1) {
                    dwTrackTickState = 4;
                }
                ReleaseChannelAndDispatch(dir);
            }
        } else {
            dwTrackTickState = dir;
            ReleaseChannelAndDispatch(dir);
        }
    } else if (pDesc->m_type0x63a == 0xb) {
        dwTrackState = 2;
        dwTrackTickState = 5;
    } else if (!pDesc->IsType0x63aInSet1234()) {
        unsigned char type = pDesc->m_type0x63a;
        if (type == 0xd) {
            dwTrackState = 6;
        } else if (type == 5 || type == 6) {
            dwTrackState = 5;
        } else if (type == 0xe || type == 0xf || type == 0x10 || type == 0x11) {
            dwTrackTickState = 4;
            dwTrackState = 7;
            dwTrackTickState = 4;
            ReleaseChannelAndDispatch(1);
        } else {
            int resourceId = (pKindDesc == 0) ? -1 : pKindDesc->resourceId;
            if (resourceId == 0xc68 || resourceId == 0xc66 || resourceId == 0xc6a || resourceId == 0xc64) {
                dwTrackState = 8;
            }
        }
    } else {
        dwTrackState = 3;
    }

    pPendingCoupleWaiter = 0;
    nOccupantRefCount = 0;
}

// FUNCTION: LOCO 0x44b030 (??_GTrackTileObj scalar deleting dtor -- compiler-generated)
// FUNCTION: LOCO 0x44b050
// TrackTileObj's destructor -- vtable 0x478308 slot 0 is reached through the `??_G`
// scalar-deleting thunk at 0x44b030, which this one definition also emits (85 B here + 30 B
// there). Both were unclaimed until this session; the class has carried a declared-only
// `virtual ~TrackTileObj()` since the family was modeled, so landing the body needs no new
// declaration anywhere -- which is why it is cheap on a header whose declaration count is a
// live dial for two other TUs (see the EnqueueClaimWaiterMaybe note in TilePlacedObj.h).
//
// The whole body is the one line below: hand back every train slot still overlapping this
// tile's footprint, then let the compiler chain ~TilePlacedObj. The /GX SEH scaffolding
// (__ehhandler, the -1 state store around the base-dtor call) is the compiler's own, emitted
// because the base subobject needs unwinding -- exactly as in ~TilePlacedObj above.
TrackTileObj::~TrackTileObj() {
    g_PeerTrainSlotQueue.ReleaseSlotsOverlappingRectMaybe(this);
}

// TrackTileObj's two OWN virtual overrides live on TU-local views rather than on the class, for
// the reason spelled out at the (deliberately absent) declarations in src/TilePlacedObj.h:
// declaring the pair there costs 1667 B of exact matches in three unrelated TUs. Same escape
// hatch as TrackConnectorTileObjPumpView0x44f340 below.
//
// They need TWO views, not one, and the split is load-bearing. 0x44b0b0 reaches slot 7 through a
// VIRTUAL call (`call [reg+0x1c]` in the original), so its view must NOT redeclare
// ReleaseChannelAndDispatch -- a redeclaration would hide the inherited virtual and turn those
// four calls into direct ones. 0x44b130 IS slot 7, so it needs the redeclaration, and reaches its
// own base body by explicit qualification.
struct TrackTileObjChannelView0x44b130 : TrackTileObj {
    void ReleaseChannelAndDispatch(unsigned int arg); // 0x44b130, slot 7 (+0x1c)
};

struct TrackTileObjTickView0x44b0b0 : TrackTileObj {
    unsigned char ResetToBaseSubFrameMaybe(); // 0x44b0b0, slot 16 (+0x40)
};

// Same TU-local-view dodge for TrackConnectorTileObj's own claim-queue pump. It is a plain
// (non-virtual) member of that class, but declaring it there costs 1103 B across two other TUs --
// see the measured note in src/TilePlacedObj.h where the declaration would otherwise go.
struct TrackConnectorTileObjPumpView0x44f340 : TrackConnectorTileObj {
    void PumpClaimWaitQueueMaybe(); // 0x44f340
};

// FUNCTION: LOCO 0x44b130
// Vtable slot 7 (+0x1c) -- TrackTileObj's override of the family's channel dispatcher. Two of the
// track kinds give the argument a second meaning before passing it on:
//   * a GATE (descriptor type 0xb) or a tile already in dwTrackState 7 treats arg as a gate
//     command -- 0 closes it (tick state 5) and 1 opens it (tick state 4). Note the asymmetry,
//     which is the original's: the CLOSE arm latches the state and delegates immediately, while
//     the OPEN arm falls through to the shared delegation at the bottom;
//   * a POINTS tile (dwTrackState 1) stores arg as the tick state verbatim.
// Every other tile just delegates. The base call is explicitly qualified because this IS the
// slot-7 body -- an unqualified call would dispatch straight back into itself.
//
// EFFECTIVE MATCH -- asmscore --len 96 (0x44b190, the next function, is the COMDAT bound): total
// 6224, align=6 reg_pen=2 identity_miss=2 byte_diff=4, insns 27/26. CONTENT-COMPLETE: every call,
// operand, branch target and all three epilogues agree. The ENTIRE residual is one instruction --
// the dwTrackState==1 arm, where the original keeps arg in edx for both the push and the store
// (`push edx` / `mov [ecx+0x110],edx`) and this compile routes it through eax (`mov eax,edx` /
// `push eax` / `mov [ecx+0x110],eax`). Pure register coin-flip; the shift is what makes it read
// as DIFF(51).
//
// THREE call sites, not one, and that is confirmed structure rather than a transcription choice:
// the original calls the base at 0x44b159, 0x44b170 AND 0x44b188, so the two early arms really do
// return rather than falling into the shared tail. Merging them would emit two call sites and is
// therefore wrong, however much tidier it looks.
//
// The switch lever that made 0x44b0b0 exact does NOT apply here and was checked, not assumed: the
// gate block's dispatch is `test edx,edx / jne` then `cmp edx,1 / jne` -- cl's if-CHAIN lowering,
// not the `sub`/`dec` ladder a dense switch emits -- so the if-chain below is already the
// original's own shape, and that whole block matches byte-for-byte. PARKED (docs/PARKED.md).
void TrackTileObjChannelView0x44b130::ReleaseChannelAndDispatch(unsigned int arg) { // TODO: sync (TU-local view)
    if (pKindDesc->m_type0x63a == 0xb || dwTrackState == 7) {
        if (arg == 0) {
            dwTrackTickState = 5;
            // `arg` rather than a literal 0 (it is provably 0 on this path). The original pushes
            // the arg REGISTER here (`push edx` at 0x44b165), which looks like it should rule a
            // literal out -- but both spellings compile BYTE-IDENTICALLY (measured), because cl
            // already has arg live in edx and will not spend an immediate to re-materialize it.
            // So the original's source is undecidable at this site; `arg` is kept as the form that
            // does not assert a constant the caller is not required to supply.
            AnimDescRefObj0x477488::ReleaseChannelAndDispatch(arg);
            return;
        }
        if (arg == 1) {
            dwTrackTickState = 4;
        }
    } else if (dwTrackState == 1) {
        dwTrackTickState = arg;
        AnimDescRefObj0x477488::ReleaseChannelAndDispatch(arg);
        return;
    }
    AnimDescRefObj0x477488::ReleaseChannelAndDispatch(arg);
}

// FUNCTION: LOCO 0x44b0b0
// Vtable slot 16 (+0x40) -- TrackTileObj's override of TilePlacedObj::ResetToBaseSubFrameMaybe
// (0x458800, the 12-byte base body). The per-tick animation step for the only two track kinds
// that animate, and the two tests are INDEPENDENT `if`s rather than an else-chain: a descriptor
// is only ever one type, but the second block re-reads pKindDesc after the first block's calls,
// which is what the original does.
//   * type 0xc (POINTS) advances its three-phase tick state 0 -> 1 -> 2 -> 0, pushing each new
//     phase out through the slot-7 dispatcher above (which, being dwTrackState 1, stores it);
//   * type 0xb (a level-crossing GATE) re-opens once no train is standing on the tile: tick
//     state 4 (opening) settles to 0, and 5 (closed) steps to 1.
// The dispatcher calls go through the VIRTUAL slot, not the qualified base -- this leaf's own
// override at 0x44b130 is what gives those arguments their gate/points meaning.
//
// Returns 1 unconditionally; the byte is what the caller tests, and the family's own base and
// its do-nothing sibling at 0x458810 both answer the same way.
//
// ⭐ EXACT (125 B). ONE lever took it there, from DIFF(89) to byte-identical in a single compile:
// both tick-state dispatches are SWITCHes, not if/else chains. The first transcription had them
// as `if (s == 0) … else if (s == 1) …`, which reached insns 51/51 -- identical instruction COUNT
// and every call, operand and branch target already agreeing -- with the whole 92032 residual
// being `test eax,eax / cmp eax,1 / cmp eax,2` where the original has the `sub eax,0 / je / dec
// eax / je / dec eax` ladder that IS VC5's dense-switch lowering. Same lever, same signature, as
// ResolveWorldClickMaybe's case-dispatch note in src/WorldBoardMaybe.cpp. Worth generalizing: a
// residual that is instruction-count-EXACT but full of `cmp`-against-consecutive-constants is a
// switch written as an if-chain, and the fix is free.
unsigned char TrackTileObjTickView0x44b0b0::ResetToBaseSubFrameMaybe() { // TODO: sync (TU-local view)
    if (pKindDesc->m_type0x63a == 0xc) {
        switch (dwTrackTickState) {
        case 0:
            ReleaseChannelAndDispatch(1);
            break;
        case 1:
            ReleaseChannelAndDispatch(2);
            break;
        case 2:
            ReleaseChannelAndDispatch(0);
            break;
        }
    }
    if (pKindDesc->m_type0x63a == 0xb) {
        if (!g_PeerTrainSlotQueue.IsTileOccupiedByAnyTrainMaybe(this)) {
            switch (dwTrackTickState) {
            case 4:
                ReleaseChannelAndDispatch(0);
                break;
            case 5:
                ReleaseChannelAndDispatch(1);
                break;
            }
        }
    }
    return 1;
}

// FUNCTION: LOCO 0x44f210
// v209: fixed a real field-mapping bug (was: dwOccupancyClaim zeroed before the switch,
// pOwningTrain got the 1/0 sentinel, pClaimWaitQueueHeadMaybe zeroed in both arms -- all 3 names
// shifted one slot too early). Raw disasm at 0x44f240/0x44f24f/0x44f259/0x44f266/0x44f26c
// pins the byte pattern (unconditional zero before the switch / 1-or-0 inside the switch /
// unconditional zero in both arms), and PeerTrainNode::UpdatePlacementTickMaybe's own disasm
// (`mov ebx,1` .. `mov [edi+0x11c],ebx` then `mov [edi+0x120],esi`, esi=that function's own
// `this`) plus NetSessionEventQueue.cpp's independent `QueuedBigObjItemPartial::
// pOwningTrain` (+0x120) / `== (void*)1` sentinel check pin the SEMANTIC mapping: +0x11c
// is dwOccupancyClaim (0=free, unconditionally zeroed here), +0x120 is pOwningTrain
// (a real void*, preset to a `(void*)1` sentinel for the 4 global-connector kind ids and later
// overwritten with the real owner), +0x124 is genuinely unmodeled (pClaimWaitQueueHeadMaybe). DIFF
// 8->3 bytes (byte pattern was already right; only the names were swapped). EFFECTIVE MATCH:
// the residual 3 bytes are pure NOP-fill padding between the ret and the 4-byte-aligned jump
// table (ours: 90 8b ff / original: 8d 49 00, both 3-byte no-ops) -- all real code is
// byte-identical up to that point (insns 42/43, the 1-insn delta is exactly this padding
// choice). Not source-steerable; don't re-grind.
TrackConnectorTileObj::TrackConnectorTileObj(unsigned int kindId) : TrackTileObj(kindId) {
    nTypeTag = 5;
    dwTrackState = 3;
    dwOccupancyClaim = 0;
    switch (kindId) {
    case 0xc42:
    case 0xc44:
    case 0xc46:
    case 0xc48:
        pOwningTrain = (void *)1;
        pClaimWaitQueueHeadMaybe = 0;
        break;
    default:
        pOwningTrain = 0;
        pClaimWaitQueueHeadMaybe = 0;
        break;
    }
}

// FUNCTION: LOCO 0x44f2a0 (??_GTrackConnectorTileObj scalar deleting dtor -- compiler-generated)
// FUNCTION: LOCO 0x44f2c0
// The ctor's counterpart: drain and free the tile's claim-wait FIFO, one node at a time. The
// vtable re-stamp and the chained ~TrackTileObj() base call (0x44b050) are both
// compiler-generated, so the whole hand-written body is this loop.
//
// The head field is re-read from the object on every iteration rather than kept in a register --
// that is the ORIGINAL's own shape (`mov eax,[esi+0x124]` inside the loop, not just before it),
// and it falls out of writing the walk against the member instead of a cached local. The
// successor is latched BEFORE the delete, as it has to be.
//
// A plain `delete` here, not the vtable-slot-0 scalar-deleting-dtor dispatch the surrounding
// train/anchor code needs: TrainClaimWaitNode is a bare two-pointer link node with no vtable, so
// the original calls operator delete (0x465cd0) directly.
TrackConnectorTileObj::~TrackConnectorTileObj() {
    while (pClaimWaitQueueHeadMaybe != 0) {
        TrainClaimWaitNode *pNext = pClaimWaitQueueHeadMaybe->pNext;
        delete pClaimWaitQueueHeadMaybe;
        pClaimWaitQueueHeadMaybe = pNext;
    }
}

// FUNCTION: LOCO 0x44f340
// The CONNECTOR-side per-tick pump of the +0x124 claim-wait FIFO -- the near-twin of
// TrackDepotTileObj::PumpClaimWaitQueueMaybe (0x412a80) below, and much the simpler of the two:
// a connector tile has no "watch the current owner drop out of coupling-ready" half at all, so
// there is no owner branch and no re-read of the head after a re-entrant call. It just advances
// the tile's own anim frame and, if a waiter is queued and the tile is unclaimed, claims the
// tile, pops the head and drives the popped train onto it.
//
// Note the claim flag differs from the depot twin's: the connector marks itself taken by writing
// dwOccupancyClaim (+0x11c), where the depot uses bClaimLockedFlag (+0x128) -- the depot has both
// fields, the connector only the first.
//
// The popped node's pTrain is latched BEFORE the delete, as it must be; the anchor re-layout,
// the sound-state change and the dwModeAMaybe=4 store all run against that latched pointer after
// the node is gone.
void TrackConnectorTileObjPumpView0x44f340::PumpClaimWaitQueueMaybe() // TODO: sync (TU-local view)
{
    AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();

    TrainClaimWaitNode *pNode = pClaimWaitQueueHeadMaybe;
    if (pNode != NULL && dwOccupancyClaim == 0) {
        dwOccupancyClaim = 1;
        PeerTrainNodePartial *pTrain = pNode->pTrain;
        pClaimWaitQueueHeadMaybe = pNode->pNext;
        delete pNode;
        pTrain->LayoutCarAnchorsMaybe(this, 0);
        pTrain->SetSoundStateMaybe(2);
        pTrain->dwModeAMaybe = 4;
    }
}

// FUNCTION: LOCO 0x412870
// v209: same field-name correction as TrackConnectorTileObj above (+0x11c
// dwOccupancyClaim / +0x120 pOwningTrain / +0x124 pClaimWaitQueueHeadMaybe) -- byte pattern
// (all 3 zeroed) is unaffected by the rename, this ctor was already EXACT MATCH.
TrackDepotTileObj::TrackDepotTileObj(unsigned int kindId) : TrackTileObj(kindId) {
    dwOccupancyClaim = 0;
    pOwningTrain = 0;
    pClaimWaitQueueHeadMaybe = 0;
    bClaimLockedFlag = 0;
    dwTrackState = 4;
}

// FUNCTION: LOCO 0x4128b0 (??_GTrackDepotTileObj scalar deleting dtor -- compiler-generated)
// FUNCTION: LOCO 0x4128d0
// The depot leaf's destructor: drain and free the tile's claim-wait FIFO, then let the compiler
// chain ~TrackTileObj (0x44b050). The vtable re-stamp to 0x477848, the /GX SEH scaffolding and
// the base-dtor call are all the compiler's own, so the loop below is the whole hand-written body.
// The class has carried a declared-only `virtual ~TrackDepotTileObj()` since the family was
// modeled, so landing this costs no new declaration on a header whose declaration count is a live
// dial for two other TUs (see the EnqueueClaimWaiterMaybe note in src/TilePlacedObj.h).
//
// ⚠ NOT a copy of the connector twin at 0x44f2c0, even though the two drain the same +0x124 list:
// that one re-reads pClaimWaitQueueHeadMaybe from the object on every iteration (`mov
// eax,[esi+0x124]` INSIDE the loop) and stores the successor back into the member, while this one
// walks a register-held local and never writes the member at all (the head is loaded once, before
// the entry test). Both shapes are the compiler faithfully rendering two different sources, so
// harmonizing the twins would break one of them -- see docs/CODEGEN.md #116.
//
// A plain `delete`, not vtable-slot-0 dispatch: TrainClaimWaitNode is a bare two-pointer link node
// with no vtable, so the original calls operator delete (0x465cd0) directly.
TrackDepotTileObj::~TrackDepotTileObj() {
    TrainClaimWaitNode *pNode = pClaimWaitQueueHeadMaybe;
    while (pNode != 0) {
        TrainClaimWaitNode *pNext = pNode->pNext;
        delete pNode;
        pNode = pNext;
    }
}

// FUNCTION: LOCO 0x4129c0
// Tries to claim this depot tile for pTrain's train, gated on NOT screen-saver mode
// (g_screenSaver.bScreenSaverMode != 1, the free 0x4a9918 read), the train's nDiscardFlag,
// and this tile's own bClaimLockedFlag. On the claim path: release any prior claim via the
// tile's own vtbl slot 7 (ReleaseChannelAndDispatch(0)), lock the tile, record the owner,
// notify the train (SetModeBMaybe(1)), and copy the tile's wPosX/wPosY grid position into the
// train's wClaimedPosXMaybe/wClaimedPosYMaybe pair as ONE dword (the documented unaligned
// pair-copy idiom -- +0x32 pair, distinct from the +0x2e wSentinelCMaybe pair). On failure:
// if the tile is still free, mark it contention-claimed (ReleaseChannelAndDispatch(2) +
// dwOccupancyClaim = 2), then either request a direction reversal (TryTransitionModeMaybe
// toward `dwReversed == 0`'s target, forced) when CanReverseDirectionMaybe allows, or park
// the train in waiting sound state 1 with the bUnk0x90/wUnk0x36=2 reversal-park countdown.
// Called from NameAnchorMaybe::AdvanceAlongTrackMaybe's IsType0x63aInSet branch; see
// docs/subsystems.md v72.
unsigned char TrackDepotTileObj::TryClaimForTrainMaybe(PeerTrainNode *pTrain) {
    PeerTrainNodeTrackPartial *pNode = (PeerTrainNodeTrackPartial *)pTrain;
    if (g_screenSaver.bScreenSaverMode != 1 && pNode->nDiscardFlag == 0 &&
        this->bClaimLockedFlag == 0) {
        if (this->dwOccupancyClaim != 0) {
            this->ReleaseChannelAndDispatch(0);
        }
        this->bClaimLockedFlag = 1;
        this->pOwningTrain = pTrain;
        pNode->SetModeBMaybe(1);
        *(unsigned int *)&pNode->wClaimedPosXMaybe = *(unsigned int *)&this->pos.wPosX;
        return 1;
    }
    if (this->dwOccupancyClaim == 0) {
        this->ReleaseChannelAndDispatch(2);
        this->dwOccupancyClaim = 2;
    }
    if (pNode->CanReverseDirectionMaybe() != 0) {
        pNode->TryTransitionModeMaybe(pNode->dwReversed == 0, 1);
        return 0;
    }
    pNode->SetSoundStateMaybe(1);
    pNode->bUnk0x90 = 1;
    pNode->wUnk0x36 = 2;
    return 0;
}

// FUNCTION: LOCO 0x412a80
// The DEPOT-side per-tick pump of the +0x124 claim-wait FIFO, and the consumer whose producer is
// KeyedList_AddByKeyMaybe. Two mutually exclusive jobs, which is why the owner test brackets both:
// while a train still owns the tile, watch for it to drop out of the coupling-ready state
// (dwModeBMaybe 0) and release the claim; once nobody owns the tile, hand it to the head waiter.
//
// The re-read of pClaimWaitQueueHeadMaybe after ClaimDecorObjMaybe is the original's and is NOT
// redundant: that call reaches back into this tile through the train (it is what
// TryClaimForTrainMaybe's release path calls too), so the head this function is about to pop may
// already be gone. The `if (pNode != NULL)` that guards the pop is the same defensive re-test.
//
// ⚠ CORRECTED 2026-07-31 (v536): this IS a vtable slot -- TrackDepotTileObj's own slot 10 (+0x28)
// override of AdvanceAnimFrameMaybe (vtable 0x477848, dword at 0x477870). The earlier note here
// concluded "plain member" because both call sites dispatch directly, which is ordinary qualified
// dispatch and proves nothing either way; the base-chaining first statement below is the real tell.
void TrackDepotTileObj::PumpClaimWaitQueueMaybe()
{
    AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();

    PeerTrainNodePartial *pOwner = (PeerTrainNodePartial *)pOwningTrain;
    if (pOwner != NULL) {
        if (pOwner->dwModeBMaybe == 0) {
            pOwningTrain = NULL;
            bClaimLockedFlag = 0;
        }
    } else if (pClaimWaitQueueHeadMaybe != NULL && bClaimLockedFlag == 0) {
        pClaimWaitQueueHeadMaybe->pTrain->ClaimDecorObjMaybe(this, 1);
        TrainClaimWaitNode *pNode = pClaimWaitQueueHeadMaybe;
        if (pNode != NULL) {
            pClaimWaitQueueHeadMaybe = pNode->pNext;
            delete pNode;
        }
    }
}

// TU-local methods-only view for 0x412940. TrackDepotTileObj's override of vtable slot 16 is
// NOT declared on the class in src/TilePlacedObj.h for the usual reason: MEASURED 2026-07-31,
// that one declaration costs -1103 B elsewhere (WorldBoardMaybe's 0x457ce0
// FindNearestObjOfCategoryMaybe -951, and this family's own 0x452b00 -152) -- the combined
// shared-header declaration parity documented on src/WidgetBase.h's RepositionWithHotspot.
// Spending the src/Obj0x477798Family.h `_v15` parity slot was tried and is NOT the answer here:
// it buys back the 0x452b00 -152 but leaves the -951 and breaks RoadVehicleActor.cpp instead
// (-504), i.e. strictly worse. The slot is already virtual on TilePlacedObj, so this view only
// re-declares it to give the definition a home.
struct TrackDepotTileObjResetView0x412940 : TrackDepotTileObj {
    unsigned char ResetToBaseSubFrameMaybe(); // 0x412940  // TODO: sync (TU-local view)
};

// FUNCTION: LOCO 0x412940
// Vtable slot 16 (+0x40) -- this class's override of TilePlacedObj::ResetToBaseSubFrameMaybe
// (whose own default body is 0x458800). ⚠ The inherited slot NAME does not describe this
// override at all: nothing here touches a subframe. It is the depot's "do you have a train to
// work with?" poll. With no owner, and provided the tile is not claim-locked, it asks the peer
// train roster to spawn or hand over a random train, and reports success. With an owner, the
// owner's own mode decides: modes 1/4/5 reverse it out of the depot (through the re-entrancy
// guard), mode 2 re-binds it to this tile, and mode 3 -- like every mode outside 1..5 -- does
// nothing. Every owner path reports failure, since the depot did not acquire anything new.
// EXACT (108 B incl. the trailing 20-byte jump table). Three source shapes were load-bearing:
// (1) the OWNER path is the fall-through and the no-owner path the out-of-line `else` -- the
// documented branch-order lever, worth DIFF 55 -> 8 on its own; (2) the switch arms `break` to a
// shared `return bAcquired` rather than each spelling `return 0` -- all three exits are
// `mov al,bl`, never `xor al,al`, so the variable is what is returned everywhere (cl then
// duplicates the 3-instruction epilogue itself); (3) `case 2` is written BEFORE `case 1/4/5`,
// which is the order cl lays the two arm bodies out in -- source order, not label order, decides
// it. ⚠ Judge this one by the rows above the jump table only: the table's entries are
// relocations on our side and real addresses in the image, so a headline byte_diff of ~26 over
// the full 108 is masked-table noise, not disagreement (CLAUDE.md's `--len`/jump-table trap).
unsigned char TrackDepotTileObjResetView0x412940::ResetToBaseSubFrameMaybe() // TODO: sync (TU-local view)
{
    unsigned char bAcquired = 0;
    PeerTrainNodePartial *pOwner = (PeerTrainNodePartial *)pOwningTrain;

    if (pOwner != NULL) {
        switch (pOwner->dwModeBMaybe) {
        case 2:
            pOwner->ClaimDecorObjMaybe(this, 0);
            break;
        case 1:
        case 4:
        case 5:
            pOwner->ReverseDirectionGuardedMaybe();
            break;
        }
    } else if (bClaimLockedFlag == 0 &&
               g_PeerTrainSlotQueue.SpawnOrAssignRandomTrain(this, NULL) != 0) {
        bAcquired = 1;
    }
    return bAcquired;
}

// FUNCTION: LOCO 0x412af0
// The DEPOT-side twin of TrackTileObj::EnqueueClaimWaiterMaybe: append pTrain to the tail of
// this depot tile's own +0x124 claim-wait FIFO. Unlike the connector half this one is a pure
// list append -- it does not touch the train's sentinel pair, mode or sound state, because its
// sole caller (PeerTrainNode::ClaimDecorObjMaybe) has already done all three.
//
// The two halves lower differently, and the difference IS the source: this one walks to the
// tail BEFORE allocating (so the walk is a plain for-loop leaving `pTail` NULL on an empty
// queue, and the empty case falls out of the SAME `if` rather than needing an inverted test),
// where the connector half allocates first and branches on the head. Writing this one in the
// connector's shape does not match. The two `pNew` field stores are duplicated into both arms
// by cl's own tail duplication, not by the source.
void TrackDepotTileObj::KeyedList_AddByKeyMaybe(PeerTrainNodePartial *pTrain)
{
    TrainClaimWaitNode *pTail = 0;
    for (TrainClaimWaitNode *pNode = pClaimWaitQueueHeadMaybe; pNode != 0; pNode = pNode->pNext) {
        pTail = pNode;
    }

    TrainClaimWaitNode *pNew = new TrainClaimWaitNode;
    if (pTail != 0) {
        pTail->pNext = pNew;
    } else {
        pClaimWaitQueueHeadMaybe = pNew;
    }
    pNew->pTrain = pTrain;
    pNew->pNext = 0;
}

// FUNCTION: LOCO 0x412b50
// The removal twin of KeyedList_AddByKeyMaybe: walk this depot tile's +0x124 claim-wait FIFO
// and unlink the first node whose train matches (wTrainId, bOwner), freeing the wrapper node
// (the train itself is not owned by the queue). Returns whether anything was removed; an empty
// queue and a no-match walk both read as 0.
// Three levers are baked in, do not undo. (a) `bRemoved = 1; break;` rather than an early
// `return 1`: the single exit is what gives the accumulator a stack home ([esp+0x13]) and the
// original's `mov al,[esp+0x13]` reload on the exhausted-walk path -- an early return keeps it
// in registers and lands 120 B / one instruction short. (b) `pNext` is hoisted out of the
// unlink if/else, matching the original's single `mov eax,[edx+4]` (the un-hoisted form emits
// the load twice). (c) pPrev is declared BEFORE bRemoved (CODEGEN #81): the reverse order swaps
// the `xor edi,edi` / `xor al,al` pair and costs exactly 4 bytes at an otherwise byte-identical
// 57/57.
unsigned char TrackDepotTileObj::KeyedList_RemoveByKeyMaybe(unsigned int wTrainId,
                                                            unsigned char bOwner) {
    TrainClaimWaitNode *pPrev = NULL;
    unsigned char bRemoved = 0;
    TrainClaimWaitNode *pNode = this->pClaimWaitQueueHeadMaybe;

    while (pNode != NULL) {
        PeerTrainNodePartial *pTrain = pNode->pTrain;
        if (pTrain->wTrainId == wTrainId && pTrain->bOwnerByteA == bOwner) {
            TrainClaimWaitNode *pNext = pNode->pNext;
            if (pPrev != NULL) {
                pPrev->pNext = pNext;
            } else {
                this->pClaimWaitQueueHeadMaybe = pNext;
            }
            delete pNode;
            bRemoved = 1;
            break;
        }
        pPrev = pNode;
        pNode = pNode->pNext;
    }
    return bRemoved;
}

// FUNCTION: LOCO 0x44f3a0
// Park pTrain on this tile's claim-wait FIFO: stamp the tile's own grid position into the
// train's sentinel pair, drop it into dwModeAMaybe 2 and sound state 0, then `new` an 8-byte
// TrainClaimWaitNode and append it to the +0x124 tail. The producer whose consumer is
// PumpClaimWaitQueueMaybe; the depot-side twin is TrackDepotTileObj::KeyedList_AddByKeyMaybe.
//
// ⚠ The downcast is real, not a modelling shortcut. pClaimWaitQueueHeadMaybe lives at +0x124,
// which is PAST this base's own end: `new TrackTileObj` allocates 0x11c (ground-truthed at the
// factory's own allocation site, 0x41de4a in NetSessionEventQueue::BigObj_CreateAndInsert),
// against 0x128 for the connector leaf and 0x12c for the depot leaf. So the queue head is a
// LEAF field, and this body -- whose `.text` neighbours are all connector members -- is really a
// TrackConnectorTileObj member that has to be REACHED through the base, because neither caller
// knows the leaf type (see the declaration's own note in src/TilePlacedObj.h). The cast is safe
// for the depot leaf too: both leaves carry the identical +0x11c/+0x120/+0x124 triple.
//
// Three source levers, each worth one compile, all found by bisecting DIFF down from 52:
//   (a) the return type is `unsigned char`, not `void` -- see the declaration's note;
//   (b) the empty-list case is the `else`, not the `if`. The original tests `head != 0` and
//       sinks the set-head store to an out-of-line tail block (`je` forward, past the epilogue);
//       writing it as `if (head == 0)` inverts the block layout and costs DIFF(47 -> 15);
//   (c) the sentinel-pair copy must be written BEFORE `dwModeAMaybe = 2`, even though the
//       original EMITS the +0x60 store first. With the statements the other way round cl
//       hoists that store above the call's own `push 0`/`mov ecx,esi` setup instead of
//       scheduling it into the setup the way the original does. Last 15 bytes of diff.
unsigned char TrackTileObj::EnqueueClaimWaiterMaybe(PeerTrainNodePartial *pTrain)
{
    *(unsigned int *)&pTrain->wSentinelCMaybe = *(unsigned int *)&this->pos.wPosX;
    pTrain->dwModeAMaybe = 2;
    pTrain->SetSoundStateMaybe(0);

    TrainClaimWaitNode *pNode = new TrainClaimWaitNode;
    pNode->pTrain = pTrain;
    pNode->pNext = 0;

    TrackConnectorTileObj *pTile = (TrackConnectorTileObj *)this;
    TrainClaimWaitNode *pTail = pTile->pClaimWaitQueueHeadMaybe;
    if (pTail != 0) {
        while (pTail->pNext != 0) {
            pTail = pTail->pNext;
        }
        pTail->pNext = pNode;
    } else {
        pTile->pClaimWaitQueueHeadMaybe = pNode;
    }
    return 0;
}

// FUNCTION: LOCO 0x44f410
// The connector-side twin of TrackDepotTileObj::KeyedList_RemoveByKeyMaybe above: same walk,
// same (wTrainId, bOwnerByteA) key, same unlink-and-free. It differs from the depot half in
// exactly two source-visible ways, and both are load-bearing: (a) this one returns from inside
// the loop where the depot half sets an accumulator and breaks, so it needs no stack home for
// the result and keeps `this` in ecx throughout (123 B against the depot's 125); (b) it spells
// `pNode->pNext` out at both unlink sites instead of hoisting it into a temp, which is why the
// original emits the `mov [eax+4]` load twice here and once there. Two hand-written near-copies
// that drifted apart in two small ways -- which is what a real twin pair looks like.
unsigned char TrackConnectorTileObj::RemoveClaimWaiterMaybe(unsigned int wTrainId,
                                                            unsigned char bOwner) {
    TrainClaimWaitNode *pPrev = NULL;
    TrainClaimWaitNode *pNode = this->pClaimWaitQueueHeadMaybe;

    while (pNode != NULL) {
        PeerTrainNodePartial *pTrain = pNode->pTrain;
        if (pTrain->wTrainId == wTrainId && pTrain->bOwnerByteA == bOwner) {
            if (pPrev != NULL) {
                pPrev->pNext = pNode->pNext;
            } else {
                this->pClaimWaitQueueHeadMaybe = pNode->pNext;
            }
            delete pNode;
            return 1;
        }
        pPrev = pNode;
        pNode = pNode->pNext;
    }
    return 0;
}
