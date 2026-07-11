// TimeOfDayMaybe -- RENAMED v482 from the address-based name `Obj0x477840` (grep that if you
// are following an older note). The evidence is TimeOfDay_IsTimeInWindowMaybe below, which
// converts this record's m_8/m_4 pair into a minute-of-day and range-tests it against a
// struct tm, plus the two embeds BigObj carries at +0x534/+0x548 that its "shifts" ini token
// fills -- a pair of these IS a half-open open/close window. Field semantics PROVEN
// 2026-07-29 (v494, by the [TimeEvents] sscanf argument ORDER, which passes &m_8/&m_4 for
// ee.ini's hr/min conversion slots): m_8 = hour, m_4 = minute (so 0x412790's m_4 + m_8*60
// minute-of-day is correct), m_c = day-of-month, m_10 = month (0x412670 validates m_10 in
// 0..11 and adds m_c to a month-offset table; the parse decrements m_10, so ini month 0 = any).
// ⭐ CORROBORATED v553 -- the hour/minute assignment NO LONGER rests on that one sscanf ordering.
// A second, independent witness: Obj0x4779e0::ParseTokenField's "shifts" handler sscanf's four
// longs into `&+0x53c, &+0x538, &+0x550, &+0x54c` of BigObj's two embeds -- m_8 before m_4, twice
// -- so that .dat line reads "openHour openMin closeHour closeMin". Two unrelated parsers in two
// unrelated files, both putting m_8 in the hour slot. `Maybe` is now kept only for the CLASS name
// (what the authors called this record is still unknown), not for the field semantics. The ten
// field names of BigObj's two embeds were derived from this layout -- see src/CursorDesc.h's
// +0x534 block, which also records which embed is `pOpen` and which is `pClose`.
// Small polymorphic value/list-node record (vtable 0x477840; ctor 0x412620
// sentinel-inits its 4 dword fields to -1, dtor 0x412660 is the bare vptr-store stub).
// Embedded twice by Obj0x4779e0 (at +0x534/+0x548 -- modeled as RAW LONG fields in
// src/CursorDesc.h, NOT as real embeds: including this header from CursorDesc.h rotates
// DPlaySessionMgr.cpp's /Og TU state and breaks SelectGridCellFromPointMaybe's EXACT, v331
// bisect; the "shifts" keyword's sscanf writes m_8/m_4 of each) and used in pairs by the
// ScriptEventLoader parsers
// (0x41fb20/0x41fbe0, untranscribed "%ld"-repeated text-line parsers pushing pairs onto
// two parallel lists -- plausible timetable/scripted-event loader). Real field semantics
// unidentified, so the address-based name is kept (docs/subsystems.md "Small object
// families and utilities"). Moved out of src/phase2_probe2.cpp 2026-07-22 (v322).
#pragma once

class TimeOfDayMaybe {
public:
    int m_4;
    int m_8;
    int m_c;
    int m_10;
    TimeOfDayMaybe();
    // ⚠ CORRECTED v546. This class used to carry a fabricated `virtual void Method0()` as its
    // slot-0 placeholder with a NON-virtual dtor beside it. There is no Method0: the dword at
    // 0x477840 is 0x412640, and 0x412640 is ??_GTimeOfDayMaybe -- the compiler-generated scalar
    // deleting destructor. The destructor IS this class's only virtual, and the placeholder was
    // an invented second name for a slot that already had an owner (exactly the failure
    // CLAUDE.md's vtable-slot rule describes). Layout is unaffected: MSVC puts the vptr at
    // offset 0 either way, so m_4 still starts at +4.
    virtual ~TimeOfDayMaybe();
};

// 0x412710, extern (free function, __cdecl) -- "is the wall clock currently inside the
// [pOpen .. pClose] window": converts each record's m_8/m_4 pair and pNow's own tm_hour/tm_min
// into a minute-of-day and tests containment, handling a window that wraps past midnight
// (pClose < pOpen). Returns 0 while either record still holds its -1 ctor sentinel -- i.e.
// "no window configured" reads as closed. The pair of records really is a HALF-OPEN SHIFT:
// BigObj's own +0x534/+0x548 "shifts" embeds are its only known caller-supplied operands
// (src/CursorDesc.h models them as raw longs, so that caller casts).
unsigned char TimeOfDay_IsTimeInWindowMaybe(struct tm *pNow, TimeOfDayMaybe *pOpen,
                                              TimeOfDayMaybe *pClose);
