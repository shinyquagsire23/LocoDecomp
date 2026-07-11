// PeerTrainSlotQueueMaybe -- singleton DAT_004a98b0 (0x58 bytes), the "fixed-4-slot
// recently-touched train" queue: own vtable 0x478370, aSlots[4] of PeerTrainNode* at +0x8,
// a 16-entry CarNetObj* sort-scratch buffer at +0x18..+0x58. Full field/method inventory in
// docs/subsystems.md ("PeerTrainSlotQueueMaybe", named/typed 2026-07-12).
//
// This is the SINGLE canonical data-layout model of the singleton (v323: absorbed
// NetSessionEventQueue.cpp's PeerTrainSlotQueuePartial per the never-duplicate-structs rule).
// DPlaySessionMgr.cpp keeps a methods-only view (PeerTrainSlotQueueEdgePartial) -- including
// this header there regresses that TU's SelectGridCellFromPointMaybe EXACT->DIFF(130) via its
// documented position sensitivity; a methods-only view has no layout to drift (same pattern
// as NetSessionEventQueueEdge). Method bodies live in src/PeerTrainSlotQueueMaybe.cpp; the
// remaining methods are declared-only (bodies in the binary / not yet transcribed).
#include <windows.h>         // RECT -- DrawVisibleCarsInTileMaybe takes one BY VALUE

struct PeerTrainNodePartial; // fwd -- src/PeerTrainNode.h
struct Pair16;               // fwd -- (x,y) pair passed by value (src/Pair16.h)
struct CarNetObj;            // fwd -- src/CarNetObj.h (pointer-only use below)
class TrackDepotTileObj;     // fwd -- src/TilePlacedObj.h (RebuildCarSlotsFromSelectionMaybe)
class TrackTileObj;          // fwd -- src/TilePlacedObj.h (pointer-only use below).
                             // `class`, not `struct`: VC5 takes a type's DEFAULT ACCESS from the
                             // LAST-SEEN tag, so a mismatched fwd decl here is the v414 hazard
                             // (it also warns C4099).

struct PeerTrainSlotQueueMaybe {
    void *pVtbl;                     // +0x00 -- vtable 0x478370 (slot 0 = scalar deleting dtor)
    unsigned short nActiveCount;     // +0x04 -- decremented on every slot clear
    unsigned short nOwnedCountMaybe; // +0x06 -- decremented only on the outright-release path;
                                     //   zeroed alongside nActiveCount by ResetAllFields (stays
                                     //   a separate 16-bit store, not merged into one dword)

    PeerTrainNodePartial *aSlots[4]; // +0x08..+0x17 -- the 4 train slots (docs/subsystems.md)
    CarNetObj *aSortScratchMaybe[16]; // +0x18..+0x57 -- 16-entry CarNetObj* sort-scratch
                                     //   buffer (CollectAndSortVisibleCarsMaybe); zeroed by
                                     //   ResetAllFields's 16-entry loop and ClearSortScratchMaybe.
                                     //   Typed CarNetObj* (was int) 2026-07-28 when
                                     //   CollectAndSortVisibleCarsMaybe -- the only writer -- was
                                     //   transcribed; layout-neutral, and it keeps that body free
                                     //   of the casts the idiom lint would otherwise flag.

    unsigned char IsSlotCountOutOfRangeMaybe(); // 0x44dbb0

    // 0x44e160 -- does any board train currently sit on this tile? Checks each occupied slot's
    // own lead anchor first, then every one of its cars' A- and B-side anchors. Body in
    // src/PeerTrainSlotQueueMaybe.cpp.
    char IsTileOccupiedByAnyTrainMaybe(TrackTileObj *pTile);

    // 0x44e200 -- refills aSortScratchMaybe with every car of every occupied slot that is
    // currently VISIBLE (neither per-car mode is a mid-extend state), then bubble-sorts the
    // collected run by screen Y so the draw walk paints back-to-front. Takes NO argument
    // (plain `ret`, not `ret 0x4` -- Ghidra's `param_1` is spurious). Body in
    // src/PeerTrainSlotQueueMaybe.cpp.
    void CollectAndSortVisibleCarsMaybe();

    // 0x44e2e0 -- the draw half of the pair above: walk the sorted scratch buffer and, for each
    // car that overlaps this one 16x16 tile box, blit its slice. On plane 1 only, it then scans
    // FORWARD from that car for the first later entry belonging to the SAME train whose own
    // wUnk0x448 plane is 0 and which also overlaps the tile, and blits that one too -- the
    // one-extra-slice rule that keeps a coupled neighbour from being clipped away at a tile seam.
    // rcTile arrives BY VALUE (`ret 0x14` = 16 + 4) and is forwarded verbatim to
    // BlitTileSliceMaybe. All three call sites are in the 0x456700 paint routine, each loading
    // ecx with this singleton. Body in src/PeerTrainSlotQueueMaybe.cpp.
    void DrawVisibleCarsInTileMaybe(RECT rcTile, short nPlaneMaybe);

    // 0x44e2d0 -- zero-fills aSortScratchMaybe (compiler recognizes the loop -> rep
    // stosd). Sole caller: WorldBoardMaybe_UpdateDirtyTilesMaybe (0x456150), on the
    // singleton.
    void ClearSortScratchMaybe();

    // 0x44dc10 -- picks/allocates one of the 4 train slots for a freshly-loaded layout's own
    // train record. Body not transcribed (large, PeerTrainNode-internals-heavy); declared only
    // for the call shape.
    void *SpawnOrAssignRandomTrain(void *pBigObj, void *pTrainRecord); // extern

    // 0x44d870 -- zeroes both counters, all 4 slots, and the scratch buffer. Called on the
    // singleton by Config_SaveWindowAndCleanExit (0x407860) and
    // AppWindow::AbortMultiplayerSession (0x406e80). Defined in
    // src/PeerTrainSlotQueueMaybe.cpp (PARKED -- see the body comment).
    void ResetAllFields();

    // 0x44df40 -- claims a free slot for a popped queued train node at the resolved
    // coord/quadrant and attaches it to the board (or, in joined mode, resolves the target
    // board plane slot first); returns 0 when the roster is full.
    char ClaimSlotForTrain(PeerTrainNodePartial *pNode, Pair16 coord, char quadrant);

    // 0x44e020 -- the roster's per-tick pump: gated on the world being on screen
    // (g_nScreenState 3 or 9), walk the four slots and, for each occupied one, derail-check it,
    // tick it, then retire it on dwSoundStateMaybe == 3 or hand it off to a peer on
    // nDeferredMoveStateMaybe == 1. Body in src/PeerTrainSlotQueueMaybe.cpp (PARTIAL,
    // DIFF(222) -- the documented `sete` prologue class + one mov ebx,eax; see the body
    // comment). Landed from docs/PARKED.md's v492 section.
    void DrainPendingSlotsMaybe();

    // 0x44d800 -- this class's own ctor (Ghidra still mis-names it
    // TrackTileObjTypeSet1234Maybe): installs vtable 0x478370, then does exactly what
    // ResetAllFields below does. Body in src/PeerTrainSlotQueueMaybe.cpp.
    // ⚠ The SINGLETON OBJECT itself is still only extern-declared, never defined, in every
    // consumer TU -- so the static-init thunk at 0x45c590 (ctor + `atexit` of the 0x45c5b0 dtor
    // thunk) is not emitted by anything in src/ yet. Defining the object is what would emit it,
    // the same way defining DecorObjMgrMaybe_00485448 is what makes src/DecorActor.cpp emit
    // 0x434500 (see that definition's note) -- but it also needs this class's dtor, which forces
    // it off the `void *pVtbl` field model onto a real vtable. Separate job.
    PeerTrainSlotQueueMaybe();
    //
    // THIS HEADER'S DECLARATION COUNT IS A LIVE DIAL for src/WorldBoardMaybe.cpp's 0x457ce0
    // (951 B). Curve measured v492 from a healthy baseline, counting ordinary member
    // declarations added on top of CheckDerailCollisionMaybe below:
    //     +0 healthy   +1 DIFF   +2 DIFF   +3 DIFF   +4 healthy   +5 healthy   +6 DIFF
    // ⚠ A CONSTRUCTOR declaration does NOT count toward it -- adding the 0x44d800 ctor plus two
    // ordinary methods lands on the +2 rung, not +3 (measured). Destructors are untested and
    // would anyway force this class off its `void *pVtbl` field model onto a real vtable.
    // DrainPendingSlotsMaybe was v492's +1 and cost the canary then; the curve is a property of
    // the whole compilation, so the canary was re-measured when it landed.

    // 0x44e3f0 -- derail check, run first in DrainPendingSlotsMaybe's per-slot loop: does any
    // OTHER board train have a car whose sprite covers pNode's own lead anchor? On a
    // pixel-accurate hit both trains are knocked into sound state 4 for a randomized 1..100
    // ticks and a derail effect (kind 0x3861) is spawned at the anchor. Body in
    // src/PeerTrainSlotQueueMaybe.cpp. Always returns 0 -- see the body comment.
    char CheckDerailCollisionMaybe(PeerTrainNodePartial *pNode);

    // 0x44e800 -- frees the (up to 8) car slots a queued train node still holds, before it is
    // re-placed or deleted. A this-ignoring thiscall (Ghidra sees __stdcall(pNode)); invoked as
    // a member so the call site reproduces the ecx=singleton load.
    void FreeQueuedTrainCarSlots(PeerTrainNodePartial *pNode);

    // 0x44d8a0 -- releases or forwards the queue slot matching (wTrainId, owner) (Ghidra:
    // ReleaseOrForwardMatchingSlotMaybe(this, uint, char, char)). Sole known caller:
    // WorldActionCursor::HandleMenuCommandMaybe's 0x3865 detach command. Returns 1 once a
    // matching slot has been cleared, 0 when nothing matched. bOwner is UNSIGNED char for the
    // same reason as ReleaseSlotsForOwnerMaybe below (bare byte compare against bOwnerByteA).
    // Body in src/PeerTrainSlotQueueMaybe.cpp.
    char ReleaseOrForwardMatchingSlotMaybe(unsigned int wTrainId, unsigned char bOwner, char bFlag);

    // 0x44e630 -- detaches a train node from the board (Ghidra namespaces it
    // PeerTrainNode_DetachFromBoardMaybe, but every known call site loads ecx with THIS
    // singleton -- same this-ignoring shape as FreeQueuedTrainCarSlots).
    void DetachFromBoardMaybe(PeerTrainNodePartial *pNode);

    // 0x44dea0 -- rebuilds a train's car slots from the caller's 4-int selection scratch
    // (Ghidra: PeerTrainNode::RebuildCarSlotsFromSelectionMaybe; again ecx = this singleton
    // at the call site, HandleMenuCommandMaybe's 0x3866 command). The first slot is retagged in
    // place; the three towed slots are dropped and re-allocated from anSelection[1..3]. Answers
    // whether anything actually changed -- both known call sites discard it. Body in
    // src/PeerTrainSlotQueueMaybe.cpp.
    //
    // pTile is the selected DEPOT tile the train is parked on: the two fields the guard reads
    // are TrackDepotTileObj's own dwOccupancyClaim (+0x11c) and pOwningTrain (+0x120), and the
    // train it hands back to ClaimDecorObjMaybe on the way out is that same tile.
    char RebuildCarSlotsFromSelectionMaybe(TrackDepotTileObj *pTile, int *anSelection);

    // 0x44da50 -- releases every board train slot owned by the given provider-slot index (a
    // departing peer's trains). Body in src/PeerTrainSlotQueueMaybe.cpp.
    //
    // The argument is UNSIGNED char, pinned 2026-07-27 (v446) by the guard's own comparison:
    // `pNode->bOwnerByteA == owner` against the unsigned-char field is a bare `mov dl,[eax+0x78];
    // cmp dl,cl` byte compare. With a signed `char` the two operands stop agreeing and cl
    // promotes BOTH to int first (`movsx edx,[esp+0x18]` / `xor ecx,ecx; mov cl,[eax+0x78]` /
    // `cmp ecx,edx`), which is the whole DIFF(88) -- everything downstream of it was just the
    // register renaming that followed.
    void ReleaseSlotsForOwnerMaybe(unsigned char owner);

    // 0x44dad0 -- releases every board train slot whose consist overlaps the dying track
    // tile's rect (sole caller: a track-tile leaf dtor). Body in
    // src/PeerTrainSlotQueueMaybe.cpp (EFFECTIVE, DIFF(126) -- see the body comment).
    void ReleaseSlotsOverlappingRectMaybe(TrackTileObj *pTile);

    // 0x44d9b0 -- full-board teardown: every occupied slot gets the unboard/detach/release
    // tail, then the two teardown COM-ish globals (DAT_00485268/DAT_0048526c) are released
    // and NULLed. Body in src/PeerTrainSlotQueueMaybe.cpp.
    void TeardownAllSlotsMaybe();

    // 0x44e830 -- click router: picks the train car (if any) under the world pixel (x, y) and
    // makes it the selection, returning whether it consumed the click. Third in
    // CommitPendingRotateMaybe's decline chain, between DecorObjMgrMaybe::ResolveClickMaybe
    // and the world board's own ResolveWorldClickMaybe. Body in
    // src/PeerTrainSlotQueueMaybe.cpp (EFFECTIVE, DIFF(1)/158 B -- one SIB operand-order byte).
    char SelectCarAtPositionMaybe(int x, int y);

    // 0x44dbd0 -- walk the four board train slots and hand bReady to each occupied one's
    // PeerTrainNodePartial::SetCarsReady, skipping any node whose dwModeBMaybe is not one of
    // 0/1/4/5. The build-mode entry path (AppWindow_EnterBuildMode) calls it with 1 to let the
    // parked trains start moving again. Body in src/PeerTrainSlotQueueMaybe.cpp.
    //
    // The argument is `bool`, not this codebase's usual `char` -- the third such pin in v446
    // (see AnimDescRefObj0x477488::SetReadyStateMaybe and
    // WorldActionCursor::Contains). It is forwarded verbatim to
    // PeerTrainNodePartial::SetCarsReady(bool), so a `char` here costs the 0/1 normalization
    // cl warns about with C4800 -- DIFF(23) at 68 bytes against the original's 61.
    void DispatchActiveSlotsMaybe(bool bReady);
};
extern PeerTrainSlotQueueMaybe g_PeerTrainSlotQueue; // DAT_004a98b0
