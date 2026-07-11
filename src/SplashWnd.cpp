// SplashWnd -- the front-end/boot window singleton. The class itself is modeled in
// src/SplashWnd.h; see src/ApplSetupWnd.h / src/NetSetupWnd.h for the two setup pages it owns.

#include <string.h>

#include "SplashWnd.h"
#include "ApplSetupWnd.h"    // the application-setup page SplashWnd owns at +0x220
#include "AppWindow.h"        // g_pApp
#include "CursorDesc.h"
#include "DPlaySessionMgr.h"
#include "DSound.h"
#include "DSoundChannel.h"    // g_pInstallPathPrefix
#include "GameNet.h"          // g_pGameNetThread
#include "GameNetMsgQueue.h"  // NetSettings / g_pNetSettings, g_nScreenState
#include "IniFile.h"
#include "LocalPlayerIdentity.h"
#include "LocoBitmap.h"
#include "NetSetupWnd.h"
#include "UIResources.h"
#include "WorldBoardMaybe.h"  // g_worldBoard

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_ClampDesktopRect
#endif

// Re-shows the whole app window stack (src/Main.cpp). Declared file-locally, as it already is in
// four other TUs -- it belongs in src/AppWindow.h, but hoisting it there is a separately-measured
// shared-header change (v340/v355/v356).
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void);   // 0x463670  // TODO: idiom

extern IDirectDrawSurface *g_pDDrawWorkSurface;           // DAT_004fd3c4
void AppWindow_SetScreenState(int newState);       // 0x408130, see src/AlbumCardWnd.cpp
// The current display mode's extent. Declared file-locally here to match the identical
// `extern unsigned int` declarations src/AlbumCardWnd.cpp, src/Main.cpp and
// src/WorldBoardMaybe.cpp already carry -- the pair belongs in one shared header, see the
// pickup's hoist list. // TODO: idiom
extern unsigned int g_dwScreenWidth;   // DAT_004851d8
extern unsigned int g_dwScreenHeight;  // DAT_00485214
// The shared text-formatting scratch buffer, declared file-locally here exactly as it already is
// in eight other TUs (see src/WidgetPicker.cpp) -- SplashWnd::Create hands whatever it currently
// holds to the player-name EDIT control as its initial window text. // TODO: idiom

unsigned char __stdcall DSound_GetOrCreateManager();  // 0x45b7e0, src/DSound.cpp

// Defined below (0x420b20); Create installs it as hwndChild's window proc.
LRESULT CALLBACK SplashChildSubclassProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

// The TU-local singleton cache the two free subclass procs use to reach the splash window (they
// are WNDPROCs, so they have no `this`). Written once by the ctor; distinct from the app-wide
// g_pSplashWnd (0x4fd378), which is what everything OUTSIDE this TU uses.
static SplashWnd *s_pSplashWnd;  // DAT_00485240

// The shared byte-returning "is the app already tearing down?" predicate (see src/Main.cpp's own
// copy for the codegen note -- the `xor eax,eax / sete al / test al,al` shape at every call site
// is what identifies it). // TODO: idiom -- this is the 4th TU-local copy; the family belongs in
// one shared header alongside g_nScreenState's own declaration.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x4202f0
// Chains WindowBase's ctor, installs SplashWnd's own vtable, publishes `this` into the TU-local
// s_pSplashWnd cache the subclass procs read, zeroes the state machine and every owned handle,
// and creates the two brushes the WM_CTLCOLORSTATIC path paints hwndChild with.
SplashWnd::SplashWnd(HINSTANCE hInstanceArg, UINT resourceIdArg)
    : WindowBase(hInstanceArg, resourceIdArg)
{
    Unk0xf0 = 0;
    s_pSplashWnd = this;
    state = 0;
    pVideoPlayer = NULL;
    bReadyForInputMaybe = 0;
    subState = 0;
    hIcon = NULL;
    bArtLoaded = 0;
    hbrSolid = CreateSolidBrush(0x5252e7);
    hbrHatch = CreateHatchBrush(HS_DIAGCROSS, 0xa5c0a);
    pNetSetupWnd = NULL;
    pApplSetupWnd = NULL;
}

// FUNCTION: LOCO 0x4203c0 // TODO: sync (Ghidra: SplashWnd::FUN_004203c0 -- real C++ dtor syntax
// needed here for the base-class chain call, same naming gap as AlbumCardWnd's and EditCardWnd's
// own dtors)
// Tears down everything the ctor and Create built: deletes the two setup pages (each through its
// own virtual dtor), deletes the two brushes, drops the art block if it is still realized, closes
// and deletes any video still playing (restoring its window proc first, exactly as
// EndActiveSession does), releases the boot music entry, then chains into WindowBase's own dtor.
//
// The compiler's own auto-generated scalar deleting destructor (`??_GSplashWnd`) sits at
// 0x4203a0 and is a free byproduct of the `virtual ~SplashWnd()` declaration in SplashWnd.h --
// it gets its own marker below, same precedent as LocoBitmap.cpp/DSound.cpp.
SplashWnd::~SplashWnd()
{
    if (pApplSetupWnd) {
        delete pApplSetupWnd;
        pApplSetupWnd = NULL;
    }
    if (pNetSetupWnd) {
        delete pNetSetupWnd;
        pNetSetupWnd = NULL;
    }
    if (hbrSolid) {
        DeleteObject(hbrSolid);
        hbrSolid = NULL;
    }
    if (hbrHatch) {
        DeleteObject(hbrHatch);
        hbrHatch = NULL;
    }
    if (bArtLoaded) {
        ReleaseArt();
    }
    if (pVideoPlayer != NULL) {
        SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC, oldVideoWndProc);
        pVideoPlayer->CloseWindow();
        delete pVideoPlayer;
        pVideoPlayer = NULL;
    }
    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    if (pEntry != NULL) {
        pEntry->Release();
    }
}

// FUNCTION: LOCO 0x4203a0 (??_GSplashWnd scalar dtor)

// FUNCTION: LOCO 0x4204d0
// Sizes the splash window to the whole desktop, loads its icon, realizes the art block, and
// chains WindowBase::Create; on failure it gives up before anything is allocated. Then builds the
// two setup pages as full-screen sibling children (the connection page first, then the
// application page), and finally the player-name field: a plain EDIT control positioned over
// windowRect, given the shared 24px UI font and a 11-character limit, subclassed to
// SplashChildSubclassProc, and handed the focus.
//
// The EDIT control is created with an EMPTY window text (the pooled "" literal at 0x4851d0 --
// corrected in v384, this was previously mis-modelled as a shared scratch global and carried a
// spurious `sic:`). The real name goes in later, from BeginModalCapture's own SetWindowTextA.
unsigned char SplashWnd::Create(HWND hwndOwner)
{
    RECT rectDesktop;
    HWND hwndDesktop = GetDesktopWindow();
    GetClientRect(hwndDesktop, &rectDesktop);
#ifdef LOCO_PORT
    Port_ClampDesktopRect(&rectDesktop); // PORT: desktop != screen here; see port/PortMode.h
#endif
    hIcon = LoadIconA(hInstance, MAKEINTRESOURCE(0x65));
    EnsureArtLoaded();
    if (!WindowBase::Create(0, hwndOwner, rectDesktop.left, rectDesktop.top,
                            rectDesktop.right - rectDesktop.left,
                            rectDesktop.bottom - rectDesktop.top, NULL, hIcon, 0,
                            WS_POPUP | WS_MAXIMIZE | WS_CLIPCHILDREN, 0)) {
        return 0;
    }

    pNetSetupWnd = new NetSetupWnd(hInstance, 0x1f6);
    pNetSetupWnd->Create(hwndSelf);
    pApplSetupWnd = new ApplSetupWnd(hInstance, 0x1f9);
    pApplSetupWnd->Create(hwndSelf);

    hwndChild = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD,
                                rectNameField.left, rectNameField.top,
                                rectNameField.right - rectNameField.left,
                                rectNameField.bottom - rectNameField.top,
                                hwndSelf, (HMENU)0x411, hInstance, NULL);
    PostMessageA(hwndChild, WM_SETFONT, (WPARAM)g_UIResources.m_hFont24, 1);
    PostMessageA(hwndChild, EM_LIMITTEXT, 0xb, 0);
    oldChildWndProc = SetWindowLongA(hwndChild, GWL_WNDPROC, (LONG)SplashChildSubclassProc);
    SetFocus(hwndChild);
    return 1;
}

// FUNCTION: LOCO 0x4206b0 // TODO: sync (Ghidra: SplashWnd::OnCreateComplete -- this is vtable
// slot 8, so it must carry WindowBase's own name for the slot, see SplashWnd.h)
// vtable slot 8 -- the post-Create init pass. Realizes the art block, takes the mouse capture,
// brings the GameNet subsystem up, refreshes the connection page's provider flags, chains
// WindowBase's own BeginModalCapture, then hides the OS cursor for good (ShowCursor counts down
// to -1), seeds the player-name edit control from the local identity and selects all of it, and
// puts the session manager into the mode the remembered "play alone" choice implies. From a cold
// state 0 it cancels any pending cursor-mode transition and keeps hwndChild hidden until the idle
// pump shows it; any pre-set state jumps straight to the wizard (state 7). Finally warms the boot
// music entry.
void SplashWnd::BeginModalCapture()
{
    subState = 0;
    bReadyForInputMaybe = 0;
    EnsureArtLoaded();
    SetCaptureMode(1);
    StartGameNetThread();
    pNetSetupWnd->RefreshProviderAvailability();
    WindowBase::BeginModalCapture();
    BringWindowToTop(hwndSelf);
    while (ShowCursor(0) >= 0) {
    }
    SetFocus(hwndChild);
    SetWindowTextA(hwndChild, g_pLocalPlayerIdentity->name);
    SendMessageA(hwndChild, EM_SETSEL, 0, -1);
    if (g_pNetSettings->bSkipSetupWizardMaybe) {
        g_pDPlaySessionMgr->SetMode(3);
    } else {
        g_pDPlaySessionMgr->SetMode(0);
    }
    if (state != 0) {
        SetState(7);
    } else {
        ScheduleModeTransition(0, 0, 0, 0, 1);
        ShowWindow(hwndChild, SW_HIDE);
    }
    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
    }
}

// FUNCTION: LOCO 0x4208f0
// Drives the boot/front-end sequencer. Each state owns which of the two setup pages is up:
// state 2 (the splash proper) puts the connection page up, 4 and 5 put the application page up,
// 3 is the hand-off between them (it re-dispatches into 4 or 5 depending on whether a secondary
// remembered choice is on file, so nothing ever rests in 3), 6 tears both down and hands the app
// its real first screen, 7 is the wizard entry, and 1 parks everything. See docs/subsystems.md's
// "SplashWnd and the boot video sequence".
//
// State 7 is where the boot music starts, but only when arriving from 0 (cold start) or 1
// (parked) -- and the DirectSound manager is created first, on the cold-start path only.
//
// EFFECTIVE MATCH (14 bytes, insns 159/159, reg_pen 0, identity_miss 0): in the state-6 arm the
// original HOISTS the `push 1` above the bSkipSetupWizardMaybe test, so the one pushed constant
// serves SetMode on the taken path and SetPriority on the skipped one (the taken path pushes a
// second 1 of its own afterwards). Ours pushes each argument at its own call. Pure scheduling --
// the instruction sequence is otherwise identical throughout. Probed and inert: spelling the
// guard `!= 0`. See docs/PARKED.md.
//
// Cases 5 and 4 are written out separately because they genuinely are two arms of the chain; cl
// 11.00 cross-jumps their identical bodies back together on its own (case 5 falls into case 4's
// ShowWindow), which is what the original does too. The state-3 arm goes the other way: its
// BeginModalCapture call is duplicated into BOTH branches in the source, because writing it once
// after the if/else makes cl compute the 4-or-5 store branchlessly (`setne`/`add ecx,4`) and the
// original clearly branches. See docs/CODEGEN.md's duplicated-tail bullets.
void SplashWnd::SetState(int stateArg)
{
    int prevState = state;
    state = stateArg;
#ifdef LOCO_PORT
    // PORT ONLY -- temporary boot diagnostic, byte-neutral for the match build.
    Port_Tracef("splash SetState %d -> %d (sub=%d)\n", prevState, stateArg, subState);
#endif
    if (stateArg == 2) {
        ShowWindow(hwndChild, SW_HIDE);
        pNetSetupWnd->BeginModalCapture();
        if (prevState == 4 || prevState == 5) {
            pApplSetupWnd->EndActiveSession();
        }
    } else if (stateArg == 5) {
        ShowWindow(hwndChild, SW_HIDE);
        pApplSetupWnd->BeginModalCapture();
    } else if (stateArg == 4) {
        ShowWindow(hwndChild, SW_HIDE);
        pApplSetupWnd->BeginModalCapture();
    } else if (stateArg == 3) {
        pNetSetupWnd->EndActiveSession();
        if (g_pNetSettings->bRememberedApplSetupValidSecondaryMaybe) {
            state = 5;
            pApplSetupWnd->BeginModalCapture();
        } else {
            state = 4;
            pApplSetupWnd->BeginModalCapture();
        }
    } else if (stateArg == 6) {
        pApplSetupWnd->EndActiveSession();
        pNetSetupWnd->EndActiveSession();
        if (g_pNetSettings->bSkipSetupWizardMaybe) {
            g_pDPlaySessionMgr->SetMode(1);
        }
        g_pGameNetThread->SetPriority(1);
        AppWindow_SetScreenState(1);
    } else if (stateArg == 7) {
        CHAR szMusicPath[0x504];

        if (pVideoPlayer != NULL) {
            SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC, oldVideoWndProc);
            pVideoPlayer->CloseWindow();
            delete pVideoPlayer;
            pVideoPlayer = NULL;
            subState = 99;
        }
        if (prevState == 0) {
            DSound_GetOrCreateManager();
        }
        if (prevState == 1 || prevState == 0) {
            wsprintfA(szMusicPath, "%svideo\\music.wav", g_pInstallPathPrefix);
            PlaySoundA(szMusicPath, (HMODULE)0, SND_ASYNC | SND_LOOP);
        }
        pApplSetupWnd->EndActiveSession();
        pNetSetupWnd->EndActiveSession();
    } else if (stateArg == 1) {
        PlaySoundA(NULL, (HMODULE)0, 0);
        ShowWindow(hwndChild, SW_HIDE);
    }
}

// FUNCTION: LOCO 0x4207c0
// The window proc SplashWnd subclasses each VideoPlayer's MCIWnd child with while a boot video
// is playing (installed/removed around every play; see SetState and DlgProc). WM_SETCURSOR
// blanks the cursor and pulls focus back to the splash window; any keypress or button-down
// posts the private "skip video" message 0x40a to it. Everything else falls through to the
// proc that was there before.
LRESULT CALLBACK SplashVideoSubclassProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int msgId = msg & 0xffff;
    switch (msgId) {
    case WM_SETCURSOR:
        SetCursor(NULL);
        SetFocus(s_pSplashWnd->hwndSelf);
        return 1;
    case WM_KEYDOWN:
    case WM_CHAR:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        PostMessageA(s_pSplashWnd->hwndSelf, 0x40a, 0, 0);
        return 0;
    }
    return CallWindowProcA((FARPROC)s_pSplashWnd->oldVideoWndProc, hwndMsg, msgId, wParam, lParam);
}

// FUNCTION: LOCO 0x420860
// vtable slot 4 -- ends the boot session. Chains WindowBase's own EndActiveSession, disarms
// input, closes and destroys any video still playing (restoring its original window proc
// first), parks the state machine at 1, drops the whole art block, and hands focus and a full
// repaint back to the main app window.
void SplashWnd::EndActiveSession()
{
    WindowBase::EndActiveSession();
    bReadyForInputMaybe = 0;
    if (pVideoPlayer != NULL) {
        SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC, oldVideoWndProc);
        pVideoPlayer->CloseWindow();
        delete pVideoPlayer;
        pVideoPlayer = NULL;
    }
    SetState(1);
    ReleaseArt();
    SetFocus(g_pApp->hwndOwner);
    InvalidateRect(g_pApp->hwndOwner, NULL, 0);
}

// FUNCTION: LOCO 0x420b20
// The window proc SplashWnd subclasses hwndChild (the player-name EDIT control) with, installed
// by Create. Forces the I-beam cursor over the field and forwards Enter/Esc to the splash
// window itself so the two labelled buttons still work while the edit control has focus.
LRESULT CALLBACK SplashChildSubclassProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UINT msgId = msg & 0xffff;
    switch (msgId) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorA(NULL, IDC_IBEAM));
        return 1;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
            PostMessageA(s_pSplashWnd->hwndSelf, WM_KEYDOWN, wParam, lParam);
            return 0;
        }
        break;
    }
    return CallWindowProcA((FARPROC)s_pSplashWnd->oldChildWndProc, hwndMsg, msgId, wParam, lParam);
}

// FUNCTION: LOCO 0x420e90
// vtable slot 0x90 (WM_ACTIVATEAPP; Ghidra: RedrawParentThunk) -- invalidate the whole splash
// window on every app activate/deactivate, then let DefWindowProcA do the rest.
LRESULT SplashWnd::OnActivateApp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    InvalidateRect(hwndSelf, NULL, 0);
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x420bb0
// vtable slot 0x54 (WM_KEYDOWN) -- the keyboard twin of OnLButtonDown's two button arms. While
// the boot sequence is still running (state 0) ANY key just posts the private "skip video"
// message 0x40a, exactly as SplashVideoSubclassProc does for keys landing on the MCI child.
// Afterwards only Enter and Esc mean anything, and only once input is armed: each draws its
// label's PRESSED art over the restored backdrop, commits, sleeps 150 ms so the press is visible,
// and then runs the same action its mouse arm does. Every other key goes to DefWindowProcA.
//
// sic: the state-0 arm reaches the window through the TU-local s_pSplashWnd cache rather than
// through `this`, even though this is a member function and the two are the same object.
//
// PARTIAL (score 495575, byte_diff 335/722, insns 237/243). Structure verified against the raw
// disasm; CALL PARITY exact. The 6-instruction deficit is entirely the SAME dead-`artRect`-local
// class SplashWnd::OnActivate and SplashWnd::OnLButtonDown are parked on: the original stores all
// four fields into a real stack slot ([esp+0x20..0x2c], frame 0x20 = two RECTs) and then rebuilds
// the by-value argument from the same registers, while this compile forwards the registers and
// drops the local. Tried and confirmed STRICTLY WORSE (525704): rewriting the three-way key
// dispatch as a flat `if (Enter) ... else if (Esc) ... else return DefWindowProcA(...)` chain --
// the nested `else { if (wParam != VK_ESCAPE) return DefWindowProcA(...); ... }` form above is the
// one that reproduces the original's block layout. See docs/PARKED.md.
LRESULT SplashWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT srcRect;
    RECT artRect;

    if (state == 0) {
        PostMessageA(s_pSplashWnd->hwndSelf, 0x40a, 0, 0);
        return 0;
    }
    if (wParam == VK_RETURN) {
        if (bReadyForInputMaybe != 0) {
            LocoBitmap *pArt = pEnterBitmapPressed;
            CursorDesc *pDesc = pEnterDescPressed;
            if (pDrawTargetMaybe != NULL) {
                CopyRect(&srcRect, &rectEnterLabel);
                OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
                pDrawTargetMaybe->RestoreOverlapBlt(rectEnterLabel, g_pDDrawWorkSurface, srcRect, 0);
            }
            artRect.left = 0;
            artRect.top = 0;
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectEnterLabel, g_pDDrawWorkSurface, artRect, 0);
            }
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
            Sleep(0x96);
            OnEnterCommitAndDispatch();
        }
    } else {
        if (wParam != VK_ESCAPE) {
            return DefWindowProcA(hwndMsg, msg, wParam, lParam);
        }
        if (bReadyForInputMaybe != 0) {
            LocoBitmap *pArt = pEscBitmapPressed;
            CursorDesc *pDesc = pEscDescPressed;
            if (pDrawTargetMaybe != NULL) {
                CopyRect(&srcRect, &rectEscLabel);
                OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
                pDrawTargetMaybe->RestoreOverlapBlt(rectEscLabel, g_pDDrawWorkSurface, srcRect, 0);
            }
            artRect.left = 0;
            artRect.top = 0;
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectEscLabel, g_pDDrawWorkSurface, artRect, 0);
            }
            CommitScreenUpdate(hwndSelf, 0, 0, 0);
            Sleep(0x96);
            this->ScheduleModeTransition(0, 0, 0, 0, 1);
            AppWindow_SetScreenState(10);
            return 0;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x420ec0
// vtable slot 0x2c -- the catch-all handler for every message SplashWnd's own per-message slots
// don't claim. Four real cases; everything else, and every non-matching sub-case, falls through
// to DefWindowProcA:
//
//   WM_CTLCOLOREDIT            -- hwndChild (the player-name EDIT control) only: dark-red text on
//                                 a transparent background, answered with the window's own solid
//                                 brush so the control paints over the splash art.
//   WM_SYSCOMMAND/SC_SCREENSAVE-- the screen saver wants in. Jump the boot sequence straight to
//                                 state 7 (only if it hasn't started yet) and re-show the app's
//                                 windows, then let DefWindowProcA run the saver.
//   0x40a                      -- the private "skip the video" message, posted by
//                                 SplashVideoSubclassProc and by OnKeyDown: go to state 7.
//   0x3b9                      -- the private "video finished" notify posted by the MCI window.
//                                 Runs the two-step boot-video sequence off subState: step 0
//                                 plays video\IgSpin.avi, step 1 the ini-configured intro
//                                 (Video/Dir), anything else ends the sequence at state 7.
//                                 Either way focus goes back to the splash window afterwards.
//
// sic: both video steps FALL THROUGH on failure instead of breaking. If IgSpin.avi's MCI child
// never materialises, step 0 drops straight into step 1's ini-configured video; if that one
// fails too it lands in the default arm and ends the sequence. Reproduced as written -- and note
// each step still bumps subState BEFORE it can fail, so the fallthrough runs step 1's body with
// subState already 1, leaving it at 2.
//
// PARTIAL MATCH (total 165222, insns 199/219 under --len 818) -- CONTENT-COMPLETE and
// structurally correct; the whole 20-instruction/65-byte gap is ONE cross-jump.
//
// The two video-start tails (SetWindowLongA / ShowWindow / SetFocus / EnableWindow / break) are
// textually identical in the source, so cl 11.00 MERGES them: case 0's test becomes `jne` into
// case 1's copy. The original keeps BOTH copies inline, with case 0's test a `je` that falls
// straight into its own copy.
//
// This is v377's duplicated-tail class, and specifically its documented HARD LIMIT -- the two
// original copies are NOT byte-identical-apart-from-displacement, they differ in register
// allocation AND scheduling:
//   copy 1 (0x4210a5): mov [esi+0x218],eax ; mov eax,[esi+0x210] ... then ecx, then edx
//   copy 2 (0x4211ad): mov edx,[esi+0x210] ; mov [esi+0x218],eax ... then eax, then ecx
// i.e. the original DUPLICATED first and ALLOCATED second, while cl 11.00 cross-jumps BEFORE
// allocation. No source shape distinguishes two textually identical blocks, so this is
// phase-ordering, not a source-shape question. Do NOT re-probe. (Probed and REFUTED anyway:
// expressing case 0's failure as an explicit `goto` into case 1 -- the only way C can write a
// CONDITIONAL fallthrough -- is much worse, total 454764, because it also reorders the blocks.)
//
// Everything else in the diff is ordinary register-allocation noise (reg_pen 28) on the
// DefWindowProcA argument marshalling and the shared reload chains.
//
// sic: both `new VideoPlayer` results are dereferenced (->hwndVideo) with no null check, so an
// allocation failure faults rather than degrading -- same as OnIdlePump's own third video.
LRESULT SplashWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char szPath[0x504];

    switch (msg) {
    case WM_CTLCOLOREDIT:
        if ((HWND)lParam == hwndChild) {
            SetTextColor((HDC)wParam, RGB(0x8c, 0, 0));
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (LRESULT)hbrSolid;
        }
        break;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_SCREENSAVE) {
            if (state == 0) {
                SetState(7);
            }
            FUN_00463670_LotsOfShowWindow();
        }
        break;

    case 0x3b9:
#ifdef LOCO_PORT
        // PORT ONLY -- temporary boot diagnostic, byte-neutral for the match build.
        Port_Tracef("splash 0x3b9 sub=%d state=%d\n", subState, state);
#endif
        switch (subState) {
        case 0:
            subState = 1;
            if (pVideoPlayer != NULL) {
                SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC, oldVideoWndProc);
                pVideoPlayer->CloseWindow();
                delete pVideoPlayer;
                pVideoPlayer = NULL;
            }
            wsprintfA(szPath, "%svideo\\IgSpin.avi", g_pInstallPathPrefix);
            pVideoPlayer = new VideoPlayer(hInstance, hwndSelf, szPath);
            if (pVideoPlayer->hwndVideo != NULL) {
                oldVideoWndProc = SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC,
                                                 (LONG)SplashVideoSubclassProc);
                ShowWindow(pVideoPlayer->hwndVideo, SW_SHOW);
                SetFocus(pVideoPlayer->hwndVideo);
                EnableWindow(pVideoPlayer->hwndVideo, 1);
                break;
            }
            // sic: falls through to the ini-configured video when IgSpin.avi won't open

        case 1:
            subState = subState + 1;
            if (pVideoPlayer != NULL) {
                SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC, oldVideoWndProc);
                pVideoPlayer->CloseWindow();
                delete pVideoPlayer;
                pVideoPlayer = NULL;
            }
            g_pIniFile->ReadString("Video", "Dir",
                                   "c:\\ProgramFiles\\LEGO MEDIA\\Constructive\\LEGO LOCO\\Video\\locointr.avi",
                                   szPath, sizeof(szPath));
            pVideoPlayer = new VideoPlayer(hInstance, hwndSelf, szPath);
            if (pVideoPlayer->hwndVideo != NULL) {
                oldVideoWndProc = SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC,
                                                 (LONG)SplashVideoSubclassProc);
                ShowWindow(pVideoPlayer->hwndVideo, SW_SHOW);
                SetFocus(pVideoPlayer->hwndVideo);
                EnableWindow(pVideoPlayer->hwndVideo, 1);
                break;
            }
            // sic: falls through to end the boot-video sequence

        default:
            if (state != 7) {
                SetState(7);
            }
            break;
        }
        SetFocus(hwndSelf);
        break;

    case 0x40a:
        SetState(7);
        break;
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x421200
// vtable slot 0x1c -- re-derives the whole front-end layout whenever the client/clip rect changes.
// Everything is authored in a fixed 1280x1024 design space (that is pDrawTargetMaybe's own size,
// see BuildDrawTargetCompositeMaybe) and then mapped onto whatever the real screen mode is:
// rectBackground becomes the design-space window of the backdrop that is actually visible
// (centered), and every other rect is authored at its design-space TOP-LEFT, sized from its own
// art descriptor's natural extent, then shifted into screen space by subtracting that origin.
// The whole pass is skipped unless the art block is realized -- the descriptors are what supply
// the sizes.
void SplashWnd::RefreshClientClipRect()
{
    WindowBase::RefreshClientClipRect();
    if (bArtLoaded) {
        rectBackground.left = (pDrawTargetMaybe->width - g_dwScreenWidth) >> 1;
        rectBackground.top = (pDrawTargetMaybe->height - g_dwScreenHeight) >> 1;
        rectBackground.right = g_dwScreenWidth + rectBackground.left;
        rectBackground.bottom = g_dwScreenHeight + rectBackground.top;

        SetRect(&rectEnterLabel, 0x387, 0x2a5, 0, 0);
        rectEnterLabel.right = pEnterDescNormal->nativeWidth + rectEnterLabel.left;
        rectEnterLabel.bottom = rectEnterLabel.top + pEnterDescNormal->nativeHeight;
        OffsetRect(&rectEnterLabel, -rectBackground.left, -rectBackground.top);

        SetRect(&rectEscLabel, 0x18b, 0x2a5, 0, 0);
        rectEscLabel.right = pEscDescNormal->nativeWidth + rectEscLabel.left;
        rectEscLabel.bottom = rectEscLabel.top + pEscDescNormal->nativeHeight;
        OffsetRect(&rectEscLabel, -rectBackground.left, -rectBackground.top);

        SetRect(&rectPlayAlone, 0x212, 0x1ea, 0, 0);
        rectPlayAlone.right = pPlayAloneDescUnchecked->nativeWidth + rectPlayAlone.left;
        rectPlayAlone.bottom = rectPlayAlone.top + pPlayAloneDescUnchecked->nativeHeight;
        OffsetRect(&rectPlayAlone, -rectBackground.left, -rectBackground.top);

        SetRect(&rectConnectOnline, 0x2c9, 0x1ea, 0, 0);
        rectConnectOnline.right = pConnectOnlineDescUnchecked->nativeWidth + rectConnectOnline.left;
        rectConnectOnline.bottom = rectConnectOnline.top + pConnectOnlineDescUnchecked->nativeHeight;
        OffsetRect(&rectConnectOnline, -rectBackground.left, -rectBackground.top);

        SetRect(&rectRememberChoiceOn, 0x387, 0x1bd, 0, 0);
        rectRememberChoiceOn.right =
            pRememberOnDescUnchecked->nativeWidth + rectRememberChoiceOn.left;
        rectRememberChoiceOn.bottom =
            rectRememberChoiceOn.top + pRememberOnDescUnchecked->nativeHeight;
        OffsetRect(&rectRememberChoiceOn, -rectBackground.left, -rectBackground.top);

        SetRect(&rectRememberChoiceOff, 0x387, 0x231, 0, 0);
        rectRememberChoiceOff.right =
            pRememberOffDescUnchecked->nativeWidth + rectRememberChoiceOff.left;
        rectRememberChoiceOff.bottom =
            rectRememberChoiceOff.top + pRememberOffDescUnchecked->nativeHeight;
        OffsetRect(&rectRememberChoiceOff, -rectBackground.left, -rectBackground.top);

        SetRect(&rectUnk0x17c, 0x12c, 0xac, 0x3d4, 0x354);
        OffsetRect(&rectUnk0x17c, -rectBackground.left, -rectBackground.top);

        SetRect(&rectNameField, 0x232, 0x2cc, 0x34d, 0x2ed);
        OffsetRect(&rectNameField, -rectBackground.left, -rectBackground.top);
#ifdef LOCO_PORT
        // PORT ONLY -- publish the finished layout to the synthetic-input driver, byte-neutral
        // for the match build. Registered HERE rather than recomputed in port/ so the
        // coordinates are the engine's own: every one of these depends on an art descriptor's
        // native extent, which is only knowable at runtime.
        Port_RegisterHitRect("enter", hwndSelf, &rectEnterLabel);
        Port_RegisterHitRect("esc", hwndSelf, &rectEscLabel);
        Port_RegisterHitRect("alone", hwndSelf, &rectPlayAlone);
        Port_RegisterHitRect("online", hwndSelf, &rectConnectOnline);
        Port_RegisterHitRect("remon", hwndSelf, &rectRememberChoiceOn);
        Port_RegisterHitRect("remoff", hwndSelf, &rectRememberChoiceOff);
        // A deliberate NON-target: the top-left corner hits none of the six, so a click here
        // can only do the one thing OnLButtonDown does before the rects are live -- state 0's
        // SetState(7), i.e. skip the boot video. Aiming at the client-rect CENTRE instead
        // would be unsafe, since (512,384) may land inside rectPlayAlone's art.
        {
            RECT rectSkip;

            SetRect(&rectSkip, 4, 4, 12, 12);
            Port_RegisterHitRect("skip", hwndSelf, &rectSkip);
        }
#endif
    }
}

// FUNCTION: LOCO 0x421500
// Realizes the whole front-end art block in one pass, latched by bArtLoaded so the two callers
// (Create and OnCreateComplete) can both call it unconditionally. Twelve consecutive TileKind
// resources 0x403..0x40f (0x40d unused) become the twelve (CursorDesc*, LocoBitmap*) pairs at
// +0x190..+0x1ec; the composited backdrop is built last. Note the load order is NOT the field
// order: the two label-button pairs come first, then the play-alone/connect-online radio art,
// then the remembered-choice radio art.
void SplashWnd::EnsureArtLoaded()
{
    if (!bArtLoaded) {
        pEnterDescNormal = g_UIResources.TileKind_GetOrLoadDescriptor(0x403);
        pEnterBitmapNormal = pEnterDescNormal->GetOrLoadFrameBitmap(0, 0);
        pEnterDescPressed = g_UIResources.TileKind_GetOrLoadDescriptor(0x404);
        pEnterBitmapPressed = pEnterDescPressed->GetOrLoadFrameBitmap(0, 0);
        pEscDescNormal = g_UIResources.TileKind_GetOrLoadDescriptor(0x405);
        pEscBitmapNormal = pEscDescNormal->GetOrLoadFrameBitmap(0, 0);
        pEscDescPressed = g_UIResources.TileKind_GetOrLoadDescriptor(0x406);
        pEscBitmapPressed = pEscDescPressed->GetOrLoadFrameBitmap(0, 0);
        pPlayAloneDescUnchecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x407);
        pPlayAloneBitmapUnchecked = pPlayAloneDescUnchecked->GetOrLoadFrameBitmap(0, 0);
        pPlayAloneDescChecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x408);
        pPlayAloneBitmapChecked = pPlayAloneDescChecked->GetOrLoadFrameBitmap(0, 0);
        pConnectOnlineDescUnchecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x409);
        pConnectOnlineBitmapUnchecked = pConnectOnlineDescUnchecked->GetOrLoadFrameBitmap(0, 0);
        pConnectOnlineDescChecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x40a);
        pConnectOnlineBitmapChecked = pConnectOnlineDescChecked->GetOrLoadFrameBitmap(0, 0);
        pRememberOnDescUnchecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x40b);
        pRememberOnBitmapUnchecked = pRememberOnDescUnchecked->GetOrLoadFrameBitmap(0, 0);
        pRememberOnDescChecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x40c);
        pRememberOnBitmapChecked = pRememberOnDescChecked->GetOrLoadFrameBitmap(0, 0);
        pRememberOffDescUnchecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x40e);
        pRememberOffBitmapUnchecked = pRememberOffDescUnchecked->GetOrLoadFrameBitmap(0, 0);
        pRememberOffDescChecked = g_UIResources.TileKind_GetOrLoadDescriptor(0x40f);
        pRememberOffBitmapChecked = pRememberOffDescChecked->GetOrLoadFrameBitmap(0, 0);
        BuildDrawTargetCompositeMaybe();
        bArtLoaded = 1;
    }
}

// FUNCTION: LOCO 0x4216f0
// Composites 5 TileKind icons (0x413/0x444/0x445/0x446/0x443) onto a freshly-allocated
// 1280x1024 LocoBitmap (pDrawTargetMaybe), each positioned at its own fixed screen offset.
// Ghidra's own decompile of this function is unfixably broken (garbled unaff_EBX/unaff_ESI
// register reads scrambling the destRect/srcRect field assignments across all 5 icon blocks) --
// transcribed directly from the raw disasm instead, same class of decompiler failure as
// LocoBitmap::ClampBlitRects (see CLAUDE.md).
//
// EXACT MATCH (994 bytes) as of v375 -- previously parked EFFECTIVE at score 148631 across
// v306-v374. TWO independent source-shape facts were needed, and neither alone was enough:
//
//  1. The SplashWnd class had to become a REAL WindowBase-derived class (v374) instead of the
//     old padded SplashWndPartial view. That alone cut the residual from 148631 to 48 --
//     register allocation snapped into place across all 5 icon blocks and the insn count went
//     332/325 -> 326/326, i.e. the "intrinsic allocator tie-break" the old autopsy claimed was
//     never intrinsic at all; it was a header-shape artifact.
//  2. The four locals must be declared ONCE at function scope, not block-scoped per icon.
//     Block scoping lets VC5 reuse (and, from the 2nd block on, SWAP) srcRect's and destRect's
//     stack slots, which is exactly what the last 48 bytes of difference were -- pure [esp+N]
//     displacement noise with align=0 / reg_pen=0. The v306 note recorded that block-scoping
//     was "confirmed NO EFFECT"; that was true only while fact 1 was still masking it.
//
// Structure was already fully verified against the raw disasm for all 5 icon blocks (Ghidra's
// own decompile of this function is unfixably broken -- garbled unaff_EBX/unaff_ESI register
// reads scramble the destRect/srcRect assignments -- so it was transcribed from raw disasm,
// same class of decompiler failure as LocoBitmap::ClampBlitRects, see CLAUDE.md).
void SplashWnd::BuildDrawTargetCompositeMaybe()
{
    CursorDesc *pDesc;
    LocoBitmap *pBmp;
    RECT srcRect;
    RECT destRect;

    pDrawTargetMaybe = new LocoBitmap();
    pDrawTargetMaybe->CreateAndFill(0x500, 0x400, 1, 0, 0);

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x413);
    pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&srcRect, 0, 0, pBmp->width, pBmp->height);
    CopyRect(&destRect, &srcRect);
    OffsetRect(&destRect, 0, 0);
    pBmp->BlitOntoBitmap(destRect, pDrawTargetMaybe, srcRect, 0);
    pDesc->ReleaseRef();

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x444);
    pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&srcRect, 0, 0, pBmp->width, pBmp->height);
    CopyRect(&destRect, &srcRect);
    OffsetRect(&destRect, 0xf4, 0x1d6);
    pBmp->BlitOntoBitmap(destRect, pDrawTargetMaybe, srcRect, 0);
    pDesc->ReleaseRef();

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x445);
    pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&srcRect, 0, 0, pBmp->width, pBmp->height);
    CopyRect(&destRect, &srcRect);
    OffsetRect(&destRect, 0x204, 0xf9);
    pBmp->BlitOntoBitmap(destRect, pDrawTargetMaybe, srcRect, 0);
    pDesc->ReleaseRef();

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x446);
    pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&srcRect, 0, 0, pBmp->width, pBmp->height);
    CopyRect(&destRect, &srcRect);
    OffsetRect(&destRect, 0x11a, 0xf0);
    pBmp->BlitOntoBitmap(destRect, pDrawTargetMaybe, srcRect, 0);
    pDesc->ReleaseRef();

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x443);
    pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&srcRect, 0, 0, pBmp->width, pBmp->height);
    CopyRect(&destRect, &srcRect);
    OffsetRect(&destRect, 0x20b, 0x2a8);
    pBmp->BlitOntoBitmap(destRect, pDrawTargetMaybe, srcRect, 0);
    pDesc->ReleaseRef();
}

// FUNCTION: LOCO 0x421ae0
// The inverse of EnsureArtLoaded (0x421500): drops a reference on all 12 realized descriptors
// in the same order they were acquired, frees the composited backdrop, and clears the latch so
// the next EnsureArtLoaded rebuilds everything. The realized LocoBitmap halves are NOT released
// here -- they belong to their descriptor's own cache.
void SplashWnd::ReleaseArt()
{
    if (bArtLoaded) {
        pEnterDescNormal->ReleaseRef();
        pEnterDescNormal = NULL;
        pEnterDescPressed->ReleaseRef();
        pEnterDescPressed = NULL;
        pEscDescNormal->ReleaseRef();
        pEscDescNormal = NULL;
        pEscDescPressed->ReleaseRef();
        pEscDescPressed = NULL;
        pPlayAloneDescUnchecked->ReleaseRef();
        pPlayAloneDescUnchecked = NULL;
        pPlayAloneDescChecked->ReleaseRef();
        pPlayAloneDescChecked = NULL;
        pConnectOnlineDescUnchecked->ReleaseRef();
        pConnectOnlineDescUnchecked = NULL;
        pConnectOnlineDescChecked->ReleaseRef();
        pConnectOnlineDescChecked = NULL;
        pRememberOnDescUnchecked->ReleaseRef();
        pRememberOnDescUnchecked = NULL;
        pRememberOnDescChecked->ReleaseRef();
        pRememberOnDescChecked = NULL;
        pRememberOffDescUnchecked->ReleaseRef();
        pRememberOffDescUnchecked = NULL;
        pRememberOffDescChecked->ReleaseRef();
        pRememberOffDescChecked = NULL;
        delete pDrawTargetMaybe;
        pDrawTargetMaybe = NULL;
        bArtLoaded = 0;
    }
}

// FUNCTION: LOCO 0x421be0
// vtable slot 0x20 -- the "this screen just became active, draw yourself" hook, the same slot
// AlbumCardWnd::OnActivate (0x404db0) overrides; the single stack argument is ignored here too.
// NOT WM_PAINT: SplashWnd leaves slot 0x6c at WindowBase's own default. (Ghidra called this
// `SplashWnd::OnPaint` until this session, which collided with the real slot-0x6c handler's name;
// corrected DB-side.)
//
// Only state 7 -- the interactive front end, once the boot videos are done -- has anything to
// draw. It arms the point cursor, stamps the visible window of the composited backdrop over the
// whole screen, repaints the four setting rects from the current NetSettings flags, draws the
// Enter and Esc labels in their NORMAL (unpressed) art, commits, and finally arms input. Every
// other state just commits whatever is already on the work surface.
//
// PARTIAL (score 489479, byte_diff 369/711, insns 226/244). Structure is fully verified against
// the raw disasm and CALL PARITY is exact; the whole 18-instruction deficit is two instances of
// the same MSVC PHASE-ORDERING difference, and neither is source-steerable:
//
// 1. The original emits the `CommitScreenUpdate; return` tail TWICE -- once for the state==0 arm
//    and once for the "neither 0 nor 7" arm -- and the two copies are NOT byte-identical, so they
//    cannot merge: the default copy holds hwndSelf in eax and needs no `mov ecx,esi` (ecx still
//    holds `this` from entry), while the state==0 copy holds it in ecx and therefore must reload
//    `this`. That is register allocation running AFTER the duplication. Our cl 11.00 cross-jumps
//    the two tails BEFORE allocation, so it emits one shared copy reached by a single `jne`.
//    The v376 duplicated-tail lever that took OnEnterCommitAndDispatch exact does NOT apply --
//    the source here already spells both tails out as separate statements. Tried and confirmed
//    NO EFFECT (score 489479 unchanged, to the point): rewriting the three-way dispatch as
//    `switch (state) { case 0: ...; case 7: ... }`.
// 2. The original keeps the four `artRect` stores in BOTH label blocks even though the slot is
//    never read back -- the by-value argument copy is rebuilt from the same registers a few
//    instructions later. That is 8 dead stores and a second 16-byte stack slot (the original's
//    frame is 0x24, ours 0x14). Same dead-local class as SplashWnd::OnLButtonDown's own park, and
//    the same probe fails here too: tried and confirmed NO EFFECT, hoisting srcRect/artRect/
//    screenRect to function scope (the v375 lever that closed BuildDrawTargetCompositeMaybe).
//    There is no `lea` for the slot anywhere, so this is not the address-taken-local class either.
// See docs/PARKED.md.
void SplashWnd::OnActivate(int reserved)
{
    RECT srcRect;
    RECT artRect;
    RECT screenRect;

#ifdef LOCO_PORT
    // PORT ONLY -- temporary boot diagnostic, byte-neutral for the match build.
    Port_Tracef("splash OnActivate state=%d art=%d target=%08lx bg=%ld,%ld,%ld,%ld\n",
                state, (int)bArtLoaded, (unsigned long)pDrawTargetMaybe,
                rectBackground.left, rectBackground.top, rectBackground.right,
                rectBackground.bottom);
#endif
    if (state == 0) {
        CommitScreenUpdate(hwndSelf, 0, 0, 0);
        return;
    }
    if (state == 7) {

        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 1, 0);

        screenRect.left = 0;
        screenRect.top = 0;
        screenRect.right = g_dwScreenWidth;
        screenRect.bottom = g_dwScreenHeight;
        pDrawTargetMaybe->RestoreOverlapBlt(screenRect, g_pDDrawWorkSurface, rectBackground, 1);
        RedrawSettingRectsMaybe();

        LocoBitmap *pArt = pEnterBitmapNormal;
        CursorDesc *pDesc = pEnterDescNormal;
        if (pDrawTargetMaybe != NULL) {
            CopyRect(&srcRect, &rectEnterLabel);
            OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
            pDrawTargetMaybe->RestoreOverlapBlt(rectEnterLabel, g_pDDrawWorkSurface, srcRect, 0);
        }
        artRect.left = 0;
        artRect.top = 0;
        artRect.right = pDesc->nativeWidth;
        artRect.bottom = pDesc->nativeHeight;
        if (pArt != NULL) {
            pArt->RestoreOverlapBlt(rectEnterLabel, g_pDDrawWorkSurface, artRect, 0);
        }

        pDesc = pEscDescNormal;
        pArt = pEscBitmapNormal;
        if (pDrawTargetMaybe != NULL) {
            CopyRect(&srcRect, &rectEscLabel);
            OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
            pDrawTargetMaybe->RestoreOverlapBlt(rectEscLabel, g_pDDrawWorkSurface, srcRect, 0);
        }
        artRect.left = 0;
        artRect.top = 0;
        artRect.right = pDesc->nativeWidth;
        artRect.bottom = pDesc->nativeHeight;
        if (pArt != NULL) {
            pArt->RestoreOverlapBlt(rectEscLabel, g_pDDrawWorkSurface, artRect, 0);
        }

        CommitScreenUpdate(hwndSelf, 0, 0, 0);
        bReadyForInputMaybe = 1;
        return;
    }
    CommitScreenUpdate(hwndSelf, 0, 0, 0);
}

// FUNCTION: LOCO 0x421eb0
// vtable slot 0x24 -- the splash window's idle pump. Two independent jobs, both idempotent so the
// pump can fire freely: (1) once the state machine reaches 7 (the interactive front end), make
// sure the player-name EDIT child is actually shown and focused; (2) while the machine is still
// at 0 and no video object exists yet, kick off the very FIRST boot clip, legoSpin.avi. The
// g_pApp->unk0x10 gate is the "skip the intro videos" switch.
//
// EFFECTIVE MATCH (score 887, byte_diff 7/349, insns 101/101, align=0, reg_pen=8). Every
// instruction lines up one-for-one and the compiled length is exact; the entire residual is a
// register rotation across the last three Win32 calls of the hwndVideo block. The original lets
// the `pVideoPlayer` load die into its own register (`mov eax,[esi+0x210]` then `mov eax,[eax+4]`),
// while this compile keeps the base pointer alive and picks a fresh register for the field
// (`mov ecx,[eax+4]`), which then rotates eax/ecx/edx through the SetFocus and EnableWindow
// reloads. Same intrinsic allocator coin-flip class as the project's other documented tie-breaks
// -- there is no source expression left to vary here (the member really is re-read from
// this->pVideoPlayer at each of the three call sites, which is what rules out a cached local).
// See docs/PARKED.md.
void SplashWnd::OnIdlePump()
{
    if (!IsWindowVisible(hwndChild) && state == 7) {
        ShowWindow(hwndChild, SW_SHOW);
        SetFocus(hwndChild);
    }
    if (pVideoPlayer == NULL && g_pApp->unk0x10 == 0 && state == 0) {
        char szPath[0x504];
        wsprintfA(szPath, "%svideo\\legoSpin.avi", g_pInstallPathPrefix);
        pVideoPlayer = new VideoPlayer(hInstance, hwndSelf, szPath);
        if (pVideoPlayer->hwndVideo != NULL) {
            oldVideoWndProc = SetWindowLongA(pVideoPlayer->hwndVideo, GWL_WNDPROC,
                                             (LONG)SplashVideoSubclassProc);
            ShowWindow(pVideoPlayer->hwndVideo, SW_SHOW);
            SetFocus(pVideoPlayer->hwndVideo);
            EnableWindow(pVideoPlayer->hwndVideo, 1);
        }
        SetFocus(hwndSelf);
        this->ScheduleModeTransition(0, 0, 0, 0, 1);
    }
}

// FUNCTION: LOCO 0x422010
// Repaints the four setting rects from the current NetSettings state -- called by OnLButtonDown
// after any of them flips a flag. Each rect gets its backdrop restored and then one of its own
// unchecked/checked art pair stamped over it, picked by the flag the rect displays:
//   rectPlayAlone / rectConnectOnline  <- bSkipSetupWizardMaybe (they are each other's inverse;
//                                         connect-online is only drawn while a provider list
//                                         exists, i.e. Unk0x10Maybe != 0)
//   rectRememberChoiceOn / ...Off      <- bUseSecondaryRememberedChoice, but only while the skip
//                                         flag is CLEAR; once it is set the pair is dead and
//                                         both rects are just erased back to the backdrop.
// sic: the last arm erases rectRememberChoiceOn with an inlined copy of
// RestoreBackdropUnderRect's body instead of calling it (confirmed by the raw disasm's call
// tally: 8 calls to 0x422570 and one hand-written CopyRect/OffsetRect/RestoreOverlapBlt trio) --
// reproduced as written.
//
// PARTIAL (score 204010, byte_diff 140/1068, insns 345/347). CALL PARITY EXACT: 8x 0x422570,
// 7x LocoBitmap::RestoreOverlapBlt, 1x 0x422440, 1x CopyRect, 1x OffsetRect -- matching the
// original's own tally instruction-for-instruction, which is what pinned the "one arm inlines
// the helper" oddity above and the fact that the connect-online stamp is written out in BOTH
// top-level arms and cross-jumped by the compiler (only one RestoreOverlapBlt survives).
// Block layout is fully aligned after correcting TWO branch polarities: the original writes
// `if (bSkipSetupWizardMaybe != 0) { checked } else { unchecked }` for the play-alone/
// connect-online pair, and `if (bUseSecondaryRememberedChoice != 0) { checked } else
// { unchecked }` for the remembered-choice pair -- i.e. the SET arm is the fallthrough in both
// (the SECOND skip test, guarding the remembered-choice pair, is the opposite way round:
// `== 0` there, with the set arm jumped away to). Writing them the natural `== 0`-first way
// costs 244675 points of block-layout churn on its own.
//
// The whole residual is one systematic zero-register tie-break, identical in 5 of the 6 art-
// stamp blocks: both compiles materialize a zero in EDX for the `artRect.left = artRect.top =
// 0` stores, but the original still tests the bitmap pointer with `test ebp,ebp` while this
// compile CSEs the zero into the compare and emits `cmp ebp,edx`. The 6th block -- the one
// where VC5 happens to have the zero in ESI rather than EDX -- uses `test ebp,ebp` in BOTH,
// which is what proves this is a register-residency artifact and not a source-shape difference.
// Same intrinsic zero-reg class as the v334/v335 residuals. Tried and confirmed NO EFFECT:
// `if (pArt)` instead of `if (pArt != NULL)` (204010 exactly, to the point). Tried and
// confirmed STRICTLY WORSE: RECT store order left/top/right/bottom instead of
// right/bottom/left/top (262600) -- the emitted store order really is source order here, even
// though the scheduler then interleaves it with the compare. See docs/PARKED.md.
void SplashWnd::RedrawSettingRectsMaybe()
{
    CursorDesc *pDesc;
    LocoBitmap *pArt;
    RECT artRect;

    if (g_pNetSettings->bSkipSetupWizardMaybe != 0) {
        pArt = pPlayAloneBitmapChecked;
        pDesc = pPlayAloneDescChecked;
        RestoreBackdropUnderRect(&rectPlayAlone);
        artRect.right = pDesc->nativeWidth;
        artRect.bottom = pDesc->nativeHeight;
        artRect.left = 0;
        artRect.top = 0;
        if (pArt != NULL) {
            pArt->RestoreOverlapBlt(rectPlayAlone, g_pDDrawWorkSurface, artRect, 0);
        }
        if (g_pNetSettings->pDetectedProviderList != 0) {
            pArt = pConnectOnlineBitmapUnchecked;
            pDesc = pConnectOnlineDescUnchecked;
            RestoreBackdropUnderRect(&rectConnectOnline);
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            artRect.left = 0;
            artRect.top = 0;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectConnectOnline, g_pDDrawWorkSurface, artRect, 0);
            }
        }
    } else {
        pArt = pPlayAloneBitmapUnchecked;
        pDesc = pPlayAloneDescUnchecked;
        RestoreBackdropUnderRect(&rectPlayAlone);
        artRect.right = pDesc->nativeWidth;
        artRect.bottom = pDesc->nativeHeight;
        artRect.left = 0;
        artRect.top = 0;
        if (pArt != NULL) {
            pArt->RestoreOverlapBlt(rectPlayAlone, g_pDDrawWorkSurface, artRect, 0);
        }
        if (g_pNetSettings->pDetectedProviderList != 0) {
            pArt = pConnectOnlineBitmapChecked;
            pDesc = pConnectOnlineDescChecked;
            RestoreBackdropUnderRect(&rectConnectOnline);
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            artRect.left = 0;
            artRect.top = 0;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectConnectOnline, g_pDDrawWorkSurface, artRect, 0);
            }
        }
    }

    if (g_pNetSettings->bSkipSetupWizardMaybe == 0) {
        if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
            pArt = pRememberOnBitmapChecked;
            pDesc = pRememberOnDescChecked;
            RestoreBackdropUnderRect(&rectRememberChoiceOn);
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            artRect.left = 0;
            artRect.top = 0;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectRememberChoiceOn, g_pDDrawWorkSurface, artRect, 0);
            }
            pArt = pRememberOffBitmapUnchecked;
            pDesc = pRememberOffDescUnchecked;
            RestoreBackdropUnderRect(&rectRememberChoiceOff);
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            artRect.left = 0;
            artRect.top = 0;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectRememberChoiceOff, g_pDDrawWorkSurface, artRect, 0);
            }
        } else {
            pArt = pRememberOnBitmapUnchecked;
            pDesc = pRememberOnDescUnchecked;
            RestoreBackdropUnderRect(&rectRememberChoiceOn);
            artRect.right = pDesc->nativeWidth;
            artRect.bottom = pDesc->nativeHeight;
            artRect.left = 0;
            artRect.top = 0;
            if (pArt != NULL) {
                pArt->RestoreOverlapBlt(rectRememberChoiceOn, g_pDDrawWorkSurface, artRect, 0);
            }
            DrawArtOverBackdrop(&rectRememberChoiceOff, 0, pRememberOffDescChecked,
                                pRememberOffBitmapChecked);
        }
    } else {
        if (pDrawTargetMaybe != NULL) {
            CopyRect(&artRect, &rectRememberChoiceOn);
            OffsetRect(&artRect, rectBackground.left, rectBackground.top);
            pDrawTargetMaybe->RestoreOverlapBlt(rectRememberChoiceOn, g_pDDrawWorkSurface,
                                                artRect, 0);
        }
        RestoreBackdropUnderRect(&rectRememberChoiceOff);
    }
}

// FUNCTION: LOCO 0x422440
// Repaint one setting rect: erase it by restoring the composited backdrop underneath, then
// stamp nFrameIndex's frame of pArt over it at pDesc's own natural extent. RedrawSettingRectsMaybe
// is the only caller and always passes frame 0, but the strip-offset arithmetic is real.
// sic: pDesc is dereferenced for its extent BEFORE the `pDesc != NULL` guard below -- a genuine
// (harmless in practice) ordering bug in the original, faithfully reproduced.
//
// EFFECTIVE MATCH (score 26453, byte_diff 33/304, insns 99/99, align=24). Every instruction
// lines up one-for-one; the whole residual is one register coin-flip and its knock-on effects:
// the original parks pDesc in EDI and nFrameIndex in ECX, this compile picks the opposite pair,
// and downstream the original reloads artRect.right from its stack slot for the by-value
// argument copy while this one keeps it in EAX. Load order, test order and store order all
// already agree. Tried and confirmed NO EFFECT: hoisting srcRect/artRect to function scope
// (the lever that closed 0x4216f0 this same session) -- score unchanged at 26453 exactly, so
// this is not the stack-slot-reuse class. Same intrinsic register coin-flip class as the
// project's other documented allocator tie-breaks. See docs/PARKED.md.
void SplashWnd::DrawArtOverBackdrop(RECT *pRect, int nFrameIndex, CursorDesc *pDesc, LocoBitmap *pArt)
{
    if (pDrawTargetMaybe != NULL) {
        RECT srcRect;
        CopyRect(&srcRect, pRect);
        OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
        pDrawTargetMaybe->RestoreOverlapBlt(*pRect, g_pDDrawWorkSurface, srcRect, 0);
    }
    RECT artRect;
    artRect.left = 0;
    artRect.top = 0;
    artRect.right = pDesc->nativeWidth;
    artRect.bottom = pDesc->nativeHeight;
    if (nFrameIndex != 0 && pDesc != NULL) {
        OffsetRect(&artRect, nFrameIndex * artRect.right, 0);
    }
    if (pArt != NULL) {
        pArt->RestoreOverlapBlt(*pRect, g_pDDrawWorkSurface, artRect, 0);
    }
}

// FUNCTION: LOCO 0x422570
// DrawArtOverBackdrop's erase half on its own: restore the composited backdrop under pRect.
// The source rect is pRect shifted into pDrawTargetMaybe's own coordinate space by the
// backdrop's origin.
void SplashWnd::RestoreBackdropUnderRect(RECT *pRect)
{
    if (pDrawTargetMaybe != NULL) {
        RECT srcRect;
        CopyRect(&srcRect, pRect);
        OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
        pDrawTargetMaybe->RestoreOverlapBlt(*pRect, g_pDDrawWorkSurface, srcRect, 0);
    }
}

// FUNCTION: LOCO 0x422610
// vtable slot 0x80 (WM_CLOSE) -- unless the app is already tearing down (screen state 10),
// end the boot session and dispatch the quit transition first, then fall through to
// WindowBase's own default WM_CLOSE handling.
LRESULT SplashWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        EndActiveSession();
        AppWindow_SetScreenState(10);
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x422660
// The Enter button's real body, called from OnLButtonDown (and from OnKeyDown) once the pressed
// glyph has been drawn. Two halves:
//
// 1. COMMIT the name typed into hwndChild (the player-name EDIT control). It is only accepted if
//    it is non-empty, contains NO punctuation/symbol character, and contains at least one letter;
//    a rejected name simply leaves the stored identity alone. An accepted one is written through
//    to all three places the identity lives: the singleton, its .usr profile file, and
//    lego.ini's [USER]Name. Either way the control is then re-synced from the singleton, so a
//    rejected edit visibly snaps back.
//
// 2. DISPATCH the next boot state, having first flushed NetSettings to disk:
//    - skip-the-wizard set  -> tear the tile grid down, drop the session to mode 1, wake the
//      network worker thread up to normal priority and jump straight into the game (app state 1).
//    - otherwise            -> build the tile grid, then try to honour a REMEMBERED protocol
//      choice: if the remembered-choice-valid flag is set and the remembered protocol (4 or 2) is
//      still available on this machine, skip the NetSetup page entirely and go straight to
//      ApplSetup (state 5 for the secondary choice, 4 for the primary). Any other case shows the
//      NetSetup wizard normally (state 2).
//
// sic: the skip-the-wizard arm re-tests g_pNetSettings->bSkipSetupWizardMaybe immediately after
// having branched on it. The test is dead by construction (the flag cannot have changed) and the
// compiler does not fold it away, because it re-loads the global through a pointer it must assume
// SaveToDisk could have aliased. Reproduced as written -- and note the SHAPE is load-bearing, not
// just the test: the arm is an early `return` with its whole three-call tail written out TWICE in
// the source, which VC5 then cross-jumps. Writing the same logic as the natural
// `if (bSkip) { SetMode(1); }` + one shared tail compiles to 142/142 instructions with identical
// registers but costs 14 bytes -- the duplicated-tail form is what lets VC5 hoist the shared
// `push 1` above the compare and reuse the pushed slot for SetMode, which the single-tail form
// cannot express.
void SplashWnd::OnEnterCommitAndDispatch()
{
    char szName[13];

    this->ScheduleModeTransition(0, 0, 0, 0, 1);
    GetWindowTextA(hwndChild, szName, sizeof(szName));
    if (strlen(szName) != 0 &&
        strpbrk(szName, "/.,\\?><|#';~@:][}{=-`+_)(*&^%$\xa3\"!\xac") == NULL &&
        strpbrk(szName, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") != NULL) {
        g_pLocalPlayerIdentity->SetNameMaybe(szName);
        Profile_SavePlayerUserFile(g_pLocalPlayerIdentity);
        g_pIniFile->WriteString("USER", "Name", szName);
    }
    SetWindowTextA(hwndChild, g_pLocalPlayerIdentity->name);
    g_pNetSettings->SaveToDisk();

    if (g_pNetSettings->bSkipSetupWizardMaybe == 0) {
        g_worldBoard.Ddraw_InitTileGridExtent(1);
        if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
            if (g_pNetSettings->bRememberedApplSetupValidSecondaryMaybe != 0 &&
                ((g_pNetSettings->rememberedProtocolSecondary == 4 &&
                  pNetSetupWnd->bProviderId4AvailableMaybe != 0) ||
                 (g_pNetSettings->rememberedProtocolSecondary == 2 &&
                  pNetSetupWnd->bProviderId2AvailableMaybe != 0))) {
                SetState(5);
                return;
            }
        } else {
            if (g_pNetSettings->bRememberedApplSetupValidPrimaryMaybe != 0 &&
                ((g_pNetSettings->rememberedProtocolPrimary == 4 &&
                  pNetSetupWnd->bProviderId4AvailableMaybe != 0) ||
                 (g_pNetSettings->rememberedProtocolPrimary == 2 &&
                  pNetSetupWnd->bProviderId2AvailableMaybe != 0))) {
                SetState(4);
                return;
            }
        }
        SetState(2);
        return;
    }

    g_worldBoard.Ddraw_InitTileGridExtent(0);
    if (g_pNetSettings->bSkipSetupWizardMaybe == 0) {  // sic: dead re-test, see above
        g_pGameNetThread->SetPriority(1);
        AppWindow_SetScreenState(1);
        return;
    }
    g_pDPlaySessionMgr->SetMode(1);
    g_pGameNetThread->SetPriority(1);
    AppWindow_SetScreenState(1);
}

// FUNCTION: LOCO 0x422820
// Bring the GameNet background subsystem up. First pick the worker thread's inter-tick sleep from
// whether the multiplayer setup wizard is being skipped -- 50 ms for the play-alone/attract-mode
// path, 30 ms when a real session is coming -- then, guarded on g_pGameNetThreadState so it only
// ever happens once, create the thread-state singleton and the worker thread and start it.
//
// Two callers: SplashWnd::OnCreateComplete (0x4206b0) on the normal boot path, and
// ScreenSaver::EnterDemoSession (0x448350) for attract mode, which sets bSkipSetupWizardMaybe
// immediately before calling in -- which is exactly the flag the sleep choice above reads.
//
// The thread-failed-to-start path dispatches vtable slot 0x14 with a literal 0. That resolves to
// WindowBase::NoOpVirtualMaybe, a shared no-op body that nothing in either window hierarchy
// overrides (and whose single stack argument it ignores), so a worker thread that fails to start
// is silently swallowed and the front end carries on as if multiplayer were live.
void SplashWnd::StartGameNetThread()
{
    if (g_pNetSettings->bSkipSetupWizardMaybe != 0) {
        g_pNetSettings->nTickSleepMs = 50;
    } else {
        g_pNetSettings->nTickSleepMs = 30;
    }

    if (g_pGameNetThreadState == 0) {
        g_pNetMsgSendQueueHead = 0;
        g_pGameNetThreadState = new GameNetThreadState(this->hInstance, this->hwndOwner);
        g_pGameNetThread = new ThreadWrapper();
        if (g_pGameNetThread->Start((void (__cdecl *)(void *))GameNetThread_TickLoop,
                                    g_pGameNetThreadState) != 1) {
            this->NoOpVirtualMaybe(0);
            return;
        }
        g_pGameNetThread->SetPriority(1);
    }
}

// vtable slot 0x40 -- WM_RBUTTONDOWN is simply WM_LBUTTONDOWN here, forwarded THROUGH the
// vtable exactly as an unqualified call to a virtual member compiles; do not "optimize" it to a
// class-qualified call. UNMARKED: this body ICF-folds onto 0x4323c0, whose marker lives on
// MapWnd::OnRButtonDown (src/MapWnd.cpp). The WindowBase hierarchy's copies fold to a
// DIFFERENT address than the other family's purely because the slot displacement differs.
LRESULT SplashWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x422930
// Ghidra called this `SplashWnd::OnLButtonUp` until this session; that was WRONG and is now
// corrected DB-side. The function pointer lives at 0x477a30 = slot 0x38 off SplashWnd's vtable
// base 0x4779f8, which is the WindowBase-wide WM_LBUTTONDOWN convention slot (see
// src/SplashWnd.h's slot map; WM_LBUTTONUP is slot 0x3c, which SplashWnd leaves at the
// DefWindowProcStub default 0x422ea0).
//
// The front-end's click dispatcher. Before the boot sequence has started (state == 0) a click
// just kicks the state machine to 7 and is swallowed; afterwards clicks are ignored entirely
// until bReadyForInputMaybe is armed.
//
// Six hit regions, tested in order:
//   rectEnterLabel      -- draw the pressed art, click sound, 150 ms, then OnEnterCommitAndDispatch
//   rectEscLabel        -- same feedback, but waits for the click sound to actually finish
//                          before tearing the cursor mode down and dispatching UI mode 10
//   rectPlayAlone       -- g_pNetSettings->bSkipSetupWizardMaybe = 1, session mode 3
//   rectConnectOnline   -- the inverse (flag 0, session mode 0); live only while a provider list
//                          exists (Unk0x10Maybe != 0). This is the "opt back into multiplayer
//                          setup" write site.
//   rectRememberChoiceOn / ...Off -- the two halves of a radio pair over
//                          bUseSecondaryRememberedChoice; both dead while the skip flag is set.
// Each of the last four repaints via RedrawSettingRectsMaybe and shares one commit tail. Every
// arm is a no-op (early return) when the flag it would write already holds the target value.
//
// The two button arms restore the backdrop under the label first (a CopyRect/OffsetRect of the
// label rect into pDrawTargetMaybe's own coordinate space, guarded because the composite is
// built lazily), then stamp the pressed art at its own natural extent (the CursorDesc's
// nativeWidth/nativeHeight).
//
// PARTIAL (score 162759, byte_diff 139/1090, insns 381/378). Structure verified against the raw
// disasm; CALL PARITY is 38 vs the original's 36, and the two extras are both accounted for
// below. Two residual classes, neither source-steerable within budget:
//
// 1. The `artRect` local's four stores are DEAD in the original (written at [esp+0x20..0x2c] in
//    both button arms, never read -- the by-value argument copy is built separately from the
//    same registers) and VC5 keeps them; this compile forwards the registers straight into the
//    argument and drops the local entirely, costing 4 instructions per arm. Confirmed the slot
//    really is dead: the only reads of `[esp+0x2c]` in the original sit at a push depth of 0x14
//    deeper, so they address the CopyRect local at [esp+0x10], not this one (the absolute-K
//    aliasing trap CLAUDE.md warns about). Tried and confirmed NO EFFECT: hoisting `RECT
//    artRect` to function scope so one local serves both arms (score unchanged, 162759 exactly).
//    No `lea` for the slot anywhere, so this is not the address-taken-local class of v371/v373.
//
// 2. The play-alone arm's tail is duplicated in the source (call parity proves it: 0x422010
//    appears TWICE, so RedrawSettingRectsMaybe genuinely has two source call sites) and MSVC
//    cross-jumps the common suffix. The original merges from `mov ecx,esi; call
//    CommitScreenUpdate` onward, leaving only the arm's own hwndSelf load + 4 pushes duplicated;
//    this compile merges later, from PlayUiSound onward, leaving an extra CommitScreenUpdate +
//    RequestModeTransitionFromSource pair (the +2 calls). Cause is a register coin-flip one
//    level up: the original loads pPointCursorDesc/pPointCursorRect/vtable into eax/ecx/edx in
//    BOTH copies so the blocks are byte-identical and merge, while this compile picks
//    eax/ecx/edx in one copy and ecx/edx/eax in the other, blocking the merge. Same intrinsic
//    class as the project's other documented allocator tie-breaks. See docs/PARKED.md.
LRESULT SplashWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;

    if (state == 0) {
        SetState(7);
        return 0;
    }
    if (bReadyForInputMaybe == 0) {
        return 0;
    }

    if (PtInRect(&rectEnterLabel, pt)) {
        LocoBitmap *pArt = pEnterBitmapPressed;
        CursorDesc *pDesc = pEnterDescPressed;
        if (pDrawTargetMaybe != 0) {
            RECT srcRect;
            CopyRect(&srcRect, &rectEnterLabel);
            OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
            pDrawTargetMaybe->RestoreOverlapBlt(rectEnterLabel, g_pDDrawWorkSurface, srcRect, 0);
        }
        RECT artRect;
        artRect.left = 0;
        artRect.top = 0;
        artRect.right = pDesc->nativeWidth;
        artRect.bottom = pDesc->nativeHeight;
        if (pArt != 0) {
            pArt->RestoreOverlapBlt(rectEnterLabel, g_pDDrawWorkSurface, artRect, 0);
        }
        g_UIResources.PlayUiSound(0x5015);
        CommitScreenUpdate(hwndSelf, 0, 0, 0);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        Sleep(0x96);
        OnEnterCommitAndDispatch();
        return 0;
    }

    if (PtInRect(&rectEscLabel, pt)) {
        LocoBitmap *pArt = pEscBitmapPressed;
        CursorDesc *pDesc = pEscDescPressed;
        if (pDrawTargetMaybe != 0) {
            RECT srcRect;
            CopyRect(&srcRect, &rectEscLabel);
            OffsetRect(&srcRect, rectBackground.left, rectBackground.top);
            pDrawTargetMaybe->RestoreOverlapBlt(rectEscLabel, g_pDDrawWorkSurface, srcRect, 0);
        }
        RECT artRect;
        artRect.left = 0;
        artRect.top = 0;
        artRect.right = pDesc->nativeWidth;
        artRect.bottom = pDesc->nativeHeight;
        if (pArt != 0) {
            pArt->RestoreOverlapBlt(rectEscLabel, g_pDDrawWorkSurface, artRect, 0);
        }
        // Purge anything already queued on the MM waveform device, then play the click through
        // our own mixer and BLOCK until that channel reports itself reclaimable -- the Esc path
        // tears the whole front end down straight afterwards, so the sound has to have finished
        // before the teardown, unlike the Enter path's fire-and-forget PlayUiSound above.
        PlaySoundA(0, 0, SND_PURGE);
        DSoundChannel *pChannel = 0;
        if (g_pDSoundManager != 0) {
            g_pDSoundManager->PlaySoundByIdWithHandle(0x5015, &pChannel);
            while (pChannel != 0 && pChannel->IsReclaimable() == 0) {
            }
            if (pChannel != 0) {
                pChannel->Release();
                pChannel = 0;
            }
        }
        CommitScreenUpdate(hwndSelf, 0, 0, 0);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        Sleep(0x96);
        this->ScheduleModeTransition(0, 0, 0, 0, 1);
        AppWindow_SetScreenState(10);
        return 0;
    }

    if (PtInRect(&rectPlayAlone, pt)) {
        if (g_pNetSettings->bSkipSetupWizardMaybe != 0) {
            return 0;
        }
        g_pNetSettings->bSkipSetupWizardMaybe = 1;
        g_pDPlaySessionMgr->SetMode(3);
        RedrawSettingRectsMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, 0);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        g_UIResources.PlayUiSound(0x5015);
        return 0;
    }
    {
        if (PtInRect(&rectConnectOnline, pt) && g_pNetSettings->pDetectedProviderList != 0) {
            if (g_pNetSettings->bSkipSetupWizardMaybe == 0) {
                return 0;
            }
            g_pNetSettings->bSkipSetupWizardMaybe = 0;
            g_pDPlaySessionMgr->SetMode(0);
        } else {
            if (g_pNetSettings->bSkipSetupWizardMaybe != 0) {
                return 0;
            }
            if (PtInRect(&rectRememberChoiceOn, pt)) {
                if (g_pNetSettings->bUseSecondaryRememberedChoice != 0) {
                    return 0;
                }
                g_pNetSettings->bUseSecondaryRememberedChoice = 1;
            } else if (PtInRect(&rectRememberChoiceOff, pt)) {
                if (g_pNetSettings->bUseSecondaryRememberedChoice == 0) {
                    return 0;
                }
                g_pNetSettings->bUseSecondaryRememberedChoice = 0;
            } else {
                return 0;
            }
        }
        RedrawSettingRectsMaybe();
    }
    CommitScreenUpdate(hwndSelf, 0, 0, 0);
    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    g_UIResources.PlayUiSound(0x5015);
    return 0;
}

// FUNCTION: LOCO 0x422d80
// vtable slot 0x50 (WM_MOUSEMOVE) -- the front-end's hover-cursor feedback. Every region that
// OnLButtonDown would actually ACT on gets the "anipoint" animated hover cursor; everything else
// (including a region whose flag already holds the value clicking it would write, which
// OnLButtonDown early-returns on) gets the plain point cursor. The guard conditions here are
// exactly OnLButtonDown's own no-op tests, inverted -- the two functions were clearly written
// against each other.
LRESULT SplashWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (pCursorBitmap == NULL) {
        return WindowBase::OnMouseMove(hwndMsg, msg, wParam, lParam);
    }

    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;

    if (PtInRect(&rectEnterLabel, pt) ||
        PtInRect(&rectEscLabel, pt) ||
        (PtInRect(&rectPlayAlone, pt) && g_pNetSettings->bSkipSetupWizardMaybe == 0) ||
        (PtInRect(&rectConnectOnline, pt) && g_pNetSettings->pDetectedProviderList != 0 &&
         g_pNetSettings->bSkipSetupWizardMaybe != 0) ||
        (PtInRect(&rectRememberChoiceOn, pt) &&
         g_pNetSettings->bUseSecondaryRememberedChoice == 0) ||
        (PtInRect(&rectRememberChoiceOff, pt) &&
         g_pNetSettings->bUseSecondaryRememberedChoice != 0)) {
        this->RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
    } else {
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    }
    return 0;
}
