// MapWnd -- the multiplayer layout/town selection screen. See src/MapWnd.h for the class layout
// and docs/subsystems.md's "MapWnd" entry for the screen's behavior.
//
// The TU's real extent is 0x430a90..0x4326f0 (MailWnd's TU ends where this one begins;
// DecorActor7Maybe picks up at 0x4326f0). All 21 functions in that range are members of this class.

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ddraw.h>

#include "MapWnd.h"
#include "AppWindow.h"        // g_pApp (0x4aa4a0)
#include "CursorDesc.h"
#include "DPlaySessionMgr.h"  // g_pDPlaySessionMgr (0x4fd3ac)
#include "GameNetMsgQueue.h"  // g_nScreenState (the app screen-state selector)
#include "LocoBitmap.h"
#include "ResourceRef.h"
#include "UIResources.h"      // g_UIResources (0x4855e8), PlaySoundAtScreenPos
#include "WorldBoardMaybe.h"  // g_worldBoard (0x4aad08)

MapWnd *g_pMapWnd; // DAT_004fd388

// 0x408130 -- the app-wide UI-mode switch. Declared file-locally the same way
// src/AlbumCardWnd.cpp and src/MailWnd.cpp do; hoisting it into src/AppWindow.h is a separately
// measured shared-header change.
void AppWindow_SetScreenState(int newState); // TODO: idiom

// 0x463670 -- the app's own "re-show every window" routine, declared file-locally exactly as
// src/Main.cpp and src/MailWnd.cpp do.
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void); // TODO: idiom

// The `unsigned char` return type is LOAD-BEARING -- it is what reproduces OnClose's
// sete-materialized branch; see docs/CODEGEN.md's byte-predicate lever (v356). Kept TU-local for
// the same reason src/MailWnd.cpp and src/GameNet.cpp keep their own copies.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// 0x425a50 -- centres `rect` inside `outer`, rewriting `rect` in place. Declared file-locally the
// same way src/WidgetPicker.cpp and src/MailWnd.cpp do.
void CenterRectInRect(RECT *outer, RECT *rect); // TODO: idiom

// 0x425ac0 -- rescales the point (*px, *py) out of pSrcRect's coordinate space into pDstRect's,
// rewriting both in place (the *1000 fixed-point ratio is the original's own).
void MapPointBetweenRects(int *px, int *py, RECT *pSrcRect, RECT *pDstRect); // TODO: idiom

// 0x440310 / 0x440390 -- tell the other peers our own provider slot just became (un)available.
// Defined in src/DPlaySessionMgr.cpp; declared file-locally rather than in the shared header for
// the same measured-rotation reason as the stubs above.
void __fastcall GameNet_BroadcastSlotEnabled(DPlaySessionMgr *pMgr);  // TODO: idiom
void __fastcall GameNet_BroadcastSlotDisabled(DPlaySessionMgr *pMgr); // TODO: idiom

// The shared DDraw back/work surface every blit on this screen targets, and the label/name
// font, both declared file-locally exactly as src/AlbumCardWnd.cpp and src/EditCardWnd.cpp do.
extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

// FUNCTION: LOCO 0x430a90
MapWnd::MapWnd(void *hInstanceParam, unsigned int resourceIdParam)
    : WindowBase(hInstanceParam, resourceIdParam) {
    InitResourceRefs();
}

// FUNCTION: LOCO 0x430b10
void MapWnd::InitResourceRefs() {
    hIcon = NULL;
    bResourcesLoaded = 0;
    nTimerId = 0;
    nHoverSlotMaybe = -1;
    Unk0x278 = 0;
    pBackgroundTileDesc = NULL;
    pBackgroundBitmap = NULL;

    pSlotPlateRef = new ResourceRef(0x3d89);
    pRefreshBtnRef = new ResourceRef(0x3d8b);
    for (int i = 0; i < 9; i++) {
        paGridCells[i] = new ResourceRef(i + 0x3da4);
    }
}

// FUNCTION: LOCO 0x430c20
void MapWnd::EnsureBackgroundTileLoaded() {
    if (pBackgroundTileDesc == NULL) {
        pBackgroundTileDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3d8a);
        pBackgroundBitmap = pBackgroundTileDesc->GetOrLoadFrameBitmap(0, 0);
    }
}

// FUNCTION: LOCO 0x430af0 (??_GMapWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x430c60
MapWnd::~MapWnd() {
    if (pBackgroundTileDesc != NULL) {
        pBackgroundTileDesc->ReleaseRef();
        pBackgroundBitmap = NULL;
    }
    if (bResourcesLoaded) {
        pExitIconDesc->ReleaseRef();
        pExitIconBitmap = NULL;
        pTitleDesc->ReleaseRef();
        pTitleBitmap = NULL;
        pSlotPlateRef->ReleaseRealized();
        pRefreshBtnRef->ReleaseRealized();
        for (int i = 0; i < 9; i++) {
            paGridCells[i]->ReleaseRealized();
        }
        bResourcesLoaded = 0;
    }
    delete pSlotPlateRef;
    pSlotPlateRef = NULL;
    delete pRefreshBtnRef;
    pRefreshBtnRef = NULL;
    for (int j = 0; j < 9; j++) {
        delete paGridCells[j];
        paGridCells[j] = NULL;
    }
}

// FUNCTION: LOCO 0x430d70
void MapWnd::BeginModalCapture() {
    if (g_pDPlaySessionMgr->connectionMode != 2) {
        AppWindow_SetScreenState(3);
        return;
    }
    AcquireResources();
    RefreshClientClipRect();
    bGridDrawnMaybe = 0;
    WindowBase::BeginModalCapture();
    GameNet_BroadcastSlotEnabled(g_pDPlaySessionMgr);
    ShowWindow(hwndSelf, SW_SHOWMAXIMIZED);
    while (ShowCursor(FALSE) >= 0) {
    }
    SetFocus(hwndSelf);
    nTimerId = SetTimer(hwndSelf, 0x4d, 120, NULL);
}

// FUNCTION: LOCO 0x430e00
void MapWnd::EndActiveSession() {
    bGridDrawnMaybe = 0;
    WindowBase::EndActiveSession();
    if (g_pDPlaySessionMgr->connectionMode == 2) {
        GameNet_BroadcastSlotDisabled(g_pDPlaySessionMgr);
    }
    SetFocus(g_pApp->hwndOwner);
    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
    KillTimer(hwndSelf, nTimerId);
    nTimerId = 0;
    if (bResourcesLoaded) {
        pExitIconDesc->ReleaseRef();
        pExitIconBitmap = NULL;
        pTitleDesc->ReleaseRef();
        pTitleBitmap = NULL;
        pSlotPlateRef->ReleaseRealized();
        pRefreshBtnRef->ReleaseRealized();
        for (int i = 0; i < 9; i++) {
            paGridCells[i]->ReleaseRealized();
        }
        bResourcesLoaded = 0;
    }
}

// FUNCTION: LOCO 0x430ef0
LRESULT MapWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
        LocoBitmap *pIcon = pExitIconBitmap;
        RECT srcRect;
        // EFFECTIVE MATCH (27 bytes, 72/72 insns). srcRect is ADDRESS-TAKEN (OffsetRect), so it
        // has a real stack home and its stores follow SOURCE order -- which makes the original's
        // own store order readable straight off the disasm. Resolving every store against the
        // rect's base (E+8, pinned twice: `lea eax,[esp+0x10]` at 0x430f42 and the four reloads
        // at 0x430f61..0x430f7f) gives 0x430f3d->+8 right, 0x430f47->+0 left, 0x430f4f->+4 top,
        // 0x430f57->+0xc bottom. So the ORIGINAL's source order is right, left, top, bottom --
        // NOT the order below. It is written the other way anyway because every faithful
        // encoding of the true order measures WORSE overall (see docs/PARKED.md): the original
        // hoists BOTH 16-bit loads to the top (0x430f33/0x430f39, killing the descriptor pointer
        // early) while still storing bottom last, and cl will only hoist the height load if the
        // value is routed through a named local -- which buys a byte-exact rect block but costs
        // a one-step eax->ecx->edx rotation across both by-value RECT argument copies
        // (DIFF 40 / 31720 vs. 27 / 16023). Same rotation class as OnMouseMove's residual.
        srcRect.right = pExitIconDesc->nativeWidth;
        srcRect.bottom = pExitIconDesc->nativeHeight;
        srcRect.left = 0;
        srcRect.top = 0;
        OffsetRect(&srcRect, srcRect.right, 0);
        pIcon->RestoreOverlapBlt(rectExitButton, g_pDDrawWorkSurface, srcRect, 0);
        Sleep(150);
        AppWindow_SetScreenState(3);
        return 0;
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x430fe0
void MapWnd::RefreshClientClipRect() {
    WindowBase::RefreshClientClipRect();
    if (bResourcesLoaded) {
        // Centre the client clip bounds inside the background tile's own native extent -- the
        // result is the SOURCE rect every background blit reads from.
        RECT rectTmp = rectClipBounds;
        rectBackgroundSrc.left = 0;
        rectBackgroundSrc.right = pBackgroundTileDesc->nativeWidth;
        rectBackgroundSrc.top = 0;
        rectBackgroundSrc.bottom = pBackgroundTileDesc->nativeHeight;
        CenterRectInRect(&rectBackgroundSrc, &rectTmp);
        rectBackgroundSrc = rectTmp;

        // Everything below hangs off the 800x600 design rect, centred in the client area. Each
        // rect is derived from the PREVIOUS one rather than re-anchored on rectLayoutBase -- the
        // original's own `add eax,0x242` / `lea edx,[ecx+0x1b2]` chain proves it (cl does not CSE
        // rectLayoutBase.left + 600 back into rectGrid.left + 578).
        rectLayoutBase.left = 0;
        rectLayoutBase.top = 0;
        rectLayoutBase.right = 800;
        rectLayoutBase.bottom = 600;
        CenterRectInRect(&rectClipBounds, &rectLayoutBase);

        rectGrid.left = rectLayoutBase.left + 22;
        rectGrid.right = rectGrid.left + 578;
        rectGrid.top = rectLayoutBase.top + 35;
        rectGrid.bottom = rectGrid.top + 434;

        rectTitleBanner.left = rectGrid.right + 59;
        rectTitleBanner.right = rectTitleBanner.left + pTitleDesc->nativeWidth;
        rectTitleBanner.top = rectGrid.top + 10;
        rectTitleBanner.bottom = rectTitleBanner.top + pTitleDesc->nativeHeight;

        arectSlots[0].top = rectTitleBanner.bottom + 11;
        arectSlots[0].left = rectGrid.right + 14;
        arectSlots[0].bottom = arectSlots[0].top + pSlotPlateRef->pCursorDesc->nativeHeight;
        arectSlots[0].right = arectSlots[0].left + pSlotPlateRef->pCursorDesc->nativeWidth;

        arectSlots[1].top = arectSlots[0].bottom + 30;
        arectSlots[1].left = arectSlots[0].left;
        arectSlots[1].right = arectSlots[0].right;
        arectSlots[1].bottom = arectSlots[1].top + pSlotPlateRef->pCursorDesc->nativeHeight;

        for (int i = 2; i < 9; i++) {
            arectSlots[i].top = arectSlots[i - 1].bottom;
            arectSlots[i].bottom = arectSlots[i - 1].bottom + pSlotPlateRef->pCursorDesc->nativeHeight;
            arectSlots[i].left = arectSlots[0].left;
            arectSlots[i].right = arectSlots[0].right;
        }

        rectSignHotspotMaybe.left = rectGrid.right + 100;
        rectSignHotspotMaybe.right = rectSignHotspotMaybe.left + 100;
        rectSignHotspotMaybe.bottom = rectGrid.bottom;
        rectSignHotspotMaybe.top = rectSignHotspotMaybe.bottom - 48;

        RECT rectBtn;
        rectBtn.left = rectGrid.right + 20;
        rectBtn.right = rectBtn.left + pRefreshBtnRef->pCursorDesc->nativeWidth;
        rectBtn.bottom = rectGrid.bottom - 2;
        rectBtn.top = rectBtn.bottom - pRefreshBtnRef->pCursorDesc->nativeHeight;
        pRefreshBtnRef->rect = rectBtn;

        rectExitButton.top = rectGrid.bottom + 18;
        rectExitButton.bottom = rectExitButton.top + pExitIconDesc->nativeHeight;
        rectExitButton.left = rectGrid.right - 12;
        rectExitButton.right = rectExitButton.left + pExitIconDesc->nativeWidth;
    }
}

// FUNCTION: LOCO 0x431270
void MapWnd::AcquireResources() {
    if (!bResourcesLoaded) {
        pExitIconDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3d87);
        pExitIconBitmap = pExitIconDesc->GetOrLoadFrameBitmap(0, 0);
        pTitleDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3d88);
        pTitleBitmap = pTitleDesc->GetOrLoadFrameBitmap(0, 0);
        pSlotPlateRef->Load();
        pRefreshBtnRef->Load();
        for (int i = 0; i < 9; i++) {
            paGridCells[i]->Load();
        }
        bResourcesLoaded = 1;
    }
}

// FUNCTION: LOCO 0x431310
void MapWnd::OnActivate(int reservedMaybe) {
    pBackgroundBitmap->RestoreOverlapBlt(rectClipBounds, g_pDDrawWorkSurface, rectBackgroundSrc, 1);
    if (g_pDPlaySessionMgr->connectionMode == 2) {
        RedrawGridMaybe(0);
    }

    RECT srcRect;
    srcRect.left = 0;
    srcRect.top = 0;
    srcRect.right = pTitleDesc->nativeWidth;
    srcRect.bottom = pTitleDesc->nativeHeight;
    OffsetRect(&srcRect,
               ((g_pDPlaySessionMgr->selectedProviderIndex / g_pDPlaySessionMgr->nProviderSlotsPerRow) * 3 +
                g_pDPlaySessionMgr->selectedProviderIndex % g_pDPlaySessionMgr->nProviderSlotsPerRow) *
                   pTitleDesc->nativeWidth,
               0);
    pTitleBitmap->RestoreOverlapBlt(rectTitleBanner, g_pDDrawWorkSurface, srcRect, 0);

    RECT srcIconRect;
    srcIconRect.right = pExitIconDesc->nativeWidth;
    srcIconRect.bottom = pExitIconDesc->nativeHeight;
    srcIconRect.left = 0;
    srcIconRect.top = 0;
    pExitIconBitmap->RestoreOverlapBlt(rectExitButton, g_pDDrawWorkSurface, srcIconRect, 0);

    pRefreshBtnRef->DrawFrame(0, NULL);

    DrawSlotPlate(arectSlots[0], g_pDPlaySessionMgr->selectedProviderIndex, 0);
    int i = 0;
    RECT *pRect = &arectSlots[1];
    int nLocalSlot = g_pDPlaySessionMgr->selectedProviderIndex;
    for (; i < 9; i++) {
        if (nLocalSlot != i) {
            DrawSlotPlate(*pRect, i, 0);
            pRect++;
        }
    }
    if (!bGridDrawnMaybe) {
        bGridDrawnMaybe = 1;
    }
    CommitScreenUpdate(hwndSelf, NULL, 0, NULL);
}

// FUNCTION: LOCO 0x431560
void MapWnd::DrawSlotPlate(RECT rect, int nSlotIndex, int reserved) {
    DPlaySessionMgrProviderSlot *pSlot = g_pDPlaySessionMgr->ProviderSlotAt(nSlotIndex);

    pSlotPlateRef->rect = rect;
    if (nSlotIndex >= g_pDPlaySessionMgr->field_0x8) {
        pSlotPlateRef->DrawFrame(2, NULL);
        return;
    }
    if (nSlotIndex == nHoverSlotMaybe) {
        pSlotPlateRef->DrawFrame(1, NULL);
    } else {
        pSlotPlateRef->DrawFrame(0, NULL);
    }

    HDC hdc = AcquireWorkSurfaceDC(hwndSelf);
    COLORREF oldColor = SetTextColor(hdc, RGB(0, 0, 0));
    int oldMode = SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, g_UIResources.m_hFont14);
    const char *pszName = pSlot->sAddressOrName;
    int nLen = strlen(pszName);
    if (nLen > 0) {
        RECT rectText = rect;
        InflateRect(&rectText, -0xc, -5);
        DrawTextA(hdc, pszName, nLen, &rectText,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    SelectObject(hdc, oldFont);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldMode);
    DrawOwnerDot(hdc, rect.right - 0x10, rect.top + (rect.bottom - rect.top) / 2 - 2, nSlotIndex);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
}

// FUNCTION: LOCO 0x4316f0
void MapWnd::RedrawGridMaybe(int nUnusedMaybe) {
    RECT rectCell = rectGrid;
    rectCell.bottom = rectCell.top + 0x90;
    int nCellRight = rectCell.left + 0xc0;
    rectCell.right = nCellRight;
    int nCellLeft = rectCell.left;

    // Erase the horizontal seams between rows straight out of the background tile.
    RECT rectSeam;
    RECT rectSeamSrc;
    CopyRect(&rectSeam, &rectGrid);
    rectSeam.left -= 10;
    rectSeam.right += 10;
    rectSeam.top -= 10;
    rectSeam.bottom = rectSeam.top + 20;
    CopyRect(&rectSeamSrc, &rectSeam);
    OffsetRect(&rectSeamSrc, rectBackgroundSrc.left, rectBackgroundSrc.top);
    for (int nSeamRow = 0; nSeamRow <= g_pDPlaySessionMgr->nProviderSlotRows; nSeamRow++) {
        pBackgroundBitmap->RestoreOverlapBlt(rectSeam, g_pDDrawWorkSurface, rectSeamSrc, 1);
        OffsetRect(&rectSeamSrc, 0, 0x90);
        OffsetRect(&rectSeam, 0, 0x90);
    }

    // ... and the vertical ones between columns.
    CopyRect(&rectSeam, &rectGrid);
    rectSeam.top -= 10;
    rectSeam.bottom += 10;
    rectSeam.left -= 10;
    rectSeam.right = rectSeam.left + 20;
    CopyRect(&rectSeamSrc, &rectSeam);
    OffsetRect(&rectSeamSrc, rectBackgroundSrc.left, rectBackgroundSrc.top);
    for (int nSeamCol = 0; nSeamCol <= g_pDPlaySessionMgr->nProviderSlotRows; nSeamCol++) {
        pBackgroundBitmap->RestoreOverlapBlt(rectSeam, g_pDDrawWorkSurface, rectSeamSrc, 1);
        OffsetRect(&rectSeamSrc, 0xc0, 0);
        OffsetRect(&rectSeam, 0xc0, 0);
    }

    int nSlot = 0;
    ResourceRef **ppRow = paGridCells;
    for (int nRow = 0; nRow < g_pDPlaySessionMgr->nProviderSlotRows; nRow++) {
        ResourceRef **ppCell = ppRow;
        for (int nCol = 0; nCol < g_pDPlaySessionMgr->nProviderSlotsPerRow; nCol++) {
            (*ppCell)->rect.left = rectCell.left;
            (*ppCell)->rect.top = rectCell.top;
            (*ppCell)->rect.right = rectCell.right;
            (*ppCell)->rect.bottom = rectCell.bottom;
            if (nSlot < g_pDPlaySessionMgr->field_0x8) {
                (*ppCell)->DrawFrame(g_pDPlaySessionMgr->aProviderSlots[nSlot].providerId == 0 ? 1 : 2,
                                     NULL);
                DrawPeerScreenshotMaybe(&rectCell, &nSlot);
                DrawPeerTrainDotsMaybe(&rectCell, &nSlot);
                nSlot++;
            }
            OffsetRect(&rectCell, 0xc1, 0);
            ppCell++;
        }
        OffsetRect(&rectCell, 0, 0x91);
        rectCell.right = nCellRight;
        rectCell.left = nCellLeft;
        ppRow += 3;
    }
}

// FUNCTION: LOCO 0x431a10
void MapWnd::DrawPeerScreenshotMaybe(RECT *pDestRect, int *pnSlotIndex) {
    DPlaySessionMgrProviderSlot *pSlot = g_pDPlaySessionMgr->ProviderSlotAt(*pnSlotIndex);
    if (pSlot != NULL && pSlot->pLayoutData != NULL) {
        LocoBitmap *pTmp = new LocoBitmap();
        pTmp->CreateAndFill(pSlot->wLayoutCols, pSlot->wLayoutRows, 0, 0, 0);
        // nLayoutDataSize is the blob's own runtime byte count (wLayoutCols * wLayoutRows, filled
        // in by the wire unpacker), not a hand-computed struct extent -- no sizeof can express it.
        memcpy(pTmp->pPixels, pSlot->pLayoutData, pSlot->nLayoutDataSize); // idiom-exempt: runtime blob length

        RECT srcRect;
        srcRect.left = 0;
        srcRect.right = pSlot->wLayoutCols;
        srcRect.top = 0;
        srcRect.bottom = pSlot->wLayoutRows;
        pTmp->RestoreOverlapBlt(*pDestRect, g_pDDrawWorkSurface, srcRect, 0x10);
        delete pTmp;
    }
}

// FUNCTION: LOCO 0x431b30
void MapWnd::DrawPeerTrainDotsMaybe(RECT *pCellRect, int *pnSlotIndex) {
    DPlaySessionMgrProviderSlot *pSlot = g_pDPlaySessionMgr->ProviderSlotAt(*pnSlotIndex);

    RECT rectBoard;
    rectBoard.left = 0;
    rectBoard.top = 0;
    rectBoard.right = pSlot->wCols;
    rectBoard.bottom = pSlot->wRows;

    HDC hdc = AcquireWorkSurfaceDC(hwndSelf);
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    int nUnitX = 1;
    int nUnitY = 1;
    MapPointBetweenRects(&nUnitX, &nUnitY, &rectBoard, pCellRect);
    int nDotW = nUnitX - pCellRect->left;
    int nDotH = nUnitY - pCellRect->top;

    for (GameNetRosterResultNode *pNode = pSlot->pResultsChainHead; pNode != NULL;
         pNode = pNode->pNext) {
        int x = pNode->posX;
        int y = pNode->posY;
        if (x > 0 || y > 0) {
            COLORREF color;
            switch (pNode->bOwnerA / g_pDPlaySessionMgr->nProviderSlotsPerRow * 3 +
                    pNode->bOwnerA % g_pDPlaySessionMgr->nProviderSlotsPerRow) {
            case 0:
                color = 0xff;
                break;
            case 1:
                color = 0x283fa;
                break;
            case 2:
                color = 0xfcfef;
                break;
            case 3:
                color = 0xc2249d;
                break;
            case 4:
                color = 0xf1500c;
                break;
            case 5:
                color = 0x8000;
                break;
            case 6:
                color = 0xff92fe;
                break;
            case 7:
                color = 0x575757;
                break;
            default:
                color = 0xd2d2d2;
                break;
            }
            HBRUSH hBrush = CreateSolidBrush(color);
            HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

            x = pNode->posX;
            y = pNode->posY;
            MapPointBetweenRects(&x, &y, &rectBoard, pCellRect);
            if (x >= pCellRect->right) {
                x = pCellRect->right - nDotW - 1;
            }
            if (x < pCellRect->left) {
                x = pCellRect->left;
            }
            if (y >= pCellRect->bottom) {
                y = pCellRect->bottom - nDotH - 1;
            }
            if (y < pCellRect->top) {
                y = pCellRect->top;
            }

            RECT rectDot;
            rectDot.right = x + nDotW;
            rectDot.bottom = y + nDotH;
            rectDot.left = x;
            rectDot.top = y;
            if (bBlinkPhaseMaybe && (unsigned)nHoverSlotMaybe == pNode->bOwnerA) {
                InflateRect(&rectDot, 5, 5);
            } else {
                InflateRect(&rectDot, 3, 3);
            }
            Ellipse(hdc, rectDot.left, rectDot.top, rectDot.right, rectDot.bottom);
            if (bBlinkPhaseMaybe && (unsigned)nHoverSlotMaybe == pNode->bOwnerA) {
                SetPixel(hdc, rectDot.left + 4, rectDot.top + 3, 0xffffff);
                SetPixel(hdc, rectDot.left + 3, rectDot.top + 4, 0xffffff);
                SetPixel(hdc, rectDot.left + 4, rectDot.top + 4, 0xffffff);
                SetPixel(hdc, rectDot.left + 5, rectDot.top + 4, 0xdcdcdc);
                SetPixel(hdc, rectDot.left + 4, rectDot.top + 5, 0xdcdcdc);
                SetPixel(hdc, rectDot.left + 5, rectDot.top + 5, 0xdcdcdc);
            } else {
                SetPixel(hdc, rectDot.left + 2, rectDot.top + 2, 0xffffff);
                SetPixel(hdc, rectDot.left + 3, rectDot.top + 3, 0xdcdcdc);
            }
            SelectObject(hdc, hOldBrush);
            DeleteObject(hBrush);
        }
    }
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    CommitScreenUpdate(hwndSelf, hdc, 1, NULL);
}

// FUNCTION: LOCO 0x431ed0
void MapWnd::DrawOwnerDot(HDC hdc, int x, int y, int nSlotIndex) {
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    COLORREF color;
    switch (nSlotIndex % g_pDPlaySessionMgr->nProviderSlotsPerRow +
            nSlotIndex / g_pDPlaySessionMgr->nProviderSlotsPerRow * 3) {
    case 0:
        color = 0xff;
        break;
    case 1:
        color = 0x283fa;
        break;
    case 2:
        color = 0xfcfef;
        break;
    case 3:
        color = 0xc2249d;
        break;
    case 4:
        color = 0xf1500c;
        break;
    case 5:
        color = 0x8000;
        break;
    case 6:
        color = 0xff92fe;
        break;
    case 7:
        color = 0x575757;
        break;
    default:
        color = 0xd2d2d2;
        break;
    }
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    RECT rectDot;
    rectDot.left = x;
    rectDot.right = x + 4;
    rectDot.top = y;
    rectDot.bottom = y + 4;
    if (bBlinkPhaseMaybe && nHoverSlotMaybe == nSlotIndex) {
        InflateRect(&rectDot, 7, 7);
    } else {
        InflateRect(&rectDot, 4, 4);
    }
    Ellipse(hdc, rectDot.left, rectDot.top, rectDot.right, rectDot.bottom);
    if (bBlinkPhaseMaybe && nHoverSlotMaybe == nSlotIndex) {
        SetPixel(hdc, rectDot.left + 5, rectDot.top + 4, 0xffffff);
        SetPixel(hdc, rectDot.left + 4, rectDot.top + 5, 0xffffff);
        SetPixel(hdc, rectDot.left + 5, rectDot.top + 5, 0xffffff);
        SetPixel(hdc, rectDot.left + 6, rectDot.top + 5, 0xdcdcdc);
        SetPixel(hdc, rectDot.left + 5, rectDot.top + 6, 0xdcdcdc);
        SetPixel(hdc, rectDot.left + 6, rectDot.top + 6, 0xdcdcdc);
    } else {
        SetPixel(hdc, rectDot.left + 3, rectDot.top + 3, 0xffffff);
        SetPixel(hdc, rectDot.left + 4, rectDot.top + 4, 0xdcdcdc);
    }
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

// FUNCTION: LOCO 0x432120
LRESULT MapWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        AppWindow_SetScreenState(3);
        return 0;
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x432170
LRESULT MapWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    POINT pt;
    pt.x = LOWORD(lParam);
    pt.y = HIWORD(lParam);

    if (PtInRect(&rectExitButton, pt)) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        RECT srcRect;
        srcRect.right = pExitIconDesc->nativeWidth;
        srcRect.bottom = pExitIconDesc->nativeHeight;
        srcRect.left = 0;
        srcRect.top = 0;
        OffsetRect(&srcRect, srcRect.right, 0);
        pExitIconBitmap->RestoreOverlapBlt(rectExitButton, g_pDDrawWorkSurface, srcRect, 0);
        Sleep(150);
        AppWindow_SetScreenState(3);
        return 0;
    }
    if (PtInRect(&rectTitleBanner, pt)) {
        g_UIResources.PlaySoundAtScreenPos(rand() / 0x1fff + 0x500f, pt.x, pt.y, 4);
        return 0;
    }
    if (PtInRect(&rectSignHotspotMaybe, pt)) {
        g_UIResources.PlaySoundAtScreenPos(rand() / 0x1fff + 0x50f3, pt.x, pt.y, 4);
        return 0;
    }
    if (PtInRect(&pRefreshBtnRef->rect, pt)) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        pRefreshBtnRef->DrawFrame(1, NULL);
        CommitRectUpdate(pRefreshBtnRef->rect);
        g_pDPlaySessionMgr->LayoutNet_RequestLayoutList();
        Sleep(150);
        pRefreshBtnRef->DrawFrame(0, NULL);
        CommitRectUpdate(pRefreshBtnRef->rect);
    }
    return 0;
}

// FUNCTION: LOCO 0x4323c0
LRESULT MapWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4323e0
LRESULT MapWnd::OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (wParam == 0x4d) {
        bBlinkPhaseMaybe = !bBlinkPhaseMaybe;
        if (g_pDPlaySessionMgr->connectionMode == 2) {
            RedrawGridMaybe(0);
        }
        DrawSlotPlate(arectSlots[0], g_pDPlaySessionMgr->selectedProviderIndex, 0);
        int i = 0;
        RECT *pRect = &arectSlots[1];
        int nLocalSlot = g_pDPlaySessionMgr->selectedProviderIndex;
        for (; i < 9; i++) {
            if (nLocalSlot != i) {
                DrawSlotPlate(*pRect, i, 0);
                pRect++;
            }
        }
        RECT rectUpdate;
        rectUpdate.right = arectSlots[0].right;
        rectUpdate.top = rectGrid.top;
        rectUpdate.left = rectGrid.left;
        rectUpdate.bottom = rectGrid.bottom;
        CommitRectUpdate(rectUpdate);
    }
    return WindowBase::OnTimerDefaultMaybe(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4324f0
LRESULT MapWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_SCREENSAVE) {
        AppWindow_SetScreenState(3);
        FUN_00463670_LotsOfShowWindow();
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x432540
LRESULT MapWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    POINT pt;
    pt.x = LOWORD(lParam);
    pt.y = HIWORD(lParam);

    if (PtInRect(&rectExitButton, pt) || PtInRect(&rectSignHotspotMaybe, pt) ||
        PtInRect(&rectTitleBanner, pt)) {
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        return 0;
    }

    // The "leaving a slot" reset is written out TWICE, once per exit path, rather than being
    // hoisted into a shared tail: cl tail-merges the two copies from the `PUSH pt.x` down, but
    // leaves each with its own guard -- and the two guards read pt.y from DIFFERENT places (EBP
    // on the not-in-grid path, the spill slot on the out-of-range path, EBP having been reused
    // for rectGrid.top by then). One shared block cannot produce that.
    //
    // The hover-CHANGED path likewise `return`s from inside its own `if` rather than falling
    // through to the shared tail: the original's `je` at 0x43262c jumps clear over an inlined
    // copy of the transition call straight to the shared tail at 0x4326a4, and only a
    // branch-private copy gives the scheduler somewhere to sink `nHoverSlotMaybe = nSlot` --
    // it lands at 0x432655, between `mov ecx,esi` and the `call`. Writing the call once after
    // the `if` instead merges both paths onto one tail and strands the store ahead of it.
    //
    // EFFECTIVE MATCH (14 bytes, align=0): all 156 instructions agree in order and operands;
    // the two LAST-emitted transition tails allocate the same three loads one register step
    // around the eax->ecx->edx cycle (orig +0x64->eax, +0x60->ecx, vtable->edx; mine ecx, edx,
    // eax), and the pt.y shuttle at 0x134 rides the same rotation (orig ecx, mine edx). The
    // FIRST tail -- the branch-private one above -- already allocates exactly like the original,
    // so this is a per-block allocator tie-break, not a source shape.
    if (PtInRect(&rectGrid, pt) && bGridDrawnMaybe) {
        int nCol = (pt.x - rectGrid.left) / ((rectGrid.right - rectGrid.left) / 3) + 1;
        int nRow = (pt.y - rectGrid.top) / ((rectGrid.bottom - rectGrid.top) / 3) + 1;
        if (nCol <= g_pDPlaySessionMgr->nProviderSlotsPerRow &&
            nRow <= g_pDPlaySessionMgr->nProviderSlotRows) {
            int nSlot = (nRow - 1) * g_pDPlaySessionMgr->nProviderSlotsPerRow + nCol - 1;
            if (nSlot != nHoverSlotMaybe) {
                g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
                nHoverSlotMaybe = nSlot;
                RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
                return 0;
            }
        } else if (nHoverSlotMaybe >= 0) {
            g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
            nHoverSlotMaybe = -1;
        }
    } else if (nHoverSlotMaybe >= 0) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
        nHoverSlotMaybe = -1;
    }
    RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    return 0;
}
