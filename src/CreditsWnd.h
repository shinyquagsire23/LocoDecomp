// CreditsWnd -- a PopupWndBase-derived overlay playing the LEGO/credits animation
// (bootstrap singleton #8, g_pCreditsWnd/DAT_004fd390, class string CREDITSWINDOWCLASS
// id 509). Constructed last of the 8 singletons. DDraw-composited: Blt's its off-screen
// surface onto the primary. Loads resource id 0x3daf via a delimited text resource
// (BuildResourcePath/DecodeAndDrawFrame) driving a 216x216 canvas widget
// (pCanvasWidget, the LocoBitmap-vtable canvas class) on a 50ms timer ramp.
// See docs/subsystems.md's CreditsWnd entry for the full derivation.
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "PopupWndBase.h"
#include "LocoBitmap.h"
#include "CursorDesc.h"

// ⚖ THIS HEADER'S DECLARATION COUNT IS A THRESHOLD, and v537 crossed it deliberately.
// src/WorldBoardMaybe.cpp includes this header (for g_pCreditsWnd), and the FIRST declaration
// added here costs WorldBoardMaybe_ResetAllTilesMaybe (0x454fe0) its 211-byte EXACT. It is a
// THRESHOLD and not a parity bit, MEASURED three ways in one session: adding 1, 2 and 5
// declarations all cost exactly the same 211 B. So the toll is paid ONCE and every further
// declaration here is free -- which is why v537 landed FIVE vtable-slot overrides
// (0x40f7a0/0x40f870/0x40f840/0x40f7f0/0x40f820, +203 B, all EXACT on the first compile) rather
// than stopping at the first: net -8 B for five functions and five closed vtable slots.
// ⇒ If you are adding anything at all to this class, add EVERYTHING you have -- the marginal
//   declaration is free and only the first one is not. Retiring back below the threshold would
//   need all five gone, so 0x454fe0 is not recoverable by trimming here.
class CreditsWnd : public PopupWndBase {
public:
    CreditsWnd(HINSTANCE hInstance, UINT resourceId); // 0x40f1c0 -- EXACT, see src/CreditsWnd.cpp
    bool Create(HWND hwndOwner);                      // 0x40f510 -- EXACT, see src/CreditsWnd.cpp

    unsigned int nAnimProgress; // +0x118 -- kept as frameRampAccum/10 (a 0-100ish
                                       // blend/progress value), written by
                                       // AdvanceAnimationFrame; not yet read elsewhere.
    int frameRampCounter; // +0x11c -- a full int despite the small range (ramps up by 2/tick
                            // capped at 0xf; -10/0xfffffff6 sentinel) -- confirmed 4 bytes wide
                            // via its own *(int*)&frameRampCounter access idiom in both
                            // Show and AdvanceAnimationFrame, absorbing what looked
                            // like a 1-byte-field + 3-byte-pad gap up to frameRampAccum.
    int frameRampAccum; // +0x120 -- signed (confirmed via AdvanceAnimationFrame's own
                          // frameRampAccum/10 site, which compiles to the signed
                          // imul+sar+add-correction divide-by-10 idiom in the original, not
                          // the unsigned mul+shr idiom an unsigned type would produce).
    unsigned int nTimerId;       // +0x124 -- SetTimer(hwndSelf,0x7a,0x32,NULL)'s id
    unsigned int nFrameIndex;    // +0x128 -- current animation frame
    unsigned char bResourcesLoaded; // +0x12c -- gates the 3-resource release in OnExit;
                                           // set by InitPreviewCanvasLazy once its own
                                           // one-shot lazy init has run.
    unsigned char bAnimationStartedFlag; // +0x12d -- reset 0 in Show/OnExit; set 1 exactly once
                                   // by an un-namespaced function (0x40f8b1, AdvanceAnimationFrame-
                                   // adjacent, gated on bResourcesLoaded). Read by 3 vtable-slot
                                   // functions (0x40f7a0/0x40f7f0/0x40f840, confirmed via CreditsWnd's
                                   // own vtable DATA xrefs at +0x34/+0x50/+0x60) to gate input-
                                   // forwarding vs. a dismiss/PlaySoundById(0x5015)/clear-flag path.
    unsigned char pad0x12e[2];
    void *pFrameBitmap; // +0x130 -- pTileDesc's own GetOrLoadFrameBitmap(0,0)
                               // result (a vtable+4 call), set by InitPreviewCanvasLazy.
    CursorDesc *pTileDesc; // +0x134 -- TileKind_GetOrLoadDescriptor(0x3daf)'s
                                       // result -- resource id 0x3daf's bucket (0xe) maps to
                                       // CursorDesc per TileKind_GetOrLoadDescriptor's
                                       // own bucket table (docs/subsystems.md); released via
                                       // its own ReleaseRef (vtable slot 2) in OnExit.
    RECT rectCanvasPlacement; // +0x138 -- the canvas anchor rect, SetRect(0xf,0x14,0xf1,0xda)
                              // (15,20 - 241,218) by RefreshClientRect (0x40f5c0); its left/top
                              // feed BlitFadeCanvas's own OffsetRect of rectCanvas.
    unsigned short wColorKey; // +0x148 -- 16-bit DDraw colorkey (0x6b94 @ RGB555,
                                     // 0xd714 @ RGB565), picked by Show from the newly
                                     // created animation surface's own pixel format.
    unsigned char pad0x14a[2];
    IDirectDrawSurface *pAnimSurface; // +0x14c -- the 196x216 credits-animation DirectDraw
                                             // surface, lazily created by Show; Release()'d
                                             // unconditionally in OnExit (a plain COM
                                             // IUnknown::Release, vtable slot 2 -- confirmed via
                                             // the raw disasm's vtable+8 call shape).
    HICON hIcon; // +0x150 -- LoadIconA(hInstance, id 0x65), set by Create.
    char szResourcePathBuf[0x1000]; // +0x154 -- scratch buffer for the delimited
                                           // multi-entry resource path text, built by
                                           // BuildResourcePath and scanned by
                                           // DecodeAndDrawFrame.
    unsigned int nTileKindId; // +0x1154 -- the <NNN>-tag TileKind id parsed out of the
                                     // current frame's own entry by DecodeAndDrawFrame.
    LocoBitmap *pCanvasWidget; // +0x1158 -- the 216x196 (0xd8x0xc4) owned canvas, allocated
                                 // by InitPreviewCanvasLazy.
    RECT rectCanvas; // +0x115c -- BlitFadeCanvas's own source/dest positioning rect
                            // for pCanvasWidget's fade blit (offset by rectCanvasPlacement's
                            // left/top).
    CursorDesc *pOwnedObj2; // +0x116c -- a TileKind descriptor cache (same vtable family
                                   // as pTileDesc -- confirmed via the raw disasm's own
                                   // vtable+8 ReleaseRef call shape in OnExit); repopulated
                                   // by DecodeAndDrawFrame from nTileKindId.
    LocoBitmap *pTileBitmap; // +0x1170 -- the current TileKind's own realized frame bitmap
                                    // (pOwnedObj2->GetOrLoadFrameBitmap(0,0)'s result),
                                    // blitted by DecodeAndDrawFrame.
    RECT rectTile; // +0x1174 -- destination rect for pTileBitmap's blit + the
                          // frame caption's own DrawTextA rect, positioned by DecodeAndDrawFrame.

    // FUNCTION: LOCO 0x40f6a0 -- lazily inits the credits preview: fetches pTileDesc/
    // pFrameBitmap (TileKind id 0x3daf), allocates+creates the owned 216x196 raw-8bpp
    // pCanvasWidget, gated by bResourcesLoaded so it only runs once. See src/CreditsWnd.cpp.
    void InitPreviewCanvasLazy();

    // FUNCTION: LOCO 0x40fe50 -- builds the 3-part credits resource path into
    // szResourcePathBuf (RF-archive-first, loose-file-fallback, newline-delimited getline
    // loop). EFFECTIVE MATCH, byte_diff 192/1004 -- see src/CreditsWnd.cpp and docs/PARKED.md.
    void BuildResourcePath();

    // FUNCTION: LOCO 0x40f980 -- decodes/advances to frame nFrameIndex: NOT a pixel-level
    // decode (an earlier docs pass guessed "RF-Huffman scratch decode" -- corrected v199/v200:
    // it is a caption/frame-metadata decode driven by a delimited text resource). EFFECTIVE
    // MATCH, byte_diff 148/1214 (use --len 1214) -- see src/CreditsWnd.cpp and docs/PARKED.md.
    unsigned char DecodeAndDrawFrame(int nFrameIndex);

    // FUNCTION: LOCO 0x410280 -- fade-blend blit of pCanvasWidget (see plate comment in
    // Ghidra). See src/CreditsWnd.cpp.
    int BlitFadeCanvas();

    // FUNCTION: LOCO 0x40f890 -- vtable slot 0x1c override of PopupWndBase::OnDrawContent.
    // Ignores its PAINTSTRUCT* arg: gated on bResourcesLoaded, one-shot sets
    // bAnimationStartedFlag, blits pFrameBitmap over the whole window rect, then
    // BlitFadeCanvas + SetCursorDesc(cursorNormal) + CommitScreenUpdate. See
    // src/CreditsWnd.cpp.
    virtual void OnDrawContent(PAINTSTRUCT *pPs);

    // FUNCTION: LOCO 0x40f2a0 -- slot-8 override of PopupWndBase::Show (real, per-class; NOT
    // shared with BuildToolCursorWnd/TutorialWnd, which inherit the base body). See
    // src/CreditsWnd.cpp.
    virtual void Show();

    // FUNCTION: LOCO 0x40f3c0 -- WM_TIMER tick handler for the 0x7a-id timer Show
    // starts. See src/CreditsWnd.cpp.
    void AdvanceAnimationFrame();

    // FUNCTION: LOCO 0x40f480 -- slot-4 override of PopupWndBase::OnExit, same role as
    // BuildToolCursorWnd::OnExit/TutorialWnd::OnExit (base PopupWndBase::OnExit + cleanup).
    // See src/CreditsWnd.cpp.
    virtual void OnExit();

    // FUNCTION: LOCO 0x40f5c0 -- slot-0x18 override of PopupWndBase::RefreshClientRect
    // (confirmed by the class vtable dword at 0x477698). See src/CreditsWnd.cpp.
    virtual void RefreshClientRect();

    // Destructor 0x40f290 (vtable slot 0; the ??_G scalar-deleting thunk is 0x40f270).
    // Empty body: the vtable re-stamp (0x477680) and the PopupWndBase base chain are all
    // compiler-generated under /GX.
    virtual ~CreditsWnd();

    // FUNCTION: LOCO 0x40f760 -- slot-0x7c override of PopupWndBase::OnClose (confirmed by
    // the class vtable dword at 0x4776fc). Suppresses the close (returns 0) while the app
    // is alive and not already tearing down (g_nScreenState != 10); only lets the base
    // body run once shutdown is underway. See src/CreditsWnd.cpp.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x40f7a0 -- slot-0x34 override of PopupWndBase::OnLButtonDown (confirmed by
    // the class vtable dword at 0x4776b4; base 0x477680). "Click to skip the credits": once the
    // scroll has actually started, a click clears bAnimationStartedFlag, ends the session and
    // acknowledges with UI sound 0x5015, swallowing the click. Before the animation starts the
    // click is not ours -- it goes to the shared DefWindowProcStub default. See src/CreditsWnd.cpp.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // Slot-0x3c (WM_RBUTTONDOWN) override -- declared-only. The class vtable dword at 0x4776bc
    // holds 0x451520, not the base's DefWindowProcStub, so a right-click skips the credits
    // exactly like a left-click. 0x451520's body is one ICF-folded `return OnLButtonDown(...)`
    // through the vtable, shared by this class, BuildToolCursorWnd and TutorialWnd; it is
    // transcribed and marked as TutorialWnd::OnRButtonDown in src/TutorialWnd.cpp, so only that
    // copy can carry the address's marker. Recovered in v544 -- invisible until PopupWndBase's
    // vtable model reached its full 37 slots.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x40f870 -- slot-0x2c override of PopupWndBase::OnTimerDefault (confirmed by
    // the class vtable dword at 0x4776ac). The credits scroll's clock: while the animation is
    // running, every timer tick aimed at this window's own hwndSelf advances one frame. Swallows
    // the message either way. See src/CreditsWnd.cpp.
    virtual LRESULT OnTimerDefault(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x40f840 -- slot-0x4c override of PopupWndBase::OnMouseMove (class vtable
    // dword at 0x4776cc). Mouse movement only means anything once the scroll is running; before
    // that the window swallows it rather than letting the base do its hover work.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x40f7f0 -- slot-0x50 override of PopupWndBase::OnKeyDown (class vtable
    // dword at 0x4776d0). The KEYBOARD half of the "skip the credits" gesture: byte-for-byte the
    // same dismiss as OnLButtonDown, minus the base-default tail -- a keypress before the scroll
    // starts is simply swallowed rather than passed on.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x40f820 -- slot-0x60 override of PopupWndBase::OnKillFocus (class vtable
    // dword at 0x4776e0). Losing focus ends the credits UNCONDITIONALLY: no bAnimationStartedFlag
    // guard and no sound, just clear the flag and run OnExit.
    virtual LRESULT OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
};
extern CreditsWnd *g_pCreditsWnd; // DAT_004fd390
