// TimeOfDayMaybe ctor/dtor bodies -- see the header for the class writeup.
#include "TimeOfDayMaybe.h"

#include <time.h>

// 0x47e410 -- cumulative days-before-month, non-leap: the day-of-year both window predicates
// below build as `table[month] + dayOfMonth`. Exactly 24 bytes in the original (a string
// begins at 0x47e428), and both readers live in this TU. Non-const because the original's
// copy is in .data, not .rdata.
//
// Defined rather than merely declared for the same reason as src/WorldBoardMaybe.cpp's
// DAT_0047f108 (v566): an UNDEFINED data symbol becomes one of link/gen_stubs.py's generated
// data stubs, and those sit in an all-zero .bss mirror -- so every month would have read as
// offset 0 and every date window would have compared the wrong day-of-year, silently.
// tools/datastubs.py ranks what is left.
unsigned short g_aMonthDayOffsetMaybe[12] = {0,   31,  59,  90,  120, 151,
                                             181, 212, 243, 273, 304, 334};

// FUNCTION: LOCO 0x412620
TimeOfDayMaybe::TimeOfDayMaybe() {
    m_4 = -1;
    m_8 = -1;
    m_c = -1;
    m_10 = -1;
}

// FUNCTION: LOCO 0x412640 (??_GTimeOfDayMaybe scalar deleting dtor -- compiler-generated; this
// is vtable slot 0, the class's ONLY virtual)
// PARTIAL, DIFF at 30 B against 32: the original inlines the dtor's vptr store into the thunk
// where ours emits `call ??1TimeOfDayMaybe`. Same intrinsic residual as ??_GCarNetState
// (src/CarNetState.cpp) and NOT worth the same fix -- an inline `~TimeOfDayMaybe() {}` would
// close it, but 0x412660 has ten real out-of-line callers in the image (~Obj0x4779e0 twice,
// ~LoadEventRecordMaybe twice, ~TimedEventRecordMaybe twice and four EH unwind funclets), so
// the out-of-line body below is what the original actually had.

// FUNCTION: LOCO 0x412660
TimeOfDayMaybe::~TimeOfDayMaybe() {}

// FUNCTION: LOCO 0x412670 (Ghidra: TimeOfDay_IsDateInWindowMaybe -- already named there)
// The DATE-only half of the window predicate: both records must hold a real month
// (m_10 in 0..11 -- here a -1 month sentinel reads as CLOSED, unlike 0x412790 where it
// reads as "any date") and a real day-of-month (m_c != -1); pNow's own fields are
// trusted unguarded. Day-of-year = m_c + month-offset-table[m_10] (table at 0x47e410,
// same as 0x412790), then the same plain/wrap containment test -- NOTE the wrap arm
// here tests nNow <= nOpen || nClose <= nNow (always true when open > close, so a
// wrapping date window never closes; // sic: original logic, reproduced, cf.
// 0x412790's minute arm which tests the inverse).
unsigned char __cdecl TimeOfDay_IsDateInWindowMaybe(tm *pNow, TimeOfDayMaybe *pOpen,
                                                    TimeOfDayMaybe *pClose) {
    extern unsigned short g_aMonthDayOffsetMaybe[12]; // DAT_0047e410

    if (pOpen->m_10 < 0 || pOpen->m_10 > 0xb) {
        return 0;
    }
    if (pClose->m_10 < 0 || pClose->m_10 > 0xb) {
        return 0;
    }
    if (pOpen->m_c == -1 || pClose->m_c == -1) {
        return 0;
    }
    int nOpenDay = g_aMonthDayOffsetMaybe[pOpen->m_10] + pOpen->m_c;
    int nNowDay = g_aMonthDayOffsetMaybe[pNow->tm_mon] + pNow->tm_mday;
    int nCloseDay = g_aMonthDayOffsetMaybe[pClose->m_10] + pClose->m_c;
    if (nOpenDay <= nCloseDay) {
        return nOpenDay <= nNowDay && nNowDay <= nCloseDay;
    }
    // sic: inverted wrap test -- see the comment block above.
    return nOpenDay >= nNowDay || nNowDay >= nCloseDay;
}

// FUNCTION: LOCO 0x412710 (Ghidra: TimeOfDay_IsTimeInWindowMaybe -- already named there)
// The TIME-only half of the family, and byte-for-byte the same tail 0x412790 runs after its
// date test passes: the four-sentinel gate, then the minute-of-day (m_4 + m_8*60) containment
// test with the same plain/wrap split. Unlike 0x412670's date arm this one is NOT `sic` --
// the wrap arm here tests the sensible `nOpen <= nNow || nNow <= nClose`.
unsigned char __cdecl TimeOfDay_IsTimeInWindowMaybe(tm *pNow, TimeOfDayMaybe *pOpen,
                                                    TimeOfDayMaybe *pClose) {
    if (pOpen->m_4 == -1 || pClose->m_4 == -1 || pOpen->m_8 == -1 || pClose->m_8 == -1) {
        return 0;
    }
    int nOpenMinutes = pOpen->m_4 + pOpen->m_8 * 60;
    int nNowMinutes = pNow->tm_min + pNow->tm_hour * 60;
    int nCloseMinutes = pClose->m_4 + pClose->m_8 * 60;
    if (nOpenMinutes <= nCloseMinutes) {
        return nOpenMinutes <= nNowMinutes && nNowMinutes <= nCloseMinutes;
    }
    return nOpenMinutes <= nNowMinutes || nNowMinutes <= nCloseMinutes;
}

// FUNCTION: LOCO 0x412790 (Ghidra: TimeOfDay_IsDateTimeInWindowMaybe -- already named there)
// The full DATE+TIME window predicate: pNow is a CRT `struct tm *` whose tm_min/tm_hour/
// tm_mday/tm_mon fields alias the m_4/m_8/m_c/m_10 slots of the open/close records. First
// the day-of-year wrap test (m_c + month-offset-table[m_10], table at 0x47e410 = cumulative
// days before month, non-leap; offsets only apply when BOTH records have a real month, else
// all three stay 0 -- a -1 month sentinel reads as "any date"), then the sentinel gate on
// all four time fields, then the minute-of-day wrap test (m_4 + m_8*60). Both wrap tests
// are the same shape: [open..close] either plain (open <= close: containment) or wrapping
// past midnight/New Year (open > close: outside (close..open) instead). Declared TU-locally
// in src/ScriptEventLoader.cpp (its only caller) because of the shared-header rotation
// hazard; defined here with the rest of the family. EXACT MATCH. Two baked-in levers, do
// not undo: (a) the day-of-year values are += ACCUMULATED into the offset variables (not
// summed into fresh tOpen/tNow/tClose locals -- that spelling parks the offsets in
// different registers and leaves a setcc-folded minute half, DIFF(129)); (b) the three
// += statements are written open/now/close but /Og emits them now/open/close -- writing
// them in the EMITTED order flips the esi/ebx load pairing instead (DIFF(4)); the source
// order here is the one that reproduces the original.
unsigned char __cdecl TimeOfDay_IsDateTimeInWindowMaybe(tm *pNow, TimeOfDayMaybe *pOpen,
                                                        TimeOfDayMaybe *pClose) {
    extern unsigned short g_aMonthDayOffsetMaybe[12]; // DAT_0047e410

    int nOffOpen = 0;
    int nOffNow = 0;
    int nOffClose = 0;
    if (pOpen->m_10 >= 0 && pClose->m_10 >= 0) {
        nOffOpen = g_aMonthDayOffsetMaybe[pOpen->m_10];
        nOffNow = g_aMonthDayOffsetMaybe[pNow->tm_mon];
        nOffClose = g_aMonthDayOffsetMaybe[pClose->m_10];
    }
    nOffOpen += pOpen->m_c;
    nOffNow += pNow->tm_mday;
    nOffClose += pClose->m_c;

    unsigned char bInDate;
    if (nOffOpen <= nOffClose) {
        if (nOffOpen <= nOffNow && nOffNow <= nOffClose) {
            bInDate = 1;
        } else {
            bInDate = 0;
        }
    } else {
        if (nOffOpen <= nOffNow || nOffNow <= nOffClose) {
            bInDate = 1;
        } else {
            bInDate = 0;
        }
    }
    if (bInDate == 0) {
        return 0;
    }
    if (pOpen->m_4 == -1 || pClose->m_4 == -1 || pOpen->m_8 == -1 || pClose->m_8 == -1) {
        return 0;
    }
    int nOpenMinutes = pOpen->m_4 + pOpen->m_8 * 60;
    int nNowMinutes = pNow->tm_min + pNow->tm_hour * 60;
    int nCloseMinutes = pClose->m_4 + pClose->m_8 * 60;
    if (nOpenMinutes <= nCloseMinutes) {
        if (nOpenMinutes <= nNowMinutes && nNowMinutes <= nCloseMinutes) {
            return 1;
        }
        return 0;
    }
    if (nOpenMinutes <= nNowMinutes || nNowMinutes <= nCloseMinutes) {
        return 1;
    }
    return 0;
}
