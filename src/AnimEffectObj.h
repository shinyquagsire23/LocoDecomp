// AnimEffectObj0x477a90 (0xa4 bytes, vtable 0x477a90 stamped by
// EffectPlacedRegistryMaybe::SetCopyAtMaybe (0x424550) as the 3rd/outermost layer after
// RectFlagObj0x477820 -> AnimDescRefObj0x477488) -- a background sprite that positions itself at a (usually
// randomized) viewport-edge-relative position on construction, driven by a placement-mode
// char code ('C'/'D'/'P'/'R'/'S'/'U'/'W'), and optionally spawns a companion effect
// (smoke/dust?) through the EffectSpawner singleton when its kind descriptor carries a
// positive nShadowId. Field names mirror Ghidra's AnimEffectObj0x477a90 struct (mostly
// '...Maybe' placeholders there too).
#pragma once

#include "WidgetBase.h"

// Local view declaring the real slot-3 override (0x405c00, extern -- not yet transcribed):
// hotspot-aware reposition the ctor calls class-qualified (direct call, no vtable dispatch
// -- matches the original ctor's own `mov ecx,esi; call 0x405c00` shape). Kept OUT of
// src/WidgetBase.h: adding method decls to that shared header rotates DPlaySessionMgr.cpp's
// TU codegen and breaks SelectGridCellFromPointMaybe's EXACT match (the v325 lesson,
// re-confirmed v329).
struct AnimDescRefHotspotPartial : AnimDescRefObj0x477488 {
    // Forwarding ctor (inlined away -- the real base-ctor call is unchanged).
    AnimDescRefHotspotPartial(int nResourceId, short nSubFrameArg, int nPosX, int nPosY)
        : AnimDescRefObj0x477488(nResourceId, nSubFrameArg, nPosX, nPosY) {}
    // NO `RepositionWithHotspot` re-declaration here. It used to carry one ("0x405c00, extern"),
    // written when src/WidgetBase.h did not yet declare the virtual; that header has declared it
    // at slot 3 since, so the local copy only HID the real member. Every
    // `AnimDescRefHotspotPartial::RepositionWithHotspot(...)` call below then mangled under THIS
    // class's name -- a symbol nothing defines -- and in the port became a do-nothing stub, i.e.
    // effects were spawned and never positioned. Same defect family as the view-struct calls
    // retired in v564/v566 (CODEGEN #184); the qualified call sites still compile unchanged
    // because the name now resolves to the inherited base member, and a qualified call is
    // non-virtual dispatch either way.
};

class AnimEffectObj0x477a90 : public AnimDescRefHotspotPartial {
public:
    char chPlacementModeMaybe;  // +0x88 -- toupper()ed placement-mode char (ctor param 3)
    // +0x89 -- REAL alignment padding, NOT a member: EffectPlacedRegistryMaybe's slot-9 copy
    // (0x424550, the inlined implicit copy constructor) skips it, and an explicit pad member
    // would be copied (the DecorActor.h pad lesson, opposite polarity). Same at +0x95..+0x97.
    short wUnk0x8aMaybe;        // +0x8a -- copy of the ctor's nSubFrameArg, passed on to
                                //   EffectSpawner_SpawnSimpleMaybe
    int nTargetXMaybe;          // +0x8c -- ctor param 4; negative selects the randomized
                                //   placement branch in modes 'D'/'P'/'S'/'U'
    int nTargetYMaybe;          // +0x90 -- ctor param 5
    // +0x94 -- UNSIGNED, pinned 2026-07-27 from TickMaybe (0x423560), which uses it as a
    // per-tick step size at four sites and ZERO-extends it every time
    // (`xor eax,eax; mov al,[esi+0x94]`); a signed char would have given `movsx`.
    unsigned char bUnk0x94Maybe; // per-instance variant byte: descriptor's
                                //   bFootprintXSteps when categoryByte == 8, else
                                //   rand() % 3 + 1 (forced to 1 by modes 'D'/'U')
    // (+0x95..+0x97 -- real alignment padding, see the +0x89 note above)
    // +0x98 -- spawned companion effect (EffectSpawner result). Typed as this same class
    // rather than left `void *` so TickMaybe can drag it along by NAME; the dispatch is
    // the inherited slot-3 virtual either way, which is what the original emits.
    AnimEffectObj0x477a90 *pEffectMaybe;
    int nEffectOffsetXMaybe;    // +0x9c -- spawn offset from own rect.left
    int nEffectOffsetYMaybe;    // +0xa0 -- spawn offset from own rect.top (descriptor's
                                //   nShadowOffsetY when positive, else rand() % 0x1f + 0x28)

    AnimEffectObj0x477a90(int nResourceId, short nSubFrameArg, char chPlacementMode,
                          int nTargetX, int nTargetY);
    // Declared + DEFINED (empty body, src/AnimEffectObj.cpp) rather than declared-only: a
    // declared-but-undefined dtor here makes cl reference an undefined weak-external ??_E
    // vector-deleting-dtor from the vtable COMDAT, which GNU objdump can't parse (storage
    // class 105) -- breaking the disasm-diff loop for this TU. The real dtor's own
    // transcription is a separate future item either way.
    virtual ~AnimEffectObj0x477a90();

    // 0x423560 -- the per-frame step. EffectSpawner's own tick (0x423d70) runs this over both
    // of its effect collections and DELETES every effect that returns 1, so the return value is
    // "I am finished". Not a vtable slot (0x477a90's table stops at slot 14); the tick calls it
    // directly on each element it pulls out of the collection.
    char TickMaybe();

    // Slots 7/9/10 -- the "drag the companion along" trio. All three have ONE shape: forward
    // the IDENTICAL slot to pEffectMaybe (a virtual dispatch, since the companion may be a
    // further-derived effect), then chain this class's own base implementation directly.
    // Confirmed against vtable 0x477a90, whose slots 7/9/10 hold 0x423840/0x423890/0x423870
    // where the base table 0x477488 holds 0x405a20/0x4061b0/0x405c40.
    virtual void ReleaseChannelAndDispatch(unsigned int arg); // slot 7  (+0x1c) -- 0x423840
    virtual void SetReadyStateMaybe(bool bIsReady);           // slot 9  (+0x24) -- 0x423890
    virtual void AdvanceAnimFrameMaybe();                     // slot 10 (+0x28) -- 0x423870
};
