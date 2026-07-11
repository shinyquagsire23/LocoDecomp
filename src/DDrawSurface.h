// DDrawSurface -- the FIRST .obj in .text (0x401000..0x40161f), five DirectDraw surface
// helpers that sit between the GDI/bitmap world and the live DirectDraw surfaces
// Ddraw_Init brings up. Pinned as one translation unit by contiguity: 0x401000,
// 0x401170, 0x401280, 0x4014e0 and 0x401540 run back to back with only inter-function
// `nop` padding between them, and the very next function (0x401620) is
// PostBagFileCache's constructor, i.e. a different .obj entirely.
//
// This is NOT src/Ddraw.cpp's TU -- that one owns 0x45baa0/0x45bbc0 (Ddraw_Teardown and
// Ddraw_HResultToString), 350 KB further into .text. Same subsystem, different compile
// unit; the shared declarations live in src/Ddraw.h and are included from both.
//
// Everything here works through the DX2-era IDirectDrawSurface this toolchain's
// <ddraw.h> declares, so unlike Ddraw_Init (see src/Ddraw.cpp's note) nothing in this TU
// is blocked on a DirectX 5 SDK header: Blt/Restore/GetSurfaceDesc/GetDC/ReleaseDC/
// Lock/Unlock are all present and at the right vtable slots. The one DX5-shaped thing is
// the 124-byte CreateSurface descriptor, and DDSurfaceDescPadded0x7c (src/LocoBitmap.h)
// already models that.
#pragma once

#include <windows.h>
#include <ddraw.h>

extern "C" {
    // DAT_00485280 -- the post->>1 channel-bleed guard mask (0x3def@555bpp, 0x7bef@565bpp),
    // written once by Ddraw_Init. DDraw_DarkenRect below reads it per pixel; the LocoBitmap
    // shadow-blit family reads the same global (see src/LocoBitmap.h).
    extern unsigned short g_wChannelBleedGuardMask;

    // 0x45ba50 -- sets the surface's DDCKEY_SRCBLT transparency colorkey (magenta
    // 0x7c1f@555bpp / 0xf81f@565bpp). The 2nd parameter is caller-owned scratch the callee
    // fully overwrites (a DDCOLORKEY-sized local); every caller just hands it the address of
    // some already-allocated nearby buffer -- here, the DDSURFACEDESC we just built.
    // Ghidra: LocoBitmap::LocoBitmap_SetColorKey.
    void LocoBitmap_SetColorKey(void *pSurface, void *pScratch); // 0x45ba50
}

// 0x401000 -- LoadImageA the given .bmp off disk, create a matching offscreen DirectDraw
// surface for it, StretchBlt the GDI bitmap onto that surface, delete the GDI bitmap and
// hand the surface back (NULL if either step failed). `unused` is read into no field --
// the one call site passes LR_LOADFROMFILE, which the body hardcodes anyway.
// bVideoMemory == 1 asks for DDSCAPS_VIDEOMEMORY and enables the retry path; anything else
// goes straight to system memory with no retry.
IDirectDrawSurface *DDraw_CreateSurfaceFromFile(const char *pszPath, unsigned int unused,
                                                int nWidth, int nHeight, char bVideoMemory);

// 0x401170 -- blit a GDI bitmap onto a DirectDraw surface through the surface's own DC,
// stretching it to the surface's full extent. nWidth/nHeight of 0 mean "the bitmap's own".
// Returns the GetDC HRESULT (E_FAIL if either handle is null).
HRESULT DDraw_StretchBlitBitmapToSurface(IDirectDrawSurface *pSurface, HBITMAP hBitmap,
                                         short xSrc, short ySrc, int nWidth, int nHeight);

// 0x401280 -- present one client-area rect from the work surface to the primary surface,
// mapping client coords to screen coords (and clipping to the window) on the way. Ghidra:
// Ddraw::Ddraw_BltUpdateRect.
void Ddraw_BltUpdateRect(RECT *pRect, HWND hWnd, POINT *pScrollOffset, char bAsync);

// 0x4014e0 -- the surface's width/height, NARROWED TO 16 BITS (the body writes through both
// out-params with `mov WORD PTR`, never a full dword). src/LocoBitmap.cpp and
// src/PopupWndBase.cpp used to carry file-local `extern` declarations of this typed
// `unsigned int *` -- a WIDER type than the function writes, and (because C++ mangles the
// parameter types) a DIFFERENT symbol from this one, so those TUs were calling something
// nothing defines. Converged on this declaration in v554; their file-local decls now spell it
// exactly as here, and the `unsigned short *` casts at the int-typed call sites are the
// faithful record of the 16-bit write. Byte-neutral, measured.
void DDraw_QuerySurfaceDims(IDirectDrawSurface *pSurface, unsigned short *pOutWidth,
                            unsigned short *pOutHeight);

// 0x401540 -- halve every 16bpp pixel in the given work-surface rect (>>1 per pixel, then
// mask off the bits that bled across channel boundaries), i.e. darken it to 50%. Locks the
// shared work surface through WorldBoardMaybe's cached descriptor/guard pair if it is not
// already held, and unlocks on the way out. Always returns 1.
// The rect goes BY VALUE (pinned v516 by its only caller, WidgetBaseObj0x4784c8's slot-11
// BlitAnimFrameMaybe override at 0x454900: both call sites pass it with the by-value-struct
// `sub esp,0x10` + field-store idiom, not 4 pushes). The body reads the same four stack
// dwords either way, so this compiles byte-identical to the old 4-int model on its own.
char DDraw_DarkenRect(RECT rect);
