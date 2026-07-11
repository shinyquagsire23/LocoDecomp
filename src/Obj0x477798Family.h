// The "Obj0x477798 4-field collection family" (docs/subsystems.md: 8 vtable siblings --
// 0x477798, 0x477758, 0x477bd0, 0x477b78, 0x477b40, 0x477ae8, 0x478070, 0x477fe0 -- all
// shaped vtbl+0x0, m_ptr+0x4, m_count+0x8, m_0c+0xc, 0x10 bytes, sharing a reserve helper
// FUN_00435d10). A generic pointer-array collection base; embedded by NetSessionEventQueue
// and BigObjTrackingSetsMaybe. The real per-sibling class names are not yet identified --
// structs keep the project's Obj0x<vtable-addr> convention. Obj0x477758 (the 6th sibling)
// is already modeled as a real base/derived pair below; the 4 remaining flat siblings are what is
// left of the probe5 dtor five-pack (moved out 2026-07-22, v322; one retired v486). They are
// written as non-virtual dtors with a manual `void **vtbl` field -- a form that byte-matches while
// being the WRONG model, see the ⚠⚠ block immediately below.
#pragma once

// ⚠⚠ THE FOUR STRUCTS BELOW ARE NOT FOUR CLASSES -- and until v486 there were FIVE of them.
// Established v485, first one acted on v486. Each is the DERIVED DESTRUCTOR of one of the family's
// derived instantiations, named after the BASE vtable it re-stamps on the way out. That is why the
// "flattened form with a manual `void **vtbl` field" byte-matches: the body really is "zero my own
// live count, re-stamp the base table, then run the base destructor inline" -- three of those four
// statements being compiler-generated epilogue rather than anything a programmer wrote.
//
// The proof is the scalar deleting destructors: each derived destructor is called by EXACTLY ONE
// `??_G` thunk (0x4125c0 -> 0x412410, 0x424a70 -> 0x424460, 0x424ad0 -> 0x424a00,
// 0x436360 -> 0x435ca0, 0x4363c0 -> 0x436280) plus a handful of SEH Unwind@ funclets, and each
// thunk sits at slot 1 of the matching DERIVED vtable while the BASE vtable's slot 1 holds a
// SECOND, 62-byte thunk that INLINES the base destructor instead of calling anything (0x412580,
// 0x424a30, 0x424a90, 0x436320, 0x436380). A standalone class would not produce that pairing; a
// base/derived pair produces exactly it.
//
// ✅ RETIRED v486: `Obj0x477798`, which was `Obj0x477758::~Obj0x477758` (0x412410) all along -- the
// destructor now declared and DEFINED IN-CLASS on the derived half below. All four of that
// instantiation's affected addresses byte-match under the real model, and the two `??_G` thunks
// (0x412580 base / 0x4125c0 derived) are newly claimed, in src/PlacementCursorMaybe.cpp.
// ⛔ STILL OPEN: the four below, the derived destructors of the four registry-family
// instantiations. Retiring them is NOT the same mechanical edit -- see the ⛔ note at the foot of
// src/Obj0x477798Family.cpp for the hierarchy problem that blocks them.
struct Obj0x477bd0 { void **vtbl; void *m_ptr; int m_count; int m_0c; ~Obj0x477bd0(); };
struct Obj0x477b40 { void **vtbl; void *m_ptr; int m_count; int m_0c; ~Obj0x477b40(); };
struct Obj0x478070 { void **vtbl; void *m_ptr; int m_count; int m_0c; ~Obj0x478070(); };
struct Obj0x477fe0 { void **vtbl; void *m_ptr; int m_count; int m_0c; ~Obj0x477fe0(); };

// Relocated here 2026-07-25 from src/NetSessionEventQueue.h (a pure text move -- that
// header now includes this one from the position the definitions used to occupy, so its
// preprocessed output is unchanged). The move was forced by a SECOND consumer:
// PlacementCursorMaybe embeds an Obj0x477758 at +0x10c, and its shared header cannot pull
// in the whole NetSessionEventQueue singleton without colliding with the local partial
// views of that singleton other TUs still carry.

// Base half of the pEvents record object -- the family's shared "generic reserve" vtable
// (0x477798) belongs to this base; the derived Obj0x477758 (vtable 0x477758) adds the live
// count (m_0c) and is the table the registry calls actually dispatch through.
// This instantiation's element type (src/TilePlacedObj.h). Only forward-declared: the two
// members that actually name T live in src/Obj0x477798Family.cpp, so neither consumer of
// this header (NetSessionEventQueue, PlacementCursorMaybe) has to pull the world-object
// family in. Both of them reach their elements through the untyped GetAt/Add pair instead,
// which is why those two keep `void *`.
class TilePlacedObj;

// The capacity ramp all three growth sites share. The NEGATIVE factor is the original's, not
// a transcription artifact: every site computes `1 - (int)(n * -1.1)` from the single `fmul
// qword ds:0x477838` (-1.1) constant, i.e. one multiply and a `1 - x`, where the arithmetically
// identical `1 + (int)(n * 1.1)` would have needed an `inc`. Truncation toward zero makes the
// two spellings agree exactly for every non-negative n. Lives here rather than in the .cpp
// because Add() below is in-class, and its callers expand the ramp inline (v431).
static const double GROWTH_FACTOR_MAYBE = -1.1;

struct Obj0x477758Base {
    TilePlacedObj **m_ptr;
    int m_count;

    // Empties the two fields, then hands the whole allocate/zero/count job to the family's
    // shared reserve helper. The call is devirtualized (a plain `call 0x435d10`) because the
    // dynamic type is fixed inside a constructor. Defined in-class (not out-of-line) so it
    // always inlines at its call sites -- its own address never exists standalone in the
    // original binary. Pinned 2026-07-26 (v431) from TrackGraph::BuildAdjacencyAMaybe's own
    // expansion at 0x45ce60, which keeps the call: `mov [esp+0x2c],0x477798` (base vtable),
    // `mov [esp+0x34],ebx` (m_count = 0), `mov [esp+0x30],ebx` (m_ptr = 0), `call 0x435d10`.
    // src/PlacementCursorMaybe.cpp's site is a SECOND expansion of this same source in which
    // VC5 additionally inlined the helper itself (hence the `rep stosd` + `neg/sbb/and`
    // null-collapse there, which the header used to spell out literally).
    Obj0x477758Base(int nCapacity) {
        m_count = 0;
        m_ptr = 0;
        ReserveMaybe(nCapacity);
    }
    // slot 0 (+0x00) -- 0x435d10, the family's shared generic reserve/regrow. NOT the
    // destructor: the vtable dump at 0x477758/0x477798 puts 0x435d10 first and the scalar
    // deleting dtor (0x4125c0 / 0x412580) SECOND, so the class declares this virtual BEFORE
    // its dtor. Corrected 2026-07-25 (v408) -- the model carried the dtor at slot 0 until the
    // slot-10/13 bodies below needed to CALL this one through vtbl+0. Trims the capacity
    // request down past any trailing null slots first, then allocates/zeroes/copies/frees.
    virtual void ReserveMaybe(unsigned int nCapacity);
    virtual ~Obj0x477758Base() { // slot 1 -- the dtor unwind re-stamps this base table
        m_count = 0;
        if (m_ptr != 0) {
            operator delete(m_ptr);
        }
        m_ptr = 0;
    }
    // slot 2 (+0x08) -- 0x424020. Hand the whole array back. Reserve(0) is the family's own
    // "free everything" case (a zero capacity also nulls m_ptr), so there is nothing else to do.
    virtual void ReleaseStorage();
    // slot 3 (+0x0c) -- base 0x4356b0. Vacate slot idx and hand back whatever occupied it,
    // WITHOUT destroying it -- slot 4 is the deleting form. The CAPACITY bounds it, not the
    // live count, so a base-side remove can reach a slot past the live end. The derived
    // replaces this with the tail-shifting 0x4241e0.
    virtual TilePlacedObj *RemoveAt(unsigned int idx);
    // slot 4 (+0x10) -- 0x4356e0, and the one slot every table in the family shares outright:
    // all four registry tables and both Obj0x477758 tables name this same address, because it
    // only ever composes slot 3 with `delete`, and the delete goes through T's own vtable.
    virtual void RemoveAndDeleteAt(unsigned int idx);
    // slot 5 (+0x14) -- base 0x4244f0. Drop every reference the array holds without destroying
    // anything; the base form nulls the whole CAPACITY, the derived (0x424250) instead peels
    // the live range off the end through slot 3.
    virtual void RemoveAll();
    // slot 6 (+0x18) -- base 0x424510. The destroying form of slot 5, one RemoveAndDeleteAt
    // per slot. Derived: 0x424270, the same live-range-off-the-end shape.
    virtual void RemoveAndDeleteAll();
    // slot 7 (+0x1c) -- 0x424530, the bounds-checked raw accessor slot 8 forwards to. Checks
    // against the CAPACITY, so it can hand back a slot beyond the live end (which is exactly
    // what makes RemoveAt's own null return mean "out of range" and "empty" at once).
    virtual TilePlacedObj *GetAtMaybe(unsigned int idx);
    virtual void *GetAt(int idx);           // slot 8 (+0x20) -- 0x424030
    // slot 9 (+0x24) -- 0x412140. Heap-copies `src` and hands the copy to SetAtMaybe. The one
    // member that pins the family as a TEMPLATE over the element type: `docs/subsystems.md`
    // records 4 near-identical per-.obj twins of 0x412140, and the T-INDEPENDENT slots of all
    // 8 sibling vtables share single addresses in the 0x424xxx/0x435xxx range (one shared copy of each,
    // identical instantiations) while exactly the slots that name T -- 9/10/13/14 and the
    // deleting dtor -- stayed distinct per instantiation. This instantiation's T is
    // TilePlacedObj (`new` asks for its exact 0x10c, and the copy stamps its three vtables).
    virtual void SetCopyAtMaybe(unsigned int idx, const TilePlacedObj &src);
    // slot 10 (+0x28) -- base 0x4123a0. Grows to fit, deletes whatever already occupies the
    // slot, stores pItem there and hands it back. Overridden by the derived (0x4124b0), which
    // only adds an `idx > m_0c` reject in front of the identical body.
    virtual TilePlacedObj *SetAtMaybe(unsigned int idx, TilePlacedObj *pItem);
    virtual unsigned int Count();           // slot 11 (+0x2c) -- base 0x424010
    // slot 12 (+0x30) -- 0x424760, shared base and derived. "Is slot idx in range AND
    // non-null". Returns an int 1/0, not a byte -- the original collapses both exits through
    // eax (`mov eax,1` / `xor eax,eax`), where a char return would have used al.
    virtual int IsSlotOccupiedMaybe(unsigned int idx);
    // slot 13 (+0x34) -- NULL in the base's own vtable (no base implementation exists);
    // the derived supplies 0x412440.
    virtual int Add(void *item);
};

struct Obj0x477758 : Obj0x477758Base {
    // UNSIGNED, and every consumer can see it: Count() hands it back unsigned (v407 pinned
    // that from a CALLER's `jbe`), Add's own `m_0c >= m_count` grow guard compiles to an
    // unsigned `jb` even though m_count is signed, and Add/SetCopyAtMaybe feed it to the
    // `fild qword` zero-extending unsigned->double growth ramp rather than a plain `fild dword`.
    unsigned int m_0c;

    Obj0x477758(int nCapacity) : Obj0x477758Base(nCapacity) {
        m_0c = 0;
    }
    // slot 1 -- 0x412410. All this body does is zero the live count; the re-stamp of the BASE
    // table at 0x477798 and the array teardown after it are the compiler's own epilogue, running
    // ~Obj0x477758Base inline. That is the whole of the original's 48-byte 0x412410, and it is why
    // the family carried a bogus flat `Obj0x477798` struct for thirteen sessions (see the
    // retirement note above) -- the "manual vtbl field" that struct needed to byte-match was
    // really just the compiler's own base re-stamp.
    //
    // ⚠ DEFINED IN-CLASS, and that is load-bearing in BOTH directions -- v486 measured both:
    //   * inline is REQUIRED by the two outer destructors that embed this class.
    //     ~NetSessionEventQueue (0x41d2d0, 55 B) and ~PlacementCursorMaybe (0x410680, 116 B) are
    //     each nothing BUT this chain expanded in place; moving the definition out-of-line turned
    //     them into 14 B and a `call`, costing 171 B across two TUs.
    //   * the out-of-line copy at 0x412410 exists ANYWAY, because a virtual function always needs
    //     one for the vtable slot -- exactly the arrangement `?Add@Obj0x477758` at 0x412440
    //     already documents. Its marker is therefore a hint-only marker in
    //     src/PlacementCursorMaybe.cpp, with no source line of its own.
    // The apparent paradox -- VC5 inlines this into the outer destructors but the derived `??_G`
    // thunk at 0x4125c0 `call`s it -- is not a paradox: `??_G` is a synthesized thunk and VC5
    // declines to inline into it here, where it does inline ~Obj0x477758Base into the BASE half's
    // thunk at 0x412580. Both thunks byte-match under exactly this shape.
    ~Obj0x477758() {
        m_0c = 0;
    }
    // Genuine virtual dispatch in the original (indirect call through vtbl, not a direct
    // call). The slots the derived REPLACES: Count() = vtbl+0x2c (0x424000: `mov eax,[ecx+0xc];
    // ret`, returns the live count m_0c, where the base's own 0x424010 returns the capacity);
    // RemoveAt = vtbl+0xc (0x4241e0: bounds-checks idx via vtbl+0x1c, shifts the tail down via
    // FUN_00466ea0/memmove, decrements the live count); SetAtMaybe = vtbl+0x28 (0x4124b0);
    // Add = vtbl+0x34 (0x412440, NULL in the base). Slots 5/6 are replaced too
    // (0x424250/0x424270 over the base's 0x4244f0/0x424510) but neither pair is read yet.
    // Inherited untouched: GetAt (vtbl+0x20, 0x424030, forwards to vtbl+0x1c/0x424530 which
    // bounds-checks idx against the CAPACITY m_count then returns m_ptr[idx]), SetCopyAtMaybe,
    // ReserveMaybe and the dtor slot's body.
    // Only overrides that have BODIES in this project are re-declared. RemoveAt (0x4241e0) is
    // replaced in the real vtable too, but its body is claimed by PlacedObjRegistryMaybe (the
    // sorted instantiation reaches that same shared address), so re-declaring it here would
    // buy nothing -- the slot index, and so every call site's codegen, comes from the base --
    // while every spare declaration is a live codegen risk: this class is embedded by both
    // NetSessionEventQueue and PlacementCursorMaybe, so its declaration set reaches a wide
    // fan-out of TUs. ⚠ That risk is measured, not theoretical, and it is the PARAMETER budget
    // rather than the declaration count that this header's consumers are sensitive to --
    // see the v485 note in docs/CODEGEN.md.
    virtual TilePlacedObj *SetAtMaybe(unsigned int idx, TilePlacedObj *pItem);
    // slot 3 (+0x0c) -- 0x4241e0, over the base's non-shifting 0x4356b0. A REAL override (the
    // derived table names a different address here), and it is declared even though its body is
    // claimed elsewhere -- src/Obj0x477798Family.cpp's PlacedObjRegistryMaybe::RemoveAtShiftingTail
    // reaches the same shared address. The paragraph above used to say re-declaring it "would buy
    // nothing"; that was measured against byte-match output alone. It buys the DECLARATION-COUNT
    // dial: v486 retired the bogus `Obj0x477798` struct from this header, and the two declarations
    // that went with it cost src/WorldBoardMaybe.cpp its 951 B canary. Restoring the count with a
    // declaration that is TRUE is strictly better than keeping a struct that was false.
    virtual TilePlacedObj *RemoveAt(unsigned int idx);
    // slot 5 (+0x14) -- 0x424250, over the base's 0x4244f0. The derived cannot just null the
    // array the way the base does: its own live count has to come down with it, and only slot 3
    // knows how to do that, so it peels the LIVE range off the end one index at a time.
    virtual void RemoveAll();
    // slot 6 (+0x18) -- 0x424270, over the base's 0x424510. The destroying twin of slot 5,
    // driving slot 4 instead of slot 3 and otherwise identical to it.
    virtual void RemoveAndDeleteAll();
    // slot 11 (+0x2c) -- 0x424000, over the base's 0x424010. The LIVE count, where the base
    // hands back the capacity. This one pair of four-byte functions is the clearest statement
    // in the family of what the two counts are for.
    virtual unsigned int Count();
    // slot 13 (+0x34) -- 0x412440, NULL in the base's own vtable. Appends at the live end,
    // rolling the count back and reporting -1 if the store is rejected. Defined IN-CLASS
    // (v431) because the original inlines it at its call sites: TrackGraph::BuildAdjacencyA
    // Maybe expands the whole body at 0x45cecf -- the `fild qword`/`fmul` growth ramp, the
    // `call [vtbl+0]` reserve and the `call [vtbl+0x28]` SetAtMaybe all appear in line. Its
    // out-of-line copy still exists standalone (it is a vtable slot); the marker for that
    // copy stays in src/Obj0x477798Family.cpp.
    virtual int Add(void *item) {
        if (m_0c >= (unsigned int)m_count) {
            ReserveMaybe(1 - (int)(m_0c * GROWTH_FACTOR_MAYBE));
        }
        m_0c++;
        if (SetAtMaybe(m_0c - 1, (TilePlacedObj *)item) != 0) {
            return m_0c - 1;
        }
        m_0c--;
        return -1;
    }
    // slot 14 (+0x38) -- 0x412540, the derived's own added slot (the base vtable ends at
    // slot 13; TilePlacedObj's own vtable starts at the very next dword, 0x4777d0). Linear
    // scan of the LIVE range for pItem, returning its index or -1.
    virtual int FindIndexMaybe(TilePlacedObj *pItem);
    // slot 15 (+0x3c) -- NULL, and the LAST slot of this table: 0x477758 + 16*4 is exactly
    // 0x477798, where the base's own table starts, and the dword at 0x477794 is zero. So the
    // derived table is SIXTEEN slots, not the fifteen this header modelled before v486, and the
    // sixteenth is a virtual that is DECLARED but never DEFINED and never called -- the same
    // shape as the base's own slot 13 (`Add`, NULL in 0x477798 and supplied only by this class).
    //
    // ⚠ DELIBERATELY LEFT AS A COMMENT rather than declared as `virtual void *_v15();`. It is
    // true, and declaring it costs nothing on its own -- v486 did declare it for one commit, to
    // pay back the declaration the retired `Obj0x477798` struct took with it. But this header's
    // declaration count and src/WidgetBase.h's share a PARITY that
    // WorldBoardMaybe::FindNearestObjOfCategoryMaybe (0x457ce0, 951 B) answers to, and landing
    // AnimDescRefObj0x477488::RepositionWithHotspot pays that same debt for a great deal more
    // (+302 B). Two true declarations, one parity slot: the more valuable one wins and the other
    // stays documented here. See docs/CODEGEN.md's v486 note.
    //
    // ⚠ RE-TESTED v537 and it is NOT a substitute for a src/WidgetBase.h declaration -- the v486
    // note here used to say "if a future session removes that WidgetBase.h declaration, declare
    // this one again in the same commit", and v537 did exactly that and MEASURED it: declaring
    // `_v15` restores this TU (+152) and costs src/RoadVehicleActor.cpp -504, while leaving
    // 0x457ce0's -951 completely untouched. The two headers are NOT interchangeable currencies
    // for the same victim; 0x457ce0 answers to src/WidgetBase.h's own count. Corrected in place.
};
