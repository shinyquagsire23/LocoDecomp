#include "PopupWndBase.h"
#include "UIResources.h"
#include "WindowBase.h"
#include "AlbumCardWnd.h" // the five modal-capture window singletons
#include "EditCardWnd.h"
#include "MailWnd.h"
#include "MapWnd.h"
#include "SplashWnd.h"
#include "AppWindow.h"
#include "WorldBoardMaybe.h"
#include "PlacementCursorMaybe.h"

#include "DDrawSurface.h" // LocoBitmap_SetColorKey (extern "C" there), DDraw_QuerySurfaceDims

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_Tracef boot diagnostic
#endif

// FUNCTION: LOCO 0x413ab0
// The base ctor every derived popup runs as its base initializer. Everything it does is zero
// the object; the only two non-zero pieces are the resource id it is handed and the 0x20 x 0x20
// default cursor extent in Unk0x3c/Unk0x40, which the first cursor slot later overwrites with
// the descriptor's real native size. The class name is pulled from the same resource id out of
// the locale string table, straight into the embedded className buffer.
PopupWndBase::PopupWndBase(HINSTANCE hInstanceArg, UINT resourceIdArg) {
    hInstance = hInstanceArg;
    Unk0x3c = 0x20;
    Unk0x40 = 0x20;
    hwndSelf = NULL;
    hwndOwner = NULL;
    pOffscreenSurface = NULL;
    pCompositeSurface = NULL;
    cursorHover.nMaskSurfaceKey = 0;
    cursorNormal.nMaskSurfaceKey = 0;
    cursorHover.pBitmap = NULL;
    cursorNormal.pBitmap = NULL;
    bSuppressCursorRedraw = false;
    nCursorDescKey = 0;
    pActiveCursorDesc = NULL;
    nCursorFrameIndex = 0;
    bCreated = 0;
    resourceId = resourceIdArg;
    rectPrevCursor.left = 0;
    rectPrevCursor.right = 0;
    rectPrevCursor.top = 0;
    rectPrevCursor.bottom = 0;
    rectPrevCursorB.left = 0;
    rectPrevCursorB.right = 0;
    rectPrevCursorB.top = 0;
    rectPrevCursorB.bottom = 0;
    g_UIResources.LoadLocaleString(resourceIdArg, className, 0x32);
    Unk0x88 = 0;
    bShown = 0;
}

// FUNCTION: LOCO 0x413b50 (??_GPopupWndBase scalar deleting dtor -- compiler-generated around
// ~PopupWndBase() below; no source of its own)

// FUNCTION: LOCO 0x413b70
// Tears down exactly what LoadCursorSlots built up, in the same order: both cursor slots (each
// guarded on its own mask key, released through the descriptor's ReleaseRef, then all three
// members zeroed), then this popup's reference to the shared cursor composite surface, then its
// own offscreen surface. The composite-surface half is the same last-one-out release WindowBase's
// own dtor performs against the same two globals -- either hierarchy can be the last holder.
PopupWndBase::~PopupWndBase() {
    if (cursorNormal.nMaskSurfaceKey) {
        cursorNormal.pDesc->ReleaseRef();
        cursorNormal.pBitmap = NULL;
        cursorNormal.pDesc = NULL;
        cursorNormal.nMaskSurfaceKey = 0;
    }
    if (cursorHover.nMaskSurfaceKey) {
        cursorHover.pDesc->ReleaseRef();
        cursorHover.pBitmap = NULL;
        cursorHover.pDesc = NULL;
        cursorHover.nMaskSurfaceKey = 0;
    }
    if (pCompositeSurface) {
        extern IDirectDrawSurface *g_pCursorSurface;    // DAT_004fd3cc
        extern unsigned long g_dwCursorSurfaceRefCount; // DAT_004fd3d0

        g_dwCursorSurfaceRefCount--;
        if (g_dwCursorSurfaceRefCount == 0 && g_pCursorSurface != NULL) {
            g_pCursorSurface->Release();
            g_pCursorSurface = NULL;
            g_dwCursorSurfaceRefCount = 0;
        }
        pCompositeSurface = NULL;
    }
    if (pOffscreenSurface) {
        pOffscreenSurface->Release();
        pOffscreenSurface = NULL;
    }
}

// FUNCTION: LOCO 0x413d90
// Repositions the popup, keeping rectScreenBounds (screen coords) in step with rectWindow (the
// nominal size rect, whose right/bottom double as width/height here). The SetWindowPos flags are
// SWP_SHOWWINDOW|SWP_NOCOPYBITS (0x140) -- the whole client area is re-composited from the
// offscreen surface anyway, so there is nothing worth preserving to copy.
//
// The four member stores are order-INSENSITIVE here (left/top-first, right/bottom-first and a
// two-locals spelling all compile byte-identically); what IS load-bearing is that `x` is
// referenced before `y`, which is what seats x in esi and y in edi. Reading `y` first swaps the
// pair and costs DIFF(19) at identical length -- the whole residual, 26/26 instructions.
void PopupWndBase::Move(int x, int y) {
    rectScreenBounds.left = x;
    rectScreenBounds.top = y;
    rectScreenBounds.right = rectWindow.right + x;
    rectScreenBounds.bottom = rectWindow.bottom + y;
    SetWindowPos(hwndSelf, NULL, x, y, rectScreenBounds.right - x, rectScreenBounds.bottom - y,
                 SWP_SHOWWINDOW | SWP_NOCOPYBITS);
}

// FUNCTION: LOCO 0x413de0
// The base Create, shared unoverridden by all three PopupWndBase-derived singletons. Registers
// the window class (named after the class name the ctor pulled out of the locale string table),
// creates the HWND, then lazily creates the popup's own offscreen composite surface.
//
// Two details worth keeping in view: the caption is lifted off the OWNER window with
// GetWindowTextA rather than passed in, and the two RegisterClassA/CreateWindowExA failure paths
// both format the Win32 error into a LocalAlloc'd buffer and then immediately LocalFree it
// without ever displaying it -- dead diagnostics left over from development. Only the
// CreateWindowExA failure actually aborts, via a separate re-test of hwndSelf.
//
// EXACT MATCH, 701 B. Both remaining levers after the first compile (DIFF 128, insns 201/201)
// were pure STATEMENT ORDER on runs of independent member stores, and in both cases the
// original's machine STORE ORDER is NOT its source order -- so ordering the source to mirror
// the disasm is the wrong instinct here:
//   1. The prologue's five field stores. The disasm stores nClientWidth, hwndOwner, Unk0xdc,
//      Unk0xe0 and then -- past the WNDCLASSA `rep stos` -- nClientHeight, which reads as
//      "nClientHeight is last". It is not: VC5 holds nHeight in edx (the one scratch the
//      `rep stos` does not clobber) precisely BECAUSE the store is early in source and it can
//      sink it. `nClientWidth, nClientHeight, hwndOwner, Unk0xdc, Unk0xe0` is exact; the
//      disasm's own order is 6 bytes off (x and y land in the wrong registers). Only the
//      source position of the FIRST-loaded value and of hwndOwner are free -- WHOXY and WOHXY
//      both match, everything else tried costs 4-17 bytes.
//   2. The rectScreenBounds fill: left/top/right/bottom, NOT the disasm's
//      left/right/top/bottom. Referencing y before the `x + nWidth` sum is what gets y loaded
//      early into its own register, which in turn lets VC5 spend x's register on the sum
//      (`add ecx,eax`) instead of preserving both operands (`lea edx,[eax+ecx]`). That one
//      swap was worth 22 of the 28 residual bytes.
// The four rectOffscreenBoundsMaybe stores are order-insensitive by comparison (right/bottom
// before left/top here only because that is how they read alongside the pair above).
unsigned char PopupWndBase::Create(int nCmdShow, HWND hwndOwnerArg, int x, int y, int nWidth,
                                   int nHeight, HMENU hMenu, HICON hIconArg, UINT dwClassStyle,
                                   unsigned int dwStyleUnused, unsigned int dwUnkB,
                                   unsigned char bUnkC) {
    extern IDirectDraw2 *g_pDDraw2; // DAT_00485440
    // Reads the surface's real extent back out of its own GetSurfaceDesc.
    extern void DDraw_QuerySurfaceDims(IDirectDrawSurface *pSurface, unsigned short *pOutWidth,
                                       unsigned short *pOutHeight); // 0x4014e0, src/DDrawSurface.h
    // Fills *pDesc from the surface's own GetSurfaceDesc; bUpdateGlobals additionally recomputes
    // the subsystem-wide bit-mask/colour-key globals from the returned pixel format.
    extern void Ddraw_QuerySurfacePixelFormat(IDirectDrawSurface *pSurface, DDSURFACEDESC *pDesc,
                                              char bUpdateGlobals); // Ddraw::…, 0x45b9b0

    char szTitle[256];
    GetWindowTextA(hwndOwnerArg, szTitle, sizeof(szTitle));
    nClientWidth = nWidth;
    nClientHeight = nHeight;
    hwndOwner = hwndOwnerArg;
    Unk0xdc = x;
    Unk0xe0 = y;

    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_VREDRAW | CS_HREDRAW;
    if (dwClassStyle != 0) {
        wc.style = dwClassStyle;
    }
    wc.lpfnWndProc = PopupWndBase_WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)hInstance;
    wc.hIcon = hIconArg;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = className;
    if (RegisterClassA(&wc) == 0) {
        DWORD dwErr = GetLastError();
        if (dwErr != 0) {
            LPVOID pMsgBuf;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                           dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pMsgBuf, 0,
                           NULL);
            LocalFree(pMsgBuf);
        }
    }

    hwndSelf = CreateWindowExA(0, className, szTitle,
                               WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, x, y, nClientWidth,
                               nClientHeight, hwndOwner, hMenu, (HINSTANCE)hInstance, this);
    if (hwndSelf == NULL) {
        LPVOID pMsgBuf;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                       GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pMsgBuf,
                       0, NULL);
        LocalFree(pMsgBuf);
    }
    if (hwndSelf == NULL) {
        return 0;
    }

    bCreated = 1;
    RefreshClientRect();
    LoadCursorSlots();
    if (pOffscreenSurface == NULL) {
        DDSurfaceDescPadded0x7c ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        ddsd.ddsd.dwWidth = nWidth;
        ddsd.ddsd.dwHeight = nHeight;
        ddsd.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        ddsd.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        HRESULT hr = g_pDDraw2->CreateSurface(&ddsd.ddsd, &pOffscreenSurface, NULL);
        if (hr != 0) {
            return 0;
        }
        // sic: nWidth/nHeight are int parameters but 0x4014e0 writes only their low 16 bits
        // (`mov WORD PTR [eax],cx`), leaving each high half as whatever came in.
        DDraw_QuerySurfaceDims(pOffscreenSurface, (unsigned short *)&nWidth,
                               (unsigned short *)&nHeight);
        rectScreenBounds.left = x;
        rectScreenBounds.top = y;
        rectScreenBounds.right = x + nWidth;
        rectScreenBounds.bottom = y + nHeight;
        rectOffscreenBoundsMaybe.right = nWidth;
        rectOffscreenBoundsMaybe.bottom = nHeight;
        rectOffscreenBoundsMaybe.left = 0;
        rectOffscreenBoundsMaybe.top = 0;
        Ddraw_QuerySurfacePixelFormat(pOffscreenSurface, &ddsd.ddsd, 0);
        LocoBitmap_SetColorKey(pOffscreenSurface, &ddsd.ddsd);
        bCreated = 1;
    }
    ShowWindow(hwndSelf, nCmdShow);
    UpdateWindow(hwndSelf);
    Unk0x88 = bUnkC;
    return 1;
}

// FUNCTION: LOCO 0x414130
// Fills both cursor slots and binds the shared cursor composite surface. Called by Create, right
// after RefreshClientRect. Each slot is loaded by the same four steps -- resolve the UI resource
// id to a descriptor, realize its frame bitmap at the descriptor's own native size (0,0), Convert
// that bitmap to a live DirectDraw surface, then cache the surface pointer as the slot's mask
// key. Only the first slot also publishes the descriptor's native extent into Unk0x3c/Unk0x40.
//
// The 256x256 composite surface behind pCompositeSurface is process-wide, not per-popup:
// created on first use, refcounted through g_dwCursorSurfaceRefCount, and released by
// WindowBase's own teardown once the count falls back to zero.
//
// EXACT MATCH, 342 B, on the first compile -- which also settles the two 16-bit scratch locals
// below: they are the only 4 bytes of frame besides the 124-byte descriptor (0x80 total), so the
// pair is pinned by the frame size, and the `// sic:` overrun is real.
void PopupWndBase::LoadCursorSlots() {
    extern IDirectDraw2 *g_pDDraw2;                // DAT_00485440
    extern IDirectDrawSurface *g_pCursorSurface;   // DAT_004fd3cc
    extern unsigned long g_dwCursorSurfaceRefCount; // DAT_004fd3d0
    extern void DDraw_QuerySurfaceDims(IDirectDrawSurface *pSurface, unsigned short *pOutWidth,
                                       unsigned short *pOutHeight); // 0x4014e0, src/DDrawSurface.h
    extern void Ddraw_QuerySurfacePixelFormat(IDirectDrawSurface *pSurface, DDSURFACEDESC *pDesc,
                                              char bUpdateGlobals); // Ddraw::…, 0x45b9b0

    cursorNormal.pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x1400);
    if (cursorNormal.pDesc != NULL) {
        cursorNormal.pBitmap = cursorNormal.pDesc->GetOrLoadFrameBitmap(0, 0);
        cursorNormal.pBitmap->Convert();
        cursorNormal.nMaskSurfaceKey = (int)cursorNormal.pBitmap->pSurface;
        Unk0x3c = cursorNormal.pDesc->nativeWidth;
        Unk0x40 = cursorNormal.pDesc->nativeHeight;
    }
    cursorHover.pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x1403);
    if (cursorHover.pDesc != NULL) {
        cursorHover.pBitmap = cursorHover.pDesc->GetOrLoadFrameBitmap(0, 0);
        cursorHover.pBitmap->Convert();
        cursorHover.nMaskSurfaceKey = (int)cursorHover.pBitmap->pSurface;
    }
    if (g_pCursorSurface == NULL) {
        DDSurfaceDescPadded0x7c ddsd;
        // sic: two 16-bit scratch locals handed to a function that writes 32 bits through each
        // pointer -- see docs/engine-bugs.md. Harmless in practice: the results are never read,
        // and the 2 bytes each write runs over land in ddsd, which the very next call refills.
        unsigned short nSurfaceWidth;
        unsigned short nSurfaceHeight;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        ddsd.ddsd.dwWidth = 0x100;
        ddsd.ddsd.dwHeight = 0x100;
        ddsd.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        ddsd.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        g_pDDraw2->CreateSurface(&ddsd.ddsd, &g_pCursorSurface, NULL);
        DDraw_QuerySurfaceDims(g_pCursorSurface, &nSurfaceWidth, &nSurfaceHeight);
        Ddraw_QuerySurfacePixelFormat(g_pCursorSurface, &ddsd.ddsd, 0);
        LocoBitmap_SetColorKey(g_pCursorSurface, &ddsd.ddsd);
    }
    pCompositeSurface = g_pCursorSurface;
    ++g_dwCursorSurfaceRefCount;
}

// FUNCTION: LOCO 0x452170
int PopupWndBase::MeasureTextLineHeight() {
    extern char g_cMetricProbeChar;  // "W" -- single-char metric probe string

    HDC hdc = AcquireOffscreenSurfaceDC(hwndSelf);
    COLORREF oldColor = SetTextColor(hdc, 0xff5c00);
    int oldMode = SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, g_UIResources.m_hFont16);

    RECT rect;
    SetRect(&rect, 0, 0, 0xd9, 0x96);
    OffsetRect(&rect, 0x2a, 0x23);
    int nHeight = DrawTextA(hdc, &g_cMetricProbeChar, 1, &rect,
        DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX | DT_END_ELLIPSIS | DT_MODIFYSTRING);

    SelectObject(hdc, oldFont);
    SetBkMode(hdc, oldMode);
    SetTextColor(hdc, oldColor);
    CommitScreenUpdate(hwndSelf, hdc, 1);
    return nHeight;
}

// FUNCTION: LOCO 0x414bb0
HDC PopupWndBase::AcquireOffscreenSurfaceDC(HWND hwndTarget) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    extern void ShowFatalErrorMessageBox(int nErrorCodeMaybe); // 0x463600

    int nRetryCount = 0;
    HDC *pHdc = &hdcOffscreen;
    Ddraw_RebindWindowClipper(hwndTarget);

    HRESULT hr = pOffscreenSurface->GetDC(pHdc);
    while (hr != 0) {
        nRetryCount++;
        Sleep(10);
#ifdef LOCO_PORT
        // PORT ONLY -- boot diagnostic, byte-neutral for the match build.
        if (nRetryCount == 1) {
            Port_Tracef("AcquireOffscreenSurfaceDC: GetDC failed hr=%08x surf=%p hwnd=%p\n",
                        (unsigned)hr, (void *)pOffscreenSurface, (void *)hwndTarget);
        }
#endif
        if (nRetryCount > 1000) {
            ShowFatalErrorMessageBox(0x49);
            ExitProcess(1);
        }
        hr = pOffscreenSurface->GetDC(pHdc);
    }
    return *pHdc;
}

// FUNCTION: LOCO 0x414c20
// The release-side counterpart of AcquireOffscreenSurfaceDC, and the popup's whole "present"
// step: hand the GDI DC back to the offscreen DirectDraw surface, then Blt that surface onto the
// primary surface at this window's current screen position (rectWindow translated by
// ClientToScreen). When a software cursor is active (nCursorDescKey != 0 and redraw is not
// suppressed) the cursor sprite is STAMPED INTO the offscreen surface first -- with the region it
// covers saved off to pCompositeSurface beforehand -- and restored from that saved copy
// immediately after the present, so the offscreen buffer is left cursor-free for the next
// caller. That save/stamp/present/restore ordering is what makes pOffscreenSurface (not
// pCompositeSurface, as in the RedrawSoftwareCursor sibling) the destination of the masked
// sprite Blt; the two functions composite in opposite directions on purpose.
//
// EXACT (v554). Stack frame 0x60: ptCursor@0x10, ptOrigin@0x18, rectCursor@0x20, srcRect@0x30,
// rectBltDest@0x40, rectBltSrc@0x50, rectScreenDest@0x60.
//
// This was parked as EFFECTIVE at DIFF(62)/align 24 for many sessions on a diagnosis that has
// turned out to be wrong: the residual was read as "scratch-register naming plus one scheduling
// slot" in the two duplicated ClientToScreen/SetRect/Blt present blocks. It was not. Two
// unrelated changes closed it, in this order:
//   a. Giving this TU the five modal-capture window headers (see the concrete-typed
//      g_pMailWnd/... block in RebindClipperToActiveScreen below, which fixed a real
//      wrong-symbol bug) moved the whole function from DIFF(62)/align 24 to DIFF(2)/align 0 --
//      i.e. to a full 254/254 instruction pairing with only TWO bytes left. That is the
//      documented declaration-count dial, and it landed here as a side effect, not by design.
//   b. With the noise gone, those two bytes were legible: `mov [esp+d],ebx` twice with the two
//      displacements SWAPPED (0x4c/0x48 vs 0x48/0x4c), both storing the same zero. The original
//      zeroes rectBltDest.TOP BEFORE .LEFT -- lever 6 below.
// Lesson worth keeping: an align-24 residual can hide a two-byte source-order fact, and the
// prose diagnosis of a stale park is not evidence. Re-derive after any dial move.
//
// SIX levers landed and are load-bearing -- do not undo any of them:
//   1. `rectPrevCursor = rectCursor;` as a whole-struct assignment, NOT four field stores. This
//      alone went DIFF(462) -> DIFF(66). The tell was `lea ebx,[esi+0x68]` plus RELOADS of
//      rectCursor.left/top from their stack slots while `right` came straight from a live
//      register -- VC5's struct-copy idiom with copy propagation folding the one load it could.
//   2. The else-arm present block must sit in its OWN nested scope (hence `} else {` after a
//      `return`-terminated if). Written as sibling scopes VC5 overlaps their POINT/RECT with the
//      cursor arm's; written with the second block directly in the enclosing scope its locals
//      outlive the first block and the frame grows 8 bytes, shifting every slot.
//   3. Both clamps assign the clamped edge FIRST and derive the extent from the field
//      (`rectCursor.bottom = ...; nHeight = rectCursor.bottom - ptCursor.y;`). Deriving from
//      rectScreenBounds instead costs an extra mov per clamp.
//   4. Both clamp predicates read `rectCursor.<edge> > rectScreenBounds.<edge>`, not the
//      reversed spelling the RedrawSoftwareCursor sibling uses -- operand order picks the
//      jle/jge polarity.
//   5. Each Blt HRESULT is captured in a NAMED LOCAL and the local is tested, not the call
//      expression inline. Testing inline emits `test eax,eax`; through a local VC5 instead
//      compares against its register-allocated constant 0 (`cmp eax,ebx` / `cmp eax,ebp` -- the
//      original even swaps WHICH callee-saved register holds the zero between the two blocks).
//      Same lever closed EditCardWnd::BeginEdit (0x416b80) outright; see docs/CODEGEN.md.
//   6. `rectBltDest.top = 0;` is written BEFORE `rectBltDest.left = 0;`. VC5 emits these two
//      zero-stores in source order, so the natural left-then-top spelling is two bytes wrong
//      (and only those two). The sibling .right/.bottom pair below is NOT order-sensitive --
//      the scheduler reorders it to bottom-then-right either way.
// RULED OUT (all byte-identical no-ops, do not retry): swapping the pSurf / pRectWindow
// declaration order, and swapping the SetRect addend order to
// `ptOrigin.x + pRectWindow->left` etc.
void PopupWndBase::CommitScreenUpdate(HWND hwndTarget, HDC hdcToRelease, char bSkipRedraw) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    extern IDirectDrawSurface *g_pDDrawPrimarySurface; // DAT_004fd3c0

    if (hdcToRelease != NULL) {
        pOffscreenSurface->ReleaseDC(hdcToRelease);
    }
    if (bSkipRedraw == 0) {
        Ddraw_RebindWindowClipper(hwndTarget);
        if (nCursorDescKey != 0 && bSuppressCursorRedraw == 0) {
            nLastCursorScreenX = -1;
            nLastCursorScreenY = -1;

            POINT ptCursor;
            GetCursorPos(&ptCursor);

            // sic: pDesc is dereferenced for its hotspot pair BEFORE the null check just below --
            // the original really does read [pDesc+0x32]/[pDesc+0x34] first and only then
            // `test edi,edi`. A null descriptor here faults; the guarded width/height read that
            // follows is the only half that was written defensively.
            CursorDesc *pDesc = pActiveCursorDesc;
            ptCursor.x -= pDesc->hotspotX;
            ptCursor.y -= pDesc->hotspotY;

            int nWidth, nHeight;
            if (pDesc != NULL) {
                nHeight = pDesc->nativeHeight;
                nWidth = pDesc->nativeWidth;
            } else {
                nHeight = 0;
                nWidth = 0;
            }

            // The cursor's screen rect, clamped against the popup's own bounds on the right and
            // bottom edges only (unlike RedrawSoftwareCursor, which clamps all four and keeps the
            // left/top overshoot in nClampLeftMaybe/nClampTopMaybe -- there is no such source-side
            // offset here, so the sprite is never partially clipped at the top-left).
            RECT rectCursor;
            rectCursor.left = ptCursor.x;
            rectCursor.right = nWidth + ptCursor.x;
            rectCursor.top = ptCursor.y;
            rectCursor.bottom = nHeight + ptCursor.y;
            if (rectCursor.right > rectScreenBounds.right) {
                rectCursor.right = rectScreenBounds.right;
                nWidth = rectCursor.right - ptCursor.x;
            }
            if (rectCursor.bottom > rectScreenBounds.bottom) {
                rectCursor.bottom = rectScreenBounds.bottom;
                nHeight = rectCursor.bottom - ptCursor.y;
            }
            rectPrevCursor = rectCursor;

            RECT srcRect;
            if (pDesc->nTotalFrameCount > 1) {
                if (nCursorFrameIndex >= pDesc->nTotalFrameCount) {
                    nCursorFrameIndex = 0;
                }
                srcRect.left = pDesc->nativeWidth * nCursorFrameIndex;
                srcRect.right = srcRect.left + nWidth;
            } else {
                srcRect.left = 0;
                srcRect.right = nWidth;
            }
            srcRect.top = 0;
            srcRect.bottom = nHeight;

            RECT rectBltDest;
            rectBltDest.top = 0;
            rectBltDest.left = 0;
            rectBltDest.right = nWidth;
            rectBltDest.bottom = nHeight;

            // rectBltSrc is the cursor rect in offscreen-surface (window-local) coordinates: it is
            // the SOURCE of the background save and the DESTINATION of both the sprite stamp and
            // the restore.
            RECT rectBltSrc;
            CopyRect(&rectBltSrc, &rectCursor);
            OffsetRect(&rectBltSrc, -rectScreenBounds.left, -rectScreenBounds.top);
            pCompositeSurface->Blt(&rectBltDest, pOffscreenSurface, &rectBltSrc, 0x1000000, NULL);
            pOffscreenSurface->Blt(&rectBltSrc, (IDirectDrawSurface *)nCursorDescKey, &srcRect,
                                   0x1008000, NULL);

            IDirectDrawSurface *pSurf = pOffscreenSurface;
            RECT *pRectWindow = &rectWindow;
            POINT ptOrigin;
            ptOrigin.x = 0;
            ptOrigin.y = 0;
            ClientToScreen(hwndSelf, &ptOrigin);
            RECT rectScreenDest;
            SetRect(&rectScreenDest, pRectWindow->left + ptOrigin.x, pRectWindow->top + ptOrigin.y,
                    pRectWindow->right + ptOrigin.x, pRectWindow->bottom + ptOrigin.y);
            HRESULT hr = g_pDDrawPrimarySurface->Blt(&rectScreenDest, pSurf, pRectWindow,
                                                     0x1000000, NULL);
            if (hr != 0) {
                OutputDebugStringA("");
            }
            pOffscreenSurface->Blt(&rectBltSrc, pCompositeSurface, &rectBltDest, 0x1000000, NULL);
            PopupWndBase_RebindClipperToActiveScreen();
            return;
        } else {
            IDirectDrawSurface *pSurf = pOffscreenSurface;
            RECT *pRectWindow = &rectWindow;
            POINT ptOrigin;
            ptOrigin.x = 0;
            ptOrigin.y = 0;
            ClientToScreen(hwndSelf, &ptOrigin);
            RECT rectScreenDest;
            SetRect(&rectScreenDest, pRectWindow->left + ptOrigin.x, pRectWindow->top + ptOrigin.y,
                    pRectWindow->right + ptOrigin.x, pRectWindow->bottom + ptOrigin.y);
            HRESULT hr = g_pDDrawPrimarySurface->Blt(&rectScreenDest, pSurf, pRectWindow,
                                                     0x1000000, NULL);
            if (hr != 0) {
                OutputDebugStringA("");
            }
        }
    }
    PopupWndBase_RebindClipperToActiveScreen();
}

// FUNCTION: LOCO 0x414340 // EFFECTIVE MATCH -- 150/152 bytes, 52/53 insns (asmscore total
// 40233, byte_diff 13). Content-complete: the outer key-change guard (a real stack-resident
// bKeepRectsMaybe local, not a recomputed expression -- see below), the 2-RECT reset, and the
// conditional redraw chain all transcribed and byte-verified against raw disasm. Residual is a
// provably-dead redundant recheck (`if (nOldKey==nKey) goto setRects;`, always true given
// nothing between it and the outer `nOldKey==nKey` branch can change either value) that VC5
// eliminates into a bare `jmp` in this compile but kept as a live `cmp;je` in the original --
// tried both a cached local and fresh repeated `this->` field reads for the two comparisons,
// neither stopped the elimination (unlike the LoadIndexedFileMaybe precedent, both the
// surviving original instruction AND this recompile already agree on the `je`/equality FORM,
// so that lesson's range-vs-equality lever doesn't apply here). The second residual this note
// used to carry -- an "unrelated dead `mov ecx,esi`" before the argument-less FUN_00414ef0()
// call -- was NOT dead and NOT unrelated: it is the `this` pass to a __thiscall member that
// happens never to read `this` (v362 lever 3). Fixed by dropping `static` from
// PopupWndBase_RebindClipperToActiveScreen's declaration; only the elimination residual is
// left. See docs/PARKED.md.
void PopupWndBase::SetCursorDesc(int nKey, CursorDesc *pDesc, char bResetRects, char bRedraw) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    // A real stack-resident local, NOT a recomputed !bResetRects expression -- confirmed
    // via raw disasm: the original stores this to a stack slot at function entry (true) and
    // conditionally clears it inline inside the bResetRects branch below, then RELOADS the
    // stack slot (not a fresh comparison) at the RedrawSoftwareCursor call site.
    int nOldKey = nCursorDescKey;
    char bKeepRectsMaybe = 1;

    if (nOldKey == nKey) {
        if (nKey == 0) {
            return;
        }
        if (nOldKey == nKey) goto setRects;
    }
    nCursorDescKey = nKey;
    pActiveCursorDesc = pDesc;
    nCursorFrameIndex = 0;

setRects:
    if (bResetRects != 0) {
        bKeepRectsMaybe = 0;
        rectPrevCursor.left = 0;
        rectPrevCursor.right = 0;
        rectPrevCursor.top = 0;
        rectPrevCursor.bottom = 0;
        rectPrevCursorB.left = 0;
        rectPrevCursorB.right = 0;
        rectPrevCursorB.top = 0;
        rectPrevCursorB.bottom = 0;
    }
    if (bRedraw != 0 && bSuppressCursorRedraw == 0) {
        Ddraw_RebindWindowClipper(hwndSelf);
        RedrawSoftwareCursor(bKeepRectsMaybe);
        PopupWndBase_RebindClipperToActiveScreen();
        if (Unk0x88 != 0) {
            RedrawSoftwareCursorOverBoard(bResetRects);
        }
    }
}

// FUNCTION: LOCO 0x414fb0
// Software-cursor redraw: background-restore + masked-composite pipeline. See the header's own
// plate comment for the algorithm outline; mirrors the already-transcribed sibling
// WindowBase::CommitScreenUpdate/RedrawCustomCursor's cursor-clamp/union-dirty-rect shape
// (src/WindowBase.cpp) closely enough that its established naming (rectCursor, nClampLeftMaybe/
// nClampTopMaybe, bUnionModeMaybe, srcRect) is reused here for consistency, though the actual
// compositing differs: this uses a raw DirectDraw surface Blt (DDBLT_KEYSRC) through
// nCursorDescKey reinterpreted as a surface pointer, not the LocoBitmap RestoreOverlapBlt
// method used by that sibling.
// v196: found and fixed a genuine CONTENT gap via a full raw-disasm stack-slot trace (not just a
// codegen tie-break) -- the original unconditionally zeroes `rectBltDest.left` right after the
// two early-return checks (real addr 0x4151ee, `mov [esp+0x4c],eax` with eax=0), which our prior
// transcription never did (leaving IntersectRect's raw intersection-left value there instead,
// silently wrong input to the non-union branch's `nSrcLeftBase = rectBltDest.left`). Matches the
// sibling RedrawSoftwareCursorOverBoard, which already had this line. Frame-size residual
// (candidate 0x78 vs original's 0x7c, still 4 bytes/one dword short) is UNCHANGED by this fix --
// asmscore total 496523, byte_diff 503/1166 (was 499 pre-fix; the extra store shifts downstream
// register allocation slightly worse by raw byte count, but this is a correctness fix independent
// of the byte-match score, kept regardless). Also ruled out this session: reordering rectCursor's
// left/right/bottom/top field-assignment order (a 3rd variant beyond v194's own already-tried
// orders) -- confirmed inert, byte-identical recompile. A full stack-slot map of the whole
// function (see this file's git history / a scratch trace) did not conclusively find the extra
// persistent slot the original's 0x7c frame needs; one unexplained single WRITE-no-READ at
// esp+0x70 (real addr 0x4152e1, inside the bUnionModeMaybe branch) is a candidate for a future
// session, not yet confirmed. Do not re-try the field-reorder lever again.
void PopupWndBase::RedrawSoftwareCursor(char param_1) {
    extern IDirectDrawSurface *g_pDDrawPrimarySurface; // DAT_004fd3c0

    // Hoisted above GetCursorPos to match the original's own instruction order -- all 4 share
    // one zeroed register with no dependencies to block the hoist (same class as
    // DSound_InitDeviceAndChannelPool's WAVEFORMATEX field-init hoist, see CLAUDE.md).
    int nHeight = 0;
    char bUnionModeMaybe = 0;
    int nClampTopMaybe = 0;
    int nClampLeftMaybe = 0;

    POINT ptCursor;
    GetCursorPos(&ptCursor);

    int nWidth;
    CursorDesc *pDesc = pActiveCursorDesc;
    if (pDesc != NULL) {
        ptCursor.x -= pDesc->hotspotX;
        ptCursor.y -= pDesc->hotspotY;
        nHeight = pDesc->nativeHeight;
        nWidth = pDesc->nativeWidth;
    } else {
        nWidth = 0;
    }

    RECT rectCursor;
    rectCursor.left = ptCursor.x;
    rectCursor.right = nWidth + ptCursor.x;
    rectCursor.top = ptCursor.y;
    rectCursor.bottom = nHeight + ptCursor.y;

    if (ptCursor.x < rectScreenBounds.left) {
        nClampLeftMaybe = rectScreenBounds.left - ptCursor.x;
        nWidth = rectCursor.right - rectScreenBounds.left;
        rectCursor.left = rectScreenBounds.left;
    }
    if (ptCursor.y < rectScreenBounds.top) {
        nClampTopMaybe = rectScreenBounds.top - ptCursor.y;
        nHeight = rectCursor.bottom - rectScreenBounds.top;
        rectCursor.top = rectScreenBounds.top;
    }
    if (rectScreenBounds.right < rectCursor.right) {
        nWidth = rectScreenBounds.right - rectCursor.left;
        rectCursor.right = rectScreenBounds.right;
    }
    if (rectScreenBounds.bottom < rectCursor.bottom) {
        nHeight = rectScreenBounds.bottom - rectCursor.top;
        rectCursor.bottom = rectScreenBounds.bottom;
    }

    RECT *pPrevRect = &rectPrevCursor;
    RECT rectUnion;
    UnionRect(&rectUnion, pPrevRect, &rectCursor);

    int nUnionW = rectUnion.right - rectUnion.left;
    if (nUnionW < 0) return;
    int nUnionH = rectUnion.bottom - rectUnion.top;
    if (nUnionH < 0) return;

    if (nCursorDescKey != 0 && rectPrevCursor.right != 0 && param_1 != 0 &&
        bSuppressCursorRedraw == 0 && nUnionW < 0x100 && nUnionH < 0x100) {
        bUnionModeMaybe = 1;
    }

    rectUnion.right += 4;
    rectUnion.left -= 4;
    rectUnion.bottom += 4;
    rectUnion.top -= 4;
    if (rectScreenBounds.right < rectUnion.right) {
        rectUnion.right = rectScreenBounds.right;
    }
    if (rectScreenBounds.bottom < rectUnion.bottom) {
        rectUnion.bottom = rectScreenBounds.bottom;
    }
    if (rectUnion.top < rectScreenBounds.top) {
        rectUnion.top = rectScreenBounds.top;
    }
    if (rectUnion.left < rectScreenBounds.left) {
        rectUnion.left = rectScreenBounds.left;
    }

    if (rectPrevCursor.right != 0 && param_1 != 0 && !bUnionModeMaybe) {
        RECT rectRestoreSrc;
        CopyRect(&rectRestoreSrc, pPrevRect);
        OffsetRect(&rectRestoreSrc, -rectScreenBounds.left, -rectScreenBounds.top);
        g_pDDrawPrimarySurface->Blt(pPrevRect, pOffscreenSurface, &rectRestoreSrc, 0x1000000, NULL);
    }

    RECT rectBltDest;
    IntersectRect(&rectBltDest, &rectCursor, &rectScreenBounds);
    nLastCursorScreenX = -1;
    nLastCursorScreenY = -1;

    if (nCursorDescKey == 0) return;
    if (bSuppressCursorRedraw != 0) return;

    rectBltDest.left = 0;
    pPrevRect->left = rectCursor.left;
    pPrevRect->top = rectCursor.top;
    pPrevRect->right = rectCursor.right;
    pPrevRect->bottom = rectCursor.bottom;

    IDirectDrawSurface *pMaskSurf = (IDirectDrawSurface *)nCursorDescKey;

    if (bUnionModeMaybe) {
        int copyW = rectUnion.right - rectUnion.left;
        int copyH = rectUnion.bottom - rectUnion.top;
        rectBltDest.top = 0;
        rectBltDest.right = copyW;
        rectBltDest.bottom = copyH;

        RECT rectBltSrc;
        CopyRect(&rectBltSrc, &rectUnion);
        OffsetRect(&rectBltSrc, -rectScreenBounds.left, -rectScreenBounds.top);
        pCompositeSurface->Blt(&rectBltDest, pOffscreenSurface, &rectBltSrc, 0x1000000, NULL);

        int nFrameOffX;
        if (pActiveCursorDesc->nTotalFrameCount < 2) {
            nFrameOffX = 0;
        } else {
            if (pActiveCursorDesc->nTotalFrameCount <= nCursorFrameIndex) {
                nCursorFrameIndex = 0;
            }
            nFrameOffX = pActiveCursorDesc->nativeWidth * nCursorFrameIndex;
        }

        RECT srcRect;
        srcRect.left = nFrameOffX + nClampLeftMaybe;
        srcRect.top = nClampTopMaybe;
        srcRect.right = srcRect.left + nWidth;
        srcRect.bottom = srcRect.top + nHeight;

        RECT rectSpriteDest;
        rectSpriteDest.left = pPrevRect->left - rectUnion.left;
        rectSpriteDest.top = pPrevRect->top - rectUnion.top;
        rectSpriteDest.right = rectSpriteDest.left + nWidth;
        rectSpriteDest.bottom = rectSpriteDest.top + nHeight;

        pCompositeSurface->Blt(&rectSpriteDest, pMaskSurf, &srcRect, 0x1008000, NULL);
        g_pDDrawPrimarySurface->Blt(&rectUnion, pCompositeSurface, &rectBltDest, 0x1000000, NULL);
    } else {
        rectBltDest.top = 0;
        rectBltDest.right = nWidth;
        rectBltDest.bottom = nHeight;

        RECT rectBltSrc;
        CopyRect(&rectBltSrc, &rectUnion);
        OffsetRect(&rectBltSrc, -rectScreenBounds.left, -rectScreenBounds.top);
        pCompositeSurface->Blt(&rectBltDest, pOffscreenSurface, &rectBltSrc, 0x1000000, NULL);

        int nSrcLeftBase, nSrcTopBase, nSrcRightBase, nSrcBottomBase;
        if (pActiveCursorDesc->nTotalFrameCount > 1) {
            if (pActiveCursorDesc->nTotalFrameCount <= nCursorFrameIndex) {
                nCursorFrameIndex = 0;
            }
            nSrcLeftBase = pActiveCursorDesc->nativeWidth * nCursorFrameIndex;
            nSrcTopBase = 0;
            nSrcRightBase = nWidth;
            nSrcBottomBase = nHeight;
        } else {
            nSrcLeftBase = rectBltDest.left;
            nSrcTopBase = rectBltDest.top;
            nSrcRightBase = rectBltDest.right;
            nSrcBottomBase = rectBltDest.bottom;
        }

        RECT srcRect;
        srcRect.left = nSrcLeftBase + nClampLeftMaybe;
        srcRect.top = nSrcTopBase + nClampTopMaybe;
        srcRect.right = nSrcRightBase + nClampLeftMaybe;
        srcRect.bottom = nSrcBottomBase + nClampTopMaybe;

        pCompositeSurface->Blt(&rectBltDest, pMaskSurf, &srcRect, 0x1008000, NULL);
        g_pDDrawPrimarySurface->Blt(pPrevRect, pCompositeSurface, &rectBltDest, 0x1000000, NULL);
    }
}

// FUNCTION: LOCO 0x415440
// Map/board-view sibling of RedrawSoftwareCursor -- see the header's own plate comment for
// the 3 concrete differences (rectPrevCursorB, template screen rect + board-scroll offset,
// shared g_pDDrawWorkSurface). CONTENT-COMPLETE, NOT YET BYTE-MATCHED.
void PopupWndBase::RedrawSoftwareCursorOverBoard(char param_1) {
    extern IDirectDrawSurface *g_pDDrawPrimarySurface; // DAT_004fd3c0
    extern IDirectDrawSurface *g_pDDrawWorkSurface;    // DAT_004fd3c4
    extern RECT g_rectAppClientBounds;   // 0x485220 (tagRECT_00485220) -- static template
                                                 // screen-bounds rect, offset by the board's own
                                                 // scroll position when g_bBoardScrollFlag.
    extern unsigned char g_bBoardScrollFlag; // DAT_00485210

    char bUnionModeMaybe = 0;
    int nClampTopMaybe = 0;
    int nClampLeftMaybe = 0;

    POINT ptCursor;
    GetCursorPos(&ptCursor);

    int nWidth, nHeight = 0;
    CursorDesc *pDesc = pActiveCursorDesc;
    if (pDesc != NULL) {
        ptCursor.x -= pDesc->hotspotX;
        ptCursor.y -= pDesc->hotspotY;
        nHeight = pDesc->nativeHeight;
        nWidth = pDesc->nativeWidth;
    } else {
        nWidth = 0;
    }

    RECT rectCursor;
    rectCursor.left = ptCursor.x;
    rectCursor.top = ptCursor.y;
    rectCursor.right = nWidth + ptCursor.x;
    rectCursor.bottom = nHeight + ptCursor.y;

    RECT rectLocalBounds;
    if (g_bBoardScrollFlag) {
        CopyRect(&rectLocalBounds, &g_rectAppClientBounds);
        OffsetRect(&rectLocalBounds, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
    } else {
        CopyRect(&rectLocalBounds, &g_rectAppClientBounds);
    }

    if (rectCursor.left < rectLocalBounds.left) {
        nClampLeftMaybe = rectLocalBounds.left - rectCursor.left;
        rectCursor.left = rectLocalBounds.left;
        nWidth = rectCursor.right - rectLocalBounds.left;
    }
    if (rectCursor.top < rectLocalBounds.top) {
        nClampTopMaybe = rectLocalBounds.top - rectCursor.top;
        rectCursor.top = rectLocalBounds.top;
        nHeight = rectCursor.bottom - rectLocalBounds.top;
    }
    if (rectLocalBounds.right < rectCursor.right) {
        rectCursor.right = rectLocalBounds.right;
        nWidth = rectLocalBounds.right - rectCursor.left;
    }
    if (rectLocalBounds.bottom < rectCursor.bottom) {
        rectCursor.bottom = rectLocalBounds.bottom;
        nHeight = rectLocalBounds.bottom - rectCursor.top;
    }

    RECT *pPrevRect = &rectPrevCursorB;
    RECT rectUnion;
    UnionRect(&rectUnion, pPrevRect, &rectCursor);

    if (nCursorDescKey != 0 && rectPrevCursorB.right != 0 && param_1 != 0 &&
        bSuppressCursorRedraw == 0 && rectUnion.right - rectUnion.left < 0x100 &&
        rectUnion.bottom - rectUnion.top < 0x100) {
        bUnionModeMaybe = 1;
    }

    if (rectLocalBounds.right < rectUnion.right) {
        rectUnion.right = rectLocalBounds.right;
    }
    if (rectLocalBounds.bottom < rectUnion.bottom) {
        rectUnion.bottom = rectLocalBounds.bottom;
    }
    if (rectUnion.top < rectLocalBounds.top) {
        rectUnion.top = rectLocalBounds.top;
    }
    if (rectUnion.left < rectLocalBounds.left) {
        rectUnion.left = rectLocalBounds.left;
    }

    if (rectPrevCursorB.right != 0 && param_1 != 0 && !bUnionModeMaybe) {
        RECT rectRestoreSrc;
        CopyRect(&rectRestoreSrc, pPrevRect);
        OffsetRect(&rectRestoreSrc, -rectLocalBounds.left, -rectLocalBounds.top);
        g_pDDrawPrimarySurface->Blt(pPrevRect, g_pDDrawWorkSurface, &rectRestoreSrc, 0x1000000, NULL);
    }

    nLastCursorScreenX = -1;
    nLastCursorScreenY = -1;

    if (nCursorDescKey == 0) return;
    if (bSuppressCursorRedraw != 0) return;

    pPrevRect->left = rectCursor.left;
    pPrevRect->top = rectCursor.top;
    pPrevRect->right = rectCursor.right;
    pPrevRect->bottom = rectCursor.bottom;

    IDirectDrawSurface *pMaskSurf = (IDirectDrawSurface *)nCursorDescKey;

    RECT rectBltDest;
    rectBltDest.left = 0;

    if (bUnionModeMaybe) {
        int copyW = rectUnion.right - rectUnion.left;
        int copyH = rectUnion.bottom - rectUnion.top;
        rectBltDest.top = 0;
        rectBltDest.right = copyW;
        rectBltDest.bottom = copyH;

        RECT rectBltSrc;
        CopyRect(&rectBltSrc, &rectUnion);
        OffsetRect(&rectBltSrc, -rectLocalBounds.left, -rectLocalBounds.top);
        pCompositeSurface->Blt(&rectBltDest, g_pDDrawWorkSurface, &rectBltSrc, 0x1000000, NULL);

        int nFrameOffX;
        if (pActiveCursorDesc->nTotalFrameCount < 2) {
            nFrameOffX = 0;
        } else {
            if (pActiveCursorDesc->nTotalFrameCount <= nCursorFrameIndex) {
                nCursorFrameIndex = 0;
            }
            nFrameOffX = pActiveCursorDesc->nativeWidth * nCursorFrameIndex;
        }

        RECT srcRect;
        srcRect.left = nFrameOffX + nClampLeftMaybe;
        srcRect.top = nClampTopMaybe;
        srcRect.right = srcRect.left + nWidth;
        srcRect.bottom = srcRect.top + nHeight;

        RECT rectSpriteDest;
        rectSpriteDest.left = pPrevRect->left - rectUnion.left;
        rectSpriteDest.top = pPrevRect->top - rectUnion.top;
        rectSpriteDest.right = rectSpriteDest.left + nWidth;
        rectSpriteDest.bottom = rectSpriteDest.top + nHeight;

        pCompositeSurface->Blt(&rectSpriteDest, pMaskSurf, &srcRect, 0x1008000, NULL);
        g_pDDrawPrimarySurface->Blt(&rectUnion, pCompositeSurface, &rectBltDest, 0x1000000, NULL);
    } else {
        rectBltDest.top = 0;
        rectBltDest.right = nWidth;
        rectBltDest.bottom = nHeight;

        RECT rectBltSrc;
        CopyRect(&rectBltSrc, &rectUnion);
        OffsetRect(&rectBltSrc, -rectLocalBounds.left, -rectLocalBounds.top);
        pCompositeSurface->Blt(&rectBltDest, g_pDDrawWorkSurface, &rectBltSrc, 0x1000000, NULL);

        int nSrcLeftBase, nSrcTopBase, nSrcRightBase, nSrcBottomBase;
        if (pActiveCursorDesc->nTotalFrameCount > 1) {
            if (pActiveCursorDesc->nTotalFrameCount <= nCursorFrameIndex) {
                nCursorFrameIndex = 0;
            }
            nSrcLeftBase = pActiveCursorDesc->nativeWidth * nCursorFrameIndex;
            nSrcTopBase = 0;
            nSrcRightBase = nWidth;
            nSrcBottomBase = nHeight;
        } else {
            nSrcLeftBase = rectBltDest.left;
            nSrcTopBase = rectBltDest.top;
            nSrcRightBase = rectBltDest.right;
            nSrcBottomBase = rectBltDest.bottom;
        }

        RECT srcRect;
        srcRect.left = nSrcLeftBase + nClampLeftMaybe;
        srcRect.top = nSrcTopBase + nClampTopMaybe;
        srcRect.right = nSrcRightBase + nClampLeftMaybe;
        srcRect.bottom = nSrcBottomBase + nClampTopMaybe;

        pCompositeSurface->Blt(&rectBltDest, pMaskSurf, &srcRect, 0x1008000, NULL);
        g_pDDrawPrimarySurface->Blt(pPrevRect, pCompositeSurface, &rectBltDest, 0x1000000, NULL);
    }
}

// FUNCTION: LOCO 0x414ef0
void PopupWndBase::PopupWndBase_RebindClipperToActiveScreen() {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    // Concrete types, not WindowBase * -- each of these is one global with one true type, and
    // every other TU spells them this way (g_pMapWnd is DEFINED as MapWnd * in src/MapWnd.cpp).
    // A `WindowBase *` spelling mangles to a DIFFERENT symbol that nothing defines; all five
    // derive singly from WindowBase, so the base-member reads below are unaffected.
    extern MailWnd *g_pMailWnd;           // DAT_004fd37c
    extern AlbumCardWnd *g_pAlbumCardWnd; // DAT_004fd384
    extern EditCardWnd *g_pEditCardWnd;   // DAT_004fd380
    extern MapWnd *g_pMapWnd;             // DAT_004fd388
    extern SplashWnd *g_pSplashWnd;       // DAT_004fd378

    if (g_pMailWnd != 0 && g_pMailWnd->bModalCaptureActive != 0) {
        Ddraw_RebindWindowClipper(g_pMailWnd->hwndSelf);
        return;
    }
    if (g_pAlbumCardWnd != 0 && g_pAlbumCardWnd->bModalCaptureActive != 0) {
        Ddraw_RebindWindowClipper(g_pAlbumCardWnd->hwndSelf);
        return;
    }
    if (g_pEditCardWnd != 0 && g_pEditCardWnd->bModalCaptureActive != 0) {
        Ddraw_RebindWindowClipper(g_pEditCardWnd->hwndSelf);
        return;
    }
    if (g_pMapWnd != 0 && g_pMapWnd->bModalCaptureActive != 0) {
        Ddraw_RebindWindowClipper(g_pMapWnd->hwndSelf);
        return;
    }
    if (g_pSplashWnd != 0 && g_pSplashWnd->bModalCaptureActive != 0) {
        Ddraw_RebindWindowClipper(g_pSplashWnd->hwndSelf);
        return;
    }
    Ddraw_RebindWindowClipper(g_pApp->hwndOwner);
}

// FUNCTION: LOCO 0x4140a0
void PopupWndBase::RefreshClientRect() {
    if (bCreated == 0) {
        return;
    }
    GetClientRect(hwndSelf, &rectClient);
    nClientWidth = rectClient.right - rectClient.left;
    nClientHeight = rectClient.bottom - rectClient.top;
    rectWindow.left = rectClient.left;
    rectWindow.top = rectClient.top;
    rectWindow.right = rectClient.right;
    rectWindow.bottom = rectClient.bottom;
    nWindowWidth = rectWindow.right - rectWindow.left;
    nWindowHeight = rectWindow.bottom - rectWindow.top;
}

// FUNCTION: LOCO 0x414290 // CONTENT-COMPLETE, NOT YET BYTE-MATCHED (2026-07-18, v199): asmscore
// total 92472, byte_diff 42/163, insns 54/56. The inner `if (bRelease != 0) return;` recheck
// (provably dead -- the outer `if (bRelease != 0)` already established this) is written out
// explicitly per the raw disasm's own `test al,al; jne` at that exact position (not collapsed to
// a bare `return;`) -- tried both forms, BYTE-IDENTICAL either way, so VC5 eliminates this
// specific dead recheck regardless of phrasing (unlike the LoadIndexedFileMaybe precedent's
// range-vs-equality lever, which doesn't apply here). Residual is 2 already-documented
// intrinsic classes: (1) FIXED in v362, not intrinsic after all -- the "dead `mov ecx,esi`"
// before the PopupWndBase_RebindClipperToActiveScreen() call was that member's `this` pass
// (lever 3); the callee is no longer declared `static`; (2) a register-caching choice for
// hwndSelf/GetCapture() results (original caches into a callee-saved register across the
// intervening call; tried forcing an explicit local, byte-identical either way -- see Show's
// own comment below for the same experiment). See docs/PARKED.md.
void PopupWndBase::SetModalCapture(char bRelease) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    if (bRelease != 0) {
        if (bSuppressCursorRedraw == 0) {
            bSuppressCursorRedraw = 1;
            if (GetCapture() == hwndSelf) {
                ReleaseCapture();
            }
            Ddraw_RebindWindowClipper(hwndSelf);
            RedrawSoftwareCursor(1);
            PopupWndBase_RebindClipperToActiveScreen();
            PlacementCursorMaybe_004854c8.SetCursorCapture(0, 0, 0);
            if (Unk0x88 == 0) {
                return;
            }
            RedrawSoftwareCursorOverBoard(1);
            return;
        }
        if (bRelease != 0) {
            return;
        }
    }
    if (bSuppressCursorRedraw != 0 || GetCapture() != hwndSelf) {
        bSuppressCursorRedraw = 0;
        SetCapture(hwndSelf);
        while (ShowCursor(0) >= 0) {
        }
    }
}

// FUNCTION: LOCO 0x413d10 // CONTENT-COMPLETE, NOT YET BYTE-MATCHED (2026-07-18, v199): asmscore
// total 40446, byte_diff 16/111, insns 40/39. Residual is the original caching
// pOffscreenSurface into a callee-saved register (edi) held across the intervening
// SetModalCapture/SetTimer/ShowWindow/vtable calls, vs. this compile choosing a different
// register (esi) for the same value -- tried an explicit `IDirectDrawSurface *pOffscreen = ...;`
// local declared immediately before use (matching the DSound_InitDeviceAndChannelPool precedent),
// BYTE-IDENTICAL either way, so not source-steerable here. The "dead `mov ecx,esi`" before the
// PopupWndBase_RebindClipperToActiveScreen() call at the very end was a real `this` pass, fixed
// in v362 by making that member non-static (lever 3). See docs/PARKED.md.
void PopupWndBase::Show() {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    extern IDirectDrawSurface *g_pDDrawPrimarySurface; // DAT_004fd3c0

    SetModalCapture(0);
    nShowTimerId = SetTimer(hwndSelf, 0x43, 0xbe, NULL);
    ShowWindow(hwndSelf, SW_SHOW);
    bSuppressCursorRedraw = 1;
    bShown = 1;
    this->OnDrawContent(NULL);
    Ddraw_RebindWindowClipper(hwndSelf);
    g_pDDrawPrimarySurface->Blt(&rectScreenBounds, pOffscreenSurface, &rectScreenBounds, 0x1000000, NULL);
    PopupWndBase_RebindClipperToActiveScreen();
}

// FUNCTION: LOCO 0x413c10 (Ghidra: OnExit -- renamed in src+Ghidra 2026-07-21 when vtable slot
// 4 was modeled as a real virtual shared with the per-class OnExit overrides)
// CONTENT-COMPLETE, NOT YET BYTE-MATCHED (2026-07-18, v199): very
// close -- asmscore total 28011, byte_diff 11/252, insns 87/87 (exact instruction count match).
// Every field read/write and Blt/CopyRect/OffsetRect call site hand-verified 1:1 against raw
// disasm, including the board-scroll offset pair (g_rectAppWindowBounds/g_rectAppClientBounds)
// and the g_worldBoard.bSurfaceLockGuard conditional Unlock. Residual is the same 2
// intrinsic classes as SetModalCapture/Show above: (1) FIXED in v362 -- the "dead `mov ecx,esi`"
// before PopupWndBase_RebindClipperToActiveScreen() was its `this` pass (lever 3), and that
// member is no longer `static`; (2) hwndSelf cached into a
// callee-saved register (edi) across the Unk0x88/GetCapture/ReleaseCapture block, vs. this
// compile's own register choice. See docs/PARKED.md.
void PopupWndBase::OnExit() {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940
    extern IDirectDrawSurface *g_pDDrawPrimarySurface; // DAT_004fd3c0
    extern IDirectDrawSurface *g_pDDrawWorkSurface;    // DAT_004fd3c4
    extern RECT g_rectAppWindowBounds;              // board-scroll dest-offset template, paired w/
                                                 // g_rectAppClientBounds below; purpose
                                                 // beyond this pairing unread
    extern RECT g_rectAppClientBounds;   // 0x485220 (tagRECT_00485220), see
                                                 // RedrawSoftwareCursorOverBoard's own extern
    extern unsigned char g_bBoardScrollFlag; // DAT_00485210

    SetModalCapture(1);
    KillTimer(hwndSelf, nShowTimerId);
    if (Unk0x88 != 0 && GetCapture() == hwndSelf) {
        ReleaseCapture();
    }
    Ddraw_RebindWindowClipper(hwndSelf);

    RECT rectDst, rectSrc;
    CopyRect(&rectDst, &rectScreenBounds);
    CopyRect(&rectSrc, &rectScreenBounds);
    if (g_bBoardScrollFlag != 0) {
        OffsetRect(&rectSrc, -g_rectAppClientBounds.left, -g_rectAppClientBounds.top);
        OffsetRect(&rectDst, g_rectAppWindowBounds.left, g_rectAppWindowBounds.top);
    }
    if (g_worldBoard.bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == 0) {
        g_worldBoard.bSurfaceLockGuard = 0;
    }
    g_pDDrawPrimarySurface->Blt(&rectDst, g_pDDrawWorkSurface, &rectSrc, 0x1000000, NULL);
    PopupWndBase_RebindClipperToActiveScreen();
    ShowWindow(hwndSelf, SW_HIDE);
    bShown = 0;
}

// PopupWndBase's vtable slot 0x24 (RouteMessageMaybe) is NOT a genuine thiscall virtual -- its
// real ABI pushes `pWnd` as a plain 5th stack argument (every call site ends `ret 0x14`, no ECX
// load anywhere; see docs/subsystems.md's "PopupWndBase message dispatch"). Same reinterpret-
// the-vtable-pointer spirit as WindowBase's own WindowBaseVtableView, one slot earlier (0x24
// here vs. WindowBase's own 0x28) since PopupWndBase's own dispatch table is laid out one slot
// tighter (no separate slot-0x28-worth of gap before it).
struct PopupWndBaseVtableView {
    void *pad[9]; // slots 0x00-0x20
    LRESULT (__stdcall *pfnRouteMessage)(PopupWndBase *, HWND, UINT, WPARAM, LPARAM); // slot 0x24
};

// FUNCTION: LOCO 0x415900 (Ghidra: PopupWndBase::WndProc)
LRESULT CALLBACK PopupWndBase_WndProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PopupWndBase *pWnd = (PopupWndBase *)GetWindowLongA(hwndMsg, GWL_USERDATA);
    if (pWnd == NULL) {
        if (msg == WM_CREATE && lParam != 0) {
            pWnd = (PopupWndBase *)((CREATESTRUCTA *)lParam)->lpCreateParams;
            SetWindowLongA(hwndMsg, GWL_USERDATA, (LONG)pWnd);
        } else {
            HWND hwndParent = GetParent(hwndMsg);
            if (hwndParent != NULL) {
                pWnd = (PopupWndBase *)GetWindowLongA(hwndParent, GWL_USERDATA);
            }
            if (pWnd == NULL) {
                return DefWindowProcA(hwndMsg, msg, wParam, lParam);
            }
        }
    }
#ifdef LOCO_PORT
    // PORT ONLY -- byte-neutral for the match build, see the #else arm. This is the SAME defect
    // WindowBase_WndProc's own slot 0x28 had (fixed v557d, see src/WindowBase.cpp); the popup
    // half of the pair was missed then and is fixed here on the same evidence.
    //
    // Slot 0x24 holds a bare __stdcall free function taking pWnd as an explicit 5th stack arg
    // (0x4143e0, `ret 0x14`, no ECX use). A C++ virtual member cannot occupy that slot with that
    // ABI, so src/PopupWndBase.h keeps `_v24` as a declared-only dummy slot-holder -- and in the
    // PORT build that dummy is what lands in every generated vtable. link/gen_stubs.py sizes its
    // stub from the MANGLED name, and `?_v24@PopupWndBase@@UAEPAXXZ` says "no arguments", so the
    // stub is a bare `ret` while this call site pushes 20 bytes and expects callee cleanup. Every
    // message routed to a popup therefore leaked 20 bytes of stack AND never reached the real
    // router; enough of them and the return address is garbage and the process jumps to 0.
    //
    // Calling the real router directly is safe rather than a guess: the dword 0x004143e0 appears
    // at exactly 4 places in the original .rdata -- 0x4776a4, 0x4778bc, 0x478154, 0x47844c -- and
    // every one of them is slot 0x24 of a popup-family vtable (CreditsWnd 0x477680, the
    // PopupWndBase base 0x477898, BuildToolCursorWnd 0x478130, TutorialWnd 0x478428). No derived
    // class overrides this slot, so the indirection has one possible target anyway.
    return PopupWndBase_RouteMessage(pWnd, hwndMsg, msg, wParam, lParam);
#else
    PopupWndBaseVtableView *vt = *(PopupWndBaseVtableView **)pWnd;
    return vt->pfnRouteMessage(pWnd, hwndMsg, msg, wParam, lParam);
#endif
}

// FUNCTION: LOCO 0x414a80
LRESULT PopupWndBase::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    if (hwndMsg == hwndSelf) {
        Ddraw_RebindWindowClipper(hwndSelf);
        RedrawSoftwareCursor(1);
        PopupWndBase_RebindClipperToActiveScreen();
        if (Unk0x88 != 0) {
            RedrawSoftwareCursorOverBoard(1);
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x414ac0
LRESULT PopupWndBase::OnSize(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (bCreated != 0) {
        this->RefreshClientRect();
    }
    return 0;
}

// FUNCTION: LOCO 0x414ae0
LRESULT PopupWndBase::OnPaint(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    if (bShown == 0) {
        return 0;
    }
    Ddraw_RebindWindowClipper(hwndSelf);

    RECT rectUpdate;
    PAINTSTRUCT ps;
    if (GetUpdateRect(hwndMsg, &rectUpdate, 0) != 0) {
        HDC hdc = BeginPaint(hwndMsg, &ps);
        if (hdc == NULL) {
            EndPaint(hwndMsg, &ps);
            PopupWndBase_RebindClipperToActiveScreen();
            return 1;
        }
    }
    // sic: if GetUpdateRect returned 0 (no update region), BeginPaint above is skipped
    // entirely and this EndPaint call still runs, on a never-initialized PAINTSTRUCT -- a
    // genuine mismatched BeginPaint/EndPaint pair in the original, confirmed via raw disasm
    // (GetUpdateRect's own failure branch jumps straight past the BeginPaint call to this same
    // EndPaint site). Reproduced as-is per CLAUDE.md's "reproduce, don't fix" rule.
    EndPaint(hwndMsg, &ps);
    this->OnDrawContent(&ps);
    this->OnDrawCursorOverlay();
    PopupWndBase_RebindClipperToActiveScreen();
    return 0;
}

// 0x426a60 / 0x426ac0 / 0x426950 / 0x426ad0 (OnSetCursor/OnEraseBkgnd/
// OnMouseActivate/OnDestroy) MOVED OUT 2026-07-22 (v322) to src/WindowBase.cpp: these are
// WindowBase TU's shared default bodies (present in WindowBase_Vtbl itself; PopupWndBase's
// vtable just installs the same addresses at its own shifted slots). The declarations stay in
// PopupWndBase.h to hold this family's vtable layout.

// FUNCTION: LOCO 0x414b80
LRESULT PopupWndBase::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    bCreated = 0;
    DestroyWindow(hwndSelf);
    if (hwndOwner == NULL) {
        PostQuitMessage(0);
    }
    return 0;
}

// ---- The nineteen per-message defaults PopupWndBase_Vtbl (0x477898) installs at slots
// 0x28/0x2c/0x30/0x34/0x38/0x3c/0x40/0x44/0x48/0x50/0x54/0x5c/0x60/0x70/0x80/0x84/0x88/0x8c/0x90.
// Every one of those dwords reads 0x422ea0 -- the same address as WindowBase's own eighteen and
// as the free WindowBase_DefWindowProcStub that carries the marker for it. Per this header's
// sibling-hierarchy note, the original near-certainly wrote each family's defaults separately and
// the linker ICF-folded all thirty-seven onto one 29-byte copy, which a `__thiscall` body that
// never touches its implicit `this` is byte-identical to.
//
// Deliberately UNMARKED: one address gets one marker, and it lives on the free function in
// src/WindowBase.cpp. Bodies added because declared-only virtuals cost nothing in the match build
// but become zeroed Stub_Report slots in the PORT build -- OnMouseActivate returning 0 instead of
// DefWindowProcA's MA_ACTIVATE is a real interaction bug, not just a missing forward.
LRESULT PopupWndBase::OnUnhandledMessage(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnTimerDefault(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnCreate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnLButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnRButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnLButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnRButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnKeyUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnShowWindow(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnNotify(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnCommand(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnHotKey(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnActivateApp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

LRESULT PopupWndBase::OnWindowPosChanging(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// ---- The four remaining shared slots -- 0x58/0x6c/0x74/0x78 (WM_MOUSEACTIVATE/WM_SETCURSOR/
// WM_ERASEBKGND/WM_DESTROY) -- whose dwords read 0x426950/0x426a60/0x426ac0/0x426ad0. Those are
// the SAME four addresses WindowBase_Vtbl installs at its own 0x5c/0x70/0x78/0x7c, and the bodies
// carrying their markers live in src/WindowBase.cpp. Another ICF fold rather than a shared base:
// the two hierarchies are siblings, and these bodies only touch `hwndSelf`, which both families
// place at +0x8 -- so the two classes' copies compile to identical bytes and collapse to one.
// Unmarked for the same reason as the block above: one address, one marker.
LRESULT PopupWndBase::OnMouseActivate(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return 0;
}

LRESULT PopupWndBase::OnSetCursor(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (hwndMsg != hwndSelf) {
        return DefWindowProcA(hwndMsg, msg, wParam, lParam);
    }
    return 1;
}

LRESULT PopupWndBase::OnEraseBkgnd(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return 1;
}

LRESULT PopupWndBase::OnDestroy(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam) {
    hwndSelf = NULL;
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4143e0 // TODO: sync (Ghidra: PopupWndBase::RouteMessageMaybe -- named
// PopupWndBase_RouteMessage in src/, deliberately dropping "Maybe" to match the sibling
// WindowBase_RouteMessage convention, same as that function's own analogous drift)
// A plain __stdcall free function taking the state pointer as an explicit first parameter, NOT
// a real __thiscall method -- see this function's own header comment for the this-typing false
// start that was tried and reverted. ⚠ Case order below is the TRUE physical/source declaration
// order, ground-truthed by reading the raw jump-table/pointer-table bytes directly (Python
// struct.unpack over loco/Loco.exe at 0x414a2c/0x414a4c for the WM_CREATE-WM_CLOSE dense
// cluster, 0x414a5c for the LBUTTON/RBUTTON cluster) and cross-checking every case body's own
// physical address -- NOT ascending WM_* numeric order (Yoda lesson #33's "case bodies land in
// source declaration order" DOES apply to sparse binary-search clusters here, contrary to an
// earlier same-session assumption that only jump tables were order-sensitive; closed a
// ~1000-byte structural residual by reordering to match). One genuine surprise even within the
// LBUTTON/RBUTTON dense jump table itself: physical order is LBUTTONDOWN, LBUTTONUP,
// RBUTTONDOWN, RBUTTONUP, LBUTTONDBLCLK, RBUTTONDBLCLK -- NOT index/value order (0x201..0x206)
// -- confirmed via the raw 6-entry pointer table at 0x414a5c, each slot's target verified
// against docs/subsystems.md's own WM_*-to-slot table (0x34/0x38/0x3c/0x40/0x44/0x48
// respectively). default: is declared BEFORE case WM_HOTKEY: (WM_HOTKEY is the true physical
// last case) -- same quirk already documented for WindowBase_RouteMessage's own analogous
// tail, mirrored here deliberately.
LRESULT __stdcall PopupWndBase_RouteMessage(PopupWndBase *pWnd, HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    extern void Ddraw_RebindWindowClipper(HWND hwnd); // Ddraw::Ddraw_RebindWindowClipper, 0x45b940

    PopupWndBaseVtblProbe *pProbe = (PopupWndBaseVtblProbe *)pWnd;
    POINT pt;

    switch (msg) {
    case WM_CREATE:
        return pWnd->OnCreate(hwndMsg, msg, wParam, lParam);
    case WM_SETFOCUS:
        return pProbe->OnSetFocus(hwndMsg, msg, wParam, lParam);
    case WM_KILLFOCUS:
        return pProbe->OnKillFocus(hwndMsg, msg, wParam, lParam);
    case WM_SIZE:
        return pProbe->OnSize(hwndMsg, msg, wParam, lParam);
    case WM_PAINT:
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
    case WM_WINDOWPOSCHANGING:
        return pProbe->OnWindowPosChanging(hwndMsg, msg, wParam, lParam);
    case WM_NOTIFY:
        return pProbe->OnNotify(hwndMsg, msg, wParam, lParam);
    case WM_NCHITTEST: {
        LRESULT lHit = DefWindowProcA(hwndMsg, WM_NCHITTEST, wParam, lParam);
        if (lHit != HTCLIENT) {
            pWnd->SetModalCapture(1);
            return lHit;
        }
        pWnd->SetModalCapture(0);
        return 1;
    }
    case WM_KEYUP:
        return pProbe->OnKeyUp(hwndMsg, msg, wParam, lParam);
    case WM_KEYDOWN:
        return pProbe->OnKeyDown(hwndMsg, msg, wParam, lParam);
    case WM_COMMAND:
        return pProbe->OnCommand(hwndMsg, msg, wParam, lParam);
    case WM_TIMER:
        if (wParam == 0x43 && pWnd->pActiveCursorDesc != NULL && pWnd->bSuppressCursorRedraw == 0) {
            GetCursorPos(&pt);
            // The original tail-merges this early-out's `return 0` into the function-wide
            // shared epilogue (`cmp WORD PTR [edx+0x160],1 / jbe 0x4149ce`); this compile
            // inlines a private 6-instruction copy instead. Hoisting the body into an
            // `if (... > 1) { ... }` so both returns become ONE statement -- the shape that
            // DID merge WM_CAPTURECHANGED's epilogue below -- makes it WORSE here
            // (DIFF 986 -> 999, len 1708 -> 1712), so the merge is not source-reachable
            // this way. Left as the faithful early-out.
            if (pWnd->pActiveCursorDesc->nTotalFrameCount <= 1) {
                return 0;
            }
            pWnd->nCursorFrameIndex++;
            if (pt.x == pWnd->nLastCursorScreenX && pt.y == pWnd->nLastCursorScreenY) {
                Ddraw_RebindWindowClipper(pWnd->hwndSelf);
                pWnd->RedrawSoftwareCursor(1);
                pWnd->PopupWndBase_RebindClipperToActiveScreen();
                if (pWnd->Unk0x88 != 0) {
                    pWnd->RedrawSoftwareCursorOverBoard(1);
                }
            }
            pWnd->nLastCursorScreenX = pt.x;
            pWnd->nLastCursorScreenY = pt.y;
            return 0;
        }
        return pWnd->OnTimerDefault(hwndMsg, msg, wParam, lParam);
    case WM_MOUSEMOVE:
        if (hwndMsg != g_pApp->hwndOwner && hwndMsg != pWnd->hwndSelf) {
            return 1;
        }
        if (hwndMsg == pWnd->hwndSelf) {
            pt.x = pWnd->rectScreenBounds.left + LOWORD(lParam);
            pt.y = pWnd->rectScreenBounds.top + HIWORD(lParam);
        }
        // sic: else (message forwarded via the owner window, not sent directly to hwndSelf) pt
        // is left as whatever was last in this shared stack slot -- a genuine uninitialized-
        // read in the original, confirmed via raw disasm (the "hwndMsg != hwndSelf" branch
        // reloads pt.x/pt.y from the same stack offsets WM_TIMER's own GetCursorPos target
        // uses, with no intervening write of its own). Reproduced as-is.
        {
            HWND hWndUnder = WindowFromPoint(pt);
            if (pWnd->Unk0x88 == 0 && hWndUnder != pWnd->hwndSelf) {
                pWnd->SetModalCapture(1);
                return 0;
            }
        }
        pWnd->SetModalCapture(0);
        return pWnd->OnMouseMove(hwndMsg, msg, wParam, lParam);
    case WM_LBUTTONDOWN:
        SetForegroundWindow(pWnd->hwndSelf);
        return pWnd->OnLButtonDown(hwndMsg, msg, wParam, lParam);
    case WM_LBUTTONUP:
        return pWnd->OnLButtonUp(hwndMsg, msg, wParam, lParam);
    case WM_RBUTTONDOWN:
        return pWnd->OnRButtonDown(hwndMsg, msg, wParam, lParam);
    case WM_RBUTTONUP:
        return pWnd->OnRButtonUp(hwndMsg, msg, wParam, lParam);
    case WM_LBUTTONDBLCLK:
        return pWnd->OnLButtonDblClk(hwndMsg, msg, wParam, lParam);
    case WM_RBUTTONDBLCLK:
        return pWnd->OnRButtonDblClk(hwndMsg, msg, wParam, lParam);
    case WM_CAPTURECHANGED:
        if (pWnd->nCursorDescKey != 0) {
            if ((HWND)lParam == pWnd->hwndSelf) {
                pWnd->SetModalCapture(0);
            } else {
                pWnd->SetModalCapture(1);
                return 0;
            }
        }
        return 0;
    default:
        return pWnd->OnUnhandledMessage(hwndMsg, msg, wParam, lParam);
    case WM_HOTKEY:
        return pProbe->OnHotKey(hwndMsg, msg, wParam, lParam);
    }
}
