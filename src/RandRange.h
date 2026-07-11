// The engine's one inclusive-range random draw, shared by at least two unrelated subsystems
// (AnimEffectObj's placement modes and ScreenSaver::GetLayoutFileName's layout picker). Every
// call site in the original shows the same shape: the (hi - lo + 1) divisor is computed FIRST
// and the `lo` value is kept as the result when that divisor is zero (a degenerate inverted or
// empty range), instead of calling rand(). It is never a real function -- there is no COMDAT
// for it anywhere in Loco.exe -- so it was a macro or an inline in the original source.
//
// ⚠ It does NOT normalize an inverted range itself: call sites that can receive either order
// branch on the sign THEMSELVES and pass the two arguments swapped in each arm (see
// AnimEffectObj.cpp's case 'B'/'T' pairs and GetLayoutFileName's nCount-1 sign test). That
// per-call-site branch is source, not codegen.
#pragma once

#include <stdlib.h> // rand()

#define RAND_RANGE_MAYBE(lo, hi) (((hi) - (lo) + 1 == 0) ? (lo) : (rand() % ((hi) - (lo) + 1) + (lo)))
