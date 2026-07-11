#pragma once

#include <windows.h>  // GUID, for the DPSESSIONDESC2/service-provider declarations below

// GNetManager -- the DirectPlay transport object (the "net manager"), a global singleton held at
// g_pNetManager (0x48525c). Distinct from both the DPlaySessionMgr singleton (g_pDPlaySessionMgr) and the
// GameNetThreadState worker-thread context: this owns the live DirectPlay session/peer transport
// and is created by DPlay_InitConnection (0x45f390) and torn down + freed by
// GameNet_TeardownAndFlushQueues. The DPlay_* free functions Ghidra boxes in the `DPlay` namespace are
// really __thiscall methods on this object (this=g_pNetManager in ecx); modeled as members so the
// compiler emits the ecx-load + thiscall shape. Non-polymorphic (byte flag at offset 0, non-virtual
// dtor called directly by `delete`). Only the fields/methods the current consumers touch are modeled;
// interior bytes are padding (extend in place).

// The 8-byte header DPlay_ReceiveAndDispatch heap-allocates per received message: the sender's
// DPID plus a pointer to the separately-heap-allocated wire packet (whose own first WORD is the
// opcode). GameNet_DispatchMessage HeapFree's pPacket (conditionally, opcode-dependent) then
// lpMem itself after processing.
struct DPlayRecvMsg {
    int fromPlayerId;        // +0x0
    unsigned short *pPacket; // +0x4
};

// A found-DirectPlay-session list node (0xc bytes), as returned/owned by DPlay_FindSession's own
// internal enumeration list (this+0xd64). Plain POD (no ctor/dtor) -- DPlay_FindSession's own
// cleanup walk frees pSessionDescMem via GlobalHandle/GlobalUnlock/GlobalFree (it's HGLOBAL-
// backed DirectPlay session-desc memory) and pszName via a plain operator delete, then the node
// itself. DPlay_BuildOtherSessionsList (src/GameNet.cpp) builds its OWN separate list of this
// same node shape (only pNext/pszName populated; pSessionDescMem left uninitialized -- sic).
struct FoundSessionNode {
    FoundSessionNode *pNext;  // +0x0
    void *pSessionDescMem;    // +0x4  -- HGLOBAL-locked DirectPlay session-desc memory
    char *pszName;                 // +0x8  -- separately new_alloc'd (0x100-byte) session name
};

// One entry of GNetManager::pProviderList (8 bytes) -- "this machine actually has a working
// DirectPlay service provider of type N". Plain POD (no ctor/dtor): `new` allocates it raw and
// both fields are stored by hand, newest-first (the list is built in probe order Modem, IPX,
// TCP/IP, Serial, so it ends up reversed). Sole builder DPlay_ProbeAvailableProviders.
struct DPlayProviderNode {
    DPlayProviderNode *pNext;  // +0x0
    int nProviderType;         // +0x4 -- 1=Modem, 2=TCP/IP, 3=Serial, 4=IPX (the same 4-valued
                               //         domain as GNetManager::nProtocol)
};

// The real Win32 DirectPlay DPSESSIONDESC2 (ANSI variant -- lpszSessionNameA/lpszPasswordA, not
// the UNICODE union members; this project never includes <dplay.h>, DirectPlay is called via raw
// vtable dispatch throughout, see GNetManager's own DPlay_* methods -- this is a local, minimal
// but FULL-SIZED (0x50 bytes, matches DPlay_FindSession's own dwSize=0x50 write) mirror of it).
struct DPSessionDesc2Partial {
    unsigned int dwSize;             // +0x0
    unsigned int dwFlags;            // +0x4
    GUID guidInstance;                // +0x8  -- left at its zeroed default by DPlay_FindSession
    GUID guidApplication;             // +0x18
    unsigned int dwMaxPlayers;       // +0x28 -- left at its zeroed default by DPlay_FindSession
    unsigned int dwCurrentPlayers;   // +0x2c -- left at its zeroed default by DPlay_FindSession
    char *lpszSessionNameA;           // +0x30 -- left at its zeroed default by DPlay_FindSession
    char *lpszPasswordA;              // +0x34
    unsigned int dwReserved1;        // +0x38
    unsigned int dwReserved2;        // +0x3c
    unsigned int dwUser1;            // +0x40
    unsigned int dwUser2;            // +0x44
    unsigned int dwUser3;            // +0x48
    unsigned int dwUser4;            // +0x4c
};

// A second, independent DPSESSIONDESC2-shaped scratch buffer GNetManager keeps -- unlike
// sessionDesc (a real full 0x50-byte descriptor DPlay_FindSession populates and passes to
// EnumSessions), this one is only ever zeroed with dwSize set to its OWN 40-byte size and passed
// to SetSessionDesc after a successful join/host (DPlay_JoinOrHostSession) -- i.e. a minimal
// "clear the session flags" update, never a real populated descriptor. Genuinely truncated at
// 40 bytes (dwSize/dwFlags/guidInstance/guidApplication's worth) -- it ends exactly at
// GNetManager's own 0x160c object boundary, so it can't be the full DPSessionDesc2Partial.
struct DPSessionDescUpdatePartial {
    unsigned int dwSize;   // +0x0
    unsigned int dwFlags;  // +0x4
    char pad0x8[40 - 0x8]; // +0x8 -- always zeroed, never independently read/written
};

// LEGO Loco's own registered DirectPlay guidApplication (F9CD2546-577F-11D2-9426-00A0244BDA7A),
// pinned via the VA->file-offset raw-byte-read technique (4 separate globals at 0x479158 --
// really one contiguous 16-byte GUID constant in .rdata; DPlay_FindSession's struct-copy into
// DPSessionDesc2Partial::guidApplication decomposes to 4 dword loads from it under this
// toolchain, matching the original's own shape).
extern const GUID g_guidLocoApp;  // 0x479158

// The real Win32 DPCOMPOUNDADDRESSELEMENT (this project never includes <dplay.h> -- DirectPlay
// is called via raw vtable dispatch throughout). Used by DPlay_InitConnection to build the
// per-protocol address array passed to IDirectPlayLobby3::CreateCompoundAddress.
struct DPCompoundAddressElement {
    GUID guidDataType;     // +0x0
    unsigned int dwDataSize; // +0x10
    void *lpData;            // +0x14
};  // 0x18 bytes

// DirectPlay service-provider GUIDs (DPSPGUID_*), a contiguous 4-entry array in .rdata --
// pinned via the VA->file-offset raw-byte-read technique. DPlay_InitConnection's own
// nProtocol protocol switch selects one of these by value (1=Modem, 2=TCP/IP, 3=Serial,
// 4=IPX), confirmed definitively via the switch's own jump table (a direct array index, no
// ambiguity).
extern const GUID g_guidDPSPModem;   // 0x478fd8 -- {44EAA760-CB68-11CF-9C4E-00A0C905425E}
extern const GUID g_guidDPSPTcpIp;   // 0x478fb8 -- {36E95EE0-8577-11CF-960C-0080C7534E82}
extern const GUID g_guidDPSPSerial;  // 0x478fc8 -- {0F1D6860-88D9-11CF-9C4E-00A0C905425E}
extern const GUID g_guidDPSPIpx;     // 0x478fa8 -- {685BC400-9D2C-11CF-A9CD-00AA006886E3}

// DirectPlay address-element type GUIDs (DPAID_*) used by DPlay_InitConnection's compound-
// address element arrays. Canonical SDK names unconfirmed for several of these (this project
// never includes <dplay.h>) -- named descriptively by observed usage instead; values are pinned
// exactly via the VA->file-offset raw-byte-read technique, which is all that matters for the
// byte match.
extern const GUID g_guidDPAIDServiceProvider;  // 0x4790b8 -- element0 on every protocol
extern const GUID g_guidDPAIDPhoneMaybe;            // 0x4790f8 -- Modem: lpData=phone number
extern const GUID g_guidDPAIDModemNameMaybe;        // 0x4790d8 -- Modem: lpData=device name
extern const GUID g_guidDPAIDINetMaybe;             // 0x479118 -- TCP/IP: lpData=address string
extern const GUID g_guidDPAIDInetPortMaybe;         // 0x479138 -- TCP/IP: lpData=port (optional)
extern const GUID g_guidDPAIDComSettingsMaybe;      // 0x479148 -- Serial: lpData=port/baud settings
// IID_IDirectPlay4 and CLSID_DirectPlay -- standard, well-known DirectX values, confirmed
// exactly against the raw .rdata bytes.
extern const GUID g_iidDirectPlay4;   // 0x478f88 -- {0AB1C531-4745-11D1-A7A1-0000F803ABFC}
extern const GUID g_clsidDirectPlay;  // 0x478f98 -- {D1EB6D20-8923-11D0-9D97-00A0C90A43CB}
// IID_IDirectPlayLobby3A -- confirmed exactly against the raw .rdata bytes. This is what
// pDPlaySecondary really is, promoting the header's own IDirectPlayLobby3-shaped guess to a
// fact (its EnumAddress @vtbl+0x14 and CreateCompoundAddress @vtbl+0x38 both line up).
extern const GUID g_iidDirectPlayLobby3A;  // 0x479048 -- {2DB72491-652C-11D1-A7A8-0000F803ABFC}

// DirectPlayCreate -- this project never includes <dplay.h> (DirectPlay is called via manual
// vtable dispatch throughout); this is the one genuine cross-module bring-up call.
extern "C" long __stdcall DirectPlayCreate(GUID *lpGUID, void **lplpDP, void *pUnk);
extern "C" long __stdcall DirectPlayLobbyCreateA(GUID *lpGUIDSP, void **lplpDPL, void *lpUnk,
                                                 void *lpData, unsigned int dwDataSize);
// (0x466390 was declared here as `DPlay_ParsePortNumber`, an app helper. It is not one -- it is
// the CRT's own `atoi`, confirmed v398 by `tools/sweep.py toolchain/vc50/LIB/LIBC.LIB` matching
// it to `_atoi` in `atox.obj`, and by its body being MSVC's textbook one-liner
// `push arg; call _atol (0x4662f0); add esp,4; ret`. Its 20-odd call sites -- easter-card
// loading, save files, ApplSetupWnd::InitFields, TutorialWnd's category parser -- were never
// DirectPlay-specific. Call `atoi()` from <stdlib.h> instead; no declaration belongs here.)
// The modem EnumAddress callback (a standalone, already-compiled DPENUMADDRESSCALLBACK-shaped
// helper referenced only by address from DPlay_InitConnection -- not transcribed, its own body
// is unrelated to any TU here). Copies the phone-number element's data into
// g_pNetManager->sPhoneNumber via a hardcoded global reference, confirmed by its own guid
// compare against g_guidDPAIDPhoneMaybe.
int __stdcall DPlay_EnumAddressCallback(const GUID *pGuidType, unsigned int dwDataSize,
                                        const void *lpData, void *lpContext);  // 0x45fbd0

struct GNetManager {
    unsigned char bIsHost;         // +0x0    -- host flag (also the object base; ReceiveAndDispatch takes &this[0])
    unsigned char bJoinAttempted;  // +0x1    -- InitBigFields' 3rd param (ex-BigObj0x45e700::cM_1)
    unsigned char bAllowHostMigration;         // +0x2    -- InitBigFields' 4th param (ex-BigObj0x45e700::cM_2)
    unsigned char bUnk0x3Maybe;         // +0x3    -- CreateLobby zero-inits; no other known toucher
    unsigned char bUnk0x4Maybe;         // +0x4    -- CreateLobby zero-inits; no other known toucher
    char pad0x5[0x18 - 0x5];            // +0x5
    char sSessionName[200];        // +0x18   -- DPlay_JoinOrHostSession's real pszSessionName arg,
                                        //   truncate-copied in (no length cap applied on this one --
                                        //   only the player-name/password copies check strlen < 0x80)
    char pad0xe0[0x418 - 0xe0];         // +0xe0
    char sPlayerName[0x80];        // +0x418  -- DPlay_JoinOrHostSession's real pszPlayerName arg;
                                        //   also the DPNAME.lpszLongNameA used to CreatePlayer
    char sPassword[0x80];          // +0x498  -- DPlay_JoinOrHostSession's real pszPassword arg;
                                        //   DPlay_FindSession's own pszPasswordFilter param
                                        //   copies into this SAME field (confirms it's a password,
                                        //   not a session-name filter as previously guessed)
    int nProtocol;                 // +0x518  -- gates BOTH DPENUMSESSIONSCALLBACK2 timeout
                                        //   retry decisions: DPlay_EnumSessionsCallback
                                        //   (continue only when this==1 AND no session found
                                        //   yet) and DPlay_JoinSessionEnumCallback (continue
                                        //   only when this==1, no found-list to also check
                                        //   there); doubles as the DirectPlay PROTOCOL tag
                                        //   (1=Modem, 2=TCP/IP, 3=Serial, 4=IPX -- confirmed via
                                        //   DPlay_InitConnection's own per-protocol SP GUID
                                        //   switch). Setter: DPlay_InitConnection itself (direct
                                        //   from its nProtocolArg param when nonzero) or, when
                                        //   nProtocolArg==0, the user's radio-button pick in the
                                        //   "Select Connection" dialog (DPlay_SelectConnection
                                        //   DlgProc, 0x461020 -- resource ids 0x7d32/33/34/35 ->
                                        //   4/2/1/3 respectively).
    // DPlay_InitConnection's own scratch: the raw pszSessionName/pszAddress arg is truncate-
    // copied here with NO length cap (strcpy intrinsic, matches the sSessionName idiom) --
    // reused per-protocol as a TCP/IP hostname, a Modem device name, or a Serial/IPX numeric
    // string (parsed via atoi). wPortOrExt is nPort (the ctor's 3rd arg, the .ini-configured
    // Port for TCP/IP) truncated to a short; only ever read back for the TCP/IP branch.
    char sConnectParam[0x400];     // +0x51c
    unsigned short wPortOrExt;     // +0x91c
    char pad0x91e[0x920 - 0x91e];       // +0x91e
    int nMaxPlayers;                 // +0x920  -- InitBigFields' 2nd param (ex-BigObj0x45e700::nM_920)
    int dpidLocalPlayer;           // +0x924  -- our own DirectPlay player id
    char pad0x928[0x92c - 0x928];        // +0x928
    void *pGlobalLockedBuf;        // +0x92c  -- GlobalAlloc/GlobalLock'd 16-byte guidInstance
                                        //   copy of the session DPlay_JoinExistingSession found
                                        //   via DPlay_JoinSessionEnumCallback (or the session-
                                        //   browser dialog); torn down via GlobalHandle/GlobalUnlock/
                                        //   GlobalFree once consumed (guarded by bSkipGlobalFree)
    bool bSkipGlobalFree;          // +0x930  -- true skips the GlobalUnlock/GlobalFree teardown
                                        //   of pGlobalLockedBuf on the next join attempt
    char pad0x931[0x934 - 0x931];        // +0x931
    int nUnk0x934Maybe;                  // +0x934  -- DPlay_InitConnection zeroes this right before
                                        //   showing the Select-Connection dialog; no other known reader
    HWND hWndParent;               // +0x938  -- parent hwnd (ResetNetManager stores this->hwndOwner)
    // +0x93c -- the app HINSTANCE, used only as LoadStringA's resource module by
    // DPlay_ReportNetworkError (both the caller-supplied prefix id and the fixed message-box
    // caption id 0x7d06 come out of the .rsrc string table).
    HINSTANCE hInstance;           // +0x93c
    // +0x940 -- optional error-reporting HOOK. When set, DPlay_ReportNetworkError hands it the
    // fully-composed message text INSTEAD of showing a message box, and returns 0. __cdecl
    // (the call site cleans its own one argument), unlike almost everything else here.
    // Zero-inited by ResetNetManager; no setter transcribed yet.
    void (__cdecl *pfnErrorTextHookMaybe)(char *pszText);  // +0x940
    // +0x944 -- "errors may pop a message box". Only consulted when pfnErrorTextHookMaybe is
    // null; false makes DPlay_ReportNetworkError silent (it still records sLastErrorText).
    // Zero-inited by ResetNetManager.
    unsigned char bShowErrorMessageBoxMaybe;  // +0x944
    // +0x945 -- the last composed error message, kept for later inspection: every
    // DPlay_ReportNetworkError call strcpy's its text here regardless of how (or whether) it is
    // then displayed. sic: an unbounded strcpy from a 0x200-byte source buffer, and the field is
    // MISALIGNED (odd offset), which is why the composed-text copy is a byte-wise `rep movsb`
    // tail after the dword loop. Extent is a hypothesis: 0x200 matches the only writer's source
    // buffer, but nothing bounds it and no reader is transcribed yet -- the padding after it
    // keeps every later offset correct either way.
    char sLastErrorText[0x200];    // +0x945
    char pad0xb45[0xd48 - 0xb45];       // +0xb45
    unsigned int hrLastResult;     // +0xd48  -- HRESULT of the last EnumSessions/etc. attempt
    void (*pfnIdlePumpCallback)(); // +0xd4c  -- called (0 args) once per retry iteration while
                                        //   DPlay_FindSession is blocked waiting on DPERR_CONNECTING;
                                        //   presumably keeps the host message loop pumped
    unsigned char bSessionJoined;  // +0xd50  -- set once we've joined/hosted a session
    char pad0xd51[0xd54 - 0xd51];       // +0xd51
    // A single cached copy of the last opcode-0x32 "reliable message" payload, replaced
    // (old one released via operator delete) each time a new one arrives; DPlay_
    // ReceiveAndDispatch is the sole writer. No known reader yet -- consumers of the cache
    // (if any) are still unidentified. WIDTH CORRECTED 2026-07-20: a prior session's Ghidra
    // pass had already named this field (never synced to src/) but typed it a 2-byte word --
    // raw disasm shows both stores are DWORD-width (`mov DWORD PTR [+0xd54],0` /
    // `mov DWORD PTR [+0xd54],ecx`), so it's a real 4-byte field despite its value always
    // coming from a zero-extended 16-bit wire source (same family as the byte-spill/dword-
    // widen lessons in CLAUDE.md).
    unsigned int cbDataCacheMaybe;  // +0xd54
    void *pDataCacheMaybe;          // +0xd58
    // Outbound send-queue throttle threshold, init'd to 10 in the ctor. DPlay_SendMessage's sole
    // consumer: for unreliable sends only, when nonzero, queries the live send-queue depth
    // (IDirectPlay4::GetMessageQueue) and refuses to send ("Message Throttled by WigNet") once
    // it's exceeded. RESOLVED 2026-07-20 (was bCheckQueueDepthMaybe, no known consumer) via a
    // whole-binary xref sweep + DPlay_SendMessage transcription.
    unsigned int nSendThrottleQueueDepth;  // +0xd5c
    // +0xd60 -- a THIRD found-session-shaped list. Never written by any transcribed function;
    // only the dtor touches it, draining it exactly like pListHead2Maybe (pszName + node, no
    // pSessionDescMem GlobalFree). Typed from that drain.
    FoundSessionNode *pListHead0Maybe;  // +0xd60
    FoundSessionNode *pListHead1; // +0xd64 -- DPlay_FindSession's own enumeration list,
                                        //   drained/rebuilt on every call (see FoundSessionNode)
    // A SECOND found-session-shaped list, drained alongside pListHead1 by
    // DPlay_TeardownConnection -- but with a strictly smaller teardown: only pszName and the node
    // itself are freed, never pSessionDescMem. That matches the "only pNext/pszName populated"
    // list DPlay_BuildOtherSessionsList builds, so this is very likely where that filtered
    // "other sessions" list is parked; no writer has been transcribed yet to prove it.
    FoundSessionNode *pListHead2Maybe;  // +0xd68
    // Head of the lazily-built "which DirectPlay service providers does this machine actually
    // have?" list (DPlayProviderNode). Built exactly once by DPlay_ProbeAvailableProviders --
    // a non-null head short-circuits every later call, so the (slow: modem caps query, two
    // DirectPlayCreate probes, four CreateFileA COM-port opens) probe runs at most once per
    // process. Never freed.
    DPlayProviderNode *pProviderList;   // +0xd6c
    // Modem-only scratch: the phone-number string extracted from the modem connection-caps blob
    // by DPlay_InitConnection's EnumAddress callback (0x45fbd0, DPAID phone-number element).
    // Real size unconfirmed (no bounds check observed) -- claims the whole padding span up to
    // the known SerialPortSettings field below; re-split if a future consumer needs less.
    char sPhoneNumber[0x1570 - 0xd70]; // +0xd70
    // Serial-protocol COM port settings, built by DPlay_InitConnection from an atoi'd port number
    // (this->sConnectParam, clamped to [1,4] else 0) plus a fixed 9600-baud/8N1-shaped tail;
    // passed as the DPAID "com settings" compound-address element (only when sConnectParam
    // is non-empty).
    struct SerialPortSettings {
        int nPortNum;   // +0x0  -- atoi(sConnectParam), clamped to [1,4] else 0
        int nBaudRate;  // +0x4  -- always 0x9600 (9600 baud)
        int nUnk8Maybe;      // +0x8  -- always 0
        int nUnk0xcMaybe;    // +0xc  -- always 0
        int nByteSizeMaybe;  // +0x10 -- always 2
    } serialSettings;              // +0x1570 (0x14 bytes)
    void *pTempDPlayIface;         // +0x1584 -- transient IDirectPlay (pre-QI) object from
                                        //   DirectPlayCreate, released immediately after
                                        //   QueryInterface(IID_IDirectPlay4) populates +0x1588
    void *pDirectPlay4;            // +0x1588 -- the live IDirectPlay4 COM pointer (0 until a
                                        //   connect fully succeeds); DPlay_UiConnectHandler gates its
                                        //   whole join/retry sequence on this being non-null
    DPSessionDesc2Partial sessionDesc; // +0x158c -- scratch DPSESSIONDESC2 DPlay_FindSession
                                        //   rebuilds fresh on every call and passes to EnumSessions
    // +0x15dc -- the transient IDirectPlayLobby object DirectPlayLobbyCreateA hands back in
    // the ctor, released the moment QueryInterface(IID_IDirectPlayLobby3A) has populated
    // pDPlaySecondary. Same create-QI-release shape as pTempDPlayIface/pDirectPlay4.
    void *pTempLobbyIface;         // +0x15dc
    void *pDPlaySecondary;         // +0x15e0 -- a SEPARATE COM object (IDirectPlayLobby3-
                                        //   shaped: EnumAddress @vtbl+0x14, CreateCompoundAddress
                                        //   @vtbl+0x38), created elsewhere (the real ctor,
                                        //   DPlayLobby_Init); DPlay_InitConnection is its only known
                                        //   consumer so far.
    DPSessionDescUpdatePartial sessionDescUpdate; // +0x15e4 -- scratch minimal session-desc
                                        //   (only dwSize set, rest zeroed) DPlay_JoinOrHostSession
                                        //   passes to SetSessionDesc after a successful join/host;
                                        //   ends exactly at the object's own 0x160c boundary

    // The ctor is a one-liner forwarding to CreateLobby -- its whole body is that call.
    // The second argument is the parent HWND (stored at +0x938), NOT a flags word as a
    // prior guess had it; both known call sites pass 0.
    GNetManager(void *hInstance, HWND hWndParent);                      // 0x45e490
    // Zero-inits every scalar field (bAllowHostMigration and bShowErrorMessageBoxMaybe to
    // TRUE, nSendThrottleQueueDepth to 10, the string fields to empty), then does the real
    // bring-up: DirectPlayLobbyCreateA -> QueryInterface(IID at 0x479048) into
    // pDPlaySecondary -> release the temporary lobby object. NOT DirectSound init (an
    // early doc mislabel; that is DSound_InitDeviceAndChannelPool at 0x412c50).
    void CreateLobby(void *hInstance, HWND hWndParent);                 // 0x45e4b0
    // Non-virtual; called directly by `delete`. DPlay_TeardownConnection, then release
    // pDPlaySecondary, free the cached reliable-message payload, and drain all FOUR owned
    // lists (the three found-session ones plus pProviderList).
    ~GNetManager();                                                     // 0x45e5a0 (non-virtual)
    // Returns 1 on success, 0 on failure (not yet connected, throttled, or a real error
    // HRESULT) -- CORRECTED 2026-07-20, a prior guess had this polarity backwards (see the
    // definition's own plate comment in src/GameNet.cpp for the raw-disasm proof).
    // GameNet_SendTrainStateSync (src/GameNet.cpp) is the first caller to consume this;
    // every other call site discards it (a bare-statement call, legal regardless of return type).
    int DPlay_SendMessage(int dpid, void *buf, int len, int dwFlags); // 0x460d40 (src/GameNet.cpp)
    // Full connection teardown: releases the GlobalLock'd join-instance buffer, cancels all
    // queued DirectPlay messages + Close + Release on pDirectPlay4, drains BOTH found-session
    // lists, and resets the session identity/protocol scratch fields to their empty state.
    void DPlay_TeardownConnection();                                    // 0x45fc30
    DPlayRecvMsg *DPlay_ReceiveAndDispatch();                           // 0x4606d0 (src/GameNet.cpp, PARTIAL)
    // Real thiscall (this=g_pNetManager) with 3 stack params (`ret 0xc`), NOT the `__stdcall(void)`
    // Ghidra's own analyzer originally inferred -- classic "under-analyzed callee" tell (huge, 0x81b-
    // byte body; Ghidra's own SP tracking desynced into giant in_stack_0000NNNN offsets). Return
    // type corrected to Ghidra's own `uint` (was stale `void`) -- both DPlay_FindSession and
    // DPlay_JoinOrHostSession test the LOW BYTE of its result (`(char)result == 0` -- a real
    // success/fail status, not a bare call).
    //
    // nProtocol==0: shows the "Select Connection" dialog (resource 0x7d0a,
    // DPlay_SelectConnectionDlgProc/0x461020), which sets nProtocol itself via its radio
    // buttons; returns 0 immediately if the dialog is cancelled. nProtocol!=0: sets
    // nProtocol = nProtocol directly, truncate-copies pszSessionName into
    // sConnectParam (no length cap) and nPort into wPortOrExt.
    //
    // Then, regardless of path: bounds-checks nProtocol into [1,4] (else return 0), picks
    // the matching DPSPGUID_*, and does the classic 2-step DirectPlay bring-up: (1)
    // DirectPlayCreate(spGuid, &pTempDPlayIface, 0) -> QueryInterface(IID_IDirectPlay4,
    // &pDirectPlay4) -> Release the temp object; (2) build a per-protocol
    // DPCompoundAddressElement array (always element0 = ServiceProvider+spGuid; Modem adds
    // a device-caps-derived phone-number + device-name pair via a GetCaps-like
    // GlobalAlloc/GlobalLock dance and an EnumAddress callback (0x45fbd0, not transcribed -- an
    // already-compiled standalone helper referenced only by address); TCP/IP adds an
    // INet-address element plus an optional port element when wPortOrExt!=0; Serial adds an
    // optional COM-settings element (serialSettings, only when sConnectParam is
    // non-empty); IPX adds nothing else) and calls
    // pDPlaySecondary->CreateCompoundAddress(elements, nElements, addrBuf, &dwAddrSize)
    // (addrBuf a 0x1000-byte stack buffer). Finally releases pDirectPlay4, re-acquires a
    // fresh one via CoCreateInstance(CLSID_DirectPlay, IID_IDirectPlay4) (falling back to a
    // second DirectPlayCreate+QI dance against GUID_NULL if that fails), and calls
    // pDirectPlay4->InitializeConnection(addrBuf, 0). Every failure formats the HRESULT
    // (DPlay_FormatHresultString) into a literal-prefixed message and routes it through
    // DPlay_ReportNetworkError; on total success returns 1.
    unsigned int DPlay_InitConnection(int nProtocol, char *pszSessionName, int nPort);  // 0x45f390
    // Join (bIsHost==0, via DPlay_JoinExistingSession) or host (via
    // DPlay_HostNewSession) a session, then CreatePlayer our local player (DPNAME built
    // inline: dwSize 0x10, lpszShortNameA="" (the pooled empty literal), lpszLongNameA=
    // sPlayerName). On any failure, tears the connection down and returns 0. On overall
    // success, also does a minimal SetSessionDesc "clear flags" update via sessionDescUpdate.
    // sSessionName/sPlayerName/sPassword copies share ONE truncate-copy idiom with
    // DPlay_FindSession's own pszPasswordFilter copy (strcpy under strlen<0x80, else a
    // temporary null-poke at index 0x80 -- see that function's own "sic" 1-byte-overflow note,
    // which applies here identically to the player-name and password copies but NOT the
    // session-name copy, which has no length cap at all).
    char DPlay_JoinOrHostSession(char *pszPlayerName, char *pszSessionName, char *pszPassword);  // 0x45e730
    // Re-enumerates DirectPlay sessions (IDirectPlay4::EnumSessions, vtbl+0x34) and returns the
    // head of its own found-session list (pListHead1, drained/rebuilt on every call unless
    // bJoinAttempted is set). pszPasswordFilter copies into this->sPassword -- the
    // SAME field DPlay_JoinOrHostSession populates from its own real pszPassword arg -- so this is
    // a password filter, not a "session name filter" as a prior session guessed; every known
    // caller (DPlay_BuildOtherSessionsList, FUN_004611b0's join-session dialog proc) passes
    // NULL. On DPERR_CONNECTING (0x8877015e) retries via pfnIdlePumpCallback + Sleep(1) until
    // bJoinAttempted is set or a different result comes back; the found-session list itself is
    // populated asynchronously by the DPENUMSESSIONSCALLBACK2 at LAB_0045f2b0 (not yet transcribed).
    FoundSessionNode *DPlay_FindSession(char *pszPasswordFilter);  // 0x45f090

    // "Which DirectPlay service providers does this machine actually have?" -- builds
    // pProviderList once (see DPlayProviderNode) and returns its head. Refuses to run at all
    // once a real connection is live (pDirectPlay4 != 0 => returns 0), because it creates and
    // immediately releases throwaway IDirectPlay4 objects to test each provider.
    DPlayProviderNode *DPlay_ProbeAvailableProviders();  // 0x45eab0
    // The modem arm of that probe: DirectPlayCreate(DPSPGUID_MODEM) + a GetConnectionCaps-style
    // GlobalAlloc/GlobalLock dance to confirm a real modem device exists. Same "refuses while
    // pDirectPlay4 is live" guard.
    bool DPlay_ProbeModem();  // 0x45eec0
    // The serial arm's per-port existence test: opens "COM<digit>" (pszPortDigit is a
    // one-character string, "1".."4") and closes it again. Genuinely `inline` in the original,
    // but DECLARED-ONLY here: the original GameNet.cpp TU compiled GameNetThread_InitState's
    // "which COM ports exist?" loop (0x438c67) WITHOUT the body in scope -- that call site is a
    // real `call` to the out-of-line COMDAT copy at 0x45ee60, while the same TU's
    // DPlay_ProbeAvailableProviders expands it FOUR times. Reproducing both shapes from one TU
    // requires the definition to appear BETWEEN the two consumers in src/GameNet.cpp (an
    // out-of-class `inline` definition there, verbatim with the load-bearing source notes --
    // see it). Never reads its own `this` (but is a real member -- the 0x438c67 call site
    // still materializes ecx from the g_pNetManager global).
    bool ProbeComPort(const char *pszPortDigit);  // 0x45ee60 (out-of-line copy); body src/GameNet.cpp

    // DPlay_JoinOrHostSession's join/host callees -- synchronous IDirectPlay4::Open (vtbl+0x60);
    // on hr==0 sets bSessionJoined. Host: no args, builds+opens a fresh session. Join:
    // session-browser dialog (resource 0x7d0b) or pre-existing enum data, then Open.
    unsigned int DPlay_HostNewSession();       // 0x45fd80
    unsigned int DPlay_JoinExistingSession();  // 0x460360
    // Formats an HRESULT as a DPERR_*-named string (giant if-chain of DirectPlay error constants,
    // falling back to "Unknown Error Code: %d") into the caller's buffer. Genuinely never reads
    // its own implicit `this` (confirmed via its own raw disasm -- pure stack-arg lookup table),
    // but every call site still materializes `this` in ECX from the g_pNetManager GLOBAL (not
    // from an already-live `this` register, even when called from another GNetManager method) --
    // same this-ignoring-thiscall family as Widget::TestAndToggleMenuNodeHoverMaybe /
    // GameNetMsgQueue::EnqueueOrProcessLocalNodeMaybe; call as g_pNetManager->...(), not this->....
    void DPlay_FormatHresultString(char *pszOutBuf, int hresult);  // 0x45ff30 (declared-only)
    // Shared DirectPlay error sink (resource-string lookup + text -> persistent last-error buffer
    // -> callback hook or MessageBoxA). Other known callers: 0x43a760, 0x43c860, 0x45f090.
    int DPlay_ReportNetworkError(unsigned int nPrefixResId, char *pszMessage);  // 0x460ea0

    // Reinitializes 4 fields on an ALREADY-CONSTRUCTED GNetManager (distinct from the real ctor
    // at 0x45e490) -- sole caller GameNet_ProcessLocalCommand's case 0 (0x439550, not yet
    // transcribed), reached on a connection reset: DPlay_TeardownConnection() then
    // InitBigFields(...). Ex-`BigObj0x45e700::InitBigFields` -- that struct was a phase2-probe
    // placeholder (`src/phase2_probe3.cpp`) for a caller that reinterpret-casts g_pNetManager
    // directly to it with no offset adjustment (`(BigObj0x45e700 *)g_pNetManager`); its 4 fields
    // (offsets 0/1/2/0x920) all land inside GNetManager's own layout, confirming it was really a
    // GNetManager method all along, not a separate class -- moved in here per the
    // never-duplicate-structs rule.
    void InitBigFields(unsigned char bA, int nB, unsigned char bC, unsigned char bD); // 0x45e700
};

extern GNetManager *g_pNetManager;  // 0x48525c
