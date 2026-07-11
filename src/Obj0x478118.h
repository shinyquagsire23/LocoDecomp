// Obj0x478118 -- a small CursorDesc variant (vtable 0x478118, ctor 0x436400, LoadMaybe
// 0x436470). 0x178 bytes = CursorDesc's 0x168 plus 0x10 bytes of its own state; its ctor
// runs CursorDesc's own ctor in place and then stamps vtable 0x478118 over it, so it is a
// plain single-inheritance CursorDesc subclass (a SIBLING of Obj0x4779e0/CarKindDesc, not
// derived from either).
//
// Kept in its OWN header rather than in src/CursorDesc.h alongside its CursorDesc base and
// its Obj0x4779e0/BigObj siblings: adding a class there rotates src/Obj0x4779e0.cpp's and
// src/DPlaySessionMgr.cpp's /Og TU state and costs each of them an EXACT match (measured
// v354 for CarKindDesc; same hazard and same fix as src/TimeOfDayMaybe.h's v331 note).
//
// The descriptor factory (UIResources::TileKind_CreateDescriptor, 0x446840) picks this class
// for every EVEN kind id in categories 7 and 8 (ids 0x1c00-0x23ff); the ODD ids in those
// categories get a plain 0x168-byte CursorDesc instead.
#pragma once

#include "CursorDesc.h"   // CursorDesc -- the direct base

// This tier is the MINIFIG/PERSON kind descriptor: every field of its 0x10-byte tail is named
// by an ini keyword in its own ParseTokenField override (0x436750), read out of the image's
// string bytes rather than Ghidra's mangling labels -- `walk_speed`, `groundwidth`,
// `SpawnLimit`, `Employable`, `sex`, `PickUpSoundId`. Defaults come from the ctor's own
// initializer run (0x4364ad).
class Obj0x478118 : public CursorDesc {
public:
    // 0x436400 -- src/Obj0x478118.cpp. Declared HERE, on the real class, rather than on the
    // TU-local ctor view it used to live on: UIResources::TileKind_CreateDescriptor `new`s this
    // tier for every even kind id in categories 7 and 8, and against a TU-local view every one
    // of those calls resolved to a generated do-nothing stub instead of to the body sitting in
    // this class's own TU. Same defect class -- and same fix -- as CursorDesc's own ctor
    // (CODEGEN #161); the trace in link/stub_calls.log counted 50 such calls per run.
    Obj0x478118(unsigned int kindId, char *pszDefinition);
    // 0x436480 / 0x436460 (??_GObj0x478118) -- src/Obj0x478118.cpp. Defined OUT OF LINE: the
    // original keeps ??1 and ??_G as two separate COMDATs (0x436460 calls 0x436480), which is
    // the tell that the body was NOT written inside the class (v451's in-class-dtor lever).
    virtual ~Obj0x478118();
    // vtable slot 1 (+0x4) override -- 0x436960, src/Obj0x478118.cpp. 0x47811c holds this address
    // where CursorDesc's own 0x477c1c holds the base's 0x425670, so the override is real. It adds
    // exactly one thing to the base realizer: force this kind's PickUpSoundId resident before the
    // bitmap loads, so the pick-up sound never has to wait on a .wav read the first time the
    // player grabs a minifig. The base's per-frame-set sound preload does the same job for the
    // ANIMATION sounds; this one is for the tail field only that class has.
    virtual LocoBitmap *GetOrLoadFrameBitmap(int nWidth, int nHeight);
    // vtable slot 2 (+0x8) override -- 0x4369a0, src/Obj0x478118.cpp. The exact mirror of the
    // slot-1 override above: where that one forces this kind's PickUpSoundId RESIDENT before
    // realizing the bitmap, this one hands that same reference BACK before chaining to the
    // base's own release. So the two overrides are the acquire/release pair for the one sound
    // id this tier adds to CursorDesc.
    virtual void ReleaseRef();
    // vtable slot 3 (+0xc) override -- 0x436750, src/Obj0x478118.cpp. LoadMaybe below dispatches
    // through this virtually (`call [vtable+0xc]`), so it must be a real override, not a
    // same-named shadow.
    virtual unsigned char ParseTokenField(istream *pStream);
    // 0x436490 -- really this tier's vtable slot 4 (+0x10) `Load(kindId, pszDefinition)`
    // override, but declared NON-virtual here: CursorDesc::Load is deliberately mis-declared
    // no-arg in src/CursorDesc.h (widening it there rotates src/Obj0x4779e0.cpp for -489 B --
    // see that header's own note), so the real 2-arg signature cannot be spelled as an
    // override. Nothing dispatches through slot 4 in src/, and virtual-ness changes neither
    // this body's codegen nor the ctor's direct call to it, so the non-virtual spelling costs
    // nothing and avoids inventing a bogus extra slot. `kindId` is genuinely unused by this
    // override -- only pszDefinition is read.
    void LoadMaybe(unsigned int kindId, char *pszDefinition);
    // "this kind never declared a cursor hotspot" -- LoadMaybe's own tail seeds (0, 8) when it
    // holds. Its `unsigned char` return is LOAD-BEARING and is what takes 0x436490 from DIFF(51)
    // to EXACT: the byte return type forces the short-circuit `&&` through the branchy
    // `mov eax,1 / jmp / xor eax,eax` materialization the original has, which the same condition
    // spelled inline at the call site folds away into two direct `jne`s. This is v356's
    // byte-returning-inline-predicate lever, in its multi-condition form (no `setcc` at all --
    // see docs/CODEGEN.md). ⚠ It reads BASE fields, so its real home is CursorDesc; it lives here
    // because adding ANY member to src/CursorDesc.h rotates that header's 27 consumers (the
    // measured -489 B hazard documented on CursorDesc::Load). Move it up if that class ever cracks.
    unsigned char IsHotspotUnsetMaybe() const { return hotspotX == 0 && hotspotY == 0; }

    // +0x168/+0x169 -- the two values of the `walk_speed` keyword (each parsed as a ushort,
    // stored as its low byte). Used as a pair by the walk-step loop at 0x433000: a step
    // counter runs while `counter < +0x169`, and +0x168 is passed on as the per-step
    // distance -- hence speed-vs-step-count rather than an (x,y) pair.
    unsigned char bWalkSpeedMaybe;      // +0x168
    unsigned char bWalkStepCountMaybe;  // +0x169
    unsigned char bGroundWidth;         // +0x16a -- `groundwidth`; ctor defaults it to 8
    unsigned char bSpawnLimit;          // +0x16b -- `SpawnLimit`; ctor defaults it to 0xff
    unsigned char bEmployable;          // +0x16c -- `Employable`
    unsigned char pad0x16d[3];          // +0x16d .. +0x16f -- never written by ctor or parser
    // +0x170 -- `sex`, stored as the UPPERCASED first character of the token's value as a
    // full dword: 'M' (0x4d, also the ctor's default) or, for anything else, 'F' (0x46).
    unsigned long dwSex;
    // +0x174 -- `PickUpSoundId`; ctor defaults it to 0, and every reader treats 0 as "no
    // sound" (PlacementCursorMaybe::SetHoverObjMaybe plays it only when non-zero).
    long nPickUpSoundId;
};
