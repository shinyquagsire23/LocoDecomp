// TutorialWnd -- the largest bootstrap singleton (0x3078/12408 bytes; ctor 0x44f490, Create
// 0x450ca0, class TUTORIALWINDOWCLASS id 510), `PopupWndBase`-derived (a DirectDraw-composited
// overlay, see PopupWndBase.h -- NOT the Win32-HWND `WindowBase` hierarchy). The central
// gameplay/world-view tutorial browser: 9 `ResourceRef*` "error state" icon slots
// (pErrObj1..9), a per-voice narration channel, a 200-entry category/item table
// (categoryRecords), and nav/scroll/timer state for paging through it. See
// docs/subsystems.md's "TutorialWnd" entry for the full field writeup (RESOLVED 2026-07-12 --
// every byte of the struct is a named field, no undefined gaps).
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "PopupWndBase.h"
#include "ResourceRef.h"

struct DSoundChannel; // src/DSoundChannel.h -- pDSoundChannel below is only ever a pointer
                      // here, so forward-declare rather than drag the whole header in.

// Not a standalone class (RESOLVED 2026-07-12) -- this IS TutorialWnd's own categoryRecords
// element type; the `ResourceRefCategoryTable_*` free-function-shaped names some of
// TutorialWnd's own methods carry are a feature-area label, not evidence of a separate class.
// rectA/rectB (RESOLVED 2026-07-17): merged from 8 loose ints per the "two independent
// consumers agreeing on a coherent 4-tuple" rule -- TutorialWnd::FUN_00450520 both (1) diffs
// each pair's own Left/Top/Right/Bottom fields as an inclusive-rect bounds check and (2) passes
// &rectALeft/&rectBLeft directly to CopyRect()/SetRect()-family calls expecting a real RECT*.
struct ResourceRefCategoryRecord {
    int dwIconResourceId;
    int dwTitleStringId; // +4 -- locale string id of the item TITLE (DrawItemTitle draws it)
    int dwDescriptionStringId;
    unsigned char bUsed;
    unsigned char pad0xd[3];
    int iconId;
    unsigned char bHashFlagMaybe;
    unsigned char pad0x15[3];
    // +0x18 -- narration sound id. Set by the category parser from the record's optional `@<n>`
    // sigil (default 0x50f8); both page actions and OnDrawContent feed it to
    // SoundBank_LookupEntryById + DSound::PlaySoundByIdWithHandle.
    int dwNarrationSoundId;
    RECT rectA;
    RECT rectB;
};

class TutorialWnd : public PopupWndBase {
public:
    // FUNCTION: LOCO 0x44f490 -- see src/TutorialWnd.cpp
    TutorialWnd(HINSTANCE hInstance, UINT resourceId);
    // FUNCTION: LOCO 0x44f510 -- see src/TutorialWnd.cpp. Declared virtual (it overrides
    // PopupWndBase's own virtual dtor) so the compiler also emits the scalar-deleting-dtor
    // thunk the vtable's slot 0 actually points at (0x44f4f0).
    virtual ~TutorialWnd();
    // FUNCTION: LOCO 0x450ca0 -- see src/TutorialWnd.cpp
    unsigned char Create(HWND hwndOwner);
    // FUNCTION: LOCO 0x450ae0 -- vtable slot 4 override of PopupWndBase::OnExit; see
    // src/TutorialWnd.cpp.
    virtual void OnExit();
    // FUNCTION: LOCO 0x450d60 -- vtable slot 0x18 override of PopupWndBase::RefreshClientRect:
    // re-centers the popup on the desktop and lays out all nine pErrObj* slots' rects. See
    // src/TutorialWnd.cpp.
    virtual void RefreshClientRect();

    // ---- The five PopupWndBase message-handler overrides (vtable slots 0x28/0x2c/0x34/0x3c/
    // 0x4c). Writable as real overrides only since v397, when PopupWndBase's virtual block was
    // extended from slot 0x20 to 0x4c; before that they would have needed raw slot casts. ----

    // FUNCTION: LOCO 0x4518b0 -- vtable slot 0x28 (RouteMessage's default/fallback arm).
    // Intercepts exactly one message, WM_SYSCOMMAND/SC_SCREENSAVE: hides the tutorial view
    // (OnExit) and transitions the app back to whichever screen launched it -- 5/6/7 for
    // lastNotifySubcode 1/2/3 (mail / album / edit-card), else nGlobalStateMirror -- then
    // re-runs the app's window-visibility pass. Everything, intercepted or not, still falls
    // through to DefWindowProcA.
    virtual LRESULT OnUnhandledMessage(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x451880 -- vtable slot 0x2c (WM_TIMER passthrough). Runs the presenter
    // animation tick, but only for this window's own scroll timer (id 0x54, see
    // nScrollTimerId) and only once the presenter is actually playing.
    virtual LRESULT OnTimerDefault(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x451540 -- vtable slot 0x34 (WM_LBUTTONDOWN). Dispatches on
    // HitTestControl's code: 1 = prev page, 2 = next page, 3 = close, 7 = the current item's
    // own icon (which acts as close for two specific icon resources). Every other code, and
    // every click while the presenter is idle, is a no-op. See src/TutorialWnd.cpp.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x4527b0 -- the PRESSED half of the press-flash pair below: OnLButtonDown
    // calls this, sleeps ~150 ms, then calls RedrawControl with the same code. Only the three
    // nav buttons have a distinct pressed look (frame 1); codes 4 and 5 are no-ops.
    // See src/TutorialWnd.cpp.
    void RedrawControlPressed(int nControl);

    // FUNCTION: LOCO 0x451fb0 -- repaints ONE control, addressed by the same control code
    // HitTestControl returns (so the code is NOT the pErrObj slot number -- see
    // HitTestControl's own comment). Also the "unpress" half of the press-flash pair with
    // RedrawControlPressed above, and the presenter's own per-frame repaint (case 6). Nothing here
    // presents: cases 5 and 9 commit their own DC, case 4 deliberately does not, and the
    // pErrObj*-drawing cases leave presenting to the caller's CommitScreenUpdate.
    // See src/TutorialWnd.cpp.
    void RedrawControl(int nControl);

    // The four per-control draw helpers RedrawControl delegates to (codes 4, 5, 9 and 6). The
    // first three take the caller's already-acquired offscreen DC BY POINTER (the original
    // passes `lea ecx,[esp+8]`), so they can hand a re-acquired handle back for
    // CommitScreenUpdate. The two whose bodies have not been read keep their Ghidra FUN_ names.
    // FUNCTION: LOCO 0x452230 -- draws the currently scrolled-into-view page of the selected
    // item's description text into pErrObj6's rect. See src/TutorialWnd.cpp.
    void DrawDescriptionPage(HDC *pHdc);
    // FUNCTION: LOCO 0x452570 -- draws the selected item's title string, centered, into
    // pErrObj7's rect. See src/TutorialWnd.cpp.
    void DrawItemTitle(HDC *pHdc);
    // FUNCTION: LOCO 0x4526b0 -- draws a literal "..." into pErrObj8's rect: the "more
    // description text follows" indicator. See src/TutorialWnd.cpp.
    void DrawEllipsis(HDC *pHdc);
    // FUNCTION: LOCO 0x452c00 -- paints presenter frame nFrameIndex of pErrObj4's realized
    // bitmap strip over a freshly restored backdrop. See src/TutorialWnd.cpp.
    void DrawPresenterFrame(unsigned int nFrameIndex);
    // FUNCTION: LOCO 0x452b00 -- re-captures the 0xe8 x 0x130 presenter box from the shared
    // work surface into this window's own offscreen surface. Sole caller is DrawPresenterFrame.
    // See src/TutorialWnd.cpp.
    void RestorePresenterBackdrop();
    // FUNCTION: LOCO 0x451920 -- the prev-page action (hit code 1). Named in v398 once the body
    // was read: winds back one description page within the current item, or moves to the
    // previous item and lands on its LAST page. See src/TutorialWnd.cpp.
    void GoToPrevPage();
    // FUNCTION: LOCO 0x451c60 -- the next-page action (hit code 2). Named in v398 with
    // GoToPrevPage; the forward mirror, and simpler (no chunk-count probe). See
    // src/TutorialWnd.cpp.
    void GoToNextPage();

    // FUNCTION: LOCO 0x451520 -- vtable slot 0x3c (WM_RBUTTONDOWN): forwards straight to
    // OnLButtonDown THROUGH THE VTABLE (the original's `call [eax+0x34]`, not a direct call),
    // so an unqualified call is the faithful spelling. Same idiom as MapWnd::OnRButtonDown.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // Slot-0x50 (WM_KEYDOWN) override -- declared-only. The class vtable dword at 0x478478 holds
    // 0x426950, WindowBase::OnMouseActivate's body (a bare `return 0`) ICF-folded in here: this
    // window swallows key-downs instead of passing them to DefWindowProcStub. Transcribed and
    // marked in src/WindowBase.cpp, so this stays a declaration. Recovered in v544.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x4528e0 -- vtable slot 0x1c override of PopupWndBase::OnDrawContent: the
    // view's whole first-paint bring-up (narration start, backdrop capture, all-control
    // repaint, presenter timer restart). Ignores its PAINTSTRUCT. See src/TutorialWnd.cpp.
    virtual void OnDrawContent(PAINTSTRUCT *pPs);

    // FUNCTION: LOCO 0x4517b0 -- vtable slot 0x4c (WM_MOUSEMOVE). Runs the base handler's
    // software-cursor redraw first, then swaps the popup's cursor between its two preloaded
    // slots (PopupWndBase::cursorHover / cursorNormal) according to whether the pointer is over
    // an ENABLED control. Only acts while the presenter is playing.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // Vtable slot 0x7c (WM_CLOSE) override -- declared-only. Recovered in v544 when
    // PopupWndBase's virtual block was extended to its full 37 slots and this class's slot 31
    // stopped resolving to the inherited base body: the image's table (0x478428) holds
    // **0x40f760** here, not PopupWndBase::OnClose's 0x414b80. 0x40f760 is CreditsWnd::OnClose,
    // already transcribed and marked in src/CreditsWnd.cpp -- one ICF-folded body installed in
    // two classes' tables (both are "clear bCreated, DestroyWindow, PostQuitMessage if
    // ownerless"), so only CreditsWnd's copy can carry the address's marker and this one stays
    // a declaration. Same idiom as PopupWndBase's own WindowBase-shared handler slots.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x451e90 -- maps a client-space point to a control code, tested in a fixed priority
    // order: 1/2/3 = the pErrObj1/2/3 nav buttons (each skipped entirely unless its own
    // enabled byte is set), then 7 = pErrObj5, 6 = pErrObj4, 4 = pErrObj6, 5 = pErrObj7 and
    // 8 = pErrObj9 -- note the code is NOT the slot number, the two orderings genuinely
    // disagree. 0 = nothing hit. See src/TutorialWnd.cpp.
    // Return type is `int`, not the `byte` Ghidra infers from the callee's own `al`-shaped
    // returns: both call sites use EAX directly (`dec eax; cmp eax,3`) with NO zero-extension,
    // and an `unsigned char` return makes cl insert an `and eax,0xff` the original does not have.
    int HitTestControl(LONG x, LONG y);

    ResourceRef *pErrObj1; // +0x118
    unsigned char bErrObj1Loaded;
    unsigned char pad0x11d[3];
    ResourceRef *pErrObj2; // +0x120
    unsigned char bErrObj2Loaded;
    unsigned char pad0x125[3];
    ResourceRef *pErrObj3; // +0x128
    unsigned char bErrObj3Loaded;
    unsigned char pad0x12d[3];
    ResourceRef *pErrObj4; // +0x130
    ResourceRef *pErrObj5; // +0x134
    ResourceRef *pErrObj6; // +0x138
    ResourceRef *pErrObj7; // +0x13c
    ResourceRef *pErrObj8; // +0x140
    unsigned char bErrObjsLoaded; // +0x144 -- shared by pErrObj4..8
    unsigned char pad0x145[3];
    ResourceRef *pErrObj9; // +0x148
    bool bPresenterActiveFlag; // +0x14c -- one-shot "narration/presenter playback started"
                                      // latch: reset 0 in Launch/OnExit/Init, set once by
                                      // OnDrawContent (PlaySound + gates the WndProc's custom
                                      // dispatch when clear)
    // +0x14d -- latches the global board-scroll flag at notify time: written exactly once, by
    // NotifyOrLaunch (`mov [ebx+0x14d], dl` at 0x44f5b6, from g_bBoardScrollFlag), and read
    // exactly once, by OnExit (`cmp byte [esi+0x14d], 1` at 0x450ba5) to decide whether to run
    // the teardown half of the scrollbar/window-style fixup. CORRECTED (v401, once
    // NotifyOrLaunch's body was actually read): the entry-side counterpart in Launch reads the
    // SAME global -- 0x485210 IS g_bBoardScrollFlag, and the old note here claiming the two
    // halves were gated on two different flags was reading Ghidra's `DAT_00485210` label as if
    // it were a distinct symbol.
    // `unsigned char`, not `bool` (v401): NotifyOrLaunch copies g_bBoardScrollFlag straight into
    // it with a plain `mov [ebx+0x14d], dl`; a `bool` field makes the conversion materialize as
    // `test dl,dl; setne cl` first.
    unsigned char bBoardScrollFlagAtNotify;
    bool bIconResourcesLoadedFlag; // +0x14e -- Launch loads pErrObj1/2/3/4/9 once, then sets
                                          // this; SelectCategory reads it to gate resetting
                                          // the presenter frame index
    unsigned char pad0x14f[1];
    unsigned int nPresenterFrameIndex; // +0x150 -- presenter animation frame -- see
                                              // nAnimTickCounter below
    HICON hIcon; // +0x154 -- TutorialWnd::Create's LoadIconA result, passed straight to
                       // PopupWndBase::Create's icon param
    // +0x158 -- the narration/presenter voice channel. RESOLVED (v396): really a
    // DSoundChannel*, retyped from the old raw `void*` -- OnExit calls DSoundChannel::Release
    // on it and reads its nSoundId (+0x38) to look the entry back up in the sound bank, and
    // the WM_TIMER tick polls DSoundChannel::IsReclaimable on it.
    DSoundChannel *pDSoundChannel;
    ResourceRefCategoryRecord categoryRecords[200]; // +0x15c

    unsigned char bCategoryFileLoaded; // +0x303c -- one-shot load gate
    unsigned char pad0x303d[3];
    int nSelectedItemIndex; // +0x3040
    int nListScrollOffset; // +0x3044 -- reset 0 on view launch; gates the prev-scan branch
    int nPrevItemIndex; // +0x3048 -- -1 sentinel
    int nNextItemIndex; // +0x304c -- -1 sentinel
    int nPrevScanCursor; // +0x3050
    int nNextScanCursor; // +0x3054
    int nScrollTimerId; // +0x3058 -- SetTimer(hwnd, 0x54, 10, NULL) id
    unsigned int dwUnk0x305c; // ctor-inits to -1, never read anywhere -- vestigial/dead
    unsigned int dwUnk0x3060; // ditto
    int nAnimTickCounter; // +0x3064 -- narration-synced 0..99 wrapping tick (WM_TIMER)
    int nTextLineHeight; // +0x3068 -- current font's line height, row-snap divisor
    // +0x306c -- UNSIGNED (v401): MapNotifyToItemIndex's inner switch over it splits its search
    // tree with `cmp ecx,0x848; ja`, not `jg`, and NotifyOrLaunch's own `subCode` parameter --
    // the value stored here -- is unsigned at every call site.
    unsigned int lastNotifyCode;
    int lastNotifySubcode; // +0x3070
    int nGlobalStateMirror; // +0x3074 -- mirrors g_nScreenState, the app screen-state selector

    // FUNCTION: LOCO 0x450520 -- selects categoryRecords[nIndex] as the current item: reloads
    // pErrObj5's icon resource (categoryRecords[nIndex].dwIconResourceId) and lays out pErrObj5's
    // (icon) and pErrObj6's (description text) own rects, either from pErrObj5's realized
    // cursor size (SetRect/OffsetRect around a fixed screen anchor) or, when the category
    // record supplies its own big-enough (>0x3c wide, >0x14 tall) rectA/rectB pair, those
    // literal per-item rects instead. Refreshes nav state and the prev/next nav-button
    // enabled-state bytes (bErrObj1Loaded/bErrObj2Loaded/bErrObjsLoaded).
    void SelectCategory(int nIndex);

    // FUNCTION: LOCO 0x450850 -- NOT an "index 0=selected row" helper (a stale hypothesis from
    // before this function's body was read; corrected 2026-07-17). Draws up to nChunkCount
    // word-wrapped "pages" of the CURRENTLY SELECTED item's own description text
    // (categoryRecords[nSelectedItemIndex].dwDescriptionStringId, a locale string id) into pHdc's rect
    // (this->pErrObj6->rect), one DT_WORDBREAK|DT_MODIFYSTRING-truncated chunk at a time.
    // Returns -1 as soon as a chunk's DrawTextA call didn't need to truncate (the remaining
    // text was the LAST chunk, i.e. fully consumed); RefreshListAndNavState's own caller
    // loop re-calls this with nChunkCount = 1, 2, 3, ... until it gets -1, to find the minimum
    // chunk count that displays the whole description (see its own comment).
    int DrawDescriptionChunks(int nChunkCount, HDC *pHdc);

    // FUNCTION: LOCO 0x4500a0
    void RefreshListAndNavState();

    // FUNCTION: LOCO 0x44f560 -- generic "notify the game view" API called from UI handlers all
    // over the binary. Records subCode/code (lastNotifySubcode/lastNotifyCode above),
    // early-exits in network mode or safe-mode, checks/updates a [TUTORIAL] ini key, on
    // fallthrough launches the tutorial view. See src/TutorialWnd.cpp.
    // Return type pinned `unsigned char` (was a guessed `unsigned int`) by the only call site
    // that actually READS it, MailWnd::OnActivate (0x42e4c4): the original tests `al`, not
    // `eax`. Every other call site in the binary discards the result, so this narrowing moves no
    // other TU's codegen.
    unsigned char NotifyOrLaunch(int code, unsigned int subCode);

    // FUNCTION: LOCO 0x44f750 -- formats the pending (lastNotifySubcode, lastNotifyCode) pair
    // into the "(sub)" / "(sub,code)" token NotifyOrLaunch matches against, and appends to, the
    // [TUTORIAL] ini value. See src/TutorialWnd.cpp.
    void FormatNotifyToken(char *pszOut);

    // FUNCTION: LOCO 0x44f9a0 -- maps the pending (lastNotifySubcode, lastNotifyCode) pair onto
    // the categoryRecords index the tutorial view should open on, or -1 when the pair has no
    // tutorial page of its own. See src/TutorialWnd.cpp.
    int MapNotifyToItemIndex();

    // FUNCTION: LOCO 0x44fb10 -- loads categoryRecords via RFIndex::LoadResource
    // (RF-archive-first, ifstream file-fallback) then
    // ResourceRefCategoryTable_ParseCategoryRecordsMaybe; its one-shot gate
    // (bCategoryFileLoaded) lives in the caller, Launch. See src/TutorialWnd.cpp.
    char ResourceRefCategoryTable_LoadCategoryFile();

    // 0x44fc80 -- parses the newline/token-delimited category record stream into
    // categoryRecords. Declared only; not yet transcribed.
    char ResourceRefCategoryTable_ParseCategoryRecordsMaybe(istream *pStream);

    // FUNCTION: LOCO 0x450240 -- the tutorial view's own launch entry point (called from
    // NotifyOrLaunch's fallthrough path). Stamps the app screen-state selector, releases the
    // placement cursor's capture, dirty-marks and redraws the world board, one-shot loads the
    // category file, measures the current line height, selects categoryRecords[nIndex] (see
    // SelectCategory above), ducks background audio, one-shot loads pErrObj1/2/3/4/9, shows
    // itself (own vtable slot 0x18), starts the scroll timer, resets animation/scroll state,
    // moves itself to a fixed screen anchor, recomputes the presenter animation frame, then
    // hands input capture to whichever window launched it (mail/album/edit-card, per
    // lastNotifySubcode).
    void Launch(int nIndex);

    // FUNCTION: LOCO 0x450450 -- the presenter-animation tick, driven off the 0x54 scroll timer
    // by OnTimerDefault above. Named in v398 once the body was read. See src/TutorialWnd.cpp.
    void AdvancePresenterFrame();

    // FUNCTION: LOCO 0x451180 -- the constructor's field-init helper: zeroes/sentinels every
    // scalar, `new`s all 9 ResourceRef icon slots (in the odd 3,1,2,4,5,9,6,7,8 order the
    // original really uses -- reproduced verbatim, and matched one-for-one by
    // ReleaseIconResources below), then clears the whole 200-entry categoryRecords table.
    // Keeps its Ghidra name; it is not a separate class's method (see the header note above
    // ResourceRefCategoryRecord).
    void ResourceRefCategoryTable_InitMaybe();

    // FUNCTION: LOCO 0x451440 -- the destructor's counterpart to
    // ResourceRefCategoryTable_InitMaybe: `delete`s all 9 ResourceRef icon slots (same odd
    // order), NULLing each as it goes, then clears bIconResourcesLoadedFlag.
    void ReleaseIconResources();
};

extern TutorialWnd *g_pTutorialWnd; // DAT_004fd38c
