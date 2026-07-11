#pragma once

// GameNetThreadState -- the GameNet worker-thread context singleton (0x4fd3a4), given its own
// header so consumers that only need to POST to the send queue (src/DPlaySessionMgr.cpp's
// GameNet_Post*/LayoutNet_Post* helpers) can reach the class without dragging in the whole
// msg-0x3ea..0x3fd wire-record set in src/GameNet.h. Measured, not stylistic: including all of
// GameNet.h in src/DPlaySessionMgr.cpp rotates ApplSetupWndPartial::SelectGridCellFromPointMaybe
// (0x40aba0) off its byte match, the documented VC5 "declarations rotate codegen" effect
// (v340/v355/v356). Every member here is a pointer, so plain forward declarations suffice.
//
// It is the `this` threaded through GameNetThread_TickLoop (0x439240) -> GameNet_DispatchMessage
// (0x4396c0) -> the GameNet_* inbound-message handlers in src/GameNet.cpp; it owns the peer/train
// linked lists, the in-progress file transfers, AND the outbound send queue. Distinct from the
// DPlaySessionMgr singleton (g_pDPlaySessionMgr).
//
// sizeof 0x38, pinned by `new_alloc(0x38)` at 0x422855 (SplashWnd's net-startup path); only the
// fields the current consumers touch are typed (extend in place). The three PeerTrainNode list
// heads are reinterpreted PeerTrainNodePartial* (real nodes are the full PeerTrainNode; the
// partial covers the fields the GameNet handlers touch: pNext @ +0x70, wTrainId @ +0x7a,
// bHasDetailFlagMaybe @ +0x88).

#include <windows.h>  // HWND (hwndOwner)

struct NetMsgQueueNode;          // src/GameNetMsgQueue.h -- send/local queue node
struct PeerTrainNodePartial;     // src/PeerTrainNode.h
struct FileTransferNode;         // src/GameNet.h
struct TrainStateWireMsg;        // src/GameNet.h -- the wire records below all live there too
struct TrainRotateWireMsg;
struct FileRequestWireMsg;
struct FileBlockWireMsg;
struct UiConnectWireMsg;
struct PlayerRosterWireMsg;
struct TrainSyncWireMsg;
class CarNetState;               // src/CarNetState.h

struct GameNetThreadState {
    // The vtable at PTR_FUN_004781c4. Modeled as a real virtual dtor (rather than the manual
    // `void *pVtbl` member this carried until 2026-07-27) so `delete g_pGameNetThreadState`
    // dispatches through slot 0 the way AppWindow::SaveWindowAndCleanExit's own teardown does;
    // the compiler's implicit vptr occupies exactly the +0x0 slot the manual member did, so the
    // layout is unchanged. Body 0x438cc0 transcribed in src/GameNet.cpp (EFFECTIVE-parked v507,
    // docs/PARKED.md -- the send-queue drain carries a coupled coin-flip residual).
    virtual ~GameNetThreadState();
    void *hInstance;                   // +0x4  -- owning app instance (ctor arg); forwarded
                                       //          straight into GNetManager's own ctor
    HWND hwndOwner;                    // +0x8  -- owning window (ctor arg); becomes
                                       //          g_pNetManager->hWndParent
    unsigned char bSkipConnectMaybe;  // +0xc  -- thread-stop / "connected" gate
    unsigned char bShutdownRequestedMaybe;   // +0xd  -- reentrancy gate: DispatchMessage runs only if 0
    char pad0xe[0x10 - 0xe];            // +0xe
    int dpidCurrentPlayer;         // +0x10 -- self/target player DPID (reset to 1 when local player leaves)
    PeerTrainNodePartial *pTrainListActive;       // +0x14 -- active/pending trains (drained as notify type 0xf)
    PeerTrainNodePartial *pTrainListAwaitingAck;  // +0x18 -- sync-sent, awaiting ack (drained as notify type 0x11)
    PeerTrainNodePartial *pTrainListRehomed;      // +0x1c -- removed/rehomed trains; RemoveOrRehomeNode moves nodes here or to +0x14
    int nTickCounter;              // +0x20 -- tick/frame counter
    int nTrainAdvanceInterval;     // +0x24 -- advance cadence (ctor-init 0x14)
    FileTransferNode *pOutboundTransfers;    // +0x28 -- outbound file-transfer node list
    FileTransferNode *pInboundTransfers;     // +0x2c -- inbound file-transfer/per-player records
    unsigned char bSessionStateFlagMaybe;    // +0x30 -- set =1 in DispatchMessage state-rotate case
    char pad0x31[0x34 - 0x31];          // +0x31
    int nPendingFileReceiveCount;  // +0x34 -- pending clipart-file receive count (0 => teardown)

    // 0x438bc0 -- the real constructor: seeds the two context
    // fields above from the owning SplashWnd's own WindowBase::hInstance/hwndOwner. Reached only
    // via `new GameNetThreadState(...)` in SplashWnd::StartGameNetThread. Transcribed in
    // src/GameNet.cpp (EFFECTIVE-parked v518, docs/PARKED.md -- one COM-loop register rotation;
    // bracketed in `#pragma inline_depth(0)` so its ProbeComPort calls stay real calls, which is
    // also why this TU emits the out-of-line ProbeComPort COMDAT 0x45ee60).
    GameNetThreadState(void *hInstanceParam, HWND hwndOwnerParam);

    // The outbound send-queue aspect of the same object: the GameNet_Post*/LayoutNet_Post* free
    // helpers in src/DPlaySessionMgr.cpp build a NetMsgQueueNode and hand it to EnqueueOrFreeNode,
    // which links it onto g_pNetMsgSendQueueHead (or disposes of it when the net subsystem is
    // shutting down). StopThreadAndWait posts the type-8 shutdown command and spin-waits for the
    // worker thread to exit. Both transcribed in src/GameNet.cpp.
    void EnqueueOrFreeNode(NetMsgQueueNode *pNode);  // 0x4393d0
    void StopThreadAndWait();                        // 0x4394e0

    // Inbound-message handlers -- all __thiscall on the manager (this=ecx). Ghidra boxes them in
    // a plain `GameNet` namespace with a void* this and the GameNet_ prefix; modeled here as real
    // members so the compiler emits thiscall (multi-arg handlers pass extra args on the stack, not
    // edx -- the difference vs __fastcall only shows up past the first argument).
    void GameNetThread_ResetNetManager();             // 0x4391a0
    void GameNet_DrainPeerListAsNotify();                  // 0x43cbe0
    void GameNet_DrainBlockedTrainListAsNotify();          // 0x43cc40
    void GameNet_RemoveOrRehomeNode(unsigned int nSlot);   // 0x43a6d0
    void GameNet_HandleTrainStateAck(TrainStateWireMsg *pMsg, int nAckPlayerId);  // 0x43b6d0
    void GameNet_HandleSelfStateRotate(TrainRotateWireMsg *pMsg, int nReliable);  // 0x43a4b0
    void GameNet_HandlePlayerLeft(int nPlayerId);          // 0x43a5c0
    void GameNet_RemovePeerTrainsForPlayer(int nPlayerId); // 0x43b770
    char GameNet_TeardownAndFlushQueues();                 // 0x43ac10
    void GameNet_BeginFileTransfer(FileRequestWireMsg *pMsg, unsigned int nRequesterId);  // 0x439d00
    void GameNet_HandleFileTransferBlock(FileBlockWireMsg *pWire);  // 0x43a140
    void TrainNet_HandleMoveRequest(NetMsgQueueNode *pNode);  // 0x43ad00
    void GameNet_DispatchMessage();                        // 0x4396c0 -- the opcode dispatcher (this TU)

    void DPlay_UiConnectHandler(UiConnectWireMsg *pWire);         // 0x43c860 (msg 0x3eb) -- this TU

    void GameNet_ReceiveRosterSnapshot(PlayerRosterWireMsg *pWire);  // 0x43ce10 (msg 0x3ec) -- this TU

    void GameNet_HandleTrainStateSync(TrainSyncWireMsg *pWire, int nSenderId);  // 0x43b240 (msg 0x3f2) -- this TU

    // Declared-only opcode-dispatch callees (bodies out of this TU / not yet transcribed; all
    // thiscall on the manager). Only ever reached FROM GameNet_DispatchMessage.
    void GameNet_BroadcastPlayerRoster();                          // 0x43ccc0

    // Per-car resource-appearance pull: gated on connectionMode==1, walks a 128-slot appearance-ID
    // array, sends 6-byte opcode-0x3ed requests for unresolvable ids. Ghidra boxes this as a free
    // `NetResource_` function with an explicit void* this, but it's a real 2-arg __thiscall (this +
    // one stack arg) -- modeled here as a member per the same-family convention (CLAUDE.md) so the
    // call site reproduces thiscall codegen. Declared-only (858-byte body, not transcribed).
    void NetResource_RequestMissingAppearances(CarNetState *pState);  // 0x438e40

    // Declared-only tick-loop callees (bodies out of this TU / not yet transcribed; all thiscall
    // on the manager). GameNetThread_TickLoop drives them once per background-thread tick.
    void GameNet_ProcessLocalCommand(NetMsgQueueNode *pNode);  // 0x439550 -- transcribed, this TU
    void TrainNet_AdvanceLocalTrainSteps();                // 0x43bb00
    void NetFile_PumpPendingTransferSend();                // 0x439df0

    // Declared-only helpers (bodies out of this TU; masked-reloc calls). Both thiscall on the
    // manager. SendTrainStateSync tries to hand a train off to the peer owning the target board
    // slot; TrainNet_HandleEmptySlotHandoffMaybe handles the target-slot-empty case. Ghidra boxes them in GameNet::/TrainNet::.
    char GameNet_SendTrainStateSync(unsigned int nProviderId, PeerTrainNodePartial *pTrain,
                                         int nHeading, int nFlag);  // 0x43ae20
    void TrainNet_HandleEmptySlotHandoffMaybe(PeerTrainNodePartial *pTrain, int nHeading, int nSlot);  // 0x43b8c0

    // Board-edge helper (body 0x43c160, src/GameNet.cpp -- transcribed 2026-07-27, EFFECTIVE),
    // called once per train per tick by TrainNet_AdvanceLocalTrainSteps: when the train's current tile sits on a board edge,
    // checks the neighboring provider slot in that heading for room (HasProviderSlotRoomInHeading)
    // and hands the train off (GameNet_SendTrainStateSync, or TrainNet_HandleEmptySlotHandoffMaybe
    // when the target slot is empty), returning 1 so the caller stops the tick; otherwise reflects
    // wLocalHeading off the blocked edge and returns 0.
    char TrainNet_TryBoardEdgeHandoffMaybe(PeerTrainNodePartial *pPrev, PeerTrainNodePartial *pTrain,
                                           int x, int y, int cols, int rows);  // 0x43c160

    // Helpers reached from GameNet_ProcessLocalCommand only, thiscall on the manager, each also
    // taking pNode as an explicit stack arg -- confirmed via each real call site's `ret 0x4`
    // cleanup PLUS a `push esi(=pNode)` immediately before `mov ecx,edi(=this); call`, the
    // "caller forwards an arg the callee's own (stale) prototype hid" tell (CLAUDE.md).
    // ConnectOrJoinSession's param is really a NetMsgQueueNode* too (its own body reads
    // *(pNode+8), the pPayload offset) -- transcribed, this TU.
    void GameNet_ConnectOrJoinSession(NetMsgQueueNode *pNode);  // 0x43c410 -- transcribed, this TU
    // Returns true only when a session was actually joined/hosted (the epilogues are a literal
    // `mov al,1` / `xor al,al` -- a bool, not the `void` this was declared as while untranscribed).
    // Every known call site ignores the result.
    bool AttemptJoinOrHostSession();                        // 0x43aa00 -- transcribed, this TU
    void DPlay_PrepareInternetConnection(NetMsgQueueNode *pNode);  // 0x43a760 -- transcribed, this TU
    void DPlay_BuildOtherSessionsList(NetMsgQueueNode *pNode);  // 0x43a8b0 -- transcribed, this TU
};

// Created by SplashWnd's net-startup path (0x422820:
// `GameNetThread_InitState(new_alloc(0x38), ...)`), torn down by SaveWindowAndCleanExit. Every
// GameNet_Post*/LayoutNet_Post* producer reaches the send queue through it.
//
// (Was ALSO modeled as a separate `class GameNetMsgQueue` in src/GameNetMsgQueue.h until v378 --
// two partial views of ONE object, with bDiscardMessages@0xd == bShutdownRequestedMaybe@0xd and
// nQueueSourceId@0x10 == dpidCurrentPlayer@0x10. Merged here per the never-duplicate-a-struct
// rule; the duplicate was also a latent symbol bug, since MSVC mangles a global's C++ type into
// its name -- the two declarations named DIFFERENT symbols.)
extern GameNetThreadState *g_pGameNetThreadState;  // 0x4fd3a4

// The GameNet worker thread itself (posts/pumps DirectPlay in the background), created alongside
// the state object by SplashWnd::StartGameNetThread and handed GameNetThread_TickLoop as its entry
// point. StopThreadAndWait above enqueues a type-8 shutdown command and spin-waits on it.
struct ThreadWrapper;                              // src/ThreadWrapper.h
extern ThreadWrapper *g_pGameNetThread;            // 0x4fd398
void GameNetThread_TickLoop(GameNetThreadState *pState);  // 0x439240 -- src/GameNet.cpp
