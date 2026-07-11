// PeerTrainSlotQueueMaybe method bodies. See the header / docs/subsystems.md for the class.
#include "PeerTrainSlotQueueMaybe.h"

#include <stdio.h>               // _snprintf -- the train-name format
#include <stdlib.h>              // rand

#include "CarKindDesc.h"          // MapCarTypeIdToCategoryMaybe
#include "CarNetObj.h"            // CarNetObj -- what carSlots really points at
#include "DPlaySessionMgr.h"      // g_pDPlaySessionMgr->connectionMode
#include "DecorActor.h"           // WalkerActor::LeaveTrainMaybe
#include "EffectSpawner.h"        // DAT_004fd220 -- the derail effect spawn
#include "GameNetMsgQueue.h"      // g_nScreenState -- DrainPendingSlotsMaybe's on-screen gate
#include "LocalPlayerIdentity.h"  // g_pLocalPlayerIdentity->name
#include "LocoBitmap.h"           // LocoBitmap::IsPixelTransparentAtMaybe
#include "NetSessionEventQueue.h" // g_NetSessionEventQueue::PickRandomBigObjByCategory
#include "NameAnchorMaybe.h"      // NameAnchorMaybe::pTileMaybe / nAnchorYMaybe
#include "TilePlacedObj.h"        // TrackConnectorTileObj / TrackDepotTileObj claim fields
#include "PeerTrainNode.h"        // PeerTrainNodePartial's own fields
#include "PlacementCursorMaybe.h" // PlacementCursorMaybe_004854c8 singleton
#include "WorldActionCursor.h"    // SelectedObjWidgetMaybe_004852a0 singleton
#include "WorldBoardMaybe.h"      // g_worldBoard::GetPlaneASlotMaybe

// Local METHODS-ONLY view giving DrainPendingSlotsMaybe its per-slot tick call (0x44c3a0).
// PeerTrainNode.cpp's own PeerTrainNodeTickPartial stays TU-local there: moving it into
// src/NameAnchorMaybe.h so this TU could see it flips src/TilePlacedObj.cpp's
// SpawnSeqRecordEffectMaybe (0x4588b0) off its 143-byte EXACT (measured -- same
// declaration-parity dial family as WorldBoardMaybe.cpp's 0x457ce0, and the reason the v492
// "byte-neutral" measurement no longer holds).
struct PeerTrainNodeDrainTickPartial : PeerTrainNodePartial {
    void PeerTrainNode_UpdatePlacementTickMaybe(); // 0x44c3a0
};

// FUNCTION: LOCO 0x44dbb0
// Ex-SmallDim::IsOutOfBox (phase2_probe5.cpp) -- owner resolved 2026-07-22 (v322) via its
// only caller, WorldActionCursor::RefreshTrainCouplingMenuMaybe (0x4597e0), which invokes it
// on &DAT_004a98b0, the confirmed PeerTrainSlotQueueMaybe singleton. Bounds sanity check on
// the two counters against the 4-deep slot array. Semantics are inverted from the naive
// reading: the fallthrough (both counters in range) returns 0, the jump target (either
// counter out of range) returns 1 -- i.e. this reports "out of bounds", not "fits".
unsigned char PeerTrainSlotQueueMaybe::IsSlotCountOutOfRangeMaybe() {
    if (nActiveCount >= 4 || nOwnedCountMaybe >= 3) return 1;
    return 0;
}

// FUNCTION: LOCO 0x44e2d0
// Ex-ArrObj0x44e2d0::ClearArr (phase2_probe2.cpp) -- owner resolved 2026-07-22 (v322)
// via its sole caller, WorldBoardMaybe_UpdateDirtyTilesMaybe (0x456150), which invokes
// it on &DAT_004a98b0 (raw disasm: `mov ecx,0x4a98b0; call`) -- this singleton. Zeroes
// the 16-dword sort-scratch buffer at +0x18 (the +0x18..+0x58 tail of the 0x58-byte
// object, matching docs/subsystems.md's 16-entry CarNetObj* scratch note).
void PeerTrainSlotQueueMaybe::ClearSortScratchMaybe() {  // TODO: sync
    for (int i = 0; i < 16; i++) {
        aSortScratchMaybe[i] = 0;
    }
}

// The singleton's own vtable (0x478370, one slot: the scalar deleting dtor). Modeled as a
// `void *pVtbl` field rather than a real C++ vtable -- see the header -- so the ctor below has to
// store it by hand, exactly as src/NameAnchorMaybe.cpp does for 0x477564.
extern void *g_vtable0x478370[];

// FUNCTION: LOCO 0x44d800 (Ghidra: PeerTrainSlotQueueMaybe::TrackTileObjTypeSet1234Maybe -- a
// stale auto-name, this is the constructor)
// The constructor, and it is ResetAllFields below plus the vtable store: the original's two
// bodies are the same code with the same LEA hoist, which is exactly what you would expect if
// the author wrote the ctor first and then split the reset half out (or vice versa) rather than
// having one call the other. Reproduced as two separate bodies for that reason -- writing the
// ctor as `pVtbl = ...; ResetAllFields();` would emit a call the original does not have.
//
// EFFECTIVE MATCH -- asmscore --len 45: total 24010, align=24 **reg_pen=0 identity_miss=0**
// byte_diff=10, insns 16/16. Every instruction, operand and register agrees; only two are placed
// differently, and both are the SAME rep-stosd setup schedule ResetAllFields below is already
// parked on: the original interleaves `lea edi,[edx+0x18]` between the +0xc and +0x10 stores
// (and puts `xor eax,eax` before the `push edi`), this build groups the LEA with the `mov
// ecx,0x10` that precedes the whole store run. The address calc depends on nothing in the store
// run, so an /O2 scheduler is free to place it anywhere -- Yoda lesson #15's class, and not
// source-steerable. Measured and REFUTED this session: `memset(aSortScratchMaybe, 0,
// sizeof(aSortScratchMaybe))` in place of the 16-entry loop, and hoisting `int i` out of the
// `for` -- both bit-for-bit inert (the loop is recognized and lowered to `rep stosd` either way).
// Do not re-probe on that axis; see ResetAllFields' own note for the reordering attempts.
// PARKED (docs/PARKED.md).
PeerTrainSlotQueueMaybe::PeerTrainSlotQueueMaybe() {
    pVtbl = g_vtable0x478370;
    nActiveCount = 0;
    nOwnedCountMaybe = 0;
    aSlots[0] = 0;
    aSlots[1] = 0;
    aSlots[2] = 0;
    aSlots[3] = 0;
    for (int i = 0; i < 16; i++) {
        aSortScratchMaybe[i] = 0;
    }
}

// FUNCTION: LOCO 0x44d870
// PARKED (v3): orig hoists `lea edi,[ecx+0x18]` (the array base) to right after the m_8
// store, 3 instructions before the m_c/m_10/m_14 stores -- the rep-stosd loop itself still
// runs last. Tried moving the whole loop earlier (between m_8 and m_c): DIFF grew from 8 to
// 40, much worse (loses the recognized-loop codegen entirely). Reverted; this reads as pure
// LEA scheduling (address calc has no dependency on the later stores, so an /O2 scheduler is
// free to hoist just the LEA) -- same class as Yoda lesson #15, not source-steerable via
// statement reordering.
// (Ghidra attributes this to DAT_004a98b0 as ResetObj0x44d870::ResetAllFields, and both call
// sites -- Config_SaveWindowAndCleanExit 0x407860, AppWindow::AbortMultiplayerSession
// 0x406e80 -- invoke it on the PeerTrainSlotQueueMaybe singleton right next to that
// singleton's other methods, so it is a PeerTrainSlotQueueMaybe member. The two leading
// shorts stay separate 16-bit stores; the 4 slot pointers stay unrolled; only the 16-entry
// run becomes `rep stosd`. Moved here from NetSessionEventQueue.cpp 2026-07-22, v323, when
// the duplicate partial structs were consolidated into the shared header.)
void PeerTrainSlotQueueMaybe::ResetAllFields() {
    nActiveCount = 0;
    nOwnedCountMaybe = 0;
    aSlots[0] = 0;
    aSlots[1] = 0;
    aSlots[2] = 0;
    aSlots[3] = 0;
    for (int i = 0; i < 16; i++) {
        aSortScratchMaybe[i] = 0;
    }
}

// FUNCTION: LOCO 0x44d8a0
// Find the slot whose train matches (wTrainId, bOwner) and get it off the board: if the
// slot's lead car is the current selection it is deselected first, the peer handoff is
// reconciled, then the node is either deleted outright (nDiscardFlag == 0 -- the
// nOwnedCountMaybe decrement says this is the "release one of our own" path), handed to the
// peer that asked for it (joined mode + bFlag), or just deleted. Returns 0 when no slot
// matched, 1 once the slot has been cleared.
//
// The `if (pNode != 0)` guards around the ScalarDeletingDtor calls are the compiler's own
// `delete` expansion (vtable slot 0, arg 1), reproduced via the probe per the
// PeerTrainNodeVtblProbe precedent; pNode is always non-null past the search loop.
//
// EFFECTIVE -- DIFF(5) at 269/269 bytes, insns 103/103. Structure and register allocation
// are byte-identical (including the three cross-jumped `*ppSlot=0; nActiveCount--; return 1`
// tail copies); the entire residual is ONE scheduling tie-break in the three-predecessor
// shared tail: the original emits `mov al,1` right after `dec word [edi+4]`, this compile
// sinks it between `pop ebp` and `pop ebx` (plus the two branch displacements that shift
// with it). The search loop keeps the match-scan variable loop-local and re-fetches the
// winning slot afterwards (`pNode = aSlots[i]`) -- that re-fetch is what reproduces the
// original's `and ecx,0xff; mov esi,[edi+ecx*4+8]` rematerialization instead of carrying the
// scan pointer across the break.
char PeerTrainSlotQueueMaybe::ReleaseOrForwardMatchingSlotMaybe(unsigned int wTrainId, unsigned char bOwner, char bFlag) {
    unsigned char i = 0;
    while (true) {
        PeerTrainNodePartial *pScan = aSlots[i];
        if (pScan != 0 && pScan->wTrainId == wTrainId && pScan->bOwnerByteA == bOwner) {
            break;
        }
        i++;
        if (i < 4) {
            continue;
        }
        return 0;
    }
    PeerTrainNodePartial *pNode = aSlots[i];
    PeerTrainNodePartial **ppSlot = &aSlots[i];
    if (SelectedObjWidgetMaybe_004852a0.pSelectedObjMaybe == (AnimDescRefObj0x477488 *)pNode->carSlots[0]) {
        SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(0);
        g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(0);
    }
    g_pDPlaySessionMgr->ReconcileCarHandoff(pNode);
    if (pNode->nDiscardFlag == 0) {
        nOwnedCountMaybe--;
        if (pNode != 0) {
            ((PeerTrainNodeVtblProbe *)pNode)->ScalarDeletingDtor(1);
        }
    } else {
        if (g_pDPlaySessionMgr != 0 && g_pDPlaySessionMgr->connectionMode == 2 && bFlag != 0) {
            g_pDPlaySessionMgr->RequestTrainMoveOrReleaseNode(0, 0, pNode);
        } else if (pNode != 0) {
            ((PeerTrainNodeVtblProbe *)pNode)->ScalarDeletingDtor(1);
        }
    }
    *ppSlot = 0;
    nActiveCount--;
    return 1;
}

// DAT_00485268 / DAT_0048526c -- two COM-ish objects TeardownAllSlotsMaybe releases and
// NULLs (the push-this / `call [vtbl+8]` / __stdcall slot-2 Release shape). Nothing else
// in .text references either global, so the pointee type is unpinned -- modeled as the
// minimal 3-slot IUnknown-shaped probe, same convention as the family's other vtable probes.
struct ComObj0x485268Maybe {
    virtual void __stdcall _v0();
    virtual void __stdcall _v1();
    virtual void __stdcall Release();
};
ComObj0x485268Maybe *g_pComObj0x485268Maybe; // DAT_00485268 // TODO: sync
ComObj0x485268Maybe *g_pComObj0x48526cMaybe; // DAT_0048526c // TODO: sync

// FUNCTION: LOCO 0x44d9b0
// Full-board teardown: every occupied train slot gets ReleaseSlotsForOwnerMaybe's
// unboard/detach/release tail (no owner filter), then the two teardown COM-ish globals
// are released and NULLed. Called on the singleton by WorldBoardMaybe_ResetAllTilesMaybe
// (0x454fe0) and AppWindow's SaveWindowAndCleanExit (via the __fastcall escape hatch there).
// Ghidra hangs a spurious `param_1` on the signature (the function is a plain `ret`) --
// same artifact CollectAndSortVisibleCarsMaybe's header note calls out.
void PeerTrainSlotQueueMaybe::TeardownAllSlotsMaybe() {
    PeerTrainNodePartial **ppSlot = aSlots;
    int nSlotCount = 4;
    do {
        if (*ppSlot != 0) {
            int *pPassenger = (*ppSlot)->apPassengerMaybe;
            for (int j = 0; j < 8; j++) {
                if (pPassenger[j] != 0) {
                    ((WalkerActor *)pPassenger[j])->LeaveTrainMaybe();
                    pPassenger[j] = 0;
                }
            }
            DetachFromBoardMaybe(*ppSlot);
            ReleaseOrForwardMatchingSlotMaybe((*ppSlot)->wTrainId, (*ppSlot)->bOwnerByteA, 1);
        }
        ppSlot++;
        nSlotCount--;
    } while (nSlotCount != 0);
    if (g_pComObj0x485268Maybe != 0) {
        g_pComObj0x485268Maybe->Release();
        g_pComObj0x485268Maybe = 0;
    }
    if (g_pComObj0x48526cMaybe != 0) {
        g_pComObj0x48526cMaybe->Release();
        g_pComObj0x48526cMaybe = 0;
    }
}

// FUNCTION: LOCO 0x44e800
// Every passenger riding a train has to be put back on the ground before the train node can be
// re-placed or destroyed; LeaveTrainMaybe is the walker's own half of that (it scans this same
// array for itself), so the slot has to be cleared here too or the pair would disagree.
//
// `this` is never touched -- Ghidra reads the function as __stdcall(pNode) for that reason --
// but it IS a member: cl emits the thiscall entry either way, and every call site loads
// ecx with the singleton.
void PeerTrainSlotQueueMaybe::FreeQueuedTrainCarSlots(PeerTrainNodePartial *pNode) {
    for (int i = 0; i < 8; i++) {
        if (pNode->apPassengerMaybe[i] != 0) {
            ((WalkerActor *)pNode->apPassengerMaybe[i])->LeaveTrainMaybe();
            pNode->apPassengerMaybe[i] = 0;
        }
    }
}

// FUNCTION: LOCO 0x44dbd0
// The four mode values that pass the filter -- 0, 1, 4, 5 -- are the ones in which a train is
// on the board and idle enough to be told its cars are ready; the ones that do NOT pass are the
// mid-manoeuvre states, where flipping the flag would fight whatever the node is doing.
void PeerTrainSlotQueueMaybe::DispatchActiveSlotsMaybe(bool bReady) {
    for (int i = 0; i < 4; i++) {
        PeerTrainNodePartial *pNode = aSlots[i];
        if (pNode != 0 &&
            (pNode->dwModeBMaybe == 0 || pNode->dwModeBMaybe == 4 ||
             pNode->dwModeBMaybe == 5 || pNode->dwModeBMaybe == 1)) {
            pNode->SetCarsReady(bReady);
        }
    }
}

// FUNCTION: LOCO 0x44e160
// "Is any board train standing on this tile?" -- the lead anchor of each occupied slot first,
// then every car's A- and B-side anchor. Used to veto an operation that would disturb a tile a
// train is currently occupying.
//
// The car walk is `j <= wCarSlotCount`, the same deliberate inclusive bound documented on
// SelectCarAtPositionMaybe below: carSlots[0] is the locomotive and the count covers only the
// cars behind it. Note the cars are dereferenced UNGUARDED -- unlike SelectCarAtPositionMaybe,
// which null-checks each one -- so the two disagree about whether a slot within the count can
// be empty. That asymmetry is the original's, reproduced as found.
//
// EFFECTIVE -- DIFF(68) at 155/160 bytes, insns 62/62, align=56, reg_pen=5. Structure is exact;
// the entire residual is ONE allocation coin-flip and its cascade: the original keeps bFound in
// AL (so `return bFound` needs no move) and SPILLS the loop counter to [esp+0x10]; this compile
// does the reverse, keeping i in EAX and spilling bFound to [esp+0x13], which then costs an
// extra `xor eax,eax` and two `mov al,cl` returns. Same zero-register-rotation class as
// WorldBoardMaybe.cpp's 0x457ce0 (docs/PARKED.md).
//
// The exit shape IS source-steerable and is already paid for: spelling the lead-anchor hit as
// `bFound = 1; break;` rather than `return 1;` -- so that EVERY path returns the variable --
// took the score from 92725 to 56626 by itself. Three spellings of the loop guard then compile
// BYTE-IDENTICALLY (`i < 4 && !bFound`, a leading `if (bFound) break;`, and both together), so
// that choice is free; the clearest one is kept.
char PeerTrainSlotQueueMaybe::IsTileOccupiedByAnyTrainMaybe(TrackTileObj *pTile) {
    char bFound = 0;
    for (unsigned short i = 0; i < 4 && !bFound; i++) {
        PeerTrainNodePartial *pNode = aSlots[i];
        if (pNode != 0) {
            if (pNode->pNameMaybe != 0 && pNode->pNameMaybe->pTileMaybe == pTile) {
                bFound = 1;
                break;
            }
            for (int j = 0; j <= (int)pNode->wCarSlotCount; j++) {
                CarNetObj *pCar = (CarNetObj *)pNode->carSlots[j];
                if (pCar->pNameAMaybe->pTileMaybe == pTile ||
                    pCar->pNameBMaybe->pTileMaybe == pTile) {
                    bFound = 1;
                    break;
                }
            }
            if (bFound) break;
        }
    }
    return bFound;
}

// FUNCTION: LOCO 0x44e200
// Rebuild the draw list: collect every car of every occupied slot whose two per-car mode fields
// say it is on screen and settled (dwModeBMaybe of 2 or 5, or dwModeAMaybe of 2, mean mid-extend
// -- those are skipped), then bubble-sort the collected run by the A-anchor's screen Y so the
// caller paints back-to-front. The buffer is 16 entries and the sort's guard is a NULL, which is
// why ClearSortScratchMaybe has to run first.
//
// Takes NO parameter: the original ends in a plain `ret`, not `ret 0x4` (Ghidra infers a
// spurious `param_1`).
//
// PARTIAL -- DIFF(175) at 206/208 bytes, insns 75/77, align=120, reg_pen=28. Every basic block
// and every test is present and in the original's order; what differs is register assignment
// across the whole body plus two instructions' worth of `this` handling. The original RELOADS
// `this` from [esp+0x14] at the top of each outer iteration and jumps over that reload on the
// first pass (`jmp` into the loop body) -- the same re-read idiom SelectCarAtPositionMaybe
// below depends on -- whereas this compile keeps `this` live in a register throughout. Nothing
// at source level reaches that: both spellings of the outer guard (`if (nActiveCount == 0)
// return;` and Ghidra's wrapping `if (nActiveCount != 0) { ... }`) compile BYTE-IDENTICALLY,
// so the early return is kept as the clearer of the two. v501: the #53 subscript-induction
// idiom probed and REFUTED in all three splits (sort loop only DIFF(179), collection loops
// only DIFF(176), both DIFF(178) -- every form at or worse than the pointer-walk baseline),
// so the pointer walks stay; the residual is the `this`-reload class, not #53.
void PeerTrainSlotQueueMaybe::CollectAndSortVisibleCarsMaybe() {
    int nCollected = 0;
    if (nActiveCount == 0) {
        return;
    }
    PeerTrainNodePartial **ppNode = aSlots;
    int nSlots = 4;
    do {
        if (*ppNode != 0) {
            CarNetObj **ppOut = &aSortScratchMaybe[nCollected];
            for (int j = 0; j <= (int)(*ppNode)->wCarSlotCount; j++) {
                CarNetObj *pCar = (CarNetObj *)(*ppNode)->carSlots[j];
                if (pCar->dwModeBMaybe != 2 && pCar->dwModeBMaybe != 5 &&
                    pCar->dwModeAMaybe != 2) {
                    *ppOut = pCar;
                    nCollected++;
                    ppOut++;
                }
            }
        }
        ppNode++;
        nSlots--;
    } while (nSlots != 0);
    if (nCollected != 0) {
        char bSwapped;
        do {
            bSwapped = 0;
            CarNetObj **pp = aSortScratchMaybe;
            for (int i = 0; i < 15; i++) {
                pp++;
                CarNetObj *pNext = *pp;
                if (pNext == 0) break;
                CarNetObj *pPrev = pp[-1];
                if (pPrev->pNameAMaybe->nAnchorYMaybe > pNext->pNameAMaybe->nAnchorYMaybe) {
                    pp[-1] = pNext;
                    bSwapped = 1;
                    *pp = pPrev;
                }
            }
        } while (bSwapped);
    }
}

// FUNCTION: LOCO 0x44e2e0
// The draw half of CollectAndSortVisibleCarsMaybe: paint every collected car that overlaps this
// one 16x16 tile box, in the back-to-front order the sort left behind. The scratch buffer's NULL
// terminator is the real bound; the 16 is only the array's own size.
//
// The plane-1-only forward scan is the interesting part. Having drawn a car, it looks ahead for
// the first LATER entry that belongs to the SAME train (`pOwnerTrainNodeMaybe` equal) and whose
// own plane field is 0, and draws that one too -- one extra slice, then `break`. That is what
// keeps a coupled neighbour sitting across the tile seam from being clipped away when only the
// near car passes the hit test.
//
// rcTile arrives BY VALUE (`ret 0x14` = 16 for the RECT + 4 for the short) and is forwarded
// verbatim to BlitTileSliceMaybe, which re-derives right/bottom itself. The fourth argument to
// HitTestTileMaybe is the owning train's bUnk0x2c -- a dead parameter on the callee side, see
// the note on its declaration in src/CarNetObj.h.
//
// EFFECTIVE -- DIFF(48) at 266/272 bytes, insns 88/89, align=58, reg_pen=2. Two levers are
// already paid for and must NOT be undone:
//   * the car must be re-read as `(*ppCar)->` at every use rather than cached in a loop-local
//     (score 130107 -> 78267, insns 96 -> 88). Same lever, same direction, as
//     SelectCarAtPositionMaybe below -- that makes it twice in this one TU;
//   * HitTestTileMaybe's dead 4th parameter had to be retyped `short` -> `unsigned char`
//     (78267 -> 58257). See src/CarNetObj.h; a caller pinned a callee's parameter type.
// Refuted probe: spelling the outer loop `do { ... } while (i++ < 16)` to reproduce the
// original's compare-on-the-pre-increment-value (`inc eax; mov; dec eax; cmp; jl`) makes it
// MUCH worse (58257 -> 119062) by dragging the null check up into the prologue -- the `for`
// stays. What is left is one stack-slot assignment coin-flip (the original puts the counter at
// [esp+0x10] and the walk pointer at [esp+0x14]; this compile swaps them, and hoisting the
// counter's declaration does not move it) plus the single instruction that tail implies.
void PeerTrainSlotQueueMaybe::DrawVisibleCarsInTileMaybe(RECT rcTile, short nPlaneMaybe) {
    CarNetObj **ppCar = aSortScratchMaybe;
    for (int i = 1; i <= 16; i++) {
        if (*ppCar == 0) {
            return;
        }
        if ((*ppCar)->HitTestTileMaybe(rcTile.left, rcTile.top, nPlaneMaybe,
                                       (*ppCar)->pOwnerTrainNodeMaybe->bUnk0x2c)) {
            (*ppCar)->BlitTileSliceMaybe(rcTile);
            if (nPlaneMaybe == 1) {
                CarNetObj **ppOther = ppCar;
                for (int j = i; j < 16; j++) {
                    CarNetObj *pOther = ppOther[1];
                    if (pOther == 0) break;
                    if (pOther->pOwnerTrainNodeMaybe == (*ppCar)->pOwnerTrainNodeMaybe &&
                        pOther->wUnk0x448 == 0 &&
                        pOther->HitTestTileMaybe(rcTile.left, rcTile.top, 0,
                                                 pOther->pOwnerTrainNodeMaybe->bUnk0x2c)) {
                        aSortScratchMaybe[j]->BlitTileSliceMaybe(rcTile);
                        break;
                    }
                    ppOther++;
                }
            }
        }
        ppCar++;
    }
}

// FUNCTION: LOCO 0x44e630
// Unhook a train from every board-side cache that still points at it, so the node can be
// released or re-placed without leaving dangling back-references. In order: stop the cars, give
// back the CONNECTOR tile the train was crossing (dropping its claim-wait entry first if the
// train was still queued for it), give back the DEPOT tile it had claimed (same, plus clearing
// the tile's own owner/lock/claim triple through the shared slot-7 release virtual), then hand
// every anchor -- the lead one and both sides of every car -- back to its tile.
//
// The two grid-position pairs are both re-armed to the {-1,-1} "unset" sentinel on the way out,
// and both lookups add 1 to the Y before asking the board: the stored row is the train's own,
// the tile it occupies is the row below.
//
// The `+ 1` on wCarSlotCount's bound is the same inclusive walk documented on
// IsTileOccupiedByAnyTrainMaybe -- slot 0 is the locomotive and the count covers the towed cars
// -- and the FIRST of the two loops dereferences the slot unguarded while the second null-checks
// it, exactly the asymmetry that function has. Reproduced as found.
//
// Ghidra reads this as __stdcall(pNode) (`this` is never touched) and files it under
// PeerTrainNode; every known call site loads ecx with THIS singleton, so it is modeled as a
// member for the same reason FreeQueuedTrainCarSlots is.
void PeerTrainSlotQueueMaybe::DetachFromBoardMaybe(PeerTrainNodePartial *pNode)
{
    if (pNode == 0) return;
    pNode->SetCarsReady(false);

    TrackConnectorTileObj *pConnector = (TrackConnectorTileObj *)g_worldBoard.GetPlaneASlotMaybe(
        pNode->wSentinelCMaybe, pNode->wSentinelDMaybe + 1, 0);
    if (pConnector != 0) {
        switch (pNode->dwModeAMaybe) {
        case 2:
        case 3:
            pConnector->RemoveClaimWaiterMaybe(pNode->wTrainId, pNode->bOwnerByteA);
            // fall through -- a queued train also has to clear the claim itself
        case 0:
        case 1:
        case 4:
        case 5:
            pConnector->dwOccupancyClaim = 0;
            break;
        }
    }
    pNode->wSentinelCMaybe = -1;
    pNode->wSentinelDMaybe = -1;

    switch (pNode->dwModeBMaybe) {
    case 1:
    case 2:
    case 4:
    case 5: {
        TrackDepotTileObj *pDepot = (TrackDepotTileObj *)g_worldBoard.GetPlaneASlotMaybe(
            pNode->wClaimedPosXMaybe, pNode->wClaimedPosYMaybe + 1, 0);
        if (pDepot != 0) {
            if (pNode->dwModeBMaybe == 2) {
                pDepot->KeyedList_RemoveByKeyMaybe(pNode->wTrainId, pNode->bOwnerByteA);
            }
            if (pDepot->pOwningTrain == pNode) {
                pDepot->pOwningTrain = 0;
                pDepot->bClaimLockedFlag = 0;
                pDepot->ReleaseChannelAndDispatch(0);
                pDepot->dwOccupancyClaim = 0;
            }
        }
        break;
    }
    }
    pNode->wClaimedPosXMaybe = -1;
    pNode->wClaimedPosYMaybe = -1;

    TrackTileObj *pLeadTile = pNode->pNameMaybe->pTileMaybe;
    if (pLeadTile != 0 && pLeadTile->dwTrackState == 7) {
        pLeadTile->ReleaseChannelAndDispatch(1);
    }
    for (int i = 0; i <= (int)pNode->wCarSlotCount; i++) {
        TrackTileObj *pTileA = ((CarNetObj *)pNode->carSlots[i])->pNameAMaybe->pTileMaybe;
        if (pTileA != 0 && pTileA->dwTrackState == 7) {
            pTileA->ReleaseChannelAndDispatch(1);
        }
        TrackTileObj *pTileB = ((CarNetObj *)pNode->carSlots[i])->pNameBMaybe->pTileMaybe;
        if (pTileB != 0 && pTileB->dwTrackState == 7) {
            pTileB->ReleaseChannelAndDispatch(1);
        }
    }
    for (int j = 0; j <= (int)pNode->wCarSlotCount; j++) {
        if (pNode->carSlots[j] != 0) {
            ((CarNetObj *)pNode->carSlots[j])->pNameAMaybe->ReleaseTileClaimMaybe();
            ((CarNetObj *)pNode->carSlots[j])->pNameBMaybe->ReleaseTileClaimMaybe();
        }
    }
    if (pNode->pNameMaybe != 0) {
        pNode->pNameMaybe->ReleaseTileClaimMaybe();
    }
}

// FUNCTION: LOCO 0x44dc10
// Put a train into the first free board slot, building it two different ways depending on whether
// the caller supplied a consist. With pTrainRecord NULL (the screen-saver / idle-traffic path) the
// train is rolled at random: a locomotive from the three 0x1804/0x1806/0x1808 kinds, then 0..4
// extra cars, each independently drawn from one of three families -- passenger (category 2, the
// three 0x1866/0x1868/0x186a kinds), freight (category 3, the two 0x186c/0x186e kinds), or the
// single caboose kind 0x1870 (category 4). With a record supplied (a loaded layout, or the
// train-config menu's four-entry selection) entry [0] is the locomotive kind and [1..3] the towed
// slots, 0 meaning "leave empty" -- the same anSelection layout
// RebuildCarSlotsFromSelectionMaybe consumes.
//
// Either way the new node is named `"<local player name> <trainId>"` through car 0's own
// SetName vtable slot, and is rejected outright (destroyed, slot re-cleared) if car 0 failed to
// come up valid -- a kind whose descriptor never loaded.
//
// The two parameters are left `void *`: both call sites pass something different (
// src/NetSessionEventQueue.cpp hands in a `TrainPlacementRecord *` whose leading four ints are the
// consist, src/WorldActionCursor.cpp a plain `int[4]`), and the tile argument likewise arrives as
// a PickRandomBigObjByCategory result at one site and the selected decor object at the other.
//
// ⚠ sic: when all four slots are occupied the scan falls out with i == 4 and the shared exit
// returns `aSlots[4]` -- one past the array, which byte-aliases aSortScratchMaybe[0]. Unreachable
// as long as nActiveCount agrees with the slots (the guard above already refused a full roster),
// so it is a latent out-of-bounds read rather than a live one. Reproduced as found.
//
// PARTIAL -- DIFF(544) at 697/656 bytes, insns 228/213, reg_pen=2 identity_miss=2 byte_diff=89.
// Register allocation is essentially the original's (a penalty of 2 across a 213-instruction
// body); the whole 15-instruction excess is ONE block cl declined to cross-jump. The original
// reaches the `nActiveCount++/nOwnedCountMaybe++/ClaimDecorObjMaybe` tail at 0x44de56 from THREE
// places -- two jumps out of the random-consist arm and a fallthrough out of the selection arm --
// and emits it once; this compile emits the two source copies separately. Same VC5 /Og
// trace-layout class as the parks in docs/PARKED.md, and NOT worth "fixing" at source level: the
// obvious lever, hoisting the tail out of both arms behind a shared `if (aSlots[i] != 0)`, was
// measured and is WORSE (170870 vs 144309) -- it trades 15 duplicated instructions for a reload,
// a test and a rotation that costs reg_pen 2 -> 7.
//
// Three source levers ARE paid for and must not be undone, worth 226579 -> 144309 together:
//   * the tail stays DUPLICATED in both arms (above);
//   * `switch (rand() % 3 + 2)` with cases 2/3/4, not `switch (rand() % 3)` with 0/1/2. The
//     switch value IS the category it then passes to AllocCarSlot, and the original's otherwise
//     inexplicable `add edx,2` immediately before the switch's own `sub edx,2` normalization is
//     the tell -- the 0/1/2 spelling compiles to `sub edx,ebx` instead;
//   * the random car count is UNSIGNED (`jbe` against 0, where a signed `int` gives `jle`).
// Refuted probe: early `return aSlots[i]` for the null/destroy paths instead of nesting. That
// does share the claim tail, but cl then stops merging the three copies of the two-instruction
// `aSlots[i]` exit load, which costs more than it saves (226579).
void *PeerTrainSlotQueueMaybe::SpawnOrAssignRandomTrain(void *pBigObj, void *pTrainRecord)
{
    if (nActiveCount >= 4 || nOwnedCountMaybe >= 3) {
        return 0;
    }
    unsigned int i = 0;
    PeerTrainNodePartial **ppSlot = aSlots;
    for (;;) {
        if (*ppSlot == 0) {
            break;
        }
        i++;
        ppSlot++;
        if (i >= 4) {
            return aSlots[i]; // sic: i == 4, see the note above
        }
    }
    int *anSelection = (int *)pTrainRecord;
    char szName[12];
    PeerTrainNodePartial *pNode;
    if (anSelection == 0) {
        pNode = new PeerTrainNodePartial(rand() % 3 * 2 + 0x1804, 0, 0, 0);
        aSlots[i] = pNode;
        if (pNode != 0) {
            if (pNode->carSlots[0] != 0 && ((CarNetObj *)pNode->carSlots[0])->bValid == 1) {
                _snprintf(szName, 10, "%s %lu", g_pLocalPlayerIdentity->name, pNode->wTrainId);
                ((CarNetObjVtblProbe *)aSlots[i]->carSlots[0])->SetNameImpl(szName);
                // The switch value IS the car category -- `rand() % 3 + 2` picks 2 (passenger),
                // 3 (freight) or 4 (caboose), and each arm hands that same number back to
                // AllocCarSlot. Spelling it `switch (rand() % 3)` with cases 0/1/2 instead costs
                // the original's own `add edx,2` normalization, which is the tell.
                for (unsigned int nCars = rand() % 5; nCars > 0; nCars--) {
                    switch (rand() % 3 + 2) {
                    case 2:
                        aSlots[i]->PeerTrainNode_AllocCarSlot(rand() % 3 * 2 + 0x1866, 2, 0);
                        break;
                    case 3:
                        aSlots[i]->PeerTrainNode_AllocCarSlot(rand() % 2 * 2 + 0x186c, 3, 0);
                        break;
                    case 4:
                        aSlots[i]->PeerTrainNode_AllocCarSlot(0x1870, 4, 0);
                        break;
                    }
                }
                nActiveCount++;
                nOwnedCountMaybe++;
                aSlots[i]->ClaimDecorObjMaybe((TrackDepotTileObj *)pBigObj, 1);
            } else {
                ((PeerTrainNodeVtblProbe *)pNode)->ScalarDeletingDtor(1);
                aSlots[i] = 0;
            }
        }
    } else {
        pNode = new PeerTrainNodePartial(anSelection[0], 0, 0, 0);
        aSlots[i] = pNode;
        if (pNode != 0) {
            if (pNode->carSlots[0] != 0 && ((CarNetObj *)pNode->carSlots[0])->bValid == 1) {
                _snprintf(szName, 10, "%s %lu", g_pLocalPlayerIdentity->name, pNode->wTrainId);
                ((CarNetObjVtblProbe *)aSlots[i]->carSlots[0])->SetNameImpl(szName);
                aSlots[i]->SetCarsReady(false);
                int *pnKind = &anSelection[1];
                for (int j = 3; j != 0; j--) {
                    if (*pnKind != 0) {
                        aSlots[i]->PeerTrainNode_AllocCarSlot(
                            *pnKind, MapCarTypeIdToCategoryMaybe(*pnKind), 0);
                        aSlots[i]->SetCarsReady(false);
                    }
                    pnKind++;
                }
                nActiveCount++;
                nOwnedCountMaybe++;
                aSlots[i]->ClaimDecorObjMaybe((TrackDepotTileObj *)pBigObj, 1);
            } else {
                ((PeerTrainNodeVtblProbe *)pNode)->ScalarDeletingDtor(1);
                aSlots[i] = 0;
            }
        }
    }
    return aSlots[i];
}

// FUNCTION: LOCO 0x44e3f0
// Derail check, run once per board slot at the top of DrainPendingSlotsMaybe's tick loop: has
// pNode's lead anchor driven into the SPRITE of a car belonging to some OTHER train? The test is
// pixel-accurate, not merely rectangular -- the anchor point has to land on a non-transparent
// pixel of the other car's current heading frame -- which is what stops two trains passing on
// adjacent parallel tracks from knocking each other over.
//
// On a hit BOTH trains are pushed into sound state 4 (the derailed/stalled state) with a fresh
// randomized 1..100-tick countdown in dwUnk0x28, and a single 0x3861 effect is spawned at the
// moving train's own anchor. The other train is only knocked over if it was making a sound at
// all (dwSoundStateMaybe != 0), so a train already sitting silent is left alone.
//
// The pixel coordinate handed to the bitmap is
// `pKindDesc->nativeWidth * pCar->wHeadingMaybe` -- the left edge of the car's per-heading frame
// within its sprite sheet -- plus the anchor's offset into the car's own on-screen rect. The
// clip rect is the car's rectViewport, so a car scrolled partly off-screen cannot be hit on the
// clipped-away side.
//
// ⚠ sic: the guard at the top of the CAR loop is `dwModeAMaybe == 2 && dwModeAMaybe == 3 &&
// dwModeBMaybe == 2` -- the same three tests the OUTER slot guard performs with `!=`/`&&` just
// above, but with the senses flipped and (surely) `&&` where `||` was meant. It can never be
// true, so it never skips a car, and the outer guard is the only thing that actually filters.
// The original emits it in full (one CSE'd load, two register compares, a never-taken branch),
// so it is reproduced rather than dropped. See docs/engine-bugs.md.
//
// ⚠ The `(*ppOther)` / `pOther` MIXTURE in that guard is deliberate and load-bearing, not a
// transcription slip: cl 5.0 FOLDS `x == 2 && x == 3` to false and deletes the whole block
// whenever both reads are the SAME source expression -- measured for all four same-expression
// spellings (plain `&&`, three nested `if`s, a cached `int` local, and all-`(*ppOther)`; every
// one scores an identical 324077 with the block gone). Only two DIFFERENT expressions naming the
// same field survive the fold. That leaves one residual instruction there (the original CSEs its
// two reads into one load; this pair compiles to two memory compares), which is the price of
// having the block at all -- and it is the best of every spelling probed, 346077 -> 274454.
//
// PARTIAL -- DIFF(456) at 517/561 bytes, insns 164/171, align=268 reg_pen=54. Structure is the
// original's throughout; the residual is one whole-body register-allocation choice. The original
// keeps `pNode` in EBX and `pLead` in EBP for the entire body and SPILLS the two rect edges
// (nLeft to [esp+0x1c], nTop to [esp+0x24]) plus two of the four rectViewport dwords; this
// compile does the reverse -- it keeps the rect values in registers and reloads `pNode`/`ppOther`
// from the frame instead, which is why it is 7 instructions SHORTER rather than longer.
//
// Always returns 0 -- every path falls into the one epilogue's `xor al,al`. The single caller
// discards it, so the value carries no meaning; only the fact that a byte is returned at all
// (rather than `void`) is load-bearing for the epilogue shape.
char PeerTrainSlotQueueMaybe::CheckDerailCollisionMaybe(PeerTrainNodePartial *pNode)
{
    if (nActiveCount >= 2 && pNode != 0 && pNode->dwSoundStateMaybe == 2 &&
        pNode->dwModeAMaybe != 2 && pNode->dwModeAMaybe != 3 && pNode->dwModeBMaybe != 2) {
        NameAnchorMaybe *pLead = pNode->pNameMaybe;
        PeerTrainNodePartial **ppOther = aSlots;
        int nSlots = 4;
        do {
            PeerTrainNodePartial *pOther = *ppOther;
            if (pOther != 0 && pOther->dwModeAMaybe != 2 && pOther->dwModeAMaybe != 3 &&
                pOther->dwModeBMaybe != 2 &&
                (pOther->dwSoundStateMaybe != 4 || pOther->dwUnk0x28 != 0) && pOther != pNode) {
                unsigned short i = 0;
                do {
                    // sic: `||` where `&&` was surely meant -- the outer slot guard three lines
                    // up is the same three tests joined with `&&`, and this repeat of it is
                    // always true (dwModeAMaybe cannot be both 2 and 3).
                    if ((*ppOther)->dwModeAMaybe == 2 && pOther->dwModeAMaybe == 3 &&
                        (*ppOther)->dwModeBMaybe == 2) {
                        pOther = *ppOther;
                        continue;
                    }
                    {
                        CarNetObj *pCar = (CarNetObj *)pOther->carSlots[i];
                        int nLeft = pCar->rect.left;
                        int nTop = pCar->rect.top;
                        if ((pCar->pNameAMaybe->pTileMaybe == pLead->pTileMaybe ||
                             pCar->pNameBMaybe->pTileMaybe == pLead->pTileMaybe) &&
                            nLeft < pLead->nAnchorXMaybe &&
                            pCar->rect.right > pLead->nAnchorXMaybe &&
                            nTop < pLead->nAnchorYMaybe &&
                            pCar->rect.bottom > pLead->nAnchorYMaybe &&
                            pNode->pNameMaybe->dwTileExtendPhaseMaybe != 2 &&
                            pCar->pKindDesc->pOwnedObjA->IsPixelTransparentAtMaybe(
                                pCar->rectViewport,
                                pCar->pKindDesc->nativeWidth * pCar->wHeadingMaybe - nLeft +
                                    pLead->nAnchorXMaybe,
                                pLead->nAnchorYMaybe - nTop) == 0) {
                            pNode->SetSoundStateMaybe(4);
                            pNode->dwUnk0x28 = rand() % 100 + 1;
                            DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(
                                0x3861, 0, 'W', pNode->pNameMaybe->nAnchorXMaybe,
                                pNode->pNameMaybe->nAnchorYMaybe, 1);
                            if ((*ppOther)->dwSoundStateMaybe != 0) {
                                (*ppOther)->SetSoundStateMaybe(4);
                                (*ppOther)->dwUnk0x28 = rand() % 100 + 1;
                            }
                            break;
                        }
                    }
                    pOther = *ppOther;
                } while (++i <= pOther->wCarSlotCount);
            }
            ppOther++;
            nSlots--;
        } while (nSlots != 0);
    }
    return 0;
}

// FUNCTION: LOCO 0x44df40
// Take the first free board slot for pNode and give the train somewhere to arrive: in a SOLO or
// HOSTING session an arbitrary station of the right category, in a JOINED session (connectionMode
// 2) the specific board tile the host named. Answers 0 only when the roster is already full --
// failing to find any arrival tile is NOT a failure, the train just parks in sound state 3.
//
// The category handed to PickRandomBigObjByCategory is the connectionMode itself minus one, but
// spelled as two separate constants because the joined case is a different code path entirely:
// hosting (1) picks category 1, solo (0, and any other value) picks category 0.
//
// coord arrives BY VALUE as one dword (`ret 0xc` = 4 + 4 + 4) and is copied to a local straight
// away, which is what makes the +1 read the PARAMETER's own high half rather than the copy.
//
// PARTIAL -- DIFF(175) at 235/224 bytes, insns 81/78, align=126 reg_pen=4. Every block is the
// original's; the whole residual is ONE loop-rotation choice plus its cascade. This compile
// PEELS the `i == 0` slot test out of the scan (`mov edx,[ecx+8]` with the index folded away,
// then a bottom-tested loop starting at the increment); the original keeps the test at the loop
// TOP and lets the backedge return to it, so its three `mov edx,eax / and edx,0xffff / mov
// esi,[ecx+edx*4+8]` recur every iteration. That in turn sinks this compile's roster-full exit
// to the very end of the function, where it materializes the 0 as `xor al,al` instead of reusing
// the already-zeroed BL the original returns through.
//
// Three source levers were paid for and are already in place, worth 220747 -> 126497 together:
// (1) wrapping the whole body in `if (nActiveCount < 4)` with a single trailing
// `return bClaimed`, which is what makes bClaimed a real phi and pins it in BL (the early-return
// spelling lets cl constant-fold every exit to a literal and drops EBX entirely); (2) the
// connectionMode dispatch as a `switch`, not an if/else-if chain -- the original's `dec eax; je
// / dec eax; je` is cl's dense-switch form, and it is what tail-merges the two
// PickRandomBigObjByCategory calls onto one shared `mov ecx,0x4a9990; call`; (3) the arrival-tile
// null chain as ONE short-circuit condition rather than a nested re-test.
// Loop spellings tried, all measured: `while (aSlots[i] != 0) { i++; if (i >= 4) ... }` and
// `while (...) { if (++i >= 4) ... }` compile BYTE-IDENTICALLY (both 235 B); `for (;;) { if
// (aSlots[i] == 0) break; ... }` is WORSE (245 B, it adds a fourth epilogue). Both spellings of
// the arrival-tile condition (`p == 0 && (p = Pick(1)) == 0` with the arms one way, `p != 0 ||
// ... != 0` with them the other) also compile byte-identically, so the arm ORDER is not
// source-steerable here; giving the EnqueueClaimWaiterMaybe arm an explicit `return` is worse (245 B).
// Same VC5 /Og loop-rotation class as the parks in docs/PARKED.md. PARKED.
char PeerTrainSlotQueueMaybe::ClaimSlotForTrain(PeerTrainNodePartial *pNode, Pair16 coord,
                                               char quadrant)
{
    char bClaimed = 0;
    if (nActiveCount < 4) {
        unsigned short i = 0;
        while (aSlots[i] != 0) {
            if (++i >= 4) return bClaimed;
        }
        bClaimed = 1;
        aSlots[i] = pNode;
        pNode->nDeferredMoveStateMaybe = 0;
        nActiveCount++;

        Pair16 pos = coord;
        void *pArrivalTile;
        switch (g_pDPlaySessionMgr->connectionMode) {
        case 1:
            pArrivalTile = g_NetSessionEventQueue.PickRandomBigObjByCategory(1);
            break;
        case 2:
            if (quadrant == 1 || quadrant == 2) {
                pos.hi = coord.hi + 1;
            }
            pArrivalTile = g_worldBoard.GetPlaneASlotMaybe(pos.lo, pos.hi, 0);
            break;
        default:
            pArrivalTile = g_NetSessionEventQueue.PickRandomBigObjByCategory(0);
            break;
        }
        if (pArrivalTile == 0 &&
            (pArrivalTile = g_NetSessionEventQueue.PickRandomBigObjByCategory(1)) == 0) {
            pNode->SetSoundStateMaybe(3);
        } else {
            ((TrackTileObj *)pArrivalTile)->EnqueueClaimWaiterMaybe(pNode);
        }
    }
    return bClaimed;
}

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
                ((PeerTrainNodeDrainTickPartial *)*ppNode)->PeerTrainNode_UpdatePlacementTickMaybe();
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

// FUNCTION: LOCO 0x44dea0
// Re-tag the train parked on the selected depot tile to match the four-entry selection the
// train-config menu just built (WorldActionCursor::HandleMenuCommandMaybe's 0x3866 command):
// anSelection[0] is the lead unit's kind, anSelection[1..3] the three towed slots (0 = leave
// empty). The lead unit is retagged IN PLACE through its own vtable rather than being freed and
// rebuilt, so the locomotive keeps its identity across a re-spec; only the towed cars are
// dropped and re-allocated. Finishes by re-binding the train to the tile, which re-lays every
// car's track anchors around the new consist.
//
// Ghidra sees a `__stdcall(pTile, anSelection)` -- no `this` is read -- but both call sites
// load ecx with this singleton, so it is modeled as a member for the same reason
// FreeQueuedTrainCarSlots and DetachFromBoardMaybe are.
char PeerTrainSlotQueueMaybe::RebuildCarSlotsFromSelectionMaybe(TrackDepotTileObj *pTile,
                                                               int *anSelection)
{
    char bChanged = 0;
    PeerTrainNodePartial *pTrain;
    if (pTile->dwOccupancyClaim == 1 &&
        (pTrain = (PeerTrainNodePartial *)pTile->pOwningTrain) != 0) {
        int nLeadKind = anSelection[0];
        if (CarNetObj_GetCarTypeId(pTrain->carSlots[0]) != nLeadKind) {
            ((CarNetObj *)pTrain->carSlots[0])->SetCarTypeAndCategory(nLeadKind, -1);
            bChanged = 1;
        }
        // Three releases of slot 1, not one pass over slots 1..3: every release compacts the
        // survivors down, so the next towed car lands back in slot 1 for the following call.
        for (int i = 3; i != 0; i--) {
            pTrain->PeerTrainNode_ReleaseCarSlot(1);
        }
        int *pnKind = &anSelection[1];
        for (int j = 3; j != 0; j--) {
            if (*pnKind != 0) {
                pTrain->PeerTrainNode_AllocCarSlot(*pnKind, MapCarTypeIdToCategoryMaybe(*pnKind),
                                                  0);
                bChanged = 1;
            }
            pnKind++;
        }
        pTrain->ClaimDecorObjMaybe(pTile, 1);
        return bChanged;
    }
    return 0;
}

// FUNCTION: LOCO 0x44da50
// A peer leaving takes its trains with it: unboard every passenger, pull the node off the
// board, then release the queue slot itself. The passenger loop is FreeQueuedTrainCarSlots'
// body written out again rather than a call -- that is what the original has, and it is what
// this spelling reproduces.
//
// Note both tail calls re-read `aSlots[i]` instead of reusing the `pNode` the guard already
// loaded; the original issues that same `mov [ebp+0]` twice more, so the redundancy is the
// source's, not a scheduling artifact.
void PeerTrainSlotQueueMaybe::ReleaseSlotsForOwnerMaybe(unsigned char owner) {
    for (int i = 0; i < 4; i++) {
        PeerTrainNodePartial *pNode = aSlots[i];
        if (pNode != 0 && pNode->bOwnerByteA == owner) {
            for (int j = 0; j < 8; j++) {
                if (pNode->apPassengerMaybe[j] != 0) {
                    ((WalkerActor *)pNode->apPassengerMaybe[j])->LeaveTrainMaybe();
                    pNode->apPassengerMaybe[j] = 0;
                }
            }
            DetachFromBoardMaybe(aSlots[i]);
            ReleaseOrForwardMatchingSlotMaybe(aSlots[i]->wTrainId, owner, 0);
        }
    }
}

// FUNCTION: LOCO 0x44dad0
// A track tile is going away (sole caller: the vtable-0x478308 track-tile leaf dtor, which
// passes its own `this`): release every board train slot whose train overlaps the tile's
// rect. A slot is tested only when its train is player-relevant here -- dwModeBMaybe != 2,
// or the dying tile is itself in dwTrackState 4. The overlap test ORs across the train's
// four car slots with short-circuit (once any car intersects, later non-null cars are not
// re-tested), then the release is ReleaseSlotsForOwnerMaybe's tail written out again:
// unboard all 8 passengers, detach from the board, release/forward the slot.
//
// EFFECTIVE MATCH (v517, DIFF(126), ours 213 B vs orig 214 B, insns 77/78). Three shape
// levers are baked in and load-bearing: (a) the outer walk is a `ppSlot++/n--` do-while
// with EVERY use re-reading `*ppSlot` (an aSlots[s] for-loop caches the node in EBX and
// memory-spills the walk instead -- DIFF(185)); (b) the car index is UNSIGNED int (signed
// flips the loop backedge `jc` to `jl`); (c) the passenger loop hoists its base through a
// `int *pPassenger` local (indexing `(*ppSlot)->apPassengerMaybe[j]` re-loads the node per
// store -- DIFF(179) -- because LeaveTrainMaybe's call clobbers memory). The bOverlap fold
// `if (bOverlap || IntersectRect(...)) bOverlap = 1; else bOverlap = 0;` matched the
// branch skeleton on the first compile. The whole residual is ONE instruction-selection
// coin-flip cluster rippling from the car fetch: the original unfuses it
// (`mov eax,[ebx]; add eax,esi; mov edx,[eax]` -- pCar in EDX) where this build folds
// (`mov eax,[ebx+esi]` -- pCar in EAX), and every downstream choice follows that register
// (push edx/eax for &car->rect, lea eax/edx for &rcOverlap, ecx/eax for the Detach node,
// dl+cl swap in the Release tail), plus one MOV+ADD vs LEA pick for the passenger base.
// Same VC5 /Og selection coin-flip family as v329's LEA scheduling-swap -- possibly
// TU-phase carry from this TU's eight earlier partial siblings. Probes refuted (all
// byte-identical, kept the first form): pCar declared outside the loop;
// `&apPassengerMaybe[0]` instead of the array decay. Parked in docs/PARKED.md.
void PeerTrainSlotQueueMaybe::ReleaseSlotsOverlappingRectMaybe(TrackTileObj *pTile) {
    PeerTrainNodePartial **ppSlot = aSlots;
    int nSlotCount = 4;
    do {
        if (*ppSlot != 0 && ((*ppSlot)->dwModeBMaybe != 2 || pTile->dwTrackState == 4)) {
            char bOverlap = 0;
            RECT rcOverlap;
            for (unsigned int i = 0; i < 4; i++) {
                CarNetObj *pCar = (CarNetObj *)(*ppSlot)->carSlots[i];
                if (pCar != 0) {
                    if (bOverlap || IntersectRect(&rcOverlap, &pCar->rect, &pTile->rect)) {
                        bOverlap = 1;
                    } else {
                        bOverlap = 0;
                    }
                }
            }
            if (bOverlap) {
                int *pPassenger = (*ppSlot)->apPassengerMaybe;
                for (int j = 0; j < 8; j++) {
                    if (pPassenger[j] != 0) {
                        ((WalkerActor *)pPassenger[j])->LeaveTrainMaybe();
                        pPassenger[j] = 0;
                    }
                }
                DetachFromBoardMaybe(*ppSlot);
                ReleaseOrForwardMatchingSlotMaybe((*ppSlot)->wTrainId, (*ppSlot)->bOwnerByteA, 1);
            }
        }
        ppSlot++;
        nSlotCount--;
    } while (nSlotCount != 0);
}

// FUNCTION: LOCO 0x44e830
// Click router for the four board train slots. Declines outright unless the placement cursor
// has a pending secondary action -- i.e. the player is mid-interaction; otherwise walks each
// occupied slot's cars in order and hands the FIRST one whose own rect contains the pixel to
// the selected-object widget, consuming the click.
//
// The inner walk is `i <= wCarSlotCount`, not `<`, and that is CORRECT rather than an
// off-by-one: carSlots[0] is the locomotive and wCarSlotCount counts only the cars BEHIND it
// (see RebuildCarSlotsFromSelectionMaybe, which seeds slot 0 then fills at most 3 more), so
// 0..count is exactly the 4-entry array.
//
// EXACT (v501). Was the repo's closest non-match for months: EFFECTIVE DIFF(1) at 158/158 bytes,
// the single differing BYTE the SIB operand order of the car fetch (`[eax+edi]` vs `[edi+eax]`).
// The #53 subscript-induction sweep cracked it: writing the OUTER slot walk as
// `for (int s = 0; s < 4; s++)` with `aSlots[s]` re-reads -- instead of a hand-rolled
// `ppNode++/n--` do-while -- flips the instruction selector to the original's base/index roles.
// Two levers from the parked era remain baked in: the inner walk must be a `for` (a `do/while`
// inverts the backedge polarity, DIFF(4)), and the node must be re-read as `aSlots[s]->` at BOTH
// uses inside the body (caching it in a loop-local costs DIFF(114)).
char PeerTrainSlotQueueMaybe::SelectCarAtPositionMaybe(int x, int y)
{
    char bConsumed = 0;
    if (!PlacementCursorMaybe_004854c8.bPendingActionBMaybe) {
        return 0;
    }
    for (int s = 0; s < 4; s++) {
        if (aSlots[s] != 0) {
            for (unsigned int i = 0; i <= aSlots[s]->wCarSlotCount; i++) {
                CarNetObj *pCar = (CarNetObj *)aSlots[s]->carSlots[i];
                if (pCar != 0 && pCar->bReady == 1 && pCar->Contains(x, y) != 0) {
                    SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(
                        (int)aSlots[s]->carSlots[i]); // sic: see the note above
                    bConsumed = 1;
                    break;
                }
            }
        }
    }
    return bConsumed;
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 3 of 13: g_PeerTrainSlotQueue (DAT_004a98b0), PeerTrainSlotQueueMaybe::PeerTrainSlotQueueMaybe
// (0x44d800). (PeerTrainSlotQueueMaybe.h is already included at the top of this TU and carries
// no include guard, so re-including it here would be a C2011 redefinition, not a no-op.)
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_g_PeerTrainSlotQueue(void) {
    new (&g_PeerTrainSlotQueue) PeerTrainSlotQueueMaybe();
}
#endif // LOCO_PORT
