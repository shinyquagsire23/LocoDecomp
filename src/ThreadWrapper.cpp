// ThreadWrapper -- generic worker-thread wrapper; see src/ThreadWrapper.h and
// docs/subsystems.md's Thread-infrastructure entry for the full class writeup.
// All 8 functions decompiled 2026-07-22 (v322) after the class was fully documented
// in Ghidra.
#include "ThreadWrapper.h"

#include <string.h>

// The statically-linked CRT's _beginthreadex/_endthreadex are __cdecl at these call sites
// (caller-side ADD ESP cleanup in the original), NOT the __stdcall the later SDK headers
// declare -- declared locally to reproduce that codegen.
extern "C" unsigned long __cdecl _beginthreadex(void *, unsigned int, unsigned int (__stdcall *)(void *), void *, unsigned int, unsigned int *); // idiom-exempt: local cdecl decl required for byte-match (see comment above)
extern "C" void __cdecl _endthreadex(unsigned long); // idiom-exempt: same

// FUNCTION: LOCO 0x461610
// fastcall ctor -- `mov eax,ecx` return-this-chain confirms CONSTRUCTOR (a bare
// vtable-store dtor never does this). 4 field writes after the vtable store; written as
// body assignments (vtable-first), NOT a mem-initializer list.
ThreadWrapper::ThreadWrapper() {
    hThreadMaybe = 0;
    pfnStartRoutineMaybe = 0;
    pStartArgMaybe = 0;
    bPendingResumeMaybe = false;
}

// (The base dtor 0x461690 moved INTO the class body in v495 -- see src/ThreadWrapper.h. The
// v322 park note had planned for the frame-driver TU's g_worldLoadThread static-dtor thunk
// (0x45c790) to keep a standalone ??1 COMDAT alive once that global was defined there; in
// practice VC5 /O2 INLINES the in-class body into the compiler-generated $E thunk, so no TU
// emits ??1 any more and 0x461690's marker was dropped -- the original's standalone copy
// survives in the binary because the original frame-driver TU evidently saw a
// declaration-only view of this class. Net of the trade: ??_G below is EXACT (+65 B exact),
// 0x461690 leaves the marker table (-45 B exact).)

// FUNCTION: LOCO 0x461640 (??_GThreadWrapper scalar dtor)
// EXACT since v495 (was the v322 DIFF(19) EFFECTIVE park): with the dtor defined in-class,
// cl folds the whole 45-byte base-dtor body in here instead of CALLing it, exactly like the
// original.

// The "is the thread still alive" check (WaitForSingleObject timeout => alive, else clear
// bPendingResumeMaybe) is IsRunning itself, called by SetPriority/PollAndResume/Start and
// INLINED at those same-TU call sites (the out-of-line copy 0x461710 below remains for
// cross-TU callers like GameNet.cpp). IsRunning must be defined FIRST in this TU so the
// inliner sees its body; its AL-resident char return is what keeps the inlined shape
// spill-free in the callers.

// FUNCTION: LOCO 0x461710
char ThreadWrapper::IsRunning() {
    if (hThreadMaybe != 0) {
        if (WaitForSingleObject(hThreadMaybe, 0) == WAIT_TIMEOUT) {
            return 1;
        }
        bPendingResumeMaybe = false;
    }
    return 0;
}

// FUNCTION: LOCO 0x4616c0
BOOL ThreadWrapper::SetPriority(int nPriority) {
    char bAlive;
    if (hThreadMaybe != 0) {
        if (WaitForSingleObject(hThreadMaybe, 0) == WAIT_TIMEOUT) {
            bAlive = 1;
        } else {
            bPendingResumeMaybe = false;
            bAlive = 0;
        }
    } else {
        bAlive = 0;
    }
    if (!bAlive) {
        return 0;
    }
    return SetThreadPriority(hThreadMaybe, nPriority);
}

// FUNCTION: LOCO 0x461740
void ThreadWrapper::PollAndResume() {
    char bAlive;
    if (hThreadMaybe != 0) {
        if (WaitForSingleObject(hThreadMaybe, 0) == WAIT_TIMEOUT) {
            bAlive = 1;
        } else {
            bPendingResumeMaybe = false;
            bAlive = 0;
        }
    } else {
        bAlive = 0;
    }
    if (bAlive && bPendingResumeMaybe) {
        ResumeThread(hThreadMaybe);
        bPendingResumeMaybe = false;
    }
}

// FUNCTION: LOCO 0x461790
int ThreadWrapper::Start(void (__cdecl *pfnRoutine)(void *), void *pArg) {
    char bAlive;
    if (hThreadMaybe != 0) {
        if (WaitForSingleObject(hThreadMaybe, 0) == WAIT_TIMEOUT) {
            bAlive = 1;
        } else {
            bPendingResumeMaybe = false;
            bAlive = 0;
        }
    } else {
        bAlive = 0;
    }
    if (bAlive) {
        strcpy(szDiagBuffer, "Thread already active");
        nErrorMaybe = 0xffffffeb;
        return 0xffffffeb;
    }
    if (hThreadMaybe != 0) {
        CloseHandle(hThreadMaybe);
        hThreadMaybe = 0;
        bPendingResumeMaybe = false;
    }
    pStartArgMaybe = pArg;
    pfnStartRoutineMaybe = pfnRoutine;
    hThreadMaybe = (HANDLE)_beginthreadex(0, 0, (unsigned int (__stdcall *)(void *))TrampolineProc, this, 0, (unsigned int *)&nThreadIdMaybe);
    if (hThreadMaybe == 0) {
        strcpy(szDiagBuffer, "CreateThread failed.");
        return 0xffffffea;
    }
    return 1;
}

// FUNCTION: LOCO 0x461890
unsigned __stdcall ThreadWrapper::TrampolineProc(ThreadWrapper *pThread) {
    pThread->pfnStartRoutineMaybe(pThread->pStartArgMaybe);
    _endthreadex(0);
    return 0;
}
