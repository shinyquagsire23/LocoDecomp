// ScriptEventLoader -- the world-session manager singleton (DAT_004a99b0, static-init thunk
// 0x45c650 / atexit thunk 0x45c670). The class embeds the game's wall clock: its +0x4 field IS
// the address every other TU knows as `g_dwGameTick` (DAT_004a99b4), refreshed by the timer
// proc at 0x45c3c0 calling _time() on it, so "tick" values in here are wall-clock seconds and
// the % 10 / % 20 / % 60 pumps below are every-10s / 20s / 60s.
//
// Its own ctor/dtor/??_G (0x41f480/0x41f4b0/0x41f4d0) live in the PRECEDING .obj, which
// src/ScopedTimestampMaybe.cpp already claims under the older, narrower read of the class (its
// vtable 0x4779f4 sits one dword ahead of SplashWnd_Vtbl 0x4779f8 in .rdata, sharing the whole
// SplashWnd slot run from 0x4203a0 on -- the provenance of that shared run is unresolved; the
// 2-virtual model here is what both sides already byte-match with). Unifying the two TUs'
// models is deliberately NOT done this session (ScopedTimestampMaybe.cpp's EXACT is not to be
// rotated); this TU carries the 15 method bodies of the 0x41f4e0..0x4202b0 .obj.
//
// The two record lists it owns are loaded from ee.ini ("<install prefix>ee.ini", see
// loco/extract/ee.ini): [LoadEvents] date-window kind-slot remaps and [TimeEvents] scheduled
// effect spawns. The easter-egg half tracks unlocked decoration kinds in the [EasterEggs]
// section (read/written through g_pIniFile, i.e. LOCO.INI -- see the 0x41f7e0 note).

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AppWindow.h"            // g_pApp->hwndOwner
#include "DecorObjMgrMaybe.h"     // DecorObjMgrMaybe_00485448's category-7/8 registries
#include "DSoundChannel.h"        // g_pInstallPathPrefix
#include "EffectSpawner.h"        // DAT_004fd220
#include "GameWindowWidgetList.h" // g_gameWindowWidgetList + GetItem probe
#include "IniFile.h"              // g_pIniFile, the ee.ini-reading local IniFile
#include "TilePlacedObj.h"        // TilePlacedObj (0x420000's widget-list items)
#include "TimeOfDayMaybe.h"       // the open/close window embeds
#include "UIResources.h"          // g_UIResources
#include "WidgetBase.h"           // AnimDescRefObj0x477488 (0x4202b0's actor)
#include "WorldBoardMaybe.h"      // g_worldBoard.MarkRectDirty

// 0x446030, extern (free function, __cdecl) -- a kind id's category byte, declared TU-locally
// exactly as src/Obj0x4779e0.cpp already declares it.
extern unsigned int __cdecl TileKind_GetCategory(unsigned int kindId);

// TU-local externs (same pattern as src/Main.cpp / src/WorldBoardMaybe.cpp).
extern int g_nScreenState;              // app-state dword (3 = in-game), see src/GameNetMsgQueue.h
// The in-game app-state gate. The `unsigned char` return type is LOAD-BEARING: it is what
// reproduces the original's sete-materialized branch instead of a plain `cmp; je` (cracked in
// v356, see docs/CODEGEN.md). Kept TU-local because adding declarations to a shared header
// rotates other TUs (v340/v355/v356).
inline unsigned char IsInGameModeMaybe() { return g_nScreenState == 3; }
extern int g_forcedSeason;              // DAT_00485230, see src/AppWindow.cpp
extern int DAT_0048523c;                // set to 1 by ApplySeasonalDateWindowsMaybe below
extern unsigned char DAT_00485298;      // cleared at the end of every world idle tick
// The "placement may evict what is already there" master gate, declared TU-locally as in
// src/BuildToolButton.cpp / src/PlacementCursorMaybe.cpp / src/NetSessionEventQueue.cpp.
extern unsigned char DAT_004fd3dc;

// TU-local view of IniFile carrying the WriteInt signature THIS pair of functions needs: the
// egg-unlock paths test the return value (`cmp al,1` straight off the call), which is the
// WritePrivateProfileStringA BOOL the body's tail call leaves in eax -- the shared header's
// `void` signature discards it, and changing that header rotates every consumer. Same
// precedent as the project's other TU-local views (DecorObjMgrPaintView0x456700 &c.).
struct IniFileWriteIntView0x452db0 {
    char WriteInt(const char *section, const char *key, int value);  // 0x452db0
};

// The two sibling window predicates over TimeOfDayMaybe open/close pairs, declared TU-locally
// (src/TimeOfDayMaybe.h owns the third, 0x412710 TimeOfDay_IsTimeInWindowMaybe; adding these to
// that shared header is the same rotation hazard as everywhere else). Both are __cdecl.
// 0x412670 -- DATE half only: m_10 is a 0-based month (-1 = any) and m_c a day-of-month,
// converted to a day-of-year through the month-offset table at 0x47e410; handles a window that
// wraps past New Year.
extern unsigned char __cdecl TimeOfDay_IsDateInWindowMaybe(tm *pNow, TimeOfDayMaybe *pOpen,
                                                           TimeOfDayMaybe *pClose); // 0x412670
// 0x412790 -- the full DATE+TIME window: the 0x412670 day-of-year test, then a minute-of-day
// test computed as m_4 + m_8 * 60. The [TimeEvents] sscanf order PROVES m_8 is the hour field
// and m_4 the minute (the hr/min conversion slots land on &m_8/&m_4 respectively -- the
// arguments push m_8's address ahead of m_4's), so the * 60 is on the right field; an earlier
// note here suspected a masked swap. Wrong. m_c = day-of-month, m_10 = month, settled.
extern unsigned char __cdecl TimeOfDay_IsDateTimeInWindowMaybe(tm *pNow, TimeOfDayMaybe *pOpen,
                                                               TimeOfDayMaybe *pClose); // 0x412790

// LoadEventRecordMaybe -- one [LoadEvents] line: a date window (open/close pair, only the
// m_c day / m_10 month fields are filled) plus a kind-slot remap pair. While the wall clock is
// inside the window, ApplySeasonalDateWindowsMaybe remaps kind slot A to descriptor B (the
// seasonal art swap) and runs the easter-egg unlock check on B. new_alloc(0x34), singly-linked
// through +0x30.
class LoadEventRecordMaybe {
public:
    TimeOfDayMaybe open;                 // +0x00 -- window start (m_c day / m_10 month used)
    TimeOfDayMaybe close;                // +0x14 -- window end
    int nKindSlotIdMaybe;                // +0x28 -- ini field 5: slot to remap ("res id" A)
    int nRemapTargetKindIdMaybe;         // +0x2c -- ini field 6: descriptor to map it to (B)
    LoadEventRecordMaybe *pNextMaybe;    // +0x30
};

// TimedEventRecordMaybe -- one [TimeEvents] line: a full date+time window plus a scheduled
// effect spawn. new_alloc(0x48), singly-linked through +0x44. After firing, +0x34 is re-armed
// to now + jitter where the jitter derives from +0x30 (see ProcessTimedEventsMaybe).
class TimedEventRecordMaybe {
public:
    TimeOfDayMaybe open;                 // +0x00
    TimeOfDayMaybe close;                // +0x14
    int nEffectKindIdMaybe;              // +0x28 -- ini "res id": effect kind to spawn
    short wEffectMobilityMaybe;          // +0x2c -- ini "res fs" (%hd): mobility flag
    short pad0x2e;                       // +0x2e
    int nPeriodMaybe;                    // +0x30 -- ini "period": re-arm jitter base
    int nNextFireTickMaybe;              // +0x34 -- g_dwGameTick value at/after which it fires
    char cSpawnTypeMaybe;                // +0x38 -- ini "type" (%c): 'W'/'S'/'P'/... spawn code
    unsigned char pad0x39[3];            // +0x39
    int xMaybe;                          // +0x3c -- ini "x"
    int yMaybe;                          // +0x40 -- ini "y"
    TimedEventRecordMaybe *pNextMaybe;   // +0x44
};

class ScriptEventLoader {
public:
    ScriptEventLoader();                  // 0x41f480, this file
    virtual ~ScriptEventLoader();         // slot 0 -- src/ScopedTimestampMaybe.cpp owns the bodies
    virtual void Method0();               // placeholder slot -- see the TU header note

    // +0x4 -- the game's wall clock. This IS g_dwGameTick (DAT_004a99b4): the ctor _time()s it
    // and the timer proc at 0x45c3c0 re-_time()s it on every fire, so it tracks wall-clock
    // seconds. Declared `int` -- this TU's own pumps all use signed idiv compares against it.
    int dwGameTick;
    LoadEventRecordMaybe *pLoadEventHeadMaybe;   // +0x8 -- the [LoadEvents] list
    TimedEventRecordMaybe *pTimedEventHeadMaybe; // +0xc -- the [TimeEvents] list
    int nUnlockedEasterEggsMaybe;                // +0x10 -- also the next [EasterEggs] key index

    void TickWorldIdleMaybe();                       // 0x41fd00, this file
    unsigned char ProcessTimedEventsMaybe();         // 0x41ff20, this file
    void DeleteAllEventRecordsMaybe();               // 0x41f4e0, this file
    unsigned char LoadEventScriptsMaybe(const char *pszIniName);    // 0x41f5e0, this file
    unsigned char LoadTimeEventScriptsMaybe(const char *pszIniName); // 0x41f6e0, this file
    unsigned char LoadEasterEggsMaybe(const char *pszIniName);       // 0x41f7e0, this file
    unsigned char RecordEasterEggUnlockMaybe(int kindId);            // 0x41f8e0, this file
    void ApplySeasonalDateWindowsMaybe();                            // 0x41f970, this file
    void ProcessInsertSeqSpawnsMaybe();                              // 0x420000, this file
    void RestoreExpiredActorDescMaybe(AnimDescRefObj0x477488 *pActor); // 0x4202b0, this file
    LoadEventRecordMaybe *ParseFixedRecordMaybe(char *pszLine);        // 0x41fb20, this file
    TimedEventRecordMaybe *ParseRandomizedRecordMaybe(char *pszLine);  // 0x41fbe0, this file
};

// FUNCTION: LOCO 0x41f480
// The static singleton's ctor: zeroes the fixed-list head and the egg count (the +0xc timed
// head is NOT zeroed -- it only ever reads as BSS-zero; engine quirk) and stamps the field the
// rest of the game knows as g_dwGameTick with the current wall-clock time.
ScriptEventLoader::ScriptEventLoader() {
    this->pLoadEventHeadMaybe = 0;
    this->nUnlockedEasterEggsMaybe = 0;
    time((time_t *)&this->dwGameTick);
}

// FUNCTION: LOCO 0x41fd00
// The periodic world idle pump (called from the SelectedObjWidgetMaybe idle path at 0x42ccb6,
// throttled there to a 0x3e7 ms cadence). On every-10-second ticks it processes due time
// events, then drips the whole game-window widget list through WM_USER-range messages 0x403 /
// 0x408 (Sleep(25) between widgets) and pokes a random sample of them with 0x402 (the sample
// size is count/12 clamped to a minimum of 2, one extra iteration). On every-20-second ticks it
// walks both DecorObjMgrMaybe actor registries posting 0x404 (Sleep(1) between actors). On
// every-60-second ticks it posts 0x406 with the tick itself as wParam. Every loop bails the
// moment the app leaves screen state 3.
void ScriptEventLoader::TickWorldIdleMaybe() {
    unsigned int i = 0;
    int nTick = this->dwGameTick;
    int nMod10 = nTick % 10;
    if (nMod10 == 0 && IsInGameModeMaybe()) {
        this->ProcessTimedEventsMaybe();
    }
    if (nMod10 == 0) {
        for (i = 0; i < g_gameWindowWidgetList.nItemCount; i++) {
            if (!IsInGameModeMaybe()) break;
            AnimDescRefObj0x477488 *pWidget =
                ((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)->GetItemImpl(i);
            PostMessageA(g_pApp->hwndOwner, 0x403, (WPARAM)pWidget, 0);
            PostMessageA(g_pApp->hwndOwner, 0x408, (WPARAM)pWidget, 0);
            Sleep(0x19);
        }
    }
    if (nMod10 == 0 && IsInGameModeMaybe()) {
        unsigned int nCount = g_gameWindowWidgetList.nItemCount;
        unsigned int nPicks = nCount / 12;
        if (nPicks < 2) {
            nPicks = 2;
        }
        if (nCount > 0) {
            i = nPicks + 1;
            do {
                AnimDescRefObj0x477488 *pWidget =
                    ((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)
                        ->GetItemImpl(rand() % nCount);
                if (pWidget != 0) {
                    PostMessageA(g_pApp->hwndOwner, 0x402, (WPARAM)pWidget, 0);
                }
                i--;
            } while (i != 0);
            Sleep(0x19);
        }
    }
    int nMod20 = nTick % 20;
    unsigned int j;
    if (nMod20 == 0) {
        PlacedObjRegistryMaybe &reg7 = DecorObjMgrMaybe_00485448.regCategory7Maybe;
        for (j = 0; j < reg7.nCountMaybe; j++) {
            if (!IsInGameModeMaybe()) break;
            PostMessageA(g_pApp->hwndOwner, 0x404, (WPARAM)reg7.GetAt(j), 0);
            Sleep(1);
        }
    }
    if (nMod20 == 0) {
        PlacedObjRegistryMaybe &reg8 = DecorObjMgrMaybe_00485448.regCategory8Maybe;
        for (j = 0; j < reg8.nCountMaybe; j++) {
            if (!IsInGameModeMaybe()) break;
            PostMessageA(g_pApp->hwndOwner, 0x404, (WPARAM)reg8.GetAt(j), 0);
            Sleep(1);
        }
    }
    if (nTick % 60 == 0 && IsInGameModeMaybe()) {
        PostMessageA(g_pApp->hwndOwner, 0x406, nTick, 0);
        Sleep(1);
    }
    DAT_00485298 = 0;
}

// FUNCTION: LOCO 0x41ff20
// Fire the FIRST due [TimeEvents] record (at most one per call; TickWorldIdleMaybe calls this
// every 10 seconds). A record is due when the wall clock is inside its date+time window AND its
// re-arm tick has passed. It spawns the record's effect through the EffectSpawner, then re-arms
// +0x34: period > 0 re-arms to now + rand() % period + 1; period <= 0 to now + rand() %
// (2 - period) + period. The `period == 0` and `period == 2` special cases are both DEAD --
// each is tested on the side of the guard that excludes it (docs/engine-bugs.md material).
// Returns 1 when it fired.
unsigned char ScriptEventLoader::ProcessTimedEventsMaybe() {
    unsigned char bFiredMaybe = 0;
    time_t *pTick = (time_t *)&this->dwGameTick;
    tm *pNow = localtime(pTick);
    TimedEventRecordMaybe *pRec = this->pTimedEventHeadMaybe;
    if (pRec != 0) {
        do {
            if (TimeOfDay_IsDateTimeInWindowMaybe(pNow, &pRec->open, &pRec->close) &&
                pRec->nNextFireTickMaybe < *pTick) {
                DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(
                    pRec->nEffectKindIdMaybe, pRec->wEffectMobilityMaybe, pRec->cSpawnTypeMaybe,
                    pRec->xMaybe, pRec->yMaybe, 1);
                if (pRec->nPeriodMaybe >= 1) {
                    if (pRec->nPeriodMaybe == 0) {  // sic: dead on this side of the
                                                        // >= 1 guard (docs/engine-bugs.md)
                        bFiredMaybe = 1;
                        pRec->nNextFireTickMaybe = *pTick + 1;
                        break;
                    }
                    int nDelay = rand() % pRec->nPeriodMaybe + 1;
                    bFiredMaybe = 1;
                    pRec->nNextFireTickMaybe = *pTick + nDelay;
                    break;
                }
                int nJitter = pRec->nPeriodMaybe;
                if (2 - pRec->nPeriodMaybe != 0) {  // sic: always true here -- the test is
                                                    // period == 2, on the wrong side of the
                                                    // guard (docs/engine-bugs.md)
                    nJitter = rand() % (2 - pRec->nPeriodMaybe) + pRec->nPeriodMaybe;
                }
                bFiredMaybe = 1;
                pRec->nNextFireTickMaybe = *pTick + nJitter;
                break;
            }
            pRec = pRec->pNextMaybe;
        } while (pRec != 0);
    }
    return bFiredMaybe;
}

// FUNCTION: LOCO 0x41fb20
// Parse one [LoadEvents] line ("startDay,startMon,endDay,endMon,kindSlotId,remapTargetId" --
// see loco/extract/ee.ini) into a new LoadEventRecordMaybe and push it on the fixed list. The
// month fields are decremented after the sscanf: ini month 0 means "any" and the window
// predicate's sentinel for that is -1. Note the memset runs AFTER construction, flattening the
// two TimeOfDayMaybe vptrs the embedded ctors just stored (harmless: nothing ever dispatches on
// a record, and the shared node dtor re-stores them on the way out).
LoadEventRecordMaybe *ScriptEventLoader::ParseFixedRecordMaybe(char *pszLine) {
    LoadEventRecordMaybe *pRec = new LoadEventRecordMaybe;
    if (pRec != 0) {
        memset(pRec, 0, sizeof(LoadEventRecordMaybe));
        sscanf(pszLine, "%ld,%ld,%ld,%ld,%ld,%ld", &pRec->open.m_c, &pRec->open.m_10,
               &pRec->close.m_c, &pRec->close.m_10, &pRec->nKindSlotIdMaybe,
               &pRec->nRemapTargetKindIdMaybe);
        pRec->open.m_10--;
        pRec->close.m_10--;
        pRec->pNextMaybe = this->pLoadEventHeadMaybe;
        this->pLoadEventHeadMaybe = pRec;
    }
    return pRec;
}

// FUNCTION: LOCO 0x41fbe0
// Parse one [TimeEvents] line ("startDay,startMon,endDay,endMon,startHr,startMin,endHr,endMin,
// effectKindId,effectMobility(%hd),period,spawnType(%c),x,y" -- ee.ini) into a new
// TimedEventRecordMaybe, seed its first fire tick and push it on the timed list. The seed's
// jitter mirrors ProcessTimedEventsMaybe's re-arm with shifted constants: period < 0 draws
// rand() % (1 - period) + period, period >= 0 draws rand() % (period + 1). The two guards that
// would skip the draw (period == 1 on the negative side, period == -1 on the non-negative side)
// are both DEAD -- each is tested on the branch that excludes it, the same swapped-constants
// quirk as the re-arm.
TimedEventRecordMaybe *ScriptEventLoader::ParseRandomizedRecordMaybe(char *pszLine) {
    TimedEventRecordMaybe *pRec = new TimedEventRecordMaybe;
    if (pRec != 0) {
        memset(pRec, 0, sizeof(TimedEventRecordMaybe));
        sscanf(pszLine, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%hd,%ld,%c,%ld,%ld",
               &pRec->open.m_c, &pRec->open.m_10, &pRec->close.m_c, &pRec->close.m_10,
               &pRec->open.m_8, &pRec->open.m_4, &pRec->close.m_8, &pRec->close.m_4,
               &pRec->nEffectKindIdMaybe, &pRec->wEffectMobilityMaybe, &pRec->nPeriodMaybe,
               &pRec->cSpawnTypeMaybe, &pRec->xMaybe, &pRec->yMaybe);
        pRec->open.m_10--;
        pRec->close.m_10--;
        int nJitter;
        if (pRec->nPeriodMaybe >= 0) {
            nJitter = pRec->nPeriodMaybe + 1;
            if (nJitter != 0) {                 // sic: always true here
                                                    // (docs/engine-bugs.md)
                nJitter = rand() % (pRec->nPeriodMaybe + 1);
            }
        } else {
            nJitter = pRec->nPeriodMaybe;
            if (1 - pRec->nPeriodMaybe != 0) {  // sic: always true here
                                                    // (docs/engine-bugs.md)
                nJitter = rand() % (1 - pRec->nPeriodMaybe) + pRec->nPeriodMaybe;
            }
        }
        pRec->nNextFireTickMaybe = this->dwGameTick + nJitter;
        pRec->pNextMaybe = this->pTimedEventHeadMaybe;
        this->pTimedEventHeadMaybe = pRec;
    }
    return pRec;
}

// FUNCTION: LOCO 0x41f4e0
// The shutdown-side teardown (called from SaveWindowAndCleanExit through AppWindow.cpp's
// __fastcall escape hatch): walk both record lists, unlinking each node from the head BEFORE
// deleting it and re-reading the head after. Leaves both heads 0.
void ScriptEventLoader::DeleteAllEventRecordsMaybe() {
    LoadEventRecordMaybe *pFixed = this->pLoadEventHeadMaybe;
    while (pFixed != 0) {
        this->pLoadEventHeadMaybe = pFixed->pNextMaybe;
        delete pFixed;
        pFixed = this->pLoadEventHeadMaybe;
    }
    TimedEventRecordMaybe *pTimed = this->pTimedEventHeadMaybe;
    while (pTimed != 0) {
        this->pTimedEventHeadMaybe = pTimed->pNextMaybe;
        delete pTimed;
        pTimed = this->pTimedEventHeadMaybe;
    }
}

// FUNCTION: LOCO 0x41f540 (??1LoadEventRecordMaybe@@QAE@XZ)
// Auto-generated by the delete expressions in DeleteAllEventRecordsMaybe above -- this COMDAT
// has no source line of its own.

// FUNCTION: LOCO 0x41f590 (??1TimedEventRecordMaybe@@QAE@XZ)
// Auto-generated by the delete expressions in DeleteAllEventRecordsMaybe above -- this COMDAT
// has no source line of its own.

// FUNCTION: LOCO 0x41f5e0
// Load the [LoadEvents] section of "<install prefix><pszIniName>.ini" (called with "ee" from
// the construction-side 0x406ba0): keys 001, 002, ... until the first empty value, feeding each
// line to ParseFixedRecordMaybe. The local IniFile is constructed over LOCO.INI and then has
// its path REWRITTEN by the second sprintf (a strcpy-through-format quirk) -- that is the whole
// point of it, since ReadString keys off the object's own iniPath. Always returns 1.
unsigned char ScriptEventLoader::LoadEventScriptsMaybe(const char *pszIniName) {
    char szPath[260];
    IniFile ini("LOCO.INI");
    char szKey[260];
    int i = 0;
    sprintf(szPath, "%s%s.ini", g_pInstallPathPrefix, pszIniName);
    sprintf(ini.iniPath, szPath);
    while (1) {
        i++;
        sprintf(szKey, "%03ld", i);
        ini.ReadString("LoadEvents", szKey, "", szPath, 0x104);
        if (szPath[0] == '\0') break;
        this->ParseFixedRecordMaybe(szPath);
    }
    return 1;
}

// FUNCTION: LOCO 0x41f6e0
// The [TimeEvents] twin of LoadEventScriptsMaybe (called with "ee" from the loading screen at
// 0x45deac), feeding ParseRandomizedRecordMaybe. Always returns 1.
unsigned char ScriptEventLoader::LoadTimeEventScriptsMaybe(const char *pszIniName) {
    char szPath[260];
    IniFile ini("LOCO.INI");
    char szKey[260];
    sprintf(szPath, "%s%s.ini", g_pInstallPathPrefix, pszIniName);
    sprintf(ini.iniPath, szPath);
    int i = 0;
    while (1) {
        i++;
        sprintf(szKey, "%03ld", i);
        ini.ReadString("TimeEvents", szKey, "", szPath, 0x104);
        if (szPath[0] == '\0') break;
        this->ParseRandomizedRecordMaybe(szPath);
    }
    return 1;
}

// FUNCTION: LOCO 0x41f7e0
// Re-arm every previously-unlocked easter egg: reads [EasterEggs] keys 1, 2, ... until the
// first 0, setting each named kind descriptor's bButtonVisible and stashing the count. The
// reads go through g_pIniFile (LOCO.INI) even though the function goes to the trouble of
// pointing a LOCAL IniFile at ee.ini first -- the local is never read from, so the [EasterEggs]
// section lives in LOCO.INI, not ee.ini (engine quirk). Always returns 1.
unsigned char ScriptEventLoader::LoadEasterEggsMaybe(const char *pszIniName) {
    IniFile ini("LOCO.INI");  // sic: never read from -- the [EasterEggs] reads below all go
                              // through g_pIniFile (docs/engine-bugs.md)
    char szKey[260];
    char szPath[260];
    sprintf(szPath, "%s%s.ini", g_pInstallPathPrefix, pszIniName);
    sprintf(ini.iniPath, szPath);
    int i = 0;
    while (1) {
        i++;
        sprintf(szKey, "%ld", i);
        int kindId = g_pIniFile->ReadInt("EasterEggs", szKey, 0);
        if (kindId == 0) break;
        CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
        if (pDesc != 0) {
            pDesc->bButtonVisible = 1;
        }
    }
    this->nUnlockedEasterEggsMaybe = i - 1;
    return 1;
}

// FUNCTION: LOCO 0x41f8e0
// Record one easter-egg unlock: if the kind's descriptor resolves and is not already flagged,
// bump the unlocked count, append "EasterEggs"/<count> = <kindId> to LOCO.INI and, only when
// that write succeeds, set the descriptor's bButtonVisible (the "available in the palette"
// flag CursorDesc::IsItemAvailableMaybe reads). Returns 1 exactly when the flag got set. The
// sole caller (0x42577f) is CursorDesc::LoadMaybe's own ini-token path.
unsigned char ScriptEventLoader::RecordEasterEggUnlockMaybe(int kindId) {
    char szKey[260];
    CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
    if (pDesc == 0 || pDesc->bButtonVisible == 1) {
        return 0;
    }
    sprintf(szKey, "%ld",
            this->nUnlockedEasterEggsMaybe = this->nUnlockedEasterEggsMaybe + 1);
    char bOk = ((IniFileWriteIntView0x452db0 *)g_pIniFile)->WriteInt("EasterEggs", szKey, kindId);
    if (bOk == 1) {
        pDesc->bButtonVisible = bOk;
    }
    return bOk;
}

// FUNCTION: LOCO 0x41f970
// Apply every date-windowed [LoadEvents] remap for the current wall clock (called from
// UIResources::LoadKindDatabase's tail at 0x4461c6, right after LoadEasterEggsMaybe). First the
// tm is overridden from g_forcedSeason (the command-line/cheat season override: 1 = April 1,
// 2 = June 11, 3 = October 31, 4 = December 2, 5 = December 25 -- AppWindow.cpp's parser zeroes
// it after use). Then every fixed record inside its date window remaps its kind slot to the
// record's target descriptor and runs the easter-egg unlock on that target. Finally, on
// October 31 the whole 0x1800..0x198e kind range gets dwRenderFlags = 0x400 (the Halloween
// reskin) and the board is dirty-marked if any descriptor resolved.
void ScriptEventLoader::ApplySeasonalDateWindowsMaybe() {
    char szKey[260];
    unsigned char bKindsChangedMaybe = 0;
    DAT_0048523c = 1;
    tm *pNow = localtime((time_t *)&this->dwGameTick);
    switch (g_forcedSeason) {
    case 1:
        pNow->tm_mday = 1;
        pNow->tm_mon = 3;
        break;
    case 2:
        pNow->tm_mday = 11;
        pNow->tm_mon = 5;
        break;
    case 3:
        pNow->tm_mday = 31;
        pNow->tm_mon = 9;
        break;
    case 4:
        pNow->tm_mday = 2;
        pNow->tm_mon = 11;
        break;
    case 5:
        pNow->tm_mday = 25;
        pNow->tm_mon = 11;
        break;
    }
    LoadEventRecordMaybe *pRec = this->pLoadEventHeadMaybe;
    while (pRec != 0) {
        if (TimeOfDay_IsDateInWindowMaybe(pNow, &pRec->open, &pRec->close)) {
            g_UIResources.SetKindSlotPtrMaybe(pRec->nKindSlotIdMaybe,
                                              pRec->nRemapTargetKindIdMaybe);
            int nTargetKindId = pRec->nRemapTargetKindIdMaybe;
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(nTargetKindId);
            if (pDesc != 0 && pDesc->bButtonVisible != 1) {
                sprintf(szKey, "%ld",
                        this->nUnlockedEasterEggsMaybe = this->nUnlockedEasterEggsMaybe + 1);
                char bOk = ((IniFileWriteIntView0x452db0 *)g_pIniFile)
                               ->WriteInt("EasterEggs", szKey, nTargetKindId);
                if (bOk == 1) {
                    pDesc->bButtonVisible = bOk;
                }
            }
        }
        pRec = pRec->pNextMaybe;
    }
    if (pNow->tm_mday == 31 && pNow->tm_mon == 9) {
        int kindId = 0x1800;
        do {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
            if (pDesc != 0) {
                pDesc->dwRenderFlags = 0x400;
                bKindsChangedMaybe = 1;
            }
            kindId++;
        } while (kindId < 0x198f);
    }
    if (bKindsChangedMaybe == 1) {
        g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
    }
}

// FUNCTION: LOCO 0x420000
// The InsertSeq spawn pump (called from the FUN_004086f0_BigSwitch at 0x408908, `this` loaded
// but never read -- the PostBag-member pattern). Walks the whole game-window widget list and,
// for every placed object whose InsertSeq record carries either spawn kind (+0x568 >= 0 or
// +0x57c > 0) AND whose perimeter precondition (CheckInsertSeqPerimeterMaybe) holds, performs
// the record's payload: kind A (+0x568) pokes the object itself through slot 7 when it is 0 or
// the descriptor's own type tag, else is placed at the object's front-row tile (offset by
// +0x574/+0x578 when +0x570 == -1) and poked the same way -- a successful placement RESTARTS
// the whole walk (i = -1), since the board just changed; kind B (+0x57c) spawns as a 'W'-code
// effect with mobility +0x580 when its category is 0xe (position mode +0x584: 'S' = scroll-
// relative, 'W' = absolute, anything else = object-rect-relative), else is placed at the
// front-row tile plus +0x588/+0x58c. Runs entirely under the DAT_004fd3dc placement-eviction
// gate (saved/raised/restored), and bails mid-walk if the app leaves screen state 3.
void ScriptEventLoader::ProcessInsertSeqSpawnsMaybe() {
    unsigned char bSavedGateMaybe = DAT_004fd3dc;
    DAT_004fd3dc = 1;
    unsigned int i = 0;
    for (i = 0; i < g_gameWindowWidgetList.nItemCount; i++) {
            if (!IsInGameModeMaybe()) break;
            unsigned char bOk = 0;
            TilePlacedObj *pObj = (TilePlacedObj *)((GameWindowWidgetListProbe *)
                                                        &g_gameWindowWidgetList)
                                      ->GetItemImpl(i);
            if (pObj != 0) {
                BigObj *pDesc = pObj->pKindDesc;
                RECT rcObj = pObj->rect;
                TileGridPos pos;
                if (pDesc->lInsertSeqUnk0x568Maybe >= 0 || pDesc->lInsertSeqUnk0x57cMaybe > 0) {
                    bOk = g_worldBoard.CheckInsertSeqPerimeterMaybe(pObj);
                }
                if (bOk == 1) {
                    long kindA = pDesc->lInsertSeqUnk0x568Maybe;
                    if (kindA >= 0) {
                        short wArg = pDesc->wInsertSeqUnk0x56cMaybe;
                        if (kindA == 0 || kindA == pObj->pKindDesc->resourceId) {
                            pObj->ReleaseChannelAndDispatch(wArg);
                        } else {
                            pos = pObj->GetFrontRowTilePosMaybe();
                            if (pDesc->lInsertSeqUnk0x570Maybe == -1) {
                                pos.wPosX += pDesc->wInsertSeqUnk0x574Maybe;
                                pos.wPosY += pDesc->wInsertSeqUnk0x578Maybe;
                            }
                            TilePlacedObj *pPlaced;
                            if (kindA == 0x3010 || kindA == 0x3011 || kindA == 0x3012 ||
                                kindA == 0x3013 || kindA == 0x3014 || kindA == 0x3015 ||
                                kindA == 0x3016 || kindA == 0x3017 || kindA == 0x3018 ||
                                kindA == 0x3019 || kindA == 0x301a || kindA == 0x301b) {
                                pPlaced = (TilePlacedObj *)g_worldBoard.PlaceObject(
                                    kindA, pos.wPosX, pos.wPosY, 0, 0);
                            } else {
                                pPlaced = (TilePlacedObj *)g_worldBoard.PlaceObject(
                                    kindA, pos.wPosX, pos.wPosY, 0, 1);
                            }
                            if (pPlaced != 0) {
                                pPlaced->ReleaseChannelAndDispatch(wArg);
                                i = -1;
                            }
                        }
                    }
                    long kindB = pDesc->lInsertSeqUnk0x57cMaybe;
                    if (kindB > 0) {
                        if ((char)TileKind_GetCategory(kindB) == 0xe) {
                            int x;
                            int y;
                            if (pDesc->lInsertSeqUnk0x584Maybe != 0x53) {
                                if (pDesc->lInsertSeqUnk0x584Maybe != 0x57) {
                                    x = pDesc->lInsertSeqUnk0x588Maybe + rcObj.left;
                                    y = pDesc->lInsertSeqUnk0x58cMaybe + rcObj.top;
                                } else {
                                    x = pDesc->lInsertSeqUnk0x588Maybe;
                                    y = pDesc->lInsertSeqUnk0x58cMaybe;
                                }
                            } else {
                                x = pDesc->lInsertSeqUnk0x588Maybe + g_worldBoard.dwScrollX;
                                y = pDesc->lInsertSeqUnk0x58cMaybe + g_worldBoard.dwScrollY;
                            }
                            DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(
                                pDesc->lInsertSeqUnk0x57cMaybe, pDesc->wInsertSeqUnk0x580Maybe,
                                'W', x, y, 1);
                        } else {
                            pos = pObj->GetFrontRowTilePosMaybe();
                            g_worldBoard.PlaceObject(
                                pDesc->lInsertSeqUnk0x57cMaybe,
                                (short)(pDesc->lInsertSeqUnk0x588Maybe + pos.wPosX),
                                (short)(pDesc->lInsertSeqUnk0x58cMaybe + pos.wPosY), 0, 1);
                        }
                    }
                }
            }
        }
    DAT_004fd3dc = bSavedGateMaybe;
}

// FUNCTION: LOCO 0x4202b0
// The message-0x404 handler's per-actor expiry pass (AppWndProc calls it with wParam, one of
// DecorObjMgrMaybe's category-7/8 actors): when the actor's seq-reward claim window
// (+0x68, armed to g_dwGameTick + the record's delay by ApplySeqRecordToActorsMaybe) has run
// out, restore its ORIGINAL spawn descriptor (+0x64) through slot 6 and disarm the window.
// `this` is the ScriptEventLoader singleton, read only for the wall clock.
void ScriptEventLoader::RestoreExpiredActorDescMaybe(AnimDescRefObj0x477488 *pActor) {
    if (pActor->bValid == 1 && pActor->dwSeqRewardUntilMaybe != 0 &&
        this->dwGameTick > pActor->dwSeqRewardUntilMaybe) {
        pActor->SetDescriptor(pActor->nSpawnDescriptorIdMaybe, -1, 0);
        pActor->dwSeqRewardUntilMaybe = 0;
    }
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 7 of 13: the ScriptEventLoader singleton (DAT_004a99b0), ctor 0x41f480. The class is TU-local
// here and no TU declares an instance of it, so the object is reached through the one name the
// mirror does carry for 0x4a99b0 -- EasterEggMgr.h's g_easterEggMgrMaybe, a partial VIEW of these
// same bytes (this TU's header note: +0x4 is what every other TU calls g_dwGameTick).
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"
#include "EasterEggMgr.h" // g_easterEggMgrMaybe names this class's object (DAT_004a99b0)

void Port_Construct_ScriptEventLoader(void) {
    new ((void *)&g_easterEggMgrMaybe) ScriptEventLoader();
}

// ─── PORT SCAFFOLDING: view-spelling forwarders ────────────────────────────────
// Every method of this class is called from another TU through a METHODS-ONLY VIEW STRUCT of
// the same singleton, because ScriptEventLoader itself is TU-local here and hoisting it into a
// header is a measured-parity change nobody has paid for yet. There are FIVE such views --
// EasterEggMgrAppView0x406ba0 (src/EasterEggMgr.h), EasterEggMgrMaybe (src/UIResources.cpp),
// EasterEggMgrIdlePumpView0x42cc60 (src/WorldActionCursor.cpp), EasterEggMgrWndProcView0x4618c0
// (src/Main.cpp) -- and each one mangles its methods under ITS OWN class name. Those symbols are
// defined NOWHERE, so link/gen_stubs.py supplied all nine of them, and every call into this
// fully-transcribed TU went to `xor eax,eax; ret N` instead of to the body sitting right here.
//
// That was not cosmetic. App_LoadWorldThreadProcMaybe (src/LoadingScreen.cpp) treats a zero from
// LoadTimeEventScriptsMaybe as a fatal load failure and answers it with
// PostMessage(WM_CLOSE, wParam=3) -- and AppWndProc's WM_CLOSE arm with a nonzero wParam is
// resource string 0x14a, "An error occurred while loading. Please reinstall this software."
// The real body cannot fail: it returns 1 unconditionally. So the whole world load was being
// aborted by a stub's return value, which is exactly the failure mode CLAUDE.md records for
// UIResources::Init (0x446050) under the same cause.
//
// This is the sanctioned #ifdef LOCO_PORT shape -- "a forwarder standing in for one address
// declared under two C++ spellings" -- and it is byte-neutral for the match build by
// construction. The view structs are redeclared here PURELY to reproduce their manglings (the
// same trick link/stubs.cpp uses for its local `class DSoundChannel`); they model no fields, so
// there is no layout to drift. `this` is the same object in every spelling -- __thiscall passes
// it in ecx regardless of the declared class -- so each forwarder is a pure retype.
//
// ⚠ Delete these the moment ScriptEventLoader moves into a header its consumers can include;
// that is the real fix, and it retires the views rather than papering over them.
//
// Only EasterEggMgrAppView0x406ba0 is forwarded HERE: it is the one view that lives in a real
// shared header (src/EasterEggMgr.h), so defining its methods adds no second definition of
// anything. The other three views are TU-LOCAL to their consumers, and redeclaring them in
// src/ would be a genuine duplicate-class definition -- six new lint_idiom class-E findings,
// the exact drift hazard that rule exists to stop. Their forwarders live in link/stubs.cpp
// instead (which lint_idiom does not scan, and which already declares a local `class
// DSoundChannel` for precisely this reason), reaching the bodies through the extern "C"
// bridges at the bottom of this block.
static ScriptEventLoader *Port_AsLoader(void *pView) {
    return (ScriptEventLoader *)pView;
}

char EasterEggMgrAppView0x406ba0::LoadEventScriptsMaybe(const char *pszIniBaseName) {
    return (char)Port_AsLoader(this)->LoadEventScriptsMaybe(pszIniBaseName);
}
char EasterEggMgrAppView0x406ba0::LoadTimeEventScriptsMaybe(const char *pszIniBaseName) {
    return (char)Port_AsLoader(this)->LoadTimeEventScriptsMaybe(pszIniBaseName);
}
char EasterEggMgrAppView0x406ba0::RecordEasterEggUnlockMaybe(unsigned int nKindId) {
    return (char)Port_AsLoader(this)->RecordEasterEggUnlockMaybe((int)nKindId);
}
void EasterEggMgrAppView0x406ba0::ProcessInsertSeqSpawnsMaybe() {
    Port_AsLoader(this)->ProcessInsertSeqSpawnsMaybe();
}
void EasterEggMgrAppView0x406ba0::DeleteAllEventRecordsMaybe() {
    Port_AsLoader(this)->DeleteAllEventRecordsMaybe();
}
// The same defect one level down, found by fixing the above: with the real bodies finally
// running, LoadEasterEggsMaybe/RecordEasterEggUnlockMaybe made 39 calls to
// IniFileWriteIntView0x452db0::WriteInt -- this TU's own local view of IniFile::WriteInt
// (0x452db0), which exists only to widen the shared header's `void` return to the `char` BOOL
// the egg-unlock paths test. The real body IS transcribed (src/IniFile.cpp), but under
// IniFile's mangling, so the view's spelling was another zero-returning stub -- and a zero
// there means "the ini write failed", so no unlock ever latched its descriptor's +0x163 flag.
//
// ⚠ The forwarder answers 1 unconditionally. It cannot do better: the original's BOOL is
// whatever WritePrivateProfileStringA left in eax at the tail call, and src/IniFile.cpp's
// signature returns void, so that value is unreachable from C++. "Assume the write succeeded"
// is the right approximation -- it reproduces the success path, which is the normal one -- but
// a genuinely failing ini write will latch here where the original would not.
char IniFileWriteIntView0x452db0::WriteInt(const char *section, const char *key, int value) {
    // Not a vtable-slot evasion -- this IS the retype, the whole point of the forwarder, and
    // both spellings name the same NON-virtual body at 0x452db0.
    ((IniFile *)this)->WriteInt(section, key, value); // idiom-exempt: port-only view retype, non-virtual
    return 1;
}

// Bridges for the three TU-local views, consumed by link/stubs.cpp. Each takes the singleton
// by void* because the caller only has its own view type. Note the two RENAMES: src/
// UIResources.cpp calls 0x41f7e0 LoadUnlockTableMaybe (here LoadEasterEggsMaybe) and 0x41f970
// ApplySeasonalUnlocksMaybe (here ApplySeasonalDateWindowsMaybe) -- same addresses, different
// vocabulary, which is why nothing flagged the mismatch.
void Port_EE_LoadUnlockTable(void *pSelf, const char *pszIniBaseName) {
    Port_AsLoader(pSelf)->LoadEasterEggsMaybe(pszIniBaseName);
}
void Port_EE_ApplySeasonalUnlocks(void *pSelf) {
    Port_AsLoader(pSelf)->ApplySeasonalDateWindowsMaybe();
}
void Port_EE_TickWorldIdle(void *pSelf) {
    Port_AsLoader(pSelf)->TickWorldIdleMaybe();
}
void Port_EE_RestoreExpiredActorDesc(void *pSelf, void *pActor) {
    Port_AsLoader(pSelf)->RestoreExpiredActorDescMaybe((AnimDescRefObj0x477488 *)pActor);
}
#endif // LOCO_PORT
