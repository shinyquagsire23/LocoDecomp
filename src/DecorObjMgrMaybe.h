// DecorObjMgrMaybe (DAT_00485448) -- the ambient-world-actor manager singleton: it owns the two
// per-category registries the WalkerActor (category 7) and RoadVehicleActor (category 8)
// instances live in, ticks them, and paints them. See docs/subsystems.md's DecorObjMgrMaybe
// entry and src/DecorActor.h for the actors themselves.
//
// ONE canonical shared partial view (CLAUDE.md's "never duplicate a struct across TUs" rule);
// the full class is 124/0x7c bytes and its field map matches Ghidra's own struct 1:1.
//
// ⚠ ONE older, METHOD-ONLY view of this same singleton is left, in src/Main.cpp
// (DecorObjMgrWndProcView0x4618c0). It declares no data members -- the established "cross-TU
// callee declaration" pattern, not a divergent layout model -- so it does not conflict with
// this header. It is still a real defect though, not just cosmetic debt: a view spelling
// mangles under the VIEW's class name, so its call resolves to a symbol defined nowhere (a
// generated `xor eax,eax; ret N` stub in the port). See CODEGEN #184 and tools/lint_desync.py's
// VIEW findings, which is the worklist.
// src/WorldBoardMaybe.cpp's sibling DecorObjMgrPaintView0x456700 was retired in v564 by simply
// including this header there, and it measured FREE -- despite a v552 measurement (recorded in
// that TU as "REFUTED, do NOT re-run") that had priced the very same fold at 951 B. Re-measure
// before believing an inherited price; adding method decls to a header a TU already includes
// has rotated /Og state before (the v325/v334 bisects), but "has" is not "does".
#pragma once

#include "LockableMaybe.h"

class DecorActorBase; // src/DecorActor.h
class TilePlacedObj;  // src/TilePlacedObj.h
struct BigObjSeqRecordMaybe; // src/BigObjSeqRecordMaybe.h

// The growable, SORTED pointer array each category's live actors are registered in, modeled as
// what it is: a BASE/DERIVED pair, split out of one flattened struct 2026-07-28 (v485). Until
// then this project modeled only the DERIVED table for both registries and carried the base half
// as two flat 4-field structs in src/Obj0x477798Family.h that existed purely to home their
// destructors -- three overlapping partial models of two classes, which is why the base-side
// slot-10 bodies had nowhere to live. Reached exclusively through the vtables, so the slots are
// what is modeled. The manual `void *pVtblMaybe` this class carried until 2026-07-26 is now the
// real C++ vptr -- the layout is unchanged, since that field WAS the vptr.
//
// This is the SAME class as src/Obj0x477798Family.h's Obj0x477758Base/Obj0x477758 pair over a
// different element type -- slot for slot, field for field. DecorObjMgrMaybe's ctor (0x434500)
// proves the split: for each registry it stamps the BASE vtable, zeroes +8/+4, calls the family
// reserve (0x435d10) for 100 slots, THEN stamps the derived table and zeroes +0x10/+0x14. The
// dtor at 0x4345f0 re-stamps the two BASE tables on the way out, which is why a naive "find the
// vptr store" search finds only the base addresses.
//
// ⚠ THE MECHANISM, corrected v485 -- earlier notes here and in src/Obj0x477798Family.h called
// the shared slots "ICF-folded", and that is refuted. If the linker were folding identical
// COMDATs, the two categories' slot-10 bodies (0x4359a0 / 0x436040) and slot-17 bodies
// (0x435b60 / 0x436140) would have folded too: each pair is byte-identical apart from its own
// branch targets, down to naming the same growth constant. They did not. So the slots that have
// ONE address have one because only ONE copy was ever compiled -- an ordinary, non-template base
// class -- and the per-category duplicates are members of a per-element-type subclass that was
// genuinely emitted twice. Exactly three slots are duplicated on the base side (1, 9, 10) and
// two more on the derived side (17, 18): the destructor, "copy-construct an element", "store an
// element destroying the old", "insert" and "compare". That is the classic pre-STL hand-rolled
// typed-collection pattern, not a template.
//
// The FAMILY BASE half: vtable 0x478070 for category 7 and 0x477fe0 for category 8, 14 slots
// each (slot 13 is NULL in both -- the base has no "add"), over the four fields at +0..+0xc.
// Only the slots that genuinely name the element type get bodies of their own here: 9 and 10.
struct PlacedObjCollectionMaybe {
    // Empties the two fields, reserves nCapacity slots through the family's shared reserve
    // (devirtualized inside the ctor -- a plain `call 0x435d10`), then zeroes the live count.
    // Defined in-class so it inlines into DecorObjMgrMaybe's ctor (0x434500), the only
    // construction site; the store ORDER (capacity, array, reserve, count) is the original's.
    // Same source shape as Obj0x477758Base's own param ctor (v431).
    PlacedObjCollectionMaybe(int nCapacity) {
        nCapacityMaybe = 0;
        pArrayMaybe = 0;
        ReserveMaybe(nCapacity);
        nCountMaybe = 0;
    }
    // +0x4 -- the entry array itself. Typed rather than left `void *` so the one consumer
    // that bypasses the vtable (SetHoverObjMaybe's inlined remove-at) can index it by name
    // instead of doing raw byte math on it; layout is unchanged either way.
    DecorActorBase **pArrayMaybe;
    // +0x8 -- the ALLOCATED slot count, the family base's own m_count (src/Obj0x477798Family.h's
    // Obj0x477758Base::m_count sits at this same +0x8); nCountMaybe at +0xc is the LIVE count.
    unsigned int nCapacityMaybe;
    unsigned int nCountMaybe; // +0xc

    // slot 0 (+0x00) -- 0x435d10, the family's shared generic reserve/regrow, the same slot and
    // shape as Obj0x477758Base::ReserveMaybe. Reached by SetAtMaybe below.
    virtual void ReserveMaybe(unsigned int nCapacity);
    // slot 1 (+0x04) -- the dtor, per-instantiation like slots 9/10 (0x435ca0 re-stamps 0x478070
    // for category 7, 0x436280 re-stamps 0x477fe0 for category 8 -- was `_v01` until v518; the
    // OUT-OF-LINE bodies are still carried by the Obj0x477798Family.cpp leaves). Defined
    // in-class since v519 so it inlines into DecorObjMgrMaybe's dtor (0x4345f0), whose original
    // runs exactly this teardown on each registry member; the derived half declares no dtor of
    // its own, and its implicit one's vptr store is dead-store-eliminated against this class's,
    // which is why the original's dtor shows only the BASE vtable stores. The non-trivial dtor
    // is also load-bearing for BOTH manager special members' /GX unwind states (the ctor's
    // four state stores at 0x434500, the dtor's three at 0x4345f0). The pArray-hoist local is
    // the shape Ghidra's own decompile shows (puVar1); the store order (count, capacity,
    // delete, null) is the original's.
    // **Measured and REJECTED (v519) -- do NOT re-run:** writing this teardown on the DERIVED
    // registry class instead (with an empty virtual dtor here) costs +36 B and DIFF(135): a
    // user dtor at that level adds a SECOND /GX state per registry member, its own vptr store
    // does NOT dead-store-eliminate, and the frame grows a saved-EDI + two lea.reloads.
    virtual ~PlacedObjCollectionMaybe() {
        DecorActorBase **pArray = pArrayMaybe;
        nCountMaybe = 0;
        nCapacityMaybe = 0;
        if (pArray != 0) operator delete(pArray);
        pArrayMaybe = 0;
    }
    virtual void *_v02();
    // slot 3 (+0xc) -- base 0x4356b0, which does NOT shift: it vacates the slot and hands the
    // occupant back, and Obj0x477758Base::RemoveAt owns that address and its marker. The name
    // here is the SLOT's, taken from the derived override (0x4241e0) that this project's
    // consumers actually reach and that does shift the tail down.
    virtual void *RemoveAtShiftingTail(unsigned int nIndex);
    virtual void *_v04(); virtual void *_v05(); virtual void *_v06();
    // slot 7 (+0x1c) -- 0x424530, the raw "item at index i" accessor (slot 8 / +0x20 is the
    // bounds-checked wrapper over it, per the Obj0x477798-family note in
    // src/Obj0x477798Family.h).
    virtual void *GetAtMaybe(unsigned int nIndex);
    // slot 8 (+0x20) -- 0x424030, the bounds-checked wrapper over slot 7, matching the
    // Obj0x477798-family's own `GetAt` at the same slot. This is the accessor
    // DecorObjMgrMaybe::TickObjSeqGoalsMaybe's registry walk uses.
    virtual void *GetAt(int nIndex);
    // slot 9 (+0x24) -- the family's SetCopyAtMaybe (src/Obj0x477798Family.h's Obj0x477758Base
    // declares the same slot for its own T). This is the ONE slot in this family that genuinely
    // names the element type, so it is also the one slot where "one struct models both
    // registries" breaks down: the two instantiations have DIFFERENT bodies, 0x435700
    // (T = WalkerActor) and 0x435db0 (T = RoadVehicleActor), where the nine T-independent slots
    // are a single address shared by both tables. The two bodies are therefore
    // defined on per-instantiation leaves in src/Obj0x477798Family.cpp; this declaration is the
    // shared shape they override.
    virtual void SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src);
    // slot 10 (+0x28) -- store pItem at nIndex (growing to fit and destroying whatever occupied
    // the slot) and hand it back; the same slot and shape as Obj0x477758Base::SetAtMaybe.
    // Per-instantiation -- 0x4359a0 (category 7) and 0x436040 (category 8) on THIS table,
    // 0x435a10 / 0x4360b0 on the derived one -- even though the two categories' bodies are
    // byte-identical apart from their own branch targets, right down to naming the same
    // 0x477838 growth constant. See the leaf note in src/Obj0x477798Family.cpp.
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
    // slot 11 (+0x2c) -- base 0x424010, the CAPACITY; the derived replaces it with 0x424000,
    // the LIVE count, and that is the one every consumer here means. UNSIGNED either way:
    // TickObjSeqGoalsMaybe's `i < Count()` loop guard compiles to an unsigned `jb`.
    virtual unsigned int Count();
    // slot 12 (+0x30) -- 0x424760, "is index i occupied": the linear insertion-point scan's own
    // loop guard, tested before every GetAtMaybe. Shared base and derived.
    virtual char IsSlotOccupiedMaybe(unsigned int nIndex);
    // slot 13 (+0x34) -- NULL in both base tables; the derived supplies 0x4362b0.
    virtual void InsertInSortedPositionMaybe(void *pObj);
};

// The SORTED derived half: vtable 0x478018 over 0x478070 for category 7, 0x477f88 over 0x477fe0
// for category 8. 22 slots -- it replaces seven of the base's and adds slots 14..21 -- plus the
// two fields at +0x10/+0x14. DecorObjMgrMaybe's ctor (0x434500) proves the whole split: for each
// registry it stamps the BASE vtable, zeroes +8/+4, calls the family reserve (0x435d10) for 100
// slots, THEN stamps the derived vtable and zeroes +0x10/+0x14.
struct PlacedObjRegistryMaybe : PlacedObjCollectionMaybe {
    // Zeroes the sort key after the base's reserve; the store order (+0x10 then +0x14) is the
    // original's (0x434500). In-class so it inlines into DecorObjMgrMaybe's ctor with the base.
    PlacedObjRegistryMaybe(int nCapacity) : PlacedObjCollectionMaybe(nCapacity) {
        nSortKeyOffsetMaybe = 0;
        nSortKeyTypeMaybe = 0;
    }
    // +0x10 / +0x14 -- the SORT KEY, the pair SetSortParamsAndSortMaybe (slot 19) stashes.
    // Pinned 2026-07-27 (v446) from the two per-instantiation CompareEntriesMaybe bodies
    // (0x435c00 / 0x4361e0), which read them as `key = *(entry + nSortKeyOffsetMaybe)` under a
    // switch on nSortKeyTypeMaybe: -4/-3 = a 4-byte int, -2 = a signed short, -1 = an unsigned
    // short, and any positive N = an N-byte memcmp; ties fall back to comparing the two entry
    // POINTERS, so the order is total. Type code 0 means "no key configured", which is what
    // InsertInSortedPositionMaybe tests to skip its scan and append at the live end instead.
    // (Both fields were misnamed until v446 -- +0x14 was called nCapacityMaybe, which is what
    // +0x8 actually is.)
    unsigned int nSortKeyOffsetMaybe; // +0x10
    unsigned int nSortKeyTypeMaybe;   // +0x14

    // The slots the derived REPLACES, and the only three of them this project needs a name for
    // here. Slot 3 (+0xc) -- 0x4241e0, the tail-shifting remove-at: GetAtMaybe(nIndex) first and
    // bail on a null, else memmove the tail down over it, NULL the vacated last slot and
    // decrement nCountMaybe. RETURNS the element it removed, which is what DeregisterEntryMaybe
    // compares against its own argument. (Spelled longer than the Obj0x477758Base sibling
    // because ghidra-mcp's global-name uniqueness guard rejects a bare `RemoveAt`, and the two
    // sides are kept identical on purpose.)
    virtual void *RemoveAtShiftingTail(unsigned int nIndex);
    // Slot 10 (+0x28) -- 0x435a10 / 0x4360b0, the base's body behind one extra `nIndex >
    // nCountMaybe` reject. Per-instantiation, so the two bodies live on the leaves in
    // src/Obj0x477798Family.cpp; this declaration is the shared shape they override.
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
    // Slots 5, 6 and 11 are replaced too (0x424250 / 0x424270 / 0x424000 over the base's
    // 0x4244f0 / 0x424510 / 0x424010), and all three bodies are the shared copy owned by the
    // Obj0x477758 instantiation, where their markers are.
    // slot 13 (+0x34) -- 0x4362b0, "add pObj in sorted position": linear-scans from 0 with
    // IsSlotOccupiedMaybe/GetAtMaybe/CompareEntriesMaybe for the insertion point, then
    // InsertAtMaybe. This is the same three-slot walk PlacementCursorMaybe::ReleaseHoverObjMaybe
    // (0x411580) open-codes inline; SpawnActorForKindMaybe just calls it.
    virtual void InsertInSortedPositionMaybe(void *pObj);
    // slot 14 (+0x38) -- 0x4244b0, the whole-registry convenience over slot 16: -1 when the
    // registry is empty, else FindIndexMaybe(pObj, 0, nCountMaybe - 1).
    virtual int IndexOfMaybe(void *pObj);
    // slot 15 (+0x3c) -- 0x435aa0, an in-place quicksort of the INCLUSIVE index range
    // [nLo, nHi]: pivot = the entry at the midpoint, partition with CompareEntriesMaybe
    // (slot 18) and recurse. Shared by both instantiations. Reached through SortAllMaybe.
    virtual void SortRangeMaybe(int nLo, int nHi);
    // slot 16 (+0x40) -- binary search for pObj between the two inclusive index bounds;
    // returns its index, or -1 when it is not registered.
    virtual int FindIndexMaybe(void *pObj, unsigned int nLo, unsigned int nHi);
    // slot 17 (+0x44) -- 0x435b60 / 0x436140, insert pObj at index i, shifting the tail up.
    // RETURNS the index it inserted at, or -1 when i is past the live end. Per-instantiation
    // (see the leaf note in src/Obj0x477798Family.cpp), and the one growth site in this family
    // that ramps off a POSITIVE 1.1: `Reserve((int)(nCountMaybe * 1.1))` against the
    // `1 - (int)(n * -1.1)` every SetAtMaybe/Add site uses. The two constants are separate
    // .rdata doubles -- 0x4780b0 (+1.1) here, 0x477838 (-1.1) there -- so this is a real
    // difference in the original source, not a spelling of one ramp.
    virtual int InsertAtMaybe(unsigned int nIndex, void *pObj);
    // slot 18 (+0x48) -- the registry's own ordering predicate over two entries; > 0 means
    // pObj sorts after pOther, which is what walks the insertion-point scan forward.
    virtual int CompareEntriesMaybe(void *pObj, void *pOther);
    // slot 19 (+0x4c) -- 0x424490, stash the sort key (see the +0x10/+0x14 note above) and then
    // re-sort through slot 20. Always returns 0. Reached by nothing transcribed so far.
    virtual int SetSortParamsAndSortMaybe(unsigned int nKeyOffset, unsigned int nKeyType);
    // slot 20 (+0x50) -- 0x4244d0, re-sort the WHOLE registry (slot 15 over 0 .. nCountMaybe-1),
    // and only when more than one entry is registered. Always returns 0.
    virtual int SortAllMaybe();
    // slot 21 (+0x54) -- 0x435cd0, the registry's own "am I still in order" audit: walk the
    // adjacent pairs of the live range through slot 18 and stop at the first one that is not
    // strictly ascending. Shared by both instantiations. Nothing transcribed so far calls it,
    // which fits an assertion/debug-verify helper that survived into the release build.
    virtual char IsSortedMaybe();
#ifdef LOCO_PORT
    // PORT SCAFFOLDING ONLY -- preprocesses away for the match build, and an override reuses the
    // base's existing slot 11, so no vtable LAYOUT changes in either build.
    // The original genuinely has two addresses in this slot: base 0x424010 returns the CAPACITY,
    // this derived override 0x424000 returns the LIVE count, and every consumer here holds a
    // DERIVED registry (DecorObjMgrMaybe's ctor stamps the base vtable, reserves, then stamps
    // this one). Without the override the port's derived vtable would inherit the base spelling
    // and hand out the capacity where the original hands out the live count. Defined in
    // src/Obj0x477798Family.cpp's port block beside the other three forwarders.
    virtual unsigned int Count();
#endif
};

class DecorObjMgrMaybe {
public:
    // +0x0 -- this manager's OWN vtable pointer (0x477f70, 5 slots). Modeled as a plain
    // pointer rather than as C++ `virtual` methods because no consumer transcribed so far
    // dispatches through it; giving the class real virtuals would make the compiler want to
    // emit a vtable for it here.
    void *pVtblMaybe;
    LockableMaybe lockAMaybe;      // +0x4  -- guards regCategory7Maybe
    LockableMaybe lockBMaybe;      // +0x20 -- guards regCategory8Maybe
    int nActiveCategory7Maybe;     // +0x3c -- live WalkerActor count
    int nActiveCategory8Maybe;     // +0x40 -- live RoadVehicleActor count
    // +0x44 -- population-pressure throttle. While set, every actor's own TickMaybe takes the
    // cheap FollowLeaderStepMaybe path (chase the previous registry entry's published trail
    // anchor) instead of running its full destination/pathfinding logic. Cleared again by
    // TickCategory7And8Maybe once 300 ticks have passed since dwLastTickMaybe.
    bool bThrottleMaybe;
    unsigned char pad0x45[3];      // +0x45 .. +0x47, alignment
    int dwLastTickMaybe;           // +0x48 -- g_dwGameTick when bThrottleMaybe was last raised
    PlacedObjRegistryMaybe regCategory7Maybe; // +0x4c
    PlacedObjRegistryMaybe regCategory8Maybe; // +0x64

    // 0x434500, src/DecorActor.cpp -- the real constructor (SEH-framed; called from the CRT
    // init-term for the DAT_00485448 singleton). Constructs both registries for 100 slots, then
    // -- AFTER stamping its own vtable and zeroing the three scalars -- configures each
    // registry's sort key through a REAL virtual dispatch (slot 19): (0x7c, 10) for category 7
    // (a 10-byte memcmp), (0xc, -4) for category 8 (a 4-byte int).
    DecorObjMgrMaybe();

    // 0x4345f0, src/DecorActor.cpp -- the real destructor (SEH-framed, one /GX state per member
    // with a non-trivial dtor, run in reverse declaration order: regCategory8, regCategory7,
    // lockB, lockA). The body itself is only the manager-vtable re-stamp; each registry member
    // is torn down by the INLINED in-class ~PlacedObjCollectionMaybe (see the slot-1 note
    // above), the locks by the out-of-line LockableMaybe dtor (0x4493f0).
    ~DecorObjMgrMaybe();

    // 0x435200, src/DecorActor.cpp -- "what would I hit standing in rcNew". Walks BOTH
    // registries, skips pSelf and anything further than a sprite-height away in Y, intersects
    // rcNew with each candidate's rect and, on an overlap, runs a per-pixel mask test. Returns 0
    // for "clear", 7 for a collision with a category-7 (walker) actor and 8 for a category-8
    // (road vehicle) one -- i.e. the value IS the colliding actor's category, which is why
    // RoadVehicleActor::AdvanceMovementMaybe's special case tests for 7.
    int TestActorCollisionMaybe(RECT rcNew, DecorActorBase *pSelf);
    // 0x435020, src/DecorActor.cpp -- the pSelf == 0 tail call out of TestActorCollisionMaybe:
    // the same two registry walks and the same intersect + HasOpaquePixelInRect per candidate,
    // but with no self-exclusion, no Y-distance filter and no second (self-side) mask test.
    // Returns the same category codes, 7 or 8, or 0 for "clear". (An earlier note here called it
    // dead work that always returned 0 -- that was a Ghidra by-value-RECT framing artifact, now
    // retracted in docs/engine-bugs.md and disproved by this function being EXACT.)
    int TestRectAgainstAllActorsMaybe(RECT rcNew);
    // 0x434c50, src/DecorActor.cpp -- "did a click at screen (x, y) land on one of my actors":
    // the manager's own click handler, tried by PlacementCursorMaybe after every widget has
    // declined and before the board's own ResolveWorldClickMaybe fallback. Returns non-zero
    // when an actor consumed the click.
    char ResolveClickMaybe(int x, int y);
    // 0x434b60, src/DecorActor.cpp -- unregister pActor from whichever of the two category
    // registries its own kind descriptor's category byte selects (7 or 8), decrementing the
    // matching live count and destroying it. Silently returns for a null pActor or any other
    // category. bDeleteMaybe is the second, byte argument every known call site passes as 1;
    // it only gates the puff-of-smoke effect, never the delete.
    void DeregisterEntryMaybe(DecorActorBase *pActor, char bDeleteMaybe);
    // 0x434870, extern -- re-sort the category-7 registry (its slot-20 resort, under lockAMaybe)
    // but only once more than one walker is alive. Called whenever the walker population's sort
    // key can have changed: from DeregisterEntryMaybe, and from DecorActorBase's own ctor as soon
    // as the new actor turns out to carry a per-instance category name.
    void TickCategory7OnlyMaybe();
    // 0x434720, src/DecorActor.cpp -- the per-frame actor tick (FrameDriver): clears the
    // population-pressure throttle once 300 ticks have passed since dwLastTickMaybe, then
    // walks each category registry handing every entry its SUCCESSOR as the TickMaybe
    // argument (the final entry gets NULL from the past-the-end GetAt). Category-8 entries
    // are ticked only while bValid == 1. Ends with the category-8 registry's own
    // >= 2-gated resort under lockBMaybe -- TickCategory7OnlyMaybe's exact shape.
    void TickCategory7And8Maybe();
    // 0x4349d0, extern -- construct the right actor leaf for kindId (WalkerActor for category
    // 7, RoadVehicleActor for 8), owned by pOwner and placed at world (x, y), and register it
    // in the matching category registry. Returns 0 when the kind is unspawnable. The caller
    // holds that registry's own lock across the call. Not yet transcribed.
    DecorActorBase *SpawnActorForKindMaybe(int kindId, TilePlacedObj *pOwner, int x, int y);
    // 0x434d70, src/DecorActor.cpp -- one placed object's per-tick goal-rule pass. See
    // docs/subsystems.md's BigObjSeqRecordMaybe writeup: it counts the category-7 actors
    // standing inside pObj's own tile rect whose kind matches the MobileSeq / TotalVisits
    // record's first value, and fires the record's reward once a tally passes its threshold.
    // Returns the MobileSeq flag only (the TotalVisits one stays internal); the sole caller,
    // AppWndProc, ignores it.
    unsigned char TickObjSeqGoalsMaybe(TilePlacedObj *pObj);
    // 0x435580, src/DecorActor.cpp -- the ACTOR half of a fired goal reward, and the mirror
    // image of TilePlacedObj::ApplySeqRecordChangeMaybe: for every category-7 actor standing
    // inside rc whose kind matches pRec->paValues[0] (or -1, meaning any), apply the record's
    // SECOND, minifig-side descriptor id (+0x14/+0x18) and re-arm that actor's own
    // nAnimCooldownUntil from pRec->lUnk0x1cMaybe. Not yet transcribed.
    void ApplySeqRecordToActorsMaybe(RECT rc, BigObjSeqRecordMaybe *pRec);
    // 0x434690, extern -- but ONLY while the app is in screen state 3 (build mode): walk both
    // category registries and re-drive every actor's vtable slot 0x40 with its own cached
    // (+0xa8, +0xac) pair, i.e. put each one back where it was. Called from the build-mode
    // entry path (AppWindow_EnterBuildMode) after a board rebuild. Declared-only.
    void RestoreEntryPositionsMaybe();
    // 0x434970 -- the "give the unemployed a job" pass, and the immediate sibling of
    // RestoreEntryPositionsMaybe above in every one of its call sites. Same screen-state-3 gate,
    // but only ONE registry (category 7, the walkers -- vehicles have no workplace) and it
    // activates only the entries that need it: those with no pOwnerObjMaybe yet, whose kind
    // actually declares a bitmap footprint.
    // ⚠ Deliberately NOT declared here: transcribed in src/DecorActor.cpp through a TU-local
    // view. MEASURED 2026-08-01 -- declaring it on this class costs src/Obj0x477798Family.cpp
    // 152 B (one exact function lost, 40+7 -> 39+8). Hoist it here if that ever stops being true.
    // 0x4348a0, src/DecorActor.cpp -- paint every registered actor whose sprite intersects the
    // flushed dirty tile rect (pass 0 only): walk each category registry dispatching each
    // entry's own BlitAnimFrameMaybe (root vtable slot 11) with the rect by value, with a
    // painter's-order early-out on the Y-sorted category-8 registry. Also this manager's OWN
    // vtable slot 4 (0x477f80); the two direct callers are WorldBoardMaybe's dirty-tile
    // repaint (0x456700, through the DecorObjMgrPaintView0x456700 view as `FUN_004348a0`).
    void BlitActorsInRectMaybe(short iPlaneMaybe, RECT rect, int bFlag);
    // 0x434800, src/DecorActor.cpp (EXACT) -- dirty-mark EVERY registered actor: walk both
    // category registries and call each entry's own MarkDirty (the root's vtable slot 1, at
    // byte offset +4 -- the earlier note here called that "slot 4" from the byte offset). Body
    // read and named 2026-07-27 (v450), was FUN_00434800. Also DecorObjMgrMaybe's OWN vtable
    // slot 3 (0x477f7c); the two direct callers are AppWindow's EnterBuildMode and
    // SetScreenState, i.e. it exists to force a full repaint after a mode change. The int
    // argument really is unread by this body -- AppWindow's two call sites push 0 and 1
    // respectively, so it is presumably consumed further down some other dispatch path.
    void MarkAllEntriesDirtyMaybe(int bUnusedFlagMaybe);
    // 0x434970, body in src/DecorActor.cpp -- hand out workplaces: run the "activate" virtual
    // over every category-7 actor that has not got one yet.
    // ⚠ PRICE, v577: this declaration is measured at -152 B (src/Obj0x477798Family.cpp loses one
    // exact function, 40+7 -> 39+8). It is landed anyway. Until v577 the address had TWO
    // view-struct spellings -- src/DecorActor.cpp's DecorObjMgrActivateView0x434970, which
    // DEFINED it, and src/WorldActionCursor.cpp's DecorObjMgrIdlePumpView0x42cc60, which only
    // CALLED it -- so the world idle thread's call targeted a symbol no TU defines and ran a
    // gen_stubs stub in the port, i.e. loaded worlds never handed their actors a workplace.
    // Byte-invisible (relocations are masked and the match build never links).
    void ActivateEligibleEntriesMaybe();
};
extern DecorObjMgrMaybe DecorObjMgrMaybe_00485448; // DAT_00485448
