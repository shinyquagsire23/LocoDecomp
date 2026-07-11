// CarNetObj -- the per-car network/identity object hanging off a PeerTrainNode's car slots.
// Its class surface lives in src/CarNetObj.h, complete at 0x450 bytes since v475: the 0x88-byte
// AnimDescRefObj0x477488 base, the embedded CarNetState card at +0x88, the id/category pair after
// it, and the anchor/mode tail at +0x430..+0x44c that two TU-local views used to model separately.
// This TU holds the ctor, the two plain accessors and the state-latch method.
// See docs/subsystems.md's Train/car network section.

// ⚠ windows.h/ddraw.h first: src/PostBag.h (reached via CarNetState.h) names
// IDirectDrawSurface without declaring it, so every consumer has to supply it. Pre-existing
// debt shared with the other CarNetState.h consumers, not new here.
#include <windows.h>
#include <ddraw.h>
#include "CarNetObj.h"
#include "NameAnchorMaybe.h" // NameAnchorMaybe -- the ctor allocates the car's two anchors
#include "CarKindDesc.h"     // MapCarTypeIdToCategoryMaybe (+ CursorDesc/BigObj -- pKindDesc)
#include "LocoBitmap.h"      // LocoBitmap::RestoreOverlapBlt
#include "Ddraw.h"           // g_pDDrawWorkSurface -- BlitTileSliceMaybe's target surface
#include "PeerTrainNode.h"   // PeerTrainNodePartial::bHasDetailFlagMaybe -- the dtor's guard
#include "WorldBoardMaybe.h" // g_worldBoard.MarkRectDirty -- the dtor's repaint

// FUNCTION: LOCO 0x40d500
// Construct a car slot. nKindId is both the base's resource id and this car's own type id;
// nCategory is the small 0-4 enum; bFlag is the peer-owned/direction flag, which picks which of
// the two mode dwords starts at 2 and also seeds both anchors.
//
// The whole tail below +0x430 is populated ONLY when the base's bValid came back true -- i.e.
// when nKindId resolved to a real kind descriptor. On a bad id the object is left as a bare
// base + zeroed pointers, which is exactly what PeerTrainNode's own ctor tests for right after
// `new CarNetObj(...)` (src/PeerTrainNode.cpp, the `bValid == true` guard on carSlots[0]).
//
// ⚠ The four stores before the guard are deliberately outside it: the two anchor pointers, the
// owner back-pointer and nCarTypeIdMaybe are cleared/seeded unconditionally, so a rejected car
// still has NULL anchors rather than garbage.
CarNetObj::CarNetObj(int nKindId, int nCategory, char bFlag)
    : AnimDescRefObj0x477488(nKindId, -1, 0, 0)
{
    this->pNameAMaybe = 0;
    this->pNameBMaybe = 0;
    this->pOwnerTrainNodeMaybe = 0;
    this->nCarTypeIdMaybe = nKindId;
    if (this->bValid == true) {
        this->nCarCategory = nCategory;
        this->pNameAMaybe = new NameAnchorMaybe(bFlag);
        this->pNameBMaybe = new NameAnchorMaybe(bFlag);
        this->wHeadingMaybe = 0;
        this->wUnk0x43a = 0;
        this->bUnk0x43c = 0;
        if (bFlag == 0) {
            this->dwModeBMaybe = 2;
            this->dwModeAMaybe = 0;
        } else {
            this->dwModeBMaybe = 0;
            this->dwModeAMaybe = 2;
        }
        this->wUnk0x448 = 0;
        SetRect(&this->rect, 0, 0, 0, 0);
        this->SetAnimFrame(this->wHeadingMaybe, 1);
        this->bStateAppliedMaybe = false;
        this->bReady = false;
    }
}

// FUNCTION: LOCO 0x40d660 (??_GCarNetObj scalar deleting dtor -- compiler-generated around
// ~CarNetObj below; no source of its own)

// FUNCTION: LOCO 0x40d680
// Tear a car slot down. Only three things are this function's own -- both anchors and the owner
// back-pointer; the embedded CarNetState card and the AnimDescRefObj0x477488 base are destroyed
// by the compiler's own epilogue (which is also what pulls in the /GX unwind frame: two
// subobjects with dtors means two EH states to unwind through).
//
// ⚠ The repaint is guarded on the OWNING TRAIN, not on this car: g_worldBoard.MarkRectDirty is
// skipped when the train's bHasDetailFlagMaybe is already set, because a train being torn down
// wholesale will repaint its whole footprint once rather than once per car. Clearing the
// back-pointer happens either way.
//
// Both anchors go through NameAnchorVtblProbe rather than `delete` -- NameAnchorMaybe carries
// its vptr as a plain field, so the probe is how the original's `(**pVtbl)(1)` gets emitted.
// See src/NameAnchorMaybe.h.
CarNetObj::~CarNetObj()
{
    if (this->pNameAMaybe != NULL) {
        ((NameAnchorVtblProbe *)this->pNameAMaybe)->ScalarDeletingDtor(1);
        this->pNameAMaybe = NULL;
    }
    if (this->pNameBMaybe != NULL) {
        ((NameAnchorVtblProbe *)this->pNameBMaybe)->ScalarDeletingDtor(1);
        this->pNameBMaybe = NULL;
    }
    if (this->pOwnerTrainNodeMaybe != NULL) {
        if (this->pOwnerTrainNodeMaybe->bHasDetailFlagMaybe == 0) {
            g_worldBoard.MarkRectDirty(this->rect);
        }
        this->pOwnerTrainNodeMaybe = NULL;
    }
}

// FUNCTION: LOCO 0x40d750
// The reader CarNetObj_ApplyNetState's latch gates: hands back the embedded card only while the
// latch is set, so callers can use a non-NULL return as "this car has a card" without reaching
// into bStateAppliedMaybe themselves.
//
// ⚠ Kept as a free __fastcall taking `void *` rather than a real CarNetObj:: method, matching its
// declaration in src/CarNetState.h and its ~20 existing call sites across src/DPlaySessionMgr.cpp
// and src/GameNet.cpp, which reach cars through PeerTrainNode::carSlots (typed
// TilePlacedObjPartial *). fastcall-with-one-pointer and thiscall are the same call shape -- the
// pointer arrives in ecx either way -- so this is byte-identical to the method form; only the
// symbol name differs. Promoting it (and 0x40e0d0 below) to real methods means a cast at every
// one of those call sites, in two of the most codegen-sensitive TUs in the repo: price it as its
// own measured change, not as a drive-by.
CarNetState *__fastcall CarNetObj_GetAppliedState(void *pCar)
{
    if (((CarNetObj *)pCar)->bStateAppliedMaybe) {
        return &((CarNetObj *)pCar)->stateMaybe;
    }
    return 0;
}

// FUNCTION: LOCO 0x40d770
// Latch a card onto this car, or clear the latch. Three-way, and deliberately asymmetric:
//   * not yet latched + a card given -> copy it in, latch, report 1 (took it)
//   * already latched + a card given -> report 0 (refused; the existing card is kept)
//   * anything else (no card given, either way) -> clear the latch and report 1
// So passing NULL always succeeds and always un-latches, while passing a card only succeeds
// once. CarNetObj_GetAppliedState (0x40d750) is the reader that the latch gates.
//
// The whole card copy is one statement: `stateMaybe = *pState` picks up the
// compiler-generated CarNetState::operator=, whose inline expansion IS the bulk of this
// function's 275 bytes. That expansion is also the oracle that fixed CarNetState's layout in
// v474 -- a memberwise copy-assignment skips both the vptr and every alignment gap, so the
// bytes it declines to move are a direct readout of which members are real. See
// src/CarNetState.h's CardNameB note.
//
// ⚠ EFFECTIVE MATCH -- 276 B against 275, DIFF(240), insns 92/91, and the single extra
// instruction is a spurious `xor ecx,ecx` in the prologue. cl materialises a zero REGISTER and
// then spends it on all three guard compares plus the latch clear (`cmp dl,cl` / `cmp edx,ecx`
// x2 / `mov byte ptr [eax+0x424], cl`) where the original uses `test cl,cl` / `test edx,edx`
// x2 / `mov byte ptr [eax+0x424], 0`. Block-for-block and instruction-for-instruction identical
// otherwise, including the copy expansion. That is the zero-register RESIDENCY class
// (docs/CODEGEN.md, v375): no `if (p)` / `if (p != NULL)` respelling reaches it. cl's heuristic
// even misfires here -- the hoist costs 2 bytes and saves only 1 on the store.
//
// The block LAYOUT, on the other hand, was fully source-steerable and took three probes:
//   * `if (!flag) { copy } else if (p) { return 0 }` + shared tail -- score 222314, 287 B. The
//     outer guard's polarity is inverted, which puts the copy block on the wrong side.
//   * flipped to `if (flag) { if (p) return 0; } else if (p) { copy }` + shared tail -- 128395,
//     still 287 B. Right polarity, but cl emits the copy block BEFORE the shared tail while the
//     original has the tail first.
//   * the form below: the tail WRITTEN OUT TWICE, once per early exit, letting cl cross-jump the
//     two copies back into one -- 52130, 276 B, exact layout. This is v473's broadened #18f
//     lever (duplicate the whole thing per arm and let cl merge the tail) applied to a plain
//     early-return tail rather than a call. The lesson generalises: cl lays blocks out in SOURCE
//     order, so if the original's shared tail sits ABOVE a large block, the source cannot have
//     reached that tail by falling out of an if/else -- it reached it by repetition.
unsigned char CarNetObj::CarNetObj_ApplyNetState(CarNetState *pState)
{
    if (this->bStateAppliedMaybe) {
        if (pState != NULL) {
            return 0;
        }
        this->bStateAppliedMaybe = false;
        return 1;
    }
    if (pState == NULL) {
        this->bStateAppliedMaybe = false;
        return 1;
    }
    this->bStateAppliedMaybe = true;
    this->stateMaybe = *pState;
    return 1;
}

// FUNCTION: LOCO 0x40e0d0
// This car's own type id -- 0x1870/0x1871 are the two hand-off socket states the multiplayer
// re-tag loop in src/DPlaySessionMgr.cpp switches between. Answers -1 rather than the stored id
// when the base never resolved a kind descriptor, i.e. on exactly the cars whose ctor above
// skipped the whole +0x430 tail.
//
// ⚠ Same free-__fastcall seam as CarNetObj_GetAppliedState above, for the same reason.
int __fastcall CarNetObj_GetCarTypeId(void *pCar)
{
    if (((CarNetObj *)pCar)->pKindDesc != 0) {
        return ((CarNetObj *)pCar)->nCarTypeIdMaybe;
    }
    return -1;
}

// FUNCTION: LOCO 0x40d890
// Seat this car onto a pixel point. The A anchor takes the point itself; the B anchor is placed
// 0x16 pixels to the RIGHT of wherever A actually landed -- read back off A's own resolved
// nAnchorXMaybe/nAnchorYMaybe rather than computed from (nX, nY), so B follows A onto whatever
// tile the board snapped A to. Then the sprite is re-centered for the current heading.
//
// The pNameAMaybe null test guards BOTH anchors: a car either has both or neither (the ctor
// allocates them together). PeerTrainNode's ctor uses the 0/1 answer to decide whether the train
// it is building has a usable lead car.
unsigned char CarNetObj::ResolveBothAnchorsToPointMaybe(int nX, int nY)
{
    if (this->pNameAMaybe != NULL) {
        this->pNameAMaybe->NameAnchorMaybe_ResolveTileAnchorMaybe(nX, nY, 0);
        this->pNameBMaybe->NameAnchorMaybe_ResolveTileAnchorMaybe(
            this->pNameAMaybe->nAnchorXMaybe + 0x16, this->pNameAMaybe->nAnchorYMaybe, 0);
        this->RepositionForHeadingMaybe();
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x40e0f0
// Vtable slot 15. Re-tag this car to a different kind: stamp the id, reload the base's
// descriptor for it, and -- only if that resolved -- refresh the derived category. On a failed
// reload nCarTypeIdMaybe is left pointing at the kind that could NOT be loaded while
// nCarCategory keeps the OLD kind's category, so the two disagree; the callers only ever pass
// the 0x1870/0x1871 hand-off pair, which always resolves. (sic -- faithful, not a fix.)
//
// ⚠ The base call is CLASS-QUALIFIED in the original, not a virtual dispatch: it compiles to a
// direct `call 0x405900` where `this->SetDescriptor(...)` would have gone through slot 6. Same
// explicit-qualification tell as PlacementCursorMaybe::SetTypeMaybe's own base calls
// (src/WidgetBase.h). CarNetObj doesn't override slot 6, so the two forms would run the same
// code -- only the emitted bytes tell them apart.
unsigned char CarNetObj::SetCarTypeAndCategory(int nCarTypeId, int nSubFrameArg)
{
    unsigned char bLoaded;

    this->nCarTypeIdMaybe = nCarTypeId;
    bLoaded = AnimDescRefObj0x477488::SetDescriptor(nCarTypeId, nSubFrameArg, 0);
    if (bLoaded) {
        this->nCarCategory = MapCarTypeIdToCategoryMaybe(nCarTypeId);
    }
    return bLoaded;
}

// FUNCTION: LOCO 0x40e250
// Plane match first, then a 16x16 box overlap against this car's rect. The caller
// (PeerTrainSlotQueueMaybe::FUN_0044e2e0) walks its sorted car scratch list once per damaged
// tile, asking each car this question before paying for BlitTileSliceMaybe below -- first for
// the plane it is currently drawing, then again with nPlaneMaybe == 0 to pick up a same-train
// car sitting on the base plane.
//
// ⚠ The overlap test is INCLUSIVE on both axes (`<=` at all four edges), so a car whose rect
// merely touches the tile box edge-on counts as covering it and gets redrawn. Faithful.
//
// ⚠ EFFECTIVE MATCH -- 71 B against 73, DIFF(65), insns 26/27. Block-for-block identical; the
// whole residual is ONE `xor al,al` (exactly the 2-byte gap) that the original materialises
// BEFORE the plane compare so the mismatch exit can fall straight into the shared epilogue,
// plus the register-naming cascade that follows from it (the original spends EAX on the
// coordinate and zero-extends the field into EDX; cl here loads the field into EAX and reuses
// that same zero as the return). That is the zero-register RESIDENCY class of docs/CODEGEN.md,
// the same one CarNetObj_ApplyNetState above is parked on.
//
// ⛔ Two probes, both closed -- do not re-run:
//   * plane guard split into its own early `if (...) return 0;` -- BYTE-IDENTICAL to the &&
//     chain below (same 71 B, same DIFF(65)); cl folds the two forms.
//   * `unsigned char bHit = 0; if (...) bHit = 1; return bHit;` -- this DOES buy the early
//     `xor al,al` and matches the first 0x13 bytes, but keeping the accumulator live in AL
//     costs a second callee-saved register: cl adds a `push edi`/`pop edi` pair the original
//     doesn't have and lands at 69 B. Trading two bytes of prologue for two of residual.
unsigned char CarNetObj::HitTestTileMaybe(short nX, short nY, short nPlaneMaybe,
                                          unsigned char nUnused)
{
    if (nPlaneMaybe == this->wUnk0x448 &&
        this->rect.left <= nX + 0x10 && this->rect.right >= nX &&
        this->rect.top <= nY + 0x10 && this->rect.bottom >= nY) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x40e160
// Redraw the slice of this car that falls inside one 16x16 tile box. The destination is the
// screen-space intersection of the car's rect with the tile box; the source is the matching
// sub-rect of the car's current animation frame, which is rectViewport (the base's per-frame
// window into the kind bitmap) narrowed on whichever edges the tile box actually cuts.
//
// ⚠ The two VERTICAL edges are derived WITHOUT rectViewport.top, unlike the horizontal pair,
// which does add rectViewport.left. That asymmetry is the original's -- see
// docs/engine-bugs.md. It is invisible in practice because a car's frame window always starts
// at row 0 of its bitmap strip (the base's SetAnimFrame only ever recomputes the HORIZONTAL
// bounds of rectViewport, leaving top at whatever the descriptor loaded), so rectViewport.top
// is 0 at every reachable call. // sic:
unsigned char CarNetObj::BlitTileSliceMaybe(RECT rcTile)
{
    RECT rcVisible;
    RECT rcSrc;

    if (IntersectRect(&rcVisible, &this->rect, &rcTile)) {
        BigObj *pKind = this->pKindDesc;

        if (this->rect.left >= rcTile.left) {
            rcSrc.left = this->rectViewport.left;
        } else {
            rcSrc.left = (rcTile.left - this->rect.left) + this->rectViewport.left;
        }
        if (this->rect.top >= rcTile.top) {
            rcSrc.top = this->rectViewport.top;
        } else {
            rcSrc.top = rcTile.top - this->rect.top;
        }
        if (this->rect.right <= rcTile.left + 0x10) {
            rcSrc.right = this->rectViewport.right;
        } else {
            rcSrc.right = (rcTile.left - this->rect.left) + 0x10 + this->rectViewport.left;
        }
        if (this->rect.bottom <= rcTile.top + 0x10) {
            rcSrc.bottom = this->rectViewport.bottom;
        } else {
            rcSrc.bottom = (rcTile.top - this->rect.top) + 0x10;
        }
        pKind->pOwnedObjA->RestoreOverlapBlt(rcVisible, g_pDDrawWorkSurface, rcSrc,
                                             this->nBlitFlags);
        return 1;
    }
    return 0;
}
