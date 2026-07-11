// AlbumCardWnd -- the card-album screen (0x254/596 bytes; ctor 0x401f50, class
// ALBUMCARDWINDOWCLASS id 507). Resolution-gated (800x600 threshold, nLayoutMode) 6x3
// ResourceRef* grid (paCardGrid, 18 entries -- really 3 parallel 6-entry sub-arrays: [0..5]
// a per-slot placeholder/erase rect, [6..11] a per-slot strip-frame draw handle passed to
// ResourceRef's own DrawFrame, [12..17] used by the sibling full-redraw
// RedrawAllSlots for the same per-slot rects) + a 9-slot ResourceRef* array (paCardSlots,
// purpose not yet traced) + 8 button-icon ResourceRef* slots (+0x148..+0x164, dispatched
// by DrawButtonIcon's nCase switch -- back/forward paging arrows confirmed, the other 6 not yet
// traced to a specific UI element) + a 6-byte paging-arrow enable/edge-state block (+0x1d4..
// +0x1d9). See docs/subsystems.md's "AlbumCardWnd" entry.
//
// Field layout built directly from AlbumCardWnd_InitResourceRefs's (0x401fd0) own
// zero-init writes plus this TU's two transcribed methods' field accesses -- every access
// already goes through a named field. Fields the ctor touches but neither transcribed method
// reads back are left named Unk0xNN (size/existence confirmed by the ctor's own write, purpose
// not yet traced). Gaps the ctor doesn't touch either are left as plain padding.
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "WindowBase.h"
#include "ResourceRef.h"
#include "PostBag.h"

struct LocoBitmap;
class CarNetState;

class AlbumCardWnd : public WindowBase {
public:
    // 0x401f50 -- chains the WindowBase base ctor then AlbumCardWnd_InitResourceRefs below.
    // Not yet byte-matched -- see src/AlbumCardWnd.cpp.
    AlbumCardWnd(void *hInstanceParam, UINT resourceIdParam);

    // 0x401fd0 -- zero-initializes every scalar field the ctor owns and realizes every
    // ResourceRef-backed icon/grid/tab field. Not yet byte-matched -- see
    // src/AlbumCardWnd.cpp.
    void AlbumCardWnd_InitResourceRefs();

    HICON hIcon; // +0xe8 -- set by the shared CreateFullscreenPopupWnd helper
                       // (src/WindowBase.cpp, called from AppWindow_ConstructSingletonWindows),
                       // not by this class's own ctor -- see FullscreenPopupWndPartial there.
    // +0xec -- the background-tile SOURCE rect: RefreshClientClipRect CopyRect's the client
    // clip bounds in here, then recenters it inside the loaded tile's own native extent
    // (CenterRectInRect against a 0,0,tileW,tileH scratch rect). DrawOrEraseCardSlot then
    // offsets each slot's own rect by this rect's left/top to reach the matching pixels of
    // pBackBuffer. Confirmed a real 16-byte RECT (not the 2 loose ints + 8 pad bytes it was
    // modeled as before) by RefreshClientClipRect's own single 16-byte CopyRect write.
    RECT rectBackBufSrc; // +0xec
    bool bBackgroundTileLoaded; // +0xfc -- gates EnsureBackgroundTileLoaded's own
                                       // lazy-load (separate from bWantEraseBlit, which
                                       // gates the button-icon/card-cursor group instead)
    unsigned char pad0xfd[3];
    // +0x100 -- the DESIGN-resolution rect this screen's whole layout is anchored to:
    // RefreshClientClipRect sets it to {0,0,800,600} or {0,0,1024,768} per nLayoutMode, centers
    // it inside the client clip bounds, and then derives every child rect from it by
    // CopyRect + OffsetRect. Was inside the +0xfd pad run; recovered with RefreshClientClipRect.
    RECT rectLayoutBase; // +0x100
    bool bInputBlocked; // +0x110 -- reentrancy/tutorial-block guard: OnLButtonDown and
                              // FUN_00402690 (VK_LEFT/VK_RIGHT) both bail immediately when set;
                              // OnActivate sets it based on TutorialWnd::NotifyOrLaunch(2,0)'s
                              // result (a tutorial popup is now covering this screen)
    unsigned char bWantEraseBlit;   // +0x111 -- paired w/ bHaveBackBuffer below; both
                                          // must be set for DrawOrEraseCardSlot's empty-slot erase blit
    unsigned char bHaveBackBuffer;  // +0x112
    unsigned char pad0x113;
    int nStartIndex;   // +0x114 -- top-of-page card index (FindFirstLoadableCardAtOrAfterIndex base)
    int nBucket;       // +0x118 -- PostBagFileCache bucket (0-8) for the visible page
    int nVisibleCount; // +0x11c -- number of populated slots on this page (<=6)
    unsigned int nHitTestIndex; // +0x120 -- scratch index written by the hit-test
                                       // (HitTestUiElement, not yet transcribed): category-tab index
                                       // for case 7, grid-slot index (added to nStartIndex)
                                       // for case 8/10
    unsigned int Unk0x124; // = 9 at init (matches paCategoryTabs's own 9-entry count)
    unsigned int nCurrentBucket; // +0x128 -- mirrors nBucket (both always written
                                        // together); used as the page-indicator icon's frame
                                        // index and as case 7's bucket-switch compare target
    unsigned char bShowCardNames; // +0x12c -- DrawCardThumbnail's bTintFillOnly arg
    unsigned char pad0x12d[3];
    CarNetState *pPendingCard; // +0x130 -- currently loaded/selected card awaiting a
                                           // user decision (open-to-edit/delete-duplicate/
                                           // save-to-category), set by OnLButtonDown case 8/10
    // +0x134 -- which of this screen's two hand-authored layouts to use: 0 = the 800x600
    // design, 1 = the 1024x768 one. AlbumCardWnd_InitResourceRefs picks 1 when the desktop
    // exceeds 800x600, so 1 is the HIGHER resolution. Was named nLowRes, which read as a bool
    // AND had the polarity backwards; renamed once RefreshClientClipRect (0x4028b0) showed it
    // switched on as a real 2-case index (`switch`, not a truth test) selecting between two
    // complete, independently-authored rect layouts. A real 4-byte int, not a byte -- the
    // ctor writes it as `undefined4` and every reader in this class reads the full DWORD.
    int nLayoutMode; // +0x134
    // +0x138/+0x13c -- lazily realized by EnsureBackgroundTileLoaded (kind id 0x3c0a/
    // 0x3c0b, low-res-gated), same TileKind_GetOrLoadDescriptor+GetOrLoadFrameBitmap(0,0)
    // pair idiom as PostBag.h's pCachedBadgeTileDesc/pRealizedBadgeFrame and
    // EditCardWnd.h's pPreviewIconDesc/pPreviewIconRealized.
    CursorDesc *pBackgroundTileDesc; // +0x138
    LocoBitmap *pBackBuffer; // +0x13c -- captured background, restored to erase empty slots
    // +0x140/+0x144 -- a 2nd cursor-transition (desc,rect) pair, class-specific (mirrors
    // WindowBase's own pPointCursorDesc/pPointCursorRect at +0x64/+0x60). Set to the
    // "card selected" cursor via the inherited RequestModeTransitionFromSource (vtable slot
    // 0xc) once OnLButtonDown case 8/10 successfully loads a card into pPendingCard.
    CursorDesc *pCardCursorDesc; // +0x140
    void *pCardCursorRect;            // +0x144
    // +0x148..+0x164 -- 8 button-icon ResourceRef* slots blitted by DrawButtonIcon's own
    // nCase switch (case N -> the field named after it below) plus one (pPageIndicatorIcon)
    // read directly by FUN_00402690's arrow-key handler, not through DrawButtonIcon at all. Cases
    // 2/4/5/6 are enable-gated (dimmed vs highlighted state, gated by Unk0x1d6/Unk0x1d7/
    // bBackArrowEnabled/bForwardArrowEnabled respectively); cases 1/3/9 always draw
    // highlighted. Which UI element each case-numbered icon actually is (beyond the confirmed
    // back/forward paging arrows) isn't traced yet -- named by case number for traceability.
    ResourceRef *pExitBtnIcon;      // +0x148 -- DrawButtonIcon case 1
    ResourceRef *pEditBtnIcon;      // +0x14c -- DrawButtonIcon case 2, gated by Unk0x1d6
    ResourceRef *pDeleteBtnIcon;      // +0x150 -- DrawButtonIcon case 3
    ResourceRef *pMailBtnIcon;      // +0x154 -- DrawButtonIcon case 4, gated by Unk0x1d7
    ResourceRef *pShowNamesBtnIcon;      // +0x158 -- DrawButtonIcon case 9
    ResourceRef *pBackArrowIcon;    // +0x15c -- DrawButtonIcon case 5, gated by bBackArrowEnabled
    ResourceRef *pForwardArrowIcon; // +0x160 -- DrawButtonIcon case 6, gated by bForwardArrowEnabled
    ResourceRef *pPageIndicatorIcon; // +0x164 -- read directly by FUN_00402690 (VK_LEFT/VK_RIGHT), frame-indexed by the new page/category number
    ResourceRef *paCardGrid[18]; // +0x168 -- see class-comment breakdown above
    // +0x1b0 -- RESOLVED: these are the 9 category/bucket TAB hit-rects (nBucket ranges
    // 0-8, matching the 9-entry count exactly), not "card slots" -- HitTestUiElement's own hit-test
    // PtInRects against these and returns case 7 (switch category), stashing the tab index in
    // nHitTestIndex. Was named paCardSlots ("purpose not yet traced"); renamed once
    // OnLButtonDown's own case 7 handling confirmed the role.
    ResourceRef *paCategoryTabs[9]; // +0x1b0
    // +0x1d4 -- paging-arrow enable/edge state, read/written by both RedrawAllSlots and
    // DrawButtonIcon (case 5/6 = back/forward arrow, case 2/4 use the other two). Was a single
    // abEnableFlags[6] "one per category tab" guess; re-derived from RedrawAllSlots's own direct
    // field access -- nothing else referenced the old name/interpretation.
    bool bBackArrowEnabled;    // +0x1d4 -- cached enable-state, avoids redundant DrawButtonIcon(5) kicks
    bool bForwardArrowEnabled; // +0x1d5 -- cached enable-state, avoids redundant DrawButtonIcon(6) kicks
    bool bComposeEnabled; // +0x1d6 -- gates DrawButtonIcon case 2's icon (highlighted/dimmed)
                                // AND OnLButtonDown case 2 (opens EditCardWnd on a new/blank
                                // card, pPendingCard==NULL, i.e. the "compose" button)
    bool bMailEnabled;    // +0x1d7 -- gates DrawButtonIcon case 4's icon AND OnLButtonDown
                                // case 4 (calls MailWnd's own vtable+8, i.e. the "mail" button)
    bool bAtBucketStart;       // +0x1d8 -- mirrors (nStartIndex == 0); write-only here, not read back
    bool bAtBucketEnd;         // +0x1d9 -- mirrors "no more cards past this page in bucket"; write-only here
    char aSlotNames[6][20];      // +0x1da -- per-slot card name, strcpy'd from nameB

    // 0x404ac0 -- full redraw: DrawOrEraseCardSlot(i) for every slot in [0, nVisibleCount), then
    // (if bWantEraseBlit/bHaveBackBuffer) the same per-slot RestoreOverlapBlt using
    // paCardGrid[12+i], then per-slot name DrawTextA, then a WindowBase::CommitScreenUpdate DC
    // release/commit and a paging-arrow-state kick (DrawButtonIcon case 5/6). EXACT match.
    void RedrawAllSlots(); // 0x404ac0

    // Button-icon dispatcher: nCase (a real 10-way 0-9 jump table, ja-guarded for >9; values
    // 0/7/8 and out-of-range all no-op) selects one of the 7 button-icon fields above and draws
    // it via ResourceRef::DrawFrame, in either its "highlighted"(frame 0) or
    // "dimmed"(frame 2) state for cases 2/4/5/6 (gated by bComposeEnabled/bMailEnabled/
    // bBackArrowEnabled/bForwardArrowEnabled respectively) or unconditionally frame 0
    // for cases 1/3/9. reserved is always 0 at every observed call site, purpose untraced.
    // ret 0x8 confirms 2 explicit thiscall params despite Ghidra's decompile only recognizing 1
    // before an earlier session's set_function_prototype fix (see CLAUDE.md's "under-analyzed
    // callee" lesson). Case body order in .text follows source declaration order per the
    // documented VC5 jump-table lesson, NOT case-value order: 1, 2, 3, 9, 4, 5, 6, then the
    // shared no-op tail (cases 0/7/8 + default) -- see src/AlbumCardWnd.cpp.
    void DrawButtonIcon(int nCase, int reserved); // 0x403ba0

    // Draws (or, if the card at nStartIndex+nSlotIndex no longer loads, erases) one visible
    // thumbnail slot. Loads the card via g_pPostBagFileCache->FindFirstLoadableCardAtOrAfterIndex; if
    // missing, blanks the slot's name and (when armed) restores the slot's rect from
    // pBackBuffer. If present, draws it via PostBagCacheBundle::DrawCardThumbnail,
    // strcpy's its nameB into aSlotNames[nSlotIndex], releases the loaded card
    // (delete), then kicks paCardGrid[6+nSlotIndex]'s own strip-frame draw. Real return type is
    // unsigned char per this TU's byte-return/no-EAX-widen idiom (every path's upper 24 bits are
    // leftover garbage from an inner byte-returning call, matching the documented VC5 lesson).
    unsigned char DrawOrEraseCardSlot(int nSlotIndex); // 0x4048e0

    // The decrementing purge/dedup sweep: walks the currently-visible page in REVERSE (from
    // nStartIndex+nVisibleCount-1 down to nStartIndex), and for each loaded card
    // that PostBagFileCache::PurgeDuplicateIndexEntry reports as already indexed under its own name-bucket
    // (a duplicate/stale entry), deletes it from the cache and frees it. Afterward, if the page
    // no longer starts on a loadable card, pages back by nVisibleCount. Then triggers a
    // full redraw (RedrawAllSlots) and a WindowBase::CommitScreenUpdate commit.
    void PurgeDuplicateCards(); // 0x405520

    // Hit-test: PtInRect against pButtonIcon1/9/4/2/3Maybe, pBackArrowIcon,
    // pForwardArrowIcon (in that order), then paCardGrid[0..5]/[12..17]'s own rects (sets
    // nHitTestIndex to the slot index, returns 8 or 10 depending on which sub-array hit),
    // then paCategoryTabs[0..8] (sets nHitTestIndex to the tab index, returns 7).
    // Returns 0 if nothing hit. Called from OnLButtonDown with the WM_LBUTTONDOWN
    // lParam's LOWORD/HIWORD (x,y). EFFECTIVE MATCH (5/431 bytes -- an intrinsic dead-
    // parameter-slot-reuse tie-break, see the marker comment on the definition).
    int HitTestUiElement(POINT pt); // 0x403cd0

    // Called at the top of every real (enabled) case in OnLButtonDown's dispatch, before
    // the case's own action -- per-button press-feedback (click sound + the erase-blit idiom +
    // a "pressed" DrawFrame(1,NULL) for the matching icon field; case 7 just plays the
    // sound and redraws the page-indicator icon at nCurrentBucket). ret 0x8 confirms 2
    // explicit thiscall params (same under-analyzed-callee pattern as sibling DrawButtonIcon) --
    // reserved is unread in the body, always 0 except cases 7/8/10 which pass
    // nHitTestIndex (mirrors DrawButtonIcon's own call sites exactly). EFFECTIVE MATCH --
    // real content 100% byte-identical, see the marker comment on the definition.
    void PlayButtonPressFeedback(int nCase, int reserved); // 0x403e80

    // vtable slot 0x20 override (WindowBase default: NoOpVirtualMaybe, a bare no-op). See
    // src/AlbumCardWnd.h's field comments and src/AlbumCardWnd.cpp for the full behavioral
    // summary. EXACT/PARTIAL status: see the marker comment on the definition.
    void OnActivate(int reserved); // 0x404db0

    // vtable slot 0x1c override of WindowBase::RefreshClientClipRect (0x425d30) -- the sibling
    // of EditCardWnd's own override at 0x417180. Chains the base implementation, then
    // re-lays-out every child rect this screen owns from a design-resolution anchor rect
    // (rectLayoutBase). See src/AlbumCardWnd.cpp for the full block-by-block write-up.
    virtual void RefreshClientClipRect(); // 0x4028b0

    // vtable slot 0x38 override -- the WindowBase-wide WM_LBUTTONDOWN convention slot (see
    // docs/subsystems.md's "Shared vtable slot conventions"). Only lParam is read (split
    // into x=LOWORD/y=HIWORD and forwarded to HitTestUiElement's hit-test); hwndMsg/msg/
    // wParam are unread dead params, kept for signature parity with the vtable-wide
    // dispatch convention. See src/AlbumCardWnd.cpp for the full behavioral summary.
    // 0x404f60. Now a TRUE override (virtual inherited) of WindowBase's slot-0x38 virtual,
    // modeled 2026-07-22.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x404f60

    // vtable slot 0x40 override -- the WindowBase-wide WM_RBUTTONDOWN convention slot, ground-
    // truthed against WindowBase::RouteMessageMaybe's (0x426140) own dispatch table (see
    // docs/subsystems.md's "Shared vtable slot conventions" -- corrects an earlier guess that
    // this slot was WM_LBUTTONUP). If pPendingCard is set: deletes it, clears it, and
    // resets the cursor pair back to the base default (RequestModeTransitionFromSource,
    // slot 0xc, via pPointCursorRect/pPointCursorDesc) -- right-click cancels an
    // in-progress duplicate-card placement, same idiom as OnLButtonDown's own
    // cancel-duplicate path.
    // cancel-duplicate path. Now a TRUE override (virtual inherited) of WindowBase's slot-0x40
    // virtual, modeled 2026-07-22.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x4055e0

    // vtable slot 0x60 override -- the WindowBase-wide WM_SETFOCUS convention slot (WindowBase's
    // own default there is the shared DefWindowProcStub). While a tutorial has input blocked,
    // refuses the focus and bounces it straight back to the tutorial window, then raises that
    // window to the top; otherwise falls through to the default. src/EditCardWnd.cpp's own
    // slot-0x60 override (0x41cdf0) is the same handler against its own block condition.
    virtual LRESULT OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x405620

    // vtable slot 0x54 override (WindowBase default: the generic DefWindowProcA-only stub shared
    // by most unoverridden slots -- not yet confirmed as a cross-class WM_KEYDOWN convention slot,
    // see docs/subsystems.md's "Shared vtable slot conventions"). VK_RETURN/VK_ESCAPE (0xd/0x1b)
    // end the active session (slot 4, via the vtable) and return to state 3; VK_LEFT/VK_RIGHT
    // (0x25/0x27) page back/forward, gated by bBackArrowEnabled/bForwardArrowEnabled --
    // same press-feedback-flash idiom as OnLButtonDown's own case 5/6, but the PlayButtonPressFeedback/
    // DrawButtonIcon flash happens BEFORE the bucket-boundary check here, not after. Any other key
    // falls through to DefWindowProcA.
    // falls through to DefWindowProcA. Now a TRUE override (virtual inherited) of WindowBase's
    // slot-0x54 virtual, modeled 2026-07-22.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x402690

    // vtable slot 0x50 override -- the WindowBase-wide WM_MOUSEMOVE convention slot, ground-
    // truthed against WindowBase::RouteMessageMaybe's (0x426140) own dispatch table (see
    // docs/subsystems.md's "Shared vtable slot conventions" -- corrects an earlier guess that
    // this slot was WM_SETCURSOR; lParam here really is x=LOWORD/y=HIWORD client
    // coordinates, WM_SETCURSOR's lParam is a hit-test-code/message-id pair instead, which
    // independently confirms this is WM_MOUSEMOVE). If not input-blocked and no card is
    // pending: hit-tests the point against all 8 button-icon rects; if any hit, requests the
    // inherited anipoint cursor pair, else the default point cursor pair, both via
    // RequestModeTransitionFromSource (slot 0xc) -- continuous hover-cursor tracking
    // driven by mouse movement.
    // RequestModeTransitionFromSource (slot 0xc) -- continuous hover-cursor tracking
    // driven by mouse movement. Now a TRUE override (virtual inherited) of WindowBase's
    // slot-0x50 virtual, modeled 2026-07-22.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x405680

    // vtable slot 4 override of WindowBase::EndActiveSession (base default: 0x425990,
    // Ghidra's EndModalCapture -- an unrelated, differently-shaped body; see CLAUDE.md's
    // "shared vtable slot conventions" gotcha, slot 4 is NOT one of the documented
    // cross-class-shared slots). THIS class's name won the 2026-07-21 slot-4 unification
    // (one C++ name must serve the base default and every override), so the base and
    // EditCardWnd's own override (Ghidra: EndEdit) were renamed to match. If
    // bModalCaptureActive (the modal-capture-active flag, WindowBase's own base field) is
    // set: ends the modal capture, clears bHaveBackBuffer, and releases the
    // button-icon/card-cursor resources via ReleaseButtonIconResources.
    virtual void EndActiveSession(); // 0x402660

    // ==== ICF-FOLDED slots: declared here, BODIES LIVE IN A SIBLING TU ====
    // Both of these are real overrides on THIS class's vtable (0x4773f0), but the COMDAT the
    // linker kept for each sits inside another TU's address block, so the body cannot be
    // transcribed here without duplicating a function that already has a home. Declaring them
    // is still required and is not cosmetic: without these two lines we emit WindowBase's own
    // 0x425f70/0x426a90 into slots 0x2c and 0x80 of AlbumCardWnd's vtable, which is simply the
    // wrong table -- tools/vtable_audit.py reported slot 0x80 as a hard MISMATCH (v545) and
    // slot 0x2c as a missing override. The two sibling headers have each predicted this line
    // for several sessions ("src/AlbumCardWnd.cpp will need the identical body when its slot
    // ... is transcribed" -- src/ApplSetupWnd.h and src/EditCardWnd.h).
    //
    // slot 0x2c (WM_SYSCOMMAND catch-all) -- 0x40b4c0, folded with ApplSetupWnd's own slot
    // 0x2c; the surviving copy is in src/ApplSetupWnd.cpp.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // slot 0x80 (WM_CLOSE) -- 0x419a10, folded with EditCardWnd's own slot 0x80; the surviving
    // copy is in src/EditCardWnd.cpp.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // vtable slot 8 override of WindowBase::BeginModalCapture (0x4259c0) -- the "open the
    // album card screen" counterpart to EndActiveSession (slot 4). Acquires the button-icon/
    // card-cursor resources, re-lays-out the screen (RefreshClientClipRect, slot 0x1c), chains
    // the base modal-capture begin, maximizes + focuses the window, drops any pending card,
    // restores the default point cursor pair, unblocks input, then re-syncs the view to
    // PostBagFileCache's last-insert page/bucket (Unk0x10/Unk0x14, consumed and reset to -1
    // here): a mismatch against the current view switches bucket and forces a full redraw.
    // This is the previously-unidentified slot-8 target reached from EditCardWnd's
    // OnLButtonDown through the g_pAlbumCardWnd probe.
    virtual void BeginModalCapture(); // 0x402590

    // Releases pCardCursorDesc (CursorDesc::ReleaseRef, vtable slot 2), the 8
    // button-icon ResourceRef fields, and paCardGrid[6..11] (the strip-frame draw handles),
    // then clears bWantEraseBlit -- but only if bWantEraseBlit was set. Called from
    // EndActiveSession.
    void ReleaseButtonIconResources(); // 0x404830

    // Lazily realizes pBackgroundTileDesc/pBackBuffer (kind id 0x3c0a/0x3c0b, low-res-
    // gated) via TileKind_GetOrLoadDescriptor + GetOrLoadFrameBitmap(0,0), guarded by
    // bBackgroundTileLoaded so it only realizes once.
    void EnsureBackgroundTileLoaded(); // 0x404720

    // "Load" counterpart to ReleaseButtonIconResources: Load()s all 8 button-icon
    // ResourceRef fields and paCardGrid[6..11] (the strip-frame draw handles), then
    // realizes pCardCursorDesc/pCardCursorRect (kind id 0x3cfa) -- but only if
    // bWantEraseBlit isn't already set (same gate flag, opposite state, as
    // ReleaseButtonIconResources's own guard).
    void AcquireButtonIconResources(); // 0x404770

    virtual ~AlbumCardWnd(); // 0x402380 (Ghidra: AlbumCardWnd_DtorMaybe)
};

extern AlbumCardWnd *g_pAlbumCardWnd; // DAT_004fd384

// Padded-vtable probe, now ONLY for src/EditCardWnd.cpp's one generic dispatch through the
// g_pAlbumCardWnd GLOBAL pointer (slot 8/_v08 -- target unidentified, so it can't be a named
// call, and the global devirtualization gotcha (CLAUDE.md) rules out a plain
// `g_pAlbumCardWnd->BeginModalCapture()`-style spelling anyway). The slots this used to
// serve through `this` casts (4 = EndActiveSession, 0xc = the inherited
// WindowBase::RequestModeTransitionFromSource) are real virtuals on the class since
// 2026-07-21; their lines below are kept only to preserve the probe's own slot alignment.
struct AlbumCardWndVtblProbe {
    virtual void *_v00();        // slot 0 -- scalar deleting dtor
    virtual void *_v04();        // slot 4 -- EndActiveSession (now a real virtual on the class)
    virtual void *_v08();        // slot 8 -- BeginModalCapture (now a real virtual on the
                                   // class, 0x402590 override transcribed EXACT)
    virtual void *_v0c();        // slot 0xc -- inherited WindowBase::RequestModeTransitionFromSource
};
