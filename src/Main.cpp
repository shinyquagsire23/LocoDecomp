// Main -- the app entry-point TU (Ghidra namespace `Main::`). Hosts AppWndProc (0x4618c0, the
// main window procedure) and LocoWinMain (0x462e90), the game's WinMain body: splash dialog,
// AppWindow singleton construction, config/locale/command-line startup, the double message pump
// (GetMessage for the front-end state, PeekMessage for the in-game loop) with the FPS counter,
// and the clean-shutdown tail.
//
// TU membership is pinned by .text contiguity: 0x4618c0 (AppWndProc) runs straight into
// 0x462e90 (LocoWinMain) and then the rest of the Main:: cluster (0x463430, 0x4634f0,
// 0x463600, 0x463670) before the import thunks at 0x4637c0; the preceding run is
// ThreadWrapper's (src/ThreadWrapper.cpp). AppWndProc is therefore FIRST in this file --
// WinMain + WndProc in one file is the classic Win32 layout and matches the emission order.
//
// v366: the three "startup sound object" globals this TU used to model by hand
// (DWORD_004a9910 passed by address to two free __fastcall functions, plus DAT_004a991c and
// g_bCmdlineSFlagSet) turned out to be ONE object -- the ScreenSaver singleton, see
// ScreenSaver.h. Routing them through `g_screenSaver` is byte-neutral (DIFF 426, len 1435
// before and after): a member access on a statically-known global compiles to the same
// absolute address, and a thiscall on it to the same `mov ecx,0x4a9910`.
#define _WIN32_DCOM // CoInitializeEx is gated behind this in the VC5 SDK headers
#include "AppWindow.h"
#include "UIResources.h"
#include "DPlaySessionMgr.h"
#include "ScreenSaver.h"
#include "BuildToolButton.h"        // g_BuildToolButton (0x4aa5b8) + its WidgetPicker region
#include "BuildToolCursorWnd.h"     // g_pBuildToolCursorWnd (0x485258)
#include "PlacementCursorMaybe.h"   // PlacementCursorMaybe_004854c8
#include "TutorialWnd.h"            // g_pTutorialWnd (0x4fd38c)
#include "WorldActionCursor.h"      // g_worldActionCursor, SelectedObjWidgetMaybe_004852a0
#include "WorldBoardMaybe.h"        // g_worldBoard (0x4aad08)
#include "DecorObjMgrMaybe.h"       // DecorObjMgrMaybe_00485448 (0x485448) + TickObjSeqGoalsMaybe
#include "AlbumCardWnd.h"           // g_pAlbumCardWnd (0x4fd384)
#include "PeerTrainSlotQueueMaybe.h" // g_PeerTrainSlotQueue (0x4a98b0)
#include "MailWnd.h"                // g_pMailWnd (0x4fd37c)
#include "CreditsWnd.h"             // g_pCreditsWnd (0x4fd390)
#include "EditCardWnd.h"            // g_pEditCardWnd (0x4fd380)
#include "MapWnd.h"                 // g_pMapWnd (0x4fd388)
#include "SplashWnd.h"              // g_pSplashWnd (0x4fd378)
#include "ApplSetupWnd.h"           // SplashWnd::pApplSetupWnd's type
#include "NetSetupWnd.h"            // SplashWnd::pNetSetupWnd's type
#include "PeerTrainNode.h"          // PeerTrainNodePartial
#include "NetSessionEventQueue.h"    // g_NetSessionEventQueue (0x4a9990)
#include "ThreadWrapper.h"           // g_worldLoadThread (0x4a9ad0)

#include <ctype.h>   // _toupper (0x467710); see the #undef below
#include <objbase.h>
#include <string.h>
#include <time.h>
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: Port_Tracef boot diagnostic
#endif

// ctype.h defines _toupper as a MACRO that folds to inline arithmetic; the original emits a
// real call to the CRT function of the same name, so the macro is dropped and the declaration
// ctype.h also provides is what the WM_CHAR case binds to.
#undef _toupper

// v488: the two TU-local AppWindow views that used to sit here are GONE. src/AppWindow.h now
// models the real class -- a public `AppWindow(HINSTANCE)` constructor and a `virtual ~AppWindow()`
// matching the single-slot vtable at 0x4774c4 -- so `new AppWindow(hInstance)` and `delete g_pApp`
// below are the genuine article rather than a cast through a look-alike struct. The old
// AppWindowCtorModel0x462e90 (a derived class carrying only the ctor declaration) and
// AppWindowMainView0x462e90 (a standalone struct carrying only the virtual dtor, plus the two
// members now declared on AppWindow itself) were both retired with no byte cost anywhere.

// Cross-TU callees (names kept in sync with Ghidra; see their owning TUs).
void AppWindow_SetScreenState(int newState);              // 0x408130, see src/AlbumCardWnd.cpp
void LoadingScreen_Pump(unsigned int u);                               // 0x45e1e0
void __stdcall FrameDriver_TickMaybe(void);          // 0x45c3c0, see src/FrameDriver.cpp
void __fastcall GameNet_DrainEventQueue(DPlaySessionMgr *pMgr);  // 0x43f0c0, see src/DPlaySessionMgr.cpp

// Per-TU extern decls (kept in sync with their canonical homes).
extern int g_nScreenState;              // app-state dword (2 = front-end, 3 = in-game), see src/GameNetMsgQueue.h
// The two app-state gates the game loop opens on. The `unsigned char` return type is
// LOAD-BEARING -- it is what reproduces the original's sete-materialized branch; see
// docs/CODEGEN.md's byte-predicate lever (v356).
inline unsigned char IsFrontEndModeMaybe() { return g_nScreenState == 2; }
inline unsigned char IsInGameModeMaybe() { return g_nScreenState == 3; }
extern unsigned char DAT_004aa4a4;    // per-tick gate for the 0x45e1e0 callback
extern unsigned char DAT_00485444;    // frame-presented gate (drives the FPS counter tick)
extern HANDLE DAT_004a990c;           // event reset after each pumped frame
extern int DAT_00481914;              // FPS sample threshold in frames (grows by 1 up to 100)
extern double DAT_00481170;           // last computed FPS sample
extern int DAT_004ff12c;              // zeroed at the top of every game-loop iteration
extern int DAT_004ff118;              // (same; owners not yet transcribed)
extern int DAT_004ff128;              // (same)
// DAT_004851d0 is the pooled `""` string literal, NOT a global (v384: every other TU that used
// to declare it this way now just writes `char buf[N] = "";`, which is what emits the
// literal-byte copy + rep-stos this decl was invented to explain -- Ghidra now types the address
// as `ds ""` / `s__004851d0`). It survives HERE only as parked debt, because the three szErr
// sites below still carry a raw `memset(tail); byte copy; memset(tail)` transcription.
//
// ⬅ NEXT SESSION LEAD (v384, not yet verified -- this TU has 0 exact bytes so there is no local
// oracle): that doubled zero-fill is almost certainly NOT a "sic" at all but the documented
// TWIN-BUFFER shape -- a DEAD `char szUnusedMaybe[0x200] = "";` (whose literal-byte copy /O2
// eliminates, leaving only the tail zero-fill) immediately followed by the live
// `char szErr[0x200] = "";` (byte copy + tail zero-fill), both sharing ONE physical stack slot.
// That is exactly WindowBase::ShowFatalErrorMessageBox (0x463600), which byte-matches modelled
// that way -- see docs/CODEGEN.md's dead-scratch-buffer bullet. Confirm against the raw disasm's
// `sub esp` reservation size (one 0x200-worth per branch, not two) and this decl disappears.
extern char g_szScratchText[1];       // DAT_004851d0 // TODO: idiom // TODO: sync

// ---------------------------------------------------------------------------------------
// AppWndProc (0x4618c0) support: state predicates, globals and TU-local callee views.
// ---------------------------------------------------------------------------------------

// One byte-returning predicate per tested app state -- the v356 lever (docs/CODEGEN.md): the
// original materializes each of these as `cmp [0x4851f4],K / sete rl / test rl,rl`, which is
// what an `inline unsigned char` predicate compiles to and a plain `==` does not.
// IsFrontEndModeMaybe (== 2) and IsInGameModeMaybe (== 3) are declared above.
inline unsigned char IsAppStateZeroMaybe() { return g_nScreenState == 0; }
inline unsigned char IsAppStateOneMaybe() { return g_nScreenState == 1; }
inline unsigned char IsInGameAltModeMaybe() { return g_nScreenState == 4; }
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// Screen geometry captured by AppWindow::CheckMinimumDisplaySpec (0x406680);
// WM_DISPLAYCHANGE compares its own new-mode triple against them to spot a real mode change.
extern unsigned int g_dwScreenWidth;  // DAT_004851d8
extern unsigned int g_dwScreenHeight; // DAT_00485214
extern int g_dwScreenBpp;             // DAT_0048521c -- GetDeviceCaps(hdc, BITSPIXEL); SIGNED,
                                      // pinned by 0x406680's `jg` on the > 16 test

extern unsigned char g_bBoardScrollFlag;      // DAT_00485210 -- board is scrollable/windowed
extern RECT g_rectAppWindowBounds;            // DAT_00485200 -- last GetWindowRect of hwndOwner
extern RECT g_rectAppClientBounds;           // DAT_00485220 -- last GetClientRect of hwndOwner
// +0x4851f0: WM_SIZE parks the window's "minimized / don't paint the board" state here --
// SIZE_MINIMIZED stores 1, SIZE_RESTORED stores 0, SIZE_MAXIMIZED stores IsAppStateZeroMaybe().
// WorldBoardMaybe::UpdateDirtyTiles reads it as its "== 1 suppresses the update" gate.
extern char g_bAppMinimizedMaybe;             // DAT_004851f0
// Re-entrancy guard around the WM_DISPLAYCHANGE "you changed the display mode" message box.
extern char g_bDisplayChangeMsgUpMaybe;       // DAT_004ff138
// Set for the duration of a user drag-resize/move (WM_ENTERSIZEMOVE..WM_EXITSIZEMOVE); while
// it is set, WM_NCHITTEST stops toggling the software cursor's capture.
extern char g_bSizeMoveInProgressMaybe;       // DAT_004ff13c

// The widget that currently owns the keyboard (0x4fd3e0) now comes from src/BuildToolButton.h
// -- that class's own tick is the writer, and the base's real slot 16 (WidgetBaseObj0x4784c8::
// OnKeyDownMaybe, "did you consume this key?") is what the two dispatches below call. Retired the
// local 17-slot vtable probe this file used to carry, 2026-07-26.

// The two per-message objects WM_USER+2 / WM_USER+8 dispatch into: wParam is the object, and
// only these two slots are used (a tick/advance at +0x3c and a teardown at +0x48).
struct WndProcMsgObjVtblProbe {
    virtual void *_v00(); virtual void *_v04(); virtual void *_v08(); virtual void *_v0c();
    virtual void *_v10(); virtual void *_v14(); virtual void *_v18(); virtual void *_v1c();
    virtual void *_v20(); virtual void *_v24(); virtual void *_v28(); virtual void *_v2c();
    virtual void *_v30(); virtual void *_v34(); virtual void *_v38();
    virtual void TickMaybe(LPARAM lParam); // slot 15 (+0x3c)
    virtual void *_v40(); virtual void *_v44();
    virtual void ReleaseMaybe();           // slot 18 (+0x48)
};

// TU-local methods-only views. Parameterized method decls must not go into the shared headers
// (the v333/v334 bisects) -- the same rule the retired AppWindow views above followed.
// RETIRED v577: UIResourcesWndProcView0x4618c0 declared TickStationClockChimeMaybe (0x447400)
// on a TU-local view, so this TU's call mangled to a class no TU defines and ran a gen_stubs
// stub in the port. It is declared on the shared UIResources now (measured byte-free).

// The board tile the WM_USER+1 command 0xb detach targets, reached through its lParam. Both
// TrackConnectorTileObj and TrackDepotTileObj (src/TilePlacedObj.h) carry pOwningTrain at
// +0x120 and either could be the sender, so this TU takes the one-field common view rather
// than claiming a concrete type it cannot yet prove.
struct BoardObjTrainSlotView0x4618c0 {
    unsigned char pad0x0[0x120];
    PeerTrainNodePartial *pOwningTrain; // +0x120
};

struct BuildToolCursorWndProcView0x4618c0 : BuildToolCursorWnd {
    // 0x436d60 -- repaints the mouse-tracked ghost/preview sprite. Not yet transcribed.
    void RedrawGhostCursor();
};

struct WidgetBaseSetDescView0x4618c0 : WidgetBaseObj0x4784c8 {
    // 0x454680 -- rebinds the widget to a kind descriptor (Ghidra:
    // WidgetBaseObj0x4784c8::SetDescriptorMaybe). Not yet transcribed.
    unsigned int SetDescriptorMaybe(int nKindId, int nParamB, char bParamC);
};

struct EasterEggMgrWndProcView0x4618c0 {
    void RestoreExpiredActorDescMaybe(WPARAM wParam); // 0x4202b0 -- WM_USER+4
};
extern EasterEggMgrWndProcView0x4618c0 g_easterEggMgrMaybe; // DAT_004a99b0  // TODO: idiom

// 0x463430's shutdown globals (canonical homes: g_worldLoadThread in src/FrameDriver.cpp,
// g_uLoadingTimerId extern'd the same way in src/AppWindow.cpp).
extern ThreadWrapper g_worldLoadThread; // DAT_004a9ad0
extern UINT g_uLoadingTimerId;          // DAT_004a97a4 -- KillTimer'd on clean exit

// Cross-TU callees reached only from AppWndProc.
void __cdecl AppWindow_ApplyDisplayModeMaybe(char bParam);  // 0x407d20
void __stdcall AppWindow_ToggleWindowedModeMaybe(void);                                  // 0x407d00 -- 'W' key
void __cdecl LoadingScreen_Animate(char bFlagMaybe);                         // 0x45e210 -- WM_USER+7
void __stdcall LoadingScreen_Hide(void);                                  // 0x45e400 -- the quit routine
unsigned int __cdecl TileKind_GetCategory(unsigned int kindId);     // 0x446030, see src/UIResources.cpp
void __cdecl Ddraw_BltUpdateRect(RECT *pRect, HWND hwnd, POINT *pScrollOffset, char bParam); // 0x401280, src/DDrawSurface.h

// Same-TU callees, defined below (Main:: in Ghidra).
void __stdcall FUN_00463430_ExitRelatedMaybe(void);  // 0x463430
void __stdcall FUN_004634f0_LotsOfSendMessageA(void); // 0x4634f0, defined below
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void);   // 0x463670, defined below

// FUNCTION: LOCO 0x4618c0
// The application's window procedure -- the single largest function in the app region and, until
// v367's gap sweep, invisible to Ghidra: its ONLY reference anywhere in the binary is the data
// store at 0x406ef4 that puts it into the WNDCLASS, so no call-graph walk could reach it.
//
// Three layers, in order: (1) messages for a foreign HWND go straight to DefWindowProc; (2) in
// screen-saver mode ScreenSaver::FilterMessage gets first refusal and answers with an ACTION CODE
// (0 = not handled, 2 = return 0, 3 = return 1, anything else = DefWindowProc); (3) the game's own
// dispatch, which is TWO switches over the same message -- a small front-end one used while the
// app state is 0/1/2, and the full in-game one for every other state. Both fall through to the
// shared DefWindowProc tail, which is why the app-state test is evaluated twice.
//
// PARTIAL, PARKED (v368). asmscore --len 0x15d0 (5584 = the span to LocoWinMain, which is the
// COMDAT extent -- the five jump tables at 0x462df8..0x462e83 are part of it and Ghidra's Body
// span stops before them): total 1975015, align 1928, reg_pen 412, identity_miss 412,
// byte_diff 1695, **insns 1739/1740**. One instruction over the original, so there is no
// missing body -- structure was verified block-by-block against the raw disasm (all three
// layers, both switches' compare trees, all five jump tables, the WM_SIZING clamp quartet and
// its four dirty strips, and every epilogue's return value). Two residual classes, both
// documented and both intrinsic:
//   (1) REGISTER RESIDENCY, the dominant one (~74 of the 112 original-only rows). The original
//       dedicates ebx to `g_pApp` (reloading it after each call that could write the global)
//       and re-reads `hWnd` from [esp+0x36c] at all 26 DefWindowProcA sites; ours enregisters
//       `hWnd` in ebx and loads g_pApp into scratch. Every `mov eax,[esp+0x36c]` the dump
//       shows as original-only is this one choice. Refuted probe: reversing the guard to
//       `g_pApp->hwndOwner != hWnd` (so g_pApp is evaluated first, matching the original's
//       instruction order) is a 1-byte REGRESSION, no allocation change. Same unsteerable
//       class as docs/CODEGEN.md's zero-register / this-swap coin flips.
//   (2) CROSS-JUMPING (~65 of our 111 extra rows: 20 pop / 26 add / 10 push / 5 ret / 4 call).
//       The original tail-merges five more identical `...; return 0;` tails than we do -- most
//       visibly the `PostMessageA(hwndOwner, WM_CLOSE, 0, 0); return 0;` block, which the
//       original shares between WM_CHAR's 'Q' and WM_KEYDOWN's VK_ESCAPE while ours emits it
//       twice. Optimizer strength, not source shape.
// A third, cosmetic one: ours constant-folds the case value into the DefWindowProcA `uMsg`
// argument (`push 0x115`, 5 bytes) where the original pushes the live register (`push edi`,
// 1 byte) -- 5 sites, and the original itself DOES fold at the one block it cross-jumped
// (the shared SC_SCREENSAVE tail pushes a literal 0x112), so this is the same optimizer-
// strength axis as (2), not a separate source question. Our frame is 0x354 vs the original's
// 0x358: the original spills BOTH WM_SIZING frame metrics, we keep one in a register.
LRESULT CALLBACK AppWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    CHAR szText[0x100] = "";

    if (hWnd != g_pApp->hwndOwner) {
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }
#ifdef LOCO_PORT
    // PORT: satisfy WM_PAINT from the emulated primary.
    //
    // The original never needs this -- it draws to the real primary, which IS the
    // screen, so the main window has nothing to paint and the message falls through
    // to DefWindowProc. Here the frame lives in an offscreen buffer, and a repaint
    // that we do not answer leaves the window showing whatever the compositor had
    // (white), overwriting the frame Port_Present blitted a moment earlier.
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Port_PresentToDC(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
#endif
    if (g_screenSaver.bScreenSaverMode == 1) {
        switch (g_screenSaver.FilterMessage(hWnd, uMsg, wParam, lParam)) {
        case 0:
            break;
        case 2:
            return 0;
        case 3:
            return 1;
        default:
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }
    }
    if (uMsg == WM_DISPLAYCHANGE) {
        if (g_bDisplayChangeMsgUpMaybe != 0) {
            return 0;
        }
        if (LOWORD(lParam) == g_dwScreenWidth && HIWORD(lParam) == g_dwScreenHeight &&
            wParam == g_dwScreenBpp) {
            return 0;
        }
        ShowWindow(g_pApp->hwndOwner, SW_HIDE);
        g_UIResources.LoadLocaleString(0x14b, szText, sizeof(szText));
        g_bDisplayChangeMsgUpMaybe = 1;
        MessageBoxA((HWND)0, szText, "LEGO LOCO", MB_OK);
        FUN_00463430_ExitRelatedMaybe();
        g_bDisplayChangeMsgUpMaybe = 0;
        return 0;
    }

    // ---- front-end dispatch (app state 0, 1 or 2) --------------------------------------
    if (IsAppStateOneMaybe() || IsAppStateZeroMaybe() || IsFrontEndModeMaybe()) {
        switch (uMsg) {
        case WM_CREATE:
            return 0;
        case WM_DESTROY:
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            Ddraw_BltUpdateRect(&ps.rcPaint, g_pApp->hwndOwner, (POINT *)0, 0);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            if (wParam != 0) {
                CHAR szMsg[0x200] = "";
                // sic: the original zeroes the buffer tail, stores the first byte, then zeroes
                // the tail again -- the same triple LocoWinMain's error paths emit.
                memset(szMsg + 1, 0, sizeof(szMsg) - 1);
                g_UIResources.LoadLocaleString(0x14a, szMsg, sizeof(szMsg));
                MessageBoxA((HWND)0, szMsg, "LEGO LOCO", MB_ICONEXCLAMATION);
                FUN_00463430_ExitRelatedMaybe();
                return 0;
            }
            FUN_00463430_ExitRelatedMaybe();
            return 0;
        case WM_SETCURSOR:
        case WM_MOUSEMOVE:
            SetCursor((HCURSOR)0);
            return 0;
        case WM_KEYDOWN:
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_SCREENSAVE) {
                FUN_00463670_LotsOfShowWindow();
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_TIMER:
            if (wParam == 0x47) {
                DAT_004aa4a4 = 1;
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            return 0;
        case WM_USER + 5:
            Sleep(0x14);
            LoadingScreen_Hide();
            return 0;
        case WM_USER + 7:
            LoadingScreen_Animate((char)wParam);
            return 0;
        }
    }

    // ---- in-game dispatch (every other app state) --------------------------------------
    if (!(IsAppStateOneMaybe() || IsAppStateZeroMaybe() || IsFrontEndModeMaybe())) {
        switch (uMsg) {
        case WM_DESTROY:
            return 0;
        case WM_MOVE:
            return 0;
        case WM_SIZE:
            switch (wParam) {
            case SIZE_RESTORED:
                g_bAppMinimizedMaybe = 0;
                if (!IsNetShuttingDownMaybe()) {
                    g_UIResources.PlayUiSound(0x5467);
                }
                break;
            case SIZE_MINIMIZED:
                g_bAppMinimizedMaybe = 1;
                if (!IsNetShuttingDownMaybe()) {
                    g_UIResources.PlayUiSound(0x5465);
                }
                break;
            case SIZE_MAXIMIZED:
                g_bAppMinimizedMaybe = IsAppStateZeroMaybe();
                if (g_bBoardScrollFlag != 0) {
                    SendMessageA(g_pApp->hwndOwner, WM_SYSCOMMAND, SC_RESTORE, 0);
                    PostMessageA(g_pApp->hwndOwner, WM_SYSCOMMAND, SC_SIZE, 0);
                    return 0;
                }
                break;
            }
            if (g_pBuildToolCursorWnd->bToolActive != 0) {
                ((BuildToolCursorWndProcView0x4618c0 *)g_pBuildToolCursorWnd)->RedrawGhostCursor();
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            RECT rc;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            rc = g_rectAppClientBounds;
            OffsetRect(&rc, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
            Ddraw_BltUpdateRect(&rc, g_pApp->hwndOwner, (POINT *)&g_worldBoard.dwScrollX, 0);
            return 0;
        }
        case WM_CLOSE:
            if (g_nScreenState == 10 || g_screenSaver.bScreenSaverMode == 1) {
                FUN_00463430_ExitRelatedMaybe();
                return 0;
            }
            g_pBuildToolCursorWnd->ShowTool(0, 0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_POWER:
            // Suspend request: drop any multiplayer session and unwind the UI first.
            if (wParam == PWR_SUSPENDREQUEST) {
                if (g_pApp != (AppWindow *)0) {
                    g_pApp->AbortMultiplayerSession();
                }
                FUN_00463670_LotsOfShowWindow();
                return 1;
            }
            break;
        case WM_NCHITTEST: {
            LRESULT lHit = DefWindowProcA(hWnd, uMsg, wParam, lParam);
            if (g_bSizeMoveInProgressMaybe == 0) {
                if (lHit == HTCLIENT) {
                    if (!PlacementCursorMaybe_004854c8.bReady) {
                        PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 0);
                        return lHit;
                    }
                }
                else if (PlacementCursorMaybe_004854c8.bReady) {
                    PlacementCursorMaybe_004854c8.SetCursorCapture(0, 1, 0);
                }
            }
            return lHit;
        }
        case WM_CHAR:
            if (wParam >= 0x20 && wParam <= 0x7e) {
                if (wParam >= 0x61 && wParam <= 0x7a) {
                    wParam = _toupper(wParam);
                }
                if (g_pActiveTabWidgetMaybe == 0 ||
                    g_pActiveTabWidgetMaybe->OnKeyDownMaybe(wParam) == 0) {
                    if (wParam == 'Q') {
                        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 0, 0);
                        return 0;
                    }
                    if (wParam == 'W') {
                        AppWindow_ToggleWindowedModeMaybe();
                        return DefWindowProcA(hWnd, uMsg, wParam, lParam);
                    }
                }
            }
            break;
        case WM_KEYDOWN:
            // Printable keys are left for WM_CHAR; only the non-printable range acts here.
            if (wParam < 0x20 || wParam > 0x5a) {
                if (g_pActiveTabWidgetMaybe != 0 &&
                    g_pActiveTabWidgetMaybe->OnKeyDownMaybe(wParam) != 0) {
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    if (IsInGameModeMaybe()) {
                        ((WidgetBaseSetDescView0x4618c0 *)&g_BuildToolButton)->SetDescriptorMaybe(0x2400, 1, 0);
                        g_BuildToolButton.nButtonStateMaybe = 1;
                        AppWindow_SetScreenState(4);
                    }
                    return 0;
                }
                // sic: 'VK_Q' is a four-character literal (0x564b5f51), not a virtual-key code --
                // a leftover that no real WM_KEYDOWN wParam can ever equal.
                if (wParam == VK_ESCAPE || wParam == 'VK_Q') {
                    PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 0, 0);
                    return 0;
                }
                return 0;
            }
            break;
        case WM_SYSCOMMAND:
            switch (wParam & 0xfff0) {
            case SC_SIZE:
                if (g_bBoardScrollFlag == 0) {
                    return 0;
                }
                AppWindow_ApplyDisplayModeMaybe(1);
                return 0;
            case SC_CLOSE:
                PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 0, 0);
                return 0;
            case SC_SCREENSAVE:
                FUN_00463670_LotsOfShowWindow();
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_HSCROLL:
            switch (LOWORD(wParam)) {
            case SB_LINEUP:
                g_pApp->ScrollBoardHorizontal(lParam, -4);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_LINEDOWN:
                g_pApp->ScrollBoardHorizontal(lParam, 4);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_PAGEUP:
                g_pApp->ScrollBoardHorizontal(lParam, -0x100);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_PAGEDOWN:
                g_pApp->ScrollBoardHorizontal(lParam, 0x100);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_THUMBPOSITION:
                g_pApp->ScrollBoardHorizontal(
                    lParam, (short)HIWORD(wParam) - g_worldBoard.dwScrollX);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_VSCROLL:
            switch (LOWORD(wParam)) {
            case SB_LINEUP:
                g_pApp->ScrollBoardVertical(lParam, -4);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_LINEDOWN:
                g_pApp->ScrollBoardVertical(lParam, 4);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_PAGEUP:
                g_pApp->ScrollBoardVertical(lParam, -0x100);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_PAGEDOWN:
                g_pApp->ScrollBoardVertical(lParam, 0x100);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            case SB_THUMBPOSITION:
                g_pApp->ScrollBoardVertical(
                    lParam, (short)HIWORD(wParam) - g_worldBoard.dwScrollY);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_MOUSEMOVE:
            // The tutorial owns the mouse while it is up.
            if (g_nScreenState == 8) {
                PostMessageA(g_pTutorialWnd->hwndSelf, WM_MOUSEMOVE, wParam, lParam);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            PlacementCursorMaybe_004854c8.bMouseMovePendingMaybe = true;
            PlacementCursorMaybe_004854c8.packedMousePosMaybe = lParam;
            if (IsInGameAltModeMaybe() &&
                PlacementCursorMaybe_004854c8.pKindDesc != 0 &&
                (char)TileKind_GetCategory(
                    PlacementCursorMaybe_004854c8.pKindDesc->resourceId) != 5) {
                PlacementCursorMaybe_004854c8.AdvanceAnimFrameMaybe();
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            SetFocus(g_pApp->hwndOwner);
            SetForegroundWindow(g_pApp->hwndOwner);
            PlacementCursorMaybe_004854c8.bFlagE6Maybe = true;
            PlacementCursorMaybe_004854c8.bPendingActionAMaybe = true;
            PlacementCursorMaybe_004854c8.packedPendingPosAMaybe = lParam;
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        case WM_LBUTTONUP:
            PlacementCursorMaybe_004854c8.bFlagE6Maybe = false;
            PlacementCursorMaybe_004854c8.bPendingActionCMaybe = true;
            PlacementCursorMaybe_004854c8.packedPendingPosCMaybe = lParam;
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
            PlacementCursorMaybe_004854c8.bPendingActionBMaybe = true;
            PlacementCursorMaybe_004854c8.packedPendingPosBMaybe = lParam;
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        case WM_RBUTTONUP:
            PlacementCursorMaybe_004854c8.bPendingActionDMaybe = true;
            PlacementCursorMaybe_004854c8.packedPendingPosDMaybe = lParam;
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        case WM_SIZING: {
            // Live drag-resize. Clamp the proposed window rect so the client area can never be
            // larger than the board nor smaller than 0x80 board pixels in either axis, re-anchor
            // the scrollers, then dirty the four strips the resize newly exposed.
            RECT *pRect = (RECT *)lParam;
            RECT rc;
            int cxFrame;
            int cyFrame;

            if (g_bBoardScrollFlag == 0) {
                return 1;
            }
            cxFrame = GetSystemMetrics(SM_CXFRAME);
            cyFrame = GetSystemMetrics(SM_CYFRAME);
            if (pRect->right - pRect->left > g_worldBoard.dwViewportWidth) {
                rc.right = g_worldBoard.dwViewportWidth;
                rc.left = 0;
                rc.top = 0;
                rc.bottom = pRect->bottom - pRect->top;
                AdjustWindowRect(&rc, GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE), FALSE);
                if (rc.right + cxFrame - rc.left < pRect->right - pRect->left) {
                    pRect->right = rc.right + cxFrame - rc.left + pRect->left;
                }
                g_rectAppWindowBounds = *pRect;
            }
            rc.bottom = pRect->bottom - pRect->top;
            rc.right = 0x80;
            rc.left = 0;
            rc.top = 0;
            AdjustWindowRect(&rc, GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE), FALSE);
            if (pRect->right - pRect->left < rc.right + cxFrame - rc.left) {
                pRect->right = rc.right + cxFrame - rc.left + pRect->left;
                g_rectAppWindowBounds = *pRect;
            }
            if (pRect->bottom - pRect->top > g_worldBoard.dwViewportHeightMaybe) {
                rc.bottom = g_worldBoard.dwViewportHeightMaybe;
                rc.right = pRect->right - pRect->left;
                rc.left = 0;
                rc.top = 0;
                AdjustWindowRect(&rc, GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE), FALSE);
                if (rc.bottom + cyFrame - rc.top < pRect->bottom - pRect->top) {
                    pRect->bottom = rc.bottom + cyFrame - rc.top + pRect->top;
                }
                g_rectAppWindowBounds = *pRect;
            }
            rc.right = pRect->right - pRect->left;
            rc.left = 0;
            rc.top = 0;
            rc.bottom = 0x80;
            AdjustWindowRect(&rc, GetWindowLongA(g_pApp->hwndOwner, GWL_STYLE), FALSE);
            rc.bottom = rc.bottom + cyFrame;
            if (pRect->bottom - pRect->top < rc.bottom - rc.top) {
                pRect->bottom = rc.bottom - rc.top + pRect->top;
                g_rectAppWindowBounds = *pRect;
            }
            if (pRect->right - pRect->left > g_worldBoard.dwViewportWidth - g_worldBoard.dwScrollX) {
                g_pApp->ScrollBoardHorizontal(
                    0, g_worldBoard.dwViewportWidth - g_worldBoard.dwScrollX +
                           pRect->left - pRect->right);
            }
            else {
                g_pApp->ScrollBoardHorizontal(0, 0);
            }
            if (pRect->bottom - pRect->top > g_worldBoard.dwViewportHeightMaybe - g_worldBoard.dwScrollY) {
                g_pApp->ScrollBoardVertical(
                    0, g_worldBoard.dwViewportHeightMaybe - g_worldBoard.dwScrollY +
                           pRect->top - pRect->bottom);
            }
            else {
                g_pApp->ScrollBoardVertical(0, 0);
            }
            rc.left = g_rectAppWindowBounds.left;
            rc.right = g_rectAppWindowBounds.right;
            rc.top = pRect->top;
            rc.bottom = g_rectAppWindowBounds.top;
            OffsetRect(&rc, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
            if (rc.top < rc.bottom) {
                g_worldBoard.MarkRectDirty(rc);
            }
            rc.top = g_rectAppWindowBounds.top;
            rc.left = pRect->left;
            rc.right = g_rectAppWindowBounds.left;
            rc.bottom = g_rectAppWindowBounds.bottom;
            OffsetRect(&rc, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
            if (rc.left < rc.right) {
                g_worldBoard.MarkRectDirty(rc);
            }
            rc.top = g_rectAppWindowBounds.top;
            rc.right = pRect->right;
            rc.left = g_rectAppWindowBounds.right;
            rc.bottom = g_rectAppWindowBounds.bottom;
            OffsetRect(&rc, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
            if (rc.left < rc.right) {
                g_worldBoard.MarkRectDirty(rc);
            }
            rc.left = g_rectAppWindowBounds.left;
            rc.bottom = pRect->bottom;
            rc.top = g_rectAppWindowBounds.bottom;
            rc.right = g_rectAppWindowBounds.right;
            OffsetRect(&rc, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
            if (rc.top < rc.bottom) {
                g_worldBoard.MarkRectDirty(rc);
            }
            GetWindowRect(g_pApp->hwndOwner, &g_rectAppWindowBounds);
            GetClientRect(g_pApp->hwndOwner, &g_rectAppClientBounds);
            g_worldBoard.Ddraw_RecenterViewportOffsetMaybe();
            g_worldBoard.UpdateDirtyTiles(1);
            if (g_pBuildToolCursorWnd->bToolActive == 0) {
                return 1;
            }
            ((BuildToolCursorWndProcView0x4618c0 *)g_pBuildToolCursorWnd)->RedrawGhostCursor();
            return 1;
        }
        case WM_CAPTURECHANGED:
            // Capture went to some other window of ours -- drop the software cursor unless the
            // new owner is the main window or the tutorial.
            if (!PlacementCursorMaybe_004854c8.bReady) {
                return 0;
            }
            if ((HWND)lParam == g_pApp->hwndOwner) {
                return 0;
            }
            if ((HWND)lParam == g_pTutorialWnd->hwndSelf) {
                return 0;
            }
            PlacementCursorMaybe_004854c8.SetCursorCapture(0, 1, 0);
            return 0;
        case WM_MOVING:
            if (g_pBuildToolCursorWnd->bToolActive == 0) {
                return 0;
            }
            if (g_bBoardScrollFlag == 0) {
                return 0;
            }
            ((BuildToolCursorWndProcView0x4618c0 *)g_pBuildToolCursorWnd)->RedrawGhostCursor();
            return 0;
        case WM_POWERBROADCAST:
            if (wParam == 4) {  // PBT_APMSUSPEND, not in the VC5 SDK headers
                if (g_pApp != (AppWindow *)0) {
                    g_pApp->AbortMultiplayerSession();
                }
                FUN_00463670_LotsOfShowWindow();
                return 1;
            }
            break;
        case WM_ENTERSIZEMOVE:
            g_bSizeMoveInProgressMaybe = 1;
            return 0;
        case WM_EXITSIZEMOVE:
            g_bSizeMoveInProgressMaybe = 0;
            if (g_bBoardScrollFlag == 0) {
                return 0;
            }
            GetWindowRect(g_pApp->hwndOwner, &g_rectAppWindowBounds);
            GetClientRect(g_pApp->hwndOwner, &g_rectAppClientBounds);
            g_worldBoard.Ddraw_RecenterViewportOffsetMaybe();
            g_pApp->ScrollBoardHorizontal(0, 0);
            g_pApp->ScrollBoardVertical(0, 0);
            g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
            g_worldBoard.UpdateDirtyTiles(1);
            return 0;
        case WM_USER + 1:
            // Case order follows the original's emitted body order (a jump-table switch lays its
            // bodies out in SOURCE order, so 8 really does come first in the source).
            switch (wParam) {
            case 8:
                if (lParam == 0) {
                    return 0;
                }
                g_pMailWnd->bMailPendingMaybe = 1;
                AppWindow_SetScreenState(5);
                return 0;
            case 0:
                g_BuildToolButton.OnPressReleaseMaybe(0);
                SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(0);
                g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(0);
                if (g_pApp == (AppWindow *)0) {
                    AppWindow_SetScreenState(10);
                    return 0;
                }
                g_pApp->AbortMultiplayerSession();
                return 0;
            case 5:
                g_BuildToolButton.regionBMaybe.LoadActiveSlot();
                return 0;
            case 6:
                g_BuildToolButton.regionBMaybe.SaveActiveSlot();
                return 0;
            case 7:
                g_BuildToolButton.regionBMaybe.DeleteActiveSlot();
                return 0;
            case 9:
                // Swap the live board for another slot: checkpoint "~curr", reload, re-link.
                g_NetSessionEventQueue.SaveBoardLayout((unsigned char *)"~curr");
                g_BuildToolButton.regionBMaybe.ReloadActiveSaveState((char *)lParam);
                g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
                return 0;
            case 0xa:
                g_pAlbumCardWnd->PurgeDuplicateCards();
                return 0;
            case 0xb: {
                // Detach the train hanging off this board object before re-selecting it.
                if (lParam != 0) {
                    PeerTrainNodePartial *pTrain = ((BoardObjTrainSlotView0x4618c0 *)lParam)->pOwningTrain;
                    if (pTrain != 0) {
                        unsigned short wTrainId = pTrain->wTrainId;
                        char bOwner = pTrain->bOwnerByteA;
                        g_PeerTrainSlotQueue.FreeQueuedTrainCarSlots(pTrain);
                        g_PeerTrainSlotQueue.DetachFromBoardMaybe(
                            ((BoardObjTrainSlotView0x4618c0 *)lParam)->pOwningTrain);
                        g_PeerTrainSlotQueue.ReleaseOrForwardMatchingSlotMaybe(wTrainId, bOwner, 1);
                    }
                }
                g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(
                    SelectedObjWidgetMaybe_004852a0.pSelectedObjMaybe);
                return 0;
            }
            default:
                return 0;
            }
        case WM_USER + 2:
            if (!IsNetShuttingDownMaybe() && wParam != 0) {
                DAT_004ff12c = DAT_004ff12c + 1;
                ((WndProcMsgObjVtblProbe *)wParam)->TickMaybe(lParam);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_USER + 3:
            if (!IsNetShuttingDownMaybe()) {
                DAT_004ff118 = DAT_004ff118 + 1;
                DecorObjMgrMaybe_00485448.TickObjSeqGoalsMaybe((TilePlacedObj *)wParam);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_USER + 4:
            if (!IsNetShuttingDownMaybe()) {
                DAT_004ff128 = DAT_004ff128 + 1;
                g_easterEggMgrMaybe.RestoreExpiredActorDescMaybe(wParam);
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        case WM_USER + 6:
            if (!IsNetShuttingDownMaybe()) {
                g_UIResources.TickStationClockChimeMaybe(wParam, 0);
            }
            break;
        case WM_USER + 8:
            if (!IsNetShuttingDownMaybe() && wParam != 0) {
                ((WndProcMsgObjVtblProbe *)wParam)->ReleaseMaybe();
                return DefWindowProcA(hWnd, uMsg, wParam, lParam);
            }
            break;
        }
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

#ifdef LOCO_PORT
// PORT ONLY -- byte-neutral (the whole definition is inside the guard, so the match build never
// sees it and cannot be moved by it).
//
// Step-level probe for the in-game pump. The end-of-iteration heartbeat below establishes THAT
// the pump stops; only a marker between the individual calls says WHERE. Added v571 because the
// port terminates silently a handful of iterations into the in-game pump -- no wine exception,
// no WM_QUIT, no ExitRelated -- and the loop body is six candidate calls wide.
static void Port_PumpStep(const char *pszWhere, const MSG *pMsg)
{
    static unsigned int nStep = 0;

    if (++nStep <= 80) {
        Port_Tracef("  step %-9s msg=%04x hwnd=%p w=%08lx\n", pszWhere,
                    (unsigned int)pMsg->message, (void *)pMsg->hwnd,
                    (unsigned long)pMsg->wParam);
    }
}
#endif

// FUNCTION: LOCO 0x462e90
// EFFECTIVE MATCH -- v356 halved it with the byte-predicate lever (asmscore --len 1440:
// total 183550 -> **104284**, align 178 -> 102, byte_diff 170 -> 94, insns 441 -> 449 of 451;
// cc.sh DIFF 453 -> 426). Whole-function structure verified against the raw disasm (splash dialog +
// centering, new-expression SEH scaffolding, the five startup-failure returns -2/-1/0/-3/1,
// single-instance FindWindowA, the double message pump, FPS counter, clean-shutdown tail).
// Residual was FOUR stacked documented intrinsic /Og coin-flip classes; (1) is now FIXED:
// the prologue sete-materialization of `(g_nScreenState == 2)` and `(g_nScreenState == 3)` (the
// 0x4393d0 class) is reproduced by routing both gates through the byte-returning inline
// predicates above -- the class was never intrinsic, see docs/CODEGEN.md. Still open: (2) the zero-register class (orig dedicates ebp=0 through the startup half and
// ebx=0 inside the game loop; ours uses edi=0 -- every `push ebp`/`cmp x,ebp`/
// `mov [x],ebp` vs `push 0`/`test`/`mov [x],0` row in the dump is this class); (3)
// slot-vs-register residency on nRunResult (orig stacks it at frame+4 -- the dead
// new-expression temp slot -- ours keeps it in a register; declaration-order and
// int-vs-WPARAM probes confirmed NO EFFECT); (4) symmetric lea-register swaps
// (edx vs eax/ecx on the &msg leas, the #29/#30 class). See docs/PARKED.md.
//
// WinMain body. Splash dialog up first (centered via SM_CXFULLSCREEN/SM_CYFULLSCREEN), then
// the AppWindow singleton (its ctor stores g_pApp itself is NOT relied on -- the result is
// assigned). Every startup-failure path funnels into msg.wParam (the function's single
// return value): -2 app-object alloc failed, -1 config directories invalid, 0 startup-sound
// init failed, -3 high-res/display-mode check failed, 1 another instance is already running
// (foregrounded instead), or FUN_00406ba0's own WPARAM when the "Run" startup fails.
int __stdcall LocoWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    CHAR szErr[0x200];
    CHAR szTitle[100];
    HWND hSplash;
    HWND hOther;
    int bConfigOk;
    int nFrameCount = 0;
    int nLastTime = 0;
    int nPumpBudget = 0;
    WPARAM nRunResult;
    int t;

    hSplash = CreateDialogParamA(hInstance, MAKEINTRESOURCEA(0x71), (HWND)0, (DLGPROC)0, 0);
    if (hSplash != (HWND)0) {
        LoadStringA(hInstance, 1, szTitle, sizeof(szTitle));
        SetWindowPos(hSplash, (HWND)0,
                     (GetSystemMetrics(SM_CXFULLSCREEN) - 0x2a3) / 2,
                     (GetSystemMetrics(SM_CYFULLSCREEN) - 0x1c2) / 2,
                     0x2a3, 0x1c2, 0);
        UpdateWindow(hSplash);
        ShowWindow(hSplash, 1);
        SetWindowTextA(hSplash, szTitle);
    }
    g_screenSaver.hInstance = hInstance;
    g_pApp = new AppWindow(hInstance);
    if (g_pApp == (AppWindow *)0) {
        // sic: the original zeroes the buffer tail, stores the first byte, then zeroes the
        // tail again (both memsets survive verbatim in all three error paths).
        memset(szErr + 1, 0, sizeof(szErr) - 1);
        szErr[0] = g_szScratchText[0];
        memset(szErr + 1, 0, sizeof(szErr) - 1);
        g_UIResources.LoadLocaleString(0x14a, szErr, sizeof(szErr));
        MessageBoxA((HWND)0, szErr, "LEGO LOCO", 0x30);
        msg.wParam = -2;
    }
    else {
        CoInitializeEx((LPVOID)0, 0);
        bConfigOk = g_pApp->LoadConfigDirectories();
        g_UIResources.Locale_DetectLanguage();
        if (bConfigOk == 0) {
            delete g_pApp;
            g_pApp = (AppWindow *)0;
            CoUninitialize();
            memset(szErr + 1, 0, sizeof(szErr) - 1);
            szErr[0] = g_szScratchText[0];
            memset(szErr + 1, 0, sizeof(szErr) - 1);
            g_UIResources.LoadLocaleString(0x14a, szErr, sizeof(szErr));
            MessageBoxA((HWND)0, szErr, "LEGO LOCO", 0x30);
            msg.wParam = -1;
        }
        else {
            AppWindow_ParseCommandLine(lpCmdLine);
            if (g_screenSaver.InitAndPlayIntroMusic() == 0) {
                delete g_pApp;
                g_pApp = (AppWindow *)0;
                CoUninitialize();
                msg.wParam = 0;
            }
            else {
                g_pApp->LoadWindowAndBalancing();
                if (g_pApp->CheckMinimumDisplaySpec() == 0) {
                    delete g_pApp;
                    g_pApp = (AppWindow *)0;
                    CoUninitialize();
                    msg.wParam = -3;
                }
                else if (g_screenSaver.bScreenSaverMode == 0 &&
                         (hOther = FindWindowA("LEGO LOCO", "LEGO LOCO")) != (HWND)0) {
                    SetForegroundWindow(hOther);
                    delete g_pApp;
                    g_pApp = (AppWindow *)0;
                    CoUninitialize();
                    msg.wParam = 1;
                }
                else {
                    if (hSplash != (HWND)0) {
                        UpdateWindow(hSplash);
                    }
#ifdef LOCO_PORT
                    // PORT ONLY -- boot milestones. This function owns the FOURTH and most
                    // reachable copy of the "An error occurred while loading" box (string
                    // 0x14a): a nonzero InitSubsystemsAndWindows. The box is modal and the
                    // ExitProcess is on the far side of it, so an unattended run just hangs
                    // with nothing in the log unless these fire. Byte-neutral for the match.
                    Port_Tracef("boot: InitSubsystemsAndWindows enter\n");
#endif
                    nRunResult = g_pApp->InitSubsystemsAndWindows();
#ifdef LOCO_PORT
                    Port_Tracef("boot: InitSubsystemsAndWindows -> %d\n", (int)nRunResult);
#endif
                    if (nRunResult != 0) {
                        memset(szErr + 1, 0, sizeof(szErr) - 1);
                        szErr[0] = g_szScratchText[0];
                        memset(szErr + 1, 0, sizeof(szErr) - 1);
                        g_UIResources.LoadLocaleString(0x14a, szErr, sizeof(szErr));
                        MessageBoxA((HWND)0, szErr, "LEGO LOCO", 0x30);
                        g_pApp->SaveWindowAndCleanExit();
                        delete g_pApp;
                        g_pApp = (AppWindow *)0;
                        CoUninitialize();
                        msg.wParam = nRunResult;
                    }
                    else {
                        if (hSplash != (HWND)0) {
                            UpdateWindow(hSplash);
                        }
                        if (g_screenSaver.bScreenSaverMode == 0) {
                            AppWindow_SetScreenState(2);
                        }
                        else {
                            g_screenSaver.EnterDemoSession();
                            AppWindow_SetScreenState(1);
                        }
#ifdef LOCO_PORT
                        Port_Tracef("boot: screen state set, entering front-end pump (ss=%d)\n",
                                    (int)g_screenSaver.bScreenSaverMode);
#endif
                        ShowWindow(g_pApp->hwndOwner, 1);
                        msg.message = 0;
                        PostMessageA(hSplash, 0x10, 0, 0);
                        DestroyWindow(hSplash);
                        // Front-end pump: plain GetMessage until the app state leaves 2.
                        while (msg.message != 0x12 && IsFrontEndModeMaybe()) {
                            if (GetMessageA(&msg, (HWND)0, 0, 0) > 0) {
                                TranslateMessage(&msg);
                                DispatchMessageA(&msg);
                            }
                            if (g_pDPlaySessionMgr != (DPlaySessionMgr *)0) {
                                GameNet_DrainEventQueue(g_pDPlaySessionMgr);
                            }
#ifdef LOCO_PORT
                            // PORT: the front end needs its own present, and this is the
                            // whole reason the menu came up blank.
                            //
                            // The original draws the front end straight into the REAL primary
                            // surface -- which IS the screen -- so it never needed a present
                            // and this pump has no frame tick at all. The port's primary is an
                            // emulated offscreen buffer (port/PortMode.h), so anything drawn
                            // here stays invisible until it is blitted to the window, and the
                            // only Port_Present in the program sits in FrameDriver_TickMaybe,
                            // which ONLY the in-game pump below ever calls. Result: the menu
                            // ran correctly -- sounds, input, real child controls -- while not
                            // one engine-drawn pixel ever reached the window.
                            //
                            // Presenting per dispatched message rather than on a timer is
                            // enough because this pump blocks in GetMessage: every repaint the
                            // front end performs is itself driven by a message, so a frame that
                            // changed is always followed by another iteration here.
                            Port_Present();
#endif
                        }
#ifdef LOCO_PORT
                        // PORT ONLY -- byte-neutral. The hand-off between the two pumps is the
                        // most consequential uninstrumented transition in the program: the
                        // front-end pump calls Port_Present per dispatched message (see above),
                        // while the in-game pump only reaches it through FrameDriver_TickMaybe
                        // behind the DAT_00485444 latch. So "presents simply stopped" is the
                        // EXPECTED look of leaving the front end, and is indistinguishable from
                        // a hang unless the crossing itself is logged.
                        Port_Tracef("boot: front-end pump exit msg=%04x -> in-game pump\n",
                                    (unsigned int)msg.message);
#endif
                        // In-game pump: PeekMessage; every 15th iteration lets queued input
                        // through without removing it first (PM_NOREMOVE + GetMessage drain).
                        while (msg.message != 0x12) {
                            DAT_004ff12c = 0;
                            DAT_004ff118 = 0;
                            DAT_004ff128 = 0;
                            if (nPumpBudget < 1) {
                                nPumpBudget = 0xe;
                                while (PeekMessageA(&msg, (HWND)0, 0, 0, 0) > 0) {
                                    GetMessageA(&msg, (HWND)0, 0, 0);
                                    TranslateMessage(&msg);
#ifdef LOCO_PORT
                                    Port_PumpStep("dispatch0", &msg);
#endif
                                    DispatchMessageA(&msg);
                                }
                            }
                            else {
                                nPumpBudget = nPumpBudget - 1;
                                while (PeekMessageA(&msg, (HWND)0, 0, 0, 1) > 0) {
                                    TranslateMessage(&msg);
#ifdef LOCO_PORT
                                    Port_PumpStep("dispatch1", &msg);
#endif
                                    DispatchMessageA(&msg);
                                }
                            }
#ifdef LOCO_PORT
                            Port_PumpStep("drained", &msg);
#endif
                            if (DAT_004aa4a4 != 0) {
                                LoadingScreen_Pump(0);
                            }
#ifdef LOCO_PORT
                            Port_PumpStep("loadpump", &msg);
#endif
                            MsgWaitForMultipleObjects(0, (HANDLE *)0, 0, 3, 0xbf);
#ifdef LOCO_PORT
                            Port_PumpStep("msgwait", &msg);
#endif
                            if (DAT_00485444 != 0) {
                                nFrameCount = nFrameCount + 1;
                                FrameDriver_TickMaybe();
                                ResetEvent(DAT_004a990c);
                            }
#ifdef LOCO_PORT
                            Port_PumpStep("tick", &msg);
#endif
                            if (nFrameCount >= DAT_00481914) {
                                if (DAT_00481914 < 100) {
                                    DAT_00481914 = DAT_00481914 + 1;
                                }
                                t = time((time_t *)0);
                                if (nLastTime != t && IsInGameModeMaybe()) {
                                    DAT_00481170 = (double)nFrameCount / ((double)t - (double)nLastTime);
                                }
                                nFrameCount = 0;
                                nLastTime = t;
                            }
#ifdef LOCO_PORT
                            // PORT ONLY -- byte-neutral. The first few in-game iterations, then a
                            // heartbeat. `latch` is DAT_00485444, the gate on FrameDriver_TickMaybe
                            // and hence on the port's only in-game present; if it stays 0 the world
                            // is running and nothing is being drawn, which is a completely
                            // different bug from a pump that never iterates.
                            {
                                static unsigned int nIter = 0;

                                nIter++;
                                if (nIter <= 8 || (nIter % 200) == 0) {
                                    Port_Tracef("pump #%u msg=%04x latch=%d loading=%d frames=%d\n",
                                                nIter, (unsigned int)msg.message, (int)DAT_00485444,
                                                (int)DAT_004aa4a4, (int)nFrameCount);
                                }
                            }
#endif
                        }
#ifdef LOCO_PORT
                        Port_Tracef("boot: in-game pump exit msg=%04x wParam=%d\n",
                                    (unsigned int)msg.message, (int)msg.wParam);
#endif
                        delete g_pApp;
                        g_pApp = (AppWindow *)0;
                        CoUninitialize();
                    }
                }
            }
        }
    }
    return msg.wParam;
}

// FUNCTION: LOCO 0x463430
// The clean-exit dispatcher, called from AppWndProc's quit paths above. If the app is
// shutting down already (state 1 or 10), it just hides the owner window and re-runs the
// ShowWindow pass; otherwise (with a live AppWindow) it parks state 10 and returns, letting
// the message pump unwind. Either way the tail then posts WM_CLOSE to every top-level
// window (0x4634f0), saves window state and destroys the owner window, drains the world-load
// thread, kills the loading timer, and posts WM_QUIT. DAT_00485444 (the frame-presented
// gate) is cleared last. The `state == 1 || state == 10` test is a switch's dec/sub chain
// (a plain `||` emits cmp/cmp instead), and the head's `g_pApp` reads go through one local
// (the original hoists the global into a register before the switch; without the local it
// re-reads the global per branch).
// EFFECTIVE MATCH (v521): identical structure at 53/52 insns, byte_diff 25 -- a pure
// eax/ecx allocator coin-flip (docs/CODEGEN.md #R1): the original spends the 5-byte a1
// encoding on the g_nScreenState load and parks g_pApp in ecx; cl 5.0 does the reverse
// (state in ecx, a1 on the pAppWnd load), which also accounts for the 1-byte length gap
// (180 vs 181). Probes that did NOT move it: if-form vs switch, both/neither local,
// declaration order, local rename, switch operand direct-vs-local.
void __stdcall FUN_00463430_ExitRelatedMaybe(void)
{
    AppWindow *pAppWnd = g_pApp;

#ifdef LOCO_PORT
    // PORT ONLY -- byte-neutral. This is the ONLY PostQuitMessage on the normal path, so it is
    // the one place that answers "who ended the run"; the default arm reroutes through state 10
    // and comes back here a second time, which the two lines together make legible.
    Port_Tracef("quit: ExitRelated state=%d app=%p\n", (int)g_nScreenState, (void *)pAppWnd);
#endif
    switch (g_nScreenState) {
    case 1:
    case 10:
        if (pAppWnd != (AppWindow *)0 && pAppWnd->hwndOwner != (HWND)0) {
            ShowWindow(pAppWnd->hwndOwner, 0);
            FUN_00463670_LotsOfShowWindow();
        }
        break;
    default:
        if (pAppWnd != (AppWindow *)0) {
            AppWindow_SetScreenState(10);
            return;
        }
        break;
    }
    FUN_004634f0_LotsOfSendMessageA();
    if (g_pApp != (AppWindow *)0) {
        g_pApp->SaveWindowAndCleanExit();
        if (g_pApp != (AppWindow *)0 && g_pApp->hwndOwner != (HWND)0) {
            DestroyWindow(g_pApp->hwndOwner);
            g_pApp->hwndOwner = (HWND)0;
        }
    }
    g_worldLoadThread.PollAndResume();
    if (g_uLoadingTimerId != 0 && g_pApp != (AppWindow *)0) {
        KillTimer(g_pApp->hwndOwner, g_uLoadingTimerId);
        g_uLoadingTimerId = 0;
    }
    PostQuitMessage(0);
    DAT_00485444 = 0;
}

// FUNCTION: LOCO 0x4634f0
// The close-all counterpart of FUN_00463670_LotsOfShowWindow below: post WM_CLOSE to every
// top-level window that is currently up, in the same fixed order (credits, tutorial,
// build-tool-cursor, edit-card, mail, album-card, map), then the splash screen's two setup
// pages and the splash screen itself -- re-asserting state 7 through SetState when its video
// player is attached, same as the ShowWindow pass. The nested `if (g_pSplashWnd != 0)`
// re-tests are the original's own (each SendMessageA clobbers the cached global, and the
// guard is genuinely re-evaluated after each reload), mirrored from the 0x463670 shape.
void __stdcall FUN_004634f0_LotsOfSendMessageA(void)
{
    if (g_pCreditsWnd != (CreditsWnd *)0) {
        SendMessageA(g_pCreditsWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pTutorialWnd != (TutorialWnd *)0) {
        SendMessageA(g_pTutorialWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pBuildToolCursorWnd != (BuildToolCursorWnd *)0) {
        SendMessageA(g_pBuildToolCursorWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pEditCardWnd != (EditCardWnd *)0) {
        SendMessageA(g_pEditCardWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pMailWnd != (MailWnd *)0) {
        SendMessageA(g_pMailWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pAlbumCardWnd != (AlbumCardWnd *)0) {
        SendMessageA(g_pAlbumCardWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pMapWnd != (MapWnd *)0) {
        SendMessageA(g_pMapWnd->hwndSelf, WM_CLOSE, 0, 0);
    }
    if (g_pSplashWnd != (SplashWnd *)0) {
        if (g_pSplashWnd->pApplSetupWnd != (ApplSetupWnd *)0) {
            SendMessageA(g_pSplashWnd->pApplSetupWnd->hwndSelf, WM_CLOSE, 0, 0);
        }
        if (g_pSplashWnd != (SplashWnd *)0) {
            if (g_pSplashWnd->pNetSetupWnd != (NetSetupWnd *)0) {
                SendMessageA(g_pSplashWnd->pNetSetupWnd->hwndSelf, WM_CLOSE, 0, 0);
            }
            if (g_pSplashWnd != (SplashWnd *)0) {
                if (g_pSplashWnd->pVideoPlayer != (VideoPlayer *)0) {
                    g_pSplashWnd->SetState(7);
                }
                SendMessageA(g_pSplashWnd->hwndSelf, WM_CLOSE, 0, 0);
            }
        }
    }
}

// FUNCTION: LOCO 0x463670
// The app's window-visibility pass: re-shows (nCmdShow 7 = SW_SHOWNA, no focus steal) every
// top-level window that is currently up, in a fixed order -- credits/tutorial (gated on the
// PopupWndBase bShown flag), the four modal-capture windows (edit-card, mail, album-card, map;
// gated on the WindowBase bModalCaptureActive flag), then the splash screen's two setup pages
// and the splash screen itself (re-asserting state 7 through SetState when its video player is
// attached), and finally the app's owner window unconditionally. Returns 1 once the pass has
// run; in screen-saver mode (the `-s` flag, g_screenSaver.bScreenSaverMode == 1) it does
// nothing and returns 0. Called from AppWndProc (above) and from per-window close paths across
// the window TUs. The return type is a BYTE -- the original materializes `mov al,1` /
// `xor al,al` and leaves the upper eax bits alone, so every file-local decl of this function
// spells it `unsigned char`.
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void)
{
    ApplSetupWnd *pApplSetup;
    NetSetupWnd *pNetSetup;

    if (g_screenSaver.bScreenSaverMode != 1) {
        if (g_pCreditsWnd != (CreditsWnd *)0 && g_pCreditsWnd->bShown != 0) {
            ShowWindow(g_pCreditsWnd->hwndSelf, 7);
        }
        if (g_pTutorialWnd != (TutorialWnd *)0 && g_pTutorialWnd->bShown != 0) {
            ShowWindow(g_pTutorialWnd->hwndSelf, 7);
        }
        if (g_pEditCardWnd != (EditCardWnd *)0 && g_pEditCardWnd->bModalCaptureActive != 0) {
            ShowWindow(g_pEditCardWnd->hwndSelf, 7);
        }
        if (g_pMailWnd != (MailWnd *)0 && g_pMailWnd->bModalCaptureActive != 0) {
            ShowWindow(g_pMailWnd->hwndSelf, 7);
        }
        if (g_pAlbumCardWnd != (AlbumCardWnd *)0 && g_pAlbumCardWnd->bModalCaptureActive != 0) {
            ShowWindow(g_pAlbumCardWnd->hwndSelf, 7);
        }
        if (g_pMapWnd != (MapWnd *)0 && g_pMapWnd->bModalCaptureActive != 0) {
            ShowWindow(g_pMapWnd->hwndSelf, 7);
        }
        if (g_pSplashWnd != (SplashWnd *)0) {
            pApplSetup = g_pSplashWnd->pApplSetupWnd;
            if (pApplSetup != (ApplSetupWnd *)0 && pApplSetup->field_0xe8 != 0) {
                ShowWindow(pApplSetup->hwndSelf, 7);
            }
            if (g_pSplashWnd != (SplashWnd *)0) {
                pNetSetup = g_pSplashWnd->pNetSetupWnd;
                if (pNetSetup != (NetSetupWnd *)0 && pNetSetup->bModalCaptureActive != 0) {
                    ShowWindow(pNetSetup->hwndSelf, 7);
                }
                if (g_pSplashWnd != (SplashWnd *)0 && g_pSplashWnd->bModalCaptureActive != 0) {
                    if (g_pSplashWnd->pVideoPlayer != (VideoPlayer *)0) {
                        g_pSplashWnd->SetState(7);
                    }
                    ShowWindow(g_pSplashWnd->hwndSelf, 7);
                }
            }
        }
        ShowWindow(g_pApp->hwndOwner, 7);
        return 1;
    }
    return 0;
}
