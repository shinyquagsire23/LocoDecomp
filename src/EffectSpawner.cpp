// EffectSpawner's EffectSpawner_SpawnAtPositionMaybe (0x423ab0) -- the positioned/directional
// sibling of SpawnSimpleMaybe (see src/EffectSpawner.h for the singleton writeup).

#include "EffectSpawner.h"

#include "AnimEffectObj.h" // the spawned object's class (ctor 0x422ec0)
#include "AppWindow.h"     // g_pApp->minFlyingFps, the frame-rate gate
#include "CursorDesc.h"    // the must/cant-have + live-count descriptor fields
#include "UIResources.h"   // g_UIResources.TileKind_GetOrLoadDescriptor

#ifdef LOCO_PORT
// PORT ONLY -- the family's single real slot-6 bodies (0x424510 / 0x424270), which this TU's
// own view spellings forward to at the bottom of the file. Byte-neutral for the match build.
#include "Obj0x477798Family.h"
// PORT ONLY -- PlacedObjCollectionMaybe / PlacedObjRegistryMaybe, the family spellings that own
// the sorted Add chain's real bodies (slots 7/10/12/13/17/18). See the port block at the foot.
#include "DecorObjMgrMaybe.h"
// PORT ONLY -- Port_Tracef, for the live-count diagnostic in EffectSpawner_TickMaybe.
#include "PortMode.h"
// PORT ONLY -- LocoBitmap's pPixels/pPalette/bConverted and the shadow-blit guard mask, both
// read by the v574 one-shot effectdump diagnostic in PaintSimpleEffectsMaybe.
#include "LocoBitmap.h"
#include "Ddraw.h"
#endif

extern double DAT_00481170; // last computed FPS sample (src/Main.cpp)

// Padded-vtable view of ONE embedded effect-collection sub-object (the Obj0x477798-family
// registries at EffectSpawner+0x1c/+0x34), modeled only far enough for 0x423ab0 to dispatch
// its registry Add through slot 13. Per src/NetSessionEventQueue.cpp's precedent (and
// src/CarNetObj.h's CarNetObjVtblProbe) the compiler only needs a >=14-slot vtable to select
// the `call [vtbl+0x34]` shape -- the view's own identity is irrelevant (resolved by masked
// reloc). The REAL classes are the family's still-flat registry siblings
// (src/Obj0x477798Family.h's Obj0x477b40 etc.); promoting them to the real base/derived pair
// is blocked on the v486 hierarchy problem (see the ⛔ note at the foot of
// src/Obj0x477798Family.cpp).
// ⚠ Kept TU-LOCAL on purpose: an earlier revision put this probe and the two collection
// members directly on src/EffectSpawner.h's class, and the header's 8 consumer TUs paid for
// it -- src/TilePlacedObj.cpp's SpawnSeqRecordEffectMaybe (4588b0, 143 B) lost its EXACT
// match to the declaration-count rotation (the v485/v486 parity lesson). The header models
// entry points only; the layout view lives beside the one function that has read it.
struct EffectCollectionVtblProbeMaybe {
    virtual void _v00();
    virtual void *_v01(); virtual void *_v02(); virtual void *_v03();
    // vtbl+0x10 (slot 4) -- remove the entry at nIndex (the tick's "this effect said it is
    // finished" path). Return discarded.
    virtual int RemoveAtMaybe(unsigned int nIndex);
    virtual void *_v05(); virtual void *_v06(); virtual void *_v07();
    // vtbl+0x20 (slot 8) -- entry by index, NULL for a hole (the family stores a sparse array).
    virtual void *GetEntryMaybe(unsigned int nIndex);
    virtual void *_v09(); virtual void *_v10();
    // vtbl+0x2c (slot 11) -- the entry-slot count (the sparse array's extent, not the live
    // count): every walk of a collection re-reads it each iteration, so removals inside the
    // loop are seen.
    virtual unsigned int GetSlotCountMaybe();
    virtual void *_v12();
    // vtbl+0x34 (slot 13) -- the family Add (the derived 0x412440 shape over the base's NULL
    // slot; src/Obj0x477798Family.h). Return discarded at the 0x423ab0 call sites.
    virtual int AddMaybe(void *pEffect);
};

// Layout view of `this` for 0x423ab0: only the two embedded collection offsets the original
// dispatches through (+0x1c when the trailing flag is 0, +0x34 when it is nonzero), pinned by
// padding; the rest of the object is still unread (see src/EffectSpawner.h).
struct EffectSpawnerCollectionViewMaybe {
    char pad0x00[4];
    // +0x04 -- the CANDIDATE set, the third of the ctor's three embedded registries and the
    // only one RemoveHandle below touches. Added 2026-07-31 with that body; the ctor's own
    // plate (0x4238c0, further down) already names it.
    EffectCollectionVtblProbeMaybe candidateCollectionMaybe;
    char pad0x08[0x14];
    EffectCollectionVtblProbeMaybe effectCollectionAMaybe; // +0x1c
    char pad0x20[0x14];
    EffectCollectionVtblProbeMaybe effectCollectionBMaybe; // +0x34
};

// ---------------------------------------------------------------------------------------
// 0x4238c0: the CONSTRUCTOR (Ghidra: BigObjTrackingSetsMaybe::CtorMaybe). The original reads
// the layout in full: EffectSpawner is itself polymorphic (own vtable 0x477ad0, stamped last)
// and embeds THREE 0x18-byte registry sub-objects -- the candidate set at +0x4 (base table
// 0x477bd0 / derived 0x477b78) and the placed + ghost sets at +0x1c/+0x34 (0x477b40 /
// 0x477ae8, one derived table serving both). Each sub-object ctor stamps its 14-slot base
// table, zeroes m_count/m_ptr, reserves 100 slots, then stamps its 22-slot derived table and
// zeroes the live count and the two sort-key fields; the body then asks the GHOST set to
// re-key itself through slot 19 (vtbl+0x4c = PlacedObjRegistryMaybe's
// SetSortParamsAndSortMaybe) with (0xc, -4) -- sort key = the dword at entry+0xc, type -4 =
// 4-byte int (see src/DecorObjMgrMaybe.h's sort-key note). The ghost set's reserve is the one
// site VC5 inlines (operator new(0x190) + rep stosd + the neg/sbb/and null-collapse); the
// other two keep the out-of-line call to the family's shared 0x435d10.
//
// Modeled as TU-LOCAL views for the same parity reason as the probe structs above: the real
// base/derived pair is the v486 ⛔ hierarchy problem (see the foot of
// src/Obj0x477798Family.cpp), and touching src/EffectSpawner.h is measured to cost EXACT
// matches across its 8 consumer TUs. Only what the ctor's codegen names is modeled:
// 14 base slots + 8 derived slots with slot 19's real signature, real (declared-only) virtual
// dtors so /GX builds the three unwind states, and a ReserveMaybe whose body is VISIBLE (the
// ghost site's inline expansion is the family's shared body verbatim, constant-folded against
// the fresh m_count == 0 / m_ptr == 0 state).
struct EffectCollectionCtorViewMaybe {
    void **m_ptr;   // +0x4 in the embedding (sub-object +0x4)
    int m_count;    // sub-object +0x8 -- the CAPACITY
    unsigned int m_0c; // +0xc -- the LIVE count. Zeroed at the BASE ctor's tail (after the
                       // reserve): the original's `mov [esi+0xc],0` lands between the reserve
                       // and the derived vtable stamp, which is exactly where a base-ctor-body
                       // store sits. (The Obj0x477758 pair carries this field on the DERIVED
                       // half instead -- the registry tables pin it here.)

    // slot 0 -- the family's shared reserve/regrow (0x435d10). Declared before the dtor for
    // the same reason src/Obj0x477798Family.h does: slot 1 is the scalar deleting dtor.
    // DECLARED-ONLY on this (the candidate/placed) half: the original keeps the out-of-line
    // call at those two sites, and a visible body is what lets /O2 inline it. The ghost
    // half below carries the body, because the original DOES inline it there.
    virtual void ReserveMaybe(unsigned int nCapacity);
    // slot 1 -- the family base-dtor shape ({ vtbl=base; m_count=0; if(m_ptr) delete
    // m_ptr; m_ptr=0; }, src/Obj0x477798Family.h's ~Obj0x477758Base). Body now VISIBLE:
    // the 0x4239e0 dtor inlines all three member dtors. The m_0c = 0 that the original
    // emits BEFORE the base-vtbl re-stamp lives on the DERIVED half's dtor (same split as
    // ~Obj0x477758).
    virtual ~EffectCollectionCtorViewMaybe() {
        m_count = 0;
        if (m_ptr != 0) {
            operator delete(m_ptr);
        }
        m_ptr = 0;
    }
    // Slot 2 (ReleaseStorage) takes nothing; slot 3 (RemoveAt) takes an index. Both arities
    // are the family's, read off src/DecorObjMgrMaybe.h -- see slot 13's note below for why a
    // placeholder's arity is load-bearing rather than cosmetic.
    virtual void *_v02(); virtual void *_v03(unsigned int nIndex);
    // slot 4 (+0x10) -- RemoveAtMaybe(nIndex), and slot 8 (+0x20) -- GetEntryMaybe(nIndex).
    // Both carry their real PARAMETER for the reason slot 13 below spells out; both are
    // currently unfired landmines rather than live defects, and slot 8's is unfired only
    // BECAUSE slot 13 was broken: with every Add dropped, m_0c stayed 0, so the paint
    // family's `for (i = 0; i < GetSlotCountMaybe(); i++) GetEntryMaybe(i)` walk ran zero
    // iterations and never dispatched slot 8. Give slot 13 a real body and slot 8 fires on
    // the next frame -- so the arities go in FIRST.
    virtual void *_v04(unsigned int nIndex); virtual void *_v05();
    // Slot 7 (GetAtMaybe) and slot 12 (IsSlotOccupiedMaybe) take an index, slot 9
    // (SetCopyAtMaybe) and slot 10 (SetAtMaybe) take an index plus an element. All four are
    // on the sorted Add's own dispatch path -- see the port block at the foot of this file.
    virtual void *_v06(); virtual void *_v07(unsigned int nIndex);
    virtual void *_v08(unsigned int nIndex);
    virtual void *_v09(unsigned int nIndex, const void *pSrc);
    virtual void *_v10(unsigned int nIndex, void *pItem);
    virtual void *_v11(); virtual void *_v12(unsigned int nIndex);
    // slot 13 (+0x34) -- the family Add. ⚠ The PARAMETER is not cosmetic on a placeholder:
    // EffectCollectionVtblProbeMaybe above declares this same slot `AddMaybe(void *pEffect)`,
    // and until v571 this spelling declared it with NO parameter. Both are __thiscall, so the
    // CALLEE pops -- and link/gen_stubs.py derives the pop from the MANGLED NAME, so the stub
    // standing in for this declared-only slot returned `ret 0` where every call site pushes 4
    // bytes and expects `ret 4`. The first dispatch through it therefore unbalanced the stack
    // by 4 and returned to garbage. See the port block at the foot of this file.
    virtual void *_v13(void *pObj);

    EffectCollectionCtorViewMaybe(int nCapacity) {
        m_count = 0;
        m_ptr = 0;
        ReserveMaybe(nCapacity);
        m_0c = 0;
    }
};

struct EffectRegistryCtorViewMaybe : EffectCollectionCtorViewMaybe {
    unsigned int nSortKeyOffsetMaybe; // +0x10
    unsigned int nSortKeyTypeMaybe;   // +0x14
    // slots 14..18, 20, 21: the sorted-registry members (IndexOf/SortRange/FindIndex/InsertAt/
    // CompareEntries/SortAll/IsSorted) -- placeholder signatures, only slot 19 is ever called.
    // The registry tier's own arities, likewise the family's: slot 14 IndexOfMaybe(pObj),
    // slot 15 SortRangeMaybe(lo, hi), slot 16 FindIndexMaybe(pObj, lo, hi), slot 17
    // InsertAtMaybe(index, pObj), slot 18 CompareEntriesMaybe(pObj, pOther). Slots 17 and 18
    // are both reached by the sorted Add; 14/15/16 are not, and carry their real arity for
    // the same prophylactic reason (CODEGEN #200 -- fix a vtable's arities as a set).
    virtual void *_v14(void *pObj);
    virtual void *_v15(int nLo, int nHi);
    virtual void *_v16(void *pObj, unsigned int nLo, unsigned int nHi);
    virtual void *_v17(unsigned int nIndex, void *pObj);
    virtual void *_v18(void *pObj, void *pOther);
#ifdef LOCO_PORT
    // PORT SCAFFOLDING ONLY -- an override reuses the base's existing slot 11, so no vtable
    // LAYOUT changes in either build. The DERIVED tables (0x477b78 / 0x477ae8) hold 0x424000
    // here, the m_0c at +0xc, where the base tables hold 0x424010's capacity; the registries
    // these consumers touch all carry this table. Body at the end of this file.
    virtual void *_v11();
    // PORT SCAFFOLDING ONLY -- slot 6, likewise an override of a slot the base already owns.
    // The derived tables hold 0x424270 here where the base tables hold 0x424510. Body at the
    // end of this file.
    virtual void *_v06();
    // PORT SCAFFOLDING ONLY -- slots 13 and 10, the two halves of the family Add that only the
    // DERIVED table supplies. Slot 13 is NULL in both base tables (0x477bd0 / 0x477b40, read
    // out of .rdata) and 0x4362b0 in both derived ones, so declaring it here rather than giving
    // the base spelling a body is what the image itself says. Slot 10 the base DOES supply
    // (0x424170 / 0x4246f0), but with a different body -- the derived's 0x424290 / 0x424790 add
    // an `nIndex > nCountMaybe` reject in front of it -- so this is an override for the same
    // reason slots 6 and 11 above are. Bodies at the end of this file.
    virtual void *_v13(void *pObj);
    virtual void *_v10(unsigned int nIndex, void *pItem);
    // PORT SCAFFOLDING ONLY -- slots 3 and 5, the other two the DERIVED table replaces with a
    // different body rather than merely inheriting: 0x4241e0 over the base's 0x4356b0 (the
    // remove-at that SHIFTS the tail down, against the base's that merely vacates the slot) and
    // 0x424250 over 0x4244f0. Read out of .rdata alongside slots 6/10/11. Slots 2 and 4 are ONE
    // address across all four tables (0x424020 / 0x4356e0), so they need no override and their
    // bodies sit on the base views.
    virtual void *_v03(unsigned int nIndex);
    virtual void *_v05();
#endif
    // slot 19 (vtbl+0x4c) -- PlacedObjRegistryMaybe::SetSortParamsAndSortMaybe (0x424490).
    virtual int SetSortParamsAndSortMaybe(unsigned int nKeyOffset, unsigned int nKeyType);
    virtual void *_v20(); virtual void *_v21();

    EffectRegistryCtorViewMaybe(int nCapacity) : EffectCollectionCtorViewMaybe(nCapacity) {
        nSortKeyOffsetMaybe = 0;
        nSortKeyTypeMaybe = 0;
    }
    // All this body does is zero the live count; the base-vtbl re-stamp and the array
    // teardown after it are the compiler's own epilogue (~Obj0x477758 precedent).
    ~EffectRegistryCtorViewMaybe() {
        m_0c = 0;
    }
};

// The GHOST set's half of the same pair: identical layout and slot map, but ReserveMaybe's
// body is VISIBLE -- this model's rendering of the original inlining the reserve at the
// ghost site only (operator new(0x190) + rep stosd + the neg/sbb/and null-collapse, all
// constant-folded against the fresh m_count == 0 / m_ptr == 0 state). The body is the
// family's shared 0x435d10 verbatim over an opaque element pointer.
struct EffectGhostCollectionCtorViewMaybe {
    void **m_ptr;
    int m_count;
    unsigned int m_0c;

    virtual void ReserveMaybe(unsigned int nCapacity) {
        unsigned int nWanted = nCapacity;
        if ((unsigned int)m_count > nCapacity) {
            unsigned int i = m_count;
            do {
                if (m_ptr[i - 1] != 0) {
                    break;
                }
                i--;
            } while (i > nCapacity);
            nWanted = i;
        }
        void **pOld = m_ptr;
        if (nWanted > 0) {
            m_ptr = (void **)operator new(nWanted * sizeof(void *));
            memset(m_ptr, 0, nWanted * sizeof(void *));
        }
        if (pOld != 0) {
            if (nWanted > 0) {
                unsigned int nCopy = m_count;
                if (nWanted < (unsigned int)m_count) {
                    nCopy = nWanted;
                }
                memcpy(m_ptr, pOld, nCopy * sizeof(void *));
            }
            operator delete(pOld);
        }
        m_count = m_ptr != 0 ? nWanted : 0;
        if (m_count == 0) {
            m_ptr = 0;
        }
    }
    virtual ~EffectGhostCollectionCtorViewMaybe() {
        m_count = 0;
        if (m_ptr != 0) {
            operator delete(m_ptr);
        }
        m_ptr = 0;
    }
    // Slot 2 (ReleaseStorage) takes nothing; slot 3 (RemoveAt) takes an index. Both arities
    // are the family's, read off src/DecorObjMgrMaybe.h -- see slot 13's note below for why a
    // placeholder's arity is load-bearing rather than cosmetic.
    virtual void *_v02(); virtual void *_v03(unsigned int nIndex);
    // slot 4 (+0x10) -- RemoveAtMaybe(nIndex), and slot 8 (+0x20) -- GetEntryMaybe(nIndex).
    // Both carry their real PARAMETER for the reason slot 13 below spells out; both are
    // currently unfired landmines rather than live defects, and slot 8's is unfired only
    // BECAUSE slot 13 was broken: with every Add dropped, m_0c stayed 0, so the paint
    // family's `for (i = 0; i < GetSlotCountMaybe(); i++) GetEntryMaybe(i)` walk ran zero
    // iterations and never dispatched slot 8. Give slot 13 a real body and slot 8 fires on
    // the next frame -- so the arities go in FIRST.
    virtual void *_v04(unsigned int nIndex); virtual void *_v05();
    // Slot 7 (GetAtMaybe) and slot 12 (IsSlotOccupiedMaybe) take an index, slot 9
    // (SetCopyAtMaybe) and slot 10 (SetAtMaybe) take an index plus an element. All four are
    // on the sorted Add's own dispatch path -- see the port block at the foot of this file.
    virtual void *_v06(); virtual void *_v07(unsigned int nIndex);
    virtual void *_v08(unsigned int nIndex);
    virtual void *_v09(unsigned int nIndex, const void *pSrc);
    virtual void *_v10(unsigned int nIndex, void *pItem);
    virtual void *_v11(); virtual void *_v12(unsigned int nIndex);
    // slot 13 (+0x34) -- the family Add. ⚠ The PARAMETER is not cosmetic on a placeholder:
    // EffectCollectionVtblProbeMaybe above declares this same slot `AddMaybe(void *pEffect)`,
    // and until v571 this spelling declared it with NO parameter. Both are __thiscall, so the
    // CALLEE pops -- and link/gen_stubs.py derives the pop from the MANGLED NAME, so the stub
    // standing in for this declared-only slot returned `ret 0` where every call site pushes 4
    // bytes and expects `ret 4`. The first dispatch through it therefore unbalanced the stack
    // by 4 and returned to garbage. See the port block at the foot of this file.
    virtual void *_v13(void *pObj);

    EffectGhostCollectionCtorViewMaybe(int nCapacity) {
        m_count = 0;
        m_ptr = 0;
        ReserveMaybe(nCapacity);
        m_0c = 0;
    }
};

struct EffectGhostRegistryCtorViewMaybe : EffectGhostCollectionCtorViewMaybe {
    unsigned int nSortKeyOffsetMaybe;
    unsigned int nSortKeyTypeMaybe;
    // The registry tier's own arities, likewise the family's: slot 14 IndexOfMaybe(pObj),
    // slot 15 SortRangeMaybe(lo, hi), slot 16 FindIndexMaybe(pObj, lo, hi), slot 17
    // InsertAtMaybe(index, pObj), slot 18 CompareEntriesMaybe(pObj, pOther). Slots 17 and 18
    // are both reached by the sorted Add; 14/15/16 are not, and carry their real arity for
    // the same prophylactic reason (CODEGEN #200 -- fix a vtable's arities as a set).
    virtual void *_v14(void *pObj);
    virtual void *_v15(int nLo, int nHi);
    virtual void *_v16(void *pObj, unsigned int nLo, unsigned int nHi);
    virtual void *_v17(unsigned int nIndex, void *pObj);
    virtual void *_v18(void *pObj, void *pOther);
#ifdef LOCO_PORT
    // PORT SCAFFOLDING ONLY -- an override reuses the base's existing slot 11, so no vtable
    // LAYOUT changes in either build. The DERIVED tables (0x477b78 / 0x477ae8) hold 0x424000
    // here, the m_0c at +0xc, where the base tables hold 0x424010's capacity; the registries
    // these consumers touch all carry this table. Body at the end of this file.
    virtual void *_v11();
    // PORT SCAFFOLDING ONLY -- slot 6, likewise an override of a slot the base already owns.
    // The derived tables hold 0x424270 here where the base tables hold 0x424510. Body at the
    // end of this file.
    virtual void *_v06();
    // PORT SCAFFOLDING ONLY -- slots 13 and 10; see the candidate/placed half's note above.
    virtual void *_v13(void *pObj);
    virtual void *_v10(unsigned int nIndex, void *pItem);
    // PORT SCAFFOLDING ONLY -- slots 3 and 5; see the candidate/placed half's note above.
    virtual void *_v03(unsigned int nIndex);
    virtual void *_v05();
#endif
    virtual int SetSortParamsAndSortMaybe(unsigned int nKeyOffset, unsigned int nKeyType);
    virtual void *_v20(); virtual void *_v21();

    EffectGhostRegistryCtorViewMaybe(int nCapacity) : EffectGhostCollectionCtorViewMaybe(nCapacity) {
        nSortKeyOffsetMaybe = 0;
        nSortKeyTypeMaybe = 0;
    }
    ~EffectGhostRegistryCtorViewMaybe() {
        m_0c = 0;
    }
};

// The EffectSpawner layout view the ctor constructs: own vtable (6 slots: dtor 0x4239c0,
// tick 0x423d70, the 0x423e00/0x423e80/0x423f00 paint family, BroadcastToAllEffectsMaybe
// 0x423f80), then the three registries in declaration order (member ctors run +0x4, +0x1c,
// +0x34 -- the original's order).
struct EffectSpawnerCtorViewMaybe {
    virtual ~EffectSpawnerCtorViewMaybe();          // slot 0 -- 0x4239c0
    virtual void *_v01(); virtual void *_v02(); virtual void *_v03();
    virtual void *_v04(); virtual void *_v05();
    EffectRegistryCtorViewMaybe candidateSetMaybe;  // +0x04 -- 0x477bd0 / 0x477b78
    EffectRegistryCtorViewMaybe placedSetMaybe;     // +0x1c -- 0x477b40 / 0x477ae8
    EffectGhostRegistryCtorViewMaybe ghostSetMaybe; // +0x34 -- 0x477b40 / 0x477ae8

    EffectSpawnerCtorViewMaybe();
};

// FUNCTION: LOCO 0x4238c0  // TODO: sync (deliberate: TU-local ctor view; Ghidra keeps BigObjTrackingSetsMaybe::CtorMaybe)
// EFFECTIVE MATCH, PARKED (v514) -- compiled 240 B against 242, DIFF(54), asmscore total
// 18238 (align=18 reg_pen=2 identity_miss=2 byte_diff=18), insns 76/77. Every block, call,
// constant and store is the original's, in the original's order; the whole residual is TWO
// /Og tie-breaks:
//  (1) The ghost reserve's inline collapse: the original keeps an explicit `cmp eax,ebx`
//      (test of the collapsed m_count against the resident zero register) between the
//      `and eax,0x64` and the `mov [esi+8],eax` store; this build folds the test away and
//      branches on the AND's own flags. Source spelling is the family's shared
//      `m_count = m_ptr != 0 ? nWanted : 0; if (m_count == 0) m_ptr = 0;` -- the SAME text
//      keeps its `test eax,eax` in the out-of-line 0x435d10 body in BOTH builds, so the
//      fold is an inline-context coin flip, not a source shape. Probes REFUTED:
//      routing the collapse through a named local (the #20c lever -- VC5 still folds, the
//      value here comes from neg/sbb/and with live flags, not from a call), moving the
//      m_count store below the branch (DIFF(53) but the store order goes wrong), and the
//      reversed `0 == m_count` (byte-identical).
//  (2) The own-vtable store (0x477ad0) and the EH-state-2 store schedule: the original
//      interleaves both INTO the ghost call's arg pushes (load ghost vtbl -> pushes ->
//      state store -> own-vtbl store -> `call [edx+0x4c]`, ghost vtbl in EDX); this build
//      stamps the own table before the call setup and uses EAX. Same instructions, pure
//      scheduling. Probe refuted: reference-form call (byte-identical).
// Two model facts this function PINS, kept with the views below: the registry's live count
// m_0c is zeroed at the BASE ctor's tail (the `mov [esi+0xc],0` sits between the reserve and
// the derived vtable stamp -- the Obj0x477758 pair carries it on the derived half instead),
// and only the ghost site inlines the reserve (modeled by the ghost-only bodied view class).
EffectSpawnerCtorViewMaybe::EffectSpawnerCtorViewMaybe()
    : candidateSetMaybe(100), placedSetMaybe(100), ghostSetMaybe(100) {
    // Through a pointer so the dispatch stays virtual (`call [vtbl+0x4c]`) rather than the
    // direct call a value-typed `ghostSetMaybe.SetSortParamsAndSortMaybe(...)` devirtualizes to.
    EffectGhostRegistryCtorViewMaybe *pGhostSet = &ghostSetMaybe;
    pGhostSet->SetSortParamsAndSortMaybe(0xc, 0xfffffffc);
}

// FUNCTION: LOCO 0x4239c0 (??_GEffectSpawnerCtorViewMaybe scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x4239e0  // TODO: sync (deliberate: same TU-local view as the 0x4238c0
// ctor above; Ghidra keeps BigObjTrackingSetsMaybe::DtorMaybe)
// EXACT MATCH (171 B). The DESTRUCTOR. Stamps the own vtable (0x477ad0), then runs the
// three member-registry dtors in reverse declaration order (ghost +0x34, placed +0x1c,
// candidate +0x4), each the family's base-dtor shape with its own base table (0x477b40 /
// 0x477b40 / 0x477bd0 -- reloc-masked here, the view classes' identities are irrelevant).
// EH states 1, 0, -1 bracket the three member dtors exactly as /GX builds them. The one
// lever that mattered: the m_0c = 0 the original emits BEFORE each base-vtbl re-stamp lives
// on the DERIVED half's dtor ({ m_0c = 0; } body, base teardown inlined after it) -- the
// src/Obj0x477798Family.h ~Obj0x477758 / ~Obj0x477758Base split verbatim, not a flat
// m_0c-first base dtor.
EffectSpawnerCtorViewMaybe::~EffectSpawnerCtorViewMaybe() {}

// FUNCTION: LOCO 0x423a90
// The SHUTDOWN sweep: clear ALL THREE embedded registries through the family's shared vtable
// slot 6 (+0x18) -- the same call 0x423d00 above makes over two of them, plus the CANDIDATE
// set at +0x4. Sole caller is AppWindow::SaveWindowAndCleanExit (0x407aaa), which is why the
// candidate set is only swept here: 0x423d00 is the per-world reset (WorldBoardMaybe's
// ResetAllTilesMaybe) and the candidate registry outlives a world. The collection order is the
// original's own -- placed, ghost, candidate -- not the layout order. Same free __fastcall form
// and same TU-local layout view as 0x423d00, and src/AppWindow.cpp already declares it under
// exactly this name. TODO: idiom -- fold onto the class with the next EffectSpawner.h opening.
void __fastcall EffectSpawner_ShutdownMaybe(EffectSpawner *pSpawner)
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)pSpawner;
    pView->effectCollectionAMaybe._v06();
    pView->effectCollectionBMaybe._v06();
    pView->candidateCollectionMaybe._v06();
}

// FUNCTION: LOCO 0x423ab0
// EFFECTIVE MATCH (asmscore.py --len 402: total 123822, align=122 reg_pen=16
// identity_miss=16 byte_diff=62, insns 116/126): every gate is instruction-aligned with the
// original -- the minFlyingFps fild/fcomp/test-ah-0x41 frame-rate gate with the 0x3861
// override, the descriptor MaxInstances check (zero-extended word count vs dword cap), the
// must/cant-have pair (must-have descriptor loaded unconditionally BEFORE its id==-1 test,
// exactly as the original schedules it), the new/EH-state sequence, the bValid==1 split to
// the two embedded collections' slot-0x34 Add, and the scalar-deleting-dtor delete.
//
// Residual is ONE /Og allocation cascade: the original NEVER materializes a zero register
// (the cant-have gate is `test eax,eax` + `cmp word ptr,0`, the EH-state-0 store and the
// new-null check use immediates, and ESI keeps pDesc until the ctor result overwrites it at
// 0x423bb8); this build hoists `xor esi,esi` at the cant-have gate and spends it three times
// (`cmp eax,esi`, `cmp word,si`, the EH-state store), which shortens the function by 31 B
// and tail-merges the delete path into the early-bail `xor eax,eax` epilogue the original
// keeps as a SEPARATE fourth epilogue at 0x423c2e (its delete path instead zeroes ESI for
// real and joins the shared `mov eax,esi` return). Same root class as the documented v375
// zero-register-residency + v326/v328 /Og block-layout tie-breaks. Probes REFUTED: `!= 0`
// vs `> 0` on the unsigned-short count (byte-identical), positive-bail vs nested-continuation
// gate shapes (byte-identical), and a redundant `if (pEffect != NULL)` around the delete
// chasing Ghidra's double null-check (VC5 sees through it; byte-identical).
void *EffectSpawner::EffectSpawner_SpawnAtPositionMaybe(int kindId, unsigned short mobilityFlag,
                                                        char directionChar, int x, int y,
                                                        unsigned char flag)
{
    if ((double)g_pApp->minFlyingFps <= DAT_00481170 || kindId == 0x3861) {
        CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
        if (pDesc != NULL &&
            (unsigned int)pDesc->nLiveInstanceCountMaybe < pDesc->nMaxInstances) {
            CursorDesc *pMustDesc =
                g_UIResources.TileKind_GetOrLoadDescriptor(pDesc->nMustHaveKindId);
            if (pDesc->nMustHaveKindId == -1 ||
                (pMustDesc != NULL && pMustDesc->nLiveInstanceCountMaybe != 0)) {
                CursorDesc *pCantDesc =
                    g_UIResources.TileKind_GetOrLoadDescriptor(pDesc->nCantHaveKindId);
                if (pCantDesc != NULL && pCantDesc->nLiveInstanceCountMaybe > 0) {
                    return NULL;
                }
                {
                    AnimEffectObj0x477a90 *pEffect =
                        new AnimEffectObj0x477a90(kindId, mobilityFlag, directionChar, x, y);
                    if (pEffect != NULL) {
                        if (pEffect->bValid == true) {
                            if (flag != 0) {
                                ((EffectSpawnerCollectionViewMaybe *)this)->effectCollectionBMaybe.AddMaybe(pEffect); // idiom-exempt: TU-local layout view -- declaring the members on src/EffectSpawner.h's class is measured (-143 B: TilePlacedObj.cpp's SpawnSeqRecordEffectMaybe loses EXACT to the header declaration-count parity); see the ⚠ note on EffectCollectionVtblProbeMaybe above
                                return pEffect;
                            }
                            ((EffectSpawnerCollectionViewMaybe *)this)->effectCollectionAMaybe.AddMaybe(pEffect); // idiom-exempt: same TU-local layout view as above
                            return pEffect;
                        }
                        delete pEffect;
                        pEffect = NULL;
                    }
                    return pEffect;
                }
            }
        }
    }
    return NULL;
}

// Layout view of `this` for 0x423c50: the ONE embedded collection offset the original
// dispatches through -- the CANDIDATE set at +0x4 (unlike SpawnAtPositionMaybe, which uses
// the placed/ghost sets at +0x1c/+0x34). Same TU-local probe rationale as
// EffectSpawnerCollectionViewMaybe above; slot 13 (vtbl+0x34) is again the family Add.
struct EffectSpawnerCandidateViewMaybe {
    char pad0x00[0x4];
    EffectCollectionVtblProbeMaybe candidateSetMaybe; // +0x4
};

// FUNCTION: LOCO 0x423c50
// EXACT MATCH (171 B). Spawns a SIMPLE (non-positioned) effect: unlike the 0x423ab0 sibling
// there is no fps/descriptor gating -- just `new AnimDescRefObj0x477488(id, subframe, 0, 0)`,
// and a VALID object goes into the CANDIDATE set at +0x4 (not the placed/ghost sets) after a
// RepositionWithHotspot and an nBlitFlags |= 2. The one lever that mattered: the flat
// `if (p && p->bValid) {...} else if (p) { delete p; p = NULL; }` shape -- nested forms let
// VC5 fold away the delete path's `test esi,esi` the original keeps (its first jz jump-threads
// straight to the epilogue while the delete path re-tests).
void *EffectSpawner::EffectSpawner_SpawnSimpleMaybe(int nEffectId, short wArg, int x, int y)
{
    AnimDescRefObj0x477488 *pEffect = new AnimDescRefObj0x477488(nEffectId, wArg, 0, 0);
    if (pEffect != NULL && pEffect->bValid == true) {
        pEffect->RepositionWithHotspot(x, y);
        pEffect->nBlitFlags |= 2;
        ((EffectSpawnerCandidateViewMaybe *)this)->candidateSetMaybe.AddMaybe(pEffect); // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the positioned-spawn call sites above
    }
    else if (pEffect != NULL) {
        delete pEffect;
        pEffect = NULL;
    }
    return pEffect;
}

// FUNCTION: LOCO 0x423d00 (Ghidra: EffectSpawner_ClearBothListsMaybe)
// Clears BOTH embedded effect collections (placed +0x1c, ghost +0x34) through the family's
// shared vtable slot 6 (+0x18, RemoveAll/RemoveAndDeleteAll). Sole caller: WorldBoardMaybe's
// ResetAllTilesMaybe (0x454fe0). Free __fastcall form of the member (the AppWindow.cpp
// escape-hatch pattern): byte-identical to the member shape and keeps src/EffectSpawner.h's
// measured declaration count untouched; the +0x1c/+0x34 layout view is this TU's own.
// TODO: idiom -- fold onto the class with the next EffectSpawner.h opening.
void __fastcall EffectSpawner_ClearBothListsMaybe(EffectSpawner *pSpawner)
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)pSpawner;
    pView->effectCollectionAMaybe._v06();
    pView->effectCollectionBMaybe._v06();
}

// The PAINT family (0x423e00 / 0x423e80 / 0x423f00) -- three identical bodies, one per
// collection and so one per z-plane. Each walks its own collection, skips the sparse array's
// holes and any entry that is not both bValid and bReady, and blits the survivor through the
// family's slot 11 (BlitAnimFrameMaybe) with the caller's clip rect and flag plus the entry's
// OWN nBlitFlags. The `1` the original keeps resident in BL is the `== true` on both byte
// gates. Entries are typed as the shared base rather than AnimEffectObj0x477a90 because the
// dispatch here IS the inherited virtual, unlike the tick's direct TickMaybe call.

// FUNCTION: LOCO 0x423e00
// The CANDIDATE set (+0x4) -- the "simple" effects EffectSpawner_SpawnSimpleMaybe puts there.
void EffectSpawner::PaintSimpleEffectsMaybe(RECT rectClip, char flag)
{
    EffectSpawnerCandidateViewMaybe *pView = (EffectSpawnerCandidateViewMaybe *)this; // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the spawn call sites above
    unsigned int i;

#ifdef LOCO_PORT
    // PORT DIAGNOSTIC (byte-neutral -- preprocesses away for the match build). The three-way
    // split the v573 objective asks for, in ONE change-triggered line: is this walker CALLED at
    // all (nCalls), does its entry survive the bValid/bReady gate, and does the blit fire. The
    // entry's own rect goes out with it, because this walker only runs for DIRTY tiles and the
    // rect is what says whether the effect is even inside the dirty-tile loop's reach.
    static unsigned int nCalls = 0;
    static unsigned int nBlits = 0;
    static unsigned int nIsect = 0;   // clip rect actually overlapped the effect's own rect
    static unsigned int nHaveArt = 0; // ...and pKindDesc->pOwnedObjA was loaded
    static unsigned int nLastKey = 0xffffffff;
    static unsigned char bDumped = 0; // the v574 one-shot deep dump below fires exactly once
    // ⭐ THE v573 REGRESSION CANARY, and the reason this block is worth keeping. `rows` is the
    // number of DISTINCT tile rows the dirty-tile flush has ever repainted. WorldBoardMaybe's
    // FUN_00456700 used to seed its `col` outside the row loop, so only the FIRST row of each
    // coalesced dirty rect was ever painted and this read 3-4 for a whole session; with the
    // original's per-row re-seed restored it reads the full board height (40 here). Any future
    // change that drops it back to single digits has broken incremental repaint again.
    static unsigned char aRowSeen[64];
    static unsigned int nRows = 0;
    nCalls++;
    if (rectClip.top >= 0 && (rectClip.top >> 4) < 64 && !aRowSeen[rectClip.top >> 4]) {
        aRowSeen[rectClip.top >> 4] = 1;
        nRows++;
    }
    {
        unsigned int nSlots = pView->candidateSetMaybe.GetSlotCountMaybe();
        unsigned int nNonNull = 0, nValid = 0, nReady = 0, j;
        RECT rcFirst = { 0, 0, 0, 0 };
        for (j = 0; j < nSlots; j++) {
            AnimDescRefObj0x477488 *pE =
                (AnimDescRefObj0x477488 *)pView->candidateSetMaybe.GetEntryMaybe(j);
            if (pE != NULL) {
                RECT rcHit;
                if (nNonNull == 0) {
                    rcFirst = pE->rect;
                }
                nNonNull++;
                if (pE->bValid) nValid++;
                if (pE->bReady) nReady++;
                // The two remaining early-outs inside AnimDescRefObj0x477488::
                // BlitAnimFrameMaybe (0x405e80), counted separately so a run says WHICH one
                // swallows the draw: no loaded sprite, or no overlap with this dirty tile.
                if (pE->pKindDesc != NULL && pE->pKindDesc->pOwnedObjA != NULL) nHaveArt++;
                if (IntersectRect(&rcHit, &pE->rect, &rectClip)) nIsect++;
            }
        }
        // ONE-SHOT deep dump of the first candidate that actually reaches the blit, answering
        // the v574 question "why is it DARK" without another run: the effect ORs nBlitFlags |= 2
        // at spawn and RestoreOverlapBlt dispatch case 2 IS LocoBitmap::ShadowBlit, so a dark
        // block is the RIGHT KIND of output -- what this pins is which of the three inputs is
        // wrong. ShadowBlit writes dest = pPalette[srcIndex] with index 0 = untouched dest and
        // index 1 = the 50%-darkened dest, so the source histogram alone separates "shadow
        // sprite, drawn correctly" (mixed 0/1) from "opaque sprite through a blank palette"
        // (mostly >=2 with palnz small). bConverted says whether the software ShadowBlit runs
        // at all or DirectDraw takes the flags==2 else-arm instead.
        if (!bDumped && nSlots != 0) {
            for (j = 0; j < nSlots; j++) {
                AnimDescRefObj0x477488 *pE =
                    (AnimDescRefObj0x477488 *)pView->candidateSetMaybe.GetEntryMaybe(j);
                RECT rcHit;
                if (pE == NULL || pE->pKindDesc == NULL || pE->pKindDesc->pOwnedObjA == NULL) {
                    continue;
                }
                if (!IntersectRect(&rcHit, &pE->rect, &rectClip)) {
                    continue;
                }
                {
                    LocoBitmap *pBmp = pE->pKindDesc->pOwnedObjA;
                    unsigned int n0 = 0, n1 = 0, n2 = 0, palnz = 0, k;
                    int sx, sy;
                    // Histogram the source slice rectViewport points at -- that is exactly the
                    // span BlitAnimFrameMaybe insets to build rectSrc.
                    if (pBmp->pPixels != NULL) {
                        for (sy = pE->rectViewport.top; sy < pE->rectViewport.bottom; sy++) {
                            if (sy < 0 || (unsigned)sy >= pBmp->height) continue;
                            for (sx = pE->rectViewport.left; sx < pE->rectViewport.right; sx++) {
                                unsigned char idx;
                                if (sx < 0 || (unsigned)sx >= pBmp->width) continue;
                                idx = pBmp->pPixels[sy * pBmp->width + sx];
                                if (idx == 0) n0++; else if (idx == 1) n1++; else n2++;
                            }
                        }
                    }
                    if (pBmp->pPalette != NULL) {
                        for (k = 2; k < 256; k++) {
                            if (pBmp->pPalette[k] != 0) palnz++;
                        }
                    }
                    Port_Tracef("effectdump: flags=%x sub=%d u16=%d conv=%d w=%u h=%u "
                                "pix=%p pal=%p palnz=%u src0=%u src1=%u src2=%u "
                                "rect=%d,%d,%d,%d vp=%d,%d,%d,%d mask=%x\n",
                                pE->nBlitFlags, pE->nSubFrame,
                                pE->pKindDesc->paFrameEntries != NULL
                                    ? pE->pKindDesc->paFrameEntries[pE->nSubFrame].Unk0x16Maybe
                                    : -1,
                                pBmp->bConverted, pBmp->width, pBmp->height,
                                (void *)pBmp->pPixels, (void *)pBmp->pPalette, palnz,
                                n0, n1, n2,
                                pE->rect.left, pE->rect.top, pE->rect.right, pE->rect.bottom,
                                pE->rectViewport.left, pE->rectViewport.top,
                                pE->rectViewport.right, pE->rectViewport.bottom,
                                (unsigned int)g_wChannelBleedGuardMask);
                    bDumped = 1;
                }
                break;
            }
        }
        {
            // Bucket the call count into the key so the totals keep reporting as they grow,
            // instead of one line for the whole run.
            unsigned int nKey = (nSlots << 24) | (nNonNull << 16) | (nValid << 8) | nReady;
            nKey ^= (nCalls >> 12) << 4;
            if (nKey != nLastKey) {
                Port_Tracef("paintsimple: calls=%u blits=%u isect=%u haveart=%u rows=%u "
                            "slots=%u nonnull=%u valid=%u ready=%u rc=%d,%d,%d,%d\n",
                            nCalls, nBlits, nIsect, nHaveArt, nRows, nSlots, nNonNull, nValid,
                            nReady, rcFirst.left, rcFirst.top, rcFirst.right, rcFirst.bottom);
                nLastKey = nKey;
            }
        }
    }
#endif
    for (i = 0; i < pView->candidateSetMaybe.GetSlotCountMaybe(); i++) {
        AnimDescRefObj0x477488 *pEffect =
            (AnimDescRefObj0x477488 *)pView->candidateSetMaybe.GetEntryMaybe(i);
        if (pEffect != NULL && pEffect->bValid == true && pEffect->bReady == true) {
#ifdef LOCO_PORT
            nBlits++;
#endif
            pEffect->BlitAnimFrameMaybe(rectClip, flag, pEffect->nBlitFlags);
        }
    }
}

// FUNCTION: LOCO 0x423e80
// The PLACED set (+0x1c) -- effects that share the world objects' own z-plane.
void EffectSpawner::PaintInPlaneEffectsMaybe(RECT rectClip, char flag)
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)this; // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the spawn call sites above
    unsigned int i;

    for (i = 0; i < pView->effectCollectionAMaybe.GetSlotCountMaybe(); i++) {
        AnimDescRefObj0x477488 *pEffect =
            (AnimDescRefObj0x477488 *)pView->effectCollectionAMaybe.GetEntryMaybe(i);
        if (pEffect != NULL && pEffect->bValid == true && pEffect->bReady == true) {
            pEffect->BlitAnimFrameMaybe(rectClip, flag, pEffect->nBlitFlags);
        }
    }
}

// FUNCTION: LOCO 0x423f00
// The GHOST set (+0x34) -- effects drawn ON TOP of everything else.
void EffectSpawner::PaintOnTopEffectsMaybe(RECT rectClip, char flag)
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)this; // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the spawn call sites above
    unsigned int i;

    for (i = 0; i < pView->effectCollectionBMaybe.GetSlotCountMaybe(); i++) {
        AnimDescRefObj0x477488 *pEffect =
            (AnimDescRefObj0x477488 *)pView->effectCollectionBMaybe.GetEntryMaybe(i);
        if (pEffect != NULL && pEffect->bValid == true && pEffect->bReady == true) {
            pEffect->BlitAnimFrameMaybe(rectClip, flag, pEffect->nBlitFlags);
        }
    }
}

// FUNCTION: LOCO 0x423d20
// Withdraw one previously handed-out handle from the CANDIDATE set at +0x4: linear-scan the
// sparse array for the entry that IS this pointer and drop that slot. Unlike the tick and
// broadcast walks below, this one is written as a search loop rather than a for-walk -- the
// original tests the slot count once up front, then re-reads it only at the bottom of each
// failed comparison, which is the shape a `while (GetEntry(i) != pHandle)` search produces and
// not the shape a `for (i = 0; i < GetSlotCount(); i++)` walk produces. Holes are NOT skipped:
// a NULL slot simply never equals a live handle. RemoveAtMaybe's int return is discarded.
//
// EFFECTIVE MATCH -- 74 B compiled against the original's 68, insns 37/35, and the whole excess
// is ONE peeled copy of the loop test. Both exits, both epilogues, every call and every operand
// are the original's; VC5 simply rotates this loop and duplicates the `GetEntryMaybe(i) ==
// pHandle` compare at the bottom (asmscore --dump: two `+` rows, `cmp eax,ebx` / `jne`, plus the
// pHandle parameter load sliding from before the loop to inside it). The original enters its
// loop at the TOP with the count tested once ahead of it, keeps its own separate
// loop-exhausted epilogue at 0x423d50, and lets the found-arm fall into the shared one at
// 0x423d5e.
//
// Three source shapes tried, all one compile apart, all landing on the SAME peel (DIFF 42-43,
// 74 B, 37 insns every time): (1) `while (GetEntry(i) != pHandle) { i++; if (GetSlotCount() <=
// i) return; }` then RemoveAt after the loop; (2) the `do { if (== pHandle) { RemoveAt; return;
// } i++; } while (i < GetSlotCount())` form kept below, which reads closest to the original's
// actual control flow; (3) the same `while` as (1) with a prefix `if (++i >= GetSlotCount())`
// guard -- the compound bottom-test that DOES suppress the peel on 0x44cb10's forward re-walk
// (src/PeerTrainNode.cpp). It does not suppress it here. Same family as that function's own
// parked residual; see docs/PARKED.md.
void EffectSpawner::EffectSpawner_RemoveHandle(void *pHandle)
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)this; // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the spawn call sites above
    unsigned int i = 0;

    if (pView->candidateCollectionMaybe.GetSlotCountMaybe() != 0) {
        do {
            if (pView->candidateCollectionMaybe.GetEntryMaybe(i) == pHandle) {
                pView->candidateCollectionMaybe.RemoveAtMaybe(i);
                return;
            }
            i++;
        } while (i < pView->candidateCollectionMaybe.GetSlotCountMaybe());
    }
}

// FUNCTION: LOCO 0x423d70
// The singleton's per-frame tick, run over BOTH effect collections -- the ghost set at +0x34
// first, then the placed set at +0x1c. Each walk re-reads the collection's slot count every
// iteration (removals inside the loop are meant to be seen), skips the sparse array's holes,
// and steps the entry through AnimEffectObj0x477a90::TickMaybe, whose `1` return means "I am
// finished" -- those entries are dropped through the collection's own slot 4. TickMaybe is
// called NON-virtually: 0x477a90's table stops at slot 14, so the tick reaches it directly on
// the concrete type it knows the collections hold.
void EffectSpawner::EffectSpawner_TickMaybe()
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)this; // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the spawn call sites above
    unsigned int i;

#ifdef LOCO_PORT
    // PORT DIAGNOSTIC (byte-neutral -- preprocesses away for the match build). The live counts
    // of the two walked collections, logged only when one of them CHANGES, which keeps a
    // multi-thousand-frame run to a handful of lines. This is the direct evidence that the
    // v572 Add chain works: before it, both counters were pinned at 0 for the whole run.
    {
        static unsigned int nLastB = 0xffffffff;
        static unsigned int nLastA = 0xffffffff;
        static unsigned int nLastC = 0xffffffff;
        unsigned int nB = pView->effectCollectionBMaybe.GetSlotCountMaybe();
        unsigned int nA = pView->effectCollectionAMaybe.GetSlotCountMaybe();
        // The CANDIDATE set is not walked by this tick at all -- it is the one
        // PaintSimpleEffectsMaybe (0x423e00) draws, so it is the number that says whether an
        // effect can appear on screen, and it belongs beside the other two.
        unsigned int nC = pView->candidateCollectionMaybe.GetSlotCountMaybe();
        if (nB != nLastB || nA != nLastA || nC != nLastC) {
            Port_Tracef("effects: ghost=%u placed=%u candidate=%u\n", nB, nA, nC);
            nLastB = nB;
            nLastA = nA;
            nLastC = nC;
        }
    }
#endif
    for (i = 0; i < pView->effectCollectionBMaybe.GetSlotCountMaybe(); i++) {
        AnimEffectObj0x477a90 *pEffect =
            (AnimEffectObj0x477a90 *)pView->effectCollectionBMaybe.GetEntryMaybe(i);
        if (pEffect != NULL) {
            if (pEffect->TickMaybe() == 1) {
                pView->effectCollectionBMaybe.RemoveAtMaybe(i);
            }
        }
    }
    for (i = 0; i < pView->effectCollectionAMaybe.GetSlotCountMaybe(); i++) {
        AnimEffectObj0x477a90 *pEffect =
            (AnimEffectObj0x477a90 *)pView->effectCollectionAMaybe.GetEntryMaybe(i);
        if (pEffect != NULL) {
            if (pEffect->TickMaybe() == 1) {
                pView->effectCollectionAMaybe.RemoveAtMaybe(i);
            }
        }
    }
}

// FUNCTION: LOCO 0x423f80
// Broadcasts one ready-state flag to every live entry in BOTH effect collections -- the ghost
// set at +0x34 first, then the placed set at +0x1c (the original's order). Each walk re-reads
// the collection's slot count every iteration and skips the sparse array's holes. The entries
// are the AnimEffectObj0x477a90s the two spawn paths put there, so the per-entry dispatch is
// the family's inherited slot 9 (vtbl+0x24, SetReadyStateMaybe). AppWindow_EnterBuildMode
// passes 1, which is what lets parked effects start animating again.
void EffectSpawner::BroadcastToAllEffectsMaybe(bool bFlag)
{
    EffectSpawnerCollectionViewMaybe *pView = (EffectSpawnerCollectionViewMaybe *)this; // idiom-exempt: TU-local layout view -- same measured header-parity rationale as the spawn call sites above
    unsigned int i;

    for (i = 0; i < pView->effectCollectionBMaybe.GetSlotCountMaybe(); i++) {
        AnimDescRefObj0x477488 *pEffect =
            (AnimDescRefObj0x477488 *)pView->effectCollectionBMaybe.GetEntryMaybe(i);
        if (pEffect != NULL) {
            pEffect->SetReadyStateMaybe(bFlag);
        }
    }
    for (i = 0; i < pView->effectCollectionAMaybe.GetSlotCountMaybe(); i++) {
        AnimDescRefObj0x477488 *pEffect =
            (AnimDescRefObj0x477488 *)pView->effectCollectionAMaybe.GetEntryMaybe(i);
        if (pEffect != NULL) {
            pEffect->SetReadyStateMaybe(bFlag);
        }
    }
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 8 of 13: DAT_004fd220 (EffectSpawner), ctor 0x4238c0. Constructed through the TU-local
// EffectSpawnerCtorViewMaybe, the only type here that carries the real layout -- src/EffectSpawner.h
// models entry points only, so `new (…) EffectSpawner()` would construct nothing.
//
// This is the hook the v561 crash backtrace led to: with the three embedded registries' vtable
// pointers left NULL, BroadcastToAllEffectsMaybe (0x423f80) faulted at its first
// `call [eax+0x2c]` with eax=0, reached from AppWindow_EnterBuildMode -- i.e. on entering build
// mode, every time.
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_EffectSpawner(void) {
    new ((void *)&DAT_004fd220) EffectSpawnerCtorViewMaybe();
}

// --- slot 11 on the four ctor views: the same defect as the PlacedObjCollectionMaybe family ---
// These four TU-local classes are what CONSTRUCT the spawner's three embedded registries, so the
// compiler-generated vtable THEY produce is the one every later dispatch on those sub-objects
// lands in -- including the paint/tick walks' `GetSlotCountMaybe()` (slot 11) and
// `GetEntryMaybe()` (slot 8), which spell the same slots on a different view entirely. Slot 11
// was declared-only here, so it went to a generated `xor eax,eax; ret` stub and every walk read
// a count of ZERO: no effect was ever painted, ticked or removed. Found v564 from
// link/stubs.cpp's stub_calls.log, where `_v11` was the hottest stub of the whole run.
//
// The split below is read straight out of the image's own .rdata rather than inferred: the two
// BASE tables (0x477bd0 candidate, 0x477b40 ghost) hold 0x424010 in slot 11 -- the CAPACITY at
// +0x8 -- and the two DERIVED tables (0x477b78, 0x477ae8) hold 0x424000, the m_0c at +0xc. That
// is the same base/derived slot-11 pair the Obj0x477758 family documents, and the registries
// these consumers touch all end up carrying the DERIVED table.
// ⚠ Slot 8 (0x424030 in all four tables) is NOT forwarded yet and is the next thing to fire once
// these land: its placeholder here is declared `virtual void *_v08()` with NO parameter, so a
// forwarder cannot see the index. Giving it the real signature is a declaration change in a TU
// with measured parity sensitivity, so it needs its own measurement -- see the pickup block.
void *EffectCollectionCtorViewMaybe::_v11() { return (void *)(unsigned int)m_count; }
void *EffectGhostCollectionCtorViewMaybe::_v11() { return (void *)(unsigned int)m_count; }
void *EffectRegistryCtorViewMaybe::_v11() { return (void *)m_0c; }
void *EffectGhostRegistryCtorViewMaybe::_v11() { return (void *)m_0c; }

// --- slot 0 on the CANDIDATE/PLACED half: the last of the three ctor-time stubs ------------
// Found v569 from tools/lint_desync.py's new INHERIT class, then confirmed against
// link/stubs.cpp's stub_calls.log, where BOTH of this symbol's calls come from
// EffectSpawnerCtorViewMaybe::EffectSpawnerCtorViewMaybe (0x4238c0) itself.
//
// The declaration is deliberately body-less for the MATCH build -- the original keeps an
// out-of-line `call 0x435d10` at the candidate and placed sites, and a visible body is exactly
// what would let /O2 inline it there (see the declaration's own note above). That is right for
// the match and wrong for a link: nothing defines the symbol, so link/gen_stubs.py supplies a
// `ret`, and the base ctor's `ReserveMaybe(nCapacity)` -- sitting between `m_ptr = 0` and
// `m_0c = 0` -- reserved nothing. Both the candidate set and the placed set came out of
// construction with m_ptr == NULL and m_count == 0 instead of the 100 slots they asked for,
// so every effect the spawner tried to register was dropped.
//
// The ghost half's identical declaration carries the family's shared body verbatim, and slot 0
// is 0x435d10 in ALL FOUR tables (0x477bd0 / 0x477b78 / 0x477b40 / 0x477ae8, read out of .rdata)
// -- one original address in four vtables, which is the same "two declared spellings, one
// compiled body" shape src/Obj0x477798Family.cpp's port block forwards for the
// PlacedObjCollectionMaybe family. The cast is sound by layout, not by luck: vptr at +0,
// m_ptr at +4, m_count at +8, m_0c at +0xc in both views, and the body only ever moves pointers
// and compares the two counts -- it never names the element type.
void EffectCollectionCtorViewMaybe::ReserveMaybe(unsigned int nCapacity) {
    ((EffectGhostCollectionCtorViewMaybe *)this)->EffectGhostCollectionCtorViewMaybe::ReserveMaybe(nCapacity); // idiom-exempt: one original address, two declared spellings; port-only
}

// --- slot 19 on both REGISTRY halves: 0x424490, the same defect one slot further out --------
// Also v569 / INHERIT. The ctor's one real virtual dispatch -- `pGhostSet->
// SetSortParamsAndSortMaybe(0xc, 0xfffffffc)` at 0x4238c0's tail -- reached a `ret` stub, so
// the ghost registry's sort key was left at the type code 0 that means "no key configured".
// InsertInSortedPositionMaybe (slot 13) tests exactly that field to decide whether to scan for
// an insertion point at all, so every ghost the spawner registered was appended at the live end
// in arrival order rather than sorted by the 4-byte int at +0xc.
//
// Written out rather than forwarded: slot 19 is 0x424490 in every derived table in the family
// and its whole body is two field stores plus a virtual call to slot 20, over fields this view
// already models. Slot 20 (0x4244d0) is `if (m_0c > 1) SortRange(0, m_0c - 1); return 0;` and
// is still a placeholder here -- harmless at the only call site, which runs against a registry
// that was constructed empty three statements earlier, so the real body would take its
// early-out too. Left as a stub deliberately rather than given a body it cannot dispatch
// through (slot 15 / slot 18 are placeholders with no usable signatures).
int EffectRegistryCtorViewMaybe::SetSortParamsAndSortMaybe(unsigned int nKeyOffset,
                                                           unsigned int nKeyType) {
    nSortKeyOffsetMaybe = nKeyOffset;
    nSortKeyTypeMaybe = nKeyType;
    _v20();
    return 0;
}
int EffectGhostRegistryCtorViewMaybe::SetSortParamsAndSortMaybe(unsigned int nKeyOffset,
                                                                unsigned int nKeyType) {
    nSortKeyOffsetMaybe = nKeyOffset;
    nSortKeyTypeMaybe = nKeyType;
    _v20();
    return 0;
}

// --- slot 6 on BOTH halves and BOTH tiers: the teardown sweep ------------------------------
// Found v570 from link/stubs.cpp's stub_calls.log on an interactive boot, where the two BASE
// spellings were 2 of the 4 remaining stub hits. Slot 6 is the destroying form of slot 5, and
// it is the whole body of two functions: EffectSpawner_ShutdownMaybe (0x423a90, the app's
// clean-exit sweep over all three registries) and 0x423d00's per-world reset over two of them.
// Both are a plain `call [vtbl+0x18]` in the original -- verified in the raw disasm, this is a
// real indirect dispatch, not a devirtualized value-typed call -- so the table the live object
// carries is what decides which body runs, and every one of these three registries carries the
// DERIVED table. With the slot stubbed, a world reset destroyed no effect and freed no slot.
//
// The base/derived split is read out of .rdata, same method as slot 11's above: the two BASE
// tables (0x477b40 / 0x477bd0) hold 0x424510 and the two DERIVED tables (0x477b78 / 0x477ae8)
// hold 0x424270. They are NOT interchangeable -- the base form walks the whole CAPACITY
// (`for (i = 0; i < m_count; i++) RemoveAndDeleteAt(i)`) while the derived peels the LIVE range
// off the end (`while (m_0c != 0) RemoveAndDeleteAt(m_0c - 1)`), so forwarding the derived
// tier to the base body would call RemoveAndDeleteAt on every empty slot of a 100-slot
// registry. Hence the two declared overrides above rather than four copies of one forward.
//
// ⚠ This only became safe once slot 3 was wired: slot 6's derived form drives slot 4 (0x4356e0,
// `delete RemoveAt(idx)`), slot 4 dispatches slot 3, and `?RemoveAt@Obj0x477758@@` was itself a
// `ret` stub -- which never lets m_0c come down, so the while-loop would not have terminated.
// The forwarder for it is in src/Obj0x477798Family.cpp's own port block; see its note.
//
// Type-independence is the same argument slot 0 and slot 11 make: both bodies only compare and
// decrement the two counts and dispatch through slots the live object already carries. Neither
// names the element type -- which is exactly why the original ships one copy of each for the
// whole family.
void *EffectCollectionCtorViewMaybe::_v06() {
    ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAndDeleteAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostCollectionCtorViewMaybe::_v06() {
    ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAndDeleteAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectRegistryCtorViewMaybe::_v06() {
    ((Obj0x477758 *)this)->Obj0x477758::RemoveAndDeleteAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostRegistryCtorViewMaybe::_v06() {
    ((Obj0x477758 *)this)->Obj0x477758::RemoveAndDeleteAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}

// --- the ADD chain: slots 13, 17, 10 plus the scan's 7/12/18 ------------------------------
// Found v571 from link/stubs.cpp's stub_calls.log, where `_v13` on the two collection halves
// was the whole of what remained between a booting port and a port with effects in it: 26 hits
// on the ghost half, 1 on the candidate/placed half, every one of them an effect the spawner
// registered and the registry then dropped. Slot 13 was a `ret` stub, so
// EffectSpawner_SpawnAtPositionMaybe's `pSet->AddMaybe(pEffect)` returned having stored
// nothing, and the freshly-constructed AnimEffectObj0x477a90 leaked on the spot. Nothing was
// ever painted or ticked because nothing was ever in either set.
//
// ⚠ The chain has to be walked WHOLE before any one link is forwarded (CODEGEN #199). Giving
// slot 13 alone a body just moves the drop one level down: 0x4362b0 ends in
// `InsertAtMaybe(i, pObj)` (slot 17), and slot 17 ends in `SetAtMaybe(nIndex, pObj)` (slot 10).
// Both were stubs too, so a slot-13-only fix would have run the insertion-point scan, found the
// right index, and still stored nothing -- and, worse, would have looked like a fix, since the
// stub log's `_v13` row disappears either way. The dispatches are read straight off the
// original at 0x4362b0: `call [eax+0x30]` (slot 12), `call [ebx+0x1c]` (slot **7**, not slot 8
// as docs/subsystems.md said until v572), `call [ebx+0x48]` (slot 18), `call [edx+0x44]`
// (slot 17); and at 0x424320: `call [ebx]` (slot 0) and `call [edx+0x28]` (slot 10).
//
// Which of those six the port actually needs differs per registry, and only because of the sort
// key. The candidate set (+0x4) and the placed set (+0x1c) keep type code 0, so slot 13's scan
// is skipped entirely (`i = nCountMaybe`, an append) and they need 13 -> 17 -> 10. The GHOST
// set (+0x34) is re-keyed by the ctor through slot 19 with (0xc, -4) -- and the type field is
// UNSIGNED, so -4 reads as 0xfffffffc and the `nSortKeyTypeMaybe > 0` test sends it down the
// scanning path, where slots 12, 7 and 18 all fire. Since the ghost half is the one taking 26
// of the 27 Adds, that path is the hot one, not the exceptional one.
//
// Slots 7 and 12 are ONE address across both tiers (0x424530 / 0x424760), so their bodies sit
// on the base views; slot 18 exists only on the derived tier; slots 13 and 10 are declared as
// derived overrides above for the reasons given there. Every forwarder casts to the family
// spelling that owns the real body -- src/DecorObjMgrMaybe.h's pair, whose layout is this
// view's field for field (vptr, array at +4, capacity at +8, live count at +0xc, and the two
// sort-key words at +0x10/+0x14) -- and every body it lands in dispatches its own onward calls
// VIRTUALLY on `this`, so they come back to these views rather than to the walker/road-vehicle
// registries the casts name.
void *EffectCollectionCtorViewMaybe::_v07(unsigned int nIndex) {
    return ((PlacedObjCollectionMaybe *)this)->PlacedObjCollectionMaybe::GetAtMaybe(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostCollectionCtorViewMaybe::_v07(unsigned int nIndex) {
    return ((PlacedObjCollectionMaybe *)this)->PlacedObjCollectionMaybe::GetAtMaybe(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectCollectionCtorViewMaybe::_v12(unsigned int nIndex) {
    return (void *)(unsigned int)(unsigned char)
        ((PlacedObjCollectionMaybe *)this)->PlacedObjCollectionMaybe::IsSlotOccupiedMaybe(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostCollectionCtorViewMaybe::_v12(unsigned int nIndex) {
    return (void *)(unsigned int)(unsigned char)
        ((PlacedObjCollectionMaybe *)this)->PlacedObjCollectionMaybe::IsSlotOccupiedMaybe(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectRegistryCtorViewMaybe::_v18(void *pObj, void *pOther) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::CompareEntriesMaybe(pObj, pOther); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v18(void *pObj, void *pOther) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::CompareEntriesMaybe(pObj, pOther); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectRegistryCtorViewMaybe::_v13(void *pObj) {
    ((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::InsertInSortedPositionMaybe(pObj); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostRegistryCtorViewMaybe::_v13(void *pObj) {
    ((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::InsertInSortedPositionMaybe(pObj); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectRegistryCtorViewMaybe::_v17(unsigned int nIndex, void *pObj) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::InsertAtMaybe(nIndex, pObj); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v17(unsigned int nIndex, void *pObj) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::InsertAtMaybe(nIndex, pObj); // idiom-exempt: one original address, two declared spellings; port-only
}
// Slot 8, the bounds-checked accessor over slot 7 (0x424030) -- and the single loudest row of
// the post-v572 stub log: 41526 hits in one boot-to-world run, against ZERO before slot 13 was
// wired. That is the mask v571 called: with every Add dropped, the live count stayed 0, so the
// paint and tick walks' `for (i = 0; i < GetSlotCountMaybe(); i++) GetEntryMaybe(i)` ran no
// iterations and never reached this slot. Fix the Add and the next hole downstream lights up
// immediately -- two bugs of one class, each hiding the other.
//
// The forwarded body is itself a VIRTUAL call to slot 7 (that is what the original does at
// 0x424030), so it lands on the `_v07` forwarders above rather than unconditionally on the
// family's own slot 7.
void *EffectCollectionCtorViewMaybe::_v08(unsigned int nIndex) {
    return ((PlacedObjCollectionMaybe *)this)->PlacedObjCollectionMaybe::GetAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostCollectionCtorViewMaybe::_v08(unsigned int nIndex) {
    return ((PlacedObjCollectionMaybe *)this)->PlacedObjCollectionMaybe::GetAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}

// Slots 14/15/16/20 -- the registry's SORT and LOOKUP surface, wired as a set for the same
// reason the Add chain was: each one only dispatches the others, so forwarding any one of them
// alone just relocates the drop. Slot 20 (SortAllMaybe) drives slot 15 (SortRangeMaybe), which
// recurses through itself and compares through slot 18; slot 14 (IndexOfMaybe) drives slot 16
// (FindIndexMaybe), which also compares through slot 18. All five bodies are real and
// type-independent, and slot 18 was the last of them to be wired, above.
//
// Slot 20 is the one with a witness: the ctor's `SetSortParamsAndSortMaybe(0xc, -4)` on the
// ghost set ends in a slot-20 dispatch, and it shows in the stub log at exactly 1 hit. That
// call is harmless either way -- the registry is three statements old and empty, so the real
// body takes its `nCountMaybe > 1` early-out -- but DecorObjMgrMaybe::TickCategory7OnlyMaybe
// re-keys a POPULATED registry through the same slot the moment a walker arrives with a
// per-instance category name, and there the stub would silently skip the re-sort.
// Slots 2/3/4/5 -- the REMOVE surface, and the third hole this session's chain uncovered by
// filling the one in front of it. Wiring slot 8 turned `_v04` into the loudest stub row in the
// next run (12866 hits): the tick walk could finally SEE the entries it had been blind to, and
// every effect that reported itself finished asked to be removed. As with slot 13 -> 17 -> 10,
// forwarding slot 4 alone would not have worked -- its whole body is `delete RemoveAt(idx)`,
// both halves virtual, so it drives slot 3, and a `ret` stub there returns NULL: the delete
// becomes a no-op, the slot is never vacated, and the entry is re-offered on every subsequent
// frame forever. Slot 6's teardown loop (`while (m_0c != 0) RemoveAndDeleteAt(m_0c - 1)`) would
// not terminate at all.
//
// The tier split is .rdata's, not an inference: slots 2 and 4 are one address across all four
// tables (0x424020 ReleaseStorage, 0x4356e0 RemoveAndDeleteAt -- the single most shared function
// in the family), so they get one body each on the base views; slots 3 and 5 are 0x4356b0 /
// 0x4244f0 on the base tables and 0x4241e0 / 0x424250 on the derived, and the difference is
// real behaviour (the derived remove-at SHIFTS the tail down and decrements the live count where
// the base only vacates the slot), so they get overrides declared above.
void *EffectCollectionCtorViewMaybe::_v02() {
    ((Obj0x477758Base *)this)->Obj0x477758Base::ReleaseStorage(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostCollectionCtorViewMaybe::_v02() {
    ((Obj0x477758Base *)this)->Obj0x477758Base::ReleaseStorage(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectCollectionCtorViewMaybe::_v03(unsigned int nIndex) {
    return ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostCollectionCtorViewMaybe::_v03(unsigned int nIndex) {
    return ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectRegistryCtorViewMaybe::_v03(unsigned int nIndex) {
    return ((Obj0x477758 *)this)->Obj0x477758::RemoveAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v03(unsigned int nIndex) {
    return ((Obj0x477758 *)this)->Obj0x477758::RemoveAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectCollectionCtorViewMaybe::_v04(unsigned int nIndex) {
    ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAndDeleteAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostCollectionCtorViewMaybe::_v04(unsigned int nIndex) {
    ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAndDeleteAt(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectCollectionCtorViewMaybe::_v05() {
    ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostCollectionCtorViewMaybe::_v05() {
    ((Obj0x477758Base *)this)->Obj0x477758Base::RemoveAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectRegistryCtorViewMaybe::_v05() {
    ((Obj0x477758 *)this)->Obj0x477758::RemoveAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostRegistryCtorViewMaybe::_v05() {
    ((Obj0x477758 *)this)->Obj0x477758::RemoveAll(); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}

void *EffectRegistryCtorViewMaybe::_v14(void *pObj) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::IndexOfMaybe(pObj); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v14(void *pObj) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::IndexOfMaybe(pObj); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectRegistryCtorViewMaybe::_v15(int nLo, int nHi) {
    ((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::SortRangeMaybe(nLo, nHi); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectGhostRegistryCtorViewMaybe::_v15(int nLo, int nHi) {
    ((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::SortRangeMaybe(nLo, nHi); // idiom-exempt: one original address, two declared spellings; port-only
    return 0;
}
void *EffectRegistryCtorViewMaybe::_v16(void *pObj, unsigned int nLo, unsigned int nHi) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::FindIndexMaybe(pObj, nLo, nHi); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v16(void *pObj, unsigned int nLo, unsigned int nHi) {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::FindIndexMaybe(pObj, nLo, nHi); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectRegistryCtorViewMaybe::_v20() {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::SortAllMaybe(); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v20() {
    return (void *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::SortAllMaybe(); // idiom-exempt: one original address, two declared spellings; port-only
}

void *EffectRegistryCtorViewMaybe::_v10(unsigned int nIndex, void *pItem) {
    return ((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::SetAtMaybe(nIndex, (DecorActorBase *)pItem); // idiom-exempt: one original address, two declared spellings; port-only
}
void *EffectGhostRegistryCtorViewMaybe::_v10(unsigned int nIndex, void *pItem) {
    return ((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::SetAtMaybe(nIndex, (DecorActorBase *)pItem); // idiom-exempt: one original address, two declared spellings; port-only
}
#endif // LOCO_PORT
