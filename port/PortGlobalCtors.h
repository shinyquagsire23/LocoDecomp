// PORT SCAFFOLDING -- compiled only in a `-D LOCO_PORT` build, never part of the byte-match
// product. See port/README.md for the whole design and link/init_globals.cpp for the WHY.
//
// One hook per global object that the ORIGINAL constructs from the CRT's C++ dynamic-initializer
// table (.CRT$XC at 0x47e000..0x47e038 -- thirteen entries, run by __cinit's second _initterm at
// 0x4684c7). The port's .bss mirror is zero-filled, so none of those initializers exist and
// every one of those globals would otherwise start life all-zero, vtable pointers included.
//
// Each hook is DEFINED in the src/ TU that owns the constructor, because that is the only place
// the type is reachable: several of these layouts are modeled by a TU-LOCAL view struct (a
// placement-new of the class src/EffectSpawner.h declares would construct nothing at all), and
// WorldBoardMaybe's ctor is transcribed as a free __fastcall function rather than a ctor. They
// are DECLARED here rather than TU-locally so the two sides cannot desync -- tools/lint_idiom.py
// class I, and the reason it exists.
//
// Called only from link/init_globals.cpp's Smoke_ConstructGlobals, in the XC table's own order.
// The hook name is the OBJECT's name; the comment gives the class whose ctor it runs, so the
// two do not have to agree (three of these objects are named for a still-`Maybe` class).
#pragma once

extern "C" {

void Port_Construct_g_screenSaver(void);          // XC  1 -- ScreenSaver, 0x448040
void Port_Construct_g_PeerTrainSlotQueue(void);   // XC  3 -- PeerTrainSlotQueueMaybe, 0x44d800
void Port_Construct_PlacementCursor(void);        // XC  4 -- PlacementCursorMaybe, 0x410510
void Port_Construct_DecorObjMgr(void);            // XC  5 -- DecorObjMgrMaybe, 0x434500
void Port_Construct_g_NetSessionEventQueue(void); // XC  6 -- NetSessionEventQueue, 0x41d250
void Port_Construct_ScriptEventLoader(void);      // XC  7 -- ScriptEventLoader, 0x41f480
void Port_Construct_EffectSpawner(void);          // XC  8 -- EffectSpawner, 0x4238c0
void Port_Construct_g_BuildToolButton(void);      // XC  9 -- BuildToolButton, 0x449430
void Port_Construct_g_worldActionCursor(void);    // XC 10 -- WorldActionCursor, 0x4589b0
void Port_Construct_SelectedObjWidget(void);      // XC 11 -- SelectedObjWidgetMaybe, 0x42cce0
void Port_Construct_g_worldBoard(void);           // XC 12 -- WorldBoardMaybe, 0x454cf0

// XC 2 (g_UIResources, 0x445f70) is constructed inline in link/init_globals.cpp -- its class is
// a normal header type, so no TU-local hook is needed.
// XC 13 (g_worldLoadThread, 0x461610) needs no hook at all: src/FrameDriver.cpp DEFINES that
// object for real, so our own CRT already emits and runs its initializer.

// --- View-spelling bridges for the DAT_004a99b0 singleton ---------------------
// Same shape and same reason as the hooks above: DEFINED in src/ScriptEventLoader.cpp (the only
// TU where class ScriptEventLoader is reachable -- it is TU-local there), DECLARED here so the
// consumer cannot desync from it. Consumed by link/stubs.cpp, which uses them to define the
// method spellings of the THREE TU-local view structs of this object (src/UIResources.cpp's
// EasterEggMgrMaybe, src/WorldActionCursor.cpp's EasterEggMgrIdlePumpView0x42cc60,
// src/Main.cpp's EasterEggMgrWndProcView0x4618c0). Without them every call through those views
// mangles to a symbol nothing defines and lands on a zero-returning generated stub -- which is
// what aborted the world load with the "An error occurred while loading" box. `pSelf` is always
// g_easterEggMgrMaybe; it is void* only because each caller holds a different view type.
void Port_EE_LoadUnlockTable(void *pSelf, const char *pszIniBaseName); // 0x41f7e0
void Port_EE_ApplySeasonalUnlocks(void *pSelf);                       // 0x41f970
void Port_EE_TickWorldIdle(void *pSelf);                              // 0x41fd00
void Port_EE_RestoreExpiredActorDesc(void *pSelf, void *pActor);      // 0x4202b0

}
