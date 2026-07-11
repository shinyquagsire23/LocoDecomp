// NameAnchorMaybe -- a train car's named track-anchor reference (32 bytes). Field names and
// layout per docs/subsystems.md (resolved 2026-07-14 v72: PeerTrainNode::pNameMaybe at +0x20
// is this class; the Phase-2-era "CopyObj0x40b5d0" was a duplicate model of it). Methods
// (address-interleaved in the original): ctor 0x40b500, scalar dtor 0x40b550, FUN_0040b5a0,
// CopyTailFrom 0x40b5d0, ResolveSocketIndexAtPointMaybe 0x40b610,
// NameAnchorMaybe_ResolveTileAnchorMaybe 0x40b740, TryMatchSocketMaybe 0x40b880,
// AdvanceAlongTrackMaybe 0x40bbd0 (ex-FUN_0040bbd0). Everything from the ctor (0x40b500)
// onward is transcribed in src/NameAnchorMaybe.cpp except the scalar dtor (blocked on the
// header dependency described at pVtbl below) and FUN_0040b5a0, which is still declared-only.
#pragma once

class TrackTileObj;          // fwd -- src/TilePlacedObj.h
#include "PeerTrainNode.h"    // PeerTrainNodePartial (+ PeerTrainNodeTrackPartial below)

// PeerTrainNodePartial subclass view adding the 3 extern method declarations
// AdvanceAlongTrackMaybe calls. Kept OUT of the shared header: adding them there rotated
// DPlaySessionMgr.cpp's TU codegen and broke SelectGridCellFromPointMaybe's EXACT match
// (Yoda #23 TU-context sensitivity, confirmed by revert/re-add bisect v325). Same layout,
// no fields added -- a per-consumer view, same precedent as the project's other
// partial-class views.
struct PeerTrainNodeTrackPartial : PeerTrainNodePartial {
    unsigned char CanReverseDirectionMaybe();                // 0x44d630 -- transcribed in
                                                             //   src/PeerTrainNode.cpp (Tick view)
    void TryTransitionModeMaybe(int bDirection, char param); // 0x44d5e0 -- ditto
    unsigned char ApplyLeadTileTickStateMaybe();                            // 0x44ca50 -- ditto
    unsigned char HasCarCategory2Maybe();                    // 0x44c370 -- also viewed
                                                             //   TU-locally by src/WalkerActor.cpp
};

struct PeerTrainNodeCarAdvancePartial; // fwd -- TU-local, src/NameAnchorMaybe.cpp
struct CarNetObj;           // fwd -- src/CarNetObj.h (pointer-only use below)

// Padded-vtable probe to reach NameAnchorMaybe's own slot 0, the scalar-deleting destructor at
// 0x40b550. Needed because this class models its vptr as a plain `pVtbl` field rather than a
// real `virtual ~NameAnchorMaybe()` -- see the note on that field below for the header blocker
// that forces it -- so a plain `delete pAnchor` would emit a bare operator delete instead of
// the original's `(**pVtbl)(1)`. Same precedent, and the same one-slot shape, as
// PeerTrainNodeVtblProbe (src/PeerTrainNode.h) and CarNetObjVtblProbe (src/CarNetObj.h).
// The one consumer is CarNetObj::~CarNetObj, which releases both of a car's anchors.
struct NameAnchorVtblProbe {
    virtual void ScalarDeletingDtor(int bFreeMemory); // vtbl+0x0 (slot 0)
};

struct NameAnchorMaybe {
    // +0x00 -- really this class's own vptr (single-slot vtable 0x477564: the dtor and nothing
    // else, ground-truthed against the .rdata boundary -- 0x477568 starts a different class's
    // table). Modeled as an explicit field rather than a real `virtual ~NameAnchorMaybe()`,
    // which is what it is, ONLY because of a header-dependency blocker; nothing reads it.
    //
    // ⚠ THE SCALAR DELETING DTOR (0x40b550, 70 B) IS BLOCKED ON THAT, not on being understood.
    // Its body is fully read: `if (pTileMaybe != NULL && !IsNetShuttingDownMaybe()) {
    // pTileMaybe->nOccupantRefCount--; pTileMaybe = NULL; }` then the delete-flag test and
    // `return this` -- release this anchor's claim on its tile, but NOT while the app is already
    // tearing down and the tiles are being destroyed out from under the anchors (sic). The
    // original has exactly ONE dtor COMDAT (the scalar deleting form), so it has to be written
    // IN-CLASS (SoundBankEntry's precedent in src/DSoundChannel.h) -- an out-of-line body emits a
    // separate `??1` beside the `??_G` thunk and there is no second COMDAT for it to pair with.
    // An in-class body needs TrackTileObj COMPLETE here, i.e. `#include "TilePlacedObj.h"` in
    // this header, which drags WidgetBase.h + the position-sensitive CursorDesc.h into
    // src/WalkerActor.cpp and src/PeerTrainNode.cpp. Not attempted for 70 B; measure the full
    // per-file table before ever trying it.
    void *pVtbl;
    int dwUnk0x04;          // +0x04 -- ctor always sets 1; doubles as the anchor's
                            //   socket-step direction selector (1 = next socket, 0 = prev)
    int nSocketIndexMaybe;  // +0x08
    int nAnchorXMaybe;      // +0x0c
    int nAnchorYMaybe;      // +0x10
    TrackTileObj *pTileMaybe; // +0x14 (the TilePlacedObj-family placed tile the anchor
                              //   sits on -- always a track-family leaf at runtime)
    // +0x18/+0x1c -- RESOLVED v75: a MUTUALLY EXCLUSIVE pair of extend-phase trackers keyed
    // on which of 2 disjoint tile-kind families (m_type0x63a in {1,2,3,4} vs {7,8,9,10}) the
    // anchor's current tile belongs to. dwViewportExtendPhaseMaybe (kind-1..4 tiles): 0=idle,
    // 1=socket match failed, fixed-direction advance; 2=bound crossed, settling; 4=extending
    // across the GLOBAL viewport edge (checked against g_worldBoard's own bound, cleared once
    // back on-screen). dwTileExtendPhaseMaybe (kind-7..0xa tiles): 0=idle, 1=claimed a new
    // tile, advancing across it; 4/5=extending across the TILE's own footprint boundary (via
    // ExtendAcrossTileBoundaryMaybe; 4 is checked but never SET by either AdvanceAlongTrackMaybe
    // or its per-car sibling AdvanceCarAnchorMaybe in the shipped binary -- both only ever
    // write 1 or read the 4/5 pair, see src/NameAnchorMaybe.cpp).
    int dwViewportExtendPhaseMaybe;
    int dwTileExtendPhaseMaybe;

    // Declared-only ctor (real body 0x40b500) so `new NameAnchorMaybe(...)` reproduces the
    // /GX new-alloc protection + ctor call in PeerTrainNode's own ctor -- same declared-only-ctor
    // precedent as PeerTrainNodePartial's in src/PeerTrainNode.h.
    NameAnchorMaybe(char bPeerOwned);  // 0x40b500
    // 0x40b740, extern -- seat this anchor on the world-board tile under a PIXEL point (both
    // coords converted with the project's `< 0 ? -1 : >> 4` pixel-to-tile idiom), then re-derive
    // nSocketIndexMaybe/nAnchorXMaybe/nAnchorYMaybe from the tile kind's socket table. Returns 0
    // when no tile is there. nUnused is a genuine dead stack parameter -- the body never reads
    // it, but its own `ret 0xc` proves the third slot is real (Ghidra's 2-param signature was
    // wrong; same dead-but-real-parameter tell as ComputeHeadingAngleMaybe's own nUnused).
    unsigned char NameAnchorMaybe_ResolveTileAnchorMaybe(int nX, int nY, int nUnused);

    // 0x40b5a0, extern (Ghidra: NameAnchorMaybe::FUN_0040b5a0) -- give up this anchor's claim on
    // its tile: decrement the tile's nOccupantRefCount and drop pTileMaybe. Skipped entirely once
    // g_nScreenState has reached 10, the shutdown value -- the same "do not touch tiles that are
    // already being destroyed" guard the scalar dtor carries (see the pVtbl note above; the two
    // bodies are the same code).
    void ReleaseTileClaimMaybe();

    void CopyTailFrom(NameAnchorMaybe *src); // 0x40b5d0
    // 0x40bbd0 -- advance this anchor one socket step along its tile (see the .cpp plate
    // comment); returns a continue-flag the tick loop and the reversal re-walk loop consume.
    unsigned char AdvanceAlongTrackMaybe(PeerTrainNodeTrackPartial *pTrain);

    // 0x40b880 -- try to hand this anchor over to the track tile under its current socket point
    // (see the .cpp plate comment). ⚠ RETURN POLARITY IS INVERTED from the name: NULL means the
    // hand-over SUCCEEDED and the anchor now sits on the neighbour; a non-NULL tile means it did
    // NOT, and is the tile that blocked it (the neighbour that refused, or this anchor's own
    // unchanged pTileMaybe). Every caller's `!= 0` arm is therefore the BLOCKED path.
    TrackTileObj *TryMatchSocketMaybe();

    // Extern siblings called by AdvanceAlongTrackMaybe (all address-interleaved with it):
    unsigned char TryBeginCouplingWaitMaybe(PeerTrainNodeTrackPartial *pTrain);   // 0x40c3d0, extern
    // 0x40c460 -- the points/switch arm of AdvanceAlongTrackMaybe's dwTrackState == 2 case: flip
    // the tile's branch selection (dwTrackTickState 4 <-> 5) and retry the socket hand-over,
    // reverting the flip when it still fails. See the .cpp plate comment.
    unsigned char TryAdvanceAcrossPointsMaybe(PeerTrainNodePartial *pTrain,
                                              TrackTileObj *pTile);
    unsigned char ExtendAcrossTileBoundaryMaybe();                           // 0x40cb10, extern

    // 0x40b610, extern -- re-derive nSocketIndexMaybe (and the matching anchor coordinate) from
    // a point, by scanning the tile kind's packed socket table for the entry whose tile-relative
    // offset matches. Returns non-zero when a socket matched.
    unsigned char ResolveSocketIndexAtPointMaybe(int nX, int nY);
    // 0x40cd60 -- flip this anchor's socket-step direction (dwUnk0x04) and re-seat BOTH extend
    // phases for the new direction; called when the train reverses. See the .cpp plate comment.
    void ApplyDirectionReversalMaybe(PeerTrainNodePartial *pTrain);

    // 0x40cc20 -- out-of-line twin of the viewport-bound check AdvanceAlongTrackMaybe inlines:
    // for a kind-1..4 connector tile, arm dwViewportExtendPhaseMaybe = 2 once the anchor point
    // has crossed the global viewport bound for that tile's own direction. The phase == 1 gate
    // is the CALLER's here, unlike the self-gating tile-side twin below.
    void CheckAnchorCrossedViewportMaybe();
    // 0x40cc90 -- tile-side twin of the above: self-gated on dwTileExtendPhaseMaybe == 1, tests
    // a kind-7..0xa depot tile's OWN footprint bound instead of the global viewport, and arms
    // dwTileExtendPhaseMaybe = 2.
    void CheckAnchorCrossedTileBoundMaybe();

    // 0x40c580 -- per-car sibling of AdvanceAlongTrackMaybe (see .cpp plate comment): a
    // trimmed-down version for one of a CarNetObj's own 2 anchors, with no points-crossing/
    // coupling-wait sub-calls. Reads the TRAIN's mode fields (pTrain) but writes the CAR's own
    // mode fields back (pCar).
    unsigned char AdvanceCarAnchorMaybe(PeerTrainNodeCarAdvancePartial *pTrain, CarNetObj *pCar);
};
