// WindowBase -- the root of the "real Win32 HWND" window family. See
// docs/subsystems.md's "WindowBase" and "Custom cursor system" sections for the full
// field/method map; this TU has just the destructor so far.

#include <windows.h>
#include <ddraw.h>

#include "WindowBase.h"
#include "AppWindow.h"
#include "UIResources.h"

#include "DDrawSurface.h" // LocoBitmap_SetColorKey (extern "C" there), DDraw_QuerySurfaceDims

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_Tracef / Port_WatchObject diagnostics
#endif

// FUNCTION: LOCO 0x425870 (v176: first-draft transcribed, not yet byte-matched)
// Zero-initializes every scalar/pointer field, stashes hInstanceParam/resourceIdParam, and
// loads the window's own title string into className via the shared locale-string helper.
WindowBase::WindowBase(void *hInstanceParam, unsigned int resourceIdParam)
{
    hInstance = hInstanceParam;
    hwndSelf = NULL;
    hwndOwner = NULL;
    pCursorSurface = NULL;
    pAnipointCursorRect = NULL;
    pPointCursorRect = NULL;
    pEraserCursorDesc = NULL;
    bSuppressCursorRedraw = false;
    pCursorBitmap = NULL;
    nCursorFrameCount = 0;
    nCursorWidth = 0;
    nCursorHeight = 0;
    nCursorFrameIndex = 0;
    bCursorAnimStopped = 0;
    Unk0x40 = 0;
    hCursorTimer = 0;
    nCursorHotspotX = 0;
    nCursorHotspotY = 0;
    bCreated = false;
    resourceId = resourceIdParam;
    rectLastCursorDraw.left = 0;
    rectLastCursorDraw.right = 0;
    rectLastCursorDraw.top = 0;
    rectLastCursorDraw.bottom = 0;
    g_UIResources.LoadLocaleString(resourceIdParam, className, sizeof(className));
    bModalCaptureActive = 0;
    bCursorRedrawArmed = false;
}

// FUNCTION: LOCO 0x4258f0 (??_GWindowBase scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x425910 // TODO: sync (Ghidra: WindowBase::WindowBase_DtorMaybe -- real C++
// dtor syntax needed here for base-class chaining; same naming gap as EditCardWnd's own dtor)
// Tears down the 3 owned CursorDesc pointers -- pPointCursorRect/pAnipointCursor-
// RectMaybe/pEraserCursorRect are non-owning aliases into the corresponding *DescMaybe
// pointer (set together by InitCursorDescriptors, cleared together here), so each guard
// is keyed off the alias field, not the descriptor pointer itself. The actual per-descriptor
// teardown is CursorDesc::ReleaseRef (vtable slot 2 -- a refcount decrement that
// frees the descriptor's owned sub-objects only once the count hits zero, NOT a plain
// `delete`; confirmed by decompiling the callee, see CursorDesc.h). Then releases the
// shared game-cursor DirectDraw surface (g_pCursorSurface, allocated in
// InitCursorDescriptors) once this is the last WindowBase referencing it
// (g_dwCursorSurfaceRefCount reaches 0).
WindowBase::~WindowBase() {
    if (pPointCursorRect) {
        pPointCursorDesc->ReleaseRef();
        pPointCursorDesc = NULL;
        pPointCursorRect = NULL;
    }
    if (pAnipointCursorRect) {
        pAnipointCursorDesc->ReleaseRef();
        pAnipointCursorDesc = NULL;
        pAnipointCursorRect = NULL;
    }
    if (pEraserCursorDesc) {
        pEraserCursorDesc->ReleaseRef();
        pEraserCursorDesc = NULL;
        pEraserCursorRect = 0;
    }
    if (pCursorSurface) {
        extern IDirectDrawSurface *g_pCursorSurface;   // DAT_004fd3cc
        extern unsigned long g_dwCursorSurfaceRefCount; // DAT_004fd3d0

        g_dwCursorSurfaceRefCount--;
        if (g_dwCursorSurfaceRefCount == 0 && g_pCursorSurface != NULL) {
            g_pCursorSurface->Release();
            g_pCursorSurface = NULL;
            g_dwCursorSurfaceRefCount = 0;
        }
        pCursorSurface = NULL;
    }
    bModalCaptureActive = 0;
}

// FUNCTION: LOCO 0x425990 (Ghidra: EndActiveSession -- renamed in src+Ghidra 2026-07-21 when
// vtable slot 4 was modeled as a real virtual shared with AlbumCardWnd::EndActiveSession/
// EditCardWnd's own override)
// Ends the SetCapture/hidden-cursor modal-input session BeginModalCapture (0x4259c0, not
// itself transcribed yet -- needs RedrawCustomCursor's own cursor-redraw body modeled first, see
// docs/subsystems.md's "Window system" section) started: hides the window, kills the timer it
// armed (Unk0x28), and clears both the "capture active" flag (bModalCaptureActive) and Unk0x44.
void WindowBase::EndActiveSession() {
    ShowWindow(hwndSelf, SW_HIDE);
    KillTimer(hwndSelf, hCursorTimer);
    bModalCaptureActive = 0;
    bCursorRedrawArmed = false;
}

// FUNCTION: LOCO 0x4259c0
// Begins a modal-mouse-capture/hidden-cursor session: arms the 0x78ms cursor-redraw timer
// (hCursorTimer), SetCaptures the window, drains ShowCursor(0) until the system cursor is
// actually hidden, then brackets one full RedrawCustomCursor redraw in a
// Ddraw_RebindWindowClipper() pair -- once for this window, once for the app's own main window
// (g_pApp->hwndOwner) -- before disabling and re-showing this window.
void WindowBase::BeginModalCapture() {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    hCursorTimer = SetTimer(hwndSelf, 0x43, 0x78, NULL);
    bSuppressCursorRedraw = false;
    SetCapture(hwndSelf);
    while (ShowCursor(0) >= 0) {
    }
    Ddraw_RebindWindowClipper(hwndSelf);
    RedrawCustomCursor(1);
    Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
    EnableWindow(hwndSelf, 0);
    ShowWindow(hwndSelf, SW_SHOW);
    bModalCaptureActive = 1;
}

// FUNCTION: LOCO 0x425d30
// vtable slot 0x1c -- re-measure the client area and derive every cached size from it. The
// clip-bounds rect starts life as a straight copy of the client rect; other code narrows it
// later, which is why the two are separate fields and why nClipWidth/nClipHeight are recomputed
// from rectClipBounds rather than reusing width/height. Gated on bCreated because hwndSelf is
// NULL until Create has run.
void WindowBase::RefreshClientClipRect() {
    if (bCreated) {
        GetClientRect(hwndSelf, &rectClient);
        width = rectClient.right - rectClient.left;
        height = rectClient.bottom - rectClient.top;
        rectClipBounds = rectClient;
        nClipWidth = rectClipBounds.right - rectClipBounds.left;
        nClipHeight = rectClipBounds.bottom - rectClipBounds.top;
    }
}

// FUNCTION: LOCO 0x425f20
// Take or give back the mouse capture, keeping the system cursor's hide-count in step: the
// ShowCursor counter is a signed depth, so both directions have to be pumped in a loop until it
// crosses zero rather than called once. Either way the custom cursor is redrawn afterwards,
// bracketed by a Ddraw_RebindWindowClipper() pair -- once onto this window, once back onto the
// app's own main window.
//
// Two things are load-bearing here and both were needed to reach EXACT. The redraw tail really is
// emitted twice, once per arm, rather than once after the branch (docs/CODEGEN.md's
// tail-duplication lever), so it is written out twice. And the RELEASE arm is the fallthrough:
// the original's guard is `test al,al / je <capture arm>`, so the source tests bRelease and
// returns early out of the release path. Writing the capture arm first -- even with the tail
// duplicated -- lets VC5 cross-jump the two tails back into one and loses 42 bytes.
void WindowBase::SetCaptureMode(char bRelease) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    if (bRelease) {
        bSuppressCursorRedraw = true;
        ReleaseCapture();
        while (ShowCursor(1) < 0) {
        }
        Ddraw_RebindWindowClipper(hwndSelf);
        RedrawCustomCursor(1);
        Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
        return;
    }
    bSuppressCursorRedraw = false;
    SetCapture(hwndSelf);
    while (ShowCursor(0) >= 0) {
    }
    Ddraw_RebindWindowClipper(hwndSelf);
    RedrawCustomCursor(1);
    Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
}

// FUNCTION: LOCO 0x425fd0
// vtable slot 0xc -- the convenience overload of slot 0x10: take the hotspot pair and the frame
// count off a cursor DESCRIPTOR instead of making the caller unpack them. The two shorts are
// widened into a 2-int local pair because slot 0x10 wants a pointer to a pair of ints, not the
// descriptor's own packed shorts.
void WindowBase::RequestModeTransitionFromSource(void *pCursorRect, void *pCursorDesc,
                                                 int bResetCounters, int bDoRebind) {
    CursorDesc *pDesc = (CursorDesc *)pCursorDesc;
    int hotspot[2];

    hotspot[0] = pDesc->hotspotX;
    hotspot[1] = pDesc->hotspotY;
    ScheduleModeTransition(pCursorRect, pDesc->nTotalFrameCount, hotspot,
                           (char)bResetCounters, (char)bDoRebind);
}

// FUNCTION: LOCO 0x426020 // EFFECTIVE MATCH -- 260 B vs 260, insns 98/98, DIFF(2)
// One register choice apart: in the non-strip arm the original reads the height with
// `mov eax,[ecx+0xc]` while ours reuses the now-dead pTarget register, `mov ecx,[ecx+0xc]`.
// Same instruction, same length, same everything else -- the documented register coin-flip class.
//
// Three source shapes here are NOT free choices, each worth a large chunk of the match:
//   * the two leading tests are INDEPENDENT `if`s, not an if/else. The original emits a third,
//     redundant `cmp eax,ecx` after the null check, which is exactly what re-testing the same
//     condition in a second statement produces; an if/else CSEs it away and costs ~70 bytes.
//   * bFullRedraw is a real local seeded to 1 and cleared inside the bResetCounters block, not a
//     `RedrawCustomCursor(bResetCounters == 0)` at the call site -- the latter compiles to the
//     sete-materialization idiom instead of the original's byte store.
//   * SetTimer is called in BOTH arms of the eraser test; VC5 cross-jumps the call itself and
//     leaves only the two `push <period>` immediates duplicated. Hoisting it into one call after
//     the branch forces the period through a register (`mov eax,0x32 / push eax`) instead.
//
// vtable slot 0x10 -- point the custom cursor at a new bitmap and (re)arm the redraw pump.
// Re-arming with the target already in place is a no-op except for the redraw/timer tail, which
// is why the field-update block is skipped in that case; asking for the target that is already
// NULL is a complete no-op. nCursorWidth is the per-FRAME width, so it is the bitmap width
// divided by the frame count for an animated cursor strip and the raw width otherwise.
//
// The timer period is the tell for which cursor is up: the eraser cursor pumps at 50ms, every
// other cursor at 120ms.
void WindowBase::ScheduleModeTransition(void *pTargetBitmap, unsigned int nFrameCount,
                                        void *pCursorHotspot, char bResetCounters,
                                        char bDoRebind) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    LocoBitmap *pTarget = (LocoBitmap *)pTargetBitmap;
    char bFullRedraw = 1;

    if (pCursorBitmap == pTarget) {
        if (pTarget == NULL) {
            return;
        }
    }
    if (pCursorBitmap != pTarget) {
        pCursorBitmap = pTarget;
        nCursorFrameCount = nFrameCount;
        nCursorFrameIndex = 0;
        if (pCursorHotspot != NULL) {
            nCursorHotspotX = ((int *)pCursorHotspot)[0];
            nCursorHotspotY = ((int *)pCursorHotspot)[1];
        } else {
            nCursorHotspotX = 0;
            nCursorHotspotY = 0;
        }
        if (pTarget != NULL) {
            if (nFrameCount != 0) {
                nCursorWidth = (unsigned int)pTarget->width / nFrameCount;
                nCursorHeight = pTarget->height;
            } else {
                nCursorWidth = pTarget->width;
                nCursorHeight = pTarget->height;
            }
        } else {
            nCursorWidth = 0;
            nCursorHeight = 0;
        }
    }

    if (bResetCounters != 0) {
        bFullRedraw = 0;
        rectLastCursorDraw.left = 0;
        rectLastCursorDraw.right = 0;
        rectLastCursorDraw.top = 0;
        rectLastCursorDraw.bottom = 0;
    }
    if (bDoRebind != 0 && !bSuppressCursorRedraw) {
        Ddraw_RebindWindowClipper(hwndSelf);
        RedrawCustomCursor(bFullRedraw);
        Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
    }
    if (pCursorBitmap != NULL) {
        if ((unsigned int)pCursorBitmap == pEraserCursorRect) {
            KillTimer(hwndSelf, hCursorTimer);
            hCursorTimer = SetTimer(hwndSelf, 0x43, 50, NULL);
        } else {
            KillTimer(hwndSelf, hCursorTimer);
            hCursorTimer = SetTimer(hwndSelf, 0x43, 120, NULL);
        }
    }
}

// A probe view of WindowBase's vtable for WindowBase_RouteMessage below to dispatch the
// per-message slots (0x2c-0x90) through. Those slots ARE declared as real virtuals on
// WindowBase itself now (modeled 2026-07-22 against WindowBase_Vtbl @ 0x477c30), but
// RouteMessage still can't dispatch through them directly: RouteMessage isn't a member (its
// `pWnd` is an explicit __stdcall stack arg, not `this`), and WindowBase's own defaults at
// most of these slots are declared-only/shared-stub-shaped, so the padded-dummy-virtuals
// probe idiom stays the dispatch mechanism (a pWnd cast, not a `this` cast -- outside
// lint_idiom's class F). The slot-0x1c call this probe used to serve
// (WindowBase::Create's own RefreshClientClipRect) now goes through the REAL virtual on
// WindowBase (slots 0-0x1c are modeled there); the RefreshClientClipRect line below is
// kept only to preserve the probe's own slot alignment. Slots 0x20/0x24/0x28 are dummies
// (0x28 is RouteMessageMaybe's OWN slot -- never called recursively from here); every slot
// from 0x2c on is named per the ground-truthed WM_*-to-slot table in WindowBase.h.
// Confirmed real thiscall ABI via raw disasm: each case loads pWnd into ECX only to dereference
// the vtable pointer (`mov ecx,[pWnd]; mov edx,[ecx]; ...; call [edx+slot]`), but ECX is never
// touched again before the call -- so pWnd survives in ECX as an unavoidable side effect, unlike
// RouteMessageMaybe's own slot 0x28 (WindowBaseVtableView), which is a genuine plain __stdcall
// call with pWnd as an explicit stack argument instead.
struct WindowBaseVtblProbe {
    virtual void *_v00();
    virtual void *_v04();
    virtual void *_v08();
    virtual void *_v0c();
    virtual void *_v10();
    virtual void *_v14();
    virtual void *_v18();
    virtual void RefreshClientClipRect(); // slot 0x1c
    virtual void *_v20();
    virtual void *_v24();
    virtual void *_v28(); // RouteMessageMaybe's own slot
    virtual LRESULT OnUnhandledMessageMaybe(HWND, UINT, WPARAM, LPARAM); // slot 0x2c -- default fallback
    virtual LRESULT OnTimerDefaultMaybe(HWND, UINT, WPARAM, LPARAM);    // slot 0x30 -- WM_TIMER passthrough
    virtual LRESULT OnCreate(HWND, UINT, WPARAM, LPARAM);          // slot 0x34 -- WM_CREATE
    virtual LRESULT OnLButtonDown(HWND, UINT, WPARAM, LPARAM);     // slot 0x38 -- WM_LBUTTONDOWN
    virtual LRESULT OnLButtonUp(HWND, UINT, WPARAM, LPARAM);       // slot 0x3c -- WM_LBUTTONUP
    virtual LRESULT OnRButtonDown(HWND, UINT, WPARAM, LPARAM);     // slot 0x40 -- WM_RBUTTONDOWN
    virtual LRESULT OnRButtonUp(HWND, UINT, WPARAM, LPARAM);       // slot 0x44 -- WM_RBUTTONUP
    virtual LRESULT OnLButtonDblClk(HWND, UINT, WPARAM, LPARAM);   // slot 0x48 -- WM_LBUTTONDBLCLK
    virtual LRESULT OnRButtonDblClk(HWND, UINT, WPARAM, LPARAM);   // slot 0x4c -- WM_RBUTTONDBLCLK
    virtual LRESULT OnMouseMove(HWND, UINT, WPARAM, LPARAM);       // slot 0x50 -- WM_MOUSEMOVE
    virtual LRESULT OnKeyDown(HWND, UINT, WPARAM, LPARAM);         // slot 0x54 -- WM_KEYDOWN
    virtual LRESULT OnKeyUp(HWND, UINT, WPARAM, LPARAM);           // slot 0x58 -- WM_KEYUP
    virtual LRESULT OnMouseActivate(HWND, UINT, WPARAM, LPARAM);   // slot 0x5c -- WM_MOUSEACTIVATE
    virtual LRESULT OnSetFocus(HWND, UINT, WPARAM, LPARAM);        // slot 0x60 -- WM_SETFOCUS
    virtual LRESULT OnKillFocus(HWND, UINT, WPARAM, LPARAM);       // slot 0x64 -- WM_KILLFOCUS
    virtual LRESULT OnSize(HWND, UINT, WPARAM, LPARAM);            // slot 0x68 -- WM_SIZE
    virtual LRESULT OnPaint(HWND, UINT, WPARAM, LPARAM);           // slot 0x6c -- WM_PAINT
    virtual LRESULT OnSetCursor(HWND, UINT, WPARAM, LPARAM);       // slot 0x70 -- WM_SETCURSOR
    virtual LRESULT OnShowWindow(HWND, UINT, WPARAM, LPARAM);      // slot 0x74 -- WM_SHOWWINDOW
    virtual LRESULT OnEraseBkgnd(HWND, UINT, WPARAM, LPARAM);      // slot 0x78 -- WM_ERASEBKGND
    virtual LRESULT OnDestroy(HWND, UINT, WPARAM, LPARAM);         // slot 0x7c -- WM_DESTROY
    virtual LRESULT OnClose(HWND, UINT, WPARAM, LPARAM);           // slot 0x80 -- WM_CLOSE
    virtual LRESULT OnNotify(HWND, UINT, WPARAM, LPARAM);          // slot 0x84 -- WM_NOTIFY
    virtual LRESULT OnCommand(HWND, UINT, WPARAM, LPARAM);         // slot 0x88 -- WM_COMMAND
    virtual LRESULT OnHotKey(HWND, UINT, WPARAM, LPARAM);          // slot 0x8c -- WM_HOTKEY
    virtual LRESULT OnActivateApp(HWND, UINT, WPARAM, LPARAM);     // slot 0x90 -- WM_ACTIVATEAPP
};

// WindowBase's vtable slot 0x28 (RouteMessageMaybe) is NOT a genuine thiscall virtual -- its
// real ABI pushes `pWnd` as a plain 5th stack argument (every case ends `ret 0x14`, no ECX load
// anywhere; see docs/subsystems.md's "Shared vtable slot conventions"). The padded-dummy-
// virtual-probe idiom above (WindowBaseVtblProbe) only reproduces a thiscall-shaped
// `call [vtable+off]` -- this slot needs a plain, properly-typed FUNCTION POINTER field at the
// matching byte offset instead, same reinterpret-the-vtable-pointer spirit, different shape.
struct WindowBaseVtableView {
    void *pad[10]; // slots 0x00-0x24
    LRESULT (__stdcall *pfnRouteMessage)(WindowBase *, HWND, UINT, WPARAM, LPARAM); // slot 0x28
};

// FUNCTION: LOCO 0x4272f0 (Ghidra: WindowBase::WndProc -- see src/WindowBase.h for the
// full behavioral summary.)
LRESULT CALLBACK WindowBase_WndProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowBase *pWnd = (WindowBase *)GetWindowLongA(hwndMsg, GWL_USERDATA);
    if (pWnd == NULL) {
        if (msg == WM_CREATE && lParam != 0) {
            pWnd = (WindowBase *)((CREATESTRUCTA *)lParam)->lpCreateParams;
            SetWindowLongA(hwndMsg, GWL_USERDATA, (LONG)pWnd);
#ifdef LOCO_PORT
            // PORT ONLY -- pairs with the BAD-vptr tripwire below: this is the only place an
            // HWND is bound to an object, so it is what a later bad pWnd has to be matched to.
            Port_Tracef("wnd bind hwnd=%p pWnd=%p vptr=%08x cls=%s\n", (void *)hwndMsg,
                        (void *)pWnd, pWnd ? *(unsigned int *)pWnd : 0,
                        ((CREATESTRUCTA *)lParam)->lpszClass);
            Port_WatchObject(pWnd, 8);  // the vptr word is the whole tripwire
#endif
        } else {
            HWND hwndParent = GetParent(hwndMsg);
            if (hwndParent != NULL) {
                pWnd = (WindowBase *)GetWindowLongA(hwndParent, GWL_USERDATA);
            }
            if (pWnd == NULL) {
                return DefWindowProcA(hwndMsg, msg, wParam, lParam);
            }
        }
    }
#ifdef LOCO_PORT
    // PORT ONLY -- temporary bad-vptr diagnostic. Every routed message dereferences pWnd's
    // vtable pointer, so this is the cheapest tripwire in the process for "the object behind
    // a live HWND stopped being an object". Dumps the head of the object too: whatever
    // overwrote it is usually recognisable on sight.
    {
        unsigned int vptr = *(unsigned int *)pWnd;
        static void *pLastBad = 0;
        if ((vptr < 0x400000u || vptr >= 0x620000u) && pWnd != pLastBad) {
            unsigned int *pw = (unsigned int *)pWnd;
            pLastBad = pWnd;
            Port_Tracef("BAD vptr hwnd=%p pWnd=%p vptr=%08x msg=%04x "
                        "obj=%08x %08x %08x %08x %08x %08x %08x %08x\n",
                        (void *)hwndMsg, (void *)pWnd, vptr, msg,
                        pw[0], pw[1], pw[2], pw[3], pw[4], pw[5], pw[6], pw[7]);
        }
    }
    // PORT ONLY -- byte-neutral for the match build, see the #else arm.
    //
    // Slot 0x28 holds a bare __stdcall free function taking pWnd as an explicit 5th stack arg
    // (0x426140, `ret 0x14`, no ECX use -- ground-truthed from the raw disasm). A C++ virtual
    // member cannot occupy that slot with that ABI, so src/WindowBase.h keeps `_v28` as a
    // declared-only dummy slot-holder. In the PORT build that dummy is what actually lands in
    // every generated vtable, and link/gen_stubs.py sizes its stub from the MANGLED name --
    // `?_v28@WindowBase@@UAEPAXXZ` says "no arguments", so the stub is a bare `ret` while this
    // call site pushes 20 bytes and expects callee cleanup. Every routed message therefore
    // leaked 20 bytes of stack AND never reached the real router; after 19 of them the return
    // address was garbage and the process jumped to 0 (EIP=0, the v557d crash).
    //
    // Calling the real router directly is safe rather than a guess: the dword 0x00426140 appears
    // at exactly 8 places in the original .rdata, all of them slot 0x28 of a WindowBase-family
    // vtable, so NO derived class overrides this slot and the indirection has one possible
    // target anyway.
    return WindowBase_RouteMessage(pWnd, hwndMsg, msg, wParam, lParam);
#else
    WindowBaseVtableView *vt = *(WindowBaseVtableView **)pWnd;
    return vt->pfnRouteMessage(pWnd, hwndMsg, msg, wParam, lParam);
#endif
}

// FUNCTION: LOCO 0x426140 (Ghidra: WindowBase::RouteMessageMaybe -- see src/WindowBase.h for the
// full WM_*-to-slot table this dispatches through.) The three custom-cursor inline special cases
// (WM_NCHITTEST/WM_TIMER's cursor-frame-animation sub-case/WM_MOUSEMOVE's auto-release-capture,
// plus WM_CAPTURECHANGED which is inline-only, no vtable call) all share the same
// hide-cursor-and-capture / show-cursor-and-release pair already established by
// BeginModalCapture/EndModalCapture -- same ShowCursor drain-loop idiom reused here.
// ⚠ Case order below is NOT WM_* numeric order -- it's the TRUE source declaration order,
// ground-truthed instruction-by-instruction from the raw disasm's own physical call sequence
// (Yoda lesson #33: jump-table case bodies land in source declaration order, not value order).
// Notably `default:` is declared BEFORE `case WM_HOTKEY:` -- the vtable-slot-0x2c (default)
// call site sits physically right before WM_HOTKEY's own, which is the true last call in the
// whole function. WM_CAPTURECHANGED's own success-path tail also reuses the shared `returnZero`
// goto target (not a separate inline `return 0;`) -- confirmed via raw disasm, the ONE shared
// tail at the ground-truthed address serves all 5 zero-return exits (WM_TIMER's frame-count
// guard + WM_CAPTURECHANGED's 3-way guard + its own success path), not just the first 4.
//
// EFFECTIVE MATCH (asmscore.py --len 1905: every real instruction AND the compiler-emitted
// jump-table data trailing the code are byte-identical; match.py's own diff at this candidate's
// full 1980-byte extent -- code + embedded jump tables, the true COMDAT size, vs. Ghidra's
// 1905-byte code-only "Body" figure -- finds exactly 3 differing bytes, all inside one 3-byte
// alignment-padding NOP immediately after the code and before the jump-table data: original
// `8d 49 00` (lea ecx,[ecx+0]) vs. this compile's `90 8b ff` (nop; mov edi,edi). Both are
// pure 3-byte NOP-equivalent fillers reaching the identical alignment boundary -- an intrinsic
// NOP-encoding tie-break (same class as the documented register-swap ties), not source-
// steerable. PARKED as EFFECTIVE, see docs/PARKED.md.
LRESULT __stdcall WindowBase_RouteMessage(WindowBase *pWnd, HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    WindowBaseVtblProbe *pProbe = (WindowBaseVtblProbe *)pWnd;

    switch (msg) {
    case WM_CREATE:
        return pProbe->OnCreate(hwndMsg, msg, wParam, lParam);
    case WM_SETFOCUS:
        return pProbe->OnSetFocus(hwndMsg, msg, wParam, lParam);
    case WM_KILLFOCUS:
        return pProbe->OnKillFocus(hwndMsg, msg, wParam, lParam);
    case WM_SIZE:
        return pProbe->OnSize(hwndMsg, msg, wParam, lParam);
    case WM_PAINT:
        pWnd->bCursorRedrawArmed = true;
        return pProbe->OnPaint(hwndMsg, msg, wParam, lParam);
    case WM_DESTROY:
        return pProbe->OnDestroy(hwndMsg, msg, wParam, lParam);
    case WM_CLOSE:
        return pProbe->OnClose(hwndMsg, msg, wParam, lParam);
    case WM_ERASEBKGND:
        return pProbe->OnEraseBkgnd(hwndMsg, msg, wParam, lParam);
    case WM_SHOWWINDOW:
        return pProbe->OnShowWindow(hwndMsg, msg, wParam, lParam);
    case WM_ACTIVATEAPP:
        return pProbe->OnActivateApp(hwndMsg, msg, wParam, lParam);
    case WM_MOUSEACTIVATE:
        return pProbe->OnMouseActivate(hwndMsg, msg, wParam, lParam);
    case WM_SETCURSOR:
        return pProbe->OnSetCursor(hwndMsg, msg, wParam, lParam);
    case WM_NOTIFY:
        return pProbe->OnNotify(hwndMsg, msg, wParam, lParam);
    case WM_NCHITTEST: {
        LRESULT lHit = DefWindowProcA(hwndMsg, WM_NCHITTEST, wParam, lParam);
        if (lHit == HTCLIENT) {
            pWnd->bSuppressCursorRedraw = false;
            SetCapture(pWnd->hwndSelf);
            while (ShowCursor(0) >= 0) {
            }
        } else {
            pWnd->bSuppressCursorRedraw = true;
            ReleaseCapture();
            while (ShowCursor(1) < 0) {
            }
        }
        Ddraw_RebindWindowClipper(pWnd->hwndSelf);
        pWnd->RedrawCustomCursor(1);
        Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
        return lHit;
    }
    case WM_KEYDOWN:
        return pProbe->OnKeyDown(hwndMsg, msg, wParam, lParam);
    case WM_KEYUP:
        return pProbe->OnKeyUp(hwndMsg, msg, wParam, lParam);
    case WM_COMMAND:
        return pProbe->OnCommand(hwndMsg, msg, wParam, lParam);
    case WM_TIMER:
        if (wParam == 0x43 && pWnd->pCursorBitmap != NULL &&
            !pWnd->bSuppressCursorRedraw && pWnd->bCursorAnimStopped == 0) {
            POINT ptCursor;
            GetCursorPos(&ptCursor);
            if (pWnd->nCursorFrameCount <= 1) {
                goto returnZero;
            }
            pWnd->nCursorFrameIndex++;
            if (pWnd->nCursorFrameCount <= pWnd->nCursorFrameIndex) {
                pWnd->nCursorFrameIndex = 0;
                if (pWnd->Unk0x40 != 0) {
                    pWnd->Unk0x40--;
                    if (pWnd->Unk0x40 == 0) {
                        pWnd->bCursorAnimStopped = 1;
                    }
                }
            }
            if (ptCursor.x == pWnd->nLastCursorScreenX && ptCursor.y == pWnd->nLastCursorScreenY) {
                Ddraw_RebindWindowClipper(pWnd->hwndSelf);
                pWnd->RedrawCustomCursor(1);
                Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
            }
            pWnd->nLastCursorScreenX = ptCursor.x;
            pWnd->nLastCursorScreenY = ptCursor.y;
            return 0;
        }
        return pProbe->OnTimerDefaultMaybe(hwndMsg, msg, wParam, lParam);
    case WM_MOUSEMOVE: {
        if (hwndMsg != pWnd->hwndSelf) {
            return 0;
        }
        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);
        HWND hWndUnder = WindowFromPoint(pt);
        if (!pWnd->bSuppressCursorRedraw) {
            if (hWndUnder != pWnd->hwndSelf) {
                pWnd->bSuppressCursorRedraw = true;
                ReleaseCapture();
                while (ShowCursor(1) < 0) {
                }
                Ddraw_RebindWindowClipper(pWnd->hwndSelf);
                pWnd->RedrawCustomCursor(1);
                Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
                return 0;
            }
        } else {
            if (hWndUnder != pWnd->hwndSelf) {
                return 0;
            }
            pWnd->bSuppressCursorRedraw = false;
            SetCapture(pWnd->hwndSelf);
            while (ShowCursor(0) >= 0) {
            }
            Ddraw_RebindWindowClipper(pWnd->hwndSelf);
            pWnd->RedrawCustomCursor(1);
            Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
        }
        return pProbe->OnMouseMove(hwndMsg, msg, wParam, lParam);
    }
    case WM_LBUTTONDOWN:
        SetForegroundWindow(pWnd->hwndSelf);
        return pProbe->OnLButtonDown(hwndMsg, msg, wParam, lParam);
    case WM_LBUTTONUP:
        return pProbe->OnLButtonUp(hwndMsg, msg, wParam, lParam);
    case WM_RBUTTONDOWN:
        SetForegroundWindow(pWnd->hwndSelf);
        return pProbe->OnRButtonDown(hwndMsg, msg, wParam, lParam);
    case WM_RBUTTONUP:
        return pProbe->OnRButtonUp(hwndMsg, msg, wParam, lParam);
    case WM_LBUTTONDBLCLK:
        SetForegroundWindow(pWnd->hwndSelf);
        return pProbe->OnLButtonDblClk(hwndMsg, msg, wParam, lParam);
    case WM_RBUTTONDBLCLK:
        SetForegroundWindow(pWnd->hwndSelf);
        return pProbe->OnRButtonDblClk(hwndMsg, msg, wParam, lParam);
    case WM_CAPTURECHANGED:
        if (pWnd->pCursorBitmap == NULL || (HWND)lParam == pWnd->hwndSelf || lParam == 0) {
            goto returnZero;
        }
        pWnd->bSuppressCursorRedraw = true;
        ReleaseCapture();
        while (ShowCursor(1) < 0) {
        }
        Ddraw_RebindWindowClipper(pWnd->hwndSelf);
        pWnd->RedrawCustomCursor(1);
        Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
        goto returnZero;
    default:
        return pProbe->OnUnhandledMessageMaybe(hwndMsg, msg, wParam, lParam);
    case WM_HOTKEY:
        return pProbe->OnHotKey(hwndMsg, msg, wParam, lParam);
    }

returnZero:
    return 0;
}

// The four shared per-message default handlers below are ground-truthed as WINDOWBASE's own
// (present in WindowBase_Vtbl itself at slots 0x5c/0x70/0x78/0x7c, address-contiguous with
// RouteMessageMaybe above) -- NOT PopupWndBase's, whose independently-laid-out vtable merely
// installs the same four addresses at its own shifted slots (0x58/0x6c/0x74/0x78). They were
// mislabeled PopupWndBase:: until 2026-07-22 (v322); PopupWndBase.h keeps extern declarations
// to hold that family's vtable layout. Both families carry hwndSelf at +0x8, which is what
// lets one body serve both vtables. Declared as REAL virtuals on WindowBase since
// 2026-07-22 (the whole 0x2c-0x90 message block is now modeled on the class -- see
// src/WindowBase.h); OnMouseActivateNoOp/OnEraseBkgndNoOp were renamed to the probe's slot
// names OnMouseActivate/OnEraseBkgnd at the same time (Ghidra synced 2026-07-22, v323).
// RouteMessage's dispatch still goes through the
// WindowBaseVtblProbe above, not through these declarations.

// FUNCTION: LOCO 0x426900 (WindowBase_Vtbl slot 0x50, WM_MOUSEMOVE) -- WindowBase's default
// mouse-move handler. When the message is for this window's own hwndSelf, repaints the software
// cursor at its new position, bracketed by the same Ddraw_RebindWindowClipper() pair every other
// cursor redraw in this file uses (onto this window, then back onto g_pApp->hwndOwner). Always
// forwards to DefWindowProcA afterwards, for either target. This is the handler RouteMessage's
// WM_TIMER motion check and OnPaint's synthetic PostMessageA(WM_MOUSEMOVE) both drive.
LRESULT WindowBase::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    if (hwndMsg == hwndSelf) {
        Ddraw_RebindWindowClipper(hwndSelf);
        RedrawCustomCursor(1);
        Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x426950 (WindowBase_Vtbl slot 0x5c, WM_MOUSEACTIVATE; Ghidra synced) -- bare return 0.
LRESULT WindowBase::OnMouseActivate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return 0;
}

// FUNCTION: LOCO 0x426960 (WindowBase_Vtbl slot 0x68, WM_SIZE) -- re-measures the client/clip
// rects through the slot-0x1c hook, but only once the window has actually been created (bCreated);
// CreateWindowExA itself fires WM_SIZE before WindowBase::Create has finished wiring the object up,
// and RefreshClientClipRect would then run against a half-built window.
LRESULT WindowBase::OnSize(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (bCreated) {
        RefreshClientClipRect();
    }
    return 0;
}

// FUNCTION: LOCO 0x426980 (WindowBase_Vtbl slot 0x6c, WM_PAINT; Ghidra: OnPaint) --
// WindowBase's default paint handler. Rebinds the ddraw clipper onto this window, and if
// there IS an update region, BeginPaints; a FAILED BeginPaint (NULL hdc) unwinds with
// EndPaint + clipper rebind back onto the app's main window + `return 1`. Either way the
// main path runs EndPaint (sic: unconditionally, even when GetUpdateRect returned 0 and
// ps was never initialized), fires the slot-0x20 OnActivate hook (passed the ps pointer
// as its reserved arg) and the slot-0x24 OnIdlePump hook, rebinds the clipper back onto
// g_pApp->hwndOwner, re-enables the window, and posts a synthetic WM_MOUSEMOVE with the
// current cursor position so the software cursor repaints over the new content.
LRESULT WindowBase::OnPaint(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    PAINTSTRUCT ps;
    RECT rcUpdate;
    POINT ptCursor;

    Ddraw_RebindWindowClipper(hwndSelf);
    if (GetUpdateRect(hwndMsg, &rcUpdate, FALSE) != 0) {
        if (BeginPaint(hwndMsg, &ps) == NULL) {
            EndPaint(hwndMsg, &ps);
            Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
            return 1;
        }
    }
    EndPaint(hwndMsg, &ps);
    OnActivate((int)&ps);
    OnIdlePump();
    Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
    EnableWindow(hwndSelf, TRUE);
    GetCursorPos(&ptCursor);
    PostMessageA(hwndSelf, WM_MOUSEMOVE, 0, MAKELPARAM(ptCursor.x, ptCursor.y));
    return 0;
}

// FUNCTION: LOCO 0x426a60 (WindowBase_Vtbl slot 0x70, WM_SETCURSOR) -- returns 1 (handled,
// suppress the OS cursor) if the target is this window's own hwndSelf, else forwards to
// DefWindowProcA.
LRESULT WindowBase::OnSetCursor(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (hwndMsg != hwndSelf) {
        return DefWindowProcA(hwndMsg, msg, wParam, lParam);
    }
    return 1;
}

// FUNCTION: LOCO 0x426a90 (WindowBase_Vtbl slot 0x80, WM_CLOSE) -- clears bCreated so the
// still-pending WM_SIZE/WM_DESTROY traffic stops calling back into a dying window, then destroys
// it. An UNOWNED window (hwndOwner == NULL) is by construction the application's own top-level
// window, so closing it quits the message loop.
LRESULT WindowBase::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    bCreated = false;
    DestroyWindow(hwndSelf);
    if (hwndOwner == NULL) {
        PostQuitMessage(0);
    }
    return 0;
}

// FUNCTION: LOCO 0x426ac0 (WindowBase_Vtbl slot 0x78, WM_ERASEBKGND; Ghidra synced) --
// bare return 1, the standard "claim erased" DirectDraw double-buffer idiom.
LRESULT WindowBase::OnEraseBkgnd(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return 1;
}

// FUNCTION: LOCO 0x426ad0 (WindowBase_Vtbl slot 0x7c, WM_DESTROY) -- zeroes hwndSelf, forwards
// to DefWindowProcA.
LRESULT WindowBase::OnDestroy(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    hwndSelf = NULL;
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x422ea0 (Ghidra: WindowBase::DefWindowProcStub -- see src/WindowBase.h
// for the full list of vtable slots this shared no-op-override stub is installed at.)
LRESULT __stdcall WindowBase_DefWindowProcStub(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// ---- The eighteen per-message defaults that WindowBase_Vtbl (0x477c30) installs at slots
// 0x2c/0x30/0x34/0x38/0x3c/0x40/0x44/0x48/0x4c/0x54/0x58/0x60/0x64/0x74/0x84/0x88/0x8c/0x90.
// Every one of those dwords reads 0x422ea0 in the image -- the SAME address as the free
// WindowBase_DefWindowProcStub just above, and as PopupWndBase's own nineteen. That is ICF, not
// one function wearing many hats: a `__thiscall` body that never touches its implicit `this`
// compiles to the identical 29 bytes as the `__stdcall` free function, so the linker folds all
// of them onto one copy (the same mechanism that folded this image's `??_G*` thunks).
//
// So there is exactly ONE address marker for 0x422ea0 in the repo and it stays on the free
// function above; these are its unmarked twins. (Do NOT give them markers of their own -- a
// second marker on one address double-counts it in progress.py, and writing the marker text in
// prose is enough to fool the parser: this comment used to spell it out and cost the whole file
// its pairing.) They were declared-only for ~240 sessions, which
// cost nothing in the match build (a declared-only virtual still lays the right dword in the
// vtable COMDAT) and everything in the PORT build, where each became a zeroed `Stub_Report`
// slot: `WindowBase::OnTimerDefaultMaybe` alone was the hottest stub in the whole run at 741
// calls per boot, returning 0 where the game returns DefWindowProcA's own result.
LRESULT WindowBase::OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnCreate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnLButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnRButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnLButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnRButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnKeyUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnShowWindow(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnNotify(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnCommand(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnHotKey(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT WindowBase::OnActivateApp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x425b70
// Shared RegisterClassA+CreateWindowExA helper for every WindowBase-derived singleton window.
// Steals hwndOwnerParam's own window TEXT via GetWindowTextA to use as this window's title (a
// real 256-byte stack buffer, not a resource string). nClassStyle overrides the
// registered WNDCLASSA's own style (defaults to CS_HREDRAW|CS_VREDRAW=3 when 0).
//
// EXACT MATCH (v358). Was parked EFFECTIVE at byte_diff 6 -- the low byte of 3
// field-store/param-load disp32s (x/y/height) landing in a rotated order -- with an autopsy
// claiming statement order had "zero effect". That was wrong, and the reason it read as
// intrinsic is worth remembering: **VC5 emits a run of independent scalar param->field copies
// ROTATED LEFT BY ONE from source order.** Rotating the source by one step therefore rotates
// the output by one step too, so a single trial lands on a DIFFERENT wrong rotation and looks
// like noise; you have to walk the whole cycle. Source `x,y,height` emitted `y,height,x`;
// source `y,height,x` emitted `height,x,y`; source `height,x,y` -- below -- emits `x,y,height`
// and is byte-exact. So the ORIGINAL source order for these three is height, x, y.
// (The copies must be genuinely independent -- no aliasing between them -- for the rotation
// to be a pure permutation like this.)
unsigned char WindowBase::Create(int nShowCmd, HWND hwndOwnerParam, int xParam, int yParam,
                                  int widthParam, int heightParam, HMENU hMenu, HICON hIcon,
                                  unsigned int nClassStyle, unsigned int dwStyle, unsigned int dwExStyle)
{
    char szOwnerTitle[256];

    hwndSelf = NULL;
    GetWindowTextA(hwndOwnerParam, szOwnerTitle, 0x100);
    width = widthParam;
    hwndOwner = hwndOwnerParam;
    height = heightParam;
    x = xParam;
    y = yParam;

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = 3;
    if (nClassStyle != 0) {
        wc.style = nClassStyle;
    }
    wc.lpfnWndProc = WindowBase_WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)hInstance;
    wc.hIcon = hIcon;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = className;

    if (RegisterClassA(&wc) == 0) {
        DWORD dwErr = GetLastError();
        if (dwErr != 0) {
            LPSTR pszMsg;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszMsg, 0, NULL);
            LocalFree(pszMsg);
        }
    }

#ifdef LOCO_PORT
    // PORT: 0x87000000 is WS_POPUP|WS_MAXIMIZE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, and WS_MAXIMIZE is
    // the half that has to go here. A maximized window is sized to the DESKTOP by the window
    // manager, whatever width/height the create asked for. In 1998 the game ran AT the desktop
    // resolution so the two agreed and the bit was free; under winemac the desktop is 3600x2338
    // while every surface the game paints into is g_dwScreenWidth x g_dwScreenHeight.
    //
    // Clamping the requested rect at the call sites (Port_ClampDesktopRect, seven of them) is
    // necessary but NOT sufficient on its own -- measured v569: all seven windows then asked for
    // 1024x768 and every one of them still came up with a 3600x2338 CLIENT rect, because
    // WS_MAXIMIZE overrode the request. RefreshClientClipRect copies that client rect into
    // rectClipBounds, and CommitScreenUpdate presents it out of the 1024x768 work surface --
    // a wholly black interactive boot, and the unclipped-blit heap corruption of v562.
    //
    // A WS_POPUP window has no frame, so its client rect equals the requested extent exactly.
    hwndSelf = CreateWindowExA(0, className, szOwnerTitle, 0x87000000 & ~WS_MAXIMIZE,
                                xParam, yParam, width, height,
                                hwndOwner, hMenu, (HINSTANCE)hInstance, this);
#else
    hwndSelf = CreateWindowExA(0, className, szOwnerTitle, 0x87000000, xParam, yParam, width, height,
                                hwndOwner, hMenu, (HINSTANCE)hInstance, this);
#endif
    if (hwndSelf == NULL) {
        LPSTR pszMsg;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pszMsg, 0, NULL);
        LocalFree(pszMsg);
    }
    if (hwndSelf == NULL) {
        return 0;
    }

    bCreated = 1;
#ifdef LOCO_PORT
    {   // PORT ONLY -- byte-neutral. Every WindowBase creation with its caller, so an oversized
        // client rect can be traced to the call site that asked for it. Map ret with
        // link/Loco-port.map.
        static unsigned int nCre = 0;
        if (++nCre <= 12) {
            POINT ptOrg = {0, 0};
            RECT rcWnd;
            ClientToScreen(hwndSelf, &ptOrg);
            GetWindowRect(hwndSelf, &rcWnd);
            Port_Tracef("create #%u this=%08lx hwnd=%08lx ret=%08lx cls=%s at=%d,%d %dx%d "
                        "org=%ld,%ld wnd=%ld,%ld,%ld,%ld\n", nCre,
                        (unsigned long)this, (unsigned long)hwndSelf,
                        (unsigned long)((void **)&nShowCmd)[-1], className ? className : "?",
                        xParam, yParam, widthParam, heightParam,
                        (long)ptOrg.x, (long)ptOrg.y,
                        (long)rcWnd.left, (long)rcWnd.top, (long)rcWnd.right, (long)rcWnd.bottom);
        }
    }
#endif
    this->RefreshClientClipRect();
    InitCursorDescriptorsMaybe();
    ShowWindow(hwndSelf, nShowCmd);
    UpdateWindow(hwndSelf);
    return 1;
}

// FUNCTION: LOCO 0x425dc0 (Ghidra: WindowBase::InitCursorDescriptorsMaybe)
// Resolves the three named cursor resources (0x1400 = cursors-point, 0x1403 = cursors-anipoint,
// 0x1402 = cursors-eraser -- paths into the RF archive) via the shared lazy registry
// g_UIResources/TileKind_GetOrLoadDescriptor. For each: caches the descriptor pointer and the
// return of its own GetOrLoadFrameBitmap(0, 0) (the *Rect fields are really that realized
// LocoBitmap* -- pEraserCursorRect is still declared `unsigned int` in the header, hence the
// cast). For the POINT cursor only, also seeds nCursorWidth/nCursorHeight/nCursorFrameCount
// from the descriptor's own nativeWidth/nativeHeight/nTotalFrameCount -- the same trio
// ScheduleModeTransition computes at runtime, i.e. this establishes the INITIAL transition
// state synchronously instead of going through the timer scheduler. Then allocates the shared
// game-cursor DirectDraw surface (g_pCursorSurface, a 256x256 system-memory offscreen surface
// with the magenta transparency colorkey set) if not already created, caches it in
// pCursorSurface, and bumps the refcount (released by PopupWndBase's teardown when the count
// reaches 0).
void WindowBase::InitCursorDescriptorsMaybe()
{
    extern IDirectDraw2 *g_pDDraw2;                       // DAT_00485440
    extern IDirectDrawSurface *g_pCursorSurface;          // DAT_004fd3cc
    extern unsigned long g_dwCursorSurfaceRefCount;       // DAT_004fd3d0
    extern void Ddraw_QuerySurfacePixelFormat(IDirectDrawSurface *pSurface, DDSURFACEDESC *pDesc,
                                              char bUpdateGlobals); // Ddraw::…, 0x45b9b0
    extern void DDraw_QuerySurfaceDims(IDirectDrawSurface *pSurface, unsigned short *pOutWidth,
                                       unsigned short *pOutHeight); // 0x4014e0, src/DDrawSurface.cpp

    pPointCursorDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x1400);
    if (pPointCursorDesc != NULL) {
        pPointCursorRect = pPointCursorDesc->GetOrLoadFrameBitmap(0, 0);
        nCursorWidth = pPointCursorDesc->nativeWidth;
        nCursorHeight = pPointCursorDesc->nativeHeight;
        nCursorFrameCount = pPointCursorDesc->nTotalFrameCount;
    }
    pAnipointCursorDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x1403);
    if (pAnipointCursorDesc != NULL) {
        pAnipointCursorRect = pAnipointCursorDesc->GetOrLoadFrameBitmap(0, 0);
    }
    pEraserCursorDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x1402);
    if (pEraserCursorDesc != NULL) {
        pEraserCursorRect = (unsigned int)pEraserCursorDesc->GetOrLoadFrameBitmap(0, 0);
    }
    if (g_pCursorSurface == NULL) {
        DDSurfaceDescPadded0x7c u;
        unsigned short wSurfaceHeight;
        unsigned short wSurfaceWidth;

        memset(&u, 0, sizeof(u));
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        u.ddsd.dwWidth = 0x100;
        u.ddsd.dwHeight = 0x100;
        u.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        u.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        HRESULT hr = g_pDDraw2->CreateSurface(&u.ddsd, &g_pCursorSurface, NULL);
        if (hr != 0) {
            OutputDebugStringA("CGWND - failed to create cursor surface");
        }
        DDraw_QuerySurfaceDims(g_pCursorSurface, &wSurfaceWidth, &wSurfaceHeight);
        Ddraw_QuerySurfacePixelFormat(g_pCursorSurface, &u.ddsd, 0);
        LocoBitmap_SetColorKey(g_pCursorSurface, &u.ddsd);
    }
    pCursorSurface = g_pCursorSurface;
    g_dwCursorSurfaceRefCount++;
}

// FUNCTION: LOCO 0x426b00
// Acquires an HDC on the primary DirectDraw work surface via a GetDC-shaped retry loop (up to
// 1000x, Sleep(10) between attempts; fatal FUN_00463600()/ExitProcess(1) if it never
// succeeds), bracketed by a Ddraw_RebindWindowClipper(hwndTarget) call. The acquired HDC
// is stashed at hdcWorkSurface and returned. Called from AlbumCardWnd::RedrawAllSlots to get
// an HDC for its per-slot name DrawTextA loop.
HDC WindowBase::AcquireWorkSurfaceDC(HWND hwndTarget) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    extern void ShowFatalErrorMessageBox(int nErrorCodeMaybe); // 0x463600, defined below in
                                                                      // this TU -- see its own
                                                                      // comment re: the dead
                                                                      // nErrorCodeMaybe argument.
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    int nRetryCount = 0;
    HDC *pHdc = &hdcWorkSurface;
    Ddraw_RebindWindowClipper(hwndTarget);

    HRESULT hr = g_pDDrawWorkSurface->GetDC(pHdc);
    while (hr != 0) {
        nRetryCount++;
        Sleep(10);
#ifdef LOCO_PORT
        // PORT ONLY -- boot diagnostic, byte-neutral for the match build.
        if (nRetryCount == 1) {
            Port_Tracef("AcquireWorkSurfaceDC: GetDC failed hr=%08x surf=%p hwnd=%p\n",
                        (unsigned)hr, (void *)g_pDDrawWorkSurface, (void *)hwndTarget);
        }
#endif
        if (nRetryCount > 1000) {
            ShowFatalErrorMessageBox(0x48);
            ExitProcess(1);
        }
        hr = g_pDDrawWorkSurface->GetDC(pHdc);
    }
    return *pHdc;
}

// FUNCTION: LOCO 0x426eb0 // TODO: idiom (first-draft transcription, not yet byte-matched --
// see docs/subsystems.md's "Custom cursor system" section for the field-level writeup this
// session's struct surgery produced. Two locals below (nClampLeftMaybe/nClampHeightMaybe) stand
// in for values Ghidra's own decompile shows as unaff_EBX/unaff_EDI -- register-resident copies
// of the same left/height clamp deltas computed earlier in the function that Ghidra's SSA
// tracking loses across the branchy clip/union logic (same decompiler-breaks-on-this-shape class
// already documented for LocoBitmap::RestoreOverlapBlt/ClampBlitRects, see docs/PARKED.md).)
void WindowBase::RedrawCustomCursor(char bFullRedraw) {
    extern IDirectDrawSurface *g_pDDrawPrimarySurface; // DAT_004fd3c0
    extern IDirectDrawSurface *g_pDDrawWorkSurface;    // DAT_004fd3c4

    if (!bCursorRedrawArmed) {
        return;
    }

    int nClampLeftMaybe = 0;
    bool bUnionModeMaybe = false;
    int nClampTopMaybe = 0;

    POINT ptCursor;
    GetCursorPos(&ptCursor);
    ptCursor.y -= nCursorHotspotY;
    ptCursor.x -= nCursorHotspotX;

    int nWidth = nCursorWidth;
    int nClampHeightMaybe = nCursorHeight;

    RECT rectCursor;
    rectCursor.bottom = nClampHeightMaybe + ptCursor.y;
    rectCursor.right = nWidth + ptCursor.x;
    if (rectClipBounds.right < nWidth + ptCursor.x) {
        nWidth = rectClipBounds.right - ptCursor.x;
        rectCursor.right = rectClipBounds.right;
    }
    if (rectClipBounds.bottom < rectCursor.bottom) {
        nClampHeightMaybe = rectClipBounds.bottom - ptCursor.y;
        rectCursor.bottom = rectClipBounds.bottom;
    }
    rectCursor.top = ptCursor.y;
    if (ptCursor.y < rectClipBounds.top) {
        nClampHeightMaybe = rectCursor.bottom - rectClipBounds.top;
        nClampTopMaybe = rectClipBounds.top - ptCursor.y;
        rectCursor.top = rectClipBounds.top;
    }
    rectCursor.left = ptCursor.x;
    if (ptCursor.x < rectClipBounds.left) {
        nClampLeftMaybe = rectClipBounds.left - ptCursor.x;
        nWidth = rectCursor.right - rectClipBounds.left;
        rectCursor.left = rectClipBounds.left;
    }

    RECT rectUnion;
    if (pCursorBitmap != NULL && rectLastCursorDraw.right != 0 && bFullRedraw != 0 &&
        !bSuppressCursorRedraw) {
        UnionRect(&rectUnion, &rectLastCursorDraw, &rectCursor);
        if (rectUnion.right - rectUnion.left < 0x100 && rectUnion.bottom - rectUnion.top < 0x100) {
            rectUnion.left -= 4;
            rectUnion.right += 4;
            rectUnion.bottom += 4;
            rectUnion.top -= 4;
            if (rectClipBounds.right < rectUnion.right) {
                rectUnion.right = rectClipBounds.right;
            }
            if (rectClipBounds.bottom < rectUnion.bottom) {
                rectUnion.bottom = rectClipBounds.bottom;
            }
            if (rectUnion.top < rectClipBounds.top) {
                rectUnion.top = rectClipBounds.top;
            }
            if (rectUnion.left < rectClipBounds.left) {
                rectUnion.left = rectClipBounds.left;
            }
            bUnionModeMaybe = true;
        }
    }

    if (rectLastCursorDraw.right != 0 && bFullRedraw != 0 && !bUnionModeMaybe) {
        g_pDDrawPrimarySurface->Blt(&rectLastCursorDraw, g_pDDrawWorkSurface,
                                          &rectLastCursorDraw, 0x1000000, NULL);
    }

    nLastCursorScreenX = 0xffffffff;
    nLastCursorScreenY = 0xffffffff;

    if (pCursorBitmap != NULL && !bSuppressCursorRedraw) {
        // Four field stores through a pointer local, not a whole-struct assignment: the original
        // materializes `lea edx,[esi+0x50]` into a slot (VC5 reclaims the now-dead bFullRedraw
        // parameter slot for it) and writes .left/.top/.right/.bottom through edx. Same shape as
        // the sibling PopupWndBase::RedrawSoftwareCursor, and the pointer is live all the way
        // down to the non-union path's final primary Blt, which passes it as lpDestRect.
        RECT *pPrevRect = &rectLastCursorDraw;
        pPrevRect->left = rectCursor.left;
        pPrevRect->top = rectCursor.top;
        pPrevRect->right = rectCursor.right;
        pPrevRect->bottom = rectCursor.bottom;

        if (bUnionModeMaybe) {
            int copyW = rectUnion.right - rectUnion.left;
            int copyH = rectUnion.bottom - rectUnion.top;
            RECT rectScratch = {0, 0, copyW, copyH};
            pCursorSurface->Blt(&rectScratch, g_pDDrawWorkSurface, &rectUnion, 0x1000000, NULL);

            int nFrameOffX;
            if (nCursorFrameCount < 2) {
                nFrameOffX = 0;
            } else {
                if (nCursorFrameCount <= nCursorFrameIndex) {
                    nCursorFrameIndex = 0;
                }
                nFrameOffX = nCursorFrameIndex * nCursorWidth;
            }

            RECT srcRect;
            srcRect.left = nFrameOffX + nClampLeftMaybe;
            srcRect.top = nClampTopMaybe;
            srcRect.right = srcRect.left + nWidth;
            srcRect.bottom = srcRect.top + nClampHeightMaybe;

            RECT destRect;
            destRect.left = rectLastCursorDraw.left - rectUnion.left;
            destRect.top = rectLastCursorDraw.top - rectUnion.top;
            destRect.right = destRect.left + nWidth;
            destRect.bottom = destRect.top + nClampHeightMaybe;

            // The sprite is stamped INTO the scratch at destRect (its offset within the union
            // box); the scratch is then blitted back to the primary AT THE UNION RECT, reading
            // the whole scratch region. Blitting destRect/srcRect up instead -- which is what
            // this transcription used to do -- puts the cursor a few pixels from the screen
            // origin and reads sprite-strip coordinates out of the scratch, which is both
            // halves of the "cursor stuck in the top-left with a black box" symptom.
            pCursorBitmap->RestoreOverlapBlt(destRect, pCursorSurface, srcRect, 0);
            g_pDDrawPrimarySurface->Blt(&rectUnion, pCursorSurface, &rectScratch, 0x1000000, NULL);
            return;
        }

        int copyW = nWidth;
        int copyH = nClampHeightMaybe;
        RECT rectScratch = {0, 0, copyW, copyH};
        pCursorSurface->Blt(&rectScratch, g_pDDrawWorkSurface, &rectCursor, 0x1000000, NULL);

        int nFrameOffX = 0;
        if (nCursorFrameCount > 1) {
            if (nCursorFrameCount <= nCursorFrameIndex) {
                nCursorFrameIndex = 0;
            }
            nFrameOffX = nCursorFrameIndex * nCursorWidth;
        }

        RECT srcRect;
        srcRect.left = nFrameOffX + nClampLeftMaybe;
        srcRect.top = nClampTopMaybe;
        srcRect.right = srcRect.left + copyW;
        srcRect.bottom = srcRect.top + copyH;

        // Non-union: the sprite goes into the scratch at its own origin, and the scratch is
        // blitted up to the cursor's screen rect through the same pPrevRect the four field
        // stores above were made through (0x4272d5 loads the POINTER back out of its slot
        // rather than lea'ing a rect, which is what pins this as pPrevRect and not a copy).
        pCursorBitmap->RestoreOverlapBlt(rectScratch, pCursorSurface, srcRect, 0);
        g_pDDrawPrimarySurface->Blt(pPrevRect, pCursorSurface, &rectScratch, 0x1000000, NULL);
    }
}

// FullscreenPopupWndPartial (the partial view for this method) now lives in WindowBase.h so
// Bootstrap.cpp can reach it too.

// FUNCTION: LOCO 0x402520 (Ghidra: FullscreenPopupWndPartial::CreateFullscreenPopupWnd --
// see the struct comment above for why this isn't this-typed to a real class.)
// Shared Create helper for full-desktop-sized singleton popup windows. Loads icon resource id
// 0x65, computes the desktop's client rect, and forwards to the inherited WindowBase::Create with
// those bounds. Real return type is unsigned char per this TU's byte-return/no-EAX-widen idiom
// (the original ends in a bare `setne al` with no EAX-wide clear).
unsigned char FullscreenPopupWndPartial::CreateFullscreenPopupWnd(HWND hwndOwnerParam)
{
    RECT rect;
    HWND hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &rect);
#ifdef LOCO_PORT
    Port_ClampDesktopRect(&rect); // PORT: desktop != screen here; see port/PortMode.h
#endif

    HICON hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCEA(0x65));
    this->hIcon = hIcon;

    if ((char)this->WindowBase::Create(0, hwndOwnerParam, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top, NULL, hIcon, 0, 0x81000000, 0) != 0) {
        return 1;
    }
    return 0;
}


// FUNCTION: LOCO 0x463600 (Ghidra: ShowFatalErrorMessageBox -- generic fatal-error
// MessageBoxA popup, resource string id 0x14a. nErrorCodeMaybe is a genuine dead-but-real cdecl
// argument: both known call sites (WindowBase::AcquireWorkSurfaceDC above, passing 0x48; an
// untranscribed sibling at 0x414be9, passing 0x49) push a real per-site constant with caller-side
// add-esp cleanup, but this function's own body never reads it -- see docs/engine-bugs.md.
//
// szUnusedMaybe is a genuine 100%-dead scratch buffer in the ORIGINAL binary too: the same
// `char buf[N] = "";` aggregate initializer as szMsg below (and InitFields's szPath), but never
// read afterward. Raw disasm confirms the original zeroes it (a rep-stos loop with no preceding
// literal-byte copy -- /O2 keeps a dead buffer's zero-fill but drops the initializer's own
// one-byte copy as dead, the same documented class as SaveGame_ScanSavFiles's unused
// szDirPrefix) immediately before repeating the exact same init for the real, used szMsg buffer.
void ShowFatalErrorMessageBox(int nErrorCodeMaybe) // sic: nErrorCodeMaybe is never read, see docs/engine-bugs.md
{
    char szUnusedMaybe[0x200] = "";

    char szMsg[0x200] = "";

#ifdef LOCO_PORT
    // PORT ONLY -- name the fatal path before the modal box blocks the process forever
    // (this function's own callers ExitProcess only AFTER MessageBoxA returns, so an
    // unattended run just hangs here with nothing in the log). Byte-neutral for the match.
    Port_Tracef("FATAL ShowFatalErrorMessageBox(%02x)\n", nErrorCodeMaybe);
#endif
    g_UIResources.LoadLocaleString(0x14a, szMsg, sizeof(szMsg));
    MessageBoxA(NULL, szMsg, "LEGO LOCO", MB_ICONWARNING);
}

// FUNCTION: LOCO 0x426b90 // TODO: idiom
// (v176: fixed the nCursorFrameCount branch to the literal `> 1`/else-zero form the
// original's disasm shows (was `< 2`/then-zero, an inverted Yoda-lesson-#2 comparison) --
// asmscore total 678581->658466, byte_diff 481->476, a real but small improvement. Tried two
// more declaration-order/residency levers per the prior session's own next-step suggestion
// (hoisting rectBltDest+srcRect, and separately rectUnion+rectUnion2, to the top of the
// cursor-aware branch to raise register pressure earlier) -- both compiled BYTE-IDENTICAL to
// the un-hoisted form, zero effect; reverted both, kept the natural declare-near-first-use
// order. Still PARKED -- structurally verified via an exhaustive hand-trace of the raw disasm,
// independently cross-checked instruction-by-instruction against the compiled candidate's own
// register/stack provenance, but the current compile still has a substantial residual (~60%
// byte_diff over the 789-byte body) dominated by unwanted tail-merging across this function's 3
// semantically-identical `Ddraw_RebindWindowClipper(g_pApp->hwndOwner); return;` exit
// points -- the ORIGINAL keeps these as 3 separate, non-merged copies (different register
// allocations at each, confirmed via raw disasm), while this candidate's simpler/lower-
// register-pressure shape lets /O2 cross-jump-merge at least 2 of them into one shared tail.
// Same "decompiler/register-allocation maze" class already documented for the sibling
// RedrawCustomCursor (below), whose own residual needed a live register dump across
// multiple sessions to close -- parked for a future session with that same tooling rather than
// exhausting the triage budget chasing more declaration-order tie-breaks; see docs/PARKED.md.
//
// v177: found and fixed a genuine missing branch, not a tie-break. The original re-reads
// pCursorBitmap (this+0x14) a SECOND time, right after the GetCursorPos call, and branches
// on it again to pick nHeight/nWidth (0/0 if NULL) -- even though it's already been proven
// non-NULL by the outer `if (pCursorBitmap == NULL || bSuppressCursorRedraw)` guard
// above. This survives /O2's redundant-branch elimination specifically because GetCursorPos is
// an opaque external call between the two reads -- the compiler can't prove `this->
// pCursorBitmap` is unchanged across it (unlike the same-basic-block literal-recheck case
// documented for PostBagFileCache::LoadIndexedFile, which needs a RANGE not EQUALITY
// compare to survive with no intervening call). Confirmed by raw disasm: `mov eax,[esi+0x14]`
// re-executes right after the GetCursorPos call, then `cmp eax,ebx; je` branches to a
// zero-both-locals arm, structurally identical to the outer guard's own check. Adding this as an
// explicit `if (pCursorBitmap != NULL) {...} else {...}` block dropped asmscore total
// 658466->522289 (byte_diff 476->369) -- a substantial, confirmed improvement, though NOT a
// full close (cc.sh's own naive DIFF count went UP, 628->640, only because the fix legitimately
// grew the candidate's own compiled length 751->762 bytes closer to the true 789 -- always judge
// this function via `asmscore.py --len 789 --dump`, never cc.sh's raw DIFF(), which compares
// against a window sized to the CANDIDATE's own length and is misleading whenever the candidate
// is still short). A follow-up hoist (moving the srcRect computation earlier, matching what
// looked like an early combined nFrameOffX+srcRect computation in the original disasm right
// after this fix) was tried and made it WORSE (byte_diff 369->425) -- reverted. Separately
// confirmed via RedrawCustomCursor's own raw disasm that this double-check pattern is
// NOT shared with that sibling -- RedrawCustomCursor has no antecedent
// pCursorBitmap==NULL early-exit before its own GetCursorPos call, so its nWidth/nHeight
// loads are unconditional; only the 4-way rectClipBounds clamp block itself is genuinely
// shared between the two functions, not this setup code. Remaining residual is still dominated
// by the documented 3-exit tail-merge + general register-allocation cascade, needing the same
// live-register-dump tooling as RedrawCustomCursor -- still PARKED.)
// Shared "commit a screen update" helper, cursor-aware like RedrawCustomCursor (reads/
// writes the same cursor-strip fields: nCursorWidth/HeightMaybe/FrameCountMaybe/
// FrameIndexMaybe/OriginXMaybe/OriginYMaybe, rectClipBounds, rectLastCursorDraw,
// nLastCursorScreenX/nLastCursorScreenY -- confirmed here to be the last raw screen-cursor X/Y, the same fields
// WindowBase_RouteMessage's WM_TIMER case reads to detect mouse movement; this function is
// a second writer of them).
//
// Optionally releases a caller-held HDC on g_pDDrawWorkSurface, then:
//   - bSkip != 0: only rebinds the ddraw window clipper onto the app's owner window and
//     returns (callers that already did their own inline invalidate).
//   - pCursorBitmap == NULL, or bSuppressCursorRedraw: a plain
//     Ddraw_BltUpdateRect(pUpdateRect ?: &rectClipBounds, hwndSelf, NULL, 1).
//   - otherwise: the SAME cursor-rect clamp-against-rectClipBounds algorithm as
//     RedrawCustomCursor, then (new logic, no RedrawCustomCursor equivalent) merges
//     pUpdateRect into the dirty region via IntersectRect/UnionRect against both the
//     freshly-clamped cursor rect and the previous frame's rectLastCursorDraw -- bailing to
//     a plain Ddraw_BltUpdateRect(pUpdateRect, ...) if neither intersects at all -- before
//     compositing the cursor bitmap via RestoreOverlapBlt and issuing a final
//     Ddraw_BltUpdateRect of the merged dirty rect (or rectClipBounds if pUpdateRect
//     was NULL) plus a direct Blt of the composited cursor scratch back onto
//     g_pDDrawWorkSurface (NOT the primary surface, unlike RedrawCustomCursor's own
//     final Blt). Every exit rebinds the clipper onto the app's owner window, except the very
//     first rebind (which targets hwndTarget instead).
//
// sic: if pUpdateRect != NULL but rectLastCursorDraw.right == 0 (the very first
// cursor redraw -- no previous frame to union against), the second UnionRect below never runs,
// yet the final Ddraw_BltUpdateRect call still unconditionally uses rectUnion2 as the dirty
// rect -- ground-truthed via raw disasm: the dirty-rect argument's stack address is identical
// whether or not the second UnionRect actually executed. Reproduced faithfully, not fixed; see
// docs/engine-bugs.md.
void WindowBase::CommitScreenUpdate(HWND hwndTarget, HDC hdcToRelease, char bSkip, RECT *pUpdateRect)
{
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    extern void Ddraw_BltUpdateRect(RECT *pRect, HWND hwnd, POINT *pScrollOffset, char bWaitMaybe); // Ddraw::Ddraw_BltUpdateRect, 0x401280, src/DDrawSurface.h
    extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4

    // Zero-initialized unconditionally here (before even the hdcToRelease check) in the
    // original disasm, despite only being read deep inside the cursor-aware branch below --
    // same "trivial-constant local hoisted above an intervening call" class documented for
    // DSound_InitDeviceAndChannelPool, see CLAUDE.md.
    int nClampTopMaybe = 0;
    int nClampLeftMaybe = 0;

    if (hdcToRelease != NULL) {
        g_pDDrawWorkSurface->ReleaseDC(hdcToRelease);
    }

    if (bSkip != 0) {
        Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
        return;
    }

    Ddraw_RebindWindowClipper(hwndTarget);

    if (pCursorBitmap == NULL || bSuppressCursorRedraw) {
        RECT *pRect;
        if (pUpdateRect == NULL) {
            pRect = &rectClipBounds;
        } else {
            pRect = pUpdateRect;
        }
#ifdef LOCO_PORT
        {   // PORT ONLY -- byte-neutral. Names the window behind an oversized present rect.
            static unsigned int nCsu = 0;
            if (++nCsu <= 6)
                Port_Tracef("csu #%u this=%08lx hwnd=%08lx upd=%d clip=%ld,%ld,%ld,%ld "
                            "client=%ld,%ld,%ld,%ld\n", nCsu, (unsigned long)this,
                            (unsigned long)hwndSelf, pUpdateRect != NULL,
                            (long)rectClipBounds.left, (long)rectClipBounds.top,
                            (long)rectClipBounds.right, (long)rectClipBounds.bottom,
                            (long)rectClient.left, (long)rectClient.top,
                            (long)rectClient.right, (long)rectClient.bottom);
        }
#endif
        Ddraw_BltUpdateRect(pRect, hwndSelf, NULL, 1);
    } else {
        POINT ptCursor;
        GetCursorPos(&ptCursor);
        nLastCursorScreenX = ptCursor.x;
        nLastCursorScreenY = ptCursor.y;
        ptCursor.x -= nCursorHotspotX;
        ptCursor.y -= nCursorHotspotY;

        int nHeight, nWidth;
        if (pCursorBitmap != NULL) {
            nHeight = nCursorHeight;
            nWidth = nCursorWidth;
        } else {
            nHeight = 0;
            nWidth = 0;
        }

        RECT rectCursor;
        rectCursor.right = nWidth + ptCursor.x;
        if (rectClipBounds.right < nWidth + ptCursor.x) {
            nWidth = rectClipBounds.right - ptCursor.x;
            rectCursor.right = rectClipBounds.right;
        }
        rectCursor.bottom = nHeight + ptCursor.y;
        if (rectClipBounds.bottom < rectCursor.bottom) {
            nHeight = rectClipBounds.bottom - ptCursor.y;
            rectCursor.bottom = rectClipBounds.bottom;
        }
        rectCursor.top = ptCursor.y;
        if (ptCursor.y < rectClipBounds.top) {
            nHeight = rectCursor.bottom - rectClipBounds.top;
            nClampTopMaybe = rectClipBounds.top - ptCursor.y;
            rectCursor.top = rectClipBounds.top;
        }
        rectCursor.left = ptCursor.x;
        if (ptCursor.x < rectClipBounds.left) {
            nClampLeftMaybe = rectClipBounds.left - ptCursor.x;
            nWidth = rectCursor.right - rectClipBounds.left;
            rectCursor.left = rectClipBounds.left;
        }

        int nFrameOffX;
        if (nCursorFrameCount > 1) {
            if (nCursorFrameCount <= nCursorFrameIndex) {
                nCursorFrameIndex = 0;
            }
            nFrameOffX = nCursorFrameIndex * nCursorWidth;
        } else {
            nFrameOffX = 0;
        }

        RECT rectUnion;
        RECT rectUnion2; // sic: can be read uninitialized, see function header comment

        if (pUpdateRect != NULL) {
            RECT rectTmp;
            if (!IntersectRect(&rectTmp, pUpdateRect, &rectCursor) &&
                !IntersectRect(&rectTmp, pUpdateRect, &rectLastCursorDraw)) {
                Ddraw_BltUpdateRect(pUpdateRect, hwndSelf, NULL, 1);
                Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
                return;
            }
            UnionRect(&rectUnion, &rectCursor, pUpdateRect);
            if (rectLastCursorDraw.right != 0) {
                UnionRect(&rectUnion2, &rectUnion, &rectLastCursorDraw);
            }
        }

        rectLastCursorDraw = rectCursor;

        RECT rectBltDest = {0, 0, nWidth, nHeight};
        pCursorSurface->Blt(&rectBltDest, g_pDDrawWorkSurface, &rectCursor, 0x1000000, NULL);

        RECT srcRect;
        srcRect.left = nFrameOffX + nClampLeftMaybe;
        srcRect.top = nClampTopMaybe;
        srcRect.right = srcRect.left + nWidth;
        srcRect.bottom = srcRect.top + nHeight;

        pCursorBitmap->RestoreOverlapBlt(rectCursor, g_pDDrawWorkSurface, srcRect, 0);

        RECT *pDirtyRect;
        if (pUpdateRect == NULL) {
            pDirtyRect = &rectClipBounds;
        } else {
            pDirtyRect = &rectUnion2;
        }
        Ddraw_BltUpdateRect(pDirtyRect, hwndSelf, NULL, 1);

        g_pDDrawWorkSurface->Blt(&rectCursor, pCursorSurface, &rectBltDest, 0x1000000, NULL);
    }

    Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
}

// FUNCTION: LOCO 0x426b70
void WindowBase::CommitRectUpdate(RECT rect)
{
    CommitScreenUpdate(hwndSelf, NULL, 0, &rect);
}

// FUNCTION: LOCO 0x426130
// The shared do-nothing virtual: a bare `ret 0x4`. It is installed at WindowBase's own vtable
// slots 0x14 AND 0x20, and PopupWndBase -- a separate hierarchy -- points its slots 0x10 and
// 0x1c at this same address, so one empty body backs four slots across both class families.
// sic: the declared stack argument is ignored entirely; the only known call site
// (SplashWnd::StartGameNetThread's worker-thread-failed path) passes a literal 0.
void WindowBase::NoOpVirtualMaybe(int nUnusedArg)
{
}

// FUNCTION: LOCO 0x425a50
// The rect utility ~19 call sites across the window/widget code reach for. `rect` keeps its own
// width and height and only moves; `outer` is read-only. Both halves compute the offset as
// `outerExtent / 2 - rectExtent / 2` rather than `(outerExtent - rectExtent) / 2` -- the two
// disagree by a pixel whenever exactly one extent is odd, and cl emits the original's four
// separate `cdq; sub eax,edx; sar eax,1` signed-halve sequences only for the two-division form.
void CenterRectInRect(RECT *outer, RECT *rect)
{
    int nWidth = rect->right - rect->left;
    rect->left = (outer->right - outer->left) / 2 - nWidth / 2 + outer->left;
    rect->right = rect->left + nWidth;
    int nHeight = rect->bottom - rect->top;
    rect->top = (outer->bottom - outer->top) / 2 - nHeight / 2 + outer->top;
    rect->bottom = rect->top + nHeight;
}

// FUNCTION: LOCO 0x425ac0 // EFFECTIVE MATCH -- 173 B vs 173, identical instruction stream and
// schedule; DIFF(12) is two pure coin-flip clusters: (1) the pDstRect->left temp rides edx in the
// original vs ebp in ours, (2) inside the nRatioY computation cl swapped the load order of the
// srcH/dstH extents (numerator-first in the original, denominator-first in ours) -- the X axis
// right above compiles numerator-first in BOTH. Probes that did not move it: compound vs plain
// assignment, interleaving the *px subtract between the ratios (much worse, 170 B), split
// decl/assignment, swapped decl order, hoisted nDstW temp. Parked as EFFECTIVE.
// Rescales the point (*px, *py) out of pSrcRect's coordinate space into pDstRect's, rewriting
// both in place. The per-axis ratio is computed as (dstExtent * 1000) / srcExtent first and
// applied as (*p * ratio) / 1000 -- the *1000 fixed-point detour is the original's own (cl
// lowers the first to a lea*125/shl-3 chain and the second to the 0x10624dd3 signed-divide
// magic). MapWnd::DrawPeerTrainDotsMaybe (0x431b30) is the known caller, mapping each peer
// train's board position into its layout-grid cell.
void MapPointBetweenRects(int *px, int *py, RECT *pSrcRect, RECT *pDstRect)
{
    int nRatioX = ((pDstRect->right - pDstRect->left) * 1000) / (pSrcRect->right - pSrcRect->left);
    int nRatioY = ((pDstRect->bottom - pDstRect->top) * 1000) / (pSrcRect->bottom - pSrcRect->top);
    *px -= pSrcRect->left;
    *py -= pSrcRect->top;
    *px = (*px * nRatioX) / 1000;
    *py = (*py * nRatioY) / 1000;
    *px += pDstRect->left;
    *py += pDstRect->top;
}
