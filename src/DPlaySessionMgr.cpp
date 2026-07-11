// DPlaySessionMgr roster-record marshalling helpers.
//
// Three leaf helpers that move a player's roster fields between the 60-byte network
// wire record (opcode 0x3f1 payload, 9 records/packet) and DPlaySessionMgr's own
// aProviderSlots[9] entries. Each moves the same seven fields -- providerId, a dirty
// flag, a 13-byte short/display name, a 32-byte long name/address, a tail dword, an
// enabled flag, and a trailing dword -- in the same order; the two string fields go
// through the inlined-strcpy intrinsic (repne scasb + rep movsd/movsb under /O2).

#include <string.h>
#include <stdlib.h>
#include "DPlaySessionMgr.h"
#include "GameNetMsgQueue.h"
// g_pGameNetMsgQueueLock's real class (Lock/Unlock, 0x449410/0x449420). ⚠ MEASURED PRICE: this
// one include costs ApplSetupWnd::SelectGridCellFromPointMaybe (0x40aba0) its EXACT -- 166 B,
// the v340 canary for this TU's declaration-set sensitivity. Bisected v448: the CALL rewrite
// (GameNetMsgQueue_Lock(p) -> p->Lock()) is free, and so is dropping the two free decls from
// GameNetMsgQueue.h; the cost is purely LockableMaybe's class declaration entering this TU, and
// it does not move with the include's position (probed at the top of the file, after the include
// block, and at the point of use -- all identical). Taken anyway: without it the 7 lock calls
// here target `?GameNetMsgQueue_Lock@@...`, a symbol that exists nowhere (tools/lint_alias.py),
// and 0x40aba0 is part of the ApplSetupWnd middle block this TU only holds as documented debt --
// its /Og state here is an artifact of a known-wrong TU grouping, so recover it by splitting the
// ApplSetupWnd TU out, not by re-breaking the call target.
// ⭐ UPDATE v471: the 166 B came back WITHOUT the TU split. `#include <stdio.h>` arriving in
// src/DSoundChannel.h (so RFIndex could model its .RFD handle as a real FILE*) rotated this TU's
// /Og state and 0x40aba0 is EXACT again at 166 B. The debt above is therefore PAID for now, but
// the diagnosis stands and so does the plan: this function's /Og state is still an artifact of a
// wrong TU grouping, and it will keep flipping until the ApplSetupWnd TU is actually split out.
// Do not treat the recovery as evidence the grouping is fine.
// ⭐ UPDATE v516: flipped DIFF at some point between v471 and v515 (cause not bisected), then
// flipped back EXACT at 166 B this session when the end-of-file include block gained
// `#include "GameNet.h"` (RosterSnapshotWireMsg, for GameNet_BroadcastRosterSnapshot below).
// The v471 note's prediction holds -- it keeps flipping.
#include "LockableMaybe.h"
#include "PeerTrainNode.h"
#include "CarNetObj.h"       // CarNetObj / CarNetObjVtblProbe (shared with src/GameNet.cpp)
#include "WorldBoardMaybe.h"
#include "LocoBitmap.h"
#include "ThumbnailBmp.h"
#include "CarNetState.h"
#include "UIResources.h"
#include "LocalPlayerIdentity.h"
#include "WidgetBase.h"  // AnimDescRefObj0x477488 (pKindDesc @ +0x40) -- game-window widget items
#include "GameNetThreadState.h"  // GameNetThreadState / g_pGameNetThreadState (owns the send queue)

// The install-directory path prefix (ends in a backslash), stripped off the front of every
// loose-file path. Plain extern here mirrors src/NetSessionEventQueue.cpp's own decl.
extern char g_pInstallPathPrefix[];  // DAT_004a99c8

// The shared text-formatting scratch global (its first byte seeds path buffers). Mirrors the
// same minimal extern used across src/WidgetPicker.cpp et al.

#include "Pair16.h"

// The app screen-state selector; only states 3/5/9 (in-game world) allow a queued train to be
// placed. Mirrors src/TutorialWnd.cpp's own extern.
extern int g_nScreenState;

// The NetSessionEventQueue singleton (DAT_004a9990) also owns the 4 "edge placement"
// helpers -- one per board edge, dispatched by train heading -- that resolve where a train
// hand-off connector tile lands as an (x,y) Pair16 returned by value. Each is a thiscall method
// whose body ignores `this`; every call site still loads ecx=the singleton, so the caller must
// invoke them as members to reproduce that load. This by-value model was right from v3 and is
// now also what the CALLEE side uses -- src/NetSessionEventQueue.cpp had them as free
// `__stdcall(Pair16*)` functions until v352, which is why all four sat parked; declaring them
// as real members returning by value made all four EXACT. Methods-only partial view -- no
// data members, so no layout to drift.
struct NetSessionEventQueueEdge {
    Pair16 ComputeRightEdgePlacement();   // 0x41d8f0
    Pair16 ComputeLeftEdgePlacement();    // 0x41d920
    Pair16 ComputeBottomEdgePlacement();  // 0x41d950
    Pair16 ComputeTopEdgePlacement();     // 0x41d980
};
extern NetSessionEventQueueEdge g_NetSessionEventQueueEdge; // DAT_004a9990

// The peer train-slot registry singleton (DAT_004a98b0). ClaimSlotForTrain claims a free slot for a
// popped queued train node at the resolved coord/quadrant and attaches it to the board (or, in
// joined mode, resolves the target board plane slot first); returns 0 when the roster is full.
// (v323: the data-layout half of this view moved to the canonical PeerTrainSlotQueueMaybe in
// src/PeerTrainSlotQueueMaybe.h, but #including that header here regresses
// SelectGridCellFromPointMaybe EXACT->DIFF(130) -- this TU's documented position sensitivity,
// same artifact as the end-of-file include note below. Methods-only view stays, exactly like
// NetSessionEventQueueEdge above: no data members, no layout to drift.)
struct PeerTrainSlotQueueEdgePartial {
    char ClaimSlotForTrain(void *pNode, Pair16 coord, char quadrant);  // 0x44df40
    // Free the (up to 8) car slots a queued train node still holds, before it is re-placed or
    // deleted. A this-ignoring thiscall (Ghidra sees __stdcall(pNode)); invoked as a member so the
    // call site reproduces the ecx=singleton load.
    void FreeQueuedTrainCarSlots(PeerTrainNodePartial *pNode);  // 0x44e800
    // Release every board train slot owned by the given provider-slot index (a departing peer's
    // trains). Name and argument type must track the real declaration in
    // src/PeerTrainSlotQueueMaybe.h -- this view mangles the callee under ITS class name, so a
    // spelling that drifts from the transcribed body's is a call to a symbol that exists nowhere
    // (tools/lint_alias.py; the argument is UNSIGNED char, pinned in v446 by the body's own
    // byte compare against bOwnerByteA).
    void ReleaseSlotsForOwnerMaybe(unsigned char owner);  // 0x44da50
};
extern PeerTrainSlotQueueEdgePartial g_PeerTrainSlotQueueEdge; // DAT_004a98b0

// CarNetObj's own type-id accessor (0x1870/0x1871 = the two hand-off socket states). Declared as
// a free __fastcall (this-in-ecx) exactly like CarNetObj_GetAppliedState above, so the call site
// reproduces the mov-ecx/call shape without redefining CarNetObj's full class.
extern int __fastcall CarNetObj_GetCarTypeId(void *pCar);  // 0x40e0d0

// (CarNetObjVtblProbe / CarNetObj moved to the shared src/CarNetObj.h -- included above.)

#include "GameWindowWidgetList.h"  // GameWindowWidgetList / g_gameWindowWidgetList / probe

// Padded-vtable probe to reach a game-window widget item's vtbl+0x1c (slot 7) "set UI mode"
// call. The item is really an AnimDescRefObj0x477488-derived placed object whose own slot 7
// override is unmodeled; per the established precedent the compiler only needs a >=8-slot
// vtable to select the `call [eax+0x1c]` shape (the probe's own identity is a masked reloc).
struct GameWindowWidgetItemProbe {
    virtual void *_v00(); virtual void *_v01(); virtual void *_v02(); virtual void *_v03();
    virtual void *_v04(); virtual void *_v05(); virtual void *_v06();
    virtual void SetUiModeImpl(int mode); // vtbl+0x1c (slot 7)
};

// FUNCTION: LOCO 0x4426d0
// Decode one wire record into this provider slot.
void DPlaySessionMgrProviderSlot::GameNet_UnpackRosterRecord(const GameNetRosterWireRecord *wire) {
    providerId = wire->providerId;
    bDirty = wire->bDirty;
    strcpy(sAddressOrName, wire->sAddressOrName);
    strcpy(sLongName, wire->sLongName);
    dwTailAlias = wire->dwTailAlias;
    bEnabled = wire->bEnabled;
    dwLayoutVersion = wire->dwLayoutVersion;
}

// FUNCTION: LOCO 0x442750
// Copy another provider slot's roster fields into this one.
void DPlaySessionMgrProviderSlot::CopyFrom(const DPlaySessionMgrProviderSlot *src) {
    providerId = src->providerId;
    bDirty = src->bDirty;
    strcpy(sAddressOrName, src->sAddressOrName);
    strcpy(sLongName, src->sLongName);
    dwTailAlias = src->dwTailAlias;
    bEnabled = src->bEnabled;
    dwLayoutVersion = src->dwLayoutVersion;
}

// FUNCTION: LOCO 0x4427d0
// Encode a provider slot's roster fields into this wire record.
void GameNetRosterWireRecord::GameNet_PackRosterRecord(const DPlaySessionMgrProviderSlot *slot) {
    providerId = slot->providerId;
    bDirty = slot->bDirty;
    strcpy(sAddressOrName, slot->sAddressOrName);
    strcpy(sLongName, slot->sLongName);
    dwTailAlias = slot->dwTailAlias;
    bEnabled = slot->bEnabled;
    dwLayoutVersion = slot->dwLayoutVersion;
}

// FUNCTION: LOCO 0x43d0a0
// Construct the session manager: install the vtable (+0x0), reset all provider slots, then
// zero the connection state machine -- connectionMode=3 (idle), retryCount=15,
// timeoutMs=0x960. The vtable store is compiler-synthesized from the virtual dtor.
DPlaySessionMgr::DPlaySessionMgr() {
    ResetProviders(1);
    connectionMode = 3;
    searchProviderId = 0;
    bConnectPending = false;
    field_0x7dc = 0;
    pPendingTrainQueueHead = 0;
    pCurrentTrainNodeMaybe = 0;
    nDispatchTick = 0;
    nNextTrainId = 0;
    rosterBroadcastTick = 0;
    field_0x800 = 0;
    nRosterBroadcastPeriodTicks = 0xf;
    timeoutMsMaybe = 0x960;
}

// FUNCTION: LOCO 0x43d110 (??_GDPlaySessionMgr scalar dtor)
// The compiler's own auto-generated scalar deleting destructor (calls ~DPlaySessionMgr then
// conditionally operator delete). Synthesized as a byproduct of the virtual dtor below, which
// also drives the ctor's +0x0 vtable store.

// The Win32 heap-free path for local-queue node payloads of type 0x15/0x17. Declared inline
// (rather than #include <windows.h>) to avoid rotating this already-matched TU's codegen; both
// go through the IAT (__declspec(dllimport)) so the call reproduces the original's indirect
// `call [__imp_...]` shape.
__declspec(dllimport) void *__stdcall GetProcessHeap(void);
__declspec(dllimport) int __stdcall HeapFree(void *hHeap, unsigned long dwFlags, void *lpMem);

// FUNCTION: LOCO 0x43dc30
// Destroy the session manager: re-stamp the vtable (compiler-synthesized from the virtual dtor),
// then under the GameNet queue lock drain the local message queue -- freeing each node's payload
// by opcode `type` (type 2 payload is its own owned sub-list; 0xf/0x11 release the send queue's
// pending object via its virtual dtor; 0x15/0x17 HeapFree; everything else operator delete),
// then the node itself. Release the lock, delete both queued-node lists (field_0x7dc and the
// pending-train queue), and free every provider slot's result chain and layout blob.
DPlaySessionMgr::~DPlaySessionMgr() {
    g_pGameNetMsgQueueLock->Lock();
    while (g_pNetMsgLocalQueueHead != 0) {
        void *payload = g_pNetMsgLocalQueueHead->pPayload;
        NetMsgQueueNode *pNext = g_pNetMsgLocalQueueHead->pNext;
        if (payload != 0) {
            switch (g_pNetMsgLocalQueueHead->type) {
            case 0xf:
            case 0x11:
                delete (GameNetQueuedNodeMaybe *)g_pNetMsgSendQueueHead->pPayload;
                g_pNetMsgSendQueueHead->pPayload = 0;
                break;
            case 0x15:
            case 0x17:
                HeapFree(GetProcessHeap(), 0, payload);
                g_pNetMsgLocalQueueHead->pPayload = 0;
                break;
            case 2:
                {
                    NetMsgType2PayloadNode *p =
                        (NetMsgType2PayloadNode *)g_pNetMsgLocalQueueHead->pPayload;
                    while (p != 0) {
                        NetMsgType2PayloadNode *cur = p;
                        p = p->pNext;
                        if (cur->pSubPayload != 0) {
                            operator delete(cur->pSubPayload);
                            cur->pSubPayload = 0;
                        }
                        operator delete(cur);
                    }
                }
                g_pNetMsgLocalQueueHead->pPayload = 0;
                break;
            default:
                operator delete(payload);
                g_pNetMsgLocalQueueHead->pPayload = 0;
                break;
            }
        }
        operator delete(g_pNetMsgLocalQueueHead);
        g_pNetMsgLocalQueueHead = pNext;
    }
    g_pGameNetMsgQueueLock->Unlock();

    GameNetQueuedNodeMaybe *pNode;
    while ((pNode = field_0x7dc) != 0) {
        field_0x7dc = pNode->pNext;
        delete pNode;
    }
    while ((pNode = pPendingTrainQueueHead) != 0) {
        pPendingTrainQueueHead = pNode->pNext;
        delete pNode;
    }

    DPlaySessionMgrProviderSlot *slot = aProviderSlots;
    int nSlots = 9;
    do {
        while (slot->pResultsChainHead != 0) {
            GameNetRosterResultNode *n = slot->pResultsChainHead;
            slot->pResultsChainHead = n->pNext;
            operator delete(n);
        }
        if (slot->pLayoutData != 0) {
            operator delete(slot->pLayoutData);
            slot->pLayoutData = 0;
        }
        slot++;
    } while (--nSlots != 0);
}

// FUNCTION: LOCO 0x43d130
// Reset the provider-slot grid: name the session "Default", set the 3x3 grid geometry and
// slot count (9), clear the current selection, then per slot clear its roster fields and
// stamp each with the current WorldBoard grid dimensions. bInit != 0 is the fresh-construct
// path (just null the result-chain head + layout blob); bInit == 0 is the re-reset path,
// which frees each slot's result chain node-by-node and its owned layout blob.
//
// Two exact-match levers here: (1) the per-slot loop MUST use direct `aProviderSlots[i].field`
// subscripting, NOT a hoisted `DPlaySessionMgrProviderSlot *slot = &aProviderSlots[i];` local
// -- /O2 strength-reduces both to the same down-counter + esi anchored at slot+0x38 (hot
// inner-loop pResultsChainHead field, zero-displacement), but the hoisted-pointer form
// slips the `bInit!=0` cmp two insns earlier, and a `slot++` pointer-walk anchors esi at
// slot+0 instead (byte_diff 27). The direct array walk is the idiom ResolveIdToSlot/SetMode
// already use. (2) `pLayoutData = 0` lives INSIDE both branches (the bInit!=0 arm and
// the `if (pLayoutData != 0)` free block), NOT unconditionally after -- the compiler
// cross-jumps the two stores into one shared tail, so a slot whose layout blob is already
// null skips the redundant zero-store.
void DPlaySessionMgr::ResetProviders(char bInit) {
    field_0x8 = 9;
    strcpy(sessionName, "Default");
    bLayoutSyncPingSent = 0;
    nProviderSlotsPerRow = 3;
    nProviderSlotRows = 3;
    pSelectedProvider = 0;
    selectedProviderIndex = -1;

    for (int i = 0; i < 9; i++) {
        aProviderSlots[i].providerId = 0;
        aProviderSlots[i].sAddressOrName[0] = 0;
        aProviderSlots[i].sLongName[0] = 0;
        aProviderSlots[i].bDirty = 0;
        if (bInit != 0) {
            aProviderSlots[i].pResultsChainHead = 0;
            aProviderSlots[i].pLayoutData = 0;
        } else {
            while (aProviderSlots[i].pResultsChainHead != 0) {
                GameNetRosterResultNode *n = aProviderSlots[i].pResultsChainHead;
                aProviderSlots[i].pResultsChainHead = n->pNext;
                operator delete(n);
            }
            if (aProviderSlots[i].pLayoutData != 0) {
                operator delete(aProviderSlots[i].pLayoutData);
                aProviderSlots[i].pLayoutData = 0;
            }
        }
        aProviderSlots[i].wCols = g_worldBoard.wCols;
        aProviderSlots[i].wRows = g_worldBoard.wRows;
        aProviderSlots[i].bEnabled = 0;
        aProviderSlots[i].nLayoutDataSize = 0;
        aProviderSlots[i].wLayoutCols = 0;
        aProviderSlots[i].wLayoutRows = 0;
        aProviderSlots[i].dwLayoutVersion = 0;
    }
}

// FUNCTION: LOCO 0x43d230
// Linear-scan aProviderSlots[9] for the entry whose providerId matches; return its
// index, or -1 if none. (From the DPlaySessionMgr session-management code region at
// 0x43cxxx -- a distinct source area from the roster helpers above; grouped here with
// the rest of the class's methods for now, exact .obj TU boundary TBD.)
int DPlaySessionMgr::ResolveIdToSlot(int providerId) {
    for (int i = 0; i < 9; i++) {
        if (aProviderSlots[i].providerId == (unsigned int)providerId)
            return i;
    }
    return -1;
}

// FUNCTION: LOCO 0x43d210
// Return the currently-selected provider slot, but only while a session is live
// (connectionMode == 2); otherwise NULL.
DPlaySessionMgrProviderSlot *DPlaySessionMgr::GetSelectedProvider() {
    if (connectionMode != 2)
        return 0;
    return pSelectedProvider;
}

// FUNCTION: LOCO 0x43d250
// Post a simple 4-byte layout-net opcode (0x3fa) to the send queue, addressed to one peer
// (destPlayerId), reliably. A this-ignoring thiscall method of the session-manager singleton (the
// body never touches `this`, but callers load ecx=this at the call site); see the header note.
void DPlaySessionMgr::LayoutNet_PostSimpleOpcode(int destPlayerId) {
    unsigned short *pPayload = (unsigned short *)::operator new(4);
    *pPayload = 0x3fa;
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 6;
    pNode->payloadLen = 4;
    pNode->pPayload = pPayload;
    pNode->destPlayerId = destPlayerId;
    pNode->bReliable = 1;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x43d2b0
// Transition the session-connection state machine to `mode` (0..3). No-op if already there.
// Mode 1 sets a long poll interval (500); mode 2 re-resolves the selected provider slot by
// searchProviderId, sets a short interval (20) and kicks a layout-net opcode; modes 0/3 are
// idle. Out-of-range modes clamp to 3.
void DPlaySessionMgr::SetMode(int mode) {
    if (mode == connectionMode)
        return;
    connectionMode = mode;
    switch (mode) {
    case 2:
        {
            for (int i = 0; i < 9; i++) {
                if (aProviderSlots[i].providerId == (unsigned int)searchProviderId) {
                    pSelectedProvider = &aProviderSlots[i];
                    selectedProviderIndex = i;
                    break;
                }
            }
            stateTimeoutMs = 0x14;
            LayoutNet_PostSimpleOpcode(0);
        }
        break;
    case 1:
        stateTimeoutMs = 500;
        break;
    default:
        connectionMode = 3;
        break;
    case 0:
    case 3:
        break;
    }
}

// FUNCTION: LOCO 0x43de30
// HasProviderSlotRoomInHeading: map (heading 0/90/180/270) to a grid step and test
// whether the current/given slot has a neighbour in that direction. `index` defaults to
// selectedProviderIndex when negative; heading/40 selects the axis (0=up, 2=right,
// 4=down, 6=left over the nProviderSlotsPerRow x nProviderSlotRows grid).
bool DPlaySessionMgr::HasProviderSlotRoomInHeading(int heading, int index) {
    int axis = heading / 40;
    if (index < 0)
        index = selectedProviderIndex;
    int nPerRow = nProviderSlotsPerRow;
    int row = index / nPerRow;
    int col = index - row * nPerRow;
    switch (axis) {
    case 0:
        if (row > 0) return true;
        break;
    case 2:
        if (col < nPerRow - 1) return true;
        break;
    case 4:
        if (row < nProviderSlotRows - 1) return true;
        break;
    case 6:
        if (col > 0) return true;
        break;
    }
    return false;
}

// The four constant-dispatch wrappers over HasProviderSlotRoomInHeading -- MOVED IN
// 2026-07-22 (v322) from src/phase2_probe2.cpp (their class declaration has lived in
// DPlaySessionMgr.h since v127; this retires the last out-of-TU definitions).

// FUNCTION: LOCO 0x43ddf0
bool DPlaySessionMgr::IsType0x5a() { return HasProviderSlotRoomInHeading(0x5a, -1); }

// FUNCTION: LOCO 0x43de00
bool DPlaySessionMgr::IsType0x10e() { return HasProviderSlotRoomInHeading(0x10e, -1); }

// FUNCTION: LOCO 0x43de10
bool DPlaySessionMgr::IsType0() { return HasProviderSlotRoomInHeading(0, -1); }

// FUNCTION: LOCO 0x43de20
bool DPlaySessionMgr::IsType0xb4() { return HasProviderSlotRoomInHeading(0xb4, -1); }

// FUNCTION: LOCO 0x43f000
// Producer for send-queue type 3 (join-or-host attempt). No payload. __thiscall taking pMgr
// but never reading it (confirmed: its sole transcribed call site, 0x40ab72, reloads
// ecx=g_pDPlaySessionMgr immediately before the call) -- same this-ignoring-thiscall class as
// GameNet_PostPrepareInternet/UIResources::PlayUiSound.
void __fastcall GameNet_PostAttemptJoin(DPlaySessionMgr *pMgr) {
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 3;
    pNode->pPayload = 0;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x43f030
// Producer for send-queue type 1 (prepare internet connection). Payload carries the
// "use secondary remembered choice" flag (ignored by the type-1 handler). __thiscall taking
// pMgr but never reading it (confirmed: every call site -- 0x40a296/0x40ab6d/0x40aab1/others --
// reloads ecx=g_pDPlaySessionMgr immediately before the call) -- same this-ignoring-thiscall
// class as UIResources::PlayUiSound/PlaySoundAtScreenPos.
void __fastcall GameNet_PostPrepareInternet(DPlaySessionMgr *pMgr) {
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 1;
    pNode->pPayload = (void *)(unsigned int)g_pNetSettings->bUseSecondaryRememberedChoice;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x43f070
// Producer for send-queue type 0 (reset/teardown connection). Payload = the "use secondary
// remembered choice" flag (as 0/1); bReliable is repurposed to carry pMgr->field_0x8.
void __fastcall GameNet_PostResetConnection(DPlaySessionMgr *pMgr) {
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 0;
    if (g_pNetSettings->bUseSecondaryRememberedChoice != 0)
        pNode->pPayload = (void *)1;
    else
        pNode->pPayload = 0;
    pNode->bReliable = pMgr->field_0x8;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x43ee80
// Producer for send-queue type 0xe (connect-or-join for a specific train peer). Stashes pTrainNode
// as the queued command's payload and sets its bHasDetailFlagMaybe so GameNet_ConnectOrJoinSession
// appends it to the pending-peer list (with a long countdown) instead of acting immediately.
// A this-ignoring thiscall member: its body never reads `this`, but the sole caller
// (RequestTrainMoveOrReleaseNode) loads ecx=this, so it must be a member to match that load.
char DPlaySessionMgr::GameNet_PostConnectOrJoinForNode(PeerTrainNodePartial *pTrainNode) {
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 0xe;
    pNode->pPayload = pTrainNode;
    pTrainNode->bHasDetailFlagMaybe = 1;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
    return 1;
}

// FUNCTION: LOCO 0x43eec0
// Producer for send-queue type 0x10 (move a train off the local board toward a neighbour slot).
// Picks a hand-off heading from the (fromSlot,toSlot) pair -- toSlot==0 => 0deg, fromSlot==0 =>
// 270deg, else 180deg if fromSlot<=toSlot / 90deg otherwise -- then probes that heading (and the
// fallbacks 0/270/90 in turn) for provider-slot room; the first heading with room wins. On success
// posts the train as the type-0x10 command payload with its own heading stamped, and returns 1;
// if no heading has room, returns 0 without queueing. A this-ignoring thiscall member (uses the
// g_pDPlaySessionMgr singleton explicitly, never `this`); the caller loads ecx=this so it must be a member.
// EFFECTIVE MATCH: 68/68 insns, identical structure. The sole residual is an intrinsic
// symmetric-register-swap (Yoda #29/#30): /O2 colors the two arg loads toSlot->eax/fromSlot->ecx
// where the original picks toSlot->ecx/fromSlot->eax, cascading into the cmp operand order and
// setle-vs-setg branchless-select polarity. Confirmed unsteerable -- comparison direction
// (fromSlot<=toSlot vs toSlot>=fromSlot) and branch nesting order do not move the first-load color.
char DPlaySessionMgr::TrainNet_PostMoveRequestForNode(int fromSlot, int toSlot,
                                                           PeerTrainNodePartial *pTrainNode) {
    int heading;
    if (toSlot == 0)
        heading = 0;
    else if (fromSlot == 0)
        heading = 0x10e;
    else
        heading = fromSlot <= toSlot ? 0xb4 : 0x5a;

    if (!g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(heading, -1)) {
        heading = 0;
        if (!g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(heading, -1)) {
            heading = 0x10e;
            if (!g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(heading, -1)) {
                heading = 0x5a;
                if (!g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(heading, -1))
                    return 0;
            }
        }
    }

    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 0x10;
    pNode->pPayload = pTrainNode;
    pNode->nMoveHeading = heading;
    pTrainNode->wHeading = (unsigned short)heading;
    pTrainNode->bHasDetailFlagMaybe = 1;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
    return 1;
}

// FUNCTION: LOCO 0x43efa0  (?GameNet_ResetProvidersAndPostTeardown@@YIXPAVDPlaySessionMgr@@@Z)
// Producer for send-queue type 5 (dispatched by GameNetManager_HandleQueuedEvent case 5,
// and by FUN_0043f880 on a failed connect). Clears the connect-pending flag, resets all provider
// slots, then posts a payload-less type-5 teardown trigger onto the GameNet send queue.
void __fastcall GameNet_ResetProvidersAndPostTeardown(DPlaySessionMgr *pMgr) {
    pMgr->bConnectPending = false;
    pMgr->ResetProviders(0);
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 5;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x43ded0  (?GameNet_BroadcastRosterTick@@YIXPAVDPlaySessionMgr@@@Z)
// Roster broadcast (opcode 0x3f6), fired once every nRosterBroadcastPeriodTicks ticks while connected: when the
// selected provider slot has any placement-result nodes AND at least one *other* enabled peer slot
// exists, pack each result node's (trainId, x, y, ownerA, slotKey) into an 8-byte wire record and
// post the whole list unreliably to the send queue. If there is no other enabled peer to receive
// it, skip. The return value is dead on every path (void) -- Ghidra's inferred `int` is noise.
//
// EFFECTIVE MATCH (DIFF ~248B, insns 92/98): structure is fully faithful and every real instruction
// aligns (the record-packing loop, the header stores, the enqueue tail all byte-align). Two
// residuals, both intrinsic: (1) the peer-slot scan is a two-exit mid-exit loop (match test at top,
// advance+bound at bottom) -- /O2 declines to duplicate the exhausted-return epilogue inline the way
// the original does (the original falls through to a 6-insn copy of the epilogue, mine tail-merges
// that return into the shared far epilogue and `jmp`s back to the loop top), which shifts every
// downstream offset by a constant and inflates the byte diff; same block-layout class as the sibling
// drain loop 0x43e010 (Yoda #15/#18). (2) A 1-insn register tie-break on the tick: the original
// computes `++tick` in ecx and copies it to eax for the idiv dividend, mine computes it in eax
// directly (no copy) -- symmetric-swap class (Yoda #29/#30). See docs/PARKED.md.
void __fastcall GameNet_BroadcastRosterTick(DPlaySessionMgr *pMgr) {
    // Cached in a callee-saved register up front (used by the peer-slot scan below).
    int selIdx = pMgr->selectedProviderIndex;
    if (pMgr->connectionMode != 2)
        return;
    int tick = pMgr->rosterBroadcastTick + 1;
    pMgr->rosterBroadcastTick = tick;
    if (tick % pMgr->nRosterBroadcastPeriodTicks != 0)
        return;
    pMgr->rosterBroadcastTick = 0;
    GameNetRosterResultNode *pNode = pMgr->pSelectedProvider->pResultsChainHead;
    if (pNode == 0)
        return;
    int count = pMgr->field_0x8;
    int i = 0;
    if (count <= 0)
        return;
    // Find the first *other* enabled peer slot; if there is none, there's no one to send to.
    DPlaySessionMgrProviderSlot *pSlot = pMgr->aProviderSlots;
    do {
        if (i != selIdx && pSlot->bEnabled)
            goto found;
        i++;
        pSlot++;
    } while (i < count);
    return;  // scanned every slot, no eligible peer

found:
    RosterTickWireMsg *pMsg = (RosterTickWireMsg *)::operator new(0x8000);
    pMsg->wOpcode = 0x3f6;
    // Materialize the index in a full 32-bit register before the byte store (dword load + `mov
    // [msg+4],al`), read after the opcode store so it isn't hoisted -- see LayoutNet_RequestLayoutList.
    unsigned int idx = pMgr->selectedProviderIndex;
    pMsg->bProviderIndex = (unsigned char)idx;
    pMsg->bConst1 = 1;
    pMsg->wCount = 0;
    do {
        pMsg->wCount++;
        RosterTickRecord rec;
        rec.wTrainId = (unsigned short)pNode->trainId;
        rec.wPosX = (unsigned short)pNode->posX;
        rec.wPosY = (unsigned short)pNode->posY;
        rec.bOwnerA = pNode->bOwnerA;
        rec.bSlotKey = pNode->bSlotKey;
        pMsg->records[pMsg->wCount - 1] = rec;
        pNode = pNode->pNext;
    } while (pNode != 0);

    int payloadLen = pMsg->wCount * 8 + 10;
    NetMsgQueueNode *pQNode = new NetMsgQueueNode();
    pQNode->type = 6;
    pQNode->payloadLen = payloadLen;
    pQNode->pPayload = pMsg;
    pQNode->destPlayerId = 0;
    pQNode->bReliable = 0;
    g_pGameNetThreadState->EnqueueOrFreeNode(pQNode);
}

// FUNCTION: LOCO 0x440310
// Broadcast opcode 0x3f4 (this session's provider slot is now enabled/joinable): mark the
// selected slot enabled, then post a reliable 4-byte layout-net message to the send queue.
// When not connected (connectionMode != 2) the slot pointer is NULL and the bEnabled
// store dereferences it -- reproduced as-is (the caller only broadcasts while connected).
void __fastcall GameNet_BroadcastSlotEnabled(DPlaySessionMgr *pMgr) {
    DPlaySessionMgrProviderSlot *pSlot =
        (pMgr->connectionMode != 2) ? 0 : pMgr->pSelectedProvider;
    pSlot->bEnabled = 1;  // sic: NULL-derefs when not connected
    unsigned short *pPayload = (unsigned short *)::operator new(4);
    *pPayload = 0x3f4;
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 6;
    pNode->payloadLen = 4;
    pNode->pPayload = pPayload;
    pNode->destPlayerId = 0;
    pNode->bReliable = 1;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x440390
// Broadcast opcode 0x3f5 (this session's provider slot is now disabled): when connected, clear
// the selected slot's bEnabled, then -- if the send-queue thread exists -- post a reliable
// 4-byte layout-net message.
//
// EFFECTIVE MATCH (DIFF 2): structure byte-identical. Sole residual is a symmetric register
// tie-break on the conditional slot store: the original reuses the dead `this`/ecx for the
// `pSelectedProvider` load+store (`mov ecx,[ecx+0x7cc]; mov [ecx+0x36],bl`), mine picks the
// equally-free eax (`mov eax,...`). Unlike the sibling Enabled (0x440310, a ternary whose merge
// pins the result register -- fixed via the `!=2 ? 0 : ...` form), this plain conditional store
// has no merge to constrain the choice; an explicit local pointer did not move it. Both sides
// reuse an equally-dead register (orig self-overwrites the ecx BASE, mine self-overwrites the
// dead eax comparison operand), which is what makes it a true coin flip. v352 additionally
// probed converting this to a real `DPlaySessionMgr::` member -- ABI-identical for this shape
// (ECX + no stack args + plain `ret`), and plausible since the sibling
// GameNet_BroadcastLocalOrigin (0x440410) IS a member -- result was byte-for-byte NEUTRAL, so
// it is no evidence either way and the free-function form is kept for family consistency.
// Intrinsic register-swap class (Yoda #29/#30). See docs/PARKED.md.
void __fastcall GameNet_BroadcastSlotDisabled(DPlaySessionMgr *pMgr) {
    if (pMgr->connectionMode == 2)
        pMgr->pSelectedProvider->bEnabled = 0;
    if (g_pGameNetThreadState != 0) {
        unsigned short *pPayload = (unsigned short *)::operator new(4);
        *pPayload = 0x3f5;
        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 6;
        pNode->payloadLen = 4;
        pNode->pPayload = pPayload;
        pNode->destPlayerId = 0;
        pNode->bReliable = 1;
        g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
    }
}

// FUNCTION: LOCO 0x440150
// Inbound placement-event handler for event types 0x12/0x15/0x17 (all dispatched here together by
// GameNetManager_HandleQueuedEvent):
//   * 0x12 -- place ONE train whose (x,y) is resolved from its heading (0/0x5a/0xb4/0x10e -> one of
//     the 4 edge helpers, then nudge x or y +1), filed via SetTrainPlacementResult. The
//     placement data (heading/trainId/owner bytes) is stuffed inline into the queue node.
//   * 0x15 -- apply a full roster-tick payload: (optionally, bConst1) drain the target slot's
//     result chain, then re-file each packed record via SetTrainPlacementResult, then HeapFree.
//   * 0x17 -- remove ONE train's placement result, then HeapFree the payload.
//
// EXACT (v360). Was parked for many sessions on the "dead signed-index guard" class: the
// original's `&aProviderSlots[bProviderIndex]` carries a `test;jl;xor esi` guard that a direct
// subscript folds away. That class was never intrinsic -- the guard belongs to the implicitly
// inline DPlaySessionMgr::ProviderSlotAt accessor (see DPlaySessionMgr.h); routing this site
// through it reproduced the guard and the whole function fell out byte-exact, which also closed
// the second residual below. Kept for the record: (2) the
// owner-byte push -- in every case the original loads the byte into a register still holding a
// live value (leftover `this`/coord) so the upper bytes are garbage (CONCAT31), while my compile
// finds the register free and zero-extends (`xor ecx,ecx; mov cl`). This is register-liveness,
// not source-steerable: the matching sibling HandleQueuedTrainPlacement (0x43e370)
// reproduces it only because its trainId is a WORD (its zero-extend keeps a register dirty),
// whereas here trainId is a full dword (mov eax,[+0x10]) -- a genuinely different arg shape. The
// switch case order, loop shape (for vs if+do-while), and heading signedness were all probed:
// heading must be `unsigned` (ja pivot, not jg); the rest are inert.
void DPlaySessionMgr::HandleQueuedPlacementEvent(NetMsgQueueNode *pMsg) {
    // Cases written 0x17/0x15/0x12 to match the original's block layout: the subtract-chain
    // dispatch tests values ascending (0x12,0x15,0x17), so 0x17 is the fall-through and is emitted
    // first, followed by the 0x15 then 0x12 bodies (jump targets) in source order.
    switch (pMsg->type) {
    case 0x17: {
        TrainOriginWireMsg *pPayload = (TrainOriginWireMsg *)pMsg->pPayload;
        RemoveTrainPlacementResult(pPayload->nTrainId, pPayload->bOwnerByteA,
            pPayload->bOwnerByteB);
        HeapFree(GetProcessHeap(), 0, pMsg->pPayload);
        pMsg->pPayload = 0;
        break;
    }
    case 0x15: {
        RosterTickWireMsg *pPayload = (RosterTickWireMsg *)pMsg->pPayload;
        DPlaySessionMgrProviderSlot *pSlot = ProviderSlotAt(pPayload->bProviderIndex);
        if (pPayload->bConst1 != 0) {
            while (pSlot->pResultsChainHead != 0) {
                GameNetRosterResultNode *n = pSlot->pResultsChainHead;
                pSlot->pResultsChainHead = n->pNext;
                operator delete(n);
            }
        }
        for (int i = 0; i < (int)pPayload->wCount; i++) {
            SetTrainPlacementResult(pPayload->records[i].wTrainId,
                pPayload->records[i].bOwnerA, pPayload->records[i].bSlotKey,
                pPayload->records[i].wPosX, pPayload->records[i].wPosY);
        }
        HeapFree(GetProcessHeap(), 0, pPayload);
        pMsg->pPayload = 0;
        break;
    }
    case 0x12: {
        Pair16 coord = {0, 0};
        switch (pMsg->heading) {
        case 0x5a:  // 90deg -> left edge
            coord = g_NetSessionEventQueueEdge.ComputeLeftEdgePlacement();
            coord.hi += 1;
            break;
        case 0:     // 0deg -> bottom edge
            coord = g_NetSessionEventQueueEdge.ComputeBottomEdgePlacement();
            coord.lo += 1;
            break;
        case 0xb4:  // 180deg -> top edge
            coord = g_NetSessionEventQueueEdge.ComputeTopEdgePlacement();
            coord.lo += 1;
            break;
        case 0x10e: // 270deg -> right edge
            coord = g_NetSessionEventQueueEdge.ComputeRightEdgePlacement();
            coord.hi += 1;
            break;
        }
        SetTrainPlacementResult(pMsg->eventTrainId, pMsg->bEventOwnerA,
            pMsg->bEventOwnerB, coord.lo, coord.hi);
        break;
    }
    }
}

// FUNCTION: LOCO 0x4404c0
// Remove-and-free the placement-result node matching (trainId, bOwnerA): searches the preferred
// slot chain (bOwnerB&0xff), then bOwnerA's own slot, then linearly scans the other slots; unlinks
// and deletes the first match. The first two phases share the unlink tail via a goto (the compiler's
// cross-jump). Called by GameNet_BroadcastLocalOrigin when a train leaves a slot.
//
// EFFECTIVE MATCH (DIFF 174/328, insns 134/135). v360 routed every slot lookup through the
// ProviderSlotAt accessor (see DPlaySessionMgr.h), which supplies the `jl -> NULL` guard the old
// direct-subscript form folded away: DIFF 275 -> 174 and the instruction counts now line up. The
// remainder is register allocation -- the original keeps the SLOT pointer in esi and reaches the
// chain head as `[esi+0x38]`, while this compile strength-reduces to the FIELD address
// (`lea esi,[eax+0x38]`), and the original spills the masked index to a stack slot. Not yet
// steerable. See docs/PARKED.md.
void DPlaySessionMgr::RemoveTrainPlacementResult(int trainId, unsigned char bOwnerA,
        unsigned char bOwnerB) {
    DPlaySessionMgrProviderSlot *pSlot;
    GameNetRosterResultNode *node, *prev;

    pSlot = ProviderSlotAt(bOwnerB & 0xff);
    prev = 0;
    if (pSlot != 0) {
        for (node = pSlot->pResultsChainHead; node; node = node->pNext) {
            if (node->trainId == trainId && node->bOwnerA == bOwnerA)
                goto unlink;
            prev = node;
        }
    }
    pSlot = ProviderSlotAt(bOwnerA);
    prev = 0;
    if (pSlot != 0) {
        for (node = pSlot->pResultsChainHead; node; node = node->pNext) {
            if (node->trainId == trainId && node->bOwnerA == bOwnerA) {
            unlink:
                if (prev == 0)
                    pSlot->pResultsChainHead = node->pNext;
                else
                    prev->pNext = node->pNext;
                ::operator delete(node);
                return;
            }
            prev = node;
        }
    }
    for (int i = 0; i < 9; i++) {
        pSlot = ProviderSlotAt(i);
        if (i != bOwnerA && i != (int)(bOwnerB & 0xff)) {
            prev = 0;
            for (node = pSlot->pResultsChainHead; node; node = node->pNext) {
                if (node->trainId == trainId && node->bOwnerA == bOwnerA) {
                    if (prev == 0)
                        pSlot->pResultsChainHead = node->pNext;
                    else
                        prev->pNext = node->pNext;
                    ::operator delete(node);
                    return;
                }
                prev = node;
            }
        }
    }
}

// FUNCTION: LOCO 0x440410
// Broadcast opcode 0x3f7 ("a local train left its origin slot"): a no-op unless connected
// (connectionMode == 2). When the departing train's slot key (bOwnerB) matches our own selected
// provider index, post a reliable 0xc-byte layout-net message carrying (trainId, bOwnerA, bOwnerB)
// to the send queue; then always remove the matching placement-result node. The eligibility check
// reads the DPlaySessionMgr singleton through its global (g_pDPlaySessionMgr), not `this`.
void DPlaySessionMgr::GameNet_BroadcastLocalOrigin(int trainId, unsigned char bOwnerA,
        unsigned char bOwnerB) {
    if (connectionMode == 2) {
        if (g_pDPlaySessionMgr->selectedProviderIndex == (int)(bOwnerB & 0xff)) {
            TrainOriginWireMsg *pMsg = new TrainOriginWireMsg;
            pMsg->wOpcode = 0x3f7;
            pMsg->nTrainId = trainId;
            pMsg->bOwnerByteA = bOwnerA;
            pMsg->bOwnerByteB = (unsigned char)bOwnerB;

            NetMsgQueueNode *pNode = new NetMsgQueueNode();
            pNode->type = 6;
            pNode->payloadLen = 0xc;
            pNode->pPayload = pMsg;
            pNode->destPlayerId = 0;
            pNode->bReliable = 1;
            g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
        }
        RemoveTrainPlacementResult(trainId, bOwnerA, bOwnerB);
    }
}

// FUNCTION: LOCO 0x440610
// Host-only (connectionMode==2) placement-result upsert: find-or-create the node for (trainId,
// bOwnerA) and refresh its position (x,y). On create, prepend to aProviderSlots[bOwnerB].chain. On
// update, if the node's filed slot (bSlotKey) changed, unlink it from the old slot's chain and
// re-file it under the new bOwnerB slot. Consumed by GameNet_BroadcastRosterTick.
//
// EFFECTIVE MATCH: structure faithful (the update branch is written first / fall-through with the
// create branch at the end, matching the original's `je create` layout -- branch order was a real
// lever, DIFF 250->226). v360 then routed all three slot lookups through the ProviderSlotAt
// accessor (see DPlaySessionMgr.h) -- which supplies the `jl -> NULL` guard the direct-subscript
// form folded away -- restored the original's own arm order in the unlink (`prev != 0` first, so
// the prev==0 arm goes out of line), and added the create path's own NULL check on the guarded
// slot. Score 149957 -> 24012, insns 115/113, reg_pen=0 identity_miss=0. The only residual left
// is that this compile materializes the `oldSlotKey` byte->int widening THROUGH MEMORY
// (`mov [esp+0x24],cl` / `mov ecx,[esp+0x24]`) where the original masks in register
// (`mov cl,[eax+0xd]` ... `and ecx,0xff`). Probed and refuted: `& 0xff` on the already-byte
// local (a no-op, byte-identical) and declaring oldSlotKey `unsigned int` (much worse, 88140 --
// it turns the original's byte `cmp cl,bl` into a dword compare). See docs/PARKED.md.
void DPlaySessionMgr::SetTrainPlacementResult(int trainId, unsigned char bOwnerA,
        unsigned char bOwnerB, int x, int y) {
    if (connectionMode != 2)
        return;
    GameNetRosterResultNode *node = FindTrainPlacementResult(trainId, bOwnerA, bOwnerB);
    if (node != 0) {
        // Update the existing node's position; re-file it if its slot key changed.
        node->posX = x;
        unsigned char oldSlotKey = node->bSlotKey;
        node->trainId = trainId;
        node->posY = y;
        if (oldSlotKey == (unsigned char)bOwnerB)
            return;

        // Slot changed: unlink node from its old chain, then re-file under bOwnerB.
        GameNetRosterResultNode *prev = 0;
        GameNetRosterResultNode *scan;
        DPlaySessionMgrProviderSlot *pOldSlot = ProviderSlotAt(oldSlotKey);
        for (scan = pOldSlot->pResultsChainHead; scan; scan = scan->pNext) {
            if (scan->trainId == trainId && scan->bOwnerA == bOwnerA) break;
            prev = scan;
        }
        if (scan != 0) {
            if (prev != 0) {
                prev->pNext = node->pNext;
                node->pNext = 0;
            } else {
                pOldSlot->pResultsChainHead = node->pNext;
            }
        }
        node->bSlotKey = (unsigned char)bOwnerB;
        DPlaySessionMgrProviderSlot *pNewSlot = ProviderSlotAt(bOwnerB & 0xff);
        node->pNext = pNewSlot->pResultsChainHead;
        pNewSlot->pResultsChainHead = node;
        return;
    }

    // Create a fresh node and prepend it to the bOwnerB slot's chain.
    node = (GameNetRosterResultNode *)::operator new(0x14);
    DPlaySessionMgrProviderSlot *pSlot = ProviderSlotAt(bOwnerB & 0xff);
    if (pSlot != 0) {
        node->trainId = trainId;
        node->bOwnerA = bOwnerA;
        node->posX = x;
        node->bSlotKey = (unsigned char)bOwnerB;
        node->posY = y;
        node->pNext = pSlot->pResultsChainHead;
        pSlot->pResultsChainHead = node;
    }
}

// FUNCTION: LOCO 0x440750
// Look up an existing placement-result node by (trainId, bOwnerA): tries the preferred slot chain
// (bOwnerB&0xff) first, then bOwnerA's own slot, then linearly scans the other 7 slots. Null if none.
// EFFECTIVE MATCH: structure is faithful; the dominant residual is MSVC 5.0's dead signed-index
// guard on each `&aProviderSlots[idx]` (0x4c-byte element): the original emits a `jl; ...; xor;
// test; je` (and the branchless `setl; dec; and` in the linear scan) that the recompile folds away
// because it proves the index non-negative -- the same intrinsic guard parked EFFECTIVE on
// GameNet_DrainPendingTrainQueue/RequestTrainMoveOrReleaseNode. A signed-int index local, an
// explicit `if (&slot != 0)` guard, and an unmasked index were all tried; none reproduce it (matches
// the v238 finding). See docs/PARKED.md.
GameNetRosterResultNode *DPlaySessionMgr::FindTrainPlacementResult(int trainId,
        unsigned char bOwnerA, unsigned char bOwnerB) {
    GameNetRosterResultNode *node;
    DPlaySessionMgrProviderSlot *pSlot = ProviderSlotAt(bOwnerB & 0xff);
    if (pSlot != 0) {
        for (node = pSlot->pResultsChainHead; node; node = node->pNext)
            if (node->trainId == trainId && node->bOwnerA == bOwnerA)
                return node;
    }
    pSlot = ProviderSlotAt(bOwnerA);
    if (pSlot != 0) {
        for (node = pSlot->pResultsChainHead; node; node = node->pNext)
            if (node->trainId == trainId && node->bOwnerA == bOwnerA)
                return node;
    }
    for (int i = 0; i < 9; i++) {
        if (i != bOwnerA && i != (int)(bOwnerB & 0xff)) {
            // sic: no null check on this one, unlike the two above -- the guard inside
            // ProviderSlotAt can't fire for a loop counter that starts at 0.
            for (node = ProviderSlotAt(i)->pResultsChainHead; node; node = node->pNext)
                if (node->trainId == trainId && node->bOwnerA == bOwnerA)
                    return node;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x43d620
// Broadcast a "request the current layout list" message (opcode 0x3f8, queue type 6, reliable)
// carrying this session's selected provider-slot index, then enqueue a bare type-0x19 command
// (no payload) that follows it up.
void DPlaySessionMgr::LayoutNet_RequestLayoutList() {
    LayoutReqWireMsg *pMsg = new LayoutReqWireMsg;
    pMsg->wOpcode = 0x3f8;
    // The original materializes selectedProviderIndex in a full 32-bit register before the
    // byte store (mov eax,[this+0x7d0] / mov [pMsg+4],al); a direct `field = selectedProviderIndex`
    // narrows to a byte load (mov al,[..]) instead. The int temp -- read AFTER the opcode store
    // so it isn't hoisted -- reproduces the dword load. (Mirror of the "byte spill needs an
    // unsigned int local" codegen family in CLAUDE.md.)
    unsigned int idx = selectedProviderIndex;
    pMsg->bProviderIndex = (unsigned char)idx;

    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 6;
    pNode->payloadLen = 6;
    pNode->pPayload = pMsg;
    pNode->destPlayerId = 0;
    pNode->bReliable = 1;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);

    pNode = new NetMsgQueueNode();
    pNode->type = 0x19;
    pNode->payloadLen = 0;
    pNode->pPayload = 0;
    pNode->destPlayerId = 0;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x43d350
// Snapshot the live board into a fresh LocoBitmap (rendered by WorldBoardMaybe::CaptureBoardToBitmap),
// pack it into an opcode-0x3f9 bitmap message, cache a private copy in the local provider slot,
// and send it reliably to destPlayerId. No-op unless connected (connectionMode == 2). The bitmap
// is a raw-pointer new/delete pair; /GX wraps the new-expression in automatic alloc-protection
// (the SEH prologue scaffolding here), not a user try/catch.
//
// EFFECTIVE MATCH (DIFF ~225B): structure is fully faithful (insn count 140/142; the two-insn
// gap is /O2 SPECULATIVELY hoisting the `pMsg->dwColsRows` load above the `selectedProvider
// Index >= 0` branch and spilling it across -- a hoist-or-not tie-break my compile resolves the
// other way, not a source difference). The dominant residual is the SAME intrinsic cols/rows
// register swap that parks the sibling ReplyWithStoredLayout (0x43d520): /O2 assigns the two
// `short` cols/rows loads to the ax/cx pair swapped vs. the original (original cols->cx dies
// early, rows->ax lives to the wRows store; mine cols->ax, rows->cx), which cascades into the
// pSelected-reload register (ecx vs eax), the pixel-count shuffle, the constant `bReliable=1`
// store scheduling, and the first slot-field read's addressing mode (anchored `[edi+0x44]` vs
// index-math `[edi+edx*4+0x55c]`). All levers exhausted on Reply (multiply operand order,
// named-local vs inline reads, read order); each only relocates the swap. Send is the FIRST
// LayoutNet function in the .obj so its incoming allocator state (Yoda #7) has no in-source
// predecessor to reproduce. Intrinsic register-swap class (Yoda #29/#30). See docs/PARKED.md.
void DPlaySessionMgr::LayoutNet_SendCurrentLayoutBitmap(int destPlayerId) {
    // GetSelectedProvider() inlined: the slot is only valid while connected.
    int mode = connectionMode;
    DPlaySessionMgrProviderSlot *pSelected = (mode != 2) ? 0 : pSelectedProvider;
    if (mode != 2)
        return;
    pSelected->bDirty = 1;

    LocoBitmap *pBitmap = new LocoBitmap();
    g_worldBoard.CaptureBoardToBitmap(pBitmap, 0);
    if (pBitmap != 0) {
        LayoutBitmapWireMsg *pMsg =
            (LayoutBitmapWireMsg *)::operator new(pBitmap->width * pBitmap->height + 0x28);
        pMsg->wOpcode = 0x3f9;
        short cols = (short)pBitmap->width;
        pMsg->wCols = cols;
        short rows = (short)pBitmap->height;
        int nPixelCount = cols * rows;
        pMsg->wRows = rows;
        pMsg->nPixelCount = nPixelCount;

        int nextSeq = pSelected->dwLayoutVersion + 1;
        void *pPixels = pBitmap->pPixels;
        if (selectedProviderIndex >= 0) {
            DPlaySessionMgrProviderSlot *pSlot = &aProviderSlots[selectedProviderIndex];
            if (pSlot->pLayoutData != 0)
                ::operator delete(pSlot->pLayoutData);
            pSlot->pLayoutData = ::operator new(nPixelCount);
            pSlot->nLayoutDataSize = nPixelCount;
            memcpy(pSlot->pLayoutData, pPixels, nPixelCount);  // idiom-exempt: runtime length
            pSlot->dwLayoutVersion = nextSeq;
            pSlot->dwLayoutDims = pMsg->dwColsRows;
        }
        pMsg->dwLayoutVersion = pSelected->dwLayoutVersion;
        memcpy(pMsg->data, pBitmap->pPixels, pMsg->nPixelCount);  // idiom-exempt: runtime length

        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 6;
        pNode->pPayload = pMsg;
        pNode->destPlayerId = destPlayerId;
        pNode->payloadLen = pMsg->nPixelCount + 0x19;
        pNode->bReliable = 1;
        g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
        delete pBitmap;
    }
}

// FUNCTION: LOCO 0x43d520
// Reply to a peer's layout request (destPlayerId): if a session is live and the selected
// provider slot has a stored layout blob, pack it into an opcode-0x3f9 bitmap message and send
// it reliably to that peer; if the slot has no stored layout, post a bare type-0x1b "no layout"
// command to the local queue instead. No-op when not connected (connectionMode != 2).
//
// EFFECTIVE MATCH (DIFF ~42B): structure is byte-aligned through the alloc + opcode store, but
// /O2 assigns the two `short` cols/rows loads to the ax/cx pair swapped vs. the original
// (original cols->cx, rows->ax with the wCols*wRows product landing in edx; ours lands it in
// ecx), and that single symmetric-swap choice cascades -- forcing an extra pixel-count
// register shuffle and rescheduling the constant `bReliable=1` store earlier. Confirmed
// unsteerable from source: multiply operand order (rows*cols vs cols*rows), named-local vs
// inline field reads, and cols/rows read-order were all tried; each only relocates the swap.
// The real .obj predecessor (SendCurrentLayoutBitmap 0x43d350, not yet transcribed) is absent
// here, so incoming allocator state (Yoda #7) can't be reproduced either. Intrinsic
// register-swap class (Yoda #29/#30). See docs/PARKED.md.
void DPlaySessionMgr::LayoutNet_ReplyWithStoredLayout(int destPlayerId) {
    // GetSelectedProvider() inlined: the slot is only valid while connected.
    int mode = connectionMode;
    DPlaySessionMgrProviderSlot *pSlot = (mode != 2) ? 0 : pSelectedProvider;
    if (mode != 2)
        return;
    if (pSlot->pLayoutData != 0) {
        LayoutBitmapWireMsg *pMsg =
            (LayoutBitmapWireMsg *)::operator new(pSlot->nLayoutDataSize + 0x28);
        pMsg->wOpcode = 0x3f9;
        short cols = pSlot->wLayoutCols;
        pMsg->wCols = cols;
        short rows = pSlot->wLayoutRows;
        pMsg->wRows = rows;
        pMsg->nPixelCount = rows * cols;
        pMsg->dwLayoutVersion = pSlot->dwLayoutVersion;
        memcpy(pMsg->data, pSlot->pLayoutData, pMsg->nPixelCount);  // idiom-exempt: runtime length

        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 6;
        pNode->pPayload = pMsg;
        pNode->destPlayerId = destPlayerId;
        pNode->payloadLen = pMsg->nPixelCount + 0x19;
        pNode->bReliable = 1;
        g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
    } else {
        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 0x1b;
        pNode->payloadLen = 0;
        pNode->pPayload = 0;
        pNode->destPlayerId = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
    }
}

// FUNCTION: LOCO 0x43d6c0
// Load a provider slot's saved layout bitmap from disk into the slot's layout cache: free any
// blob already cached for the slot, build the path "<install>Layouts\<slotLongName>", decode it
// through a temporary ThumbnailBmp, and on success copy the decoded dimensions + pixel data back
// into the slot. slotLongName is empty for slots with no saved layout (skipped). The ThumbnailBmp
// is a class-typed stack local, so /GX wraps the body in automatic unwind protection (the SEH
// scaffolding) to run its dtor if ThumbnailBmp_Load throws.
void DPlaySessionMgr::LayoutSet_LoadSlotBitmap(int slotIndex) {
    // Direct array subscripting (not a hoisted `pSlot` pointer) so /O2 picks the original's own
    // per-field anchors -- notably the (slotIndex+0x12)*0x4c index folding for the +0x40 field,
    // whose absolute offset 0x558 == 0x12*0x4c. (Same lever as ResetProviders' per-slot loop.)
    ThumbnailBmp thumb;
    if (aProviderSlots[slotIndex].pLayoutData != 0) {
        ::operator delete(aProviderSlots[slotIndex].pLayoutData);
        aProviderSlots[slotIndex].pLayoutData = 0;
        aProviderSlots[slotIndex].nLayoutDataSize = 0;
        aProviderSlots[slotIndex].wLayoutCols = 0;
        aProviderSlots[slotIndex].wLayoutRows = 0;
        aProviderSlots[slotIndex].dwLayoutVersion = 0;
    }
    if (strlen(aProviderSlots[slotIndex].sLongName) != 0) {
        char szPath[1284];
        wsprintfA(szPath, "%sLayouts\\%s", g_pInstallPathPrefix,
                  aProviderSlots[slotIndex].sLongName);
        if (thumb.ThumbnailBmp_Load(szPath)) {
            aProviderSlots[slotIndex].wLayoutCols = thumb.wWidth;
            aProviderSlots[slotIndex].wLayoutRows = thumb.wHeight;
            aProviderSlots[slotIndex].nLayoutDataSize =
                thumb.wHeight * aProviderSlots[slotIndex].wLayoutCols;
            void *pBlob = ::operator new(aProviderSlots[slotIndex].nLayoutDataSize);
            aProviderSlots[slotIndex].pLayoutData = pBlob;
            memcpy(pBlob, thumb.pPixels,
                   aProviderSlots[slotIndex].nLayoutDataSize);  // idiom-exempt: runtime length
        }
    }
}

// FUNCTION: LOCO 0x43e010
// Consume the pending-train queue at the poll-rate gate: acts only when nDispatchTick is an
// exact multiple of stateTimeoutMs (otherwise a no-op that resets nothing). Pops the first
// queued node whose retry-cooldown byte (+0x89) is 0. In joined mode (connectionMode==2), if the
// node's owner has no provider slot, releases it outright (virtual scalar-deleting dtor).
// Otherwise attempts placement: on failure re-prepends the node to retry next gate; on success
// scans the train's car slots for one owned by the local player with a pending mail attachment
// and plays a one-shot arrival chime. Ghidra types the return uint, but the function is really
// void -- every exit is a bare early return, leaving whatever's in eax (the idiv quotient, the
// loop counter, or a callee's result) dead.
//
// EFFECTIVE MATCH (DIFF ~312B, but 150/151 insns structurally identical): two intrinsic /O2
// residuals, each of which my source can't steer (tried the full loop-shape lever set + the
// address-of/pointer-cache/(int)-cast levers):
//   (1) The find-first-with-clear-cooldown loop is PEELED: /O2 duplicates the cooldown load
//       (one peeled iter-0 copy at the top + a rotated loop copy at the bottom) where the
//       original keeps a single shared loop-top load. Reproduced identically by do-while+goto,
//       plain while, and for(;;)+break -- the rotation is a fixed /O2 choice for this mid-exit
//       loop here, net-neutral on insn count. It cascades: the node==0 exit routes straight to
//       the epilogue (the original jmps through the after-merge's redundant node re-test) and
//       the unlink block's ecx/edx/eax get a symmetric swap (Yoda #29/#30 family).
//   (2) The `aProviderSlots[owner].providerId == 0` check: the original materializes the slot
//       ADDRESS via `lea` guarded by a DEAD `test/jl -> null` signed-index guard (MSVC's
//       non-power-of-2-element `&member_array[i]` lowering) before dereferencing; my compile
//       proves the unsigned-char index non-negative and folds it to a single direct load. The
//       guard is genuinely dead (the index is 0-255) and the decompiler removes it as
//       unreachable -- not source-steerable from an unsigned-char field access.
void __fastcall GameNet_DrainPendingTrainQueue(DPlaySessionMgr *pMgr) {
    char szPath[0x504] = "";

    // pHead/pPrev are read up front (the original hoists the queue-head load above the idiv gate
    // and the pPrev=0 clear right after it).
    PeerTrainNodePartial *pHead = (PeerTrainNodePartial *)pMgr->pPendingTrainQueueHead;
    PeerTrainNodePartial *pPrev = 0;
    if (pMgr->nDispatchTick % pMgr->stateTimeoutMs == 0) {
        pMgr->nDispatchTick = 0;
        if (pHead != 0) {
            // Find the first node whose retry-cooldown byte is clear, then unlink it. The
            // cooldown==0 exit falls DIRECTLY into the unlink (via the loop condition going
            // false), while the node==0 exit skips it with a goto -- routing the unlink through a
            // re-test of pNode instead makes /O2 peel the first cooldown read (a duplicate load).
            PeerTrainNodePartial *pNode = pHead;
            do {
                if (pNode->bAckCounterA == 0)
                    goto foundNode;
                pPrev = pNode;
                pNode = (PeerTrainNodePartial *)pNode->pNext;
            } while (pNode != 0);
            goto afterUnlink;
        foundNode:
            if (pPrev == 0)
                pMgr->pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode->pNext;
            else
                pPrev->pNext = pNode->pNext;
            pNode->pNext = 0;
        afterUnlink:
            if (pNode != 0) {
                if (pMgr->connectionMode == 2 &&
                    pMgr->aProviderSlots[pNode->bOwnerByteA].providerId == 0) {
                    delete (GameNetQueuedNodeMaybe *)pNode;
                    return;
                }
                if (pMgr->AttemptQueuedTrainPlacement(pNode) == 0) {
                    pNode->pNext = pMgr->pPendingTrainQueueHead;
                    pMgr->pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode;
                    return;
                }
                int i = 1;
                TilePlacedObjPartial **ppSlot = &pNode->carSlots[1];
                for (; i <= pNode->wCarSlotCount; i++, ppSlot++) {
                    CarNetState *pState = CarNetObj_GetAppliedState(*ppSlot);
                    if (pState != 0 &&
                        strcmp(pState->nameA, g_pLocalPlayerIdentity->name) == 0 &&
                        pState->bAttachmentSoundPlayedMaybe == 0 && pState->wAttachmentId != 0) {
                        g_pPostBagCache->PostBag_BuildAttFilePath(pState->wAttachmentId, 5, szPath);
                        g_UIResources.Sound_PlayOneShotAtPosition(
                            szPath, g_worldBoard.dwHalfWidth,
                            g_worldBoard.dwHalfHeight, 4);
                        return;
                    }
                }
            }
        }
    }
}

// FUNCTION: LOCO 0x43e1d0
// Resolve where a popped pending-train node should be placed on the board and hand it to the
// slot registry. Only acts in in-game states 3/5/9. In connecting mode (connectionMode==1) there
// is no board yet, so it hands off a null coord/quadrant. Otherwise it maps the node's heading
// (0/0x5a/0xb4/0x10e) to one of the 4 board edges via NetSessionEventQueue's edge-placement
// quartet, tagging the corresponding quadrant (bottom=4/left=1/right=2/top=3), then claims a slot
// via PeerTrainSlotQueueMaybe::ClaimSlotForTrain. Returns 0 on failure (the drain re-prepends to retry).
//
// Key modeling that landed the structure (control flow, all 4 edge-helper calls with the ecx=
// singleton load, the shared slot-registry call, and the esi=dir/edi=pNode register allocation
// all match):
//   * The state gate is a `switch` (case 3/5/9 -> break, default -> return 0), NOT an `&&`
//     chain -- reproduces the original's `sub eax,3; je; sub eax,2; je; sub eax,4; je`
//     decrement-chain (Yoda #11 family).
//   * `dir` is `int` (not `unsigned short`): the 16-bit heading is zero-extended once at load
//     and used directly by the `switch`; a `short` dir re-masks with `and eax,0xffff` at the
//     dispatch.
//   * `dir` is REUSED as the quadrant tag (a separate `char quadrant` needs an extra callee-
//     saved register, pushing dir out of esi into ebx and de-syncing the whole allocation).
//   * The edge helpers return `Pair16` BY VALUE and go through an explicit `tmp` local
//     that is field-wise copied into `coord` (`coord.lo=tmp.lo; coord.hi=tmp.hi;`) -- the
//     explicit tmp stops /O2 from specializing the connecting-mode (const 0,0 coord) path into
//     a duplicated slot-registry call, and the field-wise copy reproduces the original's
//     tmp[esp+0x18] -> coord[esp+8] word-by-word materialization instead of register-promoting
//     the whole struct.
//
// EFFECTIVE MATCH (DIFF 134B / 79-of-86 insns structurally identical): the residuals are the
// intrinsic register-allocation / instruction-selection tie-break family (Yoda #29/#30):
//   (1) Per-edge-call scratch-register scheduling: the original leaves the compute-result read
//       in ecx/edx and interleaves `mov esi,<quadrant>` before the push + `mov ecx,<singleton>`
//       after it; this compile picks eax and the opposite interleave -- same instructions,
//       source-inert ordering.
//   (2) The connecting-mode branch pushes its own literal 0 quadrant + coord (`push eax; mov
//       eax,[..]; push eax`) then joins only at `push pNode`; this compile shares the quadrant/
//       coord pushes too (dir=0 there feeds the same `push esi`). This is COUPLED to the dir-as-
//       quadrant reuse above -- separating them to force the private push regresses the register
//       allocation for a net-worse result, so the private push is accepted.
//   (3) The final `if (place() == 0) return 0; return 1;` compiles to a branchless `setne al`
//       here vs. the original's `test; jne; mov al,1` branch; inverting the condition and
//       stashing the result in a local were both tried, neither reproduced the branch.
char DPlaySessionMgr::AttemptQueuedTrainPlacement(PeerTrainNodePartial *pNode) {
    int dir = pNode->wHeading;
    switch (g_nScreenState) {
    case 3:
    case 5:
    case 9:
        break;
    default:
        return 0;
    }

    // dir is reused as the board-quadrant tag (1/4/3/2) after dispatch.
    Pair16 coord;
    if (connectionMode == 1) {
        // Still connecting -- no board placement yet.
        coord.lo = 0;
        coord.hi = 0;
        dir = 0;
    } else {
        Pair16 tmp;
        switch (dir) {
        case 0x5a:  // 90deg -> left edge
            dir = 1;
            tmp = g_NetSessionEventQueueEdge.ComputeLeftEdgePlacement();
            break;
        case 0:     // 0deg -> bottom edge
            dir = 4;
            tmp = g_NetSessionEventQueueEdge.ComputeBottomEdgePlacement();
            break;
        case 0xb4:  // 180deg -> top edge
            dir = 3;
            tmp = g_NetSessionEventQueueEdge.ComputeTopEdgePlacement();
            break;
        case 0x10e: // 270deg -> right edge
            dir = 2;
            tmp = g_NetSessionEventQueueEdge.ComputeRightEdgePlacement();
            break;
        }
        coord.lo = tmp.lo;
        coord.hi = tmp.hi;
    }
    if (g_PeerTrainSlotQueueEdge.ClaimSlotForTrain(pNode, coord, (char)dir) == 0)
        return 0;
    return 1;
}

// FUNCTION: LOCO 0x43e2e0
// Case 0xf of GameNetManager_HandleQueuedEvent: a queued "peer train wants to connect" event.
// pMsg->pPayload (+8) holds the incoming PeerTrainNode. While still connecting (connectionMode == 1)
// the node is appended onto the pending-train queue: if the queue was empty it becomes the head and
// nDispatchTick is armed one poll-interval early; otherwise it is linked onto the tail. If we
// are no longer connecting, the node is released outright via its virtual scalar-deleting dtor.
void DPlaySessionMgr::HandleQueuedTrainConnect(NetMsgQueueNode *pMsg) {
    if (connectionMode != 1) {
        PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pMsg->pPayload;
        pMsg->pPayload = 0;
        if (pTrain != 0)
            delete (GameNetQueuedNodeMaybe *)pTrain;
        return;
    }
    if (pPendingTrainQueueHead == 0) {
        pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pMsg->pPayload;
        ((PeerTrainNodePartial *)pPendingTrainQueueHead)->pNext = 0;
        ((PeerTrainNodePartial *)pPendingTrainQueueHead)->bHasDetailFlagMaybe = 0;
        nDispatchTick = stateTimeoutMs - 0x20;
        return;
    }
    GameNetQueuedNodeMaybe *pTail = pPendingTrainQueueHead;
    while (pTail->pNext != 0)
        pTail = pTail->pNext;
    PeerTrainNodePartial *pNew = (PeerTrainNodePartial *)pMsg->pPayload;
    pTail->pNext = (GameNetQueuedNodeMaybe *)pNew;
    pNew->pNext = 0;
    pNew->bHasDetailFlagMaybe = 0;
}

// FUNCTION: LOCO 0x43e370
// Case 0x11 of GameNetManager_HandleQueuedEvent: a queued "place this train" event.
// param is a NetMsgQueueNode whose pPayload (+8) holds the PeerTrainNode. When NOT hosting
// (connectionMode != 2) the placement is meaningless here, so it just detaches and deletes the
// payload node. When hosting: append the node to the pending-train queue (tail of the +0x70
// chain), resolve its board socket coord from the heading (0/0x5a/0xb4/0x10e -> one of the 4
// edge helpers, then nudge x or y by +1), file the result via SetTrainPlacementResult, and
// finally reconcile each car's hand-off socket state: if ANY car already has a live CarNetState
// (CarNetObj_GetAppliedState), flip every 0x1870 car to 0x1871; otherwise flip every 0x1871 to
// 0x1870 (each flip = a vtbl+0x3c(newKind,-1) then vtbl+0x20(carField54,1) pair).
//
// Modeling that landed the structure (the whole prologue through the SetTrainPlacementResult
// call -- enqueue, non-host delete, heading switch, all 4 edge-helper calls, the coord +1 nudge
// -- byte-matches; residuals are all in the 3 car loops):
//   * The edge-helper coord is a whole-struct `coord = tmp; coord.lo/hi += 1;` (a dword copy plus
//     a memory `inc word`), NOT the sibling AttemptQueued's field-wise copy -- here the coord is
//     later re-read (movsx) so /O2 keeps it in memory and the dword copy matches.
//   * wTrainId is passed as a 32-bit arg (`unsigned int` param): the original zero-extends it
//     (`xor eax,eax; mov ax; push eax`), proving a 32-bit callee param, not a 16-bit one.
//   * The car scan is a `for`-with-break (matches /O2's call-at-top rotation); all three car
//     loops are subscripted `pNode->carSlots[i]` walks (the #53 idiom -- see the EXACT note
//     below; the earlier hand-rolled `ppCar++` forms were the v240 park's whole residual).
//     Distinct counters i/j/k (VC5 for-scope leak) -- and the
//     retag counter being SEPARATE from the scan counter is what lets /O2 reuse ebx (the bFound
//     register) as the retag counter, keeping bFound out of a stack slot.
//   * `if (!bFound) { 1871 loop; return; } 1870 loop;` (not-found first, explicit early return):
//     matches the original's block layout (the not-found/1871 loop is the fall-through with its
//     own epilogue; the found/1870 loop is the jump target sharing the tail).
//
// EXACT (v501). Was EFFECTIVE (DIFF 200B), the residuals read as the intrinsic
// symmetric-register-swap / scheduling family (Yoda #29/#30) confined to the 3 car loops and
// parked as unsteerable at v240. The #53 subscript-induction sweep cracked it: writing all three
// car walks as subscripted `pNode->carSlots[i]` loops -- no hand-rolled `ppCar++` element pointer
// at all -- gives the allocator the original's register roles everywhere (the ebp/esi swap, the
// wTrainId zero-extend register, the hoisted `lea`, the count-reload register all resolve at
// once). The v240-era shape levers below are still load-bearing: distinct i/j/k counters (the
// retag counter being SEPARATE from the scan counter lets /O2 reuse ebx=bFound as the retag
// counter, keeping bFound out of a stack slot); `if(!bFound){1871;return;} 1870;` (not-found
// first + early return matches the block layout); wTrainId a 32-bit (`unsigned int`) arg; coord
// a whole-struct `coord=tmp; coord.lo/hi+=1;` copy.
void DPlaySessionMgr::HandleQueuedTrainPlacement(NetMsgQueueNode *pMsg) {
    if (connectionMode != 2) {
        GameNetQueuedNodeMaybe *pNode = (GameNetQueuedNodeMaybe *)pMsg->pPayload;
        pMsg->pPayload = 0;
        if (pNode != 0)
            delete pNode;
        return;
    }

    // Host: append the payload node to the tail of the pending-train queue. pMsg->pPayload is
    // re-read fresh at each use (the enqueue store aliases the queue chain).
    if (pPendingTrainQueueHead == 0) {
        pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pMsg->pPayload;
    } else {
        GameNetQueuedNodeMaybe *pTail = pPendingTrainQueueHead;
        while (pTail->pNext != 0)
            pTail = pTail->pNext;
        pTail->pNext = (GameNetQueuedNodeMaybe *)pMsg->pPayload;
    }
    PeerTrainNodePartial *pNode = (PeerTrainNodePartial *)pMsg->pPayload;

    int dir = pNode->wHeading;
    pNode->bHasDetailFlagMaybe = 0;

    Pair16 coord;
    Pair16 tmp;
    switch (dir) {
    case 0x5a:  // 90deg -> left edge
        tmp = g_NetSessionEventQueueEdge.ComputeLeftEdgePlacement();
        coord = tmp;
        coord.hi += 1;
        break;
    case 0:     // 0deg -> bottom edge
        tmp = g_NetSessionEventQueueEdge.ComputeBottomEdgePlacement();
        coord = tmp;
        coord.lo += 1;
        break;
    case 0xb4:  // 180deg -> top edge
        tmp = g_NetSessionEventQueueEdge.ComputeTopEdgePlacement();
        coord = tmp;
        coord.lo += 1;
        break;
    case 0x10e: // 270deg -> right edge
        tmp = g_NetSessionEventQueueEdge.ComputeRightEdgePlacement();
        coord = tmp;
        coord.hi += 1;
        break;
    }
    SetTrainPlacementResult(pNode->wTrainId, pNode->bOwnerByteA,
        pNode->bOwnerByteB, coord.lo, coord.hi);

    // Does any car already have a live CarNetState?
    bool bFound = false;
    for (int i = 1; i <= (int)pNode->wCarSlotCount; i++) {
        if (CarNetObj_GetAppliedState((CarNetObj *)pNode->carSlots[i]) != 0) {
            bFound = true;
            break;
        }
    }

    if (!bFound) {
        for (int k = 1; k <= (int)pNode->wCarSlotCount; k++) {
            if (CarNetObj_GetCarTypeId((CarNetObj *)pNode->carSlots[k]) == 0x1871) {
                int arg = ((CarNetObj *)pNode->carSlots[k])->nAnimValueCache;
                ((CarNetObj *)pNode->carSlots[k])->SetCarTypeAndCategory(0x1870, -1);
                ((CarNetObj *)pNode->carSlots[k])->SetStateArgMaybe(arg, 1);
            }
        }
        return;
    }
    for (int j = 1; j <= (int)pNode->wCarSlotCount; j++) {
        if (CarNetObj_GetCarTypeId((CarNetObj *)pNode->carSlots[j]) == 0x1870) {
            int arg = ((CarNetObj *)pNode->carSlots[j])->nAnimValueCache;
            ((CarNetObj *)pNode->carSlots[j])->SetCarTypeAndCategory(0x1871, -1);
            ((CarNetObj *)pNode->carSlots[j])->SetStateArgMaybe(arg, 1);
        }
    }
}

// FUNCTION: LOCO 0x4408b0
// Reconcile a train node's car composition when it changes hands. Three passes over the node's
// occupied car slots (1..wCarSlotCount): (1) for every car that has a live CarNetState whose
// stored name equals the local player's, save that card into the PostBag (category 1) and clear the
// car's net-state -- a locally-owned car is "banked" as it leaves; (2) determine whether ANY car
// still has a live CarNetState; (3) re-tag every car's hand-off socket state -- if some car still
// has detail, flip each 0x1870 car to 0x1871, otherwise flip each 0x1871 back to 0x1870. Finally, if
// any card was banked, push the resulting UI-mode change (mode 3 when currently 1, mode 2 when 0).
//
// EFFECTIVE MATCH (DIFF(331) at 413 bytes): structure is fully faithful -- the inlined strcmp,
// all three passes' filter/retag bodies, and the final UI-mode block all byte-align. Sole residual
// is pass 2 (the find-first-with-break "does any car still have detail" scan): /O2 peels its first
// iteration's pointer load in my compile (a `mov ecx,[node+0x14]` + `lea edi` + `ja exit` shell
// around the loop) while the original keeps a clean `lea edi; do{ mov ecx,[edi]; call; jne found }
// while(jbe)`. The v240 `for`-with-break rotation fix does apply to the loop in isolation, but here
// -- as the 2nd of 4 car-slot loops sharing pNode in ebp -- hoisting its own element pointer spills
// pNode to the stack and desyncs the whole register allocation (reg_pen 1->28, far worse), so the
// `if + do-while` form is kept. Two-exit mid-exit peel class (Yoda #15; cf. drain 0x43e010). See
// docs/PARKED.md.
//
// v501 #53-sweep probe matrix (passes 1/2/3 subscripted vs hand-rolled `ppCar++` walks, every
// split measured): ALL-subscript DIFF(331) len=413 (checked in -- the best); all-pointer-walk
// DIFF(338) len=423 (the old baseline); pass-2-only-subscript DIFF(379) (worst -- subscripting
// the peeled scan alone makes the peel costlier); passes 1+3 subscripted with pass 2 walked
// DIFF(338), byte-identical to baseline (passes 1 and 3 are #53-neutral). The residual stays the
// pass-2 peel class; #53 buys 7 points here but does not close it.
void DPlaySessionMgr::ReconcileCarHandoff(PeerTrainNodePartial *pNode) {
    bool bBankedAny = false;
    for (unsigned int i = 1; i <= pNode->wCarSlotCount; i++) {
        if (CarNetObj_GetAppliedState((CarNetObj *)pNode->carSlots[i]) != 0 &&
            strcmp(CarNetObj_GetAppliedState((CarNetObj *)pNode->carSlots[i])->nameA,
                   g_pLocalPlayerIdentity->name) == 0) {
            g_pPostBagCache->PostBag_SaveCardToCategory(
                CarNetObj_GetAppliedState((CarNetObj *)pNode->carSlots[i]), 1, 0);
            bBankedAny = true;
            ((CarNetObj *)pNode->carSlots[i])->CarNetObj_ApplyNetState(NULL);
        }
    }

    bool bAnyDetail = false;
    for (unsigned int j = 1; j <= pNode->wCarSlotCount; j++) {
        if (CarNetObj_GetAppliedState((CarNetObj *)pNode->carSlots[j]) != 0) {
            bAnyDetail = true;
            break;
        }
    }

    if (bAnyDetail) {
        for (unsigned int k = 1; k <= pNode->wCarSlotCount; k++) {
            if (CarNetObj_GetCarTypeId((CarNetObj *)pNode->carSlots[k]) == 0x1870) {
                int arg = ((CarNetObj *)pNode->carSlots[k])->nAnimValueCache;
                ((CarNetObj *)pNode->carSlots[k])->SetCarTypeAndCategory(0x1871, -1);
                ((CarNetObj *)pNode->carSlots[k])->SetStateArgMaybe(arg, 1);
            }
        }
    } else {
        for (unsigned int k = 1; k <= pNode->wCarSlotCount; k++) {
            if (CarNetObj_GetCarTypeId((CarNetObj *)pNode->carSlots[k]) == 0x1871) {
                int arg = ((CarNetObj *)pNode->carSlots[k])->nAnimValueCache;
                ((CarNetObj *)pNode->carSlots[k])->SetCarTypeAndCategory(0x1870, -1);
                ((CarNetObj *)pNode->carSlots[k])->SetStateArgMaybe(arg, 1);
            }
        }
    }

    if (bBankedAny) {
        if (field_0x800 == 1) {
            SetUiModeAndNotifyWidgets(3);
            return;
        }
        if (field_0x800 == 0)
            SetUiModeAndNotifyWidgets(2);
    }
}

// FUNCTION: LOCO 0x43e560
// Gate for a per-train move/release. First frees the node's held car slots (PeerTrainSlotQueue::
// FreeQueuedTrainCarSlots) and reconciles its category/state (ReconcileCarHandoff). nDiscardFlag==1 releases pNode
// outright (delete). Otherwise rebuild the car composition (RebuildOrEnqueueTrainCars); a
// rebuild that returns true while still connecting (mode 1) defers via a queue prepend. With
// multiplayer disabled (g_pNetSettings->Unk0x10==0) it also just defers. Otherwise it
// dispatches on connectionMode: mode 1 posts a connect/join and defers on failure; mode 2 checks
// the owner's provider slot -- empty slot releases pNode, occupied slot forwards the move and
// defers on failure; any other mode just defers. Every path returns 1.
//
// Block-layout levers that landed the structure (280342 -> 44012 asmscore): the nDiscardFlag==1
// release-delete lives at the END (a shared block the whole body's `if (nDiscardFlag != 1) {...}`
// falls past), NOT inline after the check -- inlining it early made the compiler tail-merge the 5
// enqueue blocks into one shared block (wrong; the original keeps all 5 inline with per-site
// registers). The joined-mode EMPTY-slot delete, by contrast, IS inline in the switch case (its
// own epilogue), so only the nDiscardFlag release reaches the end block.
//
// EXACT (v360). The last residual had been the "dead signed-index guard" on
// `&aProviderSlots[pNode->bOwnerByteA]`: the original materializes the slot ADDRESS behind a
// `jl (pSlot=0)` guard (non-power-of-2 0x4c stride) then does `cmp [pSlot],0`, whereas a direct
// unsigned-char subscript folds the guard away and loads providerId in one go. That guard is not
// intrinsic and never was -- it is the body of the implicitly inline
// DPlaySessionMgr::ProviderSlotAt accessor (see DPlaySessionMgr.h). Calling it here reproduced
// the guard exactly and the function went byte-exact.
char DPlaySessionMgr::RequestTrainMoveOrReleaseNode(int fromSlot, int toSlot,
                                                         PeerTrainNodePartial *pNode) {
    g_PeerTrainSlotQueueEdge.FreeQueuedTrainCarSlots(pNode);
    ReconcileCarHandoff(pNode);
    if (pNode->nDiscardFlag != 1) {
        if (RebuildOrEnqueueTrainCars(pNode) && connectionMode == 1) {
            pNode->pNext = pPendingTrainQueueHead;
            pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode;
            return 1;
        }
        if (g_pNetSettings->pDetectedProviderList == 0) {
            pNode->pNext = pPendingTrainQueueHead;
            pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode;
            return 1;
        }
        switch (connectionMode) {
        case 1:  // connecting
            if (!GameNet_PostConnectOrJoinForNode(pNode)) {
                pNode->pNext = pPendingTrainQueueHead;
                pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode;
            }
            return 1;
        case 2: {  // joined
            DPlaySessionMgrProviderSlot *pSlot = ProviderSlotAt(pNode->bOwnerByteA);
            if (pSlot->providerId != 0) {
                if (!TrainNet_PostMoveRequestForNode(fromSlot, toSlot, pNode)) {
                    pNode->pNext = pPendingTrainQueueHead;
                    pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode;
                }
            } else {
                delete (GameNetQueuedNodeMaybe *)pNode;  // empty owner slot: release
            }
            return 1;
        }
        default:  // any other mode: defer
            pNode->pNext = pPendingTrainQueueHead;
            pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode;
            return 1;
        }
    }
    // nDiscardFlag == 1, or a joined-but-empty owner slot: release outright
    delete (GameNetQueuedNodeMaybe *)pNode;
    return 1;
}

// FUNCTION: LOCO 0x43e690
// Re-derive pNode's car composition and, if it changed, spawn a replacement train that re-enters
// the placement queue. Scans each occupied car slot (1..N): a slot whose live CarNetState has a
// blank name is discarded (bDiscard); a valid slot is cleared (ApplyNetState 0) and reloaded via
// the PostBag Easter-card loader (LoadOrCreateEasterCard), marking bRebuilt. If ANY slot was
// rebuilt, allocates a fresh PeerTrainNode (a random car-kind id, 0x94 bytes), stamps it with the
// next train id + "LEGO LOCO" name, copies each rebuilt car's state (local player name + a new
// 0x1871 car slot) into it, then prepends the new node onto pPendingTrainQueueHead. Returns 1
// only if no slot was discarded AND still connecting (mode 1), else 0. The `new PeerTrainNode`
// carries the /GX automatic alloc-protection (the SEH prologue scaffolding), not a user try/catch.
//
// Levers that landed the structure (asmscore 269930 -> 206544, insns 183/185): the scan loop's
// discard branch is reload-first (`if (name[0x14] != 0) {reload} else {discard}`); the scan uses a
// pOut POINTER but subscripts pNode->carSlots[i] (a pure incrementing-pointer form let /O2 merge
// the two inductions into a base+index scheme the original doesn't use); the new-node's +0x74/+0x76
// heading is two word fields (zeroed as two word stores).
//
// EFFECTIVE MATCH (asmscore byte_diff 154; cc.sh DIFF 501 is inflated by the scan-loop shuffle
// misaligning the tail). Residuals are all the intrinsic register-allocation family: (1) the scan
// counter `i` spills to a stack slot vs the original keeping it in edi -- the original reloads pNode
// from its parameter stack home each iteration (freeing edi), mine keeps pNode in a register and
// spills i instead; not steerable by local-decl order or the loop shape (pointer / subscript / mixed
// all tried, mixed is best). (2) The copy loop's `delete pState` null-check is elided by /O2 (mine
// proves pState != 0 from the outer guard; the original keeps the redundant `if (p != 0)` test -- an
// explicit inner `if` had no effect, same intrinsic elision as the LoadIndexedFile family). (3)
// Scattered esi/eax/ecx symmetric register swaps in the trainId + strcpy setup.
char DPlaySessionMgr::RebuildOrEnqueueTrainCars(PeerTrainNodePartial *pNode) {
    int cars[3];
    bool bRebuilt = false;
    bool bDiscard = false;
    cars[0] = 0;
    cars[1] = 0;
    cars[2] = 0;

    if (pNode->wCarSlotCount != 0) {
        int i = 1;
        int *pOut = cars;
        do {
            CarNetObj *pCar = (CarNetObj *)pNode->carSlots[i];
            CarNetState *pState = CarNetObj_GetAppliedState(pCar);
            *pOut = (int)pState;
            if (pState != 0) {
                if (pState->nameA[0x14] != 0) {
                    pCar->CarNetObj_ApplyNetState(NULL);
                    pState = LoadOrCreateEasterCard(pState);
                    *pOut = (int)pState;
                    if (pState != 0)
                        bRebuilt = true;
                } else {
                    bDiscard = true;
                    *pOut = 0;
                }
            }
            i++;
            pOut++;
        } while (i <= (int)pNode->wCarSlotCount);
    }

    if (bRebuilt) {
        PeerTrainNodePartial *pNew =
            new PeerTrainNodePartial(((int)rand() % 3) * 2 + 0x1804, 1, 1, 0);
        int trainId = g_pDPlaySessionMgr->nNextTrainId + 1;
        g_pDPlaySessionMgr->nNextTrainId = trainId;
        pNew->wTrainId = (unsigned short)trainId;
        ((CarNetObjVtblProbe *)pNew->carSlots[0])->SetNameImpl("LEGO LOCO");
        pNew->PeerTrainNode_UpdateSelectedCar(pNode->wSelectedCarId);
        pNew->wHeading = 0;
        pNew->wLocalHeading = 0;
        pNew->wPosX = 0;
        pNew->wPosY = 0;
        pNew->bStallStepCounter = 0;
        pNew->wCheckpointPosX = 0;
        pNew->wCheckpointPosY = 0;

        int *pCar = cars;
        CarNetObj **ppNewCar = (CarNetObj **)&pNew->carSlots[0];
        int n = 3;
        do {
            CarNetState *pState = (CarNetState *)*pCar;
            if (pState != 0) {
                strcpy(pState->nameA, g_pLocalPlayerIdentity->name);
                pState->wAttachmentId = 0;
                pNew->PeerTrainNode_AllocCarSlot(0x1871, 4, 1);
                ppNewCar++;
                (*ppNewCar)->CarNetObj_ApplyNetState(pState);
                delete pState;
                *pCar = 0;
            }
            pCar++;
            n--;
        } while (n != 0);

        pNew->dwModeAMaybe = 2;
        pNew->SetSoundStateMaybe(0);
        pNew->pNext = pPendingTrainQueueHead;
        pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNew;
    }
    ReconcileCarHandoff(pNode);
    if (bDiscard || connectionMode != 1)
        return 0;
    return 1;
}

// FUNCTION: LOCO 0x43f0c0
// Drain the inbound GameNet message queue once per game tick: pop each queued node under the
// queue lock, dispatch it through GameNetManager_HandleQueuedEvent, then free it. After the
// queue is empty, run the pending-train placement drain (if any) and broadcast the roster tick.
void __fastcall GameNet_DrainEventQueue(DPlaySessionMgr *pMgr) {
    NetMsgQueueNode *pNode = 0;
    pMgr->nDispatchTick = pMgr->nDispatchTick + 1;
    while (g_pNetMsgLocalQueueHead != 0) {
        g_pGameNetMsgQueueLock->Lock();
        if (g_pNetMsgLocalQueueHead != 0) {
            pNode = g_pNetMsgLocalQueueHead;
            g_pNetMsgLocalQueueHead = pNode->pNext;
        }
        g_pGameNetMsgQueueLock->Unlock();
        if (pNode != 0) {
            pMgr->GameNetManager_HandleQueuedEvent(&pNode->type);
            operator delete(pNode);
        }
    }
    if (pMgr->pPendingTrainQueueHead != 0) {
        GameNet_DrainPendingTrainQueue(pMgr);
    }
    GameNet_BroadcastRosterTick(pMgr);
}

// FUNCTION: LOCO 0x440820
// Drive a UI-mode change into the game-window widget list: for every placed widget whose kind
// descriptor id is one of the three mailbox/postbox kinds (0xc5c/0xc5e/0xc60), invoke its
// vtbl+0x1c "set UI mode" method with the new mode (1/2/3, else 0). No-op if unchanged.
void DPlaySessionMgr::SetUiModeAndNotifyWidgets(int mode) {
    if (field_0x800 != mode) {
        unsigned int i = 0;
        while (i < g_gameWindowWidgetList.nItemCount) {
            AnimDescRefObj0x477488 *pItem =
                ((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)->GetItemImpl(i);
            int kind = pItem->pKindDesc->resourceId;
            if (kind == 0xc5c || kind == 0xc5e || kind == 0xc60) {
                switch (mode) {
                case 1:
                    ((GameWindowWidgetItemProbe *)pItem)->SetUiModeImpl(1);
                    break;
                case 2:
                    ((GameWindowWidgetItemProbe *)pItem)->SetUiModeImpl(2);
                    break;
                case 3:
                    ((GameWindowWidgetItemProbe *)pItem)->SetUiModeImpl(3);
                    break;
                default:
                    ((GameWindowWidgetItemProbe *)pItem)->SetUiModeImpl(0);
                    break;
                }
            }
            i++;
        }
        field_0x800 = mode;
    }
}

// Included here (end of file) rather than at the top so the added declarations don't shift/rotate
// this already-heavily-matched TU's earlier functions -- only the dispatcher below needs them.
#include "AppWindow.h"        // g_pApp (AppWindow::AbortMultiplayerSession this-load) + hwndOwner
#include "SplashWnd.h"       // g_pSplashWnd (the front-end window that owns the two setup pages)
#include "ApplSetupWnd.h"   // ApplSetupWnd / ProviderListNode (the application-setup page)
#include "GameNet.h"        // RosterSnapshotWireMsg (GameNet_BroadcastRosterSnapshot's 0x3f1 packet)

// Leaf handler for event type 0xc, defined at end of file (declared here so the dispatcher can
// call it without shifting the earlier matched functions). Free GameNet fastcall like its siblings.
void __fastcall GameNet_RemoveSourcePeerAndReconcile(DPlaySessionMgr *pMgr);  // 0x43f880
// Roster pack + unreliable broadcast (0x440070); not transcribed yet, declared-only.
void __fastcall GameNet_BroadcastRosterSnapshot(DPlaySessionMgr *pMgr);  // 0x440070

// FUNCTION: LOCO 0x43f2b0
// EFFECTIVE MATCH (DIFF ~250, all 156 case bodies structurally present). Two documented-intrinsic
// residuals remain, both confirmed unsteerable by cheap probes this session:
//   (1) a symmetric `this` register swap -- the original keeps `this` in ebx and the case-2
//       zero-constant in ebp; this compile flips them (this=ebp, zero=ebx). Yoda #29/#30: source
//       can't steer which of two equivalent callee-saved assignments the allocator picks. Pervades
//       ~40 instructions as ebp-vs-ebx ModRM byte diffs (identical length, no structural change).
//   (2) case 0x18's mid-exit find-first scan: /O2 rotates the loop and tail-merges its null-exit
//       return into the default epilogue (reordering the found-block + cases 0x1a/0x1c after it),
//       whereas the original keeps a dedicated inline null-exit epilogue. for/while/do-while forms
//       all compile byte-identical -- same intrinsic class as GameNet_DrainPendingTrainQueue.
// Steerable levers that DID land: case-5 connectionMode as a switch (subtract-chain + far ==0 block),
// and NOT caching case-0x16's slot index in a local (re-read so it stays scratch, freeing a
// callee-saved reg -- caching it forced the frame 4 bytes wider and worsened the swap cascade).
// Dispatch one drained inbound GameNet event by its opcode `type` (an ~18-case jump-table switch).
// The source case order matches the .text body layout recovered from the raw jump table
// (9/4/2/0xb/0xc/5/3/0xf/0x11/{0x12,0x15,0x17}/0x13/0x14/0x16/0x1b/0x18/0x1a/0x1c) so the case
// bodies land in the right memory order (jump-table switch layout follows source declaration order).
// Handlers cover: connect/teardown lifecycle (2/3/4/5/9/0xb/0xc), train connect/placement
// (0xf/0x11/0x12/0x15/0x17), provider-slot enable/disable + received-layout-bitmap apply
// (0x13/0x14/0x16), roster broadcast + pending-ack decrement + UI mode (0x1b/0x18/0x1a/0x1c).
void DPlaySessionMgr::GameNetManager_HandleQueuedEvent(int *pMsg) {
    NetMsgQueueNode *pNode = (NetMsgQueueNode *)pMsg;
    switch (pNode->type) {
    case 9:
        if (g_pNetSettings->bUseSecondaryRememberedChoice == 0)
            ApplyProviderSnapshot(pNode);
        operator delete(pNode->pPayload);
        return;
    case 4: {
        if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
            Pair16 owners;
            owners.lo = pNode->bEventOwnerA;
            owners.hi = pNode->bEventOwnerB;
            AssignProviderToSlot(pNode->destPlayerId, pNode->eventTrainId,
                                      (unsigned char *)pNode->pPayload, owners);
        }
        operator delete(pNode->pPayload);
        goto LAB_0043f3c7;
    }
    case 2: {
        if (connectionMode == 0) {
            g_pSplashWnd->pApplSetupWnd->SetProviderListMaybe((ProviderListNode *)pNode->pPayload);
            return;
        }
        NetMsgType2PayloadNode *p = (NetMsgType2PayloadNode *)pNode->pPayload;
        while (p != 0) {
            NetMsgType2PayloadNode *cur = p;
            p = p->pNext;
            if (cur->pSubPayload != 0) {
                operator delete(cur->pSubPayload);
                cur->pSubPayload = 0;
            }
            operator delete(cur);
        }
        pNode->pPayload = 0;
        return;
    }
    case 0xb:
        RemovePeerTrainsAndSlot(pNode->destPlayerId);
    LAB_0043f3c7:
        if (IsWindowVisible(g_pSplashWnd->pApplSetupWnd->hwndSelf)) {
            g_pSplashWnd->pApplSetupWnd->DrawAvatarGrid();
            g_pSplashWnd->pApplSetupWnd->CommitScreenUpdate(
                g_pSplashWnd->pApplSetupWnd->hwndSelf, 0, 0, 0);
            return;
        }
        break;
    case 0xc:
        GameNet_RemoveSourcePeerAndReconcile(this);
        return;
    case 5: {
        GameNet_ResetProvidersAndPostTeardown(this);
        queueSourceId = 0;
        searchProviderId = 0;
        switch (connectionMode) {
        case 0:
            if (g_pSplashWnd->pApplSetupWnd->field_0xe8 != 0) {
                g_pSplashWnd->pApplSetupWnd->AdvanceToNextProviderMaybe();
                return;
            }
            break;
        case 2: {
            MessageBeep(0x30);
            char szMsg[256];
            g_UIResources.LoadLocaleString(0x7e, szMsg, 0x100);
            MessageBoxA(g_pApp->hwndOwner, szMsg, "LEGO LOCO", 0);
            g_pApp->AbortMultiplayerSession();
            return;
        }
        }
        break;
    }
    case 3: {
        bConnectPending = true;
        queueSourceId = g_pGameNetThreadState->dpidCurrentPlayer;
        int destPlayerId = pNode->destPlayerId;
        searchProviderId = destPlayerId;
        if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
            aProviderSlots[0].providerId = destPlayerId;
            strcpy(aProviderSlots[0].sAddressOrName, g_pLocalPlayerIdentity->name);
        }
        if (connectionMode == 0 && g_pSplashWnd->pApplSetupWnd->field_0xe8 != 0) {
            g_pSplashWnd->pApplSetupWnd->NotifyProviderSelectedMaybe();
            return;
        }
        break;
    }
    case 0xf:
        HandleQueuedTrainConnect(pNode);
        return;
    case 0x11:
        HandleQueuedTrainPlacement(pNode);
        return;
    case 0x12:
    case 0x15:
    case 0x17:
        HandleQueuedPlacementEvent(pNode);
        return;
    case 0x13:
        if (pNode->nProviderSlotIndex >= 0) {
            aProviderSlots[pNode->nProviderSlotIndex].bEnabled = true;
            return;
        }
        break;
    case 0x14:
        if (pNode->nProviderSlotIndex >= 0) {
            aProviderSlots[pNode->nProviderSlotIndex].bEnabled = false;
            return;
        }
        break;
    case 0x16: {
        LayoutBitmapWireMsg *pWire = (LayoutBitmapWireMsg *)pNode->pPayload;
        unsigned int u2 = pWire->dwLayoutVersion;
        unsigned int colsRows = pWire->dwColsRows;
        unsigned int size = pWire->nPixelCount;
        // The slot index is re-read (not cached in a local) at each of the two guards below so it
        // stays in a scratch register -- caching it forces it into a callee-saved register, which
        // would steal `this`'s register and cascade into extra stack traffic. Within the first
        // guard the slot POINTER is hoisted (matching the original's cached &aProviderSlots[idx]).
        if (pNode->nProviderSlotIndex >= 0) {
            DPlaySessionMgrProviderSlot *pSlot = &aProviderSlots[pNode->nProviderSlotIndex];
            if (pSlot->pLayoutData != 0)
                operator delete(pSlot->pLayoutData);
            pSlot->nLayoutDataSize = size;
            pSlot->pLayoutData = operator new(size);
            memcpy(pSlot->pLayoutData, pWire->data, size);  // idiom-exempt: runtime length
            pSlot->dwLayoutDims = colsRows;
            pSlot->dwLayoutVersion = u2;
        }
        if (pNode->nProviderSlotIndex >= 0)
            aProviderSlots[pNode->nProviderSlotIndex].bDirty = true;
        HeapFree(GetProcessHeap(), 0, pNode->pPayload);
        return;
    }
    case 0x1b:
        LayoutNet_SendCurrentLayoutBitmap(0);
        return;
    case 0x18: {
        PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pPendingTrainQueueHead;
        if (pTrain != 0) {
            // Mid-exit find-first scan: /O2 rotates it (and tail-merges the null-exit return into
            // the shared epilogue) identically for the for/while/do-while forms -- an intrinsic
            // residual, same class as GameNet_DrainPendingTrainQueue's scan (see docs/PARKED.md).
            unsigned char ownerA = pNode->bEventOwnerA;
            for (;;) {
                if (ownerA == pTrain->bOwnerByteA &&
                    pNode->eventTrainId == (unsigned int)pTrain->wTrainId)
                    break;
                pTrain = (PeerTrainNodePartial *)pTrain->pNext;
                if (pTrain == 0)
                    return;
            }
            if (pTrain->bAckCounterA != 0) {
                pTrain->bAckCounterA--;
                return;
            }
        }
        break;
    }
    case 0x1a:
        ReleaseOwnerTrainsAndBroadcast(pNode);
        return;
    case 0x1c:
        SetUiModeAndNotifyWidgets(2);
        break;
    }
}

// FUNCTION: LOCO 0x43f7b0  (?GameNet_TeardownAllSessionState@@YIXPAVDPlaySessionMgr@@@Z)
// The full DirectPlay session teardown (Ghidra: GameNet::TeardownAllSessionState): post the
// provider-teardown notification, clear the two connect-bookkeeping scalars, drain BOTH
// queued-node lists (each node destroyed through its own virtual scalar-deleting dtor), walk
// the 9 provider slots freeing each one's result chain node-by-node and its owned layout
// blob, then reset the providers, drop the mode back to 0 and post the teardown notification
// again. Its only caller in the whole binary is AppWindow::AbortMultiplayerSession; declared
// in src/AppWindow.cpp (the DPlaySessionMgr.h declaration budget is frozen by MailWnd.cpp's
// 0x42f8b0 -- see the note there). Free GameNet fastcall taking `this` in ecx.
//
// Source shapes pinned by the disasm: the first list head is loaded into the walk variable
// BEFORE the two scalar zero stores (the original's load lands ahead of both); each drain
// loop re-loads the walk variable FROM the head field after the delete (not a kept pNext
// temp -- the original re-reads [edi+0x7dc]/[edi+0x7e0] every iteration); and the slot loop
// is the hoisted-pointer + down-counter form (esi anchors at pLayoutData, the chain head
// read back through [esi-0xc]), NOT ResetProviders' direct-subscript form.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 204: total 12004, align=12, reg_pen 0,
// identity_miss 0, byte_diff 4, insns 68/68, compiled 204 B = the original's exact code
// length). The WHOLE residual is ONE two-instruction scheduling swap at the SetMode(0) call:
// the original emits `push 0` BEFORE `mov ecx,edi`, this compile loads ecx first (the
// ResetProviders(0) call one statement earlier, identical shape, matched in the original's
// order). Same push/load coin-flip class as v516's parked 0x440070 tail; retry only if that
// scheduling class cracks.
void __fastcall GameNet_TeardownAllSessionState(DPlaySessionMgr *pMgr) {
    GameNet_ResetProvidersAndPostTeardown(pMgr);
    GameNetQueuedNodeMaybe *pNode = pMgr->field_0x7dc;
    pMgr->queueSourceId = 0;
    pMgr->searchProviderId = 0;
    while (pNode != 0) {
        pMgr->field_0x7dc = pNode->pNext;
        delete pNode;
        pNode = pMgr->field_0x7dc;
    }
    pNode = pMgr->pPendingTrainQueueHead;
    while (pNode != 0) {
        pMgr->pPendingTrainQueueHead = pNode->pNext;
        delete pNode;
        pNode = pMgr->pPendingTrainQueueHead;
    }
    DPlaySessionMgrProviderSlot *slot = pMgr->aProviderSlots;
    int nSlots = 9;
    do {
        GameNetRosterResultNode *n = slot->pResultsChainHead;
        while (n != 0) {
            slot->pResultsChainHead = n->pNext;
            operator delete(n);
            n = slot->pResultsChainHead;
        }
        if (slot->pLayoutData != 0) {
            operator delete(slot->pLayoutData);
            slot->pLayoutData = 0;
        }
        slot++;
        nSlots--;
    } while (nSlots != 0);
    pMgr->ResetProviders(0);
    pMgr->SetMode(0);
    GameNet_ResetProvidersAndPostTeardown(pMgr);
}

// FUNCTION: LOCO 0x43f880
// Leaf handler for queued event type 0xc (dispatched by GameNetManager_HandleQueuedEvent).
// Removes the peer identified by the current event's queueSourceId from every provider slot
// (clearing that slot's providerId + short name), resets queueSourceId, then reconciles by
// connection mode: mode 0 (join/client) redraws the application-setup layout if visible; mode 2
// (host) marks the remembered-choice flag, re-broadcasts the roster, and repaints the setup window;
// any other mode tears the providers down. Free GameNet fastcall taking `this` in ecx.
//
// EFFECTIVE MATCH (DIFF 66, insns 60/60): fully structural -- the slot-clear loop, queueSourceId
// reset, and the compare-chain switch dispatch all byte-align exactly (the prefix through offset
// 0x52 is identical). The sole residual is a symmetric eax<->ecx register rotation across every
// g_pSplashWnd scratch load in the case bodies: the original reuses the just-freed eax (the return
// register of the preceding call / the switch-key register), this compile grabs ecx instead --
// Yoda #29/#30, not source-steerable (case reorder + the faithful no-cache re-load shape both
// confirmed inert this session). See docs/PARKED.md.
void __fastcall GameNet_RemoveSourcePeerAndReconcile(DPlaySessionMgr *pMgr) {
    // Single induction pointer walking the 9 slots (the down-counter is separate from the pointer,
    // matching the original's `add eax,0x4c` / `dec edx` shape). queueSourceId is re-read each
    // iteration (a member read the slot store could alias -- Yoda #19).
    DPlaySessionMgrProviderSlot *pSlot = pMgr->aProviderSlots;
    int n = 9;
    do {
        if (pSlot->providerId == pMgr->queueSourceId) {
            pSlot->sAddressOrName[0] = 0;
            pSlot->providerId = 0;
        }
        pSlot++;
        n--;
    } while (n != 0);
    pMgr->queueSourceId = 0;
    // Subtract-chain switch (cases 0/2 tested ascending, default is the fall-through) -- the case
    // body layout is compiler-chosen and source-order-inert for a compare-chain switch (v245).
    switch (pMgr->connectionMode) {
    case 0:
        if (g_pSplashWnd->pApplSetupWnd->field_0xe8 != 0)
            g_pSplashWnd->pApplSetupWnd->RestartProviderScanMaybe();
        break;
    case 2:
        g_pNetSettings->bUseSecondaryRememberedChoice = 1;
        GameNet_BroadcastRosterSnapshot(pMgr);
        if (IsWindowVisible(g_pSplashWnd->pApplSetupWnd->hwndSelf)) {
            g_pSplashWnd->pApplSetupWnd->DrawAvatarGrid();
            g_pSplashWnd->pApplSetupWnd->CommitScreenUpdate(
                g_pSplashWnd->pApplSetupWnd->hwndSelf, 0, 0, 0);
            return;
        }
        break;
    default:
        GameNet_ResetProvidersAndPostTeardown(pMgr);
        return;
    }
}

// FUNCTION: LOCO 0x440070
// Roster snapshot pack + reliable broadcast (opcode 0x3f1): allocate one 0x228-byte
// RosterSnapshotWireMsg, stamp the header (opcode, live slot count, grid dims), pack all 9
// provider slots into the record array via GameNet_PackRosterRecord, and post the packet to
// the send queue (bReliable 1). Then, if the setup page is on screen, redraw its avatar grid
// and commit. Called by AssignProviderToSlot and by GameNet_RemoveSourcePeerAndReconcile's
// connectionMode-2 arm. The header-field names are the RECEIVER's (src/GameNet.h): from this
// producer's side nReliable is a full-dword copy of field_0x8 (the live slot count, 9) and
// bGridCols/bGridRows are the low bytes of nProviderSlotsPerRow/nProviderSlotRows.
//
// EFFECTIVE MATCH (DIFF 6, insns 67/67, byte_diff 2): structure and content complete. The ONE
// residual is a single-instruction scheduling coin-flip in the CommitScreenUpdate tail: the
// original pushes all three zero args BEFORE loading the receiver (`mov ecx,[ecx+0x220]`),
// this compile interleaves that load between the 2nd and 3rd push (identical instruction set,
// one `push esi` two bytes earlier). The sibling GameNet_RemoveSourcePeerAndReconcile
// (0x43f880) shows the SAME flip at its byte-identical original tail, so the original source
// spelling of this tail is confirmed shared and the flip is the TU-consistent /Og tie-break
// (Yoda #29/#30 family). Probes refuted, do not re-run: typed constants ((HDC)0/(RECT*)0) --
// byte-identical; `!= 0` on the IsWindowVisible gate -- byte-identical; BOOL bVisible local --
// WORSE (DIFF 8). The pSlot-after-header-stores decl order IS load-bearing (pins the original's
// one-register this/pSlot aliasing: reads via [esi-0x510] after `add esi,0x518`). See
// docs/PARKED.md.
void __fastcall GameNet_BroadcastRosterSnapshot(DPlaySessionMgr *pMgr) {
    RosterSnapshotWireMsg *pMsg = (RosterSnapshotWireMsg *)::operator new(0x228);
    pMsg->wOpcode = 0x3f1;
    pMsg->nReliable = pMgr->field_0x8;
    pMsg->bGridCols = (unsigned char)pMgr->nProviderSlotsPerRow;
    pMsg->bGridRows = (unsigned char)pMgr->nProviderSlotRows;
    DPlaySessionMgrProviderSlot *pSlot = pMgr->aProviderSlots;
    int n = 9;
    GameNetRosterWireRecord *pRec = pMsg->records;
    do {
        pRec->GameNet_PackRosterRecord(pSlot);
        pSlot++;
        pRec++;
        n--;
    } while (n != 0);
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 6;
    pNode->payloadLen = 0x228;
    pNode->pPayload = pMsg;
    pNode->destPlayerId = 0;
    pNode->bReliable = 1;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
    if (IsWindowVisible(g_pSplashWnd->pApplSetupWnd->hwndSelf)) {
        g_pSplashWnd->pApplSetupWnd->DrawAvatarGrid();
        g_pSplashWnd->pApplSetupWnd->CommitScreenUpdate(
            g_pSplashWnd->pApplSetupWnd->hwndSelf, 0, 0, 0);
    }
}

// FUNCTION: LOCO 0x43f940
// Leaf handler for queued event type 0xb (dispatched by GameNetManager_HandleQueuedEvent):
// a peer identified by destPlayerId has departed. Locate its provider slot, release the board
// train slots it owns, purge every pending-train-queue node it owns (freeing each via its virtual
// scalar dtor), and -- if a provider is currently selected -- strip its placement-result nodes from
// every slot's result chain. Then re-find the slot, clear its roster identity + layout cache, reload
// the slot's saved layout bitmap, drain its own result chain, and repaint the setup window if visible.
//
// EFFECTIVE MATCH (DIFF 432, insns 169/165): the entire clear block, LayoutSet_LoadSlotBitmap
// call, result-chain drain, and the IsWindowVisible repaint tail byte-align exactly (all the diff is
// concentrated in the two provider-slot find-loops and the two list-walk unlinks). Residuals, all
// documented-intrinsic:
//   (1) Loop-1's match ("found") store is DEFERRED by /O2 to a cold block after the ret (reached by
//       a forward `je`, jumping back); this compile keeps it inline. Trace-driven block layout
//       (Yoda #15), not source-steerable -- and it shifts every later instruction's address, which
//       is what inflates the raw byte_diff (the instructions themselves align, only their offsets
//       drift).
//   (2) Both find-loops are PEELED/rotated here (the first iteration's slot[0] load is hoisted) while
//       the original keeps the match test at the loop top -- the original redundantly RE-READS
//       destPlayerId from its stack slot every iteration (a /O2 quirk on a value param that isn't
//       reproducible from source: a param compiled to a live register can't be forced back to a
//       per-iteration memory reload).
//   (3) A pervasive counter/pointer register swap (original counter=eax/pointer=ecx; this compile the
//       reverse) cascading through the list-walk unlinks (Yoda #29/#30). Two-variable-split of the
//       counter from the result (freeing ebp for the whole-function 0-constant) and single-variable
//       list walks were the levers that landed the structure; the swap itself is a tie-break.
void DPlaySessionMgr::RemovePeerTrainsAndSlot(int destPlayerId) {
    // Locate the provider slot owning destPlayerId (or -1). The loop counter `i` is a scratch
    // register; the result `slotIndex` is committed to a stack home (it stays live across every call
    // below while ebp is pinned to the constant 0 the clear block reuses).
    DPlaySessionMgrProviderSlot *pSlot = aProviderSlots;
    int i = 0;
    int slotIndex;
    do {
        if (pSlot->providerId == destPlayerId) { slotIndex = i; goto found; }
        i++;
        pSlot++;
    } while (i < 9);
    slotIndex = -1;
found:
    if (slotIndex < 0) return;

    // Release the departed peer's board train slots, then purge every pending-train-queue node it
    // owns (restarting the walk from the head after each removal).
    g_PeerTrainSlotQueueEdge.ReleaseSlotsForOwnerMaybe((unsigned char)slotIndex);
    PeerTrainNodePartial *pNode = (PeerTrainNodePartial *)pPendingTrainQueueHead;
    PeerTrainNodePartial *pPrev = 0;
    while (pNode != 0) {
        if (slotIndex == pNode->bOwnerByteA) {
            if (pPrev == 0)
                pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pNode->pNext;
            else
                pPrev->pNext = pNode->pNext;
            pNode->pNext = 0;
            delete (GameNetQueuedNodeMaybe *)pNode;
            OutputDebugStringA("NETMAN - inbound train removed\n");
            pNode = (PeerTrainNodePartial *)pPendingTrainQueueHead;
            pPrev = 0;
        } else {
            pPrev = pNode;
            pNode = (PeerTrainNodePartial *)pNode->pNext;
        }
    }

    // Strip the peer's placement-result nodes from every slot's result chain (only while a provider
    // is selected). Each removal re-reads the list head but keeps the prev pointer (sic -- faithful).
    if (selectedProviderIndex >= 0) {
        DPlaySessionMgrProviderSlot *pS = aProviderSlots;
        int i = 9;
        do {
            GameNetRosterResultNode *pR = pS->pResultsChainHead;
            GameNetRosterResultNode *pRPrev = 0;
            while (pR != 0) {
                if (pR->bOwnerA == slotIndex) {
                    if (pRPrev == 0)
                        pS->pResultsChainHead = pR->pNext;
                    else
                        pRPrev->pNext = pR->pNext;
                    ::operator delete(pR);
                    pR = pS->pResultsChainHead;
                } else {
                    pRPrev = pR;
                    pR = pR->pNext;
                }
            }
            pS++;
            i--;
        } while (i != 0);
    }

    // Re-find the slot (the original re-derives the index here) and clear its roster identity +
    // layout cache. Direct array subscripting so /O2 folds the (j+0x12)*0x4c anchor for the +0x40
    // field exactly as LayoutSet_LoadSlotBitmap does.
    int j = 0;
    DPlaySessionMgrProviderSlot *pS2 = aProviderSlots;
    do {
        if (pS2->providerId == destPlayerId) goto found2;
        j++;
        pS2++;
    } while (j < 9);
    goto repaint;
found2:
    aProviderSlots[j].sAddressOrName[0] = 0;
    aProviderSlots[j].providerId = 0;
    aProviderSlots[j].bDirty = 0;
    if (aProviderSlots[j].pLayoutData != 0) {
        ::operator delete(aProviderSlots[j].pLayoutData);
        aProviderSlots[j].pLayoutData = 0;
        aProviderSlots[j].nLayoutDataSize = 0;
        aProviderSlots[j].wLayoutCols = 0;
        aProviderSlots[j].wLayoutRows = 0;
        aProviderSlots[j].dwLayoutVersion = 0;
    }
    LayoutSet_LoadSlotBitmap(j);
    while (aProviderSlots[j].pResultsChainHead != 0) {
        GameNetRosterResultNode *pR = aProviderSlots[j].pResultsChainHead;
        aProviderSlots[j].pResultsChainHead = pR->pNext;
        ::operator delete(pR);
    }
repaint:
    if (IsWindowVisible(g_pSplashWnd->pApplSetupWnd->hwndSelf)) {
        g_pSplashWnd->pApplSetupWnd->DrawAvatarGrid();
        g_pSplashWnd->pApplSetupWnd->CommitScreenUpdate(
            g_pSplashWnd->pApplSetupWnd->hwndSelf, 0, 0, 0);
    }
}

// FUNCTION: LOCO 0x43fb50
// Leaf handler for queued event type 0x1a (dispatched by GameNetManager_HandleQueuedEvent).
// The departing owner's id is carried in the event node's pPayload slot, reused as an int (event
// 0x1a allocates no payload -- the dispatcher just returns, freeing nothing). Releases that owner's
// board train slots, drops its single pending-train-queue node, then broadcasts a "left origin"
// (opcode 0x3f7) for -- and removes -- every placement-result node it owns on the currently-selected
// provider slot. Repaints the application-setup window if visible.
//
// EFFECTIVE MATCH: the ReleaseSlotsForOwnerMaybe call, unlink/delete block, the result-chain broadcast
// loop, and the IsWindowVisible repaint tail align exactly. The whole residual is the pending-train
// find-first scan: /O2 peels/rotates this two-exit field-read loop (duplicating the match test at
// the bottom instead of looping back to a single top test) for every source form -- the intrinsic
// drain-0x43e010 mid-exit rotation class -- which shifts every later instruction's offset (the align
// cascade), plus one extra `xor edx` that materializes the bOwnerB zero-extend the original folds
// into the compare's `and edx,0xff` (a register-scheduling tie-break under the peel's pressure).
void DPlaySessionMgr::ReleaseOwnerTrainsAndBroadcast(NetMsgQueueNode *pMsg) {
    int ownerId = (int)pMsg->pPayload;
    g_PeerTrainSlotQueueEdge.ReleaseSlotsForOwnerMaybe((unsigned char)ownerId);

    // Find-first-and-remove this owner's pending-train-queue node (if any).
    // Find-first-and-remove list walk (match test at the loop top, advance+null-check at the bottom).
    // /O2 peels/rotates this two-exit field-read scan identically for every source form (do-while,
    // goto, plain while) -- the drain 0x43e010 family's intrinsic mid-exit rotation. Kept as the
    // clearest form; the peel is the bulk of the residual (see EFFECTIVE note below).
    PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pPendingTrainQueueHead;
    PeerTrainNodePartial *pPrev = 0;
    if (pTrain != 0 && ownerId >= 0) {
        do {
            if (ownerId == pTrain->bOwnerByteA) {
                if (pPrev != 0)
                    pPrev->pNext = pTrain->pNext;
                else
                    pPendingTrainQueueHead = (GameNetQueuedNodeMaybe *)pTrain->pNext;
                pTrain->pNext = 0;
                delete (GameNetQueuedNodeMaybe *)pTrain;
                break;
            }
            pPrev = pTrain;
            pTrain = (PeerTrainNodePartial *)pTrain->pNext;
        } while (pTrain != 0);
    }

    // Broadcast + remove every placement-result node this owner holds on the selected slot. Each
    // GameNet_BroadcastLocalOrigin call removes the node, so the head is re-read after a match.
    GameNetRosterResultNode *pR = aProviderSlots[selectedProviderIndex].pResultsChainHead;
    while (pR != 0) {
        if (pR->bOwnerA == ownerId) {
            GameNet_BroadcastLocalOrigin(pR->trainId, pR->bOwnerA, pR->bSlotKey);
            pR = aProviderSlots[selectedProviderIndex].pResultsChainHead;
        } else {
            pR = pR->pNext;
        }
    }

    // Repaint the application-setup window if visible.
    if (IsWindowVisible(g_pSplashWnd->pApplSetupWnd->hwndSelf)) {
        g_pSplashWnd->pApplSetupWnd->DrawAvatarGrid();
        g_pSplashWnd->pApplSetupWnd->CommitScreenUpdate(
            g_pSplashWnd->pApplSetupWnd->hwndSelf, 0, 0, 0);
    }
}

// FUNCTION: LOCO 0x43fc50
// Apply an inbound provider-slot snapshot (inbound event type 9): reconcile each of the 9 provider
// slots against the peer's snapshot payload, broadcast our own selected slot's enable/disable
// transitions, refresh/reload bitmaps for slots that emptied or whose layout changed, reply with
// our stored layout if it diverged, then repaint the application-setup window.
//
// The snapshot payload (pMsg->pPayload) is an array of 9 DPlaySessionMgrProviderSlot records. For
// each destination slot we snapshot its old bEnabled/unkDword2, CopyFrom the source record, then
// restore unkDword2 (that field is runtime-local, not part of the wire snapshot). The source slot
// is read inline (`((DPlaySessionMgrProviderSlot*)pMsg->pPayload)[i]`) at every use rather than via
// a cached local: the original re-reads pMsg->pPayload at each source access (the CopyFrom source
// arg and the two unkDword2 compares), sharing only the i*0x4c stride derived from the destination
// induction pointer.
//
// The two GameNet_BroadcastSlot{En,Dis}abled calls are wired by call address to match the binary,
// which broadcasts opcode 0x3f4 ("Enabled", 0x440310) on an enabled->disabled transition and 0x3f5
// ("Disabled", 0x440390) on disabled->enabled -- the names read inverted vs. the transition, but the
// call targets are faithful to the original.
void DPlaySessionMgr::ApplyProviderSnapshot(NetMsgQueueNode *pMsg) {
    nProviderSlotsPerRow = pMsg->bEventGridCols;
    nProviderSlotRows = pMsg->bEventGridRows;
    field_0x8 = pMsg->eventSlotCount;

    int selectedSlot = 0;
    bool bLayoutChanged = false;
    int i = 0;
    DPlaySessionMgrProviderSlot *pSlot = &aProviderSlots[0];
    do {
        bool bWasOccupied = (pSlot->providerId != 0);
        bool bWasEmpty = !bWasOccupied;
        unsigned char bOldEnabled = pSlot->bEnabled;
        unsigned int oldUnkDword2 = pSlot->dwLayoutVersion;

        pSlot->CopyFrom(&((DPlaySessionMgrProviderSlot *)pMsg->pPayload)[i]);
        pSlot->dwLayoutVersion = oldUnkDword2;  // unkDword2 is runtime-local, not part of the snapshot

        unsigned int newProviderId = pSlot->providerId;
        if (newProviderId == (unsigned int)searchProviderId) {
            if (bOldEnabled == 0) {
                if (pSlot->bEnabled != 0)
                    GameNet_BroadcastSlotEnabled(this);   // 0x440310 (enabled->disabled transition)
            } else if (pSlot->bEnabled == 0) {
                GameNet_BroadcastSlotDisabled(this);      // 0x440390 (disabled->enabled transition)
            }
            if (selectedProviderIndex == i) {
                if (((DPlaySessionMgrProviderSlot *)pMsg->pPayload)[i].dwLayoutVersion !=
                    pSlot->dwLayoutVersion)
                    bLayoutChanged = true;
            } else {
                selectedSlot = i;
                pSelectedProvider = pSlot;
                selectedProviderIndex = i;
            }
        } else if (newProviderId == 0) {
            if (bWasOccupied || pSlot->pLayoutData == 0)
                LayoutSet_LoadSlotBitmap(i);
        } else if (bWasEmpty ||
                   oldUnkDword2 != ((DPlaySessionMgrProviderSlot *)pMsg->pPayload)[i].dwLayoutVersion) {
            LayoutNet_PostSimpleOpcode(newProviderId);
        }

        pSlot++;
        i++;
    } while (i < 9);

    if (bLayoutSyncPingSent == 0) {
        bLayoutSyncPingSent = 1;
        LayoutNet_PostSimpleOpcode(0);
    }
    if (bLayoutChanged)
        LayoutNet_ReplyWithStoredLayout(queueSourceId);

    if (connectionMode == 0)
        g_pSplashWnd->pApplSetupWnd->SelectProviderSlotMaybe(selectedSlot);

    if (IsWindowVisible(g_pSplashWnd->pApplSetupWnd->hwndSelf)) {
        g_pSplashWnd->pApplSetupWnd->DrawAvatarGrid();
        g_pSplashWnd->pApplSetupWnd->CommitScreenUpdate(
            g_pSplashWnd->pApplSetupWnd->hwndSelf, 0, 0, 0);
    }
}

// FUNCTION: LOCO 0x43fe30
// Assign/relocate a provider into a specific roster slot (inbound event type 4). Locate an existing
// provider slot either by id (providerId) or, when providerId == -1, by matching pName against each
// slot's short name. Then:
//   * if that slot IS the target slot -> just refresh its owner pair (the dwTailAlias union) + clear
//     unkDword2;
//   * else if the target slot is empty (and in range) -> copy the name/id/owner-pair into it, retarget
//     the selected-provider tracking if the id matches searchProviderId, and clear the old slot;
//   * else if it wasn't found anywhere -> drop the provider into the first free slot (empty id AND
//     empty name).
// Finally, if a connect is pending, pack + broadcast the updated roster.
//
// param_4 (owners) is a by-value Pair16 whose two 16-bit halves land in the slot's dwTailAlias
// union (wCols/wRows) as two word stores. The two top-level branches are written inverted
// (`if (providerId != -1)` and `if (found != targetSlot)`) so the compiler lays the larger
// fall-through body ahead of the je-reached out-of-line block, matching the .text layout. Every slot
// access is a direct `aProviderSlots[i].field` subscript (not a hoisted element pointer) so /O2 keeps
// the original's `this + i*0x4c`-anchored large-displacement addressing.
void DPlaySessionMgr::AssignProviderToSlot(int providerId, int targetSlot,
                                                unsigned char *pName, Pair16 owners) {
    int found = -2;
    if (providerId != -1) {
        for (int i = 0; i < 9; i++) {
            if (aProviderSlots[i].providerId == (unsigned int)providerId) {
                found = i;
                break;
            }
        }
    } else {
        for (int i2 = 0; i2 < 9; i2++) {
            if (strcmp((const char *)pName, aProviderSlots[i2].sAddressOrName) == 0) {
                found = i2;
                break;
            }
        }
    }

    if (found != targetSlot) {
        if (aProviderSlots[targetSlot].providerId == 0 && targetSlot >= 0) {
            strcpy(aProviderSlots[targetSlot].sAddressOrName, (const char *)pName);
            aProviderSlots[targetSlot].providerId = providerId;
            aProviderSlots[targetSlot].wCols = owners.lo;
            aProviderSlots[targetSlot].wRows = owners.hi;
            aProviderSlots[targetSlot].dwLayoutVersion = 0;
            if (aProviderSlots[targetSlot].providerId == (unsigned int)searchProviderId) {
                pSelectedProvider = &aProviderSlots[targetSlot];
                selectedProviderIndex = targetSlot;
            }
            if (found >= 0) {
                aProviderSlots[found].sAddressOrName[0] = 0;
                aProviderSlots[found].providerId = 0;
                aProviderSlots[found].dwLayoutVersion = 0;
            }
        } else if (found < 0) {
            for (int i3 = 0; i3 < 9; i3++) {
                if (aProviderSlots[i3].providerId == 0 &&
                    aProviderSlots[i3].sAddressOrName[0] == 0) {
                    strcpy(aProviderSlots[i3].sAddressOrName, (const char *)pName);
                    aProviderSlots[i3].providerId = providerId;
                    aProviderSlots[i3].wCols = owners.lo;
                    aProviderSlots[i3].wRows = owners.hi;
                    aProviderSlots[i3].dwLayoutVersion = 0;
                    break;
                }
            }
        }
    } else {
        aProviderSlots[found].wCols = owners.lo;
        aProviderSlots[found].wRows = owners.hi;
        aProviderSlots[found].dwLayoutVersion = 0;
    }

    if (bConnectPending)
        GameNet_BroadcastRosterSnapshot(this);
}

// FUNCTION: LOCO 0x40a4a0 (LayoutSet::SelectProviderSlotMaybe)
// Records the newly-selected provider slot; unless the secondary-remembered-choice path is
// active, refreshes the connection-status message and commits the redraw, but only while the
// window is in its ready-for-redraw state.
void ApplSetupWnd::SelectProviderSlotMaybe(int slotIndex) {
    nSelectedProviderSlot = slotIndex;
    if (g_pNetSettings->bUseSecondaryRememberedChoice == 0 && bReadyForRedrawMaybe != 0) {
        RefreshConnectStatusText();
        CommitScreenUpdate(hwndSelf, 0, 0, 0);
    }
}

// FUNCTION: LOCO 0x40a300 (LayoutSet::NotifyProviderSelectedMaybe)
// While ready for redraw: if the secondary-remembered-choice path is active, marks the manager's
// layout-sync-ping-sent flag and refreshes the connection-status message; either way redraws +
// commits, then sends a select-request (arg 0).
void ApplSetupWnd::NotifyProviderSelectedMaybe() {
    if (bReadyForRedrawMaybe != 0) {
        if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
            g_pDPlaySessionMgr->bLayoutSyncPingSent = 1;
            RefreshConnectStatusText();
        }
        DrawAvatarGrid();
        CommitScreenUpdate(hwndSelf, 0, 0, 0);
        SendSelectRequestMaybe(0);
    }
}

// FUNCTION: LOCO 0x40a350 (LayoutSet::AdvanceToNextProviderMaybe)
// Clears bUnk0x10c, then either re-sends a select-request for the next slot (nUnk0xf4+1) or --
// when the secondary-remembered-choice path is active -- resets the session's providers and
// reloads the index file; either way redraws+arms via the slot-0x20 virtual, then arms two
// redraw timers (events 0x50/50ms, 0x52/75ms) and sets nUnk0x1b0 = 2.
void ApplSetupWnd::AdvanceToNextProviderMaybe() {
    bUnk0x10c = 0;
    if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
        g_pDPlaySessionMgr->ResetProviders(0);
        LoadIndexFileMaybe(1);
    } else {
        ApplyListSelectionMaybe(nUnk0xf4 + 1);
    }
    OnActivate(0);
    hTimerA = SetTimer(hwndSelf, 0x50, 0x32, 0);
    hTimerB = SetTimer(hwndSelf, 0x52, 0x4b, 0);
    nUnk0x1b0 = 2;
}

// FUNCTION: LOCO 0x40a150 (LayoutSet::LayoutSet_RebuildAndNotifyMaybe)
// No-op (early return) if bUnk0x10c is already set; else clears bLayoutSyncPingSent, tears down
// + reconnects providers (reset teardown, reset connection, prepare internet), frees the WHOLE
// provider list (same free-loop shape as SetProviderListMaybe), posts a type-2 local-queue notification
// carrying this window's own HWND as payload, then sets bUnk0x10c.
void ApplSetupWnd::RebuildAndNotifyMaybe() {
    if (bUnk0x10c != 0) return;
    g_pDPlaySessionMgr->bLayoutSyncPingSent = 0;
    GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
    GameNet_PostResetConnection(g_pDPlaySessionMgr);
    GameNet_PostPrepareInternet(g_pDPlaySessionMgr);
    if (pListHeadMaybe == (ProviderListNode *)-1) {
        pListHeadMaybe = 0;
    }
    while (pListHeadMaybe != 0) {
        ProviderListNode *pNext = pListHeadMaybe->pNext;
        char *pszText = pListHeadMaybe->pszText;
        if (pszText != 0) {
            delete pszText;
        }
        delete pListHeadMaybe;
        pListHeadMaybe = pNext;
    }
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 2;
    pNode->pPayload = (void *)hwndSelf;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
    bUnk0x10c = 1;
}

// FUNCTION: LOCO 0x40a260 (LayoutSet::RestartProviderScanMaybe)
// Tears down + reconnects providers; unless the secondary-remembered-choice path is active,
// rebuilds the layout list, clears bLayoutSyncPingSent, and draws the connect-status text + a
// locale-string label into the +0x120 buffer -- else resets the connection, prepares for an
// internet session, and reloads the index file. Either way redraws + commits.
void ApplSetupWnd::RestartProviderScanMaybe() {
    GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
    if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
        GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
        GameNet_PostResetConnection(g_pDPlaySessionMgr);
        GameNet_PostPrepareInternet(g_pDPlaySessionMgr);
        LoadIndexFileMaybe(1);
    } else {
        RebuildAndNotifyMaybe();
        g_pDPlaySessionMgr->bLayoutSyncPingSent = 0;
        DrawProviderList(pListHeadMaybe);
        g_UIResources.LoadLocaleString(0x6f, textBuf0x120, 0x80);
        LayoutAndDrawLabel();
    }
    DrawAvatarGrid();
    CommitScreenUpdate(hwndSelf, 0, 0, 0);
}

// FUNCTION: LOCO 0x40a3d0 (LayoutSet::SetProviderListMaybe)
// Clears bUnk0x10c; resets a -1 "not yet built" sentinel head to null, then frees the WHOLE
// provider list (deleting each node's pszText, if any, then the node). If pNewHead == 0, resets
// the head back to the -1 sentinel and, when field_0xe8 is set, rebuilds+returns early;
// otherwise installs pNewHead as the new list head, calls ApplyListSelectionMaybe(0), and -- while ready
// for redraw -- redraws the list + commits.
void ApplSetupWnd::SetProviderListMaybe(ProviderListNode *pNewHead) {
    bUnk0x10c = 0;
    if (pListHeadMaybe == (ProviderListNode *)-1) {
        pListHeadMaybe = 0;
    }
    while (pListHeadMaybe != 0) {
        ProviderListNode *pNext = pListHeadMaybe->pNext;
        char *pszText = pListHeadMaybe->pszText;
        if (pszText != 0) {
            delete pszText;
        }
        delete pListHeadMaybe;
        pListHeadMaybe = pNext;
    }
    if (pNewHead == 0) {
        pListHeadMaybe = (ProviderListNode *)-1;
        if (field_0xe8 != 0) {
            RebuildAndNotifyMaybe();
            return;
        }
    } else {
        pListHeadMaybe = pNewHead;
        ApplyListSelectionMaybe(0);
        if (bReadyForRedrawMaybe != 0) {
            DrawProviderList(pListHeadMaybe);
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
        }
    }
}

// FUNCTION: LOCO 0x40aaf0 (LayoutSet::ApplyListSelectionMaybe)
// Walks pListHeadMaybe forward `index` nodes; if the walk lands on a real node (and the list
// wasn't empty to begin with), caches the index + that node's pszText, tears down + reconnects
// providers, redraws the connect-status text, commits, then re-prepares + attempts an internet
// join. Otherwise (secondary-remembered-choice inactive but the walk failed) resets the cached
// index to 0 and rebuilds the layout list. No-op entirely when the secondary-remembered-choice
// path is active.
void ApplSetupWnd::ApplyListSelectionMaybe(int index) {
    ProviderListNode *pNode = pListHeadMaybe;
    int walked = 0;
    if (g_pNetSettings->bUseSecondaryRememberedChoice == 0) {
        if (pNode != 0 && index != 0) {
            int i = index;
            walked = index;
            do {
                pNode = pNode->pNext;
            } while (--i != 0);
        }
        if (walked == index && pNode != 0) {
            nUnk0xf4 = index;
            pSelectedNodeTextMaybe = pNode->pszText;
            GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
            RefreshConnectStatusText();
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
            GameNet_PostResetConnection(g_pDPlaySessionMgr);
            GameNet_PostPrepareInternet(g_pDPlaySessionMgr);
            GameNet_PostAttemptJoin(g_pDPlaySessionMgr);
            return;
        }
        nUnk0xf4 = 0;
        RebuildAndNotifyMaybe();
    }
}

// FUNCTION: LOCO 0x40a220 (LayoutSet::AbortToDisconnectedStateMaybe)
// Cancels the cursor-mode transition, tears down + resets the session's providers, sets mode 3,
// sets SplashWnd state 7, and clears bUnk0x114. Byte-identical to FUN_0040a4e0's own inline
// "secondary path, no available provider" arm -- likely a shared helper factored out for that
// one call site.
void ApplSetupWnd::AbortToDisconnectedStateMaybe() {
    ScheduleModeTransition(0, 0, 0, 0, 1);
    GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
    g_pDPlaySessionMgr->SetMode(3);
    g_pSplashWnd->SetState(7);
    bUnk0x114 = 0;
}

// FUNCTION: LOCO 0x40aba0 (LayoutSet::SelectGridCellFromPointMaybe)
// The GRID-mode click handler: converts a client-area (x, y) into a 3x3 grid cell (col, row)
// via rectGridMaybe's own bounds divided by the fixed grid dimension 3, bounds-checks against
// g_pDPlaySessionMgr's real nProviderSlotsPerRow/nProviderSlotRows, and -- if the resulting
// flat slot index differs from nSelectedProviderSlot -- sends a select-request for it and
// redraws.
void ApplSetupWnd::SelectGridCellFromPointMaybe(int x, int y) {
    int iVar2 = (x - rectGridMaybe.left) / ((rectGridMaybe.right - rectGridMaybe.left) / 3) + 1;
    int iVar1 = (y - rectGridMaybe.top) / ((rectGridMaybe.bottom - rectGridMaybe.top) / 3);
    iVar1++;
    if ((iVar2 <= g_pDPlaySessionMgr->nProviderSlotsPerRow) &&
        (iVar1 <= g_pDPlaySessionMgr->nProviderSlotRows)) {
        iVar1--;
        int slot = iVar1 * g_pDPlaySessionMgr->nProviderSlotsPerRow + -1 + iVar2;
        if (slot != nSelectedProviderSlot) {
            SendSelectRequestMaybe(slot);
            RefreshConnectStatusText();
            DrawAvatarGrid();
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
        }
    }
}

// FUNCTION: LOCO 0x40aa20 (LayoutSet::ApplySecondaryListSelectionMaybe)
// Secondary-remembered-choice counterpart of ApplyListSelectionMaybe: tears down + resets providers, walks
// pListHeadSecondaryMaybe forward nUnk0xf4 nodes; on a real node, caches its pszText, applies
// the config line, assigns the local player into slot 0 with the current board dimensions,
// resets the selection, re-resets + re-prepares + re-attempts an internet join, then -- while
// ready for redraw -- redraws + commits.
void ApplSetupWnd::ApplySecondaryListSelectionMaybe(int deadIndexArg) {
    ProviderListNode *pNode = pListHeadSecondaryMaybe;
    GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
    for (int i = nUnk0xf4; i != 0; i--) {
        pNode = pNode->pNext;
    }
    if (pNode != 0) {
        char *pszText = pNode->pszText;
        pSelectedNodeTextSecondaryMaybe = pszText;
        g_pDPlaySessionMgr->LayoutSet_InitFromConfigFileMaybe(pszText);
        Pair16 owners;
        owners.lo = g_worldBoard.wCols;
        owners.hi = g_worldBoard.wRows;
        g_pDPlaySessionMgr->AssignProviderToSlot(-1, 0, (unsigned char *)g_pLocalPlayerIdentity->name,
                                                  owners);
        nSelectedProviderSlot = 0;
        GameNet_PostResetConnection(g_pDPlaySessionMgr);
        GameNet_PostPrepareInternet(g_pDPlaySessionMgr);
        GameNet_PostAttemptJoin(g_pDPlaySessionMgr);
        if (bReadyForRedrawMaybe != 0) {
            DrawAvatarGrid();
            RefreshConnectStatusText();
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
        }
    }
}

// FUNCTION: LOCO 0x40ac50 (LayoutSet::LayoutSet_SendSelectRequestMaybe)
// While the secondary-remembered-choice path is active: calls AssignProviderToSlot directly
// (searchProviderId if bConnectPending, else -1) with the board's own cols/rows packed as the
// owners pair, then records targetSlot. Otherwise builds and sends an opcode-0x3f0
// SelectRequestWireMsg (queue type 6, reliable) carrying targetSlot, the local player's name,
// and the board's cols/rows -- see SelectRequestWireMsg's own plate comment for how the
// receiving side reads the same bytes back -- and clears bLayoutSyncPingSent.
//
// EFFECTIVE MATCH (DIFF 4/324, insns 99/99): every instruction aligns (asmscore.py --dump
// align=0); the sole residual is a symmetric ecx/edx register-role swap across the
// destPlayerId-read/bReliable-store/bLayoutSyncPingSent-clear tail (Yoda #29/#30) -- tried both
// an `int destId` temp (matches source statement order to the original's own read-before-store
// sequence) and a cached `GameNetThreadState *pQueue` local (scored worse, DIFF 14); the temp-int
// form is closest and kept. v360 additionally probed the fully-INLINE read
// (`pNode->destPlayerId = g_pGameNetThreadState->dpidCurrentPlayer;` with no temp), on the theory
// that v360's SoundBank finding -- inline-expression vs. named-local changes codegen -- might
// apply: it is WORSE (DIFF 11), because without the temp the dpidCurrentPlayer load is no longer
// hoisted above the bReliable store, which is the original's own order. The temp is
// load-bearing for the read ORDER; the leftover ecx/edx role swap is a separate, still
// unsteerable choice. Confirmed unsteerable at this budget.
void ApplSetupWnd::SendSelectRequestMaybe(int targetSlot) {
    if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
        Pair16 owners;
        owners.lo = g_worldBoard.wCols;
        owners.hi = g_worldBoard.wRows;
        if (g_pDPlaySessionMgr->bConnectPending != 0) {
            g_pDPlaySessionMgr->AssignProviderToSlot(g_pDPlaySessionMgr->searchProviderId, targetSlot,
                (unsigned char *)g_pLocalPlayerIdentity->name, owners);
        } else {
            g_pDPlaySessionMgr->AssignProviderToSlot(-1, targetSlot,
                (unsigned char *)g_pLocalPlayerIdentity->name, owners);
        }
        nSelectedProviderSlot = targetSlot;
        return;
    }

    SelectRequestWireMsg *pMsg = new SelectRequestWireMsg;
    pMsg->wOpcode = 0x3f0;
    pMsg->nTargetSlot = targetSlot;
    strcpy(pMsg->szName, g_pLocalPlayerIdentity->name);
    pMsg->wCols = g_worldBoard.wCols;
    pMsg->wRows = g_worldBoard.wRows;

    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 6;
    pNode->pPayload = pMsg;
    pNode->payloadLen = 0x1c;
    int destId = g_pGameNetThreadState->dpidCurrentPlayer;
    pNode->bReliable = 1;
    pNode->destPlayerId = destId;
    g_pDPlaySessionMgr->bLayoutSyncPingSent = 0;
    g_pGameNetThreadState->EnqueueOrFreeNode(pNode);
}

// FUNCTION: LOCO 0x40a4e0 (LayoutSet::FUN_0040a4e0 -- WindowBase vtable+0x38 override,
// ApplSetupWnd::OnLButtonDown)
// Dispatches a click across the 5 single-button ResourceRef rects (go/exit/search/option/
// matrix), a random-sound easter-egg rect, and one of two mutually-exclusive provider-list
// click regions (LIST-mode row hit test vs GRID-mode 3x3 cell hit test, see SelectGridCellFromPointMaybe).
// Gated on !bSuppressCursorRedraw && bReadyForRedrawMaybe.
//
// PARKED (`asmscore.py --len 1339`, DIFF 804/1339 as of v300) -- structurally verified 1:1
// against the raw disasm (every branch/call/field access confirmed), but register allocation
// diverges heavily once the first virtual dispatch (ScheduleModeTransition, vtbl+0x10)
// is reached: the original keeps x/y (from lParam) pinned in edi/ebp across the WHOLE function
// and calls the vtable slot via a single `call [reg+0x10]`, while this compile spills lParam's
// point into stack-resident locals and materializes the vtable slot's function pointer into a
// register before calling -- tried both a POINT local and the wrapper-vs-direct-cast call forms
// with no change (len identical either way, confirming it's not source-syntax-steerable at this
// budget). Same class of residual as the already-parked `AlbumCardWnd::OnLButtonDown`
// (DIFF 954/1324) and `EditCardWnd::HandleLButtonDownMaybe` (0x41ac10) -- large multi-region
// WM_LBUTTONDOWN dispatchers with many PtInRect hotspots consistently resist full byte-match
// under this toolchain. See docs/PARKED.md.
LRESULT ApplSetupWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!bSuppressCursorRedraw && bReadyForRedrawMaybe) {
        POINT pt;
        pt.x = lParam & 0xffff;
        pt.y = (unsigned int)lParam >> 0x10;

        if (PtInRect(&pApGoBtn->rect, pt) &&
            (g_pDPlaySessionMgr->bConnectPending && g_pDPlaySessionMgr->bLayoutSyncPingSent)) {
            g_UIResources.PlayUiSound(0x5015);
            pApGoBtn->DrawFrame(1, 0);
            CommitRectUpdate(pApGoBtn->rect);
            Sleep(0x96);
            ScheduleModeTransition(0, 0, 0, 0, 1);
            g_pDPlaySessionMgr->SetMode(2);
            if (g_pNetSettings->bUseSecondaryRememberedChoice == 0) {
                if (g_pDPlaySessionMgr->bConnectPending == 0) {
                    AbortToDisconnectedStateMaybe();
                    return 0;
                }
                if (g_pDPlaySessionMgr->bLayoutSyncPingSent == 0) {
                    return 0;
                }
            } else if (g_pDPlaySessionMgr->bConnectPending == 0) {
                ScheduleModeTransition(0, 0, 0, 0, 1);
                GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
                g_pDPlaySessionMgr->SetMode(3);
                g_pSplashWnd->SetState(7);
                bUnk0x114 = 0;
                return 0;
            }
            g_pSplashWnd->SetState(6);
            return 0;
        }

        if (PtInRect(&pApExitBtn->rect, pt)) {
            g_UIResources.PlayUiSound(0x5015);
            pApExitBtn->DrawFrame(1, 0);
            CommitRectUpdate(pApExitBtn->rect);
            Sleep(0x96);
            ScheduleModeTransition(0, 0, 0, 0, 1);
            GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
            g_pDPlaySessionMgr->SetMode(3);
            g_pSplashWnd->SetState(7);
            bUnk0x114 = 0;
            return 0;
        }

        if (PtInRect(&pApSearchBtn->rect, pt)) {
            g_UIResources.PlayUiSound(0x5015);
            pApSearchBtn->DrawFrame(1, 0);
            CommitRectUpdate(pApSearchBtn->rect);
            Sleep(0x96);
            if (g_pNetSettings->bUseSecondaryRememberedChoice == 0) {
                RebuildAndNotifyMaybe();
                g_pDPlaySessionMgr->bLayoutSyncPingSent = 0;
                DrawProviderList(pListHeadMaybe);
                g_UIResources.LoadLocaleString(0x6f, textBuf0x120, 0x80);
                LayoutAndDrawLabel();
            } else {
                GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
                GameNet_PostResetConnection(g_pDPlaySessionMgr);
                GameNet_PostPrepareInternet(g_pDPlaySessionMgr);
                LoadIndexFileMaybe(1);
            }
            DrawAvatarGrid();
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
            pApSearchBtn->DrawFrame(0, 0);
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
            return 0;
        }

        if (PtInRect(&rectEasterEggSoundMaybe, pt)) {
            g_UIResources.PlaySoundAtScreenPos(rand() / 0x1999 + 0x50f3, pt.x, pt.y, 4);
            return 0;
        }

        if (PtInRect(&pApOptionBtn->rect, pt)) {
            g_UIResources.PlayUiSound(0x5015);
            pApOptionBtn->DrawFrame(1, 0);
            CommitRectUpdate(pApOptionBtn->rect);
            Sleep(0x96);
            GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
            g_pDPlaySessionMgr->SetMode(3);
            g_pSplashWnd->SetState(2);
            return 0;
        }

        if (!PtInRect(&rectListMaybe, pt)) {
            if (PtInRect(&rectGridMaybe, pt) &&
                (g_pDPlaySessionMgr->bLayoutSyncPingSent && g_pDPlaySessionMgr->bConnectPending)) {
                g_UIResources.PlayUiSound(0x5015);
                SelectGridCellFromPointMaybe(pt.x, pt.y);
            }
        } else {
            int iVar3 = (pt.y - rectListMaybe.top - 0xc) / nRowHeightMaybe;
            if (iVar3 != nUnk0xf4 && iVar3 < nListRowCountMaybe) {
                g_UIResources.PlayUiSound(0x5015);
                if (g_pNetSettings->bUseSecondaryRememberedChoice == 0) {
                    ApplyListSelectionMaybe(iVar3);
                } else if (g_pDPlaySessionMgr->bConnectPending != 0) {
                    nUnk0xf4 = iVar3;
                    ApplySecondaryListSelectionMaybe(iVar3);
                }
                ProviderListNode *pList = (g_pNetSettings->bUseSecondaryRememberedChoice == 0)
                                               ? pListHeadMaybe
                                               : pListHeadSecondaryMaybe;
                DrawProviderList(pList);
                DrawAvatarGrid();
                CommitScreenUpdate(hwndSelf, 0, 0, 0);
                return 0;
            }
        }
    }
    return 0;
}

// Included here (not top-of-file) to avoid shifting every earlier function's codegen in this
// already-matched TU -- adding these 3 lines at the top regressed SelectGridCellFromPointMaybe
// from EXACT to DIFF(130), a pure line-count-shift artifact (Yoda lesson #23) unrelated to this
// function's own position. See docs/PARKED.md for the confirmation.
#include <fstream.h>
#include <strstrea.h>
#include "DSoundChannel.h"  // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free

// FUNCTION: LOCO 0x409e70 (LayoutSet::LayoutSet_LoadIndexFileMaybe)
// Frees the WHOLE secondary provider list (same free-loop shape as the primary list's own
// siblings, e.g. SetProviderListMaybe), builds "<installPathPrefix>Layouts\index.lay", then loads it --
// preferring the RF archive (istrstream over a loaded resource buffer) and falling back to a
// loose ifstream -- into a single fixed 0x2000-byte heap buffer via one bulk read(). Parses the
// buffer line-by-line: a '\r' or NUL byte ends a "line"; '\n' bytes are skipped and also reset
// the line-start marker (handling both bare-LF and CRLF endings); each line longer than 4 bytes
// becomes a new ProviderListNode pushed onto pListHeadSecondaryMaybe, its text strcpy'd into a
// fresh 0x100-byte buffer. On completion (buffer exhausted), releases the RF resource buffer,
// deletes the (polymorphic) stream, frees the read buffer, then -- if bApplySelectionAfter --
// resets nUnk0xf4 to 0 and re-applies the (now index-0) secondary selection.
//
// PARKED (DIFF 548/699, v301) -- structurally complete and semantically verified 1:1 against
// the raw disasm (free-loop, RF-then-loose-file fallback, line-scan, throw sites all confirmed).
// The residual is the SAME still-open toolchain-level class already parked on
// `Wav_ParseAndLoad`/`Wav_ReadOrFindChunk` (src/Wav.cpp, docs/PARKED.md): the original uses a
// real EBP-framed, multi-entry SEH scope table (own unique unwind thunk at 0x475096, with
// distinct entries calling a shared scalar-dtor stub at 0x436a00 -- i.e. genuine base-subobject
// destruction protection for the `istrstream`/`ifstream` construction, not just the simple
// "free the raw block" protection a POD/no-base `new` gets) while this toolchain's cl 11.00
// compiles the equivalent source to the simpler ESP-relative, single-state-variable automatic
// scaffolding -- same shape class documented in CLAUDE.md's SEH-prologue lessons, just with a
// richer variant this project hasn't reproduced from source in 4+ dedicated sessions on
// Wav.cpp. Tried and confirmed inert here too: local declaration order/position (top-of-function
// vs point-of-use), unifying vs. splitting the istrstream/ifstream pointers into one `istream*`,
// goto-based single-exit vs. nested-loop-`return`. Not re-probed: an explicit user try/catch(...)
// wrapping just the two throw sites (Wav_ParseAndLoad's own v92 fix needed exactly this to
// materialize its 3rd SEH state) -- but LoadIndexFileMaybe has no local catch (Ghidra found no
// funclet), so the exceptions genuinely propagate to the caller; a `try { throw ...; } catch
// (...) { throw; }` re-throw shim might be worth one probe in a future session.
//
// NOTE: kept at end-of-file (rather than address-adjacent position) -- placing it between
// ApplyListSelectionMaybe and AbortToDisconnectedStateMaybe (its real address-order slot) regressed
// SelectGridCellFromPointMaybe from EXACT to DIFF(130), a TU-position-sensitivity artifact
// (Yoda lesson #23: prefer end-of-file additions in an already-matched TU). Confirmed the
// move restores every other function's score.
void ApplSetupWnd::LoadIndexFileMaybe(char bApplySelectionAfter) {
    void *pRfBuf = 0;
    istrstream *pRfStream = 0;
    ifstream *pFileStream = 0;
    istream *pStream = 0;
    char *pBuf = 0;
    int nRfSize = 0;
    char szIndexPathBuf[1285];  // byte 0 is never referenced -- see plate comment

    while (pListHeadSecondaryMaybe != 0) {
        ProviderListNode *pNext = pListHeadSecondaryMaybe->pNext;
        char *pszText = pListHeadSecondaryMaybe->pszText;
        if (pszText != 0) {
            delete pszText;
        }
        delete pListHeadSecondaryMaybe;
        pListHeadSecondaryMaybe = pNext;
    }

    wsprintfA(szIndexPathBuf + 1, "%sLayouts\\index.lay", g_pInstallPathPrefix);

    if (g_RFIndex.pFile != 0) {
        pRfBuf = g_RFIndex.LoadResource(
            (const unsigned char *)(szIndexPathBuf + 1 + strlen(g_pInstallPathPrefix)), &nRfSize);
        if (pRfBuf != 0) {
            pRfStream = new istrstream((char *)pRfBuf, nRfSize);
            pStream = pRfStream;
        }
    }
    if (pStream == 0) {
        pFileStream = new ifstream(szIndexPathBuf + 1, ios::nocreate | ios::binary);
        pStream = pFileStream;
    }
    if (pStream == 0) {
        throw "Failed to open stream to data";
    }
    if (pStream->rdstate() & (ios::eofbit | ios::badbit)) {
        throw "Invalid stream";
    }

    pBuf = new char[0x2000];
    memset(pBuf, 0, 0x2000);  // idiom-exempt: fixed-size scratch read buffer, matches its own new char[0x2000] alloc
    pStream->read(pBuf, 0x2000);
    int nBytesRead = pStream->gcount();

    int nBufOffset = 0;
    int nLineStart = 0;
    while (true) {
        char c;
        while (true) {
            if (nBytesRead <= nBufOffset) {
                goto cleanup;
            }
            c = pBuf[nBufOffset];
            if (c != '\n') break;
            nLineStart = nBufOffset + 1;
            nBufOffset++;
        }
        if ((c == '\r' || c == '\0') && 4 < (unsigned int)(nBufOffset - nLineStart)) {
            pBuf[nBufOffset] = '\0';
            ProviderListNode *pNode = new ProviderListNode;
            pNode->pNext = pListHeadSecondaryMaybe;
            pListHeadSecondaryMaybe = pNode;
            pNode->pszText = new char[0x100];
            strcpy(pNode->pszText, pBuf + nLineStart);
            nLineStart = nBufOffset + 1;
        }
        nBufOffset++;
    }

cleanup:
    if (pRfBuf != 0) {
        _free(pRfBuf);
    }
    if (pStream != 0) {
        delete pStream;
    }
    if (pBuf != 0) {
        delete[] pBuf;
    }
    if (bApplySelectionAfter != 0) {
        nUnk0xf4 = 0;
        ApplySecondaryListSelectionMaybe(0);
    }
}

// FUNCTION: LOCO 0x43d820 (LayoutSet::LayoutSet_InitFromConfigFileMaybe)
// Copies pConfigLine (the selected layout's name) into sessionName, builds
// "<installPathPrefix>Layouts\<sessionName>.lay", and loads it (RF archive via istrstream
// first, loose ifstream fallback) the same way LoadIndexFileMaybe loads index.lay. The file's
// first line is "count cols rows" (whitespace/newline-separated single digits, no bounds
// check on the leading scan -- sic, matches the original); `count` further lines each name one
// provider slot's long display name (CRLF-terminated, silently truncated to 32 bytes to fit
// DPlaySessionMgrProviderSlot::sLongName). count/cols/rows are then clamped to [.., 9]/[..,3]/
// [..,3], reset to the 9/3/3 default if cols*rows doesn't equal count, and further reset unless
// (cols, rows) is one of the 5 hardcoded valid combos for that count (2:2x1, 3:3x1, 4:2x2,
// 6:3x2, 9:3x3) -- mirrors the original's own switch-with-fallthrough validation cascade
// (Ghidra's own decompile renders it as nested ifs after the switch; kept in that shape here
// since it's the more literal translation of the jump-table fallthrough). Finally reloads the
// bitmap for any slot whose providerId is still 0 (freshly emptied/never-filled slots).
//
// PARKED (v302) -- structurally complete and verified against the raw disasm (stream fallback,
// header/line parsing, truncation clamp, validation cascade, catch(...) local-flag-then-cleanup
// shape via Catch@0043dba7 all confirmed). Same open toolchain-level SEH-scaffolding residual
// already parked on LoadIndexFileMaybe/Wav_ParseAndLoad: constructing the real istrstream/
// ifstream via `new` under this toolchain doesn't reproduce the original's richer EBP-framed,
// multi-entry SEH scope table. Unlike LoadIndexFileMaybe, this function DOES have a real local
// `catch (...)` (Catch@0043dba7 sets a flag then falls into the shared cleanup, matching the
// documented "dead parameter slot reused as EH scratch" pattern -- the flag lands in the high
// 3 bytes of the dead pConfigLine stack slot at [ebp+0xb]/[ebp+8]+3) -- not yet re-probed
// whether a real catch changes the SEH-shape gap (see docs/PARKED.md's LoadIndexFileMaybe entry
// for the untested "re-throw shim" idea, which doesn't apply here since a real catch already
// exists).
unsigned char DPlaySessionMgr::LayoutSet_InitFromConfigFileMaybe(char *pConfigLine) {
    void *pRfBuf = 0;
    istrstream *pRfStream = 0;
    ifstream *pFileStream = 0;
    istream *pStream = 0;
    char *pBuf = 0;
    int nRfSize = 0;
    unsigned char bSuccess = 0;
    char lineBuf[1285];  // byte 0 is never referenced, matching LoadIndexFileMaybe's own buffer

    nProviderSlotsPerRow = 3;
    nProviderSlotRows = 3;
    field_0x8 = 9;
    bLayoutSyncPingSent = 0;
    strcpy(sessionName, pConfigLine);

    wsprintfA(lineBuf + 1, "%sLayouts\\%s.lay", g_pInstallPathPrefix, sessionName);

    try {
        if (g_RFIndex.pFile != 0) {
            pRfBuf = g_RFIndex.LoadResource(
                (const unsigned char *)(lineBuf + 1 + strlen(g_pInstallPathPrefix)), &nRfSize);
            if (pRfBuf != 0) {
                pRfStream = new istrstream((char *)pRfBuf, nRfSize);
                pStream = pRfStream;
            }
        }
        if (pStream == 0) {
            pFileStream = new ifstream(lineBuf + 1, ios::nocreate | ios::binary);
            pStream = pFileStream;
        }
        if (pStream == 0) {
            throw "Failed to open stream to data";
        }
        if (pStream->rdstate() & (ios::eofbit | ios::badbit)) {
            throw "Invalid stream";
        }

        pBuf = new char[0x2000];
        memset(pBuf, 0, 0x2000);  // idiom-exempt: fixed-size scratch read buffer, matches its own new char[0x2000] alloc
        pStream->read(pBuf, 0x2000);

        int i = 0;
        while (pBuf[i] < '0' || '9' < pBuf[i]) {  // sic: no bounds check on the leading scan
            i++;
        }
        field_0x8 = pBuf[i] - '0';
        do {
            i++;
        } while (pBuf[i] < '0' || '9' < pBuf[i]);
        nProviderSlotsPerRow = pBuf[i] - '0';
        do {
            i++;
        } while (pBuf[i] < '0' || '9' < pBuf[i]);
        nProviderSlotRows = pBuf[i] - '0';
        do {
            i++;
        } while (pBuf[i] != '\n');

        int nLineStart = i + 2;
        for (int slot = 0; slot < field_0x8; slot++) {
            int j = nLineStart;
            while (pBuf[j] != '\r') {
                lineBuf[(j - nLineStart) + 1] = pBuf[j];
                j++;
            }
            lineBuf[(j - nLineStart) + 1] = 0;
            if (0x20 < strlen(lineBuf + 1)) {
                lineBuf[0x20] = 0;
            }
            strcpy(aProviderSlots[slot].sLongName, lineBuf + 1);
            nLineStart = j + 2;
        }

        if (9 < field_0x8) {
            field_0x8 = 9;
        }
        if (3 < nProviderSlotsPerRow) {
            nProviderSlotsPerRow = 3;
        }
        if (3 < nProviderSlotRows) {
            nProviderSlotRows = 3;
        }
        if (nProviderSlotsPerRow * nProviderSlotRows != field_0x8 &&
            field_0x8 <= nProviderSlotsPerRow * nProviderSlotRows) {
            field_0x8 = 9;
            nProviderSlotsPerRow = 3;
            nProviderSlotRows = 3;
        }

        switch (field_0x8) {
        case 2:
            if (nProviderSlotsPerRow == 2 && nProviderSlotRows == 1) goto layoutValid;
            break;
        case 3:
            break;
        case 4:
            goto case4Check;
        default:
            goto resetDefaults;
        case 6:
            goto case6Check;
        case 9:
            goto case9Check;
        }
        if (nProviderSlotsPerRow != 3 || nProviderSlotRows != 1) {
        case4Check:
            if (nProviderSlotsPerRow != 2 || nProviderSlotRows != 2) {
            case6Check:
                if (nProviderSlotsPerRow == 3) {
                    if (nProviderSlotRows == 2) goto layoutValid;
                case9Check:
                    if (nProviderSlotsPerRow == 3 && nProviderSlotRows == 3) goto layoutValid;
                }
            resetDefaults:
                field_0x8 = 9;
                nProviderSlotsPerRow = 3;
                nProviderSlotRows = 3;
            }
        }
    layoutValid:

        for (int slot2 = 0; slot2 < field_0x8; slot2++) {
            if (aProviderSlots[slot2].providerId == 0) {
                LayoutSet_LoadSlotBitmap(slot2);
            }
        }
        bSuccess = 1;
    } catch (...) {
        bSuccess = 0;
    }

    if (pRfBuf != 0) {
        _free(pRfBuf);
    }
    if (pStream != 0) {
        delete pStream;
    }
    if (pBuf != 0) {
        delete[] pBuf;
    }
    return bSuccess;
}

// --- LoadOrCreateEasterCard (0x43e900) TU-local decls -------------------------------
// NOTE: this whole block (decls + function) lives at the END of the TU on purpose --
// inserting it mid-file rotated SelectGridCellFromPointMaybe's codegen EXACT->DIFF(130)
// (this TU's documented position sensitivity, same artifact as the v323 include note).
// CRT-side helpers (real bodies in the CRT region; declared TU-locally with C++ linkage
// to avoid pulling GameNet.h/GNetManager.h into this position-sensitive TU -- relocs are
// masked by the matcher, so the mangled extern names cost nothing).
// Lazily loads g_pPostBagCache's aEasterNames[16][13] "easter.usr" reserved-name cache
// (real body in src/EditCardWnd.cpp).
extern void __fastcall PostBag_LoadEasterNameCache(PostBagCacheBundle *pCache); // 0x443260

// Methods-only view used to reproduce the original's `mov ecx,pResult` load in front of what
// is a callee-side plain __stdcall/this-ignoring body (same trick as NetSessionEventQueueEdge
// above; no data members, no layout to drift). The PostBag pair this used to sit beside
// (0x445620 / 0x444c70) is gone: both are real PostBagCacheBundle members declared in
// src/PostBag.h (reached here via CarNetState.h) and transcribed in src/EditCardWnd.cpp, so
// the view spelling was a byte-invisible wrong call target -- see tools/lint_alias.py.
struct CarNetStateEasterView0x43e900 {
    // Assigns the +0x94/+0x95 stamp slot/variant pair (kind 0 clears, 1/2/3 select the
    // variant and slot, -1 randomizes). A real CarNetState method (both callers pass a
    // CarNetState_CreateFromFile result as `this`).
    void AssignStampSlotVariantMaybe(int nKind, char nSlot); // 0x442bf0
};

// FUNCTION: LOCO 0x43e900
// PostBag Easter-card loader/creator keyed by a car's name (pCarState->nameA). Uniquifies the
// name against the 16-entry reserved-name table (g_pPostBagCache->aEasterNames; on a collision
// the name falls back to a decimal-index alias via _itoa), builds the
// "PostBag\Easter\<Lang>\<name>.crd"/".rsp" paths, then per retry: picks a RANDOM line out of
// each file (first line is a decimal count, truncated to 2 chars by the buf[2]=0 store;
// rand()/(0x7fff/count) selects the entry, the scan starts at offset 4 to skip the count
// line), strips '\r's past the chosen line, copies the .crd-side line (a card file name) onto
// the truncated directory path and loads that card via CreateFromFile, then expands the
// .rsp-side line (a template with "//"->'/', "/n"->CRLF, "/?"->local player name escapes)
// into the card's szDescription. Retry (up to 0x14 times) while the expanded description
// overflows 0x50 chars; finish with AssignStampSlotVariantMaybe(result,1,-1) (assigns the
// +0x94/+0x95 stamp slot/variant). Returns the loaded card, or NULL on any file/parse failure.
// A this-ignoring member (real signature __stdcall(pCarState), ret 0x4) -- defined as a member
// so the sole caller (RebuildOrEnqueueTrainCars) reproduces its ecx=this load.
//
// EFFECTIVE MATCH (v336): DIFF(1139), ours == orig 1405B (asmscore --len 1405: insns 461/456,
// align=294, reg_pen=14, byte_diff=173). Whole-function structure verified against the raw
// disasm (frame 0x8f98 identical, every local offset identical, both file-read blocks, the
// random-line picker, path surgery, escape expansion, retry loop). A minimal-TU probe (the
// function + decls alone) reproduces the IDENTICAL score, so the residual is intrinsic to
// this function's own /Og compilation, not TU context. Residual is FOUR stacked documented
// intrinsic /Og coin-flip classes: (1) the zero-register class in the prologue (orig
// dedicates ebx=0 for the nRead/bDone/nTry init stores; ours uses eax -- the v334/v335
// class); (2) slot-vs-register residency on the bCollided flag (orig stacks it at
// frame+0x12 with immediate stores and a reload-test; ours keeps it in bl -- the v335 (3)
// class; declaration-order, bool-vs-char, assignment-order and != 0 probes ALL NO EFFECT).
// This one cascades: with bl owned by the flag, ours spills pCarName to memory and gives
// pResult to edx, where orig keeps pCarName in ebx through the name loop and pResult in
// ebx afterward (~half the dump's rows are this cascade, including the '/?'
// branch's ebx reload from frame+0x20); (3) the loop-entry-guard rotation class on all
// THREE do-while loops with a top break (the nTry guard, both newline-scan loops, the
// escape loop): this compile normalizes each to a bottom-condition form (`jae exit; test; jne
// back`) while the original keeps the single `jb` back-edge -- rewriting the outer loop as
// an equivalent while() produced BYTE-IDENTICAL output, proving the rotation is compiler
// normalization, not source shape (the v329 class); (4) small reassociation residue on the
// path-surgery index (orig `mov edx,len1; mov eax,-4; sub eax,len2; add edx,eax` + immediate
// zero store vs ours `mov edx,-4; sub edx,len2; add edx,len1` + al store -- #29/#30 family;
// the named nDirLen temp already closed the earlier pointer-folded form). Levers that DID
// close (kept): szExpanded declared [0x320] not [0x50] (the frame pins the real declared
// size 0x320; 0x50 is only the logical cap), dual pSrc/pNext pointers in the escape loop
// (orig maintains both in lockstep), for(;;)+two-breaks pinning of the newline-scan loop
// bodies, unsigned i for the name-table loop (jb not jl), DWORD nRead without (int) casts
// (unsigned jb/jae throughout), TU-local view structs reproducing the dead
// ecx=g_pPostBagCache loads before the __stdcall BuildEasterCardPath/CreateFromFile calls
// and the ecx=pResult load before AssignStampSlotVariantMaybe, strcpy-then-bDone order in
// the final branch (keeps the two strcpy expansions unmerged). Retry only if the
// slot-vs-register residency class or the loop-entry-guard rotation class cracks --
// (2)+(3) are the bulk of the residual. See docs/PARKED.md.
CarNetState *DPlaySessionMgr::LoadOrCreateEasterCard(CarNetState *pCarState) {
    char szLine1[12];
    char szLine2[80];
    char szExpanded[0x320];  // logical cap is 0x50 (the bound used everywhere below), but the
                             // frame places the next buffer 0x320 bytes later -- the declared
                             // size is oversized, not the cap
    char szName[0x200] = "";
    char szCrdPath[0x504];
    char szRspPath[0x504];
    char buf[0x8000];
    DWORD nRead;
    HANDLE hFile;
    char *pCarName;
    char *pSrc;
    char *pNext;
    CarNetState *pResult;
    bool bCollided, bDone;
    int nTry, j, nCount, nPick, nOut;
    unsigned int i;
    char c, cNext;

    nRead = 0;
    bDone = 0;
    nTry = 0;
    PostBag_LoadEasterNameCache(g_pPostBagCache);
    while (nTry < 0x14) {
        bCollided = 0;
        szName[0] = 0;
        pCarName = pCarState->nameA;
        for (i = 0; i < 16; i++) {
            if (strcmp(pCarName, g_pPostBagCache->aEasterNames[i]) == 0) {
                bCollided = 1;
                _itoa(i + 1, szName, 10);
            }
            if (bCollided) {
                break;
            }
        }
        g_pPostBagCache->PostBag_BuildEasterCardPath(szName, 1, szCrdPath);
        g_pPostBagCache->PostBag_BuildEasterCardPath(szName, 0, szRspPath);
        hFile = CreateFileA(szCrdPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return NULL;
        }
        if (ReadFile(hFile, buf, 0x8000, &nRead, NULL) == 0) {
            CloseHandle(hFile);
            return NULL;
        }
        CloseHandle(hFile);
        buf[2] = 0;
        nCount = atoi(buf);
        nPick = rand() / (0x7fff / nCount);
        i = 4;
        if (nRead > 4) {
            for (;;) {
                if (nPick == 0) {
                    break;
                }
                if (buf[i] == '\n') {
                    nPick--;
                }
                i++;
                if (i >= nRead) {
                    break;
                }
            }
        }
        for (j = i + 1; j < nRead; j++) {
            if (buf[j] == '\r') {
                buf[j] = 0;
            }
        }
        strcpy(szLine1, buf + i);
        hFile = CreateFileA(szRspPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return NULL;
        }
        if (ReadFile(hFile, buf, 0x8000, &nRead, NULL) == 0) {
            CloseHandle(hFile);
            return NULL;
        }
        CloseHandle(hFile);
        buf[2] = 0;
        nCount = atoi(buf);
        nPick = rand() / (0x7fff / nCount);
        i = 4;
        if (nRead > 4) {
            for (;;) {
                if (nPick == 0) {
                    break;
                }
                if (buf[i] == '\n') {
                    nPick--;
                }
                i++;
                if (i >= nRead) {
                    break;
                }
            }
        }
        for (j = i + 1; j < nRead; j++) {
            if (buf[j] == '\r') {
                buf[j] = 0;
            }
        }
        strcpy(szLine2, buf + i);
        int nDirLen = strlen(szCrdPath) - strlen(szName) - 4;
        szCrdPath[nDirLen] = 0;
        strcat(szCrdPath, szLine1);
        strcat(szCrdPath, ".crd");
        pResult = g_pPostBagCache->CarNetState_CreateFromFile(szCrdPath);
        if (pResult == NULL) {
            return NULL;
        }
        szExpanded[0] = 0;
        strcpy(pResult->nameB, pCarName);
        pResult->wAttachmentId = 0;
        pResult->bAttachmentSoundPlayedMaybe = 1;
        nOut = 0;
        pSrc = szLine2;
        pNext = szLine2 + 1;
        do {
            c = *pSrc;
            if (c == 0) {
                szExpanded[nOut] = 0;
                break;
            }
            if (c == '/') {
                cNext = *pNext;
                if (cNext == '/') {
                    szExpanded[nOut] = 0x2f;
                    pNext += 2;
                    pSrc += 2;
                } else if (cNext == 'n') {
                    szExpanded[nOut] = 0xd;
                    szExpanded[nOut + 1] = 0xa;
                    nOut++;
                    pNext += 2;
                    pSrc += 2;
                } else if (cNext == '?') {
                    szExpanded[nOut] = 0;
                    strcat(szExpanded, g_pLocalPlayerIdentity->name);
                    nOut = strlen(szExpanded);
                    szExpanded[nOut] = ' ';
                    pNext += 2;
                    pSrc += 2;
                } else {
                    pNext += 1;
                    pSrc += 1;
                }
            } else {
                szExpanded[nOut] = c;
                pNext += 1;
                pSrc += 1;
            }
            nOut++;
        } while (nOut < 0x50);
        if (strlen(szExpanded) < 0x50) {
            strcpy(pResult->szDescription, szExpanded);
            bDone = 1;
        } else {
            szExpanded[0x4f] = 0;
            strcpy(pResult->szDescription, szExpanded);
        }
        nTry++;
        if (bDone != 0) {
            break;
        }
    }
    ((CarNetStateEasterView0x43e900 *)pResult)->AssignStampSlotVariantMaybe(1, -1);
    return pResult;
}


// The net-shutdown gate, as a byte-returning inline predicate: the `unsigned char` return is
// LOAD-BEARING -- it is what reproduces the original's `xor r,r; cmp; sete rl; test rl,rl`
// materialization instead of a plain `cmp; je` (v356 lever, docs/CODEGEN.md). Kept TU-local
// (src/GameNet.cpp carries its own copy) because declarations in the shared GameNetMsgQueue.h
// rotate the TUs that include it.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x43f140
// EFFECTIVE MATCH (asmscore --len 358: total 153871, align=150 reg_pen=32 byte_diff=111,
// insns 136/129). The body is instruction-for-instruction aligned with the original; the ENTIRE
// residual is one ebx<->ebp symmetric-register-swap (Yoda #29/#30): the original holds the zero
// constant in ebx and pNode in ebp, this compile does the reverse. That single coin-flip also
// explains the 4-byte length excess -- with the zero in ebx (the first-pushed callee-saved reg)
// the original SHRINK-WRAPS, pushing only ebx/esi before the shutdown-vs-enqueue branch and
// edi/ebp inside the shutdown arm, so both enqueue epilogues pop 2 registers where ours pop 4.
// Probed without any effect on the score: C-style declarations hoisted to the top of the
// function, an explicit if/else instead of the early return, and moving the definition from the
// end of the TU to its address-order slot before 0x43f2b0 (that last one costs an unrelated
// EXACT elsewhere in the TU -- do not repeat it).
//
// Three source levers DID land here and are the reusable part (see docs/CODEGEN.md):
//  (1) case-body ORDER. VC5 numbers a jump-table switch's blocks by ASCENDING CASE VALUE (which
//      is why the raw table has six ids for four bodies -- each label in a `case A: case B:`
//      group gets its own id) but LAYS THE BODIES OUT IN SOURCE ORDER. Writing the cases
//      descending (0x15/0x17, 0xf/0x11, 2, default) leaves the tables byte-identical and fixes
//      the layout: 345831 -> 177833. Do NOT split a grouped label pair into two bodies to chase
//      the six ids -- VC5 does not tail-merge them, and it costs 11 instructions (364050).
//  (2) re-reading `pNode->pPayload` inside case 2 and the 0xf/0x11 delete, instead of reusing
//      the outer `pPayload` local. VC5 CSEs the LOAD but not the null-ness, so the original's
//      redundant `cmp; je` guards reappear; using the local elides them.
//  (3) the case-2 walk written as `pCur = pSub; pSub = pSub->pNext; ... delete pCur;` (advance
//      at the TOP) rather than a trailing `pSub = pNextSub;`, which needs an extra register copy
//      at the loop bottom.
//
// Sibling of GameNetMsgQueue::EnqueueOrFreeNode (0x4393d0) for the LOCAL/inbound queue
// (g_pNetMsgLocalQueueHead): while the net subsystem is up, append the node at the tail under
// the queue lock; once it is shutting down nobody will ever drain the queue again, so the node
// is disposed of inline instead -- each payload kind by its own allocator (type 2 = a
// singly-linked list of owned sub-records, 0xf/0x11 = a polymorphic object released through its
// virtual dtor, 0x15/0x17 = a Win32 process-heap block, anything else = a plain operator new
// buffer). `this` is unused (see the declaration's note).
void DPlaySessionMgr::GameNetMsgQueue_EnqueueOrProcessLocalNode(NetMsgQueueNode *pNode) {
    if (IsNetShuttingDownMaybe()) {
        void *pPayload = pNode->pPayload;
        if (pPayload != 0) {
            switch (pNode->type) {
            case 0x15:
            case 0x17:
                HeapFree(GetProcessHeap(), 0, pPayload);
                break;
            case 0xf:
            case 0x11:
                delete (NetMsgPayloadObjMaybe *)pNode->pPayload;
                break;
            case 2: {
                NetMsgType2PayloadNode *pSub = (NetMsgType2PayloadNode *)pNode->pPayload;
                while (pSub != 0) {
                    NetMsgType2PayloadNode *pCur = pSub;
                    pSub = pSub->pNext;
                    if (pCur->pSubPayload != 0) {
                        operator delete(pCur->pSubPayload);
                        pCur->pSubPayload = 0;
                    }
                    operator delete(pCur);
                }
                break;
            }
            default:
                operator delete(pPayload);
                break;
            }
            pNode->pPayload = 0;
        }
        operator delete(pNode);
        return;
    }
    pNode->pNext = 0;
    g_pGameNetMsgQueueLock->Lock();
    NetMsgQueueNode *pTail = g_pNetMsgLocalQueueHead;
    if (pTail != 0) {
        NetMsgQueueNode *pCur = pTail->pNext;
        while (pCur != 0) {
            pTail = pCur;
            pCur = pTail->pNext;
        }
        pTail->pNext = pNode;
        g_pGameNetMsgQueueLock->Unlock();
        return;
    }
    g_pNetMsgLocalQueueHead = pNode;
    g_pGameNetMsgQueueLock->Unlock();
}

// ⚠ PLACEMENT IS LOAD-BEARING -- these two must stay at the END of this TU. This file aggregates
// several of the original .objs, so source position here is our own convention, not ground truth;
// but putting the pair in address order (right after ReconcileCarHandoff, ~line 1537) costs
// `ApplSetupWnd::SelectGridCellFromPointMaybe` (0x40aba0, ~1200 lines further down) its EXACT
// match -- 166 B -> DIFF(130) at 170 B, identical code reshuffled. Measured both ways. This is the
// same function src/NetSetupWnd.cpp's `??_GNetSettings` autopsy found to be hypersensitive; there
// it was declaration visibility, here it is plain source position, so the sensitivity is the
// function's own, not any one lever's. (v471: 0x40aba0 is EXACT again after an unrelated
// src/DSoundChannel.h change rotated this TU. The placement rule above was measured at the OLD
// TU state and has NOT been re-measured at the new one -- keep the pair here regardless.)
//
// FUNCTION: LOCO 0x440a80
// Top up an arriving train's empty car slots from the player's Sort\Out card queue, then hide the
// outbox UI once it is empty. Two things have to be true first: at least one occupied car slot has
// NO live CarNetState (i.e. it is an empty wagon), and at least one car is a hand-off socket
// (kind 0x1870/0x1871) -- a train with neither is not something cards can be loaded onto. When they
// are, it walks the category-2 (Sort\Out) .crd file list and, for each file while empty slots
// remain, builds a CarNetState from it, deletes the file, drops the state into the first slot that
// has none, and re-tags every category-4 car to 0x1871 so the newly-loaded wagon shows as coupled.
// Finally, if the recount says Sort\Out is now empty, the session UI mode drops out of the
// "outbox has cards" state (3 -> 2, anything else -> 0).
//
// EFFECTIVE MATCH -- 473 B vs 476 B, insns 154/155, total 19016 (align 18, byte_diff 26). We are
// exactly ONE instruction short, and it is the same fact throughout: the original RELOADS
// `pCar->pKindDesc` for the second half of the `== 0x1870 || == 0x1871` test
// (`mov esi,[esi+0x40]` at 0x440ae2) where cl 11.00 common-subexpression-eliminates it and keeps
// the first load alive in eax -- which then forces the whole first ternary to compute into ecx
// instead of destroying eax, and every register in the pair rotates from there. Nothing in
// between the two reads stores anything, so there is no source-level aliasing lever. Two shapes
// were measured: this `||` form (19016) and splitting it into `if (...) {...} else if (...) {...}`
// (WORSE -- 467 B, total 33xxx: cl then sinks the whole second test past the flag store). The
// remaining single `[esp+0x24]`-vs-`[esp+0x20]` store in the loop tail is the knock-on of that
// one-instruction offset, not an independent disagreement. v501 #53 probe: subscripting the
// carSlots retag loop is byte-IDENTICAL to the `ppCar++` walk (DIFF(320) both ways) --
// #53-neutral here, so the transcribed pointer form stays.
//
void DPlaySessionMgr::LoadOutboxCardsIntoTrain(PeerTrainNodePartial *pNode) {
    short nEmptyCars = 0;
    char bHasSocketCar = 0;

    if (g_pPostBagCache->PostBag_GetCategoryFileCountCached() == 0)
        return;

    short i = 1;
    if ((int)pNode->wCarSlotCount >= (int)i) {
        do {
            if (CarNetObj_GetAppliedState(pNode->carSlots[i]) == 0)
                nEmptyCars++;
            CarNetObj *pCar = (CarNetObj *)pNode->carSlots[i];
            if ((pCar->pKindDesc == 0 ? -1 : pCar->pKindDesc->resourceId) == 0x1870 ||
                (pCar->pKindDesc == 0 ? -1 : pCar->pKindDesc->resourceId) == 0x1871) {
                bHasSocketCar = 1;
            }
            i++;
        } while ((int)i <= (int)pNode->wCarSlotCount);
    }

    if (nEmptyCars == 0 || !bHasSocketCar)
        return;

    PostBagCrdFileNode *pFile = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(2, 0);
    while (pFile != 0 && nEmptyCars > 0) {
        CarNetState *pState = g_pPostBagCache->CarNetState_CreateFromFile(pFile->szPath);
        g_pPostBagCache->DeleteCardFileAndRefreshCount(pFile->szPath);

        short j = 1;
        while (CarNetObj_GetAppliedState(pNode->carSlots[j]) != 0)
            j++;

        pState->AssignStampSlotVariantMaybe(1, -1);
        ((CarNetObj *)pNode->carSlots[j])->CarNetObj_ApplyNetState(pState);
        if (pState != 0)
            delete pState;

        unsigned int k = 0;
        CarNetObj **ppCar = (CarNetObj **)&pNode->carSlots[0];
        do {
            if ((*ppCar)->nCarCategory == 4 && (*ppCar)->nSpawnDescriptorIdMaybe != 0x1871) {
                int arg = (*ppCar)->nAnimValueCache;
                (*ppCar)->SetCarTypeAndCategory(0x1871, -1);
                (*ppCar)->SetStateArgMaybe(arg, 1);
            }
            k++;
            ppCar++;
        } while (k <= pNode->wCarSlotCount);

        nEmptyCars--;
        PostBagCrdFileNode *pNext = pFile->pNext;
        operator delete(pFile);
        pFile = pNext;
    }

    if (g_pPostBagCache->PostBag_RecountCategoryOutFiles() == 0) {
        if (field_0x800 == 3) {
            SetUiModeAndNotifyWidgets(2);
        } else {
            SetUiModeAndNotifyWidgets(0);
        }
    }
}

// FUNCTION: LOCO 0x440a50
// Adopt a train node as the session's current one: reconcile its car hand-off state, load any
// queued outbox cards into it (skipped for a node that is about to be discarded outright), and
// remember it in pCurrentTrainNodeMaybe.
void DPlaySessionMgr::AcceptIncomingTrainNodeMaybe(PeerTrainNodePartial *pNode) {
    ReconcileCarHandoff(pNode);
    if (pNode->nDiscardFlag != 1)
        LoadOutboxCardsIntoTrain(pNode);
    pCurrentTrainNodeMaybe = pNode;
}
