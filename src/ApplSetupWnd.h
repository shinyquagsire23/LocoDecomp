// ApplSetupWnd -- the multiplayer application/provider-setup page, the second of the two setup
// windows SplashWnd owns (SplashWnd::pApplSetupWnd @ +0x220; the other is NetSetupWnd @ +0x21c).
// A real WindowBase-derived Win32 window: constructed by SplashWnd::Create (0x4204d0) with
// `push 0x260; call operator new` immediately followed by the ctor 0x408aa0 -- so sizeof is 608
// (0x260) on the allocation site's own authority, and the Ghidra Structure of the same name
// agrees (it too carries a real `base : WindowBase` component @ +0).
//
// Promoted 2026-07-25 from the padded `ApplSetupWndPartial` view that used to live in
// src/SplashWndMaybe.h behind eight dummy `_v0N()` vtable-slot holders and a hand-modeled
// +0x4/+0x8/+0x3c triple standing in for the WindowBase base. SplashWnd's own dtor pair
// (0x4203a0/0x4203c0) `delete`s this object through slot 0, and SplashWnd::SetState (0x4208f0)
// dispatches its slots 4/8 -- both need the real base, so the padded view had to go.
//
// Mixed Ghidra namespacing (unchanged by the promotion): some members are this-typed into
// `ApplSetupWnd`/`ApplSetupWndPartial` directly, others still live in the pre-existing free
// `LayoutSet` namespace (e.g. `LayoutSet::SelectProviderSlotMaybe`, ~15 functions spanning
// 0x409e70-0x43dbb4) rather than being retroactively this-typed, to avoid namespace churn on
// already-matched functions; `lint_ghidra_sync`'s last-`::`-component match accepts either
// pairing. Only the members src/ actually reads are named so far -- extend in place.
#pragma once

#include <windows.h>

#include "CursorDesc.h"
#include "ResourceRef.h"
#include "WindowBase.h"

struct LocoBitmap;

// Small intrusive singly-linked list of provider/connection-status entries, reached via
// ApplSetupWnd::pListHeadMaybe. Confirmed via SetProviderListMaybe's own free-the-whole-list
// loop (deletes pszText if any, then the node itself, walking pNext) and
// DrawProviderList's per-node DrawTextA(node->pszText) walk; Unk0x4 isn't touched by any
// function transcribed so far. A -1 (not 0) head value is a distinct "not yet built" sentinel,
// separate from a genuinely empty (0) list.
struct ProviderListNode {
    ProviderListNode *pNext;  // +0
    int Unk0x4;               // +4
    char *pszText;            // +8
};

class ApplSetupWnd : public WindowBase {
public:
    // vtable slot 0x20 -- this page's own override of WindowBase's activation redraw
    // (0x409280, declared only): restores the overlap blit, resets 3 status icons
    // (FUN_00454c30 x3), draws the connect-status text (picking one of two source pointers by
    // bUseSecondaryRememberedChoice), draws a second text block, ARMS bReadyForRedrawMaybe,
    // then commits. WindowBase's own default at this slot is the shared `ret 0x4` no-op
    // (0x426130, the same body slot 0x14/NoOpVirtualMaybe carries).
    virtual void OnActivate(int reservedMaybe);  // 0x409280

    // ==== UI/LayoutSet helpers invoked on this window ====
    // All declared-only unless noted; Ghidra namespaces most of them under `LayoutSet` (last-`::`
    // component matches either way).

    // 0x40a260 -- LayoutSet::RestartProviderScanMaybe, EXACT match, see src/DPlaySessionMgr.cpp.
    // Tears down + reconnects providers; unless the secondary-remembered-choice path is active,
    // rebuilds the layout list, clears the manager's layout-sync-ping-sent flag, and draws the
    // connect-status text + a locale-string label into the textBuf0x120 buffer -- else resets the
    // connection, prepares for an internet session, and reloads the index file. Either way
    // redraws + commits.
    void RestartProviderScanMaybe();
    // 0x40a3d0 -- LayoutSet::SetProviderListMaybe, EXACT match, see src/DPlaySessionMgr.cpp.
    // Clears bUnk0x10c; resets a -1 "not yet built" sentinel head to null, then frees the WHOLE
    // provider list (deleting each node's pszText, if any, then the node). If pNewHead == 0,
    // resets the head back to the -1 sentinel and, when field_0xe8 is set, rebuilds+returns
    // early; otherwise installs pNewHead as the new list head, calls ApplyListSelectionMaybe(0),
    // and -- while ready for redraw -- redraws the list + commits.
    void SetProviderListMaybe(ProviderListNode *pNewHead);
    // 0x40a150 -- LayoutSet::LayoutSet_RebuildAndNotifyMaybe. No-op (early return) if bUnk0x10c
    // is already set; else clears bLayoutSyncPingSent, tears down + reconnects providers (reset
    // teardown, reset connection, prepare internet), frees the WHOLE provider list (same
    // free-loop shape as SetProviderListMaybe), posts a type-2 local-queue notification carrying
    // this window's own HWND as payload, then sets bUnk0x10c so a repeat call is a no-op until
    // something clears it again (AdvanceToNextProviderMaybe).
    void RebuildAndNotifyMaybe();
    // 0x40a350 -- LayoutSet::AdvanceToNextProviderMaybe, EXACT match, see src/DPlaySessionMgr.cpp.
    // Clears bUnk0x10c, then either re-sends a select-request for the next slot (nUnk0xf4+1) or --
    // when the secondary-remembered-choice path is active -- resets the session's providers and
    // reloads the index file; either way redraws+arms via OnActivate, then arms two redraw timers
    // (events 0x50/50ms, 0x52/75ms) and sets nUnk0x1b0 = 2.
    void AdvanceToNextProviderMaybe();
    // 0x40aaf0 -- LayoutSet::ApplyListSelectionMaybe, EXACT match, see src/DPlaySessionMgr.cpp.
    // Walks pListHeadMaybe forward `index` nodes; if the walk lands on a real node (list wasn't
    // empty), caches the index + that node's pszText, tears down + reconnects providers, redraws
    // the connect-status text, commits, then re-prepares + attempts an internet join. Otherwise
    // resets the cached index to 0 and rebuilds the layout list. No-op entirely when the
    // secondary-remembered-choice path is active.
    void ApplyListSelectionMaybe(int index);
    // 0x409e70 -- LayoutSet::LayoutSet_LoadIndexFileMaybe. Frees the WHOLE secondary provider
    // list, builds "<installPathPrefix>Layouts\index.lay", loads it (RF archive first, loose
    // ifstream fallback) into a fixed 0x2000-byte buffer, then parses it line-by-line into fresh
    // ProviderListNode entries pushed onto pListHeadSecondaryMaybe. See src/DPlaySessionMgr.cpp.
    void LoadIndexFileMaybe(char bApplySelectionAfter);
    // 0x4094b0 -- repaints the scrolling provider list: restores the list rect's background,
    // measures a row with the "test" string, then walks pListHead drawing one row per node,
    // highlighting the node at index nUnk0xf4 and counting the rows into nListRowCountMaybe.
    // Caches pListHead in pDrawnListHeadMaybe on entry.
    void DrawProviderList(ProviderListNode *pListHead);  // 0x4094b0
    // 0x409770 -- re-lays and repaints the one line of status text, then steps it along the
    // current crawl leg. Structurally the same function as NetSetupWnd::LayoutAndDrawLabel.
    void LayoutAndDrawLabel();                           // 0x409770
    // 0x40a4a0 -- LayoutSet::SelectProviderSlotMaybe, EXACT match, see src/DPlaySessionMgr.cpp.
    // Records the newly-selected provider-slot index and, unless the secondary-remembered-choice
    // path is active, refreshes the status message + commits the redraw (only while the window is
    // in the ready-for-redraw state).
    void SelectProviderSlotMaybe(int slotIndex);
    // 0x40a300 -- LayoutSet::NotifyProviderSelectedMaybe, EXACT match, see
    // src/DPlaySessionMgr.cpp. While ready for redraw: if the secondary-remembered-choice path is
    // active, marks DPlaySessionMgr::bLayoutSyncPingSent and refreshes the connection-status
    // message; either way, redraws + commits, then sends a select-request (arg 0).
    void NotifyProviderSelectedMaybe();
    void SendSelectRequestMaybe(int targetSlot);  // 0x40ac50 -- LayoutSet::SendSelectRequestMaybe
    // 0x409980 -- repaints the 3x3 "who's connecting" avatar grid: one ResourceRef cell per
    // (row, column) of g_pDPlaySessionMgr's real slot dimensions, drawn frame 0 (empty),
    // frame 1 (reserved) or frame 2 plus the peer's name text (occupied).
    void DrawAvatarGrid();                        // 0x409980
    // 0x409360 -- LayoutSet::RefreshConnectStatusText, not yet transcribed (declared
    // only). Refreshes the connection-status message: optional overlap-restore blit if
    // bResourcesLoadedMaybe is set, else picks a locale string id (0x6f/0x70/0x71) off the session's connect
    // state, loads it into textBuf0x120 via UIResources::LoadLocaleString, then redraws via
    // LayoutAndDrawLabel.
    void RefreshConnectStatusText();

    // 0x40a220 -- ApplSetupWnd::AbortToDisconnectedStateMaybe. Cancels the cursor-mode
    // transition, tears down + resets the session's providers, sets mode 3, sets SplashWnd state
    // 7, and clears bUnk0x114. Byte-identical to FUN_0040a4e0's own inline "secondary path, no
    // available provider" arm -- likely a shared helper factored out for that one call site.
    void AbortToDisconnectedStateMaybe();
    // 0x40aba0 -- ApplSetupWnd::SelectGridCellFromPointMaybe. The GRID-mode (rectGridMaybe) click
    // handler: converts a client-area (x, y) into a 3x3 grid cell (col, row) via rectGridMaybe's
    // own bounds divided by the fixed grid dimension 3, bounds-checks against g_pDPlaySessionMgr's
    // real nProviderSlotsPerRow/nProviderSlotRows, and -- if the resulting flat slot index differs
    // from nSelectedProviderSlot -- sends a select-request for it and redraws. Called from
    // OnLButtonDown's rectGridMaybe branch with the raw (x, y) point.
    void SelectGridCellFromPointMaybe(int x, int y);
    // 0x40aa20 -- ApplSetupWnd::ApplySecondaryListSelectionMaybe. Secondary-remembered-choice
    // counterpart of ApplyListSelectionMaybe: tears down + resets providers, walks
    // pListHeadSecondaryMaybe forward nUnk0xf4 nodes; on a real node, caches its pszText, applies
    // the config line, assigns the local player into slot 0 with the current board dimensions,
    // resets the selection, re-resets + re-prepares + re-attempts an internet join, then -- while
    // ready for redraw -- redraws + commits. `deadIndexArg` is a genuine pushed stack argument
    // (ret 4) that the callee body never reads (it re-reads nUnk0xf4, already set by the caller
    // before the call) -- see CLAUDE.md's "dead but real parameter" tell.
    void ApplySecondaryListSelectionMaybe(int deadIndexArg);
    // 0x40a4e0 -- ApplSetupWnd::OnLButtonDown, WindowBase vtable+0x38 override (the class-wide
    // WM_LBUTTONDOWN convention slot, confirmed via a live vtable dump: ApplSetupWnd's own vtable
    // at 0x4774d0, slot 0xe/+0x38 = 0x40a4e0). Dispatches a click across the 5 single-button
    // ResourceRef rects (avatar-picker "go"/"exit"/"search"/"option"/"matrix" buttons at
    // +0x220..+0x230, each drawn via ResourceRef::DrawFrame), a random-sound easter-egg rect
    // (+0x20c), and one of two mutually-exclusive provider-list click regions: the LIST-mode rect
    // (rectListMaybe, a per-row hit test using nRowHeightMaybe) or the GRID-mode rect
    // (rectGridMaybe, delegates to SelectGridCellFromPointMaybe). Gated on
    // !bSuppressCursorRedrawMaybe && bReadyForRedrawMaybe.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // ==== WindowBase vtable overrides transcribed in src/ApplSetupWnd.cpp ====
    // Slot numbers verified against this class's own vtable at 0x4774d0.
    virtual ~ApplSetupWnd();                  // slot 0x00 -- 0x408d10 (??_G thunk at 0x408b00)
    virtual void EndActiveSession();          // slot 0x04 -- 0x409db0
    virtual void BeginModalCapture();         // slot 0x08 -- 0x409c60
    virtual void RefreshClientClipRect();     // slot 0x1c -- 0x408f70
    // slot 0x2c (WM_SYSCOMMAND catch-all). ⚠ 0x40b4c0 is ICF-FOLDED: AlbumCardWnd's vtable
    // (0x4773f0) carries the SAME address at its own slot 0x2c. The surviving COMDAT was placed
    // at 0x40b4c0, immediately after this TU's OnTimerDefaultMaybe and nowhere near
    // AlbumCardWnd's own 0x401xxx-0x405xxx block, so it is THIS file's copy that the linker kept
    // -- src/AlbumCardWnd.cpp will need the identical body when its slot 0x2c is transcribed.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    virtual LRESULT OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x30 -- 0x40b180
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);         // slot 0x50 -- 0x40b0a0
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);           // slot 0x54 -- 0x40ae20
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);             // slot 0x80 -- 0x40ada0
    // slot 0x40 (WM_RBUTTONDOWN) -- 0x4323c0, declared-only: the surviving COMDAT lives in
    // src/MapWnd.cpp (MapWnd::OnRButtonDown), and this class, NetSetupWnd and SplashWnd all
    // carry that same address in their own slot 0x40. Added v545 so the emitted vtable stops
    // holding WindowBase's default here.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    unsigned char field_0xe8;    // +0xe8  -- gates the LayoutSet redraw in event types 3/5
    char pad0xe9[0xec - 0xe9];   // +0xe9  -- unmodeled
    // Secondary-mode counterpart of pListHeadMaybe (+0xf0) -- LoadIndexFileMaybe rebuilds THIS
    // list (from `Layouts\index.lay`, per-line ProviderListNode entries),
    // ApplySecondaryListSelectionMaybe walks it (shares nUnk0xf4 as the walk index with the
    // primary list) into pSelectedNodeTextSecondaryMaybe.
    ProviderListNode *pListHeadSecondaryMaybe;  // +0xec
    // Head of the ProviderListNode list (see above); -1 = "not yet built" sentinel, distinct
    // from a genuinely empty (0) list. Part of the small intrusive-list-like +0xf0/+0xf4/+0xf8
    // cluster touched by RestartProviderScanMaybe/SetProviderListMaybe/ApplyListSelectionMaybe.
    ProviderListNode *pListHeadMaybe;  // +0xf0
    int nUnk0xf4;                // +0xf4  -- read by AdvanceToNextProviderMaybe (passed +1 to
                                 //           ApplyListSelectionMaybe); also the shared walk index
                                 //           for pListHeadSecondaryMaybe.
    // Cached pszText of the ProviderListNode found at index nUnk0xf4 by ApplyListSelectionMaybe's
    // list walk -- copied from ProviderListNode::pszText directly, purpose past the copy not yet
    // traced (no consumer transcribed yet).
    char *pSelectedNodeTextMaybe;  // +0xf8
    // Secondary-mode counterpart of pSelectedNodeTextMaybe -- ApplySecondaryListSelectionMaybe's
    // own pListHeadSecondaryMaybe walk result cache.
    char *pSelectedNodeTextSecondaryMaybe;  // +0xfc
    // LIST-mode row height (in pixels) -- initialized to 0x10 (16) by InitFields; OnLButtonDown
    // divides (y - rectListMaybe.top - 0xc) by this to get the clicked row index.
    int nRowHeightMaybe;         // +0x100
    // LIST-mode row-count bound -- OnLButtonDown rejects a computed row index >= this value.
    int nListRowCountMaybe;      // +0x104
    // +0x108 -- the list head DrawProviderList was last handed, cached purely so the next
    // OnTimer repaint can redraw the same list without the caller passing it again. Cleared by
    // InitFields, by BeginModalCapture and by EndActiveSession; never dereferenced here.
    ProviderListNode *pDrawnListHeadMaybe;  // +0x108
    unsigned char bUnk0x10c;     // +0x10c -- zeroed unconditionally at AdvanceToNextProviderMaybe's
                                 //           entry; RebuildAndNotifyMaybe's own re-entrancy guard
                                 //           (early-returns while set, sets it on completion)
    char pad0x10d[0x110 - 0x10d];  // +0x10d -- unmodeled
    int nSelectedProviderSlot;   // +0x110 -- index of the highlighted/selected provider slot,
                                 //           written by SelectProviderSlotMaybe
    unsigned char bUnk0x114;     // +0x114 -- cleared by AbortToDisconnectedStateMaybe and by
                                 //           FUN_0040a4e0's own inline duplicate of it
    char pad0x115[0x118 - 0x115];  // +0x115 -- unmodeled
    unsigned int hTimerA;        // +0x118 -- SetTimer id, event 0x50, 50ms (AdvanceToNextProviderMaybe)
    unsigned int hTimerB;        // +0x11c -- SetTimer id, event 0x52, 75ms (AdvanceToNextProviderMaybe)
    char textBuf0x120[0x1a0 - 0x120];  // +0x120 -- locale-string label buffer, sized 0x80 (128)
                                 //           bytes, written by UIResources::LoadLocaleString
    // The status label's own laid-out bounds: LayoutAndDrawLabel measures textBuf0x120 into it
    // with DT_CALCRECT|DT_SINGLELINE, centers it inside rectTextAreaMaybe, then flushes it to
    // one of that box's four edges. Exactly NetSetupWnd::rectLabelMaybe's role (+0x130 there).
    RECT rectLabelMaybe;         // +0x1a0..+0x1b0
    // +0x1b0 -- which leg of the label's crawl around the inside of rectTextAreaMaybe is
    // running: 0 = flush right, 1 = flush left, 2 = flush bottom, anything else = flush top.
    // The same field NetSetupWnd calls nUnk0x140Maybe (+0x140 there); InitFields seeds it 3,
    // BeginModalCapture and AdvanceToNextProviderMaybe both reset it to 2.
    int nUnk0x1b0;               // +0x1b0
    // +0x1b4 -- the page's own window icon, loaded (resource id 0x65) and handed straight to
    // WindowBase::Create by ApplSetupWnd::Create. NetSetupWnd::Create does the identical thing
    // into its own +0x144 slot.
    HICON hIcon;                 // +0x1b4
    unsigned char bReadyForRedrawMaybe;  // +0x1b8 -- gates SelectProviderSlotMaybe's (and
                                 //           NotifyProviderSelectedMaybe's) redraw/commit path;
                                 //           set once the window has finished its own setup
    char pad0x1b9[0x1bc - 0x1b9];  // +0x1b9 -- unmodeled
    // The page's fixed 800x600 design rect, rebuilt as {0, 0, 800, 600} on every layout pass and
    // then used as the box the window's own clip bounds are centered in. Every other rect below
    // is positioned relative to its top-left corner.
    RECT rectPageMaybe;          // +0x1bc..+0x1cc
    // A clip-bounds-sized WINDOW INTO the backdrop bitmap, centered on it -- the same trick
    // NetSetupWnd::rectIconMaybe (+0x14c there) plays. Its left/top double as the source offset
    // every RestoreOverlapBlt in this file applies to the rect being restored, which is why the
    // decompiler shows +0x1cc/+0x1d0 read as a bare (dx, dy) pair in the OffsetRect calls.
    RECT rectIconMaybe;          // +0x1cc..+0x1dc
    // LIST-mode click region -- OnLButtonDown computes a row index from
    // `(y - rectListMaybe.top - 0xc) / nRowHeightMaybe`, matched against nUnk0xf4 (current
    // selection) and nListRowCountMaybe (row count bound). Mutually exclusive with rectGridMaybe
    // -- exactly one of the two UI layouts (scrolling list vs. 3x3 grid) is active at a time.
    RECT rectListMaybe;          // +0x1dc..+0x1ec
    // GRID-mode click region -- OnLButtonDown forwards a hit here straight to
    // SelectGridCellFromPointMaybe's own 3x3-cell math (fixed grid dimension 3, independent of
    // this rect's own size).
    RECT rectGridMaybe;          // +0x1ec..+0x1fc
    // The box the one line of status text lives in: LayoutAndDrawLabel centers rectLabelMaybe
    // inside it and then crawls the label around its four edges. NetSetupWnd's counterpart is
    // its own rectTextAreaMaybe.
    RECT rectTextAreaMaybe;      // +0x1fc..+0x20c
    // Easter-egg click region: hitting it plays one of several random sound ids
    // (`rand()/0x1999 + 0x50f3`) via PlaySoundAtScreenPos, no other effect.
    RECT rectEasterEggSoundMaybe;  // +0x20c..+0x21c
    unsigned char bResourcesLoadedMaybe;   // +0x21c -- "overlap restore blit" gate, see
                                 //           RefreshConnectStatusText's own plate comment
    char pad0x21d[0x220 - 0x21d];  // +0x21d -- unmodeled
    // 5 single ResourceRef sub-objects (InitFields: ids 0x42a/0x42c/0x429/0x42b/0x42f =
    // apGo/apExit/apsearch/apoption/APmatrix). OnLButtonDown hit-tests each one's own `rect`
    // field (ResourceRef::rect, +0x4) and, on a hit, plays the UI click sound, redraws it via
    // DrawFrame(1, NULL), commits, sleeps 150ms, then runs that button's own action.
    ResourceRef *pApGoBtn;       // +0x220 -- id 0x42a
    ResourceRef *pApExitBtn;     // +0x224 -- id 0x42c
    ResourceRef *pApSearchBtn;   // +0x228 -- id 0x429
    ResourceRef *pApOptionBtn;   // +0x22c -- id 0x42b
    ResourceRef *pApMatrixBtn;   // +0x230 -- id 0x42f
    // The page's backdrop art, cached raw side by side exactly as NetSetupWnd caches its own
    // (+0x1cc/+0x1d0 there): BeginModalCapture realizes descriptor id 0x439 and its frame-0
    // bitmap, ~ApplSetupWnd and EndActiveSession both drop them again. Neither is zeroed by
    // InitFields -- bResourcesLoadedMaybe is what says whether they are live.
    CursorDesc *pIconDescMaybe;      // +0x234
    LocoBitmap *pIconRealizedMaybe;  // +0x238
    // 9 avatar-cell ResourceRef sub-objects (InitFields: ids 0x43a..0x442 = aplayer0..aplayer8),
    // doubling as the "who's connecting" provider-slot grid (see docs/subsystems.md).
    ResourceRef *aPlayerAvatars[9];  // +0x23c..+0x260

    // 0x408aa0 -- chains WindowBase's ctor with the page's own resource id 0x1f9, installs
    // ApplSetupWnd_VtblMaybe (0x4774d0), then runs InitFields (0x408b20, the 5 button + 9 avatar
    // ResourceRef allocations). Declared only.
    ApplSetupWnd(HINSTANCE hInstanceArg, UINT resourceIdArg);

    // 0x408b20 -- zeroes the page's own state and constructs the 5 button + 9 avatar
    // ResourceRefs in field order. Split out of the ctor exactly as NetSetupWnd::InitFields is.
    void InitFields();

    // 0x408f00 -- the page's own non-virtual Create(HWND), hiding WindowBase::Create the same way
    // NetSetupWnd::Create (0x4412f0) does: sizes itself to the desktop client rect, loads hIcon,
    // and chains WindowBase::Create with style 0x41000000. Called by SplashWnd::Create right
    // after the ctor. Declared only.
    unsigned char Create(HWND hwndOwnerParam);
};
