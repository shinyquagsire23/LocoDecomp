// PlacedObjRegistryMaybe -- the two-instantiation sorted registry DecorObjMgrMaybe embeds twice
// (regCategory7Maybe/regCategory8Maybe, see src/DecorObjMgrMaybe.h). Its own methods live here
// rather than in src/DecorActor.cpp -- SPLIT OUT 2026-07-27 (v457) because they are the tail of
// that TU's .text run and were paying for its churn: adding ANY `#include` line to
// DecorActor.cpp (even of a file containing nothing but `#pragma once` -- verified with an empty
// probe header) rotated CompareEntriesMaybe from MATCH to DIFF(76) and cost 156 B. A bare
// comment line does NOT do it, and moving the new code to the end of the file does NOT help, so
// it is the include DIRECTIVE COUNT itself steering cl 5.0's /Og tie-breaks, not any declaration
// the header brings in. Splitting the class out is the structural fix: this TU's own include set
// is tiny and stable, so DecorActor.cpp can grow freely from here on.
#include <string.h> // memcmp

#include "DecorActor.h"
#include "DecorObjMgrMaybe.h"

// FUNCTION: LOCO 0x435aa0
// The registry's in-place quicksort over the INCLUSIVE range [nLo, nHi] -- a textbook Hoare
// partition around the midpoint entry, then recurse on both halves. Both the ordering predicate
// and the recursion go through the vtable (slots 18 and 15), because the two instantiations
// sharing this body order their entries differently.
void PlacedObjRegistryMaybe::SortRangeMaybe(int nLo, int nHi) {
    DecorActorBase *pPivot = pArrayMaybe[(nHi + nLo) / 2];
    int i = nLo;
    int j = nHi;
    do {
        while (CompareEntriesMaybe(pArrayMaybe[i], pPivot) < 0) {
            i++;
        }
        while (CompareEntriesMaybe(pPivot, pArrayMaybe[j]) < 0) {
            j--;
        }
        if (i > j) {
            break;
        }
        DecorActorBase *pSwap = pArrayMaybe[i];
        pArrayMaybe[i] = pArrayMaybe[j];
        pArrayMaybe[j] = pSwap;
        i++;
        j--;
    } while (i <= j);
    if (nLo < j) {
        SortRangeMaybe(nLo, j);
    }
    if (i < nHi) {
        SortRangeMaybe(i, nHi);
    }
}

// FUNCTION: LOCO 0x435c00
// The registry's ordering predicate, driven by the (offset, type) sort key SetSortParamsAndSortMaybe
// stashed at +0x10/+0x14: the type code selects how to read the key sitting nSortKeyOffsetMaybe
// bytes into each entry -- -4 and -3 both mean a 4-byte int, -2 a signed short, -1 an unsigned
// short, and any positive N an N-byte memcmp (which VC5 expands to `repz cmpsb` inline). Equal
// keys fall back to comparing the two entry POINTERS, so the order is total and the quicksort
// above can never loop on a run of duplicates.
//
// ⚠ This body exists TWICE in the image -- 0x435c00 (claimed here) and 0x4361e0 -- from identical
// source separately compiled into two .objs, which is why the linker did not fold the two
// COMDATs. They differ ONLY in the operand evaluation order of the two half-word arms: 0x435c00
// loads pObj before pOther, 0x4361e0 the reverse.
//
// ⭐ WHICH TWIN THE SAME SOURCE PRODUCES IS DECIDED BY THE TU, NOT BY THE SOURCE -- established
// 2026-07-27 (v457), and it CORRECTS two earlier conclusions. This exact text compiled inside
// src/DecorActor.cpp gives 0x4361e0's order (156 B); compiled here, in a small TU of its own, it
// gives 0x435c00's (152 B) and MATCHES. So the earlier reading -- that `a - b` picks the operand
// order and therefore pins which twin is claimable -- was wrong, and so was v451's verdict that
// 0x435c00 is "structurally unreachable": it was only unreachable from the TU it was being tried
// in. Nothing about the source changed to land it.
//
// 0x435c00 is also the RIGHT twin for this file to claim on layout grounds: it sits directly
// after SortRangeMaybe (0x435aa0) above, and adjacent .text is how .obj boundaries show
// themselves -- the pair is one original translation unit. 0x4361e0 is the copy belonging to
// some OTHER .obj and stays unclaimed, since our single PlacedObjRegistryMaybe models both
// instantiations and there is exactly one declaration to hang a definition off.
int PlacedObjRegistryMaybe::CompareEntriesMaybe(void *pObj, void *pOther) {
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
