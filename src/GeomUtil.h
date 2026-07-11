// Declarations for the small free-function geometry helpers in src/GeomUtil.cpp -- see that
// file's own header comment for why they are modeled as plain __cdecl utilities with no owning
// class (their callers span the WorldBoard tile search and the actor/train AI).
#pragma once

// World PIXEL coordinate -> board TILE coordinate: 16 pixels per tile, with every negative
// coordinate collapsing onto -1 rather than rounding toward zero (so the out-of-board case is a
// single value the tile-grid bound checks reject). Consolidated here 2026-07-27 from the four
// identical TU-local copies (src/WalkerActor.cpp, src/DecorActor.cpp, src/RoadVehicleActor.cpp,
// src/NameAnchorMaybe.cpp) that CLAUDE.md had been tracking as debt; a macro adds no
// declaration, so it does not move any consumer's declaration-count parity.
#define WORLD_TO_TILE(v) ((short)((v) < 0 ? -1 : (v) >> 4))

// 0x45c7a0 -- squared distance between two points. Body in src/GeomUtil.cpp.
int CalcSqDist(int x1, int y1, int x2, int y2);

// 0x45c7c0, extern -- true iff (px, py) lies exactly ON the segment (ax, ay)-(bx, by): the
// x-span test `min(ax,bx) <= px <= max(ax,bx)` AND a zero 2D cross product. Returns only AL
// (the original leaves EAX's upper 3 bytes holding `bx`, the usual CONCAT31 tell), so every
// caller tests it as a byte.
//
// ⚠ Ghidra has this address-boxed into the `LocoBitmap` namespace (`LocoBitmap::
// IsPointOnSegmentMaybe`) purely by .text adjacency -- it is a free __cdecl function and
// touches no LocoBitmap state. Kept here rather than in src/LocoBitmap.h so its real shape is
// visible; renaming it out of that namespace on the Ghidra side is a clean follow-up.
unsigned char IsPointOnSegmentMaybe(int px, int py, int ax, int ay, int bx, int by);
