// DDrawSurface method bodies -- see DDrawSurface.h for the TU writeup.
#include <windows.h>
#include <ddraw.h>

#include "DDrawSurface.h"

#include "AppWindow.h"
#include "Ddraw.h"
#include "LocoBitmap.h"
#include "WorldBoardMaybe.h"
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: RGB565 surface pinning, see port/README.md
#endif

// FUNCTION: LOCO 0x401000
// ⚠ The retry block below is an ORIGINAL ENGINE BUG -- see docs/engine-bugs.md. When the
// video-memory CreateSurface fails, the fallback re-stamps ddsCaps with the SAME
// DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY it just failed with (the original stores the
// literal 0x4040 a second time, verified byte-wise at 0x4010fd), so the second attempt asks
// for exactly what the first one was refused. Almost certainly a typo for the system-memory
// 0x840 the non-video path uses. Reproduced, not fixed.
IDirectDrawSurface *DDraw_CreateSurfaceFromFile(const char *pszPath, unsigned int unused,
                                                int nWidth, int nHeight, char bVideoMemory)
{
    IDirectDrawSurface *pSurface;
    BITMAP bm;
    DDSurfaceDescPadded0x7c u;
    HBITMAP hBitmap;
    HRESULT hr;

    pSurface = NULL;
    if (GetFileAttributesA(pszPath) != 0xffffffff)
        hBitmap = (HBITMAP)LoadImageA(g_pApp->hInstance, pszPath, IMAGE_BITMAP, nWidth, nHeight,
                                      LR_LOADFROMFILE);
    else
        hBitmap = NULL;
    if (hBitmap == NULL)
        return NULL;

    GetObjectA(hBitmap, sizeof(BITMAP), &bm);
    memset(&u, 0, sizeof(u));
    u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
    if (nWidth == 0) {
        u.ddsd.dwWidth = bm.bmWidth;
        u.ddsd.dwHeight = bm.bmHeight;
    } else {
        u.ddsd.dwWidth = nWidth;
        u.ddsd.dwHeight = nHeight;
    }
    u.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    u.ddsd.ddsCaps.dwCaps = (bVideoMemory == 1)
                                ? (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY)
                                : (DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY);
#ifdef LOCO_PORT
    Port_ForceRgb565(&u.ddsd); // PORT: pin 565, do not inherit the desktop format
#endif
    hr = g_pDDraw2->CreateSurface(&u.ddsd, &pSurface, NULL);
    if (hr != DD_OK && bVideoMemory == 1) {
        Ddraw_HResultToString(hr);
        // sic: asks for video memory again -- see the function header note.
        u.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
        hr = g_pDDraw2->CreateSurface(&u.ddsd, &pSurface, NULL);
        if (hr != DD_OK) {
            OutputDebugStringA("DDINIT - failed to create surface");
            return NULL;
        }
    }
    LocoBitmap_SetColorKey(pSurface, &u.ddsd);
    DDraw_StretchBlitBitmapToSurface(pSurface, hBitmap, 0, 0, 0, 0);
    DeleteObject(hBitmap);
    return pSurface;
}

// FUNCTION: LOCO 0x401170
// Note the Restore() up front: this is the one path that repopulates a surface after a mode
// switch lost it, so it un-loses the surface before asking GDI for its DC. The
// CreateCompatibleDC failure is logged but NOT bailed on -- SelectObject/GetObjectA run on the
// null DC regardless, which is the original's own shape, not a transcription slip.
HRESULT DDraw_StretchBlitBitmapToSurface(IDirectDrawSurface *pSurface, HBITMAP hBitmap,
                                         short xSrc, short ySrc, int nWidth, int nHeight)
{
    HDC hdcSurface;
    HRESULT hr;
    BITMAP bm;
    DDSurfaceDescPadded0x7c u;

    if (hBitmap != NULL && pSurface != NULL) {
        HDC hdcMem;

        pSurface->Restore();
        hdcMem = CreateCompatibleDC(NULL);
        if (hdcMem == NULL)
            OutputDebugStringA("createcompatible dc failed\n");
        SelectObject(hdcMem, hBitmap);
        GetObjectA(hBitmap, sizeof(BITMAP), &bm);
        if (nWidth == 0)
            nWidth = bm.bmWidth;
        if (nHeight == 0)
            nHeight = bm.bmHeight;
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        u.ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
        pSurface->GetSurfaceDesc(&u.ddsd);
        hr = pSurface->GetDC(&hdcSurface);
        if (hr == DD_OK) {
            StretchBlt(hdcSurface, 0, 0, u.ddsd.dwWidth, u.ddsd.dwHeight, hdcMem, xSrc, ySrc,
                       nWidth, nHeight, SRCCOPY);
            pSurface->ReleaseDC(hdcSurface);
        }
        DeleteDC(hdcMem);
        return hr;
    }
    return E_FAIL;
}

// FUNCTION: LOCO 0x401280
// The source rect is captured from *pRect BEFORE any of the coordinate mapping below, so the
// blit reads work-surface pixels at the caller's own client coordinates and writes them at the
// screen coordinates those map to. pScrollOffset, when given, backs the board scroll out first.
//
// The DDERR_SURFACELOST ladder is the interesting part: a lost primary surface is Restore()d
// and the blit retried, and every such retry also re-dirties the whole board viewport, because
// a restored surface has lost its contents and the cached dirty-tile set no longer describes
// what is actually on screen.
//
// ⭐ The `else if (hr != DD_OK)` below deliberately has NO `else { return; }` arm, and that
// absence is what closed the last instruction of this function. Writing the DD_OK case as an
// explicit early `return` is functionally identical -- the shared `if (hr != DD_OK)` tail is
// false on that path either way, and the original does jump straight to the epilogue there --
// but it lets VC5 leave the Blt result in eax and compare out of eax, costing the original's
// `mov esi, eax` at 0x4013bb (insns 193/194, everything else already identical). Falling
// through instead keeps hr live to the shared tail, so it gets a callee-saved register home
// eagerly. Same family as docs/CODEGEN.md's early-return notes: an explicit `return` and a
// fall-through that provably returns are NOT interchangeable for register allocation.
void Ddraw_BltUpdateRect(RECT *pRect, HWND hWnd, POINT *pScrollOffset, char bAsync)
{
    POINT ptOrigin;
    RECT rcDest, rcSrc, rcWindow, rcClipped;
    HRESULT hr;

    rcSrc = *pRect;
    if (IsRectEmpty(pRect)) {
#ifdef LOCO_PORT
        static unsigned int nEmpty = 0; // PORT ONLY -- byte-neutral
        if (++nEmpty <= 4 || (nEmpty % 500) == 0)
            Port_Tracef("blt EMPTY #%u %ld,%ld,%ld,%ld\n", nEmpty, (long)rcSrc.left,
                        (long)rcSrc.top, (long)rcSrc.right, (long)rcSrc.bottom);
#endif
        return;
    }

    rcDest = *pRect;
    if (pScrollOffset != NULL)
        OffsetRect(&rcDest, -pScrollOffset->x, -pScrollOffset->y);

    ptOrigin.x = 0;
    ptOrigin.y = 0;
    ClientToScreen(hWnd, &ptOrigin);
    OffsetRect(&rcDest, ptOrigin.x, ptOrigin.y);

    GetWindowRect(hWnd, &rcWindow);
#ifdef LOCO_PORT
    // PORT: this function works in SCREEN coordinates because in 1998 the primary surface WAS
    // the screen. Here the "primary" is Port_CreateEmulatedPrimary's g_dwScreenWidth x
    // g_dwScreenHeight bitmap, which Port_Present blits into the app owner's CLIENT area -- so
    // the space this function must target is the app owner's client rect, not the host desktop.
    //
    // The two are not the same origin and cannot be made so. Every game window is created as a
    // borderless WS_POPUP at 0,0 and IS at 0,0 while it is still hidden; winemac moves it down
    // below the Mac menu bar (measured y=78 on this host) the moment it is shown. Without this
    // translation the front end's present came out at dst=0,78,1024,846 against a 1024x768
    // emulated primary -- pushed off the bottom, which is exactly the failure port/README.md's
    // present notes describe, and the black interactive boot of v568/v569.
    //
    // Translating by the owner's own client origin makes the whole path origin-agnostic: it is
    // the identity when the window manager does place us at 0,0, so it costs nothing on a host
    // that behaves, and it is self-correcting on one that does not.
    {
        POINT ptAppOrigin;
        ptAppOrigin.x = 0;
        ptAppOrigin.y = 0;
        ClientToScreen(g_pApp->hwndOwner, &ptAppOrigin);
        OffsetRect(&rcDest, -ptAppOrigin.x, -ptAppOrigin.y);
        OffsetRect(&rcWindow, -ptAppOrigin.x, -ptAppOrigin.y);
    }
#endif
    if (!IntersectRect(&rcClipped, &rcDest, &rcWindow)) {
#ifdef LOCO_PORT
        static unsigned int nClip = 0; // PORT ONLY -- byte-neutral
        if (++nClip <= 4 || (nClip % 500) == 0)
            Port_Tracef("blt CLIPOUT #%u dst=%ld,%ld,%ld,%ld wnd=%ld,%ld,%ld,%ld\n", nClip,
                        (long)rcDest.left, (long)rcDest.top, (long)rcDest.right,
                        (long)rcDest.bottom, (long)rcWindow.left, (long)rcWindow.top,
                        (long)rcWindow.right, (long)rcWindow.bottom);
#endif
        return;
    }

    if (bAsync == 0) {
        hr = g_pDDrawPrimarySurface->Blt(&rcDest, g_pDDrawWorkSurface, &rcSrc, DDBLT_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            hr = g_pDDrawPrimarySurface->Restore();
            if (hr == DD_OK) {
                hr = g_pDDrawPrimarySurface->Blt(&rcDest, g_pDDrawWorkSurface, &rcSrc, DDBLT_WAIT,
                                                 NULL);
                g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
            }
        }
    } else {
        hr = g_pDDrawPrimarySurface->Blt(&rcDest, g_pDDrawWorkSurface, &rcSrc, DDBLT_ASYNC, NULL);
        if (hr == DDERR_SURFACELOST) {
            hr = g_pDDrawPrimarySurface->Restore();
            if (hr == DD_OK) {
                hr = g_pDDrawPrimarySurface->Blt(&rcDest, g_pDDrawWorkSurface, &rcSrc, DDBLT_WAIT,
                                                 NULL);
                g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
            }
        } else if (hr != DD_OK) {
            hr = g_pDDrawPrimarySurface->Blt(&rcDest, g_pDDrawWorkSurface, &rcSrc, DDBLT_WAIT,
                                             NULL);
            if (hr == DDERR_SURFACELOST) {
                hr = g_pDDrawPrimarySurface->Restore();
                if (hr == DD_OK) {
                    hr = g_pDDrawPrimarySurface->Blt(&rcDest, g_pDDrawWorkSurface, &rcSrc,
                                                     DDBLT_WAIT, NULL);
                    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
                }
            }
        }
    }

    if (hr != DD_OK)
        Ddraw_HResultToString(hr);
#ifdef LOCO_PORT
    // PORT ONLY -- byte-neutral. See port/README.md's present notes: a blit whose DEST rect
    // has been pushed off the emulated primary by a non-zero client origin fails wholesale
    // (the emulated primary is exactly screen-sized), and a failed present looks identical
    // from the outside to a frame nobody drew. Report the first few of each outcome.
    {
        static unsigned int nOk = 0, nBad = 0;
        // The caller's return address, read out of this __cdecl frame's own parameter home
        // (`&pRect` IS the incoming stack slot, so [-1] is the pushed return address). Added
        // v569: four 3600x2338 blits were reaching here on the interactive boot and NONE of the
        // rects any candidate call site passes is that size, so the question "which call site"
        // had to be answered rather than narrowed. Map with link/Loco-port.map.
        void *pRet = ((void **)&pRect)[-1];
        if (hr == DD_OK) {
            if (++nOk <= 8 || (nOk % 500) == 0)
                Port_Tracef("blt ok #%u ret=%08lx src=%ld,%ld,%ld,%ld dst=%ld,%ld,%ld,%ld\n", nOk,
                            (unsigned long)pRet,
                            (long)rcSrc.left, (long)rcSrc.top, (long)rcSrc.right,
                            (long)rcSrc.bottom, (long)rcDest.left, (long)rcDest.top,
                            (long)rcDest.right, (long)rcDest.bottom);
        } else {
            if (++nBad <= 8 || (nBad % 500) == 0)
                Port_Tracef("blt FAIL #%u hr=%08lx src=%ld,%ld,%ld,%ld dst=%ld,%ld,%ld,%ld\n",
                            nBad, (long)hr, (long)rcSrc.left, (long)rcSrc.top, (long)rcSrc.right,
                            (long)rcSrc.bottom, (long)rcDest.left, (long)rcDest.top,
                            (long)rcDest.right, (long)rcDest.bottom);
        }
    }
#endif
}

// FUNCTION: LOCO 0x4014e0
void DDraw_QuerySurfaceDims(IDirectDrawSurface *pSurface, unsigned short *pOutWidth,
                            unsigned short *pOutHeight)
{
    DDSurfaceDescPadded0x7c u;

    memset(&u, 0, sizeof(u));
    u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
    if (pSurface != NULL) {
        pSurface->GetSurfaceDesc(&u.ddsd);
        *pOutWidth = (unsigned short)u.ddsd.dwWidth;
        *pOutHeight = (unsigned short)u.ddsd.dwHeight;
    }
}

// FUNCTION: LOCO 0x401540
// The lock is taken through WorldBoardMaybe's shared descriptor/guard pair rather than a local
// one, so a caller that already holds the work surface locked (UpdateDirtyTiles, the blit
// family in src/LocoBitmap.cpp) pays nothing here. The unlock is unconditional-ish on the way
// out: whoever ends up dropping the guard, drops it.
char DDraw_DarkenRect(RECT rect)
{
    // ⚠ Every one of these is an `int` holding a 16-bit-NARROWED value, not an `unsigned
    // short`. The original narrows with an explicit `and reg, 0xffff` and then compares
    // SIGNED against zero (`test/jle`); genuine `unsigned short` locals make VC5 keep the
    // values 16-bit and emit `test cx, cx / jbe` plus a re-narrowing `and` inside the loop.
    unsigned short *pPixel;
    unsigned short nPitchPixels;
    unsigned short nCols;
    unsigned short nRows;
    int row;
    int col;

    if (g_worldBoard.bSurfaceLockGuard == 0) {
        memset(g_worldBoard.aSurfaceDescScratch, 0, sizeof(g_worldBoard.aSurfaceDescScratch));
        g_worldBoard.aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)g_worldBoard.aSurfaceDescScratch, 0,
                                      NULL) == DD_OK)
            g_worldBoard.bSurfaceLockGuard = 1;
    }

    // The `(LONG)` is load-bearing: lPitch is signed in DDSURFACEDESC, and the original shifts
    // it with `sar`. Reading the scratch's own `unsigned int` element straight would give `shr`.
    // Naming an `int` temp for the unmasked shift result also costs two instructions -- it keeps
    // that value live, so VC5 zero-extends into a fresh register instead of masking in place.
    nPitchPixels = (unsigned short)((LONG)g_worldBoard.aSurfaceDescScratch[4] >> 1);
    pPixel = (unsigned short *)g_worldBoard.aSurfaceDescScratch[9] +
             (rect.top * nPitchPixels + rect.left);
    nCols = (unsigned short)(rect.right - rect.left);
    nRows = (unsigned short)(rect.bottom - rect.top);
    for (row = 0; row < nRows; row++) {
        for (col = 0; col < nCols; col++) {
            *pPixel = (unsigned short)((*pPixel >> 1) & g_wChannelBleedGuardMask);
            pPixel++;
        }
        pPixel += nPitchPixels - nCols;
    }

    if (g_worldBoard.bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == DD_OK)
        g_worldBoard.bSurfaceLockGuard = 0;
    return 1;
}
