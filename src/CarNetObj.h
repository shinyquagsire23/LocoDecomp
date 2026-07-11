#pragma once

#include "WidgetBase.h"  // AnimDescRefObj0x477488 -- CarNetObj's own base (embedded at offset 0)
#include "CarNetState.h" // CarNetState -- embedded by value at +0x88

struct NameAnchorMaybe;      // fwd -- src/NameAnchorMaybe.h (pointer-only use below)
struct PeerTrainNodePartial; // fwd -- src/PeerTrainNode.h (which includes THIS header, so the
                             //   back-pointer at +0x44c can only be a forward declaration)

// CarNetObj -- the per-car network/identity state object hanging off a PeerTrainNode's car slots.
// The class is now modeled in FULL: 0x450 bytes, which is exactly what PeerTrainNode_AllocCarSlot's
// own `operator new(0x450)` demands as a sizeof oracle. Car pointers are reached by reinterpreting
// PeerTrainNode::carSlots (per the established car-pointer reinterpretation precedent). Consumers:
// src/DPlaySessionMgr.cpp (connect/disconnect re-tag loop), src/GameNet.cpp
// (RemovePeerTrainsForPlayer apply-net-state, GameNet_SendTrainStateSync), src/PeerTrainNode.cpp
// and src/NameAnchorMaybe.cpp (the per-car anchor/mode tick).

// Padded-vtable probe, now only for reaching a CarNetObj car's slot 0xd (SetNameImpl)
// through a NON-`this` car pointer (DPlaySessionMgr.cpp's/GameNet.cpp's
// `((CarNetObjVtblProbe *)pCar)->SetNameImpl(...)` sites -- pCar casts, outside
// lint_idiom's class F). The slots this used to serve through `this` casts are now real
// virtuals: slot 8 is the inherited AnimDescRefObj0x477488::SetAnimFrame (WidgetBase.h) and
// slot 15 is CarNetObj::RetagKind below. Per src/NetSessionEventQueue.cpp's precedent
// the compiler only needs a >=16-slot vtable to select the `call [ecx+off]` shape -- the
// probe's own identity is irrelevant (resolved by masked reloc).
struct CarNetObjVtblProbe {
    virtual void _v00();
    virtual void *_v01(); virtual void *_v02(); virtual void *_v03();
    virtual void *_v04(); virtual void *_v05(); virtual void *_v06(); virtual void *_v07();
    virtual void SetStateArgImpl(int arg, int flag);            // vtbl+0x20 (slot 8)
    virtual void *_v09(); virtual void *_v10(); virtual void *_v11();
    virtual void *_v12();
    virtual void SetNameImpl(const char *pszName);              // vtbl+0x34 (slot 0xd)
    virtual void *_v14();
    virtual void RetagKindImpl(unsigned short wKind, int flag); // vtbl+0x3c (slot 15)
};

// CarNetObj embeds AnimDescRefObj0x477488 as its base at offset 0 (confirmed via CarNetObj's own
// ctor directly calling AnimDescRefObj0x477488::AnimDescRefObj0x477488, the base-class-embedding
// tell -- CLAUDE.md). This retires the old flat `pad0x0[0x54]+Unk0x54Maybe` view: that field was
// really AnimDescRefObj0x477488::nAnimValueCache (+0x54), confirmed by offset. Not a duplicate of
// TilePlacedObjPartial: a distinct class focused on CarNetObj's own state surface.
struct CarNetObj : public AnimDescRefObj0x477488 {
    // +0x88 -- the car's own identity/net-state card, embedded BY VALUE (not a pointer):
    // CarNetObj_GetAppliedState (0x40d750) returns `&pCar->stateMaybe` and
    // CarNetObj_ApplyNetState below assigns straight into it. 0x88 + 0x39c = 0x424, which is
    // exactly where the applied-state latch lands, so the two offsets confirm each other.
    // Replaced the old flat `pad0x88[0x42c - 0x88]` in v474; every offset is unchanged.
    CarNetState stateMaybe;      // +0x88
    bool bStateAppliedMaybe;     // +0x424 -- set when stateMaybe holds a valid applied card;
                                 //   gates CarNetObj_GetAppliedState's non-NULL return
    int nCarTypeIdMaybe;         // +0x428 -- the car's kind id (CarNetObj_GetCarTypeId's field)
    int nCarCategory;            // +0x42c -- small enum 0-4, derived from nCarTypeIdMaybe
                                 //   (docs/subsystems.md: CarNetObj::nCarCategory)
    // The 0x430 tail, promoted here in v475 from the two TU-local views that used to carry it
    // (src/PeerTrainNode.cpp's CarNetObjAnchorPartial and src/NameAnchorMaybe.cpp's
    // CarNetObjModePartial, which existed ONLY because this class used to stop at +0x430).
    // Every offset is Ghidra's own CarNetObj, natural alignment throughout.
    NameAnchorMaybe *pNameAMaybe; // +0x430 -- the car's A-side track anchor
    NameAnchorMaybe *pNameBMaybe; // +0x434 -- the car's B-side track anchor
    unsigned short wHeadingMaybe; // +0x438 -- per-car heading word (0/0x20/0x40/0x60)
    unsigned short wUnk0x43a;     // +0x43a
    unsigned char bUnk0x43c;      // +0x43c (+0x43d..+0x43f is alignment padding)
    int dwModeAMaybe;             // +0x440 -- per-car layout mode (4 = viewport-extend)
    int dwModeBMaybe;             // +0x444 -- per-car second mode (2 = extend-complete)
    unsigned short wUnk0x448;     // +0x448 -- set 0/1 by UpdateCarPlacementTickMaybe's own
                                  //   claimed-tile-settling branch and by its sibling
                                  //   SettleClaimedSocketMaybe; not yet observed read anywhere.
                                  //   (+0x44a..+0x44b is alignment padding)
    PeerTrainNodePartial *pOwnerTrainNodeMaybe; // +0x44c -- back-pointer to the train that owns
                                  //   this car slot; stamped by PeerTrainNode_AllocCarSlot right
                                  //   after a successful construction. The class's LAST field:
                                  //   0x44c + 4 = 0x450, matching AllocCarSlot's own
                                  //   `operator new(0x450)` sizeof oracle.

    // 0x40d500 -- construct a car slot. Body in src/CarNetObj.cpp. nKindId doubles as the base's
    // resource id and this car's own type id; the whole +0x430 tail is populated only when the
    // base's bValid comes back true.
    CarNetObj(int nKindId, int nCategory, char bFlag);
    // 0x40d680 (the `??_G` scalar-deleting thunk at 0x40d660 is compiler-generated around it).
    // Releases both anchors, drops the owner back-pointer -- repainting the car's last screen
    // rect on the way out unless the owning train is already being torn down -- then lets the
    // compiler run ~CarNetState on the embedded card and the base dtor. Body in
    // src/CarNetObj.cpp.
    virtual ~CarNetObj();

    // vtable slot 15 (+0x3c) -- CarNetObj's own kind re-tag virtual, and this class's ONLY new
    // slot (the base chain models 0-14). Body in src/CarNetObj.cpp.
    //
    // ⚠ Modeled until v477 as a declared-only `RetagKind(unsigned short, int)` PLUS an inline
    // `SetCarTypeAndCategory` wrapper that dispatched to it -- two names for one function. The
    // vtable dword at 0x477590+0x3c reads 0x40e0f0, which IS Ghidra's own
    // CarNetObj::SetCarTypeAndCategory, so the slot and the named method were always the same
    // thing. Byte-neutral at the five src/DPlaySessionMgr.cpp call sites: the inline wrapper
    // compiled to the same `call [reg+0x3c]` this direct virtual does.
    virtual unsigned char SetCarTypeAndCategory(int nCarTypeId, int nSubFrameArg);

    // 0x40d8e0, defined in src/PeerTrainNode.cpp (its address-order home, interleaved with the
    // per-car tick family) -- re-center this car's rect off its current wHeadingMaybe, using the
    // kind descriptor's per-heading {dx,dy} table. Declared HERE, not on that TU's
    // CarNetObjAnchorPartial view, because ResolveBothAnchorsToPointMaybe below calls it and the
    // two TUs have to agree on one mangled name (the exact defect class tools/lint_alias.py
    // exists to catch).
    void RepositionForHeadingMaybe();

    // 0x40d890 -- seat BOTH of this car's anchors off one pixel point: the A anchor onto the
    // world-board tile under (nX, nY), the B anchor 0x16 pixels to A's right on A's resolved
    // row, then re-center the sprite via RepositionForHeadingMaybe above. Answers 0 without
    // touching anything when this car has no anchors -- i.e. on a car whose ctor rejected its
    // kind id. Body in src/CarNetObj.cpp. Called by PeerTrainNode's own ctor.
    unsigned char ResolveBothAnchorsToPointMaybe(int nX, int nY);

    // 0x40e250 -- "is this car showing at tile (nX, nY) on plane nPlaneMaybe?": a plane match
    // against wUnk0x448 followed by a rect overlap test of this car against the 16x16 tile box
    // whose top-left is (nX, nY). Both explicit coordinate pairs are read as WORDs (`movsx`),
    // so the three parameters the body reads are 16-bit. nUnused is a genuine dead stack
    // parameter -- the function's own `ret 0x10` accounts for it and nothing in the body reads
    // it. Same "dead-but-real parameter" tell as ComputeHeadingAngleMaybe's nUnused.
    //
    // nUnused is a BYTE, pinned 2026-07-28 by the caller rather than the callee (the callee
    // cannot pin a parameter it never reads): PeerTrainSlotQueueMaybe::DrawVisibleCarsInTileMaybe
    // passes the owning train's `bUnk0x2c` at both of its call sites as a bare
    // `mov dl, byte ptr [eax+0x2c]` with the register's upper bytes left as garbage. Declared
    // `short` it was a `movzx` at each site instead -- the only two instruction mismatches left
    // in that function.
    unsigned char HitTestTileMaybe(short nX, short nY, short nPlaneMaybe, unsigned char nUnused);

    // 0x40e160 -- draw this car's sprite clipped to one 16x16 tile box. rcTile arrives BY VALUE
    // (the caller builds all four dwords on the stack; `ret 0x10` confirms the width), and only
    // its left/top are read directly -- the right/bottom edges are re-derived as left+0x10 /
    // top+0x10, which is what pins the box at one tile. Returns 0 when the tile doesn't overlap
    // this car at all. Body in src/CarNetObj.cpp.
    unsigned char BlitTileSliceMaybe(RECT rcTile);

    // vtable slot 8 (+0x20) dispatch -- resolves to the inherited
    // AnimDescRefObj0x477488::SetAnimFrame default body (0x405de0) when a car doesn't
    // override it (WidgetBase.h).
    void SetStateArgMaybe(int arg, int flag) {
        this->SetAnimFrame(arg, flag);
    }
    // Latch a card onto this car (pState != NULL) or clear the latch (pState == NULL).
    // Transcribed in src/CarNetObj.cpp. Returns AL only -- `unsigned char`, not `int`: the raw
    // disasm's three exits are `xor al,al` / `mov al,1` / `mov al,1` with nothing written to the
    // rest of eax, and all 8 call sites discard the value. The parameter was declared `int
    // flagOrState` until v474, which forced an `(int)pState` cast at every call site.
    unsigned char CarNetObj_ApplyNetState(CarNetState *pState);  // 0x40d770
};
