// MailWnd -- the post-box screen's input handling. See src/MailWnd.h for the class layout and
// docs/subsystems.md's "MailWnd" entry for the screen's behavior.
//
// The TU's real extent is 0x42d8a0..0x430a90 (MapWnd's ctor starts the next one). Every function
// in that range is now transcribed (v384); 29 of the 32 are byte-exact, the three residuals being
// OnButtonMouseUp, OnButtonMouseDown and OnSetFocus (see docs/PARKED.md).

#include <windows.h>
#include <string.h>

#include "MailWnd.h"
#include "AlbumCardWnd.h"
#include "AppWindow.h"        // g_pApp (0x4aa4a0)
#include "CarNetState.h"
#include "EditCardWnd.h"
#include "LocoBitmap.h"
#include "PostBag.h"
#include "ResourceRef.h"
#include "DPlaySessionMgr.h"  // g_pDPlaySessionMgr (0x4fd3ac)
#include "TutorialWnd.h"      // g_pTutorialWnd (0x4fd38c)
#include "WorldBoardMaybe.h"  // g_worldBoard (0x4aad08)
#include "UIResources.h"

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_ClampDesktopRect
#endif

// 0x408130 -- the app-wide UI-mode switch. Declared file-locally the same way
// src/AlbumCardWnd.cpp does; hoisting it into src/AppWindow.h is a separately-measured
// shared-header change.
void AppWindow_SetScreenState(int newState); // TODO: idiom

// 0x422ea0 -- the shared `{ return DefWindowProcA(...); }` free function that most of
// WindowBase's message-slot defaults resolve to. OnSetFocus below calls it DIRECTLY (a plain
// 4-argument __stdcall call with no `this` in ECX -- the give-away that it is a free function
// and not a base-qualified `WindowBase::OnSetFocus(...)`), so it needs a callable declaration.
// Kept file-local rather than hoisted into src/WindowBase.h because adding a declaration to
// that shared header rotates every TU that includes it (docs/CODEGEN.md, v340/v355/v356).
LRESULT CALLBACK DefWindowProcStub(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // TODO: idiom

// 0x463670 -- the app's own "re-show every window" routine, declared file-locally exactly as
// src/Main.cpp does.
unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void); // TODO: idiom

// Per-TU extern decls (kept in sync with their canonical homes).
extern int g_nScreenState;              // app-state dword, see src/GameNetMsgQueue.h
// The `unsigned char` return type is LOAD-BEARING -- it is what reproduces OnClose's
// sete-materialized branch; see docs/CODEGEN.md's byte-predicate lever (v356).
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// Inlined into all three of its call sites below (no out-of-line body survives in the binary);
// see the declaration in src/MailWnd.h for why it is modelled as a function rather than spelled
// out three times.
inline unsigned short MailWnd::GetViewedCategoryCardCountMaybe()
{
    unsigned short nCount = 0;
    if (bViewingOutboxMaybe) {
        PostBagCrdFileNode *pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(1, NULL);
        while (pNode != NULL) {
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            nCount++;
            ::operator delete(pTemp);
        }
        if (nCount > 0) {
            bButtonsEnabledMaybe = 1;
        } else {
            bButtonsEnabledMaybe = 0;
        }
        return nCount;
    }
    return g_pPostBagCache->PostBag_GetCategoryFileCountCached();
}

// FUNCTION: LOCO 0x42d8a0 (Ghidra: MailWnd::DiscardHeldCardMaybe)
// Files the held card into category 0 (the Album) and strips its attachment on the way: the
// .att file is deleted and wAttachmentId cleared, so the card survives but its enclosure does
// not. The open card's own .crd is removed from the viewed category first, then the selection
// is rebuilt exactly as the two move helpers do.
void MailWnd::DiscardHeldCardMaybe()
{
    if (pHeldCard != NULL) {
        char szAttPath[0x504];
        PostBagCrdFileNode *pNode;
        if (bViewingOutboxMaybe) {
            pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(1, NULL);
        } else {
            pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(2, NULL);
        }
        while (pNode != NULL) {
            CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
            if (pOpenCard != NULL && pCard->nPostSeqId == pOpenCard->nPostSeqId) {
                g_pPostBagCache->DeleteCardFileAndRefreshCount(pNode->szPath);
            }
            delete pCard;
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
        }
        if (pHeldCard->wAttachmentId != 0) {
            g_pPostBagCache->PostBag_BuildAttFilePath(pHeldCard->nPostSeqId, 5, szAttPath);
            DeleteFileA(szAttPath);
            pHeldCard->wAttachmentId = 0;
        }
        g_pPostBagCache->PostBag_SaveCardToCategory(pHeldCard, 0, NULL);
        delete pOpenCard;
        pOpenCard = NULL;
        SelectNextCardMaybe();
        GetViewedCategoryCardCountMaybe();
    }
}

// FUNCTION: LOCO 0x42da10 (Ghidra: MailWnd::MoveHeldCardToOutboxMaybe)
// Posts the held card: deletes the open card's .crd out of the inbox, then re-saves the held one
// into category 1 (outbox). Only meaningful while the inbox is the viewed category.
void MailWnd::MoveHeldCardToOutboxMaybe()
{
    if (pHeldCard != NULL && bViewingOutboxMaybe == 0) {
        PostBagCrdFileNode *pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(2, NULL);
        while (pNode != NULL) {
            CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
            if (pOpenCard != NULL && pCard->nPostSeqId == pOpenCard->nPostSeqId) {
                g_pPostBagCache->DeleteCardFileAndRefreshCount(pNode->szPath);
            }
            delete pCard;
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
        }
        g_pPostBagCache->PostBag_SaveCardToCategory(pHeldCard, 1, NULL);
        delete pOpenCard;
        pOpenCard = NULL;
        SelectNextCardMaybe();
        GetViewedCategoryCardCountMaybe();
    }
}

// FUNCTION: LOCO 0x42db30 (Ghidra: MailWnd::MoveHeldCardToInboxMaybe)
// Mirror of MoveHeldCardToOutboxMaybe: deletes the open card's .crd out of the outbox and
// re-saves the held one into category 2 (inbox). Only meaningful while the outbox is viewed --
// note the two guard clauses are tested in the opposite order to the outbox version's.
void MailWnd::MoveHeldCardToInboxMaybe()
{
    if (bViewingOutboxMaybe && pHeldCard != NULL) {
        PostBagCrdFileNode *pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(1, NULL);
        while (pNode != NULL) {
            CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
            if (pOpenCard != NULL && pCard->nPostSeqId == pOpenCard->nPostSeqId) {
                g_pPostBagCache->DeleteCardFileAndRefreshCount(pNode->szPath);
            }
            delete pCard;
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
        }
        g_pPostBagCache->PostBag_SaveCardToCategory(pHeldCard, 2, NULL);
        delete pOpenCard;
        pOpenCard = NULL;
        SelectNextCardMaybe();
        GetViewedCategoryCardCountMaybe();
    }
}

// FUNCTION: LOCO 0x42dc50 (Ghidra: MailWnd::DeleteOpenCardFileMaybe)
// Deletes the open card's .crd from whichever category is being viewed, dropping pOpenCard as
// soon as the matching file is found. Unlike the two move helpers this does NOT reselect -- the
// caller is expected to follow up.
void MailWnd::DeleteOpenCardFileMaybe()
{
    if (pOpenCard != NULL) {
        PostBagCrdFileNode *pNode;
        if (bViewingOutboxMaybe) {
            pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(1, NULL);
        } else {
            pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(2, NULL);
        }
        while (pNode != NULL) {
            CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
            if (pOpenCard != NULL && pCard->nPostSeqId == pOpenCard->nPostSeqId) {
                g_pPostBagCache->DeleteCardFileAndRefreshCount(pNode->szPath);
                delete pOpenCard;
                pOpenCard = NULL;
            }
            delete pCard;
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
        }
        GetViewedCategoryCardCountMaybe();
    }
}

// FUNCTION: LOCO 0x42dd50 (Ghidra: MailWnd::SelectNextCardMaybe)
// Repoints pOpenCard at the card AFTER the currently open one within the viewed category, and
// falls back to the category's first card when the open one is last (or was never set). The
// walk loads every .crd in the category just to compare nPostSeqId, deleting each probe as it
// goes -- so the "first" card is loaded up front and thrown away again unless it is needed.
void MailWnd::SelectNextCardMaybe()
{
    CarNetState *pFollowing = NULL;
    PostBagCrdFileNode *pNode;
    if (bViewingOutboxMaybe) {
        pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(1, NULL);
    } else {
        pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(2, NULL);
    }
    if (pNode != NULL) {
        CarNetState *pFirst = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
        if (pOpenCard == NULL) {
            pOpenCard = pFirst;
            do {
                PostBagCrdFileNode *pTemp = pNode;
                pNode = pNode->pNext;
                ::operator delete(pTemp);
            } while (pNode != NULL);
            return;
        }
        do {
            CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
            if (pFollowing == NULL && pCard->nPostSeqId == pOpenCard->nPostSeqId &&
                pNode->pNext != NULL) {
                pFollowing = g_pPostBagCache->CarNetState_CreateFromFile(pNode->pNext->szPath);
            }
            delete pCard;
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
        } while (pNode != NULL);
        if (pFollowing == NULL) {
            delete pOpenCard;
            pOpenCard = pFirst;
        } else {
            delete pFirst;
            delete pOpenCard;
            pOpenCard = pFollowing;
        }
    }
}

// FUNCTION: LOCO 0x42e4e0 (Ghidra: MailWnd::DrawFlagFrameMaybe)
// Blits the post-box flag. The sprite is a two-frame strip laid out horizontally, so the source
// rect is the flag rect's own size anchored at the origin, shifted right by one frame width when
// bFlagFrameMaybe selects the second frame.
void MailWnd::DrawFlagFrameMaybe()
{
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    RECT srcRect;
    if (bFlagFrameMaybe == 1) {
        SetRectEmpty(&srcRect);
        srcRect.right = rectFlagMaybe.right - rectFlagMaybe.left;
        srcRect.bottom = rectFlagMaybe.bottom - rectFlagMaybe.top;
        OffsetRect(&srcRect, srcRect.right, 0);
        pFlagBmpMaybe->RestoreOverlapBlt(rectFlagMaybe, g_pDDrawWorkSurface, srcRect, 1);
    } else {
        SetRectEmpty(&srcRect);
        srcRect.right = rectFlagMaybe.right - rectFlagMaybe.left;
        srcRect.bottom = rectFlagMaybe.bottom - rectFlagMaybe.top;
        pFlagBmpMaybe->RestoreOverlapBlt(rectFlagMaybe, g_pDDrawWorkSurface, srcRect, 1);
    }
}

// FUNCTION: LOCO 0x42e5e0 (Ghidra: MailWnd::RestoreCardBackdropMaybe)
// Blits the saved backdrop back over whatever the card layer had drawn on top of it. With
// bWithAttachment clear that is the whole screen (the backdrop's own rect restored across the
// window's clip bounds); with it set, just the two card-layer sub-rects -- the attachment/stamp
// area and rect0x654 -- each restored from the matching screen-offset slice of the backdrop.
//
// The single `call` the two paths appear to share in the original is cl's own tail merge of the
// second true-path call with the false-path one, not a shared call site in the source.
void MailWnd::RestoreCardBackdropMaybe(char bWithAttachment)
{
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    if (bInit0x5f9 && bInputEnabled) {
        if (bWithAttachment) {
            RECT rect;
            CopyRect(&rect, &rectAttachmentMaybe);
            OffsetRect(&rect, rectScreenMaybe.left, rectScreenMaybe.top);
            pCardBackdropBmp->RestoreOverlapBlt(rectAttachmentMaybe, g_pDDrawWorkSurface, rect, 1);
            CopyRect(&rect, &rect0x654);
            OffsetRect(&rect, rectScreenMaybe.left, rectScreenMaybe.top);
            pCardBackdropBmp->RestoreOverlapBlt(rect0x654, g_pDDrawWorkSurface, rect, 1);
        } else {
            pCardBackdropBmp->RestoreOverlapBlt(rectClipBounds, g_pDDrawWorkSurface,
                                                rectScreenMaybe, 1);
        }
    }
}

// FUNCTION: LOCO 0x42e760 (Ghidra: MailWnd::RedrawCardAreaMaybe)
// Full repaint of the card layer. With a card open: erase the attachment area first if this card
// has no attachment to draw there, hand the card itself to the shared thumbnail renderer, stamp
// the p0x650 overlay over rect0x654, and release the bin button. With no card open: restore the
// whole card-layer backdrop and release the inbox and bin buttons.
void MailWnd::RedrawCardAreaMaybe()
{
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    if (bInputEnabled) {
        if (pOpenCard != NULL) {
            RECT rect;
            if (pOpenCard->wAttachmentId == 0) {
                CopyRect(&rect, &rectAttachmentMaybe);
                OffsetRect(&rect, rectScreenMaybe.left, rectScreenMaybe.top);
                pCardBackdropBmp->RestoreOverlapBlt(rectAttachmentMaybe, g_pDDrawWorkSurface,
                                                    rect, 0);
            }
            g_pPostBagCache->DrawCardThumbnail(bCardSideMaybe, pOpenCard, g_pDDrawWorkSurface,
                                               rectCardMaybe, (unsigned int)hwndSelf, NULL);
            SetRectEmpty(&rect);
            rect.right = rect0x654.right - rect0x654.left;
            rect.bottom = rect0x654.bottom - rect0x654.top;
            p0x650->RestoreOverlapBlt(rect0x654, g_pDDrawWorkSurface, rect, 0);
            OnButtonMouseUp(6);
        } else {
            RestoreCardBackdropMaybe(1);
            OnButtonMouseUp(8);
            OnButtonMouseUp(6);
        }
    }
}

// PARTIAL MATCH (DIFF 152 / 724 B, insns 230/230 -- structure is settled, two codegen
// tie-breaks remain).
//   * x4: the badge-table read. The original spells the ushort->int widening as
//     `mov ax,[edi+eax*2+0x6c4]` + `and eax,0xffff` (reusing the INDEX register as the value
//     register); cl here picks `xor edx,edx` + `mov dx,[...]` into a fresh register. Invariant
//     under every source spelling probed: a direct argument expression, a named `unsigned short`
//     temp, a named `int` temp, and self-assignment (`nCount = aBadgeVariantsA[nCount];`) all
//     emit byte-identical code. Declaring ResourceRef::DrawFrame's first parameter
//     `unsigned short` DOES produce the original's form (152 -> 125) but is REFUTED by the
//     callee: 0x454c30 reads the slot with `mov esi,dword ptr [esp+0x20]` and then
//     `imul esi,eax`, a full signed 32-bit read, so the parameter really is `int`.
//   * The two remaining `push 0` schedulings fall out of the same choice.
// FUNCTION: LOCO 0x42de70 (Ghidra: MailWnd::OnButtonMouseUp)
// Repaints one icon button in its RELEASED state. `buttonId` is a HitTestButton command id
// (2..9), not a paButtons index: 2..7 map straight onto paButtons[0..5], 9 onto paButtons[7]
// (rect0x67c is that button's hot area) and 8 onto paButtons[6]. No click sound -- that is
// OnButtonMouseDown's job.
//
// sic: button 6 (paButtons[4], the bin) only gets its released sprite drawn when the live card
// count is nonzero. Deleting the last card therefore leaves the bin button visually stuck down
// until something else repaints it. See docs/engine-bugs.md.
void MailWnd::OnButtonMouseUp(int buttonId)
{
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    if (bInputEnabled) {
        switch (buttonId) {
        case 2:
            paButtons[0]->DrawFrame(0, NULL);
            break;
        case 3:
            paButtons[1]->DrawFrame(0, NULL);
            break;
        case 4:
            paButtons[2]->DrawFrame(0, NULL);
            break;
        case 5:
            paButtons[3]->DrawFrame(0, NULL);
            break;
        case 6: {
            RECT rect;
            CopyRect(&rect, &paButtons[4]->rect);
            OffsetRect(&rect, rectScreenMaybe.left, rectScreenMaybe.top);
            pCardBackdropBmp->RestoreOverlapBlt(paButtons[4]->rect, g_pDDrawWorkSurface, rect, 1);

            unsigned short nCount = GetViewedCategoryCardCountMaybe();
            if (nCount > 0) {
                paButtons[4]->DrawFrame(0, NULL);
            }
            break;
        }
        case 7: {
            unsigned short nCount = GetViewedCategoryCardCountMaybe();
            if (nCount <= 4) {
                nCount = aBadgeVariantsA[nCount];
                paButtons[5]->DrawFrame(nCount, NULL);
            } else {
                nCount = aBadgeVariantsA[4];
                paButtons[5]->DrawFrame(nCount, NULL);
            }
            break;
        }
        case 8: {
            unsigned short nCount = g_pPostBagCache->PostBag_GetCategoryFileCountCached();
            if (nCount <= 4) {
                nCount = aBadgeVariantsB[nCount];
                paButtons[6]->DrawFrame(nCount, NULL);
            } else {
                nCount = aBadgeVariantsB[4];
                paButtons[6]->DrawFrame(nCount, NULL);
            }
            break;
        }
        case 9:
            paButtons[7]->DrawFrame(0, NULL);
            break;
        }
    }
}

// PARTIAL MATCH (DIFF 468 / 716 B, insns 227/218 -- 9 instructions short).
// The gap is one construct: in case 6 the original does NOT elide the `srcRect` local. It
// reserves a 16-byte frame slot (`sub esp,0x10` in the prologue), fills srcRect there, and only
// then copies it dword-by-dword into the by-value argument block; cl here builds the argument
// block directly and never materializes the local. Probed without effect: declaring srcRect at
// function scope rather than inside the case, and dropping the `pButton` local. Whatever forces
// the original's local to be addressable is still unidentified -- note that OnButtonMouseUp's
// own case 6 gets that slot for free because CopyRect/OffsetRect take its address.
// FUNCTION: LOCO 0x42e150 (Ghidra: MailWnd::OnButtonMouseDown)
// Repaints one icon button in its PRESSED state -- the same id->button map as OnButtonMouseUp
// above, but drawing sprite frame 1 (or the badge variant + 1) instead of 0. Commands 2..7 also
// play the shared UI click sound; 8 and 9 deliberately do not.
void MailWnd::OnButtonMouseDown(int buttonId)
{
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    RECT srcRect;

    if (bInputEnabled) {
        switch (buttonId) {
        case 2:
            g_UIResources.PlayUiSound(0x5015);
            paButtons[0]->DrawFrame(1, NULL);
            break;
        case 3:
            g_UIResources.PlayUiSound(0x5015);
            paButtons[1]->DrawFrame(1, NULL);
            break;
        case 4:
            g_UIResources.PlayUiSound(0x5015);
            paButtons[2]->DrawFrame(1, NULL);
            break;
        case 5:
            g_UIResources.PlayUiSound(0x5015);
            paButtons[3]->DrawFrame(1, NULL);
            break;
        case 6: {
            g_UIResources.PlayUiSound(0x5015);
            ResourceRef *pButton = paButtons[4];
            srcRect.left = pButton->rect.left + rectScreenMaybe.left;
            srcRect.top = pButton->rect.top + rectScreenMaybe.top;
            srcRect.right = rectScreenMaybe.left + pButton->rect.right;
            srcRect.bottom = pButton->rect.bottom + rectScreenMaybe.top;
            pCardBackdropBmp->RestoreOverlapBlt(pButton->rect, g_pDDrawWorkSurface, srcRect, 1);
            paButtons[4]->DrawFrame(1, NULL);
            break;
        }
        case 7: {
            g_UIResources.PlayUiSound(0x5015);
            unsigned short nCount = GetViewedCategoryCardCountMaybe();
            if (nCount <= 4) {
                nCount = aBadgeVariantsA[nCount];
                paButtons[5]->DrawFrame(nCount + 1, NULL);
            } else {
                nCount = aBadgeVariantsA[4];
                paButtons[5]->DrawFrame(nCount + 1, NULL);
            }
            break;
        }
        case 8: {
            unsigned short nCount = g_pPostBagCache->PostBag_GetCategoryFileCountCached();
            if (nCount <= 4) {
                nCount = aBadgeVariantsB[nCount];
                paButtons[6]->DrawFrame(nCount, NULL);
            } else {
                nCount = aBadgeVariantsB[4];
                paButtons[6]->DrawFrame(nCount, NULL);
            }
            break;
        }
        case 9:
            paButtons[7]->DrawFrame(1, NULL);
            break;
        }
    }
}

// FUNCTION: LOCO 0x42e980 (Ghidra: MailWnd::InitResourceRefs)
// Second-stage construction, run separately from the ctor at 0x42e900. Clears every piece of
// runtime state, then allocates the eight icon buttons. Note [6] is constructed before [5] --
// the resource ids run 0x3cf0,0x3cf1,0x3cf2,0x3cf3,0x3cac then 0x3cf5 into slot 6, 0x3cf6 into
// slot 5, and 0x3cf9 into slot 7 -- and that the two badge tables are the count-indexed sprite
// frames OnButtonMouseUp/Down pick from.
void MailWnd::InitResourceRefs()
{
    hIcon = NULL;
    bAttachmentModalBusyMaybe = 0;
    bInit0x5f9 = 0;
    bInit0x5fa = 0;
    bInputEnabled = 0;
    pOpenCard = NULL;
    pHeldCard = NULL;
    bCardSideMaybe = 1;
    pBackdropDescMaybe = NULL;
    pCardBackdropBmp = NULL;
    nTimerId = 0;
    nFlagAnimTickMaybe = 0;
    bFlagFrameMaybe = 0;

    paButtons[0] = new ResourceRef(0x3cf0);
    paButtons[1] = new ResourceRef(0x3cf1);
    paButtons[2] = new ResourceRef(0x3cf2);
    paButtons[3] = new ResourceRef(0x3cf3);
    paButtons[4] = new ResourceRef(0x3cac);
    paButtons[6] = new ResourceRef(0x3cf5);
    paButtons[5] = new ResourceRef(0x3cf6);
    paButtons[7] = new ResourceRef(0x3cf9);

    aBadgeVariantsA[0] = 0;
    aBadgeVariantsA[1] = 1;
    aBadgeVariantsA[2] = 3;
    aBadgeVariantsA[3] = 5;
    aBadgeVariantsA[4] = 7;
    aBadgeVariantsB[0] = 0;
    aBadgeVariantsB[1] = 1;
    aBadgeVariantsB[2] = 2;
    aBadgeVariantsB[3] = 3;
    aBadgeVariantsB[4] = 4;

    bMailPendingMaybe = 0;
    nInputCooldownMaybe = 0;
    bTearingDownMaybe = 0;
}

// FUNCTION: LOCO 0x42f8b0 (Ghidra: MailWnd::RefreshClientClipRect)
// Vtable slot 0x1c. Re-lays out every rect on the mail screen after a resolution/client-size
// change. Two rects are anchored independently against the window's clip bounds: rectScreenMaybe
// (the backdrop art's own native size) and rectLayoutBaseMaybe (the fixed 800x600 design box).
// Everything else -- the eight icon buttons, the flag, the card thumbnail and the two standalone
// hot areas -- is a copy of the design box shrunk to its art's native size and then shoved into
// place by a hardcoded OffsetRect, so the whole tail is the same five-line block eight times over.
void MailWnd::RefreshClientClipRect() {
  extern void CenterRectInRect(RECT *outer, RECT *rect); // 0x425a50

  if (bInit0x5f9) {
    RECT *pRectClip;
    RECT *pRect;
    CursorDesc *pDesc;
    RECT rect;

    WindowBase::RefreshClientClipRect();
    pRectClip = &rectClipBounds;
    pRect = &rectScreenMaybe;
    rect = *pRectClip;
    pDesc = pBackdropDescMaybe;
    pRect->left = 0;
    rectScreenMaybe.right = pDesc->nativeWidth;
    rectScreenMaybe.top = 0;
    rectScreenMaybe.bottom = pDesc->nativeHeight;
    CenterRectInRect(pRect, &rect);
    *pRect = rect;

    pRect = &rectLayoutBaseMaybe;
    pRect->left = 0;
    rectLayoutBaseMaybe.top = 0;
    rectLayoutBaseMaybe.right = 800;
    rectLayoutBaseMaybe.bottom = 600;
    CenterRectInRect(pRectClip, pRect);

    CopyRect(&rect, pRect);
    rect.right = paButtons[0]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[0]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x2c, 0x22);
    paButtons[0]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[1]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[1]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x1a7, 0x196);
    paButtons[1]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[2]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[2]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x22, 0x106);
    paButtons[2]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[3]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[3]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x29e, 0x1c3);
    paButtons[3]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[4]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[4]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x2cb, 0x12e);
    paButtons[4]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[5]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[5]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0xa9, 0x15e);
    paButtons[5]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[6]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[6]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x21c, 0xb);
    paButtons[6]->rect = rect;

    CopyRect(&rect, pRect);
    rect.right = paButtons[7]->pCursorDesc->nativeWidth + rect.left;
    rect.bottom = paButtons[7]->pCursorDesc->nativeHeight + rect.top;
    OffsetRect(&rect, 0x16d, 0x1a0);
    paButtons[7]->rect = rect;

    SetRect(&rect0x67c, 0x14b, 0x1a3, 0x1ab, 0x1fb);
    OffsetRect(&rect0x67c, pRect->left, rectLayoutBaseMaybe.top);

    CopyRect(&rect0x654, pRect);
    pDesc = pDesc0x64c;
    rect0x654.right = pDesc->nativeWidth + rect0x654.left;
    rect0x654.bottom = rect0x654.top + pDesc->nativeHeight;
    OffsetRect(&rect0x654, 0x14c, 0xb0);

    CopyRect(&rectFlagMaybe, pRect);
    pDesc = pDesc0x664;
    rectFlagMaybe.right = pDesc->nativeWidth + rectFlagMaybe.left;
    rectFlagMaybe.bottom = rectFlagMaybe.top + pDesc->nativeHeight;
    OffsetRect(&rectFlagMaybe, 0x100, 0xd2);

    CopyRect(&rectCardMaybe, pRect);
    rectCardMaybe.right = rectCardMaybe.left + 300;
    rectCardMaybe.bottom = rectCardMaybe.top + 200;
    OffsetRect(&rectCardMaybe, 0x199, 0xb2);

    SetRect(&rectAttachmentMaybe, 0x14, -0xb, 0x2d, 0x3b);
    OffsetRect(&rectAttachmentMaybe, rectCardMaybe.left, rectCardMaybe.top);
  }
}

// FUNCTION: LOCO 0x430800 (Ghidra: MailWnd::OnMouseMove)
// Vtable slot 0x50. Purely a cursor-shape update: hovering a LIVE button (one whose command is
// currently actionable) arms the anipoint cursor, everything else leaves the cursor alone. The
// three buttons whose liveness depends on the card count (5, 6, 7) each recompute it, and the
// pick-up/put-down commands (0 and 9) only arm the cursor while nothing is held.
LRESULT MailWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LONG x = lParam & 0xffff;
    LONG y = (unsigned int)lParam >> 0x10;

    if (bTearingDownMaybe) {
        return 0;
    }
    if (bAttachmentModalBusyMaybe) {
        RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        return 0;
    }
    if (bInputEnabled == 0) {
        return 0;
    }

    unsigned short nCount;
    switch (HitTestButton(x, y)) {
    case 0:
        if (pHeldCard != NULL) {
            return 0;
        }
        RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
        return 0;
    case 2:
    case 3:
    case 4:
        if (pHeldCard == NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        }
        return 0;
    case 5:
        nCount = GetViewedCategoryCardCountMaybe();
        if (nCount > 0 && pHeldCard == NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        }
        return 0;
    case 6:
        nCount = GetViewedCategoryCardCountMaybe();
        if (nCount > 0 && pHeldCard == NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        }
        return 0;
    case 7:
        nCount = GetViewedCategoryCardCountMaybe();
        if (nCount > 0 && pHeldCard == NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        }
        return 0;
    case 9:
        if (pHeldCard != NULL) {
            return 0;
        }
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        return 0;
    case 1:
    case 8:
        return 0;
    }
    return 0;
}

// FUNCTION: LOCO 0x430090 (Ghidra: MailWnd::HitTestButton)
// Maps a client-space point to the command id OnLButtonDown dispatches on. The icon-button row
// is tested in paButtons order (each button's hot area is its ResourceRef's own rect), with the
// standalone rect0x67c hot area wedged between entries [5] and [6] -- which is why the ids run
// 2,3,4,5,6,7 then 9 then 8 rather than straight up. Returns 0 for a miss.
//
// The return type is unsigned because OnLButtonDown switches on it: a signed value would force
// cl to prove non-negativity before indexing the jump table.
unsigned int MailWnd::HitTestButton(LONG x, LONG y)
{
    // Member init order is load-bearing: `pt.x` first allocates x and y to the opposite pair of
    // callee-saved registers from the original's.
    POINT pt;
    pt.y = y;
    pt.x = x;

    if (PtInRect(&paButtons[0]->rect, pt)) {
        return 2;
    }
    if (PtInRect(&paButtons[1]->rect, pt)) {
        return 3;
    }
    if (PtInRect(&paButtons[2]->rect, pt)) {
        return 4;
    }
    if (PtInRect(&paButtons[3]->rect, pt)) {
        return 5;
    }
    if (PtInRect(&paButtons[4]->rect, pt)) {
        return 6;
    }
    if (PtInRect(&paButtons[5]->rect, pt)) {
        return 7;
    }
    if (PtInRect(&rect0x67c, pt)) {
        return 9;
    }
    return PtInRect(&paButtons[6]->rect, pt) ? 8 : 0;
}

// FUNCTION: LOCO 0x430190 (Ghidra: MailWnd::OnLButtonDown, WindowBase vtable+0x38 override --
// the WindowBase-wide WM_LBUTTONDOWN convention slot).
//
// Three hot areas are tested ahead of the icon-button row, because they belong to the card
// itself rather than to the chrome: the attachment/stamp area (live only while the open card
// actually carries an attachment), the card body (picking the card up onto the cursor), and the
// flag sprite (whose two-frame animation the click drives by hand). Only then does the
// icon-button row get hit-tested.
//
// Every command's held-card path is the same shape: put the cursor back the way it was (the
// inherited RequestModeTransitionFromSource on the anipoint pair), let the command consume the
// held card, then clear pHeldCard. Case body order below follows the jump table's own order at
// 0x430798, which here is plain ascending case order.
LRESULT MailWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;
    pt.x = lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 0x10;

    if (bInputEnabled == 0) {
        return 0;
    }
    if (bAttachmentModalBusyMaybe != 0) {
        return 0;
    }
    if (bTearingDownMaybe != 0) {
        return 0;
    }

    if (pOpenCard != NULL && pOpenCard->wAttachmentId != 0 &&
        PtInRect(&rectAttachmentMaybe, pt)) {
        g_UIResources.PlayUiSound(0x5464);
        if (bViewingOutboxMaybe == 0) {
            return 0;
        }
        OpenAttachmentMaybe();
        return 0;
    }

    if (pOpenCard != NULL && PtInRect(&rectCardMaybe, pt)) {
        g_UIResources.PlayUiSound(0x5015);
        pHeldCard = pOpenCard;
        RequestModeTransitionFromSource(pCardCursorRect, pCardCursorDesc, 0, 1);
        return 0;
    }

    if (PtInRect(&rectFlagMaybe, pt)) {
        if (nInputCooldownMaybe != 0) {
            return 0;
        }
        nInputCooldownMaybe = 8;
        g_UIResources.PlayUiSound(0x5114);
        nFlagAnimTickMaybe = 0x15;
        if (bFlagFrameMaybe == 1) {
            bFlagFrameMaybe = 0;
            nFlagAnimTickMaybe = 0;
            DrawFlagFrameMaybe();
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        } else {
            bFlagFrameMaybe = 1;
            DrawFlagFrameMaybe();
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        }
    }

    // The two dead low ids are spelled out rather than left to the default: without them the
    // jump table's base is the smallest live case (2) and cl 11.00 has to bias the index with an
    // `add eax,-2` the original does not have. Spelling `case 0:`/`case 1:` pins the table at 0,
    // which is exactly the original's own 10-entry table at 0x430798 (entries 0 and 1 pointing at
    // the same shared `return 0` the out-of-range path uses).
    switch (HitTestButton(pt.x, pt.y)) {
    case 0:
    case 1:
        return 0;

    case 2:
        if (pHeldCard == NULL) {
            OnButtonMouseDown(2);
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            Sleep(0x96);
            g_UIResources.PlayUiSound(0x5015);
            this->EndActiveSession();
            AppWindow_SetScreenState(3);
            return 0;
        }
        pHeldCard = NULL;
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        return 0;

    case 3:
        if (pHeldCard != NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
            DiscardHeldCardMaybe();
            pHeldCard = NULL;
            RedrawCardAreaMaybe();
            if (bViewingOutboxMaybe == 0 && pOpenCard == NULL) {
                OnButtonMouseUp(8);
            }
            OnButtonMouseUp(7);
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            return 0;
        }
        OnButtonMouseDown(3);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        ((AlbumCardWndVtblProbe *)g_pAlbumCardWnd)->_v08();
        this->EndActiveSession();
        return 0;

    case 4:
        if (pHeldCard != NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        }
        OnButtonMouseDown(4);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        g_pEditCardWnd->BeginEdit(pHeldCard);
        if (pHeldCard != NULL) {
            pHeldCard = NULL;
            pOpenCard = NULL;
        }
        this->EndActiveSession();
        bInputEnabled = 0;
        return 0;

    case 5:
        if (pHeldCard != NULL) {
            pHeldCard = NULL;
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        }
        if (bButtonsEnabledMaybe == 0) {
            return 0;
        }
        OnButtonMouseDown(5);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        OnButtonMouseUp(5);
        DeleteOpenCardFileMaybe();
        SelectNextCardMaybe();
        RestoreCardBackdropMaybe(0);
        OnButtonMouseUp(8);
        OnButtonMouseUp(2);
        OnButtonMouseUp(3);
        OnButtonMouseUp(4);
        OnButtonMouseUp(7);
        OnButtonMouseUp(6);
        RedrawCardAreaMaybe();
        if (bViewingOutboxMaybe == 0 && pOpenCard == NULL) {
            OnButtonMouseUp(8);
        }
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;

    case 6:
        if (pHeldCard != NULL) {
            pHeldCard = NULL;
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
            return 0;
        }
        if (bButtonsEnabledMaybe == 0) {
            return 0;
        }
        OnButtonMouseDown(6);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        OnButtonMouseUp(6);
        bCardSideMaybe = (bCardSideMaybe == 0);
        RedrawCardAreaMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;

    case 7:
        if (pHeldCard != NULL) {
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
            MoveHeldCardToOutboxMaybe();
            pHeldCard = NULL;
            RedrawCardAreaMaybe();
            if (bViewingOutboxMaybe == 0 && pOpenCard == NULL) {
                OnButtonMouseUp(8);
            }
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            return 0;
        }
        if (bButtonsEnabledMaybe != 0) {
            OnButtonMouseDown(7);
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            Sleep(0x96);
            SelectNextCardMaybe();
            OnButtonMouseUp(7);
            OnButtonMouseUp(8);
            OnButtonMouseUp(6);
            RedrawCardAreaMaybe();
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
            return 0;
        }
        // sic: falls through into the inbox-drop command, whose own held-card guard then always
        // fails (nothing is held on this path).

    case 8:
        if (pHeldCard == NULL) {
            return 0;
        }
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
        MoveHeldCardToInboxMaybe();
        pHeldCard = NULL;
        OnButtonMouseUp(8);
        OnButtonMouseUp(7);
        RedrawCardAreaMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;

    case 9:
        if (pHeldCard != NULL) {
            pHeldCard = NULL;
            RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
            return 0;
        }
        if (nInputCooldownMaybe != 0) {
            return 0;
        }
        nInputCooldownMaybe = 8;
        OnButtonMouseDown(9);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        OnButtonMouseUp(9);
        g_UIResources.PlayUiSound(0x5276);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return 0;
    }

    return 0;
}

// ---------------------------------------------------------------------------------------------
// Construction / teardown and the remaining WindowBase vtable overrides. Appended in address
// order; the block above is in the order the earlier sessions transcribed it.
// ---------------------------------------------------------------------------------------------

// FUNCTION: LOCO 0x42e420 (Ghidra: MailWnd::OnActivate)
// vtable slot 0x20. Only does anything once the button resources have been realized: forces the
// master input enable back on, repaints the backdrop, pops every button icon back to its
// released frame, redraws the card, and takes the focus. Finally offers the tutorial a chance to
// take over (notification code 1) -- if it does, bTearingDownMaybe latches so OnSetFocus below
// keeps bouncing the focus across to it.
void MailWnd::OnActivate(int /*reservedMaybe*/)
{
    if (bInit0x5f9) {
        if (bInputEnabled == 0) {
            bInputEnabled = 1;
        }
        RestoreCardBackdropMaybe(0);
        OnButtonMouseUp(2);
        OnButtonMouseUp(3);
        OnButtonMouseUp(4);
        OnButtonMouseUp(9);
        OnButtonMouseUp(7);
        OnButtonMouseUp(5);
        OnButtonMouseUp(6);
        OnButtonMouseUp(8);
        RedrawCardAreaMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        SetFocus(hwndSelf);
        if (g_pTutorialWnd->NotifyOrLaunch(1, 0)) {
            bTearingDownMaybe = 1;
        }
    }
}

// FUNCTION: LOCO 0x42e900 (Ghidra: MailWnd::MailWnd)
// The ctor proper only chains the base and runs the second-stage field/resource init; every
// scalar this class owns is zeroed by InitResourceRefs rather than here.
MailWnd::MailWnd(HINSTANCE hInstanceParam, UINT resourceIdParam)
    : WindowBase(hInstanceParam, resourceIdParam)
{
    InitResourceRefs();
}

// FUNCTION: LOCO 0x42e960 (??_GMailWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x42ec10 (Ghidra: MailWnd::~MailWnd)
// Releases whatever the two lazy-realize helpers built, then deletes the eight icon-button
// ResourceRefs. The `??_GMailWnd` scalar deleting destructor cl generates alongside this (vtable
// slot 0, 0x42e960) is a free byproduct and is not separately marked.
MailWnd::~MailWnd()
{
    if (bInit0x5fa) {
        pBackdropDescMaybe->ReleaseRef();
        bInit0x5fa = 0;
    }
    if (bInit0x5f9) {
        paButtons[0]->ReleaseRealized();
        paButtons[1]->ReleaseRealized();
        paButtons[2]->ReleaseRealized();
        paButtons[3]->ReleaseRealized();
        paButtons[4]->ReleaseRealized();
        paButtons[5]->ReleaseRealized();
        paButtons[6]->ReleaseRealized();
        paButtons[7]->ReleaseRealized();
        pDesc0x664->ReleaseRef();
        pDesc0x64c->ReleaseRef();
        pCardCursorDesc->ReleaseRef();
        bInit0x5f9 = 0;
    }
    if (paButtons[0] != NULL) {
        delete paButtons[0];
        paButtons[0] = NULL;
    }
    if (paButtons[1] != NULL) {
        delete paButtons[1];
        paButtons[1] = NULL;
    }
    if (paButtons[2] != NULL) {
        delete paButtons[2];
        paButtons[2] = NULL;
    }
    if (paButtons[3] != NULL) {
        delete paButtons[3];
        paButtons[3] = NULL;
    }
    if (paButtons[4] != NULL) {
        delete paButtons[4];
        paButtons[4] = NULL;
    }
    if (paButtons[5] != NULL) {
        delete paButtons[5];
        paButtons[5] = NULL;
    }
    if (paButtons[6] != NULL) {
        delete paButtons[6];
        paButtons[6] = NULL;
    }
    if (paButtons[7] != NULL) {
        delete paButtons[7];
        paButtons[7] = NULL;
    }
}

// FUNCTION: LOCO 0x42edb0 (Ghidra: MailWnd::Create)
// Fullscreen: the window is created at the desktop's own client rect. Style 0x81000000 =
// WS_POPUP | WS_VISIBLE.
unsigned char MailWnd::Create(HWND hwndOwnerParam)
{
    RECT rectDesktop;
    HWND hwndDesktop;

    hwndDesktop = GetDesktopWindow();
    GetClientRect(hwndDesktop, &rectDesktop);
#ifdef LOCO_PORT
    Port_ClampDesktopRect(&rectDesktop); // PORT: desktop != screen here; see port/PortMode.h
#endif
    hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCEA(0x65));
    if (WindowBase::Create(0, hwndOwnerParam, rectDesktop.left, rectDesktop.top,
                           rectDesktop.right - rectDesktop.left,
                           rectDesktop.bottom - rectDesktop.top,
                           NULL, hIcon, 0, 0x81000000, 0)) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x42ee20 (Ghidra: MailWnd::OnUnhandledMessageMaybe)
// vtable slot 0x2c. Two special cases on top of the default handler: a screensaver/monitor-power
// WM_SYSCOMMAND is let through to the app's own show-window routine and disarms the animation
// timer, and the private message 0x5f5 re-enables the window (the modal attachment flow disables
// it on the way out).
LRESULT MailWnd::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_SCREENSAVE) {
            FUN_00463670_LotsOfShowWindow();
            if (nTimerId != 0) {
                KillTimer(hwndSelf, 0x4d);
                nTimerId = 0;
            }
        }
        break;

    case 0x5f5:
        EnableWindow(hwndSelf, TRUE);
        break;
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x42f810 (Ghidra: MailWnd::OnKeyDown)
// vtable slot 0x54. Esc and Q are the keyboard equivalent of clicking the exit button (command
// id 2): press it, show the press, click, then hand the app back to state 3. Every other key
// falls through to DefWindowProcA -- but only while input is enabled; otherwise the message is
// swallowed entirely.
LRESULT MailWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bInputEnabled && bAttachmentModalBusyMaybe == 0 && bTearingDownMaybe == 0) {
        if (wParam != VK_ESCAPE && wParam != 'Q') {
            return DefWindowProcA(hwndMsg, msg, wParam, lParam);
        }
        OnButtonMouseDown(2);
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        Sleep(0x96);
        g_UIResources.PlayUiSound(0x5015);
        EndActiveSession();
        AppWindow_SetScreenState(3);
    }
    return 0;
}

// FUNCTION: LOCO 0x42fdf0 (Ghidra: MailWnd::EnsureBackdropRealizedMaybe)
// The backdrop is realized separately from everything else because it outlives a modal session:
// EndActiveSession drops the bInit0x5f9 resources but leaves this pair alone, and only the dtor
// releases it.
void MailWnd::EnsureBackdropRealizedMaybe()
{
    if (bInit0x5fa == 0) {
        pBackdropDescMaybe = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cf7);
        pCardBackdropBmp = pBackdropDescMaybe->GetOrLoadFrameBitmap(0, 0);
        bInit0x5fa = 1;
    }
}

// FUNCTION: LOCO 0x42fe30 (Ghidra: MailWnd::EnsureButtonResourcesRealizedMaybe)
// Called by BeginModalCapture on the way into the screen; the exact inverse of the bInit0x5f9
// block in ~MailWnd and in EndActiveSession.
void MailWnd::EnsureButtonResourcesRealizedMaybe()
{
    if (bInit0x5f9 == 0) {
        paButtons[0]->Load();
        paButtons[1]->Load();
        paButtons[2]->Load();
        paButtons[3]->Load();
        paButtons[4]->Load();
        paButtons[5]->Load();
        paButtons[6]->Load();
        paButtons[7]->Load();
        pDesc0x64c = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cf8);
        p0x650 = pDesc0x64c->GetOrLoadFrameBitmap(0, 0);
        pDesc0x664 = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cfb);
        pFlagBmpMaybe = pDesc0x664->GetOrLoadFrameBitmap(0, 0);
        pCardCursorDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3cfa);
        pCardCursorRect = pCardCursorDesc->GetOrLoadFrameBitmap(0, 0);
        bInit0x5f9 = 1;
    }
}

// FUNCTION: LOCO 0x42ff20 (Ghidra: MailWnd::OnSetFocus)
// vtable slot 0x60. Once the tutorial has taken this screen over, refuse the focus: repaint the
// tutorial window and raise it back to the top instead.
//
// EFFECTIVE MATCH -- DIFF(15), insns 32/32, structurally identical; the residual is confined to
// the four-argument forward at the tail. The original schedules it across TWO registers,
// interleaving loads and pushes (load lParam->edx, load wParam->eax, push edx, load msg->edx,
// push eax, load hwnd->eax, ...); cl here hoists THREE loads (ecx/edx/eax) before the first
// push. Same instruction count, same operands, same call -- a register/scheduling coin-flip.
// Probed without effect: routing the result through a named `LRESULT` local. Probed and REFUTED:
// calling DefWindowProcA directly instead of the 0x422ea0 stub (DIFF 15 -> 20, which is also
// what confirms the stub call is the right model -- see the declaration at the top of this file).
LRESULT MailWnd::OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bTearingDownMaybe) {
        PostMessageA(g_pTutorialWnd->hwndSelf, WM_PAINT, 0, 0);
        SetWindowPos(g_pTutorialWnd->hwndSelf, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        return 0;
    }
    return DefWindowProcStub(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x42ff80 (Ghidra: MailWnd::OnClose)
// vtable slot 0x80. Leaves the screen exactly the way the exit button and the Esc key do, unless
// the app object is already gone or the net subsystem is shutting down -- in which case the base
// handler runs instead and the window really does close.
LRESULT MailWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        g_UIResources.PlayUiSound(0x5015);
        EndActiveSession();
        AppWindow_SetScreenState(3);
        return 0;
    }
    return WindowBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x42fff0 (Ghidra: MailWnd::OnTimerDefaultMaybe)
// vtable slot 0x30, armed at 200 ms by BeginModalCapture. Drives the two-frame flag animation:
// frame 1 is shown for a single tick, frame 0 for twenty. Also ticks the button-press cooldown.
LRESULT MailWnd::OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (nTimerId != 0) {
        nFlagAnimTickMaybe++;
        if (bFlagFrameMaybe == 1) {
            bFlagFrameMaybe = 0;
            nFlagAnimTickMaybe = 0;
            DrawFlagFrameMaybe();
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        } else if (nFlagAnimTickMaybe >= 20) {
            bFlagFrameMaybe = 1;
            DrawFlagFrameMaybe();
            CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        }
        if (nInputCooldownMaybe != 0) {
            nInputCooldownMaybe--;
        }
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4307c0 (Ghidra: MailWnd::OnRButtonDown)
// vtable slot 0x40. Right-click while carrying a card just drops it: the cursor goes back to the
// generic anipoint pair and the card stays wherever it already lives on disk.
LRESULT MailWnd::OnRButtonDown(HWND /*hwndMsg*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (bAttachmentModalBusyMaybe == 0 && pHeldCard != NULL) {
        pHeldCard = NULL;
        RequestModeTransitionFromSource(pAnipointCursorRect, pAnipointCursorDesc, 0, 1);
    }
    return 0;
}

// FUNCTION: LOCO 0x42f5e0 (Ghidra: MailWnd::BeginModalCapture)
// vtable slot 8 -- the way into the screen. Note the screen always OPENS on the outbox
// (bViewingOutboxMaybe = 1), so the card-count refresh that follows is the helper's outbox
// branch every time.
void MailWnd::BeginModalCapture()
{
    while (ShowCursor(FALSE) >= 0) {
    }
    WindowBase::BeginModalCapture();
    bAttachmentModalBusyMaybe = 0;
    EnsureButtonResourcesRealizedMaybe();
    RefreshClientClipRect();
    SetFocus(hwndSelf);
    RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    bViewingOutboxMaybe = 1;
    GetViewedCategoryCardCountMaybe();
    if (nTimerId == 0) {
        nTimerId = SetTimer(hwndSelf, 0x4d, 200, NULL);
    }
    SelectNextCardMaybe();
    bTearingDownMaybe = 0;
}

// FUNCTION: LOCO 0x42f6c0 (Ghidra: MailWnd::EndActiveSession)
// vtable slot 4 -- the way out. Everything inside the guard is the exact inverse of
// BeginModalCapture; the tail runs unconditionally, because leaving the mail screen is also
// what re-syncs the multiplayer UI's "you have outgoing post" indicator.
void MailWnd::EndActiveSession()
{
    if (bModalCaptureActive) {
        WindowBase::EndActiveSession();
        while (ShowCursor(FALSE) >= 0) {
        }
        g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
        if (pOpenCard != NULL) {
            delete pOpenCard;
            pOpenCard = NULL;
            pHeldCard = NULL;
        }
        if (bInit0x5f9) {
            paButtons[0]->ReleaseRealized();
            paButtons[1]->ReleaseRealized();
            paButtons[2]->ReleaseRealized();
            paButtons[3]->ReleaseRealized();
            paButtons[4]->ReleaseRealized();
            paButtons[5]->ReleaseRealized();
            paButtons[6]->ReleaseRealized();
            paButtons[7]->ReleaseRealized();
            pDesc0x664->ReleaseRef();
            pDesc0x64c->ReleaseRef();
            pCardCursorDesc->ReleaseRef();
            bInit0x5f9 = 0;
        }
        if (nTimerId != 0) {
            KillTimer(hwndSelf, 0x4d);
        }
        nTimerId = 0;
        bInputEnabled = 0;
    }
    if (g_pPostBagCache->PostBag_RecountCategoryOutFiles() != 0) {
        g_pDPlaySessionMgr->SetUiModeAndNotifyWidgets(1);
    } else {
        g_pDPlaySessionMgr->SetUiModeAndNotifyWidgets(0);
    }
    bMailPendingMaybe = 0;
}

// FUNCTION: LOCO 0x4309b0 (Ghidra: MailWnd::SaveOpenCardToFileMaybe)
// Writes the open card back over its own .crd: scans the viewed category for the file whose
// stored card carries the same post-sequence id, deletes it, then recreates it and writes the
// 0x398-byte payload starting at wSignature. Unlike this TU's other category walks there is no
// per-iteration pOpenCard null check -- the function-entry guard already established it.
void MailWnd::SaveOpenCardToFileMaybe()
{
    if (pOpenCard != NULL) {
        PostBagCrdFileNode *pNode;
        if (bViewingOutboxMaybe) {
            pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(1, NULL);
        } else {
            pNode = g_pPostBagCache->PostBag_ScanCategoryCrdFiles(2, NULL);
        }
        while (pNode != NULL) {
            CarNetState *pCard = g_pPostBagCache->CarNetState_CreateFromFile(pNode->szPath);
            if (pCard->nPostSeqId == pOpenCard->nPostSeqId) {
                g_pPostBagCache->DeleteCardFileAndRefreshCount(pNode->szPath);
                HANDLE hFile = CreateFileA(pNode->szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                           CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD dwWritten;
                    WriteFile(hFile, &pOpenCard->wSignature, 0x398, &dwWritten, NULL);
                    CloseHandle(hFile);
                }
            }
            delete pCard;
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
        }
    }
}

// EditCardWnd_CenterFileDialogHookProcMaybe (0x419fd0, untranscribed) -- the shared centered
// file-dialog lpfnHook; declared file-locally exactly as src/EditCardWnd.cpp does.
UINT CALLBACK EditCardWnd_CenterFileDialogHookProcMaybe(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x419fd0

// FUNCTION: LOCO 0x42eea0 (Ghidra: MailWnd::PromptForAttachmentSavePathMaybe)
// The modal "where do you want the attachment?" GetSaveFileNameA prompt. szExtractPathMaybe
// arrives already holding the attachment's ORIGINAL path (OpenAttachmentMaybe ReadFile'd the .dat
// sidecar into it) and doubles as ofn.lpstrFile, so the dialog opens on that name and overwrites
// it in place with the user's choice.
//
// Returns 0 = cancelled, or the user declined an overwrite, or a hard error was reported;
// 1 = go ahead (the target does not exist, or it does and the user said yes);
// 2 = the chosen path names a directory that does not exist yet -- the caller CreateDirectoryA's
// it first. The 2 comes out of `== IDYES ? 2 : 0`, which is what emits the original's
// `setne cl; dec ecx; and ecx,2`.
//
// pszScratchText is declared but NEVER read (`ret 0x4` with no access to the slot); the caller
// passes the empty literal into it. Reproduced as-is.
//
// sic: szFilter is carefully built up ("*." plus the source file's own extension) and then never
// used -- ofn.lpstrFilter gets the empty literal instead, so the dialog shows no filter at all.
// Confirmed off the raw disasm: the only write to OFN+0x0c is `mov [esp+0x2c],0x4851d0`, and
// nothing ever reads the szFilter slot at all. See docs/engine-bugs.md.
int MailWnd::PromptForAttachmentSavePathMaybe(char *pszScratchText)
{
    char szText[0x100] = "";
    char szFileTitle[0x104] = "";
    char szInitialDir[0x104] = "c:";
    char szFilter[0x104] = "*.";
    char szDefExt[0x104] = "";

    if (strchr(szExtractPathMaybe, '.') != NULL) {
        // sic: the original really does call strchr a second time here rather than reusing the
        // guard's own result.
        strcpy(szDefExt, strchr(szExtractPathMaybe, '.') + 1);
        strcat(szFilter, szDefExt); // sic: szFilter is never handed to the dialog
    } else {
        strcat(szFilter, "*"); // sic: as above
        strcpy(szDefExt, "");
    }

    g_pPostBagCache->PostBag_ReadDatFile(pOpenCard->wAttachmentId, 5, szExtractPathMaybe);
    g_UIResources.LoadLocaleString(0x6a, szText, sizeof(szText));

    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndSelf;
    ofn.hInstance = (HINSTANCE)hInstance;
    ofn.lpstrDefExt = szDefExt;
    ofn.lpstrTitle = szText;
    ofn.lpstrFilter = ""; // sic: szFilter, built above, is dropped on the floor
    ofn.nFilterIndex = 1;
    ofn.Flags = 0x80024;
    ofn.lpfnHook = EditCardWnd_CenterFileDialogHookProcMaybe;
    ofn.lpstrFile = szExtractPathMaybe;
    ofn.nMaxFile = sizeof(szExtractPathMaybe);
    ofn.lpstrFileTitle = szFileTitle;
    ofn.nMaxFileTitle = sizeof(szFileTitle);
    ofn.lpstrInitialDir = szInitialDir;

    PostMessageA(hwndSelf, 0x5f5, 0, 0);
    bAttachmentModalBusyMaybe = 1;
    SetCursorPos(paButtons[7]->rect.left, paButtons[7]->rect.bottom + 20);
    RequestModeTransitionFromSource(pPointCursorRect, pPointCursorDesc, 0, 1);
    BOOL bChosen = GetSaveFileNameA(&ofn);
    bAttachmentModalBusyMaybe = 0;
    if (bChosen) {
        HANDLE hFile = CreateFileA(szExtractPathMaybe, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            g_UIResources.LoadLocaleString(0x6b, szText, sizeof(szText));
            if (MessageBoxA(hwndSelf, szText, "LEGO LOCO", MB_YESNO) == IDYES) {
                CloseHandle(hFile);
                return 1;
            }
            CloseHandle(hFile);
            return 0;
        }
        DWORD dwErr = GetLastError();
        if (dwErr == ERROR_FILE_NOT_FOUND) {
            return 1;
        }
        if (dwErr == ERROR_PATH_NOT_FOUND) {
            g_UIResources.LoadLocaleString(0x6d, szText, sizeof(szText));
            // The `nAnswer` local is LOAD-BEARING and was the last residual (DIFF(69) -> EXACT).
            // Spelled inline as `return MessageBoxA(...) == IDYES ? 2 : 0;` the select has no
            // spare register, so cl takes the in-place `sub eax,6; neg; sbb; and al,0xfe; add 2`
            // route instead of the original's `xor ecx,ecx; cmp eax,6; setne cl; dec ecx;
            // and ecx,2` -- and the freed-up allocation ALSO let cl head-merge this arm's whole
            // LoadLocaleString argument setup with the one below, hoisting five instructions
            // above the branch that the original keeps duplicated per arm.
            int nAnswer = MessageBoxA(hwndSelf, szText, "LEGO LOCO",
                                      MB_YESNO | MB_ICONQUESTION);
            return nAnswer == IDYES ? 2 : 0;
        }
        g_UIResources.LoadLocaleString(0x6d, szText, sizeof(szText));
        MessageBoxA(hwndSelf, szText, "LEGO LOCO", MB_ICONEXCLAMATION);
    }
    return 0;
}

// FUNCTION: LOCO 0x42f250 (Ghidra: MailWnd::OpenAttachmentMaybe)
// "Open" the open card's attachment: read the .dat sidecar (which holds the attachment's
// original path) into szExtractPathMaybe, ask the user where to put it, then MOVE the .att there
// (CopyFileA + DeleteFileA) and clear the card's wAttachmentId so the stamp stops being drawn.
//
// Every failure path is the same six-line report-and-continue block -- GetLastError,
// FormatMessageA into pszErrMsg, MessageBoxA, LocalFree, reset. It is written out literally
// rather than factored into a helper: cl hoists the three import thunks (GetLastError,
// FormatMessageA, MessageBoxA) into esi/edi/ebx across the last three copies, and all six share
// the ONE function-scope pszErrMsg slot.
//
// The `DWORD dwErr = GetLastError();` local in each block is LOAD-BEARING, and was the whole
// residual: spelled inline as FormatMessageA(..., GetLastError(), ...) the call is evaluated in
// its right-to-left argument position, i.e. AFTER the four constant pushes above it, and the
// resulting register pressure then flipped the esi/edi thunk-cache assignment and reordered the
// `mov ecx,g_pPostBagCache` in the two path-builder calls as well -- DIFF(356) across the whole
// function, from 296/296 structurally-identical instructions. Hoisting it took the function
// EXACT in one compile.
void MailWnd::OpenAttachmentMaybe()
{
    LPSTR pszErrMsg;
    DWORD dwRead;
    char szPath[0x504];
    char szDir[0x504];

    pszErrMsg = NULL;
    g_pPostBagCache->PostBag_BuildDatFilePath(pOpenCard->wAttachmentId, 5, szPath);
    HANDLE hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD dwErr = GetLastError();
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                       dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&pszErrMsg, 0, NULL);
        MessageBoxA(hwndSelf, pszErrMsg, "LEGO LOCO", MB_ICONHAND);
        LocalFree(pszErrMsg);
        pszErrMsg = NULL;
        pOpenCard->wAttachmentId = 0;
        RedrawCardAreaMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return;
    }

    if (!ReadFile(hFile, szExtractPathMaybe, 0x504, &dwRead, NULL)) {
        DWORD dwErr = GetLastError();
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                       dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&pszErrMsg, 0, NULL);
        MessageBoxA(hwndSelf, pszErrMsg, "LEGO LOCO", MB_ICONHAND);
        LocalFree(pszErrMsg);
        pszErrMsg = NULL;
        pOpenCard->wAttachmentId = 0;
        RedrawCardAreaMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
        return;
    }
    CloseHandle(hFile);

    int nChoice = PromptForAttachmentSavePathMaybe("");
    if (nChoice != 0) {
        if (nChoice == 2) {
            // The chosen path names a folder that does not exist yet -- strip the filename off
            // a scratch copy and create it first.
            strcpy(szDir, szExtractPathMaybe);
            *strrchr(szDir, '\\') = '\0';
            if (!CreateDirectoryA(szDir, NULL)) {
                DWORD dwErr = GetLastError();
                        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                               dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPSTR)&pszErrMsg, 0, NULL);
                MessageBoxA(hwndSelf, pszErrMsg, "LEGO LOCO", MB_ICONHAND);
                LocalFree(pszErrMsg);
                return;
            }
        }

        g_pPostBagCache->PostBag_BuildAttFilePath(pOpenCard->wAttachmentId, 5, szPath);
        if (!CopyFileA(szPath, szExtractPathMaybe, FALSE)) {
            DWORD dwErr = GetLastError();
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                           dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPSTR)&pszErrMsg, 0, NULL);
            MessageBoxA(hwndSelf, pszErrMsg, "LEGO LOCO", MB_ICONHAND);
            LocalFree(pszErrMsg);
            pszErrMsg = NULL;
        }
        if (DeleteFileA(szPath)) {
            g_pPostBagCache->PostBag_BuildDatFilePath(pOpenCard->wAttachmentId, 5, szPath);
            if (!DeleteFileA(szPath)) {
                DWORD dwErr = GetLastError();
                        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                               dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPSTR)&pszErrMsg, 0, NULL);
                MessageBoxA(hwndSelf, pszErrMsg, "LEGO LOCO", MB_ICONHAND);
                LocalFree(pszErrMsg);
                pszErrMsg = NULL;
            }
            pOpenCard->wAttachmentId = 0;
            SaveOpenCardToFileMaybe();
        } else {
            DWORD dwErr = GetLastError();
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                           dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPSTR)&pszErrMsg, 0, NULL);
            MessageBoxA(hwndSelf, pszErrMsg, "LEGO LOCO", MB_ICONHAND);
            LocalFree(pszErrMsg);
            pszErrMsg = NULL;
        }
        RedrawCardAreaMaybe();
        CommitScreenUpdate(hwndSelf, 0, 0, NULL);
    }
}
