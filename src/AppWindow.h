// AppWindow -- the App singleton (g_pApp/DAT_004aa4a0). Renamed v324 from AppMaybe.h's
// AppHwndPartial per the Config == AppWindow == g_pApp class unification (user finding; the
// Ghidra `Config` and `AppWindow` namespaces are all methods of this one object, see
// docs/subsystems.md's App-bootstrap section). Only the one field every current
// consumer touches (hwndOwner) is modeled; consolidated 2026-07-17 from two independently
// named per-TU partial views (DSound.cpp's AppPartial, WindowBase.cpp's AppHwndPartial) that
// had drifted into the "duplicate struct" hazard CLAUDE.md's struct-discipline rule forbids --
// the naming lint didn't catch it since the two copies had different names despite identical
// layout. See CLAUDE.md's "never duplicate a struct definition across TUs" correction.
#pragma once

#include <windows.h>
#include "IniFile.h"  // g_pIniFile -- the dtor below tears it down inline
#include <winver.h>  // VerQueryValueA / GetFileVersionInfoA -- ReadOwnFileVersion (0x4062e0)

// Alloc confirmed 0x28
struct AppWindow {
    // A Config-namespace helper (Ghidra: AppWindow::AbortMultiplayerSession,
    // __stdcall/void) that ignores `this` -- but every call site still loads ecx = g_pApp, so it
    // is modeled as a member to reproduce that this-load (same this-in-ecx-but-never-read class
    // as the UIResources helpers). Tears down the DPlaySessionMgr singleton (if any), sets a
    // still-unmodeled flag byte at g_pApp+4, ducks the DSound manager, resets the WorldBoard's
    // tiles and a second still-unmodeled ResetObj0x44d870 singleton, then dispatches
    // AppWindow_SetScreenState(2) -- a UI-mode transition. Sole confirmed caller
    // (DPlaySessionMgr.cpp) reaches it after a MessageBoxA "connection lost"-shaped notice, so
    // this reads as the multiplayer-abort/return-to-front-end path; the exact meaning of mode 2
    // and the g_pApp+4 flag are not yet pinned down.
    void AbortMultiplayerSession();  // 0x406e80

    // Another this-ignoring Config-namespace helper of the same class (its only two call sites,
    // both in ScreenSaver::LoadPasswordProvider, load ecx = g_pApp before the call even though
    // the body never touches it). Reads one REG_SZ-shaped value out of
    // HKEY_LOCAL_MACHINE\<pszSubKey>: returns 1 on success, and on a failed query zeroes the
    // caller's first byte so the buffer reads as an empty string. A failed RegOpenKeyEx leaves
    // the buffer untouched.
    char ReadHklmValue(const char *pszSubKey, LPBYTE pData, DWORD cbData,
                       const char *pszValueName); // 0x408a30

    // The horizontal / vertical board scrollers (SetScrollRange + SetScrollPos wrappers) taking
    // the originating WM_HSCROLL/WM_VSCROLL lParam and a signed scroll delta. Ghidra reads both
    // as __stdcall free functions, but every call site -- AppWndProc's scroll and WM_SIZING
    // cases -- loads ecx = g_pApp first, so they are members of the app singleton (the same
    // this-in-ecx-but-maybe-never-read class as AbortMultiplayerSession above). Neither body is
    // transcribed yet. Added to this SHARED header rather than a TU-local view after measuring
    // that it costs nothing repo-wide -- the v367 ReadHklmValue precedent.
    //
    // 0x4085e0 -- swallow or replay the mouse input queued up behind a blocking animation.
    // A real member whose body ignores `this`; promoted from a free __stdcall spelling in
    // v428 once the dial below was already spent (see the MEASURED DIAL note).
    void DrainQueuedMouseInput(char bDiscard);            // 0x4085e0

    // 0x4077a0 -- the whole-app teardown reached from AppWindow_SetScreenState's case 0xa via
    // WM_CLOSE: flushes the window rect and the CleanExit flag to lego.ini, stops both worker
    // threads, and deletes every singleton the app owns. Another `this`-ignoring member (both
    // call sites load ecx = g_pApp first).
    void SaveWindowAndCleanExit();                        // 0x4077a0

    // The two halves of the startup bootstrap, both called once from LocoWinMain and both real
    // __thiscall members (each call site loads ecx = g_pApp first -- 0x462feb and 0x463153).
    // Promoted off their free spellings in v448: src/Main.cpp had been reaching them through a
    // TU-local view under DIFFERENT names, so the emitted calls targeted symbols that exist
    // nowhere (tools/lint_alias.py). LoadConfigDirectories is another `this`-ignoring member
    // (its body never reads it, which is why the old no-argument __cdecl spelling scored as
    // well as it did); InitSubsystemsAndWindows genuinely uses `this` -- it is the AppWindow*
    // it hands to CreateMainWindow and ConstructSingletonWindows.
    unsigned char LoadConfigDirectories();                // 0x4068d0
    int InitSubsystemsAndWindows();                       // 0x406ba0
    char CreateMainWindow();                              // 0x406ed0

    void ScrollBoardHorizontal(LPARAM lParam, int nDelta); // 0x407ae0
    void ScrollBoardVertical(LPARAM lParam, int nDelta);   // 0x407bf0

    // The other two members LocoWinMain calls, promoted in v488 off src/Main.cpp's TU-local
    // AppWindowMainView0x462e90 view struct (whose remaining member is only the virtual dtor).
    // Both are real __thiscall members that use `this`: the first caches GetDesktopWindow() into
    // hwndDesktopWindow and fills the four [BALANCING] byte fields, the second reads
    // hwndDesktopWindow back to get a screen DC.
    void LoadWindowAndBalancing();                        // 0x406480
    char CheckMinimumDisplaySpec();         // 0x406680

    // 0x4062e0 -- parse the app's own VERSIONINFO FileVersion string into the four dwFileVersion*
    // fields below. Called from the ctor (0x4061e0), which is its only caller.
    void ReadOwnFileVersion();

    // ⚠ MEASURED DIAL (v422-v423) -- SPENT IN v428. Read this before adding a member here.
    // For six sessions this header was frozen because ANY declaration added to it -- member or
    // free function, at any position, under any name, and regardless of how many -- cost
    // src/MailWnd.cpp's RefreshClientClipRect (0x42f8b0) its 1332-byte exact match. v423 proved
    // the dial is SATURATED rather than on a parity cycle (1 and 3 added declarations cost
    // byte-for-byte the same) and priced the whole trade at -1642 B, so it was left alone.
    //
    // v428 spent that 1332 B for an UNRELATED reason (src/EditCardWnd.h had to grow by one
    // declaration to hold EditCardWnd::OnRButtonDown, and that header is on MailWnd.cpp's include
    // list too -- the same knife-edge, reached from a different direction). Once 0x42f8b0 was
    // already gone, this dial became FREE, and the two promotions above were taken:
    //   * DrainQueuedMouseInput (0x4085e0) -> AppWindow_StartGame (0x408350) went to a full
    //     641-byte EXACT, exactly as v423 predicted.
    //   * SaveWindowAndCleanExit (0x4077a0) became declarable at all, and byte-matched at 831 B.
    // ⚠ The second half of v423's price did NOT materialise: src/WorldBoardMaybe.cpp's
    // FindNearestObjOfCategoryMaybe (0x457ce0) was predicted to cost 951 B and did not move at
    // all at this dial position. Do not trust a stale price -- re-measure.
    //
    // ⚠⚠ v448 CORRECTION -- THE DIAL IS ON A PARITY CYCLE, NOT SATURATED. v423's "1 and 3 added
    // declarations cost byte-for-byte the same" no longer holds at this position, and the
    // difference is worth 951 B, so COUNT the declarations you add and measure each step:
    //   * +2 declarations (LoadConfigDirectories, InitSubsystemsAndWindows): 0x457ce0 FALLS,
    //     -951 B. Its residual is then DIFF(16) at 951 B with insns 327/327, align=8,
    //     reg_pen=13 -- a pure zero-register coin-flip (the original keeps the loop/zero value
    //     in ecx, that compile picks eax) plus one movsx scheduling swap. Nothing structural.
    //   * +3 declarations (the two above plus CreateMainWindow): 0x457ce0 comes BACK, and the
    //     whole trade is +723 B / +1 func with NO collateral anywhere in the other 19 consumers.
    // The 3-declaration position is the one that is checked in. Adding a FOURTH may well flip
    // 0x457ce0 straight back off -- which is the standing reason not to promote
    // ConstructSingletonWindows (0x406f90) on a whim: it already matches as a free function at
    // 2052 B, so it can only lose, and it would move the parity too.
    //
    // ⚠⚠⚠ v488 CORRECTION -- THE PARITY MODEL IS REFUTED AT THIS POSITION; THE DIAL IS SATURATED
    // AGAIN, AND 0x457ce0 IS NOW SPENT. Measured this session, one full tools/progress.py per
    // step from the v487 baseline: +1 declaration costs 0x457ce0 its 951 B, and so do +3 and +4.
    // There is no cheap count that buys it back, so v448's "add a third and it returns" recipe
    // does NOT generalise -- do not plan around it.
    //   * The +1 measurement is also the one that refutes CLAUDE.md's general "free-function
    //     declarations do not move it" rule FOR THIS HEADER: the single declaration was
    //     AppWindow_ParseCommandLine, a plain __cdecl free function, and it cost the full 951 B
    //     on its own. An `extern int` variable declaration was ruled out as the cause by moving
    //     it into the .cpp and re-measuring -- the loss stayed.
    //   * 0x457ce0's residual at this position is unchanged from v448's description: DIFF(16) at
    //     951 B, insns 327/327, align=8, reg_pen=13. It is ONE zero-register rotation (the
    //     original keeps `ring`/the zero in ecx, this compile picks eax) that cascades into a
    //     movsx scheduling swap and a `lea` operand swap. Refuted probe, do NOT re-grind:
    //     reversing the source operand order of the top-edge loop bound (`ring + ptOrigin.x`
    //     instead of `ptOrigin.x + ring`) changes NOTHING -- the swap follows the register
    //     rotation, it does not drive it.
    // BECAUSE the dial is spent, adding further declarations here is now FREE with respect to
    // 0x457ce0 -- but re-measure anyway, since MailWnd's 0x42f8b0 was a second victim of the same
    // header once before and other consumers may sit on their own knife-edges.
    //
    // What the +723 B bought: InitSubsystemsAndWindows (0x406ba0) went DIFF(8) -> full 723-byte
    // EXACT purely from being spelled as the member Ghidra says it is. CreateMainWindow
    // (0x406ed0) improved DIFF(72) -> DIFF(60) but does NOT close: insns 64/64, the residual is
    // the CreateWindowExA argument-evaluation order (the original computes the
    // bScreenSaverMode ternary into eax up front and reaches it byte-wise as `and al,0xf8`;
    // this compile keeps it in ecx as `and ecx,0xfffffff8`). Scheduling class -- left parked.
    // Adding declarations here is no longer free-by-default -- run a full tools/progress.py and
    // diff the per-file TABLE, per docs/CODEGEN.md's shared-header rotation rule.
    // 0x4061e0 / 0x4062a0. The vtable at 0x4774c4 has exactly ONE slot -- the scalar deleting
    // destructor -- so a single virtual dtor is the whole of this class's polymorphism, and the
    // compiler's vptr occupies the offset 0 that `char padVtbl[4]` used to hold open (v488).
    // sizeof stays 0x28 and every field offset below is unchanged.
    AppWindow(HINSTANCE hInstance);
    virtual ~AppWindow() {
        if (g_pIniFile != NULL) {
            delete g_pIniFile;
            g_pIniFile = NULL;
        }
    }

    HWND hwndDesktopWindow;
    HWND hwndOwner;
    HINSTANCE hInstance;
    char unk0x10; // +0x10
    // UNSIGNED, pinned 2026-07-27 by the family's only int-to-double consumer:
    // AnimDescRefObj0x477488::AdvanceAnimFrameMaybe's frame-rate throttle widens minVehicleFps
    // with `xor eax,eax; mov al,[..]` (zero-extension) before the `fild`, where a plain (signed)
    // `char` would have to `movsx`. The other three have no consumer that distinguishes them and
    // are typed to match -- they are one declaration group in the original.
    unsigned char minVehicleFps;
    unsigned char minBuildingFps;
    unsigned char minMinifigFps;
    unsigned char minFlyingFps;
    char pad0x15[3]; // +0x15
    // The app's own file version, split out of the VERSIONINFO resource's "a, b, c, d"
    // FileVersion string by ReadOwnFileVersion (0x4062e0) -- PINNED v488, this used to be recorded
    // here as four opaque dwords of unknown purpose. GameNetThreadState::GameNet_DispatchMessage
    // copies all four verbatim into the 0x3e9 "who are you" handshake reply alongside the local
    // session id, so this is the build version a peer is told about.
    unsigned int dwFileVersionMajor;  // +0x18
    unsigned int dwFileVersionMinor;  // +0x1c
    unsigned int dwFileVersionBuild;  // +0x20
    unsigned int dwFileVersionRevision;  // +0x24
};
extern "C" AppWindow *g_pApp; // DAT_004aa4a0

// 0x406790 -- the command-line scanner (see the .cpp). A real __cdecl free function, so it does
// NOT move this header's declaration-parity dial (measured v488: adding it alone left every
// consumer untouched, while the `extern int g_forcedSeason` that first sat beside it here cost
// src/WorldBoardMaybe.cpp's 0x457ce0 its 951-byte exact -- that global now lives as a file-local
// extern in src/AppWindow.cpp, beside every other global this TU owns).
void AppWindow_ParseCommandLine(char *pszCmdLine);
