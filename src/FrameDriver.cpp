// FrameDriver -- the per-frame pump pair. FrameDriver_TickMaybe (0x45c3c0) is THE frame
// driver: the in-game message loop (src/Main.cpp) calls it once per presented frame, gated on
// the DAT_00485444 latch that GameLoopTimerProcMaybe (0x45c520) -- the 28 ms timeSetEvent
// callback the bootstrap arms (see src/AppWindow.cpp) -- sets on every fire. The tick
// re-stamps g_dwGameTick from the wall clock (this is the "timer proc ... calling _time() on
// it" src/ScriptEventLoader.cpp's singleton notes refer to), drains the net event queue,
// ticks the screen saver, and then -- unless the app is in a front-end/shutdown state --
// runs every per-frame subsystem: the peer-train sound-state refresh + queue drain, the
// effect spawner, the placement cursor, the build toolbar, and (in-game/map-kiosk only) the
// selection widget, the world action cursor, the session event queue and the decor manager,
// finishing with the world board's dirty-tile flush.
//
// This TU's original .obj also OWNED the 13 app-wide singleton definitions whose CRT
// static-init/atexit thunk pairs run 0x45c530..0x45c790 (g_screenSaver, g_PeerTrainSlotQueue,
// PlacementCursorMaybe_004854c8, DecorObjMgrMaybe_00485448, g_NetSessionEventQueue, the
// ScriptEventLoader singleton at 0x4a99b0, ...). In the src tree those singletons are defined
// by their own TUs, so the thunk region stays unclaimed -- the same fold debt v494 noted for
// the ScriptEventLoader pair at 0x45c650/0x45c670. The .obj bracket is GetDSoundErrorString
// (0x45c2e0, transcribed EXACT below, v498) above and GeomUtil's pair (0x45c7a0/0x45c7c0, src/GeomUtil.cpp)
// below.
#include <time.h>

#include "ScreenSaver.h"             // g_screenSaver (0x4a9910)
#include "DPlaySessionMgr.h"         // g_pDPlaySessionMgr (0x4fd3ac)
#include "GameNetMsgQueue.h"         // g_nScreenState (0x4851f4)
#include "PeerTrainSlotQueueMaybe.h" // g_PeerTrainSlotQueue (0x4a98b0)
#include "PeerTrainNode.h"           // PeerTrainNodePartial::SetSoundStateMaybe
#include "EffectSpawner.h"           // DAT_004fd220
#include "PlacementCursorMaybe.h"    // PlacementCursorMaybe_004854c8
#include "BuildToolButton.h"         // g_BuildToolButton (0x4aa5b8)
#include "WorldActionCursor.h"       // g_worldActionCursor (0x4a9ef0), SelectedObjWidgetMaybe_004852a0
#include "NetSessionEventQueue.h"    // g_NetSessionEventQueue (0x4a9990)
#include "DecorObjMgrMaybe.h"        // DecorObjMgrMaybe_00485448
#include "WorldBoardMaybe.h"         // g_worldBoard (0x4aad08)
#include "ThreadWrapper.h"           // g_worldLoadThread (0x4a9ad0)
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: RGB565 surface pinning, see port/README.md
#endif

// The world-load worker thread AppWindow_StartGame/EnterBuildMode Start() on (declared extern
// in src/AppWindow.cpp). The frame driver's original .obj OWNED this definition -- its
// static-init/atexit thunk pair is the 0x45c770/0x45c790 run in the unclaimed region below
// (see the header comment) -- so the definition lives here too. Its static-dtor thunk
// tail-calls ??1ThreadWrapper in the original; ours inlines the now-in-class dtor body into
// the $E thunk instead (see src/ThreadWrapper.cpp's trade note -- the thunks are unverified,
// so this is byte-free).
ThreadWrapper g_worldLoadThread; // DAT_004a9ad0

// Cross-TU callees with no shared-header declaration yet (the Main.cpp pattern: a free
// __fastcall reproduces the ecx = singleton + call shape without touching a dial-loaded
// header). EffectSpawner's per-frame tick walks both effect collections. (The selection-widget
// tick that used to be declared here is now the real member
// SelectedObjWidgetMaybe::AdvanceAnimFrameMaybe, v505; the session-queue tick that used to be
// shimmed here as `NetSessionEventQueue_FUN_0041dd40` is now the real member
// NetSessionEventQueue::AdvanceAllAnimFramesMaybe, v538, reached through the header already
// included below for g_NetSessionEventQueue.)
void __fastcall GameNet_DrainEventQueue(DPlaySessionMgr *pMgr);     // 0x43f0c0, see src/DPlaySessionMgr.cpp
// (PeerTrainSlotQueueMaybe::DrainPendingSlotsMaybe, 0x44e020, used to be shimmed here as a
// free __fastcall while its header declaration was a live dial; v522 landed the real member
// on the class, so the calls below go through PeerTrainSlotQueueMaybe.h directly.)

// The frame-presented latch: GameLoopTimerProcMaybe sets it on every 28 ms timer fire, the
// in-game loop in src/Main.cpp gates the frame pump on it, and the tick below clears it
// again (drives the FPS counter tick).
extern unsigned char DAT_00485444;
// The sound suspend/resume latch pair around the peer-train queue drain. While
// DAT_004ff124 is set (sound suspended, e.g. focus loss) and DAT_004ff11c has not yet been
// serviced, the tick re-asserts sound state 2 on every parked train, drains the queue,
// clears the serviced flag and drops them all back to sound state 0.
extern unsigned char DAT_004ff124;
extern unsigned char DAT_004ff11c;
extern unsigned int g_dwGameTick; // DAT_004a99b4 -- wall-clock seconds, see src/ScriptEventLoader.cpp

// Same load-bearing `unsigned char` byte-predicate lever as src/Main.cpp's pair (docs/CODEGEN.md,
// v356): it reproduces the original's sete-materialized `xor r,r; cmp; sete r; test; jcc`
// branch shape. State 9 is the MapWnd "map kiosk" (docs/subsystems.md).
inline unsigned char IsInGameModeMaybe() { return g_nScreenState == 3; }
inline unsigned char IsMapKioskModeMaybe() { return g_nScreenState == 9; }

// FUNCTION: LOCO 0x45c2e0
// LocoBitmap_GetDSoundErrorString (Ghidra: LocoBitmap::GetDSoundErrorString) -- maps a
// DirectSound HRESULT to a static English "Channel error DSERR_*" literal for the sound
// channel's error logging (called from src/SoundBank.cpp and src/DSoundChannel.cpp, both via
// extern "C" decls). Sibling of src/Ddraw.cpp's Ddraw_HResultToString (0x45bbc0): same
// SDK-sample-derived sparse-switch shape, small enough here to stay a pure compare tree.
char *LocoBitmap_GetDSoundErrorString(int hresult) {
    switch (hresult) {
    case 0:            return "No error.";
    case -0x7fffbfff: return "Channel error DSERR_UNSUPPORTED";
    case -0x7fffbffe: return "Channel error DSERR_NOINTERFACE";
    case -0x7fffbffb: return "Channel error DSERR_GENERIC";
    case -0x7ff8fff2: return "Channel error DSERR_OUTOFMEMORY";
    case -0x7ff8ffa9: return "Channel error DSERR_INVALIDPARAM";
    case -0x7787fff6: return "Channel error DSERR_ALLOCATED";
    case -0x7787ffe2: return "Channel error DSERR_CONTROLUNAVAIL";
    case -0x7787ffce: return "Channel error DSERR_INVALIDCALL";
    case -0x7787ffba: return "Channel error DSERR_PRIOLEVELNEEDED";
    case -0x7787ff9c: return "Channel error DSERR_BADFORMAT";
    case -0x7787ff88: return "Channel error DSERR_NODRIVER";
    case -0x7787ff7e: return "Channel error DSERR_ALREADYINITIALIZED";
    case -0x7787ff60: return "Channel error DSERR_OTHERAPPHASPRIO";
    case -0x7787ff56: return "Channel error DSERR_UNINITIALIZED";
    default:          return "Unrecognized error value.";
    }
}

// FUNCTION: LOCO 0x45c3c0
void __stdcall FrameDriver_TickMaybe(void) {
    PeerTrainNodePartial **ppNode;
    int i;

    DAT_00485444 = 0;
    time((time_t *)&g_dwGameTick);
    if (g_pDPlaySessionMgr != (DPlaySessionMgr *)0) {
        GameNet_DrainEventQueue(g_pDPlaySessionMgr);
    }
    g_screenSaver.Tick();
    if (g_nScreenState <= 0 || (g_nScreenState > 2 && g_nScreenState != 10)) {
        if (IsInGameModeMaybe() || IsMapKioskModeMaybe()) {
            if (DAT_004ff124 == 1) {
                if (DAT_004ff11c == 1 && g_PeerTrainSlotQueue.nActiveCount > 0) {
                    ppNode = g_PeerTrainSlotQueue.aSlots;
                    for (i = 0; i < 4; i++) {
                        if (*ppNode != 0) {
                            (*ppNode)->SetSoundStateMaybe(2);
                        }
                        ppNode++;
                    }
                }
                g_PeerTrainSlotQueue.DrainPendingSlotsMaybe();
                DAT_004ff11c = 0;
                if (g_PeerTrainSlotQueue.nActiveCount > 0) {
                    ppNode = g_PeerTrainSlotQueue.aSlots;
                    for (i = 0; i < 4; i++) {
                        if (*ppNode != 0) {
                            (*ppNode)->SetSoundStateMaybe(0);
                        }
                        ppNode++;
                    }
                }
            }
            else {
                g_PeerTrainSlotQueue.DrainPendingSlotsMaybe();
            }
        }
        DAT_004fd220.EffectSpawner_TickMaybe();
        PlacementCursorMaybe_004854c8.AdvanceAnimFrameMaybe();
        g_BuildToolButton.AdvanceAnimFrameMaybe();
        if (IsInGameModeMaybe() || IsMapKioskModeMaybe()) {
            SelectedObjWidgetMaybe_004852a0.AdvanceAnimFrameMaybe();
            g_worldActionCursor.AdvanceAnimFrameMaybe();
            g_NetSessionEventQueue.AdvanceAllAnimFramesMaybe();
            DecorObjMgrMaybe_00485448.TickCategory7And8Maybe();
        }
        g_worldBoard.UpdateDirtyTiles(0);
    }
#ifdef LOCO_PORT
    // PORT: one present per frame, unconditionally. Putting it here rather than in
    // Ddraw_BltUpdateRect is deliberate -- the board path is not the only writer of
    // the (emulated) primary; PopupWndBase Blts to it directly from ~10 sites, and
    // a present driven off any single blit site would miss those.
    Port_Present();
#endif
}

// FUNCTION: LOCO 0x45c520
// The multimedia-timer callback the bootstrap arms at 28 ms (src/AppWindow.cpp): does nothing
// but raise the frame-presented latch the in-game loop polls -- the actual frame pump runs on
// the main thread, in FrameDriver_TickMaybe above. Ghidra types it `void (void)` because the
// body ignores every callback argument; the declaration (and the `ret 0x14`) is
// timeSetEvent's LPTIMECALLBACK.
void CALLBACK GameLoopTimerProcMaybe(UINT uId, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2) {
    DAT_00485444 = 1;}
