// PeerTrainNode -- the per-train peer/network object (0x94 bytes, ctor 0x44be50). This TU
// carries its methods as they get transcribed, starting with LayoutCarAnchorsMaybe. The
// shared partial struct view lives in src/PeerTrainNode.h; see docs/subsystems.md's
// Train/car network section for the full class writeup.

#include <math.h>            // atan -- ComputeHeadingAngleMaybe
#include "TilePlacedObj.h"   // TilePlacedObj/TrackTileObj + BigObj (via CursorDesc.h)
#include "CarNetObj.h"       // CarNetObj
#include "CarKindDesc.h"     // CarKindDesc -- the train/car kind descriptor pKindDesc really is
#include "NameAnchorMaybe.h" // NameAnchorMaybe (+ PeerTrainNodePartial via PeerTrainNode.h)
#include "DSoundChannel.h"   // DSoundChannel::SetPosition
#include "DPlaySessionMgr.h" // g_pDPlaySessionMgr + DPlaySessionMgr::SetTrainPlacementResult
#include "EffectSpawner.h"   // DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe (canonical home)

struct PeerTrainNodeTickPartial; // fwd -- defined below

// PeerTrainNode's own vtable (0x47836c). Only its slot 0 (the scalar-deleting destructor) is
// ever reached, via PeerTrainNodeVtblProbe -- the ctor below just installs the pointer. Same
// extern-array-symbol shape as src/Obj0x477798Family.cpp's own vtable family.
extern void *g_vtable0x47836c[];

// Local METHODS-ONLY view of a car slot. The 0x450-byte layout itself now lives on the shared
// CarNetObj (src/CarNetObj.h), promoted there in v475; what remains here are the per-car tick
// methods, which cannot move up yet because most of them take a PeerTrainNodeTickPartial* and
// that class is still TU-local (defined below).
struct CarNetObjAnchorPartial : CarNetObj {
    // Per-car counterpart of the train tick below (0x40d940, extern -- RET 0x8 confirmed:
    // 2 explicit args, the train and the count of already-advanced cars this tick).
    void UpdateCarPlacementTickMaybe(PeerTrainNodeTickPartial *pTrain, int nMoved);
    // 0x40e440 -- per-car follow-up ReverseDirectionMaybe runs when a car's dwModeBMaybe
    // flips back to 1; the entering-side mirror of CheckCarClearedDepotMaybe below.
    unsigned char CheckCarEnteredDepotMaybe(PeerTrainNodeTickPartial *pTrain);
    // 0x40e130 -- (re)starts the sound the car's current animation frame asks for; the caller
    // SetSoundStateMaybe reaches it on car 0 when a non-paused sound state is entered.
    void StartFrameSoundMaybe();
    // 0x405ab0, extern. REALLY an ordinary (non-virtual) member of the AnimDescRefObj0x477488
    // base (Ghidra: AnimDescRefObj0x477488::EnsureSoundPlayingMaybe), declared on this derived
    // per-consumer view rather than in the shared src/WidgetBase.h -- same header-churn
    // precedent as PeerTrainNodeTrackPartial in src/NameAnchorMaybe.h, and measured here (v349):
    // putting the declaration in WidgetBase.h cost src/DPlaySessionMgr.cpp one exact match
    // (39 -> 38, -166 B) without changing a line of its source. `this` is at offset 0 either
    // way, so the call site's bytes are identical.
    // Makes the sound-bank entry nSoundId the one this object is playing: on an id change it
    // releases the old pDSoundChannel and re-looks-up pSoundEntry (SoundBank_LookupEntryById,
    // rejecting an entry whose +9 byte isn't 1), then either acquires a fresh channel via
    // DSound::AcquireChannelForSound -- seeding dwSoundResumeTick from the frame entry's own
    // nSoundRetriggerDelay when there is one -- or, if a channel already exists and the resume
    // deadline has passed, retriggers it (DSoundChannel::ResumeOrRestart). No-ops when
    // g_pDSoundManager is NULL or nSoundId is 0.
    void EnsureSoundPlayingMaybe(unsigned int nSoundId);

    // Siblings called by UpdateCarPlacementTickMaybe (all address-interleaved with it,
    // 0x40d8e0-0x40e43e):
    // 0x40dc20 -- claimed-tile (dwTrackState==5, kind m_type0x63a==5) socket-settling handler;
    // its return (0/1) tells the caller whether it fully handled the tick.
    unsigned int SettleClaimedSocketMaybe(PeerTrainNodeTickPartial *pTrain);
    // 0x40df80 -- recomputes wHeadingMaybe from both anchors. nUnused: a genuine dead stack
    // param (confirmed by the caller's own push + this function's own `ret 0x4`; Ghidra's
    // decompile silently drops it, same "dead-but-real parameter" tell documented elsewhere --
    // see BuildToolCursorWnd.h's pDescUnused/PostBag.h's nUnused precedents). Every caller
    // passes this->rect.left (the car's OWN pre-recompute rect.left), never read in the body.
    void ComputeHeadingAngleMaybe(int nUnused);
    // 0x40d890 (seat BOTH of this car's anchors off one pixel point) is deliberately NOT
    // re-declared here: it is declared on the shared CarNetObj (src/CarNetObj.h) and DEFINED in
    // src/CarNetObj.cpp, and a duplicate declaration on this derived view HID the base's, so
    // every call from this TU mangled as ...@CarNetObjAnchorPartial@@ and targeted a symbol no
    // TU defines. That is byte-invisible (relocations are masked and the match build never
    // links) but a live defect in the port: PeerTrainNode's ctor calls it, so creating a train
    // from a depot ran a gen_stubs stub that answered 0 without seating either anchor.
    // Found v577 from loco/stub_calls.log; the inherited declaration is the correct one.
    unsigned char CompleteViewportExtendMaybe(PeerTrainNodeTickPartial *pTrain); // 0x40e340 -- dwModeAMaybe==4 follow-up
    unsigned char CheckCarLeftViewportMaybe(PeerTrainNodeTickPartial *pTrain); // 0x40e2a0 -- viewport-extend-phase-2
                                                          //   settle, per docs/subsystems.md
    void AdvanceModeBStateMaybe(PeerTrainNodeTickPartial *pTrain); // 0x40db90 -- dwModeBMaybe follow-up
    // 0x40e520 -- AdvanceModeBStateMaybe's own dwModeBMaybe==5 follow-up (depot-tile clear check).
    unsigned char CheckCarClearedDepotMaybe(PeerTrainNodeTickPartial *pTrain);
};

// Local view adding the method transcribed here (kept out of the shared header per the
// PeerTrainNodeTrackPartial precedent in src/NameAnchorMaybe.h -- header churn on
// PeerTrainNode.h rotates unrelated TU codegen).
// Recomputes every car slot's NameAnchorMaybe tile/socket/heading (and the lead
// pNameMaybe's world-pixel anchor position) after a car attach or a direction reversal.
// pTile is the tile the train's anchors are being laid out on; bKeepHeading skips the
// per-car heading stores. nLayoutMode: 2 = global-connector tile family (m_type0x63a in
// {1,2,3,4}, IsType0x63aInSet1234 -- also snapshots the tile's grid position into the
// wSentinelCMaybe/DMaybe pair), 1 = depot family ({7,8,9,0xa}, IsType0x63aInSet -- also
// pokes the tile's slot-7 virtual), 0 = anything else. Returns 0 only on a NULL pTile.
// FUNCTION: LOCO 0x44ce10
//
// EFFECTIVE MATCH (asmscore.py --len 1631: total 324574, align=316 reg_pen=73
// identity_miss=105 byte_diff=224, insns 514/476 incl. jump-table decode noise): every block
// is structurally identical to the original -- the loop-1 if/else-if m_type chain with its
// two shared anchor-init tails, the socket-table anchor math, both jump-table switch loops
// (case blocks emitted in the original's 2/8, 4/0xa, 1/7, 3/9 stream order, matched by
// reordering the source cases), the per-statement carSlots reload shapes. Residual is the
// documented VC5 /Og scheduling/reg-tie-break class: (1) else-arm LEA/interleave choices in
// the +0x26 cases (orig: LEA,mov,LEA,mov,LEA; ours: mov,LEA,mov,LEA,LEA), (2) a one-block
// EBX detour for the 18*i term in the forward 1/7 case (3/9 compiles clean), (3) load-order
// swaps inside two Y-case tail statements, (4) nStepOut=0 init placement in the reversed
// preheader. Variants tried (score path 762k -> 324k): switch vs if/else-if chain for loop 1
// (chain is correct -- the original has no jump table there), pCar local everywhere (worse),
// explicit car-pointer walk in loop 1 (worse), pA=0/pB decl placement before the null check
// (fixes the prologue: up-front push/xor of EDI+EBX), case reorder 2/8,4/0xa,1/7,3/9 (big
// win -- block stream order follows source case order), dropping the reversed loop's pCar
// local (forces the per-statement reloads), if-arm d/pCar statement swap, reversed loop as
// if-guarded do-while. The leftovers look TU-context-bound (/Og global allocation), so this
// parks until more of the PeerTrainNode cluster is transcribed into this TU. PARKED.
unsigned char PeerTrainNodePartial::LayoutCarAnchorsMaybe(TilePlacedObj *pTile, char bKeepHeading)
{
    int nLayoutMode = 0;
    NameAnchorMaybe *pA = 0;
    NameAnchorMaybe *pB;
    if (pTile == 0) {
        return 0;
    }
    BigObj *pKind = pTile->pKindDesc;
    if (pKind->IsType0x63aInSet1234()) {
        *(unsigned int *)&this->wSentinelCMaybe = *(unsigned int *)&pTile->pos.wPosX;
        nLayoutMode = 2;
    }
    else if (pKind->IsType0x63aInSet()) {
        nLayoutMode = 1;
        pTile->ReleaseChannelAndDispatch(1);
    }
    for (int i = 0; i <= (int)this->wCarSlotCount; i++) {
        pA = ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe;
        pB = ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe;
        pA->pTileMaybe = (TrackTileObj *)pTile;
        pB->pTileMaybe = (TrackTileObj *)pTile;
        if (nLayoutMode == 2) {
            ((CarNetObjAnchorPartial *)this->carSlots[i])->dwModeAMaybe = 4;
            pA->dwViewportExtendPhaseMaybe = 4;
            pB->dwViewportExtendPhaseMaybe = 4;
        }
        else {
            pA->dwTileExtendPhaseMaybe = 5;
            pB->dwTileExtendPhaseMaybe = 5;
        }
        if ((pKind->m_type0x63a == 1) || (pKind->m_type0x63a == 7)) {
            if (bKeepHeading == 0) {
                if (this->dwReversed == 0) {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0x40;
                }
                else {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0;
                }
            }
            pA->dwUnk0x04 = 0;
            pA->nSocketIndexMaybe = pKind->wSocketCount - 1;
            pB->dwUnk0x04 = 0;
            pB->nSocketIndexMaybe = pKind->wSocketCount - 1;
        }
        else if ((pKind->m_type0x63a == 2) || (pKind->m_type0x63a == 8)) {
            if (bKeepHeading == 0) {
                if (this->dwReversed == 0) {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0;
                }
                else {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0x40;
                }
            }
            pA->dwUnk0x04 = 1;
            pA->nSocketIndexMaybe = 1;
            pB->dwUnk0x04 = 1;
            pB->nSocketIndexMaybe = 1;
        }
        else if ((pKind->m_type0x63a == 3) || (pKind->m_type0x63a == 9)) {
            if (bKeepHeading == 0) {
                if (this->dwReversed == 0) {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0x20;
                }
                else {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0x60;
                }
            }
            pA->dwUnk0x04 = 1;
            pA->nSocketIndexMaybe = 1;
            pB->dwUnk0x04 = 1;
            pB->nSocketIndexMaybe = 1;
        }
        else if ((pKind->m_type0x63a == 4) || (pKind->m_type0x63a == 10)) {
            if (bKeepHeading == 0) {
                if (this->dwReversed == 0) {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0x60;
                }
                else {
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->wHeadingMaybe = 0x20;
                }
            }
            pA->dwUnk0x04 = 0;
            pA->nSocketIndexMaybe = pKind->wSocketCount - 1;
            pB->dwUnk0x04 = 0;
            pB->nSocketIndexMaybe = pKind->wSocketCount - 1;
        }
        pA->pTileMaybe = (TrackTileObj *)pTile;
        pB->pTileMaybe = (TrackTileObj *)pTile;
    }
    this->pNameMaybe->pTileMaybe = (TrackTileObj *)pTile;
    this->pNameMaybe->dwUnk0x04 = pA->dwUnk0x04;
    this->pNameMaybe->nSocketIndexMaybe = pA->nSocketIndexMaybe;
    TrackTileObj *pAnchorTile = this->pNameMaybe->pTileMaybe;
    BigObj *pAnchorKind = pAnchorTile->pKindDesc;
    this->pNameMaybe->nAnchorXMaybe =
        (int)(short)pAnchorKind->pSocketTable[pA->nSocketIndexMaybe * 2] +
        (short)pAnchorTile->pos.wPosX * 0x10;
    this->pNameMaybe->nAnchorYMaybe =
        (int)(short)pAnchorKind->pSocketTable[pA->nSocketIndexMaybe * 2 + 1] +
        (short)this->pNameMaybe->pTileMaybe->pos.wPosY * 0x10;
    if (nLayoutMode == 2) {
        this->pNameMaybe->dwViewportExtendPhaseMaybe = 4;
    }
    else {
        this->pNameMaybe->dwTileExtendPhaseMaybe = 5;
    }
    if (this->dwReversed == 0) {
        // Forward-facing: lay cars out front-to-back from the lead anchor, car 0 pinned
        // 0xc off the lead anchor's own position.
        for (int i = 0; i <= (int)this->wCarSlotCount; i++) {
            CarNetObjAnchorPartial *pCar;
            int d;
            switch (pKind->m_type0x63a) {
            case 2:
            case 8:
                if (i == 0) {
                    d = this->pNameMaybe->nAnchorXMaybe + 0xc;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[0];
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe->nAnchorXMaybe + i * 0x26;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                }
                pCar->pNameAMaybe->nAnchorXMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                pCar->pNameBMaybe->nAnchorXMaybe = pCar->pNameAMaybe->nAnchorXMaybe + 0x16;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                break;
            case 4:
            case 10:
                if (i == 0) {
                    d = this->pNameMaybe->nAnchorYMaybe + 0xc;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[0];
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe->nAnchorYMaybe + i * 0x26;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                }
                pCar->pNameAMaybe->nAnchorYMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                pCar->pNameBMaybe->nAnchorYMaybe = pCar->pNameAMaybe->nAnchorYMaybe + 0x16;
                break;
            case 1:
            case 7:
                if (i == 0) {
                    d = this->pNameMaybe->nAnchorXMaybe - 0xc;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[0];
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe->nAnchorXMaybe - i * 0x12;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                }
                pCar->pNameAMaybe->nAnchorXMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                pCar->pNameBMaybe->nAnchorXMaybe = pCar->pNameAMaybe->nAnchorXMaybe - 0x16;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                break;
            case 3:
            case 9:
                if (i == 0) {
                    d = this->pNameMaybe->nAnchorYMaybe - 0xc;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[0];
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe->nAnchorYMaybe - i * 0x12;
                    pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                }
                pCar->pNameAMaybe->nAnchorYMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                pCar = (CarNetObjAnchorPartial *)this->carSlots[i];
                pCar->pNameBMaybe->nAnchorYMaybe = pCar->pNameAMaybe->nAnchorYMaybe - 0x16;
                break;
            default:
                break;
            }
        }
    }
    else if (this->dwReversed == 1) {
        // Just-reversed: re-lay every car back-to-front from the new leading end; the
        // outermost car (i == wCarSlotCount) pins off the lead anchor, the rest step
        // 0x26 further out per car off the outermost car's B anchor.
        int i = (short)this->wCarSlotCount;
        if (i >= 0) {
            int nStepOut = 0;
            do {
            int d;
            switch (pKind->m_type0x63a) {
            case 2:
            case 8:
                if (i == this->wCarSlotCount) {
                    d = this->pNameMaybe->nAnchorXMaybe + 0xc;
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe->nAnchorXMaybe + nStepOut;
                }
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorXMaybe = ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe + 0x16;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                break;
            case 4:
            case 10:
                if (i == this->wCarSlotCount) {
                    d = this->pNameMaybe->nAnchorYMaybe + 0xc;
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe->nAnchorYMaybe + nStepOut;
                }
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorYMaybe = ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe + 0x16;
                break;
            case 1:
            case 7:
                if (i == this->wCarSlotCount) {
                    d = this->pNameMaybe->nAnchorXMaybe - 0xc;
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe->nAnchorXMaybe - nStepOut;
                }
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorXMaybe = ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe - 0x16;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorYMaybe = this->pNameMaybe->nAnchorYMaybe;
                break;
            case 3:
            case 9:
                if (i == this->wCarSlotCount) {
                    d = this->pNameMaybe->nAnchorYMaybe - 0xc;
                }
                else {
                    d = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe->nAnchorYMaybe - nStepOut;
                }
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe = d;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorXMaybe = this->pNameMaybe->nAnchorXMaybe;
                ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->nAnchorYMaybe = ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->nAnchorYMaybe - 0x16;
                break;
            default:
                break;
            }
            nStepOut += 0x26;
            i--;
            } while (i >= 0);
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// PeerTrainNode_UpdatePlacementTickMaybe (0x44c3a0) support views

// Local view adding the tick method transcribed below plus the extern helpers it calls
// (same per-consumer-view precedent as CarNetObjAnchorPartial above; extends
// PeerTrainNodeTrackPartial for AdvanceAlongTrackMaybe/CanReverseDirectionMaybe).
struct PeerTrainNodeTickPartial : PeerTrainNodeTrackPartial {
    char ReverseDirectionGuardedMaybe();   // 0x44caf0 -- the re-entry-guarded wrapper (below)
    unsigned char ReverseDirectionMaybe(); // 0x44cb10 -- flips dwReversed and re-derives the
                                   // train's tail-position reference (see below)
    void CompleteDepotExitMaybe();           // 0x44c9b0 -- dwModeBMaybe 4/5 follow-up
    unsigned char ApplyLeadTileTickStateMaybe();  // 0x44ca50 -- lead-tile tick-state dispatch (see below)
    // 0x44d4c0 -- the tile that would become the new lead after a reversal (see below)
    TrackTileObj *GetReversalTargetTileMaybe();
    unsigned char CanReverseDirectionMaybe();        // 0x44d630 (see below)
    void TryTransitionModeMaybe(int bDirection, char bForce); // 0x44d5e0 (see below)
    void SetSoundStateIfChangedMaybe(int state);     // 0x44d720 (see below)
    void PeerTrainNode_UpdatePlacementTickMaybe(); // 0x44c3a0
};

// WorldBoardMaybe (DAT_004aad08) -- local view for the methods the tick needs (kept out
// of src/WorldBoardMaybe.h: adding method decls to a shared header has rotated sibling TU
// codegen before -- the v325 PeerTrainNode.h/DPlaySessionMgr.cpp lesson).
struct WorldBoardSlotPartial {
    unsigned char pad0x0[4];
    int dwViewportWidthMaybe;  // +0x4 -- same field as the canonical WorldBoardPartial's own
                               //   dwViewportWidth (src/WorldBoardMaybe.h); kept as a same-layout
                               //   prefix in this TU-local view rather than switching to the
                               //   shared header (that would collide: this TU's own g_worldBoard
                               //   extern is already typed WorldBoardSlotPartial, not
                               //   WorldBoardPartial -- see the TODO: idiom below).
    int dwViewportHeightMaybe; // +0x8 -- ditto, WorldBoardPartial::dwViewportHeightMaybe
    // 0x455620, extern -- the plane-A slot (TilePlacedObj-family tile object) at grid (x, y),
    // or NULL. Return typed by its connector-leaf use here (+0x11c/+0x120, shared with the
    // depot leaf).
    TrackConnectorTileObj *GetPlaneASlotMaybe(short x, short y, int flag);
    // 0x455840, extern -- dirty-marks a rect region (src/WorldBoardMaybe.h's own declaration,
    // kept out of THIS TU's include per the same header-churn precedent as GetPlaneASlotMaybe).
    void MarkRectDirty(RECT rect);
};
extern WorldBoardSlotPartial g_worldBoard; // DAT_004aad08  // TODO: idiom

// Free function (NOT a this-based method -- __fastcall with a single explicit pointer arg,
// confirmed via Ghidra's own signature) that Ghidra has address-boxed into the PeerTrainNode
// namespace; called from UpdateCarPlacementTickMaybe's non-claimed-tile branch. Forward-declared
// here (PeerTrainNodeTickPartial isn't fully defined until below); body further down this file.
extern unsigned char __fastcall PeerTrainNode_DetectCarAtSocketBoundaryMaybe(PeerTrainNodeTickPartial *pTrain); // 0x44cab0

// Called by AdvanceModeBStateMaybe when this CAR's own dwModeBMaybe reaches 5 -- the "car is clearing the
// depot-family tile it was parked on" follow-up. Picks this car's TRAILING anchor (pNameAMaybe
// forward, pNameBMaybe reversed -- the OPPOSITE side from CheckCarLeftViewportMaybe's leading pick) and, if
// that anchor's tile kind is one of the 4 depot-family edges (m_type0x63a in {7,8,9,0xa}, the
// IsType0x63aInSet family), tests this car's own rect edge on the matching side against that
// tile's own footprint bound in world pixels -- the tile's grid position scaled <<4, widened on
// the far X/Y sides by the kind's bBitmapOccupancyCols/Rows. Once the car has fully cleared
// the footprint, sets this->dwModeBMaybe = 4 (the state AdvanceModeBStateMaybe's own other branch then
// waits to clear back to 0) and dispatches SetReadyStateMaybe(1) (vtable slot 9 -- re-arms
// bReady, dirty-marks, resumes the car's own DSound channel), the exact mirror of
// CheckCarLeftViewportMaybe's own SetReadyStateMaybe(0) on the leading side.
// Return type is `unsigned char`, always 0 -- the original's two exits both carry an explicit
// `xor al,al` (0x40e59e / 0x40e5e6) leaving the upper 3 bytes of EAX as whatever the vtable
// dispatch returned, the same CONCAT31 "caller only reads AL" tell as this TU's siblings
// CheckCarLeftViewportMaybe/CompleteViewportExtendMaybe; the sole caller (AdvanceModeBStateMaybe) discards it.
//
// ⚠ WAS EXACT (v349), RE-FLIPPED IN v474 and now DIFF(124) at 196 B against 220 -- so cl is
// cross-jumping all four settle tails into one again, exactly the v348 symptom below. NO source
// edit caused this: src/CarNetObj.h grew the real CarNetState member at +0x88 (every offset
// unchanged), which pulls CarNetState.h/PostBag.h into this TU and moves the /Og state this
// function's allocation rides on. Two position probes were run and BOTH failed: hoisting the
// pair above LayoutCarAnchorsMaybe does not even compile (PeerTrainNodeTickPartial is not yet
// defined there), and swapping it one slot later, past UpdateCarPlacementTickMaybe, leaves the
// TU at the same 13/26. The same header change paid +768 B / +3 EXACT in
// src/NameAnchorMaybe.cpp, so it is a net +328 B win overall -- this pair is the price.
// Recovery lead: this is a POSITION coin-flip, so re-probe it after any future
// src/CarNetObj.h work rather than treating it as settled.
//
// EXACT MATCH (v349) -- but ONLY from this position in the TU. v348 parked this function as an
// intrinsic register-allocation tie-break (total 102151, reg_pen=19: a pure EAX<->EDX role swap
// on the scratch temps in cases 7/9/10, which cross-jumped all four settle tails into one where
// the original keeps two). That diagnosis of the SYMPTOM was right and every source-shape probe
// it ran really is byte-neutral -- but the CAUSE was misattributed to the function itself. The
// allocation is decided by the optimizer state VC5 carries forward from the functions compiled
// BEFORE it in the TU: hoisted above UpdateCarPlacementTickMaybe/SettleClaimedSocketMaybe the
// SAME source text compiles byte-identical to the original (reg_pen 19 -> 0). Do NOT reorder
// this block relative to its neighbours without re-running tools/cc.sh.
// FUNCTION: LOCO 0x40e520
unsigned char CarNetObjAnchorPartial::CheckCarClearedDepotMaybe(PeerTrainNodeTickPartial *pTrain)
{
    NameAnchorMaybe *pTrailAnchor = (pTrain->dwReversed == 0) ? this->pNameAMaybe : this->pNameBMaybe;
    TrackTileObj *pTile = pTrailAnchor->pTileMaybe;
    BigObj *pKind = pTile->pKindDesc;
    switch (pKind->m_type0x63a) {
    case 7:
        if (this->rect.right > ((short)pTile->pos.wPosX + pKind->bBitmapOccupancyCols) * 16 - 16) {
            this->dwModeBMaybe = 4;
            this->SetReadyStateMaybe(1);
        }
        break;
    case 8:
        if (this->rect.left < ((short)pTile->pos.wPosX + 1) * 16) {
            this->dwModeBMaybe = 4;
            this->SetReadyStateMaybe(1);
        }
        break;
    case 9:
        if (this->rect.bottom > ((short)pTile->pos.wPosY + pKind->bBitmapOccupancyRows - 2) * 16) {
            this->dwModeBMaybe = 4;
            this->SetReadyStateMaybe(1);
        }
        break;
    case 10:
        if (this->rect.top < ((short)pTile->pos.wPosY + 1) * 16) {
            this->dwModeBMaybe = 4;
            this->SetReadyStateMaybe(1);
        }
        break;
    }
    return 0;
}

// Called by AdvanceModeBStateMaybe when this CAR's own dwModeBMaybe is 1, and by
// ReverseDirectionMaybe on every car it re-walks -- the "car is ENTERING the depot-family tile
// its trailing anchor just claimed" follow-up, and the exact mirror of
// CheckCarClearedDepotMaybe below. Same trailing-anchor pick (pNameAMaybe forward,
// pNameBMaybe reversed), but gated on that anchor's dwTileExtendPhaseMaybe being 1 ("claimed a
// new tile, advancing across it" -- the kind-7..0xa half of NameAnchorMaybe's mutually exclusive
// extend-phase pair, so the guard and the switch below agree on the tile family by construction).
// Tests the car's own rect edge against the SAME per-kind footprint bound as
// CheckCarClearedDepotMaybe, with all four comparisons INVERTED: this one fires while the car is
// still short of the bound, sets dwModeBMaybe = 2 and pauses via SetReadyStateMaybe(0); the other
// fires once the car is past it, sets dwModeBMaybe = 4 and resumes via SetReadyStateMaybe(1).
// The trailing `if (dwModeBMaybe == 2)` is deliberately OUTSIDE the phase guard, so a car already
// parked in mode 2 gets re-paused on every tick even after its anchor leaves phase 1.
// Return type is `unsigned char`, always 0 (the single shared epilogue's `xor al,al` at 0x40e504)
// -- the same CONCAT31 "caller only reads AL" tell as this TU's siblings; both callers discard it.
//
// ⚠ WAS EXACT (v349), RE-FLIPPED IN v474 to DIFF(15) at the correct 220 B -- the milder half of
// the same TU-state rotation documented on CheckCarClearedDepotMaybe above (src/CarNetObj.h's
// real CarNetState member). Fifteen bytes of the EAX<->EDX scratch-temp swap are back. Same two
// failed position probes, same recovery lead: re-probe position after CarNetObj.h work.
//
// EXACT MATCH (v349) -- position-dependent, exactly like CheckCarClearedDepotMaybe above. In its
// original (address-order) slot between CompleteViewportExtendMaybe and CheckCarClearedDepotMaybe
// it scored reg_pen=19 with the same EAX<->EDX scratch-temp swap in cases 7/9/10; hoisted here,
// ahead of UpdateCarPlacementTickMaybe, the identical source text is byte-exact. Keep it here.
// FUNCTION: LOCO 0x40e440
unsigned char CarNetObjAnchorPartial::CheckCarEnteredDepotMaybe(PeerTrainNodeTickPartial *pTrain)
{
    NameAnchorMaybe *pTrailAnchor = (pTrain->dwReversed == 0) ? this->pNameAMaybe : this->pNameBMaybe;
    if (pTrailAnchor->dwTileExtendPhaseMaybe == 1) {
        TrackTileObj *pTile = pTrailAnchor->pTileMaybe;
        BigObj *pKind = pTile->pKindDesc;
        switch (pKind->m_type0x63a) {
        case 7:
            if (this->rect.right < ((short)pTile->pos.wPosX + pKind->bBitmapOccupancyCols) * 16 - 16) {
                this->dwModeBMaybe = 2;
            }
            break;
        case 8:
            if (this->rect.left > ((short)pTile->pos.wPosX + 1) * 16) {
                this->dwModeBMaybe = 2;
            }
            break;
        case 9:
            if (this->rect.bottom < ((short)pTile->pos.wPosY + pKind->bBitmapOccupancyRows - 2) * 16) {
                this->dwModeBMaybe = 2;
            }
            break;
        case 10:
            if (this->rect.top > ((short)pTile->pos.wPosY + 1) * 16) {
                this->dwModeBMaybe = 2;
            }
            break;
        }
    }
    if (this->dwModeBMaybe == 2) {
        this->SetReadyStateMaybe(0);
    }
    return 0;
}

// The per-car counterpart of PeerTrainNode_UpdatePlacementTickMaybe's own tick (called once per
// car slot, in travel order, from that function's main do-while -- see the FUNCTION: LOCO
// 0x44c3a0 block below). Two independent duties:
// (1) A "both anchors sit on the same claimed tile" settle check: when this car's own A- and
//     B-anchors currently share one tile AND that tile is claimed (dwTrackState==5), latches
//     pTrain->bUnk0x2c and either defers to SettleClaimedSocketMaybe (the tile's kind is the
//     special m_type0x63a==5 family) or else settles wUnk0x448 from a simple "is either
//     anchor's tile bottom edge (less a 0x20 margin) still ahead of that anchor's own Y
//     position" bound check -- when anchors DON'T share a claimed tile, just clears wUnk0x448
//     and defers to the free function PeerTrainNode_DetectCarAtSocketBoundaryMaybe instead.
//     SettleClaimedSocketMaybe's own return tells this branch whether it fully handled the
//     tick (skipping duty 2 below).
// (2) Otherwise (not handled by duty 1): advances both of this car's own anchors one step
//     (NameAnchorMaybe::AdvanceCarAnchorMaybe), and if either actually moved, dirty-marks the
//     car's own rect, recomputes its heading (ComputeHeadingAngleMaybe), re-applies it through
//     the inherited vtbl-slot-8 anim-frame dispatch (SetStateArgMaybe), and repositions the
//     car's rect off the new heading (RepositionForHeadingMaybe). Then two independent
//     mode-driven follow-ups: dwModeAMaybe-driven viewport-extend-phase-2 settling
//     (CheckCarLeftViewportMaybe, gated on which anchor is on the reversed-vs-forward leading side) or a
//     mode-4 follow-up (CompleteViewportExtendMaybe); and any dwModeBMaybe follow-up (AdvanceModeBStateMaybe).
// Finally (independent of both branches above): re-derives the FAR end of the whole train (the
// same "trailing car's B-anchor / leading car's A-anchor" tile GetReversalTargetTileMaybe/
// CanReverseDirectionMaybe pick) both before and after the two duties above run; if it was a
// global-connector tile (IsType0x63aInSet1234) before but is NOT one any more by the second
// read, and that tile's own occupancy claim is still set, releases it -- the connector-tile
// hand-off claim the train's own dwModeAMaybe==2/3 branch (PeerTrainNode_UpdatePlacementTickMaybe)
// takes out is only ever released here, per car, once the train has actually moved off it.
//
// PARKED (v344): structure verified statement-by-statement against the raw disasm/decompile
// (the full 0x40d940-0x40db80 span) -- content-complete, not byte-matched. asmscore --len 576:
// total 166166 (align=160 reg_pen=55 identity_miss=55 byte_diff=116, insns 182/185) -- very
// close instruction count, almost every mismatch is an "r" (register-only) or "S" (equivalent
// operation, different shape) diff, not a missing/extra block. Root cause: the compiled
// prologue pushes ONE MORE register than the original (ebx+ebp+esi+edi here vs the original's
// ebx+esi+edi -- confirmed by diffing raw bytes at the very top of the function), which then
// cascades into a stack-offset shift for the rest of the body (values the original spills to
// `[esp+N]` land in the extra register here instead, and vice versa at a couple of sites).
// This is the SAME "TU-context /Og register-budget" class already parked and unresolved for
// this exact function's own caller-side sibling, `NameAnchorMaybe::AdvanceCarAnchorMaybe`
// (0x40c580, v343) and `AdvanceAlongTrackMaybe` (0x40bbd0, v325, 12+ variants tried) --
// registers spilled here are apparently allocated based on whole-function live-range pressure
// that isn't cleanly steerable from source shape. Tried and no effect: moving `bHandled`'s
// zero-init to the very first statement (this WAS a genuine fix -- matched the original's own
// statement order, `cVar11 = '\0';` before the far-anchor lookup, and closed one real
// instruction-ordering mismatch, total 178170 -> 166166); hoisting every local to
// declare-then-assign at the top of the function (byte-for-byte identical score, VC5 doesn't
// care about textual declaration position here). Retry only if the shared TU-context /Og
// register-budget class ever cracks generally (see 0x40bbd0's and 0x40c580's own parks).
// FUNCTION: LOCO 0x40d940
void CarNetObjAnchorPartial::UpdateCarPlacementTickMaybe(PeerTrainNodeTickPartial *pTrain, int nMoved)
{
    char bHandled = 0;
    NameAnchorMaybe *pFarAnchor;
    if (pTrain->dwReversed == 0) {
        pFarAnchor = ((CarNetObjAnchorPartial *)pTrain->carSlots[pTrain->wCarSlotCount])->pNameBMaybe;
    } else {
        pFarAnchor = ((CarNetObjAnchorPartial *)pTrain->carSlots[0])->pNameAMaybe;
    }
    TrackTileObj *pFarTile = pFarAnchor->pTileMaybe;

    TrackTileObj *pTileA = this->pNameAMaybe->pTileMaybe;
    if (pTileA != 0 && this->pNameBMaybe->pTileMaybe != 0) {
        TrackTileObj *pTileB = this->pNameBMaybe->pTileMaybe;
        if (pTileA->dwTrackState == 5 && pTileB->dwTrackState == 5 && pTileA == pTileB) {
            pTrain->bUnk0x2c = 1;
            pTileA = this->pNameAMaybe->pTileMaybe;
            if (pTileA->pKindDesc->m_type0x63a == 5) {
                bHandled = (char)this->SettleClaimedSocketMaybe(pTrain);
            } else {
                pTileB = this->pNameBMaybe->pTileMaybe;
                if ((pTileA->rect.bottom - 0x20 < this->pNameAMaybe->nAnchorYMaybe) ||
                    (pTileB->rect.bottom - 0x20 < this->pNameBMaybe->nAnchorYMaybe) ||
                    (pTileB != pTileA)) {
                    this->wUnk0x448 = 0;
                } else {
                    this->wUnk0x448 = 1;
                }
                bHandled = 0;
            }
        } else {
            this->wUnk0x448 = 0;
            PeerTrainNode_DetectCarAtSocketBoundaryMaybe(pTrain);
        }
    }

    if (bHandled == 0) {
        unsigned char bMovedA = this->pNameAMaybe->AdvanceCarAnchorMaybe(
            (PeerTrainNodeCarAdvancePartial *)pTrain, this);
        unsigned char bMovedB = this->pNameBMaybe->AdvanceCarAnchorMaybe(
            (PeerTrainNodeCarAdvancePartial *)pTrain, this);
        if (bMovedA == 1 || bMovedB == 1) {
            g_worldBoard.MarkRectDirty(this->rect);
            this->ComputeHeadingAngleMaybe(this->rect.left);
            this->SetStateArgMaybe(this->wHeadingMaybe, 1);
            this->RepositionForHeadingMaybe();
        }
        if (pTrain->dwModeAMaybe != 0) {
            int dCarMode = this->dwModeAMaybe;
            int dCompare = pTrain->dwModeAMaybe;
            if (dCarMode == 0 || (dCompare = dCarMode, dCarMode != 1)) {
                if (dCompare == 4) {
                    this->CompleteViewportExtendMaybe(pTrain);
                }
            } else if ((pTrain->dwReversed == 0) && (this->pNameBMaybe->dwViewportExtendPhaseMaybe == 2)) {
                this->CheckCarLeftViewportMaybe(pTrain);
            } else if ((pTrain->dwReversed == 1) && (this->pNameAMaybe->dwViewportExtendPhaseMaybe == 2)) {
                this->CheckCarLeftViewportMaybe(pTrain);
            }
        }
        if (pTrain->dwModeBMaybe != 0) {
            this->AdvanceModeBStateMaybe(pTrain);
        }
    }

    if (pFarTile != 0 && pFarTile->pKindDesc != 0 && pFarTile->pKindDesc->IsType0x63aInSet1234() == 1) {
        TrackTileObj *pFarTile2;
        if (pTrain->dwReversed == 0) {
            pFarTile2 = ((CarNetObjAnchorPartial *)pTrain->carSlots[pTrain->wCarSlotCount])->pNameBMaybe->pTileMaybe;
        } else {
            pFarTile2 = ((CarNetObjAnchorPartial *)pTrain->carSlots[0])->pNameAMaybe->pTileMaybe;
        }
        BigObj *pKind2 = pFarTile2->pKindDesc;
        if (pKind2 != 0 && pKind2->IsType0x63aInSet1234() != 1 &&
            ((TrackConnectorTileObj *)pFarTile)->dwOccupancyClaim == 1) {
            ((TrackConnectorTileObj *)pFarTile)->dwOccupancyClaim = 0;
        }
    }
}

// Called by UpdateCarPlacementTickMaybe when this car's own A- and B-anchors currently share
// one claimed (dwTrackState==5) tile whose kind is the special m_type0x63a==5 family. Picks
// this car's "direction anchor" -- pNameAMaybe when the train runs forward, pNameBMaybe when
// reversed -- and, if THAT anchor already sits at the socket-index boundary matching its own
// step direction (dwUnk0x04), settles both anchors' world-pixel positions straight from the
// tile's socket table and returns 0 (unhandled -- UpdateCarPlacementTickMaybe still runs its
// own per-anchor advance). Otherwise steps pNameAMaybe's own socket index by its own step
// direction and applies the SAME delta to pNameBMaybe, re-settles both anchors' positions
// (pNameAMaybe's own X/Y come from an index picked 0xb sockets off, by train direction and
// step sign -- NOT its own just-adjusted index), dirty-marks the car's rect, and finally
// repositions the car (vtbl slot 3, RepositionWithHotspot) around a footprint-table hotspot
// offset keyed by this car's own heading -- returning 1 (handled).
// FUNCTION: LOCO 0x40dc20
//
// PARKED (v345): content-complete, transcribed statement-by-statement against the raw
// disasm/decompile over the full 0x40dc20-0x40df75 span. Every raw-offset decompile artifact
// resolved to an already-named field. (v407: the last holdout -- what used to read as an
// UNALIGNED signed-short load through BigObj's byte array, `aFootprintOccupancyMask +
// heading*4 -6/-4` -- was never unaligned and was never that array. A car's pKindDesc is an
// CarKindDesc, not a BigObj, and 0x16e-6 == 0x168 lands exactly on that class's own
// `short aHeadingOffsetTableMaybe[400]`; wHeadingMaybe*4 bytes == element *2, so the pair is
// simply [h*2] (dx) and [h*2+1] (dy). Folded here and in RepositionForHeadingMaybe below,
// byte-neutral in both -- 0x40d8e0 stayed an exact MATCH across the rewrite.)
// asmscore --len 853: total 154534 (align=148 reg_pen=58 identity_miss=58 byte_diff=154,
// insns 267/265) -- every block is structurally identical to the original (same branches,
// same socket-table/anchor-position math, same RepositionWithHotspot call shape at all 3
// sites); the diff is almost entirely register-choice noise (edi/ebp swapped throughout) plus
// two real-but-intrinsic scheduling differences: (1) the prologue loads its incoming pTrain
// param from a different stack slot ([esp+0x14] into edi in the original vs [esp+0xc] into
// ebp here) -- the SAME "compiled prologue pushes a different register set" tell as this
// function's own callers/siblings' parks; (2) one `this->pKindDesc` (`[esi+0x40]`) reload is
// hoisted earlier in the original at each of the 3 RepositionWithHotspot call sites, deferred
// here. Tried with NO effect (identical score, confirming VC5 doesn't care about source-level
// shape for this class): declaring pDirAnchor via an explicit if/else instead of a ternary;
// inlining the RepositionWithHotspot x/y args directly instead of through int locals. Same
// shared TU-context /Og register-budget class already parked for this function's own
// siblings/callers in this TU (AdvanceCarAnchorMaybe 0x40c580 v343, UpdateCarPlacementTickMaybe
// 0x40d940 v344, AdvanceAlongTrackMaybe 0x40bbd0 v325) -- retry together if that class ever
// cracks.
unsigned int CarNetObjAnchorPartial::SettleClaimedSocketMaybe(PeerTrainNodeTickPartial *pTrain)
{
    NameAnchorMaybe *pDirAnchor = (pTrain->dwReversed == 0) ? this->pNameAMaybe : this->pNameBMaybe;
    int dir = pDirAnchor->dwUnk0x04;
    BigObj *pKind = pDirAnchor->pTileMaybe->pKindDesc;

    if ((dir == 1 && pDirAnchor->nSocketIndexMaybe == pKind->wSocketCount - 1) ||
        (dir == 0 && pDirAnchor->nSocketIndexMaybe == 1)) {
        this->pNameAMaybe->nAnchorXMaybe =
            (int)(short)pKind->pSocketTable[this->pNameAMaybe->nSocketIndexMaybe * 2] +
            (short)this->pNameAMaybe->pTileMaybe->pos.wPosX * 0x10;
        // sic: the Y term reads pNameBMaybe's own socket index, not pNameAMaybe's own --
        // reproduced literally; both anchors share one claimed tile here so the two
        // indices are expected to already agree.
        this->pNameAMaybe->nAnchorYMaybe =
            (int)(short)pKind->pSocketTable[this->pNameBMaybe->nSocketIndexMaybe * 2 + 1] +
            (short)this->pNameAMaybe->pTileMaybe->pos.wPosY * 0x10;
        this->pNameBMaybe->nAnchorXMaybe =
            (int)(short)pKind->pSocketTable[this->pNameBMaybe->nSocketIndexMaybe * 2] +
            (short)this->pNameBMaybe->pTileMaybe->pos.wPosX * 0x10;
        this->pNameBMaybe->nAnchorYMaybe =
            (int)(short)pKind->pSocketTable[this->pNameBMaybe->nSocketIndexMaybe * 2 + 1] +
            (short)this->pNameBMaybe->pTileMaybe->pos.wPosY * 0x10;
        return 0;
    }

    int nSocketIdx = pDirAnchor->nSocketIndexMaybe;
    if (dir == 1) {
        if ((int)(pKind->wSocketCount - 0x32) < nSocketIdx) {
            this->wUnk0x448 = 0;
        } else if (0x50 < nSocketIdx) {
            this->wUnk0x448 = 1;
        }
        if (pDirAnchor->nSocketIndexMaybe == (unsigned int)pKind->wSocketCount) {
            return 0;
        }
    } else {
        if (nSocketIdx < 0x32) {
            this->wUnk0x448 = 0;
        } else if (nSocketIdx < (int)(pKind->wSocketCount - 0x55)) {
            this->wUnk0x448 = 1;
        }
        if (pDirAnchor->nSocketIndexMaybe == 0) {
            return 0;
        }
    }

    pTrain->bUnk0x2c = 1;
    if (this->pNameAMaybe->dwUnk0x04 == 1) {
        this->pNameAMaybe->nSocketIndexMaybe = this->pNameAMaybe->nSocketIndexMaybe + 1;
        this->pNameBMaybe->nSocketIndexMaybe = this->pNameBMaybe->nSocketIndexMaybe + 1;
    } else {
        this->pNameAMaybe->nSocketIndexMaybe = this->pNameAMaybe->nSocketIndexMaybe - 1;
        this->pNameBMaybe->nSocketIndexMaybe = this->pNameBMaybe->nSocketIndexMaybe - 1;
    }

    int nOffsetIdx;
    if (pTrain->dwReversed == 0) {
        if (this->pNameAMaybe->dwUnk0x04 == 1) {
            nOffsetIdx = this->pNameAMaybe->nSocketIndexMaybe - 0xb;
            goto ApplyOffset;
        }
        nOffsetIdx = this->pNameAMaybe->nSocketIndexMaybe + 0xb;
    } else {
        if (this->pNameAMaybe->dwUnk0x04 == 1) {
            nOffsetIdx = this->pNameBMaybe->nSocketIndexMaybe - 0xb;
            goto ApplyOffset;
        }
        nOffsetIdx = this->pNameBMaybe->nSocketIndexMaybe + 0xb;
    }
ApplyOffset:
    this->pNameAMaybe->nAnchorXMaybe =
        (int)(short)pKind->pSocketTable[nOffsetIdx * 2] +
        (short)this->pNameAMaybe->pTileMaybe->pos.wPosX * 0x10;
    this->pNameAMaybe->nAnchorYMaybe =
        (int)(short)pKind->pSocketTable[nOffsetIdx * 2 + 1] +
        (short)this->pNameAMaybe->pTileMaybe->pos.wPosY * 0x10;
    this->pNameBMaybe->nAnchorXMaybe =
        (int)(short)pKind->pSocketTable[this->pNameBMaybe->nSocketIndexMaybe * 2] +
        (short)this->pNameBMaybe->pTileMaybe->pos.wPosX * 0x10;
    this->pNameBMaybe->nAnchorYMaybe =
        (int)(short)pKind->pSocketTable[this->pNameBMaybe->nSocketIndexMaybe * 2 + 1] +
        (short)this->pNameBMaybe->pTileMaybe->pos.wPosY * 0x10;

    g_worldBoard.MarkRectDirty(this->rect);

    if (pTrain->dwReversed == 0) {
        if (this->pNameAMaybe->dwUnk0x04 == 1) {
            this->RepositionWithHotspot(
                this->pNameAMaybe->nAnchorXMaybe -
                    ((CarKindDesc *)this->pKindDesc)->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2] - 0xb,
                this->pNameAMaybe->nAnchorYMaybe -
                    ((CarKindDesc *)this->pKindDesc)->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2 + 1]);
            return 1;
        }
    } else {
        if (this->pNameAMaybe->dwUnk0x04 != 1) {
            this->RepositionWithHotspot(
                this->pNameAMaybe->nAnchorXMaybe -
                    ((CarKindDesc *)this->pKindDesc)->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2] - 0xb,
                this->pNameAMaybe->nAnchorYMaybe -
                    ((CarKindDesc *)this->pKindDesc)->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2 + 1]);
            return 1;
        }
    }
    this->RepositionWithHotspot(
        this->pNameAMaybe->nAnchorXMaybe -
            ((CarKindDesc *)this->pKindDesc)->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2] + 0xb,
        this->pNameAMaybe->nAnchorYMaybe -
            ((CarKindDesc *)this->pKindDesc)->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2 + 1]);
    return 1;
}

// Recomputes wHeadingMaybe from the current A/B anchor pixel positions: single-quadrant
// fpatan(dy_abs/dx_abs) converted radians->degrees, quadrant-corrected off the SIGNED anchor
// deltas (not the abs values), two axis-aligned overrides (dy==0 forces 0/180, dx==0 forces
// 270/90 -- these win over the fpatan-derived value), then quantized into a 0-127 "compass"
// unit (128/360 degrees-per-unit) and clamped 0x80->0. Ghidra's own decompile of this function
// silently DROPS an FPU fmul scale constant (the final degrees->128ths multiply) -- transcribed
// from the raw disasm instead (CLAUDE.md's "never trust decompiled FPU pseudocode on an
// x87-heavy leaf" warning, first caught here at v76). The goto layout mirrors the original's
// own block structure 1:1 (3 corrected-value exits skip the dy==0 check entirely and jump
// straight to the dx==0 check; only the 3 "no correction" cases fall through it) -- written
// with explicit gotos rather than restructured ifs since this file's own goto-sharing
// precedent (SettleClaimedSocketMaybe's ApplyOffset) is a VC5 block-layout tell, not just
// style.
// FUNCTION: LOCO 0x40df80
void CarNetObjAnchorPartial::ComputeHeadingAngleMaybe(int nUnused)
{
    int ax = this->pNameAMaybe->nAnchorXMaybe;
    int ay = this->pNameAMaybe->nAnchorYMaybe;
    int bx = this->pNameBMaybe->nAnchorXMaybe;
    int by = this->pNameBMaybe->nAnchorYMaybe;
    double dx = (ax < bx) ? (double)(bx - ax) : (double)(ax - bx);
    double dy = (ay < by) ? (double)(by - ay) : (double)(ay - by);

    double theta = (dx != 0.0 && dy != 0.0) ? atan(dy / dx) * 57.29577950560105 : 0.0;

    if (ax > bx && ay > by) {
        theta = 180.0 - theta;
    } else if (ax > bx && ay < by) {
        theta = theta + 180.0;
    } else if (ax < bx && ay < by) {
        theta = 360.0 - theta;
    } else if (dy == 0.0) {
        theta = (ax < bx) ? 0.0 : 180.0;
    }

    if (dx == 0.0) {
        theta = (ay < by) ? 270.0 : 90.0;
    }

    theta *= 0.35555555555555557;
    this->wHeadingMaybe = (unsigned short)(int)theta;
    if (this->wHeadingMaybe == 0x80) {
        this->wHeadingMaybe = 0;
    }
}

// Reads wHeadingMaybe back as a (dx,dy) pair out of CarKindDesc::aHeadingOffsetTableMaybe
// (+0x168, `short[400]`; the car's pKindDesc is an CarKindDesc, so the pair for heading h is
// elements [h*2]/[h*2+1] -- the SAME per-heading table SettleClaimedSocketMaybe reads),
// offsets pNameAMaybe's anchor position by the looked-up
// (dx,dy) to get the sprite's new top-left, sizes the bottom-right off the descriptor's
// nativeWidth/nativeHeight, writes the inherited RECT, then calls the inherited vtable slot 3
// (RepositionWithHotspot, also re-centers the car's DSoundChannel). Net effect: every tick a
// car's anchors move, its sprite is re-positioned to a heading-dependent pixel offset --
// per-facing sprite-hotspot compensation for a car icon with a fixed set of rotated frames.
// FUNCTION: LOCO 0x40d8e0
void CarNetObj::RepositionForHeadingMaybe()
{
    CarKindDesc *pKind = (CarKindDesc *)this->pKindDesc;
    int x = this->pNameAMaybe->nAnchorXMaybe -
        pKind->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2];
    this->rect.left = x;
    this->rect.top = this->pNameAMaybe->nAnchorYMaybe -
        pKind->aHeadingOffsetTableMaybe[this->wHeadingMaybe * 2 + 1];
    this->rect.right = pKind->nativeWidth + this->rect.left;
    this->rect.bottom = pKind->nativeHeight + this->rect.top;
    this->RepositionWithHotspot(this->rect.left, this->rect.top);
}

// Called by UpdateCarPlacementTickMaybe when pTrain->dwModeBMaybe is set (any non-zero value).
// dwModeBMaybe==1: dispatches to CheckCarEnteredDepotMaybe (per-car reversal follow-up) once this car's
// TRAILING anchor (pNameBMaybe forward, pNameAMaybe reversed) has crossed into its own
// dwTileExtendPhaseMaybe==1 state. Otherwise (any other non-zero pTrain->dwModeBMaybe):
// forwards to CheckCarClearedDepotMaybe when this CAR's own dwModeBMaybe reaches 5, or clears this car's own
// dwModeBMaybe back to 0 once it's 4 and both anchors have settled off any tile-extend
// (dwTileExtendPhaseMaybe==0). Note pTrain->dwModeBMaybe (the train-level field checked at
// entry) and this->dwModeBMaybe (the CAR-level field of the same name, CarNetObjAnchorPartial's
// own +0x444) are two distinct fields, per PeerTrainNode.h's own field-pair note.
// FUNCTION: LOCO 0x40db90
void CarNetObjAnchorPartial::AdvanceModeBStateMaybe(PeerTrainNodeTickPartial *pTrain)
{
    if (pTrain->dwModeBMaybe != 0) {
        if (pTrain->dwModeBMaybe == 1) {
            if (pTrain->dwReversed == 0 && this->pNameBMaybe->dwTileExtendPhaseMaybe == 1) {
                this->CheckCarEnteredDepotMaybe(pTrain);
                return;
            }
            if (pTrain->dwReversed == 1 && this->pNameAMaybe->dwTileExtendPhaseMaybe == 1) {
                this->CheckCarEnteredDepotMaybe(pTrain);
                return;
            }
        } else {
            if (this->dwModeBMaybe == 5) {
                this->CheckCarClearedDepotMaybe(pTrain);
                return;
            }
            if (this->dwModeBMaybe == 4 &&
                this->pNameAMaybe->dwTileExtendPhaseMaybe == 0 &&
                this->pNameBMaybe->dwTileExtendPhaseMaybe == 0) {
                this->dwModeBMaybe = 0;
            }
        }
    }
}

// Called by UpdateCarPlacementTickMaybe when this car's own dwModeAMaybe reaches 2 (car's
// "reversed-vs-forward leading anchor" -- pNameBMaybe forward, pNameAMaybe reversed -- currently
// sits in viewport-extend phase 2, bound-crossed/settling). If that anchor's own tile kind is
// one of the 4 global-connector edges (m_type0x63a in {1,2,3,4}), tests THIS car's own rect edge
// on the matching side against the world viewport bound (right<0 / left>width / bottom<0 /
// height<top, keyed off m_type0x63a-1) and, once it crosses, sets this->dwModeAMaybe=2 and
// dispatches SetReadyStateMaybe(0) (vtable slot 9 -- clears bReady, dirty-marks, pauses the
// car's own DSound channel). Return type is `unsigned char`, always 0 -- confirmed via raw
// disasm: every one of the function's 3 possible exits (the initial phase!=2 bail, the
// post-switch dwModeAMaybe!=2 bail, and the fall-through after the vtbl call) funnels into ONE
// shared epilogue block with an explicit `xor al,al` before `ret`, the same "caller only reads
// AL, but AL is unconditionally 0 here" CONCAT31 tell as this TU's other siblings (no known
// caller reads the return value).
// FUNCTION: LOCO 0x40e2a0
unsigned char CarNetObjAnchorPartial::CheckCarLeftViewportMaybe(PeerTrainNodeTickPartial *pTrain)
{
    NameAnchorMaybe *pDirAnchor = (pTrain->dwReversed == 0) ? this->pNameBMaybe : this->pNameAMaybe;
    if (pDirAnchor->dwViewportExtendPhaseMaybe != 2) {
        return 0;
    }
    switch (pDirAnchor->pTileMaybe->pKindDesc->m_type0x63a - 1) {
    case 0:
        if (this->rect.right < 0) {
            this->dwModeAMaybe = 2;
        }
        break;
    case 1:
        if (this->rect.left > g_worldBoard.dwViewportWidthMaybe) {
            this->dwModeAMaybe = 2;
        }
        break;
    case 2:
        if (this->rect.bottom < 0) {
            this->dwModeAMaybe = 2;
        }
        break;
    case 3:
        if (this->rect.top > g_worldBoard.dwViewportHeightMaybe) {
            this->dwModeAMaybe = 2;
        }
        break;
    }
    if (this->dwModeAMaybe != 2) {
        return 0;
    }
    this->SetReadyStateMaybe(0);
    return 0;
}

// (Re)starts whatever sound this car's CURRENT animation frame asks for: reads the sound-bank
// id out of the descriptor's own per-frame table entry for nSubFrame and, when it is non-zero,
// hands it to the inherited EnsureSoundPlayingMaybe (0x405ab0), which does the channel
// acquire/retrigger. Id 0 means "this frame is silent" and is skipped, so a car mid-way through
// a silent frame keeps whatever channel it already had. The `short` id is sign-extended into the
// unsigned parameter (`movsx eax,ax` at 0x40e148) -- the sole non-zero sentinel that survives
// that widening is -1 (0xffffffff), which EnsureSoundPlayingMaybe treats specially as "remember
// the id even with no bank entry".
//
// EXACT MATCH (v349), on two counts that both had to be right. (1) The frame entry is addressed
// through a real pointer local used TWICE (test then pass): fold the two uses into one `short`
// local instead and VC5 folds the address into the load's own [base+idx*8+0xe] operand, dropping
// the original's standalone `lea eax,[edx+eax*8]` at 0x40e13c (10230 vs 224). (2) Like this TU's
// two depot checks, the last 2 bytes are position-dependent -- in its address-order slot the two
// independent `this` loads at 0x40e130/0x40e133 come out scheduled in the opposite order; from
// here (and only a few other slots) they match. Keep this block where it is.
//
// ⚠ The two-load order in point (2) is a genuine COIN, and this slot has now flipped it twice
// WITHOUT the block moving and without a line of this function changing. v475's 0x450-byte
// CarNetObj promotion knocked it out (EFFECTIVE, 34 B against 34, DIFF(4), insns 12/12 -- the
// original loads [ecx+0x40] before [ecx+0x28], cl picked the other order); v477's merge of
// CarNetObj::RetagKind into the real slot-15 SetCarTypeAndCategory -- a src/CarNetObj.h change
// this TU does not even reference -- put it back to EXACT, unprompted. Both flips were pure
// header-driven /Og state rotation, so do NOT treat a future flip here as a source defect, and
// do NOT try to pin it: the one probe anyone ran (hoisting the index into its own local ahead of
// the paFrameEntries[] subscript) was INERT in both directions.
// FUNCTION: LOCO 0x40e130
void CarNetObjAnchorPartial::StartFrameSoundMaybe()
{
    CursorAnimFrameEntry *pEntry = &this->pKindDesc->paFrameEntries[this->nSubFrame];
    if (pEntry->nSoundBankEntryId != 0) {
        this->EnsureSoundPlayingMaybe(pEntry->nSoundBankEntryId);
    }
}

// Called by UpdateCarPlacementTickMaybe when this car's own dwModeAMaybe is neither 0 nor 1 (the
// dwModeAMaybe==4 follow-up). First, if both of this car's own anchors are idle
// (dwViewportExtendPhaseMaybe==0) and this->dwModeAMaybe is still set, clears it and dispatches
// SetReadyStateMaybe(1) (re-arms bReady/resumes sound) -- unless that same dispatch leaves
// dwModeAMaybe non-zero again, in which case bails out early (matching the ORIGINAL's own
// re-check-after-dispatch shape, not just a plain reset). Otherwise (either anchor still
// mid-extend, or dwModeAMaybe was already clear), only continues once dwModeAMaybe stays clear.
// From there, re-derives the FAR end of the whole train (same forward/reversed pFarAnchor pick
// as UpdateCarPlacementTickMaybe's own tail block) and, if that anchor's tile is no longer a
// global-connector kind, re-arms pTrain->dwModeAMaybe back to 0. Finally, if pTrain->dwModeAMaybe
// ended up 0, releases the plane-A slot at the train's last claimed grid position
// (wSentinelCMaybe/DMaybe+1) if it's still claimed, then resets that sentinel pair to unset
// (-1/-1) -- the SAME occupancy-claim release UpdateCarPlacementTickMaybe's own tail performs for
// the per-car tick, mirrored here for the per-car dwModeAMaybe==4 follow-up path. Return type is
// `unsigned char`: 0 on either early bail (dwModeAMaybe still set after the reset/re-derive
// steps), 1 once the plane-A-slot release step runs (regardless of whether a slot was actually
// found/released) -- confirmed via raw disasm, 2 distinct explicit exits (`mov al,1` /
// `xor al,al`), no known caller reads it.
//
// PARKED (v347): content-complete, transcribed statement-by-statement against the raw disasm
// (0x40e340-0x40e43e). asmscore --len 254: total 98806 (align=98 reg_pen=7 identity_miss=7
// byte_diff=36, insns 90/85). The reset/dispatch prologue (0x40e340-0x40e37f) and the whole
// forward/reversed far-anchor re-derive block (0x40e37f-0x40e3f8, INCLUDING the original's own
// register-level code-sharing trick -- the `cmp ecx,1`/`jne` at 0x40e3ba is reached both from
// the reversed branch's own entry AND as a fall-through bail from the forward branch, reusing
// ONE physical register-held copy of pTrain->dwReversed for both purposes) reproduce
// byte-for-byte from a plain nested if/else-if with no gotos -- VC5's own optimizer found the
// same sharing unprompted. Two residual classes: (1) reg_pen=7/identity_miss=7 -- EAX vs ECX
// register-choice swaps for the reversed/forward pFarAnchor locals (same "TU-context /Og
// register-budget" class already parked for this file's siblings, e.g. AdvanceCarAnchorMaybe/
// UpdateCarPlacementTickMaybe/SettleClaimedSocketMaybe). (2) The real remaining gap: the
// ORIGINAL tail-merges EVERY "return 0" exit (the initial reset-check's own early-out included)
// into ONE shared physical epilogue at the very end of the function, reached via a single far
// `0f 85` jne from the earliest check -- my candidate instead compiles the FIRST early-out into
// its own separate, physically-duplicated small epilogue (a near `74`/`75` jump to a LOCAL
// copy of `xor al,al; pop edi; pop esi; ret 4`), shifting every subsequent byte offset and
// cascading into the reported diff even though the actual instructions are otherwise identical.
// Tried and WORSE, not better: routing all 3 early-outs through one `goto Fail; ... Fail: return
// 0;` label (98806 -> forced an extra redundant duplicated re-check block instead of merging,
// 265 total len); flattening to a single `bAbort` accumulator with one tail return (283 total
// len, worse still -- VC5 kept evaluating both branches unconditionally instead of skipping).
// The plain triple-`return 0;` shape (kept here) is the best of the 3 variants tried. Retry only
// if a general fix for VC5's own epilogue tail-merging heuristic is ever found (this is a
// DIFFERENT residual class from the plain register-budget one -- an epilogue-sharing tie-break,
// not a register one).
// FUNCTION: LOCO 0x40e340
unsigned char CarNetObjAnchorPartial::CompleteViewportExtendMaybe(PeerTrainNodeTickPartial *pTrain)
{
    if (this->pNameAMaybe->dwViewportExtendPhaseMaybe == 0 &&
        this->pNameBMaybe->dwViewportExtendPhaseMaybe == 0) {
        if (this->dwModeAMaybe != 0) {
            this->dwModeAMaybe = 0;
            this->SetReadyStateMaybe(1);
            if (this->dwModeAMaybe != 0) {
                return 0;
            }
        }
    } else if (this->dwModeAMaybe != 0) {
        return 0;
    }

    if (pTrain->dwReversed == 0) {
        NameAnchorMaybe *pFarAnchor =
            ((CarNetObjAnchorPartial *)pTrain->carSlots[pTrain->wCarSlotCount])->pNameBMaybe;
        if (pFarAnchor->dwViewportExtendPhaseMaybe == 0 && pFarAnchor->pTileMaybe != 0) {
            BigObj *pKind = pFarAnchor->pTileMaybe->pKindDesc;
            if (pKind != 0 && pKind->IsType0x63aInSet1234() == 0) {
                pTrain->dwModeAMaybe = 0;
            }
        }
    } else if (pTrain->dwReversed == 1) {
        NameAnchorMaybe *pFarAnchor = ((CarNetObjAnchorPartial *)pTrain->carSlots[0])->pNameAMaybe;
        if (pFarAnchor->dwViewportExtendPhaseMaybe == 0 &&
            ((CarNetObjAnchorPartial *)pTrain->carSlots[pTrain->wCarSlotCount])->pNameBMaybe->pTileMaybe != 0) {
            BigObj *pKind = pFarAnchor->pTileMaybe->pKindDesc;
            if (pKind != 0 && pKind->IsType0x63aInSet1234() == 0) {
                pTrain->dwModeAMaybe = 0;
            }
        }
    }

    if (pTrain->dwModeAMaybe != 0) {
        return 0;
    }

    TrackConnectorTileObj *pSlot =
        g_worldBoard.GetPlaneASlotMaybe(pTrain->wSentinelCMaybe, pTrain->wSentinelDMaybe + 1, 0);
    if (pSlot != 0 && pSlot->dwOccupancyClaim != 0) {
        pSlot->dwOccupancyClaim = 0;
        pTrain->wSentinelCMaybe = -1;
        pTrain->wSentinelDMaybe = -1;
    }
    return 1;
}

// Minimal extern views for the singletons the tick calls into (per-TU partial-view
// precedent: src/DPlaySessionMgr.cpp's NetSessionEventQueueEdge / src/WidgetPicker.cpp's
// NetSessionEventQueuePartial, both for the same DAT_004a9990 singleton).
struct NetSessionEventQueuePickPartial {
    void *PickRandomBigObjByCategory(int category); // 0x41e1f0, extern
};
extern NetSessionEventQueuePickPartial g_NetSessionEventQueuePick; // DAT_004a9990  // TODO: idiom

// The despawn "puff of smoke" effect spawner (0x423ab0) is dispatched through the canonical
// singleton in src/EffectSpawner.h (included above) -- this TU's local
// BigObjTrackingSetsPartial view (an empty view class; the same declaration as
// src/NetSessionEventQueue.cpp's retired copy) was retired v500 when 0x423ab0's definition
// landed in src/EffectSpawner.cpp (lint_alias: the local spelling aliased the now-defined
// address).

// Hands this train the depot tile pTile: records the tile's grid position into the
// wClaimedPosXMaybe/wClaimedPosYMaybe pair, then either takes ownership or, if some OTHER train
// already owns the tile, parks this train on the tile's claim-wait FIFO and does nothing else
// (PumpClaimWaitQueueMaybe's per-tick pump re-enters here once the tile frees up). On a successful claim it
// locks the tile (bClaimLockedFlag), stamps itself as pOwningTrain, drives the whole train into
// the dwModeBMaybe==5 highlight state -- the train itself, every car slot, and both of each car's
// track anchors (dwTileExtendPhaseMaybe) -- silences it via sound state 2, and finalizes with a
// full anchor re-layout on the new lead tile. The tile's dwOccupancyClaim is cleared LAST, after
// the layout, so the tile reads as unclaimed again once the train is settled on it.
//
// SetCarsReady(1) is called TWICE, once on either side of the per-car mode-5 loop -- not a
// transcription slip; both calls are in the original (0x44c1b7 and 0x44c1fc). The second one is
// what actually resumes the cars' sound channels, since SetSoundStateMaybe(2) in between has by
// then moved dwSoundStateMaybe off the paused values the first call saw.
// FUNCTION: LOCO 0x44c170
void PeerTrainNodePartial::ClaimDecorObjMaybe(TrackDepotTileObj *pTile, char bKeepHeading)
{
    // dword-pair copy of the tile's wPosX/wPosY (the documented pair-copy idiom, same shape as
    // LayoutCarAnchorsMaybe's own wSentinelCMaybe snapshot above).
    *(unsigned int *)&this->wClaimedPosXMaybe = *(unsigned int *)&pTile->pos.wPosX;
    if ((pTile->pOwningTrain != 0) && (pTile->pOwningTrain != (void *)this)) {
        pTile->KeyedList_AddByKeyMaybe(this);
        return;
    }
    pTile->bClaimLockedFlag = 1;
    pTile->pOwningTrain = (void *)this;
    this->dwModeBMaybe = 5;
    this->SetCarsReady(true);
    for (unsigned int i = 0; i <= this->wCarSlotCount; i++) {
        ((CarNetObjAnchorPartial *)this->carSlots[i])->dwModeBMaybe = 5;
        ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameAMaybe->dwTileExtendPhaseMaybe = 5;
        ((CarNetObjAnchorPartial *)this->carSlots[i])->pNameBMaybe->dwTileExtendPhaseMaybe = 5;
    }
    this->SetSoundStateMaybe(2);
    this->SetCarsReady(true);
    this->LayoutCarAnchorsMaybe(pTile, bKeepHeading);
    pTile->dwOccupancyClaim = 0;
}

// The train node's own constructor: zero the whole node, give it a lead track anchor and a
// car 0 of kind nKindId, then -- only for a LOCALLY originated train (bPeerOwned == 0) -- mint
// a train id, park the anchors just off the top of the board and file the placement with the
// session roster. A car 0 whose own ctor left bValid clear is deleted again and the node is
// left with an empty slot 0 (every consumer null-checks carSlots[0], so there is no separate
// failure flag). nDiscardFlag is stored verbatim: callers pass 1 for a throwaway/rebuild node
// and 2 for a peer-spawned one.
//
// The car-slot array is cleared TWICE (once before the anchor is built, once right before car 0
// goes in) -- both clears are in the original, at 0x44bec4 and 0x44bf00.
//
// EFFECTIVE MATCH (asmscore.py --len 598: total 51696, align=50 reg_pen=15 identity_miss=15
// byte_diff=46, insns 184/187). Every block is structurally identical -- the /GX EH-state
// ladder across both protected `new`s (-1 -> 0 -> -1 -> 1 -> -1), both car-slot clears, the
// rep-stos of apPassengerMaybe, the re-read-per-use `carSlots[wCarSlotCount]` shape (same as
// PeerTrainNode_AllocCarSlot's, and the reason the `delete`'s null re-test survives), and the
// whole locally-originated-train tail. Three residuals, all register-allocation/scheduling
// tie-breaks:
//   (1) the scheduler emits `xor ebx,ebx` one slot later than the original (a pure swap with
//       the adjacent `mov [esi+4],ecx`);
//   (2) the original spends one extra `mov eax,ecx` copying the freshly-incremented train id
//       out of ECX so ECX is free for the upcoming thiscall's `this`; this build happens to
//       land the value in EAX already and needs no copy (1 instruction);
//   (3) SetTrainPlacementResult's byte arguments: the original widens bOwnerByteA to a dword
//       through a dead incoming-parameter stack slot (`mov [esp+0x34],al; mov eax,[esp+0x34]`)
//       while this build pushes the register directly (2 instructions). Both are the SAME
//       don't-care byte->dword widening -- the choice between them is allocator-driven, not a
//       type fact (the type fact, that the owner args are bytes and not dwords, was a real
//       lever and is fixed: see src/DPlaySessionMgr.h).
// Source-shape levers that DID land, one compile each: `this->wSelectedCarId =
// this->wSelectedCarIdAMaybe;` (reading the member back rather than re-reading the descriptor
// -- MSVC forwards the just-stored value as the original's `mov cx,dx`, DIFF 233 -> 174); an
// `int` local for selectedProviderIndex (a dword load feeding two byte stores, not two byte
// loads); and `int n = mgr->nNextTrainId + 1; mgr->nNextTrainId = n;` instead of `++` (which
// re-read BOTH the global pointer and the field after the store). See docs/PARKED.md.
// FUNCTION: LOCO 0x44be50 // TODO: sync (Ghidra: PeerTrainNode::PeerTrainNode_Ctor -- real C++
//   ctor syntax here, plus the standing PeerTrainNode/PeerTrainNodePartial class-name split.
//   Deliberately NOT moved into Ghidra's PeerTrainNodePartial namespace the way this TU's other
//   methods were: that namespace has no same-named Structure, so the move would degrade this
//   function's own `this` from a typed `PeerTrainNode *` to `void *` -- and this ctor is the
//   single best field-name oracle the class has. Closes when the deferred rename of the src-side
//   view to plain `PeerTrainNode` lands; see the pickup's own deferred-refactor item.)
PeerTrainNodePartial::PeerTrainNodePartial(int nKindId, int nDiscard, char bPeerOwned,
                                           char bHasDetail)
{
    this->wSentinelCMaybe = -1;
    this->wSentinelDMaybe = -1;
    this->wClaimedPosXMaybe = -1;
    this->wClaimedPosYMaybe = -1;
    this->bHasDetailFlagMaybe = bHasDetail;
    this->nDiscardFlag = nDiscard;
    this->pVtbl = g_vtable0x47836c;
    this->bUnk0x5a = 0;
    this->bUnk0x90 = 0;
    this->bUnk0x2c = 0;
    this->dwAckPlayerId = 0;
    this->nDeferredMoveStateMaybe = 0;
    this->pNext = 0;
    memset(this->apPassengerMaybe, 0, sizeof(this->apPassengerMaybe));
    memset(this->carSlots, 0, sizeof(this->carSlots));
    this->dwModeAMaybe = 0;
    this->pNameMaybe = new NameAnchorMaybe(bPeerOwned);
    this->dwUnk0x28 = 0;
    memset(this->carSlots, 0, sizeof(this->carSlots));
    this->wCarSlotCount = 0;
    this->carSlots[0] = (TilePlacedObjPartial *)new CarNetObj(nKindId, 2, bPeerOwned);
    if (this->carSlots[this->wCarSlotCount] != 0) {
        if (((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->bValid == true) {
            ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])
                ->pOwnerTrainNodeMaybe = this;
            CarKindDesc *pKind =
                (CarKindDesc *)((CarNetObjAnchorPartial *)this->carSlots[0])->pKindDesc;
            this->wSelectedCarIdAMaybe = pKind->wCarIdAMaybe;
            this->wSelectedCarIdBMaybe = pKind->wCarIdBMaybe;
            this->wSelectedCarId = this->wSelectedCarIdAMaybe;
            this->dwReversed = 0;
            this->SetSoundStateMaybe(0);
            this->wUnk0x36 = 0;
            if (g_pDPlaySessionMgr->connectionMode == 2) {
                int nProvider = g_pDPlaySessionMgr->selectedProviderIndex;
                this->bOwnerByteA = (unsigned char)nProvider;
                this->bOwnerByteB = (unsigned char)nProvider;
            }
            else {
                this->bOwnerByteA = 1;
                this->bOwnerByteB = 1;
            }
            if (bPeerOwned == 0) {
                int nTrainId = g_pDPlaySessionMgr->nNextTrainId + 1;
                g_pDPlaySessionMgr->nNextTrainId = nTrainId;
                this->wTrainId = (unsigned short)nTrainId;
                this->pNameMaybe->NameAnchorMaybe_ResolveTileAnchorMaybe(-1, -1, 0);
                int nAnchorX = this->pNameMaybe->nAnchorXMaybe + 0xc;
                ((CarNetObjAnchorPartial *)this->carSlots[0])
                    ->ResolveBothAnchorsToPointMaybe(nAnchorX, -1);
                short wGridX;
                if (nAnchorX < 0) {
                    wGridX = -1;
                }
                else {
                    wGridX = (short)(nAnchorX >> 4);
                }
                g_pDPlaySessionMgr->SetTrainPlacementResult(this->wTrainId, this->bOwnerByteA,
                                                            this->bOwnerByteB, wGridX, -1);
            }
            this->bAckCounterA = 0;
            this->bAckCounterB = 0;
            this->pNext = 0;
            if (bPeerOwned == 0) {
                this->dwModeBMaybe = 2;
                this->dwModeAMaybe = 0;
                this->SetCarsReady(false);
            }
            else {
                this->dwModeAMaybe = 2;
                this->dwModeBMaybe = 0;
                this->SetCarsReady(true);
            }
        }
        else {
            delete (CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount];
            this->carSlots[this->wCarSlotCount] = 0;
        }
    }
}

// Tears the train down: tell the session the train's origin slot is free again (unless the app
// never had a session, or this node is one of the detail-flagged peer nodes that was never
// announced), then release every occupied car slot and the train's lead track anchor.
//
// The car loop runs 0..wCarSlotCount INCLUSIVE -- the count doubles as the highest live index
// here exactly as it does in PeerTrainNode_AllocCarSlot above, so a lone-locomotive train
// (count 0) still releases slot 0.
// FUNCTION: LOCO 0x44c0d0 // TODO: sync (Ghidra: FUN_0044c0d0 -- the standing
//   PeerTrainNode/PeerTrainNodePartial class-name split, same as the ctor above)
PeerTrainNodePartial::~PeerTrainNodePartial()
{
    this->pVtbl = g_vtable0x47836c;
    if ((g_pDPlaySessionMgr != 0) && (this->bHasDetailFlagMaybe == 0)) {
        g_pDPlaySessionMgr->GameNet_BroadcastLocalOrigin(this->wTrainId, this->bOwnerByteA,
                                                         this->bOwnerByteB);
    }
    for (int i = 0; i <= this->wCarSlotCount; i++) {
        if (this->carSlots[i] != 0) {
            delete (CarNetObjAnchorPartial *)this->carSlots[i];
            this->carSlots[i] = 0;
        }
    }
    // The lead anchor goes through NameAnchorVtblProbe rather than `delete` for the same reason
    // CarNetObj::~CarNetObj does it -- NameAnchorMaybe models its vptr as a plain field.
    if (this->pNameMaybe != 0) {
        ((NameAnchorVtblProbe *)this->pNameMaybe)->ScalarDeletingDtor(1);
        this->pNameMaybe = 0;
    }
}

// FUNCTION: LOCO 0x44c150 // TODO: sync (Ghidra: PeerTrainNode::SetModeBMaybe -- the rename IS
//   pushed; the residue is only the standing PeerTrainNode/PeerTrainNodePartial class-name split,
//   same as the ctor and dtor above)
// Written as a SWITCH over two SetCarsReady calls, not `SetCarsReady(mode != 2)` and not an
// if/else. The folded form is out because the original really does branch (two separate
// `push 0`/`push 1` + `ret 4` tails, no shared call and no setne). The if/else form is out for
// CODEGEN #100's reason: it reached insns 10/10 with the ENTIRE residual being `cmp eax,2`
// where the original has `sub eax,2`, and that subtract IS VC5's switch lowering. One-case
// switches count -- the tell is the instruction, not the number of labels.
void PeerTrainNodePartial::SetModeBMaybe(int mode)
{
    this->dwModeBMaybe = mode;
    switch (mode) {
    case 2:
        this->SetCarsReady(false);
        break;
    default:
        this->SetCarsReady(true);
        break;
    }
}

// Appends one freshly-constructed car to the train. Refuses (0) once 3 slots are in use, or if
// the slot ONE PAST the current end is already occupied -- the slots are a fixed 4-entry array
// and the count doubles as the highest live index, so the +1 probe is the real "is there room"
// test. On success the count is bumped FIRST, the car is constructed into the new slot, and the
// car's own back-pointer to this train is stamped. A car whose ctor left bValid clear is deleted
// again, its slot cleared and the count rolled back, and the whole call reports failure.
//
// EFFECTIVE MATCH (asmscore.py --len 0xe8: total 22016, align=22 reg_pen=0 identity_miss=0
// byte_diff=16, insns 69/68). Every block is structurally identical -- the /GX-protected
// operator-new(0x450) + ctor pair with its EH-state stores, the count-bump-before-construct
// order, both reloads of carSlots[wCarSlotCount], the vtable-slot-0 scalar-deleting-dtor call.
// Sole residual is where the constant 1 gets materialized: the original hoists `mov eax,1`
// ABOVE the slot reload so AL is free to be the memory-compare operand
// (`cmp byte ptr [ecx+0x18], al`), while this build materializes it late and spends one extra
// instruction loading the byte first (`mov dl,[ecx+0x18]; mov eax,1; cmp dl,al`) -- the one
// instruction of the 69/68 gap.
// Probes, one compile each, all byte-neutral or worse: `!= false` instead of `== true`
// (byte_diff 19 but total 42679 -- it swaps the folded compare for a load+test+separate
// `mov al,1`, structurally further away); hoisting an explicit `unsigned char bAdded = 1;`
// above the null check and returning IT (compiled BIT-IDENTICAL, constant-propagated straight
// back); a cached `pCar` local instead of re-reading carSlots (WORSE, total 34020 -- it lets
// value-numbering fold away the `delete`'s own null re-test, which the original DOES emit;
// the no-local form here is what recovers it). VC5 /Og constant-materialization scheduling
// class -- see docs/PARKED.md.
// FUNCTION: LOCO 0x44c220
unsigned char PeerTrainNodePartial::PeerTrainNode_AllocCarSlot(int nKindId, int nCategory,
                                                              char bFlag)
{
    if ((this->wCarSlotCount < 3) && (this->carSlots[this->wCarSlotCount + 1] == 0)) {
        this->wCarSlotCount++;
        this->carSlots[this->wCarSlotCount] =
            (TilePlacedObjPartial *)new CarNetObj(nKindId, nCategory, bFlag);
        if (this->carSlots[this->wCarSlotCount] != 0) {
            if (((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->bValid == true) {
                ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])
                    ->pOwnerTrainNodeMaybe = this;
                return 1;
            }
            delete (CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount];
            this->carSlots[this->wCarSlotCount] = 0;
        }
        this->wCarSlotCount--;
    }
    return 0;
}

// FUNCTION: LOCO 0x44c310
// AllocCarSlot's counterpart: release the car sitting in slot nIndex, then slide every slot
// above it down one place so the occupied run stays contiguous from 0. Answers 0 when the index
// is out of range or the slot was already empty.
//
// PeerTrainSlotQueueMaybe::RebuildCarSlotsFromSelectionMaybe calls it three times with a
// CONSTANT index of 1, which is exactly how it drops all three towed cars while leaving the lead
// unit in slot 0 -- each call shifts the next survivor down into slot 1 for the following one.
//
// EXACT MATCH. The compact loop has to be spelled with a SUBSCRIPT induction variable, not the
// pointer walk the decompiler suggests: `ppSlot[-1] = *ppSlot; *ppSlot = 0;` over a
// `TilePlacedObjPartial **` emits the two stores in the OPPOSITE order the original uses
// (DIFF(7), insns 37/37, the only disagreement), and an explicit `pNext` temp does not move it.
// The subscript form lets /O2 strength-reduce to the same `lea eax,[edi+esi*4+0x14]` walk while
// keeping the source's store order -- worth 8012 -> 0.
unsigned char PeerTrainNodePartial::PeerTrainNode_ReleaseCarSlot(unsigned int nIndex)
{
    if (nIndex > 3) return 0;
    if (this->carSlots[nIndex] == 0) return 0;
    delete (CarNetObj *)this->carSlots[nIndex];
    this->carSlots[nIndex] = 0;
    this->wCarSlotCount--;
    if (nIndex < 3) {
        for (unsigned int i = nIndex; i < 3; i++) {
            this->carSlots[i] = this->carSlots[i + 1];
            this->carSlots[i + 1] = 0;
        }
    }
    return 1;
}

// Checks each of the train's car slots for wUnk0x448==1 (the "anchor sits at the claimed-tile
// socket boundary" flag SettleClaimedSocketMaybe/UpdateCarPlacementTickMaybe settle); as soon
// as one is found, latches pTrain->bUnk0x2c and stops scanning. Called by
// UpdateCarPlacementTickMaybe's own non-claimed-tile branch (the claimed-tile branch latches
// bUnk0x2c itself, via SettleClaimedSocketMaybe or its own simple bound check, and never
// reaches this function).
//
// EXACT MATCH (v346). Return type is `unsigned char`, not `unsigned int`: the original leaves
// garbage in EAX's upper 3 bytes at both exits (only the low byte, bUnk0x2c, is ever a real
// value) -- the usual "caller only consumes the low byte" CONCAT31 tell already documented for
// this TU's siblings (SettleClaimedSocketMaybe); declaring `unsigned int` made the compiler
// insert a defensive `xor eax,eax` this original doesn't have. Structural note: the "found a
// match" path uses `break` (not an early `return`) to reach the SAME single shared epilogue as
// the loop-completes-without-a-match path, matching the original's own single-`ret`-per-exit
// shape (it does NOT tail-merge the two exits into one, but each of its own two exits is
// itself a single unduplicated block) -- an early `return` inside the loop instead produced a
// third, fully duplicated epilogue the original doesn't have.
// FUNCTION: LOCO 0x44cab0 (?PeerTrainNode_DetectCarAtSocketBoundaryMaybe@@YIEPAUPeerTrainNodeTickPartial@@@Z)
unsigned char __fastcall PeerTrainNode_DetectCarAtSocketBoundaryMaybe(PeerTrainNodeTickPartial *pTrain)
{
    pTrain->bUnk0x2c = 0;
    for (int i = 0; i <= (int)pTrain->wCarSlotCount; i++) {
        if (((CarNetObjAnchorPartial *)pTrain->carSlots[i])->wUnk0x448 == 1) {
            pTrain->bUnk0x2c = 1;
            break;
        }
    }
    return pTrain->bUnk0x2c;
}

// FUNCTION: LOCO 0x44caf0
// The re-entrancy guard every reversal in the game goes through: refuse outright if a reversal
// is already in flight, otherwise raise bUnk0x5a for the duration of the commit and hand back
// the commit's own result. Sole caller is TrackDepotTileObj::ResetToBaseSubFrameMaybe
// (src/TilePlacedObj.cpp), which reaches it through the canonical PeerTrainNodePartial
// declaration in src/PeerTrainNode.h; the guard exists because ReverseDirectionMaybe below
// re-walks anchors that can tick their way back into a reversal request.
char PeerTrainNodeTickPartial::ReverseDirectionGuardedMaybe() // TODO: sync (TU-local view)
{
    if (this->bUnk0x5a != 0) {
        return 0;
    }
    this->bUnk0x5a = 1;
    char bReversed = this->ReverseDirectionMaybe();
    this->bUnk0x5a = 0;
    return bReversed;
}

// NameAnchorMaybe view adding the extern siblings ReverseDirectionMaybe calls (kept off the
// shared header per the TU-codegen-rotation precedent noted at WorldBoardSlotPartial).
struct NameAnchorReversePartial : NameAnchorMaybe {
    void ApplyDirectionReversalMaybe(PeerTrainNodeTickPartial *pTrain); // 0x40cd60, extern
    void CheckAnchorCrossedViewportMaybe();                                 // 0x40cc20, extern
    void CheckAnchorCrossedTileBoundMaybe();                                 // 0x40cc90, extern
};

// Commits a direction reversal already validated by CanReverseDirectionMaybe: refuses while a
// tile/viewport extend is mid-flight (dwModeBMaybe==2 / dwModeAMaybe 2/3), swaps the train's
// dwModeAMaybe 1<->4, toggles dwReversed, then per car: notifies both anchors
// (ApplyDirectionReversalMaybe), swaps the car's dwModeAMaybe 1..2->4 / 4->1 (forcing 1 while either anchor
// is viewport-extending), and rotates the car's dwModeBMaybe (1->4, 2->5+not-ready, 4->1 +
// CheckCarEnteredDepotMaybe, 5-> same as 4 if either anchor is tile-extending else 2+not-ready). Moves the
// lead tile's nOccupantRefCount to the NEW lead tile, re-derives the lead anchor's tail from
// the new trailing car's far anchor (CopyTailFrom) and re-walks it up to 12 socket steps
// (AdvanceAlongTrackMaybe; exactly 12 when reversed, until it stalls when forward). Finally
// re-runs the anchor's CheckAnchorCrossedViewportMaybe/CheckAnchorCrossedTileBoundMaybe pair and, off the lead anchor's
// dwTileExtendPhaseMaybe, resolves the train's dwModeBMaybe: phase 0 releases any recorded depot-extend
// claim (freeing the claimed slot when this train still owns it) or parks at 4 when a car is
// still extending; phases 1/2 park at 1; phases 4/5 park at 4. Returns the last
// AdvanceAlongTrackMaybe continue-flag (0 when refused up front).
// FUNCTION: LOCO 0x44cb10
//
// EFFECTIVE MATCH (tools/match.py: 9 diff bytes, len 760 vs orig 714+pad): every block is
// structurally identical to the original -- the dwModeAMaybe 1<->4 DEC/SUB-cascade switch,
// the SETZ dwReversed toggle, the for-loop car walk (EDI pointer strength-reduction with
// per-statement [EDI] reloads), the per-car dwModeAMaybe range nest (>2 / ==4 vs else),
// the dwModeBMaybe jump-table switch with case blocks emitted in the original's 2, 1, 5, 4
// memory order (matched by reordering the source cases), the nOccupantRefCount DEC/INC move,
// both re-walk loops (the forward one's bottom-test `while (bCont != 0 && ++nSteps < 0xc)`
// form suppresses the peeled rotation a break-inside-do-while produces), and the closing
// dwTileExtendPhaseMaybe switch with case blocks in 1/2, 0, 4/5 memory order and its break-to-shared-
// return tail (per-case returns give case 4/5 its own duplicate epilogue). Sole residual is
// a 9-byte scheduling swap at +0x51: the original emits the loop-entry guard `CMP word
// [wCarSlotCount],BX; JB exit` BEFORE the `LEA EDI,[carSlots]` pointer init; this build
// schedules the LEA between the CMP and the JB (same near-jump encodings, same bytes
// otherwise). Variants tried: declaration order of i/ppCar/bCont, declaring i inside the
// for, carSlots[i] indexing instead of the pointer walk (much worse -- rotates the whole
// prologue). v501: the #51 un-cache probe too -- dropping the pCar local and re-reading
// `(*ppCar)->` at every use scores DIFF(9), byte-identical to the cached form (the explicit
// `pCar = *ppCar` re-reads already buy what #51 buys), so the cached form stays.
// One-line /Og scheduling tie-break; parked per the EFFECTIVE protocol.
unsigned char PeerTrainNodeTickPartial::ReverseDirectionMaybe()
{
    if ((this->dwModeBMaybe == 2) || (this->dwModeAMaybe == 2) || (this->dwModeAMaybe == 3)) {
        return 0;
    }
    int dMode = this->dwModeAMaybe;
    switch (dMode) {
    case 1:
        this->dwModeAMaybe = 4;
        break;
    case 4:
        this->dwModeAMaybe = 1;
        break;
    }
    this->dwReversed = (int)(this->dwReversed == 0);
    CarNetObjAnchorPartial **ppCar = (CarNetObjAnchorPartial **)&this->carSlots[0];
    unsigned char bCont;
    for (int i = 0; i <= (int)this->wCarSlotCount; i++, ppCar++) {
        ((NameAnchorReversePartial *)(*ppCar)->pNameAMaybe)->ApplyDirectionReversalMaybe(this);
        ((NameAnchorReversePartial *)(*ppCar)->pNameBMaybe)->ApplyDirectionReversalMaybe(this);
        CarNetObjAnchorPartial *pCar = *ppCar;
        int dCarMode = pCar->dwModeAMaybe;
        if (dCarMode > 0) {
            if (dCarMode > 2) {
                if (dCarMode == 4) {
                    pCar->dwModeAMaybe = 1;
                }
            }
            else {
                pCar->dwModeAMaybe = 4;
            }
        }
        pCar = *ppCar;
        if ((pCar->pNameAMaybe->dwViewportExtendPhaseMaybe == 1) || (pCar->pNameBMaybe->dwViewportExtendPhaseMaybe == 1)) {
            pCar->dwModeAMaybe = 1;
        }
        pCar = *ppCar;
        switch (pCar->dwModeBMaybe) {
        case 2:
            pCar->dwModeBMaybe = 5;
            (*ppCar)->bReady = 0;
            break;
        case 1:
            pCar->dwModeBMaybe = 4;
            break;
        case 4:
            pCar->dwModeBMaybe = 1;
            (*ppCar)->CheckCarEnteredDepotMaybe(this);
            break;
        case 5:
            if ((pCar->pNameAMaybe->dwTileExtendPhaseMaybe == 1) || (pCar->pNameBMaybe->dwTileExtendPhaseMaybe == 1)) {
                pCar->dwModeBMaybe = 1;
                (*ppCar)->CheckCarEnteredDepotMaybe(this);
            }
            else {
                pCar->dwModeBMaybe = 2;
                (*ppCar)->bReady = 0;
            }
            break;
        }
    }
    this->pNameMaybe->pTileMaybe->nOccupantRefCount--;
    if (this->dwReversed == 1) {
        this->pNameMaybe->CopyTailFrom(
            ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe);
        this->pNameMaybe->pTileMaybe->nOccupantRefCount++;
        int nSteps = 0xc;
        do {
            bCont = this->pNameMaybe->AdvanceAlongTrackMaybe(this);
            nSteps = nSteps - 1;
        } while (nSteps != 0);
    }
    else {
        this->pNameMaybe->CopyTailFrom(((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe);
        this->pNameMaybe->pTileMaybe->nOccupantRefCount++;
        int nSteps = 0;
        do {
            bCont = this->pNameMaybe->AdvanceAlongTrackMaybe(this);
        } while ((bCont != 0) && (++nSteps < 0xc));
    }
    if ((this->dwModeAMaybe == 4) && (this->pNameMaybe->dwViewportExtendPhaseMaybe == 2)) {
        this->pNameMaybe->dwViewportExtendPhaseMaybe = 4;
    }
    ((NameAnchorReversePartial *)this->pNameMaybe)->CheckAnchorCrossedViewportMaybe();
    ((NameAnchorReversePartial *)this->pNameMaybe)->CheckAnchorCrossedTileBoundMaybe();
    switch (this->pNameMaybe->dwTileExtendPhaseMaybe) {
    case 1:
    case 2:
        this->dwModeBMaybe = 1;
        this->SetCarsReady(1);
        return bCont;
    case 0:
        if ((((CarNetObjAnchorPartial *)this->carSlots[0])->dwModeBMaybe != 4) &&
            (((CarNetObjAnchorPartial *)this->carSlots[0])->dwModeBMaybe != 5) &&
            (((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->dwModeBMaybe != 4) &&
            (((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->dwModeBMaybe != 5)) {
            if (this->dwModeBMaybe != 0) {
                TrackDepotTileObj *pSlot = (TrackDepotTileObj *)g_worldBoard.GetPlaneASlotMaybe(
                    this->wClaimedPosXMaybe, this->wClaimedPosYMaybe + 1, 0);
                if ((pSlot != 0) && (pSlot->pOwningTrain == this)) {
                    pSlot->dwOccupancyClaim = 0;
                    pSlot->bClaimLockedFlag = 0;
                    pSlot->ReleaseChannelAndDispatch(0);
                    pSlot->pOwningTrain = 0;
                }
                this->wClaimedPosXMaybe = -1;
                this->wClaimedPosYMaybe = -1;
                this->dwModeBMaybe = 0;
                this->SetCarsReady(1);
            }
            return bCont;
        }
        this->dwModeBMaybe = 4;
        this->SetCarsReady(1);
        break;
    case 4:
    case 5:
        this->dwModeBMaybe = 4;
        this->SetCarsReady(1);
        break;
    default:
        break;
    }
    return bCont;
}

// Returns the tile that would become the train's new lead after a direction reversal -- the
// trailing car's B-anchor tile when currently forward, the leading car's A-anchor tile when
// currently reversed (the same target CanReverseDirectionMaybe computes) -- but only when
// that tile is a level crossing (dwTrackState==7, which needs an explicit occupancy claim;
// see docs/subsystems.md's TrackTileObj::dwTrackState enum); NULL otherwise. The mask is
// the branchless SETNZ/DEC/AND select idiom.
// FUNCTION: LOCO 0x44d4c0
TrackTileObj *PeerTrainNodeTickPartial::GetReversalTargetTileMaybe()
{
    NameAnchorMaybe *pName;
    if (this->dwReversed == 0) {
        pName = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe;
    }
    else {
        pName = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe;
    }
    TrackTileObj *pTile = pName->pTileMaybe;
    return (pTile->dwTrackState == 7) ? pTile : 0;
}

// Arms or disarms every car of the train at once: sets each occupied car slot's inherited
// AnimDescRefObj0x477488::bReady to bFlag, dirty-marks that car's rect so the change repaints,
// and pauses/resumes the car's own DSound channel to match. Skipped entirely while the train is
// mid-rebuild (dwModeAMaybe 2/3) or has a peer hand-off in flight (nDeferredMoveStateMaybe != 0).
// Car 0 additionally gets its frame sound (re)started first if it has no channel yet.
//
// Only cars whose ready state actually CHANGES are touched -- the original spells that out as an
// explicit two-conjunct test rather than a `!=` (see below), which is why bFlag is retested on
// the already-ready path.
// FUNCTION: LOCO 0x44d500
void PeerTrainNodePartial::SetCarsReady(bool bFlag)
{
    if ((this->dwModeAMaybe == 2) || (this->dwModeAMaybe == 3) ||
        (this->nDeferredMoveStateMaybe != 0)) {
        return;
    }
    if (((CarNetObjAnchorPartial *)this->carSlots[0])->pDSoundChannel == 0) {
        ((CarNetObjAnchorPartial *)this->carSlots[0])->StartFrameSoundMaybe();
    }
    for (int i = 0; i <= this->wCarSlotCount; i++) {
        if (this->carSlots[i] != 0) {
            // The original's own short-circuit shape: the `bFlag` operand is evaluated twice,
            // once per conjunct, which a plain `(pCar->bReady != 0) != (bFlag != 0)` does not
            // reproduce (that folds to a single test).
            if ((bFlag != 0 && ((CarNetObjAnchorPartial *)this->carSlots[i])->bReady == 0) ||
                (bFlag == 0 && ((CarNetObjAnchorPartial *)this->carSlots[i])->bReady != 0)) {
                ((CarNetObjAnchorPartial *)this->carSlots[i])->bReady = bFlag;
                g_worldBoard.MarkRectDirty(((CarNetObjAnchorPartial *)this->carSlots[i])->rect);
                DSoundChannel *pChannel =
                    ((CarNetObjAnchorPartial *)this->carSlots[i])->pDSoundChannel;
                if (pChannel != 0) {
                    if ((bFlag == 0) || (this->dwSoundStateMaybe == 0) ||
                        (this->dwSoundStateMaybe == 1) || (this->dwSoundStateMaybe == 4)) {
                        pChannel->Pause();
                    }
                    else {
                        pChannel->ResumeOrRestart();
                    }
                }
            }
        }
    }
}

// Gates a direction reversal: refuses while mid-transition (dwSoundStateMaybe==4 or the
// bUnk0x5a re-entry guard is set). Otherwise inspects the CURRENT lead tile's kind and the
// would-be-new-lead tile's kind (the same end GetReversalTargetTileMaybe picks) and refuses
// (0) only when both tiles exist, both kinds are track families (connector Set1234 or depot
// Set), and the two kinds DIFFER -- i.e. a reversal is only blocked when it would hand the
// lead between two mismatched track-family tiles. Returns 1 otherwise.
// FUNCTION: LOCO 0x44d630
//
// EFFECTIVE MATCH (tools/match.py: 6 diff bytes, len 139 == orig): every block identical --
// the MOV BL,1-up-front flag form, the shared post-branch lead-tile deref (via a hoisted
// pLead = pNameMaybe local), the IsType0x63aInSet1234/IsType0x63aInSet short-circuit pairs,
// the pKindB != pKindA mismatch test. Sole residual is one register-allocation tie-break at
// +0x15: the original tests dwReversed in EAX (reserving EDX for the arm-1 movzx index);
// this build tests it in EDX and movzx's into EAX instead. Same instruction count, same
// blocks -- /Og allocation coin-flip.
//
// Re-scored v359 under SP3: still 5 raw byte diffs, insns 58/58. The residual is really one
// SCHEDULING difference with the register choice riding on it -- the original emits
// `mov eax,[ecx+8] / test eax,eax / mov eax,[ecx+0x20] / jne`, i.e. it reloads eax with pLead
// INTO the test's shadow, before the branch; we load pLead first and test dwReversed in edx
// afterwards. PROBED AND REFUTED (v359): moving `pLead = this->pNameMaybe` to AFTER the
// if/else, on the theory that MSVC would re-hoist the load above the arms (it must hoist it
// somewhere, since arm 1 clobbers ecx/`this`). It does not hoist -- it keeps `this` alive and
// loads pLead after the merge, going 5 -> 15 byte diffs. The pre-branch read is the correct
// source order; only the scheduling of it is off.
unsigned char PeerTrainNodeTickPartial::CanReverseDirectionMaybe()
{
    unsigned char bOk = 1;
    if ((this->dwSoundStateMaybe == 4) || (this->bUnk0x5a != 0)) {
        return 0;
    }
    NameAnchorMaybe *pLead = this->pNameMaybe;
    NameAnchorMaybe *pName;
    if (this->dwReversed == 0) {
        pName = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe;
    }
    else {
        pName = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe;
    }
    TrackTileObj *pLeadTile = pLead->pTileMaybe;
    if (pLeadTile != 0) {
        BigObj *pKindA = pLeadTile->pKindDesc;
        if ((pKindA->IsType0x63aInSet1234() != 0) || (pKindA->IsType0x63aInSet() != 0)) {
            TrackTileObj *pTile = pName->pTileMaybe;
            if (pTile != 0) {
                BigObj *pKindB = pTile->pKindDesc;
                if (((pKindB->IsType0x63aInSet1234() != 0) || (pKindB->IsType0x63aInSet() != 0)) &&
                    (pKindB != pKindA)) {
                    bOk = 0;
                }
            }
        }
    }
    return bOk;
}

// Requests a direction change: no-op if the train already runs in the requested direction
// (dwReversed == bDirection) or a reversal is mid-flight; otherwise -- unless bForce is set --
// validates via CanReverseDirectionMaybe, then commits via ReverseDirectionMaybe under the
// bUnk0x5a re-entry guard.
// FUNCTION: LOCO 0x44d5e0
void PeerTrainNodeTickPartial::TryTransitionModeMaybe(int bDirection, char bForce)
{
    if ((this->dwReversed == bDirection) || (this->bUnk0x5a != 0)) {
        return;
    }
    if ((bForce == 0) && (this->CanReverseDirectionMaybe() == 0)) {
        return;
    }
    if (this->bUnk0x5a == 0) {
        this->bUnk0x5a = 1;
        this->ReverseDirectionMaybe();
        this->bUnk0x5a = 0;
    }
}

// SetSoundStateMaybe filtered through a no-change gate plus one blocked transition: a
// 1 -> nonzero change is dropped (state 1 may only be exited back to 0 here).
// FUNCTION: LOCO 0x44d720
void PeerTrainNodeTickPartial::SetSoundStateIfChangedMaybe(int state)
{
    if ((this->dwSoundStateMaybe != state) &&
        ((this->dwSoundStateMaybe != 1) || (state == 0))) {
        this->SetSoundStateMaybe(state);
    }
}

// Marks one of the selection-pair car ids as the train's active selection: when wCarId
// matches wSelectedCarIdAMaybe (or wSelectedCarIdBMaybe), latches it into wSelectedCarId and
// re-applies car 0's cached anim frame (ReleaseChannelAndDispatch with the pair-end selector
// 0/1, then SetStateArgMaybe of nAnimValueCache). Returns the latched wSelectedCarId (0 when
// car 0 is unclaimed).
// FUNCTION: LOCO 0x44d6c0
unsigned short PeerTrainNodePartial::PeerTrainNode_UpdateSelectedCar(short wCarId)
{
    if (this->carSlots[0] == 0) {
        return 0;
    }
    if (wCarId == (short)this->wSelectedCarIdAMaybe) {
        this->wSelectedCarId = wCarId;
        int nCache = ((CarNetObj *)this->carSlots[0])->nAnimValueCache;
        ((CarNetObj *)this->carSlots[0])->ReleaseChannelAndDispatch(0);
        ((CarNetObj *)this->carSlots[0])->SetStateArgMaybe(nCache, 0);
    }
    else if (wCarId == (short)this->wSelectedCarIdBMaybe) {
        int nCache = ((CarNetObj *)this->carSlots[0])->nAnimValueCache;
        this->wSelectedCarId = wCarId;
        ((CarNetObj *)this->carSlots[0])->ReleaseChannelAndDispatch(1);
        ((CarNetObj *)this->carSlots[0])->SetStateArgMaybe(nCache, 0);
    }
    return this->wSelectedCarId;
}

// The train's sound-state setter: refuses no-change transitions and any change while
// bUnk0x90 is set, latches dwSoundStateMaybe, then -- state 0 also clears the dwUnk0x28
// countdown; states 0/4 (fall-through) clear the wUnk0x36 wait countdown; states 0/4/1 PAUSE
// every car's DSoundChannel; any other state (2/3) resumes every car's channel -- first
// acquiring car 0's (StartFrameSoundMaybe) when it's missing -- unless a peer hand-off is deferred
// (nDeferredMoveStateMaybe) or a viewport extend is mid-flight (dwModeAMaybe 2/3).
// FUNCTION: LOCO 0x44d740
//
// EXACT (v501). Was EFFECTIVE (9 diff bytes) from v329, parked as the same /Og LEA-scheduling
// tie-break as ReverseDirectionMaybe ("if that one-insn-swap class ever cracks, retry all three
// rows together"). The #53 subscript-induction sweep cracked it: writing both car loops as
// subscripted `this->carSlots[i]` walks -- no `ppCar++` element pointer -- removes the offending
// `LEA EDI,[carSlots]` entirely, so the entry-guard scheduling question goes with it. NOTE the
// lesson does NOT transfer to ReverseDirectionMaybe (0x44cb10): its note records the subscript
// probe there as much worse (it rotates the whole prologue), so that one stays parked.
void PeerTrainNodePartial::SetSoundStateMaybe(int state)
{
    if ((this->dwSoundStateMaybe == state) || (this->bUnk0x90 != 0)) {
        return;
    }
    this->dwSoundStateMaybe = state;
    switch (state) {
    case 0:
        this->dwUnk0x28 = 0;
        // fall through
    case 4:
        this->wUnk0x36 = 0;
        // fall through
    case 1: {
        for (int i = 0; i <= (int)this->wCarSlotCount; i++) {
            DSoundChannel *pCh = ((CarNetObjAnchorPartial *)this->carSlots[i])->pDSoundChannel;
            if (pCh != 0) {
                pCh->Pause();
            }
        }
        break;
    }
    default: {
        if (this->nDeferredMoveStateMaybe != 0) {
            return;
        }
        if ((this->dwModeAMaybe == 2) || (this->dwModeAMaybe == 3)) {
            return;
        }
        CarNetObjAnchorPartial *pCar0 = (CarNetObjAnchorPartial *)this->carSlots[0];
        if (pCar0->pDSoundChannel == 0) {
            pCar0->StartFrameSoundMaybe();
        }
        for (int i = 0; i <= (int)this->wCarSlotCount; i++) {
            DSoundChannel *pCh = ((CarNetObjAnchorPartial *)this->carSlots[i])->pDSoundChannel;
            if (pCh != 0) {
                pCh->ResumeOrRestart();
            }
        }
        break;
    }
    }
}

// The dwModeBMaybe 4/5 follow-up the tick runs every pass (see the end of the tick's main
// do-while): once the trailing/leading car's relevant anchor has cleared its tile-hand-off
// (dwTileExtendPhaseMaybe == 0) and that anchor's tile is no longer in track-state 4, releases the
// depot-extend claim: clears the train's dwModeBMaybe, re-readies its cars, frees the claimed
// depot slot below the recorded wClaimedPosXMaybe/wClaimedPosYMaybe grid position, and drops
// the recorded claim (both words to the 0xffff sentinel).
// FUNCTION: LOCO 0x44c9b0
void PeerTrainNodeTickPartial::CompleteDepotExitMaybe()
{
    NameAnchorMaybe *pName;
    if (((this->dwReversed == 0) &&
         (pName = ((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->pNameBMaybe,
          pName->dwTileExtendPhaseMaybe == 0) &&
         (pName->pTileMaybe->dwTrackState != 4)) ||
        ((this->dwReversed == 1) &&
         (pName = ((CarNetObjAnchorPartial *)this->carSlots[0])->pNameAMaybe,
          pName->dwTileExtendPhaseMaybe == 0) &&
         (pName->pTileMaybe->dwTrackState != 4))) {
        this->dwModeBMaybe = 0;
        this->SetCarsReady(1);
        TrackDepotTileObj *pSlot = (TrackDepotTileObj *)g_worldBoard.GetPlaneASlotMaybe(
            this->wClaimedPosXMaybe, this->wClaimedPosYMaybe + 1, 0);
        if (pSlot != 0) {
            pSlot->dwOccupancyClaim = 0;
            pSlot->bClaimLockedFlag = 0;
            pSlot->pOwningTrain = 0;
        }
        this->wClaimedPosXMaybe = -1;
        this->wClaimedPosYMaybe = -1;
    }
}

// Single-pass form of the tick's lead-tile dwTrackTickState dispatch (same tri-state SWITCH
// shape as the tick's own two -- the sub/dec/dec cascade is the switch tell): refuses to run
// with no sound state or no lead tile (returns 0), and otherwise -- state 0: force sound
// state 2; state 1: guarded reversal then sound state 2; state 2: park into sound state 1
// (a no-op return 0 if already there); anything else: report movable (1) untouched.
// FUNCTION: LOCO 0x44ca50
unsigned char PeerTrainNodeTickPartial::ApplyLeadTileTickStateMaybe()
{
    int dState = this->dwSoundStateMaybe;
    TrackTileObj *pTile;
    if ((dState == 0) || (pTile = this->pNameMaybe->pTileMaybe, pTile == 0)) {
        return 0;
    }
    unsigned char bOk;
    switch (pTile->dwTrackTickState) {
    case 0:
        this->SetSoundStateMaybe(2);
        bOk = 1;
        break;
    case 1:
        if (this->bUnk0x5a == 0) {
            this->bUnk0x5a = 1;
            this->ReverseDirectionMaybe();
            this->bUnk0x5a = 0;
        }
        this->SetSoundStateMaybe(2);
        bOk = 1;
        break;
    case 2:
        if (dState == 1) {
            return 0;
        }
        this->SetSoundStateMaybe(1);
        return 0;
    default:
        bOk = 1;
        break;
    }
    return bOk;
}

// The train's per-tick placement driver: handles pending reversal requests and the reversal
// countdown, then -- when the train is in a movable sound state with a selection active --
// advances the lead anchor along its tile (NameAnchorMaybe::AdvanceAlongTrackMaybe), ticks
// every car in travel order (front-to-back, or back-to-front just after a reversal), spawns
// the smoke-puff effect while the 0x1808 car is moving, and drives the viewport-extend
// (dwModeAMaybe) / tile-extend (dwModeBMaybe) hand-off state machines, including the
// multiplayer ITEMKIND_TRACK_GLOBAL_{E,W,N,S} (0xc42..0xc48) connector-tile peer handoff
// (sets nDeferredMoveStateMaybe=1). After the loop it repositions each car's DSoundChannel,
// retries a failed connector hand-off, runs the dwSoundStateMaybe==4 reselect/reverse
// countdown, files the train's placement result to the session host when anything moved,
// and notifies DPlaySessionMgr on the 0xc5c..0xc60 tile kinds. Returns void.
// FUNCTION: LOCO 0x44c3a0
//
// EFFECTIVE MATCH (asmscore.py --len 1520: total 694239, align=688 reg_pen=53
// identity_miss=53 byte_diff=409, insns 536/514): every block is structurally identical to
// the original -- the sub/dec/dec cascade BOTH dwTrackTickState tri-state dispatches are
// SWITCH statements (a ==-chain mis-transcription compiled to test/cmp/cmp; the cascade is
// the tell), the dwTrackState==1 gate sharing its SetSoundStateMaybe(2) tails across case 0
// and the bUnk0x5a!=0 arm of case 1, the bMoved flag carried in AL with its true/false
// stores tail-merged (bMoved=true must be a per-case statement, NOT a declaration init --
// the init form pinned the flag in BL), the byte-counter car-tick loops (INC BL + the
// [esp+0x18] spill/AND 0xff reload idiom via a separate int index var), the reversed loop's
// if-guarded do-while pointer walk, the per-statement this->pNameMaybe reloads in the
// modeA==1 hand-off block, the 0xc42..0xc48 jump-table switch, and the 0xc5c..0xc60
// sub-chain switch. Residual is the documented VC5 /Og class (same family as the parked
// 0x40bbd0/0x44ce10 in this subsystem): (1) case-1 + the shared SetSoundStateMaybe(2) +
// bMoved=true blocks of the FIRST switch are out-of-lined after the main RET in the
// original but stay inline here -- same source case order, same relative block order, pure
// trace-driven placement (the 0x40bbd0 park class); (2) const-1 materialization timing
// (orig: EBX=1 first at the modeA check + EDI=1 inside the SetCarsReady(1) block; ours:
// EBX=1 hoisted to the loop preheader, with push-reg instead of push-imm downstream);
// (3) EffectSpawner arg-prep scheduling (y in EDX/x in EAX + the mov ecx,0x4fd220 up-front
// in the orig); (4) do-while tail rotation (jc-back vs jae-exit/jmp-back); (5) scattered
// reg tie-breaks. Variants tried (score path 762k-ish first draft -> 694k): ==-chain vs
// nested-!= vs switch for both tri-state dispatches (switch is correct), bMoved init vs
// per-case stores (per-case correct -- landed the AL flag + shared tails), for-loop vs
// byte-counter do-while for both car loops (do-while correct), 0xc5c block as ==-chain vs
// switch (switch correct -- orig's sub-chain). PARKED alongside 0x44ce10 in this TU (docs/
// PARKED.md row) -- both sit on the same /Og class and share this TU's context, so a future
// crack of the trace-layout/const-allocation class likely un-parks them together.
void PeerTrainNodeTickPartial::PeerTrainNode_UpdatePlacementTickMaybe()
{
    int nMoved = 0;
    TrackTileObj *pTile;
    if ((this->bUnk0x90 != 0) &&
        (this->bUnk0x90 = 0, this->dwReversed != (int)(this->dwReversed == 0)) &&
        (this->bUnk0x5a == 0)) {
        this->bUnk0x5a = 1;
        this->ReverseDirectionMaybe();
        this->bUnk0x5a = 0;
    }
    if ((this->wUnk0x36 != 0) && (--this->wUnk0x36 == 1) && (this->dwSoundStateMaybe == 1)) {
        this->SetSoundStateMaybe(2);
    }
    int dState = this->dwSoundStateMaybe;
    if ((dState != 0) && (dState != 4) &&
        (this->dwModeAMaybe != 2) && (this->dwModeAMaybe != 3) &&
        (this->dwModeBMaybe != 2) && (this->wSelectedCarId != 0) &&
        ((unsigned short)this->wUnk0x36 <= 1) && ((dState == 2) || (dState == 1))) {
        bool bMoved;
        pTile = this->pNameMaybe->pTileMaybe;
        if (pTile == 0) {
            bMoved = false;
        }
        else {
            unsigned int dTickState = pTile->dwTrackTickState;
            switch (dTickState) {
            case 0:
                this->SetSoundStateMaybe(2);
                bMoved = true;
                break;
            case 1:
                if (this->bUnk0x5a == 0) {
                    this->bUnk0x5a = 1;
                    this->ReverseDirectionMaybe();
                    this->bUnk0x5a = 0;
                }
                this->SetSoundStateMaybe(2);
                bMoved = true;
                break;
            case 2:
                if (dState != 1) {
                    this->SetSoundStateMaybe(1);
                }
                bMoved = false;
                break;
            default:
                bMoved = true;
                break;
            }
        }
        if (bMoved) {
            unsigned int nIter = 0;
            if (this->wSelectedCarId != 0) {
                do {
                    NameAnchorMaybe *pName = this->pNameMaybe;
                    if ((pName->dwViewportExtendPhaseMaybe == 2) || (pName->dwTileExtendPhaseMaybe == 2)) {
                        nMoved = 1;
                    }
                    else {
                        if (pName->AdvanceAlongTrackMaybe(this) == 0) {
                            break;
                        }
                        nMoved = nMoved + 1;
                        unsigned int dPhase = this->pNameMaybe->dwViewportExtendPhaseMaybe;
                        if ((dPhase == 2) || (dPhase == 3)) {
                            nMoved = 1;
                        }
                        else if ((this->dwModeBMaybe != 2) && (this->pNameMaybe->dwTileExtendPhaseMaybe == 2)) {
                            this->dwModeBMaybe = 1;
                            this->SetCarsReady(1);
                            nMoved = 1;
                        }
                    }
                    if (this->dwSoundStateMaybe == 2) {
                        CarNetObjAnchorPartial *pCar0 = (CarNetObjAnchorPartial *)this->carSlots[0];
                        BigObj *pCarKind = pCar0->pKindDesc;
                        int nCarKind;
                        if (pCarKind == 0) {
                            nCarKind = -1;
                        }
                        else {
                            nCarKind = pCarKind->resourceId;
                        }
                        if (((nCarKind == 0x1808) && (pCar0->bReady != false)) &&
                            (pCar0->dwModeBMaybe == 0) && (pCar0->dwModeAMaybe == 0)) {
                            if (this->bSmokePuffCounterMaybe == '\n') {
                                DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(
                                    0x3883, 0, 'W',
                                    pCar0->pNameAMaybe->nAnchorXMaybe + 2,
                                    pCar0->pNameAMaybe->nAnchorYMaybe - 0x12, 0);
                                this->bSmokePuffCounterMaybe = 0;
                            }
                            this->bSmokePuffCounterMaybe = this->bSmokePuffCounterMaybe + 1;
                        }
                        if (this->dwReversed == 0) {
                            unsigned char i = 0;
                            int u = 0;
                            if (this->wCarSlotCount + 1 > 0) {
                                do {
                                    ((CarNetObjAnchorPartial *)this->carSlots[u])->UpdateCarPlacementTickMaybe(this, nMoved);
                                    i = i + 1;
                                    u = i;
                                } while (u < this->wCarSlotCount + 1);
                            }
                        }
                        else if (this->dwReversed == 1) {
                            char cLast = (char)this->wCarSlotCount;
                            if (cLast > -1) {
                                CarNetObjAnchorPartial **ppCar =
                                    (CarNetObjAnchorPartial **)&this->carSlots[cLast];
                                int nLeft = cLast + 1;
                                do {
                                    (*ppCar)->UpdateCarPlacementTickMaybe(this, nMoved);
                                    ppCar = ppCar - 1;
                                    nLeft = nLeft - 1;
                                } while (nLeft != 0);
                            }
                        }
                    }
                    pTile = this->pNameMaybe->pTileMaybe;
                    if (((pTile != 0) && (pTile->dwTrackState == 1)) && (this->dwSoundStateMaybe != 0)) {
                        unsigned int dTick2 = pTile->dwTrackTickState;
                        switch (dTick2) {
                        case 0:
                            this->SetSoundStateMaybe(2);
                            break;
                        case 1:
                            if (this->bUnk0x5a != 0) {
                                this->SetSoundStateMaybe(2);
                            }
                            else {
                                this->bUnk0x5a = 1;
                                this->ReverseDirectionMaybe();
                                this->bUnk0x5a = 0;
                                this->SetSoundStateMaybe(2);
                            }
                            break;
                        case 2:
                            if (this->dwSoundStateMaybe != 1) {
                                this->SetSoundStateMaybe(1);
                            }
                            break;
                        }
                    }
                    if (this->dwModeAMaybe == 1) {
                        if ((((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->dwModeAMaybe == 2) &&
                            (((CarNetObjAnchorPartial *)this->carSlots[0])->dwModeAMaybe == 2)) {
                            this->dwModeAMaybe = 2;
                            this->pNameMaybe->dwViewportExtendPhaseMaybe = 2;
                            ((TrackConnectorTileObj *)this->pNameMaybe->pTileMaybe)->dwOccupancyClaim = 0;
                            switch (this->pNameMaybe->pTileMaybe->pKindDesc->resourceId) {
                            case 0xc42:
                            case 0xc44:
                            case 0xc46:
                            case 0xc48:
                                this->dwModeAMaybe = 3;
                                this->SetCarsReady(0);
                                this->nDeferredMoveStateMaybe = 1;
                                break;
                            default: {
                                void *pPicked = g_NetSessionEventQueuePick.PickRandomBigObjByCategory(0);
                                if (pPicked == 0) {
                                    this->SetSoundStateMaybe(3);
                                }
                                else {
                                    ((TrackTileObj *)pPicked)->EnqueueClaimWaiterMaybe(this);
                                }
                                break;
                            }
                            }
                            break;
                        }
                    }
                    else if (((this->dwModeBMaybe == 1) &&
                              (((CarNetObjAnchorPartial *)this->carSlots[0])->dwModeBMaybe == 2)) &&
                             (((CarNetObjAnchorPartial *)this->carSlots[this->wCarSlotCount])->dwModeBMaybe == 2)) {
                        TrackConnectorTileObj *pSlot;
                        this->dwModeBMaybe = 2;
                        this->SetCarsReady(0);
                        this->pNameMaybe->dwTileExtendPhaseMaybe = 2;
                        pTile = this->pNameMaybe->pTileMaybe;
                        pSlot = g_worldBoard.GetPlaneASlotMaybe(pTile->pos.wPosX, pTile->pos.wPosY + 1, 0);
                        if (pSlot == 0) {
                            return;
                        }
                        pSlot->pOwningTrain = this;
                        pSlot->ReleaseChannelAndDispatch(1);
                        pSlot->dwOccupancyClaim = 1;
                        *(unsigned int *)&this->wClaimedPosXMaybe = *(unsigned int *)&pSlot->pos.wPosX;
                        return;
                    }
                    if ((this->dwModeBMaybe == 5) || (this->dwModeBMaybe == 4)) {
                        this->CompleteDepotExitMaybe();
                    }
                    nIter = nIter + 1;
                } while (nIter < this->wSelectedCarId);
            }
            if (((this->nDeferredMoveStateMaybe == 0) && (this->dwModeAMaybe != 2)) &&
                (this->dwModeAMaybe != 3)) {
                unsigned char i = 0;
                int u = 0;
                if (this->wCarSlotCount + 1 > 0) {
                    do {
                        CarNetObjAnchorPartial *pCar = (CarNetObjAnchorPartial *)this->carSlots[u];
                        if (pCar->pDSoundChannel != 0) {
                            pCar->pDSoundChannel->SetPosition(pCar->rect.left, pCar->rect.top);
                        }
                        i = i + 1;
                        u = i;
                    } while (u < this->wCarSlotCount + 1);
                }
            }
        }
    }
    if (((this->dwModeAMaybe == 2) ||
         ((this->dwModeAMaybe == 3) && (this->nDeferredMoveStateMaybe != 1))) &&
        (g_worldBoard.GetPlaneASlotMaybe(this->wSentinelCMaybe, this->wSentinelDMaybe + 1, 0) == 0)) {
        void *pPicked = g_NetSessionEventQueuePick.PickRandomBigObjByCategory(4);
        if (pPicked == 0) {
            this->SetSoundStateMaybe(3);
            return;
        }
        ((TrackTileObj *)pPicked)->EnqueueClaimWaiterMaybe(this);
    }
    if (this->dwSoundStateMaybe == 4) {
        if (this->dwUnk0x28 == 0) {
            this->SetSoundStateMaybe(0);
            this->dwUnk0x28 = 0;
            this->PeerTrainNode_UpdateSelectedCar(this->wSelectedCarIdAMaybe);
            if (((this->dwReversed != (int)(this->dwReversed == 0)) && (this->bUnk0x5a == 0)) &&
                ((this->CanReverseDirectionMaybe() != 0) && (this->bUnk0x5a == 0))) {
                this->bUnk0x5a = 1;
                this->ReverseDirectionMaybe();
                this->bUnk0x5a = 0;
            }
            this->SetSoundStateMaybe(2);
        }
        else {
            this->dwUnk0x28 = this->dwUnk0x28 - 1;
        }
    }
    if ((nMoved != 0) &&
        ((this->dwModeBMaybe == 0) || ((3 < this->dwModeBMaybe) && (this->dwModeBMaybe < 6)))) {
        int nAnchorY = this->pNameMaybe->nAnchorYMaybe;
        short wGridY;
        if (nAnchorY < 0) {
            wGridY = -1;
        }
        else {
            wGridY = (short)(nAnchorY >> 4);
        }
        int nAnchorX = this->pNameMaybe->nAnchorXMaybe;
        short wGridX;
        if (nAnchorX < 0) {
            wGridX = -1;
        }
        else {
            wGridX = (short)(nAnchorX >> 4);
        }
        g_pDPlaySessionMgr->SetTrainPlacementResult(this->wTrainId, this->bOwnerByteA,
                                                    this->bOwnerByteB, wGridX, wGridY);
    }
    pTile = this->pNameMaybe->pTileMaybe;
    if ((((pTile != 0) && (this->dwModeBMaybe != 2)) && (this->dwModeAMaybe != 2)) &&
        (this->dwModeAMaybe != 3)) {
        switch (pTile->pKindDesc->resourceId) {
        case 0xc5c:
        case 0xc5e:
        case 0xc60:
            g_pDPlaySessionMgr->AcceptIncomingTrainNodeMaybe(this);
            break;
        }
    }
}

// FUNCTION: LOCO 0x44c370
// True if any of this train's TOWED cars (slots 1..3 -- slot 0, the lead unit, is deliberately
// skipped) is present and has nCarCategory == 2. Both callers use it the same way, as a gate
// before arming a coupling wait: NameAnchorMaybe::TryBeginCouplingWaitMaybe
// (src/NameAnchorMaybe.cpp) and WalkerActor's boarding check (src/WalkerActor.cpp), which
// together make category 2 the passenger-carrying car kind -- suggestive, still not proof, so
// the name keeps its Maybe.
//
// ⚠ src/WalkerActor.cpp declares this same address on its OWN TU-local
// `PeerTrainNodeWalkerPartial` view, so that TU's call site emits a different mangled name than
// this definition. Byte-invisible (we never link) and invisible to tools/lint_alias.py too,
// since the METHOD name is identical and only the class differs -- it goes away when this
// singleton's three partial views are finally unified.
unsigned char PeerTrainNodeTrackPartial::HasCarCategory2Maybe() // TODO: sync (Ghidra: PeerTrainNode::HasCarCategory2Maybe -- the Track view is TU-local, see above)
{
    // A result FLAG plus `break`, not an early `return 1` -- the difference is visible: the flag
    // pins the return value in AL across the whole loop, which is why the original zeroes it
    // up-front (`xor al,al` before the loop) and needs EDI for the constant 2 rather than
    // spending EAX on it. The `return 1` spelling compiles three instructions shorter.
    unsigned char bResult = 0;
    for (unsigned int i = 1; i < 4; i++) {
        if (carSlots[i] != 0 && ((CarNetObj *)carSlots[i])->nCarCategory == 2) {
            bResult = 1;
            break;
        }
    }
    return bResult;
}
