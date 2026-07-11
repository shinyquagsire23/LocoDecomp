// The Obj0x477798 4-field collection family's byte-matched destructors (moved out of
// src/phase2_probe5.cpp 2026-07-22, v322; four of the original five are left -- see the ⛔ note at
// the foot of this file), plus the element-type-dependent members of the Obj0x477758Base/
// Obj0x477758 pair (2026-07-25, v408). See the header for the family overview and for why those
// are the ones that name TilePlacedObj.
//
// Those members landed in the ORIGINAL's .obj for src/PlacementCursorMaybe.cpp -- they sit at
// 0x412140..0x412577, interleaved with that class's own methods. That is the per-instantiation
// signature: the members that name the element type get emitted into whichever .obj instantiated
// the class, while the type-independent ones exist as a single shared copy (NOT by folding -- see
// the mechanism note further down). They are grouped HERE rather than in PlacementCursorMaybe.cpp
// because they belong to this class, and because /Gy makes each one its own COMDAT -- their
// codegen does not depend on which TU emits them.
// ⚠ That freedom does NOT extend to a class's vtable, its `??_G` thunk, or the out-of-line copies
// its vtable points at: VC5 emits those only where the class is CONSTRUCTED. See the ⚠ block below.
#include <string.h>

#include "AnimEffectObj.h" // AnimEffectObj0x477a90 (EffectPlacedRegistryMaybe's T)
#include "DecorActor.h"
#include "DecorObjMgrMaybe.h"
#include "Obj0x477798Family.h"
#include "TilePlacedObj.h"

// One per still-flat sibling. `g_vtable0x477798` used to head this list; it went with the struct
// that referenced it (v486), because the base-table re-stamp it stood for turned out to be
// compiler-generated rather than a field assignment anyone wrote.
extern void *g_vtable0x477bd0[];
extern void *g_vtable0x477b40[];
extern void *g_vtable0x478070[];
extern void *g_vtable0x477fe0[];

// 5-member vtable-dtor family (found via find_leaves.py --max 60 --allow-calls, same
// shape family as TimeOfDayMaybe in phase2_probe.cpp but WITH an owned-pointer conditional
// delete): `this->m_0c = 0; this->vtbl = &<class>::vftable; this->m_count = 0;
// if (m_ptr) operator delete(m_ptr); m_ptr = 0;`. Statement order matters here: the
// vtbl store must come AFTER the first member zero (m_0c) in source for the scheduler to
// reproduce the original's early m_0c store -- this is a destructor-side counterpart to
// the ctor mem-initializer-list lesson already in CLAUDE.md (vtbl-store position is
// source-order-sensitive, not fixed-first the way real `virtual` dtors are). Named
// Obj0x<vtable-addr> per the project's existing vtable-identified-class convention
// (see TimeOfDayMaybe).

// The first of the five to be modelled properly (v486). What used to sit here as
// `Obj0x477798::~Obj0x477798` -- a flat struct with a hand-written `void **vtbl` field -- is
// really Obj0x477758's destructor, whose whole source body is `m_0c = 0`. Everything after that in
// the original's 0x412410 (the store of 0x477798 over the vtable pointer, `m_count = 0`, the
// guarded `operator delete(m_ptr)` and `m_ptr = 0`) is not source at all: it is the compiler's own
// re-stamp of the base table followed by ~Obj0x477758Base running inline. Which is exactly why the
// flat struct byte-matched for thirteen sessions while being the wrong model -- it was
// transcribing a compiler-generated epilogue as if it were a programmer's statements.
// The definition is IN-CLASS in the header (see the ⚠ note on it there for the measurements that
// forced that), so 0x412410's marker is a hint-only one in src/PlacementCursorMaybe.cpp beside the
// identically-shaped `?Add@Obj0x477758` -- this TU never constructs an Obj0x477758, so VC5 emits
// neither its vtable nor the out-of-line copies the vtable needs into this .obj.

// ⚠ NEITHER scalar deleting destructor is claimed here, and neither are the out-of-line copies of
// this pair's in-class members. VC5 emits a class's vtable, its `??_G` thunk and the standalone
// bodies the vtable points at ONLY into a TU that CONSTRUCTS the class -- not into the TU that
// defines its members. Nothing in this file constructs an Obj0x477758, so none of those COMDATs
// exist in this .obj at all. All four (0x412410 ~Obj0x477758, 0x412440 Add, 0x412580
// ??_GObj0x477758Base, 0x4125c0 ??_GObj0x477758) are claimed from src/PlacementCursorMaybe.cpp,
// which is also where the ORIGINAL's .obj put this whole 0x412xxx run.
// ⚠ Do not "fix" a marker placed here by chasing the DIFF it reports: with the COMDAT absent,
// tools/match.py falls through to marker-order pairing and every marker after it in the file
// silently shifts onto its neighbour's function. v486 hit that twice. The tell is a plausible
// small DIFF against a name that has nothing to do with the marker's address.

// FUNCTION: LOCO 0x424460
Obj0x477bd0::~Obj0x477bd0() {
    m_0c = 0;
    vtbl = g_vtable0x477bd0;
    m_count = 0;
    if (m_ptr) operator delete(m_ptr);
    m_ptr = 0;
}

// FUNCTION: LOCO 0x424a00
Obj0x477b40::~Obj0x477b40() {
    m_0c = 0;
    vtbl = g_vtable0x477b40;
    m_count = 0;
    if (m_ptr) operator delete(m_ptr);
    m_ptr = 0;
}

// FUNCTION: LOCO 0x435ca0
Obj0x478070::~Obj0x478070() {
    m_0c = 0;
    vtbl = g_vtable0x478070;
    m_count = 0;
    if (m_ptr) operator delete(m_ptr);
    m_ptr = 0;
}

// FUNCTION: LOCO 0x436280
Obj0x477fe0::~Obj0x477fe0() {
    m_0c = 0;
    vtbl = g_vtable0x477fe0;
    m_count = 0;
    if (m_ptr) operator delete(m_ptr);
    m_ptr = 0;
}

// ---------------------------------------------------------------------------------------
// The element-type-dependent half of Obj0x477758Base / Obj0x477758 (see the file header).
// ---------------------------------------------------------------------------------------

// GROWTH_FACTOR_MAYBE -- the capacity ramp all three growth sites share -- lives in the header
// now (v431), because Add() moved in-class and its callers expand the ramp inline.

// FUNCTION: LOCO 0x435d10
// PARTIAL / EFFECTIVE-adjacent -- DIFF(145), compiled 163 B against 158, insns 67/67. Every
// instruction, call, constant and branch target is present and in the original's order; the
// whole residual is ONE root cause with a register-rename cascade behind it: VC5 ROTATES the
// trailing-NULL trim loop. The original tests `m_ptr[i-1]` at the loop TOP and branches back to
// that load (`ja`); this compile peels the first load above the loop and puts the reload at the
// bottom, so the backedge polarity inverts and the two induction registers swap roles for the
// rest of the body. **THREE loop spellings measured and ALL THREE compile to byte-identical
// output -- do NOT re-run:** `do { if (p[i-1] != 0) break; i--; } while (i > nCapacity);` (this
// one, which maps 1:1 onto the original's basic blocks), the top-tested
// `while (p[i-1] == 0) { i--; if (i <= nCapacity) break; }`, and the compound
// `while (p[i-1] == 0 && --i > nCapacity) {}`. VC5 canonicalizes all three, so the rotation is
// not reachable from the source text.
// **Two levers ARE baked in, do not undo:** both zero guards must be spelled `nWanted > 0`, not
// `!= 0` (on an unsigned value the original branches `jbe` off a live zero register, not `je` --
// the same lever TrackGraph::BuildAdjacencyAMaybe needed), and the trim guard must read
// `m_count > nCapacity` in that operand order (the reversed `nCapacity < m_count` swaps which
// of the two lands in the compare's first operand). One residual is separate and cosmetic: the
// original's final `m_ptr = 0` reuses the register already holding the collapsed zero
// (`mov [ebp+4],eax`) where this compile stores an immediate.
//
// Slot 0. The family's shared generic reserve/regrow, and the one member of the pair that is
// NOT element-type-dependent (it only ever moves pointers), which is why a single shared
// copy serves every instantiation.
//
// It TRIMS the request first: a shrink request never throws away a slot that still holds
// something, so the walk starts at the current capacity and comes down only across trailing
// NULLs. Then the ordinary allocate / zero / copy-the-overlap / free-the-old dance, with the
// wrinkle that a zero final capacity also nulls m_ptr -- the two fields are kept consistent so
// every other member can test either one.
void Obj0x477758Base::ReserveMaybe(unsigned int nCapacity) {
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
    TilePlacedObj **pOld = m_ptr;
    if (nWanted > 0) {
        m_ptr = (TilePlacedObj **)operator new(nWanted * sizeof(TilePlacedObj *));
        memset(m_ptr, 0, nWanted * sizeof(TilePlacedObj *));
    }
    if (pOld != 0) {
        if (nWanted > 0) {
            unsigned int nCopy = m_count;
            if (nWanted < (unsigned int)m_count) {
                nCopy = nWanted;
            }
            memcpy(m_ptr, pOld, nCopy * sizeof(TilePlacedObj *));
        }
        operator delete(pOld);
    }
    m_count = m_ptr != 0 ? nWanted : 0;
    if (m_count == 0) {
        m_ptr = 0;
    }
}

// The family base's T-INDEPENDENT slots, 2..8 and 11..12. Every one of them is a single
// shared address serving all six of the family's tables (the four registry ones at
// 0x478070/0x478018/0x477fe0/0x477f88 and this pair's 0x477798/0x477758), because none of them
// ever names T: they only move pointers, compare against the two counts, or reach T through
// its own vtable. That is the same evidence that pins slots 9/10/13/14 as the type-dependent
// ones -- those are exactly the slots that DID stay distinct per instantiation.
//
// ⚠ src/DecorObjMgrMaybe.h's PlacedObjCollectionMaybe re-declares several of these same slots
// for its own dispatch. That is not a duplicate model but the SAME class reached over a second
// element type: the registry pair and this pair are two subclasses of one hand-written base,
// and these bodies are the base's single compiled copy, which both reach.

// FUNCTION: LOCO 0x424020
// Slot 2. Reserve(0) is the family's whole "give it all back" case: a zero final capacity nulls
// m_ptr as well as freeing it, so there is genuinely nothing left for this to do.
void Obj0x477758Base::ReleaseStorage() {
    ReserveMaybe(0);
}

// FUNCTION: LOCO 0x4356b0
// Slot 3, base implementation. Reads the slot out through the vtable rather than off m_ptr
// directly -- which is what makes the bounds check slot 7's rather than this function's -- then
// nulls it. The removed element is handed back UNDESTROYED; slot 4 is the deleting form.
TilePlacedObj *Obj0x477758Base::RemoveAt(unsigned int idx) {
    if (idx >= (unsigned int)m_count) {
        return 0;
    }
    TilePlacedObj *pRemoved = GetAtMaybe(idx);
    m_ptr[idx] = 0;
    return pRemoved;
}

// FUNCTION: LOCO 0x4356e0
// Slot 4, and the single most shared function in the family -- all six tables name it. The whole
// body is slot 3 composed with `delete`, and both halves are virtual dispatches: slot 3 through
// this object's own vtable, the destructor through the removed element's (`mov edx,[eax];
// push 1; call [edx]`, T's own slot 0). Naming T is what would have split it per instantiation,
// and it never does.
void Obj0x477758Base::RemoveAndDeleteAt(unsigned int idx) {
    delete RemoveAt(idx);
}

// FUNCTION: LOCO 0x4244f0
// Slot 5, base implementation: null the whole CAPACITY, dropping every reference without
// destroying anything. Note it re-reads m_count at the bottom of every iteration -- a slot 3
// override could have shortened the array underneath it.
void Obj0x477758Base::RemoveAll() {
    for (unsigned int i = 0; i < (unsigned int)m_count; i++) {
        m_ptr[i] = 0;
    }
}

// FUNCTION: LOCO 0x424510
// Slot 6, base implementation: the destroying form of RemoveAll, one slot-4 call per index.
// Same re-read of m_count each iteration, and for the same reason.
void Obj0x477758Base::RemoveAndDeleteAll() {
    for (unsigned int i = 0; i < (unsigned int)m_count; i++) {
        RemoveAndDeleteAt(i);
    }
}

// FUNCTION: LOCO 0x424530
// Slot 7. The bounds check is against the CAPACITY, not the live count, which is what lets
// RemoveAt reach a slot past the live end and what makes a null return mean "out of range" and
// "that slot is empty" at the same time.
TilePlacedObj *Obj0x477758Base::GetAtMaybe(unsigned int idx) {
    if (idx >= (unsigned int)m_count) {
        return 0;
    }
    return m_ptr[idx];
}

// FUNCTION: LOCO 0x424030
// Slot 8. A pure forward to slot 7 THROUGH THE VTABLE -- which is the whole point of the two
// slots existing: an override of slot 7 changes what every slot-8 caller sees.
void *Obj0x477758Base::GetAt(int idx) {
    return GetAtMaybe(idx);
}

// FUNCTION: LOCO 0x424010
// Slot 11, base implementation: the CAPACITY. The derived (0x424000) returns the live count
// from +0xc instead, and that pair is the clearest statement in the family of what the two
// counts are for.
unsigned int Obj0x477758Base::Count() {
    return m_count;
}

// FUNCTION: LOCO 0x424760
// Slot 12, shared base and derived (an occupancy test needs neither the live count nor T).
// Returns an int, not a byte: both exits go through the full eax (`mov eax,1` / `xor eax,eax`).
int Obj0x477758Base::IsSlotOccupiedMaybe(unsigned int idx) {
    if (idx < (unsigned int)m_count && m_ptr[idx] != 0) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x412140
// Slot 9. The only member that constructs an element, and therefore the one that pins this
// instantiation's T: the `new` asks for TilePlacedObj's exact 0x10c bytes and the implicit
// copy constructor's memberwise walk stamps all three of its vtables in order
// (RectFlagObj0x477820, then AnimDescRefObj0x477488, then TilePlacedObj) -- which is also what
// re-pinned three of TilePlacedObj's own field shapes this session, since a memberwise copy
// renders each member's exact width and every non-member padding byte gets skipped.
void Obj0x477758Base::SetCopyAtMaybe(unsigned int idx, const TilePlacedObj &src) {
    SetAtMaybe(idx, new TilePlacedObj(src));
}

// FUNCTION: LOCO 0x4123a0
// Slot 10, base implementation. Note it re-reads m_ptr at every one of the four uses rather
// than caching it in a local -- the original does too (four separate `mov edx,[esi+4]`), and
// only the idx*4 scaling is CSE'd.
TilePlacedObj *Obj0x477758Base::SetAtMaybe(unsigned int idx, TilePlacedObj *pItem) {
    if (idx >= (unsigned int)m_count) {
        ReserveMaybe(1 - (int)(idx * GROWTH_FACTOR_MAYBE));
    }
    if (m_ptr[idx] != 0) {
        delete m_ptr[idx];
        m_ptr[idx] = 0;
    }
    m_ptr[idx] = pItem;
    return m_ptr[idx];
}

// FUNCTION: LOCO 0x4124b0
// Slot 10, derived override: the base's body verbatim behind one extra reject. The guard is
// `>` and not `>=`, so writing one past the live end is allowed (that is exactly what Add
// below does), and it is the LIVE count m_0c that bounds it, not the capacity m_count the
// body then grows against.
//
// EFFECTIVE MATCH -- 128/129 bytes, insns 47/47, align=0, ONE byte differs. Every block,
// branch, call, constant and register assignment agrees; the sole disagreement is the SIB
// operand order of the closing `return m_ptr[idx];`, where the original encodes
// `mov eax,[edi+ecx*1]` (base = the scaled index) and this encodes `mov eax,[ecx+edi*1]`
// (base = m_ptr). The base's own copy of that identical statement, 40 bytes up the file at
// 0x4123a0, encodes it the original's way and matches EXACTLY -- so the flip is downstream of
// the extra guard's register pressure (the derived spills through ebx, and its `pop ebx` lands
// between the store and the reload), not of anything spellable in this statement. Nothing to
// grind: the two operands are commutative in the addressing mode and the source cannot name
// which becomes the ModRM base.
TilePlacedObj *Obj0x477758::SetAtMaybe(unsigned int idx, TilePlacedObj *pItem) {
    if (idx > m_0c) {
        return 0;
    }
    if (idx >= (unsigned int)m_count) {
        ReserveMaybe(1 - (int)(idx * GROWTH_FACTOR_MAYBE));
    }
    if (m_ptr[idx] != 0) {
        delete m_ptr[idx];
        m_ptr[idx] = 0;
    }
    m_ptr[idx] = pItem;
    return m_ptr[idx];
}

// Slot 13, Obj0x477758::Add (0x412440), is deliberately NOT here. Its body moved in-class to
// src/Obj0x477798Family.h in v431, because the original inlines it at its call sites (see
// TrackGraph::BuildAdjacencyAMaybe's expansion at 0x45cecf). VC5 still emits an out-of-line
// copy for the vtable slot, but only in a TU that actually CONSTRUCTS an Obj0x477758 -- which
// this TU does not, so the COMDAT is absent from this .obj entirely. The marker for it lives
// in src/PlacementCursorMaybe.cpp, which is also where the ORIGINAL's .obj put it (0x412440
// sits inside that object's 0x412140..0x412577 run -- see this file's header comment).

// The derived's three T-independent overrides. Each one exists purely because the base's
// version reasons about the CAPACITY and the derived has to reason about the LIVE count
// instead -- which is also why all three have one shared address across every instantiation in the
// family: swapping which count you read never names T.

// FUNCTION: LOCO 0x424250
// Slot 5, derived override. Where the base can simply null the whole array, the derived has to
// bring m_0c down with it, and slot 3 is the only member that knows how -- so this peels the
// live range off the END, one index at a time, and re-reads m_0c after every call rather than
// counting down a local (slot 3 is virtual, and an override is free to remove more than one).
void Obj0x477758::RemoveAll() {
    while (m_0c != 0) {
        RemoveAt(m_0c - 1);
    }
}

// FUNCTION: LOCO 0x424270
// Slot 6, derived override: RemoveAll with slot 4 in place of slot 3, and byte-for-byte the
// same shape otherwise (the two differ only in the vtable offset the call names, +0x10 vs
// +0xc). That one byte is why the two are separate functions rather than one.
void Obj0x477758::RemoveAndDeleteAll() {
    while (m_0c != 0) {
        RemoveAndDeleteAt(m_0c - 1);
    }
}

// FUNCTION: LOCO 0x424000
// Slot 11, derived override: the LIVE count, against the base's capacity at 0x424010.
unsigned int Obj0x477758::Count() {
    return m_0c;
}

// FUNCTION: LOCO 0x412540
// Slot 14, the derived's own added slot. Linear scan over the LIVE range only.
int Obj0x477758::FindIndexMaybe(TilePlacedObj *pItem) {
    int nIndex = -1;
    if (pItem == 0) {
        return nIndex;
    }
    for (unsigned int i = 0; i < m_0c; i++) {
        if (m_ptr[i] == pItem) {
            nIndex = i;
            break;
        }
    }
    return nIndex;
}

// ---------------------------------------------------------------------------------------
// PlacedObjRegistryMaybe (src/DecorObjMgrMaybe.h) -- the SORTED derived instantiation of this
// same family, and the five slots of it that are type-independent enough to have collapsed to
// one shared copy each. They land in this .obj by address: 0x4241e0/0x424490/0x4244b0/0x4244d0
// interleave with the Obj0x477bd0 and Obj0x477b40 destructors above (0x424460 / 0x424a00), and
// 0x4362b0 sits immediately after Obj0x477fe0's (0x436280). Both live registries -- the
// manager's category-7 and category-8 ones -- dispatch through these same five addresses.
// ---------------------------------------------------------------------------------------

// FUNCTION: LOCO 0x4241e0
// Slot 3. The sorted registry's remove-at, and the same body PlacementCursorMaybe::
// SetHoverObjMaybe open-codes twice inline (0x411440). GetAtMaybe bounds-checks the index for
// us, so a null return is both "out of range" and "already empty" -- either way there is
// nothing to shift. RETURNS the removed element, which is what DeregisterEntryMaybe compares
// against the actor it asked to have removed.
void *PlacedObjRegistryMaybe::RemoveAtShiftingTail(unsigned int nIndex) {
    void *pRemoved = GetAtMaybe(nIndex);
    if (pRemoved != 0) {
        if (nIndex < nCountMaybe - 1) {
            memmove(&pArrayMaybe[nIndex], &pArrayMaybe[nIndex + 1],
                    (nCountMaybe - (nIndex + 1)) * sizeof(DecorActorBase *));
        }
        pArrayMaybe[nCountMaybe - 1] = 0;
        nCountMaybe = nCountMaybe - 1;
    }
    return pRemoved;
}

// FUNCTION: LOCO 0x424490
// Slot 19. Re-key and re-sort in one act; the return value is a constant 0 nobody reads.
int PlacedObjRegistryMaybe::SetSortParamsAndSortMaybe(unsigned int nKeyOffset,
                                                      unsigned int nKeyType) {
    nSortKeyOffsetMaybe = nKeyOffset;
    nSortKeyTypeMaybe = nKeyType;
    SortAllMaybe();
    return 0;
}

// FUNCTION: LOCO 0x4244b0
// Slot 14. The whole-registry convenience over the binary search at slot 16; -1 on an empty
// registry, since the inclusive upper bound would otherwise underflow to 0xffffffff.
int PlacedObjRegistryMaybe::IndexOfMaybe(void *pObj) {
    int nIndex = -1;
    if (nCountMaybe > 0) {
        nIndex = FindIndexMaybe(pObj, 0, nCountMaybe - 1);
    }
    return nIndex;
}

// FUNCTION: LOCO 0x4244d0
// Slot 20. A one-entry registry is already sorted, so the quicksort is skipped rather than
// entered with nLo == nHi.
int PlacedObjRegistryMaybe::SortAllMaybe() {
    if (nCountMaybe > 1) {
        SortRangeMaybe(0, nCountMaybe - 1);
    }
    return 0;
}

// FUNCTION: LOCO 0x424820
// PARTIAL / EFFECTIVE-adjacent -- DIFF(70), 155 B against 153, insns 79/79. Every instruction,
// call, constant, branch target and block is the original's, in the original's order; the whole
// residual is ONE register coin-flip and its consequences. The original parks nHi in ebx and
// pObj in ebp; this compile swaps the two, and because ebp is then the busier of the pair, the
// binary half spills pObj and reloads it at each of its three uses where the original hoists it
// once. Nothing in the source names which callee-saved register a parameter lands in.
// ⚠ One lever IS baked in, do not undo: the identity test must be spelled `pObj ==
// pArrayMaybe[i]` in that operand order. Reversed, VC5 emits `cmp [eax+edi*4], reg` where the
// original has `cmp reg, [eax+edi*4]` -- the same commutative-compare lever ReserveMaybe's trim
// guard needed, and worth one point of the score on its own.
//
// Slot 16. A binary search that gives up on subdividing once the inclusive range is down to
// four entries and finishes with a linear scan -- and the scan stops on the first entry that
// does NOT sort before pObj, so it lands on the equal run rather than walking past it. The
// entry there still has to be pObj ITSELF to count as found: the ordering is by sort key, and
// several distinct entries can share one. Both halves of the recursion keep nMid, so the two
// sub-ranges OVERLAP by one; that is what makes the `> 2` cutoff safe (a two-wide range can
// never re-derive itself and spin).
int PlacedObjRegistryMaybe::FindIndexMaybe(void *pObj, unsigned int nLo, unsigned int nHi) {
    if (nHi - nLo <= 2) {
        unsigned int i;
        for (i = nLo; i <= nHi; i++) {
            if (CompareEntriesMaybe(pObj, pArrayMaybe[i]) <= 0) {
                break;
            }
        }
        if (i <= nHi && pObj == pArrayMaybe[i]) {
            return i;
        }
        return -1;
    }
    unsigned int nMid = nLo + (nHi - nLo) / 2;
    if (CompareEntriesMaybe(pObj, pArrayMaybe[nMid]) < 0) {
        return FindIndexMaybe(pObj, nLo, nMid);
    }
    return FindIndexMaybe(pObj, nMid, nHi);
}

// FUNCTION: LOCO 0x435cd0
// Slot 21. "Am I still in order": compare each adjacent pair through slot 18 and stop at the
// first one that is not strictly ascending. The running verdict is seeded to -1 so an empty or
// single-entry registry reports sorted, and it is the LOOP GUARD as well as the result -- the
// `nCmp >= 0` test sits at the top of the body, not in the for-condition, which is why the
// original tests it after the entry guard rather than folding the two together.
char PlacedObjRegistryMaybe::IsSortedMaybe() {
    int nCmp = -1;
    int i = 0;
    int nLast = nCountMaybe - 1;
    for (; i < nLast; i++) {
        if (nCmp >= 0) {
            break;
        }
        nCmp = CompareEntriesMaybe(pArrayMaybe[i], pArrayMaybe[i + 1]);
    }
    return nCmp < 0;
}

// FUNCTION: LOCO 0x4362b0
// Slot 13. Linear-scan for the insertion point, then insert -- O(n) rather than a binary
// search because the scan also has to stop at the first UNOCCUPIED slot, which the sorted
// order says nothing about. With no sort key configured (type code 0) it degenerates to an
// append at the live end.
//
// The loop shape is load-bearing and cost three compiles. The two exit tests are `break`s
// INSIDE a plain counted `for`, not the loop's own condition: that makes the counted test the
// only back edge, so VC5 emits the do-while it always emits for a `for` and leaves the two
// calls in the body un-rotated. Spelling the same logic as a `for (;;)` with the counter test
// written out at the bottom instead reads identically but lets VC5 rotate the loop -- it
// duplicates the IsSlotOccupiedMaybe test into the back edge and the function grows 26 bytes
// (DIFF(99) at 126 B). The original also emits TWO InsertAtMaybe tails from this one source
// call: the loop-exhausted path keeps its own copy rather than jumping to the else branch's.
void PlacedObjRegistryMaybe::InsertInSortedPositionMaybe(void *pObj) {
    unsigned int i;
    if (nSortKeyTypeMaybe > 0) {
        for (i = 0; i < nCountMaybe; i++) {
            if (!IsSlotOccupiedMaybe(i)) {
                break;
            }
            if (CompareEntriesMaybe(pObj, GetAtMaybe(i)) <= 0) {
                break;
            }
        }
    } else {
        i = nCountMaybe;
    }
    InsertAtMaybe(i, pObj);
}

// ---------------------------------------------------------------------------------------
// The FOUR per-instantiation leaves of the registry family -- two per category, one on each
// half of the base/derived pair. Only three slots in this whole family fail to collapse across
// the two instantiations, and each leaf exists purely to give one of those bodies a home:
//
//   category 7 (T = WalkerActor)       base 0x478070 / derived 0x478018
//   category 8 (T = RoadVehicleActor)  base 0x477fe0 / derived 0x477f88
//
//   slot  9  SetCopyAtMaybe   0x435700 / 0x435db0   (shared by each pair's two tables)
//   slot 10  SetAtMaybe       0x4359a0 / 0x435a10   (category 7: base / derived)
//                             0x436040 / 0x4360b0   (category 8: base / derived)
//
// Slot 9 fails to fold for the reason the family exists: it is the one member that CONSTRUCTS
// an element, so it is the one that names T. Slot 10 fails for a reason that has nothing to do
// with T at all -- its `fmul` names its own .obj's copy of the -1.1 growth constant, so all
// four bodies are relocation-distinct even though the source text is one line-for-line
// identical function. Every other slot in both tables is a single shared address.
//
// The leaves add no fields and no slots, so they are defined here rather than in
// src/DecorObjMgrMaybe.h: nothing outside this TU can tell the instantiations apart, and
// keeping them local costs that header nothing.
// ---------------------------------------------------------------------------------------

struct WalkerCollectionMaybe : PlacedObjCollectionMaybe {
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
};

struct RoadVehicleCollectionMaybe : PlacedObjCollectionMaybe {
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
};

struct WalkerRegistryMaybe : PlacedObjRegistryMaybe {
    virtual void SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src);
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
    virtual int InsertAtMaybe(unsigned int nIndex, void *pObj);
};

struct RoadVehicleRegistryMaybe : PlacedObjRegistryMaybe {
    virtual void SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src);
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
    virtual int InsertAtMaybe(unsigned int nIndex, void *pObj);
    virtual int CompareEntriesMaybe(void *pObj, void *pOther);
};

// The registry's own growth ramp, and NOT the family's: slot 17 below multiplies by a POSITIVE
// 1.1 (its own .rdata double at 0x4780b0) and passes the product straight to Reserve, where
// every SetAtMaybe/Add site in the family multiplies by the NEGATIVE -1.1 at 0x477838 and
// computes `1 - x` (see GROWTH_FACTOR_MAYBE in src/Obj0x477798Family.h, which explains why that
// site prefers the negative constant). Two constants, two ramps, both original.
static const double INSERT_GROWTH_FACTOR_MAYBE = 1.1;

// FUNCTION: LOCO 0x4359a0
// Slot 10, category 7's BASE table -- the family's plain store-at, with no reject in front of
// it. Structurally identical to Obj0x477758Base::SetAtMaybe further up this file (same source,
// different T), and the four-way relocation split described above is the only reason it exists
// as its own address rather than folding onto that one.
DecorActorBase *WalkerCollectionMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x436040
// Slot 10, category 8's BASE table: the category-7 twin above, line for line.
DecorActorBase *RoadVehicleCollectionMaybe::SetAtMaybe(unsigned int nIndex,
                                                       DecorActorBase *pItem) {
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x435b60
// Slot 17, category 7. Make room at nIndex, then hand the actual store to slot 10 rather than
// writing pArrayMaybe directly -- which is the whole reason this is a registry method and not a
// free helper: slot 10 is where the per-instantiation "destroy whatever was there" lives.
// Appending (nIndex == nCountMaybe) skips the shift entirely, and the vacated slot is NULLed
// before the store so slot 10's own delete has nothing stale to find.
int WalkerRegistryMaybe::InsertAtMaybe(unsigned int nIndex, void *pObj) {
    if (nIndex > nCountMaybe) {
        return -1;
    }
    if (nCountMaybe + 1 > nCapacityMaybe) {
        ReserveMaybe((int)(nCountMaybe * INSERT_GROWTH_FACTOR_MAYBE));
    }
    if (nIndex != nCountMaybe) {
        memmove(&pArrayMaybe[nIndex + 1], &pArrayMaybe[nIndex],
                (nCountMaybe - nIndex) * sizeof(DecorActorBase *));
        pArrayMaybe[nIndex] = 0;
    }
    SetAtMaybe(nIndex, (DecorActorBase *)pObj);
    nCountMaybe = nCountMaybe + 1;
    return nIndex;
}

// FUNCTION: LOCO 0x436140
// Slot 17, category 8: the twin of 0x435b60 above, and byte-identical to it apart from its own
// branch targets -- same growth constant, same register assignment, same everything. ⚠ That is
// worth stating plainly, because it refutes the natural guess: these two did NOT stay distinct
// because anything in them names the element type or a per-.obj constant. Identical text was
// simply emitted into both registry .objs and the linker never folded the copies. The same is
// true of the slot-10 pair (0x4359a0 / 0x436040) and of slot 18's pair, where only the operand
// ORDER of one compare differs (see src/PlacedObjRegistryMaybe.cpp's note on 0x435c00).
int RoadVehicleRegistryMaybe::InsertAtMaybe(unsigned int nIndex, void *pObj) {
    if (nIndex > nCountMaybe) {
        return -1;
    }
    if (nCountMaybe + 1 > nCapacityMaybe) {
        ReserveMaybe((int)(nCountMaybe * INSERT_GROWTH_FACTOR_MAYBE));
    }
    if (nIndex != nCountMaybe) {
        memmove(&pArrayMaybe[nIndex + 1], &pArrayMaybe[nIndex],
                (nCountMaybe - nIndex) * sizeof(DecorActorBase *));
        pArrayMaybe[nIndex] = 0;
    }
    SetAtMaybe(nIndex, (DecorActorBase *)pObj);
    nCountMaybe = nCountMaybe + 1;
    return nIndex;
}

// FUNCTION: LOCO 0x435700
// Slot 9, T = WalkerActor. Two statements' worth of source; the ~600 bytes are ENTIRELY the
// compiler-generated copy constructor, inlined. That makes this function a layout ORACLE for
// the whole WalkerActor chain, since a memberwise copy renders each member's exact width in
// offset order and skips every padding byte: it walks RectFlagObj0x477820 (+0x4..+0x20,
// stamping 0x477820), then AnimDescRefObj0x477488 (+0x24..+0x86, stamping 0x477488 -- and its
// szCategoryName[11] tail copies as dword+dword+WORD+BYTE, independently re-confirming the
// ELEVEN-not-twelve width src/WidgetBase.h pinned from TilePlacedObj's own copy ctor), then
// DecorActorBase (+0x88..+0xec, stamping 0x477f18) and finally WalkerActor's own
// pRidingTrainMaybe at +0xf0 (stamping 0x477eb8). `new` asks for 0xf4, confirming the modeled
// sizeof exactly. Every field offset and width in src/DecorActor.h checks out against it.
//
// The cast is this model's rendering of the template parameter: the shared base declares the
// slot over DecorActorBase because one class models both registries everywhere else, and this
// leaf is where T becomes concrete again.
void WalkerRegistryMaybe::SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src) {
    SetAtMaybe(nIndex, new WalkerActor((const WalkerActor &)src));
}

// FUNCTION: LOCO 0x435db0
// Slot 9, T = RoadVehicleActor -- the same source line, instantiated over the category-8 leaf.
// `new` asks for 0xf0, confirming the modeled sizeof, and the copy stamps four vtables just
// like the walker's (0x477820, 0x477488, 0x477f18, then 0x4780b8 for the leaf itself) -- the
// last two back to back, because this leaf's only member is the +0xec int copied after them.
//
// That adjacency is what made this the more sensitive of the two: while +0xec was modeled as a
// DecorActorBase member, the base's own 0x477f18 store had nothing left to separate it from
// the leaf's 0x4780b8 store and VC5 dead-stored it away, leaving this body one instruction
// SHORT (162/163) where the walker was merely mis-scheduled at an equal 165/165. Two symptoms,
// one cause -- see the Unk0xec note in src/DecorActor.h.
void RoadVehicleRegistryMaybe::SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src) {
    SetAtMaybe(nIndex, new RoadVehicleActor((const RoadVehicleActor &)src));
}

// The slot-10 half of the same two leaves: the family's SetAtMaybe over T = DecorActorBase,
// the derived override in both cases (`idx > <live count>` reject, then the base body
// verbatim). Structurally identical to Obj0x477758::SetAtMaybe further up this file, and the
// two instantiations differ only in which .obj's copy of the -1.1 growth constant the `fmul`
// names. ⚠ RETRACTED v485 -- all four bodies name the SAME 0x477838 constant; see the
// mechanism note in this file's leaf section.
//
// ⚠ Their two BASE-side counterparts, 0x4359a0 (category 7) and 0x436040 (category 8), stay
// unclaimed: the base half of these two instantiations has no class in this project's model at
// all. PlacedObjRegistryMaybe models the DERIVED table for both registries, and the flat
// Obj0x478070 / Obj0x477fe0 structs above model only their destructors. Giving the base half a
// real class is the family-model cleanup this file has been accumulating, not a two-line add.

// FUNCTION: LOCO 0x435a10
// EFFECTIVE MATCH -- 129/130 bytes, insns 47/47, align=0, reg_pen=2, ONE byte differs. This is
// the SAME residual 0x4124b0 carries, arriving from the same source text over a different T:
// the closing `return pArrayMaybe[nIndex];` encodes as `mov eax,[ecx+edi]` where the original
// picks the commutative `mov eax,[edi+ecx]`. The two operands are interchangeable in the
// addressing mode and no source spelling names which becomes the ModRM base, so there is
// nothing to grind -- see the fuller autopsy on 0x4124b0 above.
DecorActorBase *WalkerRegistryMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex > nCountMaybe) {
        return 0;
    }
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x4360b0
// EFFECTIVE MATCH -- the category-8 twin of 0x435a10 above, byte-for-byte the same residual
// (129/130, insns 47/47, the one commutative-SIB byte). Three instantiations of this one source
// now land on it; only the base-side 0x4123a0 escapes it, which is what pins the flip to the
// extra guard's register pressure rather than to anything in the statement itself.
DecorActorBase *RoadVehicleRegistryMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex > nCountMaybe) {
        return 0;
    }
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// ---------------------------------------------------------------------------------------
// A THIRD registry leaf, on the BigObjTrackingSetsMaybe (EffectSpawner, DAT_004fd220) side of
// the family: the "candidate set" (EffectSpawner's pCandidateSetMaybe, the +0x1c sub-object),
// whose base/derived tables are 0x477bd0 / 0x477b78 (BigObjTrackingSetsMaybe::CtorMaybe
// 0x4238e9 stamps exactly those two, in the PlacedObjRegistryMaybe base-then-derived order).
// Its slot 9 (0x424040, installed at 0x477bd0+0x24 and 0x477b78+0x24 alike) heap-copies a
// 0x88-byte element and stamps ONLY the RectFlagObj0x477820 / AnimDescRefObj0x477488 vtable
// pair -- no third stamp, and `new` asks for exactly 0x88 -- so its T is modeled as plain
// AnimDescRefObj0x477488 itself (a sliced copy of whatever effect object was registered; the
// sibling pPlacedSetMaybe/pGhostSetMaybe pair, 0x477b40/0x477ae8, keeps the full
// AnimEffectObj0x477a90 -- 0xa4 bytes, three stamps, slot 9 = 0x424550, still unclaimed).
struct EffectCandidateRegistryMaybe : PlacedObjRegistryMaybe {
    virtual void SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src);
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
    virtual int InsertAtMaybe(unsigned int nIndex, void *pObj);
    virtual int CompareEntriesMaybe(void *pObj, void *pOther);
};

// FUNCTION: LOCO 0x424040
// Slot 9, T = AnimDescRefObj0x477488 -- the same one-statement source as the Walker/
// RoadVehicle slot-9 pair above (the two casts are again the model's rendering of the
// per-leaf element type; see that note -- T is NARROWER than the slot's declared
// DecorActorBase here, so the new-expression's result takes a downcast too). The ~300 bytes
// are the inlined two-level copy chain: RectFlagObj0x477820's members (+0x4..+0x20, stamping
// 0x477820), then AnimDescRefObj0x477488's (+0x24..+0x86, stamping 0x477488), its
// szCategoryName[11] tail copying as dword+dword+WORD+BYTE exactly as the Walker walk
// re-confirmed. (Copying a REAL AnimEffectObj0x477a90 through this slot slices it to the
// shared base part, which the "candidate" reading of the set tolerates.)
void EffectCandidateRegistryMaybe::SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src) {
    SetAtMaybe(nIndex, (DecorActorBase *)new AnimDescRefObj0x477488((const AnimDescRefObj0x477488 &)src));
}

// The FOURTH registry leaf: the sibling BigObjTrackingSetsMaybe pair 0x477b40 / 0x477ae8
// (EffectSpawner's pPlacedSetMaybe AND pGhostSetMaybe -- CtorMaybe 0x4238e9 stamps both with
// the same two tables). Its slot 9 (0x424550) is the
// same one statement over the full 0xa4-byte AnimEffectObj0x477a90: the copy walk above, then
// the leaf's own members (+0x88 byte, +0x8a WORD -- +0x89 pad skipped, +0x8c/+0x90 dwords,
// +0x94 byte, +0x98/+0x9c/+0xa0 dwords) and the third, outermost 0x477a90 vtable stamp. The
// member walk matches src/AnimEffectObj.h's field model width-for-width.
struct EffectPlacedRegistryMaybe : PlacedObjRegistryMaybe {
    virtual void SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src);
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
    virtual int InsertAtMaybe(unsigned int nIndex, void *pObj);
    virtual int CompareEntriesMaybe(void *pObj, void *pOther);
};

// The two effect registries' BASE-table halves (vtable 0x477bd0 / 0x477b40), same modeling
// gap the Walker/RoadVehicle pair has -- see the note above WalkerCollectionMaybe.
struct EffectCandidateCollectionMaybe : PlacedObjCollectionMaybe {
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
};
struct EffectPlacedCollectionMaybe : PlacedObjCollectionMaybe {
    virtual DecorActorBase *SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem);
};

// FUNCTION: LOCO 0x424550
void EffectPlacedRegistryMaybe::SetCopyAtMaybe(unsigned int nIndex, const DecorActorBase &src) {
    SetAtMaybe(nIndex, (DecorActorBase *)new AnimEffectObj0x477a90((const AnimEffectObj0x477a90 &)src));
}

// The four effect-registry slot-10 bodies: same source text as the Walker/RoadVehicle
// quartet above, distinct addresses for the same reason (the per-.obj -1.1 growth constant
// reloc, not anything T names). Vtable map (read at 0x477bd0/0x477b78/0x477b40/0x477ae8
// +0x28): candidate base = 0x424170, candidate derived = 0x424290, placed base = 0x4246f0,
// placed derived = 0x424790.

// FUNCTION: LOCO 0x424170
// Slot 10, candidate set's BASE table -- the plain store-at, no reject.
DecorActorBase *EffectCandidateCollectionMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x4246f0
// Slot 10, placed set's BASE table: the candidate-set twin above, line for line.
DecorActorBase *EffectPlacedCollectionMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x424290
// Slot 10, candidate set's DERIVED table -- the base body behind the `nIndex > nCountMaybe`
// reject, same shape as WalkerRegistryMaybe::SetAtMaybe.
DecorActorBase *EffectCandidateRegistryMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex > nCountMaybe) {
        return 0;
    }
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x424320
// Slot 17, candidate set (vtable 0x477b78 slot 17 names this address; read off the image).
// The same body as WalkerRegistryMaybe::InsertAtMaybe / RoadVehicleRegistryMaybe::
// InsertAtMaybe (0x435b60 / 0x436140), shared source over this leaf's T -- distinct address
// for the same reason as the slot-10 bodies (the per-.obj +1.1 growth-constant reloc).
int EffectCandidateRegistryMaybe::InsertAtMaybe(unsigned int nIndex, void *pObj) {
    if (nIndex > nCountMaybe) {
        return -1;
    }
    if (nCountMaybe + 1 > nCapacityMaybe) {
        ReserveMaybe((int)(nCountMaybe * INSERT_GROWTH_FACTOR_MAYBE));
    }
    if (nIndex != nCountMaybe) {
        memmove(&pArrayMaybe[nIndex + 1], &pArrayMaybe[nIndex],
                (nCountMaybe - nIndex) * sizeof(DecorActorBase *));
        pArrayMaybe[nIndex] = 0;
    }
    SetAtMaybe(nIndex, (DecorActorBase *)pObj);
    nCountMaybe = nCountMaybe + 1;
    return nIndex;
}

// FUNCTION: LOCO 0x4243c0
// Slot 18, candidate set (vtable 0x477b78 slot 18 names this address; read off the image).
// The family's fourth stamp of the ordering predicate whose source is spelled out over
// PlacedObjRegistryMaybe::CompareEntriesMaybe in src/PlacedObjRegistryMaybe.cpp -- read that
// note for what each switch arm means and why the `pObj - pOther` tiebreak is there.
//
// ⭐ EFFECTIVE MATCH since v542 -- compiled 156 B against the true 152 B COMDAT, DIFF(80).
// It USED to be byte-identical to the matched 0x435c00 twin (verified instruction for
// instruction against the raw disasm; the only differences were branch targets and the
// jump-table relocation), i.e. the pObj-before-pOther operand order rather than the reversed
// 0x4361e0 order. It no longer is: v542 added AnimEffectObj0x477a90's three slot-7/9/10
// override DECLARATIONS to src/AnimEffectObj.h, which this TU includes, and that alone flipped
// this site onto the OTHER twin. Nothing about this function's own source changed.
//
// That is a genuine sharpening of the v457 rule, not just a regression: twin selection is NOT
// a fixed property of the TU. It is perturbable by the CONTENT of an unrelated included header,
// and the two twin sites in ONE TU are decided INDEPENDENTLY -- this file now emits the
// 156-byte form here and the 152-byte form at 0x4361e0, i.e. each site holds exactly the twin
// the OTHER one wants. Probed and refuted as levers (v542): a spare non-virtual declaration on
// AnimEffectObj0x477a90 (byte-identical either way -- a THRESHOLD, not a parity bit), and an
// inserted dummy definition immediately ahead of 0x4361e0 (no effect on either site).
//
// Accepted deliberately. The three overrides it paid for are earned matches -- correct source,
// EXACT on the first compile, and they correct this class's emitted vtable from 12/15 to 15/15
// slots agreeing with the image (slots 7/9/10 previously pointed at the BASE implementations
// 0x405a20/0x4061b0/0x405c40, which is wrong linked content). This site's own EXACT was never
// earned by source correctness -- v457 established it was a TU coin-flip -- so the trade is
// unearned luck for earned correctness. Retry only if twin selection itself becomes understood.
//
// ⚠ Its size in the unclaimed-gap list reads 134, which is the CODE extent only: the 4-entry
// switch table at 0x424448 is part of the same COMDAT, making the real extent 152 -- the same
// figure the matched 0x435c00 reports. Deriving --len from the next function's start is what
// keeps this from reading as an 18-byte missing tail.
int EffectCandidateRegistryMaybe::CompareEntriesMaybe(void *pObj, void *pOther) {
    int nResult;
    int nKeyType = nSortKeyTypeMaybe;
    switch (nKeyType) {
    case -4:
    case -3:
        nResult = *(int *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(int *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    case -2:
        nResult = *(short *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(short *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    case -1:
        nResult = *(unsigned short *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(unsigned short *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    default:
        nResult = memcmp((char *)pObj + nSortKeyOffsetMaybe,
                         (char *)pOther + nSortKeyOffsetMaybe, nKeyType);
        break;
    }
    if (nResult == 0) {
        nResult = (char *)pObj - (char *)pOther;
    }
    return nResult;
}

// FUNCTION: LOCO 0x424790
// Slot 10, placed set's DERIVED table: the candidate-set twin above, line for line.
// EFFECTIVE MATCH -- the same ONE commutative-SIB byte 0x435a10/0x4360b0 carry (the closing
// `return pArrayMaybe[nIndex];` picks the other ModRM base; see the autopsy there). Its
// candidate-set twin 0x424290 escapes it the way base-side 0x4123a0 does.
DecorActorBase *EffectPlacedRegistryMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    if (nIndex > nCountMaybe) {
        return 0;
    }
    if (nIndex >= nCapacityMaybe) {
        ReserveMaybe(1 - (int)(nIndex * GROWTH_FACTOR_MAYBE));
    }
    if (pArrayMaybe[nIndex] != 0) {
        delete pArrayMaybe[nIndex];
        pArrayMaybe[nIndex] = 0;
    }
    pArrayMaybe[nIndex] = pItem;
    return pArrayMaybe[nIndex];
}

// FUNCTION: LOCO 0x4248c0
// Slot 17, placed set (vtable 0x477ae8 slot 17 names this address; read off the image):
// the candidate-set twin 0x424320 above, line for line -- and byte-identical to it apart
// from branch targets, exactly the relationship 0x435b60 / 0x436140 have.
int EffectPlacedRegistryMaybe::InsertAtMaybe(unsigned int nIndex, void *pObj) {
    if (nIndex > nCountMaybe) {
        return -1;
    }
    if (nCountMaybe + 1 > nCapacityMaybe) {
        ReserveMaybe((int)(nCountMaybe * INSERT_GROWTH_FACTOR_MAYBE));
    }
    if (nIndex != nCountMaybe) {
        memmove(&pArrayMaybe[nIndex + 1], &pArrayMaybe[nIndex],
                (nCountMaybe - nIndex) * sizeof(DecorActorBase *));
        pArrayMaybe[nIndex] = 0;
    }
    SetAtMaybe(nIndex, (DecorActorBase *)pObj);
    nCountMaybe = nCountMaybe + 1;
    return nIndex;
}

// FUNCTION: LOCO 0x424960
// Slot 18, placed set (vtable 0x477ae8 slot 18 names this address; read off the image): the
// candidate-set twin 0x4243c0 above, and byte-identical to it apart from branch targets --
// exactly the relationship its slot-17 neighbours 0x424320 / 0x4248c0 have. Same COMDAT-extent
// caveat: 134 is the code, 152 with the switch table at 0x4249e8.
int EffectPlacedRegistryMaybe::CompareEntriesMaybe(void *pObj, void *pOther) {
    int nResult;
    int nKeyType = nSortKeyTypeMaybe;
    switch (nKeyType) {
    case -4:
    case -3:
        nResult = *(int *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(int *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    case -2:
        nResult = *(short *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(short *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    case -1:
        nResult = *(unsigned short *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(unsigned short *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    default:
        nResult = memcmp((char *)pObj + nSortKeyOffsetMaybe,
                         (char *)pOther + nSortKeyOffsetMaybe, nKeyType);
        break;
    }
    if (nResult == 0) {
        nResult = (char *)pObj - (char *)pOther;
    }
    return nResult;
}

// ---------------------------------------------------------------------------------------
// ⛔ WHY THE OTHER FOUR FLAT STRUCTS ARE STILL FLAT (v486)
//
// v485 established, and v486 confirmed on the first of them, that each `Obj0x<base-vtable>` struct
// in the header is really the derived destructor of one instantiation. Retiring the Obj0x477758
// one was mechanical: that pair is already modelled as a real base/derived pair, so the destructor
// simply moved onto the class that owns it. The remaining four are NOT mechanical, and the blocker
// is the shape of the hierarchy rather than any of the destructors themselves.
//
// The four registry instantiations each own a 14-slot BASE table and a 22-slot DERIVED table:
//     0x477bd0 / 0x477b78     0x477b40 / 0x477ae8     0x478070 / 0x478018     0x477fe0 / 0x477f88
// A derived destructor re-stamps its own instantiation's BASE table (0x435ca0 stores 0x478070,
// 0x436280 stores 0x477fe0), so in C++ terms the class it belongs to must DERIVE from the class
// that owns that base table. But this file's leaves are two parallel chains, not one:
//     WalkerCollectionMaybe : PlacedObjCollectionMaybe   <- owns base table 0x478070
//     WalkerRegistryMaybe   : PlacedObjRegistryMaybe     <- owns derived table 0x478018
// so `~WalkerRegistryMaybe` does not re-stamp 0x478070 and cannot be made to without reparenting
// it onto WalkerCollectionMaybe -- which would take PlacedObjRegistryMaybe's eight added slots
// (14..21) out of the shared header and duplicate them into each of the four leaves. That is a
// duplicate-class definition (lint_idiom.py class E) and a shared-`sizeof` drift hazard, traded
// for ~460 B of thunks.
//
// The real hierarchy is four levels deep and single inheritance cannot express it as modelled:
// the T-INDEPENDENT registry bodies (0x4241e0, 0x424250, 0x4362b0, 0x4244b0, 0x435aa0, 0x424820,
// 0x424490, 0x4244d0, 0x435cd0) are ONE address each, shared by all four instantiations, while
// the per-T ones (slots 9, 10, 17, 18) are four addresses each -- and the shared set sits BELOW
// the per-T set in the chain. Resolving that is what the next attempt has to do first; the
// destructors then fall out of it. Note the same 22-slot derived shape also covers the two
// BigObjTrackingSetsMaybe instantiations (0x477bd0/0x477b78 and 0x477b40/0x477ae8), whose element
// types are still unidentified -- their slot-9/10 bodies (0x424040 / 0x424550 and 0x424170 /
// 0x4246f0 / 0x424290 / 0x424790) are the ~1200 B of unclaimed family work left.
//
// v499 PROGRESS on the element types (from reading 0x424550, the 0x477b40/0x477ae8
// instantiation's slot 9): that instantiation's T is AnimEffectObj0x477a90 -- the body
// news exactly 0xa4, copies +0x04..+0xa3 field-by-field out of `src`, and stamps the
// RectFlagObj0x477820/AnimDescRefObj0x477488/AnimEffectObj0x477a90 vtable chain, i.e. it is
// `new AnimEffectObj0x477a90(src)` with an INLINED COPY CONSTRUCTOR (memberwise copies done
// per-ctor-level, the hand-written copy chain -- compare WalkerRegistryMaybe::SetCopyAtMaybe
// 0x435700 inlining DecorActor's copy ctor, src/DecorActor.h). The tail is
// `this->SetAtMaybe(idx, pCopy)` -- a VIRTUAL slot-10 (+0x28) self-call, matching the
// family's slot-9 "heap-copy and hand to slot 10" convention (Obj0x477758Family.h). The
// null-alloc arm passes 0 to the same slot. So the 0x477b40/0x477ae8 pair is the
// AnimEffectObj registry (one of the two effect collections BroadcastToAllEffectsMaybe
// walks inside EffectSpawner/DAT_004fd220, the +0x1c or +0x34 sub-object), and its
// slot-10 body 0x424790 is the matching SetAtMaybe override. Modeling the copy-ctor chain
// on AnimEffectObj0x477a90 (whose plain ctor 0x422ec0 is itself still parked, v329) is a
// prerequisite to transcribing the body.
// ---------------------------------------------------------------------------------------

// FUNCTION: LOCO 0x4361e0
// Slot 18, category 8's copy of the registry's ordering predicate -- the last member of the
// "identical source, two .objs, linker never folded them" set this file already owns for slots
// 9, 10 and 17. Its category-7 twin is 0x435c00, claimed as the shared
// PlacedObjRegistryMaybe::CompareEntriesMaybe in src/PlacedObjRegistryMaybe.cpp; read that
// function's plate for the sort-key encoding (-4/-3 int, -2 signed short, -1 unsigned short,
// positive N an N-byte memcmp, pointer-difference tie-break).
//
// PARTIAL -- compiled 152 B against the true 156 B COMDAT (0x4361e0..0x43627c: 138 B of code,
// a 2-byte `mov edi,edi` pad and the 4-entry jump table; app_funcs.txt's 138 is the CODE extent
// and truncates the compare window, CLAUDE.md's third `--len` trap). asmscore --len 156:
// total 94355, insns 58/59.
//
// ⭐ CONTENT-COMPLETE, and the residual is the SAME operand-order difference that defines the
// twins -- so this compile produced 0x435c00's twin, not this one. v457 established that WHICH
// twin a given text yields is decided by the TU and not by the source (the identical text gave
// 0x435c00's order in src/PlacedObjRegistryMaybe.cpp and 0x4361e0's in src/DecorActor.cpp);
// this TU turns out to sit on 0x435c00's side. Concretely: in both half-word arms the original
// loads pOther (`[esp+0x14]`) before pObj, and materializes the unsigned arm's difference
// through a `mov ecx,edx` / `mov eax,esi` shuffle pair -- the extra instruction -- where this
// compile loads pObj first and subtracts in place.
//
// NOT chased, deliberately. The only source edits that flip the load order are temps that
// spell out "read pOther first", and the whole point of the twin pair is that the two .objs
// hold IDENTICAL source; writing the two copies differently to make both match would encode a
// compiler artifact as a source difference. It stays claimed here because this IS its .obj
// (0x436040/0x436140/0x436280/0x4362b0 are all in this file) and because the claim is what
// documents the finding. Retry only if the twin-selection mechanism itself is ever understood.
int RoadVehicleRegistryMaybe::CompareEntriesMaybe(void *pObj, void *pOther) {
    int nResult;
    int nKeyType = nSortKeyTypeMaybe;
    switch (nKeyType) {
    case -4:
    case -3:
        nResult = *(int *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(int *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    case -2:
        nResult = *(short *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(short *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    case -1:
        nResult = *(unsigned short *)((char *)pObj + nSortKeyOffsetMaybe) -
                  *(unsigned short *)((char *)pOther + nSortKeyOffsetMaybe);
        break;
    default:
        nResult = memcmp((char *)pObj + nSortKeyOffsetMaybe,
                         (char *)pOther + nSortKeyOffsetMaybe, nKeyType);
        break;
    }
    if (nResult == 0) {
        nResult = (char *)pObj - (char *)pOther;
    }
    return nResult;
}

#ifdef LOCO_PORT
// PORT SCAFFOLDING -- compiled only in a `-D LOCO_PORT` build, byte-neutral for the match build
// (the whole block preprocesses away), and NOT part of the byte-match product.
//
// src/DecorObjMgrMaybe.h's PlacedObjCollectionMaybe re-declares this family's shared slots so
// its own dispatch can name them over a second element type -- see the note above slot 0: one
// hand-written base, ONE compiled body at 0x435d10, reached by both instantiations. That models
// the original exactly, and each declaration carries its own call-site marker, so nothing in the
// byte-match ever notices that the second spelling has no definition of its own.
//
// A LINKED build does notice. `?ReserveMaybe@PlacedObjCollectionMaybe@@UAEXI@Z` is defined
// nowhere, so link/gen_stubs.py supplies a `ret` for it -- and because the collection's own
// constructor calls it (devirtualized, right between `pArrayMaybe = 0` and `nCountMaybe = 0`),
// EVERY registry DecorObjMgrMaybe builds comes out with pArrayMaybe == NULL and
// nCapacityMaybe == 0 instead of the 100 slots it asked for. Forwarding to the single real body
// is what the original binary does with one address in two vtables.
//
// The cast is sound by layout, not by luck: vptr at +0, the array pointer at +4, the ALLOCATED
// count at +8 in both classes (DecorObjMgrMaybe.h says so at nCapacityMaybe's own declaration),
// and this body only ever moves pointers and compares the two counts -- it never names T. That
// type-independence is exactly why one shared copy serves every instantiation.
//
// ⚠ This is slot 0 ONLY. The same header stubs twelve more slots of this class, and they are NOT
// all safe to forward the same way: slots 9/10/13 (SetCopyAtMaybe/SetAtMaybe/Add) are the ones
// that genuinely name the element type -- the base's slot 9 heap-copies a TilePlacedObj at its
// exact 0x10c -- and slot 3 is named for the DERIVED tail-shifting override (0x4241e0), not the
// base's non-shifting 0x4356b0. Each of those needs its own address evidence before it is wired
// up; see the pickup block in CLAUDE.md.
void PlacedObjCollectionMaybe::ReserveMaybe(unsigned int nCapacity) {
    // The cast IS the model here, not an evasion of one: the original has ONE compiled body in
    // TWO vtables over two element types, and C++ cannot express "this declaration and that one
    // are the same function". The port build says it with a cast between two layout-identical
    // views, inside a block the match build never sees.
    ((Obj0x477758Base *)this)->Obj0x477758Base::ReserveMaybe(nCapacity); // idiom-exempt: one original address, two declared spellings; port-only
}

// --- slots 7 / 8 / 11 / 12, the same defect and the same fix as slot 0 above ----------------
// Found v564 by reading link/stubs.cpp's stub_calls.log from a `-s` boot: `Count` was the single
// hottest stub in the whole run (3480 calls) with `GetAt` right behind it (3476), both called
// once per registry entry per frame. A stub returns 0 / NULL, so DecorObjMgrMaybe's registry
// walks saw an EMPTY collection on every frame of the run -- no ambient world actor was ever
// ticked or drawn. Same root cause as slot 0: the header re-declares this family's shared slots
// over a second element type, nothing defines that second spelling, and the compiler puts the
// generated stub straight into the class's vtable.
//
// All four are in the SAFE, type-independent class the slot-0 note draws the line around -- they
// move pointers and compare the two counts, and none of them names T, so one shared body serves
// both instantiations in the original too. Slots 9/10/13 and slot 3 remain deliberately
// unforwarded: those DO name the element type (or name the derived override), and each needs its
// own address evidence first.
unsigned int PlacedObjCollectionMaybe::Count() {
    return ((Obj0x477758Base *)this)->Obj0x477758Base::Count(); // idiom-exempt: one original address, two declared spellings; port-only
}
void *PlacedObjCollectionMaybe::GetAtMaybe(unsigned int nIndex) {
    return ((Obj0x477758Base *)this)->Obj0x477758Base::GetAtMaybe(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
// Slot 8 is a pure forward to slot 7 THROUGH THE VTABLE in the original (0x424030), and it is
// written that way here for the same reason: an override of slot 7 has to change what slot 8's
// callers see. `GetAtMaybe(nIndex)` below is a virtual call on `this`, so it lands on whatever
// slot 7 the live object actually carries -- NOT unconditionally on the forwarder above.
void *PlacedObjCollectionMaybe::GetAt(int nIndex) {
    return GetAtMaybe(nIndex);
}
char PlacedObjCollectionMaybe::IsSlotOccupiedMaybe(unsigned int nIndex) {
    return (char)((Obj0x477758Base *)this)->Obj0x477758Base::IsSlotOccupiedMaybe(nIndex); // idiom-exempt: one original address, two declared spellings; port-only
}
// The DERIVED half's slot-11 override -- 0x424000, the LIVE count, against the base's 0x424010
// capacity above. See the declaration's own note in src/DecorObjMgrMaybe.h for why the port needs
// both: every registry these consumers touch ends up carrying THIS vtable.
unsigned int PlacedObjRegistryMaybe::Count() {
    return ((Obj0x477758 *)this)->Obj0x477758::Count(); // idiom-exempt: one original address, two declared spellings; port-only
}

// --- slot 3 on the DERIVED half, the same defect with the cast pointing the OTHER way -------
// 0x4241e0 is one address in every derived table in the family, and this header declares it
// TWICE: once here as `Obj0x477758::RemoveAt` (v486 restored that declaration deliberately --
// see its note in src/Obj0x477798Family.h) and once in src/DecorObjMgrMaybe.h as
// `PlacedObjRegistryMaybe::RemoveAtShiftingTail`. The BODY landed under the second spelling, so
// unlike every forwarder above this one casts away from the canonical family name rather than
// toward it. Same argument, same soundness: the body walks pointers, memmoves the tail and
// decrements the live count, and never names T.
//
// Why it matters now: slot 3 is what slot 4 (0x4356e0, `delete RemoveAt(idx)`) dispatches
// through, and slot 4 is what slot 6's derived form drives -- so with slot 3 stubbed, the
// slot-6 forwarders below would have spun forever. `Obj0x477758::RemoveAndDeleteAll` is
// `while (m_0c != 0) RemoveAndDeleteAt(m_0c - 1);`, and a `ret` stub in slot 3 never lets the
// live count come down. Wiring slot 6 without wiring slot 3 first is a HANG, not a leak.
TilePlacedObj *Obj0x477758::RemoveAt(unsigned int idx) {
    return (TilePlacedObj *)((PlacedObjRegistryMaybe *)this)->PlacedObjRegistryMaybe::RemoveAtShiftingTail(idx); // idiom-exempt: one original address, two declared spellings; port-only
}

// --- slots 10 and 17, the ADD chain's two remaining holes on BOTH tiers ---------------------
// The slot-0 note above draws its "needs its own address evidence first" line around slots
// 9/10/13; v572 supplies that evidence for slot 10 out of this file's own leaf notes, and the
// resulting forwarders are what make the family's sorted Add work at all.
//
// The chain, all of it dispatched indirectly through whatever table the live object carries:
//   slot 13 InsertInSortedPositionMaybe (0x4362b0, shared)  -- has a real body here
//     -> slot 12 IsSlotOccupiedMaybe / slot 7 GetAtMaybe    -- forwarded above
//     -> slot 18 CompareEntriesMaybe                        -- real body, src/PlacedObjRegistryMaybe.cpp
//     -> slot 17 InsertAtMaybe                              -- STUB until now
//          -> slot 0 ReserveMaybe                           -- forwarded above
//          -> slot 10 SetAtMaybe                            -- STUB until now
// With 17 and 10 stubbed, slot 13 ran its scan, found the insertion point, and then dropped the
// element on the floor without ever raising the live count -- which is exactly the symptom the
// EffectSpawner side showed (see src/EffectSpawner.cpp's port block).
//
// UNLIKE every forwarder above, slot 10 is NOT one address in both tables: it is the family's
// four-way per-instantiation split (base 0x4359a0 / 0x436040, derived 0x435a10 / 0x4360b0, and
// 0x424170 / 0x4246f0 / 0x424290 / 0x424790 on the effect side). All eight bodies are the SAME
// SOURCE TEXT -- the leaf note above this file's WalkerCollectionMaybe explains why they stayed
// distinct addresses anyway (each names its own .obj's copy of the -1.1 growth constant, a
// relocation difference, not a source one). So forwarding to one leaf is faithful in a way the
// base/derived split is not: the two TIERS differ by a real `nIndex > nCountMaybe` reject and
// must stay apart, while the two INSTANTIATIONS within a tier do not differ at all.
// Slot 17 is a single address per instantiation (0x435b60 / 0x436140) with no tier split at all
// -- it exists only on the derived table -- so it gets one forwarder.
//
// Slot 9 stays deliberately unforwarded: it is the one member that CONSTRUCTS an element, so it
// is the one that genuinely names T, and no cast can stand in for that.
DecorActorBase *PlacedObjCollectionMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    return ((WalkerCollectionMaybe *)this)->WalkerCollectionMaybe::SetAtMaybe(nIndex, pItem); // idiom-exempt: one source, eight per-instantiation addresses; port-only
}
DecorActorBase *PlacedObjRegistryMaybe::SetAtMaybe(unsigned int nIndex, DecorActorBase *pItem) {
    return ((WalkerRegistryMaybe *)this)->WalkerRegistryMaybe::SetAtMaybe(nIndex, pItem); // idiom-exempt: one source, eight per-instantiation addresses; port-only
}
int PlacedObjRegistryMaybe::InsertAtMaybe(unsigned int nIndex, void *pObj) {
    return ((WalkerRegistryMaybe *)this)->WalkerRegistryMaybe::InsertAtMaybe(nIndex, pObj); // idiom-exempt: one source, two per-instantiation addresses; port-only
}
#endif
