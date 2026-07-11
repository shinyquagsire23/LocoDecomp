// NetSessionEventQueue (DAT_004a9990) -- the world-object registry / board-layout singleton:
// it owns the placed-object record list (pEvents), the board save/load pair, the four
// train edge-placement helpers, and the BigObj-family create/remove registry entry points.
//
// This is the CANONICAL home for the class and its pEvents record-list types. They lived
// inline at the top of src/NetSessionEventQueue.cpp until 2026-07-26, when a second TU
// (src/BuildToolButton.cpp, for OnPressReleaseMaybe's SaveBoardLayout call) needed them and
// CLAUDE.md's "never duplicate a struct across TUs" ruled out a fourth partial view. The move
// is a pure text relocation -- the .cpp includes this header from exactly the position the
// definitions used to occupy, so its preprocessed output is unchanged (verified byte-neutral:
// all of that TU's matches held).
//
// ONE older per-TU partial view of this same singleton is left and is tagged debt
// (src/RoadVehicleActor.cpp's NetSessionEventQueueRoadPartial); migrate it onto this class,
// measuring the TU as it moves. src/WidgetPicker.cpp's NetSessionEventQueuePartial was retired
// in v564 by including this header there -- FREE, per-file table byte-identical repo-wide.
// That is not just cosmetic: a view spelling mangles under the VIEW's class name, so
// WidgetPicker's SaveBoardLayout/PlaceEdgeLinksAndFlush calls (the "~curr" board save path)
// resolved to symbols defined nowhere -- `xor eax,eax; ret N` stubs in the port. CODEGEN #184.
#pragma once

#include "Pair16.h"
#include "Obj0x477798Family.h"  // Obj0x477758Base / Obj0x477758 -- the pEvents record list

struct TilePlacedObjPartial; // defined below, forward-declared for BigObj_CreateAndInsert's
                              // own return type

class NetSessionEventQueue {
public:
    // No explicit vtbl field -- the compiler's own vtable slot (from the virtual dtor
    // below) already occupies offset 0; adding one here would duplicate it.
    Obj0x477758 pEvents;
    // ⚠ UNSIGNED, not `int`. The only site in the codebase that can tell the two models apart is
    // PickRandomBigObjByCategory's case -1 arm: `rand() % nPlacedObjectCount` emits an unsigned
    // `xor edx,edx; div` in the original, where an `int` field promotes the expression to a
    // signed `cdq; idiv`. Measured repo-wide: byte-neutral everywhere else.
    unsigned int nPlacedObjectCount;
    // +0x18 -- confirmed a genuine member (not the separately-Ghidra-labeled adjacent
    // g_dwReadyBigObjCountMaybe global) via ebp-relative addressing at both real access
    // sites (BigObj_CreateAndInsert/BigObj_Remove): a plain global-by-name
    // reference compiles to absolute addressing under this toolchain regardless of memory
    // adjacency, so the original source must access it as this->field.
    unsigned int dwReadyBigObjCount;

    NetSessionEventQueue();
    virtual ~NetSessionEventQueue();

    // The "edge placement" quartet -- one per board edge, dispatched by train heading by
    // DPlaySessionMgr::HandleQueuedTrainPlacement (0x43e370). Each returns the resolved
    // (x,y) as a Pair16 BY VALUE; the bodies ignore `this`, but they are genuine members
    // (every call site loads ecx = the singleton). See the definitions below.
    Pair16 ComputeRightEdgePlacement();   // 0x41d8f0
    Pair16 ComputeLeftEdgePlacement();    // 0x41d920
    Pair16 ComputeBottomEdgePlacement();  // 0x41d950
    Pair16 ComputeTopEdgePlacement();     // 0x41d980

    // 0x41dd40 -- tick every registered object's animation one frame (slot 10 on each live
    // pEvents entry). Sparse-safe: NULL slots are skipped.
    void AdvanceAllAnimFramesMaybe();

    char PlaceEdgeLinksAndFlush(unsigned char *pszName);
    unsigned char SaveBoardLayout(unsigned char *pszPath);

    // Same "genuine member whose body ignores `this`" class as the edge-placement quartet
    // above (v362, lever 3). Both call sites live in PlaceEdgeLinksAndFlush: the first
    // (0x41d338) needs no ecx setup because `this` is still live in ecx from that function's
    // own prologue, and the second (0x41d3f5) reloads it -- `mov ecx,ebp` -- only because the
    // intervening path-building `rep movs` clobbered ecx. It was modelled as a free __stdcall
    // function through v361, which cost PlaceEdgeLinksAndFlush that one instruction.
    unsigned char LoadLayoutAndPopulateBoard(unsigned char *pszPath, char bResetTiles,
                                             char bSkipDefaultFlag); // 0x41d5c0
    // Picks ONE already-placed pEvents entry at random out of those matching `category`
    // (genuinely reads `this` throughout -- confirmed via its own raw disasm, unlike the
    // edge-placement quartet below). `category` is a combined selector, not just a descriptor
    // category: -1 = any placed object, 0/1/4 = category-3 track objects in track state 3
    // filtered by pOwningTrain (0 = unowned, 1 = the global-connector sentinel, 4 = any),
    // 2 = kinds flagged bCountedInReadyBigObjCount, 3 = the 4 depot orientations, anything
    // else = a literal tile kind id matched on resourceId. Called by
    // LoadLayoutAndPopulateBoard's train-spawn loop with category 3. See the definition in
    // src/NetSessionEventQueue.cpp for the full writeup.
    void *PickRandomBigObjByCategory(int category); // 0x41e1f0

    // 0x41e120 -- rebuilds the world board from the nPlacedObjectCount entries already in
    // pEvents: dirty-marks the board, plays UI sound 0x5026, clears both effect-spawner
    // lists and tears down every peer-train slot, then (bracketed by a placement-cursor
    // capture grab/release) walks the list handing each entry to
    // WorldBoardMaybe::FlushQueuedBigObjMaybe, running one EffectSpawner tick plus a full
    // UpdateDirtyTiles pass every time the draining count crosses a multiple of 10. Callers:
    // TutorialWnd::OnExit's default arm and BuildToolButton::HandleMenuCommandMaybe.
    // EXACT MATCH (v520). NOT a vtable slot -- a plain member called directly.
    void RebuildBoardFromPlacedObjectsMaybe();

    // Registry create/insert for the world-object class family, dispatched by TileKind
    // category + BigObj::m_type0x63a; see this function's own definition below for the full
    // writeup.
    TilePlacedObjPartial *BigObj_CreateAndInsert(unsigned int kindId);
    // Registry remove/destroy counterpart; see this function's own definition below for the
    // full writeup.
    void BigObj_Remove(TilePlacedObjPartial *pItem, unsigned int bSpawnEffect);
};

extern NetSessionEventQueue g_NetSessionEventQueue; // DAT_004a9990
