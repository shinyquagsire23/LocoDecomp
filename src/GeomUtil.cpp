// GeomUtil -- small free-function geometry helpers shared across subsystems (moved out of
// phase2_probe3.cpp 2026-07-22, v322). No single owning class: CalcSqDist's callers span the
// WorldBoard tile-neighbor search (WorldBoardMaybe::GetNeighborObject 0x4579d0, the nearby
// scan at 0x457ce0) AND the train-AI wander/destination pickers (FUN_004327b0, FUN_00434260,
// FUN_00453450), so it is modeled as a plain __cdecl utility, matching the binary.

#include "GeomUtil.h"

// FUNCTION: LOCO 0x45c7a0
int CalcSqDist(int x1, int y1, int x2, int y2) {  // TODO: sync
    return (y1 - y2) * (y1 - y2) + (x1 - x2) * (x1 - x2);
}

// Point-on-segment test: the x-span bracket plus a zero 2D cross product. The cross product is
// written OUT IN EXPANDED (determinant) form rather than the tidier
// `(bx - ax) * (py - ay) == (by - ay) * (px - ax)` -- cl does no such algebra, and the original
// multiplies `bx * ay` and `by * ax` as standalone products, so the expanded spelling is the
// original's own. The `? true : false` on the cross-product test is CODEGEN.md's item-(5b)
// return-statement lever, and it is load-bearing: a plain `return a == b;` widens the result
// (`xor ecx,ecx; sete cl; mov al,cl`) where the original has a bare `sete al`. Collapsing the
// whole body into one `&&` chain is WORSE -- it turns the test into a branch (`jne`/`mov eax,1`)
// instead of the `sete`; do not re-try it.
// FUNCTION: LOCO 0x45c7c0
unsigned char IsPointOnSegmentMaybe(int px, int py, int ax, int ay, int bx, int by) {  // TODO: sync
    if (px >= (ax < bx ? ax : bx) && px <= (ax > bx ? ax : bx)) {
        return (bx - ax) * py == (by - ay) * px + bx * ay - by * ax ? true : false;
    }
    return 0;
}

// FUNCTION: LOCO 0x45c820
// One-instruction pointer dereference helper (Ghidra: LocoBitmap::FUN_0045c820) -- the
// original TU's trailing leaf, kept here by address order.
int DerefIntMaybe(int *p) {  // TODO: sync
    return *p;
}
