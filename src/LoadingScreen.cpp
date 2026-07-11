// LoadingScreen -- the "please wait" animation the cold-start path puts up while the world
// loads, plus the off-thread world load itself.
//
// The TU's real extent is 0x45de40..0x45e484: five functions sitting between TrackGraph.cpp's
// run (which ends at 0x45de39) and GameNet.cpp's GNetManager block (which starts at 0x45e490).
// Found 2026-07-26 by the "unclaimed function bracketed by same-file markers" sweep -- this run
// is bracketed by two DIFFERENT TUs, which is exactly why it stayed invisible for so long.
//
// The screen is driven entirely through one global animation object (DAT_004fd3d8), a plain
// AnimDescRefObj0x477488 on resource 0x402 centred on the display. AppWindow_StartGame raises it
// (LoadingScreen_Show), pumps it between each window's backdrop realization
// (LoadingScreen_Pump -> WM_USER+7 -> LoadingScreen_Animate), and AppWndProc tears it down on
// WM_USER+5 (LoadingScreen_Hide). DAT_004fd3d4 is its never-constructed twin: nothing in .text
// ever assigns it a non-NULL value, yet both this file and AppWindow::SaveWindowAndCleanExit
// still delete it -- vestigial, reproduced as-is.

#include <windows.h>
#include <ddraw.h>
#include <mmsystem.h>
#include <stdio.h>

#include "AppWindow.h"              // g_pApp (0x4aa4a0)
#include "BuildToolButton.h"        // g_BuildToolButton (0x4aa5b8)
#include "CursorDesc.h"             // BigObj (pKindDesc->nativeHeight)
#include "DPlaySessionMgr.h"        // g_pDPlaySessionMgr (0x4fd3ac)
#include "EasterEggMgr.h"           // g_easterEggMgrMaybe (0x4a99b0)
#include "GameNetMsgQueue.h"        // g_nScreenState (0x4851f4)
#include "NetSessionEventQueue.h"   // g_NetSessionEventQueue (0x4a9990)
#include "PlacementCursorMaybe.h"   // PlacementCursorMaybe_004854c8
#include "ScreenSaver.h"            // g_screenSaver (0x4a9910)
#include "WidgetBase.h"             // AnimDescRefObj0x477488
#include "WorldActionCursor.h"      // g_worldActionCursor, SelectedObjWidgetMaybe_004852a0
#include "WorldBoardMaybe.h"        // g_worldBoard (0x4aad08)

#ifdef LOCO_PORT
// PORT ONLY. Deliberately declared INSIDE the guard rather than hoisted to a shared header:
// src/AppWindow.h's declaration count is a measured byte dial (see CLAUDE.md), and a
// declaration that cannot exist in the match build cannot move it. src/AppWindow.cpp:128-129
// owns the real spelling; keep the two in step.
extern int g_dwScreenWidth;   // DAT_004851d8 -- clamped by Port_ClampScreenSize
extern int g_dwScreenHeight;  // DAT_00485214
#include "PortMode.h"         // Port_Tracef -- the world-load abort diagnostics below
#endif

// 0x408130 -- the app-wide UI-mode switch. Declared file-locally the same way src/MapWnd.cpp,
// src/AlbumCardWnd.cpp and src/MailWnd.cpp do.
void AppWindow_SetScreenState(int newState); // TODO: idiom

// 0x401280 -- Ddraw::Ddraw_BltUpdateRect, the work-surface -> primary presenter every screen
// ends a repaint with. Declared file-locally exactly as src/WindowBase.cpp and src/Main.cpp do.
void __cdecl Ddraw_BltUpdateRect(RECT *pRect, HWND hwnd, POINT *pScrollOffset, char bParam); // TODO: idiom

// The shared DDraw back/work surface (this screen paints straight onto it through a GDI DC), and
// the app's client-area bounds template, both declared file-locally exactly as src/MapWnd.cpp and
// src/PopupWndBase.cpp do.
extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4
extern RECT g_rectAppClientBounds;              // 0x485220 (tagRECT_00485220)

// The loading screen's two animation refs; src/AppWindow.cpp holds the other declaration.
// Only the second is ever constructed -- see this file's header comment.
extern AnimDescRefObj0x477488 *DAT_004fd3d4;    // DAT_004fd3d4  // TODO: idiom
extern AnimDescRefObj0x477488 *DAT_004fd3d8;    // DAT_004fd3d8  // TODO: idiom

// The per-tick gate the WM_TIMER 0x47 handler raises and this screen's animate step clears;
// src/Main.cpp holds the other declaration.
extern unsigned char DAT_004aa4a4;              // DAT_004aa4a4  // TODO: idiom

// lego.ini's [PROCESS] CleanExit, and the loose-file loader's install-path prefix. Declared
// file-locally exactly as src/AppWindow.cpp and src/DSoundChannel.h's own consumers do.
extern char g_bCleanExit;                       // DAT_00485218
extern char g_pInstallPathPrefix[];             // DAT_004a99c8

// One more of the cold-start load's `this`-only subsystem bring-up calls, a real member
// of an already-modeled class that its own header deliberately does not declare (adding those
// decls measurably rotated the sibling functions in their home TUs -- see
// src/WorldActionCursor.cpp's own note). Spelled with the __fastcall escape hatch, which is
// byte-identical to a this-call whose argument list is empty.
void __fastcall WorldBoardMaybe_ResetAllTilesMaybe(WorldBoardPartial *pBoard);                // 0x454fe0  // TODO: idiom

// The two screen-state predicates the cold-start wait loop spins on. The `unsigned char` return
// type is LOAD-BEARING -- it is what reproduces the sete-materialized branches; kept TU-local for
// the same reason src/Main.cpp, src/MapWnd.cpp and src/GameNet.cpp keep their own copies.
inline unsigned char IsAppStateOneMaybe() { return g_nScreenState == 1; }
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// The off-thread world load AppWindow_StartGame hands to g_worldLoadThread on a cold start: it
// brings up every subsystem whose data is too slow to load on the UI thread (placement sounds,
// the seasonal event table, the build-tool and train-coupling menu icons, the category icon
// toolbox), each failure closing the app with its own numbered WM_CLOSE code, then waits for the
// UI thread to settle on screen state 1 before deciding WHICH board layout to bring in: the
// screen saver's randomly-picked one, the connected provider's `Layouts\<name>`, or the "~curr"
// checkpoint (the last only if the previous run set [PROCESS] CleanExit).
//
// Ghidra types it `void (void)` because the body ignores the thread argument, but
// ThreadWrapper::Start's routine pointer is `void (__cdecl *)(void *)`; src/AppWindow.cpp
// declares it the same way.
//
// EFFECTIVE MATCH -- 572 B vs 591 B, insns 167/175, and the WHOLE residual is four REDUNDANT
// `test al,al; jne <same target>` pairs the original keeps and our cl 11.00 folds away, one after
// each of the four `char`-returning bring-up calls. (The first check escapes it because its
// callee returns a real `bool`.) This is the documented fold-side optimizer-STRENGTH class in
// docs/CODEGEN.md -- writing the redundancy out literally as a nested `if (!bOk) { if (!bOk) ...`
// was probed and cl folds THAT too, byte-for-byte identically, so the shape is unreachable from
// source. Everything else aligns instruction-for-instruction. Two levers WERE found and are
// load-bearing here: the two `unsigned char` inline state predicates (without them the wait loop
// compiles a plain `cmp/je` instead of the original's `sete`-materialized branches), and writing
// the "~curr" fallback out in BOTH arms so VC5 cross-jumps them itself.
//
// FUNCTION: LOCO 0x45de40
void __cdecl App_LoadWorldThreadProcMaybe(void *pArg)
{
    // Declared highest-slot-first because that is the order the original initializes them in --
    // the same `char[N] = ""` one-byte-copy-then-zero-fill shape (and the same reading of the
    // init order) as AppWindow_StartGame's own pair, src/AppWindow.cpp.
    char szInstallPath[256] = "";
    char szProviderLayoutPath[261] = "";
    char szScreenSaverLayout[261] = "";

#ifdef LOCO_PORT
    // PORT ONLY -- every `return` below is a world-load abort, and AppWndProc answers a WM_CLOSE
    // with a nonzero wParam by putting up resource string 0x14a ("An error occurred while
    // loading."). So each of these five wParam codes IS that message box, and naming which one
    // fired is the difference between a diagnosis and another run. Byte-neutral for the match.
    #define LOCO_PORT_LOADFAIL(step, code) \
        Port_Tracef("LOAD ABORT: %s failed -> WM_CLOSE wParam=%d (msgbox 0x14a)\n", step, code)
    // ENTRY markers, added v571. The five LOADFAIL codes above only report an ORDERLY abort; a
    // stage that FAULTS reports nothing, and because this runs on g_worldLoadThread a fault here
    // kills the process from under the UI thread -- which reads as the main pump stopping at an
    // arbitrary point, with no wine exception, no WM_QUIT and no ExitRelated. The last STEP line
    // in the log is then the only thing that names the stage. Byte-neutral for the match.
    #define LOCO_PORT_LOADSTEP(step) Port_Tracef("LOAD STEP: %s\n", step)
#else
    #define LOCO_PORT_LOADFAIL(step, code) ((void)0)
    #define LOCO_PORT_LOADSTEP(step) ((void)0)
#endif
    LOCO_PORT_LOADSTEP("thread entered");
    if (!PlacementCursorMaybe_004854c8.PreloadPlacementSoundsMaybe()) {
        LOCO_PORT_LOADFAIL("PreloadPlacementSoundsMaybe", 1);
        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 1, 0);
        return;
    }
    LOCO_PORT_LOADSTEP("LoadTimeEventScriptsMaybe");
    if (!g_easterEggMgrMaybe.LoadTimeEventScriptsMaybe("ee")) {
        LOCO_PORT_LOADFAIL("LoadTimeEventScriptsMaybe", 3);
        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 3, 0);
        return;
    }
    LOCO_PORT_LOADSTEP("ResetAllTilesMaybe");
    WorldBoardMaybe_ResetAllTilesMaybe(&g_worldBoard);
    LOCO_PORT_LOADSTEP("InitMenuIconsMaybe");
    if (!g_BuildToolButton.InitMenuIconsMaybe()) {
        LOCO_PORT_LOADFAIL("InitMenuIconsMaybe", 5);
        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 5, 0);
        return;
    }
    LOCO_PORT_LOADSTEP("InitTrainCouplingMenuIconsMaybe");
    if (!g_worldActionCursor.InitTrainCouplingMenuIconsMaybe()) {
        LOCO_PORT_LOADFAIL("InitTrainCouplingMenuIconsMaybe", 6);
        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 6, 0);
        return;
    }
    LOCO_PORT_LOADSTEP("LoadCategoryIconsMaybe");
    if (!SelectedObjWidgetMaybe_004852a0.LoadCategoryIconsMaybe()) {
        LOCO_PORT_LOADFAIL("LoadCategoryIconsMaybe", 7);
        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 7, 0);
        return;
    }

    // sic: the formatted install path is never read again -- the leftover of a load step that
    // used to build a path here. Reproduced because the buffer and the call are both real.
    sprintf(szInstallPath, "%s", g_pInstallPathPrefix);

    LOCO_PORT_LOADSTEP("wait for screen state 1");
    while (!IsAppStateOneMaybe() && !IsNetShuttingDownMaybe())
        Sleep(20);
    if (IsNetShuttingDownMaybe())
        return;

    // The "~curr" fallback is written out in BOTH the screen-saver and the single-player arms;
    // VC5 cross-jumps the two copies into one block, which is why the screen saver's own
    // g_bCleanExit test sits inline while the other's shares the merged tail.
    LOCO_PORT_LOADSTEP("selecting board layout");
    if (g_screenSaver.bScreenSaverMode == 1) {
        g_screenSaver.GetLayoutFileName(szScreenSaverLayout);
        if (!g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)szScreenSaverLayout)) {
            if (g_bCleanExit != 0)
                g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
        }
    } else if (g_pDPlaySessionMgr->connectionMode == 2) {
        DPlaySessionMgrProviderSlot *pSlot = g_pDPlaySessionMgr->GetSelectedProvider();
        if (pSlot != NULL) {
            wsprintfA(szProviderLayoutPath, "Layouts\\%s", pSlot->sLongName);
            g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)szProviderLayoutPath);
            g_NetSessionEventQueue.SaveBoardLayout((unsigned char *)"~curr");
        }
    } else {
        if (g_bCleanExit != 0)
            g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
    }
    LOCO_PORT_LOADSTEP("layout loaded, PostQuitRequest");
    g_screenSaver.PostQuitRequest();
}

// Raises the loading screen. Stops whatever sound is playing, whitens the whole client area
// straight through the work surface's GDI DC (GetStockObject(WHITE_BRUSH), not a black one --
// the art is a light-on-white animation), then builds the animation object on resource 0x402,
// centres it (a quarter of the way across the display, vertically centred on its own art's
// native height), silences its sound channel and paints its first frame.
//
// FUNCTION: LOCO 0x45e090
void LoadingScreen_Show()
{
    HDC hdc = NULL;

    PlaySoundA(NULL, NULL, 0);
    g_pDDrawWorkSurface->GetDC(&hdc);
    FillRect(hdc, &g_rectAppClientBounds, (HBRUSH)GetStockObject(WHITE_BRUSH));
    g_pDDrawWorkSurface->ReleaseDC(hdc);

    DAT_004fd3d8 = new AnimDescRefObj0x477488(0x402, -1, 0, 0);
#ifdef LOCO_PORT
    // PORT: this is the ONE place in src/ that asks the OS for the screen size instead of
    // reading the two globals AppWindow::LoadWindowAndBalancing already clamped (0x406480, see
    // port/PortMode.h). In 1998 those were the same number; here the desktop is whatever the
    // host says -- 3600x2338 under winemac -- while the work surface this widget paints into
    // is g_dwScreenWidth x g_dwScreenHeight. Unclamped, the loading animation landed at
    // (3600/4, 2338/2 - 245/2) = (900, 1047) on a 1024x768 surface, and because the raw-pixel
    // path of LocoBitmap::RestoreOverlapBlt does NO clipping unless the caller passes flag
    // 0x40 (this one passes 0), PixelCopyColorKeyBlit walked ~570 KB past the end of the
    // locked surface and into the CRT heap. It landed on the live SplashWnd object, whose
    // vtable pointer then became pixel data -- reached later as a wild call through
    // WindowBase_RouteMessage. The three unrelated-looking crashes chased in v562 (a DSound
    // channel with pBuffer == 0xffffffff, a fault inside ntdll's heap walker under
    // `new LocoBitmap`, and that vptr call) were all this one write.
    DAT_004fd3d8->RepositionWithHotspot(g_dwScreenWidth / 4,
                                        g_dwScreenHeight / 2 -
                                            DAT_004fd3d8->pKindDesc->nativeHeight / 2);
#else
    DAT_004fd3d8->RepositionWithHotspot(GetSystemMetrics(SM_CXSCREEN) / 4,
                                        GetSystemMetrics(SM_CYSCREEN) / 2 -
                                            DAT_004fd3d8->pKindDesc->nativeHeight / 2);
#endif
    DAT_004fd3d8->ReleaseChannelAndDispatch(0);
    DAT_004fd3d8->BlitAnimFrameMaybe(DAT_004fd3d8->rect, 0, 0);
    Ddraw_BltUpdateRect(&g_rectAppClientBounds, g_pApp->hwndOwner, NULL, 0);
}

// The between-stages pump AppWindow_StartGame runs after each window realizes its backdrop:
// bounces straight back into LoadingScreen_Animate through the app's own window proc so the
// animation keeps moving while the front end is still building itself.
//
// FUNCTION: LOCO 0x45e1e0
void LoadingScreen_Pump(unsigned int bFullCycle)
{
    SendMessageA(g_pApp->hwndOwner, WM_USER + 7, (unsigned char)bFullCycle, 0);
}

// One animation step, reached through WM_USER+7. bFullCycle == 0 is the ordinary "advance one
// frame and present it" tick; non-zero restarts the animation from its first frame and then runs
// three more frames by hand with a 75 ms beat between them, holding the last for 250 ms -- the
// deliberate little flourish the cold-start path plays once the world is up.
//
// FUNCTION: LOCO 0x45e210
void __cdecl LoadingScreen_Animate(char bFullCycle)
{
    DAT_004aa4a4 = 0;
    if (DAT_004fd3d8 == NULL)
        return;

    if (bFullCycle == 0) {
        DAT_004fd3d8->AdvanceAnimFrameMaybe();
        DAT_004fd3d8->BlitAnimFrameMaybe(DAT_004fd3d8->rect, 0, 0);
        Ddraw_BltUpdateRect(&DAT_004fd3d8->rect, g_pApp->hwndOwner, NULL, 0);
    } else {
        DAT_004fd3d8->ReleaseChannelAndDispatch(1);
        DAT_004fd3d8->BlitAnimFrameMaybe(DAT_004fd3d8->rect, 0, 0);
        Ddraw_BltUpdateRect(&DAT_004fd3d8->rect, g_pApp->hwndOwner, NULL, 0);
        Sleep(75);

        DAT_004fd3d8->AdvanceAnimFrameMaybe();
        DAT_004fd3d8->BlitAnimFrameMaybe(DAT_004fd3d8->rect, 0, 0);
        Ddraw_BltUpdateRect(&DAT_004fd3d8->rect, g_pApp->hwndOwner, NULL, 0);
        Sleep(75);

        DAT_004fd3d8->AdvanceAnimFrameMaybe();
        DAT_004fd3d8->BlitAnimFrameMaybe(DAT_004fd3d8->rect, 0, 0);
        Ddraw_BltUpdateRect(&DAT_004fd3d8->rect, g_pApp->hwndOwner, NULL, 0);
        Sleep(75);

        DAT_004fd3d8->AdvanceAnimFrameMaybe();
        DAT_004fd3d8->BlitAnimFrameMaybe(DAT_004fd3d8->rect, 0, 0);
        Ddraw_BltUpdateRect(&DAT_004fd3d8->rect, g_pApp->hwndOwner, NULL, 0);
        Sleep(250);
    }
}

// Lowers the loading screen: one last full-cycle animate, hand the app to build mode, drop both
// animation refs, stop the sound and give the main window its input back.
//
// FUNCTION: LOCO 0x45e400
void __stdcall LoadingScreen_Hide(void)
{
    SendMessageA(g_pApp->hwndOwner, WM_USER + 7, 1, 0);
    AppWindow_SetScreenState(3);
    if (DAT_004fd3d4 != NULL) {
        delete DAT_004fd3d4;
        DAT_004fd3d4 = NULL;
    }
    if (DAT_004fd3d8 != NULL) {
        delete DAT_004fd3d8;
        DAT_004fd3d8 = NULL;
    }
    PlaySoundA(NULL, NULL, 0);
    EnableWindow(g_pApp->hwndOwner, TRUE);
    SetFocus(g_pApp->hwndOwner);
}
