// UiIconListItem's own ordinary members. MenuNodeObj0x477568's own ctor/dtor/virtuals stay
// declared-only (see MenuNode.h) until a caller needs their bodies.
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "MenuNode.h"
#include "UIResources.h"  // g_UIResources, PlaySoundAtScreenPos

extern void *g_pActiveTabWidgetMaybe; // DAT_004fd3e0, see src/WidgetPicker.cpp // TODO: idiom

// FUNCTION: LOCO 0x448f30
// EXACT MATCH. pText is seeded with the empty string: the copy source is the pooled "" literal
// at 0x4851d0 (the same literal WidgetPicker.cpp's own SetLabelText("") call sites pass into
// this same field via the sibling method above -- it is a string literal, NOT a shared scratch
// global; see docs/CODEGEN.md). The manual byte-copy loop is CRT strcpy()'s own intrinsic
// shape (rep movsd+movsb after a repnz-scasb strlen scan) -- same family as the
// RebuildLocalPlayerCard/BuildPaletteLUTMaybe intrinsic-substitution lessons, not a
// hand-rolled loop. Params deliberately suffixed *Arg -- nTextLen/hFontLabel already name
// members of this class.
UiIconListItem::UiIconListItem(int nTextLenArg, WidgetBaseObj0x4784c8 *pOwner, CursorDesc *pDesc, int hFontLabelArg, unsigned short wModeFlags)
    : MenuNodeObj0x477568(pOwner, pDesc, wModeFlags)
{
    nTextLen = nTextLenArg;
    nTypeTag = 8;
    pText = new char[nTextLen + 1];
    if (pText != 0) {
        strcpy(pText, "");
    }
    bTextRedrawEnabled = false;
    hFontLabel = hFontLabelArg;
}

// FUNCTION: LOCO 0x40cfa0
// EXACT MATCH. Tail-calls InitOwnerAndDescMaybe (0x40d0b0, transcribed below).
MenuNodeObj0x477568::MenuNodeObj0x477568(WidgetBaseObj0x4784c8 *pOwner, CursorDesc *pDesc, unsigned short wModeFlags)
{
    pNext = 0;
    wModeFlagsMaybe = 0;
    nFrameIndex = 0;
    nTickCounter = 0;
    pEntry = 0;
    nTypeTag = 7;
    wSelIndexMaybe = (short)0xffff;
    bVisible = true;
    InitOwnerAndDescMaybe(pOwner, pDesc, wModeFlags);
}

// FUNCTION: LOCO 0x40d020 (??_GMenuNodeObj0x477568 scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x40d040
// Cascades down the owning intrusive list: `delete pNext` dispatches slot 0 (the scalar
// deleting dtor), so a whole menu-node chain is freed by destroying its head.
//
// ⚠ The leading explicit `RectFlagObj0x477820::~RectFlagObj0x477820()` is NOT a transcription
// artifact -- the base dtor is genuinely invoked TWICE on the same `this` (raw disasm: `call
// 0x436a00` at 0x40d06b at EH state 0, then again at 0x40d08e at EH state -1, both with
// ecx = this). Ruled out before transcribing it: (a) ICF folding a ctor onto the dtor -- the
// ctor is a DISTINCT function at 0x4369d0, which is what MenuNodeObj's own ctor calls at
// 0x40cfbd; (b) an intermediate base class -- src/MenuNode.h's direct-derivation model is
// confirmed by the vtable at 0x477568 inheriting exactly RectFlagObj's slots 1-5. The second
// call is the compiler's own base-subobject destruction (it follows the EH-state -1 store);
// the first is therefore a source-level statement, and an explicit base-dtor call is the only
// construct that emits one. Harmless -- `~RectFlagObj0x477820` is idempotent (restamps the
// vptr, re-clears bValid) -- which is exactly why it survived to ship. See docs/engine-bugs.md.
MenuNodeObj0x477568::~MenuNodeObj0x477568()
{
    this->RectFlagObj0x477820::~RectFlagObj0x477820(); // sic: redundant explicit base-dtor call
    if (pNext != 0) {
        delete pNext;
        pNext = 0;
    }
}

// FUNCTION: LOCO 0x448fe0 (??_GUiIconListItem scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x449000 // TODO: sync (Ghidra: UiIconListItem::UiIconListItem_DtorMaybe --
// real C++ dtor syntax needed here for base-class chaining; same naming gap as
// WindowBase's own dtor, see WindowBase.cpp)
// EXACT MATCH. Frees pText (scalar operator delete, matching a plain char* -- not delete[]).
UiIconListItem::~UiIconListItem()
{
    if (pText != 0) {
        delete pText;
        pText = 0;
    }
}

// FUNCTION: LOCO 0x449070
// EXACT MATCH. FUN_00467710 is the CRT's own toupper() (locale-aware; fast path is a plain
// -0x20 on lowercase ASCII) -- an intrinsic-shaped leaf, not a private helper (same family as
// the strcmp/strcpy/memset intrinsic-substitution lessons). strlen(pszText) (the untruncated
// input length, not nTextLen) is the real uppercase-loop bound -- a faithful transcription of
// the original, not a simplification: if pszText is longer than nTextLen the loop walks past
// what strncpy actually copied into pText. Param deliberately named pszText, not pText --
// would otherwise shadow the member of the same name.
char *UiIconListItem::SetLabelText(char *pszText)
{
    strncpy(pText, pszText, nTextLen);
    pText[nTextLen] = 0;
    for (int i = strlen(pszText) - 1; i >= 0; i--) {
        pText[i] = toupper(pText[i]);
    }
    return pText;
}

// FUNCTION: LOCO 0x4490d0
// EXACT MATCH (moved out of src/phase2_probe.cpp 2026-07-22, where it lived as the
// probe-local Obj0x64::GetVal0x60).
char *UiIconListItem::GetLabelText() { return pText; }

// FUNCTION: LOCO 0x4490e0
// The label's length -- a bare intrinsic strlen over the same pText GetLabelText hands out
// above, which is why it sits immediately after it in .text. Every caller (all of them in
// src/WidgetPicker.cpp) uses it purely as an "is this slot labelled at all" predicate.
unsigned UiIconListItem::GetLabelTextLength() { return strlen(pText); }

// FUNCTION: LOCO 0x40d0b0
// EXACT MATCH. Sets pOwner/pIconDesc; if either is null marks bValid false, else sets the
// inherited base rect (screen position from pDesc's field_0x2e/0x30, sized to the shadow
// bitmap) and rectLocal (0,0,shadowWidth,shadowHeight), wState=1, wModeFlagsMaybe, and
// wSelIndexMaybe=0xffff. If wModeFlags bit 2 (carousel mode) is set, grows pOwner's own
// nCarouselMaxIndex to fit this node (pIconDesc->field_0x2e / 0x39 - 2).
void MenuNodeObj0x477568::InitOwnerAndDescMaybe(WidgetBaseObj0x4784c8 *pOwnerArg, CursorDesc *pDesc, unsigned short wModeFlags)
{
    pOwner = pOwnerArg;
    pIconDesc = pDesc;
    if (pOwner == 0 || pDesc == 0) {
        bValid = false;
    } else {
        SetRect(&rect, pDesc->field_0x2eMaybe, pDesc->field_0x30Maybe,
                pDesc->wShadowFrameWidth + pDesc->field_0x2eMaybe,
                pDesc->wShadowBitmapHeight + pDesc->field_0x30Maybe);
        SetRect(&rectLocal, 0, 0, pIconDesc->wShadowFrameWidth, pIconDesc->wShadowBitmapHeight);
        wState = 1;
        wModeFlagsMaybe = wModeFlags;
        wSelIndexMaybe = 0xffff;
        if (wModeFlags & 2) {
            int nMinWidth = pIconDesc->field_0x2eMaybe / 0x39 - 2;
            if (pOwner->nCarouselMaxIndex < nMinWidth) {
                pOwner->nCarouselMaxIndex = nMinWidth;
            }
        }
    }
}

// FUNCTION: LOCO 0x40d170
// EFFECTIVE MATCH -- PARKED (asmscore.py --len 276: total 164644, align=160 reg_pen=41
// byte_diff=94, insns 99/91; see docs/PARKED.md). Modes 1-3 notify via slot 6
// (SetFrameAndNotify) with a computed frame index; mode 4 (carousel scroll-into-view) copies
// this node's rect onto pOwner's cached pLastHitNode, offsets it by the carousel scroll
// amount, recurses into pLastHitNode->SetNodeState(2), then redraws it (slot 8). Every
// pOwner->pLastHitNode access is re-read fresh (no cached local) -- matches the original's
// own per-statement reloads. `bValid == true` (not a bare `if (bValid)`) needed for the
// literal `cmp byte,1` comparison shape (Yoda #2). Residual: the compiler CSEs the vtable
// pointer load across cases 1-3 (all 3 dispatch through the same slot 6), which the original
// does NOT do (it reloads `*this` fresh at each of the 3 call sites) -- tried caching
// nButtonFrameCount, splitting the switch into early `return`s per case, and a bare `if`
// chain instead of `switch`, all three identical scores; treating this as an intrinsic
// cross-case CSE the source can't suppress, same family as the Yoda #29/#30 symmetric
// register-swap residuals extended to a shared-vtable-slot dispatch.
void MenuNodeObj0x477568::SetNodeState(short state)
{
    if (bValid == true && wState != state) {
        wState = state;
        switch (state) {
        case 1:
            SetFrameAndNotify(0);
            return;
        case 2: {
            unsigned short nFrameCount = pIconDesc->nButtonFrameCount;
            if (nFrameCount < 3) {
                SetFrameAndNotify(1);
                return;
            }
            SetFrameAndNotify(nFrameCount - 2);
            return;
        }
        case 3:
            SetFrameAndNotify(pIconDesc->nButtonFrameCount - 1);
            return;
        case 4:
            pOwner->pLastHitNode->rect = rect;
            pOwner->pLastHitNode->rect.left += pOwner->nCarouselScrollIndex * -0x39;
            pOwner->pLastHitNode->rect.right += pOwner->nCarouselScrollIndex * -0x39;
            pOwner->pLastHitNode->SetNodeState(2);
            pOwner->pLastHitNode->Draw();
            return;
        }
    }
}

// FUNCTION: LOCO 0x40d2a0
// EXACT MATCH. Sets nFrameIndex/rectLocal's left+right (sprite-sheet sub-rect for nFrame),
// redraws (slot 8), resets nTickCounter. pIconDesc->wShadowFrameWidth is re-read for BOTH
// the left and right computation (no cached local) -- matches the original's own two
// separate field reloads.
void MenuNodeObj0x477568::SetFrameAndNotify(int nFrame)
{
    if (bValid == true) {
        nFrameIndex = nFrame;
        rectLocal.left = nFrame * pIconDesc->wShadowFrameWidth;
        rectLocal.right = (nFrame + 1) * pIconDesc->wShadowFrameWidth;
        Draw();
        nTickCounter = 0;
    }
}

// FUNCTION: LOCO 0x40d2f0
// EXACT MATCH. nTickCounter increments unconditionally while bValid (even if wState != 1) --
// the store happens before the wState check. Real return type unsigned char (bare mov al,1/
// xor al,al, no EAX-wide clear).
unsigned char MenuNodeObj0x477568::TickAdvanceFrame()
{
    if (bValid == true) {
        nTickCounter++;
        if (wState == 1) {
            if (pIconDesc->nButtonFrameCount >= 4) {
                if (nTickCounter >= 3) {
                    int newFrame;
                    if ((unsigned)nFrameIndex < pIconDesc->nButtonFrameCount - 3u) {
                        newFrame = nFrameIndex + 1;
                    } else {
                        newFrame = 0;
                    }
                    if (nFrameIndex != newFrame) {
                        SetFrameAndNotify(newFrame);
                    }
                }
                return 1;
            }
        }
    }
    return 0;
}

#include "LocoBitmap.h" // end-of-file include -- see CLAUDE.md's #line-provenance lesson,
                              // keeps this matched TU's earlier EXACT functions undisturbed
#include "WorldBoardMaybe.h"

// FUNCTION: LOCO 0x40d340
// EFFECTIVE MATCH -- PARKED (asmscore.py --len 290: total 97554, align=94 reg_pen=30
// byte_diff=84, insns 97/96; see docs/PARKED.md). pOwner is cached in a local (pOwnerLocal)
// for the viewport-check + blit block only -- the carousel-scroll read above and the final
// OffsetRect below both re-read this->pOwner fresh, matching the original's own per-block
// reload pattern (distinct from SetNodeState's mode-4 case, which re-reads
// pOwner->nCarouselScrollIndex independently for both left/right -- here a single dword read
// is cached and reused, per the decompile's own dVar1 local). Positions the node at its own
// rect (shifted by nCarouselScrollIndex*-0x39 in carousel mode), clip-tests the result
// against the owner's rectViewport, and if visible blits pIconDesc's shadow bitmap (source,
// using rectLocal as its sprite-sheet sub-rect) onto the owner's own realized canvas bitmap
// (pKindDesc->pOwnedObjA, dest) via LocoBitmap::BlitOntoBitmap, then dirty-marks the placed
// rect (offset back to board-space by the owner's own rect origin) via
// WorldBoardMaybe::MarkRectDirty. Residual: the original loads `this->rect`'s 4 fields via a
// single `lea`-computed address held in one register (edx), dereferenced 4 times
// (`[edx]`/`[edx+4]`/`[edx+8]`/`[edx+0xc]`), then keeps all 4 field values purely in
// registers (eax/ebx/ecx/ebp) through the whole carousel-adjust branch, only spilling to
// localRect's real stack slot at the post-branch join point (right before pOwner gets
// re-read for the viewport fields). 5 source forms were tried for the initial rect-field
// load -- plain struct-copy assignment (`RECT localRect = rect;`, this one, the closest),
// field-by-field assignment, an explicit `RECT *pRect = &rect;` pointer (the compiler folds
// the redundant address computation away identically to the plain-field-read form, refuting
// the address-of hypothesis), an explicit `CopyRect(&localRect, &rect)` call, and hoisting
// the 4 rect fields into standalone int locals read via `this->` (matches the original's
// register-only residency THROUGH the branch, but loses the single-address-then-4-derefs
// load shape at the very top, netting a WORSE total structural diff, 229 vs 116) -- none
// reproduce the specific "cache the field address once, dereference 4 times, defer the
// stack spill past an unrelated branch" idiom; every later register-role swap (eax<->edx
// throughout the viewport check, plus 3 downstream `cmp`/`jcc` operand-order flips) cascades
// from this one unresolved root, same "intrinsic once the root cause isn't source-steerable"
// class as SetNodeState just above.
void MenuNodeObj0x477568::Draw()
{
    if (bValid == true && bVisible != 0) {
        RECT localRect = rect;
        if (wModeFlagsMaybe & 2) {
            int nScroll = pOwner->nCarouselScrollIndex;
            localRect.left += nScroll * -0x39;
            localRect.right += nScroll * -0x39;
        }
        WidgetBaseObj0x4784c8 *pOwnerLocal = pOwner;
        int nViewportLeft = pOwnerLocal->rectViewport.left;
        if (nViewportLeft <= localRect.left + nViewportLeft &&
            pOwnerLocal->rectViewport.top <= localRect.top &&
            localRect.right + nViewportLeft <= pOwnerLocal->rectViewport.right &&
            localRect.bottom <= pOwnerLocal->rectViewport.bottom) {
            RECT destRect;
            destRect.top = localRect.top;
            destRect.left = localRect.left + nViewportLeft;
            destRect.right = localRect.right + nViewportLeft;
            destRect.bottom = localRect.bottom;
            pIconDesc->pShadowBitmap->BlitOntoBitmap(destRect, pOwnerLocal->pKindDesc->pOwnedObjA,
                                                            rectLocal, 1);
            OffsetRect(&localRect, pOwner->rect.left, pOwner->rect.top);
            g_worldBoard.MarkRectDirty(localRect);
        }
    }
}

// FUNCTION: LOCO 0x40d470
// The grid-placement half of InitOwnerAndDescMaybe, run once per freshly created icon node by
// the tool-menu populate loops. pIconDesc's +0x2e/+0x30 arrive in GRID UNITS (column/row, both
// < 4 -- that bound IS the guard, and it doubles as an idempotence latch: once converted the
// row is >= 0x39-ish, so a second call is a no-op) and are converted to PIXELS IN PLACE with
// the menu's 0x39-pixel cell pitch and its own origin offsets (col*0x39 - 0x32, row*0x39 -
// 0x28). The node's inherited base rect is then rebuilt around them at the descriptor's
// shadow-frame size -- the identical SetRect InitOwnerAndDescMaybe (0x40d0b0) does, which is
// why the descriptor is re-read rather than cached: the original reloads pIconDesc from +0x44
// three separate times (once per statement), per Yoda lesson #19.
// The carousel tail is InitOwnerAndDescMaybe's own block verbatim, and note it re-reads the
// column AFTER the conversion above, so it divides the freshly written PIXEL value back down
// by the same 0x39 pitch to recover the column index.
// sic: the two multiplies are 16-bit (`imul cx,cx,0x39`) while the subtracts that follow are
// 32-bit -- ordinary short-arithmetic narrowing, reproduced by keeping the fields `short`.
void MenuNodeObj0x477568::PlaceIconInGridMaybe()
{
    if (pIconDesc->field_0x30Maybe < 4) {
        pIconDesc->field_0x2eMaybe = pIconDesc->field_0x2eMaybe * 0x39 - 0x32;
        pIconDesc->field_0x30Maybe = pIconDesc->field_0x30Maybe * 0x39 - 0x28;
        SetRect(&rect, pIconDesc->field_0x2eMaybe, pIconDesc->field_0x30Maybe,
                pIconDesc->wShadowFrameWidth + pIconDesc->field_0x2eMaybe,
                pIconDesc->wShadowBitmapHeight + pIconDesc->field_0x30Maybe);
        if (wModeFlagsMaybe & 2) {
            int nMinWidth = pIconDesc->field_0x2eMaybe / 0x39 - 2;
            if (pOwner->nCarouselMaxIndex < nMinWidth) {
                pOwner->nCarouselMaxIndex = nMinWidth;
            }
        }
    }
}

extern bool g_bLabelBlinkToggle; // DAT_00485264, see set_global's own plate comment

// FUNCTION: LOCO 0x449100 // EFFECTIVE MATCH -- 138 B vs 136, insns 59/57, byte_diff 13
// Structure, both strlen intrinsics, every constant and both call sites agree. The residual is a
// PROLOGUE SHRINK-WRAP difference: the original pushes only esi, runs the bTextRedrawEnabled
// gate, and pops esi and returns on the not-editable path, pushing edi/ebx only afterwards for
// the body proper; this compile pushes all three callee-saved registers up front and pops all
// three on the early out. Two instructions, plus one register coin-flip on the append store
// (`mov eax,[esi+0x60]` vs `mov edx,...`).
// Two levers ARE baked in: the rejected-edit paths must be EARLY RETURNS with their own copy of
// the sound call (writing the accepted path as `if (ok) { ...; Draw(); }` and falling through to
// a single shared sound call leaves TWO Draw call sites -- VC5 cross-jumps the three sound tails
// but not the two Draw calls, insns 62/57), and the length guard must read `nLen >= nTextLen` in
// that operand order (the original is `cmp ecx,[esi+0x5c] / jae`; writing `nTextLen > nLen`
// swaps the operands to `cmp [esi+0x5c],ecx / jbe`).
//
// In-place single-key editor for this node's own label -- the path that lets the user type a
// savegame name straight into a list slot. bTextRedrawEnabled doubles as the "this label is
// editable" gate, so a non-editable node consumes nothing and an editable one consumes
// everything.
//
// sic: the click/feedback sound fires on EVERY key the node consumes, not only on the rejected
// ones -- the accepted-edit path calls Draw() and then falls straight into the same
// PlaySoundAtScreenPos, which is why there is only one call site for each in the original.
bool UiIconListItem::HandleTextEditKey(unsigned int nKey) {
    if (!bTextRedrawEnabled) {
        return 0;
    }
    if (nKey == VK_BACK || nKey == VK_DELETE) {
        unsigned int nLen = strlen(pText);
        if (nLen == 0) {
            g_UIResources.PlaySoundAtScreenPos(0x5460, rect.left, rect.top, 4);
            return 1;
        }
        pText[nLen - 1] = '\0';
    } else {
        unsigned int nLen = strlen(pText);
        if (nKey < 0x20 || nKey > 0x7e) {
            return 1;
        }
        if (nLen >= (unsigned int)nTextLen) {
            g_UIResources.PlaySoundAtScreenPos(0x5460, rect.left, rect.top, 4);
            return 1;
        }
        pText[nLen] = (char)nKey;
        pText[nLen + 1] = '\0';
    }
    Draw();
    g_UIResources.PlaySoundAtScreenPos(0x5460, rect.left, rect.top, 4);
    return 1;
}

// FUNCTION: LOCO 0x449190
// EXACT MATCH. Real return type unsigned char (bare mov al,0/1, no EAX-wide clear --
// CLAUDE.md's bool-return lesson): 0 if !bVisible, else always 1. The `!bVisible` guard
// compiles a plain truthy test (test al,al) while the bTextRedrawEnabled check below compiles
// a literal `cmp byte,1` (matching the established bValid/bVisible `== true` idiom) -- both
// shapes appear in the SAME function, mirrored here exactly from the disasm. Calls the base
// override first; if it did NOT auto-advance a sprite frame (returned 0) and
// bTextRedrawEnabled and nTickCounter is a multiple of 3, also redraws (virtual Draw(), slot
// 8) -- an extra per-tick label-only refresh independent of the base's own frame cadence.
unsigned char UiIconListItem::TickAdvanceFrame()
{
    if (!bVisible) {
        return 0;
    }
    unsigned char bAdvanced = MenuNodeObj0x477568::TickAdvanceFrame();
    if (bAdvanced == 0 && bTextRedrawEnabled == true && nTickCounter % 3 == 0) {
        Draw();
    }
    return 1;
}

// FUNCTION: LOCO 0x4491d0
// EXACT MATCH. Draws the base icon (base Draw()) then the text label. Guards on
// pOwner/pOwner->pKindDesc/bVisible/pOwnedObjA->pSurface all non-null (single flat
// short-circuit &&, matching the base Draw's own bValid&&bVisible shape). Allocates a
// (nTextLen+2)-byte scratch buffer (freed via a plain scalar `delete`, matching the ctor's own
// `new char[]`/scalar-`delete` mismatch -- no array cookie for a trivial-dtor POD array under
// this compiler). destRect is a whole-struct copy of this->rect with `.left += 4` applied
// after (matching base Draw's own `RECT localRect = rect;` idiom, NOT a field-by-field
// build -- tried both, only the struct-copy form byte-matches), shifted up 2px via OffsetRect
// only when wState==2. The color select needed literal `if (wState==3) {...} else {...}`
// branches around each SetTextColor call, not a ternary -- CLAUDE.md's "boolean value passed
// as a call argument" lesson; the ternary form compiled a branchless sbb/and/add sequence the
// original doesn't have (this was the function's only real structural residual). GetDC's the
// canvas surface (pOwnedObjA->pSurface, a real IDirectDrawSurface -- no devirtualization
// concern, this is a genuine external COM interface); on success, sets up transparent-bkmode
// GDI text state (silver 0x808080 when wState==3, else near-black 0x202020) and DrawTextA's
// either pText directly, or (in the blink-cursor branch) `pText + "_"` formatted into the
// scratch buffer -- toggling the shared g_bLabelBlinkToggle each call, gated on this node's
// owner being the currently-active tab widget AND bTextRedrawEnabled. Restores the prior
// bkmode/color/font and ReleaseDC's before freeing the scratch buffer (freed on every path
// once allocated, even a GetDC failure).
void UiIconListItem::Draw()
{
    if (pOwner != 0 && pOwner->pKindDesc != 0 && bVisible != 0 &&
        pOwner->pKindDesc->pOwnedObjA->pSurface != 0) {
        char *pBuf = new char[nTextLen + 2];
        if (pBuf != 0) {
            MenuNodeObj0x477568::Draw();
            RECT destRect = rect;
            destRect.left += 4;
            if (wState == 2) {
                OffsetRect(&destRect, 0, -2);
            }
            IDirectDrawSurface *pSurface = pOwner->pKindDesc->pOwnedObjA->pSurface;
            HDC hdc;
            if (pSurface->GetDC(&hdc) == 0) {
                int oldBkMode = SetBkMode(hdc, TRANSPARENT);
                COLORREF oldColor;
                if (wState == 3) {
                    oldColor = SetTextColor(hdc, 0x808080);
                } else {
                    oldColor = SetTextColor(hdc, 0x202020);
                }
                HGDIOBJ oldFont = SelectObject(hdc, (HGDIOBJ)hFontLabel);
                if (g_bLabelBlinkToggle == false || bTextRedrawEnabled == false ||
                    pOwner != g_pActiveTabWidgetMaybe) {
                    DrawTextA(hdc, pText, -1, &destRect, DT_SINGLELINE | DT_VCENTER);
                    g_bLabelBlinkToggle = true;
                } else {
                    sprintf(pBuf, "%s_", pText);
                    DrawTextA(hdc, pBuf, -1, &destRect, DT_SINGLELINE | DT_VCENTER);
                    g_bLabelBlinkToggle = false;
                }
                SetBkMode(hdc, oldBkMode);
                SetTextColor(hdc, oldColor);
                SelectObject(hdc, oldFont);
                pOwner->pKindDesc->pOwnedObjA->pSurface->ReleaseDC(hdc);
            }
            delete pBuf;
        }
    }
}
