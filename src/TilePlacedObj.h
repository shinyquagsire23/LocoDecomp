// TilePlacedObj and its 3 track-family leaves (TrackTileObj/
// TrackConnectorTileObj/TrackDepotTileObj) -- the world-object class family
// allocated by BigObj::BigObj_CreateAndInsert (src/NetSessionEventQueue.cpp) and placed
// onto WorldBoardMaybe's own tile array. See docs/subsystems.md's BigObj/TilePlacedObj
// entry for the full field-identification writeup this header is based on. This is the
// canonical shared header for the family (CLAUDE.md's "never duplicate a struct across TUs"
// rule) -- every consumer includes THIS header rather than declaring its own local view.
//
// Real inheritance chain, ground-truthed by each ctor's own direct, named call to its base
// type's ctor (CLAUDE.md's "read the ctor literally" signal): TilePlacedObj embeds
// AnimDescRefObj0x477488 (src/WidgetBase.h) at offset 0; TrackTileObj extends
// TilePlacedObj; TrackConnectorTileObj and TrackDepotTileObj BOTH extend
// TrackTileObj directly (NOT each other -- their own ctors independently call
// TrackTileObj::TrackTileObj, confirmed via Ghidra decompile of both 0x44f210 and
// 0x412870).
#pragma once

#include "WidgetBase.h"
#include "CursorDesc.h" // BigObj

class PeerTrainNode;
class DecorActorBase; // src/DecorActor.h -- the ambient world actors this object spawns
struct BigObjSeqRecordMaybe; // src/BigObjSeqRecordMaybe.h -- the InsertSeq/MobileSeq/TotalVisits
                             // goal record the two methods below act on
class TilePlacedObj;
struct PeerTrainNodePartial; // src/PeerTrainNode.h -- the real, modeled train node

// One tile's slot in ONE of the world's two navigation graphs. WorldBoardMaybe builds TWO
// TrackGraphs (pTrackGraphAMaybe / pTrackGraphBMaybe at +0x52488/+0x5248c,
// src/WorldBoardMaybe.h) and every placed tile carries a slot for each, so the same tile can
// sit on both networks at once. Identified 2026-07-26 by transcribing RoadVehicleActor, whose
// pathfinder is WalkerActor's with every one of these three offsets shifted up by exactly 0x24
// and pTrackGraphBMaybe in place of pTrackGraphAMaybe -- i.e. the same code walking the other
// graph's slot. The two slots are the LAST thing in the class, and that is what pins the split:
// +0xc4 plus two 0x24-byte slots comes to exactly the 0x10c the allocator asks for.
//   slot A (+0xc4) -- graph A, the paths\* network (category 0xc), walked by WalkerActor
//   slot B (+0xe8) -- graph B, the roads\* network (category 0xd), driven by RoadVehicleActor
// TilePlacedObj +0x88 -- this object's position in the world TILE grid (pixel = value << 16ths).
// A struct rather than two loose members, pinned 2026-07-25 by TilePlacedObj's implicit copy
// constructor (inlined into Obj0x477758Base::SetCopyAtMaybe, 0x412140): it copies +0x88 with a
// single DWORD move, and that compiler never coalesces two adjacent scalar members -- the three
// adjacent bytes at +0x8c/+0x8d/+0x8e in the very same copy stay three separate byte moves.
// Independently corroborated by the "pair-copy" idiom already documented in
// src/PeerTrainNode.cpp and src/NameAnchorMaybe.cpp, where the original hands this pair around
// as ONE dword (`*(unsigned int *)&pTile->wPosX`) -- that reads as a plain struct assignment in
// the original source. Folding PeerTrainNode's own +0x7e and wClaimedPosXMaybe pairs onto this
// same type (which would retire those casts) is a clean follow-up, not yet done.
//
// SIGNED, settled 2026-07-27 after standing open since v413 (both members were `unsigned short`
// until then, on no evidence beyond the Hungarian `w`). The discriminating site is
// WorldBoardPartial::CheckInsertSeqPerimeterMaybe (0x456d90), whose fourth perimeter walk needs
// `start.wPosY - 1` in an INT context to compare against a `short` loop variable: the original
// widens it with `movsx eax, WORD PTR [esp+0x2a]`, and the unsigned model compiles instead to
// `mov eax, DWORD PTR [esp+0x22]; and eax, 0xffff`. That is v463's widening-instruction oracle
// firing on the one function in the repo that reaches for a raw member in a signed int context
// rather than through an already-`short` local. Flipping both members scored that function
// 308029 -> 87078 and was measured byte-neutral across every other row of progress.py's
// per-file table, so nothing else in src/ can currently tell the two models apart.
struct TileGridPos {
    short wPosX;
    short wPosY;
};

// A wrapper node in a connector/depot tile's own FIFO of trains waiting to claim/couple at this
// tile once it frees up (+0x124 on both TrackConnectorTileObj/TrackDepotTileObj). Not itself
// transcribed yet -- both leaf dtors (0x44f2c0/0x4128d0) walk-and-delete this list, EnqueueClaimWaiterMaybe
// (called from PeerTrainNode::PeerTrainNode_UpdatePlacementTickMaybe's non-global-connector
// branch) appends to its tail, RemoveClaimWaiterMaybe removes a specific node by (wTrainId, byte), and
// PumpClaimWaitQueueMaybe (a per-tick pump) pops the head once pOwningTrain==0 and hands the popped train to
// PeerTrainNode::ClaimDecorObjMaybe. Deliberately NOT named pPendingTrainQueueHead -- that exact
// identifier is DPlaySessionMgr's own, conceptually-similar-but-structurally-distinct session-level
// queue (src/DPlaySessionMgr.h).
struct TrainClaimWaitNode {
    // PeerTrainNodePartial, not the opaque fwd-only `PeerTrainNode`, for the same reason
    // KeyedList_AddByKeyMaybe takes that type below: the removal half has to compare the node's
    // own wTrainId/bOwnerByteA, and a cast at that read site buys nothing. Same object either
    // way -- `PeerTrainNode` is a never-defined handle type.
    PeerTrainNodePartial *pTrain;
    TrainClaimWaitNode *pNext;
};

// 0x10c/268 bytes; ctor 0x4580a0 (was InitFieldsMaybe). The generic/non-track placed-object
// base -- used directly for every non-track (TileKind_GetCategory(kindId) != 3) kind id.
class TilePlacedObj : public AnimDescRefObj0x477488 {
public:
    TileGridPos pos; // +0x88 -- tile-grid position (pixel = value<<4), set by
                     // WorldBoardMaybe_PlaceObjectMaybe after registration
    // +0x8c -- how many owned-actor SLOTS this instance has, i.e. how big a staff it hires:
    // the ctor rolls it as `rand() % pKindDesc->bSpawnVariance + 1` (0 when the descriptor's
    // own variance is 0). Named 2026-07-25 from SpawnOwnedActorMaybe (0x458430), which uses it
    // as the loop bound over apOwnedActorMaybe and refuses to spawn anything at all when it is
    // 0; it was `bSpawnOffset`, "a random per-instance offset seed", while only the ctor had
    // been read.
    unsigned char bOwnedActorSlotsMaybe; // +0x8c
    // +0x8d -- how many of those slots are currently filled with a LIVE actor: incremented by
    // SpawnOwnedActorMaybe on each actor that survives its post-placement CanStandAtMaybe test
    // (was `bCarSlotOccupiedCount`, a guess made while only the ctor's own zeroing was known).
    unsigned char bLiveOwnedActorsMaybe;      // +0x8d
    // +0x8e/+0xa4 -- the actors this placed object currently owns. Identified 2026-07-25 from
    // DecorActorBase::~DecorActorBase (src/DecorActor.cpp), which scans exactly 5 slots at
    // +0xa4 for itself, clears the slot it finds, and decrements +0x8e. Pairs with the kind
    // descriptor's own bMaxEmployees (+0x516) / aPossibleMinifigs (+0x524) /
    // the two shift-time sub-objects at +0x534/+0x548 (src/CursorDesc.h) -- i.e. a building
    // employs up to 5 minifigs and each of them points back here via
    // DecorActorBase::pOwnerObjMaybe.
    unsigned char bSpawnedActorCountMaybe; // +0x8e (zeroed by the ctor)
    // +0x8f is a REAL one-byte gap, not a member. The ctor's own two 20-byte bulk zeroes start
    // at `lea eax,[esi+0x90]` / `lea edx,[esi+0xa4]` (raw disasm at 0x458167/0x458197), which
    // only lands right if +0x8f is skipped. It used to be carried as an explicit `pad0x8f`
    // member, which put the offsets right but the SHAPE wrong: the implicit copy constructor
    // inlined into Obj0x477758Base::SetCopyAtMaybe (0x412140) copies every member and skips
    // every padding byte, and the original skips +0x8f. It is genuine 4-byte alignment padding
    // in front of the DWORD-aligned array below -- which is also why that array cannot be the
    // `unsigned char[20]` it was modeled as (that would sit at +0x8f and copy as 20 byte moves;
    // the original copies it with one `rep movsd` of 5).
    // +0x90 -- the actors this object OWNS: five slots of DecorActorBase*, registered with the
    // DecorObjMgrMaybe singleton. Identified 2026-07-25 from ~TilePlacedObj (0x458270), which
    // walks all five and hands each non-null one to DecorObjMgrMaybe::DeregisterEntryMaybe
    // before clearing the slot -- so these are registry entries, not the mere back-references
    // apSpawnedActorMaybe holds. (Carried as `int pad0x90[5]` until then; the copy constructor
    // inlined into Obj0x477758Base::SetCopyAtMaybe had already pinned it as DWORD-wide.)
    DecorActorBase *apOwnedActorMaybe[5];   // +0x90 .. +0xa3 (zeroed by the ctor)
    DecorActorBase *apSpawnedActorMaybe[5]; // +0xa4 .. +0xb7 (zeroed by the ctor)
    // +0xb8 -- the goal-rule cooldown deadline, in game ticks (zeroed by the ctor). RESOLVED
    // 2026-07-25 from FUN_00434d70, the per-object goal tick: it runs the rule only while
    // `cooldown <= g_dwGameTick + pKindDesc->lEEReplayDelay`, and stamps
    // `g_dwGameTick + 0x3c` here the moment a goal fires -- so a rule cannot re-fire for 60
    // ticks, less whatever the descriptor's EEReplayDelay token buys back.
    unsigned int dwSeqGoalCooldownUntilMaybe; // +0xb8
    unsigned int dwPlacementArgB;       // +0xbc, unmodeled (zeroed by the ctor)
    unsigned char bSaveableFlag;      // +0xc0 -- "ready"/valid flag; ctor sets 1, read/cleared
                                      // elsewhere (src/NetSessionEventQueue.cpp)
                                      // +0xc1 .. +0xc3 is alignment padding, NOT a member --
                                      // 0x412140's memberwise copy steps straight from +0xc0 to
                                      // +0xc4 (it used to be carried as an explicit pad0xc1[3],
                                      // which that copy then dutifully reproduced)
    // +0xc4 .. +0x10b -- this tile's slot in EACH of the world's two navigation graphs.
    // WorldBoardMaybe builds TWO TrackGraphs (pTrackGraphAMaybe / pTrackGraphBMaybe at
    // +0x52488/+0x5248c, src/WorldBoardMaybe.h) and every placed tile carries a slot for each,
    // so the same tile can sit on both networks at once. The A group is graph A, the paths\*
    // network (category 0xc) WalkerActor walks; the B group is graph B, the roads\* network
    // (category 0xd) RoadVehicleActor drives. Identified 2026-07-26 by transcribing
    // RoadVehicleActor, whose pathfinder is WalkerActor's with every one of these three offsets
    // shifted up by exactly 0x24 and pTrackGraphBMaybe in place of pTrackGraphAMaybe -- i.e. the
    // same code walking the other graph's slot. The two groups are the LAST thing in the class,
    // and that is what pins the split: +0xc4 plus two 0x24-byte groups comes to exactly the
    // 0x10c the allocator asks for.
    //
    // SIX FLAT MEMBERS, not the `TileGraphSlot` struct (nor the `TileGraphSlot[2]` array) they
    // were modeled as until 2026-07-25. 0x412140's memberwise copy walks all six individually
    // -- 16-byte inline block, 16-byte inline block, dword, and again -- whereas ANY struct
    // spelling makes the group a single POD member that VC5 copies bitwise, as one `rep movsd`
    // of 9 per struct member (or 0x12 for the whole array). The 16-byte RECT members earlier in
    // the same copy show the same rule from the other side: a POD struct member of 16 bytes or
    // less block-copies inline, above that it goes to `rep movsd`. Still Maybe-rung: the
    // direction->compass mapping itself is unread.
    //
    // The adjacent connected tile in direction d, 0 if none (reverse == (d + 2) & 3). Also
    // zeroed as a group of 4 by BigObj_Remove on a paired-tile hand-off, see
    // src/NetSessionEventQueue.cpp.
    TilePlacedObj *apNeighbourTileAMaybe[4]; // +0xc4 .. +0xd0
    // That link's cost, 0 meaning "no link" -- so the argmin both pathfinders run skips zeros
    // and treats a zero incumbent as worse than any nonzero.
    unsigned int anNeighbourCostAMaybe[4];   // +0xd4 .. +0xe0
    // This tile's TrackGraph node index within that graph, -1 when the tile isn't a graph node
    // (a plain run of path/road between junctions) -- which is exactly the case that sends both
    // pathfinders to the cost argmin instead. The ctor sets both graphs' copies to -1.
    int nGraphNodeIdAMaybe;                  // +0xe4
    TilePlacedObj *apNeighbourTileBMaybe[4]; // +0xe8 .. +0xf4
    unsigned int anNeighbourCostBMaybe[4];   // +0xf8 .. +0x104
    int nGraphNodeIdBMaybe;                  // +0x108

    TilePlacedObj(unsigned int kindId);
    // 0x4583c0 -- this object's FOOTPRINT rect in world pixels: the kind descriptor's own
    // local footprint rect (BigObj +0x61c) offset by this object's own rect.left/top.
    // Returns 0 (leaving *pRect empty) when there is no descriptor or the descriptor's
    // footprint rect is itself empty.
    unsigned char GetFootprintRectMaybe(RECT *pRect);
    // 0x458310 -- the tile position of this object's FRONT row: its own pos, moved back up by
    // the descriptor's bitmap-occupancy depth minus its footprint depth. Returned BY VALUE (as
    // one dword), which is what pins `pos` as a struct rather than two loose shorts.
    TileGridPos GetFrontRowTilePosMaybe();
    // 0x458350 -- entry/exit point nIndex (0..3, one per edge) in WORLD pixels: the kind
    // descriptor's own local aEntryExitMaybe pair offset by this object's rect.left/top.
    // Returns {-1,-1} when there is no descriptor, or when that edge's pair is itself {-1,-1}.
    POINT GetEntryExitPointMaybe(int nIndex);
    // 0x458820 -- "the goal in pRec was just met": retarget this object to the record's
    // replacement descriptor (or, when that id is 0, just poke its anim through slot 7), then
    // -- unless another object is already selected, or the world widget is sitting in mode 3 --
    // play the reward sound and select this object.
    void ApplySeqRecordChangeMaybe(BigObjSeqRecordMaybe *pRec);
    // 0x4588b0 -- the effect half of the same reward: spawn the record's effect at its point,
    // resolved in whichever coordinate space lSpaceCharMaybe names.
    void SpawnSeqRecordEffectMaybe(BigObjSeqRecordMaybe *pRec);
    // This class's own scalar-deleting destructor is the real free-block-size source for
    // `pObj->ScalarDelete(1)` (src/NetSessionEventQueue.cpp), so the vtable it stamps must be
    // distinct from AnimDescRefObj0x477488's own.
    virtual ~TilePlacedObj();
    // slot 15 (+0x3c) -- 0x458430, a real OVERRIDE of AnimDescRefObj0x477488's own slot-15
    // body (0x4062a0). "Hire one more owned actor of kind kindId": picks a free slot in
    // apOwnedActorMaybe, a random standing spot inside this object's footprint rect, and asks
    // DecorObjMgrMaybe to build and register the actor there.
    virtual DecorActorBase *SpawnOwnedActorMaybe(int kindId);
    // slot 16 (+0x40) -- 0x458800.
    virtual unsigned char ResetToBaseSubFrameMaybe();
    // slot 17 (+0x44) -- 0x458810, the family's do-nothing default.
    virtual unsigned char OnPlacedObjEventMaybe();
    // slot 18 (+0x48) -- 0x458940. Re-runs this object's OPENING-HOURS test against the wall
    // clock and swaps its subframe between the descriptor's closed-frame-set index
    // (wClosedFS) and its default active one (wActiveFrameSetIndex) whenever the answer
    // has changed.
    virtual void UpdateOpeningHoursFrameMaybe();
};

// 0x11c/284 bytes; ctor 0x44ae80 (was InitFieldsMaybe). The track family's default/fallback
// leaf, used when m_type0x63a is in neither IsType0x63aInSet1234 nor IsType0x63aInSet.
class TrackTileObj : public TilePlacedObj {
public:
    unsigned int dwTrackState;     // +0x10c -- 0-8 classification, see docs/subsystems.md
                                         // for the full m_type0x63a -> state enum
    unsigned int dwTrackTickState; // +0x110 -- 0-5, tri-state at runtime (0/1/2 only)
    unsigned short nOccupantRefCount; // +0x114
    unsigned char pad0x116[2];             // +0x116 .. +0x117, alignment gap before the pointer
    PeerTrainNode *pPendingCoupleWaiter; // +0x118

    TrackTileObj(unsigned int kindId);
    // 0x44b050, defined in src/TilePlacedObj.cpp -- hands back every train slot overlapping this
    // tile, then chains ~TilePlacedObj. Its `??_G` scalar-deleting thunk is 0x44b030.
    virtual ~TrackTileObj();
    // ⚠ This leaf's two OWN virtual overrides -- slot 7 (+0x1c) ReleaseChannelAndDispatch at
    // 0x44b130 and slot 16 (+0x40) ResetToBaseSubFrameMaybe at 0x44b0b0 -- are deliberately NOT
    // declared here, and that is measured, not stylistic. Declaring the pair costs 1667 B of
    // exact matches across three TUs that neither declare nor call them: src/WorldBoardMaybe.cpp's
    // 0x457ce0 (−951 B) and src/Obj0x477798Family.cpp (−152 B; recorded as
    // src/PlacedObjRegistryMaybe.cpp until v532 measured it), both already named in the
    // EnqueueClaimWaiterMaybe note below, plus src/RoadVehicleActor.cpp (−504 B), which that note
    // does not list -- so the N=5..7 "both EXACT" window it records is STALE and must not be
    // trusted as a way to buy these in. They live on TU-local views in src/TilePlacedObj.cpp
    // instead, the same escape hatch TrackConnectorTileObjPumpView0x44f340 uses there.

    // 0x44f3a0, extern -- park pTrain on this tile's claim-wait FIFO: copy the tile's own grid
    // position into the train's wSentinelCMaybe/DMaybe pair, drop the train into dwModeAMaybe 2
    // and sound state 0, then `new` an 8-byte TrainClaimWaitNode and append it to the +0x124
    // tail. The CONNECTOR-side twin of TrackDepotTileObj::KeyedList_AddByKeyMaybe (which
    // appends the same node shape to the same offset on the depot leaf), and the producer whose
    // consumer is PumpClaimWaitQueueMaybe.
    //
    // ⚠ THIS HEADER'S DECLARATION COUNT IS A LIVE CODEGEN DIAL for two TUs that neither declare
    // nor call anything added here: src/WorldBoardMaybe.cpp's 0x457ce0 (951 B) and a 152 B second
    // victim. ⚠ v532 re-measured that second victim by actually adding a real declaration
    // (PumpClaimWaitQueueMaybe) and reading the full per-file table: it is
    // src/Obj0x477798Family.cpp, NOT src/PlacedObjRegistryMaybe.cpp as recorded below and on the
    // sibling note above. The 951 B + 152 B = 1103 B total is confirmed. Measured 2026-07-28 by adding N dummy member
    // declarations on top of this session's four real ones, everything else held fixed:
    // N=0 both EXACT, N=1 0x457ce0 DIFF / the other EXACT, N=2..4 the same, N=5..7 both EXACT
    // again, N=8..9 0x457ce0 DIFF. So it is neither a 1-bit parity nor a clean period, and the
    // usable window is not even one declaration wide in the safe direction. It also does not
    // depend on this header alone -- an earlier sweep this session, taken before
    // src/NameAnchorMaybe.h gained ReleaseTileClaimMaybe, put the boundaries in different places.
    // Re-measure with a FULL tools/progress.py before adding or removing anything here; a
    // per-file tools/cc.sh on the TU you are working in will not show it.
    //
    // Declared on the shared BASE rather than on either leaf: PeerTrainNode's own
    // UpdatePlacementTickMaybe and PeerTrainSlotQueueMaybe::ClaimSlotForTrain both reach it
    // through a PickRandomBigObjByCategory / GetPlaneASlotMaybe result whose leaf type is not
    // known at the call site. The +0x124 field it walks is currently modeled on each leaf
    // separately; that duplication -- not this declaration -- is what should move down to the
    // base once a tile dtor is transcribed and pins the layout.
    // Returns 0, always. The return value is NOT a modelling guess: declared `void` this body
    // compiles to 97 bytes against the original's 101, and the 4 missing bytes are exactly the
    // two `xor al,al` epilogue zeroings. No caller reads it.
    unsigned char EnqueueClaimWaiterMaybe(PeerTrainNodePartial *pTrain);

};

// 0x128/296 bytes; ctor 0x44f210 (was InitFieldsMaybe). m_type0x63a in {1,2,3,4} (the 4 global
// map-edge/multiplayer-connector tunnel directions, IsType0x63aInSet1234).
class TrackConnectorTileObj : public TrackTileObj {
public:
    unsigned int dwOccupancyClaim; // +0x11c -- 0 = free; PeerTrainNode::
                                         // UpdatePlacementTickMaybe writes 1 on train hand-off
                                         // (confirmed via that function's own raw disasm: `mov
                                         // ebx,1` .. `mov [edi+0x11c],ebx`); ctor always writes
                                         // 0 here (unclaimed at construction)
    void *pOwningTrain;        // +0x120 -- ctor writes a literal `(void*)1` sentinel for
                                     // the 4 ITEMKIND_TRACK_GLOBAL_{E,W,N,S} kind ids, else 0;
                                     // PeerTrainNode::UpdatePlacementTickMaybe overwrites this
                                     // with the real PeerTrainNode* owner at runtime (`mov
                                     // [edi+0x120],esi` where esi is that function's own
                                     // `this`). Cross-checked against NetSessionEventQueue.cpp's
                                     // own `QueuedBigObjItemPartial::pOwningTrain` (+0x120)
                                     // and its `== (void*)1` sentinel check, gated on
                                     // `dwTrackState==3` -- 3-way independent confirmation.
    TrainClaimWaitNode *pClaimWaitQueueHeadMaybe;   // +0x124 -- FIFO of trains waiting to claim/
                                            // couple at this tile; see TrainClaimWaitNode above

    TrackConnectorTileObj(unsigned int kindId);
    virtual ~TrackConnectorTileObj(); // not yet transcribed -- see TilePlacedObj's own dtor note

    // 0x44f410 -- the removal half of the +0x124 FIFO: unlink and delete the waiter whose train
    // matches (wTrainId, bOwnerByteA), answering whether one was found.
    // ⚠ MOVED here from the shared TrackTileObj base (where it sat declared-only, alongside
    // EnqueueClaimWaiterMaybe, because that one's callers do not know the leaf type). This one's
    // only caller -- PeerTrainSlotQueueMaybe::DetachFromBoardMaybe -- already holds a
    // TrackConnectorTileObj*, and the body walks the +0x124 field that only the leaves model, so
    // the base was the wrong home. Its `.text` neighbours (the ctor 0x44f210, the dtor 0x44f2c0,
    // EnqueueClaimWaiterMaybe 0x44f3a0 and PumpClaimWaitQueueMaybe 0x44f340) are all connector
    // members too. A MOVE, not an addition -- this header's declaration count is unchanged, which
    // matters for the dial documented on EnqueueClaimWaiterMaybe above.
    // bOwner is UNSIGNED char for the byte-compare reason spelled out on the depot twin.
    unsigned char RemoveClaimWaiterMaybe(unsigned int wTrainId, unsigned char bOwner);

    // NOT declared: TrackConnectorTileObj::PumpClaimWaitQueueMaybe, 0x44f340 -- the CONNECTOR-side
    // per-tick pump, near-twin of TrackDepotTileObj::PumpClaimWaitQueueMaybe (advance the tile's
    // own anim frame, then -- if the tile is unclaimed and a waiter is queued -- claim it, pop the
    // head node, and drive the popped train into LayoutCarAnchorsMaybe / sound state 2 /
    // dwModeAMaybe 4). It has no caller in src/ yet, and this header's declaration COUNT is a
    // live codegen dial for src/WorldBoardMaybe.cpp's 0x457ce0 and src/Obj0x477798Family.cpp.
    //
    // ⚠ THE DIAL IS NOW PRICED, not merely suspected -- v532 transcribed the function, put this
    // declaration in, and measured the full per-file table: 0x457ce0 loses its whole 951 B and
    // src/Obj0x477798Family.cpp a further 152 B, for 1103 B against the 88 B the function itself
    // is worth. The body lives on a TU-local view struct in src/TilePlacedObj.cpp instead (the
    // TrackTileObjTickView0x44b0b0 precedent), which costs nothing here. Do not "just try it"
    // again -- it has been tried and it is 1103 B.
};

// 0x12c/300 bytes; ctor 0x412870 (was InitFieldsMaybe). m_type0x63a in {7,8,9,0xa} (the 4
// depot orientations, IsType0x63aInSet). Same +0x11c/+0x120/+0x124 field shape as
// TrackConnectorTileObj, independently re-initialized (own direct
// TrackTileObj::TrackTileObj base-ctor call, not chained through the Set1234
// sibling), plus one extra trailing byte.
class TrackDepotTileObj : public TrackTileObj {
public:
    unsigned int dwOccupancyClaim; // +0x11c -- see TrackConnectorTileObj's own field
                                         // comment; same semantics, shared by PeerTrainNode::
                                         // UpdatePlacementTickMaybe
    void *pOwningTrain;        // +0x120 -- ctor always writes 0 here (no global-connector
                                     // special case, unlike the Set1234 sibling)
    TrainClaimWaitNode *pClaimWaitQueueHeadMaybe;   // +0x124 -- see TrainClaimWaitNode above
    unsigned char bClaimLockedFlag;   // +0x128, unmodeled (zeroed by the ctor)

    TrackDepotTileObj(unsigned int kindId);
    virtual ~TrackDepotTileObj(); // not yet transcribed -- see TilePlacedObj's own dtor note

    // 0x4129c0, extern -- tries to claim this tile for pTrain (releasing any prior claim via
    // the tile's own vtbl slot 7 + a PeerTrainNode::SetModeBMaybe notify, recording the claimed
    // position); on failure requests a direction reversal or parks in a waiting sound state.
    // Called from NameAnchorMaybe::AdvanceAlongTrackMaybe's IsType0x63aInSet branch (the
    // predicate that selects this leaf). See docs/subsystems.md v72.
    unsigned char TryClaimForTrainMaybe(PeerTrainNode *pTrain);

    // 0x412af0, extern -- appends pTrain to the TAIL of this tile's own
    // pClaimWaitQueueHeadMaybe FIFO (walks to the last node, `new`s an 8-byte
    // TrainClaimWaitNode, links it). The DEPOT-side twin of the connector tile's own
    // EnqueueClaimWaiterMaybe, and the producer whose consumer is PumpClaimWaitQueueMaybe (the per-tick pump that
    // pops the head once pOwningTrain frees up and hands the train to
    // PeerTrainNode::ClaimDecorObjMaybe). Sole caller is ClaimDecorObjMaybe itself.
    // ⚠ The name is Ghidra's and is a MISNOMER -- there is no KeyedList involved, the body only
    // walks this tile's own +0x124 chain. The phantom `KeyedList` NAMESPACE that used to wrap it
    // is fixed (v538, when the body landed: the function now sits in class TrackDepotTileObj with
    // a typed `this`, matching the removal half below). The namespace was free to correct; the
    // METHOD name is not, since callers and docs grep for it.
    // Takes PeerTrainNodePartial*, not the opaque fwd-only `PeerTrainNode`, so the sole
    // caller can pass its own `this` without a cast (a cast of `this` is lint_idiom
    // class F). Same object either way -- `PeerTrainNode` is a never-defined handle type.
    void KeyedList_AddByKeyMaybe(PeerTrainNodePartial *pTrain);

    // 0x412b50 -- the removal twin of KeyedList_AddByKeyMaybe, and the depot-side counterpart of
    // TrackConnectorTileObj::RemoveClaimWaiterMaybe: drop the waiter whose train matches
    // (wTrainId, bOwnerByteA) from this tile's own FIFO. Ghidra had it in the same phantom
    // `KeyedList` namespace as the add half; that half is fixed (it is in TrackDepotTileObj now),
    // the add half's is not. The METHOD name is still Ghidra's and still says `KeyedList` --
    // renaming both to Enqueue/RemoveClaimWaiterMaybe on both sides remains the clean follow-up.
    // ⚠ bOwner is UNSIGNED char (same reasoning as PeerTrainSlotQueueMaybe's own by-owner
    // methods): the body's `pTrain->bOwnerByteA == bOwner` is a bare `cmp [esi+0x78],al` byte
    // compare, which only survives when both operands already have the same type -- a plain
    // `char` parameter promotes the comparison to int and costs a `movsx`.
    unsigned char KeyedList_RemoveByKeyMaybe(unsigned int wTrainId, unsigned char bOwner);

    // 0x412a80 -- the DEPOT-side per-tick pump of the +0x124 FIFO: once this tile's pOwningTrain
    // frees up, pop the head waiter and hand its train to ClaimDecorObjMaybe. The consumer named
    // in the TrainClaimWaitNode note above; its connector twin is
    // TrackConnectorTileObj::PumpClaimWaitQueueMaybe. TRANSCRIBED EXACT 2026-07-31 in
    // src/TilePlacedObj.cpp. ⚠ CORRECTED 2026-07-31 (v536) -- the previous note here concluded
    // "a PLAIN MEMBER, not a vtable slot (it appears in no tile vtable)", and that is WRONG. It IS
    // a vtable slot: this class's own slot 10 (+0x28) override of
    // AnimDescRefObj0x477488::AdvanceAnimFrameMaybe, the family-wide per-frame tick.
    // TrackDepotTileObj's vtable is 0x477848 (its dtor at 0x4128d0 re-stamps it) and the dword at
    // 0x477870 is this function. The old reading rested on both call sites dispatching DIRECTLY,
    // which is ordinary qualified dispatch and is evidence of nothing; the body's first statement
    // chaining `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe()` is what a slot-10 override does.
    // Left declared non-virtual deliberately: the byte-match is unaffected (both callers really
    // are direct), and adding `virtual` here is a declaration-parity change that has to be
    // measured on its own -- see this TU's 0x412940 note for how expensive that can be.
    void PumpClaimWaitQueueMaybe();
};
