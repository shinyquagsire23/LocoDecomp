// WorldBoardMaybe (WorldBoardPartial) method bodies -- see WorldBoardMaybe.h for the
// canonical shared partial view of the singleton.
#include "WorldBoardMaybe.h"
#include "TrackGraph.h"   // the ctor's `new TrackGraph(7/8)` pair (0x454cf0)
// g_BuildToolButton -- the per-dirty-rect toolbar blit. ⚠ PRICE, measured v566 from a clean
// baseline: this include costs 1162 B / 2 funcs, ALL of it inside this TU --
// FindNearestObjOfCategoryMaybe (0x457ce0) loses its 951 B EXACT to DIFF(16) and
// WorldBoardMaybe_ResetAllTilesMaybe (0x454fe0) its 211 B to DIFF(2). Paid because the
// alternative was a view struct calling a symbol that exists nowhere (see the retirement note
// further down), which is a live defect, not a style question.
//
// ⭐ The 951 B half is RECOVERABLE and the lever is cheap: ONE additional file-scope
// declaration anywhere in this TU brings 0x457ce0 straight back to MATCH (probed v566 with a
// throwaway `extern int`; two extra declarations behave the same, so it is a threshold, not the
// parity v490 inferred). It was left unspent only because no HONEST declaration was owed here
// at the time -- the next real global or free function this file needs to declare pays for
// itself twice. 0x454fe0's 211 B did not respond to the same lever.
//
// ⚠ Do NOT try folding the TU-local `Ddraw_BltUpdateRect` extern below onto DDrawSurface.h at
// the same time: probed v566, it lands on the WRONG side and 0x457ce0 stays DIFF(16).
#include "BuildToolButton.h"
#include "DecorActor.h"   // PlacementCursorMaybe::pHoverObjMaybe's pointee -- this TU reads its
                          // `rect` through the pointer, so the forward declaration in
                          // src/PlacementCursorMaybe.h is not enough here (2026-07-26)
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: Port_Tracef dirty-tile diagnostics
#endif

// FUNCTION: LOCO 0x456140
// Clears bFlag0x3cMaybe (+0x3c), ignores both stack params, returns false (RET 0x8 --
// two stack params consumed but never read). Only callers: PlacementCursorMaybe::
// TickMaybe's pending-placement commit paths (0x41095d/0x4109bc), invoked on the
// DAT_004aad08 singleton. Moved out of src/phase2_probe2.cpp 2026-07-22 (v322, was the
// probe-local FlagObj0x456140::ResetFlag0x3c).
bool WorldBoardPartial::ResetFlag0x3c(int, int) {  // TODO: sync
    bFlag0x3cMaybe = false;
    return false;
}

// 0x45cdf0 -- the "thiscall init that zeroes 3 dwords then sets a trailing short" this TU used
// to carry as a TU-local ShortInitObj0x45cdf0 placeholder is the TrackGraph CONSTRUCTOR, and now
// lives in src/TrackGraph.cpp. The owner evidence was right (both callers, 0x454d38/0x454d6d,
// sit in WorldBoardMaybe_InitFieldsMaybe) but the conclusion was not: those two call sites are
// `pTrackGraphAMaybe = new TrackGraph(7)` and `pTrackGraphBMaybe = new TrackGraph(8)`, not an
// embedded sub-object -- each is preceded by its own `push 0x2c; call operator new`.

// --- 0x456150: UpdateDirtyTiles (dirty-tile-rect coalescing + DDraw blit) ---
// ⭐ v552: the canonical SelectedObjWidgetMaybe and WorldActionCursor classes and both of their
// singletons. FIVE TU-local views of those two objects were folded onto them and deleted here:
// the DATA view SelectedObjWidgetView0x456150 (a pad0x0[0x88]/bActive/pad/animRectMaybe flat
// model -- a genuine duplicate definition of a class that already existed), the methods-only
// SelectedObjWidgetPaintView0x456700 and SelectedObjWidgetClickView0x455d60, and
// WidgetObjPaintView0x456700 + DecorSelectWidgetView0x455d60 over g_worldActionCursor. Every
// cast-through-a-view call site became an ordinary member call on the global object, and
// `.animRectMaybe` became `.animDescMaybe.rect` -- the same address by construction
// (animDescMaybe is at +0xe4 and RectFlagObj0x477820::rect at +0x8, so +0xec, exactly what the
// view's own comment claimed).
//
// ⚠ PRICE, measured v552 and CURRENT: 211 B, all of it 0x454fe0's known parity coin flip (see
// its own note). NOTHING else in this TU and no other TU in the repo moves -- in particular
// 0x457ce0's knife-edged 951 B SURVIVES the fold, which is what v551 feared and why this was
// deferred. Paid deliberately: a duplicate model is the one debt this project does not carry.
// ⭐ v564: the DecorObjMgr and BigObjTrackingSets paint views are GONE too, folded onto the real
// DecorObjMgrMaybe / EffectSpawner (see the two includes below), and the price was ZERO -- this
// TU byte-identical, 0x457ce0's 951 B intact, every other row in the repo unchanged.
// ⚠ That REFUTES this line's own v552 predecessor, which measured the DecorObjMgr fold at
// 0x457ce0's full 951 B and left a "REFUTED, do NOT re-run" warning here. The toll had simply
// EXPIRED -- v560's corollary, now three for three (v560's RectFlagObj bodies, v563's 0x405a50,
// this). Re-measure a priced-and-withheld verdict before believing it; it cost one compile.
// They were also not merely cosmetic debt: a view spelling mangles under the VIEW's class name,
// so all four calls resolved to a symbol defined nowhere -- in the port, `xor eax,eax; ret N`
// stubs, i.e. no decor actors and no effects painted on the world board at all. CODEGEN #184.
// Only BuildToolButtonPaintView0x456700 is left, and only because 0x49c00's body is untranscribed.
#include "WorldActionCursor.h"
#include "PlacementCursorMaybe.h"
#include "PeerTrainSlotQueueMaybe.h"
#include "AppWindow.h"
#include "LocoBitmap.h"
#include "EffectSpawner.h" // DAT_004fd220 + the three-way effect paint family
#include "DecorObjMgrMaybe.h" // DecorObjMgrMaybe_00485448 + BlitActorsInRectMaybe
#include <string.h>

// TU-local methods-only views for UpdateDirtyTiles' untranscribed sibling callees. Declaring
// these parameterized method decls in the SHARED headers (WorldBoardMaybe.h /
// PeerTrainSlotQueueMaybe.h) rotates DPlaySessionMgr.cpp's /Og state and breaks
// SelectGridCellFromPointMaybe's EXACT (bisect-proven v334 -- the v333 CursorDesc.h lesson
// again: parameterized method decls in a shared header are load-bearing for that TU).
struct WorldBoardDirtyTileView0x456150 : WorldBoardPartial {
    void FUN_00456700(RECT rect);       // 0x456700 -- paint one flushed rect (by-value RECT)
    char FUN_00456c60(void *pListHead); // 0x456c60 -- dirty-rect-list coalesce step
    void FUN_00456d10(void *pListHead); // 0x456d10 -- coalesce finalize
};
// (PeerTrainSlotQueueView0x456150 retired v564: src/PeerTrainSlotQueueMaybe.h's real class
// already declares CollectAndSortVisibleCarsMaybe, so the derived view was a pure NAME-HIDING
// shadow -- and because the shadow mangles under the VIEW's class name, the call below reached a
// symbol defined nowhere. In the port it was the single hottest stub left after the collection
// forwarders, once per frame: no train car was ever collected or sorted for drawing.
// ⚠ lint_desync.py's VIEW check does NOT catch this shape -- the view is reached by CASTING an
// existing global, not by declaring that global a second time under the view's type, so the
// "some global is declared under >=2 class types" precondition never fires. See CODEGEN #187.)

// Per-TU extern decls (kept in sync with their canonical homes; GameNet.h for
// g_nScreenState, LocoBitmap.cpp/AlbumCardWnd.cpp for g_pDDrawWorkSurface, AlbumCardWnd.cpp
// for g_dwScreenWidth, PopupWndBase.cpp for g_bBoardScrollFlag/g_rectAppClientBounds).
extern int g_nScreenState;                              // app-state dword (3/4 = in-game modes)
// The two in-game app-state gates. The `unsigned char` return type is LOAD-BEARING: it is what
// reproduces the original's sete-materialized branch (`xor r,r; cmp; sete rl; test rl,rl`)
// instead of a plain `cmp; je`. Cracked in v356 -- see docs/CODEGEN.md and the UpdateDirtyTiles
// autopsy below. Kept TU-local (not hoisted into a shared header) because adding declarations
// to a shared header rotates other TUs' codegen (v340/v355/v356).
inline unsigned char IsInGameModeMaybe() { return g_nScreenState == 3; }
inline unsigned char IsInGameAltModeMaybe() { return g_nScreenState == 4; }

// ⚠ 0x456700's own gate at 0x456890 is the SAME sete shape in the original, but routing it
// through IsInGameModeMaybe() nets that function WORSE (478854 -> 488419) even though the gate
// itself is what the original does -- the local win is swamped by register-allocation fallout
// elsewhere in a function that is still 470k from matching. It is therefore deliberately left
// written inline there; revisit once the rest of 0x456700 is closer.
extern char g_bAppMinimizedMaybe;                             // sibling state byte; ==1 suppresses the update
extern unsigned char g_bBoardScrollFlag;              // DAT_00485210
extern unsigned int g_dwScreenWidth;                  // DAT_004851d8
extern unsigned int g_dwScreenHeight;                 // DAT_00485214
extern RECT g_rectAppClientBounds;                   // 0x485220 (tagRECT_00485220) -- the
                                                      // scroll-extent rect offset by dwScrollX/Y
extern IDirectDrawSurface *g_pDDrawWorkSurface;       // DAT_004fd3c4


// (The placement cursor itself is the real class now -- see the PlacementCursorMaybe.h
// include above; its old TU-local views were retired 2026-07-25.)

// The backdrop descriptor (g_pBackdropDesc) -- the real CursorDesc, already fully modeled in
// src/CursorDesc.h (visible here transitively via DecorActor.h -> TilePlacedObj.h). It is
// new_alloc(0x168)'d and CursorDesc-ctor'd by ReloadActiveSaveState (0x429f10) from the
// path "backdrop\<savename>", and released by SaveWindowAndCleanExit (0x4077a0). Only its
// realized bitmap (pOwnedObjA, +0x10 -- the board back-buffer) is read here.
extern CursorDesc *g_pBackdropDesc; // DAT_004fd3c8

// Ddraw::Ddraw_BltUpdateRect, 0x401280 (same decl as src/WindowBase.cpp).
extern void Ddraw_BltUpdateRect(RECT *pRect, HWND hwnd, POINT *pScrollOffset, char bWaitMaybe);

// 0x47f108 -- the packed dirty-bitmap's bit-mask table, MSB FIRST: entry i is 0x80 >> i, so bit
// index N lives in byte N>>3 under mask 0x80>>(N&7). Every reader and writer of
// pDirtyBitmapMaybe indexes it (MarkRectDirty, WorldBoardMaybe_PlaceObjectMaybe,
// FlushQueuedBigObjMaybe, UpdateDirtyTiles, FUN_00456700 -- all five xrefs are in this TU,
// which is why the definition belongs here). Non-const because the original's copy sits in
// .data (0x47e000..0x485200), not .rdata.
//
// ⭐ This was the whole reason the port never rendered a board (v566). Until it was DEFINED it
// was one of link/gen_stubs.py's generated DATA stubs, and those live in a .bss mirror -- all
// zeros. So `bitmap[N>>3] |= 0` marked nothing and `0 & bitmap[N>>3]` found nothing: the board
// scanned all 64x48 tiles every frame, coalesced zero dirty rects, and blitted nothing, for
// ~1850 consecutive frames. The earlier comment here even recorded the mask as `1<<(i&7)`,
// which is the wrong direction -- neither error is visible to a compiler or to any lint.
unsigned char DAT_0047f108[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

// The 0x14-byte dirty-rect list node accumulated by UpdateDirtyTiles: a RECT plus a
// singly-linked next pointer at +0x10. Nodes are operator-new'd raw (no ctor) and freed
// with operator delete after blitting.
struct DirtyRectNode0x456150 {
    RECT rect;
    DirtyRectNode0x456150 *pNext;
};

// 0x454fa0 (Ghidra: Ddraw::Ddraw_RecenterViewportOffsetMaybe)
// Re-derives just the viewport CENTRE, which is Ddraw_InitTileGridExtent's own centre
// calculation below plus the current scroll offset. That is the whole difference between the
// two: 0x454e60 zeroes the scroll and recentres on the app client rect (the full re-derive after
// a resize), this one recentres WITHOUT disturbing the scroll, so it is what the scrollers and
// the display-mode switch call after moving the view.
//
// ⭐ v577 FOLDED THE THREE SPELLINGS INTO ONE MEMBER, and the note that used to sit here (that
// the TU-local view struct was "LOAD-BEARING") described a real measurement of the wrong
// question. The declaration-count effect is real -- as a free __fastcall function this body
// costs 0x457ce0 (FindNearestObjOfCategoryMaybe) its 951-byte EXACT, and as a one-method view it
// does not -- but BOTH of those spellings left the address with three names across three TUs
// (this one, src/AppWindow.cpp's WorldBoardDisplayModeView0x407d20, src/Main.cpp's
// WorldBoardWndProcView0x4618c0), only ONE of which was ever defined. The other two TUs' five
// call sites therefore targeted a symbol no TU defines: byte-invisible, and a live gen_stubs
// stub in the port. It is now an ordinary member of WorldBoardPartial, which is what __thiscall
// on the board means. Priced at -484 B / -1 func and landed anyway; see the declaration's own
// note in src/WorldBoardMaybe.h.
// FUNCTION: LOCO 0x454fa0
void WorldBoardPartial::Ddraw_RecenterViewportOffsetMaybe() // TODO: sync (Ghidra ns is Ddraw;
                                                        // really __thiscall on the board)
{
    dwViewportCenterXMaybe =
        (g_rectAppClientBounds.right - g_rectAppClientBounds.left) / 2 + dwScrollX +
        g_rectAppClientBounds.left;
    dwViewportCenterYMaybe =
        (g_rectAppClientBounds.bottom - g_rectAppClientBounds.top) / 2 + dwScrollY +
        g_rectAppClientBounds.top;
}

// FUNCTION: LOCO 0x454e60
// (Re)derives the board's tile-grid extent from the live screen size (clamped to
// [1024,1280]x[768,1024], default 1024x768 when bFlag!=0 or the width is out of range),
// zeroes rcViewport's origin + both scroll offsets, and re-derives every dependent field in
// one pass: half extents, viewport center (midpoint of the app client rect), wCols/wRows
// (extent/16), then reallocs the packed dirty bitmap (1 bit per tile, 0xff = all-dirty) to
// match. Called once from SplashWnd::OnEnterCommitAndDispatch (0x422660) with the board
// singleton and NetSettingsMaybe's config byte. Ghidra: Ddraw::Ddraw_InitTileGridExtent.
// (Placed after the <string.h> include above -- this TU's mid-file include pattern.)
// EFFECTIVE MATCH (315 B == 315 B, DIFF(4)). Every instruction pairs; the whole residual is
// ONE commutative-load scheduling coin-flip, twice: the original loads wCols (+0x3e) before
// wRows (+0x40) in the two `wCols * wRows` movsx pairs (alloc size, memset count), this build
// loads them the other way round. Operand-order flip (`wRows * wCols`) is BYTE-IDENTICAL --
// cl canonicalizes the commutative imul's load schedule, same class as 0x405ab0's
// add-accumulator coin-flip (v509). See docs/PARKED.md.
void WorldBoardPartial::Ddraw_InitTileGridExtent(char bFlag) {  // TODO: sync (Ghidra ns is Ddraw)
    if (bFlag == 0) {
        if ((int)g_dwScreenWidth < 0x400) {
            dwViewportWidth = 0x400;
            dwViewportHeightMaybe = 0x300;
        } else if ((int)g_dwScreenWidth > 0x500) {
            dwViewportWidth = 0x500;
            dwViewportHeightMaybe = 0x400;
        } else {
            dwViewportWidth = g_dwScreenWidth;
            dwViewportHeightMaybe = g_dwScreenHeight;
        }
    } else {
        dwViewportWidth = 0x400;
        dwViewportHeightMaybe = 0x300;
    }
    rcViewport.left = 0;
    rcViewport.top = 0;
    dwHalfWidth = dwViewportWidth / 2;
    rcViewport.right = dwViewportWidth;
    rcViewport.bottom = dwViewportHeightMaybe;
    dwScrollX = 0;
    dwScrollY = 0;
    dwHalfHeight = dwViewportHeightMaybe / 2;
    dwViewportCenterXMaybe =
        (g_rectAppClientBounds.right - g_rectAppClientBounds.left) / 2 +
        g_rectAppClientBounds.left;
    dwViewportCenterYMaybe =
        (g_rectAppClientBounds.bottom - g_rectAppClientBounds.top) / 2 +
        g_rectAppClientBounds.top;
    wCols = (short)(dwViewportWidth / 16);
    wRows = (short)(dwViewportHeightMaybe / 16);
    if (pDirtyBitmapMaybe != NULL) {
        ::operator delete(pDirtyBitmapMaybe);
        pDirtyBitmapMaybe = NULL;
    }
    pDirtyBitmapMaybe = (unsigned char *)::operator new(wCols * wRows / 8 + 1);
    if (pDirtyBitmapMaybe != NULL) {
        memset(pDirtyBitmapMaybe, 0xff, wCols * wRows / 8 + 1);  // idiom-exempt: runtime length
    }
}

// FUNCTION: LOCO 0x456150
// PARKED (v334): EFFECTIVE -- structure fully transcribed and instruction-aligned
// (asmscore.py --len 1448: insns 465/471, align=416, byte_diff=464); the whole residual is
// FOUR stacked documented intrinsic /Og coin-flip classes, no source lever found:
//  (1) FIXED in v356 by the byte-predicate lever (total 509762 -> 424323): the leading
//      `(g_nScreenState == 3 || == 4)` materializes via xor/sete/test TWICE in the
//      original (the CLAUDE.md "prologue sete-materialization" class, proven not
//      source-steerable at 0x4393d0) -- ours emits plain cmp/branch, 6 insns shorter, which
//      cascades EVERY downstream jump displacement (why raw DIFF ~1170 >> real gap);
//  (2) `this`->edi vs ebp prologue allocation swap (0x45ca10 symmetric-register-swap class);
//  (3) the inner tile loop's induction variables: the original stacks `col` ([esp+0x24]) and
//      keeps xPix in ebp (frame 0x44), this compile keeps `col` in ebp and xPix in edi
//      (frame 0x40) -- the one-slot frame delta shifts every [esp+0x58] param-slot reference
//      by 4;
//  (4) the zero-register class: the original dedicates ebp=0 from the blit loop onward
//      (cmp x,ebp / push ebp for Lock's NULL args / `or reg,-1` vs imm-store on the -1
//      ternary arms), this compile uses test/literal-0 forms.
// Real levers that DID matter (kept): scroll-path temps written as if/else direct-store
// assignments (imm-store arms), the SelectedObj clamp expressions duplicated textually (the
// original recomputes the ternary for the store), the ((x <= 0) - 1) & x branchless
// clamp-to-zero, the `if (!bFirst) UnionRect else copy` inverted layout, the dirty-bit test
// as `(mask & bitmap) != 0` with the extend block first, and g_worldBoard (not this->) for
// the wCols/pDirtyBitmap reads inside the tile loop. See docs/PARKED.md.
// Per-frame dirty-tile flush: walks the packed dirty bitmap (pDirtyBitmapMaybe,
// 1 bit per tile), coalesces each row's dirty runs into one accumulated RECT, appends
// every finished run to a linked list of DirtyRectNode0x456150s, back-buffer-bilts each
// rect through the board LocoBitmap, adds the placement cursor (+hover ghost) rects,
// coalesces the list (FUN_00456c60/FUN_00456d10), then for each rect: locks the shared
// work surface if needed (bSurfaceLockGuard), paints the rect (FUN_00456700), unlocks,
// BltUpdateRect's it to the front buffer, and frees the node. Finally clears the whole
// dirty bitmap and the train-slot sort scratch.
void WorldBoardPartial::UpdateDirtyTiles(char bParam)  // TODO: sync (Ghidra ns is WorldBoardMaybe)
{
    DirtyRectNode0x456150 *pHead = NULL;
    DirtyRectNode0x456150 *pTail = NULL;
    if ((IsInGameModeMaybe() || IsInGameAltModeMaybe()) && g_bAppMinimizedMaybe != 1) {
        g_PeerTrainSlotQueue.CollectAndSortVisibleCarsMaybe();
        RECT rcDirty;
        SetRectEmpty(&rcDirty);
        short colMin, colMax, rowMin, rowMax;
        if ((g_bBoardScrollFlag != 0 ||
             (int)g_worldBoard.dwViewportWidth > (int)g_dwScreenWidth) && bParam == 0) {
            // scroll path: derive the visible tile span from the scroll offset + the
            // window rect (the -1-for-negative ternaries are the original's "off the near
            // edge" marker; /Og widens the shorts' slot stores to dword where dead).
            int scrollX = this->dwScrollX;
            if (scrollX < 0) colMin = -1; else colMin = (short)(scrollX >> 4);
            if (scrollX + g_rectAppClientBounds.right < 0) colMax = -1;
            else colMax = (short)((scrollX + g_rectAppClientBounds.right) >> 4);
            int scrollY = this->dwScrollY;
            if (scrollY < 0) rowMin = -1; else rowMin = (short)(scrollY >> 4);
            if (scrollY + g_rectAppClientBounds.bottom < 0) rowMax = -1;
            else rowMax = (short)((scrollY + g_rectAppClientBounds.bottom) >> 4);
            if (colMax < (short)this->wCols) colMax++;
            if (rowMax < (short)this->wRows) rowMax++;
            if (SelectedObjWidgetMaybe_004852a0.bActive != 0) {
                // extend the flush span to cover the selected object's on-screen rect
                int v = SelectedObjWidgetMaybe_004852a0.animDescMaybe.rect.left;
                if (colMin >= (short)(v < 0 ? -1 : (v >> 4))) {
                    if (v < 0) colMin = -1; else colMin = (short)(v >> 4);
                }
                colMin = ((colMin <= 0) - 1) & colMin;
                v = SelectedObjWidgetMaybe_004852a0.animDescMaybe.rect.right;
                if (colMax <= (short)(v < 0 ? -1 : (v >> 4))) {
                    if (v < 0) colMax = -1; else colMax = (short)(v >> 4);
                }
                if (colMax >= (short)this->wCols) colMax = this->wCols;
                v = SelectedObjWidgetMaybe_004852a0.animDescMaybe.rect.top;
                if (rowMin >= (short)(v < 0 ? -1 : (v >> 4))) {
                    if (v < 0) rowMin = -1; else rowMin = (short)(v >> 4);
                }
                rowMin = ((rowMin <= 0) - 1) & rowMin;
                v = SelectedObjWidgetMaybe_004852a0.animDescMaybe.rect.bottom;
                if (rowMax <= (short)(v < 0 ? -1 : (v >> 4))) {
                    if (v < 0) rowMax = -1; else rowMax = (short)(v >> 4);
                }
                if (rowMax >= (short)this->wRows) rowMax = this->wRows;
            }
        } else {
            colMax = (short)this->wCols;
            rowMax = (short)this->wRows;
            colMin = 0;
            rowMin = 0;
        }
        bool bFirst = true;
        if (rowMin < rowMax) {
            int row = rowMin;
            int rowY = row << 4;
            int rowsLeft = rowMax - rowMin;
            do {
                if (colMin < colMax) {
                    int col = colMin;
                    int xPix = col << 4;
                    int colsLeft = colMax - col;
                    do {
                        unsigned int idx = (short)g_worldBoard.wCols * row + col;
                        if ((DAT_0047f108[idx & 7] & g_worldBoard.pDirtyBitmapMaybe[idx >> 3]) != 0) {
                            // dirty tile: extend the current run
                            RECT rcTile;
                            rcTile.left = xPix;
                            rcTile.top = rowY;
                            rcTile.right = xPix + 16;
                            rcTile.bottom = rowY + 16;
                            if (!bFirst) {
                                UnionRect(&rcDirty, &rcTile, &rcDirty);
                            } else {
                                rcDirty = rcTile;
                                bFirst = false;
                            }
                        } else {
                            // clean tile: close the current run into a list node
                            if (!bFirst) {
                                DirtyRectNode0x456150 *p =
                                    (DirtyRectNode0x456150 *)::operator new(sizeof(DirtyRectNode0x456150));
                                p->rect.left = rcDirty.left;
                                p->rect.top = rcDirty.top;
                                p->rect.right = rcDirty.right;
                                p->pNext = NULL;
                                p->rect.bottom = rcDirty.bottom;
                                if (pTail != NULL) {
                                    pTail->pNext = p;
                                } else {
                                    pHead = p;
                                }
                                pTail = p;
                                bFirst = true;
                            }
                        }
                        col++;
                        xPix += 16;
                        colsLeft--;
                    } while (colsLeft != 0);
                }
                // row end: close the row's final run
                if (!bFirst) {
                    DirtyRectNode0x456150 *p =
                        (DirtyRectNode0x456150 *)::operator new(sizeof(DirtyRectNode0x456150));
                    p->rect.left = rcDirty.left;
                    p->rect.top = rcDirty.top;
                    p->rect.right = rcDirty.right;
                    p->pNext = NULL;
                    p->rect.bottom = rcDirty.bottom;
                    if (pTail != NULL) {
                        pTail->pNext = p;
                    } else {
                        pHead = p;
                    }
                    pTail = p;
                    bFirst = true;
                }
                row++;
                rowY += 16;
                rowsLeft--;
            } while (rowsLeft != 0);
        }
        for (DirtyRectNode0x456150 *p = pHead; p != NULL; p = p->pNext) {
            g_pBackdropDesc->pOwnedObjA->RestoreOverlapBlt(p->rect, g_pDDrawWorkSurface,
                                                               p->rect, 1);
        }
        if (PlacementCursorMaybe_004854c8.bReady != 0) {
            if (PlacementCursorMaybe_004854c8.pHoverObjMaybe != NULL) {
                DirtyRectNode0x456150 *p =
                    (DirtyRectNode0x456150 *)::operator new(sizeof(DirtyRectNode0x456150));
                AnimDescRefObj0x477488 *pHover = PlacementCursorMaybe_004854c8.pHoverObjMaybe;
                p->rect.left = pHover->rect.left;
                p->rect.top = pHover->rect.top;
                p->rect.right = pHover->rect.right;
                p->pNext = NULL;
                p->rect.bottom = pHover->rect.bottom;
                if (pTail != NULL) {
                    pTail->pNext = p;
                } else {
                    pHead = p;
                }
                pTail = p;
            }
            DirtyRectNode0x456150 *p =
                (DirtyRectNode0x456150 *)::operator new(sizeof(DirtyRectNode0x456150));
            p->rect.left = PlacementCursorMaybe_004854c8.rect.left;
            p->rect.top = PlacementCursorMaybe_004854c8.rect.top;
            p->rect.right = PlacementCursorMaybe_004854c8.rect.right;
            p->pNext = NULL;
            p->rect.bottom = PlacementCursorMaybe_004854c8.rect.bottom;
            if (pTail != NULL) {
                pTail->pNext = p;
            } else {
                pHead = p;
            }
        }
        // idiom-exempt TU-local methods-only view (NOT a vtable probe): the sibling decls
        // can't live in the shared header (rotates DPlaySessionMgr.cpp, v334 bisect)
        WorldBoardDirtyTileView0x456150 *pBoard = (WorldBoardDirtyTileView0x456150 *)this;
#ifdef LOCO_PORT
        // PORT ONLY -- byte-neutral. "The gate passes" and "the gate passes and finds work"
        // are different facts, and only the second one can put pixels on the screen.
        {
            static unsigned int nTick = 0;
            int nNodes = 0;
            for (DirtyRectNode0x456150 *pN = pHead; pN != NULL; pN = pN->pNext)
                nNodes++;
            if ((++nTick % 60) == 0 || nTick <= 4)
                Port_Tracef("dirty tick=%u state=%d nodes=%d span=%d,%d..%d,%d scroll=%ld,%ld "
                            "cols=%d rows=%d bmp=%p\n",
                            nTick, (int)g_nScreenState, nNodes, (int)colMin, (int)rowMin,
                            (int)colMax, (int)rowMax, (long)this->dwScrollX,
                            (long)this->dwScrollY, (int)this->wCols, (int)this->wRows,
                            (void *)this->pDirtyBitmapMaybe);
        }
#endif
        if (pHead != NULL) {
            while (pBoard->FUN_00456c60(pHead) != 0) {
            }
            pBoard->FUN_00456d10(pHead);
        }
        while (pHead != NULL) {
            if (this->bSurfaceLockGuard == 0) {
                memset(this->aSurfaceDescScratch, 0, sizeof(this->aSurfaceDescScratch));
                this->aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
                if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)this->aSurfaceDescScratch,
                                              0, NULL) == 0) {
                    this->bSurfaceLockGuard = 1;
                }
            }
            pBoard->FUN_00456700(pHead->rect);
            if (this->bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == 0) {
                this->bSurfaceLockGuard = 0;
            }
            Ddraw_BltUpdateRect((RECT *)pHead, g_pApp->hwndOwner,
                                (POINT *)&g_worldBoard.dwScrollX, 1);
            DirtyRectNode0x456150 *pNext = pHead->pNext;
            ::operator delete(pHead);
            pHead = pNext;
        }
        if (this->pDirtyBitmapMaybe != NULL) {
            int nTiles = (short)this->wRows * (short)this->wCols;
            memset(this->pDirtyBitmapMaybe, 0, nTiles / 8 + 1); // idiom-exempt dynamic byte
        }                                                       // count: (wCols*wRows)/8+1
                                                                // packed-bitmap bytes, no
                                                                // struct field to sizeof
        g_PeerTrainSlotQueue.ClearSortScratchMaybe();
    }
}

// --- 0x456700: paint one flushed dirty rect (per-tile plane-B object draws into the
// locked work surface, then the overlay widgets' rect-invalidates) ---
// TU-local views for FUN_00456700 only -- same shared-header-rotation constraint as the
// 0x456150 views above (v334 bisect). Everything for this function is APPEND-ONLY so the
// parked UpdateDirtyTiles / EXACT ResetFlag0x3c bodies above don't rotate.

// Raw record of the tile-kind descriptor's pRawBufferMaybe array (stride 0x18); only the
// +0x17 flag byte is read here (Ghidra name bDoubleSpeedFlag).
struct TileKindRawRecView0x456700 {
    unsigned char pad0x0[0x17];
    unsigned char bDoubleSpeedFlag; // +0x17
};

// Minimal view of a placed object's kind descriptor (pObj+0x40): +0x4 is the kind id fed
// to TileKind_GetCategory, +0x20 the raw-record array indexed by nSubFrameMaybe.
struct TilePlacedObjKindDescView0x456700 {
    unsigned char pad0x0[0x4];
    unsigned int resourceId;                      // +0x4 (Ghidra name)
    unsigned char pad0x8[0x20 - 0x8];
    TileKindRawRecView0x456700 *pRawBufferMaybe;  // +0x20 (Ghidra name)
};

// Padded-vtable probe (AnimDescRefSlot7Probe precedent, src/WorldActionCursor.cpp) to
// reach TilePlacedObj's draw slots 11 (+0x2c) and 12 (+0x30) -- both take a by-value
// RECT plus two trailing ints (always 0, 0 here). Data layout from Ghidra's
// TilePlacedObj: nSubFrameMaybe (+0x28) indexes pKindDesc's raw-record array.
struct TilePlacedObjPaintView0x456700 {
    virtual void *_v00(); virtual void *_v04(); virtual void *_v08(); virtual void *_v0c();
    virtual void *_v10(); virtual void *_v14(); virtual void *_v18(); virtual void *_v1c();
    virtual void *_v20(); virtual void *_v24(); virtual void *_v28();
    virtual void DrawPlaneBSlot0x2cMaybe(RECT rect, int a, int b); // slot 11 (+0x2c)
    virtual void DrawPlaneBSlot0x30Maybe(RECT rect, int a, int b); // slot 12 (+0x30)
    unsigned char pad0x4[0x28 - 0x4];
    int nSubFrameMaybe;                           // +0x28 (Ghidra name)
    unsigned char pad0x2c[0x40 - 0x2c];
    TilePlacedObjKindDescView0x456700 *pKindDesc; // +0x40 (Ghidra name)
};

// Board tile cell (Ghidra BoardTileCellMaybe, 0x40 bytes, array at this+0x44, flat index
// col*65+row). Only plane B is touched here: 7 slots at +0x20, count byte at +0x3c.
struct BoardTileCellView0x456700 {
    unsigned char pad0x0[0x20];
    TilePlacedObjPaintView0x456700 *aPlaneBSlotsMaybe[7]; // +0x20 (Ghidra name)
    char bPlaneBCountMaybe;                               // +0x3c (Ghidra name)
    unsigned char pad0x3d[0x40 - 0x3d];
};

// This-function view of the WorldBoard singleton out to the tile-cell array (Ghidra
// aTileCellsMaybe, this+0x44, flat index col*65+row). Extending the canonical
// WorldBoardMaybe.h header out here instead would rotate DPlaySessionMgr.cpp (v334
// bisect) -- same constraint as the 0x456150 views above.
struct WorldBoardPaintView0x456700 {
    unsigned char pad0x0[0x44];
    BoardTileCellView0x456700 aTileCellsMaybe[5265]; // +0x44 (Ghidra name)
};

// Methods-only sibling views for this function's callees (none of these decls can live
// in the shared headers -- see the 0x456150 view comment above).
// (The effect-paint family is the real EffectSpawner now -- see the `#include
// "EffectSpawner.h"` above. Its old TU-local BigObjTrackingSetsPaintView0x456700 was retired
// 2026-08-01: all three paint bodies are transcribed there, and a view spelling mangles under
// the VIEW's class name, so all three calls below resolved to a symbol defined nowhere -- in
// the port, a `xor eax,eax; ret N` stub, i.e. NO effects painted on the board at all.
// CODEGEN #184. Measured free: this TU and every other row byte-identical.)
// (The toolbar blit is the real BuildToolButton now -- see the `#include "BuildToolButton.h"`
// above. Its old TU-local BuildToolButtonPaintView0x456700 was retired v566, for the same
// reason as the effect-paint family below it and by the same recipe: the view spelled the
// method `BlitAllRegionsMaybe(RECT, int)` on its OWN class, so it mangled to a symbol nothing
// defines while src/BuildToolButton.cpp's real `BuildToolButton::BlitAllRegionsMaybe(RECT,
// char)` sat right there. In the port it was a `xor eax,eax; ret N` stub -- and once v566's
// dirty-bitmap fix made the board paint at all, it became the HOTTEST stub in the run at 703
// calls, i.e. the toolbar was being skipped on every dirty rect of every frame. CODEGEN #184.)
// (The decor-actor blit is the real DecorObjMgrMaybe now -- see the `#include
// "DecorObjMgrMaybe.h"` above. Its old TU-local DecorObjMgrPaintView0x456700 was retired
// 2026-08-01.)
// TileKind_GetCategory, 0x446030 (same decl as src/Obj0x4779e0.cpp).
extern unsigned int __cdecl TileKind_GetCategory(unsigned int kindId);

// FUNCTION: LOCO 0x456700
// PARKED (v339): EFFECTIVE -- structure fully transcribed and instruction-aligned
// (asmscore.py --len 1364: insns 407/424, align=310, reg_pen=59, byte_diff=302;
// cc.sh DIFF(1058), compiled len 1299). The entire pass-loop body (slot fetch + bounds
// guard, both vtable draws, the pass switch, the category-3 branch pair incl. the
// movsx ax/WORD-spill arg temps, the double-speed slot-0x30 draw), all 6 per-tile
// widget-invalidations, and the whole 0x456b23..0x456c45 tail (both IntersectRect
// blocks, the unlock/relock pair, the 0x7c Lock scratch) are structurally faithful.
// Residual is THREE stacked documented intrinsic /Og coin-flip classes:
//  (1) the 0x4393d0 sete-materialization class on `g_nScreenState == 3` (orig
//      mov/xor/sete/test, ours cmp-mem/jne, 3 insns shorter -- shared with the
//      v334/v335/v337/v338 parks, proven not source-steerable);
//  (2) induction-var residency (the v334 (3) class): the original keeps row, col,
//      colIdx and the slot-index induction in stack slots with load-modify-store
//      updates ([esp+0x18]/[esp+0x2c]/[esp+0x30]/[esp+0x34]); ours register-allocates
//      part of the set across the col loop (frame 0x50 vs 0x54);
//  (3) count-pointer init formation: orig folds the cell+count offset as
//      `(idx+2)<<6` + this; ours `lea [idx*0x40 + this + 0x80]` -- same address,
//      formation coin-flip.
// Levers that DID matter (kept): int temps feeding the four ternary tile clamps (the
// UpdateDirtyTiles scroll-path idiom), `switch (pass)` for the 0/1 dispatch (the
// sub/dec dec-chain), SHORT iPlaneMaybe params on DrawVisibleCarsInTileMaybe/BlitActorsInRectMaybe (the
// original's `movsx ax` + WORD slot spills prove the short formals), and walking the
// cell array with a BoardTileCellView0x456700* (pCell/pCellCol) instead of index math.
// Retry only if the 0x4393d0 sete class or the induction-var residency class cracks.
// See docs/PARKED.md.
// Paints one flushed dirty-rect into the (already locked, by the caller) work surface:
// for each tile in the rect's tile span, if the tile's dirty bit is set
// (g_worldBoard.pDirtyBitmapMaybe, bit row*wCols+col) it walks the cell's plane-B object
// slots (bPlaneBCountMaybe clamped to >= 2 passes), virtual-draws each slotted object,
// and -- in app-state 3 only -- repaints the train/decor overlay helpers for passes
// 0/1, then double-speed-flagged objects get the slot-0x30 second draw. Each dirty tile
// finishes by invalidating the same tile rect on the overlay widgets (tracking sets,
// build tool button, sub-icons, selection highlight, placement cursor). Finally, if the
// rect intersects the selected object's bounds the selection widget is redrawn directly
// (unlocking/relocking the work surface around it), and the placement cursor is
// invalidated if the rect intersects its own or its hover ghost's bounds.
void WorldBoardDirtyTileView0x456150::FUN_00456700(RECT rect)  // TODO: sync
{
    BoardTileCellView0x456700 *pCells =
        ((WorldBoardPaintView0x456700 *)this)->aTileCellsMaybe; // idiom-exempt TU-local
        // data view (NOT a vtable probe): the canonical WorldBoardMaybe.h header can't
        // be extended out to the cell array without rotating DPlaySessionMgr.cpp (v334
        // bisect).
    short colMin, colMax, row, rowMax;
    int left = rect.left;
    if (left < 0) colMin = -1; else colMin = (short)(left >> 4);
    int right = rect.right;
    if (right < 0) colMax = -1; else colMax = (short)(right >> 4);
    int top = rect.top;
    if (top < 0) row = -1; else row = (short)(top >> 4);
    int bottom = rect.bottom;
    if (bottom < 0) rowMax = -1; else rowMax = (short)(bottom >> 4);
    if (row < rowMax) {
        BoardTileCellView0x456700 *pCell = &pCells[colMin * 0x41 + row];
        int yPix = row << 4;
        int rowIdx = row;
        do {
            // ⚠ `col` is re-seeded from colMin on EVERY row -- the original's row-loop back
            // edge at 0x456b1d jumps to 0x4567a4, which is exactly the `mov ecx,[esp+0x1c];
            // mov [esp+0x2c],ecx` pair that reloads it. Hoisting this out of the loop (as
            // this transcription did until v573) leaves col == colMax after the first row's
            // inner walk, so every later row fails the `col < colMax` guard and ONLY THE FIRST
            // ROW OF EACH DIRTY RECT IS EVER REPAINTED. Measured live: the paint side saw
            // tile rows 0/31/35 only -- the top row of each coalesced rect -- while
            // MarkRectDirty was setting bits across all 48 rows.
            int col = colMin;
            if (col < colMax) {
                int xPix = col << 4;
                int colIdx = col;
                BoardTileCellView0x456700 *pCellCol = pCell;
                do {
                    unsigned int dirtyIdx = (short)g_worldBoard.wCols * rowIdx + colIdx;
                    if ((DAT_0047f108[dirtyIdx & 7] & g_worldBoard.pDirtyBitmapMaybe[dirtyIdx >> 3]) != 0) {
                        RECT rcTile;
                        rcTile.left = xPix;
                        rcTile.top = yPix;
                        rcTile.right = xPix + 0x10;
                        rcTile.bottom = yPix + 0x10;
                        char count = pCellCol->bPlaneBCountMaybe;
                        if (count <= 2) count = 2;
                        char i = 0;
                        if (count >= 0) {
                            int pass = 0;
                            do {
                                TilePlacedObjPaintView0x456700 *pObj;
                                if ((short)col < 0 || (short)col > 0x51 ||
                                    (short)row < 0 || (short)row > 0x41) {
                                    pObj = NULL;
                                } else {
                                    pObj = pCellCol->aPlaneBSlotsMaybe[pass];
                                }
                                if (pObj != NULL) {
                                    pObj->DrawPlaneBSlot0x2cMaybe(rcTile, 0, 0);
                                }
                                if (g_nScreenState == 3) {
                                    switch (pass) {
                                    case 0:
                                        if (pObj != NULL &&
                                            (unsigned char)TileKind_GetCategory(
                                                pObj->pKindDesc->resourceId) == 3) {
                                            g_PeerTrainSlotQueue.DrawVisibleCarsInTileMaybe(rcTile, i);
                                            DecorObjMgrMaybe_00485448.BlitActorsInRectMaybe(i, rcTile, 0);
                                        } else {
                                            DecorObjMgrMaybe_00485448.BlitActorsInRectMaybe(i, rcTile, 0);
                                            g_PeerTrainSlotQueue.DrawVisibleCarsInTileMaybe(rcTile, i);
                                        }
                                        break;
                                    case 1:
                                        g_PeerTrainSlotQueue.DrawVisibleCarsInTileMaybe(rcTile, i);
                                        DAT_004fd220.PaintInPlaneEffectsMaybe(rcTile, 1);
                                        break;
                                    }
                                }
                                if (pObj != NULL &&
                                    pObj->pKindDesc->pRawBufferMaybe[pObj->nSubFrameMaybe]
                                        .bDoubleSpeedFlag == 1) {
                                    pObj->DrawPlaneBSlot0x30Maybe(rcTile, 0, 0);
                                }
                                i++;
                                pass++;
                            } while (i <= count);
                        }
                        DAT_004fd220.PaintSimpleEffectsMaybe(rcTile, 1);
                        DAT_004fd220.PaintOnTopEffectsMaybe(rcTile, 1);
                        g_BuildToolButton.BlitAllRegionsMaybe(rcTile, 1);
                        g_worldActionCursor.RepositionSubIconsMaybe(rcTile, 1);
                        SelectedObjWidgetMaybe_004852a0.HideChildIfBaseFlagMaybe(rcTile, 1);
                        if (SelectedObjWidgetMaybe_004852a0.bActive == 0) {
                            PlacementCursorMaybe_004854c8.FUN_00411c50(rcTile, 1);
                        }
                    }
                    pCellCol += 0x41;
                    xPix += 0x10;
                    colIdx++;
                    col++;
                } while ((short)col < colMax);
            }
            pCell += 1;
            row++;
            yPix += 0x10;
            rowIdx++;
        } while ((short)row < rowMax);
    }
    RECT rcClip;
    if (IntersectRect(&rcClip, &SelectedObjWidgetMaybe_004852a0.animDescMaybe.rect, &rect)) {
        if (SelectedObjWidgetMaybe_004852a0.bActive == 0) {
            return;
        }
        if (this->bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == 0) {
            this->bSurfaceLockGuard = 0;
        }
        SelectedObjWidgetMaybe_004852a0.BlitIconToolboxClippedMaybe(rcClip);
        SelectedObjWidgetMaybe_004852a0.BlitIconToolboxMaybe(rcClip);
        if (this->bSurfaceLockGuard == 0) {
            memset(this->aSurfaceDescScratch, 0, sizeof(this->aSurfaceDescScratch));
            this->aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
            if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)this->aSurfaceDescScratch,
                                          0, NULL) == 0) {
                this->bSurfaceLockGuard = 1;
            }
        }
    }
    if (SelectedObjWidgetMaybe_004852a0.bActive != 0 &&
        (IntersectRect(&rcClip, &PlacementCursorMaybe_004854c8.rect, &rect) != 0 ||
         (PlacementCursorMaybe_004854c8.pHoverObjMaybe != NULL &&
          IntersectRect(&rcClip, &PlacementCursorMaybe_004854c8.pHoverObjMaybe->rect,
                        &rect) != 0))) {
        PlacementCursorMaybe_004854c8.FUN_00411d10();
    }
}

// FUNCTION: LOCO 0x456c60
// One coalesce pass over UpdateDirtyTiles' dirty-rect list. For each node, walk the
// tail: inflate the candidate by 1px (so edge-touching rects count as overlapping),
// and if it intersects the anchor, deflate it again and union it into the anchor,
// unlink + operator-delete the merged node, and rescan from the same predecessor
// (a merge can expose a new overlap with the next node). Returns 1 if anything
// merged; UpdateDirtyTiles loops on it until a full pass merges nothing, then calls
// the 0x456d10 finalize. `this` is unused in the original.
char WorldBoardDirtyTileView0x456150::FUN_00456c60(void *pListHead) {  // TODO: sync
    char bMerged = 0;
    RECT rcUnion;
    for (DirtyRectNode0x456150 *pNode = (DirtyRectNode0x456150 *)pListHead; pNode != NULL;
         pNode = pNode->pNext) {
        DirtyRectNode0x456150 *pPrev = pNode;
        DirtyRectNode0x456150 *pCur = pNode->pNext;
        while (pCur != NULL) {
            InflateRect(&pCur->rect, 1, 1);
            if (IntersectRect(&rcUnion, &pNode->rect, &pCur->rect) != 0) {
                InflateRect(&pCur->rect, -1, -1);
                UnionRect(&rcUnion, &pCur->rect, &pNode->rect);
                pNode->rect = rcUnion;
                bMerged = 1;
                pPrev->pNext = pCur->pNext;
                ::operator delete(pCur);
                pCur = pPrev;
            } else {
                InflateRect(&pCur->rect, -1, -1);
            }
            pPrev = pCur;
            pCur = pCur->pNext;
        }
    }
    return bMerged;
}

// FUNCTION: LOCO 0x456d10
// The coalesce FINALIZE the sibling above hands off to: one pass clipping every surviving
// dirty-rect node to the live viewport, dropping any node that no longer intersects it at all.
// A node that survives is rewritten IN PLACE to the clipped rect (IntersectRect writes to a
// local first because it may not alias its own source operand); a node that does not is
// unlinked from its predecessor, reported, and operator-delete'd -- the same raw
// `::operator delete` the rest of this list's lifetime uses, which is why no null check
// precedes it. `this` is unused in the original, exactly as in FUN_00456c60.
//
// The clip target is the GLOBAL g_worldBoard.rcViewport (absolute 0x4aad14 = g_worldBoard +
// 0xc), not anything reached through `this` -- which is what leaves ecx dead in the body even
// though the call site loads it.
//
// ⚠ The message is "...world draw chain", not "...world draw": Ghidra's auto-label
// (s_Invalid_Rect_found_in_world_draw_0047f124) truncates at its length cap, so the literal was
// taken from the image's own bytes at 0x47f124. See CLAUDE.md's s_-label rule.
//
// EFFECTIVE MATCH -- DIFF(36) at the correct length 127, insns 49/49, and the ONLY disagreement
// is where the surviving-node branch schedules its `pNode = pNode->pNext` load: the original
// emits the four-dword rect copy first and the load last, we emit the load first. Both keep two
// copies of the node pointer live (ecx for the store base, esi for the advance); the residual is
// that one load plus the `mov eax,esi` in the delete branch sliding by the same amount. Four
// probes, none of which moved it: (1) hoisting `pPrev = pNode` above the copy, (2) field-by-field
// rect stores instead of the struct assignment (WORSE -- 125 B, DIFF(68), so the struct
// assignment is confirmed as the original's form), (3) advancing through `pPrev->pNext` so two
// distinct pointer variables straddle the store -- CSE'd straight back, (4) hoisting the RECT
// local out of the loop the way the 0x456c60 sibling does. Scheduling tie-break class; parked.
void WorldBoardDirtyTileView0x456150::FUN_00456d10(void *pListHead) {  // TODO: sync
    DirtyRectNode0x456150 *pPrev = NULL;
    DirtyRectNode0x456150 *pNode = (DirtyRectNode0x456150 *)pListHead;
    while (pNode != NULL) {
        RECT rcClipped;
        if (IntersectRect(&rcClipped, &pNode->rect, &g_worldBoard.rcViewport) != 0) {
            pNode->rect = rcClipped;
            pPrev = pNode;
            pNode = pNode->pNext;
        } else {
            OutputDebugStringA("Invalid Rect found in world draw chain");
            if (pPrev != NULL) {
                pPrev->pNext = pNode->pNext;
            }
            DirtyRectNode0x456150 *pDead = pNode;
            pNode = pNode->pNext;
            ::operator delete(pDead);
        }
    }
}

// FUNCTION: LOCO 0x455840
// Marks every board tile overlapped by a screen-space rect dirty in the packed 1-bit
// dirty bitmap (bit index row*wCols + col, mask table DAT_0047f108). Tiles are 16x16 px,
// so pixel->tile is an arithmetic shift by 4; a negative pixel coordinate yields the
// off-board sentinel -1 rather than the floor of the shift. The inclusive max corner is
// derived from the rect's exclusive right/bottom edges (hence the -1 before the shift).
// The clamps read `this`, but the inner store goes through the g_worldBoard singleton
// directly (an inlined per-tile helper in the original).
void WorldBoardPartial::MarkRectDirty(RECT rect) {  // TODO: sync (Ghidra ns is WorldBoardMaybe)
#ifdef LOCO_PORT
    { // PORT ONLY -- byte-neutral. UpdateDirtyTiles finding nothing every frame is only half a
      // diagnosis; this says whether the marks never arrive or arrive and miss the board.
        static unsigned int nMark = 0, nGated = 0;
        if (!(IsInGameAltModeMaybe() || IsInGameModeMaybe())) {
            if (++nGated <= 4 || (nGated % 1000) == 0)
                Port_Tracef("mark GATED #%u state=%d rect=%ld,%ld,%ld,%ld\n", nGated,
                            (int)g_nScreenState, (long)rect.left, (long)rect.top,
                            (long)rect.right, (long)rect.bottom);
        } else if (++nMark <= 12 || (nMark % 500) == 0) {
            Port_Tracef("mark #%u rect=%ld,%ld,%ld,%ld vp=%ld,%ld,%ld,%ld\n", nMark,
                        (long)rect.left, (long)rect.top, (long)rect.right, (long)rect.bottom,
                        (long)this->rcViewport.left, (long)this->rcViewport.top,
                        (long)this->rcViewport.right, (long)this->rcViewport.bottom);
        }
    }
#endif
    if (IsInGameAltModeMaybe() || IsInGameModeMaybe()) {
        short colMin = (rect.left < 0) ? -1 : (short)(rect.left >> 4);
        short rowMin = (rect.top < 0) ? -1 : (short)(rect.top >> 4);
        short colMax = (rect.right - 1 < 0) ? -1 : (short)((rect.right - 1) >> 4);
        short rowMax = (rect.bottom - 1 < 0) ? -1 : (short)((rect.bottom - 1) >> 4);
        if (colMin < 0) {
            colMin = 0;
        }
        if (colMax > this->wCols - 1) {
            colMax = this->wCols - 1;
        }
        if (rowMin < 0) {
            rowMin = 0;
        }
        if (rowMax > this->wRows - 1) {
            rowMax = this->wRows - 1;
        }
        for (short row = rowMin; row <= rowMax; row++) {
            for (short col = colMin; col <= colMax; col++) {
                unsigned int bit = g_worldBoard.wCols * row + col;
                g_worldBoard.pDirtyBitmapMaybe[bit >> 3] |= DAT_0047f108[bit & 7];
            }
        }
    }
}

// RETIRED v577: LocoBitmapCaptureView0x457080 was a TU-local methods-only view of the bitmap
// target, kept because "including src/LocoBitmap.h here pulls its whole member-declaration set
// into this TU" and cost FindNearestObjOfCategoryMaybe (0x457ce0) its EXACT. Both halves of that
// note had expired: this TU ALREADY includes src/LocoBitmap.h (line 78, added later for other
// reasons), so the view bought no insulation at all, and 0x457ce0 had already lost its EXACT to
// this session's Ddraw_RecenterViewportOffsetMaybe fold. Meanwhile the view was a live defect --
// CreateAndFill is a real LocoBitmap method defined in src/LocoBitmap.cpp, so this TU's call
// mangled to ...@LocoBitmapCaptureView0x457080@@ and ran a gen_stubs stub in the port, i.e. the
// save-game/minimap snapshot never actually created its bitmap.

// The same guarded plane-A slot lookup as GetBoardPlaneASlotMaybe below, but `this`-relative:
// CaptureBoardToBitmap's original inlines it through the enclosing method's own `this` (EDI),
// not through the g_worldBoard singleton. sic: same off-by-one `>` bound pair as 0x455620.
static inline TilePlacedObj *GetThisPlaneASlotMaybe(WorldBoardPartial *pBoard, short col,
                                                    short row) {
    if (col < 0 || col > BOARD_TILE_COLS || row < 0 || row > BOARD_TILE_ROWS) {
        return 0;
    }
    return pBoard->aTileCellsMaybe[col * BOARD_TILE_ROWS + row].aPlaneASlotsMaybe[0];
}

// FUNCTION: LOCO 0x457080
// Renders the board into pBitmap as one 8bpp pixel per tile (the minimap/layout snapshot the
// layout-bitmap network message and the save-game thumbnail both capture): creates the bitmap
// at exactly (wCols, wRows), then walks every cell's plane-A slot 0 and writes one palette
// index per tile, keyed off the placed object's kind descriptor. Bare categoryBytes map
// directly (2 -> 3, 4 -> 2, 12 -> 7, 13 -> 6, 5..11 transparent); the track family
// (categoryByte 3) switches again on the descriptor's resourceId, where the four 2x2-footprint
// kinds (0xc1e/0xc20/0xc22/0xc24 -- the points/switch pieces) only paint the NON-anchor cells
// of the footprint, keyed off the object's own pos. The two residual `wPosY + k` truth-value
// tests in the 0xc20/0xc22 arms are the original's own (`test ecx,ecx` right after the add,
// row not involved) -- dead code in practice behind the anchor guards, transcribed as-is.
// EFFECTIVE MATCH (v497, DIFF(276), compiled 632 B vs 597 B, insns 188/194 -- see
// docs/PARKED.md): instruction-for-instruction identical from the prologue through the switch
// dispatch and all four coordinate arms; the entire residual is VC5 cross-jumping TWO tail
// pairs that the original keeps duplicated (case 0xc24's tail into 0xc1e's, 0xc22's into
// 0xc20's -- each a [cmp/test; jcc; mov byte [ebx],5; jmp] suffix, ours inserted a 5-byte jmp
// and deleted the 10-byte copy, shifting everything downstream 20 B). The original's own
// copies are byte-identical apart from branch WIDTH (near in the early cases, short in the
// late ones), i.e. our compile is the BETTER-optimized one -- the v348 0x40e520 class.
// Probes run (all INERT, do not re-run): else-if vs else-{ if } vs plain two-ifs (worse,
// DIFF(406)), nested else-if (X) { if (Y) }, volatile-qualified stores, stacked
// default/0xc1f/0xc21/0xc23 labels, vc50-RTM vs SP3 (byte-identical output), +1/+2 member
// declarations on the header dial. (The LocoBitmap.h-include probe is moot as of v577 -- the
// include is already in this TU and the view struct it was weighed against is retired.)
bool WorldBoardPartial::CaptureBoardToBitmap(void *pBitmap_, unsigned char bParam) {  // TODO: sync (Ghidra ns is WorldBoardMaybe)
    LocoBitmap *pBitmap = (LocoBitmap *)pBitmap_;
    pBitmap->CreateAndFill(wCols, wRows, 0, 0, bParam);
    unsigned char *pOut = pBitmap->pPixels;
    unsigned short row;
    unsigned short col;
    for (row = 0; row < wRows; row++) {
        for (col = 0; col < wCols; pOut++, col++) {
            if (aTileCellsMaybe[col * BOARD_TILE_ROWS + row].aPlaneASlotsMaybe[0] == 0) {
                continue;
            }
            TilePlacedObj *pObj = GetThisPlaneASlotMaybe(this, col, row);
            BigObj *pKindDesc = pObj->pKindDesc;
            switch (pKindDesc == 0 ? (unsigned char)0 : pKindDesc->categoryByte) {
            case 3:
                switch (pKindDesc == 0 ? -1 : pKindDesc->resourceId) {
                case 0xc24:
                    if (pObj->pos.wPosX != col || pObj->pos.wPosY != row ||
                        (pObj->pos.wPosX + 2 == col && pObj->pos.wPosY + 1 == row)) {
                        *pOut = 5;
                    } else if (pObj->pos.wPosX + 1 == col && pObj->pos.wPosY + 2 == row) {
                        *pOut = 5;
                    }
                    break;
                case 0xc22:
                    if (pObj->pos.wPosX + 2 != col || pObj->pos.wPosY != row ||
                        (pObj->pos.wPosX == col && pObj->pos.wPosY + 1 == row)) {
                        *pOut = 5;
                    } else if (pObj->pos.wPosX + 1 == col && pObj->pos.wPosY + 2) {
                        *pOut = 5;
                    }
                    break;
                case 0xc20:
                    if (pObj->pos.wPosX != col || pObj->pos.wPosY + 2 != row ||
                        (pObj->pos.wPosX + 1 == col && pObj->pos.wPosY == row)) {
                        *pOut = 5;
                    } else if (pObj->pos.wPosX + 2 == col && pObj->pos.wPosY + 1) {
                        *pOut = 5;
                    }
                    break;
                case 0xc1e:
                    if (pObj->pos.wPosX + 2 != col || pObj->pos.wPosY + 2 != row ||
                        (pObj->pos.wPosX + 1 == col && pObj->pos.wPosY == row)) {
                        *pOut = 5;
                    } else if (pObj->pos.wPosX == col && pObj->pos.wPosY + 1 == row) {
                        *pOut = 5;
                    }
                    break;
                default:
                    *pOut = 5;
                    break;
                }
                break;
            case 2:
                *pOut = 3;
                break;
            case 4:
                *pOut = 2;
                break;
            case 13:
                *pOut = 6;
                break;
            case 12:
                *pOut = 7;
                break;
            default:
                break;
            }
        }
    }
    return true;
}

#include "GeomUtil.h" // CalcSqDist

// The plane-A slot lookup as the ORIGINAL inlines it into the ring search below: it reaches the
// singleton through g_worldBoard rather than through `this` (the enclosing method's own wCols/
// wRows reads still go through `this`), and slot 0 of the plane is folded in. The out-of-line
// WorldBoardPartial::GetPlaneASlotMaybe (0x455620) is a separate, `this`-relative function with
// an explicit slot index; this one is inlined at all four of its call sites.
//
// sic: both bound checks are off by one -- the grid is [0 .. BOARD_TILE_COLS-1] x
// [0 .. BOARD_TILE_ROWS-1], so col == BOARD_TILE_COLS reads a cell one full column past the end
// of the array and row == BOARD_TILE_ROWS reads the next column's row 0. 0x455620 has the very
// same pair of `>` comparisons, so this is the engine's own shared idiom, not a local slip.
static inline TilePlacedObj *GetBoardPlaneASlotMaybe(short col, short row) {
    if (col < 0 || col > BOARD_TILE_COLS || row < 0 || row > BOARD_TILE_ROWS) {
        return 0;
    }
    return g_worldBoard.aTileCellsMaybe[col * BOARD_TILE_ROWS + row].aPlaneASlotsMaybe[0];
}

// FUNCTION: LOCO 0x457b60
// The track-graph EDGE COST between two adjacent tile kinds, or -1 when the two do not connect
// at all -- the gate every one of this TU's four graph walkers runs before accepting a neighbour,
// and the per-step weight LinkTrackGraphNeighbours*Maybe accumulates along a run. Path-to-path
// (both categoryByte 0xc) costs 50 and track-to-track (both 3) costs 10, so a route prefers rail
// over walking by 5:1. A path and a track only meet at a LEVEL CROSSING, and only in the two
// shapes the rest of the engine recognises as one (compare NetSessionEventQueue's own
// crossing scan): the track side must be a points/switch orientation (IsType0x63aInSet12) with
// the path side above resourceId 0x3010, or the track side must be a straight
// (IsType0x63aInSetE) -- either way the crossing costs 10, and anything else is no edge.
// A MEMBER that never reads `this` -- see the header. `ret 0x8` either way.
int WorldBoardPartial::TrackGraph_GetLinkCostAMaybe(BigObj *pKindA, BigObj *pKindB) {  // TODO: sync
    int nCost = -1;
    short catA = pKindA->categoryByte;
    short catB = pKindB->categoryByte;
    if (catA == 12 && catB == 12) {
        nCost = 50;
    } else if (catA == 3 && catB == 3) {
        nCost = 10;
    } else if (catA == 12 && catB == 3) {
        if (pKindB->IsType0x63aInSet12() && pKindA->resourceId >= 0x3011) {
            nCost = 10;
        }
        if (pKindB->IsType0x63aInSetE()) {
            nCost = 10;
        }
    } else if (catA == 3 && catB == 12) {
        if (pKindA->IsType0x63aInSet12() && pKindB->resourceId >= 0x3011) {
            nCost = 10;
        }
        if (pKindA->IsType0x63aInSetE()) {
            nCost = 10;
        }
    }
    return nCost;
}

// src/DecorActor.cpp's GetActorCategoryMaybe, widened to the type the one caller below compares
// it against: the original narrows the compare to 16 bits (`xor cx,cx`/`mov cl,al`/`cmp cx,...`),
// which only happens when both operands are 16-bit. The `(unsigned char)` on the zero arm is
// load-bearing for the same reason it is over there -- it keeps the merge 8-bit.
static inline unsigned short GetPlacedObjCategoryMaybe(TilePlacedObj *pTile) {
    return pTile->pKindDesc == 0 ? (unsigned char)0 : pTile->pKindDesc->categoryByte;
}

// FUNCTION: LOCO 0x457ce0
// The nearest plane-A placed object whose kind descriptor's own categoryByte equals nCategory,
// or NULL. Walks square rings outward from the tile containing (x, y) -- top edge left-to-right,
// then right edge downward, then bottom edge right-to-left, then left edge upward, each edge
// dropping the corner the previous one already covered -- and keeps the best squared distance
// seen so far. nSearchRangeMaybe is a PIXEL range, converted to a ring count by the same
// shift-by-4 (16 px per tile, negatives collapsing onto -1) the rest of the board code uses.
// The "found" test sits at the TOP of the ring loop rather than at the bottom, so the whole ring
// that produced a hit is finished before the search stops -- which is what makes the result the
// nearest object rather than merely the first one found.
//
// THREE source shapes here are load-bearing, all three found by bisecting the residual down:
//  (1) the ring's four edge bounds are INLINE EXPRESSIONS, not named locals. Hoisting them into
//      `int left/top/right/bottom` lets VC5 common-subexpression the `max(left, 0)` that the top
//      edge's start and the left edge's fixed column both need, so the left edge reloads a stale
//      copy out of a spill slot instead of recomputing it -- six instructions the original
//      really does emit twice (217216 -> 25501 on asmscore, and it is what closes the
//      instruction count at 327/327). VC5 still CSEs the `nCol +/- ring` subtractions itself,
//      which is where the original's own four spill slots come from.
//  (2) `i` and `j` are ONE scan variable and ONE fixed-coordinate variable reused by all four
//      edges -- not a `col`/`row` pair -- which is what the original's registers say: the edge
//      being walked is always in edi and the fixed coordinate always in ebp, even though the two
//      swap between the horizontal and vertical edges.
//  (3) the tile origin is a single 4-byte POINTS, not two `short` locals. Two separate shorts
//      each get their own DWORD spill slot; POINTS packs them into one, which is the only way to
//      reproduce the original's pair of WORD stores into the (dead) 4th parameter's home slot at
//      [esp+0x44]/[esp+0x46] -- the last 4 bytes of the residual. Declaration ORDER matters for
//      the same reason: the pair must be declared after pTile/i/j to land in that slot.
// v512: EXACT lost (DIFF(16), 951 B both sides) to the 0x4589b0 ctor bundle -- bisected to the
// WidgetBase.h ctor DEFAULT-ARGS edit ALONE (reverting it restores EXACT; the WorldActionCursor.h
// decls are not involved). The residual is the v375 zero-register-residency class plus the
// commutative-LEA swap: the original zeroes ECX as the ring counter (`xor ecx,ecx` / `cmp
// [esp+0x24],cx` / `movsx ebx,cx` / loop-bottom `inc ecx`), ours uses EAX, and the origin loads
// swap order (`movsx eax,[esp+0x44]; movsx ebx,cx` vs ours reversed, `lea esi,[ebx+eax]` vs
// `[eax+ebx]`). Re-win probes refuted this session: 1-3 spare struct decls at TU top, 1-2 free
// function decls, a forward-class+free-decl pair (all byte-identical DIFF(16) -- a SIGNATURE-shape
// dial touch, not count), `short ring; for (;...)` and `++ring` (both byte-identical). See
// docs/PARKED.md.
TilePlacedObj *WorldBoardPartial::FindNearestObjOfCategoryMaybe(  // TODO: sync (Ghidra ns is
                                                                  // WorldBoardMaybe)
    unsigned short nCategory, int x, int y, int nSearchRangeMaybe) {
    TilePlacedObj *pBest = 0;
    int nBestSqDist = 999999999;
    TilePlacedObj *pTile;
    short i;
    short j;
    short nMaxRing = (nSearchRangeMaybe < 0) ? -1 : (short)(nSearchRangeMaybe >> 4);
    POINTS ptOrigin;

    ptOrigin.x = (x < 0) ? -1 : (short)(x >> 4);
    ptOrigin.y = (y < 0) ? -1 : (short)(y >> 4);

    for (short ring = 0; ring <= nMaxRing; ring++) {
        if (pBest != 0) {
            break;
        }

        // Top edge, left to right.
        i = max(ptOrigin.x - ring, 0);
        j = max(ptOrigin.y - ring, 0);
        for (; i <= ptOrigin.x + ring && i < wCols; i++) {
            pTile = GetBoardPlaneASlotMaybe(i, j);
            if (pTile != 0 && GetPlacedObjCategoryMaybe(pTile) == nCategory) {
                int nSqDist = CalcSqDist(pTile->hotspotPosX, pTile->hotspotPosY, x, y);
                if (nSqDist < nBestSqDist) {
                    pBest = pTile;
                    nBestSqDist = nSqDist;
                }
            }
        }

        // Right edge, top to bottom -- starting one row below the corner the top edge covered.
        j = min(ptOrigin.x + ring, wCols);
        i = max(ptOrigin.y - ring + 1, 0);
        for (; i <= ptOrigin.y + ring && i < wRows; i++) {
            pTile = GetBoardPlaneASlotMaybe(j, i);
            if (pTile != 0 && GetPlacedObjCategoryMaybe(pTile) == nCategory) {
                int nSqDist = CalcSqDist(pTile->hotspotPosX, pTile->hotspotPosY, x, y);
                if (nSqDist < nBestSqDist) {
                    pBest = pTile;
                    nBestSqDist = nSqDist;
                }
            }
        }

        // Bottom edge, right to left.
        i = min(ptOrigin.x + ring - 1, wCols);
        j = min(ptOrigin.y + ring, wRows);
        for (; i >= ptOrigin.x - ring && i >= 0; i--) {
            pTile = GetBoardPlaneASlotMaybe(i, j);
            if (pTile != 0 && GetPlacedObjCategoryMaybe(pTile) == nCategory) {
                int nSqDist = CalcSqDist(pTile->hotspotPosX, pTile->hotspotPosY, x, y);
                if (nSqDist < nBestSqDist) {
                    pBest = pTile;
                    nBestSqDist = nSqDist;
                }
            }
        }

        // Left edge, bottom to top -- stopping one row short of the top edge's corner.
        j = max(ptOrigin.x - ring, 0);
        i = min(ptOrigin.y + ring - 1, wRows);
        for (; i > ptOrigin.y - ring && i >= 0; i--) {
            pTile = GetBoardPlaneASlotMaybe(j, i);
            if (pTile != 0 && GetPlacedObjCategoryMaybe(pTile) == nCategory) {
                int nSqDist = CalcSqDist(pTile->hotspotPosX, pTile->hotspotPosY, x, y);
                if (nSqDist < nBestSqDist) {
                    pBest = pTile;
                    nBestSqDist = nSqDist;
                }
            }
        }
    }
    return pBest;
}

#include "UIResources.h"           // g_UIResources.TileKind_GetOrLoadDescriptor
#include "NetSessionEventQueue.h"  // g_NetSessionEventQueue.BigObj_CreateAndInsert

extern char DAT_004fd3dc; // "placement may evict what is already there" master gate; only
                          // PlaceObject's first (pure-test) legality pass is skipped when it is 0

// FUNCTION: LOCO 0x4550c0
// PARTIAL (2026-07-26, rescored v419): structurally complete and instruction-aligned in the loop
// bodies, but 250 insns against 240 and asmscore 286183 (align=276 reg_pen=89 byte_diff=283,
// --len 791). ⚠ That is UP from 274182, and deliberately so: the plane-B dirty-bit index was
// transposed here until v419 (`wCols * nCol + nScanRow`, i.e. row-major bit for a column-major
// board walk). The original's own `imul eax,edi / add eax,ecx` has edi = the scan ROW and ecx =
// the running COLUMN -- the same `wCols * row + col` MarkRectDirty writes and both readers
// (UpdateDirtyTiles, 0x456700) consult -- so the old spelling dirtied the transposed tile. It
// cost nothing visible because both forms are one `imul` plus one `add`; only the register roles
// differ, which asmscore scores as alignment noise. Correctness wins over the 12k.
// Probed down from 634981; the four levers that moved it, each worth keeping:
//   (1) the descriptor's three extents are cached in SHORT locals up front, not re-read into
//       the bias expression -- the original loads all three with `movzx r16, byte` and then
//       subtracts them as 32-bit registers, which only a 16-bit local explains (634981 ->
//       442255, and `short` beats `unsigned short` by another 12k);
//   (2) the row bias goes into its OWN local via an if/else rather than mutating the `row`
//       parameter -- the original stores through the same slot on both arms;
//   (3) the two per-cell writes index `aTileCellsMaybe[...]` directly instead of through a
//       `BoardTileCellMaybe *pCell` local, which is what lets VC5 fold the cell index and the
//       slot index into one `[esi + idx*4 + 0x48]` the way the original does (442255 -> 313881);
//   (4) the high-water-mark test is spelled `slot >= (char)count`, not `(char)count <= slot`
//       -- same meaning, but the original's `cmp dl,[eax]` / `jl` has the operands that way
//       round (313881 -> 286307).
// The residual is a register-pressure difference concentrated in the prologue: the original
// keeps `this` in esi throughout where this compile spills it, and keeps the running column in
// ebp where this compile spills that too, so the four legality-call argument setups and both
// loop headers schedule differently. Hoisting `nRow + nRowBias` into a shared local for the two
// legality calls (which is what the original's own `add ebp,eax` before the second call looks
// like) closes the instruction count to 246/240 but costs 108k of alignment -- tried and
// reverted, do not re-try without a better lever for the spills first.
//
// Places a brand-new object of the given kind at tile (col, row) and stamps it into every board
// cell the kind's TWO occupancy tables claim, returning the object or NULL if it would not fit.
//
// The kind descriptor carries two independent footprints, and this walks both:
//   - the PHYSICAL footprint (aFootprintOccupancyMask, bFootprintX/YStepsMaybe deep by
//     bFootprintLayerCount layers) goes into the cells' plane-A slots, one slot per layer;
//   - the BITMAP footprint (aBitmapOccupancySlotGrid, bBitmapOccupancyCols/Rows) goes
//     into plane B, and its grid entry is the 1-BASED slot index to claim, so 0 means "this tile
//     is not covered". Only the plane-B pass dirties tiles -- plane A is bookkeeping, plane B is
//     what is actually drawn.
// Each plane's per-cell count is a high-water mark (`<=`, so it is the highest slot index in use,
// not a population count).
//
// The two footprints can disagree about where the object's origin is, so unless the caller says
// it has already done so (bAlreadyHotspotAdjustedMaybe == 1), row is biased up by the difference
// between the two Y extents before anything else -- and both legality passes then add that bias
// straight back, i.e. they are always asked about the row the CALLER named.
TilePlacedObjPartial *WorldBoardPartial::PlaceObject(  // TODO: sync (Ghidra ns is WorldBoardMaybe)
    unsigned int kindId, short col, short row, char bAlreadyHotspotAdjustedMaybe,
    unsigned int arg5) {
    TilePlacedObj *pObj = 0;

    // sic: `<=` against the extents, so the last legal tile is one past the grid -- the same
    // off-by-one the plane-A slot accessors have (see GetBoardPlaneASlotMaybe above).
    if (col >= 0 && col <= wCols && row >= 0 && row <= wRows) {
        BigObj *pDesc = (BigObj *)g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
        if (pDesc != 0) {
            short nBitmapRows = pDesc->bBitmapOccupancyRows;
            short nFootprintRows = pDesc->bFootprintYSteps;
            short nBitmapCols = pDesc->bBitmapOccupancyCols;
            int nRowBias = nBitmapRows - nFootprintRows;
            short nRow;

            if (bAlreadyHotspotAdjustedMaybe == 1) {
                nRow = row;
            } else {
                nRow = row - (short)nRowBias;
            }
            if (nRow >= 0 &&
                (DAT_004fd3dc == 0 ||
                 TestOrClearFootprintMaybe(0, pDesc, col, (short)(nRow + nRowBias), arg5) != 0)) {
                if (TestOrClearFootprintMaybe(1, pDesc, col, (short)(nRow + nRowBias), arg5) != 0 &&
                    (pObj = (TilePlacedObj *)g_NetSessionEventQueue.BigObj_CreateAndInsert(kindId))
                        != 0) {
                    short y;
                    int nScanRow = nRow + nRowBias;

                    for (y = 0; y < pDesc->bFootprintYSteps; y++, nScanRow++) {
                        short x;
                        int nCol = col;

                        for (x = 0; x < pDesc->bFootprintXSteps; x++, nCol++) {
                            char layer;

                            for (layer = 0; layer < pDesc->bFootprintLayerCount; layer++) {
                                if (pDesc->aFootprintOccupancyMask[x][y][layer] == 1) {
                                    int nCell = (short)nCol * BOARD_TILE_ROWS + (short)nScanRow;

                                    aTileCellsMaybe[nCell].aPlaneASlotsMaybe[layer] = pObj;
                                    if (layer >= aTileCellsMaybe[nCell].bPlaneACountMaybe) {
                                        aTileCellsMaybe[nCell].bPlaneACountMaybe = layer;
                                    }
                                }
                            }
                        }
                    }

                    nScanRow = nRow;
                    for (y = 0; y < nBitmapRows; y++, nScanRow++) {
                        short x;
                        int nCol = col;

                        for (x = 0; x < nBitmapCols; x++, nCol++) {
                            if (pDesc->aBitmapOccupancySlotGrid[x][y] != 0) {
                                int nCell = (short)nCol * BOARD_TILE_ROWS + (short)nScanRow;
                                char slot = pDesc->aBitmapOccupancySlotGrid[x][y] - 1;
                                unsigned int bit;

                                aTileCellsMaybe[nCell].aPlaneBSlotsMaybe[slot] = pObj;
                                if (slot >= aTileCellsMaybe[nCell].bPlaneBCountMaybe) {
                                    aTileCellsMaybe[nCell].bPlaneBCountMaybe = slot;
                                }
                                bit = g_worldBoard.wCols * (short)nScanRow + (short)nCol;
                                g_worldBoard.pDirtyBitmapMaybe[bit >> 3] |= DAT_0047f108[bit & 7];
                            }
                        }
                    }

                    pObj->pos.wPosY = nRow;
                    pObj->pos.wPosX = col;
                    pObj->RepositionWithHotspot(col << 4, nRow << 4);
                }
                return (TilePlacedObjPartial *)pObj;
            }
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x455ab0
// PARTIAL (v419): structurally complete and instruction-aligned -- asmscore.py --len 686 gives
// insns 216/215 with almost every dump row marked `r` (same mnemonic, different register), i.e.
// the whole residual is register-role allocation, not missing or extra body. total 234224, down
// from 433375 on the first draft; the one lever that mattered is recorded on BoardTileCellMaybe
// in the header (both plane counts are SIGNED char -- that alone took the plane-B slot loop from
// a two-counter spilled construct with a `jne` bottom back to the original's `dec bl / test bl,bl
// / jge`, and the count comparison from a 32-bit `and ecx,0xff; cmp ecx,ebp` to `cmp al,bl`).
// The three concrete gaps left, all probed and none source-steerable:
//   (1) the dirty-bit store: the original read-modify-writes through a register (`mov cl,[edx] /
//       or cl,al / mov [edx],cl`) where this compile folds it to one `or byte ptr [edx],al`.
//       Spelling it out longhand, routing it through an explicit temp, and hoisting the pointer
//       all still fold. Note PlaceObject's own stamp of the SAME bitmap does emit the folded
//       `or [ecx],al` in the original, so this is not a shared inline helper -- the two sites
//       genuinely differ, which makes it a scheduler coin flip rather than a source shape.
//   (2) loop 1's inner-loop entry: this compile hoists `(int)nCol` out of the row loop and jumps
//       into the loop (one extra `jmp`), where the original recomputes `movsx edi,di` at the top
//       of each row. ⚠ The original does the OPPOSITE in loop 2 -- there IT hoists `(int)nCol`
//       into a slot and lets the row back-edge land on the guard -- so the two loops disagree in
//       the original itself and no single source shape can produce both.
//   (3) `this` lives in edx in the original and ebp here (the documented symmetric-register-swap
//       class), which is what makes the frame one dword bigger (sub esp,0x28 vs 0x24) and shifts
//       every parameter-slot reference by 4.
// Probed and REVERTED (all neutral or worse, do not re-try): every declaration-order permutation
// of the six prologue locals (neutral); `row + col * BOARD_TILE_ROWS` (neutral); nCell/slot/i
// hoisted to function scope (neutral); assigning loop 2's biased start into the ROW LOOP
// VARIABLE rather than into nRow (256349 -- worse, even though that is literally which stack
// slot the original writes). See docs/PARKED.md.
//
// PlaceObject's exact inverse. Walks BOTH of the kind's occupancy footprints over the board --
// the BITMAP footprint's rows/cols first (plane B), then the PHYSICAL footprint (plane A, whose
// origin is biased by the same Y-extent difference PlaceObject un-biases by) -- and at every cell
// clears any slot still holding pItem, then compacts that plane's high-water mark back down over
// whatever trailing holes are left. Only the plane-B pass dirty-marks the tiles it touches, which
// is the same asymmetry PlaceObject has (plane A is bookkeeping, plane B is what is drawn).
//
// Each plane's per-cell scan runs slot-DOWNWARD, and the two planes disagree about where to
// start: plane B always sweeps all 7 slots from 6, while plane A starts at the cell's own current
// count. Both then run the same trailing-hole compaction (`for (i = count; i >= 0; i--)`, whose
// else-arm assigns `i = 0` so the following `i--` ends the loop -- an early-out written as an
// assignment rather than a break).
char WorldBoardPartial::FlushQueuedBigObjMaybe(  // TODO: sync (Ghidra ns is WorldBoardMaybe)
    TilePlacedObj *pItem, unsigned int bSpawnEffect) {
    if (pItem != 0 && pItem->bSaveableFlag != 0 && pItem->bValid == 1) {
        BigObj *pKindDesc = pItem->pKindDesc;
        short nCol = pItem->pos.wPosX;
        short nRow = pItem->pos.wPosY;
        unsigned short usEndCol = nCol + pKindDesc->bBitmapOccupancyCols;
        unsigned short usEndRow = nRow + pKindDesc->bBitmapOccupancyRows;
        short row;
        short col;

        for (row = nRow; row < usEndRow; row++) {
            for (col = nCol; col < usEndCol; col++) {
                int nCell = col * BOARD_TILE_ROWS + row;
                char slot;
                int i;

                for (slot = 6; slot >= 0; slot--) {
                    if (aTileCellsMaybe[nCell].aPlaneBSlotsMaybe[slot] == pItem) {
                        unsigned int bit;

                        if (aTileCellsMaybe[nCell].bPlaneBCountMaybe == slot) {
                            aTileCellsMaybe[nCell].bPlaneBCountMaybe--;
                        }
                        aTileCellsMaybe[nCell].aPlaneBSlotsMaybe[slot] = 0;
                        bit = g_worldBoard.wCols * row + col;
                        g_worldBoard.pDirtyBitmapMaybe[bit >> 3] |= DAT_0047f108[bit & 7];
                    }
                }
                for (i = aTileCellsMaybe[nCell].bPlaneBCountMaybe; i >= 0; i--) {
                    if (aTileCellsMaybe[nCell].aPlaneBSlotsMaybe[i] == 0) {
                        aTileCellsMaybe[nCell].bPlaneBCountMaybe--;
                    } else {
                        i = 0;
                    }
                }
            }
        }

        nRow = nRow + (pKindDesc->bBitmapOccupancyRows - pKindDesc->bFootprintYSteps);
        usEndRow = pKindDesc->bFootprintYSteps + nRow;
        usEndCol = pKindDesc->bFootprintXSteps + nCol;
        for (row = nRow; row < usEndRow; row++) {
            for (col = nCol; col < usEndCol; col++) {
                int nCell = col * BOARD_TILE_ROWS + row;
                char slot;
                int i;

                for (slot = aTileCellsMaybe[nCell].bPlaneACountMaybe; slot >= 0; slot--) {
                    if (aTileCellsMaybe[nCell].aPlaneASlotsMaybe[slot] == pItem) {
                        if (aTileCellsMaybe[nCell].bPlaneACountMaybe == slot) {
                            aTileCellsMaybe[nCell].bPlaneACountMaybe--;
                        }
                        aTileCellsMaybe[nCell].aPlaneASlotsMaybe[slot] = 0;
                    }
                }
                for (i = aTileCellsMaybe[nCell].bPlaneACountMaybe; i >= 0; i--) {
                    if (aTileCellsMaybe[nCell].aPlaneASlotsMaybe[i] == 0) {
                        aTileCellsMaybe[nCell].bPlaneACountMaybe--;
                    } else {
                        i = 0;
                    }
                }
            }
        }
        g_NetSessionEventQueue.BigObj_Remove((TilePlacedObjPartial *)pItem, bSpawnEffect);
        return 1;
    }
    return 0;
}

// The second of this TU's two app-state byte-predicates (see IsInGameModeMaybe above for why the
// `unsigned char` return type is load-bearing). Only TestOrClearFootprintMaybe consults state 1.
inline unsigned char IsAppStateOneMaybe() { return g_nScreenState == 1; }

// TU-local methods-only extension of BigObj (src/CursorDesc.h), same pattern as
// PeerTrainSlotQueueView0x456150 above. ⚠ Declaring IsAtMatchingBoardEdgeMaybe on BigObj ITSELF
// is what the real source surely does, but it costs two other TUs an EXACT match apiece --
// src/WalkerActor.cpp's UpdateFacingFromStepDeltaMaybe (0x4331b0, 112 B) and
// src/Obj0x4779e0.cpp's ParseEntryExitMaybe (0x41f0c0, 489 B), both flipping to a mirrored
// operand-load order in a subtraction. That is the same shared-header declaration-COUNT rotation
// src/WorldBoardMaybe.h carries its own warning about, reproduced here for CursorDesc.h (v419
// bisect: removing the one declaration restores both, exactly). Retire this view in favour of a
// real BigObj declaration once CursorDesc.h's declaration set is closer to the original's.
struct BigObjEdgeView0x4553e0 : BigObj {
    // 0x44bdb0, extern -- true when this map-edge/connector kind (m_type0x63a in {1,2,3,4}, one
    // value per board edge) is being placed AT the board edge its own direction demands: type 1
    // wants column 0, type 2 the right edge (col + bBitmapOccupancyCols == wCols), type 3
    // the top (row == bBitmapOccupancyRows - bFootprintYSteps) and type 4 the bottom
    // (row + bBitmapOccupancyRows == wRows). This is what refuses to let a tunnel mouth be
    // built anywhere but on its own board edge. Not yet transcribed.
    char IsAtMatchingBoardEdgeMaybe(short col, short row);
};

// FUNCTION: LOCO 0x4553e0
// PlaceObject's footprint legality pass, run twice per placement: bClearMaybe == 0 just asks
// whether every tile the kind's PHYSICAL footprint wants is free, bClearMaybe == 1 asks the same
// question and EVICTS whatever it finds on the way (one sound plus one FlushQueuedBigObjMaybe per
// displaced object). It walks exactly the mask PlaceObject then stamps, but with the loops nested
// layer-outermost rather than row-outermost.
//
// "Free" is a three-way test, and only the middle case is a refusal: an empty plane-A slot is
// fine, and so is an occupied one whose occupant is evictable -- evictable meaning the master
// placement-evicts-things gate DAT_004fd3dc is on, the occupant is a saveable object, and either
// the placement cursor is not snap-locked or the app is in one of the two states that allow it.
// Anything else clears the result to 0 and the walk carries on (it does NOT return early, so the
// eviction pass still runs to completion for the tiles that did pass).
//
// ⚠ EFFECTIVE MATCH as of v471 -- DIFF(27) at an unchanged 572 B. It was EXACT and its source
// was NOT touched: the flip is /Og TU-state rotation from `#include <stdio.h>` landing in
// src/DSoundChannel.h (which this TU consumes transitively) so RFIndex could model its .RFD
// handle as a real FILE*. Same declaration-set-sensitive coin-flip class as this repo's other
// serial flippers (TilePlacedObj's 0x458310 / 0x4588b0), just the largest one yet. The trade
// was measured BOTH ways repo-wide and taken deliberately: the stdio.h spelling is +754 B / +4
// funcs net, while forward-declaring FILE instead restores this function but costs
// PeerTrainNode (-474), DPlaySessionMgr (-166), TilePlacedObj (-83) and LocoBitmap (-124), for
// 135420/574 against 135695/578. **Do NOT re-autopsy this as a source-shape problem** -- see
// docs/PARKED.md.
char WorldBoardPartial::TestOrClearFootprintMaybe(  // TODO: sync (Ghidra ns is WorldBoardMaybe)
    char bClearMaybe, BigObj *pDesc, short col, short row, unsigned int arg5) {
    char bOkMaybe = 1;

    if (pDesc->bFootprintXSteps + col > wCols ||
        pDesc->bFootprintYSteps + row > wRows) {
        return 0;
    }
    // A map-edge connector kind may only be placed on the edge its own direction names.
    if (pDesc->categoryByte == 3 && pDesc->IsType0x63aInSet1234() == 1) {
        bOkMaybe = ((BigObjEdgeView0x4553e0 *)pDesc)->IsAtMatchingBoardEdgeMaybe(col, row);
    }
    if (bOkMaybe == 1 && pDesc->bFootprintXSteps != 0 &&
        pDesc->bFootprintYSteps != 0) {
        bOkMaybe = 1;
        for (short layer = 0; layer < pDesc->bFootprintLayerCount; layer++) {
            for (short y = 0; y < pDesc->bFootprintYSteps; y++) {
                for (short x = 0; x < pDesc->bFootprintXSteps; x++) {
                    TilePlacedObj *pTile;

                    if (pDesc->aFootprintOccupancyMask[x][y][layer] != 0 &&
                        (pTile = aTileCellsMaybe[(x + col) * BOARD_TILE_ROWS + y + row]
                                     .aPlaneASlotsMaybe[layer]) != 0) {
                        if (DAT_004fd3dc == 1 && pTile->bSaveableFlag == 1 &&
                            (!PlacementCursorMaybe_004854c8.bSnapLockMaybe ||
                             IsInGameModeMaybe() || IsAppStateOneMaybe())) {
                            if (bClearMaybe != 0) {
                                g_UIResources.PlaySoundAtScreenPos(0x5024, col << 4, row << 4, 4);
                                pTile = aTileCellsMaybe[(short)(x + col) * BOARD_TILE_ROWS +
                                                        (short)(y + row)]
                                            .aPlaneASlotsMaybe[layer];
                                if (pTile != 0) {
                                    FlushQueuedBigObjMaybe(pTile, arg5);
                                }
                            }
                        } else {
                            bOkMaybe = 0;
                        }
                    }
                }
            }
        }
    }
    return bOkMaybe;
}

#include "TilePlacedObj.h"    // the plane-A occupants this dispatches on (pKindDesc + 2 virtuals)
#include "CreditsWnd.h"       // g_pCreditsWnd -- the 0x820 kind opens the credits window
#include "DPlaySessionMgr.h"  // g_pDPlaySessionMgr->connectionMode

// The top-level UI mode switch (0x408130); cases 5/6/7/9 are the four front-end screens the
// world-board kinds below open. Same decl as src/BuildToolButton.cpp's.
extern void AppWindow_SetScreenState(int newState);

extern int DAT_00485234;   // build-tool mode: 1 = bulldoze, 2 = place PlacementCursorMaybe's
                           // current nTypeIdMaybe (src/PlacementCursorMaybe.cpp's own decl)
extern char DAT_004aa648;  // a global input lock: while nonzero the build tool refuses every
                           // world click, and PlacementCursorMaybe (0x4117b0) forces cursor
                           // kind 0x1404 instead of the tool's own


// TU-local methods-only view of the board for this function's own two entry points, for the
// same reason src/PlacementCursorMaybe.cpp's WorldBoardClickView0x411000 exists: declaring
// them on the shared WorldBoardPartial costs src/LocoBitmap.cpp an EXACT function (measured
// v407). ResolveWorldClickMaybe itself is defined ON the view, so no cast is needed for it --
// this TU has no callers of it, and the one TU that does already casts.
struct WorldBoardClickView0x455d60 : WorldBoardPartial {
    // ⚠ ResolveWorldClickMaybe / GetTopPlaneBSlotMaybe / GetPlaneBTopSlotAtPixelMaybe were
    // declared here until v576 and are now on the real WorldBoardPartial (src/WorldBoardMaybe.h)
    // -- src/PlacementCursorMaybe.cpp spelled them on a SECOND view and its calls therefore went
    // to a symbol nothing defined. Do not re-add them here; see that header's note.
    // 0x4557c0, defined below -- the GRID POSITION of the plane-A object in slot iSlot at grid
    // (x, y), i.e. that object's own origin tile rather than the queried cell, or (-1, -1).
    // src/NameAnchorMaybe.cpp's WorldBoardSlotOriginView0x40b880 spells the third parameter
    // `plane`; it is a slot index within plane A, not a plane selector.
    TileGridPos GetPlaneASlotGridPosMaybe(short x, short y, short iSlot);
    // 0x455960, defined below -- bulldoze every object on the screen-pixel segment from
    // (xPrev, yPrev) to (x, y): steps a unit vector along it, and flushes whatever plane-A
    // slot each step lands on.
    unsigned char EraseAlongDragSegmentMaybe(int x, int y, int xPrev, int yPrev);
};

// FUNCTION: LOCO 0x4556f0
// The plane-B occupant under the screen pixel (x, y) -- the pixel-keyed companion of
// GetTopPlaneBSlotMaybe below, and the reason both of them are here: a click arrives in pixels
// and has to be resolved against the tile grid.
//
// It returns the slot the cell's own count POINTS AT rather than scanning for the topmost
// occupied one, so a cell whose count has run ahead of its contents yields NULL from that slot
// directly. That is the whole difference between the two functions, and it is why this one needs
// no loop.
//
// sic: no bounds check at all, unlike every grid-keyed lookup in this TU -- a negative pixel maps
// to tile -1 (the engine's own `< 0 ? -1 : >> 4` idiom, used verbatim by RefreshVisibleTileSpan
// above) and then indexes the cell array at -66, i.e. before its start. Both callers happen to
// resolve the pixel against the board first, so the read is never actually reached.
//
// ⚠ The `BoardTileCellMaybe *pCell` local is LOAD-BEARING and must not be folded back into two
// subscripts of `aTileCellsMaybe[nCell]`. The compiler CSEs the cell address either way, so both
// spellings emit the same 25 instructions in the same order -- but the named pointer is what puts
// the accumulator in `esi` and the row temp in `edx`, which is the original's allocation; the
// subscript form swaps them and moves the `pop esi`, for a 14-byte residual that no other lever
// touched (measured this session: an `x < 0 ? -1 : x >> 4` ternary and reordering the col/row
// declarations are both bit-for-bit inert).
TilePlacedObj *WorldBoardPartial::GetPlaneBTopSlotAtPixelMaybe(int x, int y) // TODO: sync
{
    short col;
    short row;
    BoardTileCellMaybe *pCell;

    if (x < 0) {
        col = -1;
    } else {
        col = (short)(x >> 4);
    }
    if (y < 0) {
        row = -1;
    } else {
        row = (short)(y >> 4);
    }
    pCell = &this->aTileCellsMaybe[col * BOARD_TILE_ROWS + row];
    return pCell->aPlaneBSlotsMaybe[pCell->bPlaneBCountMaybe];
}

// FUNCTION: LOCO 0x455620
// The out-of-line, `this`-relative plane-A slot lookup: the object sitting in slot iSlot of the
// plane-A array at grid (x, y), or NULL when the tile is off-board. This is the form the rest of
// the engine CALLS (~15 sites across seven TUs, all of them passing slot 0); the two file-static
// helpers above -- GetThisPlaneASlotMaybe and GetBoardPlaneASlotMaybe -- are the shapes VC5
// INLINES into this TU's own two big walkers, with slot 0 folded in.
//
// sic: the same off-by-one `>` bound pair as every other member of this family -- the grid is
// [0 .. BOARD_TILE_COLS-1] x [0 .. BOARD_TILE_ROWS-1], so x == BOARD_TILE_COLS reads a cell one
// full column past the end of the array and y == BOARD_TILE_ROWS reads the next column's row 0.
// The engine's own shared idiom, reproduced rather than fixed.
//
// ⭐ The `(base + x * ROWS)[y]` shape is LOAD-BEARING and took three compiles to find; do not
// "simplify" it back. The flat `aTileCellsMaybe[x * ROWS + y]` this TU uses everywhere else
// scales the SUM once -- `(x*65 + y) * 64`, 26 instructions, DIFF(35) and three bytes short --
// whereas the original scales the two terms SEPARATELY (`this + x*65*64`, then `+ y*64`), which
// is what a column-pointer-then-row-index expression emits. Hoisting that same expression into a
// NAMED LOCAL (`BoardTileCellMaybe *pColumn = &aTileCellsMaybe[x * ROWS]; return pColumn[y]…`)
// gets the 27-instruction shape right but SWAPS the register pair the two bound checks run in
// (x lands in dx/y in ax rather than the original's x in ax/y in dx) and additionally cost
// WorldBoardMaybe_ResetAllTilesMaybe its own 211-byte exact -- the local is the whole difference.
TilePlacedObj *WorldBoardPartial::GetPlaneASlotMaybe(short x, short y, short iSlot) {  // TODO: sync (Ghidra ns is WorldBoardMaybe)
    if (x < 0 || x > BOARD_TILE_COLS || y < 0 || y > BOARD_TILE_ROWS) {
        return 0;
    }
    return (this->aTileCellsMaybe + x * BOARD_TILE_ROWS)[y].aPlaneASlotsMaybe[iSlot];
}

// FUNCTION: LOCO 0x455670
// The topmost occupied slot at tile (x, y): backward-scan the cell's slot array from the count
// down, and return both the object and (via pOutIndex) the slot it sat in. NULL, with pOutIndex
// left untouched, when the cell is empty or the tile is off-board.
//
// ⭐ RENAMED 2026-07-31: was GetTopPlaneASlotMaybe, and the name was simply WRONG -- the body
// reads plane B throughout, bPlaneBCountMaybe (+0x3c) and aPlaneBSlotsMaybe (+0x20), confirmed
// from the raw bytes (`[edx+ecx]` at (cell+2)*64 is 128-0x44 = +0x3c; `[ecx+esi*4+0x64]` is
// 0x64-0x44 = +0x20). The long-standing SWAP hypothesis -- that this and its pixel-keyed sibling
// 0x4556f0 GetPlaneBTopSlotAtPixelMaybe had traded names -- is REFUTED: 0x4556f0 reads plane B
// too, so both were plane-B accessors and only this one was misnamed. The pair is grid-keyed vs
// pixel-keyed, not plane A vs plane B. The A/B convention itself is grounded: 0x455620
// GetPlaneASlotMaybe reads cell +0x4, and the group's other twin pair (0x4557c0 plane A /
// 0x455740 plane B) differs in exactly ONE byte, the array-base displacement.
//
// sic: the same off-by-one `>` bound pair as 0x455620 and GetBoardPlaneASlotMaybe above -- x ==
// BOARD_TILE_COLS reads a cell one full column past the end of the array. The engine's own
// shared idiom, reproduced.
//
// EFFECTIVE MATCH -- asmscore --len 118: total 16893, align=16 reg_pen=8 identity_miss=8
// byte_diff=13. Every instruction, operand, branch target and both epilogues agree; the ENTIRE
// residual is ONE register coin-flip and its knock-on. The original parks `y` in `si` and
// `x * BOARD_TILE_ROWS` in `edi`, so the sum needs a 3-address `lea esi,[edi+edx]`; this compile
// parks them the other way round (`di`/`esi`) and folds the sum into a 2-byte `add esi,edx`.
// That one byte shifts everything after it, which is the whole of DIFF(79).
//
// Two source shapes WERE load-bearing and are already applied -- do not undo them:
//   (1) the slot scan is a `while`, not `if (count >= 0) { do … while }`. The count-negative case
//       must fall into the SAME `return pResult` as the loop: the original's `jl` at 0x4556b3
//       targets the epilogue AFTER the `xor eax,eax`, i.e. it returns the (still null) pResult
//       rather than taking the bounds-failure `return 0`. Total 40901 -> 16893 on that alone;
//   (2) inside the hit branch, `pResult = pObj` comes BEFORE `iSlot = -1` (`mov eax,esi` then
//       `or edx,-1`, not the reverse).
// Measured and REFUTED this session (do not re-run): `y + x * BOARD_TILE_ROWS`; `unsigned int`
// nCell; hoisting `pObj` out of the loop; splitting the declaration of `iSlot` from its
// initialization; and splitting the multiply into its own named local (`int nCol = x * ROWS;
// int nCell = nCol + y;` -- the obvious way to keep the product alive for a `lea`, and
// bit-for-bit inert). A `BoardTileCellMaybe &cell` reference is much worse (76249): it hoists
// the cell address and drops to 42 instructions. PARKED (docs/PARKED.md).
TilePlacedObj *WorldBoardPartial::GetTopPlaneBSlotMaybe( // TODO: sync (TU-local view)
    short x, short y, short *pOutIndex)
{
    TilePlacedObj *pResult = 0;

    if (x >= 0 && x <= BOARD_TILE_COLS && y >= 0 && y <= BOARD_TILE_ROWS) {
        int nCell = x * BOARD_TILE_ROWS + y;
        short iSlot = this->aTileCellsMaybe[nCell].bPlaneBCountMaybe;
        while (iSlot >= 0) {
            TilePlacedObj *pObj = this->aTileCellsMaybe[nCell].aPlaneBSlotsMaybe[iSlot];
            if (pObj != 0) {
                *pOutIndex = iSlot;
                pResult = pObj;
                iSlot = -1;
            }
            iSlot--;
        }
        return pResult;
    }
    return 0;
}

// FUNCTION: LOCO 0x4557c0
// Where does the plane-A object in slot iSlot at grid (x, y) actually STAND? Multi-tile objects
// occupy several cells but hold one origin, so every cell they cover answers the same position --
// which is what makes this the identity test callers use ("are these two cells the same object").
// (-1, -1) means the slot is empty or the tile is off-board.
//
// Returned BY VALUE. At 4 bytes that still compiles to the hidden-return-pointer convention here
// (`ret 0x10` for three shorts plus the pointer), which is what the call sites in
// src/NameAnchorMaybe.cpp already emit.
//
// sic: the slot is read BEFORE the bounds are checked, so an off-board (x, y) indexes the cell
// array out of range and only then has its result discarded. The `pObj = 0` / re-test pair is the
// original's own shape, not a transcription artefact -- the bounds failure nulls the pointer
// rather than branching straight to the (-1, -1) return, which is why there are two consecutive
// null tests on the same variable.
//
// EFFECTIVE MATCH -- asmscore --len 121: total 26621, align=24 reg_pen=23 identity_miss=28
// byte_diff=41, insns 45/45. Every instruction, operand, branch target and both epilogues agree.
// The residual is entirely register allocation: the original parks `y` in `ax` (caller-saved) and
// the cell accumulator in `esi`, and loads `y` only AFTER computing `x * BOARD_TILE_ROWS`; this
// compile parks `y` in `si` and the accumulator in `edi`, and loads `y` first -- which also moves
// the `push edi` two instructions earlier and swaps the register pair in the hidden-return copy.
// The nesting IS load-bearing and must not be flattened: the outer `if (pObj != 0)`'s else-arm is
// the (-1, -1) return, so its `je` targets the failure epilogue directly; an `&&`-flattened guard
// would fall through into the second null test instead. Measured and REFUTED this session (do not
// re-run): a named `int nCell` temp; `pos.wPosY` assigned before `pos.wPosX`; declaring `pObj`
// ahead of `pos`; and initializing `pObj` at its declaration instead of assigning. The named
// `BoardTileCellMaybe *pCell` lever that made 0x4556f0 exact does not apply here -- the slot index
// is folded into the same scaled index as the cell, so there is no separate cell address to name.
// PARKED (docs/PARKED.md).
TileGridPos WorldBoardClickView0x455d60::GetPlaneASlotGridPosMaybe( // TODO: sync (TU-local view)
    short x, short y, short iSlot)
{
    TileGridPos pos;
    TilePlacedObj *pObj;

    pos.wPosX = -1;
    pos.wPosY = -1;
    pObj = this->aTileCellsMaybe[x * BOARD_TILE_ROWS + y].aPlaneASlotsMaybe[iSlot];
    if (pObj != 0) {
        if (x < 0 || x > BOARD_TILE_COLS || y < 0 || y > BOARD_TILE_ROWS) {
            pObj = 0;
        }
        if (pObj != 0) {
            return pObj->pos;
        }
    }
    return pos;
}

// FUNCTION: LOCO 0x455d60
// PARTIAL (v420): structurally complete and instruction-aligned -- asmscore --len 988: insns
// 348/343, align=336 reg_pen=17 byte_diff=158, total 338028 (down from 1475673 on the first
// transcription). Four levers moved it, all kept:
//   (1) the state dispatch is a SWITCH, not an if/else chain -- the original's
//       `sub ecx,3 / je / dec ecx / jne` is switch lowering, and an if/else emits `cmp/je`
//       pairs instead;
//   (2) case 4 is written BEFORE case 3 even though the compare chain tests 3 first. MSVC sorts
//       the chain by case value but lays the bodies out in SOURCE order, and the original's
//       case-4 body is the dispatch's fall-through (1475673 -> 686356);
//   (3) the bulldoze arm's drag branch ends in a `break`, not a `return bConsumed` -- an early
//       return lets VC5 constant-fold the just-assigned 1 into `mov al,1`, where the original
//       reloads the byte local like every other exit (686356 -> 459273 with (4));
//   (4) the "nothing under the cursor" pair of selection calls is spelled `a() == 0 && b() == 0`
//       with the FAILURE case in the then-arm, not `a() || b()` with the success case -- that is
//       what puts both `bConsumed = 1` stores behind forward `jne`s and leaves the 0 store on
//       the fall-through, instead of collapsing the second test into a `setne` (402057 ->
//       338028).
// Two residual gaps, both probed and refuted -- do NOT re-grind:
//   (a) the original computes case 4's col/row BEFORE the switch dispatch (they are scheduled
//       into the prologue's pushes), which only a FUNCTION-scope pair reproduces; but at
//       function scope VC5 must keep `col` live across case 3's own recompute and spills it,
//       growing the frame from `push ecx` to `sub esp,8` -- measured 416476, i.e. 78k WORSE
//       than leaving the pair per-case. Reassigning the shared pair in case 3 instead of
//       declaring a second one scores bit-identically (416476), so there is no third spelling.
//       The whole `align=336` is this one block's position cascading into every jump
//       displacement.
//   (b) the two drag-threshold temporaries: the original allocates ONE register (ebp) for
//       `dwLastBulldozeXMaybe` and then `dwLastBulldozeYMaybe`, so it must reload the X field
//       before pushing it, and keeps `x` in the volatile ecx, so it must reload `x` after the
//       call; this compile has both in preserved registers and needs neither reload. That is
//       the documented symmetric-register-swap class, not a source shape.
//
// The board's own click handler, keyed on the top-level app state (g_nScreenState).
//
// State 4 is BUILD mode, and it is the build tool that decides what the click means: mode 1
// bulldozes (one tile, or -- if the previous click was more than 16 pixels away and the button
// never came up -- the whole segment between the two), mode 2 places the cursor's current kind
// and plays the accept (0x501a) or reject (0x501b) sound. Either way the click is consumed.
//
// State 3 is PLAY mode: the object under the cursor gets its two click virtuals, and then a
// handful of special kinds open a front-end screen instead of merely being selected -- the
// tutorial signs (0x818/0x848), the credits sign (0x820), the postbox family (0xc5c..0xc60),
// and the map-edge connectors (0xc42..0xc48) plus the station family (0x3011..0x301b), the last
// two of which open the multiplayer screen only while a session is actually connected. Anything
// else -- including a click on bare ground -- falls through to the selection widgets.
char WorldBoardPartial::ResolveWorldClickMaybe(int x, int y) {  // TODO: sync
    char bConsumed = 0;
    short sSlot;

    switch (g_nScreenState) {
    case 4: {
        short col = x < 0 ? -1 : (short)(x >> 4);
        short row = y < 0 ? -1 : (short)(y >> 4);
        TilePlacedObj *pObj = GetTopPlaneBSlotMaybe(col, row, &sSlot);

        if (DAT_00485234 == 1 && PlacementCursorMaybe_004854c8.bPendingActionAMaybe == 1 &&
            DAT_004aa648 == 0) {
            if (pObj != 0) {
                if (bFlag0x3cMaybe &&
                    (x - dwLastBulldozeXMaybe >= 0x10 || y - dwLastBulldozeYMaybe >= 0x10)) {
                    ((WorldBoardClickView0x455d60 *)&g_worldBoard)
                        ->EraseAlongDragSegmentMaybe(x, y, dwLastBulldozeXMaybe,
                                                     dwLastBulldozeYMaybe);
                    dwLastBulldozeXMaybe = x;
                    bConsumed = 1;
                    bFlag0x3cMaybe = true;
                    dwLastBulldozeYMaybe = y;
                    break;
                }
                g_worldBoard.FlushQueuedBigObjMaybe(pObj, 1);
            }
            dwLastBulldozeXMaybe = x;
            bConsumed = 1;
            bFlag0x3cMaybe = true;
            dwLastBulldozeYMaybe = y;
        } else if (DAT_00485234 == 2 && PlacementCursorMaybe_004854c8.nTypeIdMaybe != -1 &&
                   PlacementCursorMaybe_004854c8.bPendingActionAMaybe == 1 &&
                   DAT_004aa648 == 0) {
            TilePlacedObjPartial *pPlaced = g_worldBoard.PlaceObject(
                PlacementCursorMaybe_004854c8.nTypeIdMaybe, col, row, 0, 1);

            if (PlacementCursorMaybe_004854c8.bPendingActionCMaybe == 0) {
                if (pPlaced != 0) {
                    g_UIResources.PlaySoundAtScreenPos(0x501a, x, y, 4);
                } else if (PlacementCursorMaybe_004854c8.bSnapLockMaybe == 0) {
                    g_UIResources.PlaySoundAtScreenPos(0x501b, x, y, 4);
                }
            }
            bConsumed = 1;
        } else if (pObj != 0 && PlacementCursorMaybe_004854c8.bPendingActionAMaybe == 1) {
            bConsumed = 0;
        }
        break;
    }
    case 3: {
        // sic: this case computes its own pair rather than sharing case 4's, and declares them
        // the other way round -- which is what makes the original evaluate the row expression
        // first here and the column expression first there.
        short row = y < 0 ? -1 : (short)(y >> 4);
        short col = x < 0 ? -1 : (short)(x >> 4);
        TilePlacedObj *pObj = GetTopPlaneBSlotMaybe(col, row, &sSlot);

        if (PlacementCursorMaybe_004854c8.bPendingActionAMaybe && pObj != 0) {
            pObj->ResetToBaseSubFrameMaybe();
            bConsumed = 1;
        }
        if (pObj != 0) {
            if (PlacementCursorMaybe_004854c8.bPendingActionBMaybe == 1) {
                bConsumed = pObj->OnPlacedObjEventMaybe();
                switch (pObj->pKindDesc->resourceId) {
                case 0x818:
                    AppWindow_SetScreenState(7);
                    break;
                case 0x820:
                    g_pCreditsWnd->Show();
                    break;
                case 0x848:
                    AppWindow_SetScreenState(6);
                    break;
                case 0xc5c:
                case 0xc5e:
                case 0xc60:
                    AppWindow_SetScreenState(5);
                    break;
                case 0xc42:
                case 0xc44:
                case 0xc46:
                case 0xc48:
                case 0x3011:
                case 0x3013:
                case 0x3015:
                case 0x3017:
                case 0x3019:
                case 0x301b:
                    if (g_pDPlaySessionMgr->connectionMode == 2) {
                        AppWindow_SetScreenState(9);
                    } else {
                        bConsumed = SelectedObjWidgetMaybe_004852a0.SelectObjMaybe((int)pObj);
                    }
                    break;
                default:
                    if (PlacementCursorMaybe_004854c8.bPendingActionBMaybe == 1) {
                        bConsumed = SelectedObjWidgetMaybe_004852a0.SelectObjMaybe((int)pObj);
                    }
                    break;
                }
            }
        } else if (PlacementCursorMaybe_004854c8.bPendingActionBMaybe == 1) {
            if (SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(0) == 0 &&
                g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(0) == 0) {
                bConsumed = 0;
            } else {
                bConsumed = 1;
            }
        }
        break;
    }
    }
    return bConsumed;
}

#include <math.h> // sqrt -- mid-file include, same pattern as this TU's TilePlacedObj.h/
                    // CreditsWnd.h above: placed here so nothing before 0x455960's own body
                    // sees a changed declaration environment.

// FUNCTION: LOCO 0x455960
// EFFECTIVE MATCH (v509, DIFF(203), compiled 321 B vs 323 B, asmscore 58258 with insns
// **107/107** and reg_pen only 2 -- the score is almost all align bytes from shifted rows).
// The integer prologue, the local slot map (dx 0xc / dy 0x10 / dist 0x18 / fdx 0x20 / fdy
// 0x28 / fx 0x30 / fy 0x38), the ENTIRE loop body (unit-step, both __ftol clamps, the
// GetTopPlaneBSlotMaybe + FlushQueuedBigObjMaybe calls, the dist -= 1.0 bottom test) and the
// epilogue pair instruction-for-instruction. The whole residual is the FP-STACK SCHEDULING in
// the setup: cl 11.00 spills the parked x/y doubles before the squares' faddp and carries one
// fewer live FP-stack entry through the two fdivs than the original does, so every fxch index
// in the block shifts (original: sum first, fx spilled pre-fsqrt, fy spilled AFTER the
// divisions; ours: both spilled pre-sum). Probes refuted (all byte-identical output): moving
// fy's declaration after the dist assignment, moving it after BOTH divisions. Refuted with a
// regression: swapping the sqrt sum order (DIFF(206), 319 B). Levers that DID matter (kept):
// `double dist;` declared UNINITIALIZED first, then fdx/fdy as raw `(double)` conversions
// (spilled to their own slots, squared through memory, divided IN PLACE after the sqrt) --
// the fused `double fdx = (double)dx / dist;` form mis-assigns every slot below dist; the
// tile coords as a `short aTileXY[2]` pair (forces the word stores + dword re-reads of the
// original; plain short locals stay in registers); dx declared before dy (int slot order).
// The bulldoze-drag erase ResolveWorldClickMaybe's case 4 calls once the cursor has moved at
// least 0x10 pixels from the last erase point: walks the screen-pixel segment from
// (xPrev, yPrev) to (x, y) in 1.0-pixel steps along its unit vector, converts each step's
// position to a tile coordinate (the `x < 0 ? -1 : x >> 4` clamp idiom, here applied to the
// __ftol'd doubles), and flushes whatever occupies the topmost plane-A slot there
// (FlushQueuedBigObjMaybe on g_worldBoard, spawn-effect on). Returns whether anything was
// flushed -- the lone caller ignores the result, but the original materializes it in al
// (`xor bl,bl` / `mov bl,1` / `mov al,bl`), so the return type is a byte.
unsigned char WorldBoardClickView0x455d60::EraseAlongDragSegmentMaybe(int x, int y, int xPrev, int yPrev) // TODO: sync (TU-local view)
{
    int dx = x - xPrev;
    int dy = y - yPrev;
    unsigned char bErased = 0;
    double dist;
    double fdx = (double)dx;
    double fdy = (double)dy;
    double fx = (double)x;
    dist = sqrt(fdx * fdx + fdy * fdy);
    fdx = fdx / dist;
    fdy = fdy / dist;
    double fy = (double)y;
    while (dist > 0.0) {
        short aTileXY[2];
        short sSlot;
        int ix;
        int iy;
        TilePlacedObj *pObj;

        fx += fdx;
        fy += fdy;
        ix = (int)fx;
        aTileXY[0] = ix < 0 ? -1 : (short)(ix >> 4);
        iy = (int)fy;
        aTileXY[1] = iy < 0 ? -1 : (short)(iy >> 4);
        pObj = GetTopPlaneBSlotMaybe(aTileXY[0], aTileXY[1], &sSlot);
        if (pObj != 0) {
            g_worldBoard.FlushQueuedBigObjMaybe(pObj, 1);
            bErased = 1;
        }
        dist -= 1.0;
    }
    return bErased;
}

// FUNCTION: LOCO 0x456d90
// "Does this object's required ring of neighbours hold where it now stands?" -- the placement
// precondition its kind descriptor's InsertSeq record (BigObj +0x560/+0x564) encodes. Starting
// from the object's FRONT-row tile, walks the one-tile-wide perimeter around its footprint
// clockwise -- right along the row above, down the column to its right, left along the row
// below, up the column to its left -- consuming ONE InsertSeq entry per tile as it goes: -1 =
// don't care, 0 = that tile must be EMPTY, anything else = the tile's plane-A occupant must be
// of the kind carrying that resourceId. One running index and one running verdict are shared by
// all four walks, and every one of them stops the moment either the sequence runs out or the
// verdict has already gone false. The footprint extents come from the descriptor's BITMAP
// occupancy pair, not its physical one. Only caller: FUN_00420000.
// sic: the bottom-left corner tile is visited TWICE -- the row-below walk runs one column PAST
// the footprint (down to start.wPosX - 1) and the column-to-the-left walk then starts on that
// very tile, because the x++ that undoes its overshoot has no matching y-- beside it the way the
// two earlier hand-offs do. So a full ring consumes 2*cols + 2*rows + 5 InsertSeq entries rather
// than the 2*cols + 2*rows + 4 tiles it actually covers. See docs/engine-bugs.md.
//
// EFFECTIVE MATCH (DIFF 27 / 752, insns 241/241 -- an IDENTICAL instruction multiset). Every
// byte from offset 0x67 to the end of the COMDAT, all four walks and the out-of-line cold block
// included, is byte-identical; the whole residual is SIX rows of prologue scheduling. The
// original emits [movzx cols, movzx rows, load temp, add posX, add posY]; we emit
// [movzx cols, add posX, movzx rows, ... add posY], which frees pKind's register before pObj's
// instead of after, so the `start.wPosY` reload lands in edi rather than esi -- the one reg_pen.
// Probes run and REJECTED (do not re-run): swapping the two extents' commutative operands
// (`pObj->pos.wPosX + pKind->bBitmapOccupancyCols - 1`) is BYTE-IDENTICAL, cl normalizes
// it; moving `start = ...` after the two extent computations is far worse (DIFF 542 -- the call
// really is first in the source, and hoisting the extents above it forces both across the call);
// declaring x/y ahead of xEnd/yEnd is worse (DIFF 108). Declaration order DID pay twice and is
// load-bearing as written: bOk ahead of pKind took DIFF 56 -> 31 (it is what puts pKind, not
// bOk, in the dead incoming-argument slot at [esp+0x38]), and i after x/y took 31 -> 27.
// A minimal-TU probe (this function + its two headers alone) reproduces DIFF(27) EXACTLY, so
// the residual is INTRINSIC, not /Og TU-state carry -- repositioning it in this file cannot
// help. docs/PARKED.md.
unsigned char WorldBoardPartial::CheckInsertSeqPerimeterMaybe(  // TODO: sync (Ghidra ns is
                                                               // WorldBoardMaybe)
        TilePlacedObj *pObj) {
    char bOk = 1;
    BigObj *pKind = pObj->pKindDesc;
    TileGridPos start;
    start = pObj->GetFrontRowTilePosMaybe();
    short xEnd = pKind->bBitmapOccupancyCols + pObj->pos.wPosX - 1;
    short yEnd = pKind->bBitmapOccupancyRows + pObj->pos.wPosY - 1;
    short x = start.wPosX;
    short y = start.wPosY - 1;
    unsigned int i = 0;
    TilePlacedObj *pOcc;
    long lWant;

    // the row ABOVE the footprint, left to right
    for (; x <= xEnd + 1 && i < pKind->ulInsertSeqCountMaybe && bOk == 1; x++) {
        lWant = pKind->paInsertSeqValues[i];
        if (lWant != -1) {
            if (x < 0 || x >= g_worldBoard.wCols || y < 0 || y >= g_worldBoard.wRows) {
                bOk = 0;
            } else {
                pOcc = aTileCellsMaybe[x * BOARD_TILE_ROWS + y].aPlaneASlotsMaybe[0];
                if ((pOcc == NULL && lWant != 0) ||
                    (pOcc != NULL && lWant != pOcc->pKindDesc->resourceId)) {
                    bOk = 0;
                }
            }
        }
        i++;
    }
    x--;
    y++;
    // the column to the RIGHT of the footprint, top to bottom
    for (; y <= yEnd + 1 && i < pKind->ulInsertSeqCountMaybe && bOk == 1; y++) {
        lWant = pKind->paInsertSeqValues[i];
        if (lWant != -1) {
            if (x < 0 || x >= g_worldBoard.wCols || y < 0 || y >= g_worldBoard.wRows) {
                bOk = 0;
            } else {
                pOcc = aTileCellsMaybe[x * BOARD_TILE_ROWS + y].aPlaneASlotsMaybe[0];
                if ((pOcc == NULL && lWant != 0) ||
                    (pOcc != NULL && lWant != pOcc->pKindDesc->resourceId)) {
                    bOk = 0;
                }
            }
        }
        i++;
    }
    y--;
    x--;
    // the row BELOW the footprint, right to left
    for (; x >= start.wPosX - 1 && i < pKind->ulInsertSeqCountMaybe && bOk == 1; x--) {
        lWant = pKind->paInsertSeqValues[i];
        if (lWant != -1) {
            if (x < 0 || x >= g_worldBoard.wCols || y < 0 || y >= g_worldBoard.wRows) {
                bOk = 0;
            } else {
                pOcc = aTileCellsMaybe[x * BOARD_TILE_ROWS + y].aPlaneASlotsMaybe[0];
                if ((pOcc == NULL && lWant != 0) ||
                    (pOcc != NULL && lWant != pOcc->pKindDesc->resourceId)) {
                    bOk = 0;
                }
            }
        }
        i++;
    }
    x++;
    // the column to the LEFT of the footprint, bottom to top
    for (; y >= start.wPosY - 1 && i < pKind->ulInsertSeqCountMaybe && bOk == 1; y--) {
        lWant = pKind->paInsertSeqValues[i];
        if (lWant != -1) {
            if (x < 0 || x >= g_worldBoard.wCols || y < 0 || y >= g_worldBoard.wRows) {
                bOk = 0;
            } else {
                pOcc = aTileCellsMaybe[x * BOARD_TILE_ROWS + y].aPlaneASlotsMaybe[0];
                if ((pOcc == NULL && lWant != 0) ||
                    (pOcc != NULL && lWant != pOcc->pKindDesc->resourceId)) {
                    bOk = 0;
                }
            }
        }
        i++;
    }
    return bOk;
}


// The two kind-descriptor reads the track-graph walkers make through a placed object that may
// carry no descriptor at all. Deliberately NOT GetPlacedObjCategoryMaybe above: that one is
// widened to `unsigned short` because FindNearestObjOfCategoryMaybe's compare is 16-bit, where
// the walker below compares a plain byte (`cmp cl,0xc`).
static inline unsigned char GetPlacedObjCategoryByteMaybe(TilePlacedObj *pTile) {
    return pTile->pKindDesc == 0 ? (unsigned char)0 : pTile->pKindDesc->categoryByte;
}
static inline int GetPlacedObjKindIdMaybe(TilePlacedObj *pTile) {
    return pTile->pKindDesc == 0 ? -1 : pTile->pKindDesc->resourceId;
}

// GetBoardPlaneASlotMaybe's `this`-relative twin: the same inlined plane-A slot lookup (same
// off-by-one `>` bound checks, same flat col*BOARD_TILE_ROWS+row index, same fold of slot 0),
// but reached through the board object the caller already holds rather than through the
// g_worldBoard global. The track-graph walkers are methods, so the original addresses the grid
// off `this` (`mov esi,[ebx+ebp*1+0x48]`) where the ring search addresses it absolutely.
static inline TilePlacedObj *GetBoardPlaneASlotOfMaybe(WorldBoardPartial *pBoard, short col,
                                                       short row) {
    if (col < 0 || col > BOARD_TILE_COLS || row < 0 || row > BOARD_TILE_ROWS) {
        return 0;
    }
    return pBoard->aTileCellsMaybe[col * BOARD_TILE_ROWS + row].aPlaneASlotsMaybe[0];
}

// FUNCTION: LOCO 0x4573e0
// "Is pTile a JUNCTION of graph A" -- i.e. does it deserve a TrackGraphNode of its own rather
// than being a plain run of path/track between two junctions. Four steps:
//  (1) walk each of the four directions THROUGH any intervening plain runs (BigObj::
//      IsPlainRunMaybe) until a non-run tile is reached, then drop that branch entirely if
//      TrackGraph_GetLinkCostAMaybe says the two kinds do not connect;
//  (2) count the surviving branches under an OPPOSITE-PAIR rule -- branch i only counts when
//      the perpendicular branch i+1 also survived, or the branch straight opposite it did not.
//      A tile with exactly two opposite branches is therefore a plain pass-through and scores 0;
//  (3) for graph selector 7 only, and only on a full 4-way crossing, discount the four DIAGONAL
//      neighbours whose kind category is 0xc (path) -- a crossing hemmed in by footpaths on all
//      four corners is not a junction the vehicles can turn at;
//  (4) two specific kinds force a node regardless of the count.
// Nonzero = node. Called from TrackGraph::BuildAdjacencyAMaybe (src/TrackGraph.cpp).
//
// sic: step (3) OVERWRITES the branch array with the four diagonals, so step (4) inspects the
// diagonals rather than the orthogonal branches on exactly the paths that reach it -- the
// scratch array is reused for two unrelated purposes. The out-of-line cold block at 0x457695
// (which reloads apBranch[2]/apBranch[3] into esi/ebx before rejoining step 4) is the compiler
// making the two paths agree about that, so the reuse is in the original source, not a
// misreading. See docs/engine-bugs.md.
//
// The GetNeighborObject casts are the shared header's declaration returning TilePlacedObjPartial*
// so src/NetSessionEventQueue.cpp can keep holding the result in its own TU-local view; the
// object really is a TilePlacedObj (same debt as PlaceObject's own return type).
//
// EXACT. THREE source shapes were load-bearing, each worth a documented lever:
//  (1) the SEPARATE COUNTDOWN `n` in the first two loops is the whole difference between
//      DIFF(610) and MATCH. Where a loop's index is still LIVE IN THE BODY (`dir` is an argument
//      to GetNeighborObject; `i` feeds the two `& 3` subscripts), cl uses that index for the exit
//      test and emits `cmp dir,4 / jl`. The original instead emits `dec n / jne` beside a
//      still-incrementing index -- an extra trip counter, spilled to [esp+0x14] in the first loop
//      (which is exactly the 4 bytes by which the original's `sub esp,0x1c` frame is bigger than
//      an index-controlled one) and held in esi in the second. cl does not invent that counter;
//      the source has to. ⚠ Contrast the THIRD loop, left as a plain `for (i = 0; i < 4; i++)`
//      on purpose: `i` is dead in that body, so cl eliminates it and synthesises the reversed
//      counter itself (`mov eax,4 / dec eax / jne`) -- writing an explicit `n` there too would be
//      unfaithful. The tell for which one a loop needs is whether the index survives into the
//      body. A pointer-controlled loop (`for (pp = a; pp < a + 4; pp++)`) is NOT the same thing
//      and was REJECTED at 104175: cl keeps the pointer compare (`lea edx,[esp+0x28] / cmp / jb`).
//  (2) TrackGraph_GetLinkCostAMaybe is a __thiscall MEMBER, not the free __stdcall function it
//      was first transcribed as -- the `mov ecx,ebp` in front of its call site here is the only
//      evidence anywhere, since the callee never reads `this` and so matches byte-for-byte under
//      either spelling (`ret 0x8` both ways). See its own note above.
//  (3) the inner null test is spelled `!= NULL` first (65384 vs 95391): the original's `je` skips
//      FORWARD to the null arm, so the descriptor load is the fall-through. Lever #18n again, and
//      again worth reaching for before any register-noise triage.
char WorldBoardPartial::IsTrackGraphNodeAMaybe(TilePlacedObj *pTile,  // TODO: sync
                                              short nCategoryMaybe) {
    if (pTile == NULL) {
        return 0;
    }

    BigObj *pKind = pTile->pKindDesc;
    if (pKind->IsPlainRunMaybe()) {
        return 0;
    }

    TilePlacedObj *apBranch[4];
    int dir;
    int i;
    int n;

    dir = 0;
    for (n = 4; n != 0; n--) {
        apBranch[dir] = (TilePlacedObj *)GetNeighborObject((TilePlacedObjPartial *)pTile, dir);
        if (apBranch[dir] != NULL) {
            BigObj *pFarKind = apBranch[dir]->pKindDesc;
            while (pFarKind != NULL && pFarKind->IsPlainRunMaybe()) {
                apBranch[dir] = (TilePlacedObj *)GetNeighborObject(
                    (TilePlacedObjPartial *)apBranch[dir], dir);
                if (apBranch[dir] != NULL) {
                    pFarKind = apBranch[dir]->pKindDesc;
                } else {
                    pFarKind = NULL;
                }
            }
            if (apBranch[dir] != NULL && TrackGraph_GetLinkCostAMaybe(pKind, pFarKind) < 0) {
                apBranch[dir] = NULL;
            }
        }
        dir++;
    }

    char nBranches = 0;
    i = 0;
    for (n = 4; n != 0; n--) {
        if (apBranch[i] != NULL &&
            (apBranch[(i + 1) & 3] != NULL || apBranch[(i - 2) & 3] == NULL)) {
            nBranches++;
        }
        i++;
    }

    if (nCategoryMaybe == 7 && nBranches == 4) {
        apBranch[0] =
            GetBoardPlaneASlotOfMaybe(this, pTile->pos.wPosX - 1, pTile->pos.wPosY - 1);
        apBranch[1] =
            GetBoardPlaneASlotOfMaybe(this, pTile->pos.wPosX - 1, pTile->pos.wPosY + 1);
        apBranch[2] =
            GetBoardPlaneASlotOfMaybe(this, pTile->pos.wPosX + 1, pTile->pos.wPosY - 1);
        apBranch[3] =
            GetBoardPlaneASlotOfMaybe(this, pTile->pos.wPosX + 1, pTile->pos.wPosY + 1);
        for (i = 0; i < 4; i++) {
            if (apBranch[i] != NULL && GetPlacedObjCategoryByteMaybe(apBranch[i]) == 12) {
                nBranches--;
            }
        }
    }

    if ((apBranch[0] != NULL && GetPlacedObjKindIdMaybe(apBranch[0]) == 0xc50) ||
        (apBranch[1] != NULL && GetPlacedObjKindIdMaybe(apBranch[1]) == 0xc52) ||
        (apBranch[2] != NULL && GetPlacedObjKindIdMaybe(apBranch[2]) == 0xc50) ||
        (apBranch[3] != NULL && GetPlacedObjKindIdMaybe(apBranch[3]) == 0xc52)) {
        nBranches = 1;
    }
    return nBranches;
}

// FUNCTION: LOCO 0x457c20
// TrackGraph_GetLinkCostAMaybe's graph-B twin: the ROAD network's edge cost between two adjacent
// tile kinds, or -1 when the two do not connect at all. Every accepted edge costs the same flat
// 100 -- graph B has no rail-versus-foot preference to express the way graph A's 50/10 split
// does, so this is a pure connectivity table rather than a weighting. Categories are 3 = track,
// 0xc = path, 0xd = road, and the table reads:
//   road  -> road, road -> path            always connect
//   road  -> track, track -> road          only across a points/switch orientation (set {0x10,0x11})
//   path  -> road                          only when the path side is a plain run
//   path  -> path                          only when BOTH sides are plain runs
// Everything else, including the whole of category 3 on the B side unless it is that same
// points/switch orientation, is no edge.
//
// A MEMBER that never reads `this`, exactly like its graph-A twin: the entry `mov ecx,[esp+8]`
// clobbers ecx immediately, and only IsTrackGraphNodeBMaybe's `mov ecx,ebp` in front of the call
// says the argument was ever passed. `ret 0x8` either way.
//
// The `case 12: break;` arms are in the ORIGINAL, not padding: cl lowers each of these switches
// as a `sub`/`je` chain over the cases actually present (`sub esi,0xc / je / dec esi / jne`), so
// an arm that merely falls through to the -1 default is still visible in the codegen. A lone
// `if (catA == 13)` would have compiled to a single `cmp`.
int WorldBoardPartial::TrackGraph_GetLinkCostBMaybe(BigObj *pKindA, BigObj *pKindB) {  // TODO: sync
    unsigned short catA = pKindA->categoryByte;
    switch (pKindB->categoryByte) {
    case 3:
        if (pKindB->IsType0x63aInSet10()) {
            switch (catA) {
            case 12:
                break;
            case 13:
                return 100;
            }
        }
        break;
    case 12:
        if (pKindB->IsPlainRunMaybe()) {
            switch (catA) {
            case 12:
                return pKindA->IsPlainRunMaybe() ? 100 : -1;
            case 13:
                return 100;
            }
        }
        break;
    case 13:
        switch (catA) {
        case 3:
            return pKindA->IsType0x63aInSet10() ? 100 : -1;
        case 12:
            return 100;
        case 13:
            return 100;
        }
        break;
    }
    return -1;
}

// FUNCTION: LOCO 0x4576b0
// IsTrackGraphNodeAMaybe's graph-B twin -- "is pTile a junction of the ROAD network". Identical
// code against graph B's own link-cost table, minus the selector-7 diagonal discount: steps (1),
// (2) and (4) of the A form, with step (3) simply absent. See that function for the full
// writeup and for the three load-bearing source shapes reproduced here.
//
// nCategoryMaybe is accepted and never read. Both node tests are called from their TrackGraph's
// build pass with that graph's own nGraphKindMaybe (`mov cx,[ebp+0xc] / push ecx`, at 0x45cebb
// and 0x45d23b), so the two share one signature and only A does anything with the selector --
// which is exactly why this one still returns via `ret 0x8`.
char WorldBoardPartial::IsTrackGraphNodeBMaybe(TilePlacedObj *pTile, short) {  // TODO: sync
    if (pTile == NULL) {
        return 0;
    }

    BigObj *pKind = pTile->pKindDesc;
    if (pKind->IsPlainRunMaybe()) {
        return 0;
    }

    TilePlacedObj *apBranch[4];
    int dir;
    int i;
    int n;

    dir = 0;
    for (n = 4; n != 0; n--) {
        apBranch[dir] = (TilePlacedObj *)GetNeighborObject((TilePlacedObjPartial *)pTile, dir);
        if (apBranch[dir] != NULL) {
            BigObj *pFarKind = apBranch[dir]->pKindDesc;
            while (pFarKind != NULL && pFarKind->IsPlainRunMaybe()) {
                apBranch[dir] = (TilePlacedObj *)GetNeighborObject(
                    (TilePlacedObjPartial *)apBranch[dir], dir);
                if (apBranch[dir] != NULL) {
                    pFarKind = apBranch[dir]->pKindDesc;
                } else {
                    pFarKind = NULL;
                }
            }
            if (apBranch[dir] != NULL && TrackGraph_GetLinkCostBMaybe(pKind, pFarKind) < 0) {
                apBranch[dir] = NULL;
            }
        }
        dir++;
    }

    char nBranches = 0;
    i = 0;
    for (n = 4; n != 0; n--) {
        if (apBranch[i] != NULL &&
            (apBranch[(i + 1) & 3] != NULL || apBranch[(i - 2) & 3] == NULL)) {
            nBranches++;
        }
        i++;
    }

    if ((apBranch[0] != NULL && GetPlacedObjKindIdMaybe(apBranch[0]) == 0xc50) ||
        (apBranch[1] != NULL && GetPlacedObjKindIdMaybe(apBranch[1]) == 0xc52) ||
        (apBranch[2] != NULL && GetPlacedObjKindIdMaybe(apBranch[2]) == 0xc50) ||
        (apBranch[3] != NULL && GetPlacedObjKindIdMaybe(apBranch[3]) == 0xc52)) {
        nBranches = 1;
    }
    return nBranches;
}

#include "TrackGraph.h" // the two graphs BuildTrackGraphsIfReadyMaybe rebuilds

#include "ThreadWrapper.h" // g_worldLoadThread -- the idle pump FUN_00457320 waits out
extern ThreadWrapper g_worldLoadThread; // DAT_004a9ad0, see src/FrameDriver.cpp

// FUNCTION: LOCO 0x457320
// The force-rebuild entry, and the other half of the handshake with the world idle thread
// (WorldIdleEventPumpThreadProc, src/WorldActionCursor.cpp). Three steps, in order:
//   1. drop bTrackGraphsBuiltFlag, so the next BuildTrackGraphsIfReadyMaybe below rebuilds;
//   2. BUSY-WAIT for the idle thread to actually exit, 50 ms at a time. That thread's own body
//      does the once-per-world graph build behind the same flag, so rebuilding underneath it
//      would race -- this is the join, done by polling rather than by a wait handle;
//   3. rebuild each graph's adjacency, skipping a graph that was never allocated.
// Note the asymmetry with BuildTrackGraphsIfReadyMaybe: that one re-checks the screen state
// between the two graphs, this one does not -- by here the idle thread is provably gone.
void WorldBoardPartial::FUN_00457320() { // TODO: idiom  // TODO: sync (partial-view class name)
    bTrackGraphsBuiltFlag = 0;
    while (g_worldLoadThread.IsRunning()) {
        Sleep(50);
    }
    if (pTrackGraphAMaybe != NULL) {
        pTrackGraphAMaybe->BuildAdjacencyAMaybe();
    }
    if (pTrackGraphBMaybe != NULL) {
        pTrackGraphBMaybe->BuildAdjacencyBMaybe();
    }
}

// FUNCTION: LOCO 0x457380
// The "both track graphs are built" step: while the app is in screen state 3, ask each of the
// two TrackGraphs that has any nodes at all to rebuild all its routes, then raise
// bTrackGraphsBuiltFlag. The screen state is re-tested BETWEEN the two graphs, so a state change
// during graph A's rebuild leaves graph B untouched and the flag down -- which is what makes
// FUN_00457320 (the force-rebuild entry, which only clears that flag) sufficient to schedule a
// retry. Both gates go through IsInGameModeMaybe for its `unsigned char` return: that is what
// reproduces the original's sete-materialized test (`xor eax,eax / cmp / sete al / test al,al`)
// rather than a plain `cmp / jne`.
void WorldBoardPartial::BuildTrackGraphsIfReadyMaybe() {  // TODO: sync
    if (IsInGameModeMaybe()) {
        if (pTrackGraphAMaybe != NULL && pTrackGraphAMaybe->nNodeCount != 0) {
            pTrackGraphAMaybe->BuildAllRoutesMaybe();
        }
        if (IsInGameModeMaybe()) {
            if (pTrackGraphBMaybe != NULL && pTrackGraphBMaybe->nNodeCount != 0) {
                pTrackGraphBMaybe->BuildAllRoutesMaybe();
            }
            bTrackGraphsBuiltFlag = 1;
        }
    }
}

// FUNCTION: LOCO 0x457830
// Fill pTile's four graph-A neighbour slots. For each direction, walk outward one tile at a time
// accumulating the run's cost, and stop at the first tile that is either unreachable (negative
// link cost), already a graph node of its own, or off the board. The tile RECORDED in the slot is
// the last non-plain-run tile seen -- so a run of plain track/path between two junctions collapses
// into one edge whose cost is the sum of its steps, which is exactly the edge
// TrackGraph::BuildAdjacencyAMaybe then shares between the two nodes.
//
// Note the cost is accumulated for EVERY accepted step including the one that lands on the next
// junction, while the slot is only updated on non-plain-run tiles: the two are deliberately not
// the same test.
void WorldBoardPartial::LinkTrackGraphNeighboursAMaybe(TilePlacedObj *pTile) {  // TODO: sync
    if (pTile != NULL) {
        BigObj *pKind = pTile->pKindDesc;
        int dir;
        int n;

        dir = 0;
        for (n = 4; n != 0; n--) {
            pTile->apNeighbourTileAMaybe[dir] = 0;
            pTile->anNeighbourCostAMaybe[dir] = 0;
            TilePlacedObj *pStep =
                (TilePlacedObj *)GetNeighborObject((TilePlacedObjPartial *)pTile, dir);
            while (pStep != NULL) {
                BigObj *pStepKind = pStep->pKindDesc;
                int nCost = TrackGraph_GetLinkCostAMaybe(pKind, pStepKind);
                if (nCost < 0) {
                    break;
                }
                if (!pStepKind->IsPlainRunMaybe()) {
                    pTile->apNeighbourTileAMaybe[dir] = pStep;
                }
                pTile->anNeighbourCostAMaybe[dir] += nCost;
                if (pStep->nGraphNodeIdAMaybe >= 0) {
                    break;
                }
                pStep = (TilePlacedObj *)GetNeighborObject((TilePlacedObjPartial *)pStep, dir);
            }
            dir++;
        }
    }
}

// FUNCTION: LOCO 0x457900
// LinkTrackGraphNeighboursAMaybe's graph-B twin -- the same walk against the tile's B-side
// neighbour/cost/node-id trio and graph B's own link-cost table.
void WorldBoardPartial::LinkTrackGraphNeighboursBMaybe(TilePlacedObj *pTile) {  // TODO: sync
    if (pTile != NULL) {
        BigObj *pKind = pTile->pKindDesc;
        int dir;
        int n;

        dir = 0;
        for (n = 4; n != 0; n--) {
            pTile->apNeighbourTileBMaybe[dir] = 0;
            pTile->anNeighbourCostBMaybe[dir] = 0;
            TilePlacedObj *pStep =
                (TilePlacedObj *)GetNeighborObject((TilePlacedObjPartial *)pTile, dir);
            while (pStep != NULL) {
                BigObj *pStepKind = pStep->pKindDesc;
                int nCost = TrackGraph_GetLinkCostBMaybe(pKind, pStepKind);
                if (nCost < 0) {
                    break;
                }
                if (!pStepKind->IsPlainRunMaybe()) {
                    pTile->apNeighbourTileBMaybe[dir] = pStep;
                }
                pTile->anNeighbourCostBMaybe[dir] += nCost;
                if (pStep->nGraphNodeIdBMaybe >= 0) {
                    break;
                }
                pStep = (TilePlacedObj *)GetNeighborObject((TilePlacedObjPartial *)pStep, dir);
            }
            dir++;
        }
    }
}

// "This kind declares no entry/exit point on edge nEdge at all", the gate GetNeighborObject runs
// before it bothers resolving a neighbour. The `unsigned char` return is load-bearing for the
// same reason IsInGameModeMaybe's is: it reproduces the original's MATERIALIZED boolean
// (`mov eax,1 / jmp / xor eax,eax / test al,al / jne`) instead of a plain branch to the exit.
static inline unsigned char HasNoEntryExitOnEdgeMaybe(BigObj *pKind, int nEdge) {
    return pKind->aEntryExitMaybe[nEdge * 2] == 0 && pKind->aEntryExitMaybe[nEdge * 2 + 1] == 0;
}

// FUNCTION: LOCO 0x4579d0
// The placed object on the board tile adjacent to pItem across edge `direction` (0 = -x, 1 = +y,
// 2 = +x, 3 = -y), or NULL. This is the primitive all four track-graph walkers step with, and it
// is a good deal more than a grid offset: the step is taken from pItem's own ENTRY/EXIT POINT on
// that edge, not from its tile position, and the result is only accepted if the two objects'
// facing entry/exit points actually meet.
//   - a kind whose entry_exit record leaves this edge at (0, 0) has no connection there at all;
//   - the walk starts at this edge's entry/exit point converted to tile coordinates, and steps
//     one tile in the edge's direction;
//   - the neighbour is then asked for ITS entry/exit point on the OPPOSITE edge ((dir - 2) & 3),
//     and the two points must lie within a squared distance of 0x11 (i.e. ~4 pixels) of each
//     other. A neighbour whose own connector faces elsewhere is rejected, which is what stops a
//     run of track from turning a corner it does not physically make.
// A neighbour with no kind descriptor skips the proximity test and is returned as-is.
//
// The four switch arms each carry their own inlined slot lookup rather than sharing one after the
// switch -- that is what the original emits, and cl then cross-jumps the two pairs whose tails
// are identical (arms 0/2 both leave the row in ax, arms 1/3 both leave it in dx), which is why
// only two copies of the index computation survive into the binary.
//
// EFFECTIVE MATCH (DIFF 52 / 392, insns 146/145). The COMDAT is 392 bytes -- 376 of code, a
// 3-byte alignment nop and the 16-byte jump table -- so `--len` must come from the next
// function's start (0x457b60), not from Ghidra's `Body:` span, which stops at the last
// INSTRUCTION and would truncate the window by the whole table. Everything outside four rows is
// byte-identical, and those four are one instruction of register allocation around the SECOND
// entry/exit point: the original still has `pt` occupying [esp+0x10] when it materializes
// `ptFar` and so gives ptFar its own retbuf at [esp+0x18], reading it back through the returned
// pointer (`mov ecx,[eax+4]`); we let cl reuse pt's slots for ptFar and reach them off esp.
//
// TWO source shapes here were load-bearing and are worth more than this function:
//  (1) the two entry guards are ONE `||`, not two separate `if`s (212002 -> 91964, and it is
//      what took the length from 412 to 396). Two `return NULL;` statements each got their own
//      inlined epilogue; short-circuited into one condition, both arms jump to a single shared
//      exit, which is exactly the original's `je 0x169` / `jne 0x169` pair. When a dump shows
//      duplicated epilogues against an original that has one, look for a missing `||`.
//  (2) `pt` is ASSIGNED and `ptFar` is INITIALIZED, and the two spellings are NOT
//      interchangeable -- this is lever #34 read in both directions within one function.
//      `POINT pt; pt = f();` emits a copy out of the returned temp into the local (here a
//      REDUNDANT one, since cl gave temp and local the same slot: `mov eax,[eax+4]` /
//      `mov [esp+0x14],eax` writes pt.y over itself), while `POINT ptFar = f();` forwards the
//      retbuf and emits no copy at all. Initializing `pt` instead cost 249974 vs 68911.
// Probes run and REJECTED (do not re-run): hoisting the CalcSqDist result into a named local,
// and moving the `POINT pt;` declaration to the top of the function -- both BYTE-IDENTICAL.
// docs/PARKED.md.
TilePlacedObjPartial *WorldBoardPartial::GetNeighborObject(  // TODO: sync
    TilePlacedObjPartial *pItemArg, int direction) {
    POINT pt;
    TilePlacedObj *pItem = (TilePlacedObj *)pItemArg;
    TilePlacedObj *pNeighbour = NULL;

    BigObj *pKind = pItem->pKindDesc;
    if (pKind == NULL || HasNoEntryExitOnEdgeMaybe(pKind, direction)) {
        return NULL;
    }

    pt = pItem->GetEntryExitPointMaybe(direction);
    short col = WORLD_TO_TILE(pt.x);
    short row = WORLD_TO_TILE(pt.y);
    switch (direction) {
    case 0:
        pNeighbour = GetBoardPlaneASlotOfMaybe(this, col - 1, row);
        break;
    case 1:
        pNeighbour = GetBoardPlaneASlotOfMaybe(this, col, row + 1);
        break;
    case 2:
        pNeighbour = GetBoardPlaneASlotOfMaybe(this, col + 1, row);
        break;
    case 3:
        pNeighbour = GetBoardPlaneASlotOfMaybe(this, col, row - 1);
        break;
    }

    if (pNeighbour != NULL && pNeighbour->pKindDesc != NULL) {
        POINT ptFar = pNeighbour->GetEntryExitPointMaybe((direction - 2) & 3);
        if (CalcSqDist(pt.x, pt.y, ptFar.x, ptFar.y) > 0x11) {
            return NULL;
        }
    }
    return (TilePlacedObjPartial *)pNeighbour;
}

// Cross-TU callees with no shared-header declaration yet (the FrameDriver.cpp pattern).
// The EffectSpawner one MUST name the real class in its signature even with no
// EffectSpawner.h include -- a TU-local-view-typed declaration would mangle to a different
// symbol, the v445 byte-invisible wrong-call-target class.
class EffectSpawner; // fwd -- src/EffectSpawner.h (this TU carries its own TU-local view
                     // of DAT_004fd220 instead, see 0x456700)
void __fastcall EffectSpawner_ClearBothListsMaybe(EffectSpawner *pSpawner);   // 0x423d00, src/EffectSpawner.cpp
void __fastcall NetSessionEventQueue_ResetPlacedObjectsMaybe(NetSessionEventQueue *pQueue); // 0x41e100, src/NetSessionEventQueue.cpp
void __fastcall WorldBoardMaybe_ResetAllTilesMaybe(WorldBoardPartial *pBoard);   // 0x454fe0, defined below
void __fastcall WorldBoardMaybe_ShutdownMaybe(WorldBoardPartial *pBoard);        // 0x454de0, the AppWindow.cpp escape-hatch decl

// The class vtable (0x478520), stored at +0x0 by the ctor below. Same extern-symbol form as
// PeerTrainNode.cpp's g_vtable0x47836c (resolved by the link smoke-test scaffolding).
extern void *g_vtable0x478520[];

// FUNCTION: LOCO 0x454cf0  // TODO: sync (deliberate: free __fastcall escape-hatch form; Ghidra keeps the real ctor name WorldBoardMaybe::WorldBoardMaybe)
// The WorldBoardMaybe CONSTRUCTOR: stamp the vtable, wipe every tile (ResetAllTilesMaybe's
// whole job), zero the scroll offsets, then heap-construct the two TrackGraphs side by side
// (`new TrackGraph(7)` graph A over the paths network, `new TrackGraph(8)` graph B over the
// roads network -- src/TrackGraph.cpp), null the dirty-bitmap pointer and the two flag bytes,
// and zero the 31-dword DDSurfaceDesc scratch (the rep stosd IS the compiler's memset
// recognition). Written as the same free __fastcall escape-hatch form as its two neighbours
// below: WorldBoardPartial models +0x0 as pad, not a vftable member, and the shared header's
// declaration count is load-bearing. Returns `this` in EAX, ctor-style.
WorldBoardPartial *__fastcall WorldBoardMaybe_InitFieldsMaybe(WorldBoardPartial *pBoard)
{
    *(void **)pBoard = g_vtable0x478520;
    WorldBoardMaybe_ResetAllTilesMaybe(pBoard);
    pBoard->dwScrollX = 0;
    pBoard->dwScrollY = 0;
    pBoard->pTrackGraphAMaybe = new TrackGraph(7);
    pBoard->pTrackGraphBMaybe = new TrackGraph(8);
    pBoard->pDirtyBitmapMaybe = 0;
    pBoard->bFlag0x3cMaybe = false;
    pBoard->bSurfaceLockGuard = 0;
    memset(pBoard->aSurfaceDescScratch, 0, sizeof(pBoard->aSurfaceDescScratch));
    return pBoard;
}

// FUNCTION: LOCO 0x454de0 (Ghidra: WorldBoardMaybe::FUN_00454de0)
// Full board teardown, SaveWindowAndCleanExit's board half: wipe every tile (the 0x454fe0
// reset above's whole job), then destroy+free both track graphs (graph A first) and finally
// the dirty bitmap. Same free __fastcall escape-hatch form as its callee (the AppWindow.cpp
// declaration already exists), keeping WorldBoardMaybe.h's measured declaration count
// untouched. TODO: idiom.
void __fastcall WorldBoardMaybe_ShutdownMaybe(WorldBoardPartial *pBoard)
{
    WorldBoardMaybe_ResetAllTilesMaybe(pBoard);
    if (pBoard->pTrackGraphAMaybe != 0) {
        delete pBoard->pTrackGraphAMaybe;
        pBoard->pTrackGraphAMaybe = 0;
    }
    if (pBoard->pTrackGraphBMaybe != 0) {
        delete pBoard->pTrackGraphBMaybe;
        pBoard->pTrackGraphBMaybe = 0;
    }
    if (pBoard->pDirtyBitmapMaybe != 0) {
        delete pBoard->pDirtyBitmapMaybe;
        pBoard->pDirtyBitmapMaybe = 0;
    }
}

// FUNCTION: LOCO 0x454fe0 (Ghidra: WorldBoardMaybe::WorldBoardMaybe_ResetAllTilesMaybe)
// Full-board wipe, called on world load (LoadLayoutAndPopulateBoard), on teardown
// (SaveWindowAndCleanExit via the 0x454de0 shutdown wrapper, AbortMultiplayerSession) and
// from App_LoadWorldThreadProcMaybe / InitFieldsMaybe: release the hover object, tear down
// all four train slots, clear both effect collections, reset the placed-object registry,
// zero the whole tile-cell array (the 0x14910-dword rep stosd IS the compiler's memset
// recognition), all-1s-fill the dirty bitmap (signed nTiles/8 + 1 bytes -- the cdq/and-7/
// sar idiom, shorts movsx'd), reset every cell's two plane counts to -1 (col-major flat
// index, bPlaneB stored before bPlaneA), and force a repaint unless the app is in state 1.
// Defined as the free __fastcall AppWindow.cpp already declares (the escape-hatch
// pattern): keeps WorldBoardMaybe.h's measured declaration count untouched. TODO: idiom.
// ⚠ v552: flipped OFF again, DIFF(2) at 211 B -- the SAME commutative-load coin flip described
// below, so nothing here was re-autopsied. Trigger: this TU's five TU-local views of the two
// widget singletons were folded onto their canonical classes (see the WorldActionCursor.h
// include at the top of the file). That is the "landing ANY new body re-rolls it" rule below
// running in the losing direction, and it is the WHOLE measured price of the fold -- no other
// function in this TU and no other TU in the repo moved by a byte. Deliberately paid: the views
// were a real duplicate model of classes that already exist, which is the one thing this project
// does not allow itself. Retry only on the next TU-context rotation, as always.
// ⚠ REFUTED v552, do NOT re-run: re-spelling this free __fastcall as the real member
// `WorldBoardPartial::ResetAllTilesMaybe()` -- the CODEGEN #149 move that closed 0x42d280 this
// same session -- is INERT here. Still DIFF(2), byte-identical residual, so #149 is a lever to
// TRY on a __fastcall-escape-hatch residual, NOT a general rule that the form is always the
// cause. And it is not free to try: the one member declaration it needs on src/WorldBoardMaybe.h
// costs 0x457ce0 its full 951 B (that header's documented counter). Net -951 for +0.
// ⭐ EXACT again as of 2026-08-01, and the history is the lesson. It was exact, fell to
// DIFF(2) when v522's 0x456c60 body rotated this TU's /Og state (the v479 definition-count
// parity class), and its park said "retry only on TU-context rotation". Adding
// GetPlaneASlotGridPosMaybe's body (0x4557c0) this session supplied exactly that rotation and
// handed all 211 B back for free -- the residual had been the memset-count
// `(wRows * wCols) / 8 + 1` imul pair loading wRows (+0x40) before wCols (+0x3e) instead of the
// other way round, a commutative-load coin-flip no source spelling could reach (cl canonicalizes
// the operand order, so the obvious flip is byte-identical).
// The general form: a parity-class residual is not owned by the function it appears in. Landing
// ANY new body in the TU re-rolls it, in either direction -- which is the case FOR landing
// declared-only bodies one at a time and reading the whole per-file table each time, not just
// the row you were aiming at.
void __fastcall WorldBoardMaybe_ResetAllTilesMaybe(WorldBoardPartial *pBoard)
{
    PlacementCursorMaybe_004854c8.ReleaseHoverObjMaybe();
    g_PeerTrainSlotQueue.TeardownAllSlotsMaybe();
    EffectSpawner *pSpawner = (EffectSpawner *)&DAT_004fd220; // idiom-exempt: the TU-local paint view IS the EffectSpawner singleton
    EffectSpawner_ClearBothListsMaybe(pSpawner);
    NetSessionEventQueue_ResetPlacedObjectsMaybe(&g_NetSessionEventQueue);
    memset(pBoard->aTileCellsMaybe, 0, sizeof(pBoard->aTileCellsMaybe));
    if (pBoard->pDirtyBitmapMaybe != 0) {
        memset(pBoard->pDirtyBitmapMaybe, 0xff,
               (pBoard->wRows * pBoard->wCols) / 8 + 1);
    }
    for (int row = 0; row < BOARD_TILE_ROWS; row++) {
        for (int col = 0; col < BOARD_TILE_COLS; col++) {
            pBoard->aTileCellsMaybe[col * BOARD_TILE_ROWS + row].bPlaneBCountMaybe = -1;
            pBoard->aTileCellsMaybe[col * BOARD_TILE_ROWS + row].bPlaneACountMaybe = -1;
        }
    }
    if (g_pApp != NULL && g_pApp->hwndOwner != NULL && !IsAppStateOneMaybe()) {
        InvalidateRect(g_pApp->hwndOwner, NULL, 0);
        UpdateWindow(g_pApp->hwndOwner);
    }
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 12 of 13: g_worldBoard (DAT_004aad08), ctor 0x454cf0. Transcribed in the free __fastcall
// escape-hatch form, so this is a plain call rather than a placement new.
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_g_worldBoard(void) {
    WorldBoardMaybe_InitFieldsMaybe(&g_worldBoard);
}
#endif // LOCO_PORT
