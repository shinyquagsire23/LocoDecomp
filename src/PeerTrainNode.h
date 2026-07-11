#pragma once

struct NameAnchorMaybe;    // fwd -- src/NameAnchorMaybe.h
class TilePlacedObj;       // fwd -- src/TilePlacedObj.h (LayoutCarAnchorsMaybe's argument)
class TrackDepotTileObj;   // fwd -- src/TilePlacedObj.h (ClaimDecorObjMaybe's argument)

// Partial view of PeerTrainNode -- the per-train peer/network object. The real Ghidra struct is
// fully modeled (148 bytes / 0x94, with 4 named CarNetObj* car-slot fields at +0x10..+0x1c and a
// bHasDetailFlagMaybe byte at +0x88); the full class is out of scope here (see docs/subsystems.md's
// Train/car network section). Only the fields the current consumers touch are modeled -- extend in
// place as more are discovered. carSlots is modeled as a 4-entry array (not 4 separate named
// fields, unlike the real Ghidra struct) since NetSessionEventQueue's SaveBoardLayout iterates all
// 4 generically.

struct TilePlacedObjPartial; // fwd -- carSlots really points at CarNetObj*, reinterpreted (below)

// Padded-vtable probe to reach PeerTrainNode's own slot 0 (the scalar-deleting destructor) --
// same precedent as CarNetObjVtblProbe (src/CarNetObj.h). GameNet_HandleTrainStateSync calls it
// directly (`(**pVtbl)(1)`) to release a superseded train node found on the active list.
struct PeerTrainNodeVtblProbe {
    virtual void ScalarDeletingDtor(int bFreeMemory); // vtbl+0x0 (slot 0)
};

struct PeerTrainNodePartial {
    void *pVtbl;                       // +0x0 -- vtable (scalar-deleting dtor at slot 0)
    int nDiscardFlag;                  // +0x4 -- 1 == "release this train outright" (delete node);
                                       //   also observed as 2 (a transient spawn-mode value some
                                       //   consumers fold into "discard", others check only ==1)
    int dwReversed;               // +0x8
    unsigned short wCarSlotCount; // +0xc -- number of occupied car slots
                                       //   (GameNet_DrainPendingTrainQueue scans slots 1..N)
    char pad0xe[0x10 - 0xe];           // +0xe
    TilePlacedObjPartial *carSlots[4]; // +0x10 (really CarNetObj*[4]; reused as
                                       //   TilePlacedObjPartial* purely to reach the shared
                                       //   vtbl+0x34 dispatch shape via the same probe class)
    NameAnchorMaybe *pNameMaybe;           // +0x20 -- the train's lead track anchor (see
                                           //   docs/subsystems.md's NameAnchorMaybe entry)
    unsigned short wSelectedCarIdAMaybe; // +0x24 -- car-id endpoint A of the selection pair
                                         //   (WorldActionCursor::HandleMenuCommandMaybe's
                                         //   0x380e/0x3810 select-car commands)
    unsigned short wSelectedCarIdBMaybe; // +0x26 -- car-id endpoint B of the pair
    int dwUnk0x28;                     // +0x28 -- countdown gated by dwSoundStateMaybe==4 in
                                       //   UpdatePlacementTickMaybe (decremented per tick; the
                                       //   reselect-car + reverse block fires when it hits 0)
    unsigned char bUnk0x2c;            // +0x2c -- set to 1 by CarNetObjAnchorPartial::
                                       //   UpdateCarPlacementTickMaybe when both of a car's
                                       //   anchors sit on the same claimed (dwTrackState==5)
                                       //   tile; not yet observed read anywhere
    char pad0x2d[0x2e - 0x2d];         // +0x2d
    short wSentinelCMaybe;             // +0x2e -- with wSentinelDMaybe, the grid position of
    short wSentinelDMaybe;             // +0x30   the tile the train most recently claimed/
                                       //   handed off to; always accessed as ONE dword pair
                                       //   (`*(unsigned int *)&wSentinelCMaybe`, the
                                       //   documented dword-pair-copy idiom -- the field pair
                                       //   sits at UNALIGNED +0x2e, so a real unsigned int
                                       //   member would force padding and shift +0x32 on)
    short wClaimedPosXMaybe;           // +0x32 -- with wClaimedPosYMaybe, a SECOND grid-pos
    short wClaimedPosYMaybe;           // +0x34   pair (Ghidra duplicates the wSentinelCMaybe/
                                       //   wSentinelDMaybe names here): UpdatePlacementTickMaybe's
                                       //   depot-extend claim path copies the just-claimed tile's
                                       //   wPosX/wPosY in as ONE dword (same pair-copy idiom as
                                       //   wSentinelCMaybe), so kept as two shorts
    short wUnk0x36;                    // +0x36 -- wait countdown: armed to ~200 by
                                       //   TryBeginCouplingWaitMaybe, set to 2 on a
                                       //   reversal-park (paired with bUnk0x90 below)
    // +0x38 -- the train's PASSENGER slots, RESOLVED 2026-07-26 (was aUnk0x38Maybe, "8-dword
    // table indexed by the mode-6 decor icon slot"). WalkerActor::BoardTrainMaybe claims the
    // first entry that is 0 or already itself and stores its own `this` there;
    // WalkerActor::LeaveTrainMaybe scans for itself and clears the slot. Both loops are
    // `cmp 0x8 / jb` over indices 0..7, which is what pins the count at 8 (Ghidra's decompile
    // renders them as `< 9` and `< 8` respectively -- a folded-increment artifact, refuted
    // against the raw disasm at 0x433579 / 0x4336ce). This is the same array
    // WorldActionCursor's 0x2c09 mode-6 decor-icon case feeds to
    // SelectedObjWidgetMaybe::SelectObjMaybe -- i.e. clicking a train's minifig icon selects
    // that passenger. Kept `int` (not DecorActorBase*) so the existing 0x2c09 call site keeps
    // its already-established `(int)`-shaped pointer handoff; consumers that need the real
    // type cast at the use, same as that TU's sibling `(int)pTrain->carSlots[0]`.
    int apPassengerMaybe[8];
    unsigned short wSelectedCarId;// +0x58 -- selected car id; copied into a rebuilt train
    unsigned char bUnk0x5a;            // +0x5a -- reversal re-entry guard: set around every
                                       //   ReverseDirectionMaybe call so nested ticks don't
                                       //   re-trigger a reversal
    char pad0x5b;                      // +0x5b
    int dwSoundStateMaybe;             // +0x5c -- train sound/state dword (0/1/2 compared by
                                       //   the 0x380e/0x380f/0x3810 commands)
    int dwModeAMaybe;                  // +0x60 -- set to 2 on a freshly-rebuilt train node
    int dwModeBMaybe;                  // +0x64 -- highlight/coupling-ready flag (5 = highlight
                                       //   per docs/subsystems.md); values 4/5 also gate
                                       //   AdvanceAlongTrackMaybe's state-4-tile claim-release
                                       //   tail. NOT a mode-A/mode-B pair on one axis.
    int nDeferredMoveStateMaybe;       // +0x68 -- peer-handoff deferred-move state: 0 = none,
                                       //   1 = requested (UpdatePlacementTickMaybe on reaching a
                                       //   global-connector tile), 2 = dispatched (see
                                       //   docs/subsystems.md's PeerTrainSlotQueueMaybe entry)
    unsigned char bSmokePuffCounterMaybe;            // +0x6c -- smoke-puff effect cooldown counter (0..10);
                                       //   wraps to 0 (spawning the 0x3883 effect) at 10
    char pad0x6d[0x70 - 0x6d];         // +0x6d
    void *pNext;                  // +0x70 -- pending-train-queue chain link (byte-aliases
                                       //   GameNetQueuedNodeMaybe::pNext at the same offset)
    unsigned short wHeading;      // +0x74 -- NETWORK hand-off heading (0/0x5a/0xb4/0x10e =
                                       //   0/90/180/270deg); picks the board edge/quadrant on
                                       //   cross-board hand-off/blocked-queue reversal. (Was
                                       //   modeled as an int dwHeadingMaybe; split to a word here
                                       //   since RebuildOrEnqueueTrainCars zeroes +0x74/+0x76 as
                                       //   two separate word stores -- readers still movzx it.)
    unsigned short wLocalHeading;      // +0x76 -- LOCAL track-following heading, driven every tick
                                       //   by TrainNet_AdvanceLocalTrainSteps (junction-following)
                                       //   and reset to the reflected heading by
                                       //   TrainNet_TryBoardEdgeHandoffMaybe (0x43c160) when a
                                       //   board-edge placement is blocked;
                                       //   disjoint call sites from wHeading -- NOT dword halves of
                                       //   one legacy field despite the shared zero-init site.
    unsigned char bOwnerByteA;    // +0x78 -- provider-slot index of the train's owner
    char pad0x79[0x7a - 0x79];         // +0x79
    unsigned short wTrainId;      // +0x7a -- train id; arg1 of SetTrainPlacementResult
    unsigned char bOwnerByteB;    // +0x7c -- secondary owner byte; arg3 of the same call
    char pad0x7d[0x7e - 0x7d];         // +0x7d
    // Local movement/stall-detection block, driven by TrainNet_AdvanceLocalTrainSteps: current
    // tile position + a periodic "did we actually move" checkpoint. Position fields are read via
    // movsx (signed) with 0xffff/-1 as an "unset" sentinel.
    short wPosX;                       // +0x7e -- current tile X (sentinel group zeroed on a
                                       //   rebuilt train node)
    short wPosY;                       // +0x80 -- current tile Y
    unsigned char bStallStepCounter;   // +0x82 -- ticks since the last checkpoint re-check;
                                       //   compared against a randomized threshold (rand()/6553+3)
                                       //   to trigger a stall re-check, then reset to 0
    char pad0x83[0x84 - 0x83];         // +0x83
    short wCheckpointPosX;             // +0x84 -- tile X at the last stall checkpoint (0xffff
                                       //   sentinel = unset); compared against wPosX when
                                       //   bStallStepCounter's threshold fires
    short wCheckpointPosY;             // +0x86 -- tile Y at the last stall checkpoint
    unsigned char bHasDetailFlagMaybe; // +0x88 -- GameNet_PostConnectOrJoinForNode sets this
                                       //   so GameNet_ConnectOrJoinSession appends the train to the
                                       //   pending-peer list with a long countdown
    unsigned char bAckCounterA;   // +0x89 -- retry-cooldown byte; the drain pops the first
                                       //   queued node whose value is 0
    unsigned char bAckCounterB;   // +0x8a -- reset to 0 by GameNet_HandleTrainStateSync
                                       //   alongside bOwnerByteB (secondary owner's own retry
                                       //   counter, mirroring bAckCounterA)
    char pad0x8b[0x8c - 0x8b];         // +0x8b
    int dwAckPlayerId;            // +0x8c -- DPID of the peer that acked this train's sync
                                       //   (GameNet_HandleTrainStateAck stores it; RemovePeerTrains
                                       //   matches on it)
    unsigned char bUnk0x90;            // +0x90 -- set to 1 alongside wUnk0x36=2 on a
                                       //   reversal-park (AdvanceAlongTrackMaybe)
    char pad0x91[0x94 - 0x91];         // +0x91 -- (full node is 0x94 bytes)

    // Declared-only ctor (real body 0x44be50) so `new PeerTrainNodePartial(...)` reproduces the
    // /GX new-alloc protection + PeerTrainNode_Ctor call in RebuildOrEnqueueTrainCars.
    PeerTrainNodePartial(int nKindId, int mode, char a, char b);  // 0x44be50
    // 0x44c0d0 -- the ctor's counterpart. NON-virtual on purpose: this class models its vptr as
    // the plain pVtbl field above, so the real vtable slot 0 (the ??_G scalar deleting dtor at
    // 0x44c0b0, reached by consumers through PeerTrainNodeVtblProbe) is the compiler-generated
    // thunk we deliberately leave unclaimed, and this is the single `??1` COMDAT beside it.
    // Body in src/PeerTrainNode.cpp.
    ~PeerTrainNodePartial();
    // 0x44c150 -- move the train into mode-B state `mode` and re-derive the cars' ready flag
    // from it: every state EXCEPT 2 leaves the cars ready, 2 (the claim-wait park) stands them
    // down. Body in src/PeerTrainNode.cpp. Promoted here in v532 from the one-method TU-local
    // PeerTrainNodeClaimPartial view in src/TilePlacedObj.cpp, which was its only consumer.
    void SetModeBMaybe(int mode);
    void SetCarsReady(bool bFlag); // 0x44d500, extern -- PeerTrainNode::SetCarsReady
                                   //   (bool, not char: the body stores bFlag straight
                                   //   into the cars' own bool bReady with no setne)
    unsigned short PeerTrainNode_UpdateSelectedCar(short wCarId); // 0x44d6c0
    // 0x44c220. Return is `unsigned char`, not int: the failure exits `xor al,al` and
    // leave EAX's upper 3 bytes holding the 1 the success path hoisted -- the usual
    // CONCAT31 "caller only reads AL" tell. Every caller discards it anyway.
    unsigned char PeerTrainNode_AllocCarSlot(int nKindId, int nCategory, char bFlag);
    // 0x44c310 -- the AllocCarSlot counterpart: release the car in slot nIndex and compact the
    // slots above it down one place. Same `unsigned char` CONCAT31 return shape as AllocCarSlot,
    // and likewise discarded by every caller.
    unsigned char PeerTrainNode_ReleaseCarSlot(unsigned int nIndex);
    // 0x44c170 -- (re)bind this train to the depot tile it now occupies: snapshot the tile's
    // grid position, take over pOwningTrain (queueing behind the current owner if there is
    // one), then re-lay the car anchors. Body in src/PeerTrainNode.cpp. Promoted here from that
    // TU's local PeerTrainNodeLayoutPartial view when RebuildCarSlotsFromSelectionMaybe
    // (src/PeerTrainSlotQueueMaybe.cpp) became the second TU to call it.
    void ClaimDecorObjMaybe(TrackDepotTileObj *pTile, char bKeepHeading);
    // 0x44ce10 -- re-lay every car's NameAnchorMaybe tile/socket/heading around pTile. Promoted
    // out of the same TU-local view as ClaimDecorObjMaybe above, which is its only caller.
    unsigned char LayoutCarAnchorsMaybe(TilePlacedObj *pTile, char bKeepHeading);
    void SetSoundStateMaybe(int state);                      // 0x44d740
    // 0x44caf0 -- the re-entrancy-guarded wrapper around ReverseDirectionMaybe: refuses (returns
    // 0) if bUnk0x5a is already set, otherwise sets the guard for the duration of the reversal
    // and hands back the reversal's own result. Every reversal in the codebase goes through this
    // guard rather than calling ReverseDirectionMaybe raw, which is what makes bUnk0x5a's
    // "nested ticks can't re-enter" role above concrete. Not itself transcribed yet.
    char ReverseDirectionGuardedMaybe();
};
