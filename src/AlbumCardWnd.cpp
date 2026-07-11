#include <windows.h>
#include <ddraw.h>
#include <string.h>

#include "AlbumCardWnd.h"
#include "LocoBitmap.h"
#include "CarNetState.h"
#include "EditCardWnd.h"
#include "TutorialWnd.h"
#include "MailWnd.h"
#include "UIResources.h"
#include "BuildToolCursorWnd.h"

extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

// AlbumCardWndVtblProbe + g_pAlbumCardWnd: declared in src/AlbumCardWnd.h (this class's own
// header) so other TUs reaching into this class's vtable (e.g. src/EditCardWnd.cpp) share the
// same partial view instead of duplicating it.
// MailWndVtblProbe + g_pMailWnd: declared in src/MailWnd.h, shared the same way.

extern TutorialWnd *g_pTutorialWnd; // DAT_004fd38c

// The global app-state dispatcher (see docs/subsystems.md's own entry) -- a whole separate
// subsystem, not yet transcribed here.
void AppWindow_SetScreenState(int newState); // 0x408130

extern unsigned int g_dwScreenWidth;  // DAT_004851d8
extern unsigned int g_dwScreenHeight; // DAT_00485214

// FUNCTION: LOCO 0x401f50 (v176: first-draft transcribed, not yet byte-matched)
// Chains the WindowBase base ctor, then runs AlbumCardWnd_InitResourceRefs (below) to
// zero every scalar field and realize the class's own ResourceRef-backed icon/grid/tab
// arrays.
AlbumCardWnd::AlbumCardWnd(void *hInstanceParam, UINT resourceIdParam)
    : WindowBase(hInstanceParam, resourceIdParam)
{
    AlbumCardWnd_InitResourceRefs();
}

// FUNCTION: LOCO 0x401fd0 (v176: first-draft transcribed, not yet byte-matched)
// Zero-initializes every scalar field the ctor owns, resolves nLayoutMode from the desktop
// resolution, then realizes every ResourceRef-backed field: the 8 button icons, the 18-slot
// paCardGrid (3 parallel 6-entry sub-arrays -- placeholder rect, strip-frame draw handle, and
// RedrawAllSlots's own rect, see src/AlbumCardWnd.h), the 9-slot paCategoryTabs, and the
// 6 per-slot name buffers' first byte. Finishes by defaulting every paging/enable flag to true.
void AlbumCardWnd::AlbumCardWnd_InitResourceRefs()
{
    hIcon = NULL;
    bWantEraseBlit = 0;
    nStartIndex = 0;
    nBucket = 0;
    nVisibleCount = 0;
    nHitTestIndex = 0;
    Unk0x124 = 9;
    nCurrentBucket = 0;
    bShowCardNames = 1;
    bHaveBackBuffer = 0;
    bInputBlocked = false;
    if ((int)g_dwScreenWidth > 0x320 || (int)g_dwScreenHeight > 0x258) {
        nLayoutMode = 1;
    } else {
        nLayoutMode = 0;
    }

    pExitBtnIcon = new ResourceRef(0x3c04);
    pEditBtnIcon = new ResourceRef(0x3c09);
    pDeleteBtnIcon = new ResourceRef(0x3c05);
    pMailBtnIcon = new ResourceRef(0x3c08);
    pShowNamesBtnIcon = new ResourceRef(0x3c0f);
    pBackArrowIcon = new ResourceRef(0x3c06);
    pForwardArrowIcon = new ResourceRef(0x3c07);
    if (nLayoutMode == 0) {
        pPageIndicatorIcon = new ResourceRef(0x3c0c);
    } else {
        pPageIndicatorIcon = new ResourceRef(0x3c0d);
    }

    for (int i = 0; i < 6; i++) {
        paCardGrid[i] = new ResourceRef(0);
        paCardGrid[6 + i] = new ResourceRef(0x3c0e);
        paCardGrid[12 + i] = new ResourceRef(0);
        aSlotNames[i][0] = 0;
    }

    for (int i2 = 0; i2 < 9; i2++) {
        paCategoryTabs[i2] = new ResourceRef(0);
    }

    bBackArrowEnabled = true;
    bForwardArrowEnabled = true;
    bComposeEnabled = true;
    bMailEnabled = true;
    bAtBucketStart = true;
    bAtBucketEnd = true;
    pBackgroundTileDesc = NULL;
    pPendingCard = NULL;
    bBackgroundTileLoaded = false;
}

// FUNCTION: LOCO 0x4048e0
// Draws (or, if the card at nStartIndex+nSlotIndex no longer loads, erases) one visible
// thumbnail slot. See src/AlbumCardWnd.h for the full behavioral summary.
unsigned char AlbumCardWnd::DrawOrEraseCardSlot(int nSlotIndex)
{
    CarNetState *pCard = g_pPostBagFileCache->FindFirstLoadableCardAtOrAfterIndex(nStartIndex + nSlotIndex, nBucket);
    if (pCard != NULL) {
        g_pPostBagCache->DrawCardThumbnail(bShowCardNames, pCard, g_pDDrawWorkSurface,
                                                       paCardGrid[nSlotIndex]->rect, (unsigned int)hwndSelf, NULL);
        strcpy(aSlotNames[nSlotIndex], pCard->nameB); // sic: dest is 20 bytes, nameB up to 21
        delete pCard;
        paCardGrid[nSlotIndex + 6]->DrawFrame(0, NULL);
        return 1;
    }

    aSlotNames[nSlotIndex][0] = 0;

    RECT rect = paCardGrid[nSlotIndex]->rect;
    if (bWantEraseBlit && bHaveBackBuffer) {
        RECT destRect, srcRect;
        CopyRect(&destRect, &rect);
        CopyRect(&srcRect, &rect);
        OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
        OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
        unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
        if (ok != 1) {
            OutputDebugStringA("AW Blit failure reported");
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x405520
// The decrementing purge/dedup sweep. See src/AlbumCardWnd.h for the full behavioral summary.
void AlbumCardWnd::PurgeDuplicateCards()
{
    int i = nStartIndex + nVisibleCount - 1;
    if (i >= nStartIndex) {
        do {
            CarNetState *pCard = g_pPostBagFileCache->FindFirstLoadableCardAtOrAfterIndex(i, nBucket);
            if (pCard != NULL) {
                if (g_pPostBagFileCache->PurgeDuplicateIndexEntry(pCard) == 1) {
                    g_pPostBagCache->DeleteCardById(pCard, 0);
                    delete pCard;
                }
            }
            i--;
        } while (i >= nStartIndex);
    }

    if ((unsigned int)nStartIndex > 0) {
        CarNetState *pCard = g_pPostBagFileCache->FindFirstLoadableCardAtOrAfterIndex(nStartIndex, nBucket);
        if (pCard == NULL) {
            nStartIndex -= nVisibleCount;
        }
    }

    RedrawAllSlots();
    CommitScreenUpdate(hwndSelf, 0, 0, NULL);
}

// FUNCTION: LOCO 0x404ac0
// Full redraw of the visible card grid. See src/AlbumCardWnd.h for the full behavioral summary.
//
// EFFECTIVE MATCH (asmscore.py --len 747: insns 225/231, byte_diff 53, total 112383):
// structurally identical (same loads/stores/compares, no missing/extra logic) -- the sole
// residuals are 2 small clusters where the back-arrow and forward-arrow state blocks load
// nBucket/the arrow-enabled flag and store the mirror flag in a different order/register
// than the original (e.g. orig loads nBucket THEN stores bAtBucketStart THEN loads
// cVar1; ours loads nBucket THEN loads cVar1 THEN stores bAtBucketStart). Tried:
// hoisting nBucket into an explicit local before the store (no effect, byte-identical
// output), reordering the cVar1-then-store statements to match compiled order (made it
// substantially worse -- confirms statement order DOES matter here, just not in the direction
// tried), and inverting the forward-arrow's >=8 branch polarity (no improvement). Matches the
// documented "block layout is trace-driven and mostly not source-steerable" class (Yoda #15) --
// PARKED, see docs/PARKED.md; don't re-grind without a genuinely new lever.
void AlbumCardWnd::RedrawAllSlots()
{
    unsigned char cVar1;
    HDC hdc;
    unsigned int i;

    for (i = 0; i < (unsigned int)nVisibleCount; i++) {
        DrawOrEraseCardSlot(i);
        RECT rect = paCardGrid[12 + i]->rect;
        if (bWantEraseBlit && bHaveBackBuffer) {
            RECT destRect, srcRect;
            CopyRect(&destRect, &rect);
            CopyRect(&srcRect, &rect);
            OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
            OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
            unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
            if (ok != 1) {
                OutputDebugStringA("AW Blit failure reported");
            }
        }
    }

    hdc = AcquireWorkSurfaceDC(hwndSelf);
    for (i = 0; i < (unsigned int)nVisibleCount; i++) {
        if (bShowCardNames == 1) {
            SetBkMode(hdc, TRANSPARENT);
            COLORREF oldColor = SetTextColor(hdc, RGB(0, 0, 0));
            int oldMode = SetBkMode(hdc, TRANSPARENT);
            HGDIOBJ oldFont = SelectObject(hdc, g_UIResources.m_hFont16);
            DrawTextA(hdc, aSlotNames[i], -1, &paCardGrid[12 + i]->rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFont);
            SetTextColor(hdc, oldColor);
            SetBkMode(hdc, oldMode);
            SetBkMode(hdc, oldMode);
        }
    }

    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);

    if (nStartIndex == 0) {
        bAtBucketStart = true;
        cVar1 = bBackArrowEnabled;
        if (nBucket != 0) goto joined_backArrow;
        if (cVar1 != 1) goto skip_backArrow;
        bBackArrowEnabled = false;
    } else {
        cVar1 = bBackArrowEnabled;
        bAtBucketStart = false;
joined_backArrow:
        if (cVar1 != 0) goto skip_backArrow;
        bBackArrowEnabled = true;
    }
    DrawButtonIcon(5, 0);
skip_backArrow:

    unsigned int nRecordCount = g_pPostBagFileCache->FUN_401810_GetCountDiv24();
    if ((unsigned int)(nStartIndex + nVisibleCount) < nRecordCount) {
        cVar1 = bForwardArrowEnabled;
        bAtBucketEnd = false;
    } else {
        bAtBucketEnd = true;
        cVar1 = bForwardArrowEnabled;
        if ((unsigned int)nBucket >= 8) {
            if (cVar1 != 1) {
                return;
            }
            bForwardArrowEnabled = false;
            DrawButtonIcon(6, 0);
            return;
        }
    }
    if (cVar1 == 0) {
        bForwardArrowEnabled = true;
        DrawButtonIcon(6, 0);
    }
}

// FUNCTION: LOCO 0x403ba0
// Button-icon dispatcher. See src/AlbumCardWnd.h for the full behavioral summary.
//
// Real 10-way (0-9) jump table -- case body order in .text is source declaration order, NOT
// case-value order (VC5 lesson): 1, 2, 3, 9, 4, 5, 6, then the shared cases-0/7/8-plus-default
// no-op tail. Declared in that exact order below to match.
void AlbumCardWnd::DrawButtonIcon(int nCase, int reserved)
{
    switch (nCase) {
    case 1:
        pExitBtnIcon->DrawFrame(0, NULL);
        break;
    case 2:
        if (bComposeEnabled == true) {
            pEditBtnIcon->DrawFrame(0, NULL);
        } else {
            pEditBtnIcon->DrawFrame(2, NULL);
        }
        break;
    case 3:
        pDeleteBtnIcon->DrawFrame(0, NULL);
        break;
    case 9:
        pShowNamesBtnIcon->DrawFrame(0, NULL);
        break;
    case 4:
        if (bMailEnabled == true) {
            pMailBtnIcon->DrawFrame(0, NULL);
        } else {
            pMailBtnIcon->DrawFrame(2, NULL);
        }
        break;
    case 5:
        if (bBackArrowEnabled == true) {
            pBackArrowIcon->DrawFrame(0, NULL);
        } else {
            pBackArrowIcon->DrawFrame(2, NULL);
        }
        break;
    case 6:
        if (bForwardArrowEnabled == true) {
            pForwardArrowIcon->DrawFrame(0, NULL);
        } else {
            pForwardArrowIcon->DrawFrame(2, NULL);
        }
        break;
    case 0:
    case 7:
    case 8:
        break;
    }
}

// FUNCTION: LOCO 0x403cd0
// Hit-test: see src/AlbumCardWnd.h for the full behavioral summary.
//
// The POINT is a BY-VALUE PARAMETER, not a local built from two int params -- that is what
// makes this an exact match. Parked from v141 to v361 as an "intrinsic dead-parameter-slot-reuse
// tie-break" (the loop counter's spill slot landed at [esp+0x18], y's dead slot, where the
// original uses [esp+0x14]); every probe back then flipped some spelling of a `POINT pt; pt.x =
// x; pt.y = y;` LOCAL, which is the wrong axis. Two int params leave two independent 4-byte dead
// slots and VC5 reuses the higher one; ONE dead 8-byte aggregate param is a single slot whose
// base is the lower address, so the spill lands at +0x14 for free. Codegen is otherwise
// byte-identical between the two spellings, so the only observable that distinguishes them is
// this slot -- see docs/CODEGEN.md.
int AlbumCardWnd::HitTestUiElement(POINT pt)
{
    if (PtInRect(&pExitBtnIcon->rect, pt)) {
        return 1;
    }
    if (PtInRect(&pShowNamesBtnIcon->rect, pt)) {
        return 9;
    }
    if (PtInRect(&pMailBtnIcon->rect, pt)) {
        return 4;
    }
    if (PtInRect(&pEditBtnIcon->rect, pt)) {
        return 2;
    }
    if (PtInRect(&pDeleteBtnIcon->rect, pt)) {
        return 3;
    }
    if (PtInRect(&pBackArrowIcon->rect, pt)) {
        return 5;
    }
    if (PtInRect(&pForwardArrowIcon->rect, pt)) {
        return 6;
    }

    unsigned short i;
    for (i = 0; i < 6; i++) {
        if (PtInRect(&paCardGrid[i]->rect, pt)) {
            nHitTestIndex = i;
            return 8;
        }
        if (PtInRect(&paCardGrid[i + 12]->rect, pt)) {
            nHitTestIndex = i;
            return 10;
        }
    }
    for (unsigned short j = 0; j < 9; j++) {
        if (PtInRect(&paCategoryTabs[j]->rect, pt)) {
            nHitTestIndex = j;
            return 7;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x403e80
// Per-button press-feedback: see src/AlbumCardWnd.h for the full behavioral summary. Same
// erase-blit idiom as DrawOrEraseCardSlot/RedrawAllSlots/OnActivate above, duplicated per case
// (not factored into a shared helper -- matches this class's own established style).
//
// Real 10-way (0-9) jump table -- case body order in .text is source declaration order, NOT
// case-value order (VC5 lesson, same as this class's sibling DrawButtonIcon): 1, 2, 3, 9, 4, 5,
// 6, 7, then the shared cases-0/8 no-op tail. Declared in that exact order below to match.
// sic: case 6 plays its click sound AFTER (not before, unlike every other case) its
// DrawFrame call -- confirmed via raw disasm, not a decompiler artifact.
//
// EFFECTIVE MATCH -- content-complete, real body 100% byte-identical (`asmscore.py --len 2165`:
// insns 634/634 once the gated cases' guard polarity was inverted, byte_diff 0 within the true
// 2165-byte extent). `verify.py`/`cc.sh` still print DIFF(3)/len=2208: our compiled COMDAT
// carries 43 extra trailing alignment-padding bytes (zero-filled to the next 32-byte boundary)
// that `tools/match.py`'s `trim_pad` doesn't strip (it only trims trailing 0x90/0xCC, not NUL
// runs) -- comparing that padding against whatever sits at the same offset in the shipped EXE
// (the shared switch's own jump-table data, a different structure entirely) is what produces
// the 3 spurious byte mismatches. Real per-case content confirmed byte-for-byte via
// `asmscore.py --dump --len 2165`. Two real structural fixes this session: (1) the whole
// function's real signature is `(int nCase, int reserved)`, not the 1-param signature
// Ghidra had -- `ret 0x8` at every return site (not `ret 0x4`) and 2 pushes at every call site
// were the tell, same under-analyzed-callee pattern as sibling DrawButtonIcon; every
// OnLButtonDown call site was fixed to pass the real 2nd arg (`nHitTestIndex` for
// cases 7/8/10, `0` elsewhere, mirroring DrawButtonIcon's own already-correct call sites). (2) the
// 4 gated cases (2/4/5/6)'s guard polarity was inverted from the natural `if (!enabled) {dimmed;
// return;} enabled...` guard-clause shape to `if (enabled) {enabled...; return;} dimmed...` --
// the natural shape put the (correct, semantically identical) SHORT dimmed-path on the
// fall-through, while the original places the LONG enabled-path as the fall-through and the
// short dimmed-path out-of-line at a distant shared tail; this is the same "if/else branch order
// is a genuine correctness lever when the two arms have real, unequal-sized content" class
// already documented on `AlbumCardWnd::DrawOrEraseCardSlot` (3rd confirmation).
void AlbumCardWnd::PlayButtonPressFeedback(int nCase, int reserved)
{
    switch (nCase) {
    case 1:
        g_UIResources.PlayUiSound(0x5015);
        {
            RECT rect = pExitBtnIcon->rect;
            if (bWantEraseBlit && bHaveBackBuffer) {
                RECT destRect, srcRect;
                CopyRect(&destRect, &rect);
                CopyRect(&srcRect, &rect);
                OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                if (ok != 1) {
                    OutputDebugStringA("AW Blit failure reported");
                }
            }
        }
        pExitBtnIcon->DrawFrame(1, NULL);
        return;

    case 2:
        if (bComposeEnabled == true) {
            g_UIResources.PlayUiSound(0x5015);
            {
                RECT rect = pEditBtnIcon->rect;
                if (bWantEraseBlit && bHaveBackBuffer) {
                    RECT destRect, srcRect;
                    CopyRect(&destRect, &rect);
                    CopyRect(&srcRect, &rect);
                    OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                    OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                    unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                    if (ok != 1) {
                        OutputDebugStringA("AW Blit failure reported");
                    }
                }
            }
            pEditBtnIcon->DrawFrame(1, NULL);
            return;
        }
        pEditBtnIcon->DrawFrame(2, NULL);
        return;

    case 3:
        g_UIResources.PlayUiSound(0x5015);
        {
            RECT rect = pDeleteBtnIcon->rect;
            if (bWantEraseBlit && bHaveBackBuffer) {
                RECT destRect, srcRect;
                CopyRect(&destRect, &rect);
                CopyRect(&srcRect, &rect);
                OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                if (ok != 1) {
                    OutputDebugStringA("AW Blit failure reported");
                }
            }
        }
        pDeleteBtnIcon->DrawFrame(1, NULL);
        return;

    case 9:
        g_UIResources.PlayUiSound(0x5015);
        {
            RECT rect = pShowNamesBtnIcon->rect;
            if (bWantEraseBlit && bHaveBackBuffer) {
                RECT destRect, srcRect;
                CopyRect(&destRect, &rect);
                CopyRect(&srcRect, &rect);
                OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                if (ok != 1) {
                    OutputDebugStringA("AW Blit failure reported");
                }
            }
        }
        pShowNamesBtnIcon->DrawFrame(1, NULL);
        return;

    case 4:
        if (bMailEnabled == true) {
            g_UIResources.PlayUiSound(0x5015);
            {
                RECT rect = pMailBtnIcon->rect;
                if (bWantEraseBlit && bHaveBackBuffer) {
                    RECT destRect, srcRect;
                    CopyRect(&destRect, &rect);
                    CopyRect(&srcRect, &rect);
                    OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                    OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                    unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                    if (ok != 1) {
                        OutputDebugStringA("AW Blit failure reported");
                    }
                }
            }
            pMailBtnIcon->DrawFrame(1, NULL);
            return;
        }
        pMailBtnIcon->DrawFrame(2, NULL);
        return;

    case 5:
        if (bBackArrowEnabled == true) {
            g_UIResources.PlayUiSound(0x5015);
            {
                RECT rect = pBackArrowIcon->rect;
                if (bWantEraseBlit && bHaveBackBuffer) {
                    RECT destRect, srcRect;
                    CopyRect(&destRect, &rect);
                    CopyRect(&srcRect, &rect);
                    OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                    OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                    unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                    if (ok != 1) {
                        OutputDebugStringA("AW Blit failure reported");
                    }
                }
            }
            pBackArrowIcon->DrawFrame(1, NULL);
            return;
        }
        pBackArrowIcon->DrawFrame(2, NULL);
        return;

    case 6:
        if (bForwardArrowEnabled == true) {
            {
                RECT rect = pForwardArrowIcon->rect;
                if (bWantEraseBlit && bHaveBackBuffer) {
                    RECT destRect, srcRect;
                    CopyRect(&destRect, &rect);
                    CopyRect(&srcRect, &rect);
                    OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
                    OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
                    unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
                    if (ok != 1) {
                        OutputDebugStringA("AW Blit failure reported");
                    }
                }
            }
            pForwardArrowIcon->DrawFrame(1, NULL);
            g_UIResources.PlayUiSound(0x5015); // sic: sound after the draw here, unlike every other case
            return;
        }
        pForwardArrowIcon->DrawFrame(2, NULL);
        return;

    case 7:
        g_UIResources.PlayUiSound(0x5015);
        pPageIndicatorIcon->DrawFrame(nCurrentBucket, NULL);
        return;

    case 0:
    case 8:
        break;
    }
}

// FUNCTION: LOCO 0x404db0 (Ghidra: AlbumCardWnd::OnActivate, WindowBase vtable+0x20
// override -- base default is WindowBase::NoOpVirtualMaybe, a bare no-op). See src/AlbumCardWnd.h
// for the full behavioral summary. Same erase-blit idiom as DrawOrEraseCardSlot/RedrawAllSlots above.
void AlbumCardWnd::OnActivate(int reserved)
{
    if (bHaveBackBuffer == 0) {
        bHaveBackBuffer = 1;
    }

    if (bWantEraseBlit && bHaveBackBuffer) {
        RECT rect = rectClipBounds;
        RECT destRect, srcRect;
        CopyRect(&destRect, &rect);
        CopyRect(&srcRect, &rect);
        OffsetRect(&destRect, rectClipBounds.left, rectClipBounds.top);
        OffsetRect(&srcRect, rectBackBufSrc.left, rectBackBufSrc.top);
        unsigned char ok = pBackBuffer->RestoreOverlapBlt(destRect, g_pDDrawWorkSurface, srcRect, 1);
        if (ok != 1) {
            OutputDebugStringA("AW Blit failure reported");
        }
    }

    DrawButtonIcon(4, 0);
    DrawButtonIcon(1, 0);
    DrawButtonIcon(2, 0);
    DrawButtonIcon(3, 0);
    DrawButtonIcon(9, 0);
    DrawButtonIcon(5, 0);
    DrawButtonIcon(6, 0);
    pPageIndicatorIcon->DrawFrame(nCurrentBucket, NULL);
    RedrawAllSlots();
    CommitScreenUpdate(hwndSelf, 0, 0, NULL);

    if ((unsigned char)g_pTutorialWnd->NotifyOrLaunch(2, 0) != 0) {
        bInputBlocked = true;
    }
}

// FUNCTION: LOCO 0x404f60 (Ghidra: AlbumCardWnd::OnLButtonDown, WindowBase vtable+0x38
// override -- the WindowBase-wide WM_LBUTTONDOWN convention slot). See src/AlbumCardWnd.h for
// the full behavioral summary. Case body order below follows the jump table's own address order
// (read via the VA->file-offset raw-byte technique), NOT case-value order, per the documented
// VC5 jump-table lesson: 1, 4, 2, 3, 7, 8, 9, 10, then 5, 6.
//
// PARKED (`asmscore.py --len 1384`, DIFF 954/1324 as of v167 -- down from v166's 1000/1384
// after fixing every PlayButtonPressFeedback call site to pass its real 2nd arg, see docs/PARKED.md).
LRESULT AlbumCardWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int x = lParam & 0xffff;
    unsigned int y = (unsigned int)lParam >> 0x10;

    if (bInputBlocked != false) {
        return 0;
    }

    POINT pt;
    pt.y = y;
    pt.x = x;
    int nCmdId = HitTestUiElement(pt);

    if (pPendingCard != NULL) {
        if (nCmdId == 2) {
            g_UIResources.PlayUiSound(0x5015);
            g_pEditCardWnd->BeginEdit(pPendingCard);
            this->EndActiveSession();
            if (pPendingCard != NULL) {
                pPendingCard = NULL;
            }
            bHaveBackBuffer = 0;
            pPendingCard = NULL;
            return 0;
        }
        if (nCmdId != 3) {
            if (nCmdId != 4) {
                return 0;
            }
            g_UIResources.PlayUiSound(0x5015);
            CarNetState *pBusyCard = pPendingCard;
            g_pPostBagCache->PostBag_SaveCardToCategory(pBusyCard, 2, NULL);
            ((MailWndVtblProbe *)g_pMailWnd)->Refresh();
            this->EndActiveSession();
            pPendingCard = pBusyCard;
            bHaveBackBuffer = 0;
            return 0;
        }
        g_UIResources.PlayUiSound(0x5015);
        CarNetState *pBusyCard = pPendingCard;
        if (g_pPostBagFileCache->PurgeDuplicateIndexEntry(pBusyCard) == 1) {
            g_pPostBagCache->DeleteCardById(pBusyCard, 0);
            delete pBusyCard;
            RedrawAllSlots();
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            pBusyCard = NULL;
        }
        pPendingCard = pBusyCard;
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        return 0;
    }

    unsigned int uTemp;
    int nStartCandidate;
    CarNetState *pCard;

    switch (nCmdId) {
    case 1:
        PlayButtonPressFeedback(nCmdId, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        this->EndActiveSession();
        AppWindow_SetScreenState(3);
        return 0;

    case 4:
        if (bMailEnabled == true) {
            PlayButtonPressFeedback(nCmdId, 0);
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            ((MailWndVtblProbe *)g_pMailWnd)->Refresh();
            this->EndActiveSession();
            bHaveBackBuffer = 0;
            return 0;
        }
        break;

    case 2:
        if (bComposeEnabled == true) {
            PlayButtonPressFeedback(nCmdId, 0);
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            g_pEditCardWnd->BeginEdit(pPendingCard);
            this->EndActiveSession();
            if (pPendingCard != NULL) {
                pPendingCard = NULL;
            }
            bHaveBackBuffer = 0;
            return 0;
        }
        break;

    case 3:
        PlayButtonPressFeedback(nCmdId, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        DrawButtonIcon(nCmdId, 0);
        g_pBuildToolCursorWnd->ShowTool(10, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;

    case 7:
        uTemp = nHitTestIndex;
        if (nCurrentBucket != uTemp) {
            nCurrentBucket = uTemp;
            nBucket = uTemp;
            nStartIndex = 0;
            RedrawAllSlots();
            PlayButtonPressFeedback(nCmdId, nHitTestIndex);
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            return 0;
        }
        break;

    case 8:
        PlayButtonPressFeedback(nCmdId, nHitTestIndex);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        DrawButtonIcon(nCmdId, nHitTestIndex);
        uTemp = nHitTestIndex;
        g_UIResources.PlayUiSound(0x5015);
        nStartCandidate = nBucket;
        nCmdId = nStartIndex + uTemp;
        goto LAB_load_card;

    case 9:
        PlayButtonPressFeedback(nCmdId, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        DrawButtonIcon(nCmdId, 0);
        bShowCardNames = bShowCardNames == 0;
        RedrawAllSlots();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;

    case 10:
        PlayButtonPressFeedback(nCmdId, nHitTestIndex);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        DrawButtonIcon(nCmdId, nHitTestIndex);
        uTemp = nHitTestIndex;
        g_UIResources.PlayUiSound(0x5015);
        nStartCandidate = nBucket;
        nCmdId = nStartIndex + uTemp;
    LAB_load_card:
        pCard = g_pPostBagFileCache->FindFirstLoadableCardAtOrAfterIndex(nCmdId, nStartCandidate);
        pPendingCard = pCard;
        if (pCard != NULL) {
            this->RequestModeTransitionFromSource(pCardCursorRect, pCardCursorDesc, 0, 1);
        }
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;

    case 5:
        if (bBackArrowEnabled != true) {
            return 0;
        }
        PlayButtonPressFeedback(nCmdId, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        if (bAtBucketStart == true) {
            uTemp = nCurrentBucket - 1;
            nCurrentBucket = uTemp;
            nBucket = uTemp;
            uTemp = g_pPostBagFileCache->LoadBucketAndGetRecordCount(uTemp);
            nStartIndex = uTemp / nVisibleCount;
            uTemp = g_pPostBagFileCache->FUN_401810_GetCountDiv24();
            if ((uTemp % nVisibleCount == 0) &&
                ((uTemp = g_pPostBagFileCache->FUN_401810_GetCountDiv24()) != 0)) {
                nStartIndex = nStartIndex - 1;
            }
            nStartIndex = nStartIndex * nVisibleCount;
            pPageIndicatorIcon->DrawFrame(nCurrentBucket, NULL);
            goto LAB_page_redraw;
        }
        nStartCandidate = nStartIndex - nVisibleCount;
        goto LAB_page_apply;

    case 6:
        if (bForwardArrowEnabled != true) {
            return 0;
        }
        PlayButtonPressFeedback(nCmdId, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        if (bAtBucketEnd == true) {
            uTemp = nCurrentBucket + 1;
            nCurrentBucket = uTemp;
            nBucket = uTemp;
            nStartIndex = 0;
            pPageIndicatorIcon->DrawFrame(uTemp, NULL);
            goto LAB_page_redraw;
        }
        nStartCandidate = nStartIndex + nVisibleCount;
    LAB_page_apply:
        nStartIndex = nStartCandidate;
    LAB_page_redraw:
        RedrawAllSlots();
        DrawButtonIcon(nCmdId, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;
    }
    return 0;
}

// FUNCTION: LOCO 0x402690 (Ghidra: AlbumCardWnd::OnKeyDown, WindowBase vtable+0x54
// override -- see src/AlbumCardWnd.h for the full behavioral summary.) Case body order below
// follows the jump table's own physical address order (confirmed via objdump: 0xd/0x1b combined,
// then 0x25, then 0x27, then the shared page-redraw tail, then default last), matching the
// documented VC5 jump-table source-declaration-order lesson.
//
// EFFECTIVE MATCH (asmscore.py --len 495: insns 160/136, byte_diff 58 positional/8 real
// (see below), align 144 -- almost entirely trailing jump-table-data decode noise, a documented
// tool caveat). Two real structural levers closed this from an initial DIFF(264)/547 bytes:
// (1) both early-return guards (bBackArrowEnabled/bForwardArrowEnabled false) must
// `goto` the function's own single trailing `return 0;`, not have their own local `return 0;`
// -- independent return-0 sites don't auto-share an epilogue under /O2, but a real `goto` to one
// does (see CLAUDE.md's LocoBitmap::BuildPaletteLUT lesson); (2) the GetCountDiv24 recheck
// needs `> 0`, not `!= 0`, to reproduce the original's `test;jbe` shape (the redundant-recheck
// range-vs-equality lesson). A 3rd lever mattered for the bucket-start block specifically:
// `nCurrentBucket--; nBucket = nCurrentBucket;` (two statements, second one a
// fresh read) reproduces the original's real edx-then-copy-to-eax codegen, whereas a single
// `uTemp` shared across both stores (the natural-looking idiom, and what the bucket-END block
// below uses successfully) does not -- confirmed via a live compile that the two near-identical
// blocks pick asymmetric register strategies in the ORIGINAL too (decrement path splits into 2
// registers, increment path doesn't), so this isn't a copy-paste error on our part, just context-
// sensitive allocation. Remaining DIFF(8): 3 pure register-choice swaps (subtract/add operand
// load order at 2 sites, a hwndSelf-load register choice at a 3rd) -- tried reordering the
// arithmetic operands textually, no effect (byte-identical output either way), confirming the
// documented symmetric-register-swap residual class (Yoda #29/#30): intrinsic, not source-
// steerable. PARKED as EFFECTIVE.
LRESULT AlbumCardWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bInputBlocked != false) {
        return 0;
    }

    unsigned int uTemp;
    int nStartCandidate;

    switch (wParam) {
    case 0xd:
    case 0x1b:
        this->EndActiveSession();
        AppWindow_SetScreenState(3);
        return 0;

    case 0x25:
        if (bBackArrowEnabled != true) {
            goto LAB_return0;
        }
        PlayButtonPressFeedback(5, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        DrawButtonIcon(5, 0);
        if (bAtBucketStart == true) {
            nCurrentBucket--;
            nBucket = nCurrentBucket;
            uTemp = g_pPostBagFileCache->LoadBucketAndGetRecordCount(nCurrentBucket);
            nStartIndex = uTemp / nVisibleCount;
            uTemp = g_pPostBagFileCache->FUN_401810_GetCountDiv24();
            if ((uTemp % nVisibleCount == 0) &&
                ((uTemp = g_pPostBagFileCache->FUN_401810_GetCountDiv24()) > 0)) {
                nStartIndex = nStartIndex - 1;
            }
            nStartIndex = nVisibleCount * nStartIndex;
            pPageIndicatorIcon->DrawFrame(nCurrentBucket, NULL);
            goto LAB_page_redraw;
        }
        nStartCandidate = nStartIndex - nVisibleCount;
        break;

    case 0x27:
        if (bForwardArrowEnabled != true) {
            goto LAB_return0;
        }
        PlayButtonPressFeedback(6, 0);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        DrawButtonIcon(6, 0);
        if (bAtBucketEnd == true) {
            uTemp = nCurrentBucket + 1;
            nCurrentBucket = uTemp;
            nBucket = uTemp;
            nStartIndex = 0;
            pPageIndicatorIcon->DrawFrame(uTemp, NULL);
            goto LAB_page_redraw;
        }
        nStartCandidate = nStartIndex + nVisibleCount;
        break;

    default:
        return DefWindowProcA(hwndMsg, msg, wParam, lParam);
    }

    nStartIndex = nStartCandidate;
LAB_page_redraw:
    RedrawAllSlots();
    CommitScreenUpdate(hwndSelf, 0, 0, NULL);
LAB_return0:
    return 0;
}

// FUNCTION: LOCO 0x4028b0
// vtable slot 0x1c override of WindowBase::RefreshClientClipRect -- the direct sibling of
// EditCardWnd's own override (0x417180, src/EditCardWnd.cpp) and the same shape: chain the
// base implementation (base-qualified, dispatch-bypassing), then re-lay-out this screen's
// ENTIRE child-rect set, so a client-rect refresh (i.e. a resize) repositions everything.
// sic: the bWantEraseBlit guard wraps the base call TOO, not just the layout work -- when the
// button-icon/card-cursor group isn't realized this override skips WindowBase's own
// rectClient/width/height/rectClipBounds refresh entirely, unlike EditCardWnd's sibling
// override which always chains first. Ground-truthed: `test al,al; je <epilogue>` precedes
// the `call 0x425d30`, and hoisting the base call out of the guard is a measurable
// regression (total 1042136 -> 1182910).
//
// The whole layout is anchored to rectLayoutBase, a design-resolution rect ({0,0,800,600} or
// {0,0,1024,768}) centered inside the freshly-refreshed client clip bounds; every child rect
// is then CopyRect'd off it and OffsetRect'd into place. Three block shapes repeat:
//   * button icons -- rect sized from the icon's own realized CursorDesc native extent,
//     then offset to its fixed design-space position;
//   * card slots (paCardGrid[0..5]) and their strip-frame twins (paCardGrid[6..11]) --
//     a fixed 300x200 thumbnail box;
//   * per-slot name labels (paCardGrid[12..17]) -- the same origin as the matching slot,
//     pushed down past the thumbnail and shrunk to a 300x25 caption strip.
// The 9 category tabs are the one loop: the page-indicator icon's own height is divided into
// 9 equal 30px-wide bands walked downward by OffsetRect.
//
// The 800x600 layout is a 2x2 grid of 4 slots (nVisibleCount = 4) with every button along
// the bottom edge (y = 0x20f); the 1024x768 one is a 2x3 grid of 6 (nVisibleCount = 6) with
// the buttons stacked down the right edge (x = 0x397). nVisibleCount is therefore the page
// CAPACITY here, set by layout, and later clamped downward by the paging code.
//
// nLayoutMode is a compare-tree `switch` in the original (`sub ecx,eax; je case0; dec ecx; jne
// out`), not an if/else chain -- an out-of-range value falls straight through to the return
// without laying anything out.
void AlbumCardWnd::RefreshClientClipRect()
{
    extern void CenterRectInRect(RECT *outer, RECT *rect); // 0x425a50

    if (bWantEraseBlit) {
        RECT rc;
        RECT rcTile;
        ResourceRef *pRef;
        int nTab;

        WindowBase::RefreshClientClipRect();

        switch (nLayoutMode) {
        case 0: // 800x600
            rectLayoutBase.left = 0;
            rectLayoutBase.top = 0;
            rectLayoutBase.right = 800;
            rectLayoutBase.bottom = 600;
            CenterRectInRect(&rectClipBounds, &rectLayoutBase);
            SetRectEmpty(&rcTile);
            rcTile.right = pBackgroundTileDesc->nativeWidth;
            rcTile.bottom = pBackgroundTileDesc->nativeHeight;
            CopyRect(&rectBackBufSrc, &rectClipBounds);
            CenterRectInRect(&rcTile, &rectBackBufSrc);
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pExitBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pExitBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x26c, 0x20f);
            pExitBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pEditBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pEditBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x69, 0x20f);
            pEditBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pDeleteBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pDeleteBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x19, 0x20f);
            pDeleteBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pMailBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pMailBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0xb9, 0x20f);
            pMailBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pShowNamesBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pShowNamesBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x108, 0x20f);
            pShowNamesBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pBackArrowIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pBackArrowIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x21e, 0x20f);
            pBackArrowIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pForwardArrowIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pForwardArrowIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x2bc, 0x20f);
            pForwardArrowIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2f5, 0xf);
            pRef = pPageIndicatorIcon;
            rc.right = pRef->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pRef->pCursorDesc->nativeHeight + rc.top;
            pRef->rect = rc;
            rc.bottom = (rc.bottom - rc.top) / 9 + rc.top;
            rc.right = rc.left + 30;
            for (nTab = 0; nTab < 9; nTab++) {
                paCategoryTabs[nTab]->rect = rc;
                OffsetRect(&rc, 0, rc.bottom - rc.top);
            }
            nVisibleCount = 4;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2d, 0x22);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[0]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2d, 0x106);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[1]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x1be, 0x22);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[2]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x1be, 0x106);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[3]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2d, 0x22);
            OffsetRect(&rc, 0, 203);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[12]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2d, 0x106);
            OffsetRect(&rc, 0, 203);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[13]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x1be, 0x22);
            OffsetRect(&rc, 0, 203);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[14]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x1be, 0x106);
            OffsetRect(&rc, 0, 203);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[15]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2b, 0x20);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[6]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x2b, 0x104);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[7]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x1bc, 0x20);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[8]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x1bc, 0x104);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[9]->rect = rc;
            break;
        case 1: // 1024x768
            rectLayoutBase.left = 0;
            rectLayoutBase.top = 0;
            rectLayoutBase.right = 1024;
            rectLayoutBase.bottom = 768;
            CenterRectInRect(&rectClipBounds, &rectLayoutBase);
            SetRectEmpty(&rcTile);
            rcTile.right = pBackgroundTileDesc->nativeWidth;
            rcTile.bottom = pBackgroundTileDesc->nativeHeight;
            CopyRect(&rectBackBufSrc, &rectClipBounds);
            CenterRectInRect(&rcTile, &rectBackBufSrc);
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pExitBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pExitBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x397, 0x145);
            pExitBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pEditBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pEditBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x397, 0x5e);
            pEditBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pDeleteBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pDeleteBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x397, 0x11);
            pDeleteBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pMailBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pMailBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x397, 0xab);
            pMailBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pShowNamesBtnIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pShowNamesBtnIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x397, 0xf8);
            pShowNamesBtnIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pBackArrowIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pBackArrowIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x22, 0x279);
            pBackArrowIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            rc.right = pForwardArrowIcon->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pForwardArrowIcon->pCursorDesc->nativeHeight + rc.top;
            OffsetRect(&rc, 0x397, 0x279);
            pForwardArrowIcon->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x366, 0x8);
            pRef = pPageIndicatorIcon;
            rc.right = pRef->pCursorDesc->nativeWidth + rc.left;
            rc.bottom = pRef->pCursorDesc->nativeHeight + rc.top;
            pRef->rect = rc;
            rc.bottom = (rc.bottom - rc.top) / 9 + rc.top;
            rc.right = rc.left + 30;
            for (nTab = 0; nTab < 9; nTab++) {
                paCategoryTabs[nTab]->rect = rc;
                OffsetRect(&rc, 0, rc.bottom - rc.top);
            }
            nVisibleCount = 6;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9d, 0x1b);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[0]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9d, 0xff);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[1]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9d, 0x1e3);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[2]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22e, 0x1b);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[3]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22e, 0xff);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[4]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22e, 0x1e3);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[5]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9d, 0x1b);
            OffsetRect(&rc, 0, 200);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[12]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9d, 0xff);
            OffsetRect(&rc, 0, 200);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[13]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9d, 0x1e3);
            OffsetRect(&rc, 0, 200);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[14]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22e, 0x1b);
            OffsetRect(&rc, 0, 200);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[15]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22e, 0xff);
            OffsetRect(&rc, 0, 200);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[16]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22e, 0x1e3);
            OffsetRect(&rc, 0, 200);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 25;
            paCardGrid[17]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9b, 0x19);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[6]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9b, 0xfd);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[7]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x9b, 0x1e1);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[8]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22c, 0x19);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[9]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22c, 0xfd);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[10]->rect = rc;
            CopyRect(&rc, &rectLayoutBase);
            OffsetRect(&rc, 0x22c, 0x1e1);
            rc.right = rc.left + 300;
            rc.bottom = rc.top + 200;
            paCardGrid[11]->rect = rc;
            break;
        }
    }
}

// FUNCTION: LOCO 0x4055e0 (Ghidra: AlbumCardWnd::OnRButtonDown, WindowBase vtable+0x40
// override -- the class-wide WM_RBUTTONDOWN convention slot (corrected from an earlier
// WM_LBUTTONUP guess, see WindowBase::RouteMessageMaybe's ground-truth dispatch table). See
// src/AlbumCardWnd.h for the full behavioral summary.)
LRESULT AlbumCardWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (pPendingCard != NULL) {
        delete pPendingCard;
        pPendingCard = NULL;
        this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    }
    return 0;
}

// FUNCTION: LOCO 0x405620 (WindowBase vtable+0x60 override -- the class-wide WM_SETFOCUS
// convention slot. See src/AlbumCardWnd.h for the behavioral summary.)
// EFFECTIVE MATCH: 32/32 instructions, same length (92 B), same operands, same call -- the
// entire residual is the 6-instruction argument setup for the tail stub call. The original
// rotates just TWO scratch registers (edx/eax), reloading each parameter from its esp-relative
// slot as the pushes shift esp; cl gives us a three-register (ecx/edx/eax) load-all-up-front
// schedule instead. Byte-identical everywhere above the join, so this is a pure allocator
// coin-flip, not a source-shape problem. This is the SAME residual class as
// MailWnd::OnSetFocus (0x42fe80) two TUs over -- identical construct, identical symptom --
// where routing the result through a named LRESULT local and calling DefWindowProcA directly
// were both already probed and refuted. Probed here additionally without effect: an explicit
// `else` around the fall-through. See docs/PARKED.md.
LRESULT AlbumCardWnd::OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bInputBlocked) {
        PostMessageA(g_pTutorialWnd->hwndSelf, WM_SETFOCUS, 0, 0);
        SetWindowPos(g_pTutorialWnd->hwndSelf, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        return 0;
    }
    return WindowBase_DefWindowProcStub(hwndMsg, msg, wParam, lParam);
}

// g_pApp / g_nScreenState reached without pulling AppWindow.h into the whole TU, and the
// screen-saver interceptor -- the same shapes several sibling TUs already carry locally.
class AppWindow;
extern AppWindow *g_pApp;  // DAT_004aa4a0
extern int g_nScreenState; // DAT_004851f4
extern unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void); // 0x463670 // TODO: idiom
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// vtable slot 0x2c -- the page's catch-all, byte-for-byte ApplSetupWnd::OnUnhandledMessageMaybe
// and ICF-folded onto it at 0x40b4c0, where the marker lives (src/ApplSetupWnd.cpp). Intercepts
// the screen-saver system command to re-run the app's own window-visibility pass, then returns
// DefWindowProcA's result unconditionally. UNMARKED -- one address, one marker.
LRESULT AlbumCardWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_SCREENSAVE) {
        FUN_00463670_LotsOfShowWindow();
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x80 (WM_CLOSE) -- byte-for-byte EditCardWnd::OnClose and ICF-folded onto it at
// 0x419a10, where the marker lives (src/EditCardWnd.cpp); src/EditCardWnd.h's declaration already
// records the fold from the other side. While the app is alive and not already tearing down, the
// album answers a close by ending the session and bouncing the front end to screen state 3,
// swallowing the close itself. UNMARKED -- one address, one marker.
LRESULT AlbumCardWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        this->EndActiveSession();
        AppWindow_SetScreenState(3);
        return 0;
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x405680 (Ghidra: AlbumCardWnd::OnMouseMove, WindowBase vtable+0x50
// override -- the class-wide WM_MOUSEMOVE convention slot (corrected from an earlier
// WM_SETCURSOR guess, see WindowBase::RouteMessageMaybe's ground-truth dispatch table); see
// src/AlbumCardWnd.h for the full behavioral summary.)
LRESULT AlbumCardWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int x = lParam & 0xffff;
    unsigned int y = (unsigned int)lParam >> 0x10;

    if (bInputBlocked != false) {
        return 0;
    }

    if (pPendingCard == NULL) {
        POINT pt;
        pt.y = y;
        pt.x = x;

        if (PtInRect(&pExitBtnIcon->rect, pt) ||
            PtInRect(&pEditBtnIcon->rect, pt) ||
            PtInRect(&pDeleteBtnIcon->rect, pt) ||
            PtInRect(&pMailBtnIcon->rect, pt) ||
            PtInRect(&pShowNamesBtnIcon->rect, pt) ||
            PtInRect(&pBackArrowIcon->rect, pt) ||
            PtInRect(&pForwardArrowIcon->rect, pt) ||
            PtInRect(&pPageIndicatorIcon->rect, pt)) {
            this->RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        } else {
            this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x402590 (Ghidra: AlbumCardWnd::BeginModalCapture, WindowBase vtable+8
// override -- see src/AlbumCardWnd.h for the full behavioral summary.)
void AlbumCardWnd::BeginModalCapture()
{
    AcquireButtonIconResources();
    RefreshClientClipRect();
    WindowBase::BeginModalCapture();
    ShowWindow(hwndSelf, SW_MAXIMIZE);
    SetFocus(hwndSelf);
    if (pPendingCard != NULL) {
        delete pPendingCard;
        pPendingCard = NULL;
    }
    this->RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    bInputBlocked = false;
    PostBagFileCache *pCache = g_pPostBagFileCache;
    int nPageStart = pCache->Unk0x10;
    int nSavedBucket = pCache->Unk0x14;
    nPageStart = nPageStart - (int)((unsigned int)nPageStart % (unsigned int)nVisibleCount);
    if (nPageStart >= 0 && nSavedBucket >= 0) {
        if (nStartIndex != nPageStart || (int)nCurrentBucket != nSavedBucket) {
            nCurrentBucket = (unsigned int)nSavedBucket;
            nBucket = nSavedBucket;
            nStartIndex = 0;
            RedrawAllSlots();
            nStartIndex = nPageStart;
        }
        pCache = g_pPostBagFileCache;
        pCache->Unk0x10 = -1;
        pCache->Unk0x14 = -1;
    }
}

// FUNCTION: LOCO 0x402660 (Ghidra: AlbumCardWnd::EndActiveSession, WindowBase vtable+4
// override -- see src/AlbumCardWnd.h for the full behavioral summary.)
void AlbumCardWnd::EndActiveSession()
{
    if (bModalCaptureActive != 0) {
        WindowBase::EndActiveSession();
        bHaveBackBuffer = 0;
        ReleaseButtonIconResources();
    }
}

// FUNCTION: LOCO 0x404830 (Ghidra: AlbumCardWnd::ReleaseButtonIconResources -- see
// src/AlbumCardWnd.h for the full behavioral summary.)
void AlbumCardWnd::ReleaseButtonIconResources()
{
    if (bWantEraseBlit != 0) {
        pCardCursorDesc->ReleaseRef();
        pCardCursorDesc = NULL;
        pExitBtnIcon->ReleaseRealized();
        pEditBtnIcon->ReleaseRealized();
        pDeleteBtnIcon->ReleaseRealized();
        pMailBtnIcon->ReleaseRealized();
        pShowNamesBtnIcon->ReleaseRealized();
        pBackArrowIcon->ReleaseRealized();
        pForwardArrowIcon->ReleaseRealized();
        pPageIndicatorIcon->ReleaseRealized();
        for (int i = 6; i < 12; i++) {
            paCardGrid[i]->ReleaseRealized();
        }
        bWantEraseBlit = 0;
    }
}

// FUNCTION: LOCO 0x404720 (Ghidra: AlbumCardWnd::EnsureBackgroundTileLoaded -- see
// src/AlbumCardWnd.h for the full behavioral summary.)
// EXACT match. Two levers needed: (1) nLayoutMode is a real 4-byte int/BOOL field, not the
// bool+3-pad-bytes an earlier session had modeled (every reader/writer in this class's own
// address range reads/writes the full DWORD at +0x134 -- see AlbumCardWnd.h's field comment);
// (2) the kind-id selection must be written as a duplicated if/else call (not a ternary-local)
// per CLAUDE.md's wsprintfA-format-string lesson extended to this shape -- a ternary-local
// compiles to the branchLESS setne+add form (the two kind ids differ by exactly 1), while the
// original genuinely branches; the duplicated-call form reproduces the real branch. Branch
// polarity also mattered: `if (!nLayoutMode) {0x3c0a} else {0x3c0b}` (false case first) matches
// the original's fall-through; the inverted order compiled with the jcc flipped.
void AlbumCardWnd::EnsureBackgroundTileLoaded()
{
    if (!bBackgroundTileLoaded) {
        if (!nLayoutMode) {
            pBackgroundTileDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3c0a);
        } else {
            pBackgroundTileDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3c0b);
        }
        pBackBuffer = pBackgroundTileDesc->GetOrLoadFrameBitmap(0, 0);
        bBackgroundTileLoaded = true;
    }
}

// FUNCTION: LOCO 0x404770 (Ghidra: AlbumCardWnd::AcquireButtonIconResources -- see
// src/AlbumCardWnd.h for the full behavioral summary.)
// EFFECTIVE MATCH (DIFF 12/180): the sole residual is where the compiler schedules the
// `pCardCursorDesc = ...` field store relative to the following virtual call's vtable-
// load/arg-push sequence -- the same intrinsic /O2 scheduling tie-break already documented on
// EditCardWnd::BuildEditUiResources's identical TileKind_GetOrLoadDescriptor +
// GetOrLoadFrameBitmap(0,0) pair (both the 2-statement form here and a chained
// `x = (field = call())->Method()` expression compiled identically there); not source-steerable.
void AlbumCardWnd::AcquireButtonIconResources()
{
    if (bWantEraseBlit == 0) {
        pExitBtnIcon->Load();
        pEditBtnIcon->Load();
        pDeleteBtnIcon->Load();
        pMailBtnIcon->Load();
        pShowNamesBtnIcon->Load();
        pBackArrowIcon->Load();
        pForwardArrowIcon->Load();
        pPageIndicatorIcon->Load();
        for (int i = 6; i < 12; i++) {
            paCardGrid[i]->Load();
        }
        pCardCursorDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cfa);
        pCardCursorRect = pCardCursorDesc->GetOrLoadFrameBitmap(0, 0);
        bWantEraseBlit = 1;
    }
}

// FUNCTION: LOCO 0x401fb0 (??_GAlbumCardWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x402380 // TODO: sync (Ghidra: AlbumCardWnd_DtorMaybe -- real C++ dtor syntax
// needed here for the base-class chain call, see WindowBase.h; same naming gap as EditCardWnd's
// own dtor)
// Tears down every owned resource: conditionally releases the button-icon/card-cursor group
// (ReleaseButtonIconResources, only if bWantEraseBlit is set), releases
// pBackgroundTileDesc (CursorDesc::ReleaseRef, vtable slot 2), deletes each of
// the 7 button-icon ResourceRef fields, deletes all 18 paCardGrid entries (in the original's
// own physical order: [i], [i+12], [i+6] for i in [0,6) -- an induction-pointer anchor artifact,
// not semantically meaningful), deletes all 9 paCategoryTabs entries, deletes
// pPageIndicatorIcon, deletes pPendingCard, then chains into WindowBase's own dtor.
//
// The compiler's own auto-generated scalar deleting destructor (`??_GAlbumCardWnd`, Ghidra:
// FUN_00401fb0) is a free byproduct of the `virtual ~AlbumCardWnd()` declaration (see
// AlbumCardWnd.h) -- not independently transcribed, same precedent as EditCardWnd.cpp's own
// `??_GEditCardWnd` marker.
AlbumCardWnd::~AlbumCardWnd()
{
    if (bWantEraseBlit) {
        ReleaseButtonIconResources();
    }
    if (pBackgroundTileDesc) {
        pBackgroundTileDesc->ReleaseRef();
        pBackgroundTileDesc = NULL;
    }
    if (pExitBtnIcon) {
        delete pExitBtnIcon;
        pExitBtnIcon = NULL;
    }
    if (pEditBtnIcon) {
        delete pEditBtnIcon;
        pEditBtnIcon = NULL;
    }
    if (pDeleteBtnIcon) {
        delete pDeleteBtnIcon;
        pDeleteBtnIcon = NULL;
    }
    if (pMailBtnIcon) {
        delete pMailBtnIcon;
        pMailBtnIcon = NULL;
    }
    if (pShowNamesBtnIcon) {
        delete pShowNamesBtnIcon;
        pShowNamesBtnIcon = NULL;
    }
    if (pBackArrowIcon) {
        delete pBackArrowIcon;
        pBackArrowIcon = NULL;
    }
    if (pForwardArrowIcon) {
        delete pForwardArrowIcon;
        pForwardArrowIcon = NULL;
    }
    for (int i = 0; i < 6; i++) {
        if (paCardGrid[i]) {
            delete paCardGrid[i];
            paCardGrid[i] = NULL;
        }
        if (paCardGrid[i + 12]) {
            delete paCardGrid[i + 12];
            paCardGrid[i + 12] = NULL;
        }
        if (paCardGrid[i + 6]) {
            delete paCardGrid[i + 6];
            paCardGrid[i + 6] = NULL;
        }
    }
    for (int i2 = 0; i2 < 9; i2++) {
        if (paCategoryTabs[i2]) {
            delete paCategoryTabs[i2];
            paCategoryTabs[i2] = NULL;
        }
    }
    if (pPageIndicatorIcon) {
        delete pPageIndicatorIcon;
        pPageIndicatorIcon = NULL;
    }
    if (pPendingCard) {
        delete pPendingCard;
        pPendingCard = NULL;
    }
}
