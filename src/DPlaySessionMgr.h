#pragma once

struct PeerTrainNodePartial; // fwd -- pending-train-queue node (src/PeerTrainNode.h)
class CarNetState;      // fwd -- per-car net/identity state (src/CarNetState.h)
struct Pair16;          // fwd -- (x,y)/owner pair passed by value (src/Pair16.h)

// Minimal partial view of the DPlaySessionMgr multiplayer-session-manager singleton
// (g_pDPlaySessionMgr -- the real class is already modeled in Ghidra at 2052 bytes with a rich set of
// provider-slot/roster/timeout fields; that full struct is far out of scope here). Only
// connectionMode (+0x7c4) and the 4 "does a connected peer have room in this heading"
// dispatch wrappers are needed by known callers so far.
//
// The 4 wrappers (0x43de00/0x43de10/0x43ddf0/0x43de20) all forward `this` unchanged into
// DPlaySessionMgr::HasProviderSlotRoomInHeading (0x43de30, extern -- not transcribed here,
// see its own real signature/behavior in docs/subsystems.md). Modeled as thiscall members
// (byte-identical to Ghidra's own displayed __fastcall-with-explicit-this-arg rendering -- a
// single register-passed pointer compiles the same either way) per the already-matched
// src/phase2_probe2.cpp precedent (formerly the placeholder class `DispatchObj0x43de30`,
// moved here and renamed now that the real identity is confirmed).
// One entry of DPlaySessionMgr::aProviderSlots[9] (0x4c bytes) -- docs/subsystems.md's own
// "DPlaySessionMgrProviderSlot" section has the full field set. The +0x5..+0x35 span, once
// modeled as a single sAddressOrName[49] blob, is really three distinct fields the roster
// pack/unpack/copy helpers (GameNet_Unpack/PackRosterRecord, CopyFrom -- src/DPlaySessionMgr.cpp)
// touch separately: a 13-byte short name (+0x5, strcpy'd), a 32-byte long name (+0x12, strcpy'd),
// and a 4-byte tail dword (+0x32). The +0x37..+0x47 runtime-only fields (results-chain link,
// layout blob) are populated by other code paths, not the roster message, so kept as sized pad.
struct GameNetRosterWireRecord;  // fwd -- roster wire record (below)
struct NetMsgQueueNode;          // fwd -- send/local queue node (src/GameNetMsgQueue.h)

// Wire payload for opcode 0x3f8 ("request current layout list"): a 6-byte broadcast carrying
// the local provider-slot index. LayoutNet_RequestLayoutList sends it (queue type 6),
// immediately followed by a bare type-0x19 queue command with no payload.
struct LayoutReqWireMsg {
    unsigned short wOpcode;              // +0x0  -- 0x3f8
    unsigned short pad0x2;               // +0x2  -- alignment gap, never written (sent as heap garbage)
    unsigned char bProviderIndex;  // +0x4  -- (byte)selectedProviderIndex
};  // sizeof 6

// Wire payload for opcode 0x3f0 ("request a provider-slot select", the LayoutSet family's own
// client->host notify): the target provider slot index, the local player's own name, and a copy
// of the board's cols/rows. Built by ApplSetupWndPartial::SendSelectRequestMaybe (0x40ac50,
// queue type 6). The receiving side (src/GameNet.cpp's opcode dispatcher case 6, GameNet.h's
// own `LayoutNameWireMsg`) reads the SAME wire bytes back as `nReliable` (+0x4 -- really this
// slot index; NetMsgQueueNode's own bReliable/eventTrainId union alias makes both names
// correct for the same 4 bytes once the received node reaches GameNetManager_HandleQueuedEvent's
// case 4) and bOwnerA/bOwnerB (+0x16/+0x18, the low byte of each word here -- board dims never
// exceed 255, so the high byte is simply unused, not a mismatch).
struct SelectRequestWireMsg {
    unsigned short wOpcode;   // +0x0  -- 0x3f0
    unsigned short pad0x2;    // +0x2  -- alignment gap, never written (sent as heap garbage)
    int nTargetSlot;          // +0x4  -- requested provider slot index
    char szName[13];          // +0x8  -- local player's name, NUL-terminated (strcpy'd)
    char pad0x15;              // +0x15 -- alignment gap, never written (sent as heap garbage)
    unsigned short wCols;      // +0x16 -- g_worldBoard.wCols (only the low byte is read back)
    unsigned short wRows;      // +0x18 -- g_worldBoard.wRows (only the low byte is read back)
    char pad0x1a[2];           // +0x1a -- alignment gap, never written (sent as heap garbage)
};  // sizeof 0x1c

// Wire payload for opcode 0x3f7 ("a local train left its origin slot"): the train's id plus its
// two owner bytes. Built by GameNet_BroadcastLocalOrigin (0x440410). Default alignment
// already lands the int at +0x4 (a 2-byte gap after the opcode, left unset like the sibling
// LayoutReqWireMsg), so no packing is needed; sizeof 0xc matches the operator new(0xc).
struct TrainOriginWireMsg {
    unsigned short wOpcode;       // +0x0  -- 0x3f7
    unsigned short pad0x2;        // +0x2  -- alignment gap, never written (sent as heap garbage)
    int nTrainId;                 // +0x4  -- trainId
    unsigned char bOwnerByteA;   // +0x8  -- owning player id
    unsigned char bOwnerByteB;   // +0x9  -- provider slot key (low byte of bOwnerB)
};  // sizeof 0xc

// One packed per-train roster record inside an opcode-0x3f6 broadcast (8 bytes): the low words of
// a placement-result node's trainId/x/y plus its two owner bytes. Built by
// GameNet_BroadcastRosterTick (0x43ded0) from each GameNetRosterResultNode in the selected
// slot's chain.
struct RosterTickRecord {
    unsigned short wTrainId;    // +0x0  (unsigned short)node->trainId
    unsigned short wPosX;       // +0x2  (unsigned short)node->posX
    unsigned short wPosY;       // +0x4  (unsigned short)node->posY
    unsigned char bOwnerA;      // +0x6  node->bOwnerByteA
    unsigned char bSlotKey;     // +0x7  node->bSlotKeyMaybe
};  // sizeof 8

// Wire payload for opcode 0x3f6 (a roster/placement tick broadcast): a small header followed by one
// 8-byte RosterTickRecord per placement-result node. The producer indexes the records from +0x9
// (records[wCount-1] after pre-incrementing wCount), so the array is byte-packed to sit unaligned
// there; the buffer is over-allocated (operator new(0x8000)) since the node count is unbounded.
#pragma pack(push, 1)
struct RosterTickWireMsg {
    unsigned short wOpcode;         // +0x0  -- 0x3f6
    unsigned char pad0x2[2];        // +0x2  -- left unset
    unsigned char bProviderIndex;   // +0x4  -- (byte)selectedProviderIndex
    unsigned char pad0x5;           // +0x5  -- left unset
    unsigned short wCount;          // +0x6  -- record count (pre-incremented per node)
    unsigned char bConst1;          // +0x8  -- constant 1
    RosterTickRecord records[1];    // +0x9  -- one per node (unaligned, indexed 1-based)
};
#pragma pack(pop)

// Wire payload for opcode 0x3f9 (a stored/current layout bitmap): a 0x14-byte header followed
// by the raw 8bpp bitmap bytes. Built by LayoutNet_ReplyWithStoredLayout (0x43d520) and
// LayoutNet_SendCurrentLayoutBitmap (0x43d350); the buffer is operator new'd as
// (bitmapByteCount + 0x28) so the header + payload fit with slack. Variable length, so no
// fixed sizeof is used (the allocation size is computed by hand). Byte-packed so the +0x6
// cols/rows pair can be modeled as a dword-union without alignment padding: the
// SendCurrentLayoutBitmap producer stores the two shorts, then re-reads them as ONE dword
// (mov ecx,[msg+6]) to stash the packed cols/rows into the local provider slot's +0x40 field.
#pragma pack(push, 1)
struct LayoutBitmapWireMsg {
    unsigned short wOpcode;          // +0x0  -- 0x3f9
    unsigned char pad0x2[4];         // +0x2  -- header bytes left unset by the producers
    union {                          // +0x6
        unsigned int dwColsRows;  // +0x6  -- cols|rows packed, re-read as one dword
        struct {
            short wCols;             // +0x6  -- bitmap width  (slot's layout cols)
            short wRows;             // +0x8  -- bitmap height (slot's layout rows)
        };
    };
    unsigned char pad0xa[2];         // +0xa
    unsigned int dwLayoutVersion;     // +0xc  <- slot +0x48
    unsigned int nPixelCount;        // +0x10 = wCols * wRows
    unsigned char data[1];           // +0x14 -- 8bpp bitmap bytes (variable length)
};
#pragma pack(pop)

// A roster/placement-result node: a per-provider-slot linked-list entry
// (DPlaySessionMgrProviderSlot::pResultsChainHead is the head, `next` @ +0x10). The full
// record is modeled in Ghidra (id / x @ +0x4 / y @ +0x8 / ownerByteA @ +0xc / ownerByteB @
// +0xd -- see SetTrainPlacementResult in docs/subsystems.md); only the next link is
// needed here, where ~DPlaySessionMgr frees each slot's chain node-by-node.
struct GameNetRosterResultNode {
    int trainId;                  // +0x0
    int posX;                // +0x4
    int posY;                // +0x8
    unsigned char bOwnerA;   // +0xc  -- owning player id (also a valid slot index)
    unsigned char bSlotKey;  // +0xd  -- the provider slot this node is filed under (ownerB)
    unsigned char pad0xe[2];      // +0xe
    GameNetRosterResultNode *pNext;  // +0x10
};

// Both records are byte-packed: the slot's dwTailAlias sits at the 2-unaligned
// offset +0x32 (proving no compiler padding), and sizeof stays 0x4c so
// DPlaySessionMgr::aProviderSlots[9] keeps its 0x4c stride.
#pragma pack(push, 1)
struct DPlaySessionMgrProviderSlot {
    // 0x4426d0 -- decode a 60-byte wire record into this slot
    void GameNet_UnpackRosterRecord(const GameNetRosterWireRecord *wire);
    // 0x442750 -- copy another slot's roster fields into this one
    void CopyFrom(const DPlaySessionMgrProviderSlot *src);

    unsigned int providerId;    // +0x0
    unsigned char bDirty;  // +0x4
    char sAddressOrName[13];     // +0x5  -- short/display name (copied into EditCardWnd roster)
    char sLongName[32];          // +0x12 -- long name/address span
    // +0x32 -- a 4-byte span the roster pack/unpack/copy helpers move as one dword
    // (dwTailAlias), but ResetProviders overwrites as two 16-bit board-grid dims copied
    // from g_worldBoard -- a genuine byte-level alias, so modeled as a union.
    union {
        unsigned int dwTailAlias;  // +0x32
        struct {
            unsigned short wCols;  // +0x32 -- WorldBoard grid columns copy
            unsigned short wRows;  // +0x34 -- WorldBoard grid rows copy
        };
    };
    unsigned char bEnabled;  // +0x36
    unsigned char pad0x37;        // +0x37 -- alignment pad (never accessed anywhere)
    GameNetRosterResultNode *pResultsChainHead;  // +0x38 -- per-slot result-node list head
    unsigned int nLayoutDataSize;  // +0x3c
    // +0x40 -- byte-level alias like dwTailAlias: the wire producer
    // (GameNetManager_HandleQueuedEvent case 0x16, not yet transcribed) reads/writes it as
    // one 4-byte dword (dwLayoutDims), but ResetProviders zeroes it as two separate 16-bit
    // stores -- modeled as a union. (Corrects the refuted "RESOLVED 2026-07-12: genuinely one
    // dword, the 2x16-bit stores are codegen noise" claim -- a single `unsigned int = 0`
    // compiles to ONE dword store, off by 1 insn from ResetProviders' byte-exact two-word form.)
    union {
        unsigned int dwLayoutDims;  // +0x40
        struct {
            short wLayoutCols;  // +0x40 -- stored layout bitmap width (signed: LoadSlotBitmap
                                     //          sign-extends it (movsx) in the cols*rows product)
            short wLayoutRows;  // +0x42 -- stored layout bitmap height
        };
    };
    void *pLayoutData;       // +0x44 -- owned layout blob (operator delete'd on teardown)
    unsigned int dwLayoutVersion;  // +0x48
};

// The 60-byte wire record (network opcode 0x3f1 payload, 9 records/packet) that
// GameNet_UnpackRosterRecord decodes into a DPlaySessionMgrProviderSlot and
// GameNet_PackRosterRecord encodes back out. Field order differs from the slot's --
// the two string spans sit adjacent (+0xc/+0x19) with the scalar fields packed ahead.
struct GameNetRosterWireRecord {
    // 0x4427d0 -- encode a slot's roster fields into this wire record
    void GameNet_PackRosterRecord(const DPlaySessionMgrProviderSlot *slot);

    unsigned int providerId;      // +0x0  -> slot +0x0
    unsigned int dwTailAlias;  // +0x4  -> slot +0x32
    unsigned int dwLayoutVersion;  // +0x8  -> slot +0x48
    char sAddressOrName[13];       // +0xc  -> slot +0x5  (short name)
    char sLongName[32];            // +0x19 -> slot +0x12 (long name)
    unsigned char bEnabled;  // +0x39 -> slot +0x36
    unsigned char bDirty;    // +0x3a -> slot +0x4
    unsigned char pad0x3b;         // +0x3b (trailing byte, unmapped)
};
#pragma pack(pop)

// The session-manager singleton is a polymorphic class: its ctor (0x43d0a0) stores a
// vtable pointer at +0x0 (0x4781c8, a 16+-slot table whose slot 0 is the ??_G scalar
// deleting destructor at 0x43d110), so it is modeled here with a `virtual ~DPlaySessionMgr()`
// -- the compiler then synthesizes the +0x0 vtable-store in the ctor and the ??_G scalar
// dtor as byproducts. The vtable is NOT a WindowBase table (the ctor calls no base ctor);
// the full slot layout is still unmodeled (only the virtual dtor is declared so far).
// A queued network object (a PeerTrainNode or sibling): polymorphic (vtable @ +0x0), chained
// via +0x70, and destroyed through its virtual scalar-deleting destructor. ~DPlaySessionMgr
// walks two such lists (field_0x7dc and pPendingTrainQueueHead), `delete`-ing each node --
// the dtor is declared-only (extern) so `delete` emits the original's vtbl[0](1) call.
struct GameNetQueuedNodeMaybe {
    virtual ~GameNetQueuedNodeMaybe();  // +0x0 vtable
    char pad0x4[0x70 - 4];              // +0x4
    GameNetQueuedNodeMaybe *pNext;      // +0x70
};

class DPlaySessionMgr {
public:
    DPlaySessionMgr();  // 0x43d0a0
    virtual ~DPlaySessionMgr();  // 0x43dc30 (+ ??_G scalar dtor 0x43d110) -- byte-matched
    void ResetProviders(char bInit);  // 0x43d130
    bool HasProviderSlotRoomInHeading(int heading, int index);  // 0x43de30
    int ResolveIdToSlot(int providerId);  // 0x43d230
    DPlaySessionMgrProviderSlot *GetSelectedProvider();  // 0x43d210
    void SetMode(int mode);  // 0x43d2b0
    bool IsType0x10e();  // 0x43de00
    bool IsType0();       // 0x43de10 -- naming placeholder, purpose unclear  // TODO: sync
    bool IsType0x5a();    // 0x43ddf0
    bool IsType0xb4();    // 0x43de20
    // Post a simple 4-byte layout-net opcode (0x3fa) to the send queue for one peer. A this-ignoring
    // thiscall (Ghidra models 0x43d250 __stdcall in the LayoutNet namespace, last component matches);
    // declared as a member so callers reproduce the ecx=this load at the call site.
    void LayoutNet_PostSimpleOpcode(int destPlayerId);  // 0x43d250
    void LayoutNet_RequestLayoutList();  // 0x43d620
    void LayoutNet_ReplyWithStoredLayout(int destPlayerId);  // 0x43d520
    void LayoutNet_SendCurrentLayoutBitmap(int destPlayerId);  // 0x43d350
    void LayoutSet_LoadSlotBitmap(int slotIndex);  // 0x43d6c0
    // Try to place a popped pending-train node onto the board; returns 0 on failure (the drain
    // then re-prepends the node to retry next gate). Declared-only (extern), see docs/subsystems.md.
    char AttemptQueuedTrainPlacement(PeerTrainNodePartial *pNode);  // 0x43e1d0
    void HandleQueuedTrainPlacement(NetMsgQueueNode *pMsg);  // 0x43e370
    // Handles inbound placement events 0x12/0x15/0x17 (dispatched together by
    // GameNetManager_HandleQueuedEvent): 0x12 = place one train resolved from a heading;
    // 0x15 = apply a full roster-tick payload (clear + re-file a slot's result chain); 0x17 =
    // remove one train's placement result. 0x15/0x17 own a heap wire payload freed via HeapFree.
    void HandleQueuedPlacementEvent(NetMsgQueueNode *pMsg);  // 0x440150
    // The inbound-queue event dispatcher (~18-case jump-table switch over the drained node's opcode
    // `type`). pMsg is &node->type, i.e. the node reinterpreted as its leading int field (cast back
    // to NetMsgQueueNode* inside). The case bodies are laid out in SOURCE order, which the transcription
    // orders to match the .text body layout recovered from the jump table (9/4/2/0xb/0xc/5/3/...).
    void GameNetManager_HandleQueuedEvent(int *pMsg);  // 0x43f2b0

    // Declared-only leaf handlers invoked by GameNetManager_HandleQueuedEvent's cases. All take
    // DPlaySessionMgr `this` in ecx (Ghidra namespaces the FUN_/GameNet_ ones under GameNet -- the
    // last-component names match); bodies are UI/setup-entangled and not transcribed here.
    // Apply an inbound provider-slot snapshot (event type 9): copy each of the 9 slots from the
    // peer's snapshot, broadcast our own slot's enable/disable transitions, reload/refresh bitmaps
    // for newly-emptied or changed slots, reply with the stored layout if ours changed, then repaint.
    void ApplyProviderSnapshot(NetMsgQueueNode *pMsg);  // 0x43fc50 -- event type 9
    // Assign/relocate a provider into a roster slot (event type 4): find it by id (or by name when
    // providerId==-1), then place it in targetSlot (or the first free slot), retarget the selection,
    // clear the old slot, and pack+broadcast the roster if a connect is pending.
    void AssignProviderToSlot(int providerId, int targetSlot, unsigned char *pName,
                                   Pair16 owners);  // 0x43fe30 -- event type 4
    // Parses a "provider config" line (a Layouts\index.lay entry naming one layout) and loads
    // that layout's own "Layouts\<name>.lay" file (RF archive first, loose ifstream fallback):
    // a "count cols rows\r\n" header followed by `count` provider-slot display names, one
    // per line. Reloads any now-empty slot's bitmap and returns whether the whole load
    // succeeded (0 on any thrown error, caught locally). See src/DPlaySessionMgr.cpp.
    unsigned char LayoutSet_InitFromConfigFileMaybe(char *pConfigLine);  // 0x43d820
    void RemovePeerTrainsAndSlot(int destPlayerId); // 0x43f940 -- event type 0xb
    void HandleQueuedTrainConnect(NetMsgQueueNode *pMsg);  // 0x43e2e0 -- event type 0xf
    void ReleaseOwnerTrainsAndBroadcast(NetMsgQueueNode *pMsg);  // 0x43fb50 -- event type 0x1a
    // Bounds-guarded provider-slot accessor. Implicitly inline (in-class body), and it is the
    // source of the "dead signed-index guard" the raw disasm shows all over this subsystem: at
    // every inlined site VC5 emits the `i < 0` test and a NULL result even though the caller
    // masks the index to 0..255 first, so the guard can never fire. Written `? :` rather than an
    // `if` because the compiler if-converts it to the branchless `setl; dec; and` form inside
    // loops (see FindTrainPlacementResult's third loop) -- both spellings of the same accessor.
    DPlaySessionMgrProviderSlot *ProviderSlotAt(int i) {
        return i >= 0 ? &aProviderSlots[i] : 0;
    }
    // Roster placement-result node management (host-only, connectionMode==2). Each result node
    // records a train's placement (id, x/y, owner) filed onto aProviderSlots[bOwnerB].pResultsChain.
    GameNetRosterResultNode *FindTrainPlacementResult(int trainId, unsigned char bOwnerA,
        unsigned char bOwnerB);  // 0x440750
    // bOwnerB is a BYTE, not a dword: every call site pushes it with garbage upper bits
    // (`mov dl,[..]; push edx` -- no zero-extension), and the body itself only ever uses its low
    // byte. Pinned by PeerTrainNode's own ctor, whose call site is otherwise instruction-exact.
    void SetTrainPlacementResult(int trainId, unsigned char bOwnerA, unsigned char bOwnerB,
        int x, int y);  // 0x440610
    void RemoveTrainPlacementResult(int trainId, unsigned char bOwnerA, unsigned char bOwnerB);  // 0x4404c0
    // Broadcast opcode 0x3f7 (a local train left its origin slot) then remove its placement-result
    // node. Only when connected, and only broadcasts when the departing slot is our own local one.
    void GameNet_BroadcastLocalOrigin(int trainId, unsigned char bOwnerA, unsigned char bOwnerB);  // 0x440410
    // Gate for a per-train move/release request. Rebuilds the train's car composition, then
    // either releases (delete), defers (enqueue onto pPendingTrainQueueHead), or forwards the
    // move by connection mode. Returns 1 always.
    char RequestTrainMoveOrReleaseNode(int fromSlot, int toSlot, PeerTrainNodePartial *pNode);  // 0x43e560
    // Re-derive pNode's car slots; if any were rebuilt, spawn a replacement train node and enqueue
    // it. Returns true only if no car was discarded AND still connecting (mode 1).
    char RebuildOrEnqueueTrainCars(PeerTrainNodePartial *pNode);  // 0x43e690
    // Reconcile a train node's car composition: release any locally-owned cars to the PostBag,
    // then sync every car's hand-off socket state (0x1870<->0x1871), then notify the UI widgets.
    void ReconcileCarHandoff(PeerTrainNodePartial *pNode);  // 0x4408b0
    // 0x440a80 -- top up pNode's empty car slots from the Sort\Out card queue; see
    // src/DPlaySessionMgr.cpp.
    void LoadOutboxCardsIntoTrain(PeerTrainNodePartial *pNode);  // 0x440a80
    // 0x440a50 -- ReconcileCarHandoff + LoadOutboxCardsIntoTrain, then adopt pNode as current.
    void AcceptIncomingTrainNodeMaybe(PeerTrainNodePartial *pNode);  // 0x440a50
    // Set the session's UI mode (this+0x800) and drive the mode change into the game-window widget
    // list (vtbl+0x1c per matching widget). Declared-only; body 0x440820 untranscribed.
    void SetUiModeAndNotifyWidgets(int mode);  // 0x440820
    // PostBag Easter-card loader/creator keyed by a car's name (transcribed in
    // DPlaySessionMgr.cpp, EFFECTIVE-parked -- body 0x43e900 is a large ~0x8f98-frame file
    // loader). A this-ignoring thiscall (Ghidra namespaces it here too);
    // invoked as a member so RebuildOrEnqueueTrainCars reproduces the ecx=this load.
    CarNetState *LoadOrCreateEasterCard(CarNetState *pCarState);  // 0x43e900
    // Post an opcode "move this train" request to the connected host (defined in DPlaySessionMgr.cpp;
    // a this-ignoring thiscall member -- see the definition's EFFECTIVE-match note).
    char TrainNet_PostMoveRequestForNode(int fromSlot, int toSlot, PeerTrainNodePartial *pNode);  // 0x43eec0
    // Producer for send-queue type 0xe (connect-or-join for a specific train peer). A this-ignoring
    // thiscall (Ghidra models 0x43ee80 __stdcall in the GameNet namespace, last component matches);
    // invoked as a member so the sole caller reproduces the ecx=this load.
    char GameNet_PostConnectOrJoinForNode(PeerTrainNodePartial *pTrainNode);  // 0x43ee80
    // enqueue/process a node on the local (inbound) queue. A thiscall method of the
    // session-manager singleton whose body ignores `this` (every call site still loads
    // ecx=g_pDPlaySessionMgr); declared here so `g_pDPlaySessionMgr->...()` reproduces the ecx-load + call.
    // (Ghidra models 0x43f140 in the GameNet namespace; last-component name matches.)
    void GameNetMsgQueue_EnqueueOrProcessLocalNode(NetMsgQueueNode *pNode);  // 0x43f140

    // vtable pointer implicit @ +0x0 (compiler-synthesized; deliberately omitted here)
    // One-shot latch: fires opcode 0x3fa (LayoutNet_PostSimpleOpcode(0)) exactly once per
    // session lifetime, the first time ApplyProviderSnapshot finishes applying a full
    // provider-slot roster snapshot; reset to 0 by ResetProviders (ctor/session reset).
    unsigned char bLayoutSyncPingSent;  // +0x4
    unsigned char pad0x5[3];           // +0x5
    int field_0x8;                     // +0x8  -- set to 9 by ResetProviders; copied into a
                                       //          send-node's bReliable field by PostReset
    int nProviderSlotsPerRow;          // +0xc  -- grid width (set to 3 by ResetProviders)
    int nProviderSlotRows;             // +0x10 -- grid height (set to 3 by ResetProviders)
    char sessionName[32];         // +0x14 -- session name buffer (strcpy'd "Default")
    char pad0x34[0x518 - 0x34];        // +0x34
    DPlaySessionMgrProviderSlot aProviderSlots[9];  // +0x518
    int connectionMode;  // +0x7c4
    bool bConnectPending;  // +0x7c8
    unsigned char pad0x7c9[3];  // +0x7c9
    DPlaySessionMgrProviderSlot *pSelectedProvider;  // +0x7cc
    int selectedProviderIndex;  // +0x7d0
    int searchProviderId;  // +0x7d4 -- provider id SetMode's mode-2 scan looks up
    int queueSourceId;     // +0x7d8 -- snapshot of GameNetThreadState::dpidCurrentPlayer taken
                                //          when a connect (event type 3) starts; cleared on teardown (5)
    GameNetQueuedNodeMaybe *field_0x7dc;  // +0x7dc -- queued-node list head (dtor deletes each)
    GameNetQueuedNodeMaybe *pPendingTrainQueueHead;  // +0x7e0 -- pending train-peer queue head
    // +0x7e4 -- the train node most recently adopted by AcceptIncomingTrainNodeMaybe (0x440a50);
    // only ever written (ctor clears it, that one function stores into it), never read so far.
    PeerTrainNodePartial *pCurrentTrainNodeMaybe;
    int nNextTrainId;       // +0x7e8
    int nDispatchTick;     // +0x7ec
    int stateTimeoutMs;    // +0x7f0 -- per-mode poll/timeout interval (500 for mode 1,
                                //          20 for mode 2)
    int rosterBroadcastTick;  // +0x7f4
    int nRosterBroadcastPeriodTicks;        // +0x7f8 -- ctor inits to 0xf
    int timeoutMsMaybe;         // +0x7fc -- ctor inits to 0x960
    int field_0x800;            // +0x800
};

// The multiplayer-session-manager singleton pointer (also externed locally in
// src/NetSessionEventQueue.cpp / src/EditCardWnd.cpp -- kept a raw DAT_ auto-name per the
// struct-instance Hungarian wall). Used here for the local-queue enqueue path.
extern DPlaySessionMgr *g_pDPlaySessionMgr;
