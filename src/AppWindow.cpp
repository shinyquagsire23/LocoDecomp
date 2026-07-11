// AppWindow -- the app singleton's own translation unit (Ghidra namespace `AppWindow`, the
// 0x4068d0..0x408a30 run of .text). Thirteen functions are transcribed here so far:
//
//   * ScrollBoardHorizontal / ScrollBoardVertical (0x407ae0 / 0x407bf0) -- the WM_HSCROLL and
//     WM_VSCROLL board scrollers, an exact mirror-image pair.
//   * ToggleWindowedModeMaybe (0x407d00) -- the 'W' key's display-mode flip.
//   * ApplyDisplayModeMaybe (0x407d20) -- the windowed/fullscreen switch itself.
//   * BuildTool_SetAutoCurveConnectModeMaybe (0x4089d0) -- the build-tool mode setter.
//   * ReadHklmValue (0x408a30) -- a small HKEY_LOCAL_MACHINE REG_SZ reader.
//
//   * AbortMultiplayerSession (0x406e80) -- the multiplayer-abort / return-to-front-end path.
//   * CreateMainWindow (0x406ed0) -- the "LEGO LOCO" window class and the app's one window.
//   * DrainQueuedMouseInput (0x4085e0) -- swallow or replay the mouse input a blocking animation
//     let pile up behind it.
//   * StartGame (0x408350) -- screen state 1's arm: realize the UI windows and load the world,
//     off-thread on a cold start and inline on a re-entry.
//   * EnterBuildMode (0x4086f0) -- screen state 3's arm: tear down whichever screen is being
//     left and resume the live world board.
//
//   * ConstructSingletonWindows (0x406f90) -- the bootstrap: constructs all eight top-level
//     singleton windows in sequence (Splash, Mail, Map, BuildToolCursor, AlbumCard, EditCard,
//     Tutorial, Credits). Each is heap-allocated with its resource id, then Create()d against
//     the parent HWND; any failure tears down the windows built so far and returns a distinct
//     negative code (-2 .. -17). Called once from Config_FUN_00406ba0. The teardown chains
//     delete the already-created windows in a fixed order (Mail, Splash, Map, BuildToolCursor,
//     AlbumCard, EditCard, Tutorial, Credits -- creation order with Mail/Splash swapped), each
//     `delete` dispatching through the window's virtual dtor (vtable slot 0).
//   * LocoBitmap_LoadThumbPalSingletonMaybe (0x45c8a0) -- the bootstrap's misc\thumbpal.bmp
//     load into DAT_004ff110; one of its hard startup failures.
//
// (Renamed from src/Bootstrap.cpp in v421 once more AppWindow-namespace functions landed here;
// the TU's real identity is the app singleton, and src/AppWindow.h is its header.)
//
// v422 retired this file's `BootstrapAppContext` struct -- a partial view of AppWindow itself under
// a different name (its +0x8 "hwndParent" is AppWindow::hwndOwner and its +0xc is hInstance), i.e.
// exactly the duplicate-struct hazard src/AppWindow.h's own header note describes having already
// cleaned up twice. ConstructSingletonWindows now takes a real AppWindow*.

#include <windows.h>
#include <mmsystem.h>          // PlaySoundA (WINMM) -- AppWindow_StartGame's sound stop
#include <direct.h>            // _getcwd  -- AppWindow_LoadConfigDirectories' registry self-heal
#include <stdio.h>             // _fcloseall -- the clean-exit CRT flush
#include <stdlib.h>            // srand -- the bootstrap's RNG seed
#include <time.h>              // time  -- what that seed is taken from
#include <string.h>            // strcpy/strcat/strlen (all inlined by /O2 at these call sites)
#include <sys/types.h>
#include <sys/stat.h>          // struct _stat / _stat -- the install-directory existence check
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: screen-size clamp, see port/README.md
#endif

#include "WindowBase.h"        // FullscreenPopupWndPartial (Map/AlbumCard shared Create helper)
#include "SplashWnd.h"
#include "MailWnd.h"
#include "MapWnd.h"
#include "BuildToolCursorWnd.h"
#include "AlbumCardWnd.h"
#include "EditCardWnd.h"
#include "TutorialWnd.h"
#include "CreditsWnd.h"
#include "AppWindow.h"
#include "WorldBoardMaybe.h"
#include "ScreenSaver.h"
#include "UIResources.h"
#include "GameNetMsgQueue.h"   // g_nScreenState (the app screen-state selector)
#include "PlacementCursorMaybe.h"
#include "DPlaySessionMgr.h"   // g_pDPlaySessionMgr
#include "DSound.h"            // g_pDSoundManager (DSound_SetTemporaryDuck)
#include "PeerTrainSlotQueueMaybe.h"
#include "NetSessionEventQueue.h" // g_NetSessionEventQueue (the board layout loader)
#include "ThreadWrapper.h"     // g_worldLoadThread
#include "IniFile.h"           // g_pIniFile
#include "DecorObjMgrMaybe.h"  // DecorObjMgrMaybe_00485448
#include "EffectSpawner.h"     // DAT_004fd220
#include "LockableMaybe.h"    // g_pGameNetMsgQueueLock's real class (deleted below)
#include "GameNetThreadState.h" // g_pGameNetThreadState / g_pGameNetThread
#include "PostBag.h"            // g_pPostBagCache / g_pPostBagFileCache
#include "LocalPlayerIdentity.h" // g_pLocalPlayerIdentity
#include "WidgetBase.h"         // AnimDescRefObj0x477488 (the two loading-screen anim refs)
#include "WorldActionCursor.h"  // SelectedObjWidgetMaybe / WorldActionCursor singletons
#include "BuildToolButton.h"    // g_BuildToolButton

// 0x43f7b0 (Ghidra: GameNet::TeardownAllSessionState) -- the full DirectPlay session teardown:
// posts the provider-teardown notification, drains both pending-node lists (each node freed through
// its own virtual dtor), walks the 9 layout slots freeing each one's node chain and display buffer,
// resets the providers, drops the mode back to 0, and posts the teardown notification again. Its
// only caller in the whole binary is AbortMultiplayerSession below. Defined in
// src/DPlaySessionMgr.cpp (v519, EFFECTIVE-parked DIFF(4)).
//
// Declared here rather than in src/DPlaySessionMgr.h, which is its proper home, because ANY new
// declaration in that header -- member or free function alike -- rotates src/MailWnd.cpp's
// RefreshClientClipRect (0x42f8b0) out of its 1332-byte exact match. Measured both ways; see
// src/MailWnd.h's own note on that TU's declaration-count sensitivity and docs/CODEGEN.md.
void __fastcall GameNet_TeardownAllSessionState(DPlaySessionMgr *pMgr);

// 0x408130 -- the app's screen-state switch, defined further down this TU. Prototyped here because
// AppWindow::AbortMultiplayerSession (0x406e80) calls it from above the definition; see
// src/AlbumCardWnd.cpp, which declares it the same way.
void AppWindow_SetScreenState(int nNewState); // TODO: idiom

// 0x408350 / 0x4086f0 -- screen state 1's and state 3's arms of the dispatcher at 0x408130. Both
// are defined further down this TU but sit at HIGHER addresses than their caller, so the original
// source needed these prototypes ahead of the switch just as this transcription does.
void __cdecl AppWindow_StartGame(int nPrevState);
void __cdecl AppWindow_EnterBuildMode(int nPrevState);

// 0x406ed0 / 0x406f90 -- the main window and the eight singleton windows, both defined further
// down this TU but at HIGHER addresses than the bootstrap at 0x406ba0 that calls them, so the
// original source needed these prototypes ahead of it just as this transcription does.
char __fastcall AppWindow_CreateMainWindow(AppWindow *pApp);
int __fastcall AppWindow_ConstructSingletonWindows(AppWindow *pApp);

// The DirectDraw2 object the whole renderer draws through; screen state 0xa hands the display mode
// back to the desktop through it on the way out. Declared the same way in src/LocoBitmap.cpp,
// src/EditCardWnd.cpp and src/CreditsWnd.cpp.
extern IDirectDraw2 *g_pDDraw2;         // DAT_00485440  // TODO: idiom

// The two cached window rects the whole UI offsets against; see src/Main.cpp, which declares the
// same pair the same way.
extern RECT g_rectAppWindowBounds;      // DAT_00485200 -- last GetWindowRect of hwndOwner
extern RECT g_rectAppClientBounds;      // DAT_00485220 -- last GetClientRect of hwndOwner
extern unsigned char g_bBoardScrollFlag; // DAT_00485210 -- board is scrollable/windowed

// Screen geometry, all captured by AppWindow::LoadWindowAndBalancing (0x406480). SIGNED: that
// function halves the first two with `cdq; sub eax,edx; sar eax,1` (a signed /2, not the `shr` an
// unsigned would give) and CheckMinimumDisplaySpec compares the width with `jle`
// and `jge`. src/TutorialWnd.cpp already declares the two halves as `int`.
extern int g_dwScreenWidth;             // DAT_004851d8
extern int g_dwScreenHeight;            // DAT_00485214
extern int g_dwScreenHalfWidth;         // DAT_004851f8
extern int g_dwScreenHalfHeight;        // DAT_004851fc

// The whole desktop as a rect, kept alongside the two window rects above and reset by the app
// ctor (0x406230) before LoadWindowAndBalancing fills it in.
extern RECT g_rectScreenBounds;         // DAT_004851e0

// DAT_0048521c -- GetDeviceCaps(hdc, BITSPIXEL) of the desktop DC; declared the same way in
// src/Main.cpp. DAT_004851f0 is WM_SIZE's "minimized / don't paint the board" state, likewise.
extern int g_dwScreenBpp;
extern char g_bAppMinimizedMaybe;

// DAT_00485238 -- "the board scroll bars are currently shown". Every write in ApplyDisplayModeMaybe
// is paired with a ShowScrollBar of the same polarity, and the two scroll-range helpers (0x407ae0 /
// 0x407bf0, see src/AppWindow.h) read it as their "there is a scroll bar to range" gate. Also
// written by the app ctor (0x406247) and by LoadWindowAndBalancing (0x4064f1).
extern unsigned char g_bScrollBarsVisible;

// The build-tool mode selector, shared with src/WorldBoardMaybe.cpp and
// src/PlacementCursorMaybe.cpp; DAT_004aa7dc is its companion "a tool mode is live" byte,
// consumed by FUN_00410d20 and still unnamed.
extern int DAT_00485234;
extern unsigned char DAT_004aa7dc;

// The season override parsed off the command line (0 = none, 1 Easter, 2 Desert, 3 Halloween,
// 4 Winter, 5 XMas). AppWindow_ParseCommandLine is its only writer and 0x41f970 -- the easter-egg
// manager's calendar lookup, not transcribed yet -- is its only reader in the whole binary, so it
// has no shared header to live in (the same situation as g_szRemoteResPath below).
extern int g_forcedSeason;              // DAT_00485230

// The 150 ms WM_TIMER (id 0x47) AppWindow_StartGame arms for the duration of the world load and
// FUN_004086f0_BigSwitch's state-1 arm kills once the board is up.
extern UINT g_uLoadingTimerId;          // DAT_004a97a4

// lego.ini's [DIRECTORIES] Res -- the install prefix stripped off the front of every loose-file
// path, always kept with a trailing backslash. Declared the same way in src/DSoundChannel.h.
extern char g_pInstallPathPrefix[];     // DAT_004a99c8

// lego.ini's [DIRECTORIES] RemoteRes -- the optional network share the loose-file loader falls
// back to, "" when unset and always forced empty in attract mode. Also kept trailing-backslashed.
// AppWindow_LoadConfigDirectories (0x4068d0) is its only reader or writer in the whole binary
// (7 xrefs, all inside that one function), so it has no shared header to live in.
extern char g_szRemoteResPath[];        // DAT_004a97a8

// lego.ini's [PROCESS] CleanExit, read (default 1) and immediately written back as 0 by
// LoadWindowAndBalancing (0x406630) so a crash leaves it clear for the next run. Only a run whose
// predecessor shut down cleanly trusts the "~curr" checkpoint enough to reload it.
extern char g_bCleanExit;               // DAT_00485218

// The one shared worker thread the app reuses for both of its long jobs: the cold-start world
// load below, and FUN_004086f0_BigSwitch's post-transition idle pump.
extern ThreadWrapper g_worldLoadThread; // DAT_004a9ad0

// 0x45de40 (Ghidra: AppWindow::App_LoadWorldThreadProcMaybe) -- the off-thread world load
// AppWindow_StartGame hands to g_worldLoadThread on a cold start. Lives in another .text run and
// is not transcribed yet; Ghidra types it `void (void)` because the body ignores the thread
// argument, but ThreadWrapper::Start's routine pointer is `void (__cdecl *)(void *)`.
void __cdecl App_LoadWorldThreadProcMaybe(void *pArg); // TODO: idiom

// 0x45e090 -- raises the loading screen: stops any playing sound, blacks the client area out
// through the DirectDraw work surface, then builds and centers the "loading" animation object.
void LoadingScreen_Show(); // TODO: idiom

// 0x45e1e0 -- the between-stages pump the cold-start path runs after each window realizes its
// backdrop; see src/Main.cpp, which declares it the same way.
void LoadingScreen_Pump(unsigned int u); // TODO: idiom

// The world's frame counter, bumped by the main pump and shipped as the WM_APP+6 (0x406) tick
// message's wParam.
extern unsigned int g_dwGameTick;       // DAT_004a99b4

// DAT_004a99b0 is the seasonal/easter-egg unlock manager src/UIResources.cpp models as
// EasterEggMgrMaybe -- but TU-locally, so there is no header to include here yet; it sits
// immediately below g_dwGameTick and is also torn down by SaveWindowAndCleanExit (0x4077a0) via
// FUN_0041f4e0. Spelled as a __fastcall free function because its one entry point takes no
// argument beyond `this`, which makes __fastcall and __thiscall byte-identical at the call site
// (verified: this call site matches). Fold onto EasterEggMgrMaybe when that class gets a shared
// header -- and re-measure, per the MEASURED DIAL note in src/AppWindow.h.
// Formalized in v429 from the old `extern int DAT_004a99b0` + two __fastcall thunks: the
// bootstrap at 0x406ba0 needed a method with a STACK argument, and the __fastcall escape hatch
// only reproduces a this-call whose argument list is empty (a second __fastcall parameter goes in
// edx, not on the stack). Promoted out of this TU into src/EasterEggMgr.h in v435 when
// src/LoadingScreen.cpp needed the sibling [TimeEvents] loader -- the include sits HERE, exactly
// where the struct used to, so this TU's token stream is unchanged. src/Main.cpp's
// EasterEggMgrWndProcView0x4618c0 and src/UIResources.cpp's own EasterEggMgrMaybe are still
// independent views of the same singleton and should be folded onto it too. The FULL
// field+method model landed 2026-07-29 (v494) as class `ScriptEventLoader` in
// src/ScriptEventLoader.cpp; see that TU and src/EasterEggMgr.h's note for the fold direction.
#include "EasterEggMgr.h"

// SaveWindowAndCleanExit's teardown tail. Every one of these is a real member of an
// already-modeled class, but none is declared in that class's shared header yet, and this TU
// deliberately does not grow those headers just to reach them -- see the __fastcall escape
// hatch above (each takes no argument beyond `this`, which makes __fastcall and __thiscall
// byte-identical at the call site). Fold each onto its class when that class is next opened.
void __fastcall WorldBoardMaybe_ResetAllTilesMaybe(WorldBoardPartial *pBoard);             // 0x454fe0  // TODO: idiom
void __fastcall WorldBoardMaybe_ShutdownMaybe(WorldBoardPartial *pBoard);                  // 0x454de0  // TODO: idiom
void __fastcall EffectSpawner_ShutdownMaybe(EffectSpawner *pSpawner);                      // 0x423a90  // TODO: idiom
void __fastcall NetSessionEventQueue_ShutdownMaybe(NetSessionEventQueue *pQueue);          // 0x41d310  // TODO: idiom

// 0x446050 (Init) is NOT declared here. It used to be, as a __fastcall free function taking
// UIResources* -- and that spelling mangles to a symbol nothing defines, because
// src/UIResources.cpp defines the body as a member. The two agreed with Ghidra and with each
// other's addresses, so lint_ghidra_sync and the byte-match were both satisfied while the
// emitted call went to a generated stub that returns 0 -- which read to the bootstrap as
// "resources failed to come up" and produced the fatal MessageBox. It is now
// UIResources::Init, declared in src/UIResources.h and called through g_UIResources below.
// 0x42a5f0 -- drops the process-wide shared 8-bit palette. A plain free function (no `this`).
void LocoBitmap_FreeSharedPalette();                                                       // 0x42a5f0  // TODO: idiom

// The shared thumbnail-palette bitmap (misc\thumbpal.bmp, loaded once at bootstrap) and the
// global scratch string the loader below dead-copies out of -- neither owner transcribed yet.
extern LocoBitmap *DAT_004ff110;                       // DAT_004ff110  // TODO: idiom
extern char DAT_004ff114[];                            // DAT_004ff114  // TODO: idiom

// FUNCTION: LOCO 0x45c8a0
// Loads misc\thumbpal.bmp into the shared thumbnail-palette global (DAT_004ff110); its failure
// is one of the bootstrap's hard startup failures (the 0x406ba0 caller bails with -1). A plain
// free function (no `this`), like LocoBitmap_FreeSharedPalette above.
bool LocoBitmap_LoadThumbPalSingletonMaybe()
{
    char szThumbPalPath[260];

    DAT_004ff110 = new LocoBitmap();
    // sic: dead copy -- the sprintf below overwrites every byte this strcpy writes.
    strcpy(szThumbPalPath, DAT_004ff114);
    sprintf(szThumbPalPath, "%smisc\\thumbpal.bmp", g_pInstallPathPrefix);
    DAT_004ff110->Load(szThumbPalPath, 1, 0, 0);
    if (DAT_004ff110 != NULL) {
        return true;
    }
    return false;
}

// 0x45c520 -- the multimedia-timer callback the bootstrap arms at 28 ms, and the thing that
// actually pumps a frame (by raising the DAT_00485444 latch the in-game loop gates on; the
// pump itself is FrameDriver_TickMaybe, and both bodies live in src/FrameDriver.cpp). Ghidra
// types it `void (void)` because the body ignores every callback argument, but timeSetEvent's
// LPTIMECALLBACK is what the address is handed to.
void CALLBACK GameLoopTimerProcMaybe(UINT uId, UINT uMsg, DWORD dwUser,
                                     DWORD dw1, DWORD dw2);                                // 0x45c520  // TODO: idiom

// The frame-pump event the main loop resets after each pumped frame (src/Main.cpp holds the
// other declaration), and the timeSetEvent id the 0x406ba0 bootstrap arms.
extern HANDLE DAT_004a990c;                            // DAT_004a990c  // TODO: idiom
extern UINT DAT_00485438;                              // DAT_00485438  // TODO: idiom

// The active save's backdrop descriptor, and the loading screen's two animation refs.
extern CursorDesc *g_pBackdropDesc;                    // DAT_004fd3c8  // TODO: idiom
extern AnimDescRefObj0x477488 *DAT_004fd3d4;           // DAT_004fd3d4  // TODO: idiom
extern AnimDescRefObj0x477488 *DAT_004fd3d8;           // DAT_004fd3d8  // TODO: idiom

// 0x42cc60 -- the world idle/event pump the build-mode entry path runs on g_worldLoadThread, the
// same worker AppWindow_StartGame uses for the cold-start load. Not transcribed yet.
void __cdecl WorldIdleEventPumpThreadProc(void *pArg); // TODO: idiom

// The app's install-path / config bootstrap, and the lowest-addressed function in this TU.
// Resolves where the game is installed, self-healing the registry if the key is gone; opens
// lego.ini from there; and reads the two resource-directory settings the whole loose-file
// loader keys off. Its return value gates a hard startup failure in LocoWinMain.
//
// The registry value is read into its own scratch buffer rather than straight into szInstallDir
// because the self-heal path needs szInstallDir untouched to receive _getcwd's answer instead.
//
// PARTIAL -- asmscore 21 (align=0 reg_pen=0 identity_miss=0 byte_diff=21, insns 242/242) at the
// original's exact 712-byte length. EVERY instruction matches, in order, in the same registers;
// the entire residual is ONE stack slot that failed to overlay. The original reuses dwType's
// dword (frame +0x14) for the compiler temp that holds `operator new`'s result between the
// allocation and the ctor call -- Ghidra sees the two as one variable (`local_a3c`, typed
// IniFile* but also passed as RegQueryValueExA's lpType) precisely because they share the slot.
// This compile gives that temp its own dword instead, so the frame is `sub esp,0xa38` rather
// than 0xa34 and all 21 differing bytes are the resulting +4 displacement shifts (0x18->0x14,
// 0x40->0x44, 0x2c->0x30, ...). Nothing else differs.
//
// Refuted probes, do NOT re-grind: dwType at function scope instead of block scope (same 21);
// swapping the dwType/dwSize declaration order (same 21, VC5 picks the order itself); moving
// dwSize into the query block alongside dwType (WORSE, 72). The declaration order of the four
// function-scope locals below IS load-bearing and already correct -- it is what got the frame
// from 0xa38-with-wrong-order to the current byte-exact instruction stream.
//
// The app singleton's constructor and destructor -- the first two functions in this .obj, and
// the reason AppWindow is modelled as a polymorphic class at all: its vtable (0x4774c4) has
// exactly ONE slot, the scalar deleting destructor at 0x4062a0, which is why src/AppWindow.h can
// declare `virtual ~AppWindow()` and nothing else and still reproduce every `push 1; call [vtbl]`
// delete site. The `char padVtbl[4]` that used to hold offset 0 open is gone: the compiler's own
// vptr now occupies it, so sizeof stays 0x28 and every field offset is unchanged.
//
// FUNCTION: LOCO 0x4061e0
AppWindow::AppWindow(HINSTANCE hInstance)
{
    this->unk0x10 = 0;
    g_uLoadingTimerId = 0;
    this->hwndOwner = NULL;
    this->hInstance = hInstance;
    this->hwndDesktopWindow = GetDesktopWindow();

    if (DAT_00485234 != 0) {
        DAT_00485234 = 0;
        DAT_004aa7dc = 0;
        PlacementCursorMaybe_004854c8.nTypeIdMaybe = -1;
    }

    g_nScreenState = 1;
    g_bAppMinimizedMaybe = 0;
    g_bScrollBarsVisible = 0;
    g_bBoardScrollFlag = 0;
    SetRect(&g_rectScreenBounds, 0, 0, 0, 0);
    g_dwScreenWidth = 0;
    g_dwScreenHeight = 0;
    g_dwScreenHalfWidth = 0;
    g_dwScreenHalfHeight = 0;
    SetRect(&g_rectAppWindowBounds, 0, 0, 0, 0);
    SetRect(&g_rectAppClientBounds, 0, 0, 0, 0);

    this->dwFileVersionMajor = 0;
    this->dwFileVersionMinor = 0;
    this->dwFileVersionBuild = 0;
    this->dwFileVersionRevision = 0;

    ReadOwnFileVersion();
}

// The app singleton's destructor is DEFINED IN-CLASS in src/AppWindow.h, which is load-bearing:
// the original's 0x4062a0 is the compiler-generated scalar-deleting-dtor thunk with the dtor body
// INLINED into it (vptr re-stamp, IniFile teardown, then the `test [esp+8],1` delete flag and
// `ret 4`) and no separate `??1` COMDAT anywhere in the image. An out-of-line dtor here instead
// produces the two-COMDAT shape -- a 33-byte `??1` plus a 30-byte `??_G` thunk that calls it --
// which cannot match at either address. Defining it in the class body is what makes VC5 inline
// it, and it is the reason this header includes IniFile.h.
//
// FUNCTION: LOCO 0x4062a0 (??_GAppWindow@@ scalar deleting dtor -- compiler-generated)

// Pull the app's own FileVersion out of its VERSIONINFO resource and split it into the four
// dwFileVersion* fields. This is what those fields ARE -- src/AppWindow.h used to record them as
// four opaque dwords of unknown meaning because their only other appearance is
// GameNetThreadState::GameNet_DispatchMessage copying them verbatim into the 0x3e9 "who are you"
// handshake reply. They are the protocol/build version the peer is told about.
//
// The VERSIONINFO string is the comma-separated "a, b, c, d" form, so the delimiter set is ", ".
// Note the strtok continuation idiom: instead of the usual NULL, each subsequent call restarts
// explicitly at `pszTok + strlen(pszTok) + 1` -- one past the NUL strtok just wrote over the
// delimiter, i.e. exactly where the internal cursor already was. It works, but it is the author's
// own hand-rolled continuation rather than strtok's, and it is byte-pinned by the four
// `lea eax,[esi+ecx*1+0x1]` sites.
//
// PARTIAL -- len 416 (exact), insns 146/148, align=16, byte_diff=17. Structurally complete; the
// residual is two instructions plus three SIB coin-flips:
//   * The original emits a SECOND `test ebp,ebp; je` immediately before `operator delete`, i.e. a
//     null check that is provably redundant inside the enclosing `if (pInfo != NULL)`. Every shape
//     tried here gets that check FOLDED by VC5. Refuted, do NOT re-grind: an explicit
//     `if (pInfo != NULL) ::operator delete(pInfo);` (folds), `delete pInfo;` inside the guard
//     (folds), the same hoisted OUT of the guard to a sibling statement (folds, and loses 4 more
//     bytes), and `new BYTE[n]` / `delete [] pInfo` (identical to the scalar forms -- the array
//     operators are ICF-folded onto 0x465ce0/0x465cd0 anyway).
//   * `mov [esp+0x1c],ebp` vs `mov [esp+0x1c],0`: the original reuses the already-zeroed pInfo
//     register for lpValue's NULL init. The declaration order below IS load-bearing and already
//     the best of the two -- swapping pInfo above lpValue costs 4 bytes, not gains.
//   * Three `lea eax,[esi+ecx+1]` vs `lea edx,[ecx+esi+1]`: the same SIB base/index coin-flip as
//     LoadWindowAndBalancing above (v329 class), here with a destination-register rotation on top.
//
// FUNCTION: LOCO 0x4062e0
void AppWindow::ReadOwnFileVersion()
{
    char szVersion[4096] = "";
    char szFileName[1284];
    DWORD dwHandle;
    UINT cbValue;
    LPVOID lpValue = NULL;
    BYTE *pInfo = NULL;

    GetModuleFileNameA(GetModuleHandleA(NULL), szFileName, sizeof(szFileName));

    DWORD dwInfoSize = GetFileVersionInfoSizeA(szFileName, &dwHandle);
    if (dwInfoSize != 0) {
        pInfo = (BYTE *)::operator new(dwInfoSize);
    }
    if (pInfo != NULL) {
        if (GetFileVersionInfoA(szFileName, 0, dwInfoSize, pInfo)) {
            if (VerQueryValueA(pInfo, "\\StringFileInfo\\080904B0\\FileVersion", &lpValue,
                               &cbValue) && cbValue > 0) {
                strcpy(szVersion, (char *)lpValue);
            }
        }
        delete pInfo;
    }

    if (strlen(szVersion) != 0) {
        char *pszTok = strtok(szVersion, ", ");
        this->dwFileVersionMajor = atoi(pszTok);
        pszTok = strtok(pszTok + strlen(pszTok) + 1, ", ");
        this->dwFileVersionMinor = atoi(pszTok);
        pszTok = strtok(pszTok + strlen(pszTok) + 1, ", ");
        this->dwFileVersionBuild = atoi(pszTok);
        pszTok = strtok(pszTok + strlen(pszTok) + 1, ", ");
        this->dwFileVersionRevision = atoi(pszTok);
    }
}

// Re-read the desktop geometry and everything in lego.ini that depends on it. Called once from
// LocoWinMain, after the ini file exists and before the main window is created.
//
// The window rect is clamped in two independent steps per axis, and the second step is NOT a
// symmetric clamp of the first: the origin is snapped to 10 if it is off-screen at all, then the
// EXTENT is capped so the window is at most (screen - 10) wide -- which is why the two `10`s do
// different jobs and why the right/bottom fix-ups are written relative to the already-corrected
// left/top. The [BALANCING] quartet is the per-category animation frame-rate floor the actor
// updaters throttle against; each is stored back into the app singleton as a byte.
//
// EFFECTIVE MATCH -- insns 130/130, align=0, byte_diff=2. The entire residual is the operand
// order inside the two extent-clamp LEAs: the original encodes `lea edx,[ecx+edx*1-0xa]` (SIB
// base = the clamped left/top, index = the screen extent), this compile picks the other operand
// as the SIB base and emits `lea edx,[edx+ecx-0xa]`. Same instruction, same length, same result.
// Refuted probes, do NOT re-grind: parenthesizing the extent as `left + (width - 10)` (no
// change), and writing the sum in the reverse source order `width - 10 + left` (no change) --
// VC5 is not taking the base/index assignment from source operand order here. The v329 LEA
// scheduling-swap / register coin-flip class.
//
// FUNCTION: LOCO 0x406480
void AppWindow::LoadWindowAndBalancing()
{
    this->hwndDesktopWindow = GetDesktopWindow();
    g_dwScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    g_dwScreenHeight = GetSystemMetrics(SM_CYSCREEN);
#ifdef LOCO_PORT
    // PORT: the game treats these two globals AS the screen -- main window size,
    // emulated primary size, viewport maths -- and CheckMinimumDisplaySpec rejects
    // any width outside 800..1280. Clamp once, here, before anything derives from
    // them; see port/PortMode.h.
    Port_ClampScreenSize(&g_dwScreenWidth, &g_dwScreenHeight);
#endif
    g_dwScreenHalfWidth = g_dwScreenWidth / 2;
    g_dwScreenHalfHeight = g_dwScreenHeight / 2;
    SetRect(&g_rectScreenBounds, 0, 0, g_dwScreenWidth, g_dwScreenHeight);
    g_bAppMinimizedMaybe = 0;
    g_bScrollBarsVisible = 0;
    g_bBoardScrollFlag = 0;

    g_rectAppWindowBounds.left =
        g_pIniFile->ReadInt("WINDOW_ATTRIBUTES", "RectLeft", 50);
    g_rectAppWindowBounds.top =
        g_pIniFile->ReadInt("WINDOW_ATTRIBUTES", "RectTop", 50);
    g_rectAppWindowBounds.right =
        g_pIniFile->ReadInt("WINDOW_ATTRIBUTES", "RectRight", g_dwScreenWidth - 50);
    g_rectAppWindowBounds.bottom =
        g_pIniFile->ReadInt("WINDOW_ATTRIBUTES", "RectBottom", g_dwScreenHeight - 50);

    if (g_rectAppWindowBounds.left < 0 || g_rectAppWindowBounds.left > g_dwScreenWidth) {
        g_rectAppWindowBounds.left = 10;
    }
    if (g_rectAppWindowBounds.right - g_rectAppWindowBounds.left > g_dwScreenWidth - 10) {
        g_rectAppWindowBounds.right = g_rectAppWindowBounds.left + (g_dwScreenWidth - 10);
    }
    if (g_rectAppWindowBounds.top < 0 || g_rectAppWindowBounds.top > g_dwScreenHeight) {
        g_rectAppWindowBounds.top = 10;
    }
    if (g_rectAppWindowBounds.bottom - g_rectAppWindowBounds.top > g_dwScreenHeight - 10) {
        g_rectAppWindowBounds.bottom = g_rectAppWindowBounds.top + (g_dwScreenHeight - 10);
    }

    this->minVehicleFps = (unsigned char)g_pIniFile->ReadInt("BALANCING", "MinVehicleFPS", 20);
    this->minBuildingFps = (unsigned char)g_pIniFile->ReadInt("BALANCING", "MinBuildingFPS", 18);
    this->minMinifigFps = (unsigned char)g_pIniFile->ReadInt("BALANCING", "MinMinifigFPS", 16);
    this->minFlyingFps = (unsigned char)g_pIniFile->ReadInt("BALANCING", "MinFlyingFPS", 14);

    g_bCleanExit = (char)g_pIniFile->ReadInt("PROCESS", "CleanExit", 1);
    g_pIniFile->WriteInt("PROCESS", "CleanExit", 0);
}

// The minimum-spec gate, shown once at startup: the display must be a non-palette device
// (GetDeviceCaps(NUMCOLORS) returns -1 above 8bpp) of at most 16bpp, there must be a mouse, and
// the desktop must be between 800 and 1280 pixels wide. Any failure puts up a localized
// MessageBoxA and returns 0, which aborts the launch. String 0x7a is the display-mode complaint
// and 0x7b the no-mouse one -- three of the four failure paths share 0x7a, which is why the
// compiler cross-jumps them into one tail.
//
// FUNCTION: LOCO 0x406680
char AppWindow::CheckMinimumDisplaySpec()
{
    char szMessage[256];

    HDC hdc = GetDC(this->hwndDesktopWindow);
    int nNumColors = GetDeviceCaps(hdc, NUMCOLORS);
    g_dwScreenBpp = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(this->hwndDesktopWindow, hdc);

#ifdef LOCO_PORT
    // PORT: the original demands a 16bpp desktop because it renders 16bpp and blits
    // straight to the real primary. The port keeps the engine 16bpp but gives it an
    // emulated 565 primary and presents through GDI (port/PortMode.h), so any
    // non-palette desktop of at least 16bpp is fine. Still reject 8bpp: a palette
    // device has no 16-bit surface format to emulate INTO cheaply, and the engine
    // has no 8bpp path at all.
    if (nNumColors > -1 && g_dwScreenBpp < 16) {
        g_UIResources.LoadLocaleString(0x7a, szMessage, sizeof(szMessage));
        MessageBoxA(NULL, szMessage, "LEGO LOCO", 0);
        return 0;
    }
#else
    if (nNumColors > -1 || g_dwScreenBpp > 16) {
        g_UIResources.LoadLocaleString(0x7a, szMessage, sizeof(szMessage));
        MessageBoxA(NULL, szMessage, "LEGO LOCO", 0);
        return 0;
    }
#endif
    if (GetSystemMetrics(SM_MOUSEPRESENT) == 0) {
        g_UIResources.LoadLocaleString(0x7b, szMessage, sizeof(szMessage));
        MessageBoxA(NULL, szMessage, "LEGO LOCO", 0);
        return 0;
    }
    if (g_dwScreenWidth > 1280) {
        g_UIResources.LoadLocaleString(0x7a, szMessage, sizeof(szMessage));
        MessageBoxA(NULL, szMessage, "LEGO LOCO", 0);
        return 0;
    }
    if (g_dwScreenWidth < 800) {
        g_UIResources.LoadLocaleString(0x7a, szMessage, sizeof(szMessage));
        MessageBoxA(NULL, szMessage, "LEGO LOCO", 0);
        return 0;
    }
    return 1;
}

// The command-line scanner, called once from LocoWinMain before anything is constructed. Two
// independent things come off the line: the `-s` screen-saver switch (which the Windows control
// panel passes when it launches a .scr, and which the app also accepts on its own command line),
// and an optional season name that overrides the calendar-derived one the easter-egg manager
// would otherwise pick (0x41f970 is the sole reader of g_forcedSeason).
//
// sic: the delimiter set is " /", so '/' is consumed as a separator and no token can ever begin
// with it -- which makes the `"/s"` comparison below permanently dead. The original tests it
// anyway, first of the three, and the redundant call is in the shipped code.
//
// FUNCTION: LOCO 0x406790
void AppWindow_ParseCommandLine(char *pszCmdLine)
{
    char bScreenSaverSwitch = 0;

    g_forcedSeason = 0;
    for (char *pszTok = strtok(pszCmdLine, " /"); pszTok != 0; pszTok = strtok(0, " /")) {
        if (_stricmp(pszTok, "/s") == 0 || _stricmp(pszTok, "-s") == 0 ||
            _stricmp(pszTok, "s") == 0) {
            bScreenSaverSwitch = 1;
        } else if (_stricmp(pszTok, "Easter") == 0) {
            g_forcedSeason = 1;
        } else if (_stricmp(pszTok, "Desert") == 0) {
            g_forcedSeason = 2;
        } else if (_stricmp(pszTok, "Halloween") == 0) {
            g_forcedSeason = 3;
        } else if (_stricmp(pszTok, "Winter") == 0) {
            g_forcedSeason = 4;
        } else if (_stricmp(pszTok, "XMas") == 0) {
            g_forcedSeason = 5;
        }
    }
    if (bScreenSaverSwitch) {
        g_screenSaver.bScreenSaverMode = 1;
    }
}

// FUNCTION: LOCO 0x4068d0
unsigned char AppWindow::LoadConfigDirectories()
{
    // Declaration order is load-bearing: VC5 lays these out last-declared-lowest, reproducing
    // the original's frame slots in order (hKey, dwType, dwSize, st, szInstallDir, abRegValue
    // running upward). The two buffer sizes are read straight off that frame: szInstallDir
    // spans +0x40..+0x53f and abRegValue +0x540..+0xa43, and 0x504 is also what the original
    // seeds dwSize with, so the odd-looking 1284 is the real declared size, not a guess.
    BYTE abRegValue[1284];
    char szInstallDir[1280];
    struct _stat st;
    DWORD dwSize = sizeof(abRegValue);
    HKEY hKey;

    // Each Reg* status goes through its own named local: that is what makes VC5 test it against
    // the function's live zero register (`cmp eax,ebx`) instead of `test eax,eax` -- see
    // docs/CODEGEN.md's named-local-vs-inline-call-result rule. Keeping them SEPARATE (rather
    // than reusing one status across both checks) is what keeps each one dead after its own
    // test, so none of them costs a save across the intervening call.
    LONG lOpen = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                               "SOFTWARE\\Intelligent Games\\LEGO LOCO\\Path", 0, KEY_READ,
                               &hKey);
    if (lOpen == ERROR_SUCCESS) {
        DWORD dwType = 0;
        LONG lQuery = RegQueryValueExA(hKey, NULL, NULL, &dwType, abRegValue, &dwSize);
        RegCloseKey(hKey);
        if (lQuery == ERROR_SUCCESS) {
            strcpy(szInstallDir, (char *)abRegValue);
            goto haveInstallDir;
        }
    }

    // No usable key -- neither status is kept past its own test, which is why this fallback is
    // reached by a jump from two places rather than sitting in an `else`. Fall back to the
    // working directory and write it back so the next run finds the key.
    {
        szInstallDir[0] = '\0';
        _getcwd(szInstallDir, 256);
        // Measured before the create call and incremented in place to cover the terminator,
        // which is what the original's dec-then-inc pair around the inlined strlen encodes.
        DWORD cbValue = strlen(szInstallDir);
        cbValue++;
        LONG lCreate = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                                       "SOFTWARE\\Intelligent Games\\LEGO LOCO\\Path", 0, NULL,
                                       0, KEY_ALL_ACCESS, NULL, &hKey, NULL);
        if (lCreate == ERROR_SUCCESS) {
            RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE *)szInstallDir, cbValue);
            RegCloseKey(hKey);
        }
    }

haveInstallDir:

    strcat(szInstallDir, "\\");
    strcat(szInstallDir, "lego.ini");
    g_pIniFile = new IniFile(szInstallDir);

    g_pIniFile->ReadString("DIRECTORIES", "Res", ".\\", g_pInstallPathPrefix, 256);
    g_pIniFile->ReadString("DIRECTORIES", "RemoteRes", "", g_szRemoteResPath, 256);
    // Attract mode never serves from the remote share.
    if (g_screenSaver.bScreenSaverMode == 1) {
        g_szRemoteResPath[0] = '\0';
    }

    int nRemote = strlen(g_szRemoteResPath);
    if (nRemote == 0 || g_szRemoteResPath[nRemote - 1] != '\\') {
        strcat(g_szRemoteResPath, "\\");
    }

    // The install prefix is validated WITHOUT its trailing separator (_stat on "C:\Foo\" fails
    // on Win32), then gets one put back for the rest of the run. nPrefix is the length actually
    // checked, so a bare drive root ("C:\" -> "C:") fails the > 2 test the same way an empty
    // setting does.
    // Unsigned is load-bearing: the original's `> 2` test compiles to jbe, not jle.
    unsigned int nPrefix = strlen(g_pInstallPathPrefix);
    if (g_pInstallPathPrefix[nPrefix - 1] == '\\') {
        g_pInstallPathPrefix[nPrefix - 1] = '\0';
        nPrefix--;
    }
    char bOk = (_stat(g_pInstallPathPrefix, &st) == 0 && nPrefix > 2);
    strcat(g_pInstallPathPrefix, "\\");
    return bOk;
}

// The app's whole "Run" startup, and the exact construction mirror of SaveWindowAndCleanExit
// (0x4077a0) below. LocoWinMain calls it once, right after the splash window is up; a nonzero
// return is turned straight into the UIResources string 0x14a error box and a clean exit, so
// every failure path here reports -1 except the singleton-window constructor, whose own -2..-17
// codes are passed through untouched.
//
// Order is: park the app in screen state 0; seed the RNG off the wall clock; null every window
// and GameNet queue pointer that the teardown path will later test; construct the six heap-owned
// subsystem singletons; read the three [MOUSE] acceleration settings; create the main window;
// size the board's tile grid to it; load ee.ini's event scripts; bring the UI-resource layer up;
// construct the eight singleton windows; restore the "~curr" checkpoint; load the thumbnail
// palette; and finally create the frame-pump event and arm the 28 ms multimedia timer that
// drives it.
//
// Ghidra reads this as __thiscall AppWindow::InitSubsystemsAndWindows and it does use `this` (it
// is the AppWindow* both CreateMainWindow and ConstructSingletonWindows are called on), but it is
// spelled as a free __fastcall here for the same reason those two are -- see src/AppWindow.h's
// MEASURED DIAL note.
//
// EFFECTIVE -- DIFF(8) at the original's exact 723 bytes, asmscore insns 209/209 with reg_pen=0
// and identity_miss=0: every instruction is present, in the same registers, and only THREE of
// them sit in a different slot. All three are the same phenomenon -- VC5 defers a reloc-bearing
// non-push instruction until AFTER the argument-push group, and this compile emits it before:
//   * `mov ds:mouseAccel[0],eax` and `mov ds:mouseAccel[1],eax` land right after the
//     `mov ecx,[g_pIniFile]` instead of immediately before the following `call ReadInt`;
//   * `mov ecx,OFFSET g_easterEggMgrMaybe` lands before `push OFFSET "ee"` instead of after.
// The other two stores in the same basic block (g_pPostBagFileCache's, and mouseAccel[2]'s) DO
// sink correctly, so VC5 clearly can emit the original's schedule here; a repo-wide scan settles
// it further -- push-before-`mov ecx,imm32` occurs 152 times across the built objects and the
// reversed order exactly ONCE, in this function. src/UIResources.cpp compiles the byte-identical
// construct `g_easterEggMgrMaybe.LoadUnlockTableMaybe("ee")` the RIGHT way round, so the lever is
// this function's own scheduler context, not the call's spelling.
//
// Refuted probes, do NOT re-grind (each a single compile, all DIFF(8) unchanged): three plain
// `extern int` globals instead of the PlacementCursorMaybe member (kills the "global aggregate
// member" aliasing theory); intermediate `int nSetN` locals between the call and the store;
// `void`/`char *` instead of `char`/`const char *` on LoadEventScriptsMaybe; dropping <stdlib.h>
// and <time.h> for hand-written srand/time declarations (kills the declaration-dial theory);
// spelling UIResources::Init as a real member call instead of the __fastcall escape hatch (it
// shares the easter-egg call's basic block, so it was the best structural candidate).
//
// FUNCTION: LOCO 0x406ba0
int AppWindow::InitSubsystemsAndWindows()
{
    AppWindow_SetScreenState(0);
    srand(time(NULL));

    g_pSplashWnd = NULL;
    g_pMailWnd = NULL;
    g_pMapWnd = NULL;
    g_pEditCardWnd = NULL;
    g_pAlbumCardWnd = NULL;
    g_pGameNetThread = NULL;
    g_pNetMsgSendQueueHead = NULL;
    g_pNetMsgLocalQueueHead = NULL;
    g_pGameNetThreadState = NULL;

    // Six heap singletons, in the order SaveWindowAndCleanExit later deletes them. Each is a
    // plain `g_p = new T;`: under /GX that is the allocate / null-test / construct / store shape
    // the original shows, with the frame's unwind state stepped -1 -> N -> -1 around each ctor so
    // a throwing ctor frees the raw block.
    g_pGameNetMsgQueueLock = new LockableMaybe;
    g_pNetSettings = new NetSettings;
    g_pDPlaySessionMgr = new DPlaySessionMgr;
    g_pPostBagCache = new PostBagCacheBundle;
    g_pLocalPlayerIdentity = new LocalPlayerIdentity;
    g_pPostBagFileCache = new PostBagFileCache;

    PlacementCursorMaybe_004854c8.mouseAccelDisableMaybe[0] =
        g_pIniFile->ReadInt("MOUSE", "Setting1", 0);
    PlacementCursorMaybe_004854c8.mouseAccelDisableMaybe[1] =
        g_pIniFile->ReadInt("MOUSE", "Setting2", 0);
    PlacementCursorMaybe_004854c8.mouseAccelDisableMaybe[2] =
        g_pIniFile->ReadInt("MOUSE", "Setting3", 0);

    if (!CreateMainWindow()) {
        return -1;
    }

    g_worldBoard.Ddraw_InitTileGridExtent(0);
    g_easterEggMgrMaybe.LoadEventScriptsMaybe("ee");

    // Each of the three remaining status checks goes through its own named local, which is what
    // makes VC5 test it against the function's live zero register (`cmp al,bl`) rather than
    // `test al,al` -- see AppWindow_LoadConfigDirectories above for the same rule.
    unsigned char bResourcesUp = g_UIResources.Init();
    if (bResourcesUp == 0) {
        return -1;
    }

    int nWindowsRc = AppWindow_ConstructSingletonWindows(this);
    if (nWindowsRc != 0) {
        return nWindowsRc;
    }

    g_BuildToolButton.regionBMaybe.ReloadActiveSaveState("");

#ifdef LOCO_PORT
    Port_Tracef("boot: reloadActiveSaveState done\n");
#endif
    bool bThumbPalUp = LocoBitmap_LoadThumbPalSingletonMaybe();
    if (!bThumbPalUp) {
        return -1;
    }

    DAT_004a990c = CreateEventA(NULL, TRUE, FALSE, "GameLoop");
    if (DAT_004a990c == NULL) {
        return -1;
    }
#ifdef LOCO_PORT
    Port_Tracef("boot: thumbpal+event ok\n");
#endif

    // 14 ms is the finest period the timer service is asked to honour, and the pump itself runs
    // at 28 ms (~36 Hz). A refused timeBeginPeriod simply leaves the pump unarmed rather than
    // failing startup.
    MMRESULT mmr = timeBeginPeriod(14);
    if (mmr == TIMERR_NOERROR) {
        DAT_00485438 = timeSetEvent(28, 14, GameLoopTimerProcMaybe, 0, TIME_PERIODIC);
    }
    return 0;
}

// The multiplayer-abort / return-to-front-end path: tear the DirectPlay session down (if one is
// up), raise the still-unmodeled g_pApp+0x10 flag, duck the sound manager, wipe the board and the
// peer train-slot registry, then hand the app over to screen state 2. Reached from
// src/DPlaySessionMgr.cpp after a "connection lost"-shaped notice.
//
// `this` is never read -- see src/AppWindow.h for why it is nonetheless a member (every call site
// loads ecx = g_pApp), and note that the body reaches the singleton through the g_pApp global
// instead.
//
// FUNCTION: LOCO 0x406e80
void AppWindow::AbortMultiplayerSession()
{
    if (g_pDPlaySessionMgr != 0) {
        GameNet_TeardownAllSessionState(g_pDPlaySessionMgr);
    }
    g_pApp->unk0x10 = 1;
    if (g_pDSoundManager != 0) {
        g_pDSoundManager->DSound_SetTemporaryDuck(true);
    }
    g_worldBoard.ResetAllTiles();
    g_PeerTrainSlotQueue.ResetAllFields();
    AppWindow_SetScreenState(2);
}

// src/Main.cpp's main window procedure (Ghidra: Main::AppWndProc, 0x4618c0). There is no Main.h to
// declare it in.
LRESULT CALLBACK AppWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// The app's window class ("LEGO LOCO") and its one top-level window, created screen-sized at the
// origin as a borderless clipping popup -- WS_EX_TOPMOST only in screen-saver (attract) mode, where
// nothing may draw over us. Caches the resulting client rect into g_rectAppClientBounds and returns
// whether the window came up.
//
// Ghidra reads this as __thiscall AppWindow::CreateMainWindow and it genuinely does use `this` --
// see src/AppWindow.h's MEASURED DIAL note for why it is nonetheless a free __fastcall taking the
// singleton by pointer rather than a member of that class.
//
// EFFECTIVE -- asmscore 87820 (align=86 reg_pen=16 identity_miss=15 byte_diff=70, insns 64/64) and
// 193 bytes against the original's 192. Content-complete: the instruction SEQUENCE is identical and
// the only real difference is which register carries the dwExStyle select. VC5 hoists three of the
// four non-immediate CreateWindowExA arguments into registers before the pushes begin and defers the
// fourth to a register recycled by the hInstance push; the original defers nWidth and gives eax to
// the select, this compile defers the select and gives eax to nWidth. eax is what buys the original
// its 192nd byte: the mask step encodes as `and al, 0xf8` (2 bytes) there and as `and ecx, -8`
// (3 bytes) here, and the identity_miss rows are that one choice cascading.
//
// Refuted probes, do NOT re-grind: hoisting the select into its own `DWORD dwExStyle` statement
// before the call, and writing it `== 1 ? WS_EX_TOPMOST : 0` instead of `!= 1 ? 0 : WS_EX_TOPMOST`
// (both bit-identical). The `g_screenSaver.bScreenSaverMode` member spelling is not the cause
// either -- src/ScreenSaver.h documents that a member access from another TU is exactly what
// compiles to the absolute load the original shows. Modeling it as a real __thiscall member scores
// slightly better (51473) but is not available; see the header note.
//
// FUNCTION: LOCO 0x406ed0
char AppWindow::CreateMainWindow()
{
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = AppWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = this->hInstance;
    wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCE(101));
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "LEGO LOCO";
    RegisterClassA(&wc);

    // Written `!= 1` (not `== 1`) because that is the predicate VC5's dec/neg/sbb select-between-
    // two-constants sequence computes; see docs/CODEGEN.md.
    this->hwndOwner = CreateWindowExA(
        g_screenSaver.bScreenSaverMode == 1 ? WS_EX_TOPMOST : 0,
        "LEGO LOCO", "LEGO LOCO",
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, g_dwScreenWidth, g_dwScreenHeight,
        NULL, NULL, this->hInstance, NULL);
    if (this->hwndOwner == NULL) {
        return 0;
    }
    GetClientRect(this->hwndOwner, &g_rectAppClientBounds);
#ifdef LOCO_PORT
    // PORT ONLY, byte-neutral. g_rectAppClientBounds is what the front-end/loading-screen
    // presenter blits (LoadingScreen.cpp's Ddraw_BltUpdateRect(&g_rectAppClientBounds, ...)),
    // so a wrong value here is a black front end with every gate upstream reading healthy --
    // the same failure shape as v566's dirty-bitmap table. Print the requested size beside the
    // one the window actually got.
    Port_Tracef("app owner hwnd=%p want=%ux%u client=%ld,%ld,%ld,%ld\n",
                (void *)this->hwndOwner, g_dwScreenWidth, g_dwScreenHeight,
                g_rectAppClientBounds.left, g_rectAppClientBounds.top,
                g_rectAppClientBounds.right, g_rectAppClientBounds.bottom);
#endif
    return 1;
}

// FUNCTION: LOCO 0x406f90
int __fastcall AppWindow_ConstructSingletonWindows(AppWindow *pApp)
{
    // 1. Splash
    g_pSplashWnd = new SplashWnd(pApp->hInstance, 0x1f8);
    if (g_pSplashWnd == 0) {
        return -2;
    }
    if (!g_pSplashWnd->Create(pApp->hwndOwner)) {
        delete g_pSplashWnd;
        return -3;
    }

    // 2. Mail
    g_pMailWnd = new MailWnd(pApp->hInstance, 0x1f5);
    if (g_pMailWnd == 0) {
        delete g_pSplashWnd;
        return -4;
    }
    if (!g_pMailWnd->Create(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        return -5;
    }

    // 3. Map
    g_pMapWnd = new MapWnd(pApp->hInstance, 0x1f7);
    if (g_pMapWnd == 0) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        return -6;
    }
    if (!((FullscreenPopupWndPartial *)g_pMapWnd)->CreateFullscreenPopupWnd(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        return -7;
    }

    // 4. BuildToolCursor
    g_pBuildToolCursorWnd = new BuildToolCursorWnd(pApp->hInstance, 0x1fc);
    if (g_pBuildToolCursorWnd == 0) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        return -8;
    }
    if (!g_pBuildToolCursorWnd->Create(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        return -9;
    }

    // 5. AlbumCard
    g_pAlbumCardWnd = new AlbumCardWnd(pApp->hInstance, 0x1fb);
    if (g_pAlbumCardWnd == 0) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        return -10;
    }
    if (!((FullscreenPopupWndPartial *)g_pAlbumCardWnd)->CreateFullscreenPopupWnd(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        return -11;
    }

    // 6. EditCard
    g_pEditCardWnd = new EditCardWnd(pApp->hInstance, 0x1fa);
    if (g_pEditCardWnd == 0) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        return -12;
    }
    if (!g_pEditCardWnd->Create(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        delete g_pEditCardWnd;
        return -13;
    }

    // 7. Tutorial
    g_pTutorialWnd = new TutorialWnd(pApp->hInstance, 0x1fe);
    if (g_pTutorialWnd == 0) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        delete g_pEditCardWnd;
        return -14;
    }
    if (!g_pTutorialWnd->Create(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        delete g_pEditCardWnd;
        delete g_pTutorialWnd;
        return -15;
    }

    // 8. Credits
    g_pCreditsWnd = new CreditsWnd(pApp->hInstance, 0x1fd);
    if (g_pCreditsWnd == 0) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        delete g_pEditCardWnd;
        delete g_pTutorialWnd;
        return -16;
    }
    if (!g_pCreditsWnd->Create(pApp->hwndOwner)) {
        delete g_pMailWnd;
        delete g_pSplashWnd;
        delete g_pMapWnd;
        delete g_pBuildToolCursorWnd;
        delete g_pAlbumCardWnd;
        delete g_pEditCardWnd;
        delete g_pTutorialWnd;
        delete g_pCreditsWnd;
        return -17;
    }

    return 0;
}

// ---------------------------------------------------------------------------------------------
// AppWindow::ApplyDisplayModeMaybe (0x407d20) and the globals it drives.
// ---------------------------------------------------------------------------------------------

// g_bScrollBarsVisible is declared with the rest of this TU's globals at the top of the file --
// LoadWindowAndBalancing (0x406480) clears it well before this point.

// The build-tool mode selector and its companion byte are declared with the rest of this TU's
// globals at the top of the file -- the app ctor (0x4061e0) resets both.

// The app screen-state selector's "the net subsystem is tearing down" test, spelled the same way
// src/MapWnd.cpp spells it -- the original materializes it through xor/sete/test rather than
// branching on the compare directly.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// 0x454fa0 (Ghidra: Ddraw::Ddraw_RecenterViewportOffsetMaybe; __thiscall on the board). Not
// declared in src/WorldBoardMaybe.h -- src/Main.cpp reaches it through its own
// WorldBoardWndProcView0x4618c0 the same way.

// The WM_HSCROLL / WM_VSCROLL board scrollers, an exact mirror-image pair. Each takes the
// originating message's lParam (never read -- AppWndProc passes it at all seven call sites
// anyway) and a signed scroll delta in board pixels, clamps the board's scroll offset into
// [0, nMaxPos] where nMaxPos is how much of the viewport does not fit in the client area,
// repaints, and then mirrors the new offset back into the real scroll bar -- but only when
// there IS one (g_bScrollBarsVisible).
//
// A viewport that already fits (nMaxPos <= 0) returns early WITHOUT repainting.
//
// Both are __thiscall members that never read `this`; see src/AppWindow.h for why they are
// modeled that way rather than as the __stdcall free functions Ghidra reads them as.
//
// FUNCTION: LOCO 0x4077a0
// The whole-app teardown, reached from AppWindow_SetScreenState's case 0xa via WM_CLOSE (and from
// LocoWinMain's own abort path). Both call sites load ecx = g_pApp first, so this is an AppWindow
// member whose body never reads `this` -- the same class as DrainQueuedMouseInput above.
//
// Order is: persist the window rect and the clean-exit flag to lego.ini; stop the GameNet worker
// and the world-load worker; reset the train-slot queue and the board; delete all sixteen owned
// singletons; release the backdrop descriptor; then a tail of no-arg per-singleton shutdowns.
//
// The sixteen deletes are a uniform `if (p) { delete p; p = NULL; }`. VC5 folds `delete`'s own
// implicit null test into the source's `if`, so each compiles to a single `cmp ecx,edi` against
// the hoisted `xor edi,edi` zero register -- except the GameNet worker, where the intervening
// StopThreadAndWait call blocks the fold and both tests survive.
//
// The backdrop descriptor is NOT refcounted here: ReleaseRef (CursorDesc vtable slot 8) always
// runs, but the delete is gated on resourceId == -1, i.e. "only free this descriptor if it is a
// STANDALONE one (as the per-save backdrop is), never an interned tile-kind descriptor".
void AppWindow::SaveWindowAndCleanExit()
{
    g_pIniFile->WriteInt("WINDOW_ATTRIBUTES", "RectLeft", g_rectAppWindowBounds.left);
    g_pIniFile->WriteInt("WINDOW_ATTRIBUTES", "RectTop", g_rectAppWindowBounds.top);
    g_pIniFile->WriteInt("WINDOW_ATTRIBUTES", "RectRight", g_rectAppWindowBounds.right);
    g_pIniFile->WriteInt("WINDOW_ATTRIBUTES", "RectBottom", g_rectAppWindowBounds.bottom);
    g_pIniFile->WriteInt("PROCESS", "CleanExit", 1);

    if (g_pGameNetThread != NULL) {
        g_pGameNetThreadState->StopThreadAndWait();
        delete g_pGameNetThread;
        g_pGameNetThread = NULL;
    }
    while (g_worldLoadThread.IsRunning()) {
        Sleep(100);
    }
    g_PeerTrainSlotQueue.PeerTrainSlotQueueMaybe::TeardownAllSlotsMaybe();
    g_PeerTrainSlotQueue.ResetAllFields();
    WorldBoardMaybe_ResetAllTilesMaybe(&g_worldBoard);

    if (g_pSplashWnd != NULL) {
        delete g_pSplashWnd;
        g_pSplashWnd = NULL;
    }
    if (g_pMailWnd != NULL) {
        delete g_pMailWnd;
        g_pMailWnd = NULL;
    }
    if (g_pAlbumCardWnd != NULL) {
        delete g_pAlbumCardWnd;
        g_pAlbumCardWnd = NULL;
    }
    if (g_pEditCardWnd != NULL) {
        delete g_pEditCardWnd;
        g_pEditCardWnd = NULL;
    }
    if (g_pMapWnd != NULL) {
        delete g_pMapWnd;
        g_pMapWnd = NULL;
    }
    if (g_pBuildToolCursorWnd != NULL) {
        delete g_pBuildToolCursorWnd;
        g_pBuildToolCursorWnd = NULL;
    }
    if (g_pTutorialWnd != NULL) {
        delete g_pTutorialWnd;
        g_pTutorialWnd = NULL;
    }
    if (g_pCreditsWnd != NULL) {
        delete g_pCreditsWnd;
        g_pCreditsWnd = NULL;
    }
    if (g_pGameNetThreadState != NULL) {
        delete g_pGameNetThreadState;
        g_pGameNetThreadState = NULL;
    }
    if (g_pNetSettings != NULL) {
        delete g_pNetSettings;
        g_pNetSettings = NULL;
    }
    if (g_pDPlaySessionMgr != NULL) {
        delete g_pDPlaySessionMgr;
        g_pDPlaySessionMgr = NULL;
    }
    if (g_pPostBagCache != NULL) {
        delete g_pPostBagCache;
        g_pPostBagCache = NULL;
    }
    if (g_pPostBagFileCache != NULL) {
        delete g_pPostBagFileCache;
        g_pPostBagFileCache = NULL;
    }
    if (g_pGameNetMsgQueueLock != NULL) {
        delete g_pGameNetMsgQueueLock;
        g_pGameNetMsgQueueLock = NULL;
    }
    if (g_pLocalPlayerIdentity != NULL) {
        delete g_pLocalPlayerIdentity;
        g_pLocalPlayerIdentity = NULL;
    }
    if (g_pIniFile != NULL) {
        delete g_pIniFile;
        g_pIniFile = NULL;
    }
    if (DAT_004fd3d4 != NULL) {
        delete DAT_004fd3d4;
        DAT_004fd3d4 = NULL;
    }
    if (DAT_004fd3d8 != NULL) {
        delete DAT_004fd3d8;
        DAT_004fd3d8 = NULL;
    }
    if (g_pBackdropDesc != NULL) {
        g_pBackdropDesc->ReleaseRef();
        if (g_pBackdropDesc->resourceId == -1) {
            delete g_pBackdropDesc;
        }
        g_pBackdropDesc = NULL;
    }
    LocoBitmap_FreeSharedPalette();
    if (DAT_004a990c != NULL) {
        CloseHandle(DAT_004a990c);
        DAT_004a990c = NULL;
    }
    timeKillEvent(DAT_00485438);
    timeEndPeriod(14);

    WorldBoardMaybe_ShutdownMaybe(&g_worldBoard);
    SelectedObjWidgetMaybe_004852a0.SelectedObjWidgetMaybe::ClearOwned();
    g_worldActionCursor.WorldActionCursor::ClearOwned();
    g_BuildToolButton.BuildToolButton::ClearOwned();
    EffectSpawner_ShutdownMaybe(&DAT_004fd220);
    g_easterEggMgrMaybe.DeleteAllEventRecordsMaybe();
    NetSessionEventQueue_ShutdownMaybe(&g_NetSessionEventQueue);
    PlacementCursorMaybe_004854c8.ShutdownMaybe();
    g_UIResources.Shutdown();
    _fcloseall();
}

// FUNCTION: LOCO 0x407ae0
void AppWindow::ScrollBoardHorizontal(LPARAM lParam, int nDelta)
{
    // The whole client rect is copied to a local even though only .left/.right are read: VC5
    // forwards the two loaded values straight into the subtraction and drops their stores, but
    // still emits the dead .top/.bottom ones -- 27 bytes and the `sub esp, 0x10` frame that
    // reading the two globals directly does not produce. Same in the vertical twin below, which
    // needs .top/.bottom and leaves .left/.right dead instead.
    RECT rect = g_rectAppClientBounds;
    int nMaxPos = (rect.left - rect.right) + g_worldBoard.dwViewportWidth;
    if (nMaxPos <= 0) {
        return;
    }

    if (nDelta > 0) {
        if (g_worldBoard.dwScrollX > nMaxPos) {
            g_worldBoard.dwScrollX = nMaxPos;
            nDelta = 1;
        }
        // Re-tested against the value the clamp above may have just written -- the original
        // really does compare twice rather than folding the two into one branch.
        if (g_worldBoard.dwScrollX <= nMaxPos) {
            if (g_worldBoard.dwScrollX + nDelta > nMaxPos) {
                nDelta = nMaxPos - g_worldBoard.dwScrollX;
            }
            g_worldBoard.dwScrollX += nDelta;
        }
    } else {
        if (g_worldBoard.dwScrollX < 0) {
            g_worldBoard.dwScrollX = 0;
        }
        if (g_worldBoard.dwScrollX > 0) {
            if (g_worldBoard.dwScrollX + nDelta < 0) {
                nDelta = -g_worldBoard.dwScrollX;
            }
            g_worldBoard.dwScrollX += nDelta;
        }
    }

    g_worldBoard.Ddraw_RecenterViewportOffsetMaybe();
    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
    if (g_bScrollBarsVisible) {
        SetScrollRange(g_pApp->hwndOwner, SB_HORZ, 0, nMaxPos, FALSE);
        SetScrollPos(g_pApp->hwndOwner, SB_HORZ, g_worldBoard.dwScrollX, TRUE);
    }
}

// FUNCTION: LOCO 0x407bf0
void AppWindow::ScrollBoardVertical(LPARAM lParam, int nDelta)
{
    RECT rect = g_rectAppClientBounds;
    int nMaxPos = (rect.top - rect.bottom) + g_worldBoard.dwViewportHeightMaybe;
    if (nMaxPos <= 0) {
        return;
    }

    if (nDelta > 0) {
        if (g_worldBoard.dwScrollY > nMaxPos) {
            g_worldBoard.dwScrollY = nMaxPos;
            nDelta = 1;
        }
        if (g_worldBoard.dwScrollY <= nMaxPos) {
            if (g_worldBoard.dwScrollY + nDelta > nMaxPos) {
                nDelta = nMaxPos - g_worldBoard.dwScrollY;
            }
            g_worldBoard.dwScrollY += nDelta;
        }
    } else {
        if (g_worldBoard.dwScrollY < 0) {
            g_worldBoard.dwScrollY = 0;
        }
        if (g_worldBoard.dwScrollY > 0) {
            if (g_worldBoard.dwScrollY + nDelta < 0) {
                nDelta = -g_worldBoard.dwScrollY;
            }
            g_worldBoard.dwScrollY += nDelta;
        }
    }

    g_worldBoard.Ddraw_RecenterViewportOffsetMaybe();
    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
    if (g_bScrollBarsVisible) {
        SetScrollRange(g_pApp->hwndOwner, SB_VERT, 0, nMaxPos, FALSE);
        SetScrollPos(g_pApp->hwndOwner, SB_VERT, g_worldBoard.dwScrollY, TRUE);
    }
}

void __cdecl AppWindow_ApplyDisplayModeMaybe(char bFullscreen);

// The 'W' key's windowed/fullscreen toggle: flips whichever mode the board is in right now.
// g_bBoardScrollFlag is raised by the windowed arm of ApplyDisplayModeMaybe and cleared by the
// fullscreen one, so it doubles as "we are currently windowed".
//
// FUNCTION: LOCO 0x407d00
void __stdcall AppWindow_ToggleWindowedModeMaybe(void)
{
    if (g_bBoardScrollFlag == 0) {
        AppWindow_ApplyDisplayModeMaybe(0);
    } else {
        AppWindow_ApplyDisplayModeMaybe(1);
    }
}

// The two window styles this function switches the main window between. Windowed mode is an
// ordinary sizable frame with both scroll bars; fullscreen is a maximized borderless popup, which
// only grows scroll bars when the board is wider than the screen and we are NOT in screen-saver
// (attract) mode.
#define APPWND_STYLE_WINDOWED   (WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | \
                                 WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL)  // 0x92ff0000
#define APPWND_STYLE_FULLSCREEN (WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_MAXIMIZE) // 0x93000000

// Switch the main window between windowed and fullscreen. bFullscreen == 0 gives a sizable,
// captioned window clamped to the board's viewport size (and raises g_bBoardScrollFlag, the
// "board scrolls under the window" flag every popup offsets against); bFullscreen != 0 restores
// the maximized screen-sized popup and clears it. Bound to the 'W' key through the
// AppWindow_ToggleWindowedModeMaybe toggle, and forced either way by the tutorial (src/TutorialWnd.cpp) and the
// WM_USER dispatch in src/Main.cpp.
//
// Both paths bracket themselves with a full-viewport MarkRectDirty so the board repaints under
// the new frame. The GetWindowLongA right after the first one throws its result away -- the
// original really does make the call and ignore it.
//
// PARTIAL -- asmscore 229933 (align=220 reg_pen=88 byte_diff=263, insns 306/305), down from
// 303490 on the first transcription. Structurally complete: the ONE extra instruction is a
// second `xor` in the prologue. The original materializes exactly one zero register (ebx =
// hWndInsertAfter), reuses it for the dword `dwScrollX/dwScrollY = 0` stores and the NULL/FALSE
// pushes, and still spells every BYTE-sized zero as an immediate (`mov byte ptr [g_bBoardScroll
// Flag], 0`, `mov al,[..] / test al,al`). This compile has one register more to spare, so VC5
// builds a SECOND dedicated zero and folds it into the byte ops (`mov [..], bl`, `cmp [..], bl`)
// -- one instruction cheaper per site but one `xor` dearer overall, and the resulting register
// renaming plus the 2-byte length delta is what the whole align=220 residual is: a positional
// cascade, not disagreeing code. The spare register exists because the original also pins
// GetWindowLongA's import slot in edi for the WHOLE function (3 call sites) where this compile
// only caches it for the last two.
//
// Refuted probes, do NOT re-grind (each scored bit-identically at 229933 unless noted):
// hoisting `dwStyle` to function scope seeded by the discarded GetWindowLongA; `!bFullscreen`
// vs `bFullscreen == 0`; declaring `rect` after `hWndInsertAfter`; hoisting `uFlags` to
// function scope; `hWndInsertAfter` instead of `NULL` at the windowed SetWindowPos; `0` instead
// of `NULL`. Actively WORSE: `BOOL bClamped` (241942), an `int` clamp tally (247940), and
// declaring `hWndInsertAfter` down in the fullscreen arm where it is actually used (311861).
//
// FUNCTION: LOCO 0x407d20
void __cdecl AppWindow_ApplyDisplayModeMaybe(char bFullscreen)
{
    RECT rect;
    // Declared up here, not down in the fullscreen arm that is its only real consumer: the
    // original's `xor ebx, ebx` sits in the prologue, and VC5 then borrows that same zero
    // register for the windowed arm's scroll-offset stores and its NULL SetWindowPos argument.
    HWND hWndInsertAfter = NULL;

    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
    g_worldBoard.UpdateDirtyTiles(1);
    GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE);

    if (bFullscreen == 0) {
        g_UIResources.PlayUiSound(0x5465);
        g_bBoardScrollFlag = 1;
        g_bScrollBarsVisible = 1;
        SetWindowLongA(g_pApp->hwndOwner, GWL_STYLE, (LONG)APPWND_STYLE_WINDOWED);

        rect = g_rectAppWindowBounds;
        g_worldBoard.dwScrollX = 0;
        g_worldBoard.dwScrollY = 0;
        SetWindowPos(g_pApp->hwndOwner, NULL, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowScrollBar(g_pApp->hwndOwner, SB_BOTH, TRUE);

        // Shrink the frame back to the board if the window is now bigger than the whole
        // viewport, then re-apply it anchored at the old top-left.
        GetClientRect(g_pApp->hwndOwner, &rect);
        char bClamped = 0;
        if (rect.right - rect.left > g_worldBoard.dwViewportWidth) {
            rect.right = rect.left + g_worldBoard.dwViewportWidth;
            bClamped = 1;
        }
        if (rect.bottom - rect.top > g_worldBoard.dwViewportHeightMaybe) {
            rect.bottom = rect.top + g_worldBoard.dwViewportHeightMaybe;
            bClamped = 1;
        }
        if (bClamped) {
            AdjustWindowRect(&rect, GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE), FALSE);
            rect.bottom += GetSystemMetrics(SM_CYHSCROLL);
            // Two statements, not `right = right + metric + (anchor - left)`: written as one
            // expression VC5 reassociates it into `metric + ((anchor - left) + right)` and folds
            // the call result in last. Splitting it makes the original's `add ecx, eax` (right +
            // metric) land first, worth 14k of asmscore on its own.
            rect.right += GetSystemMetrics(SM_CXVSCROLL);
            rect.right += g_rectAppWindowBounds.left - rect.left;
            rect.bottom += g_rectAppWindowBounds.top - rect.top;
            rect.left = g_rectAppWindowBounds.left;
            rect.top = g_rectAppWindowBounds.top;
            // The anchor is spelled as the global, not the just-assigned rect.left/rect.top --
            // the original keeps both globals live in registers across the two stores.
            SetWindowPos(g_pApp->hwndOwner, NULL, g_rectAppWindowBounds.left,
                         g_rectAppWindowBounds.top, rect.right - g_rectAppWindowBounds.left,
                         rect.bottom - g_rectAppWindowBounds.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            ShowScrollBar(g_pApp->hwndOwner, SB_BOTH, TRUE);
            g_rectAppWindowBounds = rect;
        }
    } else {
        if (g_worldBoard.dwViewportWidth < (int)g_dwScreenWidth) {
            return;
        }
        if (!IsNetShuttingDownMaybe()) {
            g_UIResources.PlayUiSound(0x5467);
        }
        g_worldBoard.dwScrollX = 0;
        g_worldBoard.dwScrollY = 0;
        g_bBoardScrollFlag = 0;

        LONG dwStyle = (LONG)APPWND_STYLE_FULLSCREEN;
        if (g_worldBoard.dwViewportWidth > (int)g_dwScreenWidth) {
            if (g_screenSaver.bScreenSaverMode == 1) {
                g_bScrollBarsVisible = 0;
                ShowScrollBar(g_pApp->hwndOwner, SB_BOTH, FALSE);
            } else {
                g_bScrollBarsVisible = 1;
                dwStyle = (LONG)(APPWND_STYLE_FULLSCREEN | WS_VSCROLL | WS_HSCROLL);
                ShowScrollBar(g_pApp->hwndOwner, SB_BOTH, TRUE);
            }
        } else {
            ShowScrollBar(g_pApp->hwndOwner, SB_BOTH, FALSE);
            g_bScrollBarsVisible = 0;
        }
        SetWindowLongA(g_pApp->hwndOwner, GWL_STYLE, dwStyle);

        SetRect(&rect, 0, 0, g_dwScreenWidth, g_dwScreenHeight);
        AdjustWindowRect(&rect, GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE), FALSE);

        UINT uFlags = SWP_NOACTIVATE;
        // Written negated -- the original lays the SWP_NOZORDER body out FIRST and jumps
        // forward to the HWND_TOPMOST one, which is the `!=` spelling, not the `==` one.
        if (g_screenSaver.bScreenSaverMode != 1) {
            uFlags = SWP_NOZORDER | SWP_NOACTIVATE;
        } else {
            hWndInsertAfter = HWND_TOPMOST;
        }
        SetWindowPos(g_pApp->hwndOwner, hWndInsertAfter, rect.left, rect.top,
                     rect.right - rect.left, rect.bottom - rect.top, uFlags);
    }

    GetClientRect(g_pApp->hwndOwner, &g_rectAppClientBounds);
    g_worldBoard.Ddraw_RecenterViewportOffsetMaybe();

    // With the scroll bars hidden but the board still wider than the screen, attract mode has no
    // way to scroll -- so it parks the view in the middle instead.
    if (g_bScrollBarsVisible == 0 && g_worldBoard.dwViewportWidth > (int)g_dwScreenWidth &&
        g_screenSaver.bScreenSaverMode == 1) {
        g_worldBoard.dwScrollX =
            (g_worldBoard.dwViewportWidth + g_rectAppClientBounds.left -
             g_rectAppClientBounds.right) / 2;
        g_worldBoard.dwScrollY =
            (g_rectAppClientBounds.top + g_worldBoard.dwViewportHeightMaybe -
             g_rectAppClientBounds.bottom) / 2;
    }

    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
}

// THE app's screen-state switch -- every screen transition in the game funnels through here.
// g_nScreenState holds the state the app is currently in; the argument is the state to move TO, and
// a transition to the state already current is a no-op. The old value is captured first and handed
// to whichever arm needs to know what is being LEFT (states 1, 3 and 8 do).
//
// The states:
//   1  start game        -- hand off to AppWindow_StartGame (0x408350).
//   2  front end         -- drop the placement cursor's capture and raise the splash window.
//   3  build mode        -- hand off to AppWindow_EnterBuildMode (0x4086f0).
//   4  auto-curve track  -- a sub-mode of build mode, entered from the toolbar toggle at 0x44a9d0.
//                           Freezes the world: clears the hover object, then quiesces the decor
//                           manager, the effect spawner and the peer train-slot queue with 0, and
//                           clears the board's rebuild flag so nothing redraws under the tool.
//   5  mail              -- duck the sound, drop the cursor capture, raise MailWnd.
//   6  album card        -- likewise for AlbumCardWnd.
//   7  edit card         -- likewise, but EditCardWnd is opened through BeginEdit(NULL) (a fresh
//                           card rather than a clone) instead of the shared slot-8 "raise" virtual.
//   8  tutorial          -- does NOT raise a window: it only records the state being left in the
//                           tutorial window, which restores it when the tutorial finishes
//                           (src/TutorialWnd.cpp writes g_nScreenState back from that mirror).
//   9  map               -- like 5/6, plus one extra: leaving the auto-curve tool (state 4)
//                           publishes the layout bitmap first, so the map shows current track.
//   0xa quit             -- play the shutdown sound (0x5026) and BLOCK until its channel reports
//                           itself reclaimable, hand DirectDraw back to normal cooperative level
//                           so the desktop mode is restored, then post WM_CLOSE to the main window.
//
// The state is stored BEFORE the switch runs, so an arm that re-enters this function (arm 1 does,
// via AppWindow_StartGame's tail call with 3) sees the new value already in place.
//
// FUNCTION: LOCO 0x408130
void AppWindow_SetScreenState(int nNewState)
{
    int nPrevState = g_nScreenState;

    if (g_nScreenState == nNewState) {
        return;
    }
    g_nScreenState = nNewState;
#ifdef LOCO_PORT
    // PORT ONLY -- byte-neutral. g_nScreenState is the app's top-level mode and the SplashWnd
    // lesson (v570) applies to it just as much: the two message pumps in LocoWinMain are
    // selected by IsFrontEndModeMaybe(), so this one variable decides which loop runs and
    // therefore whether Port_Present is called at all. Invisible in a log without this.
    Port_Tracef("screenstate %d -> %d\n", nPrevState, nNewState);
#endif

    switch (nNewState) {
    case 1:
        AppWindow_StartGame(nPrevState);
        break;

    case 2:
        PlacementCursorMaybe_004854c8.SetCursorCapture(0, 1, 0);
        g_pSplashWnd->BeginModalCapture();
        break;

    case 3:
        AppWindow_EnterBuildMode(nPrevState);
        break;

    case 4:
        PlacementCursorMaybe_004854c8.SetHoverObjMaybe(NULL);
        DecorObjMgrMaybe_00485448.MarkAllEntriesDirtyMaybe(0);
        DAT_004fd220.BroadcastToAllEffectsMaybe(0);
        g_PeerTrainSlotQueue.DispatchActiveSlotsMaybe(0);
        g_worldBoard.bBoardDirtyNeedsRebuildFlag = 0;
        break;

    case 5:
        if (g_pDSoundManager != NULL) {
            g_pDSoundManager->DSound_SetTemporaryDuck(1);
        }
        PlacementCursorMaybe_004854c8.SetCursorCapture(0, 0, 0);
        g_pMailWnd->BeginModalCapture();
        break;

    case 6:
        if (g_pDSoundManager != NULL) {
            g_pDSoundManager->DSound_SetTemporaryDuck(1);
        }
        PlacementCursorMaybe_004854c8.SetCursorCapture(0, 0, 0);
        g_pAlbumCardWnd->BeginModalCapture();
        break;

    case 7:
        if (g_pDSoundManager != NULL) {
            g_pDSoundManager->DSound_SetTemporaryDuck(1);
        }
        PlacementCursorMaybe_004854c8.SetCursorCapture(0, 0, 0);
        g_pEditCardWnd->BeginEdit(NULL);
        break;

    case 8:
        g_pTutorialWnd->nGlobalStateMirror = nPrevState;
        break;

    case 9:
        if (g_pDSoundManager != NULL) {
            g_pDSoundManager->DSound_SetTemporaryDuck(1);
        }
        if (nPrevState == 4) {
            g_pDPlaySessionMgr->LayoutNet_SendCurrentLayoutBitmap(0);
        }
        PlacementCursorMaybe_004854c8.SetCursorCapture(0, 0, 0);
        g_pMapWnd->BeginModalCapture();
        break;

    case 10:
        {
            // The same block-until-finished shape src/SplashWnd.cpp uses for its Esc click: the
            // window is about to be destroyed, so the sound has to have played out first.
            DSoundChannel *pChannel = 0;
            if (g_pDSoundManager != NULL) {
                g_pDSoundManager->PlaySoundByIdWithHandle(0x5026, &pChannel);
                while (pChannel != 0 && pChannel->IsReclaimable() == 0) {
                }
                if (pChannel != 0) {
                    pChannel->Release();
                    pChannel = 0;
                }
            }
            if (g_pDDraw2 != NULL) {
                g_pDDraw2->SetCooperativeLevel(g_pApp->hwndOwner, DDSCL_NORMAL);
            }
            PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 0, 0);
        }
        break;
    }
}

// The app's "start game" handler -- screen state 1's arm of AppWindow_SetScreenState,
// which is its only caller and hands it the state the app is coming FROM (a parameter this body
// never reads, but the call site really does push it and clean it up, so it is a genuine
// __cdecl argument, not Ghidra's `void (void)`).
//
// Two quite different paths, selected by g_pApp->unk0x10 -- the "the world's windows have already
// been realized once" flag that AppWindow::AbortMultiplayerSession raises on its way back to the
// front end:
//
//   * unk0x10 == 0 (cold start): tear the splash window's session down, drop the placement
//     cursor's capture, put the loading screen up (0x45e090), disable the main window and drain
//     the mouse input that piled up behind it, then realize the Mail / EditCard / AlbumCard
//     backdrops -- plus the Map's, but only in connection mode 2 -- pumping (0x45e1e0) and
//     draining between each. The world itself is then loaded OFF-THREAD by
//     App_LoadWorldThreadProcMaybe, and the window is invalidated so the loading art paints.
//     Skipped entirely under the screen saver, which has no UI windows to realize.
//
//   * unk0x10 != 0 (re-entry): load the board layout SYNCHRONOUSLY instead -- the screen saver's
//     own randomly-picked layout, else the connected provider's `Layouts\<name>`, else the
//     "~curr" checkpoint but only if the previous run set [PROCESS] CleanExit -- then realize the
//     same four backdrops inline, stop any playing sound, and hand the app straight to screen
//     state 3 (build mode).
//
// The five DrainQueuedMouseInput call sites in the original each load ecx = g_pApp first (as does
// the sixth, over in EditCardWnd::AnimateDecalPickerPageWipe), so that function is really an
// AppWindow member whose body ignores `this` -- the same class as AbortMultiplayerSession and
// ReadHklmValue. It is deliberately NOT modeled that way: promoting it would need a declaration in
// src/AppWindow.h, and v422 measured that ANY declaration added to that header costs
// src/MailWnd.cpp's RefreshClientClipRect (0x42f8b0) its 1332-byte exact match -- more than this
// function's whole 641 bytes. Those five missing `mov ecx, g_pApp` loads are the entire residual
// here; see src/AppWindow.h's MEASURED DIAL note and docs/PARKED.md.
//
// v423 MEASURED THE WHOLE TRADE END TO END rather than reasoning about it, and the model is now
// PROVEN rather than merely inferred -- do not re-run this experiment:
//   * Promoting DrainQueuedMouseInput to a real `void AppWindow::DrainQueuedMouseInput(char)`
//     and calling it `g_pApp->DrainQueuedMouseInput(1)` at all five sites takes THIS function to
//     a full 641-byte EXACT match, and 0x4085e0 still matches as the member. So the five loads
//     really are a __thiscall `this` setup and the free-function spelling is the only thing
//     missing -- the residual has no other content.
//   * It costs src/MailWnd.cpp 1332 B (0x42f8b0) AND src/WorldBoardMaybe.cpp 951 B (0x457ce0,
//     FindNearestObjOfCategoryMaybe). Net -1642 B repo-wide. Reverted on that basis.
//   * v422's escape hatch does NOT reproduce: adding a SECOND and THIRD declaration to
//     src/AppWindow.h (probed with dummies, since the dial is not name- or content-dependent)
//     leaves both losses byte-for-byte identical to the one-declaration case. The dial is
//     saturated, not on a parity cycle -- the committed baseline is simply the lucky register
//     assignment for those two functions.
//   * 0x42f8b0's own residual was re-read at 397/396 insns: a register-rotation cascade (the
//     original zero-extends a word straight into edx via `xor edx,edx / mov dx,[ecx+0x14]`,
//     ours routes through eax and pays one extra `mov edx,eax`). Not a local-order swap like
//     WalkerActor's 0x4331b0, so the knife-edge is still NOT understood.
// Promote all three (this, CreateMainWindow, ConstructSingletonWindows) the moment 0x42f8b0's
// cascade cracks -- the correct model is worth +641 here and is known-good.
//
// FUNCTION: LOCO 0x408350
void __cdecl AppWindow_StartGame(int nPrevState)
{
    // Declared in this order because the original initializes the higher stack slot first: both
    // are `char[261] = ""`, which VC5 compiles as a ONE-byte copy of the pooled "" literal
    // followed by 260 zeroed bytes -- and it loads that one byte into dl just once for the pair.
    char szProviderLayoutPath[261] = "";
    char szScreenSaverLayout[261] = "";

    g_uLoadingTimerId = SetTimer(g_pApp->hwndOwner, 0x47, 150, NULL);
    if (g_pDSoundManager != NULL) {
        g_pDSoundManager->DSound_SetTemporaryDuck(1);
    }

    if (g_pApp->unk0x10 == 0) {
        g_pSplashWnd->EndActiveSession();
        PlacementCursorMaybe_004854c8.SetCursorCapture(0, 1, 0);
        LoadingScreen_Show();
        EnableWindow(g_pApp->hwndOwner, FALSE);
        g_pApp->DrainQueuedMouseInput(1);
        if (g_screenSaver.bScreenSaverMode != 1) {
            g_pMailWnd->EnsureBackdropRealizedMaybe();
            LoadingScreen_Pump(0);
            g_pApp->DrainQueuedMouseInput(1);
            g_pEditCardWnd->BuildPreviewCanvasAMaybe();
            LoadingScreen_Pump(0);
            g_pApp->DrainQueuedMouseInput(1);
            g_pAlbumCardWnd->EnsureBackgroundTileLoaded();
            LoadingScreen_Pump(0);
            g_pApp->DrainQueuedMouseInput(1);
            if (g_pDPlaySessionMgr->connectionMode == 2) {
                g_pMapWnd->EnsureBackgroundTileLoaded();
                LoadingScreen_Pump(0);
                g_pApp->DrainQueuedMouseInput(1);
            }
        }
        g_worldLoadThread.Start(App_LoadWorldThreadProcMaybe, NULL);
        InvalidateRect(g_pApp->hwndOwner, NULL, FALSE);
        UpdateWindow(g_pApp->hwndOwner);
        return;
    }

    if (g_screenSaver.bScreenSaverMode == 1) {
        g_screenSaver.GetLayoutFileName(szScreenSaverLayout);
        if (!g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)szScreenSaverLayout)) {
            if (g_bCleanExit) {
                g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
            }
        }
    } else if (g_pDPlaySessionMgr->connectionMode == 2) {
        DPlaySessionMgrProviderSlot *pProvider = g_pDPlaySessionMgr->GetSelectedProvider();
        if (pProvider != NULL) {
            wsprintfA(szProviderLayoutPath, "Layouts\\%s", pProvider->sLongName);
            g_NetSessionEventQueue.PlaceEdgeLinksAndFlush(
                (unsigned char *)szProviderLayoutPath);
        }
    } else if (g_bCleanExit) {
        g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
    }

    g_pSplashWnd->EndActiveSession();
    g_pMailWnd->EnsureBackdropRealizedMaybe();
    g_pEditCardWnd->BuildPreviewCanvasAMaybe();
    g_pAlbumCardWnd->EnsureBackgroundTileLoaded();
    if (g_pDPlaySessionMgr->connectionMode == 2) {
        g_pMapWnd->EnsureBackgroundTileLoaded();
    }
    PlaySoundA(NULL, NULL, 0);
    AppWindow_SetScreenState(3);
}

// Screen state 3's arm of AppWindow_SetScreenState -- "enter build mode", i.e. get the
// player back onto the live world board. Its only caller passes the state being LEFT (that is why
// the switch runs over values the dispatcher itself treats as destinations), so each arm is the
// teardown for one departing screen and the shared tail is the world resuming:
//
//   1 (start game)    -- the world has just finished loading: settle the display mode against the
//                        board's viewport, retire the loading timer, publish the layout, kick the
//                        game tick, honour [ScreenSaver] Sound while in screen-saver mode, force
//                        one board rebuild, and let the tutorial fire its stage-5 notification.
//                        Falls through into arm 4, which does the rest of the bring-up.
//   4 (auto-curve)    -- kick the tick, publish the layout, take the placement cursor back, and
//                        rebuild the board if anything dirtied it while the tool was up.
//   2 (front end)     -- refuses the transition: parks the state selector back on 2 and returns
//                        WITHOUT resuming the world.
//   5/6/7 (mail,      -- close all three card-family windows (they share a screen), un-duck the
//    album, edit)        sound, take the cursor back, and dirty the whole viewport.
//   9 (map)           -- the same, for the map window alone.
//
// FUNCTION: LOCO 0x4086f0
void __cdecl AppWindow_EnterBuildMode(int nPrevState)
{
    // sic: 127 bytes of stack zeroed in the prologue for a buffer no arm ever reads -- a leftover
    // scratch/format local. VC5 kept the initializer's inline `rep stos` (it lowers the memset
    // before dead-store elimination runs) but dropped the one-byte "" copy at the front, which is
    // why this shows as a 127-byte fill starting one byte into a 128-byte slot rather than the
    // 1-byte-copy + 127-zero shape that AppWindow_StartGame's two live buffers above produce.
    char szUnused[128] = "";

    switch (nPrevState) {
    case 2:
        g_nScreenState = 2;
        return;

    case 5:
    case 6:
    case 7:
        g_pMailWnd->EndActiveSession();
        g_pAlbumCardWnd->EndActiveSession();
        g_pEditCardWnd->EndActiveSession();
        if (g_pDSoundManager != NULL) {
            g_pDSoundManager->DSound_SetTemporaryDuck(0);
        }
        PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 0);
        g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
        break;

    case 9:
        g_pMapWnd->EndActiveSession();
        if (g_pDSoundManager != NULL) {
            g_pDSoundManager->DSound_SetTemporaryDuck(0);
        }
        PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 0);
        g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
        break;

    case 1:
        if ((int)g_worldBoard.dwViewportWidth < (int)g_dwScreenWidth) {
            AppWindow_ApplyDisplayModeMaybe(0);
        } else {
            AppWindow_ApplyDisplayModeMaybe(1);
        }
        KillTimer(g_pApp->hwndOwner, g_uLoadingTimerId);
        g_uLoadingTimerId = 0;
        g_pDPlaySessionMgr->LayoutNet_SendCurrentLayoutBitmap(0);
        PostMessageA(g_pApp->hwndOwner, 0x406, g_dwGameTick, 0);
        if (g_screenSaver.bScreenSaverMode == 1 && g_pDSoundManager != NULL) {
            if (g_pIniFile->ReadInt("ScreenSaver", "Sound", 0)) {
                g_pDSoundManager->DSound_SetPersistentMute(1);
            } else {
                g_pDSoundManager->DSound_SetPersistentMute(0);
            }
        }
        g_worldBoard.FUN_00457320();
        g_pTutorialWnd->NotifyOrLaunch(5, 0);
        // sic: falls through -- arm 4 is the rest of the bring-up and the original shares it.

    case 4:
        PostMessageA(g_pApp->hwndOwner, 0x406, g_dwGameTick, 0);
        g_pDPlaySessionMgr->LayoutNet_SendCurrentLayoutBitmap(0);
        PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 0);
        g_easterEggMgrMaybe.ProcessInsertSeqSpawnsMaybe();
        if (g_worldBoard.bBoardDirtyNeedsRebuildFlag != 0) {
            g_worldBoard.FUN_00457320();
            DecorObjMgrMaybe_00485448.RestoreEntryPositionsMaybe();
        }
        break;
    }

    DecorObjMgrMaybe_00485448.MarkAllEntriesDirtyMaybe(1);
    DAT_004fd220.BroadcastToAllEffectsMaybe(1);
    g_PeerTrainSlotQueue.DispatchActiveSlotsMaybe(1);
    g_worldLoadThread.Start(WorldIdleEventPumpThreadProc, NULL);
    if (DAT_00485234 != 0) {
        DAT_00485234 = 0;
        DAT_004aa7dc = 0;
        PlacementCursorMaybe_004854c8.nTypeIdMaybe = -1;
    }
    PlacementCursorMaybe_004854c8.UpdateCursorForAppStateMaybe();
    if (g_pDSoundManager != NULL) {
        g_pDSoundManager->DSound_SetTemporaryDuck(0);
    }
}

// Drain the run of mouse input sitting at the head of the message queue, stopping at the first
// message that is not WM_SETCURSOR, WM_MOUSEMOVE, or one of the left/right button up/down pair.
// Called wherever a blocking animation has held the main pump and the clicks that piled up behind
// it must not fire against the UI that has since changed underneath them -- the transition
// sequencer at 0x408350 does it at five points, and the decal-picker page wipe (0x41923a) once.
//
// bDiscard != 0 throws the input away, forcing the cursor off rather than letting the window
// procedure choose one; bDiscard == 0 translates and dispatches each message on the way past.
//
// WM_LBUTTONDBLCLK (0x203) is deliberately NOT in the button run -- the jump table routes it to
// the default arm, so a double-click ends the drain instead of being eaten.
//
// FUNCTION: LOCO 0x4085e0
void AppWindow::DrainQueuedMouseInput(char bDiscard)
{
    MSG msg;

    while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE) > 0) {
        char bStop = 0;
        // Cases in ascending order, which is also the body layout the jump table implies.
        switch (msg.message) {
        case WM_SETCURSOR:
            if (bDiscard) {
                // sic: unlike every other arm this one does NOT remove the message, so a
                // WM_SETCURSOR really sitting in the queue would spin here forever. It cannot --
                // WM_SETCURSOR is only ever sent, never posted, so PeekMessage never returns it.
                SetCursor(NULL);
            } else {
                PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            break;

        case WM_MOUSEMOVE:
            if (bDiscard) {
                PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
            } else {
                PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
            break;

        default:
            bStop = 1;
            break;
        }
        if (bStop) {
            break;
        }
    }
}

// The build-tool mode selector's setter (DAT_00485234: 0 = no tool, 1 = bulldoze, 2 = place
// whatever PlacementCursorMaybe currently holds). A no-op when the mode is already what was
// asked for; otherwise it stores the new mode and fixes up the two pieces of state that hang
// off it -- the pending cursor type id, and DAT_004aa7dc, which every tool mode but 0 raises.
// Entering either live tool mode also dirties the board so a rebuild+autosave is queued.
//
// FUNCTION: LOCO 0x4089d0
void __cdecl AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(int nMode)
{
    if (DAT_00485234 != nMode) {
        DAT_00485234 = nMode;
        // A switch, not an if/else chain: the original's `sub eax,0 / je / dec eax / je /
        // dec eax / jne` is VC5's compare-chain switch lowering. The chain is ordered by case
        // VALUE while the bodies keep SOURCE order, which is why case 2 is written first here.
        switch (nMode) {
        case 2:
            g_worldBoard.bBoardDirtyNeedsRebuildFlag = 1;
            DAT_004aa7dc = 1;
            break;
        case 1:
            PlacementCursorMaybe_004854c8.nTypeIdMaybe = -1;
            g_worldBoard.bBoardDirtyNeedsRebuildFlag = 1;
            DAT_004aa7dc = 1;
            break;
        case 0:
            DAT_004aa7dc = 0;
            PlacementCursorMaybe_004854c8.nTypeIdMaybe = -1;
            break;
        }
    }
}

// Read one REG_SZ-shaped value out of HKEY_LOCAL_MACHINE\<pszSubKey>. Returns 1 only when both
// the open and the query succeed; a failed QUERY zeroes the caller's first byte so the buffer
// reads as an empty string, while a failed OPEN leaves it untouched. Both call sites are in
// ScreenSaver::LoadPasswordProvider, and both load ecx = g_pApp even though this body never
// touches `this` -- see src/AppWindow.h.
//
// FUNCTION: LOCO 0x408a30
char AppWindow::ReadHklmValue(const char *pszSubKey, LPBYTE pData, DWORD cbData,
                              const char *pszValueName)
{
    HKEY hKey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, pszSubKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwType = 0;
        LONG lResult = RegQueryValueExA(hKey, pszValueName, NULL, &dwType, pData, &cbData);
        RegCloseKey(hKey);
        if (lResult == ERROR_SUCCESS) {
            return 1;
        }
        *pData = 0;
    }
    return 0;
}
