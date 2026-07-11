// PopupWndBase -- the DirectDraw-composited overlay window base (see docs/subsystems.md's
// "The 8 bootstrap singleton windows" table; NOT the same hierarchy as WindowBase, the real
// Win32 HWND family -- see WindowBase.h's own note). Base of BuildToolCursorWnd (0x1d4),
// TutorialWnd (0x3078), and CreditsWnd (0x1184). Sizeof 0x118 (280 bytes).
//
// Field layout mirrors the Ghidra DB directly, same convention as WindowBase.h: no explicit
// vtable field (the compiler synthesizes it), `Unk0xNN`-named fields match Ghidra's own
// placeholder names verbatim (size known, purpose not), untouched byte ranges Ghidra still
// shows as raw undefined bytes are left as plain `pad0xNN` blocks. Only sparsely modeled so
// far -- most of the struct beyond the fields below remains unread.
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "CursorDesc.h"

struct LocoBitmap; // src/LocoBitmap.h -- only ever a pointer here.

// One preloaded cursor slot: the descriptor, the frame bitmap realized from it, and the
// DirectDraw mask surface that bitmap owns. PopupWndBase keeps two (see cursorNormal /
// cursorHover below); FUN_00414130 fills a slot with the same four steps each time --
// UIResources::TileKind_GetOrLoadDescriptor(id) -> pDesc, pDesc->GetOrLoadFrameBitmap(0, 0) ->
// pBitmap, LocoBitmap::Convert(pBitmap), then pBitmap->pSurface -> nMaskSurfaceKey (and, for the
// first slot only, the descriptor's own +0x14/+0x16 native size into Unk0x3c/Unk0x40).
struct PopupCursorSlot {
    // +0x0 -- typed `int` rather than `IDirectDrawSurface *` deliberately: this is the value
    // SetCursorDesc takes as its `int nKey` parameter, and that signature is already
    // byte-matched. It is genuinely a surface pointer -- the same dual-purpose documented on
    // nCursorDescKey (+0x14), which is the field SetCursorDesc stores it into.
    int nMaskSurfaceKey;
    // +0x4 -- the realized frame bitmap. Written and released with the rest of the slot, but no
    // transcribed consumer reads it back; it exists so the slot owns a reference.
    LocoBitmap *pBitmap;
    CursorDesc *pDesc; // +0x8 -- released via its vtable slot 8 (ReleaseRef) by ~PopupWndBase
};

class PopupWndBase {
public:
    // 0x413ab0 (Ghidra: PopupWndBase::Ctor) -- declared-only, like the dtor below. Every
    // derived class's own ctor calls it as its base initializer; TutorialWnd's (0x44f490) is
    // the first transcribed one.
    PopupWndBase(HINSTANCE hInstance, UINT resourceIdArg);

    // 0x413b70 -- the exact inverse of LoadCursorSlots: releases both cursor slots, this
    // popup's share of the process-wide cursor composite surface, and its own offscreen
    // surface. Defined out-of-line in src/PopupWndBase.cpp (unlike LocoBitmap's, which is
    // deliberately inline) -- the original has a real out-of-line ??1 COMDAT at 0x413b70 plus
    // the compiler's own ??_G scalar-deleting thunk at 0x413b50, which is exactly what an
    // out-of-line definition emits.
    virtual ~PopupWndBase();

    // ---- vtable slots 0x4-0x90, declaration order IS vtable order (slot numbers
    // ground-truthed; see docs/subsystems.md's "PopupWndBase message dispatch" section).
    // The block grew 0x20 -> 0x4c in v397 (so TutorialWnd's five message-handler overrides
    // could be real `virtual` overrides rather than raw slot casts), -> 0x50 in v482, and
    // -> the FULL table 0x90 in v544 (see the slot 0x54-0x90 block comment below for the
    // ground-truthing). PopupWndBaseVtblProbe (below) is therefore fully redundant as a TYPE
    // now -- every slot it declares is a real virtual on the class -- but it is deliberately
    // RETAINED and PopupWndBase_RouteMessage still dispatches through it: measured in v544,
    // deleting it and retargeting those 17 call sites to `pWnd->` is byte-IDENTICAL for this
    // TU yet costs src/WorldBoardMaybe.cpp's 0x454fe0 its 211-byte exact, one of that header
    // -declaration-count parity effects CLAUDE.md warns about. src/WindowBase.h keeps its own
    // probe on the same footing, so this is the family's established shape, not an oversight. ----

    // vtable slot 4 (0x413c10, Ghidra: Hide -- renamed OnExit in src 2026-07-21 when the slot
    // was modeled as a real virtual: one C++ name must serve the base default and every
    // per-class override, and the derived overrides (CreditsWnd::OnExit, TutorialWnd::OnExit,
    // BuildToolCursorWnd::OnExit) were already named OnExit; sync parked via // TODO: sync at
    // the definition until Ghidra is renamed). The base default for the per-class OnExit
    // override family, and itself the Hide half of the Show/Hide pair: calls
    // SetModalCapture(1) (release), KillTimer's nShowTimerId, releases mouse capture if
    // Unk0x88 is set and this window holds it, rebinds the ddraw window clipper, then Blt's
    // rectScreenBounds back from the offscreen work surface to the primary surface (the mirror
    // image of Show's own Blt, which goes the other direction) -- applying the board-scroll
    // offset rects (g_rectAppWindowBounds/g_rectAppClientBounds) the same way
    // RedrawSoftwareCursorOverBoard does when g_bBoardScrollFlag is set, and releasing the
    // work surface's lock guard first if held. Finishes by rebinding the clipper to the
    // active screen, ShowWindow(SW_HIDE), and clearing bShown. Called base-qualified
    // (dispatch-bypassing) by the derived OnExit overrides' own bodies. Transcribed
    // 2026-07-18 (v199) -- see src/PopupWndBase.cpp for compile status.
    virtual void OnExit();

    // vtable slot 8 (0x413d10) -- shows the popup: calls SetModalCapture(0) (acquire path),
    // starts the cursor-redraw timer (id 0x43, ~190ms, stashed at nShowTimerId), sets
    // bSuppressCursorRedraw/bShown, calls its own vtable slot 0x1c(0) and rebinds the ddraw
    // clipper, then Blt's the primary surface from the offscreen surface through the popup's
    // own rect (rectScreenBounds). Inherited unoverridden by BuildToolCursorWnd/TutorialWnd
    // (both point their own vtable slot 8 here; multiple vtable DATA xrefs, plus direct calls
    // from TutorialWnd::Launch and BuildToolCursorWnd::ShowTool) -- CORRECTED 2026-07-17
    // (v191): NOT shared by CreditsWnd, whose own slot 8 is a different, unread function
    // (0x40f2a0) -- a live slot-0/4/8/0xc dump across all 3 singletons disproved the earlier
    // "shared across every singleton" claim for this one slot (see docs/subsystems.md's
    // PopupWndBase entry). Transcribed 2026-07-18 (v199), straight-line/no branches -- see
    // src/PopupWndBase.cpp for compile status.
    virtual void Show();

    // vtable slot 0xc (0x414340) -- confirmed shared across all 3 PopupWndBase-derived
    // singletons (BuildToolCursorWnd, TutorialWnd, CreditsWnd all point vtable slot 0xc at
    // this exact address) -- unlike slot 4 (genuinely overridden per class, see
    // docs/subsystems.md) this one is never overridden. Sets the popup's active cursor/icon
    // descriptor (pActiveCursorDesc/nCursorDescKey), no-oping a same-key re-set; optionally
    // zeroes both cached redraw rects (bResetRects); optionally (bRedraw, and only
    // when not already hidden) rebinds the ddraw clipper and runs the full
    // RedrawSoftwareCursor/PopupWndBase_RebindClipperToActiveScreen/
    // RedrawSoftwareCursorOverBoard redraw chain -- own multi-session arc; both sibling
    // redraw methods are CONTENT-COMPLETE, still not byte-matched (see their own plate
    // comments). Polymorphic call sites (the original's raw `call [ecx+0xc]` shape) just
    // call it unqualified now that it's a real virtual.
    virtual void SetCursorDesc(int nKey, CursorDesc *pDesc, char bResetRects, char bRedraw);

    virtual void *_v10(); // slot 0x10 -- shared no-op stub (WindowBase::NoOpVirtualMaybe,
                          // takes 1 stack arg, 0x426130 `ret 0x4`)
    // vtable slot 0x14 (0x413de0) -- the real base Create, promoted from the old `_v14()`
    // placeholder (v396) so TutorialWnd::Create can call it base-qualified rather than
    // through a raw slot cast. Never overridden by any of the three derived singletons (all
    // three vtables point slot 0x14 straight here), and every call site reaches it as a
    // direct call, not a dispatch.
    // Return type pinned `unsigned char` (not Ghidra's `undefined4`) by its only transcribed
    // caller, TutorialWnd::Create: the original narrows with `test al,al; setne al`, which a
    // dword return would instead spell `neg/sbb/neg`.
    // The window title is NOT a parameter -- it is lifted off hwndOwnerArg with GetWindowTextA,
    // so a popup inherits its owner's caption. The window style is hardcoded
    // WS_POPUP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN; the only style the caller controls is the
    // CLASS style (dwClassStyle), which defaults to CS_HREDRAW|CS_VREDRAW when passed 0.
    // dwStyleUnused/dwUnkB are genuinely dead -- neither stack slot is read anywhere in the
    // body. Transcribed in src/PopupWndBase.cpp.
    virtual unsigned char Create(int nCmdShow, HWND hwndOwnerArg, int x, int y, int nWidth,
                                 int nHeight, HMENU hMenu, HICON hIconArg, UINT dwClassStyle,
                                 unsigned int dwStyleUnused, unsigned int dwUnkB,
                                 unsigned char bUnkC);

    // vtable slot 0x18 (0x4140a0, called generically by OnSize below)
    // The PopupWndBase analog of WindowBase::RefreshClientClipRect. Gated on bCreated:
    // GetClientRect's into rectClient, derives nClientWidth/nClientHeight from
    // it, snapshots rectClient into rectWindow, then derives nWindowWidth/
    // nWindowHeight from THAT snapshot. CONTENT-COMPLETE, NOT YET BYTE-MATCHED (2026-07-18):
    // every field read/write order confirmed 1:1 against raw disasm (the original reloads
    // rectWindow's own fields fresh from memory for the 2nd width/height pair rather than
    // reusing registers -- matched here); residual (DIFF 79/114) is our compile needing 2 extra
    // callee-saved spill registers (ebp/ebx) the original doesn't, a register-allocation
    // artifact, likely TU-position-dependent (Yoda lesson #7) -- not yet isolated to a specific
    // source lever, candidate for a future session.
    virtual void RefreshClientRect();

    // vtable slot 0x1c -- per-class "draw content" hook (called with the PAINTSTRUCT* from
    // OnPaint, NULL from Show); base default = the shared one-stack-arg no-op (0x426130).
    virtual void OnDrawContent(PAINTSTRUCT *pPs);
    // vtable slot 0x20 -- per-class "draw cursor overlay" hook; base default = the shared
    // bare-ret stub (0x4661a0).
    virtual void OnDrawCursorOverlay();

    // vtable slot 0x24 -- the message router itself (0x4143e0). NOT a real __thiscall method:
    // PopupWndBase_RouteMessage (declared at the bottom of this header) is a __stdcall free
    // function taking pWnd as an explicit 5th stack arg, so it cannot be spelled as a virtual
    // here; this placeholder exists only to hold the slot's position for the handlers below.
    // Never called through -- PopupWndBase_WndProc reaches 0x4143e0 as a direct call.
    virtual void *_v24();

    // ---- vtable slots 0x28-0x4c: the per-WM_* message handlers RouteMessage dispatches to.
    // The base class installs the shared DefWindowProcStub (0x422ea0) in every one of 0x28
    // through 0x48 -- a free function living in WindowBase's TU, so those nine are declared-only
    // here (same idiom as WindowBase.h's own handler block). Only slot 0x4c has a real
    // PopupWndBase body. TutorialWnd overrides 0x28/0x2c/0x34/0x3c/0x4c. NOTE: a slot number in
    // THIS hierarchy does not mean the same WM_* as the identically-numbered WindowBase slot --
    // the two dispatch tables were laid out independently. ----
    virtual LRESULT OnUnhandledMessage(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x28 -- fallback default
    virtual LRESULT OnTimerDefault(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);     // 0x2c -- WM_TIMER
    virtual LRESULT OnCreate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);           // 0x30 -- WM_CREATE
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);      // 0x34 -- WM_LBUTTONDOWN
    virtual LRESULT OnLButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);        // 0x38 -- WM_LBUTTONUP
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);      // 0x3c -- WM_RBUTTONDOWN
    virtual LRESULT OnRButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);        // 0x40 -- WM_RBUTTONUP
    virtual LRESULT OnLButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);    // 0x44 -- WM_LBUTTONDBLCLK
    virtual LRESULT OnRButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);    // 0x48 -- WM_RBUTTONDBLCLK

    // FUNCTION: LOCO 0x414a80 (vtable slot 0x4c, WM_MOUSEMOVE) -- only acts if the message's
    // target hwnd is this window's own hwndSelf: rebinds the ddraw clipper, redraws the
    // software cursor (+ the over-board variant if Unk0x88 is set), rebinds the clipper to the
    // active screen. Otherwise a no-op (always returns 0). **EXACT since v362**: the residual
    // (DIFF 22/62) was the supposed "dead mov ecx,esi before the static
    // PopupWndBase_RebindClipperToActiveScreen() call" -- which was never dead. It is that
    // member's `this` pass; dropping `static` from its declaration closed this function
    // outright (lever 3). Became a real `virtual` in v397 when this block was extended to
    // 0x4c; the promotion is codegen-inert for the body itself (verified by a before/after
    // COMDAT byte diff), it only lets TutorialWnd::OnMouseMove be a genuine override.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // 0x50 -- WM_KEYDOWN. Base default: the shared DefWindowProcStub (0x422ea0), so declared-only
    // here. The block is extended this far (v482) for BuildToolCursorWnd::OnKeyDown (0x437180),
    // the keyboard half of that window's yes/no confirmation. Same reasoning as v397's extension
    // to 0x4c: the promotion is codegen-inert for every existing body, it only lets the override
    // be a genuine one. Slot numbering follows this hierarchy's own table -- it is WindowBase's
    // 0x54 shifted by the one slot PopupWndBase's table lacks at the front.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // ---- vtable slots 0x54-0x90 (slots 21-36), the rest of the message-handler block.
    // Extended to the FULL table in v544 from tools/vtable_audit.py's finding that this class
    // was modeled ~15 slots short: our vtable COMDAT emitted 21 entries where the image's
    // (0x477898) runs 0..36 (37 entries, then a NULL dword at 0x47792c that is NOT part of it --
    // WindowBase's 0x477c30 and CreditsWnd's 0x477680 end the same way). Truncating the model
    // there did not merely lose documentation: CreditsWnd's and BuildToolCursorWnd's own
    // OnClose/OnKillFocus overrides, having no base slot to override, were appended as NEW
    // virtuals and landed at slots 21/22 instead of 31/24.
    //
    // Names and slot assignments are NOT new guesses -- they are PopupWndBaseVtblProbe's own,
    // ground-truthed from PopupWndBase_RouteMessage's real switch, and independently confirmed
    // against the image here: the table is structurally PARALLEL to WindowBase's (0x477c30)
    // shifted by exactly the one slot this hierarchy lacks at the front (its router sits at 9,
    // WindowBase's at 10), and every slot the two share resolves to the SAME address --
    // OnMouseActivate 22/23 = 0x426950, OnSetCursor 27/28 = 0x426a60, OnEraseBkgnd 29/30 =
    // 0x426ac0, OnDestroy 30/31 = 0x426ad0. Those four bodies live in WindowBase's TU and are
    // declared-only here (see the WindowBase-shared-defaults note further down): the two classes
    // are SIBLINGS over a shared handler block, not one deriving from the other, so the original
    // near-certainly defined each twice and the linker ICF-folded the identical pairs -- only one
    // of the two can ever carry the address's marker. Slot 36 (WM_WINDOWPOSCHANGING) is this
    // hierarchy's own; WindowBase's table has no counterpart and ends at its slot 36
    // (WM_ACTIVATEAPP), so the two tables are the same LENGTH despite the one-slot shift. ----
    virtual LRESULT OnKeyUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);          // 0x54 -- WM_KEYUP. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    virtual LRESULT OnMouseActivate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // 0x58 -- WM_MOUSEACTIVATE. 0x426950, WindowBase's TU -- declared-only.
    virtual LRESULT OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);       // 0x5c -- WM_SETFOCUS. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    // 0x60 -- WM_KILLFOCUS. Base default: DefWindowProcStub 0x422ea0, so declared-only here.
    // Overridden by CreditsWnd (0x40f820) and BuildToolCursorWnd (0x438890).
    virtual LRESULT OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x414ac0 (vtable slot 0x64, WM_SIZE) -- if bCreated, calls vtable +0x18
    // (RefreshClientRect) generically. EXACT MATCH.
    virtual LRESULT OnSize(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // FUNCTION: LOCO 0x414ae0 (vtable slot 0x68, WM_PAINT) -- only acts if bShown: rebinds the
    // clipper, GetUpdateRect/BeginPaint/EndPaint, then calls vtable +0x1c (passed the
    // PAINTSTRUCT*) and vtable +0x20 (no args) before rebinding the clipper to the active
    // screen -- the per-class "draw content"/"draw cursor overlay" hooks; both are the shared
    // WindowBase::NoOpVirtualMaybe/bare-ret defaults on the base class itself. Content includes
    // a genuine reproduced engine quirk (see the .cpp definition's own `sic:` comment --
    // EndPaint runs on an uninitialized PAINTSTRUCT when GetUpdateRect finds no update region,
    // since the original skips BeginPaint but not the EndPaint that follows it).
    // **EXACT since v362**: what an earlier note called a "dead mov ecx,esi before the static
    // RebindClipper call" was that member's `this` pass (lever 3, see OnMouseMove above), and
    // the second suspected residual -- the original's "redundant" vtable-pointer/this reloads
    // before its 2nd back-to-back vtable call -- turned out not to be a residual at all once
    // the first was fixed.
    virtual LRESULT OnPaint(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // Slots 0x6c/0x74/0x78 (WM_SETCURSOR/WM_ERASEBKGND/WM_DESTROY) -- installed with
    // WindowBase's shared default bodies (0x426a60/0x426ac0/0x426ad0), which live in
    // WindowBase's TU (address-contiguous with WindowBase_RouteMessage et al, and present in
    // WindowBase_Vtbl itself at slots 0x70/0x78/0x7c) and read hwndSelf at the +0x8 offset both
    // families share. The DEFINITIONS moved to WindowBase.cpp 2026-07-22 (v322) -- declared here
    // only to hold this family's vtable layout; the *NoOp names were shortened to WindowBase's
    // own virtual names (OnMouseActivate/OnEraseBkgnd) the same day. NOTE: a PopupWndBase-
    // hierarchy slot NUMBER does not name the same WM_* as the identically-numbered WindowBase
    // slot (OnMouseActivate sits at 0x58 here but at 0x5c there) -- but, per the block comment
    // above, the two tables DO agree on the underlying handler ADDRESS at every shared slot.
    virtual LRESULT OnSetCursor(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // 0x6c -- 0x426a60, extern
    virtual LRESULT OnShowWindow(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x70 -- WM_SHOWWINDOW. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    virtual LRESULT OnEraseBkgnd(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x74 -- 0x426ac0, extern
    virtual LRESULT OnDestroy(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);    // 0x78 -- 0x426ad0, extern

    // FUNCTION: LOCO 0x414b80 (vtable slot 0x7c, WM_CLOSE) -- clears bCreated, DestroyWindow's
    // hwndSelf, and PostQuitMessage(0)'s if hwndOwner is null (a top-level, ownerless popup
    // closing quits the whole app). Overridden by CreditsWnd (0x40f760, which chains back to
    // this base body) and BuildToolCursorWnd (0x437ea0).
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    virtual LRESULT OnNotify(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);            // 0x80 -- WM_NOTIFY. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    virtual LRESULT OnCommand(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);           // 0x84 -- WM_COMMAND. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    virtual LRESULT OnHotKey(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);            // 0x88 -- WM_HOTKEY. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    virtual LRESULT OnActivateApp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);       // 0x8c -- WM_ACTIVATEAPP. Default: DefWindowProcStub 0x422ea0 -- declared-only.
    virtual LRESULT OnWindowPosChanging(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x90 -- WM_WINDOWPOSCHANGING, the last slot. Default: DefWindowProcStub 0x422ea0 -- declared-only.

    void *hInstance;
    HWND hwndSelf;
    HWND hwndOwner;
    unsigned int resourceId;
    int nCursorDescKey; // +0x14 -- change-detection key paired with pActiveCursorDesc;
                              // SetCursorDesc no-ops a re-set that repeats the same nonzero
                              // key. CONFIRMED dual-purpose (v192, RedrawSoftwareCursor): the SAME 4-byte
                              // value is ALSO a real IDirectDrawSurface* -- a color-keyed cursor
                              // MASK surface, passed straight through to
                              // (masksurf)->Blt(..., DDBLT_KEYSRC|DDBLT_WAIT) as the source
                              // surface for the masked cursor-sprite blit. Kept `int` here (not
                              // retyped) to avoid touching SetCursorDesc's already-matched
                              // signature; RedrawSoftwareCursor casts it locally at the point of use.
    tagRECT rectScreenBounds; // +0x18 -- this window's own on-screen bounds rect (screen
                                    // coords), set by Move (x,y at left/top; x+width,y+height
                                    // at right/bottom). Read directly as a RECT* by RedrawSoftwareCursor
                                    // (IntersectRect/clip-clamp against the cursor rect).
    // +0x28 -- the offscreen surface's own bounds, in the surface's OWN coordinate space
    // (origin 0,0), as opposed to rectScreenBounds just above which is the same extent in
    // screen coords. Create fills it {0, 0, w, h} from the dimensions DDraw_QuerySurfaceDims
    // reports back for the surface it just created -- which is why the pair is not simply
    // rectScreenBounds normalized: DirectDraw is free to hand back a surface LARGER than the
    // requested extent. Modeled as one RECT (rather than two loose pairs) because Create writes
    // exactly 0/0/w/h into the four consecutive dwords; kept `...Maybe` until a reader turns up.
    RECT rectOffscreenBoundsMaybe;
    IDirectDrawSurface *pOffscreenSurface; // Unk0x38 (Ghidra's old placeholder name; unrelated to
                                                  // WindowBase's own distinct field at the same
                                                  // numeric offset)
    unsigned int Unk0x3c;
    unsigned int Unk0x40;
    CursorDesc *pActiveCursorDesc; // +0x44 -- the popup's currently-active icon/cursor
                                              // descriptor, set by SetCursorDesc; confirmed
                                              // via RedrawSoftwareCursor dereferencing it at the same
                                              // offsets as CursorDesc's own
                                              // nativeWidth/nativeHeight (+0x14/+0x16).
    int nCursorFrameIndex; // +0x48 -- current index into pActiveCursorDesc's horizontal
                                 // frame strip (really a scale/zoom-level index -- bounded by the
                                 // descriptor's own nTotalFrameCount field, not necessarily animation);
                                 // reset to 0 whenever the descriptor changes, clamped/advanced by
                                 // RedrawSoftwareCursor.
    unsigned int nShowTimerId; // +0x4c -- SetTimer(hwndSelf, 0x43, 0xbe, NULL)'s return
                                     // value, stashed by Show; KillTimer's target in Hide.
    int nLastCursorScreenX; // +0x50 -- last raw GetCursorPos().x; structural twin of
                                   // WindowBase::nLastCursorScreenX. Reset to -1 (sentinel/
                                   // invalid) by BOTH RedrawSoftwareCursor and its sibling
                                   // RedrawSoftwareCursorOverBoard on every real redraw pass
                                   // to force re-detection next tick -- CORRECTED (v195's "never
                                   // read by either" was refuted): the real reader is
                                   // PopupWndBase_RouteMessage's WM_TIMER case, which compares it
                                   // against a fresh GetCursorPos() to detect mouse movement.
    int nLastCursorScreenY; // +0x54 -- paired Y half of nLastCursorScreenX.
    bool bSuppressCursorRedraw; // +0x58 -- gates the WM_TIMER cursor-anim branch and the redraw
                                       // fast path; SetModalCapture toggles it in the same
                                       // release/acquire pattern as ShowCursor.
    unsigned char pad0x59[3];
    IDirectDrawSurface *pCompositeSurface; // +0x5c -- intermediate composite surface:
                                                  // RedrawSoftwareCursor blits the saved background
                                                  // (pOffscreenSurface) into it, then the
                                                  // masked cursor sprite (nCursorDescKey's
                                                  // surface) on top, then blits the composite onto
                                                  // the DDraw primary surface. Shares the SAME
                                                  // screen-space coordinate system as
                                                  // pOffscreenSurface/the primary surface
                                                  // (rects are passed through unmodified between
                                                  // them -- not a small per-sprite buffer).
    unsigned char pad0x60[4];
    HDC hdcOffscreen; // Unk0x64 -- GetDC out-param scratch for
                            // AcquireOffscreenSurfaceDC/its release counterpart (CommitScreenUpdate)
    RECT rectPrevCursor;  // +0x68 -- previous frame's redraw rect (RedrawSoftwareCursor's own
                                // dirty-rect union/cache against the newly computed cursor rect).
    RECT rectPrevCursorB; // +0x78 -- zeroed alongside rectPrevCursor in the same
                                // idiom; CONFIRMED 2nd reader (v195): this is
                                // RedrawSoftwareCursorOverBoard's own "previous frame" rect
                                // (its exact analog of RedrawSoftwareCursor's own
                                // rectPrevCursor/+0x68) -- the two sibling redraw methods
                                // each own one of the pair.
    unsigned char Unk0x88;
    unsigned char pad0x89[7];
    // +0x90 and +0x9c -- the popup's two preloaded cursor slots (see PopupCursorSlot above).
    // RESOLVED v397: what used to be modeled here as a loose `int` + `pad0x94[4]` +
    // `CursorDesc*` + an opaque `pad0x9c[0xc]` is really TWO IDENTICAL 12-byte records. The
    // evidence is symmetry across three functions: FUN_00414130 loads UI resource 0x1400 into
    // the first and 0x1403 into the second by exactly the same four-step sequence; ~PopupWndBase
    // tears both down by exactly the same guard-release-zero sequence; and TutorialWnd::
    // OnMouseMove picks between them by hover state, feeding whichever it picks to SetCursorDesc
    // as the (key, desc) argument pair -- which is what pins member 0 and member 2 as that
    // call's two arguments and leaves member 1 as the intermediate nobody else reads.
    PopupCursorSlot cursorNormal; // +0x90 -- UI resource 0x1400, the resting cursor
    PopupCursorSlot cursorHover;  // +0x9c -- UI resource 0x1403, shown over an enabled control
    char className[50];
    unsigned char pad0xda[1];
    unsigned char bCreated;
    int Unk0xdc; // +0xdc -- used as an x-left coordinate by BuildToolCursorWnd::OnExit's own
                  // dirty-rect SetRect; purpose beyond that unread.
    int Unk0xe0; // +0xe0 -- used as a y-top coordinate alongside Unk0xdc; purpose beyond that
                  // unread.
    int nClientWidth;  // +0xe4 -- rectClient.right - rectClient.left, set by
                              // RefreshClientRect
    int nClientHeight; // +0xe8 -- rectClient.bottom - rectClient.top
    int nWindowWidth;  // +0xec -- rectWindow.right - rectWindow.left
    int nWindowHeight; // +0xf0 -- rectWindow.bottom - rectWindow.top
    RECT rectClient;    // +0xf4 -- raw GetClientRect() result, written by RefreshClientRect
    RECT rectWindow;    // +0x104 -- this window's own nominal size rect (left/top usually
                              // 0,0; Move reads right/bottom as width/height added to the
                              // new x/y); also used directly as BuildToolCursorWnd's slot-A icon
                              // destination rect (DrawAllIconSlots), so the slot-A icon fills the
                              // entire popup.
    unsigned char bShown;
    unsigned char pad0x115[3];

    // FUNCTION: LOCO 0x414130 -- fills both cursor slots and binds the shared cursor composite
    // surface. Loads UI resource 0x1400 into cursorNormal and 0x1403 into cursorHover by the
    // same four steps each (see PopupCursorSlot above), lazily creates the process-wide 256x256
    // g_pCursorSurface the first time round, points pCompositeSurface at it and bumps its
    // refcount. Called by Create, immediately after RefreshClientRect. Transcribed in
    // src/PopupWndBase.cpp.
    void LoadCursorSlots();

    // FUNCTION: LOCO 0x414bb0
    // Acquires an HDC on this window's own offscreen DirectDraw surface (pOffscreenSurface)
    // via a GetDC-shaped retry loop (up to 1000x, Sleep(10) between attempts; fatal
    // ShowFatalErrorMessageBox(0x49)/ExitProcess(1) if it never succeeds), bracketed by a
    // Ddraw_RebindWindowClipper(hwndTarget) call. The acquired HDC is stashed at
    // hdcOffscreen and returned. Every call site passes this->hwndSelf as
    // hwndTarget. Same shape as WindowBase::AcquireWorkSurfaceDC (0x426b00), just against
    // a per-object offscreen surface instead of the global work surface.
    HDC AcquireOffscreenSurfaceDC(HWND hwndTarget);

    // FUNCTION: LOCO 0x414c20 -- release-side counterpart to AcquireOffscreenSurfaceDC AND the
    // popup's present step. ReleaseDC's hdcToRelease (via pOffscreenSurface's own vtable slot
    // +0x68) when non-null, then -- unless bSkipRedraw -- Blt's the offscreen surface onto the
    // primary surface at rectWindow translated by ClientToScreen, stamping the software cursor
    // into the offscreen surface for the duration of that present when one is active. NOTE the
    // third parameter's polarity: NONZERO SKIPS the redraw (every `(..., hdc, 1)` call site is a
    // pure DC release; every `(..., NULL, 0)` site is a pure present) -- it was declared
    // `bFullRedraw` here while untranscribed, which read exactly backwards. Transcribed in
    // src/PopupWndBase.cpp.
    void CommitScreenUpdate(HWND hwndTarget, HDC hdcToRelease, char bSkipRedraw);

    // FUNCTION: LOCO 0x414fb0
    // Redraws the software cursor via a background-restore + masked-composite pipeline:
    // 1) Computes the cursor's current screen rect from GetCursorPos() minus the descriptor's
    //    hotspot, clipped against rectScreenBounds; UnionRect's it with the previous frame's
    //    rect (rectPrevCursor) to get one dirty rect covering old+new positions.
    // 2) If bKeepRectsMaybe (param_1) and other conditions hold and the dirty rect is small
    //    (<0x100 square), takes a "small update" fast path; otherwise a full-dirty-rect redraw.
    // 3) Restores the saved background under the OLD cursor rect from pOffscreenSurface.
    // 4) Composites: pOffscreenSurface (background) -> pCompositeSurface, then the
    //    masked cursor sprite frame (nCursorDescKey's own surface, DDBLT_KEYSRC) on top of
    //    pCompositeSurface, then pCompositeSurface -> the DDraw primary surface.
    // Transcribed 2026-07-18 (v193) after an exhaustive raw-disasm trace (Ghidra's own decompile
    // has a confirmed confusion bug in both frame-offset blocks -- a transient `push 0x0` call
    // arg gets mismodeled as a persistent NULL local later dereferenced, and unrelated
    // "unaff_ESI"/"unaff_EDI" register values are really the surviving nClipWidth/nClipHeight
    // invariants computed ~100 instructions earlier -- do not trust that decompile's local_78/
    // unaff_ESI/unaff_EDI narrative in those two blocks). CONTENT-COMPLETE, NOT YET BYTE-MATCHED
    // (PARTIAL): every field read/write and Blt call site hand-verified 1:1 against raw disasm
    // (see the struct-field comments this session added: rectScreenBounds, nLastCursorScreenX/Y,
    // pCompositeSurface, plus CursorDesc's hotspotX/hotspotY/nTotalFrameCount), but
    // the compiled candidate still carries a large register-allocation/stack-layout residual --
    // same complexity class as WindowBase::CommitScreenUpdate/RedrawCustomCursor
    // (src/WindowBase.cpp), which needed multiple sessions and a live-register-dump lever (since
    // ruled out) to close. Do NOT re-suggest that lever here either.
    // v194 (2026-07-18) update: rewrote the bUnionModeMaybe early-return-then-fallthrough shape
    // (`if (bUnionModeMaybe) { ...; return; } <else-body>`) as a genuine `if (...) {...} else
    // {...}` -- both compile to an IDENTICAL single test/branch at the machine level (confirmed
    // by reading the original's own branch at raw offset 0x20d: a single `test cl,cl; je` on the
    // cached bUnionModeMaybe byte), but the real if/else SHAPE let the optimizer share more of the
    // two branches' mutually-exclusive RECT locals (rectBltSrc/srcRect declared separately in
    // each branch) -- compiled frame dropped 0x98 -> 0x78 (was 28 bytes OVER the original's 0x7c,
    // now 4 bytes UNDER it: one dword too much is now being shared that the original does NOT
    // share). asmscore (vs. the real Ghidra body length 1166): total 490519 (was ~490563),
    // byte_diff 499/1166 (was 543), insns 380/390 (unchanged) -- align (474) and reg_pen (145)
    // components are UNCHANGED, confirming the bulk of the residual is a separate, deep
    // register-allocation cascade, not the coalescing issue this fix addressed.
    // RULED OUT this session: reordering the `rectCursor.{left,right,top,bottom}` field
    // assignments to mirror the original's own store order (right, left, bottom, top, per the
    // raw disasm at 0x41500e-0x415026) produced a BYTE-IDENTICAL recompile -- the optimizer
    // already reschedules these freely regardless of source statement order; don't re-try this.
    // The whole algorithm/every field was re-confirmed correct via a fresh, independent
    // instruction-by-instruction trace of the ENTIRE original function this session (not just
    // spot-checked) -- the remaining work is a pure codegen-matching problem, not an
    // understanding gap.
    // v196 (2026-07-18): a full byte-by-byte stack-slot reconciliation pass (per the prior
    // pickup) found a genuine CONTENT gap, not just codegen noise: the original unconditionally
    // zeroes `rectBltDest.left` right after the two early-return checks (real addr 0x4151ee,
    // `mov [esp+0x4c],eax` with eax=0) -- our transcription never set it, silently leaving
    // IntersectRect's raw intersection-left value there instead (wrong input to the non-union
    // branch's `nSrcLeftBase = rectBltDest.left`). Fixed; now matches the sibling
    // RedrawSoftwareCursorOverBoard, which already had this line (see its own comment below
    // -- that comment's claim that RedrawSoftwareCursor "derives rectBltDest.left from an
    // explicit IntersectRect" was itself imprecise/superseded by this finding, corrected).
    // Frame-size residual is UNCHANGED by the content fix (still 0x78 vs the original's 0x7c, one
    // dword short) -- asmscore total 496523, byte_diff 503/1166 (was 499 pre-fix; the extra store
    // shifts downstream register allocation slightly, expected and not a regression signal, kept
    // regardless since it's a correctness fix independent of score). A 3rd field-reorder variant
    // (left,right,bottom,top, matching the original's store TIMING at 0x5e-0x76) was also tried
    // and is ALSO inert -- byte-identical. A full esp-relative slot map of the whole function
    // (built via a dedicated research pass this session) did not conclusively identify the extra
    // persistent slot the original's 0x7c frame needs; the one loose thread is a single
    // WRITE-with-no-confirmed-READ at esp+0x70 (real addr 0x4152e1, inside the bUnionModeMaybe
    // branch) -- a candidate for a future session, not yet confirmed real. Do not re-try the
    // field-reorder lever again (3 variants now ruled out); do not re-suggest the live-register-
    // dump lever (already ruled out for the CommitScreenUpdate family this belongs to).
    void RedrawSoftwareCursor(char param_1);

    // FUNCTION: LOCO 0x415440
    // Sibling of RedrawSoftwareCursor (v195, transcribed via raw-disasm trace -- Ghidra's
    // own decompile has the SAME confirmed confusion bug in the frame-offset/mask-blit block as
    // that sibling; do not trust its local_38/uStack_88/unaff_EBX narrative there). Same
    // background-restore + masked-composite cursor pipeline, but three concrete differences:
    // 1) Uses rectPrevCursorB (+0x78), not rectPrevCursor (+0x68), as its own
    //    "previous frame" rect -- confirms rectPrevCursorB really is a 2nd, independent
    //    consumer (this function), closing the "no confirmed 2nd reader yet" note that used to
    //    sit on that field.
    // 2) Clips against a LOCAL copy of a static/template screen rect (0x485220) instead of this
    //    window's own rectScreenBounds: CopyRect's the template, then OffsetRect's it by
    //    WorldBoardMaybe's own current scroll position (dwScrollX/dwScrollY, +0x1c/
    //    +0x20 -- newly split out of WorldBoardMaybe.h's own pad0x1c[8] this session, confirmed
    //    already-named in the live Ghidra DB) whenever a global flag (0x485210) is set.
    // 3) Restores/composites from a SHARED g_pDDrawWorkSurface (DAT_004fd3c4), not this
    //    window's own per-instance pOffscreenSurface -- this is the map/board's own cursor
    //    redraw (drawn over the shared board view), while RedrawSoftwareCursor is the
    //    per-popup-window variant. rectBltDest is always {0,0,width,height} here (no
    //    IntersectRect call exists in this function at all -- confirmed via a full-.text import
    //    scan for ordinal 0x47726c) whereas RedrawSoftwareCursor calls IntersectRect against
    //    its own bounds but then ALSO unconditionally zeroes rectBltDest.left right after (v196
    //    finding, see that method's own comment) -- so both siblings end up with rectBltDest.left
    //    always 0 in practice, just reached differently; this is a genuine algorithmic difference
    //    in HOW each computes it, not transcription noise -- do not force them to literally
    //    match. CONTENT-COMPLETE, NOT YET BYTE-MATCHED (own multi-session arc, own residual
    //    class expected -- same family as RedrawSoftwareCursor/WindowBase::
    //    CommitScreenUpdate/RedrawCustomCursor). First-compile asmscore (vs. the real Ghidra
    //    body length 1210): total 461194, byte_diff 434/1210, insns 383/394 -- comparable
    //    residual ratio to RedrawSoftwareCursor's own (499/1166); dump shows only register/
    //    slot-choice ('r'/'S') noise, no structural bugs, on the very first transcription
    //    attempt -- a strong signal the content model above is correct.
    void RedrawSoftwareCursorOverBoard(char param_1);

    // FUNCTION: LOCO 0x452170
    // Measures the current font's one-line pixel height: draws a single-char DrawTextA("W",
    // DT_CALCRECT) probe into a fixed placeholder box (SetRect(0,0,0xd9,0x96) offset to
    // (0x2a,0x23)) on this window's own offscreen surface via
    // AcquireOffscreenSurfaceDC/CommitScreenUpdate, returning DrawTextA's height result. Sole caller
    // (TutorialWnd::Launch) uses only the return value, stashed as nTextLineHeight (a
    // row-snap modulus divisor) -- transcribed 2026-07-17, EXACT match.
    int MeasureTextLineHeight();

    // 0x413d90 -- repositions the popup at (x, y): stores x/y at field_0x18/field_0x1c, derives
    // field_0x20/field_0x24 (= a cached width/height at field_0x10c/field_0x110, plus x/y --
    // i.e. the popup's own right/bottom), then SetWindowPos(hwndSelf, NULL, x, y, right-x,
    // bottom-y, SWP_NOZORDER|SWP_NOACTIVATE). Sole confirmed caller: TutorialWnd::Launch.
    // Not yet transcribed -- declared only.
    void Move(int x, int y); // 0x413d90

    // FUNCTION: LOCO 0x414ef0
    // Rebinds the ddraw window clipper onto whichever front-end singleton screen is currently
    // shown (its own WindowBase::bModalCaptureActive != 0), checked in fixed priority order MailWnd /
    // AlbumCardWnd / EditCardWnd / MapWnd / SplashWnd, falling back to the main app window
    // (g_pApp->hwndOwner) if none are shown. An ORDINARY (non-static) member that simply never
    // reads its own `this` -- every one of its 10 call sites is preceded by `mov ecx,esi`, which
    // is the `this` pass, not dead code. It was modelled `static` through v361 on the inverted
    // reading of that same evidence ("the callee ignores `this`, so the ecx setup must be dead");
    // a callee that ignores `this` cannot be told apart by its OWN bytes -- __thiscall/0-args and
    // __cdecl/0-args both end in a bare `ret` -- so only the call sites can decide, and they are
    // unanimous. Restoring the member-ness supplies the `mov ecx` in all six PopupWndBase callers
    // that were short one instruction.
    void PopupWndBase_RebindClipperToActiveScreen();

    // FUNCTION: LOCO 0x414290 -- toggles the popup's OS-level input capture and cursor
    // visibility. bRelease!=0 (release request): no-op if bSuppressCursorRedraw already set; else sets
    // bSuppressCursorRedraw=1, ReleaseCapture() if this window currently holds it, rebinds the ddraw window
    // clipper, redraws the software cursor (+ the over-board variant too if Unk0x88 is set),
    // rebinds the clipper to the active screen, and disables PlacementCursorMaybe's own
    // cursor-capture flag. bRelease==0 (default/acquire path, called from Show): if bSuppressCursorRedraw
    // is set OR this window doesn't already hold capture, sets bSuppressCursorRedraw=0, SetCapture(hwndSelf),
    // and spins ShowCursor(0) until the OS cursor-display counter goes negative (fully hides the
    // OS cursor). NOTE: Show calls this with bRelease=0 (which sets bSuppressCursorRedraw=0) then
    // immediately overwrites bSuppressCursorRedraw=1 itself right after -- bSuppressCursorRedraw's exact semantics beyond
    // "capture toggle state used by this function" aren't fully pinned down yet; don't assume a
    // simple 0/1="released" reading holds project-wide. Called directly by RouteMessageMaybe
    // below (3 sites), Show, Hide, CreditsWnd::OnExit, and TutorialWnd::OnExit -- NOT
    // by BuildToolCursorWnd::OnExit (checked, refuted v197). Transcribed 2026-07-18 (v199) -- see
    // src/PopupWndBase.cpp for compile status.
    void SetModalCapture(char bRelease);

};

// A probe view of PopupWndBase's vtable for PopupWndBase_RouteMessage's pWnd dispatch, kept in
// the padded-dummy-virtuals probe idiom (a pWnd cast, not a `this` cast -- outside lint_idiom's
// class F). ⚠ As of v544 EVERY slot this declares is also a real `virtual` on PopupWndBase
// itself, so as a TYPE this is now pure redundancy -- it survives ONLY as a measured byte lever:
// deleting it and retargeting RouteMessage's 17 dispatches to `pWnd->` leaves this TU
// byte-identical but costs src/WorldBoardMaybe.cpp 211 bytes (see the class's own slot-0x4-0x90
// block comment). Historically it was the class's ONLY model of the slots above 0x4c, which is
// why its names are the ones the class adopted: they were ground-truthed from
// PopupWndBase_RouteMessage's own switch long before the class could spell them.
// NOTE: slot numbers do
// NOT correspond to WindowBase's own same-numbered slots -- two unrelated class hierarchies
// with independently laid out dispatch tables (confirmed mismatched at slot 0x58 -- see
// OnMouseActivate's own comment above; the two tables nonetheless agree on the underlying
// handler ADDRESS at every shared slot, being parallel with a one-slot shift). Slots 0x1c/0x20 are confirmed via the shared
// stub bodies' own `ret` cleanup (0x426130 = `ret 0x4`, one stack arg; 0x4661a0 = bare `ret`,
// zero args), matching OnPaint's own PAINTSTRUCT*-then-no-args call pair. CONSOLIDATED
// 2026-07-18: this struct used to be duplicated (a divergent 4-slot subset in
// src/BuildToolCursorWnd.cpp, only declaring the 2 slots that file itself calls) -- merged into
// one canonical shared definition here per CLAUDE.md's "never duplicate a struct definition
// across TUs" rule, same fix pattern as UIResourcesPartial's own 2026-07-17 consolidation.
struct PopupWndBaseVtblProbe {
    virtual void *_v00(); // dtor
    virtual void *_v04(); // Hide base default -- genuinely overridden per subclass (e.g.
                            // BuildToolCursorWnd's own is FUN_00436f70, still un-named)
    virtual void *_v08(); // Show
    virtual void SetCursorDesc(int, CursorDesc *, char, char); // slot 0xc -- confirmed
                            // CONSISTENT across all 3 derived singletons (0x414340)
    virtual void *_v10(); // shared no-op (WindowBase::NoOpVirtualMaybe, takes 1 stack arg)
    virtual void *_v14(); // Create
    virtual void RefreshClientRect(); // slot 0x18
    virtual void OnDrawContent(PAINTSTRUCT *pPs); // slot 0x1c -- per-class "draw content"
                                                         // hook; base default = shared no-op
    virtual void OnDrawCursorOverlay();            // slot 0x20 -- per-class "draw cursor
                                                         // overlay" hook; base default = shared
                                                         // bare-ret stub
    virtual void *_v24(); // RouteMessageMaybe's own slot -- never called recursively from here
    virtual LRESULT OnUnhandledMessage(HWND, UINT, WPARAM, LPARAM); // slot 0x28 -- default fallback
    virtual LRESULT OnTimerDefault(HWND, UINT, WPARAM, LPARAM);    // slot 0x2c -- WM_TIMER passthrough
    virtual LRESULT OnCreate(HWND, UINT, WPARAM, LPARAM);          // slot 0x30 -- WM_CREATE
    virtual LRESULT OnLButtonDown(HWND, UINT, WPARAM, LPARAM);     // slot 0x34 -- WM_LBUTTONDOWN
    virtual LRESULT OnLButtonUp(HWND, UINT, WPARAM, LPARAM);       // slot 0x38 -- WM_LBUTTONUP
    virtual LRESULT OnRButtonDown(HWND, UINT, WPARAM, LPARAM);     // slot 0x3c -- WM_RBUTTONDOWN
    virtual LRESULT OnRButtonUp(HWND, UINT, WPARAM, LPARAM);       // slot 0x40 -- WM_RBUTTONUP
    virtual LRESULT OnLButtonDblClk(HWND, UINT, WPARAM, LPARAM);   // slot 0x44 -- WM_LBUTTONDBLCLK
    virtual LRESULT OnRButtonDblClk(HWND, UINT, WPARAM, LPARAM);   // slot 0x48 -- WM_RBUTTONDBLCLK
    virtual LRESULT OnMouseMove(HWND, UINT, WPARAM, LPARAM);  // slot 0x4c -- WM_MOUSEMOVE
    virtual LRESULT OnKeyDown(HWND, UINT, WPARAM, LPARAM);         // slot 0x50 -- WM_KEYDOWN
    virtual LRESULT OnKeyUp(HWND, UINT, WPARAM, LPARAM);           // slot 0x54 -- WM_KEYUP
    virtual LRESULT OnMouseActivate(HWND, UINT, WPARAM, LPARAM); // slot 0x58 -- WM_MOUSEACTIVATE
    virtual LRESULT OnSetFocus(HWND, UINT, WPARAM, LPARAM);        // slot 0x5c -- WM_SETFOCUS
    virtual LRESULT OnKillFocus(HWND, UINT, WPARAM, LPARAM);       // slot 0x60 -- WM_KILLFOCUS
    virtual LRESULT OnSize(HWND, UINT, WPARAM, LPARAM);       // slot 0x64 -- WM_SIZE
    virtual LRESULT OnPaint(HWND, UINT, WPARAM, LPARAM);      // slot 0x68 -- WM_PAINT
    virtual LRESULT OnSetCursor(HWND, UINT, WPARAM, LPARAM);  // slot 0x6c -- WM_SETCURSOR
    virtual LRESULT OnShowWindow(HWND, UINT, WPARAM, LPARAM);      // slot 0x70 -- WM_SHOWWINDOW
    virtual LRESULT OnEraseBkgnd(HWND, UINT, WPARAM, LPARAM); // slot 0x74 -- WM_ERASEBKGND
    virtual LRESULT OnDestroy(HWND, UINT, WPARAM, LPARAM);    // slot 0x78 -- WM_DESTROY
    virtual LRESULT OnClose(HWND, UINT, WPARAM, LPARAM);      // slot 0x7c -- WM_CLOSE
    virtual LRESULT OnNotify(HWND, UINT, WPARAM, LPARAM);          // slot 0x80 -- WM_NOTIFY
    virtual LRESULT OnCommand(HWND, UINT, WPARAM, LPARAM);         // slot 0x84 -- WM_COMMAND
    virtual LRESULT OnHotKey(HWND, UINT, WPARAM, LPARAM);          // slot 0x88 -- WM_HOTKEY
    virtual LRESULT OnActivateApp(HWND, UINT, WPARAM, LPARAM);     // slot 0x8c -- WM_ACTIVATEAPP
    virtual LRESULT OnWindowPosChanging(HWND, UINT, WPARAM, LPARAM); // slot 0x90 -- WM_WINDOWPOSCHANGING
};

// FUNCTION: LOCO 0x415900 (Ghidra: PopupWndBase::WndProc) -- recovers `this` from GWL_USERDATA
// exactly like WindowBase_WndProc, then forwards to vtable slot 0x24 (RouteMessageMaybe).
LRESULT CALLBACK PopupWndBase_WndProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

// FUNCTION: LOCO 0x4143e0 // TODO: sync (Ghidra: PopupWndBase::RouteMessageMaybe) -- a plain __stdcall free
// function taking the state pointer as an explicit first parameter, NOT a real __thiscall
// method (confirmed via raw disasm: every call site pushes pWnd as a 5th explicit stack arg,
// ret 0x14 = 5 dwords popped, no this-in-ECX idiom) -- exactly mirroring WindowBase::
// RouteMessage's own signature. A this-typing attempt on this function in Ghidra was a
// documented false start (corrupted the decompile) and was reverted; see docs/subsystems.md's
// "PopupWndBase message dispatch" table for the full WM_*-to-slot mapping this dispatches
// through, ground-truthed directly from this function's own real branches. CONTENT-COMPLETE,
// NOT YET BYTE-MATCHED (2026-07-18): every case's behavior confirmed 1:1 against raw disasm,
// including a genuine reproduced engine bug in the WM_MOUSEMOVE case (see the .cpp definition's
// own `sic:` comment -- an uninitialized POINT read when the message arrives via the owner
// window rather than hwndSelf directly). Case order was reordered THIS session from ascending
// WM_* numeric order to the TRUE ground-truthed physical/declaration order (read directly from
// the compiled jump/pointer tables in loco/Loco.exe -- see the definition's own comment), which
// is real, necessary progress, but the residual (DIFF ~1007/1716) did not close as much as
// expected from that fix alone. The note used to blame the remainder on the "dead mov ecx,esi
// before the static PopupWndBase_RebindClipperToActiveScreen() call" class (present once, in the
// WM_TIMER case) cascading into short-vs-near jump ENCODING choices at nearby branch sites. That
// premise was WRONG and is now FIXED (v362, lever 3): the ecx setup was that member's `this`
// pass, the call site is now `pWnd->PopupWndBase_RebindClipperToActiveScreen()`, and the
// 2-byte shrinkage is gone.
//
// v364 re-measured against the TRUE COMDAT extent and found the real residual class. ⚠ Score
// this function with `--len 0x6a0` (= 0x414a80 - 0x4143e0, the NEXT function's start), or
// `--len 0x64c` for code-only: the `Body:` span stops at the `ret 0x14` (0x414a2b) and excludes
// the trailing jump table AND the byte index table at 0x414a2c-0x414a80. v363's "insns 666/670,
// FOUR extra instructions" came from the no-`--len` default window and is not a real reading.
//
// TWO fixes landed (DIFF 998 -> 986, len 1720 -> 1708, byte_diff 165 -> 164):
//   1. WM_CAPTURECHANGED was a flat early-out chain; the original NESTS it, so the
//      `nCursorDescKey == 0` path and the `SetModalCapture(0)` path share ONE trailing
//      `return 0` (`je 0x4149ce` + fall-through) while `SetModalCapture(1)` keeps its own
//      epilogue copy. Restructuring to the nested shape removed a duplicated 6-instruction
//      epilogue -- exactly 12 bytes.
//   2. The WM_TIMER frame-count guard is `<= 1`, NOT `< 2`: the original emits
//      `cmp WORD PTR [edx+0x160],1 / jbe`, and `< 2` compiles to `cmp ...,2 / jae`. Same
//      predicate, different constant, different bytes. VC5 does NOT canonicalize these.
//
// TWO hypotheses REFUTED, do not retry:
//   - Swapping the WM_MOUSEMOVE if/else arms to match the original's emitted arm order
//     (`if (Unk0x88 != 0 || hWndUnder == hwndSelf)` first) is a NO-OP: verified byte-identical
//     COMDATs before/after. VC5 canonicalizes `if (A==0 && B!=C) X; Y` and
//     `if (A!=0 || B==C) Y; X` to the same code, so the dump's apparent arm swap at ~0x484 is
//     an ALIGNER artifact from ~22 bytes of accumulated upstream drift, not a source defect.
//   - Applying fix 1's nested-if shape to WM_TIMER's own early-out makes it WORSE
//     (DIFF 986 -> 999, len 1708 -> 1712).
//
// REMAINING: one duplicated 6-instruction `return 0` epilogue at ~0x39a (WM_TIMER's early-out;
// the original tail-merges it into the function-wide shared epilogue at 0x4149ce) plus
// register/scheduling noise around the WM_MOUSEMOVE POINT build (~0x459-0x477). Own
// multi-session arc; do not re-derive the case order or the WM_MOUSEMOVE algorithm from scratch.
LRESULT __stdcall PopupWndBase_RouteMessage(PopupWndBase *pWnd, HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
