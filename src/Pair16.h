#pragma once

// A resolved (x, y)-shaped pair returned packed into one dword (lo=x, hi=y) by the
// NetSessionEventQueue edge-placement quartet -- see src/NetSessionEventQueue.cpp
// and phase2_probe3.cpp's own "0x41d920 / 0x41d980" write-up for the calling shape.
struct Pair16 {
    short lo;
    short hi;
};
