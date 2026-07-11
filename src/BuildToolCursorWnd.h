// BuildToolCursorWnd -- a PopupWndBase-derived build/placement-tool cursor overlay (bootstrap
// singleton #4, g_pBuildToolCursorWnd/DAT_00485258, class string MSGBOXWINDOWCLASS). ⚠ That
// class string is NOT stale dev vocabulary, and this comment said it was until v482: the window
// really is a message box. OnLButtonDown (0x437f90) and OnKeyDown (0x437180) below implement a
// yes/no confirmation -- icon slot B is "yes", icon slot C is "no", Enter/Y/y and Esc/N/n are
// their keyboard equivalents -- and the debug string this file emits on a failed blit is
// "Error drawing mb bitmap", mb for message box. The old "verified no message-box behavior
// anywhere in its call graph" check was looking in the wrong place: nothing CALLS those two,
// they are reached only as vtable slots off the message router. Owns a DDraw offscreen
// surface, a software-drawn mouse-tracked ghost/preview sprite (RedrawGhostCursor, 0x436d60), a
// 14-mode (toolMode 0-13) cursor-bitmap set (LoadCursorBitmapSet, 0x437670), and the 4-slot
// animated icon-overlay system below (docs/subsystems.md's BuildToolCursorWnd entry).
//
// Corrects a 2026-07-17 (v186) misattribution: DrawAllIconSlots/FUN_00437cf0/FUN_00437900 were
// guessed to be TutorialWnd methods from address-clustering alone (the "TutorialWnd method
// cluster" address range 0x437cf0-0x4527da). A vtable DATA-xref (DrawAllIconSlots lives at
// BuildToolCursorWnd's own vtable+0x1c, ground-truthed via its ctor's `this->base.vtable =
// &PTR_FUN_00478130` store) proved the real owner -- see CLAUDE.md's "content oracles beat
// address-boxing" lesson family.
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "PopupWndBase.h"
#include "CursorDesc.h"
#include "LocoBitmap.h"

// A minimal partial view of the heap-allocated record BuildToolCursorWnd's ghost-cursor handle
// points at -- spawned by the DAT_004fd220 collection (Ghidra namespace `EffectSpawner`, effect
// id 0x2c0d), read for its +8 RECT and released back to that collection by OnExit. Polymorphic:
// RedrawGhostCursor dispatches slots 1 and 3 on it. Only the +8 RECT and those two slots are
// modeled; the rest of the layout and the other vtable slots are unread.
struct WorldDirtyRectNodeMaybe {
    virtual void *_v00();                 // slot 0 (+0x0)
    // slot 1 (+0x4) -- called with no args immediately before MoveTo when the ghost sprite is
    // repositioned; behavior unread, presumably "unlink from the current tile bucket".
    virtual void DetachMaybe();
    virtual void *_v08();                 // slot 2 (+0x8)
    // slot 3 (+0xc) -- repositions the spawned effect at a world-space (x, y).
    virtual void MoveTo(int x, int y);

    unsigned char pad0x4[4];
    RECT rect; // +8
};

class BuildToolCursorWnd : public PopupWndBase {
public:
    BuildToolCursorWnd(HINSTANCE hInstance, UINT resourceId); // 0x436b20
    virtual ~BuildToolCursorWnd(); // 0x436bb0
    // 0x436c50 -- the original narrows the base Create's byte result with the byte-wide
    // `test al,al; setne al`. Reproducing that is a matter of the RETURN STATEMENT'S SPELLING,
    // not of this return type: `return Base::Create(...) != 0;` compiles the dword-wide
    // `neg al; sbb eax,eax; neg eax` (DIFF(14)) under BOTH `bool` and `unsigned char`, while the
    // `if (...) return 1; return 0;` form used in the body is EXACT under both. Measured v444;
    // `bool` kept because that is what Ghidra's own signature says.
    bool Create(HWND hwndOwner);

    // 0x437670 -- (re)realizes the 14-mode (toolMode 0-13) CursorDesc/LocoBitmap set into the
    // pDescA/B/C/D + pBitmapA/B/C/D slots and latches bCursorResLoaded. Declared-only; called
    // by Create and ShowTool.
    void LoadCursorBitmapSet();

    // 0x436d60 -- software-draws the mouse-tracked ghost/preview sprite for the active tool.
    // Declared-only; called by ShowTool and the mouse-move path.
    void RedrawGhostCursor();

    unsigned char bModeChangeNotifyPending; // +0x118 -- zeroed at NotifyToolModeChanged's entry, then read
                                    // back later in the SAME function (also duplicated in sibling
                                    // FUN_004370f0) as part of a PostMessage(g_pApp[2], 0x401,
                                    // wParam, lParam) whose wParam/lParam derive from toolMode/
                                    // toolParam2Maybe/itself; exact role beyond that not yet read
    unsigned char pad0x119[3];
    unsigned int toolParam2Maybe; // +0x11c
    unsigned char bIconDrawReadyFlag; // +0x120 -- latched to 1 in DrawAllIconSlots once
                                   // bCursorResLoaded != 0; gates every icon-slot blit in this
                                   // cluster (DrawIconFrame/DrawToolLabel/DrawAllIconSlots/
                                   // StartSlotDAnimation/AdvanceSlotDAnimation all check it first)
    unsigned char pad0x121[3];
    int toolMode;                 // +0x124, 0-13
    HICON hIcon;                  // +0x128
    RECT rectIconSlotB;               // +0x12c -- static icon-slot B on-screen rect
    RECT rectIconSlotC;               // +0x13c -- static icon-slot C on-screen rect
    RECT rectD;               // +0x14c -- the animated/current-tool icon slot D rect;
                                    // also DrawToolLabel's own text label rect
    // +0x15c -- the UNION of rectIconSlotB and rectIconSlotC, recomputed by RefreshClientRect
    // (UnionRect(&this->rectIconSlotUnion, &rectIconSlotC, &rectIconSlotB)) so OnMouseMove can
    // ask one question -- "is the pointer anywhere in the yes/no button pair, including the gap
    // between them?" -- with a single PtInRect. Was pad0x15c[0x10] until v483.
    RECT rectIconSlotUnion;
    unsigned char bCursorResLoaded;          // +0x16c
    unsigned char bCursorResLoadedCompanion; // +0x16d
    unsigned char pad0x16e[2];
    CursorDesc *pDescB;  // +0x170
    LocoBitmap *pBitmapB;     // +0x174 -- pDescB's realized frame bitmap
    CursorDesc *pDescC;  // +0x178
    LocoBitmap *pBitmapC;     // +0x17c
    CursorDesc *pDescD;  // +0x180
    LocoBitmap *pBitmapD;     // +0x184
    CursorDesc *pDescA;  // +0x188
    LocoBitmap *pBitmapA;     // +0x18c
    unsigned int nCurrentFrameIndex; // +0x190 -- slot D's own frame index, advanced by
                                           // AdvanceSlotDAnimation (called every tick from the WM_TIMER
                                           // handler OnAnimTimer); 0xffffffff means "inactive"
    unsigned int nAnimSubPhase;      // +0x194 -- 0->2->4->5->6 transition counter
    unsigned int nFrameIndexC;       // +0x198 -- slot C's frame multiplier
    unsigned int nRedrawTimerId;     // +0x19c -- SetTimer(hwndSelf,1,200,NULL) id, set by
                                            // ShowTool; KillTimer's target in OnExit.
    short nAnimState;                // +0x1a0 -- current animation-set selector (0-4)
    unsigned char pad0x1a2[6];
    unsigned int nFrameIndexBStopValue;  // +0x1a8 -- when nFrameIndexB reaches this
                                               // value, OnAnimTimer stops advancing slot B
    unsigned int nFrameIndexB;           // +0x1ac -- slot B's own frame counter
    unsigned int nFrameIndexBLoopStart;  // +0x1b0 -- wrap-to value once past the loop end
    unsigned int nFrameIndexBLoopEnd;    // +0x1b4 -- last frame index before wrapping
    unsigned int nAnimTickCount; // +0x1b8
    unsigned char bToolActive;    // +0x1bc -- gates other placed objects' drag-start
                                   // (FUN_0044a0c0) while a tool is active
    unsigned char pad0x1bd[3];
    WorldDirtyRectNodeMaybe *pDirtyRectHandle; // +0x1c0 -- a world-board dirty-rect tracking
                                   // handle; OnExit reads its own +8 RECT and removes the handle
                                   // from its owning collection (opaque
                                   // FUN_00423d20/DAT_004fd220) once flushed.
    RECT rectPrevGhost;            // +0x1c4 -- the PREVIOUS frame's ghost-sprite rect, snapshotted
                                   // by RedrawGhostCursor (CopyRect from the handle's own +8 RECT)
                                   // just before the handle is moved, so the union of old and new
                                   // can be dirtied in one go. Was pad0x1c4[0x10] until v444.

    // FUNCTION: LOCO 0x437900 -- shared blit helper: draws one frame of pBitmap into *pRect
    // (offset horizontally by nFrameIndex*width when nFrameIndex != 0), gated by bIconDrawReadyFlag.
    // pDescUnused is a genuine dead parameter (every caller passes a CursorDesc*,
    // never read in this body -- see CLAUDE.md's dead-parameter "sic" family).
    void DrawIconFrame(RECT *pRect, int nFrameIndex, CursorDesc *pDescUnused, LocoBitmap *pBitmap);

    // FUNCTION: LOCO 0x437cf0 -- when toolMode==8, draws a shadow-outlined locale string (id
    // 0x6c) centered in rectD: 4 offset dark passes then a final white top pass. Shared
    // epilogue of DrawAllIconSlots, StartSlotDAnimation (cases 3/4), and AdvanceSlotDAnimation (case 4), always
    // called immediately after a blit into rectD/pBitmapD.
    void DrawToolLabel();

    void ShowTool(int mode, int param2); // 0x436ec0

    // FUNCTION: LOCO 0x4379c0 -- draws all 4 icon slots in sequence (A, B, C, then
    // StartSlotDAnimation(4) + an own vtable+0xc call, then D + DrawToolLabel as its epilogue).
    // See src/BuildToolCursorWnd.cpp and docs/subsystems.md for the full derivation.
    //
    // ⚠ CORRECTED in v545, and like 0x438890 below the correction is a BYTE fix rather than
    // bookkeeping. This was modeled as a zero-argument `void DrawAllIconSlots()` -- a name
    // invented from the behavior -- but it IS PopupWndBase's slot 7 (vtable+0x1c, which this
    // header's own class note has recorded all along) and the image ends it `ret 0x4`: ONE
    // stack argument, which a zero-arg __thiscall cannot emit (it compiles to a bare `ret`).
    // The base contract for the slot is OnDrawContent(PAINTSTRUCT *). The body never reads the
    // argument -- it repaints the whole 4-slot cluster unconditionally -- which is exactly why
    // the arity went unnoticed for so long. Found by tools/vtable_audit.py; same family as the
    // v544 0x438890 correction and CLAUDE.md's v477 CarNetObj lesson.
    virtual void OnDrawContent(PAINTSTRUCT *pPs);

    // FUNCTION: LOCO 0x438280 -- (re)starts slot D's animation in one of 5 states (0-4):
    // state 0 is the idle loop (entry chosen dynamically by pDescD's own wActiveFrameSetIndex
    // selector); states 1/2 seed a single frame from fixed entries in pDescD's raw
    // per-frame table (paFrameEntries) and share a common draw tail; state 3 draws entry 0's
    // own frame directly (a one-shot "return to base" pose) then its own DrawToolLabel +
    // vtable+0xc epilogue; state 4 seeds entry 5's frame the same way, transitioning
    // nAnimSubPhase to 5 (continued by AdvanceSlotDAnimation's own case 4). No-ops if already in
    // the requested state.
    void StartSlotDAnimation(unsigned int nState);

    // FUNCTION: LOCO 0x438590 -- per-tick frame ADVANCE for slot D's animation (called every
    // WM_TIMER tick by OnAnimTimer, after StartSlotDAnimation may have (re)started a state). Inert
    // (returns immediately) once nCurrentFrameIndex is the 0xffffffff sentinel. Cases 0-2
    // just re-seed a fixed frame from pDescD's raw table each tick (same entries
    // StartSlotDAnimation uses to start those states) and share StartSlotDAnimation's own draw tail via
    // LAB_0043833f-equivalent shared logic. Case 3 counts frames up to entry0's own count
    // field, restarting state 0 via StartSlotDAnimation(0) once exhausted. Case 4 is a 2-phase
    // sub-machine driven by nAnimSubPhase: sub-phase 5 counts DOWN from entry5's own
    // frame count then transitions to sub-phase 6 (reseeding from entry 6); sub-phase 6 counts
    // UP until entry6's own count field, then restarts state 0.
    void AdvanceSlotDAnimation();

    // FUNCTION: LOCO 0x438890 -- vtable slot 0x60 (WM_KILLFOCUS) override. State-change side
    // effect: clears bModeChangeNotifyPending, plays a UI sound cue (PlayUiSound(0x5015)), then
    // for most tool modes conditionally PostMessageA's the main app window (g_pApp->hwndOwner,
    // WM 0x401) with toolMode/toolParam2Maybe or (mode 8) bModeChangeNotifyPending's own
    // (already-zeroed, so always-0 -- sic) value, calls the base class's own vtable+4 slot, then
    // clears bIconDrawReadyFlag (deactivating the whole icon-overlay cluster). Also reached by
    // ShowTool (0x436ec0, not yet transcribed) whenever the active tool mode changes.
    //
    // ⚠ CORRECTED in v544, and the correction is a BYTE fix, not just bookkeeping. This was
    // modeled as a zero-argument `void NotifyToolModeChanged()` -- a name invented from the
    // behavior -- until tools/vtable_audit.py, run against PopupWndBase's newly-complete 37-slot
    // table, reported this address sitting in slot 24 where the base holds DefWindowProcStub.
    // The image agrees: 0x438890 ends in `xor eax,eax; pop esi; ret 0x10`, i.e. FOUR stack
    // arguments and an LRESULT return, which a `void ()` signature cannot emit (it compiles to a
    // bare `ret`). Exactly CLAUDE.md's v477 CarNetObj lesson -- the `.rdata` vtable dword is the
    // ground truth for what a slot is, and a plausible behavioral name is not evidence about the
    // signature. There were no call sites, so nothing else had to move.
    virtual LRESULT OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x438940 -- the WM_TIMER handler driving this whole 4-slot animation
    // system: gated on bIconDrawReadyFlag and hwndSelf, restarts slot D's idle state (StartSlotDAnimation(3))
    // once nAnimTickCount exceeds 0x27 ticks, always calls AdvanceSlotDAnimation to advance slot D,
    // then separately advances slot B's own independent frame counter
    // (nFrameIndexB/nFrameIndexBStopValue/nFrameIndexBLoopStart/
    // nFrameIndexBLoopEnd) and blits it. Falls through to DefWindowProcA either way.
    //
    // This IS PopupWndBase's slot 11 (vtable+0x2c, WM_TIMER), promoted from an ordinary member
    // in v545 -- the arity was already right (`ret 0x10`, four stack args, LRESULT), only the
    // `virtual` was missing, so unlike OnDrawContent above this one is pure vtable modeling and
    // cost nothing. It has no call site anywhere in src/; the message router reaches it through
    // the slot alone.
    virtual LRESULT OnTimerDefault(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x4370f0 -- the shared "commit the tool-mode change" tail: PostMessageA's
    // the main app window (g_pApp->hwndOwner, WM 0x401) with toolMode/toolParam2Maybe -- or, for
    // mode 8, bModeChangeNotifyPending's own value -- then calls the base's vtable+4 (OnExit)
    // slot and clears bIconDrawReadyFlag. NotifyToolModeChanged (0x438890) is this exact tail
    // behind its own `bModeChangeNotifyPending = 0` + PlayUiSound(0x5015) prefix, which is why
    // the two bodies read as duplicates.
    void PostToolModeChangeMaybe();

    // FUNCTION: LOCO 0x437f90 -- vtable slot 0x34 (WM_LBUTTONDOWN). The MOUSE half of this
    // window's yes/no confirmation: icon slot B is the "yes" hit rect and icon slot C the "no"
    // one. Both arms flash their own icon (frame A, 150 ms, frame B), play the same 0x5015 cue
    // and then commit -- "yes" with bModeChangeNotifyPending latched to 1 and an explicit
    // PostToolModeChangeMaybe() call, "no" with it cleared and the same tail expanded in line.
    // OnKeyDown (0x437180) is the KEYBOARD half of the identical decision.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // Slot-0x3c (WM_RBUTTONDOWN) override -- declared-only. The class vtable dword at 0x47816c
    // holds 0x451520: a right-click takes the same yes/no arm a left-click does. Same
    // ICF-folded `return OnLButtonDown(...)` body CreditsWnd installs at its own slot 15,
    // transcribed and marked as TutorialWnd::OnRButtonDown. Recovered in v544.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // Slot-0x90 (WM_WINDOWPOSCHANGING) override -- declared-only, and the LAST slot in this
    // hierarchy's table. The class vtable dword at 0x4781c0 holds 0x426ac0, which is
    // WindowBase::OnEraseBkgnd's body (a bare `return 1`) ICF-folded in here -- i.e. this window
    // claims to have handled the message rather than letting DefWindowProcStub run. Transcribed
    // and marked in src/WindowBase.cpp. Recovered in v544.
    virtual LRESULT OnWindowPosChanging(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x437180 -- vtable slot 0x50 (WM_KEYDOWN). The KEYBOARD half of the same
    // yes/no confirmation OnLButtonDown drives with the mouse: Enter/Y/y take the "yes" arm
    // (icon slot B's flash), Esc/N/n the "no" arm (icon slot C's). Everything after the key
    // dispatch is the same code as the corresponding OnLButtonDown arm.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x438ad0 -- vtable slot 0x4c (WM_MOUSEMOVE). The HOVER feedback half of
    // the yes/no confirmation OnLButtonDown commits: hovering the "yes" rect (icon slot B) or
    // the "no" rect (icon slot C) swaps in the base class's hover cursor and starts slot D's
    // animation state 1 or 2 respectively; leaving BOTH restores the resting cursor and, only
    // if the pointer has also left their bounding union (i.e. it is not merely in the gap
    // between the two buttons) and slot D is currently in one of those two hover states, drops
    // back to idle state 0. Always chains the base handler, whose return value is its own.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x437ea0 -- vtable slot 0x7c (WM_CLOSE) override of PopupWndBase::OnClose
    // (a plain member on the base, so this `virtual` is the CreditsWnd.h precedent, not a true
    // C++ override). Swallows the close while the app is alive and not tearing down -- running
    // NotifyToolModeChanged's own commit tail (PlayUiSound(0x5015), the toolMode-keyed
    // PostMessageA switch, OnExit, bIconDrawReadyFlag = 0) and returning 0; only during
    // shutdown (g_nScreenState == 10, or g_pApp gone) does the base body run. See
    // src/BuildToolCursorWnd.cpp.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x4374f0 -- vtable slot 0x18 override of PopupWndBase::RefreshClientRect.
    // Runs the base implementation, then lays this message box out from the loaded cursor
    // resource set: the three icon-slot rects are placed at FIXED origins (slot B at (0xa4,
    // 0x21), slot C at (0xa4, 0x92), slot D at (10, 0x14)) and sized from their descriptors,
    // slot B's width coming from pBitmapB->width / pDescB->nTotalFrameCount (the per-frame
    // width of a horizontal strip) and slots C/D from pDescC's SHADOW size and pDescD's native
    // size. rectIconSlotUnion is then the union of B and C. Finally the WINDOW itself is
    // resized to pDescA's native size and centered against the full screen -- with
    // SWP_HIDEWINDOW, so the layout pass never makes the box visible on its own.
    // Inert entirely while bCursorResLoaded is 0, i.e. before LoadCursorBitmapSet has run.
    virtual void RefreshClientRect();

    // FUNCTION: LOCO 0x436f70 -- vtable slot 4 override of PopupWndBase::OnExit (see the
    // top-of-file comment for the per-class slot-4 divergence). Transcribed 2026-07-18 (v199)
    // -- see src/BuildToolCursorWnd.cpp for the full body and compile status.
    virtual void OnExit();
};
extern BuildToolCursorWnd *g_pBuildToolCursorWnd; // DAT_00485258
