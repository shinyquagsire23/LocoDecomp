// ApplSetupWnd -- the multiplayer application/provider-setup page, the second of the two setup
// windows SplashWnd owns. See src/ApplSetupWnd.h for the class layout.
//
// The TU's real extent is 0x408aa0..0x40b4c0 -- ~7 KB in 19 functions, bracketed by
// src/AppWindow.cpp below and src/NameAnchorMaybe.cpp above, and split in two by a middle block
// (0x409e70..0x40ac50) that src/DPlaySessionMgr.cpp claimed in an earlier session. Those middle
// functions ARE this class's -- they are all `LayoutSet::`-namespaced ApplSetupWnd methods -- but
// they are left where they are for now: moving a matched function between TUs re-rolls its
// codegen context, and every one of them is already EXACT. Fold them in only alongside a
// full-repo before/after byte diff.
//
// Discovered by the v436 run-of->=3 sweep: a contiguous run of unclaimed functions whose
// neighbours on either side belong to two DIFFERENT files, which is what an unclaimed whole TU
// looks like to both halves of that sweep.

#include <windows.h>

#include "ApplSetupWnd.h"
#include "GameNetMsgQueue.h"   // NetSettings, g_pNetSettings (0x4fd3a8)
#include "UIResources.h"       // g_UIResources, SoundBankEntry
#include "DSoundChannel.h"     // SoundBankEntry
#include "LocoBitmap.h"        // RestoreOverlapBlt, g_pDDrawWorkSurface
#include "DPlaySessionMgr.h"   // g_pDPlaySessionMgr, DPlaySessionMgrProviderSlot
#include "AppWindow.h"         // g_pApp
#include "SplashWnd.h"         // g_pSplashWnd, SplashWnd::SetState

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_ClampDesktopRect
#endif

// DAT_004fd3c4 -- the shared off-screen compositing surface, declared file-locally exactly as
// src/NetSetupWnd.cpp, src/TutorialWnd.cpp and src/PopupWndBase.cpp do.
extern IDirectDrawSurface *g_pDDrawWorkSurface;  // DAT_004fd3c4

// 0x43efa0 -- clears the connect-pending flag, resets every provider slot and posts a type-5
// teardown onto the GameNet send queue. Defined in src/DPlaySessionMgr.cpp; declared file-locally
// exactly as src/MapWnd.cpp declares its own DPlaySessionMgr helpers.
void __fastcall GameNet_ResetProvidersAndPostTeardown(DPlaySessionMgr *pMgr); // TODO: idiom

// 0x463670 -- the "re-show every window" helper src/Main.cpp owns; declared file-locally there
// too, because that TU has no header of its own yet.
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void); // TODO: idiom

// The byte-returning inline predicate over the shared app-state global -- see docs/CODEGEN.md's
// "the sete-materialized branch is a byte-returning inline predicate". Kept TU-local exactly as
// src/NetSetupWnd.cpp, src/MailWnd.cpp and src/AppWindow.cpp each do.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// Chains WindowBase's ctor with the page's own resource id, then runs InitFields. The vtable
// store between the two is the compiler's, not the source's.
//
// FUNCTION: LOCO 0x408aa0
ApplSetupWnd::ApplSetupWnd(HINSTANCE hInstanceArg, UINT resourceIdArg)
    : WindowBase(hInstanceArg, resourceIdArg)
{
    InitFields();
}

// FUNCTION: LOCO 0x408b00 (??_GApplSetupWnd scalar deleting dtor -- compiler-generated around
// ~ApplSetupWnd() below; no source of its own)

// Zeroes the page's own state and constructs the 5 button + 9 avatar ResourceRefs. The
// `new ResourceRef(id)` expressions each compile to the /GX new-expression alloc-protection
// dance (operator new, null-check, ctor, EH-state bump) -- that scaffolding is the compiler's.
//
// FUNCTION: LOCO 0x408b20
void ApplSetupWnd::InitFields()
{
    int i;

    hIcon = NULL;
    bResourcesLoadedMaybe = 0;
    pListHeadSecondaryMaybe = NULL;
    pListHeadMaybe = NULL;
    nUnk0xf4 = 0;
    nRowHeightMaybe = 0x10;
    nListRowCountMaybe = 0;
    bUnk0x10c = 0;
    field_0xe8 = 0;
    nSelectedProviderSlot = 0;
    pSelectedNodeTextMaybe = NULL;
    pSelectedNodeTextSecondaryMaybe = NULL;
    pDrawnListHeadMaybe = NULL;
    bReadyForRedrawMaybe = 0;
    hTimerA = 0;
    hTimerB = 0;
    bUnk0x114 = 0;
    nUnk0x1b0 = 3;

    pApGoBtn = new ResourceRef(0x42a);
    pApExitBtn = new ResourceRef(0x42c);
    pApSearchBtn = new ResourceRef(0x429);
    pApOptionBtn = new ResourceRef(0x42b);
    pApMatrixBtn = new ResourceRef(0x42f);

    textBuf0x120[0] = '\0';

    for (i = 0; i < 9; i++) {
        aPlayerAvatars[i] = new ResourceRef(i + 0x43a);
    }
}

// Frees both provider lists node-by-node (each node's pszText first, then the node), undoes
// BeginModalCapture's realize pass, releases the shared click sound, then deletes the 14
// ResourceRefs. The button deletes are NOT in field order -- search/exit/option/go/matrix -- and
// that order is the original's own.
//
// FUNCTION: LOCO 0x408d10
ApplSetupWnd::~ApplSetupWnd()
{
    ProviderListNode *pNext;
    SoundBankEntry *pEntry;
    int i;

    while (pListHeadSecondaryMaybe != NULL) {
        pNext = pListHeadSecondaryMaybe->pNext;
        if (pListHeadSecondaryMaybe->pszText != NULL) {
            delete pListHeadSecondaryMaybe->pszText;
        }
        delete pListHeadSecondaryMaybe;
        pListHeadSecondaryMaybe = pNext;
    }

    if (pListHeadMaybe == (ProviderListNode *)-1) {
        pListHeadMaybe = NULL;
    }
    while (pListHeadMaybe != NULL) {
        pNext = pListHeadMaybe->pNext;
        if (pListHeadMaybe->pszText != NULL) {
            delete pListHeadMaybe->pszText;
        }
        delete pListHeadMaybe;
        pListHeadMaybe = pNext;
    }

    if (bResourcesLoadedMaybe) {
        pApGoBtn->ReleaseRealized();
        pApExitBtn->ReleaseRealized();
        pApSearchBtn->ReleaseRealized();
        pApOptionBtn->ReleaseRealized();
        pApMatrixBtn->ReleaseRealized();
        pIconDescMaybe->ReleaseRef();
        pIconDescMaybe = NULL;
        for (i = 0; i < 9; i++) {
            aPlayerAvatars[i]->ReleaseRealized();
        }
        bResourcesLoadedMaybe = 0;
    }

    pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    if (pEntry != NULL) {
        pEntry->Release();
    }

    delete pApSearchBtn;
    pApSearchBtn = NULL;
    delete pApExitBtn;
    pApExitBtn = NULL;
    delete pApOptionBtn;
    pApOptionBtn = NULL;
    delete pApGoBtn;
    pApGoBtn = NULL;
    delete pApMatrixBtn;
    pApMatrixBtn = NULL;

    for (i = 0; i < 9; i++) {
        delete aPlayerAvatars[i];
        aPlayerAvatars[i] = NULL;
    }
}

// Sizes the page to the whole desktop client rect and chains WindowBase's 11-argument virtual
// Create with WS_POPUP|WS_VISIBLE-shaped style 0x41000000 -- the identical body
// NetSetupWnd::Create (0x4412f0) has.
//
// The `hwndDesktop` local is load-bearing, not tidying: written as the nested
// `GetClientRect(GetDesktopWindow(), &rectDesktop)`, cl evaluates the argument list
// right-to-left and emits the `lea` of &rectDesktop BEFORE the GetDesktopWindow call, while the
// original calls GetDesktopWindow first (0x408f06) and only then takes the rect's address.
// Naming the result forces that order. Same lever closed NetSetupWnd::Create.
//
// FUNCTION: LOCO 0x408f00
unsigned char ApplSetupWnd::Create(HWND hwndOwnerParam)
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

// vtable slot 0x20 -- the page's full repaint: restore the whole client clip's background out of
// the backdrop bitmap, refresh the connect-status line, redraw three of the five buttons, repaint
// whichever provider list the remembered-choice mode selects, repaint the avatar grid, arm the
// redraw gate, and flip the composed frame to the screen.
//
// FUNCTION: LOCO 0x409280
void ApplSetupWnd::OnActivate(int reservedMaybe)
{
    pIconRealizedMaybe->RestoreOverlapBlt(rectClipBounds, g_pDDrawWorkSurface,
                                          rectIconMaybe, 1);
    RefreshConnectStatusText();
    pApExitBtn->DrawFrame(0, NULL);
    pApSearchBtn->DrawFrame(0, NULL);
    pApOptionBtn->DrawFrame(0, NULL);

    // Two whole calls, one per arm -- NOT a ternary and not an if/else over a local. Both of
    // those collapse to a single `push` after the branch merge; the original keeps a `push`
    // INSIDE each arm (0x40931f / 0x409328) over one shared `mov ecx,esi; call` tail, which is
    // what cl's cross-jump leaves behind when the call itself is duplicated in the source.
    if (g_pNetSettings->bUseSecondaryRememberedChoice) {
        DrawProviderList(pListHeadSecondaryMaybe);
    } else {
        DrawProviderList(pListHeadMaybe);
    }
    DrawAvatarGrid();

    bReadyForRedrawMaybe = 1;
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
}

// Re-lays and repaints the page's one line of status text. The label's own background is
// restored from the backdrop bitmap first (so successive strings do not smear), then the string
// is measured with DT_CALCRECT|DT_SINGLELINE into rectLabelMaybe, centered inside
// rectTextAreaMaybe, and finally pushed to one of the four edges of that box by nUnk0x1b0.
// Structurally the same function as NetSetupWnd::LayoutAndDrawLabel (0x4421d0).
//
// FUNCTION: LOCO 0x409770
void ApplSetupWnd::LayoutAndDrawLabel()
{
    RECT rect = rectTextAreaMaybe;
    HDC hdc;
    HGDIOBJ hOldFont;
    int nTextHeight;

    bUnk0x114 = 1;

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
    nTextHeight = DrawTextA(hdc, textBuf0x120, -1, &rectLabelMaybe, DT_CALCRECT | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);

    rectLabelMaybe.bottom = rectLabelMaybe.top + nTextHeight - 4;
    CenterRectInRect(&rectTextAreaMaybe, &rectLabelMaybe);

    switch (nUnk0x1b0) {
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

// Repaints the scrolling provider list into rectListMaybe. The list rect's background is
// restored from the backdrop first, then one row per node is drawn -- the row at index nUnk0xf4
// in the selection colour, the rest in the normal one -- stopping as soon as a row would fall
// past the bottom of the rect. An empty (or "not yet built", -1) list draws locale string 0x7f
// instead. Row height is measured once with a literal "test" string and cached in
// nRowHeightMaybe, which is what OnLButtonDown divides a click's y by.
//
// EFFECTIVE MATCH -- 695 B vs 690, insns 215/216, every call and every store present and in the
// original's own order. The residual is one register-allocation coin-flip with a length knock-on:
// the original parks `this` in edi and keeps the CSE'd `&rectListMaybe` in ebx across the whole
// rect-adjust block, so its four field loads encode as 2-3-byte `[ebx+N]`; this compile parks
// `this` in esi, spills the pointer after one use, and re-materializes the other three as 6-byte
// `[esi+0x1e4]`-style absolutes -- which is the entire 5-byte overrun. Three shapes measured, all
// identical at 279: field order left/top/right/bottom vs the emitted store order
// right/left/top/bottom (cl canonicalizes both), and `rectRow.bottom > rectListMaybe.bottom` vs
// the operands reversed. Two levers ARE baked in and must not be undone -- see the per-arm
// SetTextColor note in the loop below, worth 32 points and 3 bytes.
//
// FUNCTION: LOCO 0x4094b0
void ApplSetupWnd::DrawProviderList(ProviderListNode *pListHead)
{
    RECT rect = rectListMaybe;
    RECT rectRow;
    HDC hdc;
    int nOldBkMode;
    COLORREF oldBkColor;
    COLORREF oldTextColor;
    HGDIOBJ hOldFont;
    int nTextHeight;
    int nRows;
    char szEmptyText[512];

    pDrawnListHeadMaybe = pListHead;

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
    nOldBkMode = SetBkMode(hdc, TRANSPARENT);
    oldBkColor = SetBkColor(hdc, 0x2525dc);
    oldTextColor = SetTextColor(hdc, 0xff5c00);
    hOldFont = SelectObject(hdc, g_UIResources.m_hFont14);
    nListRowCountMaybe = 0;

    rectRow = rectListMaybe;
    nTextHeight = DrawTextA(hdc, "test", -1, &rectRow, DT_CALCRECT | DT_SINGLELINE);
    rectRow.left = rectListMaybe.left + 0xc;
    rectRow.top = rectListMaybe.top + 0xc;
    rectRow.right = rectListMaybe.right - 0xc;
    rectRow.bottom = rectListMaybe.bottom - 0xc;

    nRows = 0;
    if (pListHead != (ProviderListNode *)-1) {
        int nRowHeight = nTextHeight + 4;

        while (pListHead != NULL) {
            // Written out in both arms, not as a ternary on the colour: the original pushes a
            // different constant inside each arm (0x409675 / 0x40967c) over one shared
            // `push esi; call SetTextColor` tail.
            if (nRows == nUnk0xf4) {
                SetTextColor(hdc, 0x2525dc);
            } else {
                SetTextColor(hdc, 0xff5c00);
            }
            DrawTextA(hdc, pListHead->pszText, -1, &rectRow, DT_SINGLELINE);
            rectRow.top += nRowHeight;
            rectRow.bottom = rectRow.top + nRowHeight;
            if (rectRow.bottom > rectListMaybe.bottom) {
                break;
            }
            pListHead = pListHead->pNext;
            nRows++;
        }
        nListRowCountMaybe = nRows;
    }

    if (nRows == 0) {
        SetBkMode(hdc, TRANSPARENT);
        g_UIResources.LoadLocaleString(0x7f, szEmptyText, 0x200);
        DrawTextA(hdc, szEmptyText, -1, &rectRow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SetBkMode(hdc, nOldBkMode);
    SetBkColor(hdc, oldBkColor);
    SetTextColor(hdc, oldTextColor);
    SelectObject(hdc, hOldFont);
    nRowHeightMaybe = nTextHeight + 4;
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
}

// vtable slot 0x1c -- the page's whole layout pass, and a no-op until BeginModalCapture has
// realized the resources (every rect below is sized from a realized descriptor's own natural
// width/height). Two anchors: a fixed 800x600 page centered in the window's clip bounds, and the
// provider grid pinned 0x1b/0x27 in from that page's top-left corner. Everything else hangs off
// those -- the list box fills the space right of the grid, the exit/go buttons pin to the page's
// bottom-right corner, the search/option buttons sit under the list box, and the easter-egg strip
// fills the gap between the option button and the list box's right edge. rectIconMaybe is the odd
// one out: a clip-bounds-sized WINDOW centered ON the backdrop bitmap, which is what every
// RestoreOverlapBlt in this file needs to stretch the backdrop over the whole screen -- the same
// trick NetSetupWnd::RefreshClientClipRect plays with its own rectIconMaybe.
//
// PARTIAL -- 657 B vs 771, insns 179/207, call parity 3/3, every rect and every constant present
// and arithmetically identical to the original's. The gap is entirely cl RE-USING values the
// original RELOADS: the original re-reads each descriptor's nativeWidth/nativeHeight and each
// just-written rect field out of memory (12 `xor r,r; mov r16,[...]` zero-extend pairs; this
// compile emits 10 and forwards the rest from registers), and it materializes each button's four
// values into stack slots [esp+0x10..0x1c] before a 4-load/4-store copy into the ResourceRef.
// Four shapes measured against `--len 771` -- do NOT re-run:
//   page-relative constants + direct field writes        444752, insns 187/207, 707 B
//   chained constants + one shared scratch RECT per btn  481503, insns 180/207, 657 B
//   chained constants + direct field writes (this one)   427399, insns 179/207, 657 B
// The scratch-RECT shape is the one the original's stack traffic literally shows, and it is the
// WORST of the three here, because cl folds the copy away again and then has one more live value
// to schedule -- so the stack slots are cl's own spill decision, not a source-level local.
// The chained constants ARE load-bearing and must not be "simplified" back to page-relative
// absolutes (444752 -> 427399): see the comment on the button block below.
//
// FUNCTION: LOCO 0x408f70
void ApplSetupWnd::RefreshClientClipRect()
{
    WindowBase::RefreshClientClipRect();

    if (bResourcesLoadedMaybe) {
        RECT rectIconWindow;
        ResourceRef *pBtn;

        rectPageMaybe.left = 0;
        rectPageMaybe.right = 800;
        rectPageMaybe.top = 0;
        rectPageMaybe.bottom = 600;
        CenterRectInRect(&rectClipBounds, &rectPageMaybe);

        rectIconWindow = rectClipBounds;
        rectIconMaybe.left = 0;
        rectIconMaybe.right = pIconDescMaybe->nativeWidth;
        rectIconMaybe.top = 0;
        rectIconMaybe.bottom = pIconDescMaybe->nativeHeight;
        CenterRectInRect(&rectIconMaybe, &rectIconWindow);
        rectIconMaybe = rectIconWindow;

        rectGridMaybe.left = rectPageMaybe.left + 0x1b;
        rectGridMaybe.right = rectGridMaybe.left + 0x1ee;
        rectGridMaybe.top = rectPageMaybe.top + 0x27;
        rectGridMaybe.bottom = rectGridMaybe.top + 0x173;

        pBtn = pApMatrixBtn;
        rectTextAreaMaybe.left = rectGridMaybe.left - 2;
        rectTextAreaMaybe.top = rectGridMaybe.bottom + 0xf;
        rectTextAreaMaybe.right = rectTextAreaMaybe.left + pBtn->pCursorDesc->nativeWidth;
        rectTextAreaMaybe.bottom = rectTextAreaMaybe.top + pBtn->pCursorDesc->nativeHeight;
        pBtn->rect = rectTextAreaMaybe;

        rectListMaybe.left = rectGridMaybe.right + 0x11;
        rectListMaybe.top = rectGridMaybe.top;
        rectListMaybe.bottom = rectGridMaybe.bottom - 0x1e;
        rectListMaybe.right = rectPageMaybe.right - 0x18;

        // Every constant below is chained off the PREVIOUS rect, not re-derived from the page:
        // the original's `lea`/`add` operands are 0x6b/-0x28 off rectListMaybe (0x4090fb,
        // 0x40910d) and 0x1ee/0x173/-2/0xf off rectGridMaybe (0x40903f..0x40905e), never the
        // page-relative absolutes those work out to.
        pBtn = pApExitBtn;
        pBtn->rect.top = rectListMaybe.bottom + 0x6b;
        pBtn->rect.right = rectListMaybe.right - 0x28;
        pBtn->rect.bottom = pBtn->rect.top + pBtn->pCursorDesc->nativeHeight;
        pBtn->rect.left = pBtn->rect.right - pBtn->pCursorDesc->nativeWidth;

        pBtn = pApGoBtn;
        pBtn->rect.top = pApExitBtn->rect.top + 1;
        pBtn->rect.bottom = pBtn->rect.top + pBtn->pCursorDesc->nativeHeight;
        pBtn->rect.right = pApExitBtn->rect.left;
        pBtn->rect.left = pBtn->rect.right - pBtn->pCursorDesc->nativeWidth;

        pBtn = pApSearchBtn;
        pBtn->rect.left = rectListMaybe.left + 0xd;
        pBtn->rect.right = pBtn->rect.left + pBtn->pCursorDesc->nativeWidth;
        pBtn->rect.top = rectListMaybe.bottom + 0xd;
        pBtn->rect.bottom = pBtn->rect.top + pBtn->pCursorDesc->nativeHeight;

        pBtn = pApOptionBtn;
        pBtn->rect.left = pApSearchBtn->rect.right + 7;
        pBtn->rect.right = pBtn->rect.left + pBtn->pCursorDesc->nativeWidth;
        pBtn->rect.top = pApSearchBtn->rect.top;
        pBtn->rect.bottom = pBtn->rect.top + pBtn->pCursorDesc->nativeHeight;

        rectEasterEggSoundMaybe.right = rectListMaybe.right;
        rectEasterEggSoundMaybe.left = pApOptionBtn->rect.right + 10;
        rectEasterEggSoundMaybe.top = pApOptionBtn->rect.top;
        rectEasterEggSoundMaybe.bottom = pApOptionBtn->rect.bottom;
    }
}

// Repaints the "who's connecting" avatar grid over rectGridMaybe: one aPlayerAvatars cell per
// (row, column) of the session manager's own slot dimensions, stepped 0xa5 across and 0x7c down
// from a 0xa4 x 0x7b cell. A cell past the manager's slot count draws frame 0 (disabled); a slot
// with no provider id draws frame 1 (empty); an occupied one draws frame 2 and centres that
// peer's short name over it. Note the row stride is the FIXED array width 3 while the column
// bound is the manager's own nProviderSlotsPerRow -- they agree only because ResetProviders
// always seeds a 3x3 grid. // sic:
//
// EFFECTIVE MATCH -- 735 B vs 725, insns 220/219, every call, constant and store present. The
// residual is the nested loops' induction-variable allocation: the original strength-reduces the
// slot index into a single register and derives the 0x4c byte stride with an
// `lea r,[i+i*8]`/`lea r,[i+r*2]`/`shl r,2` chain (0x409abd), where this compile keeps the index
// and the offset in separate stack slots. Four shapes measured: `providerId == 0` first vs
// `!= 0` first (431 vs 435 -- cl canonicalizes the arm order either way), indexed
// `aPlayerAvatars[nRow * 3 + nCol]` vs an explicit two-level pointer walk (431 vs 432), and the
// two rect-build orders noted at the top (431 vs 594). Do not re-run them.
//
// FUNCTION: LOCO 0x409980
void ApplSetupWnd::DrawAvatarGrid()
{
    RECT rect;
    RECT rectCell;
    int nRow;
    int nCol;
    int nSlot;

    // Store order is load-bearing (v438's "read the store order" lesson): this interleaving --
    // cell left/top/bottom, then the whole-rect copy, then cell right -- is the original's own.
    // The tidy alternative (rect copy first, then cell left/top/right/bottom) scores 594 against
    // this shape's 431.
    rectCell.left = rectGridMaybe.left;
    rectCell.top = rectGridMaybe.top;
    rectCell.bottom = rectCell.top + 0x7b;
    rect = rectGridMaybe;
    rectCell.right = rectCell.left + 0xa4;

    if (bResourcesLoadedMaybe) {
        RECT rectDest;
        RECT rectSrc;

        CopyRect(&rectDest, &rect);
        CopyRect(&rectSrc, &rect);
        OffsetRect(&rectDest, rectClipBounds.left, rectClipBounds.top);
        OffsetRect(&rectSrc, rectIconMaybe.left, rectIconMaybe.top);
        pIconRealizedMaybe->RestoreOverlapBlt(rectDest, g_pDDrawWorkSurface, rectSrc, 1);
    }

    nSlot = 0;
    for (nRow = 0; nRow < g_pDPlaySessionMgr->nProviderSlotRows; nRow++) {
        for (nCol = 0; nCol < g_pDPlaySessionMgr->nProviderSlotsPerRow; nCol++) {
            ResourceRef *pCell = aPlayerAvatars[nRow * 3 + nCol];

            pCell->rect = rectCell;

            if (nSlot < g_pDPlaySessionMgr->field_0x8) {
                if (g_pDPlaySessionMgr->aProviderSlots[nSlot].providerId == 0) {
                    pCell->DrawFrame(1, NULL);
                } else {
                    HDC hdc;
                    int nOldBkMode;
                    COLORREF oldTextColor;
                    HGDIOBJ hOldFont;

                    pCell->DrawFrame(2, NULL);
                    hdc = AcquireWorkSurfaceDC(hwndSelf);
                    nOldBkMode = SetBkMode(hdc, TRANSPARENT);
                    oldTextColor = SetTextColor(hdc, 0);
                    hOldFont = SelectObject(hdc, g_UIResources.m_hFont16);
                    DrawTextA(hdc, g_pDPlaySessionMgr->aProviderSlots[nSlot].sAddressOrName, -1,
                              &rectCell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, hOldFont);
                    SetBkMode(hdc, nOldBkMode);
                    SetTextColor(hdc, oldTextColor);
                    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
                }
                nSlot++;
            } else {
                pCell->DrawFrame(0, NULL);
            }

            OffsetRect(&rectCell, 0xa5, 0);
        }

        OffsetRect(&rectCell, 0, 0x7c);
        rectCell.left = rectGridMaybe.left;
        rectCell.right = rectCell.left + 0xa4;
    }
}

// Refreshes the one-line connect-status message. While a connect is pending AND the layout-sync
// ping has gone out, the "go" button is redrawn and the message becomes string 0x71
// ("connecting"); otherwise the button's own rect is restored out of the backdrop and the message
// is the primary or secondary "pick a provider" string. Either way the text is reloaded into
// textBuf0x120 and relaid.
//
// FUNCTION: LOCO 0x409360
void ApplSetupWnd::RefreshConnectStatusText()
{
    if (g_pDPlaySessionMgr->bConnectPending && g_pDPlaySessionMgr->bLayoutSyncPingSent) {
        pApGoBtn->DrawFrame(0, NULL);
        g_UIResources.LoadLocaleString(0x71, textBuf0x120, 0x80);
    } else {
        RECT rect = pApGoBtn->rect;

        if (bResourcesLoadedMaybe) {
            RECT rectDest;
            RECT rectSrc;

            CopyRect(&rectDest, &rect);
            CopyRect(&rectSrc, &rect);
            OffsetRect(&rectDest, rectClipBounds.left, rectClipBounds.top);
            OffsetRect(&rectSrc, rectIconMaybe.left, rectIconMaybe.top);
            pIconRealizedMaybe->RestoreOverlapBlt(rectDest, g_pDDrawWorkSurface, rectSrc, 1);
        }

        // All three LoadLocaleString calls are written out in full, one per arm -- hoisting the
        // string id into a local and calling once collapses the three `push <id>` sites into a
        // single one. cl cross-jumps them back together onto a shared `mov ecx,g_UIResources;
        // call` tail at 0x409498, and even hoists the common `push 0x80` of the inner pair out
        // ahead of the test (0x409478) while leaving the outer arm its own (0x409392).
        if (g_pNetSettings->bUseSecondaryRememberedChoice) {
            g_UIResources.LoadLocaleString(0x70, textBuf0x120, 0x80);
        } else {
            g_UIResources.LoadLocaleString(0x6f, textBuf0x120, 0x80);
        }
    }

    LayoutAndDrawLabel();
}

// vtable slot 0x08 -- the page's activation hook. The resource-realize half is one-shot-gated on
// bResourcesLoadedMaybe (the backdrop descriptor is realized inline, then all 14 ResourceRefs are
// loaded); the rest runs on every activation: re-layout, reset the redraw gate, put the session
// manager into mode 0, kick off either a provider rebuild or -- in secondary-remembered-choice
// mode -- a providers reset plus an index-file reload, take the modal capture, and arm both
// redraw timers.
//
// FUNCTION: LOCO 0x409c60
void ApplSetupWnd::BeginModalCapture()
{
    SoundBankEntry *pEntry;
    int i;

    if (bResourcesLoadedMaybe == 0) {
        pIconDescMaybe = g_UIResources.TileKind_GetOrLoadDescriptor(0x439);
        pIconRealizedMaybe = pIconDescMaybe->GetOrLoadFrameBitmap(0, 0);
        pApGoBtn->Load();
        pApExitBtn->Load();
        pApSearchBtn->Load();
        pApOptionBtn->Load();
        pApMatrixBtn->Load();
        for (i = 0; i < 9; i++) {
            aPlayerAvatars[i]->Load();
        }
        bResourcesLoadedMaybe = 1;
    }

    RefreshClientClipRect();
    bReadyForRedrawMaybe = 0;
    pDrawnListHeadMaybe = NULL;
    field_0xe8 = 1;
    g_pDPlaySessionMgr->SetMode(0);
    bUnk0x10c = 0;

    if (g_pNetSettings->bUseSecondaryRememberedChoice) {
        g_pDPlaySessionMgr->ResetProviders(0);
        LoadIndexFileMaybe(1);
    } else {
        RebuildAndNotifyMaybe();
    }

    WindowBase::BeginModalCapture();
    SetFocus(hwndSelf);
    hTimerA = SetTimer(hwndSelf, 0x50, 0x32, NULL);
    hTimerB = SetTimer(hwndSelf, 0x52, 0x4b, NULL);

    pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
    }

    nUnk0x1b0 = 2;
}

// vtable slot 0x04 -- the deactivation half of BeginModalCapture: drops the drawn-list cache and
// the realize pass, then stops both redraw timers.
//
// FUNCTION: LOCO 0x409db0
void ApplSetupWnd::EndActiveSession()
{
    int i;

    field_0xe8 = 0;
    bUnk0x114 = 0;
    bUnk0x10c = 0;
    WindowBase::EndActiveSession();
    pDrawnListHeadMaybe = NULL;

    if (bResourcesLoadedMaybe) {
        pApGoBtn->ReleaseRealized();
        pApExitBtn->ReleaseRealized();
        pApSearchBtn->ReleaseRealized();
        pApOptionBtn->ReleaseRealized();
        pApMatrixBtn->ReleaseRealized();
        pIconDescMaybe->ReleaseRef();
        pIconDescMaybe = NULL;
        for (i = 0; i < 9; i++) {
            aPlayerAvatars[i]->ReleaseRealized();
        }
        bResourcesLoadedMaybe = 0;
    }

    KillTimer(hwndSelf, hTimerA);
    KillTimer(hwndSelf, hTimerB);
}

// vtable slot 0x50 (WM_MOUSEMOVE) -- purely a cursor-shape update. Hovering any of the four
// live tray buttons -- or the 3x3 provider grid -- arms the anipoint cursor; anywhere else falls
// back to the plain point cursor. The "go" button and the grid are only live while the session
// is actually connecting (bConnectPending), and the grid additionally needs the layout-sync ping
// to have gone out. The "matrix" button (+0x230) is deliberately NOT hit-tested here.
//
// FUNCTION: LOCO 0x40b0a0
LRESULT ApplSetupWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;

    if (bSuppressCursorRedraw) {
        return 0;
    }

    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;

    if ((PtInRect(&pApGoBtn->rect, pt) && g_pDPlaySessionMgr->bConnectPending) ||
        PtInRect(&pApExitBtn->rect, pt) ||
        PtInRect(&pApSearchBtn->rect, pt) ||
        PtInRect(&pApOptionBtn->rect, pt) ||
        (PtInRect(&rectGridMaybe, pt) && g_pDPlaySessionMgr->bLayoutSyncPingSent &&
         g_pDPlaySessionMgr->bConnectPending)) {
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
    } else {
        RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    }
    return 0;
}

// vtable slot 0x80 (WM_CLOSE) -- while the app is alive and not already tearing down, closing
// this page does not close anything: it cancels the pending cursor-mode transition, tears the
// session down and bounces the front end back to SplashWnd state 7. Exactly the same body
// AbortToDisconnectedStateMaybe (0x40a220) carries -- written out here rather than called,
// which is what the original does too.
//
// FUNCTION: LOCO 0x40ada0
LRESULT ApplSetupWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        ScheduleModeTransition(NULL, 0, NULL, 0, 1);
        GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
        g_pDPlaySessionMgr->SetMode(3);
        g_pSplashWnd->SetState(7);
        bUnk0x114 = 0;
        return 0;
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x40 -- WM_RBUTTONDOWN is simply WM_LBUTTONDOWN here, forwarded THROUGH the
// vtable exactly as an unqualified call to a virtual member compiles; do not "optimize" it to a
// class-qualified call. UNMARKED: this body ICF-folds onto 0x4323c0, whose marker lives on
// MapWnd::OnRButtonDown (src/MapWnd.cpp). The WindowBase hierarchy's copies fold to a
// DIFFERENT address than the other family's purely because the slot displacement differs.
LRESULT ApplSetupWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x2c -- the page's catch-all. Its only job is to intercept the screen-saver
// system command and re-run the app's own window-visibility pass before letting DefWindowProcA
// do whatever it was going to do anyway (the return value is DefWindowProcA's, unconditionally).
//
// FUNCTION: LOCO 0x40b4c0
LRESULT ApplSetupWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_SCREENSAVE) {
        FUN_00463670_LotsOfShowWindow();
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x30 (WM_TIMER) -- the page's animation, structurally the same function as
// NetSetupWnd::OnTimerDefaultMaybe (0x4423d0): the status label crawls one pixel per tick around
// the inside edge of rectTextAreaMaybe, and each tick repaints it clipped to that box.
// nUnk0x1b0 is the leg of the circuit -- 0 = crawling left, 1 = right, 2 = up, 3 = down -- and
// the four-way test at the tail turns the corner (re-laying the label from scratch) whenever it
// has run off the far edge of the leg it was on.
//
// This page owns TWO timers where NetSetupWnd owns one: 0x50 drives the crawl, and 0x52 is
// re-armed to a 50ms period on every one of its own ticks (BeginModalCapture starts it at 75ms).
// Both ids run the same repaint; anything else falls through to the base default.
//
// FUNCTION: LOCO 0x40b180
LRESULT ApplSetupWnd::OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bUnk0x114 && wParam == 0x52) {
        KillTimer(hwndSelf, hTimerB);
        hTimerB = SetTimer(hwndSelf, wParam, 0x32, NULL);
    }

    if (bUnk0x114 == 0 || (wParam != 0x50 && wParam != 0x52)) {
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

    switch (nUnk0x1b0) {
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
        DrawTextA(hdc, textBuf0x120, -1, &rectDraw, DT_SINGLELINE | DT_RIGHT);
    } else if (rectLabelMaybe.top < rectTextAreaMaybe.top) {
        DrawTextA(hdc, textBuf0x120, -1, &rectDraw, DT_SINGLELINE | DT_BOTTOM);
    } else if (rectLabelMaybe.bottom > rectTextAreaMaybe.bottom) {
        DrawTextA(hdc, textBuf0x120, -1, &rectDraw, DT_SINGLELINE | DT_TOP);
    } else {
        DrawTextA(hdc, textBuf0x120, -1, &rectDraw, DT_SINGLELINE | DT_LEFT);
    }

    SetBkMode(hdc, nOldBkMode);
    SelectObject(hdc, hOldFont);
    SetTextColor(hdc, oldTextColor);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
    pApMatrixBtn->DrawFrame(0, NULL);

    if (rectLabelMaybe.left >= rectTextAreaMaybe.right && nUnk0x1b0 == 1) {
        nUnk0x1b0 = 2;
        LayoutAndDrawLabel();
    } else if (rectLabelMaybe.bottom <= rectTextAreaMaybe.top && nUnk0x1b0 == 2) {
        nUnk0x1b0 = 0;
        LayoutAndDrawLabel();
    } else if (rectLabelMaybe.right <= rectTextAreaMaybe.left && nUnk0x1b0 == 0) {
        nUnk0x1b0 = 3;
        LayoutAndDrawLabel();
    } else if (rectLabelMaybe.top >= rectTextAreaMaybe.bottom && nUnk0x1b0 == 3) {
        nUnk0x1b0 = 1;
        LayoutAndDrawLabel();
    }

    CommitRectUpdate(rectTextAreaMaybe);
    return 0;
}

// EFFECTIVE MATCH -- 628 B vs 640, insns 212/213, reg_pen 3. ONE instruction differs in the
// whole function: at the point where the VK_UP and VK_DOWN arms merge into the shared
// upper-bound test, the original materializes the row count into a register first
// (`mov eax,[esi+0x104]; cmp edi,eax`) while ours folds it into the compare
// (`cmp edi,[esi+0x104]`). Both arms already hold the count in eax on their own edge, so this
// is purely which edge cl chose to rematerialize it on -- a CSE-availability tie-break at a
// two-predecessor merge, not a source-shape difference. Six source shapes were measured and
// FOUR of them compile to byte-identical output (combined `||` guard / two separate ifs /
// UP-local `< 0` guard + shared upper bound / per-arm SetState(6)), so the construct is
// genuinely under-determined here. Two shapes are strictly worse and are what pinned the rest
// of the function: `nListRowCountMaybe <= nIndex` operand order (98367 vs 88365) and UP-before-
// DOWN case order (160512) -- the latter is what proves the original's source order is DOWN
// first, even though the jump table lists the cases by ascending key code.
//
// vtable slot 0x54 (WM_KEYDOWN) -- the keyboard mirror of OnLButtonDown. RETURN is the "go"
// button, ESCAPE the "exit" button (each flashing its own art for 150ms before acting), and
// UP/DOWN walk the provider list one wrapped row at a time. Everything else, and every arm that
// finds nothing to select, falls through to DefWindowProcA.
//
// The switch arms only COMPUTE the wrapped row index; the upper-bound test and the whole
// select-and-repaint tail sit once, after the switch. Writing the tail out per arm instead
// costs 200 bytes -- cl does not cross-jump two copies of a block that long, so the shared
// tail has to be shared in the source too. The two arms' bounds checks are asymmetric because
// their resets are: DOWN lands on 0, which the shared upper-bound test already covers, while
// UP lands on nListRowCountMaybe - 1, still negative on an empty list.
//
// FUNCTION: LOCO 0x40ae20
LRESULT ApplSetupWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int nIndex;

    if (bReadyForRedrawMaybe == 0) {
        return 0;
    }

    switch (wParam) {
    case VK_RETURN:
        if (g_pDPlaySessionMgr->bConnectPending == 0) {
            return 0;
        }
        pApGoBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        Sleep(0x96);
        ScheduleModeTransition(NULL, 0, NULL, 0, 1);
        g_pDPlaySessionMgr->SetMode(2);
        if (g_pNetSettings->bUseSecondaryRememberedChoice) {
            if (g_pDPlaySessionMgr->bConnectPending == 0) {
                ScheduleModeTransition(NULL, 0, NULL, 0, 1);
                GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
                g_pDPlaySessionMgr->SetMode(3);
                g_pSplashWnd->SetState(7);
                bUnk0x114 = 0;
                return 0;
            }
        } else {
            if (g_pDPlaySessionMgr->bConnectPending == 0) {
                AbortToDisconnectedStateMaybe();
                return 0;
            }
            if (g_pDPlaySessionMgr->bLayoutSyncPingSent) {
                g_pSplashWnd->SetState(6);
            }
            return 0;
        }
        g_pSplashWnd->SetState(6);
        return 0;

    case VK_ESCAPE:
        pApExitBtn->DrawFrame(1, NULL);
        CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
        Sleep(0x96);
        ScheduleModeTransition(NULL, 0, NULL, 0, 1);
        GameNet_ResetProvidersAndPostTeardown(g_pDPlaySessionMgr);
        g_pDPlaySessionMgr->SetMode(3);
        g_pSplashWnd->SetState(7);
        bUnk0x114 = 0;
        return 0;

    case VK_DOWN:
        nIndex = nUnk0xf4 + 1;
        if (nIndex >= nListRowCountMaybe) {
            nIndex = 0;
        }
        break;

    case VK_UP:
        nIndex = nUnk0xf4 - 1;
        if (nIndex < 0) {
            nIndex = nListRowCountMaybe - 1;
        }
        break;

    default:
        return DefWindowProcA(hwndMsg, msg, wParam, lParam);
    }

    if (nIndex < 0 || nIndex >= nListRowCountMaybe) {
        return DefWindowProcA(hwndMsg, msg, wParam, lParam);
    }

    if (g_pNetSettings->bUseSecondaryRememberedChoice == 0) {
        g_UIResources.PlayUiSound(0x5015);
        ApplyListSelectionMaybe(nIndex);
    } else if (g_pDPlaySessionMgr->bConnectPending) {
        g_UIResources.PlayUiSound(0x5015);
        nUnk0xf4 = nIndex;
        ApplySecondaryListSelectionMaybe(nIndex);
    }

    if (g_pNetSettings->bUseSecondaryRememberedChoice) {
        DrawProviderList(pListHeadSecondaryMaybe);
    } else {
        DrawProviderList(pListHeadMaybe);
    }
    DrawAvatarGrid();
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}
