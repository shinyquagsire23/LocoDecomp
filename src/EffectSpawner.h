// EffectSpawner (DAT_004fd220) -- the global effect/dirty-rect-handle collection singleton
// (Ghidra's own namespace for 0x423c50 is `EffectSpawner`). The layout IS now read, by the
// constructor (0x4238c0, src/EffectSpawner.cpp's TU-local EffectSpawnerCtorViewMaybe): own
// vtable 0x477ad0 at +0, then three 0x18-byte registry sub-objects -- candidate set +0x4
// (0x477bd0/0x477b78), placed set +0x1c and ghost set +0x34 (0x477b40/0x477ae8). It is NOT
// promoted onto this class: the declaration-count parity below is measured, so this header
// keeps modeling only the entry points consumers dispatch, as bodiless members, so each call
// site's this-in-ecx + explicit-push shape reproduces.
//
// This is the CANONICAL home for the singleton (CLAUDE.md's "never duplicate a struct across
// TUs"). ONE older per-TU partial view still exists and is tagged `// TODO: idiom`
// (src/BuildToolCursorWnd.cpp's WorldDirtyRectCollectionMaybe); migrate it onto this class,
// measuring the TU as it moves. src/AnimEffectObj.cpp's BigObjTrackingSetsAnimPartial was
// retired in v574 -- FREE, per-file table byte-identical repo-wide, like v564's below. It too
// was a live defect and not cosmetic: the ctor's `nShadowId > 0` drop-shadow spawn resolved to
// a symbol defined nowhere, so the port stubbed it and EVERY animated world object's drop
// shadow was silently swallowed (8 lost spawns in one boot-to-world run). Retiring it took the
// port's stub set 257 -> 254 symbols and its runtime stub frontier 5 rows -> 4.
// src/WorldBoardMaybe.cpp's BigObjTrackingSetsPaintView0x456700 was retired in v564 by
// including this header there -- FREE, per-file table byte-identical repo-wide. It was a real
// defect, not cosmetic: a view spelling mangles under the VIEW's class name, so UpdateDirtyTiles'
// three paint calls resolved to symbols defined nowhere -- `xor eax,eax; ret N` stubs in the
// port, i.e. no effects painted on the world board at all. CODEGEN #184. Two more were retired v500
// (src/NetSessionEventQueue.cpp's BigObjTrackingSetsMaybe and src/PeerTrainNode.cpp's
// BigObjTrackingSetsPartial) when 0x423ab0's definition landed in src/EffectSpawner.cpp --
// both moved their call sites to DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe with zero
// per-TU byte drift (measured: both rows unchanged).
//
// ⭐ DECLARATION BUDGET, re-measured v533 and MUCH looser than the v485/v486 note above implies:
// FOUR new METHOD declarations (the tick + the three-way paint family) plus a new
// `#include <windows.h>` landed here in one session at ZERO cost -- every one of the eight
// consumer TUs' rows byte-identical, src/TilePlacedObj.cpp included. What v485/v486 measured at
// -143 B was putting the two COLLECTION MEMBERS (a layout change, which resizes the class for
// all eight) on it; that finding still stands and is why the layout stays in the .cpp's
// TU-local views. The two are different dials -- do not let the member-declaration price talk
// you out of declaring a method. Price the next batch anyway (v486's rule: a budget is a
// property of the current repo-wide parity, not of the header).
#pragma once

#include <windows.h> // RECT, by value in the paint family's signatures

class EffectSpawner {
public:
    // 0x423c50 -- spawns a simple effect object (an AnimEffectObj0x477a90 in practice) at
    // (x, y) and returns it. Real return type int* in Ghidra; every caller stores it as an
    // opaque object pointer.
    void *EffectSpawner_SpawnSimpleMaybe(int nEffectId, short wArg, int x, int y);
    // 0x423d20 -- removes/releases a previously handed-out handle (a spawned effect, or a
    // world-board dirty-rect handle). A real __thiscall method: this in ECX, one stack arg.
    void EffectSpawner_RemoveHandle(void *pHandle);
    // 0x423ab0 -- the positioned/directional sibling of SpawnSimpleMaybe: same effect-id +
    // mobility-flag pair, plus a one-char direction code, the spawn point, and a trailing
    // flag. Declared with the parameter names the five per-TU views above already use.
    void *EffectSpawner_SpawnAtPositionMaybe(int kindId, unsigned short mobilityFlag,
                                             char directionChar, int x, int y,
                                             unsigned char flag);
    // 0x423e00 / 0x423e80 / 0x423f00 -- the PAINT family, one entry point per collection and
    // therefore one per z-plane: simple effects come out of the candidate set (+0x4), in-plane
    // effects out of the placed set (+0x1c) and on-top effects out of the ghost set (+0x34).
    // All three are the same body -- walk the collection, skip holes and any entry that is not
    // both bValid and bReady, and blit it through the family's slot 11 with the CLIP rect and
    // flag they were handed plus the entry's OWN nBlitFlags. Vtable slots 2/3/4 of the
    // singleton's own table (not modeled here -- see the .cpp's EffectSpawnerCtorViewMaybe).
    void PaintSimpleEffectsMaybe(RECT rectClip, char flag);
    void PaintInPlaneEffectsMaybe(RECT rectClip, char flag);
    void PaintOnTopEffectsMaybe(RECT rectClip, char flag);
    // 0x423d70 -- the per-frame tick over BOTH effect collections (+0x34 ghost first, then
    // +0x1c placed): steps every live entry's AnimEffectObj0x477a90::TickMaybe and removes the
    // ones that report themselves finished. This is the singleton's own vtable slot 1, but the
    // class's table is not modeled here (see the TU-local EffectSpawnerCtorViewMaybe in the
    // .cpp, whose `_v01` IS this address) -- a plain __thiscall member emits the same body.
    void EffectSpawner_TickMaybe();
    // 0x423f80 -- broadcast one flag to every live entry in BOTH effect collections (the
    // sub-objects at +0x1c and +0x34): walk each collection through its own vtable (slot 0x2c =
    // entry count, slot 0x20 = entry by index) and dispatch each non-null entry's own slot 0x24
    // with the flag. The build-mode entry path (AppWindow_EnterBuildMode) passes 1, which is what
    // lets parked effects start animating again -- the exact shape DecorObjMgrMaybe::MarkAllEntriesDirtyMaybe
    // uses over ITS two registries. Declared-only.
    void BroadcastToAllEffectsMaybe(bool bFlag);
};
extern EffectSpawner DAT_004fd220;
