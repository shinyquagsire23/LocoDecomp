#pragma once

// The GameNet outbound send-queue and its nodes.
//
// Producers (GameNet_Post* / LayoutNet_Post* free helpers in src/DPlaySessionMgr.cpp)
// allocate a NetMsgQueueNode, fill in its opcode `type` and optional payload, and hand it
// to GameNetThreadState::EnqueueOrFreeNode (0x4393d0), which links it onto the queue or
// frees it on failure. The enqueue `this` is g_pGameNetThreadState (0x4fd3a4) -- the
// GameNet thread/message-processing context singleton, which owns the send queue (see
// src/GameNet.h and docs/subsystems.md). Each node is 0x1c
// bytes; its constructor zeroes only pPayload and pNext (the producer sets the rest).

// The same 0x1c-byte node also serves as an inbound/queued EVENT record (dispatched by
// DPlaySessionMgr::GameNetManager_HandleQueuedEvent on `type`). Event type 0x12 (place a
// single train resolved from a heading) stuffs its data inline into the generic send fields:
// heading @ +0x4, trainId @ +0x10, and the two owner bytes @ +0x14/+0x15. The union views below
// name those overlays without changing any offset (send producers keep the original names).
struct NetMsgQueueNode {
    NetMsgQueueNode() : pPayload(0), pNext(0) {}

    int type;                 // +0x0  -- send-queue opcode / inbound-event type
    union {
        int payloadLen;  // +0x4  (send)
        unsigned int heading; // +0x4 (inbound event 0x12: 0/0x5a/0xb4/0x10e; unsigned -> the
                                   //       switch pivot compares with `ja`, not `jg`)
    };
    void *pPayload;           // +0x8
    int destPlayerId;    // +0xc
    union {
        int bReliable;   // +0x10 (send)
        int eventTrainId;// +0x10 (inbound event 0x12)
        int nProviderSlotIndex; // +0x10 (inbound events 0x13/0x14/0x16: target provider slot)
        int eventSlotCount;     // +0x10 (inbound event 9: total provider-slot count -> mgr field_0x8)
        int nMoveHeading;       // +0x10 (local move-request command: 0/0x5a/0xb4/0x10e direction;
                                     //        signed -- the slot-offset switch pivot compares with `jg`)
    };
    union {
        int Unk0x14;          // +0x14
        struct {
            unsigned char bEventOwnerA;  // +0x14 (inbound event 0x12)
            unsigned char bEventOwnerB;  // +0x15 (inbound event 0x12)
        };
        struct {
            unsigned char bEventGridCols;  // +0x14 (inbound event 9: provider-grid columns)
            unsigned char bEventGridRows;  // +0x15 (inbound event 9: provider-grid rows)
        };
    };
    NetMsgQueueNode *pNext;   // +0x18
};

// The payload of a type-2 local-queue node: its own singly-linked list of owned sub-records
// (next @ +0x0, an owned sub-payload @ +0x8). ~DPlaySessionMgr's local-queue drain frees
// each node's sub-payload (if any) then the node itself, walking `pNext`.
struct NetMsgType2PayloadNode {
    NetMsgType2PayloadNode *pNext;  // +0x0
    void *pad4;                     // +0x4
    void *pSubPayload;              // +0x8
};

// The two GameNet message queues (both NetMsgQueueNode singly-linked lists, `pNext` @ +0x18),
// drained under g_pGameNetMsgQueueLock by ~DPlaySessionMgr:
//   * local/inbound drain queue  g_pNetMsgLocalQueueHead (0x4fd3a0) -- ~13 callers
//   * outbound send queue        g_pNetMsgSendQueueHead  (0x4fd39c) -- 6-node cap
extern NetMsgQueueNode *g_pNetMsgLocalQueueHead;  // 0x4fd3a0
extern NetMsgQueueNode *g_pNetMsgSendQueueHead;   // 0x4fd39c

// The GameNet message-queue lock object (heap-allocated; *g_pGameNetMsgQueueLock holds
// the pointer). Its Lock()/Unlock() (0x449410/0x449420) wrap Enter/LeaveCriticalSection on an
// embedded CRITICAL_SECTION; both are real LockableMaybe members, declared in
// src/LockableMaybe.h and transcribed in src/LockableMaybe.cpp. This header used to carry its
// own `GameNetMsgQueue_Lock`/`_Unlock` free-__fastcall spelling of the same two addresses so
// that consumers got the mov-ecx/call shape without including LockableMaybe.h -- but a free
// spelling of an address that already HAS a definition elsewhere is a call to a symbol that
// exists nowhere (tools/lint_alias.py), so it is gone: the two callers (src/GameNet.cpp,
// src/DPlaySessionMgr.cpp) include LockableMaybe.h and call the members. The call shape is
// byte-identical either way; the include itself is not free in src/DPlaySessionMgr.cpp -- see
// the measured price recorded at that include. The POINTER stays forward-declared so this
// header keeps its light include set:
// SaveWindowAndCleanExit deletes it through its vtable, which a `void *` cannot express --
// `delete (void *)p` compiles to a bare `operator delete` instead of the original's
// `mov edx,[ecx]; push 1; call [edx]`. A TU that deletes it includes LockableMaybe.h itself.
class LockableMaybe;
extern LockableMaybe *g_pGameNetMsgQueueLock;        // 0x4fd394

// The app screen-state selector. Written only by AppWindow_SetScreenState (0x408130), which
// performs the transition it names; value 10 means the app is quitting, so the net subsystem is
// shutting down and no one will ever drain the send queue -- EnqueueOrFreeNode then disposes
// nodes inline. Full state list at AppWindow_SetScreenState's definition in src/AppWindow.cpp.
extern int g_nScreenState;              // DAT_004851f4

// A polymorphic payload attached to send-queue node types 0xe/0x10; disposed via its virtual
// scalar-deleting destructor (vtbl[0], flags=1) rather than a raw operator delete like the other
// node types' plain-buffer payloads. Declared-only dtor -- the concrete type is not yet modeled.
struct NetMsgPayloadObjMaybe {
    virtual ~NetMsgPayloadObjMaybe();
};

// The send queue's owning object (g_pGameNetThreadState @ 0x4fd3a4) is GameNetThreadState --
// see src/GameNet.h, which declares both the type and the global. This header deliberately does
// NOT carry a second partial view of it (v378; see the note beside the extern there).

// Partial view of the networking-settings singleton (*g_pNetSettings), real size 0xb0 bytes
// (Ghidra struct `NetSettings`). The remembered-choice pair (protocol id + its per-protocol
// custom value, domain 1-4 = Modem/TCP/Serial/IPX per docs/subsystems.md) is duplicated for
// primary (+0x28.., used when not hosting and not in connectionMode==1) and secondary (+0x1c..,
// used when hosting) -- DPlay_PrepareInternetConnection (0x43a760) is the sole consumer so far.
// DPlayProviderNode must be COMPLETE here, not merely declared: ~NetSettings is defined
// in-class below and `delete`s the nodes it owns.
#include "GNetManager.h"

struct NetSettings {
    // +0x0 is a vtable slot, not data: AppWindow::SaveWindowAndCleanExit tears this singleton
    // down with the same `mov edx,[ecx]; push 1; call [edx]` scalar-deleting-dtor shape as its
    // fifteen siblings. Modeled as a real virtual dtor 2026-07-27 (the old `pad0x0[7]` swallowed
    // the slot); the implicit vptr is +0x0..+0x3 and the remaining 3 bytes stay pad, so
    // bSkipSetupWizardMaybe is still at +0x7. Declared only.
    // 0x440c60 -- reads NetSettings.dat back into this blob (or defaults it). Declared only;
    // it is what `new NetSettings` in AppWindow's bootstrap (0x406ba0) dispatches to.
    NetSettings();
    // Frees the detected-provider list this singleton owns. Defined IN-CLASS deliberately: the
    // image has no standalone `??1NetSettings` at all -- the body is folded into the
    // compiler-generated `??_GNetSettings` scalar deleting destructor (0x440cc0), which is what
    // an in-class definition produces (the same shape `DSound::~DSound` uses in src/DSound.h).
    // FUNCTION: LOCO 0x440cc0 (??_GNetSettings scalar dtor)
    virtual ~NetSettings()
    {
        DPlayProviderNode *pNode = pDetectedProviderList;
        while (pNode != NULL) {
            pDetectedProviderList = pNode->pNext;
            delete pNode;
            pNode = pDetectedProviderList;
        }
    }
    // +0x4/+0x6, promoted off the pad 2026-07-26 from the ctor (0x440c60, which seeds them 0x6a
    // and 0) and SaveToDisk, whose flat WriteFile starts at &magicMaybe -- i.e. the on-disk
    // NetSettings.dat record is this object minus the vptr, 0xac bytes, and magicMaybe is its
    // leading format stamp. bValidSaveLoadedMaybe is LoadOrInitFromDisk's own "the file was
    // there and the stamp matched" result.
    unsigned short magicMaybe;                    // +0x4
    unsigned char bValidSaveLoadedMaybe;          // +0x6
    unsigned char bSkipSetupWizardMaybe;          // +0x7 -- set by ScreenSaver::EnterDemoSession
                                                       //   so the attract-mode session skips the
                                                       //   multiplayer setup wizard
    unsigned char bUseSecondaryRememberedChoice;  // +0x8
    char pad0x9[0xc - 9];                              // +0x9
    int nTickSleepMs;                             // +0xc -- worker-thread inter-tick Sleep(ms)
    // +0x10 -- the head of the "which DirectPlay providers does this machine actually have?"
    // list, built by GNetManager::DPlay_ProbeAvailableProviders (src/GNetManager.h) and handed
    // here by GameNetThread_InitState. Retyped off `int Unk0x10Maybe` 2026-07-26 once
    // NetSetupWnd::RefreshProviderAvailability was read -- it walks this list node by node. A
    // NULL head therefore means "no multiplayer transport exists", which is exactly the
    // short-circuit its other consumers test for (a train move falls back to a local enqueue in
    // RequestTrainMove..., and SplashWnd greys out its "connect online" hit rect).
    DPlayProviderNode *pDetectedProviderList;          // +0x10
    // +0x14..+0x17 -- per-COM-port probe results ("COM0".."COM3"), filled in order by
    // GameNetThreadState's ctor (0x438bc0) looping GNetManager::ProbeComPort. Promoted off the
    // pad when the ctor was transcribed (2026-07-30); no reader transcribed yet.
    unsigned char bComPortAvailableMaybe[4];           // +0x14
    // "the secondary remembered choice below is populated and still worth honouring" --
    // SplashWnd::OnEnterCommitAndDispatch gates the whole skip-the-wizard shortcut on it.
    unsigned char bRememberedApplSetupValidSecondaryMaybe;  // +0x18
    char pad0x19[0x1c - 0x19];                         // +0x19
    int rememberedProtocolSecondary;              // +0x1c -- domain 1-4, used when hosting
    int nRememberedCustomValueSecondary;          // +0x20 -- itoa'd when secondary==3
    unsigned char bRememberedApplSetupValidPrimaryMaybe;    // +0x24 -- the primary half of the
                                                       //   flag above
    char pad0x25[0x28 - 0x25];                         // +0x25
    int rememberedProtocolPrimary;                // +0x28 -- domain 1-4, used when not hosting
    char szRememberedAddrPrimary[0x40];           // +0x2c -- used when primary==1
    char szRememberedAddrPrimaryAlt[0x40];        // +0x6c -- used when primary==2
    int nRememberedCustomValuePrimary;            // +0xac -- itoa'd when primary==3 (immediately
                                                       //   follows the alt addr buffer, no gap)

    // 0x440ea0 -- flat 0xac-byte WriteFile of this blob (from magicMaybe on) to NetSettings.dat.
    void SaveToDisk();

    // 0x440d00 -- SaveToDisk's counterpart: reads NetSettings.dat back over the same 0xac-byte
    // span, or leaves the ctor's defaults in place. Declared only; called by the ctor.
    void LoadOrInitFromDisk();
};

extern NetSettings *g_pNetSettings;  // 0x4fd3a8
