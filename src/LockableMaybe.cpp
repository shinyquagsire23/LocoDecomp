// LockableMaybe method bodies -- see the header for the class writeup. All four are one-line
// CRITICAL_SECTION wrappers, so the whole class fits in this TU.
#include "LockableMaybe.h"

// FUNCTION: LOCO 0x4493a0
// The vtable store at 0x4493a6 is the compiler's, not the source's.
LockableMaybe::LockableMaybe()
{
    InitializeCriticalSection(&m_cs);
}

// EFFECTIVE MATCH -- 30 B vs 41, DIFF(24). The original INLINES the dtor's vtable store and
// DeleteCriticalSection call into this thunk where ours CALLS ??1LockableMaybe, i.e. the
// in-class-dtor shape. NOT probed here, because the identical lever has now been measured and
// REJECTED twice on repo-wide grounds -- src/ThumbnailBmp.cpp (cost src/DPlaySessionMgr.cpp
// 345 B) and src/IniFile.cpp (cost src/TutorialWnd.cpp 249 B, net -224 B) -- and this class has
// strictly more exposure than either: besides the heap-allocated g_pGameNetMsgQueueLock, it is
// EMBEDDED twice in DecorObjMgrMaybe, so every TU that destroys one of those would inline it
// too. Retry only as part of a deliberate repo-wide sweep of all three classes at once.
//
// FUNCTION: LOCO 0x4493c0 (??_GLockableMaybe scalar deleting dtor -- compiler-generated around
// ~LockableMaybe() below; no source of its own)

// FUNCTION: LOCO 0x4493f0
LockableMaybe::~LockableMaybe()
{
    DeleteCriticalSection(&m_cs);
}

// FUNCTION: LOCO 0x449410
// CRITICAL_SECTION lock wrapper (IAT-indirect call). Many callers
// (0x46b770/0x46b810/0x46b850/0x464d90/0x470150/0x470460/0x46f350 families) -- a
// widely-used locking primitive. Always returns true.
bool LockableMaybe::Lock()
{
    EnterCriticalSection(&m_cs);
    return true;
}

// FUNCTION: LOCO 0x449420
bool LockableMaybe::Unlock()
{
    LeaveCriticalSection(&m_cs);
    return true;
}
