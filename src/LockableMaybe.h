// LockableMaybe -- small polymorphic lock base (0x1c bytes: +0 vtable 0x4782a4, +4 a
// CRITICAL_SECTION; its ONLY virtual is the dtor, slot 0 = scalar deleting dtor).
// Ctor 0x4493a0 (InitializeCriticalSection(this+4)), dtor 0x4493f0, Unlock 0x449420 --
// none transcribed yet. Built both by heap allocation (GameNet's g_pGameNetMsgQueueLock
// DAT_004fd394, built by Config_FUN_00406ba0) and in-place as an embedded member
// (DecorObjMgrMaybe's lockAMaybe/lockBMaybe at +4/+0x20). Lock/Unlock are the actual
// API -- per docs/subsystems.md they behave as free functions on the polymorphic object,
// modeled here as ordinary members (identical this-in-ecx codegen). Moved out of
// src/phase2_probe2.cpp 2026-07-22 (v322).
#pragma once

#include <windows.h>

class LockableMaybe {
public:
    LockableMaybe();          // 0x4493a0 -- what `new LockableMaybe` in AppWindow's
                              //   bootstrap (0x406ba0) dispatches to
    virtual ~LockableMaybe(); // 0x4493f0
    CRITICAL_SECTION m_cs;    // +4

    // Both return a hardcoded `true` (`mov al,1; ret`), never a real success flag -- the
    // Win32 EnterCriticalSection/LeaveCriticalSection pair they wrap has no failure mode.
    bool Lock();              // 0x449410
    bool Unlock();            // 0x449420
};
