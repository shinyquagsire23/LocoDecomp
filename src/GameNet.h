#pragma once

// GameNetThreadState -- the GameNet worker-thread context object: the `this` threaded
// through GameNetThread_TickLoop (0x439240) -> GameNet_DispatchMessage (0x4396c0) -> the
// GameNet_* inbound-message handlers in src/GameNet.cpp. It owns the peer/train linked lists
// and the in-progress file transfers. Distinct from the DPlaySessionMgr singleton (g_pDPlaySessionMgr)
// and from the GameNetMsgQueue send-queue aspect (g_pGameNetThreadState @ 0x4fd3a4).
//
// Real Ghidra struct is 0x38 bytes; only the fields the current consumers touch are typed here
// (extend in place as more are discovered). The three PeerTrainNode list heads are reinterpreted
// PeerTrainNodePartial* (real nodes are the full PeerTrainNode; the partial covers the fields the
// GameNet handlers touch: pNext @ +0x70, wTrainId @ +0x7a, bHasDetailFlagMaybe @ +0x88).

#include "PeerTrainNode.h"       // PeerTrainNodePartial
#include "ThreadWrapper.h"    // ThreadWrapper (g_pGameNetThread worker-thread handle)
#include "DPlaySessionMgr.h"     // GameNetRosterWireRecord, DPlaySessionMgrProviderSlot
#include "CarNetState.h"    // CarNetState -- embedded by value in TrainSyncCarRecordSend

struct NetMsgQueueNode;          // fwd -- send/local queue node (src/GameNetMsgQueue.h)

// A file-transfer list node (0x1c bytes). ONE struct serves BOTH the outbound list (manager +0x28,
// GameNet_BeginFileTransfer, OPENs a local .att clipart file GENERIC_READ) AND the inbound list
// (manager +0x2c, pInboundTransfers: GameNet_HandleFileTransferBlock receives peer file blocks,
// GameNet_HandlePlayerLeft cleans up a leaving player's record). Plain POD (no ctor/dtor): `new`
// allocates it raw and every field is stored by hand. (Was two divergent partial views --
// FileTransferNode + PlayerRecordNodeMaybe -- consolidated v257 per the never-duplicate rule.)
struct FileTransferNode {
    int dwPeerId;              // +0x0  -- peer DPID (outbound: requester we send to;
                                    //          inbound: owning player, matched by HandlePlayerLeft)
    unsigned short wAttId;    // +0x4  -- file id (BuildAttFilePath key)
    unsigned short wXferId;    // +0x6  -- secondary id/key (inbound search key vs wire +8)
    unsigned char bOwnerByteA;     // +0x8  -- (inbound) low byte copied into notify +0x14
    char pad0x9[1];                 // +0x9
    unsigned short wTrainId;    // +0xa  -- (inbound) copied (zero-extended) into notify +0x10
    HANDLE hFile;                   // +0xc  -- open file handle (INVALID/0 => node freed/closed)
    unsigned char bBlockStage;   // +0x10
    char pad0x11[1];                // +0x11
    unsigned short blockCount;      // +0x12 -- blocks sent/received (sequence counter)
    unsigned char bCooldownTicks;   // +0x14
    char pad0x15[0x18 - 0x15];      // +0x15
    FileTransferNode *pNext;   // +0x18 -- singly-linked list chain
};

// Inbound "peer requested a file from us" wire message (opcode 0x11 / msg 0x3fb). Only the two
// id words the transfer setup reads are modeled; the opcode occupies the first 4 bytes.
struct FileRequestWireMsg {
    char pad0x0[4];                 // +0x0 -- packet header/opcode
    unsigned short wAttId;    // +0x4 -- file id (also the BuildAttFilePath lookup key)
    unsigned short wXferId;    // +0x6 -- secondary id/key
};

// Inbound msg 0x3ee ("here's a pushed clipart file", world/resource push to a joining client): a
// PostBag category/index byte pair identifying the local cache path, then a raw data block.
struct ClipartFileWireMsg {
    char pad0x0[4];              // +0x0 -- packet header/opcode
    unsigned char bDescByte;     // +0x4
    unsigned char bIndexByte;    // +0x5
    unsigned short pad0x6;       // +0x6
    unsigned int nDataLen;       // +0x8 -- WriteFile length
    char data[1];                // +0xc -- payload bytes (variable length nDataLen)
};

// Inbound file-transfer data block (opcode 0x12 / msg 0x3fc): one chunk of a clipart .att/.dat file
// a peer is streaming to us. GameNet_HandleFileTransferBlock matches it to an inbound transfer node
// by wXferId and writes the payload (blockType 0=first block/create file, 1=interim, 2=final).
// Also used to BUILD the outbound send (NetFile_PumpPendingTransferSend, new_alloc'd/uninitialized):
// only wOpcode is written there -- wPad ships as uninitialized heap garbage. sic: word store, not
// dword (same idiom as GameNet_TeardownAndFlushQueues's "leaving" 0x3fd message).
struct FileBlockWireMsg {
    unsigned short wOpcode;         // +0x0 -- packet header/opcode (0x3fc)
    unsigned short wPad;            // +0x2
    unsigned int nDataLen;          // +0x4 -- payload byte count (WriteFile length)
    unsigned short wXferId;    // +0x8 -- transfer key (matched against node wXferId)
    unsigned short wBlockSeq;  // +0xa -- expected running block-sequence count
    unsigned char bBlockType;       // +0xc -- 0=first, 1=interim, >=2=final
    char data[1];                   // +0xd -- payload bytes (variable length nDataLen)
};

// Outbound msg 0x3ed ("I don't have this clipart locally, please push it to me") -- the REQUEST
// half of the 0x3ee ClipartFileWireMsg push above, built one-per-missing-appearance by
// NetResource_RequestMissingAppearances. sic: the two selector bytes are in the OPPOSITE order
// to ClipartFileWireMsg's (index at +0x4, category/desc at +0x5), and wPad ships as
// uninitialized heap garbage -- same word-store-then-two-byte-stores idiom as FileBlockWireMsg's
// own outbound build above. See docs/engine-bugs.md.
struct ClipartRequestWireMsg {
    unsigned short wOpcode;         // +0x0 -- packet header/opcode (0x3ed)
    unsigned short wPad;            // +0x2 -- never written; sent as heap garbage
    unsigned char bIndexByte;       // +0x4 -- DecalSlot::placementSeq / the stamp slot byte
    unsigned char bDescByte;        // +0x5 -- DecalSlot::packedKind / the packed stamp category
};

// Function-local dedup list node (12 bytes, raw `new`/`delete`, no ctor) built and torn down
// entirely inside NetResource_RequestMissingAppearances: the set of DISTINCT
// (packedKind, placementSeq) pairs found across the card's 128 decal slots. The 6 bytes between
// the pair and the chain pointer are never touched.
struct MissingClipartNode {
    unsigned char bDescByte;        // +0x0 -- DecalSlot::packedKind
    unsigned char bIndexByte;       // +0x1 -- DecalSlot::placementSeq
    char pad0x2[6];                 // +0x2
    MissingClipartNode *pNext;      // +0x8
};




// Inbound train-state wire message (the payload of DirectPlay train-sync/ack packets). Only the
// fields the current handlers read are modeled; the header/opcode occupies the first 4 bytes.
struct TrainStateWireMsg {
    char pad0x0[4];                 // +0x0 -- packet header/opcode
    unsigned short wTrainId;   // +0x4 -- train id (matched against PeerTrainNode +0x7a)
    unsigned char bOwnerByteA;     // +0x6 -- source provider slot (matched against +0x78)
    unsigned char bOwnerByteB;     // +0x7 -- secondary owner byte (copied into +0x7c / notify +0x15)
    unsigned short wCount;     // +0x8 -- car/state count (copied into notify +0x4)
};

// One 936-byte per-car record of the msg 0x3f2 train-state-sync wire format (below). The first
// 0x10 bytes are consumed directly (AllocCarSlot args + detail flag); when bHasDetailFlagMaybe is
// set, the remaining bytes are copied field-by-field into a CarNetState (src/CarNetState.h)
// -- the "UnkNN" field names here mirror the DESTINATION CarNetState field each one feeds
// (see the offset comments). sic: only 20 of nameB's 21 bytes are ever copied (the record's own
// nameB[20] is the real read span) -- CarNetState::nameB's 21st byte is left uninitialized.
struct TrainSyncCarRecord {
    unsigned int nCarTypeId;        // +0x0  -- PeerTrainNode_AllocCarSlot arg1 (kind)
    unsigned int nCarCategory;         // +0x4  -- PeerTrainNode_AllocCarSlot arg2 (flag)
    unsigned char bHasDetail;  // +0x8  -- 0 = alloc-only record, no CarNetState payload
    char pad0x9[0x10 - 0x9];            // +0x9
    unsigned short wSignature;          // +0x10 -> CarNetState::wSignature
    char pad0x12[0x14 - 0x12];          // +0x12
    unsigned int ownerClientId;    // +0x14 -> CarNetState::ownerClientId
    unsigned int nPostSeqId;            // +0x18 -> CarNetState::nPostSeqId
    char nameA[21];                     // +0x1c -> CarNetState::nameA (full 21B copy)
    char nameB[21];                     // +0x31 -> CarNetState::nameB (only first 20B copied)
    unsigned short wAttachmentId;       // +0x46 -> CarNetState::wAttachmentId
    unsigned int bAttachmentSoundPlayedMaybe; // +0x48 -> CarNetState::bAttachmentSoundPlayedMaybe
    unsigned char byIdentityColorR;     // +0x4c -> CarNetState::byIdentityColorR
    unsigned char byIdentityColorG;     // +0x4d -> CarNetState::byIdentityColorG
    unsigned char byIdentityColorB;     // +0x4e -> CarNetState::byIdentityColorB
    char szDescription[80];             // +0x4f -> CarNetState::szDescription
    unsigned char byStampSlotB;         // +0x9f -> CarNetState::byStampSlotB
    unsigned char byStampSlotA;         // +0xa0 -> CarNetState::byStampSlotA
    unsigned char byStampVariantA;      // +0xa1 -> CarNetState::byStampVariantA
    unsigned char decalSlotsRaw[0x3a2 - 0xa2];  // +0xa2 -> CarNetState::decalSlots (768B raw copy)
    char pad0x3a2[0x3a4 - 0x3a2];       // +0x3a2
    unsigned int UnkTrailingMaybe;      // +0x3a4 -> CarNetState::Unk0x398 (both ends genuinely
                                       //   write-only/dead -- no promotion evidence found)
};                                       // total 0x3a8 = 936 bytes stride

// Inbound msg 0x3f2 ("train state sync", send side: GameNet_SendTrainStateSync): a fixed
// train-identity header, then up to 4 reserved TrainSyncCarRecord slots (matching
// PeerTrainNode's own 4 physical car slots) -- the trailing 4th slot's space doubles as a
// NUL-terminated name string for the engine (car slot 0) when fewer than 4 records are sent
// (see GameNet_HandleTrainStateSync's SetNameImpl call, always at the fixed +0xb10 offset).
struct TrainSyncWireMsg {
    char pad0x0[4];                     // +0x0  -- packet header/opcode
    unsigned short wHeading;       // +0x4
    unsigned short wTrainId;       // +0x6
    unsigned short wSelectedCar;   // +0x8  -- PeerTrainNode_UpdateSelectedCar arg
    unsigned char bOwnerByteA;     // +0xa
    char pad0xb[1];                     // +0xb
    unsigned int dwReversed;       // +0xc
    int nKindId;                   // +0x10 -- PeerTrainNodePartial ctor arg1
    unsigned char bCarCount;       // +0x14
    char pad0x15[0x18 - 0x15];          // +0x15
    TrainSyncCarRecord records[4]; // +0x18 -- 4 reserved slots (0x3a8 each)
};

// One 936-byte per-car record as actually BUILT by the msg 0x3f2 send side
// (GameNet_SendTrainStateSync) -- confirmed via the ctor/dtor wrapper pair at
// 0x43b220/0x43b230 (`this+0xc` tail-jmp straight into CarNetState's own real ctor/dtor,
// 0x442850/0x442a00): the detail payload is a REAL embedded CarNetState object (placement-
// constructed), not a field-by-field copy like the receive side's scratch-local unpack
// (TrainSyncCarRecord above). Same overall 0x3a8 stride (0xc header + 0x39c
// sizeof(CarNetState)), just reached differently on this side of the wire.
struct TrainSyncCarRecordSend {
    unsigned int nCarTypeId;    // +0x0  -- CarNetObj_GetCarTypeId(car)
    unsigned int nCarCategory;     // +0x4  -- CarNetObj::nCarCategory
    unsigned char bHasDetail; // +0x8 -- 0 = alloc-only record, no CarNetState payload
    char pad0x9[0xc - 0x9];         // +0x9
    CarNetState state;         // +0xc, sizeof 0x39c -> total 0x3a8
};

// The msg 0x3f2 SEND-side counterpart of TrainSyncWireMsg -- a fixed 0xb1c-byte allocation
// (new_alloc(0xb1c) at the call site) covering the same 0x18-byte identity header (field names
// mirror TrainSyncWireMsg's own), exactly 3 detail-record slots (car slots 1..3; slot 0, the
// locomotive, is folded into the header's nKindId/szName instead), and a trailing
// fixed 12-byte name buffer (NOT a 4th record slot -- the receive side's "4th slot doubles as
// name" comment describes how the RECEIVER'S wider TrainSyncWireMsg accepts this shorter
// message, not this struct's own layout). 0x18 + 3*0x3a8 + 0xc = 0xb1c, confirmed exact.
struct TrainSyncWireMsgSend {
    unsigned short wOpcode;              // +0x0  -- 0x3f2
    unsigned short wPad;                 // +0x2  -- zeroed (unlike most wire messages' wPad, sic)
    unsigned short wHeading;        // +0x4
    unsigned short wTrainId;        // +0x6
    unsigned short wSelectedCar;    // +0x8
    unsigned char bOwnerByteA;      // +0xa
    char pad0xb[1];                      // +0xb
    unsigned int dwReversed;        // +0xc
    int nKindId;                    // +0x10 -- locomotive (car slot 0)'s own type id
    unsigned char bCarCount;        // +0x14 -- live detail-record count, built up as records fill
    char pad0x15[0x18 - 0x15];           // +0x15
    TrainSyncCarRecordSend records[3]; // +0x18
    char szName[12];                // +0xb10 -- locomotive's own AnimDescRefObj0x477488::szCategoryName
};

// Outbound reply to msg 0x3f2 (opcode 0x3f3, "train state ack" -- received by
// GameNet_HandleTrainStateAck as TrainStateWireMsg). wPad ships uninitialized, same idiom as
// FileBlockWireMsg/WelcomeReplyMsg's own wPad.
struct TrainStateAckWireMsg {
    unsigned short wOpcode;      // +0x0 -- 0x3f3
    unsigned short wPad;         // +0x2
    unsigned short wTrainId;         // +0x4
    unsigned char bOwnerByteA;       // +0x6
    unsigned char bOwnerByteB; // +0x7
    unsigned short wHeading;         // +0x8
};

// Inbound "rotate self train 180deg" wire message. Handed to the game thread as the payload of a
// type-0x17 notify, then (if the message targets the local player's own provider slot) used to find
// the matching active train and flip its heading. Distinct layout from TrainStateWireMsg: the owner
// slot is a dword-read train id at +0x4 and a byte owner at +0x8 (no count/second-owner fields).
struct TrainRotateWireMsg {
    char pad0x0[4];             // +0x0 -- packet header/opcode
    int nTrainId;          // +0x4 -- train id (matched against PeerTrainNode +0x7a)
    unsigned char bOwner;  // +0x8 -- owning provider slot (matched against selectedProviderIndex)
};

// Inbound msg 0x3ea ("session/state update"): a fresh session id plus a state-rotate flag byte.
// GameNet_DispatchMessage resets every active train's heading to a disconnect sentinel (32000) and
// re-broadcasts the roster on receipt.
struct SessionWelcomeWireMsg {
    char pad0x0[4];          // +0x0 -- packet header/opcode
    unsigned int sessionId;  // +0x4 -- new g_pLocalPlayerIdentity->sessionId
    unsigned char bStateFlag; // +0x8 -- nonzero => this->bSessionStateFlagMaybe = 1
};

// Outbound reply to msg 1000 ("who are you"): echoes the (now-updated) session id plus 4 opaque
// app-identity dwords (AppWindow +0x18..+0x24). wPad ships uninitialized (sic, same idiom as
// FileBlockWireMsg's own wPad).
struct WelcomeReplyMsg {
    unsigned short wOpcode;  // +0x0 -- 0x3e9
    unsigned short wPad;     // +0x2
    unsigned int sessionId;  // +0x4
    unsigned int dwFileVersionMajor;  // +0x8  -- g_pApp->unkIdentAMaybe
    unsigned int dwFileVersionMinor;  // +0xc  -- g_pApp->unkIdentBMaybe
    unsigned int dwFileVersionBuild;  // +0x10 -- g_pApp->unkIdentCMaybe
    unsigned int dwFileVersionRevision;  // +0x14 -- g_pApp->unkIdentDMaybe
};

// Inbound msg 0x3f0 ("host, broadcast a named layout-sync command", host-only): a fixed 13-byte
// NUL-terminated name span (matching LocalPlayerIdentity's own name[13] convention) plus a
// reliability dword and two owner bytes. The handler copies the name into a new, ALSO fixed
// 13-byte heap buffer with a plain strcpy -- sic: an unbounded copy with no length check, faithful
// to the original (both source and dest happen to share the same fixed width in practice).
struct LayoutNameWireMsg {
    char pad0x0[4];              // +0x0 -- packet header/opcode
    unsigned int nReliable; // +0x4
    char szName[13];             // +0x8
    char pad0x15;                // +0x15
    unsigned char bOwnerA;       // +0x16
    char pad0x17;                // +0x17
    unsigned char bOwnerB;       // +0x18
};

// Inbound msg 0x3f1 ("full roster snapshot"): a reliability dword, a provider-grid col/row byte
// pair, then 9 back-to-back 60-byte GameNetRosterWireRecord entries (no padding between them --
// matches DPlaySessionMgrProviderSlot's own 9-slot roster array on the receiving side).
struct RosterSnapshotWireMsg {
    unsigned short wOpcode;             // +0x0 -- 0x3f1 (written by the send-side producer,
                                        //   GameNet_BroadcastRosterSnapshot; the inbound
                                        //   dispatch reads the opcode before casting, so the
                                        //   receiver side never touches it)
    char pad0x2[2];                     // +0x2 -- left unset
    unsigned int nReliable;        // +0x4
    unsigned char bGridCols;            // +0x8
    unsigned char bGridRows;            // +0x9
    char pad0xa[0xc - 0xa];             // +0xa
    GameNetRosterWireRecord records[9]; // +0xc
};

// Opaque per-car state block (0x390 bytes) used by the msg 0x3ec player-roster wire format -- a raw
// copy of a CarNetStateAlt snapshot (see CarNetStateAlt::CarNetStateAlt_CreateFromState,
// the send-side producer in GameNet_BroadcastPlayerRoster). Consumed opaquely via
// CarNetState_CreateFromWireRecord; no fields are modeled here.
struct RosterCarStateBlockMaybe {
    unsigned char raw[0x390];
};

// Inbound msg 0x3ec ("peer's player-roster/train-state snapshot"; send side:
// GameNet_BroadcastPlayerRoster, 0x43ccc0): opcode, the sender's session id, a duplicated car
// count (sic, see below), the real car count, then nCarCount back-to-back RosterCarStateBlockMaybe
// entries. Distinct from RosterSnapshotWireMsg (msg 0x3f1's own fixed-9-slot provider-roster
// format) despite the similar name/purpose.
struct PlayerRosterWireMsg {
    unsigned short wOpcode;           // +0x0
    char pad0x2[0x4 - 0x2];           // +0x2
    unsigned int sessionId;    // +0x4 -- g_pLocalPlayerIdentity->sessionId
    unsigned int dwCarCountDup;  // +0x8 -- sic: GameNet_BroadcastPlayerRoster writes the same
                                       //   value here as nCarCount below; the receiver never reads
                                       //   it, and the alloc size (+0x14) over-reserves 4 bytes past
                                       //   the real 0x10-byte header -- reproduced verbatim
    int nCarCount;                    // +0xc -- number of trailing car-state blocks
    RosterCarStateBlockMaybe records[1];  // +0x10 -- variable length (nCarCount)
};

// Inbound msg 0x3f8 ("a peer's train left its origin slot"): just the leaving provider-slot index.
struct SlotByteWireMsg {
    char pad0x0[4];        // +0x0 -- packet header/opcode
    unsigned char bSlot;   // +0x4
};

// Inbound small control opcode 0x14 ("player left"): just the leaving player's DPID. Distinct
// (smaller opcode-value range, outside the 0x3ea+ gameplay jump table) from msg 0x3fd, which
// reaches the same GameNet_HandlePlayerLeft handler via *lpMem instead.
struct PlayerLeftWireMsg {
    char pad0x0[4];    // +0x0 -- packet header/opcode
    int nPlayerId;      // +0x4
};

// Inbound msg 0x3eb ("connect UI request"): a connect-attempt flag byte followed by a
// NUL-terminated string field whose meaning depends on the flag -- unused when bConnect is set
// (the handler goes straight to IniFile-driven session join/retry instead), else the URL suffix
// wsprintfA'd after "http:\\" and (after a safe-charset validation pass) handed to ShellExecuteA.
struct UiConnectWireMsg {
    char pad0x0[8];              // +0x0 -- packet header/opcode + unused
    unsigned char bConnect;      // +0x8 -- low bit: 1 = attempt DirectPlay connect/join/retry,
                                 //          0 = treat szData as a URL suffix and launch it
    char szData[1];              // +0x9 -- NUL-terminated, variable length
};


// 0x467ea0 is the CRT's own `_itoa` (CONFIRMED v401 by reading its body: it dispatches to
// `xtoa` at 0x467ee0 with is_neg = (radix == 10 && value < 0), which is verbatim the CRT
// source). It therefore needs no declaration here at all -- <stdlib.h> has it. The old
// speculative name `Itoa10Signed` is retired.

#include "GameNetThreadState.h"  // GameNetThreadState / g_pGameNetThreadState
