#pragma once

// ThreadWrapper -- a generic worker-thread wrapper (owns a Win32 thread HANDLE and a
// pending-resume flag). Partial view of the real Ghidra class (1049 bytes, was Obj0x479168):
// +0xc..+0x407 of the ~1KB diagnostic buffer is unmodeled tail. Start copies a failure/status
// string into szDiagBuffer on error paths; nErrorMaybe is its status-code companion (only the
// "already active" path writes it). The deferred user callback+arg (+0x410/+0x414) is invoked
// by TrampolineProc (a real _beginthreadex entry-point signature, kept a static member).
#include <windows.h>

struct ThreadWrapper {
    int nErrorMaybe;                 // +0x4 -- Start's status code (0xffffffeb = already active)
    char szDiagBuffer[0x400];        // +0x8 -- Start's failure/status string buffer
    int nThreadIdMaybe;              // +0x408
    HANDLE hThreadMaybe;             // +0x40c
    void (__cdecl *pfnStartRoutineMaybe)(void *); // +0x410
    void *pStartArgMaybe;            // +0x414
    bool bPendingResumeMaybe;        // +0x418

    ThreadWrapper();                 // 0x461610
    // Base dtor 0x461690 ("Close" in Ghidra -- closes the handle); scalar deleting dtor
    // 0x461640 (vtable slot 0). DEFINED IN-CLASS since v495: the original's ??_G has this
    // whole 45-byte body INLINED (the out-of-line form cost ??_G a DIFF(19) call -- the v322
    // park), so the body lives here where cl can fold it in. See src/ThreadWrapper.cpp for
    // the full trade note, including why the standalone ??1 copy (0x461690) is no longer
    // emitted by any TU.
    virtual ~ThreadWrapper() {
        if (hThreadMaybe != 0) {
            CloseHandle(hThreadMaybe);
            hThreadMaybe = 0;
            bPendingResumeMaybe = false;
        }
    }
    BOOL SetPriority(int nPriority); // 0x4616c0
    char IsRunning();                // 0x461710 -- low-byte flag (1 while the thread is alive)
    void PollAndResume();            // 0x461740
    int Start(void (__cdecl *pfnRoutine)(void *), void *pArg); // 0x461790
    static unsigned __stdcall TrampolineProc(ThreadWrapper *pThread); // 0x461890
};
