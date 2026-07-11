// WindowBase -- the root of the "real Win32 HWND" window family (see docs/subsystems.md's
// "Window system" section; NOT the same hierarchy as PopupWndBase, the DirectDraw-composited
// overlay base). Ctor 0x425870, sizeof 0xe8 (232 bytes).
//
// Field layout mirrors the Ghidra DB directly (built across earlier sessions from
// WindowBase::Create/InitCursorDescriptors/ScheduleModeTransition -- see
// docs/subsystems.md's "Custom cursor system" section). No explicit vtable field -- the
// compiler synthesizes the pointer at +0x0 automatically once a virtual function exists (see
// LocoBitmap.h's same precedent). `Unk0xNN`-named fields match Ghidra's own placeholder names
// verbatim (size known, purpose not); untouched byte ranges Ghidra still shows as raw
// undefined bytes are left as plain `pad0xNN` blocks.
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "CursorDesc.h"
#include "LocoBitmap.h"

class WindowBase {
public:
    void *hInstance;
    HWND hwndSelf;
    HWND hwndOwner;
    unsigned int resourceId;
    LocoBitmap *pCursorBitmap;  // Unk0x14 -- the active cursor's own realized bitmap/rect
                                      // object (ScheduleModeTransition's pTargetRectMaybe);
                                      // non-virtual RestoreOverlapBlt is called directly on it
                                      // by RedrawCustomCursor's redraw path.
    int nCursorWidth;  // Unk0x18 -- ScheduleModeTransition: pTargetRectMaybe->+8
    int nCursorHeight; // Unk0x1c -- ScheduleModeTransition: pTargetRectMaybe->+0xc
    int nCursorFrameCount; // Unk0x20 -- doubles as ScheduleModeTransition's
                                 // scaleDivisorMaybe AND (in RedrawCustomCursor) the number of
                                 // equal-width frames packed side by side in pCursorSurface
                                 // (an animated cursor strip, e.g. anipoint); <2 = static/no strip.
    int nCursorFrameIndex; // Unk0x24 -- current strip frame, wraps to 0 at
                                 // nCursorFrameCount, multiplied by nCursorWidth for
                                 // the strip source-rect offset.
    unsigned int hCursorTimer; // Unk0x28 -- SetTimer/KillTimer id 0x43 (cursor redraw pump)
    int nCursorHotspotX; // Unk0x2c -- hotspot X subtracted from GetCursorPos in RedrawCustomCursor
    int nCursorHotspotY; // Unk0x30 -- hotspot Y subtracted from GetCursorPos in RedrawCustomCursor
    int nLastCursorScreenX; // Unk0x34 -- last raw GetCursorPos().x, cached by CommitScreenUpdate
                                  // and WindowBase_RouteMessage's WM_TIMER case to detect mouse
                                  // movement; reset to -1 by RedrawCustomCursor each redraw to
                                  // force re-detection next tick.
    int nLastCursorScreenY; // Unk0x38 -- paired Y half of nLastCursorScreenX.
    bool bSuppressCursorRedraw; // Unk0x3c -- gates RedrawCustomCursor's actual draw/erase work and
                                      // the Ddraw_RebindWindowClipper() bracket around it in
                                      // ScheduleModeTransition; BeginModalCapture clears
                                      // it to 0 (allow) on entry.
    bool bCursorAnimStopped; // Unk0x3d -- gates the WM_TIMER cursor-animation branch; set once
                                   // the paired repeat-counter Unk0x40 counts down to 0 (a "play
                                   // N more animation cycles then stop" latch).
    unsigned char pad0x3e[2];
    unsigned int Unk0x40; // repeat-counter paired with bCursorAnimStopped; own origin/semantics
                                 // not yet traced
    bool bCursorRedrawArmed; // Unk0x44 -- gates RedrawCustomCursor's entire body; set by a
                                   // WndProc-dispatched message (vtable slot 0x6c, WM_* id not
                                   // yet pinned), cleared by EndModalCapture.
    unsigned char pad0x45[3];
    IDirectDrawSurface *pCursorSurface; // Unk0x48 -- cached copy of g_pCursorSurface
                                              // (set once by InitCursorDescriptors); also
                                              // doubles as the "cursor system inited" flag for
                                              // this window (refcounts DAT_004fd3cc/DAT_004fd3d0,
                                              // see WindowBase_DtorMaybe) and as the per-window
                                              // scratch surface RedrawCustomCursor composites the
                                              // cursor image into before blitting to the primary
                                              // surface.
    HDC hdcWorkSurface; // Unk0x4c -- the DC AcquireWorkSurfaceDC caches from
                              // g_pDDrawWorkSurface->GetDC() and returns
    RECT rectLastCursorDraw; // cursor-erase rect, saved by RedrawCustomCursor (cast directly
                                   // as RECT* there -- a real RECT, not 4 loose fields)
    void *pPointCursorRect;
    CursorDesc *pPointCursorDesc;
    void *pAnipointCursorRect;
    CursorDesc *pAnipointCursorDesc;
    unsigned int pEraserCursorRect;
    CursorDesc *pEraserCursorDesc;
    char className[50];
    unsigned char pad0xaa;
    bool bCreated;
    int x;
    int y;
    int width;
    int height;
    int nClipWidth;  // = rectClipBounds.right - .left, see RefreshClientClipRect
    int nClipHeight; // = rectClipBounds.bottom - .top, see RefreshClientClipRect
    RECT rectClient;     // raw GetClientRect() result, written by RefreshClientClipRect
    RECT rectClipBounds; // saved snapshot of rectClient; clamp bounds consumed by
                               // RedrawCustomCursor's custom-cursor redraw
    unsigned char bModalCaptureActive; // Unk0xe4 -- BeginModalCapture sets it 1, EndModalCapture
                                              // and the dtor clear it 0
    unsigned char pad0xe5[3];

    // 0x425870 -- zero-initializes every scalar/pointer field, stashes hInstanceParam/
    // resourceIdParam, and loads the window's own title string into className via the shared
    // locale-string helper (see src/WindowBase.cpp's own UIResourcesPartial). Not yet
    // byte-matched -- see src/WindowBase.cpp.
    WindowBase(void *hInstanceParam, unsigned int resourceIdParam);

    // 0x425910 (Ghidra: WindowBase::WindowBase_DtorMaybe) -- see src/WindowBase.cpp.
    virtual ~WindowBase();

    // The vtable slot layout below is FULLY ground-truthed against WindowBase_Vtbl @
    // 0x477c30 (dumped 2026-07-22, v322+): slot 0 = scalar deleting dtor (above), slot 4 =
    // EndActiveSession, slot 8 = BeginModalCapture, slot 0xc = RequestModeTransitionFromSource,
    // slot 0x10 = ScheduleModeTransition, slot 0x14 = NoOpVirtualMaybe, slot 0x18 = Create,
    // slot 0x1c = RefreshClientClipRect (declared further below, after RedrawCustomCursor),
    // slots 0x20-0x90 = the per-message handler block (declared right after
    // RefreshClientClipRect; WM_*-to-slot table in docs/subsystems.md's "Shared vtable slot
    // conventions"). Declaration order here IS the vtable order.

    // vtable slot 4 (0x425990, Ghidra: EndModalCapture -- renamed in src 2026-07-21 when the
    // slot was modeled as a real virtual: one C++ name must serve the base default and both
    // known overrides, AlbumCardWnd::EndActiveSession (0x402660, the name source) and
    // EditCardWnd's own (0x416f70, Ghidra: EndEdit); sync parked via // TODO: sync at the
    // renamed definitions until Ghidra is renamed to match). Ends the SetCapture/hidden-cursor
    // modal-input session BeginModalCapture starts: see src/WindowBase.cpp. Called directly
    // (base-qualified, bypassing dispatch) from several classes' OnCreateComplete/
    // OnFirstActivateMaybe and from EditCardWnd's own edit-session begin/end methods.
    // EXACT match (src/WindowBase.cpp).
    virtual void EndActiveSession();
    // vtable slot 8 (0x4259c0) -- the begin half of the modal-mouse-capture pair
    // (SetCapture/ShowCursor(0) + a cursor-redraw bracket via RedrawCustomCursor, gated by
    // Unk0xe4; see docs/subsystems.md's "Window system" section). Called directly (not
    // through the vtable) from several classes' OnCreateComplete/OnFirstActivateMaybe and from
    // EditCardWnd's own edit-session begin/end methods. EXACT match (src/WindowBase.cpp).
    virtual void BeginModalCapture();
    // vtable slot 0xc (0x425fd0, inherited unoverridden by AlbumCardWnd/EditCardWnd) -- arms a
    // cursor-mode transition from the given source rect/descriptor pair. EXACT match
    // (src/WindowBase.cpp). pCursorDesc is really a CursorDesc *: the body reads its
    // hotspotX/hotspotY shorts and nTotalFrameCount off it and forwards them to slot 0x10.
    virtual void RequestModeTransitionFromSource(void *pCursorRect, void *pCursorDesc, int bResetCounters, int bDoRebind);
    // vtable slot 0x10 (0x426020, inherited) -- schedules a cursor-mode transition onto a
    // target bitmap (pTargetBitmap doubles as the target rect, see pCursorBitmap above; it is
    // really a LocoBitmap *, whose width/height at +8/+0xc are what nCursorWidth/nCursorHeight
    // are derived from). EFFECTIVE MATCH, DIFF(2) (src/WindowBase.cpp).
    virtual void ScheduleModeTransition(void *pTargetBitmap, unsigned int nCursorFrameCount,
                                        void *pCursorHotspot, char bResetCounters, char bDoRebind);
    // vtable slot 0x14 (0x426130, installed at slot 0x20 too) -- a trivial no-op body (`ret 0x4`,
    // one stack arg; shared across both class hierarchies' vtables -- PopupWndBase installs the
    // same address at its own slots 0x10/0x1c). Never overridden in WindowBase's family;
    // declared only.
    // sic: takes one stack arg (see the `ret 0x4` above) that the shared body ignores entirely.
    // SplashWnd::StartGameNetThread's worker-thread-failed path is the one known call site, and
    // it passes a literal 0 and discards the result.
    // Return type is `void`, not the `void *` this used to say: the body is a bare `ret 0x4`
    // with no `mov eax`, and cl 11.00 rejects a value-returning function that falls off the end
    // outright (error C2561, not a warning), so the original cannot have declared a return type
    // here. Defined in src/WindowBase.cpp.
    virtual void NoOpVirtualMaybe(int nUnusedArg);
    // vtable slot 0x18 (0x425b70) -- RegisterClassA + CreateWindowExA, stores the new HWND at
    // +0x08 hwndSelf; the passed-in "owner" HWND is used ONLY to steal its window TEXT as the new
    // window's title (see docs/subsystems.md's "Window system" section). Real param count
    // confirmed via `ret 0x2c` (11 dwords) -- Ghidra's own auto-analysis only inferred 9.
    // `nClassStyle` (formerly guessed as a resource id) really overrides the registered
    // WNDCLASSA's own `style` field (defaults to 3/CS_HREDRAW|CS_VREDRAW when 0). sic: the
    // trailing `dwStyle`/`dwExStyle` pair is a genuine dead-parameter engine bug (see
    // docs/engine-bugs.md): the body hardcodes its own `CreateWindowExA` style (0x87000000)
    // instead of reading either one, despite every real caller passing a distinct, real value.
    // Real return type is `unsigned char` per this TU's byte-return/no-EAX-widen idiom (bare
    // `xor al,al`/`mov al,1`, no `CONCAT31`-shaped masking). Modeled as a REAL virtual
    // 2026-07-22 (ground-truthed in WindowBase_Vtbl @ 0x477c30); EditCardWnd/MailWnd
    // declare their own same-named NON-virtual Create(HWND) that hides this one (different
    // signature -- not an override). Definition in src/WindowBase.cpp (EFFECTIVE MATCH).
    virtual unsigned char Create(int nShowCmd, HWND hwndOwnerParam, int xParam, int yParam, int widthParam, int heightParam, HMENU hMenu, HICON hIcon, unsigned int nClassStyle, unsigned int dwStyle, unsigned int dwExStyle); // 0x425b70

    // 0x425dc0 (Ghidra: WindowBase::InitCursorDescriptorsMaybe) -- resolves the three shared
    // cursor descriptors (point 0x1400 / anipoint 0x1403 / eraser 0x1402) through
    // g_UIResources.TileKind_GetOrLoadDescriptor, caches each descriptor and its realized
    // GetOrLoadFrameBitmap(0, 0) bitmap (the *Rect fields are really LocoBitmap*, see
    // pCursorBitmap), seeds nCursorWidth/nCursorHeight/nCursorFrameCount from the POINT
    // descriptor's own nativeWidth/nativeHeight/nTotalFrameCount (the initial transition
    // state ScheduleModeTransition later recomputes), then lazily creates the shared
    // 256x256 system-memory cursor scratch surface g_pCursorSurface (refcounted via
    // g_dwCursorSurfaceRefCount, released by PopupWndBase's teardown) and caches it in
    // pCursorSurface. Definition in src/WindowBase.cpp.
    void InitCursorDescriptorsMaybe();

    // 0x426eb0 -- the custom-cursor redraw routine BeginModalCapture/
    // ScheduleModeTransition bracket in a Ddraw_RebindWindowClipper() pair. Erases the
    // previous frame's cursor rect (rectLastCursorDraw) from the primary surface by
    // blitting the corresponding region back from the work surface, then (if armed and not
    // suppressed) composites the current cursor frame onto pCursorSurface (a per-window
    // scratch surface) and blits that up to the primary surface. bFullRedraw selects
    // whether the erase/composite work happens at all (BeginModalCapture always passes 1;
    // ScheduleModeTransition passes the inverse of its own bResetCountersMaybe param). If
    // the old and new cursor rects are close together (their union fits in a 256x256 box), a
    // single combined erase+draw blit is used instead of two separate ones.
    void RedrawCustomCursor(char bFullRedraw);

    // 0x425d30 (named v137) -- vtable slot 0x1c default body: refreshes
    // rectClient/width/height from a fresh GetClientRect, snapshots into
    // rectClipBounds, derives nClipWidth/nClipHeight. Every WindowBase-derived
    // singleton but EditCardWnd inherits this unoverridden; EditCardWnd's own override
    // (EditCardWnd::RefreshClientClipRect, src/EditCardWnd.cpp) calls this base version
    // first via a base-qualified (dispatch-bypassing) call. EXACT match (src/WindowBase.cpp).
    // rectClipBounds is seeded by a whole-struct assignment from rectClient, not four field
    // stores -- that is what produces the original's inline 16-byte copy through a pointer.
    virtual void RefreshClientClipRect();

    // ==== vtable slots 0x20-0x90: the per-message handler block ====
    // Ground-truthed against WindowBase_Vtbl @ 0x477c30 (2026-07-22); the WM_*-to-slot table is
    // in docs/subsystems.md's "Shared vtable slot conventions". Every slot from 0x2c on is a
    // real thiscall virtual taking (HWND, UINT, WPARAM, LPARAM) -> LRESULT. Slots whose default
    // body is DefWindowProcStub (0x422ea0 -- a shared `{ DefWindowProcA(...); }` __stdcall free
    // function, not a member) are declared only here, as are the untranscribed FUN_004269xx
    // defaults; the four slots with real named WindowBase member bodies (0x5c/0x70/0x78/0x7c)
    // have their definitions in src/WindowBase.cpp (all EXACT).
    // slot 0x20 -- the "window just became active, redraw yourself" hook. WindowBase's own
    // default is the SAME NoOpVirtualMaybe body (0x426130) installed a second time -- which is
    // what pins the signature: `ret 0x4`, i.e. exactly one stack arg, ignored by the default.
    // The name and the `int` parameter come from the two known overrides, AlbumCardWnd::OnActivate
    // (0x404db0, the name source -- src/AlbumCardWnd.cpp) and ApplSetupWnd::OnActivate (0x409280,
    // declared only -- src/ApplSetupWnd.h); both redraw their whole client area and neither reads
    // the argument. Declared only here. (Was a `void *_v20()` dummy slot-holder until 2026-07-25,
    // which forced ApplSetupWnd's override to be reached through a hand-rolled probe wrapper.)
    virtual void OnActivate(int reservedMaybe);
    // slot 0x24 -- WindowBase's own default is the shared bare-`ret` no-op 0x4661a0, i.e. "no idle
    // work to do". SplashWnd DOES override it (0x421eb0, see src/SplashWnd.cpp), which is what
    // pinned the name and the void-return/no-arg signature; the old `_v24` dummy claimed the slot
    // was never overridden. Declared only.
    virtual void OnIdlePump();
    virtual void *_v28(); // slot 0x28 -- WindowBase_RouteMessage (0x426140): NOT a real thiscall
                          // virtual (a bare __stdcall free fn stored in the array, pWnd an
                          // explicit 5th stack arg; see WindowBaseVtableView in
                          // src/WindowBase.cpp). Kept as a dummy slot-holder -- do NOT declare
                          // RouteMessage as a member.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x2c -- fallback default. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);     // slot 0x30 -- WM_TIMER passthrough. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnCreate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);     // slot 0x34 -- WM_CREATE. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x38 -- WM_LBUTTONDOWN. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only. Overridden by AlbumCardWnd (0x404f60) and EditCardWnd (0x41ac10).
    virtual LRESULT OnLButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // slot 0x3c -- WM_LBUTTONUP. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x40 -- WM_RBUTTONDOWN. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only. Overridden by AlbumCardWnd (0x4055e0).
    virtual LRESULT OnRButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // slot 0x44 -- WM_RBUTTONUP. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnLButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x48 -- WM_LBUTTONDBLCLK. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnRButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x4c -- WM_RBUTTONDBLCLK. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // slot 0x50 -- WM_MOUSEMOVE. Default: FUN_00426900 (untranscribed) -- declared only. Overridden by AlbumCardWnd (0x405680). (Ghidra renamed to match 2026-07-22, v323)
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);    // slot 0x54 -- WM_KEYDOWN. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only. Overridden by AlbumCardWnd (0x402690), ApplSetupWnd (0x40ae20) and EditCardWnd (0x417040).
    virtual LRESULT OnKeyUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);      // slot 0x58 -- WM_KEYUP. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnMouseActivate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x5c -- WM_MOUSEACTIVATE. WindowBase's own body 0x426950 (bare return 0; Ghidra: OnMouseActivateNoOp -- renamed to match the probe's slot name; Ghidra synced 2026-07-22, v323). Definition in src/WindowBase.cpp, EXACT.
    virtual LRESULT OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);   // slot 0x60 -- WM_SETFOCUS. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // slot 0x64 -- WM_KILLFOCUS. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnSize(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);       // slot 0x68 -- WM_SIZE. Default: FUN_00426960 (untranscribed) -- declared only. (Ghidra renamed OnSize 2026-07-22, v323)
    virtual LRESULT OnPaint(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);      // slot 0x6c -- WM_PAINT. WindowBase's own body 0x426980. Definition in src/WindowBase.cpp, EXACT. (Ghidra renamed OnPaint 2026-07-22, v323)
    virtual LRESULT OnSetCursor(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // slot 0x70 -- WM_SETCURSOR. WindowBase's own body 0x426a60. Definition in src/WindowBase.cpp, EXACT.
    virtual LRESULT OnShowWindow(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x74 -- WM_SHOWWINDOW. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnEraseBkgnd(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x78 -- WM_ERASEBKGND. WindowBase's own body 0x426ac0 (bare return 1; Ghidra: OnEraseBkgndNoOp -- renamed to match the probe's slot name; Ghidra synced 2026-07-22, v323). Definition in src/WindowBase.cpp, EXACT.
    virtual LRESULT OnDestroy(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);    // slot 0x7c -- WM_DESTROY. WindowBase's own body 0x426ad0. Definition in src/WindowBase.cpp, EXACT.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);      // slot 0x80 -- WM_CLOSE. Default: FUN_00426a90 (untranscribed) -- declared only. (Ghidra renamed OnClose 2026-07-22, v323)
    virtual LRESULT OnNotify(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);     // slot 0x84 -- WM_NOTIFY. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnCommand(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);    // slot 0x88 -- WM_COMMAND. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnHotKey(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);     // slot 0x8c -- WM_HOTKEY. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.
    virtual LRESULT OnActivateApp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // slot 0x90 -- WM_ACTIVATEAPP. Default: DefWindowProcStub 0x422ea0, a shared free function -- declared only.

    // 0x426b90 -- a shared "commit a screen update" helper, cursor-aware like
    // RedrawCustomCursor (reads/writes the same cursor-strip fields above, including
    // Unk0x34/Unk0x38 -- confirmed here as the last RAW screen-cursor X/Y, the same fields
    // WindowBase_RouteMessage's WM_TIMER case compares against to detect mouse movement;
    // this function is a second writer of them). If bSkip is nonzero, only rebinds the
    // ddraw window clipper (onto g_pApp->hwndOwner) and returns (used by callers that
    // already did their own inline invalidate, e.g. AlbumCardWnd::RedrawAllSlots releasing its
    // own DC). Otherwise: optionally releases hdcToRelease on g_pDDrawWorkSurface,
    // rebinds the clipper onto hwndTarget, then either a plain
    // Ddraw_BltUpdateRect(pUpdateRect ?: &rectClipBounds, hwndSelf) -- if no active/
    // unsuppressed custom cursor -- or the same cursor-rect clamp-against-rectClipBounds
    // algorithm as RedrawCustomCursor, merged with pUpdateRect via IntersectRect/
    // UnionRect (bailing to a plain Ddraw_BltUpdateRect if neither intersects at all), then a
    // LocoBitmap::RestoreOverlapBlt composite and a final dirty-rect Ddraw_BltUpdateRect plus a
    // direct Blt of the cursor scratch back onto g_pDDrawWorkSurface (not the primary
    // surface, unlike RedrawCustomCursor's own final Blt). See src/WindowBase.cpp for
    // match status. Called from AlbumCardWnd's FUN_00405520/RedrawAllSlots and many of its
    // per-message handlers (always with pUpdateRect==NULL currently).
    void CommitScreenUpdate(HWND hwndTarget, HDC hdcToRelease, char bSkip, RECT *pUpdateRect); // 0x426b90

    // 0x426b70 -- convenience wrapper: CommitScreenUpdate(hwndSelf, NULL, false, &rect). ~30
    // call sites across many windows, each passing a different dirty rect. Ghidra originally
    // mis-showed this as a 0-arg fastcall call at every site because the by-value RECT
    // stack-copy idiom (sub esp,0x10 + field stores, no push) was not recognized as a formal
    // parameter -- fixed via set_function_prototype (see CLAUDE.md's "by-value struct argument"
    // lesson). See src/WindowBase.cpp.
    void CommitRectUpdate(RECT rect); // 0x426b70

    // Acquires an HDC on the primary work surface via a GetDC-shaped vtable slot 0x44 retry
    // loop (up to 1000x, Sleep(10) between attempts; fatal FUN_00463600(0x48)/ExitProcess(1) if
    // it never succeeds), guarded by a Ddraw_RebindWindowClipper(hwndTarget) bracket -- the
    // acquired HDC is stashed at hdcWorkSurface and returned. EXACT match, src/WindowBase.cpp.
    // Called from AlbumCardWnd::RedrawAllSlots to get an HDC for its per-slot name DrawTextA loop.
    HDC AcquireWorkSurfaceDC(HWND hwndTarget); // 0x426b00

    // 0x425f20 -- a capture/system-cursor toggle called on OTHER windows' own base (not
    // necessarily self), e.g. by TutorialWnd::Launch/OnExit when handing input focus
    // between the launching popup and the underlying gameplay window. bRelease==0: grabs
    // capture (SetCapture(hwndSelf)), hides the system cursor (ShowCursor(0) looped to <0),
    // rebinds the ddraw clipper onto hwndSelf, RedrawCustomCursor(1), then rebinds the
    // clipper onto g_pApp's own window. bRelease!=0: the inverse (ReleaseCapture,
    // ShowCursor(1) looped to >=0), same clipper-rebind/redraw bracket -- which is emitted
    // separately in each arm, not shared. EXACT match (src/WindowBase.cpp).
    void SetCaptureMode(char bRelease); // 0x425f20
};

// WndProc/RouteMessageMaybe/DefWindowProcStub are NOT WindowBase methods -- Ghidra
// namespaces them under WindowBase (WindowBase::WndProc etc) purely for grouping; none
// read/write an implicit `this`, and RouteMessageMaybe/DefWindowProcStub's real ABI isn't
// even thiscall (see docs/subsystems.md's "Shared vtable slot conventions"). C++ symbol names
// flatten the Ghidra "Namespace::Name" via lint_ghidra_sync's "::"->"_" accepted variant.

// 0x425a50 -- centers `rect` within `outer`, preserving rect's own width/height (rewrites
// rect's left/top/right/bottom in place; `outer` is read-only). A plain rect utility, not a
// WindowBase method -- same grouping-only namespace as the WndProc trio above. Canonical
// home; src/LocoBitmap.cpp, src/MapWnd.cpp and src/WidgetPicker.cpp still carry their own
// file-local copies of this decl (pre-existing class-I idiom debt, tracked there).
void CenterRectInRect(RECT *outer, RECT *rect);

// 0x4272f0 -- the shared WNDCLASSA.lpfnWndProc every WindowBase-derived singleton window
// installs (see WindowBase::Create above). Recovers `this` from GWL_USERDATA; on WM_CREATE with
// a non-null lParam, seeds it from CREATESTRUCTA::lpCreateParams (the pointer Create passed as
// CreateWindowExA's lpParam) and stores it back via SetWindowLongA. If this window doesn't have
// its own object yet and the message isn't WM_CREATE, falls back to reading the PARENT window's
// own GWL_USERDATA (a child control sharing its owner's WindowBase). Falls back to
// DefWindowProcA if no object can be recovered at all; otherwise forwards to the recovered
// object's own RouteMessageMaybe (vtable slot 0x28).
LRESULT CALLBACK WindowBase_WndProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x4272f0

// 0x426140 -- WindowBase's own default body at vtable slot 0x28 (see docs/subsystems.md's
// "Shared vtable slot conventions" for the full WM_*-to-slot table this routes every message
// through). NOT a real virtual despite living in the vtable array: confirmed __stdcall with
// pWnd as a plain explicit stack argument (ret 0x14, no ECX load anywhere), unlike the real
// thiscall virtuals at the per-message slots it dispatches to. Never overridden by any
// currently-known derived class. EFFECTIVE MATCH -- see src/WindowBase.cpp (1905 bytes of real
// code, exact; a 3-byte alignment-NOP tie-break before the trailing jump-table data is parked).
LRESULT __stdcall WindowBase_RouteMessage(WindowBase *pWnd, HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x426140

// 0x422ea0 -- the shared "no override, just forward to DefWindowProcA" stub installed at nearly
// every unoverridden per-message vtable slot (+0x2c..+0x4c, +0x54, +0x58, +0x60, +0x64, +0x74,
// +0x84..+0x90 in WindowBase's own vtable). EXACT match, src/WindowBase.cpp.
// Returns LRESULT, not void: every overriding slot that falls through to this stub does so as
// `return WindowBase_DefWindowProcStub(...)`, passing DefWindowProcA's own result straight back
// out in EAX (the original's tail is a bare `call 0x422ea0` + `ret`, with no EAX rewrite).
LRESULT __stdcall WindowBase_DefWindowProcStub(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x422ea0

// A minimal partial view for the shared CreateFullscreenPopupWnd helper (0x402520) -- not a real
// standalone class. Both real callers (MapWnd, AlbumCardWnd) place an HICON at +0xe8, immediately
// after WindowBase's own sizeof (0xe8) -- confirmed via AppWindow_ConstructSingletonWindows
// (0x406f90) calling this same function with g_pMapWnd and g_pAlbumCardWnd in turn, each right
// after its own ctor. AlbumCardWnd's own real field at this offset is named hIcon too
// (src/AlbumCardWnd.h). Lives here (not WindowBase.cpp-local) so Bootstrap.cpp can reach it.
struct FullscreenPopupWndPartial : public WindowBase {
    HICON hIcon; // +0xe8

    unsigned char CreateFullscreenPopupWnd(HWND hwndOwnerParam); // 0x402520
};
