// InitFields -- the field-init helper called from EditCardWnd's own ctor
// (0x415980, not itself transcribed this session). Builds ~35 ResourceRef icon/button
// handles (each `new ResourceRef(resourceId)` -- the "new_alloc + null-check + call"
// shape at every site is the compiler's own /GX new-expression alloc-protection scaffolding,
// not hand code, see CLAUDE.md), zeroes the 64-slot decal-thumbnail cache and the 12-slot
// random-pick array, loads a "post\Edit\colour.dat" palette-triple config file (RF-archived
// first, else loose file -- same idiom as SoundBankEntry::ResolvePathAndCheckExists,
// src/SoundBank.cpp) and parses it into 10 RGB-ish byte triples, picks 12 distinct
// pseudo-random decal-slot indices, and seeds a table of 17 sequential string ids for the
// roster-list labels.
//
// PARKED (v131, DIFF 2215/2633, orig 2653B): every `this->field` access in Ghidra's own
// decompile now goes through a named struct field (see EditCardWnd.h) -- the struct-mapping
// half of this session's work is complete and reusable by every other not-yet-transcribed
// EditCardWnd method. This specific function's byte-match is not yet closed. Confirmed this
// session: the ~1284-byte (0x504) scratch buffer preceding the wsprintfA'd colour.dat path is
// real (a plain `= ""` aggregate initializer: the pooled empty literal's byte is copied in and
// the tail zeroed) --
// moving its declaration to the very top of the function (matching the original's own
// zero-fill-before-any-`this`-write instruction order) was necessary but the compiler still
// schedules our copy of it a few statements later than the original's, an unclosed residual.
// The dominant remaining diff class is a SYSTEMATIC esi<->ebp / edi<->esi register swap
// spanning nearly the entire function (this/zero-const/minus-one-const each bound to a
// different physical register than the original) -- matches the already-documented
// "symmetric-register-swap residual class is INTRINSIC" (CLAUDE.md, Yoda lesson #29/#30,
// VC5-confirmed elsewhere) rather than anything source-steerable; not re-probed further this
// session. The colour.dat RF-archive-vs-loose-file load block (structurally modeled on
// SoundBankEntry::ResolvePathAndCheckExists's idiom) has smaller open residuals of its
// own, not yet root-caused. See CLAUDE.md's pickup for next steps.

#include <windows.h>
#include <ddraw.h>
#include <stdlib.h>
#include <string.h>
#include <fstream.h>
#include <strstrea.h>
#include <io.h>

#include "EditCardWnd.h"
#include "LocoBitmap.h"
#include "LocalPlayerIdentity.h"
#include "DSoundChannel.h" // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free
#include "UIResources.h"
#include "AppWindow.h" // g_pApp/AppWindow
#include "MailWnd.h"
#include "AlbumCardWnd.h"
#include "ScreenSaver.h" // SaveGame_ScanSavFiles is a ScreenSaver member defined in this TU
#include "WorldBoardMaybe.h" // g_worldBoard (dwHalfWidth/dwHalfHeight, OnTimerDefaultMaybe)
#include "TutorialWnd.h"     // g_pTutorialWnd (NotifyOrLaunch)
#include "DDrawSurface.h" // LocoBitmap_SetColorKey (extern "C" there), DDraw_QuerySurfaceDims
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: RGB565 surface pinning, see port/README.md
#endif


// FUNCTION: LOCO 0x415980
// The whole body is `WindowBase(hInstance, resourceId)` plus `InitFields()`. The /GX frame
// (push -1 / push handler / fs:0 link) and the two EH-state stores are compiler-generated: the
// base subobject is already live once WindowBase's ctor returns, so cl has to arm an unwind
// that destroys it if InitFields throws. Nothing in the source spells any of that.
EditCardWnd::EditCardWnd(HINSTANCE hInstance, UINT resourceId)
    : WindowBase(hInstance, resourceId)
{
    InitFields();
}

// FUNCTION: LOCO 0x415a00
void EditCardWnd::InitFields() {
    char szPath[0x504] = "";

    bPickerVsRosterGate = 0;
    hwndEdit = NULL;
    nEditMode = 0;
    hIcon = NULL;
    pIdentityTextBuffer = NULL;
    bNeedsCleanup = false;
    field_0x2b0 = 1;
    byDecalKindPending = 1;
    byDecalSubkindPending = 0;
    byDecalSubkindCommitted = 0;
    nDecalPickerScrollA = -1;
    nDecalPickerScrollB = -1;
    nSelectedDecalSlot = -1;
    field_0x24c = 0;
    bLocalCardBuilt = true;
    field_0xf0 = 1;
    field_0x388 = 0;
    hDecalHitTestTimer = 0;
    hRosterRefreshTimer = 0;
    nClickSoundCooldown = 0;
    bDecalTimerArmedMaybe = false;
    hBackgroundBrush = CreateSolidBrush(0xe8e8e8);

    pExitBtn = new ResourceRef(0x3c8c);
    pPostBtn = new ResourceRef(0x3c8e);
    pDecalHitTestToggleBtn = new ResourceRef(0x3c8f);
    pAlbumBtn = new ResourceRef(0x3c90);
    pDeleteBtn = new ResourceRef(0x3cac);
    pNetworkRosterBtn = new ResourceRef(0x3cbc);
    pBtnResMaybe_3cbe = new ResourceRef(0x3cbe);
    pBtnResMaybe_3cc2 = new ResourceRef(0x3cc2);
    pMailBtn = new ResourceRef(0x3cc3);
    pRosterScrollUpBtn = new ResourceRef(0x3cba);
    pRosterScrollDownBtn = new ResourceRef(0x3cbb);

    pPreviewIconRealized = NULL;
    pPreviewIconDesc = NULL;
    pOrigEditWndProc = NULL;

    pDecalScrollBackBtn = new ResourceRef(0x3c92);
    pDecalScrollForwardBtn = new ResourceRef(0x3c93);

    field_0x170 = 0;
    field_0x174 = 0;

    pDecalKindBtn1 = new ResourceRef(0x3c94);
    pDecalKindBtn2 = new ResourceRef(0x3c95);
    pDecalKindBtn3 = new ResourceRef(0x3c96);
    pDecalKindBtn4 = new ResourceRef(0x3c97);
    pDecalKindBtn5 = new ResourceRef(0x3c98);
    pDecalKindBtn6 = new ResourceRef(0x3c99);
    pRedWheelBtn = new ResourceRef(0x3cbf);
    pYellowWheelBtn = new ResourceRef(0x3cc0);
    pBlueWheelBtn = new ResourceRef(0x3cc1);

    for (int i = 0; i < 16; i++) {
        paDecalCategoryBtn[i] = new ResourceRef(0x3c9a + i);
    }
    pRandomizeBtnRes = new ResourceRef(0x3cab);

    for (int i2 = 0; i2 < 64; i2++) {
        paDecalThumbCache[i2] = NULL;
    }

    nDecalPickerRowOffset = 0;
    pDecalPickerSurfaceA = NULL;
    pDecalPickerSurfaceB = NULL;
    field_0x2b4 = 0;
    field_0x2b5 = 0;
    field_0x594 = 0;
    field_0x59c = 0;

    for (int i3 = 0; i3 < 10; i3++) {
        paColorSwatchBtn[i3] = new ResourceRef(0x3cad + i3);
        aColorRGBTriple[i3 * 3] = 0;
        aColorRGBTriple[i3 * 3 + 1] = 0;
        aColorRGBTriple[i3 * 3 + 2] = 0;
    }
    pHelpBitmapCache = NULL;

    wsprintfA(szPath, "%spost\\Edit\\colour.dat", g_pInstallPathPrefix);

    char *pBuf = (char *)::operator new(0x2000);
    void *pRfBuf = NULL;
    istream *pStream = NULL;

    if (g_RFIndex.pFile != NULL) {
        int nBufSize;
        pRfBuf = g_RFIndex.LoadResource((const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nBufSize);
        if (pRfBuf != NULL) {
            pStream = new istrstream((char *)pRfBuf, nBufSize);
        }
    }
    if (pStream == NULL) {
        pStream = new ifstream(szPath, ios::nocreate | ios::binary);
        if (pStream == NULL) {
            goto cleanup;
        }
    }
    if (pStream->rdstate() == 0) {
        pStream->read(pBuf, 0x2000);
        if (pStream->gcount() != 0 && pStream->gcount() < 0x2000) {
            unsigned char *pTriple = aColorRGBTriple;
            int pos = 0;
            for (int row = 10; row != 0; row--) {
                for (int col = 0; col < 3; col++) {
                    while (pBuf[pos] == ' ' || pBuf[pos] == '\n' ||
                           (pBuf[pos] == '\r' && pos <= 0x1fff)) {
                        pos++;
                    }
                    pTriple[col] = (unsigned char)atoi(pBuf + pos);
                    for (; pBuf[pos] != ' ' && pBuf[pos] != '\n' && pBuf[pos] != '\r' &&
                           pos < 0x2000; pos++) {
                    }
                }
                pTriple += 3;
            }
        }
    }

cleanup:
    if (pBuf != NULL) {
        ::operator delete(pBuf);
    }
    if (pStream != NULL) {
        delete pStream;
    }
    if (pRfBuf != NULL) {
        _free(pRfBuf);
    }

    for (int i4 = 0; i4 < 12; i4++) {
        int nPick = rand() / 0x421 + 1;
        for (int j = 0; j < i4; j++) {
            if (aRandomDecalPickIndex[j] == nPick) {
                nPick = rand() / 0x421 + 1;
                j = -1;
            }
        }
        aRandomDecalPickIndex[i4] = (unsigned char)nPick;
    }

    nSelectedRosterIndex = -1;
    nRosterCount = 999;
    aRosterLabelStringId[0] = 0x526c;
    aRosterLabelStringId[1] = 0x526d;
    aRosterLabelStringId[2] = 0x526e;
    aRosterLabelStringId[3] = 0x526f;
    aRosterLabelStringId[4] = 0x5270;
    aRosterLabelStringId[5] = 0x527e;
    aRosterLabelStringId[6] = 0x527f;
    aRosterLabelStringId[7] = 0x5280;
    aRosterLabelStringId[8] = 0x5281;
    aRosterLabelStringId[9] = 0x5282;
    aRosterLabelStringId[10] = 0x5283;
    aRosterLabelStringId[11] = 0x5284;
    aRosterLabelStringId[12] = 0x5285;
    aRosterLabelStringId[13] = 0x5286;
    aRosterLabelStringId[14] = 0x5287;
    aRosterLabelStringId[15] = 0x5288;
    aRosterLabelStringId[16] = 0x5289;
}

// FUNCTION: LOCO 0x4159e0 (??_GEditCardWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x4166b0 // TODO: sync (Ghidra: EditCardWnd_DtorMaybe -- real C++ dtor syntax
// needed here for the base-class chain call, see WindowBase.h; same naming gap as LocoBitmap's
// own header-inline dtor, just not sidestepped by an inline definition this time)
// Tears down the object: releases pIdentityTextBuffer (a CarNetState*, despite the name --
// see EditCardWnd.h), the background brush, the help/certificate canvas bitmap, conditionally
// tears down the transient decal-editor UI (TeardownBuiltUi, not itself transcribed this
// session), then releases every ResourceRef button handle, the 3 button arrays (decal
// category/color swatch/decal thumbnail cache), and finally the 2 decal-picker DirectDraw
// surfaces. Field-by-field clear placement (inside vs. after the guarding `if`) is reproduced
// exactly per field -- see docs/subsystems.md's `EditCardWnd` entry.
//
// The compiler's own auto-generated scalar deleting destructor (`??_GEditCardWnd`, Ghidra:
// EditCardWnd::FUN_004159e0) is a free byproduct of the `virtual ~EditCardWnd()` declaration
// (see EditCardWnd.h) -- not independently transcribed, same precedent as LocoBitmap.cpp's own
// `??_GLocoBitmap` marker.
EditCardWnd::~EditCardWnd() {
    if (pIdentityTextBuffer) {
        delete pIdentityTextBuffer;
        pIdentityTextBuffer = NULL;
    }
    if (hBackgroundBrush) {
        DeleteObject(hBackgroundBrush);
        hBackgroundBrush = NULL;
    }
    if (pHelpBitmapCache) {
        delete pHelpBitmapCache;
        pHelpBitmapCache = NULL;
    }
    if (bNeedsCleanup) {
        TeardownBuiltUi();
    }

    delete pExitBtn; pExitBtn = NULL;
    delete pPostBtn; pPostBtn = NULL;
    delete pDecalHitTestToggleBtn; pDecalHitTestToggleBtn = NULL;
    delete pAlbumBtn; pAlbumBtn = NULL;
    delete pDeleteBtn; pDeleteBtn = NULL;
    delete pNetworkRosterBtn; pNetworkRosterBtn = NULL;
    delete pBtnResMaybe_3cbe; pBtnResMaybe_3cbe = NULL;
    delete pBtnResMaybe_3cc2; pBtnResMaybe_3cc2 = NULL;
    delete pMailBtn; pMailBtn = NULL;
    delete pDecalScrollBackBtn; pDecalScrollBackBtn = NULL;
    delete pDecalScrollForwardBtn; pDecalScrollForwardBtn = NULL;

    if (pRosterScrollUpBtn) {
        delete pRosterScrollUpBtn;
        pRosterScrollUpBtn = NULL;
    }
    if (pRosterScrollDownBtn) {
        delete pRosterScrollDownBtn;
        pRosterScrollDownBtn = NULL;
    }

    delete pDecalKindBtn1; pDecalKindBtn1 = NULL;
    delete pDecalKindBtn2; pDecalKindBtn2 = NULL;
    delete pDecalKindBtn3; pDecalKindBtn3 = NULL;
    delete pDecalKindBtn4; pDecalKindBtn4 = NULL;
    delete pDecalKindBtn5; pDecalKindBtn5 = NULL;
    delete pDecalKindBtn6; pDecalKindBtn6 = NULL;
    delete pRedWheelBtn; pRedWheelBtn = NULL;
    delete pYellowWheelBtn; pYellowWheelBtn = NULL;
    delete pBlueWheelBtn; pBlueWheelBtn = NULL;

    for (int i = 0; i < 16; i++) {
        delete paDecalCategoryBtn[i];
        paDecalCategoryBtn[i] = NULL;
    }

    if (pRandomizeBtnRes) {
        delete pRandomizeBtnRes;
        pRandomizeBtnRes = NULL;
    }

    for (int i2 = 0; i2 < 64; i2++) {
        if (paDecalThumbCache[i2]) {
            delete paDecalThumbCache[i2];
            paDecalThumbCache[i2] = NULL;
        }
    }

    for (int i3 = 0; i3 < 10; i3++) {
        if (paColorSwatchBtn[i3]) {
            delete paColorSwatchBtn[i3];
            paColorSwatchBtn[i3] = NULL;
        }
    }

    if (pDecalPickerSurfaceA) {
        pDecalPickerSurfaceA->Release();
        pDecalPickerSurfaceA = NULL;
    }
    if (pDecalPickerSurfaceB) {
        pDecalPickerSurfaceB->Release();
        pDecalPickerSurfaceB = NULL;
    }
}

// FUNCTION: LOCO 0x4180a0 // TODO: sync (Ghidra: TeardownBuiltUi -- real C++ method syntax
// needed here, same naming gap as this TU's other two markers)
// Tears down the transient decal-editor UI built while actively editing: releases every
// ResourceRef button handle's realized resource (ReleaseRealized, self-guarding, see
// src/ResourceRef.cpp -- no per-field null check needed here either), then releases
// pPreviewIconDesc and resets pPreviewIconRealized. Whole body gated on
// bNeedsCleanup.
//
// pPreviewIconDesc's own release (last statement before the field clears) is a REAL
// virtual call, not a bug: field-mapping FUN_00417f20 (this class's own "build" counterpart,
// same bNeedsCleanup gate, opposite state) shows the field is populated from
// `TileKind::TileKind_GetOrLoadDescriptor`, whose own return type is a `BigObj*`
// (docs/subsystems.md's `BigObj` entry) -- a class confirmed (ctor `0x44b190`) to derive from
// `Obj0x4779e0` <- `CursorDesc`, inheriting (un-overridden) `CursorDesc`'s own vtable
// slot 2 = `ReleaseRef` (ground-truthed by reading `BigObj`'s vtable, `0x478358`, slot by
// slot: slot 2 resolves to `CursorDesc::ReleaseRef` at `0x4257f0`). Typed here to the
// `CursorDesc` base (see EditCardWnd.h) rather than a full `BigObj` -- only the inherited
// slots 1/2 are ever used through this field.
//
// CORRECTED 2026-07-16: an earlier pass (v134) modeled this same call as a genuine engine bug
// (a `LocoBitmap*` dispatching through a stray vtable slot into an unrelated class), reasoning
// from `EditCardWnd_BuildPreviewCanvasBMaybe` (`0x4216f0`) as supposed proof the field held a
// real `LocoBitmap*`. That function turned out to be mis-attributed: its only caller
// (`SplashWnd::FUN_00421500`) is unambiguously a `SplashWnd` method, and Ghidra's own
// (independently-built) `SplashWnd` struct already names the analogous offset
// `pDrawTargetMaybe` -- so `0x4216f0` was never an `EditCardWnd` method at all. Retyped its
// `this` to `SplashWnd*` and renamed it `SplashWnd::BuildDrawTargetCompositeMaybe`; the
// `docs/engine-bugs.md` entry is retracted (see that file). No probe-class hack is needed here
// any more -- `ReleaseRef()` is a real, declared virtual (src/CursorDesc.h).
void EditCardWnd::TeardownBuiltUi() {
    if (bNeedsCleanup) {
        pRosterScrollUpBtn->ReleaseRealized();
        pRosterScrollDownBtn->ReleaseRealized();
        pExitBtn->ReleaseRealized();
        pPostBtn->ReleaseRealized();
        pDecalHitTestToggleBtn->ReleaseRealized();
        pAlbumBtn->ReleaseRealized();
        pDeleteBtn->ReleaseRealized();
        pNetworkRosterBtn->ReleaseRealized();
        pBtnResMaybe_3cbe->ReleaseRealized();
        pBtnResMaybe_3cc2->ReleaseRealized();
        pMailBtn->ReleaseRealized();
        pDecalScrollBackBtn->ReleaseRealized();
        pDecalScrollForwardBtn->ReleaseRealized();
        pDecalKindBtn1->ReleaseRealized();
        pDecalKindBtn2->ReleaseRealized();
        pDecalKindBtn3->ReleaseRealized();
        pDecalKindBtn4->ReleaseRealized();
        pDecalKindBtn5->ReleaseRealized();
        pDecalKindBtn6->ReleaseRealized();
        pRedWheelBtn->ReleaseRealized();
        pYellowWheelBtn->ReleaseRealized();
        pBlueWheelBtn->ReleaseRealized();

        for (int i = 0; i < 16; i++) {
            paDecalCategoryBtn[i]->ReleaseRealized();
        }
        for (int i2 = 0; i2 < 10; i2++) {
            paColorSwatchBtn[i2]->ReleaseRealized();
        }
        pRandomizeBtnRes->ReleaseRealized();

        pPreviewIconDesc->ReleaseRef();

        pPreviewIconDesc = NULL;
        pPreviewIconRealized = NULL;
        bNeedsCleanup = false;
    }
}

LRESULT CALLBACK EditCardWnd_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Sizes the screen to the whole desktop client rect, chains WindowBase's 11-argument virtual
// Create with WS_POPUP|WS_MAXIMIZE-shaped style 0x81000000, then stands up the child EDIT the
// card's description text is typed into. The `hwndDesktop` local is the same load-bearing lever
// NetSetupWnd::Create needed (src/NetSetupWnd.cpp): written as the nested
// GetClientRect(GetDesktopWindow(), &rect), cl evaluates right-to-left and emits the `lea` of
// &rect BEFORE the GetDesktopWindow call, while the original calls GetDesktopWindow first.
//
// The EDIT is created hidden (no WS_VISIBLE -- BeginEdit below shows and hides it by hand),
// given the shared 16px UI font and a 0x4f-character limit to match szDescription's own size,
// then subclassed. // sic: unlike NetSetupWnd::CreateInputBox, nothing null-checks the
// CreateWindowExA result before three messages are posted at it.
//
// FUNCTION: LOCO 0x4169e0
unsigned char EditCardWnd::Create(HWND hwndOwner) {
    RECT rect;
    HWND hwndDesktop = GetDesktopWindow();

    GetClientRect(hwndDesktop, &rect);
#ifdef LOCO_PORT
    Port_ClampDesktopRect(&rect); // PORT: desktop != screen here; see port/PortMode.h
#endif
    hIcon = LoadIconA(hInstance, MAKEINTRESOURCE(0x65));
    BuildEditUiResources();

    if (!WindowBase::Create(0, hwndOwner, rect.left, rect.top, rect.right - rect.left,
                            rect.bottom - rect.top, NULL, hIcon, 0, 0x81000000, 0)) {
        return 0;
    }

    hwndEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                               WS_CHILD | ES_MULTILINE | ES_WANTRETURN,
                               rectDescriptionEdit.left, rectDescriptionEdit.top,
                               rectDescriptionEdit.right - rectDescriptionEdit.left,
                               rectDescriptionEdit.bottom - rectDescriptionEdit.top,
                               hwndSelf, (HMENU)0x411, (HINSTANCE)hInstance, NULL);
    PostMessageA(hwndEdit, WM_SETFONT, (WPARAM)g_UIResources.m_hFont16, 1);
    PostMessageA(hwndEdit, EM_LIMITTEXT, 0x4f, 0);
    pOrigEditWndProc = (WNDPROC)SetWindowLongA(hwndEdit, GWL_WNDPROC,
                                               (LONG)EditCardWnd_EditSubclassProc);
    TeardownBuiltUi();
    return 1;
}

// The description EDIT's subclass proc, installed by Create above. Two jobs, and the same pair
// of jobs NetSetupWnd_EditSubclassProc (0x4417e0) has: claim WM_SETCURSOR and put a real system
// cursor back over the box (the game hides the system cursor and paints its own everywhere
// else), and hand Escape back to the screen -- an EDIT swallows it, which would otherwise make
// the key dead whenever the description box has focus. The one behavioural difference from the
// NetSetupWnd sibling: this one forwards Escape only, not Enter -- the description box is a
// multiline EDIT (ES_MULTILINE|ES_WANTRETURN), so Return is a real newline here.
//
// FUNCTION: LOCO 0x416b00
LRESULT CALLBACK EditCardWnd_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UINT msgId = msg & 0xffff;

    switch (msgId) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorA(NULL, IDC_IBEAM));
        return 1;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostMessageA(g_pEditCardWnd->hwndSelf, WM_KEYDOWN, VK_ESCAPE, lParam);
            return 0;
        }
        break;
    }
    return CallWindowProcA((FARPROC)g_pEditCardWnd->pOrigEditWndProc, hwnd, msgId, wParam, lParam);
}

// FUNCTION: LOCO 0x416b80
// The "open/begin editing" entry point, and the exact mirror image of EndActiveSession below:
// where that one tears the edit session down, this one stands it up. Gated on the same
// bModalCaptureActive flag (this side requires it CLEAR, that side requires it SET), so the pair
// cannot re-enter each other. Rebuilds the UI resources, takes the modal mouse capture, maximizes
// the window and hides the description edit box, resets the whole decal-editor selection state,
// destroys every cached decal thumbnail, and recreates the two system-memory decal-picker
// surfaces at the picker grid's own size -- colour-keying each one right after creation via the
// SAME DDSURFACEDESC scratch it just created it from (Ddraw_QuerySurfacePixelFormat fills the
// scratch from the fresh surface, LocoBitmap_SetColorKey then overwrites it as its DDCOLORKEY
// out-param; the two calls only ever share the buffer, never its contents).
//
// Then one of two identity paths. With a clone source (AlbumCardWnd's grid handing over an
// existing card) it takes OWNERSHIP of that card -- deleting whatever card it already held --
// puts the card's description into the edit box, clears the attachment fields, mirrors the card's
// three identity colours into the editor's live channel values, and stamps the LOCAL player's
// name into the card's nameB. Without one it only acts if it is holding no card at all, in which
// case it mints a fresh local-player card and blanks the edit box. Finishes by rebuilding the
// roster list and arming the 50 ms roster-refresh timer (id 0x53).
//
// EXACT MATCH, 633 B. Two levers, both worth remembering:
//   1. The CreateSurface HRESULT is captured in a NAMED LOCAL and the local is tested. Testing
//      the call expression inline compiles to `test eax,eax`; through a local VC5 compares
//      against its register-allocated constant 0 (`cmp eax,ebx`, ebx being the same zero the
//      whole function already uses for its stores and NULL pushes). That was the entire
//      remaining residual here (DIFF 16 -> 12) and it is the same lever that took
//      PopupWndBase::CommitScreenUpdate from DIFF(66) to DIFF(62).
//   2. The DDSURFACEDESC field-assignment ORDER is a source fact, worth 12 bytes: dwWidth before
//      dwHeight (the two loads schedule ahead of the constant stores either way, but their
//      relative order survives), and ddsCaps.dwCaps BEFORE dwFlags. The obvious
//      size/flags/width/height/caps spelling gets both pairs backwards.
void EditCardWnd::BeginEdit(CarNetState *pCloneSource) {
    extern IDirectDraw2 *g_pDDraw2; // DAT_00485440
    // Fills *pDesc from the surface's own GetSurfaceDesc; bUpdateGlobals additionally recomputes
    // the subsystem-wide bit-mask/colour-key globals from the returned pixel format. Every call
    // site here passes 0 -- the globals are already seeded by the primary surface at startup.
    extern void Ddraw_QuerySurfacePixelFormat(IDirectDrawSurface *pSurface, DDSURFACEDESC *pDesc,
                                              char bUpdateGlobals); // Ddraw::…, 0x45b9b0

    if (bModalCaptureActive == 0) {
        BuildEditUiResources();
        RefreshClientClipRect();
        bNoMoreRosterRows = false;
        WindowBase::BeginModalCapture();
        ShowWindow(hwndSelf, SW_MAXIMIZE);
        ShowWindow(hwndEdit, SW_HIDE);
        SetFocus(hwndSelf);
        nEditMode = 1;
        byDecalKindPending = 1;
        byDecalSubkindPending = 0;
        byDecalSubkindCommitted = 0;
        nDecalPickerScrollA = -1;
        nDecalPickerScrollB = -1;
        nSelectedDecalSlot = -1;

        for (int i = 0; i < 64; i++) {
            if (paDecalThumbCache[i]) {
                delete paDecalThumbCache[i];
                paDecalThumbCache[i] = NULL;
            }
        }

        DDSurfaceDescPadded0x7c ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.ddsd.dwSize = 0x7c;
        ddsd.ddsd.dwWidth = rectDecalPickerGrid.right - rectDecalPickerGrid.left;
        ddsd.ddsd.dwHeight = rectDecalPickerGrid.bottom - rectDecalPickerGrid.top;
        ddsd.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        ddsd.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;

#ifdef LOCO_PORT
        Port_ForceRgb565(&ddsd.ddsd); // PORT: pin 565, do not inherit the desktop format
#endif
        HRESULT hr = g_pDDraw2->CreateSurface(&ddsd.ddsd, &pDecalPickerSurfaceA, NULL);
        if (hr != 0) {
            OutputDebugStringA("EDIT WINDOW - failed to create surface 1");
        }
        Ddraw_QuerySurfacePixelFormat(pDecalPickerSurfaceA, &ddsd.ddsd, 0);
        LocoBitmap_SetColorKey(pDecalPickerSurfaceA, &ddsd.ddsd);

#ifdef LOCO_PORT
        // Re-stamp: the QuerySurfacePixelFormat/SetColorKey pair above rewrites the
        // descriptor in place, so the second CreateSurface would otherwise go out
        // with whatever those left behind.
        Port_ForceRgb565(&ddsd.ddsd);
#endif
        hr = g_pDDraw2->CreateSurface(&ddsd.ddsd, &pDecalPickerSurfaceB, NULL);
        if (hr != 0) {
            OutputDebugStringA("EDIT WINDOW - failed to create surface 2");
        }
        Ddraw_QuerySurfacePixelFormat(pDecalPickerSurfaceB, &ddsd.ddsd, 0);
        LocoBitmap_SetColorKey(pDecalPickerSurfaceB, &ddsd.ddsd);

        field_0x594 = 0;
        field_0x59c = 0;
        nDecalPickerRowOffset = 0;

        if (pCloneSource != NULL) {
            if (pIdentityTextBuffer != NULL) {
                delete pIdentityTextBuffer;
                pIdentityTextBuffer = NULL;
            }
            pIdentityTextBuffer = pCloneSource;
            SetWindowTextA(hwndEdit, pCloneSource->szDescription);
            pIdentityTextBuffer->wAttachmentId = 0;
            pIdentityTextBuffer->bAttachmentSoundPlayedMaybe = 1;
            CarNetState *pCard = pIdentityTextBuffer;
            byIdentityColor0 = pCard->byIdentityColorR;
            byIdentityColor1 = pCard->byIdentityColorG;
            byIdentityColor2 = pCard->byIdentityColorB;
            strcpy(pCard->nameB.sz, g_pLocalPlayerIdentity->name);
        } else if (pIdentityTextBuffer == NULL) {
            RebuildLocalPlayerCard();
            SetWindowTextA(hwndEdit, "");
        }

        BuildPlayerRosterList();
        hRosterRefreshTimer = SetTimer(hwndSelf, 0x53, 0x32, NULL);
    }
}

#include "PostBag.h" // PostBagCacheBundle / g_pPostBagCache

extern void __fastcall PostBag_ReleaseCachedBitmaps(PostBagCacheBundle *pCache); // 0x443440, see below

// FUNCTION: LOCO 0x416f70 (Ghidra: EndActiveSession -- renamed in src+Ghidra 2026-07-21 when
// WindowBase vtable slot 4 was modeled as a real virtual shared with
// AlbumCardWnd::EndActiveSession)
// The "cancel/close edit" counterpart to BeginEdit (0x416b80): ends the modal mouse capture
// (WindowBase::EndActiveSession, called base-qualified -- it clears bModalCaptureActive, the
// very flag this function is gated on), tears down the
// built decal-editor UI (TeardownBuiltUi), kills both edit-session timers, releases the 2
// decal-picker DirectDraw surfaces, and destroys every cached decal-thumbnail bitmap (a real
// `delete` -- LocoBitmap has a virtual dtor, matching ~EditCardWnd's own identical loop).
// PostBag_ReleaseCachedBitmaps(g_pPostBagCache) (real body below, ex-FUN_00443440)
// walks the 256-slot paBitmapSlots array, deleting each non-null entry -- same shape as
// the paDecalThumbCache loop just above it, a different region of the shared singleton
// (see src/PostBag.h).
void EditCardWnd::EndActiveSession() {
    if (bModalCaptureActive != 0) {
        WindowBase::EndActiveSession();
        TeardownBuiltUi();
        nEditMode = 0;

        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        if (hRosterRefreshTimer != 0) {
            KillTimer(hwndSelf, hRosterRefreshTimer);
            hRosterRefreshTimer = 0;
        }

        if (pDecalPickerSurfaceA) {
            pDecalPickerSurfaceA->Release();
            pDecalPickerSurfaceA = NULL;
        }
        if (pDecalPickerSurfaceB) {
            pDecalPickerSurfaceB->Release();
            pDecalPickerSurfaceB = NULL;
        }

        for (int i = 0; i < 64; i++) {
            if (paDecalThumbCache[i]) {
                delete paDecalThumbCache[i];
                paDecalThumbCache[i] = NULL;
            }
        }

        PostBag_ReleaseCachedBitmaps(g_pPostBagCache);

        bLocalCardBuilt = true;
        field_0xf0 = 1;
    }
}

// UIResources/g_UIResources (DAT_004855e8) -- see src/UIResources.h.

// FUNCTION: LOCO 0x417f20 // TODO: sync (Ghidra: BuildEditUiResources -- real C++ method
// syntax needed here, same naming gap as this TU's other markers)
// The "build" counterpart to TeardownBuiltUi (same bNeedsCleanup gate, opposite
// state): realizes the preview icon descriptor (kind id 0x3cb9, one below pRosterScrollUpBtn's
// own resource id -- the two are allocated as a contiguous id block) via
// TileKind_GetOrLoadDescriptor, then GetOrLoadFrameBitmap(0,0) on it for
// pPreviewIconRealized, then loads every button/decal-category/color-swatch
// ResourceRef in the same field order InitFields constructed them.
//
// EFFECTIVE MATCH (DIFF 12/369, insns 82/82 -- structurally identical): the sole residual is
// where the compiler schedules the `pPreviewIconDesc = ...` store relative to the
// following virtual call's vtable-load/arg-push sequence -- the original stores the field
// immediately after the TileKind_GetOrLoadDescriptor call returns, then reads the vtable
// off the still-cached EAX; ours schedules the same store right before the `call [edx+4]`
// instead. Both orderings are legal (no aliasing hazard between [esi+0x1f0] and [eax]) and
// both compiled forms tried -- two sequential statements, and a chained
// `x = (field = call())->Method()` expression -- produced byte-identical output, so this is an
// intrinsic /O2 scheduling tie-break (Yoda lesson #4/#29 family), not source-steerable.
void EditCardWnd::BuildEditUiResources() {
    if (!bNeedsCleanup) {
        pPreviewIconDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cb9);
        pPreviewIconRealized = pPreviewIconDesc->GetOrLoadFrameBitmap(0, 0);

        pRosterScrollUpBtn->Load();
        pRosterScrollDownBtn->Load();
        pExitBtn->Load();
        pPostBtn->Load();
        pDecalHitTestToggleBtn->Load();
        pAlbumBtn->Load();
        pDeleteBtn->Load();
        pNetworkRosterBtn->Load();
        pBtnResMaybe_3cbe->Load();
        pBtnResMaybe_3cc2->Load();
        pMailBtn->Load();
        pDecalScrollBackBtn->Load();
        pDecalScrollForwardBtn->Load();
        pDecalKindBtn1->Load();
        pDecalKindBtn2->Load();
        pDecalKindBtn3->Load();
        pDecalKindBtn4->Load();
        pDecalKindBtn5->Load();
        pDecalKindBtn6->Load();
        pRedWheelBtn->Load();
        pYellowWheelBtn->Load();
        pBlueWheelBtn->Load();

        for (int i = 0; i < 16; i++) {
            paDecalCategoryBtn[i]->Load();
        }
        for (int i2 = 0; i2 < 10; i2++) {
            paColorSwatchBtn[i2]->Load();
        }
        pRandomizeBtnRes->Load();

        bNeedsCleanup = true;
    }
}

extern char g_szDefaultDescriptionMaybe; // "..." default szDescription placeholder text, see plate comment
extern LocalPlayerIdentity *g_pLocalPlayerIdentity; // DAT_004aa4a8

#include "IniFile.h"  // IniFile, g_pIniFile (consolidated -- was a local PostBagIniFileMaybe copy)

// FUNCTION: LOCO 0x41a0e0 // TODO: sync (Ghidra: RebuildLocalPlayerCard -- real C++ method
// syntax needed here, same naming gap as this TU's other markers)
// Rebuilds the local player's own identity-card object: releases any existing
// pIdentityTextBuffer, allocates+constructs a new CarNetState, strcpy's the "..."
// placeholder into its szDescription and the local player's own name (g_pLocalPlayerIdentity
// ->name) into its nameB, re-caches the 3 identity color bytes, resets the roster/decal
// selection indices, clears field_0x189, and one-time-latches bLocalCardBuilt. Called from
// Config_InitClientIdentity whenever the display name is recomputed, and from
// BeginEdit (0x416b80) when no clone-source card
// was passed in.
void EditCardWnd::RebuildLocalPlayerCard() {
    if (pIdentityTextBuffer) {
        delete pIdentityTextBuffer;
        pIdentityTextBuffer = NULL;
    }
    pIdentityTextBuffer = new CarNetState();
    strcpy(pIdentityTextBuffer->szDescription, &g_szDefaultDescriptionMaybe);
    strcpy(pIdentityTextBuffer->nameB, g_pLocalPlayerIdentity->name);

    byIdentityColor0 = pIdentityTextBuffer->byIdentityColorR;
    byIdentityColor1 = pIdentityTextBuffer->byIdentityColorG;
    byIdentityColor2 = pIdentityTextBuffer->byIdentityColorB;
    nSelectedRosterIndex = -1;
    nSelectedDecalSlot = -1;
    field_0x189 = 0;
    if (!bLocalCardBuilt) {
        bLocalCardBuilt = true;
    }
}
// FUNCTION: LOCO 0x417180 (Ghidra: EditCardWnd::FUN_00417180, WindowBase vtable+0x1c override
// -- confirmed the class-wide default is WindowBase::RefreshClientClipRect itself, see
// docs/subsystems.md; this override calls the base implementation first, then re-lays-out the
// entire decal-editor widget cluster (help canvas, 800x600/300x200 sub-canvases, ~35
// ResourceRef button rects, the 16-slot decal-category grid, the 10-slot color-swatch
// row) whenever the client clip rect refreshes -- i.e. on window resize.
//
// Session discovery: ResourceRef's own +0x4 16-byte gap (previously a hypothesized,
// unconfirmed RECT -- see src/ResourceRef.h) is confirmed a real RECT here -- this
// function is the SECOND independent consumer writing all 4 sub-fields as a coherent
// CenterRectInRect-shaped group (satisfies CLAUDE.md's struct-merge bar). ~19 of
// EditCardWnd's own padding regions are likewise merged into named RECT fields in
// src/EditCardWnd.h this session (rectUnk0xNN -- type confirmed via the same
// SetRect/CenterRectInRect/OffsetRect/InflateRect call shapes seen here, purpose per-field
// not yet traced beyond "some button/canvas layout rect", kept at the Unk-tier per the
// project's uncertainty ladder).
//
// ⭐ REWRITTEN (v371) -- was PARKED for several sessions as "an intrinsic whole-function
// register-PRESSURE tie-break" with `insns 733/938` (205 instructions short) and a
// 2885608 asmscore total. That verdict was WRONG, and the way it was wrong is the reusable
// lesson: the original's constant round-tripping of values through ESP-relative stack slots
// was read as the allocator spilling under pressure. It is not spilling at all -- it is a
// single **address-taken local `RECT rect`**. `CenterRectInRect(pRVar1, &rect)` takes its
// address, so VC5 cannot enregister it; every `rect.field = ...` is a real store and every
// read a real load. Ghidra's decompiler hides this completely, because it folds the
// trailing whole-struct assignment `pBtn->rect = rect;` into four separate member stores and
// attributes the `[esp+N]` writes (really `rect`'s own slots) to the destination's fields.
// The tell in the raw disasm is that the original loads the destination pointer ONCE and
// stores all four fields through it (`mov [ecx],eax / [ecx+4] / [ecx+8] / [ecx+0xc]`).
//
// Four independent source-shape levers closed 86% of the residual (2885608 -> 391073,
// insns 733/938 -> 940/938). All four are now in docs/CODEGEN.md:
//   1. The one `RECT rect` local + `pBtn->rect = rect;` per block (~25 blocks). Biggest lever.
//   2. `SetRect(p, l, t, r, b)` is NOT a call here -- the original writes the four fields
//      inline. Our two SetRect calls made VC5 hoist the import thunk into `ebp`, burning a
//      callee-saved register across the whole function.
//   3. Ghidra's `unsigned short` temporaries for `pCursorDesc->nativeWidth/nativeHeight` are
//      artifacts. The original zero-extends AT THE LOAD (`xor ebx,ebx; mov bx,[..]`); a
//      ushort-typed local instead keeps a partial register and masks later (`and ebx,0xffff`).
//      Widening the temps to `int` is semantically identical (they already promoted).
//   4. The `width` vs 0x320 test is `>` with the subtracting arm FIRST, not `<=`. The original
//      is `cmp ecx,0x320; jle <plain-left arm>`; an earlier session pinned the `<=` literal
//      from that same `jle` and got the polarity backwards.
//
// Refuted here, do NOT retry: reordering the local declarations (point-of-first-use vs the
// top-of-function cluster) is bit-identical -- confirmed twice now, in the prior session and
// again in v371. That negative is consistent with the residual never having been an
// allocation problem in the first place.
//
// REMAINING (~391073, PARTIAL): a pure `edi`<->`ebp` coin-flip -- the original assigns `edi`
// to pRVar2 (&rectClipBounds, later &rectIdentityPreview) and `ebp` to pRVar1, and we assign
// them the other way round; plus the related choice of which register holds the reused zero
// (`xor ebx,ebx` early in the original vs `xor ebp,ebp` later in ours). That is the documented
// register coin-flip / zero-reg class (v329/v334/v335). Content is complete: insns 940/938.
//
// ⚠ TOOLING GOTCHA (still valid, see CLAUDE.md): Ghidra's own `get_function_by_address`
// reports this function's TRUE body as 0x417180-0x417f11 = 3473 bytes; always pass
// `tools/asmscore.py src/EditCardWnd.cpp 0x417180 --dump --len 3473` on this function, never
// the bare form (which truncates to the candidate's own, possibly-short, compiled length).
//
// Locals still mirror Ghidra's decompiled variable set (pRVar1/pRVar2/lprc/pRVar5/pRVar6/
// iVar8/nRow) rather than invented per-button names, since individual button semantics aren't
// independently confirmed yet (see EditCardWnd.h's pBtnResMaybe_<resourceIdHex> precedent).
void EditCardWnd::RefreshClientClipRect() {
  extern void CenterRectInRect(RECT *outer, RECT *rect); // 0x425a50

  WindowBase::RefreshClientClipRect();
  if (bNeedsCleanup) {
    RECT *pRVar2;
    RECT rect;
    RECT *pRVar1;
    RECT *lprc;
    int iVar8;
    ResourceRef *pRVar5;
    int nSwatchesLeft;
    ResourceRef **ppSwatch;
    ResourceRef *pRVar6;
    int nRow;
    ResourceRef **ppCategoryBtn;

    pRVar2 = &rectClipBounds;
    rect = *pRVar2;
    if (pHelpBitmapCache) {
      pRVar1 = &rectHelpCanvas;
      pRVar1->left = 0;
      rectHelpCanvas.right = pHelpBitmapCache->width;
      rectHelpCanvas.top = 0;
      rectHelpCanvas.bottom = pHelpBitmapCache->height;
      CenterRectInRect(pRVar1, &rect);
      *pRVar1 = rect;
    }
    pRVar1 = &rectMainCanvas;
    pRVar1->left = 0;
    rectMainCanvas.top = 0;
    rectMainCanvas.right = 800;
    rectMainCanvas.bottom = 600;
    CenterRectInRect(pRVar2, pRVar1);
    pRVar2 = &rectIdentityPreview;
    pRVar2->left = 0;
    rectIdentityPreview.right = 300;
    rectIdentityPreview.top = 0;
    rectIdentityPreview.bottom = 200;
    CenterRectInRect(pRVar1, pRVar2);
    OffsetRect(pRVar2,0,-0x1f);
    lprc = &rectDescriptionEditBorder;
    iVar8 = pRVar2->left + 10;
    rectDescriptionEdit.left = iVar8;
    rectDescriptionEdit.right = (rectIdentityPreview.right - pRVar2->left) / 2 + -0x14 + iVar8;
    rectDescriptionEdit.top = rectIdentityPreview.top + 10;
    rectDescriptionEdit.bottom = rectIdentityPreview.bottom + -10;
    *lprc = rectDescriptionEdit;
    InflateRect(lprc,2,2);
    rectDescriptionEditBorder.top = rectDescriptionEditBorder.top + 1;
    rectDescriptionHitZone = *lprc;
    iVar8 = rectIdentityPreview.right - 8;
    rectRosterPanel.right = iVar8;
    rectRosterPanel.left = (iVar8 - (rectIdentityPreview.right - pRVar2->left) / 2) + 0x10;
    rectRosterPanel.top = rectIdentityPreview.top + 10;
    rectRosterPanel.bottom = rectIdentityPreview.bottom + -10;
    pRVar5 = pRosterScrollUpBtn;
    rect.top = rectRosterPanel.top + 2;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    rect.left = rectRosterPanel.left + 1;
    rect.right = rectRosterPanel.right - 1;
    pRVar5->rect = rect;
    rectRosterTitle.top = rect.bottom;
    rectRosterTitle.bottom = rect.bottom + 0x18;
    rectRosterTitle.left = rect.left;
    rectRosterTitle.right = rect.right;
    pRVar5 = pRosterScrollDownBtn;
    rect.bottom = rectRosterPanel.bottom - 2;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.left = rectRosterPanel.left + 1;
    rect.right = rectRosterPanel.right - 1;
    pRVar5->rect = rect;
    rectRosterList.bottom = pRosterScrollDownBtn->rect.top - 4;
    rectRosterList.top = rectRosterTitle.bottom;
    rectRosterList.left = rectRosterPanel.left + 2;
    rectRosterList.right = rectRosterPanel.right - 2;
    rectRosterBadge = rectRosterTitle;
    rectRosterBadge.left = rectRosterList.left;
    rectRosterBadge.top = rectRosterBadge.top + 0x20;
    rectRosterBadge.bottom = rectRosterBadge.bottom + 0x2d;
    pRVar5 = pDeleteBtn;
    rect.top = rectIdentityPreview.top - (pRVar5->pCursorDesc->nativeHeight >> 1);
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    rect.left = rectIdentityPreview.right + 10;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    rectColorChannelBar0.left = pRVar2->left + 0x1d;
    rectColorChannelBar0.right = rectColorChannelBar0.left + 0x15;
    rectColorChannelBar0.bottom = rectIdentityPreview.top + -0x3d;
    rectColorChannelBar0.top = rectColorChannelBar0.bottom - 0x23;
    pRVar5 = pRedWheelBtn;
    rect.left = rectColorChannelBar0.right + 8;
    rect.top = rectColorChannelBar0.top + -0x1c;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    pRVar5->rect = rect;
    rectColorChannelBar1.left = rectColorChannelBar0.right + 0x56;
    rectColorChannelBar1.right = rectColorChannelBar1.left + 0x15;
    rectColorChannelBar1.bottom = rectColorChannelBar0.bottom;
    rectColorChannelBar1.top = rectColorChannelBar0.top;
    pRVar5 = pYellowWheelBtn;
    rect.left = rectColorChannelBar1.right + 8;
    nSwatchesLeft = 10;
    rect.top = rectColorChannelBar1.top + -0x1c;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    pRVar5->rect = rect;
    rectColorChannelBar2.left = rectColorChannelBar1.right + 0x58;
    rectColorChannelBar2.right = rectColorChannelBar2.left + 0x15;
    rectColorChannelBar2.bottom = rectColorChannelBar0.bottom;
    rectColorChannelBar2.top = rectColorChannelBar0.top;
    pRVar5 = pBlueWheelBtn;
    rect.left = rectColorChannelBar2.right + 8;
    rect.top = rectColorChannelBar2.top + -0x1c;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    pRVar5->rect = rect;
    rectUnk0x288.left = rectColorChannelBar0.left;
    rectUnk0x288.top = pBlueWheelBtn->rect.top;
    rectUnk0x288.right = pBlueWheelBtn->rect.right;
    ppSwatch = paColorSwatchBtn;
    rectUnk0x288.bottom = pBlueWheelBtn->rect.bottom;
    rect.left = pRVar1->left + 0x99;
    rect.top = rectMainCanvas.top + 0x57;
    do {
      pRVar5 = *ppSwatch;
      rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
      rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
      pRVar5->rect = rect;
      rect.top = rect.bottom + 1;
      ppSwatch = ppSwatch + 1;
      nSwatchesLeft--;
    } while (nSwatchesLeft != 0);
    rectColorSwatchRow.left = paColorSwatchBtn[0]->rect.left;
    rectColorSwatchRow.top = paColorSwatchBtn[0]->rect.top;
    rectColorSwatchRow.right = paColorSwatchBtn[9]->rect.right;
    rectColorSwatchRow.bottom = paColorSwatchBtn[9]->rect.bottom;
    pRVar5 = pPostBtn;
    rect.bottom = rectMainCanvas.bottom - 3;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.left = pRVar1->left + 0x10;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar2 = &rectSendButton;
    *pRVar2 = rect;
    if (width > 0x320) {
      pRVar2->left = pRVar1->left + -0x32;
    }
    else {
      pRVar2->left = pRVar1->left;
    }
    pRVar5 = pAlbumBtn;
    rectSendButton.top = rectSendButton.top + -10;
    rectSendButton.right = rectSendButton.right + 10;
    rect.top = rectMainCanvas.top + 0x72;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    rect.right = rectMainCanvas.right - 0x25;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    pRVar5 = pMailBtn;
    rect.top = rectMainCanvas.top + 0xb9;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    rect.right = rectMainCanvas.right - 0x25;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    pRVar5 = pDecalHitTestToggleBtn;
    rect.top = rectMainCanvas.top + 0x103;
    rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
    rect.right = rectMainCanvas.right - 0x25;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    pRVar5 = pExitBtn;
    rect.bottom = rectMainCanvas.bottom;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = rectMainCanvas.right - 0x23;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    pRVar5 = pNetworkRosterBtn;
    rect.bottom = rect.top - 10;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    rect.bottom = rect.top - 0xd;
    pRVar5 = pBtnResMaybe_3cbe;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = rectMainCanvas.right - 0x4e;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    pRVar5 = pBtnResMaybe_3cc2;
    rect.left = rect.right + 8;
    rect.bottom = rect.bottom - 7;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    pRVar5->rect = rect;
    pRVar5 = pRandomizeBtnRes;
    rectDecalPickerGrid.right = rectMainCanvas.right + -0x90;
    rectDecalPickerGrid.left = pRVar1->left + 0x90;
    rectDecalPickerGrid.bottom = rectMainCanvas.bottom + -0x3d;
    rectDecalPickerGrid.top = rectDecalPickerGrid.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.bottom = rectDecalPickerGrid.bottom;
    rect.left = rectDecalPickerGrid.left;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = rectDecalPickerGrid.right;
    pRVar5->rect = rect;
    pRVar5 = pDecalScrollForwardBtn;
    rect.bottom = rectMainCanvas.bottom - 3;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = rectDecalPickerGrid.right - 5;
    rect.left = rect.right - pRVar5->pCursorDesc->nativeWidth;
    pRVar5->rect = rect;
    pRVar5 = pDecalScrollBackBtn;
    rect.bottom = pDecalScrollForwardBtn->rect.bottom;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.left = rectDecalPickerGrid.left + 5;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar5 = pDecalKindBtn1;
    rect.bottom = rectMainCanvas.bottom;
    rect.left = rectDecalPickerGrid.left + 0x4d;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar5 = pDecalKindBtn2;
    rect.bottom = pDecalKindBtn1->rect.bottom;
    rect.left = pDecalKindBtn1->rect.left + 0x3d;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar5 = pDecalKindBtn3;
    rect.bottom = pDecalKindBtn2->rect.bottom;
    rect.left = pDecalKindBtn2->rect.left + 0x3d;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar5 = pDecalKindBtn4;
    rect.bottom = pDecalKindBtn3->rect.bottom;
    rect.left = pDecalKindBtn3->rect.left + 0x3d;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar5 = pDecalKindBtn5;
    // sic: the 5th tab takes its bottom edge from tab 3, not tab 4 -- every tab bitmap is the
    // same height so the copy-paste slip is invisible at runtime. Verified against 0x417d3e
    // (`mov ecx,[esi+0x310]` = pDecalKindBtn3) -- not a transcription error.
    rect.bottom = pDecalKindBtn3->rect.bottom;
    rect.left = pDecalKindBtn4->rect.left + 0x3d;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    pRVar5 = pDecalKindBtn6;
    rect.bottom = pDecalKindBtn5->rect.bottom;
    rect.left = pDecalKindBtn5->rect.left + 0x3d;
    rect.top = rect.bottom - pRVar5->pCursorDesc->nativeHeight;
    rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
    pRVar5->rect = rect;
    nRow = 0;
    pRVar5 = pDecalKindBtn1;
    rectDecalKindRow.left = pRVar5->rect.left;
    pRVar6 = pDecalKindBtn6;
    rectDecalKindRow.top = pRVar5->rect.top;
    rectDecalKindRow.right = pRVar6->rect.right;
    rectDecalKindRow.bottom = pRVar6->rect.bottom;
    ppCategoryBtn = paDecalCategoryBtn;
    do {
      iVar8 = 0;
      do {
        pRVar5 = paDecalCategoryBtn[0];
        rect.top = (pRVar5->pCursorDesc->nativeHeight + 5) * nRow + 7 +
                   rectMainCanvas.top;
        rect.left = (pRVar5->pCursorDesc->nativeWidth + 5) * iVar8 + 0x12 + pRVar1->left;
        rect.right = pRVar5->pCursorDesc->nativeWidth + rect.left;
        rect.bottom = pRVar5->pCursorDesc->nativeHeight + rect.top;
        pRVar5 = *ppCategoryBtn;
        ppCategoryBtn = ppCategoryBtn + 1;
        iVar8 = iVar8 + 1;
        pRVar5->rect = rect;
      } while (iVar8 < 2);
      nRow = nRow + 1;
    } while (nRow < 8);
    pRVar5 = paDecalCategoryBtn[0];
    rectDecalCategoryGrid.left = pRVar5->rect.left;
    rectDecalCategoryGrid.top = pRVar5->rect.top;
    pRVar5 = paDecalCategoryBtn[0xf];
    rectDecalCategoryGrid.right = paDecalCategoryBtn[1]->rect.right;
    rectDecalCategoryGrid.bottom = pRVar5->rect.bottom;
  }
}

// FUNCTION: LOCO 0x443000
// The PostBag cache singleton's ctor -- `new PostBagCacheBundle` in AppWindow's bootstrap
// (0x406ba0) dispatches here. Clears the three parallel 256-slot clip-art cache arrays
// (bitmap / LRU access-order / packed key), seeds nCachedCategoryFileCount to its -1
// "uncached" sentinel (see PostBag_GetCategoryFileCountCached, 0x443670) and drops the
// badge descriptor/frame pair, then ensures all ten PostBag directories exist. The
// subdirectory names are the same set PostBag_ScanCategoryCrdFiles switches over.
PostBagCacheBundle::PostBagCacheBundle()
{
    char szPath[260];

    for (int i = 0; i < 256; i++) {
        paBitmapSlots[i] = NULL;
        aSlotAccessOrder[i] = 0;
        aSlotKeys[i].kindHi = 0;
        aSlotKeys[i].kindLo = 0;
        aSlotKeys[i].slotIndex = 0;
    }
    nNextAccessCounter = 0;
    nCachedCategoryFileCount = -1;
    pCachedBadgeTileDesc = NULL;
    pRealizedBadgeFrame = NULL;
    bEasterNameCacheLoaded = 0;

    wsprintfA(szPath, "%s%s", g_pInstallPathPrefix, "PostBag");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s\\Easter", g_pInstallPathPrefix, "PostBag");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Sort");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Sort\\In");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Sort\\Out");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Sort\\Bag");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\AlbIndex");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Album");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Att_In");
    CreateDirectoryA(szPath, NULL);
    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "PostBag", "\\Att_Out");
    CreateDirectoryA(szPath, NULL);
}

#include "DPlaySessionMgr.h" // DPlaySessionMgr/DPlaySessionMgrProviderSlot


extern DPlaySessionMgr *g_pDPlaySessionMgr; // see src/NetSessionEventQueue.cpp

// FUNCTION: LOCO 0x443440
// Teardown loop for g_pPostBagCache's paBitmapSlots array (see src/PostBag.h) -- a
// different region of the shared singleton than PostBag_LoadEasterNameCache's own
// aEasterNames cache. Same call shape (vtable slot 0, scalar-deleting-destructor, arg=1)
// as EditCardWnd::EndActiveSession's own paDecalThumbCache LocoBitmap* deletion loop --
// element type hypothesized as LocoBitmap* by that analogy, not independently confirmed.
void __fastcall PostBag_ReleaseCachedBitmaps(PostBagCacheBundle *pCache)
{
    LocoBitmap **p = pCache->paBitmapSlots;
    for (int i = 0x100; i != 0; i--) {
        if (*p != NULL) {
            delete *p;
            *p = NULL;
        }
        p++;
    }
}

extern int g_nEasterLocaleId; // DAT_004a97a0

// FUNCTION: LOCO 0x443260
// Lazily loads a 16-entry x 13-byte easter-name cache (g_pPostBagCache's aEasterNames,
// latched via bEasterNameCacheLoaded -- see src/PostBag.h) from
// "<install>PostBag\Easter\<Lang>\easter.usr", where <Lang>
// is picked by g_nEasterLocaleId (0-9, jump table at 0x443410, recovered via a raw
// VA->file-offset read per CLAUDE.md's GUID/double-constant recipe) against the 9-language
// folder-suffix table at 0x47ebf8 (unlisted values 0/3 and out-of-range both fall to the
// default "\Easter\Eng" arm, matching the jump table's own address reuse). Each of up to 16
// newline/CR-delimited lines (max 12 chars) is copied into its slot. The read buffer's first
// byte comes from the pooled empty literal and the rest is zeroed -- the ordinary `= ""`
// aggregate initializer this TU uses everywhere -- and that is load-bearing, since the trailing CR/LF-skip loop
// below has no bounds check and relies on hitting a zero byte past a short read. Ghidra's own
// decompile mishandles the jump table (spurious "unreachable block" warnings on every case) --
// ground-truthed via raw disasm + a python struct.unpack of the table bytes, not decompile text.
//
// PARKED (DIFF 173/429): two real, triaged-but-unclosed structural residuals, both probed
// within budget without success (each fix attempt below was tried in isolation and reverted --
// none improved the diff, two made it measurably worse):
//   1. Switch dispatch table base: the original emits a flat 0-9 (10-entry) jump table with NO
//      index adjustment (`cmp eax,9; ja default; jmp [eax*4+table]`). Ours instead compiles to
//      a 1-9 (9-entry) table with a `lea eax,[ecx-1]; cmp eax,8` normalization -- i.e. /O2
//      decided the "dense" range worth tabling is 1-9, not 0-9, even after adding explicit
//      `case 0: case 3:` labels sharing the default arm (tried, zero effect on the compiled
//      bytes -- the extra labels don't change the table strategy since they carry no unique
//      code). Whatever makes the original treat 0 as table-worthy isn't visible from this
//      surface syntax.
//   2. Null-terminate addressing: after the char-copy loop, the original recomputes the
//      terminator's address via `this + charsCopied` (an extra "mov edx,this; add ecx,edx"
//      pair) instead of reusing the same per-entry base pointer the copy loop just used --
//      i.e. it does NOT keep a single cached name-slot pointer live across the "extra scan"
//      sub-loop, even though the copy loop itself clearly does use one (Yoda lesson #19
//      territory, but the expected fix -- matching "no cached local, always reload" -- made
//      the diff bigger, not smaller, when tried both as a direct offset recompute and as an
//      incrementally-advanced (non-recomputed) per-entry pointer). Ruled out TU-position
//      noise: a minimal-TU probe (this function + a bare extern-only header, no rest of
//      EditCardWnd.cpp) reproduced the IDENTICAL score (total=188304) -- confirmed INTRINSIC
//      to this function's own body, not cross-function register-allocation carry (Yoda #7).
//      No further lever identified; would need the function's real neighbor TU (unknown --
//      likely a PostBag.cpp this project hasn't mapped yet) to test true same-TU context.
void __fastcall PostBag_LoadEasterNameCache(PostBagCacheBundle *pCache)
{
    char *pFlag = (char *)&pCache->bEasterNameCacheLoaded;
    if (*pFlag == 0) {
        char szReadBuf[0x2000] = "";

        for (int i = 0; i < 16; i++) {
            pCache->aEasterNames[i][0] = 0;
        }

        const char *pszLangSuffix;
        switch (g_nEasterLocaleId) {
        case 1: pszLangSuffix = "\\Easter\\Dan"; break;
        case 2: pszLangSuffix = "\\Easter\\Dut"; break;
        case 4: pszLangSuffix = "\\Easter\\Fre"; break;
        case 5: pszLangSuffix = "\\Easter\\Ger"; break;
        case 6: pszLangSuffix = "\\Easter\\Ita"; break;
        case 7: pszLangSuffix = "\\Easter\\Nor"; break;
        case 8: pszLangSuffix = "\\Easter\\Spa"; break;
        case 9: pszLangSuffix = "\\Easter\\Swe"; break;
        case 0:
        case 3:
        default: pszLangSuffix = "\\Easter\\Eng"; break;
        }

        char szPath[0x504];
        wsprintfA(szPath, "%s%s%s\\easter.usr", g_pInstallPathPrefix, "PostBag", pszLangSuffix);

        HANDLE hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hFile != (HANDLE)-1) {
            DWORD dwBytesRead;
            if (ReadFile(hFile, szReadBuf, sizeof(szReadBuf), &dwBytesRead, NULL)) {
                int i2 = 0;
                // NB: flat byte-offset walk, NOT `aEasterNames[i]` -- unlike the sibling loop in
                // BuildPlayerRosterList (0x416e00), which is a row subscript. The two forms are
                // distinguishable (see docs/CODEGEN.md "SIB base/index role"); the subscript
                // form costs 90 extra diffs and 12 bytes here, so the original really did write
                // these two loops over the same array differently.
                for (int nameOff = 0; nameOff < 0xd0; nameOff += 0xd) {
                    char *pName = (char *)pCache->aEasterNames + nameOff;
                    int nChars = 0;
                    while (i2 < (int)dwBytesRead) {
                        char c = szReadBuf[i2];
                        if (c == '\r') {
                            i2++;
                            break;
                        }
                        pName[nChars] = c;
                        nChars++;
                        i2++;
                        if (nChars >= 12) {
                            break;
                        }
                    }
                    if (nChars >= 12) {
                        while (szReadBuf[i2] != '\n') {
                            if (i2 >= (int)dwBytesRead) {
                                break;
                            }
                            i2++;
                        }
                    }
                    pName[nChars] = 0;
                    while (szReadBuf[i2] == '\n' || szReadBuf[i2] == '\r') {
                        i2++;
                    }
                }
            }
            CloseHandle(hFile);
        }
        *pFlag = 1;
    }
}

// FUNCTION: LOCO 0x443670
// Cached wrapper around PostBag_ScanCategoryCrdFiles(2, 0) (category 2 = "Sort Out";
// param2=0 -- see src/PostBag.h) -- counts and frees the returned .crd file list, caching the
// count in nCachedCategoryFileCount (-1 = uncached). The scan's own list nodes are freed
// with a plain operator delete, not any per-node destructor -- they're raw new_alloc(0x508)
// path buffers, not real objects.
short PostBagCacheBundle::PostBag_GetCategoryFileCountCached()
{
    if (nCachedCategoryFileCount >= 0) {
        return nCachedCategoryFileCount;
    }

    short nCount = 0;
    PostBagCrdFileNode *pNode = PostBag_ScanCategoryCrdFiles(2, 0);
    if (pNode != NULL) {
        do {
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            nCount++;
            ::operator delete(pTemp);
        } while (pNode != NULL);
    }
    nCachedCategoryFileCount = nCount;
    return nCount;
}

// FUNCTION: LOCO 0x445170
// Uncached recount of category-2 (Sort Out) .crd files -- same scan+free+tally idiom as
// PostBag_GetCategoryFileCountCached just above, but it never touches the cache field.
// Some callers (e.g. LocalPlayerIdentity::SetNameMaybe) discard the return value, using
// the call purely for its node-list side effect after a client-identity change.
//
// DEFINED as the PostBagCacheBundle member it really is (v412), not the free-shaped twin
// v362 left here: all four call sites in the binary load `mov ecx,[g_pPostBagCache]`. The
// body never reads `this`, so the member spelling is byte-identical -- and it lets the
// inner PostBag_ScanCategoryCrdFiles call resolve to that CLASS's member declaration
// (ecx still live from the prologue, so MSVC emits nothing for it), which is what finally
// retires this header's duplicate free __stdcall declaration of 0x4446f0.
short PostBagCacheBundle::PostBag_RecountCategoryOutFiles()
{
    short nCount = 0;
    PostBagCrdFileNode *pNode = PostBag_ScanCategoryCrdFiles(2, 0);
    while (pNode != NULL) {
        PostBagCrdFileNode *pTemp = pNode;
        pNode = pNode->pNext;
        nCount++;
        ::operator delete(pTemp);
    }
    return nCount;
}

// FUNCTION: LOCO 0x444fb0
// Deletes pszPath, then (only if that succeeded) refreshes nCachedCategoryFileCount by
// rescanning category 2 (Sort Out) -- same scan+free+tally idiom as
// PostBag_GetCategoryFileCountCached. A no-op (cache untouched) if the delete fails.
//
// The original's `mov ecx,ebx` before the PostBag_ScanCategoryCrdFiles call is NOT the dead
// register reload it was autopsied as from v100 to v361 -- it is a real `this` pass. That callee
// is a __thiscall PostBagCacheBundle member that never reads `this` (see src/PostBag.h, which
// already models three siblings the same way); declaring it as a member so this call site becomes
// an implicit `this->` call closed the function. `this` needs reloading here specifically because
// DeleteFileA clobbered ecx -- at the class's two other call sites (0x443686, 0x445177) it is
// still live from the prologue and MSVC correctly emits nothing, which is why they matched even
// while the callee was modelled as a free __stdcall function.
void PostBagCacheBundle::DeleteCardFileAndRefreshCount(const char *pszPath)
{
    if (!DeleteFileA(pszPath)) {
        return;
    }

    short nCount = 0;
    PostBagCrdFileNode *pNode = PostBag_ScanCategoryCrdFiles(2, 0);
    while (pNode != NULL) {
        PostBagCrdFileNode *pTemp = pNode;
        pNode = pNode->pNext;
        nCount++;
        ::operator delete(pTemp);
    }
    nCachedCategoryFileCount = nCount;
}

// FUNCTION: LOCO 0x4442b0
// Get-or-load for the 256-slot LRU clip-art bitmap cache (g_pPostBagCache's
// paBitmapSlots/aSlotKeys/aSlotAccessOrder -- see src/PostBag.h). On a cache hit,
// bumps the slot's LRU stamp and returns a FRESH COPY (LocoBitmap's copy ctor) of the cached
// bitmap -- the cache keeps its own copy too, never hands out its own instance. On a miss,
// builds "<install>Clipart\<pattern>.bmp" (pattern keyed off kindHi/kindLo/slotIndex -- 6
// pattern families by kindHi range, see the switch below), loads it, and on success either
// inserts a COPY into the first free slot, or -- if the cache is full and bSuppressEvictMaybe==0
// (a request to evict is signalled by a FALSE/0 byte, not true) -- evicts the globally
// least-recently-used slot (lowest aSlotAccessOrder) and inserts there. The caller's own
// freshly-loaded bitmap is returned regardless of whether the cache-side insert/evict copy
// itself succeeds (a failed cache-copy alloc just leaves that slot null, sic).
LocoBitmap *PostBagCacheBundle::ClipartBitmapCache_GetOrLoad(unsigned char kindHi, unsigned char kindLo, unsigned char slotIndex, char bSuppressEvictMaybe)
{
    char szFullPath[0x504] = "";

    char szFileName[64] = "";

    for (int i = 0; i < 256; i++) {
        if (paBitmapSlots[i] != NULL &&
            aSlotKeys[i].kindHi == kindHi &&
            aSlotKeys[i].kindLo == kindLo &&
            aSlotKeys[i].slotIndex == slotIndex) {
            aSlotAccessOrder[i] = nNextAccessCounter;
            return new LocoBitmap(paBitmapSlots[i]);
        }
    }

    switch (kindHi) {
    case 31:
        wsprintfA(szFileName, "S0%03d.bmp", slotIndex);
        break;
    case 30:
        wsprintfA(szFileName, "R%01d%03d.bmp", kindLo - 1, slotIndex);
        break;
    case 16: case 17: case 18: case 19: case 20:
    case 21: case 22: case 23: case 24: case 25:
        wsprintfA(szFileName, "%c%01d%03d.bmp", (unsigned char)(kindHi + 0x58), kindLo - 1, slotIndex);
        break;
    case 26: case 27: case 28: case 29:
        wsprintfA(szFileName, "%c%01d%03d.bmp", (unsigned char)(kindHi + 0x5a), kindLo - 1, slotIndex);
        break;
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15:
        wsprintfA(szFileName, "%01x%01d%03d.bmp", kindHi, kindLo - 1, slotIndex);
        break;
    default:
        wsprintfA(szFileName, "0%01d%03d.bmp", kindLo - 1, slotIndex);
        break;
    }

    wsprintfA(szFullPath, "%sClipart\\%s", g_pInstallPathPrefix, szFileName);

    LocoBitmap *pBitmap = new LocoBitmap();
    pBitmap->Load(szFullPath, 0, 0, 0);

    if (pBitmap->pPixels == NULL && pBitmap->pSurface == NULL) {
        delete pBitmap;
        return NULL;
    }

    for (int i2 = 0; i2 < 256; i2++) {
        if (paBitmapSlots[i2] == NULL) {
            aSlotKeys[i2].kindHi = kindHi;
            aSlotKeys[i2].kindLo = kindLo;
            aSlotKeys[i2].slotIndex = slotIndex;
            aSlotAccessOrder[i2] = nNextAccessCounter;
            paBitmapSlots[i2] = new LocoBitmap(pBitmap);
            return pBitmap;
        }
    }

    if (bSuppressEvictMaybe != 0) {
        return pBitmap;
    }

    unsigned int nMinOrder = aSlotAccessOrder[0];
    int nEvictIdx = 0;
    for (int i3 = 1; i3 < 256; i3++) {
        if ((int)aSlotAccessOrder[i3] < (int)nMinOrder) {
            nEvictIdx = i3;
            nMinOrder = aSlotAccessOrder[i3];
        }
    }
    if (paBitmapSlots[nEvictIdx] != NULL) {
        delete paBitmapSlots[nEvictIdx];
    }
    paBitmapSlots[nEvictIdx] = NULL;
    aSlotKeys[nEvictIdx].kindHi = kindHi;
    aSlotKeys[nEvictIdx].kindLo = kindLo;
    aSlotKeys[nEvictIdx].slotIndex = slotIndex;
    aSlotAccessOrder[nEvictIdx] = nNextAccessCounter;
    paBitmapSlots[nEvictIdx] = new LocoBitmap(pBitmap);
    return pBitmap;
}

// FUNCTION: LOCO 0x416e00
// Builds the roster-label table (aRosterNames, 26 x 13-byte slots): in network mode (connection
// mode 2), walks DPlaySessionMgr's 9 provider slots, skipping the local player's own slot
// (selectedProviderIndex) and any slot with no providerId, copying each remaining
// slot's name; else falls back to one localized default label (string id 0x6e). Either way,
// nRosterCount is latched, then PostBag_LoadEasterNameCache lazily loads the shared
// "easter.usr" name cache (real body above, ex-FUN_00443260)
// and every non-empty entry in it is unconditionally appended (capped at 26 total), with any
// remaining unused slots zero-filled.
//
// The easter-name loop MUST be written as a 2-D ROW SUBSCRIPT (`aEasterNames[i2]`, i2 counting
// rows 0..15) and NOT as flat byte arithmetic (`(char *)aEasterNames + i2`, i2 += 0xd). Both
// emit the identical instruction stream -- MSVC strength-reduces the row subscript to the same
// stride-13 induction variable -- but they pick OPPOSITE SIB base/index roles for
// `[ptr + idx + 0xb13]`: the subscript form makes the ARRAY POINTER the base (what the original
// has), the pointer-arithmetic form makes the OFFSET the base. That one encoding bit was this
// function's entire residual for ~40 sessions. See docs/CODEGEN.md "SIB base/index role".
void EditCardWnd::BuildPlayerRosterList() {
    char szDefaultLabel[13] = "";
    g_UIResources.LoadLocaleString(0x6e, szDefaultLabel, sizeof(szDefaultLabel));

    unsigned int nCount = 0;
    if (g_pDPlaySessionMgr->connectionMode == 2) {
        for (int i = 0; i < 9; i++) {
            if (i != g_pDPlaySessionMgr->selectedProviderIndex &&
                g_pDPlaySessionMgr->aProviderSlots[i].providerId != 0) {
                strcpy(aRosterNames[nCount], g_pDPlaySessionMgr->aProviderSlots[i].sAddressOrName);
                nCount++;
            }
        }
    } else {
        strcpy(aRosterNames[0], szDefaultLabel);
        nCount = 1;
    }
    nRosterCount = nCount;

    PostBag_LoadEasterNameCache(g_pPostBagCache);
    for (int i2 = 0; i2 < 16; i2++) {
        char *pName = g_pPostBagCache->aEasterNames[i2];
        if (*pName != '\0') {
            strcpy(aRosterNames[nCount], pName);
            nCount++;
        }
    }
    if ((int)nCount < 0x1a) {
        for (int i3 = nCount; i3 < 0x1a; i3++) {
            aRosterNames[i3][0] = '\0';
        }
    }
}

// FUNCTION: LOCO 0x4446f0
// Scans a PostBag category directory for ".crd" postcard files matching the local player's
// client id (or, when pszSubDir != NULL, a named subdirectory + client id -- used by the
// "Design" category to scope a scan to one design's own subfolder). Returns a singly-linked
// list of PostBagCrdFileNode nodes (src/PostBag.h), one per file whose first 2 bytes equal the
// 0x66 ".crd" signature -- non-matching files are silently skipped (their scratch node freed).
// Ground-truthed against the raw disasm, not Ghidra's own decompile text (four separate
// %s-substitution jump tables, one per wsprintfA call, each independently confirmed via a
// VA->file-offset dword read -- CLAUDE.md's switch-jump-table recipe): the outer category
// switch's SOURCE declaration order is 0,1,2,3,6,4,5,7 (case 6, the language sub-switch, is
// declared between case 3 and case 4) -- NOT numeric order -- and the inner language sub-switch
// reuses the exact same order already established in PostBag_LoadEasterNameCache above
// (1,2,4,5,6,7,8,9,default(0,3)=Eng). _findfirst/_findnext/_findclose enumeration idiom is the
// same one EnumerateFiles (WidgetPickerObj0x477cc8's own method, src/WidgetPicker.cpp)
// established.
//
// PARKED (EFFECTIVE, asmscore total 863318, insns 445 orig/305+jump-table-tail compiled): two
// real structural fixes closed the bulk of a much larger initial gap (score 928127 with both
// bugs present):
//   1. nClientId (g_pLocalPlayerIdentity->clientId) must be loaded ONCE, hoisted
//      above the if/else, into a local -- the raw disasm loads it unconditionally right at
//      function entry (before the pszSubDir test), even though it's only consumed inside
//      each branch's first wsprintfA call. Re-reading the field separately in each branch (the
//      naive transcription) doubled the load and showed as two "missing at top" diffs.
//   2. The if/else branch order matters for which branch becomes the fall-through: writing
//      `if (pszSubDir != NULL) {...} else {...}` (NOT `== NULL`) reproduces the original's
//      own physical layout, where the pszSubDir!=NULL body is the fall-through and the
//      ==NULL body is the forward jump target. The opposite source order compiled with the
//      pszSubDir argument silently DROPPED from one wsprintfA call's push sequence (6
//      pushes/0x18 stack cleanup instead of the required 7/0x1c) -- not a cosmetic reorder, a
//      real generated-code bug from the wrong branch being on the fall-through path.
// Remaining residual is the already-documented class from PostBag_LoadEasterNameCache's
// own PARKED note just above: the g_nEasterLocaleId language sub-switch's dispatch-table
// shape (original's flat unadjusted 0-9 table vs. this compile's 1-9 `lea eax,[x-1]`-normalized
// table), reproduced here independently 3 times (once per language-switch instance in this
// function) with the same untried-lever status -- plus pervasive register-allocation "r"
// tie-breaks spanning nearly the whole function body (Yoda lesson #29/#30's intrinsic
// symmetric-register-swap class). No further lever identified within budget.
PostBagCrdFileNode *PostBagCacheBundle::PostBag_ScanCategoryCrdFiles(int nCategory, const char *pszSubDir)
{
    char szSearchPattern[0x504] = "";

    char szDirPrefix[0x504] = "";

    DWORD dwRead = 0;
    PostBagCrdFileNode *pHead = NULL;
    unsigned int nClientId = g_pLocalPlayerIdentity->clientId;

    if (pszSubDir != NULL) {
        const char *pszCategory = NULL;
        switch (nCategory) {
        case 0: pszCategory = "\\Album"; break;
        case 1: pszCategory = "\\Sort\\In"; break;
        case 2: pszCategory = "\\Sort\\Out"; break;
        case 3: pszCategory = "\\Sort\\Bag"; break;
        case 6:
            switch (g_nEasterLocaleId) {
            case 1: pszCategory = "\\Easter\\Dan"; break;
            case 2: pszCategory = "\\Easter\\Dut"; break;
            case 4: pszCategory = "\\Easter\\Fre"; break;
            case 5: pszCategory = "\\Easter\\Ger"; break;
            case 6: pszCategory = "\\Easter\\Ita"; break;
            case 7: pszCategory = "\\Easter\\Nor"; break;
            case 8: pszCategory = "\\Easter\\Spa"; break;
            case 9: pszCategory = "\\Easter\\Swe"; break;
            case 0:
            case 3:
            default: pszCategory = "\\Easter\\Eng"; break;
            }
            break;
        case 4: pszCategory = "\\Att_Out"; break;
        case 5: pszCategory = "\\Att_In"; break;
        case 7: pszCategory = "\\Design"; break;
        }
        wsprintfA(szSearchPattern, "%s%s%s\\%s\\%03d*.crd", g_pInstallPathPrefix, "PostBag", pszCategory,
                  pszSubDir, nClientId);

        const char *pszCategory2 = NULL;
        switch (nCategory) {
        case 0: pszCategory2 = "\\Album"; break;
        case 1: pszCategory2 = "\\Sort\\In"; break;
        case 2: pszCategory2 = "\\Sort\\Out"; break;
        case 3: pszCategory2 = "\\Sort\\Bag"; break;
        case 6:
            switch (g_nEasterLocaleId) {
            case 1: pszCategory2 = "\\Easter\\Dan"; break;
            case 2: pszCategory2 = "\\Easter\\Dut"; break;
            case 4: pszCategory2 = "\\Easter\\Fre"; break;
            case 5: pszCategory2 = "\\Easter\\Ger"; break;
            case 6: pszCategory2 = "\\Easter\\Ita"; break;
            case 7: pszCategory2 = "\\Easter\\Nor"; break;
            case 8: pszCategory2 = "\\Easter\\Spa"; break;
            case 9: pszCategory2 = "\\Easter\\Swe"; break;
            case 0:
            case 3:
            default: pszCategory2 = "\\Easter\\Eng"; break;
            }
            break;
        case 4: pszCategory2 = "\\Att_Out"; break;
        case 5: pszCategory2 = "\\Att_In"; break;
        case 7: pszCategory2 = "\\Design"; break;
        }
        wsprintfA(szDirPrefix, "%s%s%s\\%s\\", g_pInstallPathPrefix, "PostBag", pszCategory2, pszSubDir);
    } else {
        const char *pszCategory = NULL;
        switch (nCategory) {
        case 0: pszCategory = "\\Album"; break;
        case 1: pszCategory = "\\Sort\\In"; break;
        case 2: pszCategory = "\\Sort\\Out"; break;
        case 3: pszCategory = "\\Sort\\Bag"; break;
        case 6:
            switch (g_nEasterLocaleId) {
            case 1: pszCategory = "\\Easter\\Dan"; break;
            case 2: pszCategory = "\\Easter\\Dut"; break;
            case 4: pszCategory = "\\Easter\\Fre"; break;
            case 5: pszCategory = "\\Easter\\Ger"; break;
            case 6: pszCategory = "\\Easter\\Ita"; break;
            case 7: pszCategory = "\\Easter\\Nor"; break;
            case 8: pszCategory = "\\Easter\\Spa"; break;
            case 9: pszCategory = "\\Easter\\Swe"; break;
            case 0:
            case 3:
            default: pszCategory = "\\Easter\\Eng"; break;
            }
            break;
        case 4: pszCategory = "\\Att_Out"; break;
        case 5: pszCategory = "\\Att_In"; break;
        case 7: pszCategory = "\\Design"; break;
        }
        wsprintfA(szSearchPattern, "%s%s%s\\%03d*.crd", g_pInstallPathPrefix, "PostBag", pszCategory,
                  nClientId);

        const char *pszCategory2 = NULL;
        switch (nCategory) {
        case 0: pszCategory2 = "\\Album"; break;
        case 1: pszCategory2 = "\\Sort\\In"; break;
        case 2: pszCategory2 = "\\Sort\\Out"; break;
        case 3: pszCategory2 = "\\Sort\\Bag"; break;
        case 6:
            switch (g_nEasterLocaleId) {
            case 1: pszCategory2 = "\\Easter\\Dan"; break;
            case 2: pszCategory2 = "\\Easter\\Dut"; break;
            case 4: pszCategory2 = "\\Easter\\Fre"; break;
            case 5: pszCategory2 = "\\Easter\\Ger"; break;
            case 6: pszCategory2 = "\\Easter\\Ita"; break;
            case 7: pszCategory2 = "\\Easter\\Nor"; break;
            case 8: pszCategory2 = "\\Easter\\Spa"; break;
            case 9: pszCategory2 = "\\Easter\\Swe"; break;
            case 0:
            case 3:
            default: pszCategory2 = "\\Easter\\Eng"; break;
            }
            break;
        case 4: pszCategory2 = "\\Att_Out"; break;
        case 5: pszCategory2 = "\\Att_In"; break;
        case 7: pszCategory2 = "\\Design"; break;
        }
        wsprintfA(szDirPrefix, "%s%s%s\\", g_pInstallPathPrefix, "PostBag", pszCategory2);
    }

    struct _finddata_t findData;
    long hFind = _findfirst(szSearchPattern, &findData);
    if (hFind != -1) {
        short wSig;
        do {
            if (findData.name[0] != '.') {
                PostBagCrdFileNode *pNode = (PostBagCrdFileNode *)::operator new(sizeof(PostBagCrdFileNode));
                pNode->szPath[0] = 0;
                pNode->pNext = NULL;
                wsprintfA(pNode->szPath, "%s%s", szDirPrefix, findData.name);

                HANDLE hFile = CreateFileA(pNode->szPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                if (hFile == (HANDLE)-1 || !ReadFile(hFile, &wSig, 2, &dwRead, NULL)) {
                    wSig = 0;
                }
                if (dwRead != 2) {
                    wSig = 0;
                }
                CloseHandle(hFile);

                if (wSig == 0x66) {
                    pNode->pNext = pHead;
                    pHead = pNode;
                } else {
                    ::operator delete(pNode);
                }
            }
        } while (_findnext(hFind, &findData) == 0);
        _findclose(hFind);
    }

    return pHead;
}

// FUNCTION: LOCO 0x448390
// Scans "<install>SaveGame\*.sav" (bScrSaver==0) or "<install>ScrSaver\*.sav" (bScrSaver!=0 --
// sic: the ScrSaver-directory format string still ends in ".sav", not ".scr", despite the
// directory name) via the same _findfirst/_findnext/_findclose enumeration idiom as
// PostBag_ScanCategoryCrdFiles/WidgetPickerObj0x477cc8::EnumerateFiles. Builds a
// singly-linked list of PostBagCrdFileNode nodes (same 0x504-name+0x504-next-ptr shape,
// new_alloc(0x508) per node), one per non-dotfile match -- unlike
// PostBag_ScanCategoryCrdFiles, no directory prefix is concatenated, each node's
// szPath holds the bare filename only. List order is prepend (last-enumerated file ends
// up first). Sole confirmed caller (ScreenSaver::GetLayoutFileName/0x4481b0) passes
// bScrSaver=1 to pick a random screensaver layout file when "[ScreenSaver] Random" is set in
// lego.ini.
//
// ⚠ This is a `this`-ignoring MEMBER of ScreenSaver (declared in src/ScreenSaver.h), not the
// free __stdcall it was modelled as through v366 -- its caller loads ecx = this. The BODY stays
// in THIS TU because it is already EXACT here and relocating it would rotate this TU's /Og
// state; __thiscall and __stdcall generate the identical epilogue (`ret 4`) for a one-argument
// function whose `this` is never read, so the convention change costs nothing here.
//
// EXACT MATCH (len 266/266). The unused szDirPrefix buffer is genuinely dead in the original
// too (zeroed via memset, never read) -- /O2 only eliminates the paired `buf[0] = <magic>;`
// literal-byte copy as dead, not the zero-fill itself, matching every OTHER `char buf[N] = "";`
// in this TU. The wsprintfA format-string
// selection had to be two separate `if (bScrSaver) {call A} else {call B}` statements, NOT a
// `pszFmt = cond ? A : B; wsprintfA(dest, pszFmt, arg);` local -- the original hoists the
// shared/invariant vararg (g_pInstallPathPrefix, a compile-time-constant array address) to
// ONE push before the branch, then cross-jumps both arms' fmt-string+buffer pushes into one
// shared call site (Yoda lesson #18's cross-jump-with-per-arm-pushes shape); the ternary-local
// form pushes the vararg AFTER computing the selected fmt pointer instead, wrong order.
PostBagCrdFileNode *ScreenSaver::SaveGame_ScanSavFiles(char bScrSaver)
{
    char szSearchPattern[0x504] = "";

    char szDirPrefix[0x504] = "";

    PostBagCrdFileNode *pHead = NULL;

    if (bScrSaver) {
        wsprintfA(szSearchPattern, "%sScrSaver\\*.sav", g_pInstallPathPrefix);
    } else {
        wsprintfA(szSearchPattern, "%sSaveGame\\*.sav", g_pInstallPathPrefix);
    }

    struct _finddata_t findData;
    long hFind = _findfirst(szSearchPattern, &findData);
    if (hFind != -1) {
        do {
            if (findData.name[0] != '.') {
                PostBagCrdFileNode *pNode = (PostBagCrdFileNode *)::operator new(sizeof(PostBagCrdFileNode));
                pNode->szPath[0] = 0;
                pNode->pNext = NULL;
                strcpy(pNode->szPath, findData.name);
                pNode->pNext = pHead;
                pHead = pNode;
            }
        } while (_findnext(hFind, &findData) == 0);
        _findclose(hFind);
    }

    return pHead;
}

// FUNCTION: LOCO 0x444d00
// Builds one of 8 PostBag category folder paths (same category-name/language-locale switch
// idiom as PostBag_ScanCategoryCrdFiles above -- source declaration order 0,1,2,3,6,4,5,7,
// confirmed via the same raw VA->file-offset jump-table read), saves pCard into that folder
// (CarNetState::SaveCardFile), refreshes the cached Sort\Out file count (always
// rescans category 2 regardless of nCategory -- sic, matches
// PostBag_GetCategoryFileCountCached's own hardcoded category 2), and for nCategory==0
// (Album) also inserts a PostBagFileCache index entry (FUN_00401850, not yet transcribed).
//
// CORRECTED 2026-07-17: NOT a folder-ensure/init function despite its ex-name
// (PostBag_InitCategoryFolderMaybe) -- it never creates a directory itself;
// CarNetState::SaveCardFile just CreateFileA/WriteFiles straight into it. See
// docs/subsystems.md's PostBag family entry.
//
// PARKED (asmscore total 557207, insns 219/155, DIFF 243/656): content-complete, two residual
// classes (see docs/PARKED.md for the full autopsy): (1) the already-documented flat-0-9-vs-
// normalized-1-9 easter-locale jump-table shape (same as PostBag_LoadEasterNameCache/
// PostBag_ScanCategoryCrdFiles above). (2) the `if (!pCard->SaveCardFile(...)) return
// 0;` guard's fall-through direction is inverted from the original (original falls through on
// SUCCESS with the failure tail placed at the function's end; this compiles with failure as the
// fall-through instead) -- 3 source phrasings tried, byte-identical to each other, confirmed
// not steerable from this surface syntax. Unlike its siblings, a minimal-TU probe scored WORSE
// than the real-TU compile, so this is NOT simply TU-position noise -- an open question if this
// cluster ever gets its own mapped TU.
unsigned char PostBagCacheBundle::PostBag_SaveCardToCategory(CarNetState *pCard, int nCategory,
                                                                  char *pszSubDir)
{
    char szDir[0x504];

    if (pszSubDir != NULL) {
        const char *pszCategory = NULL;
        switch (nCategory) {
        case 0: pszCategory = "\\Album"; break;
        case 1: pszCategory = "\\Sort\\In"; break;
        case 2: pszCategory = "\\Sort\\Out"; break;
        case 3: pszCategory = "\\Sort\\Bag"; break;
        case 6:
            switch (g_nEasterLocaleId) {
            case 1: pszCategory = "\\Easter\\Dan"; break;
            case 2: pszCategory = "\\Easter\\Dut"; break;
            case 4: pszCategory = "\\Easter\\Fre"; break;
            case 5: pszCategory = "\\Easter\\Ger"; break;
            case 6: pszCategory = "\\Easter\\Ita"; break;
            case 7: pszCategory = "\\Easter\\Nor"; break;
            case 8: pszCategory = "\\Easter\\Spa"; break;
            case 9: pszCategory = "\\Easter\\Swe"; break;
            case 0:
            case 3:
            default: pszCategory = "\\Easter\\Eng"; break;
            }
            break;
        case 4: pszCategory = "\\Att_Out"; break;
        case 5: pszCategory = "\\Att_In"; break;
        case 7: pszCategory = "\\Design"; break;
        }
        wsprintfA(szDir, "%s%s%s\\%s", g_pInstallPathPrefix, "PostBag", pszCategory, pszSubDir);
    } else {
        const char *pszCategory = NULL;
        switch (nCategory) {
        case 0: pszCategory = "\\Album"; break;
        case 1: pszCategory = "\\Sort\\In"; break;
        case 2: pszCategory = "\\Sort\\Out"; break;
        case 3: pszCategory = "\\Sort\\Bag"; break;
        case 6:
            switch (g_nEasterLocaleId) {
            case 1: pszCategory = "\\Easter\\Dan"; break;
            case 2: pszCategory = "\\Easter\\Dut"; break;
            case 4: pszCategory = "\\Easter\\Fre"; break;
            case 5: pszCategory = "\\Easter\\Ger"; break;
            case 6: pszCategory = "\\Easter\\Ita"; break;
            case 7: pszCategory = "\\Easter\\Nor"; break;
            case 8: pszCategory = "\\Easter\\Spa"; break;
            case 9: pszCategory = "\\Easter\\Swe"; break;
            case 0:
            case 3:
            default: pszCategory = "\\Easter\\Eng"; break;
            }
            break;
        case 4: pszCategory = "\\Att_Out"; break;
        case 5: pszCategory = "\\Att_In"; break;
        case 7: pszCategory = "\\Design"; break;
        }
        wsprintfA(szDir, "%s%s%s", g_pInstallPathPrefix, "PostBag", pszCategory);
    }

    if (!pCard->SaveCardFile(szDir)) {
        return 0;
    }

    short nCount = 0;
    PostBagCrdFileNode *pNode = PostBag_ScanCategoryCrdFiles(2, 0);
    if (pNode != NULL) {
        do {
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            nCount++;
            ::operator delete(pTemp);
        } while (pNode != NULL);
    }
    nCachedCategoryFileCount = nCount;

    if (nCategory == 0) {
        g_pPostBagFileCache->IndexCard(pCard);
    }
    return 1;
}

// FUNCTION: LOCO 0x401850
// Builds a 24-byte {szName[20], nId} record from pCard's name/id fields, picks the
// alphabetic bucket (0-8) from the uppercased first letter of the name, loads that bucket's
// index file (LoadIndexedFile -- a no-op if it's already the currently-loaded bucket),
// finds the sorted insertion position via a linear scan comparing against each record's own
// name (an inlined strcmp idiom, same intrinsic-substitution family as ActivateTab's own
// sorted-position search -- see CLAUDE.md), and inserts (InsertRecord). Only called by
// PostBagCacheBundle::PostBag_SaveCardToCategory for Album saves (nCategory==0).
//
// PARKED (v151, asmscore.py --len 516 total 344794, insns 222/198): content-complete and
// structurally correct -- the sorted-insert search really does have TWO differently-shaped
// loops (the >0 branch is a do-while incrementing before its bound check; the ==0 branch is a
// while whose bound check uses the PRE-increment index, reproduced here via the
// `pRecords[nPos++]` post-increment-in-subscript idiom -- confirmed via careful disasm
// trace that the original's own insertion position ends up ONE PAST the last exact-duplicate
// name, not at the first non-equal entry, i.e. it's a genuine off-by-one quirk in the ORIGINAL,
// faithfully reproduced here, not a bug in this transcription). Residual: (1) `this` is
// spilled to a stack slot and reloaded once near the top of the original, vs. staying
// register-resident in this compile (likely just the callee-saved-register save forced by the
// intervening `LoadIndexedFile` call, not source-steerable); (2) both loops' original
// bodies share their bound-check/loop-back code as ONE fallthrough block (entered once, looped
// via a single conditional jump back into the middle of that same block), while this compile
// always emits a separate small "increment + advance + bound-recheck" tail after the main
// compare -- tried folding the bound check into the loop condition (removing an explicit
// `break`) which measurably helped (DIFF 314->298) but didn't fully close it. Same
// "no mapped PostBag.cpp TU yet" cluster as its siblings above -- see docs/subsystems.md.
void PostBagFileCache::IndexCard(CarNetState *pCard)
{
    PostBagAlbumIndexRecord rec;
    strcpy(rec.szName, pCard->nameB);
    rec.nId = pCard->nPostSeqId;

    char szNameUpperMaybe[20];
    strcpy(szNameUpperMaybe, pCard->nameB);
    _strupr(szNameUpperMaybe);

    int nBucket;
    switch (szNameUpperMaybe[0]) {
    case 'A': case 'B': case 'C': nBucket = 0; break;
    case 'D': case 'E': case 'F': nBucket = 1; break;
    case 'G': case 'H': case 'I': case 'J': nBucket = 2; break;
    case 'K': case 'L': case 'M': nBucket = 3; break;
    case 'N': case 'O': case 'P': case 'Q': nBucket = 4; break;
    case 'R': case 'S': case 'T': nBucket = 5; break;
    case 'U': case 'V': case 'W': nBucket = 6; break;
    case 'X': case 'Y': case 'Z': nBucket = 7; break;
    default: nBucket = 8; break;
    }
    LoadIndexedFile(nBucket);

    int nCount = nRecordsBytes / sizeof(PostBagAlbumIndexRecord);
    int nPos = 0;
    if (nCount != 0) {
        if (strcmp(rec.szName, pRecords[0].szName) > 0) {
            do {
                nPos++;
            } while (nPos < nCount && strcmp(rec.szName, pRecords[nPos].szName) > 0);
        } else if (strcmp(rec.szName, pRecords[0].szName) == 0) {
            while (nPos < nCount && strcmp(rec.szName, pRecords[nPos++].szName) == 0) {
            }
        }
    }
    InsertRecord(nPos, &rec);
}

// FUNCTION: LOCO 0x401aa0
// Re-derives pCard's OWN name-bucket (same A-Z mapping as IndexCard), loads THAT bucket's
// index, and checks whether a record with pCard's own id (nPostSeqId) is already present in it --
// i.e. "is this card correctly indexed under its own name's bucket". AlbumCardWnd::
// PurgeDuplicateCards's sweep deletes any visible card this returns true for (a duplicate/stale
// entry), and this function also removes the found index record itself (RemoveRecordAtIndex)
// -- the caller only deletes the .crd file, so the index entry has to be pruned here. The name
// re-derivation (strcpy+_strupr(szNameUpperMaybe)+switch) happens a SECOND time, unconditionally,
// right after LoadIndexedFile -- dead-but-real, matching the documented "big scratch buffer
// idiom kept by /O2 even when provably unread" class (CLAUDE.md), just with a strcpy instead of
// a memset: the original's raw disasm re-scans/re-copies pCard->nameB into the SAME stack
// slot a second time and the result is never read again.
//
// PARKED (v179, match.py DIFF(82)/366): the switch/jump-table dispatch, LoadIndexedFile
// call, and both guard checks (nRecordsBytes/pRecords) are BYTE-IDENTICAL to the
// original (confirmed via asmscore.py --dump: zero diff for the first 0xd9/217 bytes). The
// entire residual is confined to the linear id-search loop (offsets ~0xd9-0x110): the original
// walks a raw `unsigned int *` pointer (lea'd once to &pRecords[0].nId, then
// `add reg,0x18` each iteration -- a genuine incrementing-pointer idiom, confirmed via raw
// disasm), while every transcription attempt here (both a `pRecords[i].nId`
// array-indexed form AND a `PostBagAlbumIndexRecord *p; p->nId; p++` pointer-walk form,
// tried with both local-declaration orders) picks a DIFFERENT register allocation for the loop
// counter vs. the record pointer than the original -- same class as RemoveRecordAtIndex's
// own EBP-as-SIB-base residual just below (see its own PARKED note), likely the same root cause
// (an intrinsic allocator tie-break on which value lands in which physical register at this
// specific program point, not steerable via source expression/declaration order). The
// array-indexed form (kept here) scored best of the variants tried (asmscore.py total 180041 vs
// 188043 for the pointer-walk form). Not re-grinding further without a genuinely new lever --
// see CLAUDE.md's triage-budget rule.
unsigned char PostBagFileCache::PurgeDuplicateIndexEntry(CarNetState *pCard)
{
    char szNameUpperMaybe[24];
    strcpy(szNameUpperMaybe, pCard->nameB);
    _strupr(szNameUpperMaybe);

    int nBucket;
    switch (szNameUpperMaybe[0]) {
    case 'A': case 'B': case 'C': nBucket = 0; break;
    case 'D': case 'E': case 'F': nBucket = 1; break;
    case 'G': case 'H': case 'I': case 'J': nBucket = 2; break;
    case 'K': case 'L': case 'M': nBucket = 3; break;
    case 'N': case 'O': case 'P': case 'Q': nBucket = 4; break;
    case 'R': case 'S': case 'T': nBucket = 5; break;
    case 'U': case 'V': case 'W': nBucket = 6; break;
    case 'X': case 'Y': case 'Z': nBucket = 7; break;
    default: nBucket = 8; break;
    }
    LoadIndexedFile(nBucket);

    if (nRecordsBytes != 0 && pRecords != NULL) {
        strcpy(szNameUpperMaybe, pCard->nameB);

        int nCount = nRecordsBytes / sizeof(PostBagAlbumIndexRecord);
        int i = 0;
        unsigned int nId = pRecords[0].nId;
        while (nId != pCard->nPostSeqId) {
            i++;
            if (i > nCount) {
                i = -1;
                break;
            }
            nId = pRecords[i].nId;
        }
        if (i != -1) {
            RemoveRecordAtIndex(i);
            return 1;
        }
    }
    return 0;
}

// 0x401650, the ??_GPostBagFileCache scalar deleting dtor, is deliberately NOT claimed here --
// see the measurement written into its declaration in src/PostBag.h (+36 B here, -249 B in
// src/TutorialWnd.cpp, net -213 B).

// FUNCTION: LOCO 0x401620
// Marks the cache as holding no bucket (-1 in nLoadedBucket and in both Unk0x10/Unk0x14, the
// insert-position and insert-time-bucket stashes) with an empty record array, then immediately
// warms bucket 1. nRecordsBytes is deliberately NOT written here -- LoadIndexedFile authors it
// along with pRecords.
PostBagFileCache::PostBagFileCache()
{
    nLoadedBucket = -1;
    pRecords = NULL;
    Unk0x10 = -1;
    Unk0x14 = -1;
    LoadIndexedFile(1);
}

// FUNCTION: LOCO 0x401690
// Grows the currently-loaded bucket's record array by one and inserts pRecord at index nPos,
// keeping it sorted: reallocate to the new size, copy the [0, nPos) prefix, copy pRecord into
// the new slot, copy the [nPos, count) suffix, free the old array. Only caller is
// IndexCard, with the sorted-search position it just found.
//
// PARKED (v152, asmscore.py --len 200: insns 77/77 match, byte_diff=23, total 44243):
// content-complete and structurally near-identical -- the prefix/suffix byte counts are each
// computed ONCE (nPrefixBytes/nSuffixDstOff) and REUSED across the guard check, the memcpy
// call, and the record-insert address, reproducing the original's own CSE'd byte-offset
// register (confirmed via disasm: the original's `lea edi,[ebp+eax*1+0]` insert-address reuses
// the SAME eax register computed for the prefix memcpy's size arg, not a fresh nPos*sizeof
// recompute) -- an earlier draft using array indexing (`pNew[nPos] = *pRecord`) forced a
// redundant recompute and scored far worse (total 109703, byte_diff=63). Residual: (1) the
// original computes the prefix SHL (nPos*sizeof) before the `add esp,4` new-call-cleanup,
// this compile after it -- an instruction-scheduling order around the call site, not
// source-steerable (tried reordering statements, no effect); (2) the original's prefix-count
// zero-guard emits `jbe` while an structurally-identical suffix-count guard emits `je` (both
// present in the original itself) -- tried `!= 0` and `> 0` on the prefix guard, both compiled
// to `je` here, so the original's `jbe` is an intrinsic instruction-selection tie-break (Yoda
// #6 family), not reachable via comparison-operator choice. Same "no mapped PostBag.cpp TU
// yet" cluster as its siblings -- see docs/subsystems.md.
void PostBagFileCache::InsertRecord(int nPos, PostBagAlbumIndexRecord *pRecord)
{
    Unk0x14 = nLoadedBucket;
    Unk0x10 = nPos;

    if (pRecords == NULL) {
        PostBagAlbumIndexRecord *pNew =
            (PostBagAlbumIndexRecord *)::operator new(sizeof(PostBagAlbumIndexRecord));
        pRecords = pNew;
        nRecordsBytes = sizeof(PostBagAlbumIndexRecord);
        *pNew = *pRecord;
        return;
    }

    PostBagAlbumIndexRecord *pNew = (PostBagAlbumIndexRecord *)::operator new(
        nRecordsBytes + sizeof(PostBagAlbumIndexRecord));

    unsigned int nPrefixBytes = nPos * sizeof(PostBagAlbumIndexRecord);
    if (nPrefixBytes != 0) {
        memcpy(pNew, pRecords, nPrefixBytes); // idiom-exempt: runtime length, sizeof-derived above
    }

    memcpy((char *)pNew + nPrefixBytes, pRecord, sizeof(PostBagAlbumIndexRecord));

    unsigned int nSuffixDstOff = (nPos + 1) * sizeof(PostBagAlbumIndexRecord);
    unsigned int nSuffixBytes = nRecordsBytes - nPrefixBytes;
    if (nSuffixBytes != 0) {
        memcpy((char *)pNew + nSuffixDstOff, (char *)pRecords + nPrefixBytes, nSuffixBytes); // idiom-exempt: runtime length, sizeof-derived above
    }

    ::operator delete(pRecords);
    pRecords = pNew;
    nRecordsBytes += sizeof(PostBagAlbumIndexRecord);
}

// FUNCTION: LOCO 0x401760
// Removes the record at index nIndex from the currently-loaded bucket, keeping the rest
// contiguous: if it's the sole record, frees the whole array; otherwise reallocates one record
// smaller, copies the [0, nIndex) prefix and (nIndex, count) suffix into the new array, frees
// the old one. Companion to InsertRecord -- the delete-record half of the sorted-array
// maintenance. Only caller: PurgeDuplicateIndexEntry, on the record it just found.
//
// PARKED (v179, match.py DIFF(48)/171, asmscore.py --len 171 total=223 byte_diff=3
// insns 72/72): content-complete and structurally exact -- the ONLY semantic difference is one
// `lea edi,[reg1+reg2]` whose original picks a non-ebp base register (`[edx+ebp]`) while this
// compile picks ebp as the base (`[ebp+edx]`). That single swap is NOT cosmetic here: x86 SIB
// encoding reserves mod=00/base=101(ebp) for disp32-only addressing, so a REAL zero-displacement
// `[ebp+reg]` needs an explicit 1-byte disp8=0x00 (mod=01) that `[reg+ebp-as-index]` doesn't --
// making this compile's LEA literally ONE BYTE LONGER, which then shifts every subsequent byte
// by 1 and cascades into match.py's raw (non-realigned) diff count of 48. Tried swapping the
// addition operand order in both the source memcpy-arg expressions (`pNew+nIndex` vs
// `nIndex+pNew`) -- zero effect on the emitted register choice either way, confirming this is
// the SAME class as the already-documented symmetric-register-swap residuals (Yoda #29/#30),
// just with a precise root cause identified: which operand the allocator happens to leave in
// ebp at that program point, not source expression order. See CLAUDE.md's "KEY VC++ x86
// byte-matching lessons" for the general pattern this refines.
void PostBagFileCache::RemoveRecordAtIndex(int nIndex)
{
    if (nRecordsBytes == sizeof(PostBagAlbumIndexRecord)) {
        ::operator delete(pRecords);
        nRecordsBytes = 0;
        pRecords = NULL;
        return;
    }

    PostBagAlbumIndexRecord *pNew = (PostBagAlbumIndexRecord *)::operator new(
        nRecordsBytes - sizeof(PostBagAlbumIndexRecord));

    unsigned int nPrefixBytes = nIndex * sizeof(PostBagAlbumIndexRecord);
    if ((int)nPrefixBytes < (int)nRecordsBytes) {
        if ((int)nPrefixBytes > 0) {
            memcpy(pNew, pRecords, nPrefixBytes); // idiom-exempt: runtime length, sizeof-derived above
        }

        unsigned int nSuffixBytes = nRecordsBytes - (nIndex + 1) * sizeof(PostBagAlbumIndexRecord);
        if ((int)nSuffixBytes > 0) {
            memcpy(pNew + nIndex, pRecords + nIndex + 1, nSuffixBytes); // idiom-exempt: runtime length, sizeof-derived above
        }

        ::operator delete(pRecords);
        pRecords = pNew;
        nRecordsBytes -= sizeof(PostBagAlbumIndexRecord);
    }
}

// FUNCTION: LOCO 0x401810
// Loaded-bucket record count (nRecordsBytes / sizeof(PostBagAlbumIndexRecord) == 0x18 --
// the divide-by-24 magic-multiply reciprocal codegen independently corroborates the 0x18
// record stride, see docs/subsystems.md). Callers: AlbumCardWnd's RedrawAllSlots/
// OnKeyDown/OnLButtonDown paging checks. Moved out of src/phase2_probe2.cpp 2026-07-22
// (v322, was the probe-local DivObj0x401810::GetCountDiv24).
unsigned int PostBagFileCache::FUN_401810_GetCountDiv24() {  // TODO: sync
    return nRecordsBytes / 24;
}

// FUNCTION: LOCO 0x401820
// Same record count as FUN_401810_GetCountDiv24, but switches the loaded bucket first when the
// caller is asking about a different one. Called by AlbumCardWnd::OnLButtonDown's page-back
// handler with the candidate nStartIndex's bucket, for the wraparound check.
unsigned int PostBagFileCache::LoadBucketAndGetRecordCount(int nBucket)
{
    if (nLoadedBucket != nBucket) {
        LoadIndexedFile(nBucket);
    }
    return nRecordsBytes / sizeof(PostBagAlbumIndexRecord);
}

// FUNCTION: LOCO 0x401c90
// Flushes the currently-loaded bucket (if any, i.e. nLoadedBucket != -1) to
// "<install>PostBag\AlbIndex\<clientId><bucket>.ind": always deletes the old file first, then
// rewrites it from pRecords/nRecordsBytes only if both are non-empty, and only then
// frees the in-memory array and resets the loaded-bucket state to "none". If the bucket was
// loaded but the array is empty/null, only the loaded-bucket/records fields are cleared -- the
// just-deleted file is simply left absent. Only caller: LoadIndexedFile, below.
void PostBagFileCache::SaveIndexFile()
{
    if (nLoadedBucket == -1) {
        return;
    }

    char szPath[0x504];
    wsprintfA(szPath, "%s%s%s\\%03d%04d.ind", &g_pInstallPathPrefix, "PostBag", "\\AlbIndex",
              g_pLocalPlayerIdentity->clientId, nLoadedBucket);
    DeleteFileA(szPath);

    if (nRecordsBytes != 0 && pRecords != NULL) {
        HANDLE hFile = CreateFileA(szPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD dwErr = GetLastError();
            LPSTR pszMsg;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszMsg, 0, NULL);
            LocalFree(pszMsg);
            return;
        }

        DWORD dwWritten;
        if (!WriteFile(hFile, pRecords, nRecordsBytes, &dwWritten, NULL)) {
            DWORD dwErr = GetLastError();
            LPSTR pszMsg;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszMsg, 0, NULL);
            LocalFree(pszMsg);
            return;
        }
        CloseHandle(hFile);
        ::operator delete(pRecords);
        pRecords = NULL;
        nLoadedBucket = -1;
        nRecordsBytes = 0;
        return;
    }

    pRecords = NULL;
    nLoadedBucket = -1;
}

// FUNCTION: LOCO 0x401df0
// Switches the in-memory bucket to nBucket: a no-op if it's already loaded, else flushes the
// current bucket via SaveIndexFile (unconditionally -- SaveIndexFile itself no-ops if
// nLoadedBucket is already -1), then, unless nBucket is -1 (a pure "unload" request), opens
// "<install>PostBag\AlbIndex\<clientId><bucket>.ind" and reads it whole into a fresh buffer. Any
// open/size-query failure (missing file, zero size, or a query error) leaves the cache in the
// "loaded but empty" state (nLoadedBucket = nBucket, pRecords = NULL,
// nRecordsBytes = 0) rather than propagating an error. The dwSize == 0 branch right before
// the alloc+read is genuinely unreachable (already handled above) -- a real defensive
// belt-and-suspenders check in the original, reproduced faithfully rather than simplified away.
// A failed ReadFile leaves nLoadedBucket/the open handle untouched (sic: a handle leak) --
// same as the original.
void PostBagFileCache::LoadIndexedFile(int nBucket)
{
    if (nBucket == nLoadedBucket) {
        return;
    }
    SaveIndexFile();
    if (nBucket == -1) {
        return;
    }

    char szPath[0x504];
    wsprintfA(szPath, "%s%s%s\\%03d%04d.ind", &g_pInstallPathPrefix, "PostBag", "\\AlbIndex",
              g_pLocalPlayerIdentity->clientId, nBucket);

    HANDLE hFile = CreateFileA(szPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        nLoadedBucket = nBucket;
        nRecordsBytes = 0;
        pRecords = NULL;
        return;
    }

    DWORD dwSize = GetFileSize(hFile, NULL);
    if (dwSize == 0) {
        nLoadedBucket = nBucket;
        nRecordsBytes = 0;
        pRecords = NULL;
        CloseHandle(hFile);
        return;
    }
    if (dwSize == 0xffffffff) {
        nLoadedBucket = nBucket;
        nRecordsBytes = 0;
        pRecords = NULL;
        CloseHandle(hFile);
        return;
    }

    if (dwSize > 0) {
        PostBagAlbumIndexRecord *pBuffer = (PostBagAlbumIndexRecord *)::operator new(dwSize);
        pRecords = pBuffer;
        nRecordsBytes = dwSize;

        DWORD dwRead;
        if (!ReadFile(hFile, pBuffer, dwSize, &dwRead, NULL)) {
            DWORD dwErr = GetLastError();
            LPSTR pszMsg;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszMsg, 0, NULL);
            LocalFree(pszMsg);
            return;
        }
    } else {
        pRecords = NULL;
        nRecordsBytes = 0;
    }

    nLoadedBucket = nBucket;
    CloseHandle(hFile);
}

// FUNCTION: LOCO 0x444c70
// Factory: new_alloc(0x39c)s + default-ctors a CarNetState, then LoadCardFile(pszPath)
// into it; on success returns the object, on failure deletes it (scalar-deleting-dtor slot 0,
// flag=1) and returns NULL. Plain __stdcall free function (not a real this-bound method despite
// the CarNetState:: Ghidra namespace) -- `new CarNetState();` / `delete pCard;`
// reproduce the /GX alloc-protection scaffolding and null-guarded delete exactly.
CarNetState *PostBagCacheBundle::CarNetState_CreateFromFile(const char *pszPath)
{
    CarNetState *pCard = new CarNetState();
    if (pCard->LoadCardFile(pszPath)) {
        return pCard;
    }
    delete pCard;
    return NULL;
}

// FUNCTION: LOCO 0x401c10
// Read-by-index counterpart to IndexCard/InsertRecord. Loads nBucket, then walks the
// records array starting at nStartIndex, byte-offset style (matches nRecordsBytes being a
// raw byte count, not a record count -- same field used elsewhere via division). For each
// record builds its .crd path (PostBag_BuildCrdPath on nId) and tries to load it
// (CreateFromFileMaybe); on a missing/corrupt file (NULL) it advances to the next record instead
// of failing outright, so this really is find-first-loadable-card-at-or-after-index, not a
// strict by-index lookup. Returns NULL once nStartIndex is past the loaded records. Callers:
// FUN_00405520 (a decrementing purge/dedup sweep) and DrawOrEraseCardSlot (thumbnail list rendering).
//
// Structural note: the outer `if (pRecords != NULL) do {...} while(cond)` shape peels the
// do-while's first iteration under /O2 (duplicating the whole loop body, ~50 extra bytes) --
// same class as WidgetPickerObj0x477cc8::HandleSavegameMenuNode's documented fix (CLAUDE.md).
// Restructuring as a plain `while (cond) {...}` (the bound check alone as the loop's own
// condition, with the null check factored out as a separate early return above it) avoids the
// peel and closed the bulk of the residual (asmscore total 281431->22239, byte_diff 121->19).
//
// EFFECTIVE MATCH (asmscore.py --len 0x74: insns 38/39, byte_diff 28, total 26248).
// v361 corrected the MODEL: the two `mov ecx,[g_pPostBagCache]` loads are NOT the "unrelated,
// unused global reloads" this was parked as from v168 -- they are the two callees' `this`.
// PostBag_BuildCrdPath (0x445930) and CarNetState_CreateFromFile (0x444c70) are both __thiscall
// PostBagCacheBundle members that never read `this`; every call site of each in the binary loads
// ecx from the singleton (see src/PostBag.h). The old autopsy came within one step of this --
// it noted `this` here is a PostBagFileCache*, not a PostBagCacheBundle* -- but concluded the
// reload was dead rather than that the ecx belongs to the CALLEE, not this function.
// That took the instruction count from 36/39 to 38/39.
// Sole remaining residual (1 instruction): the original materializes the record address into a
// register and dereferences it separately (`lea eax,[esi+ecx+0x14]` … `mov eax,[eax]`, with the
// first ecx load sandwiched between) where this compile fuses it into one SIB `mov
// eax,[base+idx+0x14]`, and consequently pushes the literal `0` argument one slot later than the
// original. Probed and REFUTED for the lea: a `PostBagAlbumIndexRecord *pRecord = &pRecords[i];`
// pointer local (emits `add eax,esi` + `mov edx,[eax+0x14]`, total 36700, WORSE) and an
// implicitly-inline `RecordAt(int i)` accessor returning `&pRecords[i]` (still fuses the load --
// it only shuffles registers, total 22344; NOT the v360 inline-accessor class despite the
// superficially matching "materialized lea vs fused load" symptom). See docs/PARKED.md.
CarNetState *PostBagFileCache::FindFirstLoadableCardAtOrAfterIndex(int nStartIndex, int nBucket)
{
    char szPath[0x504];
    LoadIndexedFile(nBucket);
    if (pRecords == NULL) {
        return NULL;
    }

    int i = nStartIndex;
    while (i * (int)sizeof(PostBagAlbumIndexRecord) < (int)nRecordsBytes) {
        unsigned int nId = pRecords[i].nId;
        g_pPostBagCache->PostBag_BuildCrdPath(nId, 0, szPath);
        CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(szPath);
        i++;
        if (pCard != NULL) {
            return pCard;
        }
    }

    return NULL;
}

// FUNCTION: LOCO 0x445930
// Builds "<install>PostBag\<category>\<nId as %07d>.crd" -- the sole dependency of
// PostBagFileCache::FindFirstLoadableCardAtOrAfterIndex above. Same 8-way category switch + easter-locale
// sub-switch idiom as PostBag_SaveCardToCategory (source declaration order 0,1,2,3,6,4,5,7,
// confirmed via a raw VA->file-offset jump-table read: the outer table has 8 entries and the
// inner easter-locale table is a genuine flat 0-9 (10-entry) table with both value 0 and value 3
// pointing at the same target as the out-of-range default).
//
// ============================================================================================
// CANONICAL NOTE for this cluster's easter-locale sub-switch (v352). Referenced by every
// PostBag_* function below that carries one. Supersedes the old "flat 0-9 vs. normalized 1-9
// jump-table shape" park, which was diagnosed wrong for 12 sessions.
//
// THE SHAPE (solved). The inner switch's two Eng arms are SEPARATE source arms, and `default:`
// is written FIRST:
//     switch (g_nEasterLocaleId) {
//     default: ... "\\Easter\\Eng";   <- written first; MSVC emits its block first
//     case 1..case 9 (no 0, no 3) ...
//     case 0: case 3: ... "\\Easter\\Eng";   <- written LAST, a distinct arm
//     }
// Because `case 0` is a real label, the case set's minimum is 0, so MSVC emits the original's
// FLAT 10-entry table (`cmp eax,9; ja`) with no `dec eax` normalization. The old source folded
// `case 0:`/`case 3:`/`default:` into ONE arm, which drops 0 from the case set, makes the
// minimum 1, and forces the `dec eax; cmp eax,8` 9-entry table. Restructuring removed the
// `dec` at all 7 sites in this TU and cut DIFF roughly in half across the cluster
// (0x445400/0x445510 77->48, 0x445930 69->40, 0x445000 133->98, 0x4451a0 194->154).
// Confirmed against the raw tables: entries 0 and 3 and the out-of-range `ja` ALL target the
// last block, and the original carries a genuinely UNREACHABLE `mov eax,"\\Easter\\Eng"; jmp`
// block at the first arm position that nothing in either jump table points at.
//
// THE REMAINING RESIDUAL (toolchain-level, NOT source-level, and CLOSED as of v353). Both Eng
// arms assign the SAME string literal. /Gf string pooling makes those two literals one symbol,
// which makes the two arms' basic blocks literally identical, which lets the optimizer merge
// them -- losing exactly one 7-byte block. Defeating pooling (either `/Ox`, i.e. /O2 minus /Gf,
// or v352's textually-different-literals hack) suppresses the merge and takes 0x445400,
// 0x445510 and 0x445930 to byte-EXACT.
//
// We keep NEITHER workaround, and the flag set is NOT wrong -- v352's suspicion was refuted:
//   * /O2 == /Ox /Gf == /Ox /GF, verified to the byte repo-wide (38464 B / 235 funcs each).
//     Pooling is load-bearing: /Ox alone costs 3401 B / 2 funcs (mostly src/CursorDesc.cpp).
//   * Per-file overrides are impossible here: 0x445930 (wants pooling OFF) and 0x445a40
//     PostBag_ReadDatFile (wants pooling ON) are ADJACENT in .text -- same .obj, same flags.
//   * The /Ox "match" is byte-coincidental, not semantic. In the /Ox .obj all 10 inner blocks
//     are LIVE; in the original the first block is DEAD (zero references image-wide). The
//     original MERGED and then left the orphan behind, which cl 11.00.7022 never does. Only
//     relocation masking makes the two agree.
// So this is a VC5 compiler-BUILD difference, not a command-line one. See docs/PARKED.md.
// ============================================================================================
//
// PARKED (asmscore.py --len 0x110 -- NOTE the true COMDAT extent, which INCLUDES the 72 bytes
// of trailing jump table; app_funcs.txt's 187 and Ghidra's `Body` span are the CODE-only
// extent and silently truncate the compare window): DIFF 40. Content-complete and structurally
// aligned; the outer category switch and the trailing wsprintfA call (including its exact
// operand push order) match byte-for-byte. Sole residual is the tail-merged Eng block above.
void PostBagCacheBundle::PostBag_BuildCrdPath(int nId, int nCategory, char *pszOut)
{
    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(pszOut, "%s%s%s\\%07d.crd", g_pInstallPathPrefix, "PostBag", pszCategory, nId);
}

// FUNCTION: LOCO 0x445000
// The .crd-delete counterpart to PostBag_SaveCardToCategory: builds pCard's own .crd path
// inline (same 8-way category switch + easter-locale sub-switch idiom as
// PostBag_BuildCrdPath above, using pCard->nPostSeqId as the id -- NOT a call to
// PostBag_BuildCrdPath itself, the original duplicates the switch verbatim here too, per
// this cluster's already-documented "same switch reimplemented at >=8 sites" pattern), deletes
// it, then (only on success) refreshes nCachedCategoryFileCount via the same
// scan+free+tally idiom as DeleteCardFileAndRefreshCount above.
//
// PARKED (asmscore.py --len 0x11c: total 246085, byte_diff 85, insns 119/84): content-complete
// and structurally aligned. Two residuals, both already-documented intrinsic classes in this
// cluster, no new lever tried: (1) the 5th confirmation of the "flat 0-9 vs. normalized 1-9"
// easter-locale jump-table shape (see PostBag_BuildCrdPath above and docs/PARKED.md).
// (2) a dead `mov ecx,ebx` (`this` reloaded into ecx) immediately before the
// PostBag_ScanCategoryCrdFiles call, even though that callee is a plain __stdcall free
// function with no implicit this/ecx argument -- the exact same residual already parked on
// DeleteCardFileAndRefreshCount (Yoda #29/#30 dead-register-reload family, see
// docs/PARKED.md), reproduced here with a different `this` type (PostBagCacheBundle* either way,
// so not itself a new data point, just a 2nd confirmation on a sibling call site). See
// docs/PARKED.md.
void PostBagCacheBundle::DeleteCardById(CarNetState *pCard, int nCategory)
{
    char szPath[0x504];
    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(szPath, "%s%s%s\\%07d.crd", g_pInstallPathPrefix, "PostBag", pszCategory, pCard->nPostSeqId);

    if (!DeleteFileA(szPath)) {
        return;
    }

    short nCount = 0;
    PostBagCrdFileNode *pNode = PostBag_ScanCategoryCrdFiles(2, 0);
    while (pNode != NULL) {
        PostBagCrdFileNode *pTemp = pNode;
        pNode = pNode->pNext;
        nCount++;
        ::operator delete(pTemp);
    }
    nCachedCategoryFileCount = nCount;
}

// FUNCTION: LOCO 0x443550
// Deletes every non-dotfile matched by pszDirWithWildcard's _findfirst scan (a "<dir>\*.*"
// pattern). Builds the per-file delete path from a TRUNCATED copy of pszDirWithWildcard: strcpy
// the full wildcarded pattern into szDir, then overwrite szDir[strlen(szDir) - 3] with a NUL --
// landing exactly on the leading '*' of the trailing "\*.*", so szDir ends up "<dir>\" (trailing
// backslash kept, wildcard stripped). Generic helper, no PostBag-specific knowledge -- `this`
// is real (see PostBag_ClearWorkingFolders below) but never read here, same "this-in-ecx
// but never read" class as UIResources::PlayUiSound/PlaySoundAtScreenPos (CLAUDE.md).
void PostBagCacheBundle::PostBag_ClearFolder(char *pszDirWithWildcard)
{
    char szDir[0x504] = "";

    char szPath[0x504] = "";

    strcpy(szDir, pszDirWithWildcard);
    szDir[strlen(szDir) - 3] = 0;

    struct _finddata_t findData;
    long hFind = _findfirst(pszDirWithWildcard, &findData);
    if (hFind != -1) {
        do {
            if (findData.name[0] != '.') {
                wsprintfA(szPath, "%s%s", szDir, findData.name);
                SetFileAttributesA(szPath, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(szPath);
            }
        } while (_findnext(hFind, &findData) == 0);
        _findclose(hFind);
    }
}

// FUNCTION: LOCO 0x443470
// Clears the 4 working folders that accumulate transient per-session files: Sort\In, Sort\Out,
// Att_Out, Att_In (each built as "<install>PostBag<category>\*.*" then handed to
// PostBag_ClearFolder above). Sole caller: PostBag_CacheDtorMaybe (0x4431f0), which calls
// this as a real member (this->PostBag_ClearWorkingFolders()) -- confirmed by this
// function's own `this` forwarding unchanged into every PostBag_ClearFolder call despite
// never touching a PostBagCacheBundle field itself.
void PostBagCacheBundle::PostBag_ClearWorkingFolders()
{
    char szPattern[0x504] = "";

    wsprintfA(szPattern, "%s%s%s\\*.*", g_pInstallPathPrefix, "PostBag", "\\Sort\\In");
    PostBag_ClearFolder(szPattern);
    wsprintfA(szPattern, "%s%s%s\\*.*", g_pInstallPathPrefix, "PostBag", "\\Sort\\Out");
    PostBag_ClearFolder(szPattern);
    wsprintfA(szPattern, "%s%s%s\\*.*", g_pInstallPathPrefix, "PostBag", "\\Att_Out");
    PostBag_ClearFolder(szPattern);
    wsprintfA(szPattern, "%s%s%s\\*.*", g_pInstallPathPrefix, "PostBag", "\\Att_In");
    PostBag_ClearFolder(szPattern);
}

// FUNCTION: LOCO 0x4451a0
// Deletes both the .att and .dat sidecar files for an attachment id in a category directory
// (same 8-way category switch idiom as PostBag_BuildCrdPath -- duplicated verbatim per
// this cluster's established multi-site-reimplementation pattern). No rescan/count refresh,
// unlike the .crd-delete counterpart DeleteCardById. nId is masked to its low 16 bits in
// the format call -- the caller only ever loads a 16-bit id into the low half of a 32-bit reg.
//
// PARKED (asmscore.py --len 0x260: total 577202, byte_diff 182, insns 197/219): content-complete
// and structurally aligned. Sole residual: the 6th+ confirmation of this cluster's
// already-documented easter-locale sub-switch residual (SOLVED as to shape in v352 -- see the CANONICAL NOTE above PostBag_BuildCrdPath/0x445930; what remains is the tail-merged duplicate "\Easter\Eng" block, a toolchain-level gap), reproduced
// TWICE within this one function (once per duplicated category switch) -- the bulk of the raw
// byte_diff is the jump-table DATA blob differing purely as a byproduct of the table-shape
// difference, not a separate bug. Not re-probed -- no new lever beyond what's already exhausted
// on 5+ siblings (PostBag_BuildCrdPath/DeleteCardById/PostBag_ScanCategoryCrdFiles
// /PostBag_SaveCardToCategory/PostBag_LoadEasterNameCache). See docs/PARKED.md.
void PostBagCacheBundle::PostBag_DeleteAttachmentFiles(int nCategory, unsigned short nId)
{
    char szPath[0x504] = "";

    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(szPath, "%s%s%s\\%08d.att", g_pInstallPathPrefix, "PostBag", pszCategory, nId & 0xffff);
    DeleteFileA(szPath);

    const char *pszCategory2 = NULL;
    switch (nCategory) {
    case 0: pszCategory2 = "\\Album"; break;
    case 1: pszCategory2 = "\\Sort\\In"; break;
    case 2: pszCategory2 = "\\Sort\\Out"; break;
    case 3: pszCategory2 = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        case 1: pszCategory2 = "\\Easter\\Dan"; break;
        case 2: pszCategory2 = "\\Easter\\Dut"; break;
        case 4: pszCategory2 = "\\Easter\\Fre"; break;
        case 5: pszCategory2 = "\\Easter\\Ger"; break;
        case 6: pszCategory2 = "\\Easter\\Ita"; break;
        case 7: pszCategory2 = "\\Easter\\Nor"; break;
        case 8: pszCategory2 = "\\Easter\\Spa"; break;
        case 9: pszCategory2 = "\\Easter\\Swe"; break;
        case 0:
        case 3:
        default: pszCategory2 = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory2 = "\\Att_Out"; break;
    case 5: pszCategory2 = "\\Att_In"; break;
    case 7: pszCategory2 = "\\Design"; break;
    }
    wsprintfA(szPath, "%s%s%s\\%08d.dat", g_pInstallPathPrefix, "PostBag", pszCategory2, nId & 0xffff);
    DeleteFileA(szPath);
}

// FUNCTION: LOCO 0x445400
// Builds "<install>PostBag<category>\<nId as %08d>.att" into pszOut (same 8-way category
// switch idiom as PostBag_BuildCrdPath/PostBag_DeleteAttachmentFiles -- one half of
// that function's own inline duplication, factored out here as a real callable helper).
// Callers: multiplayer attachment file-transfer subsystem (GameNet_*/NetFile_*).
//
// PARKED (asmscore.py --len 0x110: total 244090, byte_diff 90, insns 88/88): content-complete
// and structurally aligned. Sole residual: the 7th confirmation of this cluster's
// already-documented easter-locale sub-switch residual (SOLVED as to shape in v352 -- see the CANONICAL NOTE above PostBag_BuildCrdPath/0x445930; what remains is the tail-merged duplicate "\Easter\Eng" block, a toolchain-level gap) (single
// instance, matching PostBag_BuildCrdPath's own 1x confirmation). Not re-probed. See
// docs/PARKED.md.
void PostBagCacheBundle::PostBag_BuildAttFilePath(unsigned short nId, int nCategory, char *pszOut)
{
    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(pszOut, "%s%s%s\\%08d.att", g_pInstallPathPrefix, "PostBag", pszCategory, nId & 0xffff);
}

// FUNCTION: LOCO 0x445510
// Builds "<install>PostBag<category>\<nId as %08d>.dat" into pszOut -- the .dat sidecar
// counterpart to PostBag_BuildAttFilePath above (same category switch, same callers).
//
// PARKED (asmscore.py --len 0x110: total 241763, byte_diff 93, insns 88/85): content-complete
// and structurally aligned, structurally identical residual to PostBag_BuildAttFilePath
// above (8th confirmation of the same easter-locale jump-table shape, single instance). Not
// re-probed. See docs/PARKED.md.
void PostBagCacheBundle::PostBag_BuildDatFilePath(unsigned short nId, int nCategory, char *pszOut)
{
    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(pszOut, "%s%s%s\\%08d.dat", g_pInstallPathPrefix, "PostBag", pszCategory, nId & 0xffff);
}

// FUNCTION: LOCO 0x445620
// Builds "<install>PostBag\Easter\<Lang><pszName><ext>" into pszOut, ext = ".crd" if bCrd else
// ".rsp" -- no outer 8-way category switch, unlike its siblings; always targets the Easter
// locale subfolder. Sole caller: LoadOrCreateEasterCard. The manual strlen-then-copy shape
// in the original disasm (repnz scasb for length, rep movsd+movsb for the copy) is the same
// compiler strcpy-intrinsic idiom already documented on EditCardWnd::RebuildLocalPlayerCard.
//
// PARKED (asmscore.py --len 0xe0: total 175715, byte_diff 85, insns 79/76): content-complete.
// Two residuals: (1) the 9th confirmation of this cluster's "flat 0-9 vs. normalized 1-9"
// easter-locale jump-table shape. (2) a minor register-scheduling tie-break on the strcpy
// intrinsic's destination-address `lea` (original computes &szExt once, early, right after the
// prologue; this compile schedules the equivalent lea right before the copy instead) -- tried
// both a ternary and an if/else assignment for the .crd/.rsp selection, and both declaration
// orders for szExt vs the extension pointer local, all four compiled byte-identical to each
// other (Yoda #29/#30 intrinsic tie-break family). Not re-probed further. See docs/PARKED.md.
void PostBagCacheBundle::PostBag_BuildEasterCardPath(char *pszName, char bCrd, char *pszOut)
{
    char szExt[8];
    char *pszExt = ".crd";
    if (bCrd == 0) {
        pszExt = ".rsp";
    }
    strcpy(szExt, pszExt);

    char *pszCategory;
    switch (g_nEasterLocaleId) {
    case 1: pszCategory = "\\Easter\\Dan"; break;
    case 2: pszCategory = "\\Easter\\Dut"; break;
    case 4: pszCategory = "\\Easter\\Fre"; break;
    case 5: pszCategory = "\\Easter\\Ger"; break;
    case 6: pszCategory = "\\Easter\\Ita"; break;
    case 7: pszCategory = "\\Easter\\Nor"; break;
    case 8: pszCategory = "\\Easter\\Spa"; break;
    case 9: pszCategory = "\\Easter\\Swe"; break;
    default: pszCategory = "\\Easter\\Eng"; break;
    }
    wsprintfA(pszOut, "%s%s%s\\%s%s", g_pInstallPathPrefix, "PostBag", pszCategory, pszName, szExt);
}

// FUNCTION: LOCO 0x445700
// Builds "<install>Clipart\<filename>" into pszOut, creating the Clipart\ directory first if it
// doesn't exist. The 32-way kindHi-range filename switch verbatim duplicates
// PostBagCacheBundle::ClipartBitmapCache_GetOrLoad's own switch (confirmed via matching
// literals AND jump-table case-body order: 31, 30, [16-25], [26-29], [0-15], default). bDescByte
// packs kindHi (upper 5 bits) and kindLo-1 (lower 3 bits). Callers: network resource-sync
// (NetResource_RequestMissingAppearances, GameNet_DispatchMessage).
void PostBagCacheBundle::PostBag_BuildClipartFilePath(unsigned char bDescByte, unsigned char nIndex, char *pszOut)
{
    char szFileName[64] = "";

    unsigned char kindHi = bDescByte >> 3;
    unsigned char kindLo = (bDescByte & 7) + 1;
    switch (kindHi) {
    case 31:
        wsprintfA(szFileName, "S0%03d.bmp", nIndex & 0xff);
        break;
    case 30:
        wsprintfA(szFileName, "R%01d%03d.bmp", kindLo - 1, nIndex & 0xff);
        break;
    case 16: case 17: case 18: case 19: case 20:
    case 21: case 22: case 23: case 24: case 25:
        wsprintfA(szFileName, "%c%01d%03d.bmp", (unsigned char)(kindHi + 0x58), kindLo - 1, nIndex & 0xff);
        break;
    case 26: case 27: case 28: case 29:
        wsprintfA(szFileName, "%c%01d%03d.bmp", (unsigned char)(kindHi + 0x5a), kindLo - 1, nIndex & 0xff);
        break;
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15:
        wsprintfA(szFileName, "%01x%01d%03d.bmp", kindHi, kindLo - 1, nIndex & 0xff);
        break;
    default:
        wsprintfA(szFileName, "0%01d%03d.bmp", kindLo - 1, nIndex & 0xff);
        break;
    }

    char szClipartDir[260];
    wsprintfA(szClipartDir, "%sClipart\\", g_pInstallPathPrefix);
    if (GetFileAttributesA(szClipartDir) == 0xffffffff) {
        CreateDirectoryA(szClipartDir, NULL);
    }
    wsprintfA(pszOut, "%sClipart\\%s", g_pInstallPathPrefix, szFileName);
}

// FUNCTION: LOCO 0x445a40
// Reads up to 0x400 bytes of a category .dat sidecar file into pOutBuf (same 8-way category
// switch + path format as PostBag_BuildDatFilePath, duplicated verbatim per this
// cluster's established multi-site-reimplementation pattern). Sole caller: FUN_0042eea0
// (MailWnd/AlbumCardWnd-area).
//
// PARKED (asmscore.py --len 0x190: total 295898, byte_diff 108, insns 125/129): content-complete
// and structurally aligned. dwRead's declaration was moved to the TOP of the function (matching
// the original, which zeroes it before even the szPath zero-init loop) -- closed a real
// structural gap (byte_diff 188->137). Remaining two residuals, both already-documented
// intrinsic classes: (1) the 10th confirmation of this cluster's "flat 0-9 vs. normalized 1-9"
// easter-locale jump-table shape. (2) a cascading edx/ecx/eax register-role swap through the
// CreateFileA/ReadFile argument push sequence -- the symmetric-register-swap tie-break family
// (Yoda #29/#30), not source-steerable. Not re-probed further. See docs/PARKED.md.
void PostBagCacheBundle::PostBag_ReadDatFile(unsigned short nId, int nCategory, void *pOutBuf)
{
    DWORD dwRead = 0;
    char szPath[0x504] = "";

    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(szPath, "%s%s%s\\%08d.dat", g_pInstallPathPrefix, "PostBag", pszCategory, nId & 0xffff);

    HANDLE hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        ReadFile(hFile, pOutBuf, 0x400, &dwRead, NULL);
        CloseHandle(hFile);
    }
}

// FUNCTION: LOCO 0x445f20
// Standalone version of the "[POSTCARD] NextAttId" ini-counter alloc/wrap: reads the counter
// (default 1), wraps back to 1 once it exceeds 0x7ffc, writes counter+1 back, returns the
// pre-increment value. Sole caller: GameNet_HandleTrainStateSync.
// PostBag_ImportAttachmentFile duplicates this same logic inline rather than calling it.
unsigned short PostBagCacheBundle::PostBag_AllocNextAttId()
{
    unsigned int nId = g_pIniFile->ReadInt("POSTCARD", "NextAttId", 1);
    if ((unsigned short)nId > 0x7ffc) {
        nId = 1;
    }
    g_pIniFile->WriteInt("POSTCARD", "NextAttId", (nId & 0xffff) + 1);
    return (unsigned short)nId;
}

// FUNCTION: LOCO 0x445bd0
// Allocates a new attachment id (duplicates PostBag_AllocNextAttId's own ini-counter logic
// inline rather than calling it), writes a .dat placeholder whose content IS the source path
// string pszSrcPath itself (zero-padded to 0x400 bytes) -- not real metadata; confirmed via
// disasm, resolving the earlier "possible sic" flag as deliberate original behavior, not a
// transcription artifact. CopyFileA's the real source into the new .att path only after the
// placeholder .dat write succeeds. Sole caller: EditCardWnd_ImportDecalImageMaybe.
//
// PARKED (asmscore.py --len 0x350: total 764370, byte_diff 270, insns 273/300): content-complete
// and semantically faithful (params/return values traced against the raw disasm, not just the
// decompile). Several compounding residuals, all already-documented intrinsic classes: (1) the
// 11th/12th confirmation of the easter-locale sub-switch residual (SOLVED as to shape in v352 -- see the CANONICAL NOTE above PostBag_BuildCrdPath/0x445930; what remains is the tail-merged duplicate "\Easter\Eng" block, a toolchain-level gap)
// (the category switch is duplicated twice in this function, like DeleteAttachmentFilesMaybe).
// (2) a cascading esi/ebp register-role swap spanning most of the function (Yoda #29/#30
// symmetric-register-swap family). (3) the original caches wsprintfA's IAT pointer in ebp once
// and reuses it via `call ebp` at both call sites; this compile reloads via indirect `call
// [iat]` at each site instead -- an import-pointer-caching decision, not source-steerable via
// any construct tried. (4) the CreateFileA-failure early exit gets its own inline duplicate
// epilogue instead of sharing the tail with the success path, unlike the original (which jumps
// to one shared pop/ret sequence) -- tried the established `goto done;`-with-a-single-trailing-
// `return` restructuring (CLAUDE.md's BuildPaletteLUT precedent), compiled BYTE-IDENTICAL
// to the plain multi-return version, confirming this specific case does NOT respond to that
// lever (unlike the precedent, which had only 2 identical `return 0;` sites -- this function's
// 3 exits return 3 different values, a materially different shape). Not re-probed further. See
// docs/PARKED.md.
unsigned int PostBagCacheBundle::PostBag_ImportAttachmentFile(int nCategory, char *pszSrcPath)
{
    char szPath[0x504] = "";

    char szDatContent[0x400] = "";

    DWORD dwWritten = 0;
    strcpy(szDatContent, pszSrcPath);

    unsigned int nId = g_pIniFile->ReadInt("POSTCARD", "NextAttId", 1);
    if ((unsigned short)nId > 0x7ffc) {
        nId = 1;
    }
    unsigned int nId16 = nId & 0xffff;
    g_pIniFile->WriteInt("POSTCARD", "NextAttId", nId16 + 1);

    const char *pszCategory = NULL;
    switch (nCategory) {
    case 0: pszCategory = "\\Album"; break;
    case 1: pszCategory = "\\Sort\\In"; break;
    case 2: pszCategory = "\\Sort\\Out"; break;
    case 3: pszCategory = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        default: pszCategory = "\\Easter\\Eng"; break;
        case 1: pszCategory = "\\Easter\\Dan"; break;
        case 2: pszCategory = "\\Easter\\Dut"; break;
        case 4: pszCategory = "\\Easter\\Fre"; break;
        case 5: pszCategory = "\\Easter\\Ger"; break;
        case 6: pszCategory = "\\Easter\\Ita"; break;
        case 7: pszCategory = "\\Easter\\Nor"; break;
        case 8: pszCategory = "\\Easter\\Spa"; break;
        case 9: pszCategory = "\\Easter\\Swe"; break;
        case 0:
        case 3: pszCategory = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory = "\\Att_Out"; break;
    case 5: pszCategory = "\\Att_In"; break;
    case 7: pszCategory = "\\Design"; break;
    }
    wsprintfA(szPath, "%s%s%s\\%08d.dat", g_pInstallPathPrefix, "PostBag", pszCategory, nId16);

    unsigned int nResult;
    HANDLE hFile = CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        nResult = 0;
        goto done;
    }

    if (!WriteFile(hFile, szDatContent, 0x400, &dwWritten, NULL)) {
        nResult = CloseHandle(hFile) & 0xffff0000;
        goto done;
    }
    CloseHandle(hFile);

    const char *pszCategory2;
    pszCategory2 = NULL;
    switch (nCategory) {
    case 0: pszCategory2 = "\\Album"; break;
    case 1: pszCategory2 = "\\Sort\\In"; break;
    case 2: pszCategory2 = "\\Sort\\Out"; break;
    case 3: pszCategory2 = "\\Sort\\Bag"; break;
    case 6:
        switch (g_nEasterLocaleId) {
        case 1: pszCategory2 = "\\Easter\\Dan"; break;
        case 2: pszCategory2 = "\\Easter\\Dut"; break;
        case 4: pszCategory2 = "\\Easter\\Fre"; break;
        case 5: pszCategory2 = "\\Easter\\Ger"; break;
        case 6: pszCategory2 = "\\Easter\\Ita"; break;
        case 7: pszCategory2 = "\\Easter\\Nor"; break;
        case 8: pszCategory2 = "\\Easter\\Spa"; break;
        case 9: pszCategory2 = "\\Easter\\Swe"; break;
        case 0:
        case 3:
        default: pszCategory2 = "\\Easter\\Eng"; break;
        }
        break;
    case 4: pszCategory2 = "\\Att_Out"; break;
    case 5: pszCategory2 = "\\Att_In"; break;
    case 7: pszCategory2 = "\\Design"; break;
    }
    wsprintfA(szPath, "%s%s%s\\%08d.att", g_pInstallPathPrefix, "PostBag", pszCategory2, nId16);
    CopyFileA(pszSrcPath, szPath, 0);
    nResult = nId;

done:
    return nResult;
}

// FUNCTION: LOCO 0x4441c0
// Trivial leaf: computes a 3-channel additive/clamp tint color from 3 input bytes (not real
// HSV/blend math). byG dominates (sets the R/G/B baseline), byB nudges toward red, byR nudges
// toward blue; each channel clamped to [0,255]. Returns packed 0x00RRGGBB. Sole caller is
// DrawCardThumbnail, a PostBagCacheBundle method at 0x4437c0 (untranscribed) that tints
// the card background fill when a flag byte is set.
//
// PARKED (asmscore.py --len 0xec: total 137966, byte_diff 126, insns 68/77): content-complete,
// semantically faithful (params/return traced against raw disasm, not just the decompile).
// Single coherent root cause spanning nearly the whole diff: the original spills nG to a
// stack slot (reserved via a "push ecx"-as-scratch-space trick, no real ecx value preserved --
// stdcall doesn't require it) and keeps nR/nB in ebx/ecx; this compile keeps all 3 channels in
// registers (esi/edi/ecx), needing one more callee-saved push, one fewer stack access. Tried:
// swapping local declaration order (nR,nB,nG) and rewriting the byB/byR branches with compound
// -=/+= operators instead of explicit x = x - y -- both compiled to the same or a marginally
// worse score, no lever found to force the spill. Register-vs-stack-slot allocation choice,
// not a content/structure bug -- see Yoda lesson #13 (slot-vs-register residency) and the
// #29/#30 intrinsic-tie-break family; likely TU-position-dependent (Yoda #7) given this is the
// 39th/last function in this TU. Not re-probed further within the triage budget.
unsigned int PostBagCacheBundle::PostBag_ComputeTintColor(unsigned char byR, unsigned char byG, unsigned char byB)
{
    int nR = 0xff;
    int nG = 0xff;
    int nB = 0xff;
    if (byG != 0) {
        nR = 0xff - byG;
        nB = (byG >> 2) + 0xff;
        nG = (byG >> 1) + 0xff;
    }
    if (byB != 0) {
        nB -= byB;
        nG -= byB >> 1;
        nR += byB / 3;
    }
    if (byR != 0) {
        nG -= byR;
        nR -= byR;
        nB += byR / 3;
    }
    if (nB < 0) {
        nB = 0;
    }
    if (nG < 0) {
        nG = 0;
    }
    if (nR < 0) {
        nR = 0;
    }
    if (nB > 0xff) {
        nB = 0xff;
    }
    if (nG > 0xff) {
        nG = 0xff;
    }
    if (nR > 0xff) {
        nR = 0xff;
    }
    return (nR << 16) | (nG << 8) | nB;
}

// FUNCTION: LOCO 0x4440a0
// Shared placed-item draw primitive for DrawLastPlacedItem (below)/FUN_004437c0
// (DrawCardThumbnail, untranscribed): unpacks pRecord's packed kind/subkind byte +
// variant/grid-offset bytes, resolves the clip-art LocoBitmap via
// ClipartBitmapCache_GetOrLoad, computes a centered dest rect (clipped against rectClip)
// and the matching source-crop rect, then RestoreOverlapBlt's it and releases the bitmap.
//
// nUnused: a genuine 7th stack argument, confirmed via the caller's own push sequence and
// this function's `ret 0x1c` purge (28 bytes = 7 dwords, one more than the 6 originally shown
// by Ghidra's analyzer, which had misidentified rectClip's 4 flattened dwords as loose scalar
// params -- retyped as a RECT-by-value here, matching the caller's own by-value construction).
// Ghidra's dataflow found ZERO reads of nUnused anywhere in the body, so it's a
// dead/vestigial trailing parameter (possibly read by a different, not-yet-found caller), not
// a decompiler under-analysis gap.
//
// PARKED (asmscore.py --len 0x114: total 125230, byte_diff 110, insns 104/105): content-
// complete and semantically faithful -- params, the RECT-by-value clip box, and the hidden
// 7th argument were all traced against the raw disasm, not just the decompile. Residual is
// pervasive register-allocation/scheduling noise spanning almost the entire body (register
// role swaps and a handful of structural substitutions in the destRect/srcRect field-write
// tail), not a content gap -- same class as PostBag_ComputeTintColor just above and
// likely TU-position-dependent (Yoda #7) given this is also near the end of this TU. Not
// probed further within the triage budget; the harder part of this function (recovering the
// true 7-parameter signature) is done, so a future session can focus purely on register-alloc
// levers if picked back up.
void PostBagCacheBundle::DrawPlacedClipartItem(IDirectDrawSurface *pTargetSurface, RECT rectClip, DecalSlot *pRecord, unsigned int nUnused)
{
    LocoBitmap *pBitmap = ClipartBitmapCache_GetOrLoad(pRecord->packedKind >> 3,
                                                             (pRecord->packedKind & 7) + 1,
                                                             pRecord->placementSeq, 0);
    if (pBitmap != NULL) {
        unsigned int nSrcW = pBitmap->width;
        unsigned int nSrcH = pBitmap->height;
        int nDestLeft = (pRecord->xHalf * 2 - (nSrcW >> 1)) + rectClip.left;
        int nDestRight = nDestLeft + nSrcW;
        int nSrcTop = 0;
        int nDestTop = (pRecord->yHalf * 2 - (nSrcH >> 1)) + rectClip.top;
        int nDestBottom = nDestTop + nSrcH;
        int nSrcLeft = 0;
        if (nDestLeft < rectClip.left) {
            nSrcLeft = rectClip.left - nDestLeft;
            nDestLeft = rectClip.left;
        }
        if (nDestTop < rectClip.top) {
            nSrcTop = rectClip.top - nDestTop;
            nDestTop = rectClip.top;
        }
        if (rectClip.right < nDestRight) {
            nSrcW = nSrcW + (rectClip.right - nDestRight);
            nDestRight = rectClip.right;
        }
        if (rectClip.bottom < nDestBottom) {
            nSrcH = nSrcH + (rectClip.bottom - nDestBottom);
            nDestBottom = rectClip.bottom;
        }

        RECT destRect;
        destRect.top = nDestTop;
        destRect.left = nDestLeft;
        destRect.right = nDestRight;
        destRect.bottom = nDestBottom;
        RECT srcRect;
        srcRect.top = nSrcTop;
        srcRect.left = nSrcLeft;
        srcRect.right = nSrcW;
        srcRect.bottom = nSrcH;
        pBitmap->RestoreOverlapBlt(destRect, pTargetSurface, srcRect, 0);
        delete pBitmap;
    }
}

// FUNCTION: LOCO 0x4436c0
// Draws the single most-recently-placed clip-art decal on pCard: a backward do-while scan of
// decalSlots[127..0] for the first slot with placementSeq != 0 (recovered as
// &pCard->decalSlots[i].placementSeq by tracing the caller's own stack layout --
// Ghidra's decompile showed a raw `param_1 + 0x391` byte walk, but the exact +1 offset from
// each candidate record's own base address matches placementSeq precisely, and the whole 128-slot
// span aligns exactly with CarNetState's own decalSlots[128] array at +0x96). On a
// match, forwards it to DrawPlacedClipartItem by value; either way then unconditionally
// frames rectClip with a BLACK_BRUSH border via pTargetSurface->GetDC/FrameRect/ReleaseDC.
//
// Two blocks duplicated verbatim from DrawCardThumbnail (FUN_004437c0, untranscribed):
// the lazy badge-tile realization (pCachedBadgeTileDesc/pRealizedBadgeFrame, tile id
// 0x3cbd) and the LRU access-stamp bump+overflow-reset (nNextAccessCounter/
// aSlotAccessOrder) that ClipartBitmapCache_GetOrLoad's callers are each individually
// responsible for advancing once per external draw call -- same "category switch" inline-
// duplication pattern already documented elsewhere in this cluster (docs/subsystems.md).
//
// rectClip is reused BY ITS OWN ADDRESS for the FrameRect call (not a separate local) --
// confirmed via raw disasm: the `lea ecx,[esp+0x1c]` right before FrameRect computes the exact
// stack address of the incoming by-value rectClip parameter itself, not a copy. Ghidra's own
// decompile mislabels this as `(RECT *)&pCard` and the HDC as `unaff_ESI` (a stack/register-
// tracking artifact once the true parameter list is applied) -- ground-truthed against the
// disasm instead (GetDC's own out-param, loaded via [esp+0xc] right before the call).
//
// PARKED (asmscore.py --len 0xfc: total 109864, byte_diff 94, insns 81/84): content-complete
// and semantically faithful -- the scan index, the hidden RECT-by-address FrameRect call, and
// the GetDC/ReleaseDC pairing were all traced against the raw disasm, not just the decompile.
// Residual is a 1-register prologue difference (original reserves 4 callee-saved/scratch slots
// -- an extra `push ecx` for local_4/hdc's own stack slot, then ebx/esi/edi -- ours reserves
// 3), cascading into every subsequent stack-relative offset. Hoisting `HDC hdc = 0;` to the top
// of the function (matching the original's own immediate post-prologue zero-store, before the
// badge lazy-init block) recovered part of the gap (total 115871->109864) but not the register
// count itself. Same pervasive register-allocation-noise class as this TU's two other tail
// PARKED functions (PostBag_ComputeTintColor, DrawPlacedClipartItem just above) --
// a third confirmation of the "this TU's tail is register-carry-sensitive" hypothesis
// (docs/subsystems.md, Yoda lesson #7). Not probed further within the triage budget.
void PostBagCacheBundle::DrawLastPlacedItem(CarNetState *pCard, IDirectDrawSurface *pTargetSurface, RECT rectClip, unsigned int nUnused)
{
    HDC hdc = 0;

    if (pRealizedBadgeFrame == NULL) {
        pCachedBadgeTileDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cbd);
        pRealizedBadgeFrame = pCachedBadgeTileDesc->GetOrLoadFrameBitmap(0, 0);
    }

    unsigned int nCounter = nNextAccessCounter + 1;
    nNextAccessCounter = nCounter;
    if (nCounter > 0xefffffff) {
        nNextAccessCounter = 1;
        for (int i = 0; i < 256; i++) {
            aSlotAccessOrder[i] = 1;
        }
    }

    int i2 = 127;
    do {
        if (pCard->decalSlots[i2].placementSeq != 0) {
            DrawPlacedClipartItem(pTargetSurface, rectClip, &pCard->decalSlots[i2], nUnused);
            break;
        }
        i2--;
    } while (i2 >= 0);

    pTargetSurface->GetDC(&hdc);
    HBRUSH hbr = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FrameRect(hdc, &rectClip, hbr);
    pTargetSurface->ReleaseDC(hdc);
}

// FUNCTION: LOCO 0x443f00
// Corner "postmark stamp" badge A: kindHi 0x1e ("R%01d%03d.bmp" family, byVariant selects
// the digit). Positions inset (-10,+10) from rectClip's own top-right corner (via a local copy
// of the full rectClip, all 4 fields read).
//
// Recovered the real 6-parameter signature (this, pTargetSurface, RECT rectClip by value,
// bySlotIndex, byVariant, nUnused) by tracing the raw disasm stack layout --
// Ghidra's own analyzer had shown `in_stack_00000018`/`in_stack_0000001c` unresolved byte args
// feeding the ClipartBitmapCache_GetOrLoad call, the same class of under-analysis as
// DrawPlacedClipartItem/DrawLastPlacedItem above. `ret 0x20` (32 bytes = 8 stack
// dwords: pTargetSurface + rectClip's 4 + 2 selector bytes + 1 more) confirms a trailing 8th
// dword nUnused, unread anywhere in the body -- same vestigial trailing param as this
// cluster's other draw primitives.
void PostBagCacheBundle::DrawCornerBadgeA(IDirectDrawSurface *pTargetSurface, RECT rectClip, unsigned char bySlotIndex, unsigned char byVariant, unsigned int nUnused)
{
    unsigned int nCounter = nNextAccessCounter + 1;
    nNextAccessCounter = nCounter;
    if (nCounter > 0xefffffff) {
        nNextAccessCounter = 1;
        for (int i = 0; i < 256; i++) {
            aSlotAccessOrder[i] = 1;
        }
    }

    LocoBitmap *pBitmap = ClipartBitmapCache_GetOrLoad(0x1e, byVariant, bySlotIndex, 0);
    if (pBitmap != NULL) {
        RECT rectBadge;
        CopyRect(&rectBadge, &rectClip);
        rectBadge.left = rectBadge.right - pBitmap->width;
        rectBadge.bottom = rectBadge.top + pBitmap->height;
        OffsetRect(&rectBadge, -10, 10);

        RECT destRect;
        destRect.top = rectBadge.top;
        destRect.left = rectBadge.left;
        destRect.right = rectBadge.right;
        destRect.bottom = rectBadge.bottom;
        RECT srcRect;
        srcRect.top = 0;
        srcRect.left = 0;
        srcRect.right = pBitmap->width;
        srcRect.bottom = pBitmap->height;
        pBitmap->RestoreOverlapBlt(destRect, pTargetSurface, srcRect, 0);
        delete pBitmap;
    }
}

// FUNCTION: LOCO 0x443ff0
// Corner "postmark stamp" badge B: kindHi 0x1f ("S0%03d.bmp" family -- no kindLo needed, the
// format string never references it, so the call hardcodes kindLo=1). Pins exactly at
// rectClip's top-right corner (1px inset), reading only rectClip.top/.right.
//
// Real 5-parameter signature recovered the same way as DrawCornerBadgeA. rectClip.left/
// .bottom occupy real stack slots (the caller passes the identical by-value RECT to both this
// and DrawCornerBadgeA) but are genuinely unread here -- confirmed by Ghidra's own
// analyzer typing those two slots "undefined4" (no discovered use) even after the by-value
// RECT was applied; the raw disasm shows one of them (rectClip.bottom's slot) reused purely as
// dead scratch space for an intermediate sum, never re-read as the parameter. `ret 0x1c`
// (28 bytes = 7 stack dwords: pTargetSurface + rectClip's 4 + 1 selector byte + 1 more)
// confirms the same trailing-unused-dword shape as its sibling.
void PostBagCacheBundle::DrawCornerBadgeB(IDirectDrawSurface *pTargetSurface, RECT rectClip, unsigned char bySlotIndex, unsigned int nUnused)
{
    unsigned int nCounter = nNextAccessCounter + 1;
    nNextAccessCounter = nCounter;
    if (nCounter > 0xefffffff) {
        nNextAccessCounter = 1;
        for (int i = 0; i < 256; i++) {
            aSlotAccessOrder[i] = 1;
        }
    }

    LocoBitmap *pBitmap = ClipartBitmapCache_GetOrLoad(0x1f, 1, bySlotIndex, 0);
    if (pBitmap != NULL) {
        RECT destRect;
        destRect.top = rectClip.top + 1;
        destRect.left = (rectClip.right - 1) - pBitmap->width;
        destRect.right = rectClip.right - 1;
        destRect.bottom = pBitmap->height + rectClip.top + 1;
        RECT srcRect;
        srcRect.top = 0;
        srcRect.left = 0;
        srcRect.right = pBitmap->width;
        srcRect.bottom = pBitmap->height;
        pBitmap->RestoreOverlapBlt(destRect, pTargetSurface, srcRect, 0);
        delete pBitmap;
    }
}

extern unsigned int g_dwTextBufSeed;    // seeds buf1's leading dword before LoadLocaleString overwrites it
extern unsigned int g_dwTextBufSeed2;    // seeds buf2's leading dword before LoadLocaleString overwrites it
extern unsigned short g_wTextBufSeed2Tail;  // seeds buf2's next word before LoadLocaleString overwrites it
extern char g_szDefaultCardNameMaybe[];          // default name string, used when pCard->nameA is empty

// FUNCTION: LOCO 0x4437c0
// Draws one card's full postbag/album thumbnail. Real 6-param/9-stack-dword signature
// re-derived from raw disasm (`ret 0x24` = 36 bytes = 9 dwords, cross-checked against all 3
// call sites' own push sequences -- 0x404956/0x418a76/0x42e846, which agree exactly), since
// Ghidra's own decompile of this function is unreliable even with the correct parameter
// storage applied (a net-esp-confusion bug, same class as BeginEdit
// -- see CLAUDE.md; every `unaff_retaddr`/`unaff_EBP`/misbound `local_*` in the raw decompile
// was ground-truthed against the raw disasm instead, not read off the decompile text).
//
// Lazily realizes the shared "network game count" badge tile (pCachedBadgeTileDesc/
// pRealizedBadgeFrame, tile id 0x3cbd) and bumps the shared LRU access-stamp counter
// exactly like DrawLastPlacedItem -- both blocks duplicated inline verbatim, same
// pattern as this cluster's other "category switch" duplication (docs/subsystems.md). buf1/
// buf2 are pre-seeded from 3 small opaque globals before LoadLocaleString(100/101, ...)
// overwrites them -- a real, disasm-confirmed dead-then-overwritten initializer (same class
// as CLAUDE.md's "big scratch buffer... zeroed... kept even though unread" lesson), not a
// decompiler artifact.
//
// Non-tint branch: white background fill, an optional caller hover-highlight rect (outer
// EDGE_RAISED border) and, gated on that SAME condition, an inset (2px) EDGE_SUNKEN box
// around the about-to-be-drawn name label -- 2 independent highlight-related draws sharing
// one condition. pCard->szDescription (word-wrapped description, DT_WORDBREAK|DT_NOPREFIX|
// DT_EDITCONTROL) fills the left half. A divider-line grid then lays out the right column:
// a vertical center divider, the name (nameA, falling back to a shared default string
// g_szDefaultCardNameMaybe when empty) with locale-string label buf1 above it, buf2's label below the
// center, and nameB at the bottom -- each row separated by a horizontal divider that is
// SKIPPED whenever pHighlightRect is non-NULL (2 of the 3 horizontal dividers are
// conditional this way; the 3rd, between buf2's row and nameB's row, is unconditional).
// g_UIResources.m_hFont14 (the label/name font) is deliberately re-selected before each of its 3 uses
// rather than cached, and is saved/restored TWICE (once right after buf2's row, again after
// nameB's row selects it back) -- a literal, faithfully-reproduced redundancy, not a
// transcription bug. The one CreatePen'd divider pen and the SetTextColor/SetBkMode set for
// this whole label grid are each saved once up front and restored once at the very end.
//
// Common tail (both branches): pTargetSurface->ReleaseDC, then bTintFillOnly is TESTED
// A SECOND TIME (not a separate misbound local, as an earlier pass of this investigation
// suspected -- it's a genuine re-read of the same parameter) to pick either the up-to-128-
// slot DrawPlacedClipartItem loop (a FORWARD scan stopping at the first EMPTY slot --
// unlike DrawLastPlacedItem's backward "last occupied slot" scan) or the 2 corner
// "postmark stamp" badges (DrawCornerBadgeB then AMaybe, using pCard->byStampSlotB/94/95 as
// their selector bytes). If pCard->wAttachmentId != 0, the realized badge tile is stamped via
// RestoreOverlapBlt at a fixed (rectClip.left+20, rectClip.top-11) offset (same idiom as
// DrawLastPlacedItem's own badge-tile draw). Finally re-acquires the DC for one last
// FrameRect(&rectClip, BLACK_BRUSH) highlight (same idiom as DrawLastPlacedItem's own
// trailing frame) and deletes the background brush. Called from 3 different windows
// (DrawOrEraseCardSlot/init-area, FUN_004189a0/EditCardWnd-area, FUN_0042e760/MailWnd-area, all
// still untranscribed).
void PostBagCacheBundle::DrawCardThumbnail(unsigned char bTintFillOnly, CarNetState *pCard,
                                                IDirectDrawSurface *pTargetSurface, RECT rectClip,
                                                unsigned int nUnused, RECT *pHighlightRect)
{
    char buf1[16];
    memcpy(buf1, &g_dwTextBufSeed, sizeof(g_dwTextBufSeed));
    memset(buf1 + sizeof(g_dwTextBufSeed), 0, sizeof(buf1) - sizeof(g_dwTextBufSeed));
    char buf2[16];
    memcpy(buf2, &g_dwTextBufSeed2, sizeof(g_dwTextBufSeed2));
    memcpy(buf2 + sizeof(g_dwTextBufSeed2), &g_wTextBufSeed2Tail, sizeof(g_wTextBufSeed2Tail));
    memset(buf2 + sizeof(g_dwTextBufSeed2) + sizeof(g_wTextBufSeed2Tail), 0,
           sizeof(buf2) - sizeof(g_dwTextBufSeed2) - sizeof(g_wTextBufSeed2Tail));

    HBRUSH hbrBg = CreateSolidBrush(0xe6e6e6);

    if (pRealizedBadgeFrame == NULL) {
        pCachedBadgeTileDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cbd);
        pRealizedBadgeFrame = pCachedBadgeTileDesc->GetOrLoadFrameBitmap(0, 0);
    }

    g_UIResources.LoadLocaleString(100, buf1, sizeof(buf1));
    g_UIResources.LoadLocaleString(101, buf2, sizeof(buf2));

    unsigned int nCounter = nNextAccessCounter + 1;
    nNextAccessCounter = nCounter;
    if (nCounter > 0xefffffff) {
        nNextAccessCounter = 1;
        for (int i = 0; i < 256; i++) {
            aSlotAccessOrder[i] = 1;
        }
    }

    HDC hdc;
    pTargetSurface->GetDC(&hdc);
    if (bTintFillOnly) {
        unsigned int tintColor = this->PostBag_ComputeTintColor(pCard->byIdentityColorR, pCard->byIdentityColorG, pCard->byIdentityColorB);
        HBRUSH hbrTint = CreateSolidBrush(tintColor);
        FillRect(hdc, &rectClip, hbrTint);
        DeleteObject(hbrTint);
    } else {
        FillRect(hdc, &rectClip, (HBRUSH)GetStockObject(WHITE_BRUSH));
        if (pHighlightRect != NULL) {
            FillRect(hdc, pHighlightRect, hbrBg);
            DrawEdge(hdc, pHighlightRect, EDGE_RAISED, BF_RECT);
        }

        RECT rectText;
        rectText.left = rectClip.left + 10;
        rectText.top = rectClip.top + 10;
        rectText.right = rectClip.left + (rectClip.right - rectClip.left) / 2 - 10;
        rectText.bottom = rectClip.bottom - 10;

        if (pHighlightRect != NULL) {
            RECT rectHL = rectText;
            InflateRect(&rectHL, 2, 2);
            FillRect(hdc, &rectHL, hbrBg);
            DrawEdge(hdc, &rectHL, EDGE_SUNKEN, BF_RECT);
        }

        if (strlen(pCard->szDescription) > 0) {
            COLORREF oldColor = SetTextColor(hdc, 0xff5c00);
            int oldBkMode = SetBkMode(hdc, TRANSPARENT);
            HGDIOBJ oldFont = SelectObject(hdc, g_UIResources.m_hFont16);
            DrawTextA(hdc, pCard->szDescription, -1, &rectText, DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
            SelectObject(hdc, oldFont);
            SetTextColor(hdc, oldColor);
            SetBkMode(hdc, oldBkMode);
        }

        HPEN hPen = CreatePen(PS_SOLID, 2, 0x808080);
        HGDIOBJ hOldPen = SelectObject(hdc, hPen);

        int midX = rectClip.left + (rectClip.right - rectClip.left) / 2;
        MoveToEx(hdc, midX, rectClip.top + 10, NULL);
        LineTo(hdc, midX, rectClip.bottom - 10);

        int midY = rectClip.top + (rectClip.bottom - rectClip.top) / 2;

        int x2;
        if (pHighlightRect == NULL) {
            MoveToEx(hdc, midX + 10, midY, NULL);
            LineTo(hdc, rectClip.right - 20, midY);
            x2 = midX + 10;
        } else {
            x2 = midX + 14;
        }

        RECT rectName;
        rectName.left = x2;
        rectName.top = midY - 20;
        rectName.right = rectClip.right - 20;
        rectName.bottom = midY;
        HGDIOBJ oldFont2 = SelectObject(hdc, g_UIResources.m_hFont14);
        COLORREF oldColor2 = SetTextColor(hdc, 0xff5c00);
        int oldBkMode2 = SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, (strlen(pCard->nameA) > 0) ? pCard->nameA : g_szDefaultCardNameMaybe, -1, &rectName, 0);

        if (pHighlightRect == NULL) {
            MoveToEx(hdc, midX + 10, midY - 30, NULL);
            LineTo(hdc, rectClip.right - 20, midY - 30);
        }

        RECT rectLabel1;
        rectLabel1.left = midX + 10;
        rectLabel1.top = midY - 50;
        rectLabel1.right = rectClip.right - 20;
        rectLabel1.bottom = midY - 30;
        SelectObject(hdc, g_UIResources.m_hFont14);
        DrawTextA(hdc, buf1, -1, &rectLabel1, 0);

        MoveToEx(hdc, midX + 10, midY + 30, NULL);
        LineTo(hdc, rectClip.right - 20, midY + 30);

        RECT rectLabel2;
        rectLabel2.left = midX + 10;
        rectLabel2.top = midY + 10;
        rectLabel2.right = rectClip.right - 20;
        rectLabel2.bottom = midY + 30;
        DrawTextA(hdc, buf2, -1, &rectLabel2, 0);
        SelectObject(hdc, oldFont2);

        MoveToEx(hdc, midX + 10, midY + 60, NULL);
        LineTo(hdc, rectClip.right - 20, midY + 60);

        SelectObject(hdc, g_UIResources.m_hFont14);
        RECT rectNameB;
        rectNameB.left = midX + 10;
        rectNameB.top = midY + 40;
        rectNameB.right = rectClip.right - 20;
        rectNameB.bottom = midY + 60;
        DrawTextA(hdc, pCard->nameB, -1, &rectNameB, 0);

        SelectObject(hdc, oldFont2);
        SelectObject(hdc, hOldPen);
        SetTextColor(hdc, oldColor2);
        SetBkMode(hdc, oldBkMode2);
        DeleteObject(hPen);
    }
    pTargetSurface->ReleaseDC(hdc);

    if (bTintFillOnly) {
        for (int i = 0; i < 128; i++) {
            if (pCard->decalSlots[i].placementSeq == 0) {
                break;
            }
            DrawPlacedClipartItem(pTargetSurface, rectClip, &pCard->decalSlots[i], nUnused);
        }
    } else {
        DrawCornerBadgeB(pTargetSurface, rectClip, pCard->byStampSlotB, nUnused);
        DrawCornerBadgeA(pTargetSurface, rectClip, pCard->byStampSlotA, pCard->byStampVariantA, nUnused);
    }

    if (pCard->wAttachmentId != 0) {
        LocoBitmap *pBadge = (LocoBitmap *)pRealizedBadgeFrame;
        RECT destRect;
        destRect.left = rectClip.left + 20;
        destRect.top = rectClip.top - 11;
        destRect.right = destRect.left + pBadge->width;
        destRect.bottom = destRect.top + pBadge->height;
        RECT srcRect;
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = pBadge->width;
        srcRect.bottom = pBadge->height;
        pBadge->RestoreOverlapBlt(destRect, pTargetSurface, srcRect, 0);
    }

    pTargetSurface->GetDC(&hdc);
    FrameRect(hdc, &rectClip, (HBRUSH)GetStockObject(BLACK_BRUSH));
    pTargetSurface->ReleaseDC(hdc);
    DeleteObject(hbrBg);
}

// ---- declarations to prepend before the function ----

extern void AppWindow_SetScreenState(int newState); // 0x408130, see src/AlbumCardWnd.cpp
extern IDirectDraw2 *g_pDDraw2;                     // DAT_00485440
extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4
extern int g_nSurfaceFormatTag; // DAT_00485274 -- 0x22b=555bpp, 0x235=565bpp, see LocoBitmap.cpp

// FUNCTION: LOCO 0x417040 (Ghidra: EditCardWnd::FUN_00417040, WindowBase vtable+0x54 override
// -- WM_KEYDOWN; declared in src/EditCardWnd.h). RETURN/ESCAPE both act as clicking the exit
// button: restore the button's help-canvas-offset backdrop, flash its pressed frame for 150ms,
// then end the edit session back to screen state 3 -- the keyboard twin of OnLButtonDown's
// PtInRect(&pExitBtn->rect) arm below, same mode 8/10 gate. Everything else falls to
// DefWindowProcA.
// EFFECTIVE MATCH (301 B vs 305, insns 99/101, total 71272). Both mode gates, the wParam
// 0xd/0x1b test, the DefWindowProcA tail, the DrawFrame/CommitRectUpdate pExitBtn re-reads,
// the Sleep/virtual EndActiveSession/SetScreenState tail and the srcRect local->outgoing copy
// all pair; the whole residual is /Og scheduling inside the exit block: the original reserves
// the outgoing srcRect area (`sub esp,0x10`) BEFORE computing the offset fields and loads the
// four rect fields in memory order (eax,ecx,edx,edi), this build computes two fields first and
// loads left,right,top,bottom into ecx,edi,edx,eax; plus the EndActiveSession vtable-pointer
// edx-vs-eax coin flip. Levers that DID pay (baked in, do not undo): (a) a named `destRect`
// local (pRes->rect field copies, register-promoted) is what reproduces the original's
// memory-resident srcRect -- without it cl forwards the computed fields straight to the
// outgoing area (279 B, insns 97/101, total 157647); (b) DrawFrame/CommitRectUpdate written
// as direct `pExitBtn->` uses, NOT through pRes (the original re-reads [esi+0x2c4] per
// statement). Probes refuted (do not re-run): full direct `pExitBtn->` everywhere (230/236);
// pRes for all three (162 but insns 97/101, total 157647 -- shorter code, WORSE structure);
// &srcRect pointer spelling (byte-identical to plain); destRect/srcRect statement-order swaps
// (306 B, total 77304 / byte-identical). See docs/PARKED.md.
LRESULT EditCardWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (nEditMode != 8 && nEditMode != 10) {
        if (wParam != 0xd && wParam != 0x1b) {
            return DefWindowProcA(hwndMsg, msg, wParam, lParam);
        }
        ResourceRef *pRes = pExitBtn;
        RECT destRect;
        destRect.left = pRes->rect.left;
        destRect.right = pRes->rect.right;
        destRect.top = pRes->rect.top;
        destRect.bottom = pRes->rect.bottom;
        RECT srcRect;
        srcRect.left = destRect.left + rectHelpCanvas.left;
        srcRect.right = destRect.right + rectHelpCanvas.left;
        srcRect.top = destRect.top + rectHelpCanvas.top;
        srcRect.bottom = destRect.bottom + rectHelpCanvas.top;
        pHelpBitmapCache->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
        pExitBtn->DrawFrame(1, NULL);
        CommitRectUpdate(pExitBtn->rect);
        Sleep(0x96);
        this->EndActiveSession();
        AppWindow_SetScreenState(3);
    }
    return 0;
}

// FUNCTION: LOCO 0x418210 (WindowBase vtable+0x20 override
// -- the "window just became active, redraw yourself" hook; base default is the shared
// NoOpVirtualMaybe body, ground-truthed against this class's vtable at 0x477930). Full-client
// redraw on (re)activation: restores the help-canvas backdrop over the whole clip bounds
// (destRect = rectClipBounds, srcRect = rectHelpCanvas -- the canvas is stored at the window's
// own origin, so the two rects line up 1:1), repaints the identity preview, roster list and
// all 10 color-swatch buttons, re-syncs the channel bars and picker-button highlights, then
// either resets the decal picker from scratch (nDecalPickerScrollB < 0: surfaces not yet
// built -- zero their dirty flags and scroll offset and re-select the pending subkind) or
// redraws the existing picker page and its scroll button. Finally, if field_0xf0 is set
// (focus-restore request), retakes focus and offers tutorial code 3 -- a launched tutorial
// forces nEditMode = 10 (the "inactive" mode OnKeyDown/OnLButtonDown both early-out on).
void EditCardWnd::OnActivate(int reservedMaybe)
{
    pHelpBitmapCache->RestoreOverlapBlt(rectClipBounds, g_pDDrawWorkSurface, rectHelpCanvas, 1);
    this->RedrawIdentityPreview();
    this->RedrawRosterList();
    for (int i = 0; i < 10; i++) {
        paColorSwatchBtn[i]->DrawFrame(0, NULL);
    }
    this->RedrawColorChannelBars(1);
    this->SyncDecalPickerButtonHighlights();
    if (nDecalPickerScrollB < 0) {
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        field_0x594 = 0;
        field_0x59c = 0;
        nDecalPickerRowOffset = 0;
        this->SelectDecalSubkind(byDecalSubkindPending, 1);
    } else {
        this->RedrawDecalThumbnailGrid(NULL, 0);
        pBtnResMaybe_3cbe->DrawFrame(0, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    }
    if (field_0xf0 != 0) {
        SetFocus(hwndSelf);
        field_0xf0 = 0;
        if (g_pTutorialWnd->NotifyOrLaunch(3, 0) != 0) {
            nEditMode = 10;
        }
    }
}

// The OnLButtonDown callee audit's original "genuinely __fastcall free function, not
// a real EditCardWnd method" hypothesis is now fully refuted for all 3 of its candidates
// (0x4189a0, 0x41a050, and 0x419680 -- the last one this session): every one turned out to be
// an ordinary __thiscall EditCardWnd method once retyped (Ghidra's original __fastcall(int)/
// (int*)/(void*) framing was just an under-analyzed prototype, not a real non-this calling
// convention). See RedrawIdentityPreview/ArmDecalHitTestMode/RedrawRosterList
// below.

// This class's own vtable slot 4 (EndActiveSession, 0x416f70) and slot 0xc (NOT overridden --
// resolves to the inherited WindowBase::RequestModeTransitionFromSource, 0x425fd0) are called
// GENERICALLY (through the vtable, not explicit qualification) throughout
// OnLButtonDown below -- ordinary unqualified virtual calls since 2026-07-21, when
// WindowBase's vtable was modeled through slot 0x1c (ground-truthed against this class's real
// vtable at 0x477930: slot0=scalar dtor, slot4=EndActiveSession/0x416f70, slot8=BeginModalCapture
// (inherited, unused here), slot0xc=RequestModeTransitionFromSource/0x425fd0,
// slot0x10=ScheduleModeTransition/0x426020 inherited).

// FUNCTION: LOCO 0x41ac10
LRESULT EditCardWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bDecalTimerArmedMaybe = true;
    int nMode = nEditMode;
    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;
    HWND hwndTarget;
    ResourceRef *pRes;

    if (nMode == 8) {
        return 0;
    }
    if (nMode == 10) {
        return 0;
    }
    if (nMode == 4) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        nEditMode = 1;
        ShowWindow(hwndEdit, 0);
        SetFocus(hwndSelf);
        GetWindowTextA(hwndEdit, pIdentityTextBuffer->szDescription, 0x50);
        this->RedrawIdentityPreview();
        CommitRectUpdate(rectIdentityPreview);
    }
    if (PtInRect(&rectDescriptionHitZone, pt) && bLocalCardBuilt == false) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        nEditMode = 4;
        SetWindowTextA(hwndEdit, strcmp(pIdentityTextBuffer->szDescription, &g_szDefaultDescriptionMaybe) == 0 ? "" : pIdentityTextBuffer->szDescription);
        SendMessageA(hwndEdit, 0xb1, 0, -1);
        ShowWindow(hwndEdit, 5);
        SetFocus(hwndEdit);
        return 0;
    }
    if (PtInRect(&rectRosterPanel, pt) && bLocalCardBuilt == false) {
        if (nEditMode == 7) {
            g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
            this->HandleRosterClick(pt.x, pt.y);
            return 0;
        }
        if (PtInRect(&rectRosterBadge, pt)) {
            g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
            this->BuildPlayerRosterList();
            if (hDecalHitTestTimer != 0) {
                KillTimer(hwndSelf, hDecalHitTestTimer);
                hDecalHitTestTimer = 0;
            }
            nEditMode = 1;
            nSelectedDecalSlot = -1;
            bCursorAnimStopped = 0;
            Unk0x40 = 0;
            pDecalHitTestToggleBtn->DrawFrame(0, NULL);
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            nEditMode = 7;
            bNoMoreRosterRows = 0;
            field_0x170 = 0;
            field_0x174 = 0;
            this->RedrawRosterList();
            CommitRectUpdate(rectIdentityPreview);
            return 0;
        }
    }
    if (nEditMode == 7) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        nEditMode = 1;
        this->RedrawIdentityPreview();
        CommitRectUpdate(rectIdentityPreview);
    }
    if (PtInRect(&pExitBtn->rect, pt)) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        pRes = pExitBtn;
        RECT srcRect;
        srcRect.left = pRes->rect.left + rectHelpCanvas.left;
        srcRect.right = pRes->rect.right + rectHelpCanvas.left;
        srcRect.top = pRes->rect.top + rectHelpCanvas.top;
        srcRect.bottom = pRes->rect.bottom + rectHelpCanvas.top;
        pHelpBitmapCache->RestoreOverlapBlt(pRes->rect, g_pDDrawWorkSurface, srcRect, 1);
        pRes->DrawFrame(1, NULL);
        CommitRectUpdate(pRes->rect);
        Sleep(0x96);
        this->EndActiveSession();
        AppWindow_SetScreenState(3);
        return 0;
    }
    if (PtInRect(&pNetworkRosterBtn->rect, pt) && g_pDPlaySessionMgr->connectionMode == 2) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        if (pIdentityTextBuffer->wAttachmentId == 0) {
            pNetworkRosterBtn->DrawFrame(1, NULL);
            CommitRectUpdate(pNetworkRosterBtn->rect);
            if (hDecalHitTestTimer != 0) {
                KillTimer(hwndSelf, hDecalHitTestTimer);
                hDecalHitTestTimer = 0;
            }
            nEditMode = 1;
            nSelectedDecalSlot = -1;
            bCursorAnimStopped = 0;
            Unk0x40 = 0;
            pDecalHitTestToggleBtn->DrawFrame(0, NULL);
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            PostMessageA(hwndSelf, 0x5f5, 0, 0);
            PostMessageA(hwndSelf, 0x5f6, 0, 0);
            return 0;
        }
        pNetworkRosterBtn->DrawFrame(0, NULL);
        g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pIdentityTextBuffer->wAttachmentId);
        pIdentityTextBuffer->wAttachmentId = 0;
        this->RedrawIdentityPreview();
        CommitRectUpdate(rectIdentityPreview);
        CommitRectUpdate(pNetworkRosterBtn->rect);
        return 0;
    }
    if (!PtInRect(&pDecalHitTestToggleBtn->rect, pt)) {
        if (!PtInRect(&pAlbumBtn->rect, pt)) {
            if (PtInRect(&pMailBtn->rect, pt) && nEditMode != 6) {
                g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                pMailBtn->DrawFrame(1, NULL);
                CommitRectUpdate(pMailBtn->rect);
                Sleep(0x96);
                ((MailWndVtblProbe *)g_pMailWnd)->Refresh();
                this->EndActiveSession();
                return 0;
            }
            if (!PtInRect(&rectSendButton, pt) &&
                (!PtInRect(&pMailBtn->rect, pt) || nEditMode != 6)) {
                if (PtInRect(&pDeleteBtn->rect, pt) && bPickerVsRosterGate == 0) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    pRes = pDeleteBtn;
                    RECT srcRect;
                    srcRect.left = pRes->rect.left + rectHelpCanvas.left;
                    srcRect.right = pRes->rect.right + rectHelpCanvas.left;
                    srcRect.top = pRes->rect.top + rectHelpCanvas.top;
                    srcRect.bottom = pRes->rect.bottom + rectHelpCanvas.top;
                    pHelpBitmapCache->RestoreOverlapBlt(pRes->rect, g_pDDrawWorkSurface, srcRect, 0);
                    pRes->DrawFrame(1, NULL);
                    CommitRectUpdate(pRes->rect);
                    if (hDecalHitTestTimer != 0) {
                        KillTimer(hwndSelf, hDecalHitTestTimer);
                        hDecalHitTestTimer = 0;
                    }
                    nEditMode = 1;
                    nSelectedDecalSlot = -1;
                    bCursorAnimStopped = 0;
                    Unk0x40 = 0;
                    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                    Sleep(0x96);
                    bLocalCardBuilt = bLocalCardBuilt == false;
                    this->RedrawIdentityPreview();
                    CommitRectUpdate(rectIdentityPreview);
                    pRes = pDeleteBtn;
                    RECT srcRect2;
                    srcRect2.left = pRes->rect.left + rectHelpCanvas.left;
                    srcRect2.right = pRes->rect.right + rectHelpCanvas.left;
                    srcRect2.top = pRes->rect.top + rectHelpCanvas.top;
                    srcRect2.bottom = pRes->rect.bottom + rectHelpCanvas.top;
                    pHelpBitmapCache->RestoreOverlapBlt(pRes->rect, g_pDDrawWorkSurface, srcRect2, 0);
                    pRes->DrawFrame(0, NULL);
                    CommitRectUpdate(pRes->rect);
                    unsigned char bSubkind;
                    if (bLocalCardBuilt == false) {
                        byDecalKindPending = 1;
                        byDecalSubkindCommitted = byDecalSubkindPending;
                        bSubkind = 0x1f;
                    } else {
                        bSubkind = byDecalSubkindCommitted;
                    }
                    this->SelectDecalSubkind(bSubkind, 0);
                    if (bLocalCardBuilt != false) {
                        return 0;
                    }
                    nEditMode = 4;
                    SetWindowTextA(hwndEdit, strcmp(pIdentityTextBuffer->szDescription, &g_szDefaultDescriptionMaybe) == 0 ? "" : pIdentityTextBuffer->szDescription);
                    SendMessageA(hwndEdit, 0xb1, 0, -1);
                    ShowWindow(hwndEdit, 5);
                    SetFocus(hwndEdit);
                    return 0;
                }
                if (PtInRect(&pDecalScrollForwardBtn->rect, pt) && bLocalCardBuilt != false && field_0x2b4 != 0) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    if (nEditMode == 9) {
                        if (hDecalHitTestTimer != 0) {
                            KillTimer(hwndSelf, hDecalHitTestTimer);
                            hDecalHitTestTimer = 0;
                        }
                        nEditMode = 1;
                        bCursorAnimStopped = 0;
                        Unk0x40 = 0;
                        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                    }
                    if (hDecalHitTestTimer != 0) {
                        KillTimer(hwndSelf, hDecalHitTestTimer);
                        hDecalHitTestTimer = 0;
                    }
                    nEditMode = 1;
                    nSelectedDecalSlot = -1;
                    bCursorAnimStopped = 0;
                    Unk0x40 = 0;
                    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                    pDecalScrollForwardBtn->DrawFrame(1, NULL);
                    CommitRectUpdate(pDecalScrollForwardBtn->rect);
                    unsigned int bMoved = this->StepDecalPickerScroll(1);
                    if ((char)bMoved == '\0') {
                        Sleep(0x96);
                    }
                    this->AnimateDecalPickerPageWipe(1, 1);
                    return 0;
                }
                if (PtInRect(&pDecalScrollBackBtn->rect, pt) && bLocalCardBuilt != false && field_0x2b5 != 0) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    if (nEditMode == 9) {
                        if (hDecalHitTestTimer != 0) {
                            KillTimer(hwndSelf, hDecalHitTestTimer);
                            hDecalHitTestTimer = 0;
                        }
                        nEditMode = 1;
                        bCursorAnimStopped = 0;
                        Unk0x40 = 0;
                        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                    }
                    if (hDecalHitTestTimer != 0) {
                        KillTimer(hwndSelf, hDecalHitTestTimer);
                        hDecalHitTestTimer = 0;
                    }
                    nEditMode = 1;
                    nSelectedDecalSlot = -1;
                    bCursorAnimStopped = 0;
                    Unk0x40 = 0;
                    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                    pDecalScrollBackBtn->DrawFrame(1, NULL);
                    CommitRectUpdate(pDecalScrollBackBtn->rect);
                    unsigned int bMoved = this->StepDecalPickerScroll(0);
                    if ((char)bMoved == '\0') {
                        Sleep(0x96);
                    }
                    this->AnimateDecalPickerPageWipe(0, 1);
                    return 0;
                }
                if (PtInRect(&rectDecalKindRow, pt) && bLocalCardBuilt != false) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    this->CycleDecalKindHighlight(pt);
                    return 0;
                }
                if (PtInRect(&pBtnResMaybe_3cc2->rect, pt)) {
                    pBtnResMaybe_3cc2->DrawFrame(1, NULL);
                    CommitRectUpdate(pBtnResMaybe_3cc2->rect);
                    {
                        unsigned int nRand = rand();
                        g_UIResources.PlaySoundAtScreenPos((int)nRand % 10 + 0x5114, pBtnResMaybe_3cc2->rect.left, pBtnResMaybe_3cc2->rect.top, 4);
                    }
                    Sleep(0x96);
                    pBtnResMaybe_3cc2->DrawFrame(0, NULL);
                    pRes = pBtnResMaybe_3cc2;
commitAndReturn:
                    CommitRectUpdate(pRes->rect);
                    return 0;
                }
                if (PtInRect(&pBtnResMaybe_3cbe->rect, pt)) {
                    unsigned int nRand = rand();
                    g_UIResources.PlaySoundAtScreenPos((int)nRand / 0x1999 + 0x526c, pBtnResMaybe_3cc2->rect.left, pBtnResMaybe_3cc2->rect.top, 4);
                    return 0;
                }
                if (PtInRect(&rectDecalCategoryGrid, pt) && bLocalCardBuilt != false) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    unsigned char nCat = 0;
                    do {
                        if (PtInRect(&paDecalCategoryBtn[nCat]->rect, pt)) {
                            if (byDecalSubkindPending == nCat) {
                                return 0;
                            }
                            if (bPickerVsRosterGate != 0) {
                                nCat += 0x10;
                            }
                            this->SelectDecalSubkind(nCat, 0);
                            return 0;
                        }
                        nCat++;
                    } while (nCat < 0x10);
                    return 0;
                }
                if (PtInRect(&rectDecalPickerGrid, pt)) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    if (nDecalPickerScrollA < 0) {
                        return 0;
                    }
                    if (nDecalPickerScrollB < 0) {
                        return 0;
                    }
                    int nSpan = nDecalPickerScrollA - nDecalPickerScrollB;
                    if (nSpan < 0) {
                        return 0;
                    }
                    for (int i = 0; i <= nSpan; i++) {
                        if (PtInRect(&aDecalPickerRowRect[i], pt)) {
                            this->SelectDecalSlot(i + nDecalPickerScrollB);
                            return 0;
                        }
                    }
                    return 0;
                }
                if ((PtInRect(&rectColorChannelBar2, pt) || PtInRect(&pBlueWheelBtn->rect, pt)) && bLocalCardBuilt != false) {
                    this->AdjustIdentityColorChannel(2, 1, pt.x, pt.y);
                    return 0;
                }
                if ((PtInRect(&rectColorChannelBar0, pt) || PtInRect(&pRedWheelBtn->rect, pt)) && bLocalCardBuilt != false) {
                    this->AdjustIdentityColorChannel(0, 1, pt.x, pt.y);
                    return 0;
                }
                if ((PtInRect(&rectColorChannelBar1, pt) || PtInRect(&pYellowWheelBtn->rect, pt)) && bLocalCardBuilt != false) {
                    this->AdjustIdentityColorChannel(1, 1, pt.x, pt.y);
                    return 0;
                }
                if (PtInRect(&rectColorSwatchRow, pt) && bLocalCardBuilt != false) {
                    g_UIResources.PlaySoundAtScreenPos(0x527a, rectColorSwatchRow.left, rectColorSwatchRow.top, 4);
                    this->ApplyColorSwatchPreset(pt);
                    return 0;
                }
                RECT *pArtRect = &rectIdentityPreview;
                if (PtInRect(pArtRect, pt) && bLocalCardBuilt != false) {
                    if (nEditMode == 9) {
                        if (nClickSoundCooldown < 1) {
                            nClickSoundCooldown = 10;
                            g_UIResources.PlayUiSound(0x501f);
                        }
                        unsigned char bHit = pIdentityTextBuffer->RemoveDecalAtPoint(pt.x - pArtRect->left, pt.y - rectIdentityPreview.top);
                        if (bHit == 0) {
                            return 0;
                        }
                        this->RedrawIdentityPreview();
                        RECT rectDirty;
                        rectDirty.top = rectIdentityPreview.top;
                        rectDirty.left = pArtRect->left;
                        rectDirty.right = rectIdentityPreview.right;
                        rectDirty.bottom = rectIdentityPreview.bottom;
                        InflateRect(&rectDirty, 0x32, 0x32);
                        CommitRectUpdate(rectDirty);
                        return 0;
                    }
                    if (nEditMode == 2) {
                        unsigned int nRand = rand();
                        g_UIResources.PlaySoundAtScreenPos(aRosterLabelStringId[(int)nRand % 0x11], pt.x, pt.y, 4);
                        if (pIdentityTextBuffer->wAttachmentId != 0) {
                            g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pIdentityTextBuffer->wAttachmentId);
                            pIdentityTextBuffer->wAttachmentId = 0;
                            pNetworkRosterBtn->DrawFrame(0, NULL);
                            this->RedrawIdentityPreview();
                        }
                        LocoBitmap *pThumb = paDecalThumbCache[nSelectedDecalSlot - nDecalPickerScrollB];
                        unsigned char bPlaced = pIdentityTextBuffer->AddDecal(
                            byDecalSubkindPending, byDecalKindPending,
                            (char)nSelectedDecalSlot + 1,
                            pt.x - pArtRect->left, pt.y - rectIdentityPreview.top,
                            *(unsigned char *)&pThumb->height,   // sic: reads height's low byte only (LocoBitmap.h)
                            pThumb->bOwnsPalette);                // sic: reads an unrelated flag field, see docs/engine-bugs.md
                        if (bPlaced == 0 || bLocalCardBuilt == false) {
                            this->RedrawIdentityPreview();
                        } else {
                            field_0x189 = 1;
                            g_pPostBagCache->DrawLastPlacedItem(pIdentityTextBuffer, g_pDDrawWorkSurface, *pArtRect, (unsigned int)hwndSelf);
                        }
                        RECT rectDirty;
                        rectDirty.top = rectIdentityPreview.top;
                        rectDirty.left = pArtRect->left;
                        rectDirty.right = rectIdentityPreview.right;
                        rectDirty.bottom = rectIdentityPreview.bottom;
                        InflateRect(&rectDirty, 0x32, 0x32);
                        CommitRectUpdate(rectDirty);
                        return 0;
                    }
                }
                if (PtInRect(pArtRect, pt) && nEditMode == 1) {
                    g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                    this->RequestModeTransitionFromSource(pPreviewIconRealized, pPreviewIconDesc, 0, 1);
                    nEditMode = 6;
                    return 0;
                }
                if (nEditMode != 6) {
                    return 0;
                }
                g_UIResources.PlayUiSound(0x5015);
                if (hDecalHitTestTimer != 0) {
                    KillTimer(hwndSelf, hDecalHitTestTimer);
                    hDecalHitTestTimer = 0;
                }
                nEditMode = 1;
                nSelectedDecalSlot = -1;
                bCursorAnimStopped = 0;
                Unk0x40 = 0;
                pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            } else {
                g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                pPostBtn->DrawFrame(1, NULL);
                CommitRectUpdate(pPostBtn->rect);
                Sleep(0x96);
                if (pIdentityTextBuffer->nameA[0] == 0) {
                    MessageBeep(0x30);
                    if (hDecalHitTestTimer != 0) {
                        KillTimer(hwndSelf, hDecalHitTestTimer);
                        hDecalHitTestTimer = 0;
                    }
                    nEditMode = 1;
                    nSelectedDecalSlot = -1;
                    bCursorAnimStopped = 0;
                    Unk0x40 = 0;
                    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                    bNoMoreRosterRows = 0;
                    field_0x170 = 0;
                    field_0x174 = 0;
                    bool bWasBuilt = bLocalCardBuilt != false;
                    if (bWasBuilt) {
                        bLocalCardBuilt = false;
                        this->RedrawIdentityPreview();
                    }
                    nEditMode = 7;
                    this->RedrawRosterList();
                    // sic: reuses the click-point local rather than a fresh one -- that single
                    // address-taken use is what forces `pt` into memory for the WHOLE function
                    // (~119 reloads); a separate local here lets VC5 enregister it and loses
                    // ~150 instructions of reload traffic.
                    pt.x = rectRosterPanel.left + 0x19;
                    pt.y = rectRosterPanel.top + 0x46;
                    ClientToScreen(hwndSelf, &pt);
                    SetCursorPos(pt.x, pt.y);
                    CommitScreenUpdate(hwndSelf, NULL, 0, &rectIdentityPreview);
                    if (bWasBuilt) {
                        byDecalSubkindCommitted = byDecalSubkindPending;
                        this->SelectDecalSubkind(0x1f, 0);
                    }
                    nEditMode = 7;
                    pPostBtn->DrawFrame(0, NULL);
                    pRes = pPostBtn;
                    goto commitAndReturn;
                }
                g_UIResources.PlaySoundAtScreenPos(0x5273, pt.x, pt.y, 4);
                g_pPostBagCache->PostBag_SaveCardToCategory(pIdentityTextBuffer, 2, NULL);
                if (hDecalHitTestTimer != 0) {
                    KillTimer(hwndSelf, hDecalHitTestTimer);
                    hDecalHitTestTimer = 0;
                }
                nEditMode = 1;
                nSelectedDecalSlot = -1;
                bCursorAnimStopped = 0;
                Unk0x40 = 0;
                pDecalHitTestToggleBtn->DrawFrame(0, NULL);
                this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                pPostBtn->DrawFrame(0, NULL);
                pNetworkRosterBtn->DrawFrame(0, NULL);
                CommitScreenUpdate(hwndSelf, NULL, 0, NULL);

                // sic: the values below are stale reads left over from the vtable-slot-0xc
                // call's own arguments just above (a genuine leftover-register-reuse artifact,
                // not a real rect) -- see docs/engine-bugs.md.
                RECT garbageRectSic;
                garbageRectSic.left = 0;
                garbageRectSic.top = (LONG)pPointCursorRect;
                garbageRectSic.right = (LONG)pPointCursorDesc;
                garbageRectSic.bottom = 0;

                DDSurfaceDescPadded0x7c ddsd;
                memset(&ddsd, 0, sizeof(ddsd));
                ddsd.ddsd.dwSize = 0x7c;
                ddsd.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
                ddsd.ddsd.dwHeight = rectIdentityPreview.bottom - rectIdentityPreview.top;
                ddsd.ddsd.dwWidth = rectIdentityPreview.right - rectIdentityPreview.left;
#ifdef LOCO_PORT
                Port_ForceRgb565(&ddsd.ddsd); // PORT: pin 565
#endif
                IDirectDrawSurface *pTempSurface;
                if (g_pDDraw2->CreateSurface(&ddsd.ddsd, &pTempSurface, NULL) != 0) {
                    return 0;
                }
                RECT rect = rectIdentityPreview;
                rect.top -= 0x1e;
                pTempSurface->Blt(&rect, g_pDDrawWorkSurface, &rectIdentityPreview, DDBLT_WAIT, NULL);
                while (rect.right - rect.left > 10) {
                    RECT destRect = rectIdentityPreview;
                    destRect.top -= 0x1e;
                    RECT srcRect;
                    srcRect.left = rectIdentityPreview.left + rectHelpCanvas.left;
                    srcRect.top = rectIdentityPreview.top - 0x1e + rectHelpCanvas.top;
                    srcRect.right = rectIdentityPreview.right + rectHelpCanvas.left;
                    srcRect.bottom = rectIdentityPreview.bottom + rectHelpCanvas.top;
                    pHelpBitmapCache->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                    InflateRect(&rect, -8, -5);
                    // TODO: verify the exact captured-"before" rect passed as the 3rd (src) arg
                    // here against raw disasm before byte-matching -- see docs/subsystems.md's
                    // OnLButtonDown writeup (own multi-session arc, still unconfirmed).
                    g_pDDrawWorkSurface->Blt(&rect, pTempSurface, &rect, DDBLT_WAIT, NULL);
                    CommitRectUpdate(garbageRectSic);
                    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                    Sleep(5);
                }
                pTempSurface->Release();
                Sleep(0x14);
                bool bWasBuilt = bLocalCardBuilt;
                this->RebuildLocalPlayerCard();
                this->RedrawIdentityPreview();
                this->RedrawColorChannelBars(1);
                if (bWasBuilt == false) {
                    this->SelectDecalSubkind(byDecalSubkindCommitted, 1);
                    return 0;
                }
            }
            hwndTarget = hwndSelf;
        } else {
            g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
            pAlbumBtn->DrawFrame(1, NULL);
            CommitRectUpdate(pAlbumBtn->rect);
            Sleep(0x96);
            if (nEditMode != 6) {
                ((AlbumCardWndVtblProbe *)g_pAlbumCardWnd)->_v08();
                this->EndActiveSession();
                return 0;
            }
            if (pIdentityTextBuffer->wAttachmentId != 0) {
                pNetworkRosterBtn->DrawFrame(0, NULL);
                g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pIdentityTextBuffer->wAttachmentId);
                pIdentityTextBuffer->wAttachmentId = 0;
            }
            g_pPostBagCache->PostBag_SaveCardToCategory(pIdentityTextBuffer, 0, NULL);
            if (hDecalHitTestTimer != 0) {
                KillTimer(hwndSelf, hDecalHitTestTimer);
                hDecalHitTestTimer = 0;
            }
            nEditMode = 1;
            nSelectedDecalSlot = -1;
            bCursorAnimStopped = 0;
            Unk0x40 = 0;
            pDecalHitTestToggleBtn->DrawFrame(0, NULL);
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            pAlbumBtn->DrawFrame(0, NULL);
            bool bWasBuilt = bLocalCardBuilt;
            this->RebuildLocalPlayerCard();
            this->RedrawIdentityPreview();
            this->RedrawColorChannelBars(1);
            if (bWasBuilt == false) {
                this->SelectDecalSubkind(byDecalSubkindCommitted, 1);
                return 0;
            }
            hwndTarget = hwndSelf;
        }
    } else if (nEditMode == 6) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        pDecalHitTestToggleBtn->DrawFrame(1, NULL);
        CommitRectUpdate(pDecalHitTestToggleBtn->rect);
        if (pIdentityTextBuffer->wAttachmentId != 0) {
            pNetworkRosterBtn->DrawFrame(0, NULL);
            g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pIdentityTextBuffer->wAttachmentId);
            pIdentityTextBuffer->wAttachmentId = 0;
        }
        bool bWasBuilt = bLocalCardBuilt;
        this->RebuildLocalPlayerCard();
        this->RedrawIdentityPreview();
        this->RedrawColorChannelBars(1);
        if (bWasBuilt == false) {
            this->SelectDecalSubkind(byDecalSubkindCommitted, 1);
        } else {
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        }
        Sleep(100);
        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        nEditMode = 1;
        nSelectedDecalSlot = -1;
        bCursorAnimStopped = 0;
        Unk0x40 = 0;
        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        hwndTarget = hwndSelf;
    } else {
        if (nEditMode != 9) {
            g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
            nClickSoundCooldown = 0;
            this->ArmDecalHitTestMode();
            return 0;
        }
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        nClickSoundCooldown = 0;
        if (nEditMode != 9) {
            return 0;
        }
        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        nEditMode = 1;
        bCursorAnimStopped = 0;
        Unk0x40 = 0;
        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        hwndTarget = hwndSelf;
    }
    CommitScreenUpdate(hwndTarget, NULL, 0, NULL);
    return 0;
}

// FUNCTION: LOCO 0x41a050
void EditCardWnd::ArmDecalHitTestMode()
{
    if (nEditMode != 9) {
        nDecalHitTestInterval = 200;
        if (hDecalHitTestTimer == 0) {
            hDecalHitTestTimer = SetTimer(hwndSelf, 0x44, 200, NULL);
        }
        nSelectedDecalSlot = -1;
        nEditMode = 9;
        pDecalHitTestToggleBtn->DrawFrame(1, NULL);
        this->RequestModeTransitionFromSource((void *)pEraserCursorRect, pEraserCursorDesc, 0, 1);
        bCursorAnimStopped = 1;
        Unk0x40 = 0;
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    }
}

// FUNCTION: LOCO 0x4189a0
void EditCardWnd::RedrawIdentityPreview()
{
    RECT destRect;
    RECT srcRect;
    RECT *pDest = &rectIdentityPreview;
    int nDestLeft = pDest->left;
    int nSrcLeft = rectHelpCanvas.left + nDestLeft;
    int nDestTop = pDest->top - 0xb;
    int nDestRight = pDest->right;
    int nDestBottom = pDest->bottom;
    int nSrcTop = rectHelpCanvas.top + nDestTop;
    destRect.top = nDestTop;
    destRect.left = nDestLeft;
    destRect.right = nDestRight;
    destRect.bottom = nDestBottom;
    srcRect.top = nSrcTop;
    srcRect.left = nSrcLeft;
    srcRect.right = nDestRight + nSrcLeft - nDestLeft;
    srcRect.bottom = nDestBottom + nSrcTop - nDestTop;
    pHelpBitmapCache->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
    if (pIdentityTextBuffer != NULL) {
        g_pPostBagCache->DrawCardThumbnail(bLocalCardBuilt, pIdentityTextBuffer, g_pDDrawWorkSurface, rectIdentityPreview, (unsigned int)hwndSelf, &rectRosterBadge);
    }
}

// FUNCTION: LOCO 0x419680
void EditCardWnd::RedrawRosterList()
{
    HWND hwndTarget;
    HDC hdc;
    HGDIOBJ hOldFont;
    COLORREF oldColor;
    int oldMode;
    HBRUSH hbr;
    int nHeight;
    int nRowTop;
    LONG nRowBottom;
    char (*pName)[13];
    unsigned int nIndex;
    bool bFirstPass;
    COLORREF color;
    RECT rowRect;
    char szTitle[16];

    memcpy(szTitle, &g_dwTextBufSeed, sizeof(g_dwTextBufSeed));
    memset(szTitle + sizeof(g_dwTextBufSeed), 0, sizeof(szTitle) - sizeof(g_dwTextBufSeed));
    bNoMoreRosterRows = 0;
    nIndex = 0;
    g_UIResources.LoadLocaleString(100, szTitle, sizeof(szTitle));
    if (nEditMode == 7) {
        hwndTarget = hwndSelf;
        field_0x178 = 0;
        hdc = AcquireWorkSurfaceDC(hwndTarget);
        hOldFont = SelectObject(hdc, g_UIResources.m_hFont16);
        oldColor = SetTextColor(hdc, 0x40c05c);
        oldMode = SetBkMode(hdc, TRANSPARENT);
        hbr = (HBRUSH)GetStockObject(WHITE_BRUSH);
        FillRect(hdc, &rectRosterPanel, hbr);
        DrawTextA(hdc, szTitle, -1, &rectRosterTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawEdge(hdc, &rectRosterList, EDGE_SUNKEN, BF_RECT | BF_SOFT);
        bFirstPass = field_0x174 == 0;
        if (bFirstPass) {
            field_0x170 = 0;
            field_0x17c = 0;
        }
        nRowBottom = rectRosterList.bottom - 2;
        nIndex = field_0x170;
        rowRect.left = rectRosterList.left + 2;
        nRowTop = rectRosterList.top + 2;
        rowRect.right = rectRosterList.right - 2;
        rowRect.top = nRowTop;
        rowRect.bottom = nRowBottom;
        if (nRowTop < rectRosterList.bottom - 0xc) {
            pName = aRosterNames + nIndex;
            do {
                if ((*pName)[0] == '\0') {
                    bNoMoreRosterRows = 1;
                    break;
                }
                if ((int)field_0x170 <= (int)nIndex) {
                    color = (nSelectedRosterIndex == (int)nIndex) ? 0xa0aff : 0x40c05c;
                    SetTextColor(hdc, color);
                    nHeight = DrawTextA(hdc, *pName, -1, &rowRect, DT_SINGLELINE);
                    nRowTop = rowRect.top + nHeight;
                    if (bFirstPass) {
                        field_0x17c = field_0x17c + 1;
                    }
                    field_0x178 = nHeight;
                    nRowBottom = rowRect.bottom;
                    rowRect.top = nRowTop;
                }
                nIndex = nIndex + 1;
                pName = pName + 1;
            } while (nRowTop < nRowBottom - 10);
        }
        nIndex = nIndex - 1;
        field_0x174 = nIndex;
        SelectObject(hdc, hOldFont);
        SetTextColor(hdc, oldColor);
        SetBkMode(hdc, oldMode);
        CommitScreenUpdate(hwndTarget, hdc, 1, NULL);
        pRosterScrollUpBtn->DrawFrame(0, NULL);
        pRosterScrollDownBtn->DrawFrame(0, NULL);
    }
    if (aRosterNames[nIndex + 1][0] == '\0') {
        bNoMoreRosterRows = 1;
    }
}

// FUNCTION: LOCO 0x41a360 // EFFECTIVE MATCH -- 83/83 insns structurally matching, byte_diff
// 1/254. Sole residual: the SIB base/index roles of `mov al,[<this> + <idx> + 0x370]`. The
// ORIGINAL is `[esi+edx*1+0x370]` (base=esi/this, index=edx/the masked subscript); we emit
// base=edx, index=esi. (An earlier autopsy here had these two sides swapped -- objdump of
// 0x41a432 is `8a 84 16`, SIB 0x16 = base esi, index edx.)
//
// The class is now understood -- see docs/CODEGEN.md "SIB base/index role" -- and it is what
// fixed the sibling residual in BuildPlayerRosterList (0x416e00) above. The rule: whichever
// operand is the result of an ARITHMETIC op takes the base slot. Confirmed against a minimal
// reproducer: `arr[i]` and `arr[pOther->field]` (index is a plain variable / a bare memory
// load) both keep base=this, matching the original; every form that computes the subscript
// takes base=index. Probed and REFUTED for this call site, all still base=index: `i & 0xff`,
// `0xff & i`, `(unsigned char)i`, `i % 256`, a `unsigned char` PARAMETER (callee-side mask),
// `i &= 0xff` as its own statement, the mask hoisted to a local (with and without an
// intervening call, and with the local kept live afterwards), a local `S *p = this` alias, a
// pointer-to-array local, and a 2-D `arr2[0][i & 0xff]` row form. The original masks the
// subscript (`and edx,0xff` at 0x41a42c) yet still gets base=this, so some fifth source shape
// produces a non-arithmetic subscript here; nothing tried reaches it. Parked at 1 byte.
void EditCardWnd::SelectDecalSlot(unsigned int nSlotIndex)
{
    if (bLocalCardBuilt != false) {
        if (nEditMode == 9) {
            if (hDecalHitTestTimer != 0) {
                KillTimer(hwndSelf, hDecalHitTestTimer);
                hDecalHitTestTimer = 0;
            }
            nEditMode = 1;
            bCursorAnimStopped = 0;
            Unk0x40 = 0;
            pDecalHitTestToggleBtn->DrawFrame(0, NULL);
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        }
        nEditMode = 2;
        nSelectedDecalSlot = nSlotIndex;
        LocoBitmap *pThumb = paDecalThumbCache[nSlotIndex];
        POINT ptCenter;
        ptCenter.x = (unsigned int)pThumb->width >> 1;
        ptCenter.y = (unsigned int)pThumb->height >> 1;
        this->ScheduleModeTransition(pThumb, 0, &ptCenter, 0, 1);
        field_0x388 = 0;
        return;
    }
    pIdentityTextBuffer->byStampSlotB = aRandomDecalPickIndex[nSlotIndex & 0xff] + 1;
    RedrawIdentityPreview();
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
}

// FUNCTION: LOCO 0x4198b0
void EditCardWnd::RedrawDecalKindButtons()
{
    if (field_0x2b0 != 0) {
        pDecalKindBtn1->DrawFrame(0, NULL);
        pDecalKindBtn2->DrawFrame(0, NULL);
        pDecalKindBtn3->DrawFrame(0, NULL);
        pDecalKindBtn4->DrawFrame(0, NULL);
        pDecalKindBtn5->DrawFrame(0, NULL);
        pDecalKindBtn6->DrawFrame(0, NULL);
        switch (byDecalKindPending) {
        case 1:
            pDecalKindBtn1->DrawFrame(1, NULL);
            return;
        case 2:
            pDecalKindBtn2->DrawFrame(1, NULL);
            return;
        case 3:
            pDecalKindBtn3->DrawFrame(1, NULL);
            return;
        case 4:
            pDecalKindBtn4->DrawFrame(1, NULL);
            return;
        case 5:
            pDecalKindBtn5->DrawFrame(1, NULL);
            return;
        case 6:
            pDecalKindBtn6->DrawFrame(1, NULL);
            return;
        }
    } else {
        pDecalKindBtn1->DrawFrame(2, NULL);
        pDecalKindBtn2->DrawFrame(2, NULL);
        pDecalKindBtn3->DrawFrame(2, NULL);
        pDecalKindBtn4->DrawFrame(2, NULL);
        pDecalKindBtn5->DrawFrame(2, NULL);
        pDecalKindBtn6->DrawFrame(2, NULL);
    }
}

// Pages the decal-picker thumbnail row one screenful in the requested direction. Both halves
// are the same walk: start at the slot just past (or just before) the current page, lazily
// populate paDecalThumbCache from the clip-art cache, and keep accumulating thumbnail widths
// (+4px for the first, +10px per subsequent gap) until the running total no longer fits the
// grid's own width. field_0x2b4/field_0x2b5 come out as the "can page forward"/"can page
// backward" enables for the two scroll arrows.
//
// The forward half has a second, unrelated source of slot indices: when the pending subkind is
// 0x1f (the "random" clip-art family) and bPickerVsRosterGate is clear, consecutive slots come
// from aRandomDecalPickIndex[] instead of counting up, so the picker shows a fixed shuffled
// sample rather than the first N.
//
// sic: the backward half's own entry guard is dead code -- `(unsigned char)(nDecalPickerScrollB
// - 1)` can never be negative, so the `< 0` test the compiler still emits (`and ebx,0xff` then
// `jl`) never fires. Kept as written; it mirrors the loop's own live `< 0` break below it.
//
// EFFECTIVE MATCH (771 B vs 754, insns 241/239, total 140961). Block layout, branch polarity and
// the whole control-flow skeleton are instruction-for-instruction identical; every residual
// descends from ONE callee-saved-register tie-break in the FORWARD half. Five values are live
// across the ClipartBitmapCache_GetOrLoad call (this/the element-address induction var/nIdx/
// nSlot/nWidth) and only four callee-saved registers exist: the original spends ebp on nSlot and
// demotes nWidth to ecx-plus-a-spill through [esp+0x14] (the slot nOldScrollB already owns);
// cl here spends ebp on nWidth and homes nSlot in memory instead. The 4 extra instructions are
// exactly that spill code, and the `cmp r,ebp`-vs-`test r,r` rows downstream are the knock-on
// effect of ebp holding a live zero here where the original already has nSlot in it.
// Probes run and REFUTED (do not re-run): scoping nSlot/nWidth/the cursor per-branch instead of
// per-function (inert -- VC5's allocator is live-range based, not scope based); walking a
// `LocoBitmap **ppThumb` cursor instead of subscripting paDecalThumbCache[nSlot] (BYTE-IDENTICAL
// both ways, tested twice, so the pointer-vs-index question cannot be settled from the bytes and
// the subscript form is kept as the simpler one); reusing nOldScrollB itself as the forward
// width accumulator to lengthen its live range (inert); initializing nSlot at its declaration
// (`int nSlot = 0;`) instead of inside the random arm, on the theory that the original shares one
// `xor ebp,ebp` between nRandWalk's and nSlot's zeroing -- strictly WORSE, 140961 -> 156978.
// Probes that DID pay, all folded in above: writing the `field_0x2b5` seed as
// `if (nOldScrollB <= 0) ... else ...` rather than the inverted `> 0` spelling (the original
// jumps `jg` to the =1 arm, i.e. the ZERO case is the one written first); dropping a redundant
// `nRandWalk = 0` from the random arm; hoisting `field_0x2b4 = 1` above the nOldScrollB read;
// and CODEGEN.md's item-(5) `? true : false` lever on BOTH `field_0x2b5` result assignments,
// which drops the two `xor r,r`-before-`setg` widenings (162975 -> 140961).
// FUNCTION: LOCO 0x419260
bool EditCardWnd::StepDecalPickerScroll(char bForward)
{
    int nRandWalk = 0;

    field_0x2b4 = 1;
    int nOldScrollB = nDecalPickerScrollB;
    if (nOldScrollB <= 0) {
        field_0x2b5 = 0;
    } else {
        field_0x2b5 = 1;
    }

    if (bForward != 0) {
        int nIdx;
        int nSlot;
        int nOldScrollA;

        if (byDecalSubkindPending == 0x1f && bPickerVsRosterGate == 0) {
            nSlot = 0;
            nIdx = aRandomDecalPickIndex[nRandWalk];
            nOldScrollA = -1;
        } else {
            nIdx = (unsigned char)(nDecalPickerScrollA + 1);
            nOldScrollA = nDecalPickerScrollA;
            nSlot = nIdx;
        }
        if (nIdx > 64) {
            field_0x2b4 = 0;
            return false;
        }
        if (paDecalThumbCache[nSlot] == NULL) {
            paDecalThumbCache[nSlot] = g_pPostBagCache->ClipartBitmapCache_GetOrLoad(
                byDecalSubkindPending, byDecalKindPending, nIdx + 1, 1);
        }
        if (paDecalThumbCache[nSlot] != NULL) {
            int nWidth = (paDecalThumbCache[nSlot])->width + 4;
            field_0x2b5 = 1;
            while (nWidth < rectDecalPickerGrid.right - rectDecalPickerGrid.left) {
                if (byDecalSubkindPending == 0x1f && bPickerVsRosterGate == 0) {
                    nRandWalk++;
                    nIdx = aRandomDecalPickIndex[nRandWalk];
                } else {
                    nIdx++;
                }
                nSlot++;
                if (nSlot > 64) {
                    nSlot--;
                    field_0x2b4 = 0;
                    break;
                }
                nWidth += 10;
                if (paDecalThumbCache[nSlot] == NULL) {
                    paDecalThumbCache[nSlot] = g_pPostBagCache->ClipartBitmapCache_GetOrLoad(
                        byDecalSubkindPending, byDecalKindPending, nIdx + 1, 1);
                }
                if (paDecalThumbCache[nSlot] == NULL) {
                    nSlot--;
                    field_0x2b4 = 0;
                    break;
                }
                nWidth += (paDecalThumbCache[nSlot])->width;
            }
            if (nWidth > rectDecalPickerGrid.right - rectDecalPickerGrid.left) {
                nSlot--;
            }
            nDecalPickerScrollA = nSlot;
            nDecalPickerScrollB = nOldScrollA + 1;
            field_0x2b5 = nOldScrollA + 1 > 0 ? true : false;
            return true;
        }
        field_0x2b4 = 0;
        return false;
    }

    int nSlot = (unsigned char)(nDecalPickerScrollB - 1);
    if (nSlot >= 0) {
        if (paDecalThumbCache[nSlot] == NULL) {
            paDecalThumbCache[nSlot] = g_pPostBagCache->ClipartBitmapCache_GetOrLoad(
                byDecalSubkindPending, byDecalKindPending, nSlot + 1, 1);
        }
        if (paDecalThumbCache[nSlot] != NULL) {
            int nWidth = (paDecalThumbCache[nSlot])->width + 4;
            field_0x2b4 = 1;
            while (nWidth < rectDecalPickerGrid.right - rectDecalPickerGrid.left) {
                nSlot--;
                if (nSlot < 0) {
                    nSlot++;
                    break;
                }
                nWidth += 10;
                if (paDecalThumbCache[nSlot] == NULL) {
                    paDecalThumbCache[nSlot] = g_pPostBagCache->ClipartBitmapCache_GetOrLoad(
                        byDecalSubkindPending, byDecalKindPending, nSlot + 1, 1);
                }
                if (paDecalThumbCache[nSlot] == NULL) {
                    nSlot++;
                    field_0x2b5 = 0;
                    break;
                }
                nWidth += (paDecalThumbCache[nSlot])->width;
            }
            if (nWidth > rectDecalPickerGrid.right - rectDecalPickerGrid.left) {
                nSlot++;
            }
            nDecalPickerScrollB = nSlot;
            nDecalPickerScrollA = nOldScrollB - 1;
            field_0x2b5 = nSlot > 0 ? true : false;
            return true;
        }
    }
    field_0x2b5 = 0;
    return false;
}

// FUNCTION: LOCO 0x419560
void EditCardWnd::SyncDecalPickerButtonHighlights()
{
    pExitBtn->DrawFrame(0, NULL);
    pPostBtn->DrawFrame(0, NULL);
    pAlbumBtn->DrawFrame(0, NULL);
    pDeleteBtn->DrawFrame(0, NULL);
    pBtnResMaybe_3cc2->DrawFrame(0, NULL);
    pMailBtn->DrawFrame(0, NULL);
    if (g_pDPlaySessionMgr->connectionMode == 2) {
        if (pIdentityTextBuffer->wAttachmentId != 0) {
            pNetworkRosterBtn->DrawFrame(1, NULL);
        } else {
            pNetworkRosterBtn->DrawFrame(0, NULL);
        }
    }
    if (nEditMode == 9) {
        pDecalHitTestToggleBtn->DrawFrame(1, NULL);
    } else {
        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
    }
    RedrawDecalKindButtons();
    for (int i = 0; i < 16; i++) {
        int bHighlight;
        if (bPickerVsRosterGate == 0) {
            if (byDecalSubkindPending == i || bLocalCardBuilt == false) {
                goto highlightOn;
            }
            bHighlight = 0;
        } else {
            if ((unsigned int)(byDecalSubkindPending - 0x10) == i || bLocalCardBuilt == false) {
                goto highlightOn;
            }
            bHighlight = 0;
        }
        goto doDraw;
highlightOn:
        bHighlight = 1;
doDraw:
        paDecalCategoryBtn[i]->DrawFrame(bHighlight, NULL);
    }
}

// FUNCTION: LOCO 0x41a460
// EFFECTIVE MATCH (asmscore.py --len 495: insns 161/161, byte_diff 3). The POINT is a BY-VALUE
// parameter (see AlbumCardWnd::HitTestUiElement), and `hDecalHitTestTimer` must be read directly
// in both arms of the nEditMode==9 if/else rather than hoisted into a local -- VC5 CSEs it into
// the common predecessor either way, but only the direct read gets it scheduled after the `cmp`,
// where nEditMode dies and the timer load can reuse eax. Those two facts took this 29 -> 5 bytes
// in v361. Sole residual: the SECOND of the two duplicated KillTimer arms loads hwndSelf into ecx
// before pushing the timer, where the original pushes the timer first and reloads hwndSelf into
// the freed eax; our FIRST arm emits the original's exact pattern, so it is a per-arm scheduling
// coin-flip, not a source-shape gap. See docs/PARKED.md.
void EditCardWnd::CycleDecalKindHighlight(POINT pt)
{
    unsigned int byOldKind = byDecalKindPending;
    if (field_0x2b0 == 0) {
        return;
    }
    if (PtInRect(&pDecalKindBtn1->rect, pt)) {
        byDecalKindPending = 1;
    } else if (PtInRect(&pDecalKindBtn2->rect, pt)) {
        byDecalKindPending = 2;
    } else if (PtInRect(&pDecalKindBtn3->rect, pt)) {
        byDecalKindPending = 3;
    } else if (PtInRect(&pDecalKindBtn4->rect, pt)) {
        byDecalKindPending = 4;
    } else if (PtInRect(&pDecalKindBtn5->rect, pt)) {
        byDecalKindPending = 5;
    } else if (PtInRect(&pDecalKindBtn6->rect, pt)) {
        byDecalKindPending = 6;
    }
    if (byOldKind == byDecalKindPending) {
        return;
    }
    nDecalPickerScrollB = -1;
    nDecalPickerScrollA = -1;
    nSelectedDecalSlot = -1;
    for (int i = 0; i < 64; i++) {
        if (paDecalThumbCache[i]) {
            delete paDecalThumbCache[i];
            paDecalThumbCache[i] = NULL;
        }
    }
    if (nEditMode == 9) {
        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        nEditMode = 1;
    } else {
        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        nEditMode = 1;
        nSelectedDecalSlot = -1;
    }
    bCursorAnimStopped = 0;
    Unk0x40 = 0;
    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    RedrawDecalKindButtons();
    StepDecalPickerScroll(1);
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    AnimateDecalPickerPageWipe(1, 1);
}

// FUNCTION: LOCO 0x41a210
void EditCardWnd::SelectDecalSubkind(unsigned char param_1, unsigned char param_2)
{
    unsigned int byOldSubkind = byDecalSubkindPending;
    byDecalSubkindPending = param_1;
    nDecalPickerScrollB = -1;
    nDecalPickerScrollA = -1;
    nSelectedDecalSlot = -1;
    if (hDecalHitTestTimer != 0) {
        KillTimer(hwndSelf, hDecalHitTestTimer);
        hDecalHitTestTimer = 0;
    }
    nEditMode = 1;
    nSelectedDecalSlot = -1;
    bCursorAnimStopped = 0;
    Unk0x40 = 0;
    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    for (int i = 0; i < 64; i++) {
        if (paDecalThumbCache[i]) {
            delete paDecalThumbCache[i];
            paDecalThumbCache[i] = NULL;
        }
    }
    byDecalKindPending = 1;
    field_0x2b0 = 1;
    LocoBitmap *pBitmap = g_pPostBagCache->ClipartBitmapCache_GetOrLoad(byDecalSubkindPending, 2, 1, 0);
    if (pBitmap == NULL) {
        field_0x2b0 = 0;
    } else {
        delete pBitmap;
    }
    SyncDecalPickerButtonHighlights();
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    StepDecalPickerScroll(1);
    if (byOldSubkind == byDecalSubkindPending && param_2 == 0) {
        RedrawDecalThumbnailGrid(NULL, 0);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        return;
    }
    AnimateDecalPickerPageWipe(1, 1);
}

// FUNCTION: LOCO 0x418340
void EditCardWnd::ApplyColorSwatchPreset(POINT pt)
{
    for (int i = 0; i < 10; i++) {
        if (PtInRect(&paColorSwatchBtn[i]->rect, pt)) {
            paColorSwatchBtn[i]->DrawFrame(1, NULL);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            Sleep(0x96);
            byIdentityColor0 = aColorRGBTriple[i * 3];
            byIdentityColor1 = aColorRGBTriple[i * 3 + 1];
            byIdentityColor2 = aColorRGBTriple[i * 3 + 2];
            RedrawColorChannelBars(1);
            if (pIdentityTextBuffer != NULL) {
                pIdentityTextBuffer->byIdentityColorR = (unsigned char)byIdentityColor0;
                pIdentityTextBuffer->byIdentityColorG = (unsigned char)byIdentityColor1;
                pIdentityTextBuffer->byIdentityColorB = (unsigned char)byIdentityColor2;
                RedrawIdentityPreview();
            }
            for (int j = 0; j < 10; j++) {
                paColorSwatchBtn[j]->DrawFrame(0, NULL);
            }
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            return;
        }
    }
}

// FUNCTION: LOCO 0x418450
void EditCardWnd::AdjustIdentityColorChannel(int param_1, char param_2, int param_3, int param_4)
{
    nEditMode = 5;
    if (hDecalHitTestTimer != 0) {
        KillTimer(hwndSelf, hDecalHitTestTimer);
        hDecalHitTestTimer = 0;
    }
    field_0x250 = param_1;
    field_0x254 = param_2;
    if (field_0x24c == 0) {
        field_0x24c = SetTimer(hwndSelf, 0x4d, 100, NULL);
    }
    switch (param_1) {
    case 0: {
        int nNewValue;
        pRedWheelBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if (byIdentityColor0 < 0xff && param_2 != 0) {
            if (nClickSoundCooldown < 1) {
                nClickSoundCooldown = 10;
                g_UIResources.PlaySoundAtScreenPos(0x5279, param_3, param_4, 4);
            }
            nNewValue = byIdentityColor0 + 6;
        } else {
            if (byIdentityColor0 < 1 || param_2 != 0) {
                goto clampAndRedraw;
            }
            if (nClickSoundCooldown < 1) {
                nClickSoundCooldown = 10;
                g_UIResources.PlaySoundAtScreenPos(0x5279, param_3, param_4, 4);
            }
            nNewValue = byIdentityColor0 - 6;
        }
        byIdentityColor0 = nNewValue;
        break;
    }
    case 1:
        pYellowWheelBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if (byIdentityColor1 < 0xff && param_2 != 0) {
            if (nClickSoundCooldown < 1) {
                nClickSoundCooldown = 10;
                g_UIResources.PlaySoundAtScreenPos(0x5279, param_3, param_4, 4);
            }
            byIdentityColor1 = byIdentityColor1 + 6;
        } else if (byIdentityColor1 > 0 && param_2 == 0) {
            if (nClickSoundCooldown < 1) {
                nClickSoundCooldown = 10;
                g_UIResources.PlaySoundAtScreenPos(0x5279, param_3, param_4, 4);
            }
            byIdentityColor1 = byIdentityColor1 - 6;
        }
        break;
    case 2:
        pBlueWheelBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if (byIdentityColor2 < 0xff && param_2 != 0) {
            if (nClickSoundCooldown < 1) {
                nClickSoundCooldown = 10;
                g_UIResources.PlaySoundAtScreenPos(0x5279, param_3, param_4, 4);
            }
            byIdentityColor2 = byIdentityColor2 + 6;
        } else if (byIdentityColor2 > 0 && param_2 == 0) {
            if (nClickSoundCooldown < 1) {
                nClickSoundCooldown = 10;
                g_UIResources.PlaySoundAtScreenPos(0x5279, param_3, param_4, 4);
            }
            byIdentityColor2 = byIdentityColor2 - 6;
        }
        break;
    }
clampAndRedraw:
    if (byIdentityColor2 < 0) {
        byIdentityColor2 = 0;
    }
    if (byIdentityColor0 < 0) {
        byIdentityColor0 = 0;
    }
    if (byIdentityColor1 < 0) {
        byIdentityColor1 = 0;
    }
    if (byIdentityColor2 > 0xff) {
        byIdentityColor2 = 0xff;
    }
    if (byIdentityColor0 > 0xff) {
        byIdentityColor0 = 0xff;
    }
    if (byIdentityColor1 > 0xff) {
        byIdentityColor1 = 0xff;
    }
    RedrawColorChannelBars(0);
    if (pIdentityTextBuffer != NULL) {
        pIdentityTextBuffer->byIdentityColorR = (unsigned char)byIdentityColor0;
        pIdentityTextBuffer->byIdentityColorG = (unsigned char)byIdentityColor1;
        pIdentityTextBuffer->byIdentityColorB = (unsigned char)byIdentityColor2;
    }
    RedrawIdentityPreview();
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
}

// FUNCTION: LOCO 0x418780
void EditCardWnd::RedrawColorChannelBars(char bReleaseCaptureMaybe)
{
    HBRUSH hbrTrackBg = (HBRUSH)GetStockObject(BLACK_BRUSH);
    HBRUSH hbrChannel0 = CreateSolidBrush(0xff);
    HBRUSH hbrChannel1 = CreateSolidBrush(0xffff);
    HBRUSH hbrChannel2 = CreateSolidBrush(0xff0000);
    int nPercentScaleMaybe = ((rectColorChannelBar0.bottom - rectColorChannelBar0.top) * 100) / 0xff;
    HDC hdc = AcquireWorkSurfaceDC(hwndSelf);
    RECT fillRect;

    FillRect(hdc, &rectColorChannelBar0, hbrTrackBg);
    if (byIdentityColor0 != 0) {
        int nBarHeight = (nPercentScaleMaybe * byIdentityColor0) / 100;
        if (nBarHeight == 0) {
            nBarHeight = 1;
        }
        fillRect = rectColorChannelBar0;
        fillRect.top = fillRect.bottom - nBarHeight;
        FillRect(hdc, &fillRect, hbrChannel0);
    }
    FillRect(hdc, &rectColorChannelBar1, hbrTrackBg);
    if (byIdentityColor1 != 0) {
        int nBarHeight = (nPercentScaleMaybe * byIdentityColor1) / 100;
        if (nBarHeight == 0) {
            nBarHeight = 1;
        }
        fillRect = rectColorChannelBar1;
        fillRect.top = fillRect.bottom - nBarHeight;
        FillRect(hdc, &fillRect, hbrChannel1);
    }
    FillRect(hdc, &rectColorChannelBar2, hbrTrackBg);
    if (byIdentityColor2 != 0) {
        int nBarHeight = (nPercentScaleMaybe * byIdentityColor2) / 100;
        if (nBarHeight == 0) {
            nBarHeight = 1;
        }
        fillRect = rectColorChannelBar2;
        fillRect.top = fillRect.bottom - nBarHeight;
        FillRect(hdc, &fillRect, hbrChannel2);
    }
    DeleteObject(hbrTrackBg);
    DeleteObject(hbrChannel0);
    DeleteObject(hbrChannel1);
    DeleteObject(hbrChannel2);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
    if (bReleaseCaptureMaybe != 0) {
        pRedWheelBtn->DrawFrame(0, NULL);
        pYellowWheelBtn->DrawFrame(0, NULL);
        pBlueWheelBtn->DrawFrame(0, NULL);
    }
}

// FUNCTION: LOCO 0x41a650
void EditCardWnd::HandleRosterClick(LONG param_1, int param_2)
{
    POINT pt;
    pt.x = param_1;
    pt.y = param_2;
    if (PtInRect(&pRosterScrollUpBtn->rect, pt)) {
        pRosterScrollUpBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if ((int)field_0x170 > 0) {
            field_0x170 = field_0x170 - field_0x17c;
            if ((int)field_0x170 < 0) {
                field_0x170 = 0;
            }
            RedrawRosterList();
        }
        Sleep(0x32);
        pRosterScrollUpBtn->DrawFrame(0, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        return;
    }
    pt.x = param_1;
    pt.y = param_2;
    if (PtInRect(&pRosterScrollDownBtn->rect, pt)) {
        pRosterScrollDownBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if (bNoMoreRosterRows == 0) {
            field_0x170 = field_0x174 + 1;
            RedrawRosterList();
        }
        Sleep(0x32);
        pRosterScrollDownBtn->DrawFrame(0, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        return;
    }
    pt.x = param_1;
    pt.y = param_2;
    if (PtInRect(&rectRosterList, pt)) {
        int nOffset = param_2 - rectRosterList.top;
        if (field_0x178 != 0) {
            int nIndex = nOffset / (int)field_0x178 + field_0x170;
            char szName[16];
            strcpy(szName, aRosterNames[nIndex]);
            if (szName[0] != '\0') {
                nSelectedRosterIndex = nIndex;
                if (pIdentityTextBuffer != NULL) {
                    if ((int)nRosterCount <= nIndex) {
                        pIdentityTextBuffer->nameA[0x14] = 1;
                    } else {
                        pIdentityTextBuffer->nameA[0x14] = 0;
                    }
                    strcpy(pIdentityTextBuffer->nameA, szName);
                }
                this->RequestModeTransitionFromSource(pPreviewIconRealized, pPreviewIconDesc, 0, 1);
                nEditMode = 6;
                RedrawIdentityPreview();
                CommitRectUpdate(rectIdentityPreview);
            }
        }
    }
}

// FUNCTION: LOCO 0x418a90
void EditCardWnd::RedrawDecalThumbnailGrid(IDirectDrawSurface *pTargetSurface, char bFullRedraw)
{
    if (pTargetSurface == NULL) {
        bFullRedraw = 0;
        pTargetSurface = g_pDDrawWorkSurface;
    }
    if (bFullRedraw == 0) {
        RECT destRect;
        destRect.left = rectDecalPickerGrid.left;
        destRect.top = rectDecalPickerGrid.top;
        destRect.right = rectDecalPickerGrid.right;
        destRect.bottom = pRandomizeBtnRes->rect.top + 1;
        RECT srcRect;
        srcRect.left = rectDecalPickerGrid.left + rectHelpCanvas.left;
        srcRect.top = rectDecalPickerGrid.top + rectHelpCanvas.top;
        srcRect.right = rectHelpCanvas.left + rectDecalPickerGrid.right;
        srcRect.bottom = rectHelpCanvas.top + destRect.bottom;
        pHelpBitmapCache->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
    } else {
        DDBLTFX fx;
        fx.dwSize = sizeof(DDBLTFX);
        if (g_nSurfaceFormatTag == 0x22b) {
            fx.dwFillColor = 0x7c1f;
        } else {
            fx.dwFillColor = (g_nSurfaceFormatTag == 0x235) ? 0xf81f : 0xff00ff;
        }
        pTargetSurface->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);
    }

    if (bFullRedraw == 0) {
        pRandomizeBtnRes->DrawFrame(0, NULL);
    } else {
        int w = rectDecalPickerGrid.right - rectDecalPickerGrid.left;
        int h = rectDecalPickerGrid.bottom - rectDecalPickerGrid.top;
        unsigned short iconH = pRandomizeBtnRes->pCursorDesc->nativeHeight;
        RECT destRect;
        destRect.left = 0;
        destRect.top = h - iconH;
        destRect.right = w;
        destRect.bottom = h;
        RECT srcRect;
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = w;
        srcRect.bottom = iconH;
        ((LocoBitmap *)pRandomizeBtnRes->nRealizedHandle)->RestoreOverlapBlt(destRect, pTargetSurface, srcRect, 0);
    }

    if (nDecalPickerScrollB >= 0 && nDecalPickerScrollA >= 0) {
        int x = rectDecalPickerGrid.right;
        if (bFullRedraw != 0) {
            x -= rectDecalPickerGrid.left;
        }
        x -= 4;
        int i = nDecalPickerScrollB;
        LocoBitmap **ppThumb = paDecalThumbCache + i;
        while (i <= nDecalPickerScrollA) {
            int y = rectDecalPickerGrid.bottom;
            if (bFullRedraw != 0) {
                y -= rectDecalPickerGrid.top;
            }
            unsigned int h = (*ppThumb)->height;
            int w = (*ppThumb)->width;
            int destLeft = x - w;
            int destTop, srcTop;
            if ((h >> 2) * 3 < 0x38) {
                y = y - 0x1c + (h >> 2);
                destTop = y - h;
                srcTop = 0;
            } else if ((h / 3) * 2 < 0x38) {
                y = y - 0x1c + h / 3;
                destTop = y - h;
                srcTop = 0;
            } else if ((h & 0xfffffffe) < 0x70) {
                y = y - 0x1c + (h >> 1);
                destTop = y - h;
                srcTop = 0;
            } else if (h < 0x54) {
                destTop = y - h;
                srcTop = 0;
            } else {
                destTop = y - 0x54;
                srcTop = h - 0x54;
            }
            RECT destRect;
            destRect.left = destLeft;
            destRect.top = destTop;
            destRect.right = x;
            destRect.bottom = y;
            RECT srcRect;
            srcRect.left = 0;
            srcRect.top = srcTop;
            srcRect.right = w;
            srcRect.bottom = h;
            (*ppThumb)->RestoreOverlapBlt(destRect, pTargetSurface, srcRect, 0);

            int rowIdx = i - nDecalPickerScrollB;
            i = i + 1;
            RECT *pRow = &aDecalPickerRowRect[rowIdx];
            pRow->left = destLeft;
            pRow->top = destTop;
            ppThumb = ppThumb + 1;
            pRow->right = x;
            x = destLeft - 10;
            pRow->bottom = y;
        }
    }

    if (bFullRedraw == 0) {
        if (field_0x2b4 != 0 && bLocalCardBuilt) {
            pDecalScrollForwardBtn->DrawFrame(0, NULL);
        } else {
            pDecalScrollForwardBtn->DrawFrame(2, NULL);
        }
        if (field_0x2b5 != 0 && bLocalCardBuilt) {
            pDecalScrollBackBtn->DrawFrame(0, NULL);
        } else {
            pDecalScrollBackBtn->DrawFrame(2, NULL);
        }
    }
}

// FUNCTION: LOCO 0x418e20
void EditCardWnd::AnimateDecalPickerPageWipe(char bForward, unsigned char param_2)
{
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    EnableWindow(hwndSelf, 0);
    pBtnResMaybe_3cbe->DrawFrame(1, NULL);
    CommitRectUpdate(pBtnResMaybe_3cbe->rect);
    g_UIResources.PlayUiSound(0x5274);

    IDirectDrawSurface *pNewSurface, *pOldSurface;
    unsigned char bOldReadyMaybe;
    if (nDecalPickerRowOffset == 0) {
        pOldSurface = pDecalPickerSurfaceB;
        pNewSurface = pDecalPickerSurfaceA;
        bOldReadyMaybe = field_0x59c;
        nDecalPickerRowOffset = 1;
        field_0x59c = 0;
        field_0x594 = 1;
    } else {
        bOldReadyMaybe = field_0x594;
        pNewSurface = pDecalPickerSurfaceB;
        pOldSurface = pDecalPickerSurfaceA;
        nDecalPickerRowOffset = 0;
        field_0x59c = 1;
        field_0x594 = 0;
    }

    Ddraw_RebindWindowClipper(hwndSelf);
    RedrawDecalThumbnailGrid(pNewSurface, 1);

    unsigned char bDrawOldSlideMaybe; // sic: left uninitialized when bOldReadyMaybe!=0 -- relies
                                       // on stack-slot reuse with the setne result computed
                                       // during the page-toggle above (always 1 on that path),
                                       // see docs/engine-bugs.md
    if (bOldReadyMaybe == 0) {
        int h = rectDecalPickerGrid.bottom - rectDecalPickerGrid.top;
        int w = rectDecalPickerGrid.right - rectDecalPickerGrid.left;
        unsigned short iconH = pRandomizeBtnRes->pCursorDesc->nativeHeight;
        RECT destRect;
        destRect.left = 0;
        destRect.top = h - iconH;
        destRect.right = w;
        destRect.bottom = h;
        RECT srcRect;
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = w;
        srcRect.bottom = iconH;
        ((LocoBitmap *)pRandomizeBtnRes->nRealizedHandle)->RestoreOverlapBlt(destRect, pOldSurface, srcRect, 0);
        bDrawOldSlideMaybe = 1;
    }

    int width0 = rectDecalPickerGrid.right - rectDecalPickerGrid.left;
    int nSteps = (width0 + ((width0 >> 31) & 3)) >> 2;
    if (nSteps > 0) {
        int nEaseThreshold = nSteps / 3;
        int nOffset = 0;
        unsigned int nSleepMs = 0;
        for (int i = 0; i < nSteps; i++) {
            RECT destRect, srcRect;
            if (bForward == 0) {
                destRect.left = rectDecalPickerGrid.right - nOffset;
                destRect.top = rectDecalPickerGrid.top;
                destRect.right = rectDecalPickerGrid.right;
                destRect.bottom = rectDecalPickerGrid.bottom;
                srcRect.left = 0;
                srcRect.top = 0;
                srcRect.right = nOffset;
                srcRect.bottom = rectDecalPickerGrid.bottom - rectDecalPickerGrid.top;
            } else {
                destRect.left = rectDecalPickerGrid.left;
                destRect.top = rectDecalPickerGrid.top;
                destRect.right = rectDecalPickerGrid.left + nOffset;
                destRect.bottom = rectDecalPickerGrid.bottom;
                int width = rectDecalPickerGrid.right - rectDecalPickerGrid.left;
                srcRect.left = width - nOffset;
                srcRect.top = 0;
                srcRect.right = width;
                srcRect.bottom = rectDecalPickerGrid.bottom - rectDecalPickerGrid.top;
            }
            g_pDDrawWorkSurface->Blt(&destRect, pNewSurface, &srcRect, DDBLT_WAIT | DDBLT_KEYSRC, NULL);

            if (bDrawOldSlideMaybe != 0) {
                if (bForward == 0) {
                    destRect.right = destRect.left;
                    destRect.left = rectDecalPickerGrid.left;
                    srcRect.left = nOffset;
                    srcRect.right = rectDecalPickerGrid.right - rectDecalPickerGrid.left;
                } else {
                    destRect.left = destRect.right;
                    destRect.right = rectDecalPickerGrid.right;
                    srcRect.left = 0;
                    srcRect.right = (rectDecalPickerGrid.right - rectDecalPickerGrid.left) - nOffset;
                }
                g_pDDrawWorkSurface->Blt(&destRect, pOldSurface, &srcRect, DDBLT_WAIT | DDBLT_KEYSRC, NULL);
            }

            CommitRectUpdate(rectDecalPickerGrid);
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            if (nSleepMs != 0) {
                Sleep(nSleepMs);
            }
            nSleepMs = (i < nEaseThreshold) ? 2 : (i < nSteps / 2);
            nOffset += 4;
        }
    }

    Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
    RedrawDecalThumbnailGrid(NULL, 0);
    CommitRectUpdate(rectDecalPickerGrid);
    pBtnResMaybe_3cbe->DrawFrame(0, NULL);
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    g_pApp->DrainQueuedMouseInput(0);
    EnableWindow(hwndSelf, 1);
}

// FUNCTION: LOCO 0x416460
// Lazily builds the 1280x1024 "help/certificate canvas" (pHelpBitmapCache), sibling to
// SplashWnd's own BuildDrawTargetCompositeMaybe (src/SplashWnd.cpp) -- same shape (composite
// N TileKind icon frames onto a freshly-allocated LocoBitmap via BlitOntoBitmap, releasing
// each icon's CursorDesc afterward), same Ghidra decompile brokenness (garbled unaff_EBX/
// unaff_ESI/unaff_EBP/unaff_EDI scrambling every destRect/srcRect field) -- transcribed
// straight from raw disasm, see CLAUDE.md.
//
// UNLIKE BuildDrawTargetCompositeMaybe, there is only ONE SetRect call total (building a
// single full-canvas {0,0,0x500,0x400} RECT), and each of the 4 icon blocks passes that SAME
// rect as BOTH destRect and srcRect, with no per-icon CopyRect/OffsetRect step and no read of
// the loaded icon bitmap's own width/height -- confirmed instruction-for-instruction for all 4
// blocks (each one's "destRect"/"srcRect" scratch dwords trace back to the identical
// [esp+0x14]/[esp+0x1c]/[esp+0x30]/[esp+0x34]-relative reads of the SAME stack rect,
// re-derived fresh per block since each call cleans up its own stack back to the same
// baseline). Net effect: each of the 4 icons fully overwrites the whole canvas in turn, so
// only the LAST icon (0x3cc6) is ever actually visible in pHelpBitmapCache -- the first 3
// blits are pure dead work. Reproduced as-is (not a transcription bug): see
// docs/engine-bugs.md.
//
// EXACT MATCH (581 bytes). Was parked EFFECTIVE at score 104822 (byte_diff 132/580, insns
// 185/184) with an autopsy blaming an intrinsic this/CursorDesc* register-role swap; that
// verdict was wrong and is retracted. The function became exact without any source change to
// it -- almost certainly at v358's VC5 RTM -> SP3 toolchain switch (see docs/PARKED.md's own
// banner, which lists this same class of collapse) -- and the stale PARKED row simply went
// unnoticed until v375 re-tested it. Structure was already fully verified against the raw
// disasm (every icon id, every BlitOntoBitmap arg, the shared rect, the ReleaseRef tail call).
// Re-confirmed v375: the block-scoped local shape below and a function-scope-hoisted variant
// (the lever that closed the SplashWnd sibling that same session) compile BYTE-IDENTICALLY
// here, so the scoping choice is genuinely free for this function.
void EditCardWnd::BuildPreviewCanvasAMaybe() {
    if (pHelpBitmapCache == NULL) {
        RECT rect;
        SetRect(&rect, 0, 0, 0x500, 0x400);

        pHelpBitmapCache = new LocoBitmap();
        pHelpBitmapCache->CreateAndFill(0x500, 0x400, 1, 0, 0);

        {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3caa);
            LocoBitmap *pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
            pBmp->BlitOntoBitmap(rect, pHelpBitmapCache, rect, 0);
            pDesc->ReleaseRef();
        }
        {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cc4);
            LocoBitmap *pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
            pBmp->BlitOntoBitmap(rect, pHelpBitmapCache, rect, 0);
            pDesc->ReleaseRef();
        }
        {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cc5);
            LocoBitmap *pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
            pBmp->BlitOntoBitmap(rect, pHelpBitmapCache, rect, 0);
            pDesc->ReleaseRef();
        }
        {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cc6);
            LocoBitmap *pBmp = pDesc->GetOrLoadFrameBitmap(0, 0);
            pBmp->BlitOntoBitmap(rect, pHelpBitmapCache, rect, 0);
            pDesc->ReleaseRef();
        }
    }
}

// EditCardWnd_CenterFileDialogHookProcMaybe (0x419fd0, untranscribed) -- GetOpenFileNameA's
// own lpfnHook callback, immediately following this function in address order.
UINT CALLBACK EditCardWnd_CenterFileDialogHookProcMaybe(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x419fd0

// 0x463670 -- the "re-show every window" helper src/Main.cpp owns; declared file-locally there
// and in every other consumer TU (see src/NetSetupWnd.cpp).
extern unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void);   // 0x463670

// vtable slot 0x2c override -- the WindowBase catch-all (OnUnhandledMessageMaybe), ground-truthed
// against this class's own vtable at 0x477930 (slot 0x2c = 0x419a60). Three real cases on top of
// the DefWindowProcA fallback: WM_CTLCOLOREDIT for this window's own child EDIT paints it
// green-on-background by returning the shared background brush; a WM_SYSCOMMAND whose command
// bits are SC_SCREENSAVE re-shows the whole window stack (same idiom as NetSetupWnd's own slot
// 0x2c, src/NetSetupWnd.cpp); and the two app-private messages OnLButtonDown posts
// (0x5f5 = re-import the decal image, 0x5f6 = re-enable the window after the modal file dialog).
//
// FUNCTION: LOCO 0x419a60
LRESULT EditCardWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_SCREENSAVE) {
            FUN_00463670_LotsOfShowWindow();
        }
        break;
    case WM_CTLCOLOREDIT:
        if (lParam == (LPARAM)hwndEdit) {
            SetTextColor((HDC)wParam, 0x40c05c);
            SetBkMode((HDC)wParam, OPAQUE);
            SetBkColor((HDC)wParam, 0xe8e8e8);
            return (LRESULT)hBackgroundBrush;
        }
        break;
    case 0x5f5:
        ImportDecalImageMaybe();
        break;
    case 0x5f6:
        EnableWindow(hwndSelf, TRUE);
        break;
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x419b10
void EditCardWnd::ImportDecalImageMaybe() {
    // All six buffers are plain aggregate initializers -- cl copies the (pooled) literal's own
    // bytes out of its storage and then `rep stos`es the tail, which is exactly the
    // `mov dl,ds:<"">` / `mov ax,ds:<"c:">` + `rep stosd` shape the original opens with. See
    // docs/CODEGEN.md; the hand-rolled `szBuf[0] = g_szScratchText[0]; memset(szBuf + 1, ...)`
    // spelling this replaced only happens to agree for the `""` cases and was outright wrong for
    // szInitialDir (it emitted immediate 'c'/':' stores instead of the 3-byte literal copy).
    char szFile[0x504] = "";
    char szFileTitle[0x104] = "";
    char szTitle[0x100] = "";
    char szInitialDir[0x104] = "c:";
    char szMsgBuf[0x100] = "";
    char szSuffix[0x100] = "";

    LPCSTR pszSysErrMaybe = NULL;
    UINT nErrStringIdMaybe;

    nEditMode = 8;
    g_UIResources.LoadLocaleString(0x68, szTitle, sizeof(szTitle));
    if (pIdentityTextBuffer->wAttachmentId != 0) {
        g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pIdentityTextBuffer->wAttachmentId);
        pIdentityTextBuffer->wAttachmentId = 0;
        this->RedrawIdentityPreview();
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    }

RetryOpen:
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.hwndOwner = hwndSelf;
    ofn.hInstance = (HINSTANCE)hInstance;
    ofn.lpstrFileTitle = szFileTitle;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrTitle = szTitle;
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "all(*.*)\0*.*";
    ofn.nFilterIndex = 1;
    ofn.nMaxFile = sizeof(szFile);
    ofn.nMaxFileTitle = sizeof(szFileTitle);
    ofn.Flags = 0x81830;
    ofn.lpstrDefExt = "*";
    ofn.lpfnHook = EditCardWnd_CenterFileDialogHookProcMaybe;
    ofn.lpstrFile = szFile;
    if (!GetOpenFileNameA(&ofn)) {
        goto Done;
    }
    strcpy(szFile, ofn.lpstrFile);

    {
    HANDLE hFile = CreateFileA(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD dwErr = GetLastError();
            if (dwErr == 0) {
                nErrStringIdMaybe = 0x66;
                goto ShowErrorAndRetry;
            }
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszSysErrMaybe, 0, NULL);
            MessageBoxA(hwndSelf, pszSysErrMaybe, "LEGO LOCO", 0x10);
            LocalFree((HLOCAL)pszSysErrMaybe);
            pszSysErrMaybe = NULL;
            goto RetryOpen;
        }

        DWORD dwSize = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
        if (dwSize == 0) {
            nErrStringIdMaybe = 0x69;
            goto ShowErrorAndRetry;
        }
        if (dwSize > 0xfa000 || dwSize == 0xffffffff) {
            DWORD dwErr = GetLastError();
            if (dwErr == 0) {
                g_UIResources.LoadLocaleString(0x67, szMsgBuf, sizeof(szMsgBuf));
                wsprintfA(szSuffix, " %dk", 1000);
                strcat(szMsgBuf, szSuffix);
                MessageBoxA(hwndSelf, szMsgBuf, "LEGO LOCO", 0x10);
            } else {
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszSysErrMaybe, 0, NULL);
                MessageBoxA(hwndSelf, "LEGO LOCO", pszSysErrMaybe, 0x10); // sic: caption/text swapped
                LocalFree((HLOCAL)pszSysErrMaybe);
                pszSysErrMaybe = NULL;
            }
            goto RetryOpen;
        }

        pIdentityTextBuffer->wAttachmentId = (unsigned short)g_pPostBagCache->PostBag_ImportAttachmentFile(4, szFile);
        if (_stricmp(szFile + strlen(szFile) - 3, "wav") == 0) {
            pIdentityTextBuffer->bAttachmentSoundPlayedMaybe = 0;
        } else {
            pIdentityTextBuffer->bAttachmentSoundPlayedMaybe = 1;
        }
        if (pIdentityTextBuffer->bAttachmentSoundPlayedMaybe == 0) {
            g_UIResources.Sound_PlayOneShotAtPosition(szFile, rectIdentityPreview.left, rectIdentityPreview.top, 4);
        }
        this->RedrawIdentityPreview();
        goto Done;
    }

ShowErrorAndRetry:
    g_UIResources.LoadLocaleString(nErrStringIdMaybe, szMsgBuf, sizeof(szMsgBuf));
    MessageBoxA(hwndSelf, szMsgBuf, "LEGO LOCO", 0x10);
    goto RetryOpen;

Done:
    pNetworkRosterBtn->DrawFrame(pIdentityTextBuffer->wAttachmentId != 0, NULL);
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    if (hDecalHitTestTimer != 0) {
        KillTimer(hwndSelf, hDecalHitTestTimer);
        hDecalHitTestTimer = 0;
    }
    nEditMode = 1;
    nSelectedDecalSlot = -1;
    bCursorAnimStopped = 0;
    Unk0x40 = 0;
    pDecalHitTestToggleBtn->DrawFrame(0, NULL);
    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
}

// FUNCTION: LOCO 0x419fd0
// GetOpenFileNameA's OFN_ENABLEHOOK callback, shared by this TU's decal import above and
// src/MailWnd.cpp's own attachment browse. Its only job is to center the common file dialog: on
// WM_INITDIALOG it takes the dialog's PARENT (the real file-dialog window -- the hook proc's own
// hDlg is the invisible child template window, which is why nothing here moves hDlg itself),
// measures it and the desktop, and hands both to the same WindowBase::CenterRectInRect the rest
// of the UI uses. That helper rewrites the SECOND rect in place, so the parent rect comes back
// already centered and MoveWindow just replays it -- with bRepaint FALSE, since the dialog has
// not been shown yet.
//
// Returns 0 for every message, WM_INITDIALOG included: an OFNHookProc returning 0 means "let the
// dialog's own procedure handle this too", which is required here -- the dialog still has to run
// its own WM_INITDIALOG.
UINT CALLBACK EditCardWnd_CenterFileDialogHookProcMaybe(HWND hDlg, UINT msg, WPARAM wParam,
                                                        LPARAM lParam)
{
    if (msg == WM_INITDIALOG) {
        HWND hwndDialog;
        HWND hwndDesktop;
        RECT rectDialog;
        RECT rectDesktop;

        hwndDialog = GetParent(hDlg);
        GetWindowRect(hwndDialog, &rectDialog);
        hwndDesktop = GetDesktopWindow();
        GetWindowRect(hwndDesktop, &rectDesktop);
        CenterRectInRect(&rectDesktop, &rectDialog);
        MoveWindow(hwndDialog, rectDialog.left, rectDialog.top, rectDialog.right - rectDialog.left,
                   rectDialog.bottom - rectDialog.top, FALSE);
    }
    return 0;
}

// FUNCTION: LOCO 0x41ca80
// Vtable slot 0x40 -- the WindowBase-wide WM_RBUTTONDOWN convention slot (ground-truthed against
// this class's own vtable at 0x477930, where slot 0x38 is OnLButtonDown/0x41ac10 and slot 0x50 is
// OnMouseMove/0x41ce50). Ghidra's EditCardWnd_HandleLButtonUpMaybe name was wrong -- the same
// slot-mislabelling family as its EditCardWnd_HandleSetCursorMaybe guess for OnMouseMove.
//
// Right-click is the card editor's universal CANCEL, plus the decrement half of the colour
// steppers:
//   - modes 8/10 (the two "busy"/transition modes) swallow it entirely, exactly as OnLButtonDown
//     does;
//   - mode 6 (a roster name is being picked) and mode 9 (decal hit-test polling) both stop the
//     0x44 timer pump, un-highlight the eraser toggle and restore the point cursor;
//   - mode 4 (the name edit box is up) commits the typed text back into the card and hides the
//     edit control, then FALLS THROUGH into the ordinary mode-1 handling below (nEditMode is 1 by
//     then, so the `nEditMode == 9` test that follows takes its else arm);
//   - a right-click inside rectIdentityPreview while leaving mode 9 detaches any network
//     attachment from the card and rebuilds it from scratch -- the same "reset this card to the
//     local player's identity" action OnLButtonDown reaches via the album button;
//   - anywhere else, after cancelling back to mode 1, the three RGB channel bars (and their
//     wheel buttons) step their channel DOWN: the identical PtInRect chain OnLButtonDown runs,
//     with AdjustIdentityColorChannel's increase flag 0 instead of 1.
// The mode-9 arm re-tests `nEditMode == 9` immediately after the sound call even though nothing
// in between can change it; that redundant test is in the original and is reproduced here.
LRESULT EditCardWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;
    int nMode = nEditMode;

    if (nMode == 8) {
        return 0;
    }
    if (nMode == 10) {
        return 0;
    }
    if (nMode == 6) {
        g_UIResources.PlayUiSound(0x5015);
        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        nEditMode = 1;
        nSelectedDecalSlot = -1;
        bCursorAnimStopped = 0;
        Unk0x40 = 0;
        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        return 0;
    }
    if (nMode == 4) {
        g_UIResources.PlayUiSound(0x5015);
        nEditMode = 1;
        ShowWindow(hwndEdit, 0);
        SetFocus(hwndSelf);
        GetWindowTextA(hwndEdit, pIdentityTextBuffer->szDescription, 0x50);
        this->RedrawIdentityPreview();
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    }
    if (nEditMode == 9) {
        g_UIResources.PlayUiSound(0x5015);
        if (nEditMode == 9) {
            if (hDecalHitTestTimer != 0) {
                KillTimer(hwndSelf, hDecalHitTestTimer);
                hDecalHitTestTimer = 0;
            }
            nEditMode = 1;
            bCursorAnimStopped = 0;
            Unk0x40 = 0;
            pDecalHitTestToggleBtn->DrawFrame(0, NULL);
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        }
        if (PtInRect(&rectIdentityPreview, pt)) {
            if (pIdentityTextBuffer->wAttachmentId != 0) {
                pNetworkRosterBtn->DrawFrame(0, NULL);
                g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pIdentityTextBuffer->wAttachmentId);
                pIdentityTextBuffer->wAttachmentId = 0;
            }
            bool bWasBuilt = bLocalCardBuilt;
            this->RebuildLocalPlayerCard();
            this->RedrawIdentityPreview();
            this->RedrawColorChannelBars(1);
            if (bWasBuilt == false) {
                this->SelectDecalSubkind(byDecalSubkindCommitted, 1);
                return 0;
            }
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            return 0;
        }
    } else {
        if (hDecalHitTestTimer != 0) {
            KillTimer(hwndSelf, hDecalHitTestTimer);
            hDecalHitTestTimer = 0;
        }
        nEditMode = 1;
        nSelectedDecalSlot = -1;
        bCursorAnimStopped = 0;
        Unk0x40 = 0;
        pDecalHitTestToggleBtn->DrawFrame(0, NULL);
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        if ((PtInRect(&rectColorChannelBar2, pt) || PtInRect(&pBlueWheelBtn->rect, pt)) && bLocalCardBuilt != false) {
            this->AdjustIdentityColorChannel(2, 0, pt.x, pt.y);
            return 0;
        }
        if ((PtInRect(&rectColorChannelBar0, pt) || PtInRect(&pRedWheelBtn->rect, pt)) && bLocalCardBuilt != false) {
            this->AdjustIdentityColorChannel(0, 0, pt.x, pt.y);
            return 0;
        }
        if ((PtInRect(&rectColorChannelBar1, pt) || PtInRect(&pYellowWheelBtn->rect, pt)) && bLocalCardBuilt != false) {
            this->AdjustIdentityColorChannel(1, 0, pt.x, pt.y);
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x41cdf0 (Ghidra: EditCardWnd::EditCardWnd_HandleSetFocusMaybe -- WindowBase
// vtable+0x60 override, the class-wide WM_SETFOCUS convention slot. See src/EditCardWnd.h.)
// EFFECTIVE MATCH: same residual class as AlbumCardWnd::OnSetFocus (0x405620) and
// MailWnd::OnSetFocus (0x42fe80) -- see the autopsy on the AlbumCardWnd one. Identical length
// and instruction count; the whole diff is the two-register-rotation vs. load-all-up-front
// argument schedule for the tail stub call. See docs/PARKED.md.
LRESULT EditCardWnd::OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (nEditMode == 10) {
        PostMessageA(g_pTutorialWnd->hwndSelf, WM_SETFOCUS, 0, 0);
        SetWindowPos(g_pTutorialWnd->hwndSelf, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        return 0;
    }
    return WindowBase_DefWindowProcStub(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x41ce50
// EXACT (v420, 1020 bytes). Three source shapes were load-bearing and all three are ordinary
// idiomatic choices, not tricks: the two interior bounds of the decal-drag zone are written with
// pt on the LEFT (`pt.y > rectUnk0x288.bottom`, not `... < pt.y`) so VC5 emits `cmp ebx,[mem]`
// rather than the mirrored form; the 8/10 and "drag anchor still set" paths each END in their own
// point-cursor call plus `return 0` instead of falling through to a single shared tail (VC5 then
// tail-merges the three copies itself, which is what puts the merged block early rather than at
// the end); and the roster-list test is spelled `if (!PtInRect(...))` with the hand-cursor arm
// first, since the original's fall-through is the MISS case.
// Vtable slot 0x50 -- the WindowBase-wide WM_MOUSEMOVE convention slot (ground-truthed against
// this class's own vtable at 0x477930; Ghidra's EditCardWnd_HandleSetCursorMaybe name predated
// that read and is wrong -- WM_SETCURSOR is slot 0x70).
//
// It only ever changes the cursor. Everything the editor can be hovering that is CLICKABLE gets
// the "anipoint" (hand) cursor; everything else gets the plain point cursor; and a handful of
// modes suppress the change entirely (8 and 10 fall straight through to the point cursor, while
// 9/2/6 return without touching it at all -- those modes drive the cursor from their own timer
// pumps instead). Mode 7 is the roster list, which inverts the rule: inside the list the cursor
// is the plain point one, outside it the hand.
//
// Mode 2 with a card built is the decal DRAG: while the pointer is in the open area bounded by
// the swatch row on the left, the album button on the right, rectUnk0x288 above and the picker
// grid below, the cursor becomes the dragged decal's own thumbnail, hotspotted at its centre.
LRESULT EditCardWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;
    int nMode = nEditMode;

    if (nMode == 8 || nMode == 10) {
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        return 0;
    }
    if (nMode == 2 && bLocalCardBuilt) {
        if (pt.y < rectDecalPickerGrid.top && pt.y > rectUnk0x288.bottom &&
            pt.x > rectColorSwatchRow.right && pt.x < pAlbumBtn->rect.left) {
            POINT ptHotspot;

            field_0x388 = 1;
            ptHotspot.x = (unsigned int)paDecalThumbCache[nSelectedDecalSlot]->width >> 1;
            ptHotspot.y = (unsigned int)paDecalThumbCache[nSelectedDecalSlot]->height >> 1;
            this->ScheduleModeTransition(paDecalThumbCache[nSelectedDecalSlot], 0, &ptHotspot, 0,
                                         1);
            return 0;
        }
        if (field_0x388 == 0) {
            return 0;
        }
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        return 0;
    }
    if (nMode == 9) {
        return 0;
    }
    if (nMode == 2) {
        return 0;
    }
    if (nMode == 6) {
        return 0;
    }
    if (nMode == 7) {
        if (!PtInRect(&rectRosterList, pt)) {
            this->RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc,
                                                  0, 1);
        } else {
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        }
        return 0;
    }
    if (PtInRect(&pExitBtn->rect, pt) || PtInRect(&pAlbumBtn->rect, pt) ||
        PtInRect(&pPostBtn->rect, pt) || PtInRect(&pDeleteBtn->rect, pt) ||
        PtInRect(&pDecalHitTestToggleBtn->rect, pt) ||
        (PtInRect(&pDecalScrollBackBtn->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalScrollForwardBtn->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalKindBtn1->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalKindBtn2->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalKindBtn3->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalKindBtn4->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalKindBtn5->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&pDecalKindBtn6->rect, pt) && bLocalCardBuilt) ||
        (PtInRect(&rectDecalCategoryGrid, pt) && bLocalCardBuilt) ||
        PtInRect(&rectDecalPickerGrid, pt) || PtInRect(&pBtnResMaybe_3cc2->rect, pt) ||
        (PtInRect(&pNetworkRosterBtn->rect, pt) &&
         g_pDPlaySessionMgr->connectionMode == 2) ||
        (PtInRect(&rectColorChannelBar0, pt) && bLocalCardBuilt) ||
        (PtInRect(&rectColorChannelBar1, pt) && bLocalCardBuilt) ||
        (PtInRect(&rectColorChannelBar2, pt) && bLocalCardBuilt) ||
        (PtInRect(&rectColorSwatchRow, pt) && bLocalCardBuilt) ||
        (PtInRect(&rectIdentityPreview, pt) && nEditMode == 1) ||
        PtInRect(&pBlueWheelBtn->rect, pt) || PtInRect(&pRedWheelBtn->rect, pt) ||
        PtInRect(&pYellowWheelBtn->rect, pt)) {
        this->RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0,
                                              1);
        return 0;
    }
    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    return 0;
}

// FUNCTION: LOCO 0x41a8a0
// Vtable slot 0x30 (WM_TIMER). Three timer ids are live in this window:
//  - 0x44, the decal hit-test polling pump ArmDecalHitTestMode arms for edit mode 9: polls
//    the cursor once per interval, and while it sits over rectIdentityPreview asks the card
//    buffer to remove a decal at the preview-relative point. A miss just relaxes the
//    interval back to 200 ms; a hit re-arms the pump 25 ms FASTER (bottoming out at 50 ms),
//    restarts it, plays the click sound (rate-limited by nClickSoundCooldown), and redraws
//    + commits the preview.
//  - 0x4d, the RGB colour-wheel auto-repeat: while a wheel is dragged (mode 5) nudges the
//    active channel by field_0x254's direction once per tick.
//  - 0x53, the click-sound cooldown: plain decrement-to-zero.
// Every path falls through to DefWindowProcA (the slot's default behavior), whose return
// value is the handler's own.
//
// PARTIAL -- EFFECTIVE, content-complete (asmscore --len 410: total 14674, align=12
// reg_pen=24 identity_miss=24 byte_diff=34, insns 117/117). Two levers LANDED and are
// load-bearing: the dispatch is a `switch` on wParam, not an if/else-if chain -- only the
// switch produces the original's reverse-order case-body layout (0x53 body first, 0x44
// last) -- and the miss/hit arms are `if (bRemoved != 0) { hit } else { = 200 }` (the
// original's `je` outlines the =200 store AFTER the hit body), plus the cooldown gate is
// `<= 0` (`test eax,eax; jg`), not `< 1` (`cmp eax,1; jge`). Residuals: (1) ONE push-
// scheduling coin-flip at the RemoveDecalAtPoint call -- the original computes xRel, then
// yRel, then pushes yRel/xRel, while this compile pushes yRel before computing xRel (named
// xRel/yRel locals AND a POINT ptRel temp both compile byte-identically to the plain form,
// refuted); (2) pervasive eax/ecx/edx register-name permutations across the KillTimer/
// SetTimer/DefWindowProcA push sequences -- the documented symmetric-register-swap
// intrinsic class. docs/PARKED.md row added v499.
LRESULT EditCardWnd::OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (wParam) {
    case 0x44:
        if (bDecalTimerArmedMaybe && nEditMode == 9) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwndSelf, &pt);
            if (PtInRect(&rectIdentityPreview, pt) && bLocalCardBuilt) {
                unsigned char bRemoved = pIdentityTextBuffer->RemoveDecalAtPoint(
                    pt.x - rectIdentityPreview.left, pt.y - rectIdentityPreview.top);
                if (bRemoved != 0) {
                    KillTimer(hwndSelf, hDecalHitTestTimer);
                    if (nDecalHitTestInterval > 50) {
                        nDecalHitTestInterval -= 25;
                    }
                    hDecalHitTestTimer = SetTimer(hwndSelf, 0x44, nDecalHitTestInterval, NULL);
                    if (nClickSoundCooldown <= 0) {
                        nClickSoundCooldown = 10;
                        g_UIResources.PlayUiSound(0x501f);
                    }
                    RedrawIdentityPreview();
                    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
                } else {
                    nDecalHitTestInterval = 200;
                }
            }
        }
        break;
    case 0x4d:
        if (nEditMode == 5) {
            AdjustIdentityColorChannel(field_0x250, field_0x254, g_worldBoard.dwHalfWidth,
                                       g_worldBoard.dwHalfHeight);
        }
        break;
    case 0x53:
        if (nClickSoundCooldown != 0) {
            nClickSoundCooldown--;
        }
        break;
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x41aa40
// Vtable slot 0x3c (WM_LBUTTONUP). Only two edit modes react to the button coming back up:
// mode 5 (an RGB colour wheel is being dragged) ends the drag -- back to browsing mode 1,
// the auto-repeat timer is killed and all three wheel buttons are redrawn unpressed -- and
// mode 9 (the decal hit-test polling mode ArmDecalHitTestMode arms) restarts the inherited
// cursor animation. Modes 8 and 10 are explicitly inert. Always returns 0.
LRESULT EditCardWnd::OnLButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int nMode = nEditMode;
    bDecalTimerArmedMaybe = false;
    if (nMode != 8 && nMode != 10) {
        if (nMode == 5) {
            nEditMode = 1;
            if (field_0x24c != 0) {
                KillTimer(hwndSelf, field_0x24c);
                field_0x24c = 0;
            }
            pRedWheelBtn->DrawFrame(0, NULL);
            pYellowWheelBtn->DrawFrame(0, NULL);
            pBlueWheelBtn->DrawFrame(0, NULL);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
            return 0;
        }
        if (nMode == 9) {
            bCursorAnimStopped = false;
            nCursorFrameIndex = 0;
            Unk0x40 = 1;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x41aae0
// Vtable slot 0x44 (WM_RBUTTONUP) -- see the declaration in src/EditCardWnd.h. OnLButtonUp's
// mode-5 arm, verbatim, minus that function's bDecalTimerArmedMaybe reset and mode-9 arm.
LRESULT EditCardWnd::OnRButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int nMode = nEditMode;
    if (nMode != 8 && nMode != 10) {
        if (nMode == 5) {
            nEditMode = 1;
            if (field_0x24c != 0) {
                KillTimer(hwndSelf, field_0x24c);
                field_0x24c = 0;
            }
            pRedWheelBtn->DrawFrame(0, NULL);
            pYellowWheelBtn->DrawFrame(0, NULL);
            pBlueWheelBtn->DrawFrame(0, NULL);
            CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x41ab70
// Vtable slot 0x48 (WM_LBUTTONDBLCLK). Ghidra called this EditCardWnd_HandleMouseMoveMaybe and
// its plate hypothesized WM_MOUSEMOVE -- a slot mislabel (0x41ce50 already holds slot 0x50);
// both renamed in Ghidra alongside this transcription. Only roster-build mode 7 reacts: a double-click landing
// inside the roster panel commits the pick -- back to browsing mode 1, with the identity preview
// strip repainted and its rect alone committed to the screen. Modes 8 and 10 are inert, exactly
// as in OnLButtonUp/OnLButtonDown; every other mode falls through to DefWindowProcA.
LRESULT EditCardWnd::OnLButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int nMode = nEditMode;
    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;

    if (nMode == 8 || nMode == 10) {
        return 0;
    }
    if (nMode == 7 && PtInRect(&rectRosterPanel, pt)) {
        nEditMode = 1;
        RedrawIdentityPreview();
        CommitRectUpdate(rectIdentityPreview);
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// DAT_004851f4 -- the app screen-state selector. Declared TU-locally rather than by pulling
// src/GameNetMsgQueue.h into this file, exactly as src/CreditsWnd.cpp does: this TU is 83
// functions wide and that header's declaration set moves the parity dial documented below.
extern int g_nScreenState;

// The byte-returning inline predicate over that global -- see docs/CODEGEN.md's "the
// sete-materialized branch is a byte-returning inline predicate". Kept TU-local exactly as
// src/ApplSetupWnd.cpp, src/CreditsWnd.cpp, src/NetSetupWnd.cpp, src/MailWnd.cpp,
// src/BuildToolCursorWnd.cpp and src/AppWindow.cpp each do.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x419a10
// Vtable slot 0x80 (WM_CLOSE). While the app is alive and not already tearing down, the card
// editor answers a close by ENDING THE SESSION and bouncing the front end to screen state 3 --
// the same exit path OnKeyDown's RETURN/ESCAPE arm takes after flashing the exit button, minus
// the flash -- and swallows the close itself (returns 0). Only once shutdown is already underway
// does the base WindowBase::OnClose actually destroy the window.
//
// The guard's SETZ normalization is the byte-returning IsNetShuttingDownMaybe inline, the same
// shape CreditsWnd::OnClose and ApplSetupWnd::OnClose carry (see src/AppWindow.h).
//
// ⚠ ICF-FOLDED with AlbumCardWnd's slot 0x80 -- see the declaration in src/EditCardWnd.h.
LRESULT EditCardWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        this->EndActiveSession();
        AppWindow_SetScreenState(3);
        return 0;
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}
