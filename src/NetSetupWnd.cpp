// NetSetupWnd -- the multiplayer connection-setup wizard page, plus the NetSettings singleton
// whose remembered protocol choice it edits. See src/NetSetupWnd.h for the window's layout and
// src/GameNetMsgQueue.h for NetSettings'.
//
// The TU's real extent is 0x440a50..0x4426d0 -- ~7 KB sitting inside DPlaySessionMgr.cpp's marker
// span but belonging to neither neighbour. Everything from 0x440c60 up is transcribed. The two
// functions BELOW that, 0x440a50 (39 B) and 0x440a80 (476 B), are NOT this class's: both are
// DPlaySessionMgr methods (they touch this+0x7e4 / this+0x800 and call
// DPlaySessionMgr::ReconcileCarHandoff and SetUiModeAndNotifyWidgets), so they belong in
// src/DPlaySessionMgr.cpp and are deliberately left out of this file.

#include <windows.h>
#include <stdlib.h>

#include "NetSetupWnd.h"
#include "GameNetMsgQueue.h"   // NetSettings, g_pNetSettings (0x4fd3a8)
#include "GNetManager.h"       // DPlayProviderNode
#include "UIResources.h"       // g_UIResources, SoundBankEntry
#include "DSoundChannel.h"     // SoundBankEntry
#include "LocoBitmap.h"        // RestoreOverlapBlt, g_pDDrawWorkSurface
#include "AppWindow.h"         // g_pApp
#include "SplashWnd.h"         // g_pSplashWnd, SplashWnd::SetState

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_ClampDesktopRect
#endif

NetSetupWnd *g_pNetSetupWnd;  // DAT_00485260

// The loose-file loader's install-path prefix and the "%s%s" path-join format both this file's
// NetSettings::SaveToDisk and every other loose-file writer use. Declared file-locally exactly as
// src/AppWindow.cpp and src/LoadingScreen.cpp do.
extern char g_pInstallPathPrefix[];  // DAT_004a99c8

// 0x463670 -- the "re-show every window" helper src/Main.cpp owns; declared file-locally there
// too, because that TU has no header of its own yet.
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void);   // 0x463670

// DAT_004fd3c4 -- the shared off-screen compositing surface, declared file-locally exactly as
// src/TutorialWnd.cpp and src/PopupWndBase.cpp do.
extern IDirectDrawSurface *g_pDDrawWorkSurface;  // DAT_004fd3c4

// The byte-returning inline predicate over the shared app-state global -- see docs/CODEGEN.md's
// "the sete-materialized branch is a byte-returning inline predicate". Kept TU-local exactly as
// src/MailWnd.cpp, src/DPlaySessionMgr.cpp, src/AppWindow.cpp and src/LoadingScreen.cpp each do.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// The settings blob's own defaults: protocol id 4 (IPX) on both the primary and the secondary
// remembered choice, custom value 2, no remembered ApplSetup selection on either side, and the
// setup wizard skipped until something asks for it. LoadOrInitFromDisk then overwrites the whole
// lot from NetSettings.dat if that file is present and its stamp matches.
//
// EFFECTIVE MATCH -- 85 B, insns 25/25, every store present and in the original's own order. The
// residual is purely which register each of the three shared constants lands in (the original
// takes 2 in ecx / 0 in eax / 4 in edx up front; ours takes 2 in eax and re-derives 0 later), and
// the displacement bytes that follow from it. Two source orderings were measured: the emitted
// order (total 36374) and this one, plain declaration order (28144) -- neither closes it, and
// declaration order is what a human would have written anyway.
//
// FUNCTION: LOCO 0x440c60
NetSettings::NetSettings()
{
    magicMaybe = 0x6a;
    bValidSaveLoadedMaybe = 0;
    bSkipSetupWizardMaybe = 1;
    bUseSecondaryRememberedChoice = 0;
    nTickSleepMs = 30;
    pDetectedProviderList = NULL;
    bRememberedApplSetupValidSecondaryMaybe = 0;
    rememberedProtocolSecondary = 4;
    nRememberedCustomValueSecondary = 2;
    bRememberedApplSetupValidPrimaryMaybe = 0;
    rememberedProtocolPrimary = 4;
    szRememberedAddrPrimary[0] = '\0';
    szRememberedAddrPrimaryAlt[0] = '\0';
    nRememberedCustomValuePrimary = 2;
    LoadOrInitFromDisk();
}

// ~NetSettings' BODY is not here: the image has no standalone `??1NetSettings`, so the body lives
// IN-CLASS in src/GameNetMsgQueue.h, where it folds into the compiler-generated `??_GNetSettings`
// scalar deleting destructor exactly as the original does. This TU is where that COMDAT lands, so
// the marker stays here (hint-only, no source line of its own -- the same shape `??_GDSound` uses).
//
// FUNCTION: LOCO 0x440cc0 (??_GNetSettings scalar dtor; ~NetSettings() itself inlines into this
// wrapper -- see the inline definition in GameNetMsgQueue.h)

// SaveToDisk's counterpart, called straight out of the ctor: reads NetSettings.dat back over the
// same flat 0xac-byte span, or -- if the file is missing, unreadable or carries the wrong stamp --
// re-seeds every field to the same defaults the ctor just wrote and writes a fresh file out.
//
// pDetectedProviderList lives INSIDE the serialized span (+0x10), so the live provider-list head
// would be clobbered by the read; the function saves it across the whole body and restores it on
// the way out. // sic: it does NOT restore it on the ReadFile-failed path -- that one early-returns
// straight after CloseHandle, leaving the list head NULL. Harmless in practice only because
// LoadOrInitFromDisk runs from the ctor, before GameNetThread_InitState ever hands a list over.
//
// EFFECTIVE MATCH -- 406 B vs 406 B, insns 131/131, exact length, total 450. Only four
// instructions differ, and all four are the same fact twice over: the two copies of the
// `pDetectedProviderList = pSavedProviderList` restore tail read their local from [esp+0x10]
// where the original reads [esp+0x14] (and so land in eax/ecx where the original uses edx).
// The original's frame is dwRead@+0x10, pSavedProviderList@+0x14, dwWritten@+0x18; ours is
// pSavedProviderList@+0x10, dwRead@+0x14, dwWritten@+0x18 -- i.e. cl 11.00 puts the ONE
// non-address-taken (merely spilled) local below both address-taken DWORDs, while the original
// has it wedged between them. Six orderings/shapes were measured and every one of them scored
// identically (450): saved-local declared first / between / last / after the buffers, buffers
// hoisted above the scalars, the saved local given an initializer instead of an assignment,
// the saved local retyped from `DPlayProviderNode *` to a plain `DWORD` with casts, and an
// extra unused pointer local inserted ahead of it. Declaration order, declaration form and
// local type are all inert here -- the address-taken/spilled split is the allocator's own, so
// there is no source-level lever. (The one shape that scored WORSE, 56810, was giving the
// saved local an initializer -- it hoists the read of pDetectedProviderList above the
// bValidSaveLoadedMaybe store and reshuffles the prologue.)
//
// FUNCTION: LOCO 0x440d00
void NetSettings::LoadOrInitFromDisk()
{
    HANDLE hFile;
    DWORD dwRead = 0;
    DPlayProviderNode *pSavedProviderList;
    DWORD dwWritten;
    char szPath[1284] = "";
    char szWritePath[1284];

    pSavedProviderList = pDetectedProviderList;
    pDetectedProviderList = NULL;
    bValidSaveLoadedMaybe = 0;

    wsprintfA(szPath, "%s%s", g_pInstallPathPrefix, "NetSettings.dat");
    hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        if (!ReadFile(hFile, &magicMaybe, 0xac, &dwRead, NULL)) {
            CloseHandle(hFile);
            return;
        }
        CloseHandle(hFile);

        if (magicMaybe != 0x6a) {
            magicMaybe = 0x6a;
            bRememberedApplSetupValidSecondaryMaybe = 0;
            rememberedProtocolSecondary = 4;
            nRememberedCustomValueSecondary = 2;
            bRememberedApplSetupValidPrimaryMaybe = 0;
            rememberedProtocolPrimary = 4;
            szRememberedAddrPrimary[0] = '\0';
            szRememberedAddrPrimaryAlt[0] = '\0';
            nRememberedCustomValuePrimary = 2;
            bSkipSetupWizardMaybe = 1;
            bUseSecondaryRememberedChoice = 0;
            nTickSleepMs = 30;
            bValidSaveLoadedMaybe = 0;

            wsprintfA(szWritePath, "%s%s", g_pInstallPathPrefix, "NetSettings.dat");
            hFile = CreateFileA(szWritePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                if (!WriteFile(hFile, &magicMaybe, 0xac, &dwWritten, NULL)) {
                    CloseHandle(hFile);
                    pDetectedProviderList = pSavedProviderList;
                    return;
                }
                CloseHandle(hFile);
                pDetectedProviderList = pSavedProviderList;
                return;
            }
        } else {
            bValidSaveLoadedMaybe = 1;
        }
        pDetectedProviderList = pSavedProviderList;
    }
}

// Flat 0xac-byte dump of everything after the vptr into <install>NetSettings.dat. A failed
// CreateFileA is silently ignored -- the settings simply do not persist.
//
// EFFECTIVE MATCH -- 114 B vs 116 B, insns 37/38. The whole residual is ONE dead `test eax,eax`
// the original emits on WriteFile's return between the CloseHandle argument push and the call,
// with no branch consuming it: a leftover normalization of a result nothing reads. Same fold-side
// class as src/LoadingScreen.cpp's four redundant byte tests -- our cl 11.00 drops it.
//
// FUNCTION: LOCO 0x440ea0
void NetSettings::SaveToDisk()
{
    HANDLE hFile;
    DWORD dwWritten;
    char szPath[1284];

    wsprintfA(szPath, "%s%s", g_pInstallPathPrefix, "NetSettings.dat");
    hFile = CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                        FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, &magicMaybe, 0xac, &dwWritten, NULL);
        CloseHandle(hFile);
    }
}

// Chains WindowBase's ctor with the page's own resource id, installs this class's vtable and
// seeds the page's widgets.
//
// FUNCTION: LOCO 0x440f20
NetSetupWnd::NetSetupWnd(HINSTANCE hInstanceArg, UINT resourceIdArg)
    : WindowBase(hInstanceArg, resourceIdArg)
{
    InitFields();
}

// Zeroes the page's own state, makes the background brush, and constructs the seven button
// ResourceRefs in field order, then publishes the singleton. The seven `new ResourceRef(id)`
// expressions each compile to the /GX new-expression alloc-protection dance (operator new,
// null-check, ctor, EH-state bump) -- that scaffolding is the compiler's, not the source's.
//
// FUNCTION: LOCO 0x440fa0
void NetSetupWnd::InitFields()
{
    hRedrawTimer = 0;
    bUnk0xe8Maybe = 0;
    nUnk0x140Maybe = 3;
    szLabelText[0] = '\0';
    hwndInputBox = NULL;
    hIcon = NULL;
    bResourcesLoadedMaybe = 0;
    bProviderId4AvailableMaybe = 0;
    bProviderId2AvailableMaybe = 0;
    hBackgroundBrush = CreateSolidBrush(0xa8c4d8);

    pGoBtn = new ResourceRef(0x419);
    pExitBtn = new ResourceRef(0x41a);
    pIpxBtn = new ResourceRef(0x417);
    pTcpBtn = new ResourceRef(0x418);
    pInputBox = new ResourceRef(0x41f);
    pMatrixBtn = new ResourceRef(0x420);
    pTrayBtn = new ResourceRef(0x421);

    pIconDescMaybe = NULL;
    g_pNetSetupWnd = this;
}

// Undoes InitFields plus OnFirstActivateMaybe's realize pass, in that order: drops the raw eighth
// icon descriptor's reference and the seven ResourceRefs' realized handles (only while they are
// still loaded), deletes the seven ResourceRefs, releases the shared click sound, deletes the
// background brush, then chains WindowBase's own dtor. g_pNetSetupWnd is deliberately left
// dangling -- nothing here clears it. // sic:
//
// FUNCTION: LOCO 0x440f80 (??_GNetSetupWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x441190
NetSetupWnd::~NetSetupWnd()
{
    if (bResourcesLoadedMaybe) {
        pIconDescMaybe->ReleaseRef();
        pGoBtn->ReleaseRealized();
        pExitBtn->ReleaseRealized();
        pIpxBtn->ReleaseRealized();
        pTcpBtn->ReleaseRealized();
        pInputBox->ReleaseRealized();
        pMatrixBtn->ReleaseRealized();
        pTrayBtn->ReleaseRealized();
        bResourcesLoadedMaybe = 0;
    }

    delete pGoBtn;
    delete pExitBtn;
    delete pIpxBtn;
    delete pTcpBtn;
    delete pInputBox;
    delete pMatrixBtn;
    delete pTrayBtn;

    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    if (pEntry != NULL) {
        pEntry->Release();
    }

    if (hBackgroundBrush) {
        DeleteObject(hBackgroundBrush);
        hBackgroundBrush = NULL;
    }
}

// Sizes the page to the whole desktop client rect and chains WindowBase's 11-argument virtual
// Create with WS_POPUP|WS_VISIBLE-shaped style 0x41000000.
//
// Two levers, both load-bearing. The `? true : false` ternary on the return is docs/CODEGEN.md's
// bool-materialization lever: without it cl emits the `neg/sbb/neg` idiom where the original has
// a bare `test al,al; setne al`. And the `hwndDesktop` local pins argument-evaluation ORDER --
// written as the nested `GetClientRect(GetDesktopWindow(), &rectDesktop)`, cl evaluates
// right-to-left and emits the `lea` of &rectDesktop BEFORE the GetDesktopWindow call, while the
// original calls GetDesktopWindow first. That one swap was reported as an "effective" register
// rotation for many sessions; it was really a source-order tell, and naming the result closed the
// whole function (found on ApplSetupWnd::Create 0x408f00, which has the identical body).
//
// FUNCTION: LOCO 0x4412f0
unsigned char NetSetupWnd::Create(HWND hwndOwnerParam)
{
    RECT rectDesktop;
    HWND hwndDesktop = GetDesktopWindow();

    GetClientRect(hwndDesktop, &rectDesktop);
#ifdef LOCO_PORT
    Port_ClampDesktopRect(&rectDesktop); // PORT: desktop != screen here; see port/PortMode.h
#endif
    hIcon = LoadIconA(hInstance, MAKEINTRESOURCE(0x65));
    return WindowBase::Create(0, hwndOwnerParam, rectDesktop.left, rectDesktop.top,
                              rectDesktop.right - rectDesktop.left,
                              rectDesktop.bottom - rectDesktop.top,
                              NULL, hIcon, 0, 0x41000000, 0) != 0 ? true : false;
}

// Walks the detected-provider list and marks which of the two protocol ids the remembered-choice
// fields can name are still present on this machine. Purely additive -- nothing ever clears the
// two flags, so a provider list that shrinks between runs leaves them stale.
//
// FUNCTION: LOCO 0x4419c0
void NetSetupWnd::RefreshProviderAvailability()
{
    DPlayProviderNode *pNode;

    for (pNode = g_pNetSettings->pDetectedProviderList; pNode != NULL; pNode = pNode->pNext) {
        switch (pNode->nProviderType) {
        case 2:
            bProviderId2AvailableMaybe = 1;
            break;
        case 4:
            bProviderId4AvailableMaybe = 1;
            break;
        }
    }
}

// Lazily builds the child EDIT the page types the connection address into. hwndInputBox doubles
// as the "already built" flag, so a second call is a no-op. The control is created hidden (no
// WS_VISIBLE -- the page blits its own frame around it and shows it separately), given the 24pt
// UI font and a 0x40-character limit to match szRememberedAddrPrimaryAlt's own size, seeded with
// the remembered address, then subclassed.
//
// FUNCTION: LOCO 0x441720
void NetSetupWnd::CreateInputBox()
{
    if (hwndInputBox == NULL) {
        hwndInputBox = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL,
                                       rectInputBox.left, rectInputBox.top,
                                       rectInputBox.right - rectInputBox.left,
                                       rectInputBox.bottom - rectInputBox.top,
                                       hwndSelf, (HMENU)0x41f, (HINSTANCE)hInstance, NULL);
        if (hwndInputBox != NULL) {
            PostMessageA(hwndInputBox, WM_SETFONT, (WPARAM)g_UIResources.m_hFont24, 1);
            PostMessageA(hwndInputBox, EM_LIMITTEXT, 0x40, 0);
            SetWindowTextA(hwndInputBox, g_pNetSettings->szRememberedAddrPrimaryAlt);
            pOrigInputBoxProc = (WNDPROC)SetWindowLongA(hwndInputBox, GWL_WNDPROC,
                                                        (LONG)NetSetupWnd_EditSubclassProc);
        }
    }
}

// The EDIT subclass proc. Two jobs: claim WM_SETCURSOR and set the real system I-beam over the
// box (the game hides the system cursor and paints its own everywhere else, so the text field has
// to put a genuine one back by hand), and hand Enter and Escape back to the wizard page -- an
// EDIT swallows both, which would otherwise make the two keys dead whenever the box has focus.
//
// FUNCTION: LOCO 0x4417e0
LRESULT CALLBACK NetSetupWnd_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UINT msgId = msg & 0xffff;

    switch (msgId) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorA(NULL, IDC_IBEAM));
        return 1;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
            PostMessageA(g_pNetSetupWnd->hwndSelf, WM_KEYDOWN, wParam, lParam);
            return 0;
        }
        break;
    }
    return CallWindowProcA((FARPROC)g_pNetSetupWnd->pOrigInputBoxProc, hwnd, msgId, wParam, lParam);
}

// vtable slot 0x08 -- the page's activation hook, one-shot-gated on bResourcesLoadedMaybe for the
// resource-realize half. The eighth (raw) icon descriptor is realized inline here rather than via
// a ResourceRef, then all seven ResourceRefs are loaded. // sic: the provider-availability walk
// below is a third, inlined copy of RefreshProviderAvailability's body, and unlike that one it
// re-runs on EVERY activation rather than only the first.
//
// FUNCTION: LOCO 0x441870
void NetSetupWnd::BeginModalCapture()
{
    bUnk0x148Maybe = 0;

    if (bResourcesLoadedMaybe == 0) {
        pIconDescMaybe = g_UIResources.TileKind_GetOrLoadDescriptor(0x439);
        pIconRealizedMaybe = pIconDescMaybe->GetOrLoadFrameBitmap(0, 0);
        pGoBtn->Load();
        pExitBtn->Load();
        pIpxBtn->Load();
        pTcpBtn->Load();
        pInputBox->Load();
        pMatrixBtn->Load();
        pTrayBtn->Load();
        bResourcesLoadedMaybe = 1;
    }

    RefreshClientClipRect();

    for (DPlayProviderNode *pNode = g_pNetSettings->pDetectedProviderList; pNode != NULL;
         pNode = pNode->pNext) {
        switch (pNode->nProviderType) {
        case 2:
            bProviderId2AvailableMaybe = 1;
            break;
        case 4:
            bProviderId4AvailableMaybe = 1;
            break;
        }
    }

    WindowBase::BeginModalCapture();
    SetFocus(hwndSelf);
    RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);

    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
    }

    hRedrawTimer = SetTimer(hwndSelf, 0x50, 0x32, NULL);
    nUnk0x140Maybe = 2;
    g_UIResources.LoadLocaleString(0x79, szLabelText, 0x40);
    LayoutAndDrawLabel();
}

// vtable slot 0x04 -- the deactivation half of BeginModalCapture: stops the 50ms redraw pump and
// drops every realized resource handle (the ResourceRef objects themselves survive; only their
// loaded bitmaps go), then chains the base teardown. The release block is byte-for-byte the same
// one the destructor runs -- the dtor re-runs it because a window can be destroyed without ever
// having been deactivated.
//
// FUNCTION: LOCO 0x441a00
void NetSetupWnd::EndActiveSession()
{
    KillTimer(hwndSelf, hRedrawTimer);

    if (bResourcesLoadedMaybe) {
        pIconDescMaybe->ReleaseRef();
        pGoBtn->ReleaseRealized();
        pExitBtn->ReleaseRealized();
        pIpxBtn->ReleaseRealized();
        pTcpBtn->ReleaseRealized();
        pInputBox->ReleaseRealized();
        pMatrixBtn->ReleaseRealized();
        pTrayBtn->ReleaseRealized();
        bResourcesLoadedMaybe = 0;
    }

    WindowBase::EndActiveSession();
}

// vtable slot 0x20 -- the page's full repaint: restore the background under the whole client
// clip from the work surface, draw the tray/go/exit buttons, repaint the status text, then flip
// the composed frame to the screen.
//
// FUNCTION: LOCO 0x441a90
void NetSetupWnd::OnActivate(int reservedMaybe)
{
    pIconRealizedMaybe->RestoreOverlapBlt(rectClipBounds, g_pDDrawWorkSurface, rectIconMaybe, 1);
    pTrayBtn->DrawFrame(0, NULL);
    pGoBtn->DrawFrame(0, NULL);
    pExitBtn->DrawFrame(0, NULL);
    DrawStatusTextMaybe();
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    bUnk0x148Maybe = 1;
}

// The page's "which protocol is selected" repaint, called from OnActivate and from every input
// handler that changes the remembered choice. The tray button is always drawn unpressed; the IPX
// and TCP buttons are drawn pressed only when they name the currently remembered protocol, and
// only when that provider was actually detected on this machine. The child EDIT is only visible
// in the one state that needs a typed address -- primary/TCP -- where it also takes the focus and
// gets its frame graphic painted around it.
//
// FUNCTION: LOCO 0x441b40
void NetSetupWnd::DrawStatusTextMaybe()
{
    pTrayBtn->DrawFrame(0, NULL);

    if (g_pNetSettings->bUseSecondaryRememberedChoice) {
        ShowWindow(hwndInputBox, SW_HIDE);
        if (bProviderId4AvailableMaybe) {
            if (g_pNetSettings->rememberedProtocolSecondary == 4) {
                pIpxBtn->DrawFrame(1, NULL);
            } else {
                pIpxBtn->DrawFrame(0, NULL);
            }
        }
        if (bProviderId2AvailableMaybe) {
            if (g_pNetSettings->rememberedProtocolSecondary == 2) {
                pTcpBtn->DrawFrame(1, NULL);
            } else {
                pTcpBtn->DrawFrame(0, NULL);
            }
        }
    } else {
        if (bProviderId4AvailableMaybe) {
            if (g_pNetSettings->rememberedProtocolPrimary == 4) {
                pIpxBtn->DrawFrame(1, NULL);
                ShowWindow(hwndInputBox, SW_HIDE);
            } else {
                pIpxBtn->DrawFrame(0, NULL);
            }
        }
        if (bProviderId2AvailableMaybe) {
            if (g_pNetSettings->rememberedProtocolPrimary == 2) {
                pInputBox->DrawFrame(0, NULL);
                pTcpBtn->DrawFrame(1, NULL);
                ShowWindow(hwndInputBox, SW_SHOW);
                SetFocus(hwndInputBox);
            } else {
                ShowWindow(hwndInputBox, SW_HIDE);
                pTcpBtn->DrawFrame(0, NULL);
            }
        }
    }
}

// vtable slot 0x80 (WM_CLOSE) -- while the app is alive and not already tearing down, closing
// this page does not close anything: it cancels the pending cursor-mode transition and bounces
// the front end back to SplashWnd state 7. Only once the app is gone does it fall through to
// WindowBase's real close.
//
// FUNCTION: LOCO 0x441f20
LRESULT NetSetupWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        ScheduleModeTransition(NULL, 0, NULL, 0, 1);
        g_pSplashWnd->SetState(7);
        return 0;
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x1c -- the page's whole layout pass, run from BeginModalCapture once the seven
// button resources are realized (and a no-op before that, because every rect below is sized from a
// realized descriptor's own natural width/height). Everything hangs off two anchors: a fixed
// 800x600 page centered in the window's clip bounds, and the tray button pinned 0x18/0x24 in from
// that page's top-left corner. The icon's source rect is the odd one out -- it is a clip-bounds
// -sized WINDOW INTO the icon bitmap, centered on the icon, which is what OnActivate's
// RestoreOverlapBlt(rectClipBounds, ..., rectIconMaybe, 1) needs to stretch the backdrop over the
// whole screen.
//
// FUNCTION: LOCO 0x441360
void NetSetupWnd::RefreshClientClipRect()
{
    WindowBase::RefreshClientClipRect();

    if (bResourcesLoadedMaybe) {
        RECT rect;

        rectPageMaybe.left = 0;
        rectPageMaybe.right = 800;
        rectPageMaybe.top = 0;
        rectPageMaybe.bottom = 600;
        CenterRectInRect(&rectClipBounds, &rectPageMaybe);

        rect = rectClipBounds;
        rectIconMaybe.left = 0;
        rectIconMaybe.right = pIconDescMaybe->nativeWidth;
        rectIconMaybe.top = 0;
        rectIconMaybe.bottom = pIconDescMaybe->nativeHeight;
        CenterRectInRect(&rectIconMaybe, &rect);
        rectIconMaybe = rect;

        rectTrayMaybe.left = rectPageMaybe.left + 0x18;
        rectTrayMaybe.right = rectTrayMaybe.left + pTrayBtn->pCursorDesc->nativeWidth;
        rectTrayMaybe.top = rectPageMaybe.top + 0x24;
        rectTrayMaybe.bottom = rectTrayMaybe.top + pTrayBtn->pCursorDesc->nativeHeight;
        pTrayBtn->rect = rectTrayMaybe;

        rectTextAreaMaybe.left = rectTrayMaybe.left + 1;
        rectTextAreaMaybe.right = rectTextAreaMaybe.left + pMatrixBtn->pCursorDesc->nativeWidth;
        rectTextAreaMaybe.top = rectTrayMaybe.bottom + 6;
        rectTextAreaMaybe.bottom = rectTextAreaMaybe.top + pMatrixBtn->pCursorDesc->nativeHeight;
        pMatrixBtn->rect = rectTextAreaMaybe;

        rectEasterEggSoundMaybe.right = rectPageMaybe.right - 0x14;
        rectEasterEggSoundMaybe.left = rectEasterEggSoundMaybe.right - 0x64;
        rectEasterEggSoundMaybe.bottom = rectTrayMaybe.bottom + 0x50;
        rectEasterEggSoundMaybe.top = rectEasterEggSoundMaybe.bottom - 0x78;

        rect.top = rectTrayMaybe.bottom + 0x43;
        rect.bottom = rect.top + pExitBtn->pCursorDesc->nativeHeight;
        rect.right = rectTrayMaybe.right + 0xc8;
        rect.left = rect.right - pExitBtn->pCursorDesc->nativeWidth;
        pExitBtn->rect = rect;

        rect.top = pExitBtn->rect.top + 1;
        rect.bottom = rect.top + pGoBtn->pCursorDesc->nativeHeight;
        rect.right = pExitBtn->rect.left;
        rect.left = rect.right - pGoBtn->pCursorDesc->nativeWidth;
        pGoBtn->rect = rect;

        rect.left = 0;
        rect.right = pInputBox->pCursorDesc->nativeWidth;
        rect.bottom = 0;
        rect.top = -pInputBox->pCursorDesc->nativeHeight;
        CenterRectInRect(&rectTrayMaybe, &rect);
        OffsetRect(&rect, -3, 0x46);
        pInputBox->rect = rect;

        rectInputBox.top = pInputBox->rect.top + 6;
        rectInputBox.bottom = pInputBox->rect.bottom - 0xa;
        rectInputBox.left = pInputBox->rect.left + 0x48;
        rectInputBox.right = pInputBox->rect.right - 6;

        rect.left = pInputBox->rect.left;
        rect.right = rect.left + pIpxBtn->pCursorDesc->nativeWidth;
        rect.top = rect.left - rectTrayMaybe.left + rectTrayMaybe.top;
        rect.bottom = rect.top + pIpxBtn->pCursorDesc->nativeHeight;
        pIpxBtn->rect = rect;

        rect.top = pIpxBtn->rect.top;
        rect.bottom = rect.top + pTcpBtn->pCursorDesc->nativeHeight;
        rect.right = pInputBox->rect.right;
        rect.left = rect.right - pTcpBtn->pCursorDesc->nativeWidth;
        pTcpBtn->rect = rect;

        CreateInputBox();
    }
}

// vtable slot 0x40 -- WM_RBUTTONDOWN is simply WM_LBUTTONDOWN here, forwarded THROUGH the
// vtable exactly as an unqualified call to a virtual member compiles; do not "optimize" it to a
// class-qualified call. UNMARKED: this body ICF-folds onto 0x4323c0, whose marker lives on
// MapWnd::OnRButtonDown (src/MapWnd.cpp). The WindowBase hierarchy's copies fold to a
// DIFFERENT address than the other family's purely because the slot displacement differs.
LRESULT NetSetupWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x38 (WM_LBUTTONDOWN) -- the page's whole hit-test, gated on bUnk0x148Maybe so a
// click that lands before the first full paint is ignored. Go and Exit both flash their button for
// 150 ms and then leave the page (Go persisting the typed address and the "this half of the
// remembered choice is now valid" flag first); the two protocol buttons only rewrite the remembered
// protocol id, and only when their provider was actually detected. Anything left over lands in the
// easter-egg rect, which just plays one of a handful of random sounds where the click was.
//
// FUNCTION: LOCO 0x441c80
LRESULT NetSetupWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;

    pt.x = LOWORD(lParam);
    pt.y = HIWORD(lParam);

    if (bUnk0x148Maybe) {
        if (PtInRect(&pGoBtn->rect, pt)) {
            pGoBtn->DrawFrame(1, NULL);
            g_UIResources.PlayUiSound(0x5015);
            CommitRectUpdate(pGoBtn->rect);
            Sleep(150);
            ScheduleModeTransition(NULL, 0, NULL, 0, 1);
            GetWindowTextA(hwndInputBox, g_pNetSettings->szRememberedAddrPrimaryAlt, 0x40);
            if (g_pNetSettings->bUseSecondaryRememberedChoice) {
                g_pNetSettings->bRememberedApplSetupValidSecondaryMaybe = 1;
            } else {
                g_pNetSettings->bRememberedApplSetupValidPrimaryMaybe = 1;
            }
            g_pNetSettings->SaveToDisk();
            g_pSplashWnd->SetState(3);
        } else if (PtInRect(&pExitBtn->rect, pt)) {
            pExitBtn->DrawFrame(1, NULL);
            g_UIResources.PlayUiSound(0x5015);
            CommitRectUpdate(pExitBtn->rect);
            Sleep(150);
            ScheduleModeTransition(NULL, 0, NULL, 0, 1);
            g_pSplashWnd->SetState(7);
        } else if (PtInRect(&pIpxBtn->rect, pt) && bProviderId4AvailableMaybe) {
            if (g_pNetSettings->bUseSecondaryRememberedChoice) {
                g_pNetSettings->rememberedProtocolSecondary = 4;
            } else {
                g_pNetSettings->rememberedProtocolPrimary = 4;
            }
            DrawStatusTextMaybe();
            g_UIResources.PlayUiSound(0x5015);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        } else if (PtInRect(&pTcpBtn->rect, pt) && bProviderId2AvailableMaybe) {
            if (g_pNetSettings->bUseSecondaryRememberedChoice) {
                g_pNetSettings->rememberedProtocolSecondary = 2;
            } else {
                g_pNetSettings->rememberedProtocolPrimary = 2;
            }
            DrawStatusTextMaybe();
            g_UIResources.PlayUiSound(0x5015);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        } else if (PtInRect(&rectEasterEggSoundMaybe, pt)) {
            int nSoundId = rand() / 0x1fff + 0x500f;
            g_UIResources.PlaySoundAtScreenPos(nSoundId, pt.x, pt.y, 4);
        }
    }
    return 0;
}

// Re-lays and repaints the page's one line of status text. The label's own background is restored
// from the icon bitmap first (so successive strings do not smear), then the string is measured with
// DT_CALCRECT|DT_SINGLELINE into rectLabelMaybe, centered inside rectTextAreaMaybe, and finally
// pushed to one of the four edges of that box by nUnk0x140Maybe -- 0 = flush left, 1 = flush right,
// 2 = flush bottom, anything else = flush top. Only the layout is done here; the actual glyphs are
// blitted by the next repaint pass.
//
// FUNCTION: LOCO 0x4421d0
void NetSetupWnd::LayoutAndDrawLabel()
{
    RECT rect = rectTextAreaMaybe;
    HDC hdc;
    HGDIOBJ hOldFont;
    int nTextHeight;

    bUnk0xe8Maybe = 1;

    if (bResourcesLoadedMaybe) {
        RECT rectDest;
        RECT rectSrc;

        CopyRect(&rectDest, &rect);
        CopyRect(&rectSrc, &rect);
        OffsetRect(&rectDest, rectClipBounds.left, rectClipBounds.top);
        OffsetRect(&rectSrc, rectIconMaybe.left, rectIconMaybe.top);
        pIconRealizedMaybe->RestoreOverlapBlt(rectDest, g_pDDrawWorkSurface, rectSrc, 1);
    }

    hdc = AcquireWorkSurfaceDC(hwndSelf);
    hOldFont = SelectObject(hdc, g_UIResources.m_hFont20);
    rectLabelMaybe = rectTextAreaMaybe;
    nTextHeight = DrawTextA(hdc, szLabelText, -1, &rectLabelMaybe, DT_CALCRECT | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);

    rectLabelMaybe.bottom = rectLabelMaybe.top + nTextHeight - 4;
    CenterRectInRect(&rectTextAreaMaybe, &rectLabelMaybe);

    switch (nUnk0x140Maybe) {
    case 0:
        OffsetRect(&rectLabelMaybe, rectTextAreaMaybe.right - rectLabelMaybe.left, 0);
        break;
    case 1:
        OffsetRect(&rectLabelMaybe, rectTextAreaMaybe.left - rectLabelMaybe.right, 0);
        break;
    case 2:
        OffsetRect(&rectLabelMaybe, 0, rectTextAreaMaybe.bottom - rectLabelMaybe.top);
        break;
    default:
        OffsetRect(&rectLabelMaybe, 0, rectTextAreaMaybe.top - rectLabelMaybe.bottom);
        break;
    }
}

// vtable slot 0x54 (WM_KEYDOWN) -- the keyboard half of OnLButtonDown's Go/Exit buttons, gated on
// the same bUnk0x148Maybe "the page has finished its first paint" flag. Enter commits (flash the Go
// button, remember that the wizard has been through once, persist, advance to SplashWnd state 3);
// Escape cancels back to state 7. Both flash their button for 150 ms first, which is why the
// commit is bracketed by a CommitScreenUpdate/Sleep pair.
//
// FUNCTION: LOCO 0x441f80
LRESULT NetSetupWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bUnk0x148Maybe) {
        switch (wParam) {
        case VK_RETURN:
            pGoBtn->DrawFrame(1, NULL);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            Sleep(150);
            ScheduleModeTransition(NULL, 0, NULL, 0, 1);
            GetWindowTextA(hwndInputBox, g_pNetSettings->szRememberedAddrPrimaryAlt, 0x40);
            if (g_pNetSettings->bUseSecondaryRememberedChoice) {
                g_pNetSettings->bRememberedApplSetupValidSecondaryMaybe = 1;
            } else {
                g_pNetSettings->bRememberedApplSetupValidPrimaryMaybe = 1;
            }
            g_pNetSettings->SaveToDisk();
            g_pSplashWnd->SetState(3);
            break;
        case VK_ESCAPE:
            pExitBtn->DrawFrame(1, NULL);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            Sleep(150);
            ScheduleModeTransition(NULL, 0, NULL, 0, 1);
            g_pSplashWnd->SetState(7);
            break;
        default:
            return DefWindowProcA(hwndMsg, msg, wParam, lParam);
        }
    }
    return 0;
}

// vtable slot 0x50 (WM_MOUSEMOVE) -- pure cursor feedback: over any of the four live buttons the
// page asks for the animated "pointing hand" cursor, everywhere else the plain pointer. The two
// protocol buttons only count as live when their provider was actually detected, so hovering a
// greyed-out IPX/TCP button gives the plain cursor.
//
// FUNCTION: LOCO 0x442090
LRESULT NetSetupWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;

    pt.x = LOWORD(lParam);
    pt.y = HIWORD(lParam);

    if (PtInRect(&pGoBtn->rect, pt) || PtInRect(&pExitBtn->rect, pt) ||
        (PtInRect(&pIpxBtn->rect, pt) && bProviderId4AvailableMaybe) ||
        (PtInRect(&pTcpBtn->rect, pt) && bProviderId2AvailableMaybe)) {
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
    } else {
        RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    }
    return 0;
}

// vtable slot 0x30 (WM_TIMER) -- the 50 ms pump BeginModalCapture starts, and the whole of the
// page's animation: the status label crawls one pixel per tick around the inside edge of
// rectTextAreaMaybe, and each tick repaints it clipped to that box. nUnk0x140Maybe is the leg of
// the circuit -- 0 = crawling left, 1 = right, 2 = up, 3 = down -- and the four-way test at the
// tail turns the corner (and re-lays the label from scratch) whenever the label has run off the
// far edge of the leg it was on. Timer id 0x50 is the only one the page owns; anything else falls
// through to the base default.
//
// FUNCTION: LOCO 0x4423d0
LRESULT NetSetupWnd::OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bUnk0xe8Maybe == 0 || wParam != 0x50) {
        return WindowBase::OnTimerDefaultMaybe(hwndMsg, msg, wParam, lParam);
    }

    RECT rect = rectTextAreaMaybe;
    HDC hdc;
    HGDIOBJ hOldFont;
    int nOldBkMode;
    COLORREF oldTextColor;
    RECT rectDraw;

    if (bResourcesLoadedMaybe) {
        RECT rectDest;
        RECT rectSrc;

        CopyRect(&rectDest, &rect);
        CopyRect(&rectSrc, &rect);
        OffsetRect(&rectDest, rectClipBounds.left, rectClipBounds.top);
        OffsetRect(&rectSrc, rectIconMaybe.left, rectIconMaybe.top);
        pIconRealizedMaybe->RestoreOverlapBlt(rectDest, g_pDDrawWorkSurface, rectSrc, 1);
    }

    hdc = AcquireWorkSurfaceDC(hwndSelf);

    switch (nUnk0x140Maybe) {
    case 0:
        OffsetRect(&rectLabelMaybe, -1, 0);
        break;
    case 1:
        OffsetRect(&rectLabelMaybe, 1, 0);
        break;
    case 2:
        OffsetRect(&rectLabelMaybe, 0, -1);
        break;
    default:
        OffsetRect(&rectLabelMaybe, 0, 1);
        break;
    }
    IntersectRect(&rectDraw, &rectLabelMaybe, &rectTextAreaMaybe);

    hOldFont = SelectObject(hdc, g_UIResources.m_hFont20);
    nOldBkMode = SetBkMode(hdc, TRANSPARENT);
    oldTextColor = SetTextColor(hdc, 0x32c8fa);

    if (rectLabelMaybe.left < rectTextAreaMaybe.left) {
        DrawTextA(hdc, szLabelText, -1, &rectDraw, DT_SINGLELINE | DT_RIGHT);
    } else if (rectLabelMaybe.top < rectTextAreaMaybe.top) {
        DrawTextA(hdc, szLabelText, -1, &rectDraw, DT_SINGLELINE | DT_BOTTOM);
    } else if (rectLabelMaybe.bottom > rectTextAreaMaybe.bottom) {
        DrawTextA(hdc, szLabelText, -1, &rectDraw, DT_SINGLELINE | DT_TOP);
    } else {
        DrawTextA(hdc, szLabelText, -1, &rectDraw, DT_SINGLELINE | DT_LEFT);
    }

    SetBkMode(hdc, nOldBkMode);
    SelectObject(hdc, hOldFont);
    SetTextColor(hdc, oldTextColor);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
    pMatrixBtn->DrawFrame(0, NULL);

    if (rectLabelMaybe.left >= rectTextAreaMaybe.right && nUnk0x140Maybe == 1) {
        nUnk0x140Maybe = 2;
        LayoutAndDrawLabel();
    } else if (rectLabelMaybe.bottom <= rectTextAreaMaybe.top && nUnk0x140Maybe == 2) {
        nUnk0x140Maybe = 0;
        LayoutAndDrawLabel();
    } else if (rectLabelMaybe.right <= rectTextAreaMaybe.left && nUnk0x140Maybe == 0) {
        nUnk0x140Maybe = 3;
        LayoutAndDrawLabel();
    } else if (rectLabelMaybe.top >= rectTextAreaMaybe.bottom && nUnk0x140Maybe == 3) {
        nUnk0x140Maybe = 1;
        LayoutAndDrawLabel();
    }

    CommitRectUpdate(rectTextAreaMaybe);
    return 0;
}

// vtable slot 0x2c -- the catch-all handler. Two real cases: a WM_SYSCOMMAND whose command bits
// are SC_SCREENSAVE re-shows the whole window stack (the screensaver having covered it), and
// WM_CTLCOLOREDIT for this page's own child EDIT paints it in the page's orange-on-background
// colours by returning the shared background brush. Everything else is DefWindowProcA.
//
// FUNCTION: LOCO 0x442150
LRESULT NetSetupWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_SCREENSAVE) {
            FUN_00463670_LotsOfShowWindow();
        }
        break;
    case WM_CTLCOLOREDIT:
        if (lParam == (LPARAM)hwndInputBox) {
            SetTextColor((HDC)wParam, 0xff5c00);
            SetBkMode((HDC)wParam, TRANSPARENT);
            return (LRESULT)hBackgroundBrush;
        }
        break;
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}
