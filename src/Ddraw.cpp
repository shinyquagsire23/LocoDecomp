// Ddraw method bodies -- see Ddraw.h for the subsystem writeup.
#include <windows.h>
#include <ddraw.h>

#include "AppWindow.h"       // g_pApp (Ddraw_Init's clipper owner window)
#include "Ddraw.h"
#include "DDrawSurface.h" // LocoBitmap_SetColorKey's own declaration
#include "LocoBitmap.h"
#include "ScreenSaver.h"     // g_screenSaver.bScreenSaverMode
#include "WorldBoardMaybe.h" // g_worldBoard's viewport dimensions
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: emulated 565 primary, see port/README.md
extern int g_dwScreenWidth;  // DAT_004851d8
extern int g_dwScreenHeight; // DAT_00485214
#endif

// The thumbnail loader's cached singletons, 0x4ff0f8..0x4ff110. They are file-scope here rather
// than in Ddraw.h because that is what the image proves: LocoBitmap_ReleaseThumbPalSingletonMaybe
// below is the ONLY xref of any kind to the six COM pointers (nothing in .text ever WRITES them,
// so all six arms are dead at runtime), which is exactly the footprint of a file-scope static in
// the original TU. All the image proves about their type is a vtable with Release at +0x8, hence
// IUnknown -- and their purpose is unrecoverable from a release path alone, so they keep their
// Ghidra auto names.
extern IUnknown *DAT_004ff0f8;
extern IUnknown *DAT_004ff0fc;
extern IUnknown *DAT_004ff100;
extern IUnknown *DAT_004ff104;
extern IUnknown *DAT_004ff108;
extern IUnknown *DAT_004ff10c;

// The shared thumbnail-palette bitmap (misc\thumbpal.bmp). Its loader, the sibling
// LocoBitmap_LoadThumbPalSingletonMaybe (0x45c8a0), is currently parked in src/AppWindow.cpp and
// carries its own declaration of this global; fold the two together when that function moves to
// the TU its address (0x45c8a0, immediately below 0x45c970) says it belongs to.
extern LocoBitmap *DAT_004ff110;  // TODO: idiom

// Brings DirectDraw up: create the object, take the DX5 IDirectDraw4 view of it, set the
// cooperative level, create the primary and work surfaces, publish the surface pixel format,
// key the work surface's magenta, and attach a clipper to the primary surface.
//
// ⚠ The descriptor is the DX5 124-byte DDSURFACEDESC2, not this toolchain's 108-byte
// DDSURFACEDESC -- see DDSurfaceDescPadded0x7c in src/LocoBitmap.h, the shared scratch every
// TU that builds a CreateSurface descriptor already uses for exactly this. Every field this
// function touches (dwSize, dwFlags, dwHeight, dwWidth, ddsCaps.dwCaps at +0x68) sits at an
// IDENTICAL offset in both structs, so the padded form is not a workaround here, it is the
// right layout; only sizeof differs, which is precisely what the padding supplies. The one thing
// the old header genuinely cannot supply is IID_IDirectDraw4, declared in src/Ddraw.h.
//
// EFFECTIVE MATCH -- DIFF(78) of 728, insns 225/225, same length, one register coin-flip.
// The original puts the red channel mask in eax and the 0x3def/0x7bef guard constant in ecx;
// we get the swap, and the two `mov word ptr` guard stores then schedule ahead of the three
// dword mask stores instead of between them. Every other byte agrees. It is a genuine tie: the
// original spends an extra byte on `66 89 0d` (cx -> moffs) to keep eax for the two 5-byte
// `a3` short-form R/B stores; ours spends it the other way for the two 6-byte `66 a3` guard
// stores -- identical total size, so nothing in the cost model breaks the tie.
// Probed and REFUTED (none move it off total=20487): guard local typed `int` vs
// `unsigned short`; a named `dwRBitMask` local feeding both the compare and the store;
// declaring wGuardMask first vs last; `DAT_00485284 = g_wChannelBleedGuardMask` (re-read, to
// shorten the local's live range); and all three source orderings of the R/G/B/guard stores
// (guard-first, mask-first, and interleaved R,G,guard,guard,B -- the scheduler emits the same
// code for all three, so store order is driven by the register assignment, not the reverse).
// FUNCTION: LOCO 0x45b500
unsigned char Ddraw_Init()
{
    unsigned short wWorkSurfaceWidth;
    unsigned short wWorkSurfaceHeight;
    unsigned short wGuardMask;
    DDSurfaceDescPadded0x7c scratch;
    HWND hwndOwner;
    HRESULT hr;

    hr = DirectDrawCreate(NULL, &g_pDDraw, NULL);
    if (hr != DD_OK) {
        Ddraw_HResultToString(hr);
        return 0;
    }
    hr = g_pDDraw->QueryInterface(IID_IDirectDraw4, (LPVOID *)&g_pDDraw2);
    if (hr != DD_OK) {
        Ddraw_HResultToString(hr);
        return 0;
    }

    if (g_screenSaver.bScreenSaverMode != 1) {
        hr = g_pDDraw2->SetCooperativeLevel(NULL, DDSCL_NORMAL);
    } else {
        hr = g_pDDraw2->SetCooperativeLevel(
            g_pApp->hwndOwner, DDSCL_FULLSCREEN | DDSCL_NOWINDOWCHANGES | DDSCL_EXCLUSIVE);
    }
    if (hr != DD_OK) {
        Ddraw_HResultToString(hr);
        return 0;
    }

    memset(&scratch, 0, sizeof(scratch));
    scratch.ddsd.dwSize = sizeof(scratch);
#ifdef LOCO_PORT
    // PORT: no real primary at all. A screen-sized system-memory RGB565 surface
    // stands in for it, so the ~10 `g_pDDrawPrimarySurface->Blt(...)` sites across
    // Ddraw_BltUpdateRect and PopupWndBase stay 565->565 and never ask DirectDraw
    // for a converting blit. FrameDriver_TickMaybe presents it via GDI.
    g_pDDrawPrimarySurface =
        Port_CreateEmulatedPrimary(g_pDDraw2, g_pApp->hwndOwner, g_dwScreenWidth, g_dwScreenHeight);
    if (g_pDDrawPrimarySurface == NULL) {
        Ddraw_HResultToString(DDERR_GENERIC);
        return 0;
    }
#else
    scratch.ddsd.dwFlags = DDSD_CAPS;
    scratch.ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hr = g_pDDraw2->CreateSurface(&scratch.ddsd, &g_pDDrawPrimarySurface, NULL);
    if (hr != DD_OK) {
        Ddraw_HResultToString(hr);
        return 0;
    }
#endif

    scratch.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    scratch.ddsd.dwHeight = g_worldBoard.dwViewportHeightMaybe;
    scratch.ddsd.dwWidth = g_worldBoard.dwViewportWidth;
    scratch.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
#ifdef LOCO_PORT
    // PORT: pin the work surface to 565 instead of inheriting the desktop's format.
    // The 555-vs-565 republish below then reads 565 back off the surface and picks
    // tag 0x235 exactly as it would on a real 565 desktop -- nothing downstream
    // needs to know the desktop is something else.
    Port_ForceRgb565(&scratch.ddsd);
#endif
    hr = g_pDDraw2->CreateSurface(&scratch.ddsd, &g_pDDrawWorkSurface, NULL);
    if (hr != DD_OK) {
        Ddraw_HResultToString(hr);
        // sic: the retry re-stores the SAME caps and repeats the identical call -- there is no
        // video-memory/system-memory fallback here, both attempts ask for system memory. The
        // compiler proves it: one `mov esi,0x840` feeds both `mov [desc+0x68],esi` stores.
        scratch.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        hr = g_pDDraw2->CreateSurface(&scratch.ddsd, &g_pDDrawWorkSurface, NULL);
        if (hr != DD_OK) {
            Ddraw_HResultToString(hr);
            return 0;
        }
    }

    DDraw_QuerySurfaceDims(g_pDDrawWorkSurface, &wWorkSurfaceWidth, &wWorkSurfaceHeight);

    // The same 555-vs-565 republish Ddraw_QuerySurfacePixelFormat (0x45b9b0) below does, written
    // out longhand rather than called: the emitted code is a pair of direct GetSurfaceDesc vtable
    // calls, never a `call 0x45b9b0`, and the helper is defined AFTER this function in the TU so
    // VC5 cannot have inlined it.
    g_pDDrawWorkSurface->GetSurfaceDesc(&scratch.ddsd);
    if (scratch.ddsd.ddpfPixelFormat.dwRBitMask == 0x7c00) {
        g_nSurfaceFormatTag = 0x22b;
        g_nRedShiftPos = 10;
        g_nGreenWidth = 5;
        wGuardMask = 0x3def;
    } else {
        g_nSurfaceFormatTag = 0x235;
        g_nRedShiftPos = 11;
        g_nGreenWidth = 6;
        wGuardMask = 0x7bef;
    }
    g_nRBitMask = scratch.ddsd.ddpfPixelFormat.dwRBitMask;
    g_nGBitMask = scratch.ddsd.ddpfPixelFormat.dwGBitMask;
    g_wChannelBleedGuardMask = wGuardMask;
    DAT_00485284 = wGuardMask;
    g_nBBitMask = scratch.ddsd.ddpfPixelFormat.dwBBitMask;

    // Magenta is the blit transparency key: 0x7c1f in 555, 0xf81f in 565. The descriptor is
    // refetched here even though the block above just filled it and nothing below reads it.
    // sic: neither arm taken leaves srcColorKey uninitialised and SetColorKey is called anyway.
    {
        DDCOLORKEY srcColorKey;
        // Held in a local across both calls -- the original keeps it in esi rather than
        // reloading the global, which one flat `g_pDDrawWorkSurface->` pair per call does not do.
        IDirectDrawSurface *pWorkSurface = g_pDDrawWorkSurface;

        pWorkSurface->GetSurfaceDesc(&scratch.ddsd);
        if (g_nSurfaceFormatTag == 0x22b) {
            srcColorKey.dwColorSpaceLowValue = 0x7c1f;
            srcColorKey.dwColorSpaceHighValue = 0x7c1f;
        } else if (g_nSurfaceFormatTag == 0x235) {
            srcColorKey.dwColorSpaceLowValue = 0xf81f;
            srcColorKey.dwColorSpaceHighValue = 0xf81f;
        }
        pWorkSurface->SetColorKey(DDCKEY_SRCBLT, &srcColorKey);
    }

    // ⚠ The scoping above and below is load-bearing, not cosmetic: the original frame is 0x88
    // bytes and puts pClipper at the SAME slot the (by then dead) DDCOLORKEY occupied. VC5 only
    // merges stack slots across disjoint LEXICAL scopes, so at one flat function scope the frame
    // grows to 0x8c and every descriptor offset shifts by 4.
#ifdef LOCO_PORT
    // PORT: the clipper exists only to stop blits to the REAL primary from
    // scribbling outside the window. The emulated primary is a private offscreen
    // buffer -- nothing else can be damaged by it, GDI clips the present to the
    // window, and DirectDraw rejects SetClipper on a non-primary anyway.
    (void)hwndOwner;
#else
    {
        IDirectDrawClipper *pClipper = NULL;

        hwndOwner = g_pApp->hwndOwner;
        if (g_pDDrawPrimarySurface->GetClipper(&pClipper) == DDERR_NOCLIPPERATTACHED) {
            g_pDDraw2->CreateClipper(0, &pClipper, NULL);
        } else {
            g_pDDrawPrimarySurface->SetClipper(NULL);
        }
        pClipper->SetHWnd(0, hwndOwner);
        g_pDDrawPrimarySurface->SetClipper(pClipper);
        pClipper->Release();
    }
#endif
    return 1;
}

// FUNCTION: LOCO 0x45b940
// Re-points the primary surface's clipper at `hwnd`. The GetClipper/CreateClipper-else-detach
// pair is Ddraw_Init's own clipper block verbatim (0x45b500 above), just parameterised on the
// window instead of always taking g_pApp->hwndOwner: either the surface has no clipper yet and
// one is created, or it has one and is detached from it first so SetHWnd cannot race a live
// binding. The trailing Release drops the reference GetClipper/CreateClipper handed over --
// after SetClipper the surface holds its own.
// Every popup that shows, hides or moves calls this (src/PopupWndBase.cpp, src/EditCardWnd.cpp)
// so DirectDraw's idea of the visible region follows the window that is actually on top.
void Ddraw_RebindWindowClipper(HWND hwnd)
{
    IDirectDrawClipper *pClipper = NULL;

    if (g_pDDrawPrimarySurface->GetClipper(&pClipper) == DDERR_NOCLIPPERATTACHED) {
        g_pDDraw2->CreateClipper(0, &pClipper, NULL);
    } else {
        g_pDDrawPrimarySurface->SetClipper(NULL);
    }
    pClipper->SetHWnd(0, hwnd);
    g_pDDrawPrimarySurface->SetClipper(pClipper);
    pClipper->Release();
}

// FUNCTION: LOCO 0x45b9b0
// Fetches a surface's DDSURFACEDESC and, when asked, republishes the process-wide pixel-format
// globals from it. Both 15bpp (5-5-5) and 16bpp (5-6-5) are supported and the whole distinction
// is made by one test on the red channel mask; nothing here reads the DDPIXELFORMAT's own
// dwRGBBitCount. The five call sites that pass bUpdateGlobals == 0 (src/PopupWndBase.cpp,
// src/WindowBase.cpp, src/EditCardWnd.cpp) just want the descriptor filled in.
// ⚠ The `pDesc` the callers hand over is the inner DDSURFACEDESC of a DDSurfaceDescPadded0x7c --
// see src/LocoBitmap.h for why that scratch is 0x10 bytes larger than this toolchain's real
// sizeof(DDSURFACEDESC).
void Ddraw_QuerySurfacePixelFormat(IDirectDrawSurface *pSurface, DDSURFACEDESC *pDesc,
                                   char bUpdateGlobals)
{
    pSurface->GetSurfaceDesc(pDesc);
    if (bUpdateGlobals) {
        unsigned short wGuardMask;

        if (pDesc->ddpfPixelFormat.dwRBitMask == 0x7c00) {
            g_nSurfaceFormatTag = 0x22b;
            g_nRedShiftPos = 10;
            g_nGreenWidth = 5;
            wGuardMask = 0x3def;
        } else {
            g_nSurfaceFormatTag = 0x235;
            g_nRedShiftPos = 11;
            g_nGreenWidth = 6;
            wGuardMask = 0x7bef;
        }
        g_wChannelBleedGuardMask = wGuardMask;
        DAT_00485284 = wGuardMask;
        g_nRBitMask = pDesc->ddpfPixelFormat.dwRBitMask;
        g_nGBitMask = pDesc->ddpfPixelFormat.dwGBitMask;
        g_nBBitMask = pDesc->ddpfPixelFormat.dwBBitMask;
    }
}

// FUNCTION: LOCO 0x45ba50
// Stamp the family's transparency colorkey (magenta) onto a surface, in whichever of the two
// supported pixel formats is live. The caller-owned pScratch is a DDSURFACEDESC the callee
// overwrites and then ignores -- the GetSurfaceDesc call is there purely to make the surface
// resolve/restore itself before the key is set, and every caller just hands over whatever
// descriptor buffer it already had lying around.
//
// ⚠ sic: when g_nSurfaceFormatTag is NEITHER 0x22b nor 0x235 the DDCOLORKEY is left
// UNINITIALIZED and handed to SetColorKey anyway -- the original has no else arm and no
// initializer (verified byte-wise: both stores live on the shared cross-jumped tail at 0x45ba82,
// which the no-match path branches straight past). Unreachable in practice, since
// Ddraw_QuerySurfacePixelFormat above can only ever publish those two values. Reproduced, not
// fixed.
void LocoBitmap_SetColorKey(void *pSurface, void *pScratch)
{
    DDCOLORKEY ck;

    ((IDirectDrawSurface *)pSurface)->GetSurfaceDesc((DDSURFACEDESC *)pScratch);
    if (g_nSurfaceFormatTag == 0x22b) {
        ck.dwColorSpaceLowValue = 0x7c1f;
        ck.dwColorSpaceHighValue = 0x7c1f;
    } else if (g_nSurfaceFormatTag == 0x235) {
        ck.dwColorSpaceLowValue = 0xf81f;
        ck.dwColorSpaceHighValue = 0xf81f;
    }
    ((IDirectDrawSurface *)pSurface)->SetColorKey(DDCKEY_SRCBLT, &ck);
}

// FUNCTION: LOCO 0x45baa0
// Symmetric counterpart to Ddraw_Init: drops both surfaces, the thumbnail loader's cached
// palette singletons, and finally the two DirectDraw objects -- each store nulled as it goes, so
// a second call is a no-op. The whole body is gated on the outer IDirectDraw ever having been
// created, which is what makes it safe to call from the failure paths of a partial Init.
void Ddraw_Teardown()
{
    if (g_pDDraw != NULL) {
        if (g_pDDrawPrimarySurface != NULL) {
            g_pDDrawPrimarySurface->Release();
            g_pDDrawPrimarySurface = NULL;
        }
        if (g_pDDrawWorkSurface != NULL) {
            g_pDDrawWorkSurface->Release();
            g_pDDrawWorkSurface = NULL;
        }
        LocoBitmap_ReleaseThumbPalSingletonMaybe();
        if (g_pDDraw2 != NULL) {
            g_pDDraw2->SetCooperativeLevel(NULL, DDSCL_NORMAL);
            g_pDDraw2->Release();
            g_pDDraw2 = NULL;
        }
        g_pDDraw->Release();
        g_pDDraw = NULL;
    }
}

// FUNCTION: LOCO 0x45c970
// Drops the thumbnail loader's cached singletons -- the six dead COM pointers above, then the
// thumbnail-palette bitmap itself. Ddraw_Teardown is the only caller. Every arm nulls its global
// as it goes, so a second call is a no-op, exactly like Ddraw_Teardown's own surface arms.
// ⚠ The six COM arms are dead code in the shipped binary: no function in .text ever stores a
// non-null value into any of the six, so every `if` here is always false at runtime. Reproduced
// as-is (the original's release path is what it is), and the source order below is the original's
// -- note 0x4ff0f8, the LOWEST address, is released LAST.
void LocoBitmap_ReleaseThumbPalSingletonMaybe()
{
    if (DAT_004ff0fc != NULL) {
        DAT_004ff0fc->Release();
        DAT_004ff0fc = NULL;
    }
    if (DAT_004ff100 != NULL) {
        DAT_004ff100->Release();
        DAT_004ff100 = NULL;
    }
    if (DAT_004ff104 != NULL) {
        DAT_004ff104->Release();
        DAT_004ff104 = NULL;
    }
    if (DAT_004ff108 != NULL) {
        DAT_004ff108->Release();
        DAT_004ff108 = NULL;
    }
    if (DAT_004ff10c != NULL) {
        DAT_004ff10c->Release();
        DAT_004ff10c = NULL;
    }
    if (DAT_004ff0f8 != NULL) {
        DAT_004ff0f8->Release();
        DAT_004ff0f8 = NULL;
    }
    if (DAT_004ff110 != NULL) {
        delete DAT_004ff110;
        DAT_004ff110 = NULL;
    }
}

// FUNCTION: LOCO 0x45bbc0
// Ddraw_HResultToString -- maps a DirectDraw/Direct3D HRESULT to a static English
// description for error logging. One big sparse switch (129 cases + default), clearly
// SDK-sample-derived. All returns are string literals (verify masks the relocations).
//
// EFFECTIVE MATCH (residual class: VC5 sparse-switch cluster-merge / two-level-table).
// The case SET is byte-verified identical to the original: the original's 61 sweep
// `cmp eax,imm` values + its 4 jump tables' 68 non-default entries union to exactly
// these 129 cases. The first 251 instructions (through the 0x8876017c-family table)
// byte-match. The divergence is ONE lowering choice in the 0x88760208..0x88760319
// region: the original keeps a pairwise sweep + 3 direct dword jump tables (spans
// 29/32/13) there, while this c2 always merges the whole region into a single
// byte-index-compressed two-level table. Refuted levers (docs/PARKED.md row): source
// case order (sorted either way, shuffled -- clustering is order-independent), VC5
// SP3 c1/c1xx/c2 from the vs97sp3 cabs (identical .obj), VC4.2 c2, every flag combo
// tried (/O1 /O2 /Os /Ot /Od /Og- /G3 /G4 /G5 /G6 /GB), C vs C++ frontend.
char *Ddraw_HResultToString(int hr)
{
    switch (hr) {
    case -0x7fffbfff: return "Action not supported.";
    case -0x7fffbffb: return "Generic failure.";
    case -0x7ff8fff2: return "DirectDraw does not have enough memory to perform the operation.";
    case -0x7ff8ffa9: return "One or more of the parameters passed to the function are incorrect.";
    case -0x7789fffb: return "This object is already initialized.";
    case -0x7789fff6: return "This surface can not be attached to the requested surface.";
    case -0x7789ffec: return "This surface can not be detached from the requested surface.";
    case -0x7789ffd8: return "Support is currently not available.";
    case -0x7789ffc9: return "An exception was encountered while performing the requested operation.";
    case -0x7789ffa6: return "Height of rectangle provided is not a multiple of reqd alignment.";
    case -0x7789ffa1: return "Unable to match primary surface creation request with existing primary surface.";
    case -0x7789ff9c: return "One or more of the caps bits passed to the callback are incorrect.";
    case -0x7789ff92: return "DirectDraw does not support the provided cliplist.";
    case -0x7789ff88: return "DirectDraw does not support the requested mode.";
    case -0x7789ff7e: return "DirectDraw received a pointer that was an invalid DIRECTDRAW object.";
    case -0x7789ff6f: return "The pixel format was invalid as specified.";
    case -0x7789ff6a: return "Rectangle provided was invalid.";
    case -0x7789ff60: return "Operation could not be carried out because one or more surfaces are locked.";
    case -0x7789ff56: return "There is no 3D present.";
    case -0x7789ff4c: return "Operation could not be carried out because there is no alpha accleration hardware present or available.";
    case -0x7789ff33: return "No cliplist available.";
    case -0x7789ff2e: return "Operation could not be carried out because there is no color conversion hardware present or available.";
    case -0x7789ff2c: return "Create function called without DirectDraw object method SetCooperativeLevel being called.";
    case -0x7789ff29: return "Surface doesn't currently have a color key";
    case -0x7789ff24: return "Operation could not be carried out because there is no hardware support of the destination color key.";
    case -0x7789ff1f: return "Operation requires the application to have exclusive mode but the application does not have exclusive mode.";
    case -0x7789ff1a: return "Flipping visible surfaces is not supported.";
    case -0x7789ff10: return "There is no GDI present.";
    case -0x7789ff06: return "Operation could not be carried out because there is no hardware present or available.";
    case -0x7789ff01: return "Requested item was not found.";
    case -0x7789fefc: return "Operation could not be carried out because there is no overlay hardware present or available.";
    case -0x7789fee8: return "Operation could not be carried out because there is no appropriate raster op hardware present or available.";
    case -0x7789fede: return "Operation could not be carried out because there is no rotation hardware present or available.";
    case -0x7789feca: return "Operation could not be carried out because there is no hardware support for stretching.";
    case -0x7789fec4: return "DirectDrawSurface is not in 4 bit color palette and the requested operation requires 4 bit color palette.";
    case -0x7789fec3: return "DirectDrawSurface is not in 4 bit color index palette and the requested operation requires 4 bit color index palette.";
    case -0x7789fec0: return "DirectDrawSurface is not in 8 bit color mode and the requested operation requires 8 bit color.";
    case -0x7789feb6: return "Operation could not be carried out because there is no texture mapping hardware present or available.";
    case -0x7789feb1: return "Operation could not be carried out because there is no hardware support for vertical blank synchronized operations.";
    case -0x7789feac: return "Operation could not be carried out because there is no hardware support for zbuffer blitting.";
    case -0x7789fea2: return "Overlay surfaces could not be z layered based on their BltOrder because the hardware does not support z layering of overlays.";
    case -0x7789fe98: return "The hardware needed for the requested operation has already been allocated.";
    case -0x7789fe84: return "DirectDraw does not have enough memory to perform the operation.";
    case -0x7789fe82: return "The hardware does not support clipped overlays.";
    case -0x7789fe80: return "Can only have ony color key active at one time for overlays.";
    case -0x7789fe7d: return "Access to this palette is being refused because the palette is already locked by another thread.";
    case -0x7789fe70: return "No src color key specified for this operation.";
    case -0x7789fe66: return "This surface is already attached to the surface it is being attached to.";
    case -0x7789fe5c: return "This surface is already a dependency of the surface it is being made a dependency of.";
    case -0x7789fe52: return "Access to this surface is being refused because the surface is already locked by another thread.";
    case -0x7789fe48: return "Access to surface refused because the surface is obscured.";
    case -0x7789fe3e: return "Access to this surface is being refused because the surface memory is gone. The DirectDrawSurface object representing this surface should have Restore called on it.";
    case -0x7789fe34: return "The requested surface is not attached.";
    case -0x7789fe2a: return "Height requested by DirectDraw is too large.";
    case -0x7789fe20: return "Size requested by DirectDraw is too large, but the individual height and width are OK.";
    case -0x7789fe16: return "Width requested by DirectDraw is too large.";
    case -0x7789fe02: return "FOURCC format requested is unsupported by DirectDraw.";
    case -0x7789fdf8: return "Bitmask in the pixel format requested is unsupported by DirectDraw.";
    case -0x7789fde7: return "Vertical blank is in progress.";
    case -0x7789fde4: return "Informs DirectDraw that the previous Blt which is transfering information to or from this Surface is incomplete.";
    case -0x7789fdd0: return "Rectangle provided was not horizontally aligned on required boundary.";
    case -0x7789fdcf: return "The GUID passed to DirectDrawCreate is not a valid DirectDraw driver identifier.";
    case -0x7789fdce: return "A DirectDraw object representing this driver has already been created for this process.";
    case -0x7789fdcd: return "A hardware-only DirectDraw object creation was attempted but the driver did not support any hardware.";
    case -0x7789fdcc: return "This process already has created a primary surface.";
    case -0x7789fdcb: return "Software emulation not available.";
    case -0x7789fdca: return "Region passed to Clipper::GetClipList is too small.";
    case -0x7789fdc9: return "An attempt was made to set a cliplist for a clipper object that is already monitoring an hwnd.";
    case -0x7789fdc8: return "No clipper object attached to surface object.";
    case -0x7789fdc7: return "Clipper notification requires an HWND or no HWND has previously been set as the CooperativeLevel HWND.";
    case -0x7789fdc6: return "HWND used by DirectDraw CooperativeLevel has been subclassed, this prevents DirectDraw from restoring state.";
    case -0x7789fdc5: return "The CooperativeLevel HWND has already been set. It can not be reset while the process has surfaces or palettes created.";
    case -0x7789fdc4: return "No palette object attached to this surface.";
    case -0x7789fdc3: return "No hardware support for 16 or 256 color palettes.";
    case -0x7789fdc2: return "Return if a clipper object is attached to the source surface passed into a BltFast call.";
    case -0x7789fdc1: return "No blitter hardware present.";
    case -0x7789fdc0: return "No DirectDraw ROP hardware.";
    case -0x7789fdbf: return "Returned when GetOverlayPosition is called on a hidden overlay.";
    case -0x7789fdbe: return "Returned when GetOverlayPosition is called on an overlay that UpdateOverlay has never been called on to establish a destination.";
    case -0x7789fdbd: return "Returned when the position of the overlay on the destination is no longer legal for that destination.";
    case -0x7789fdbc: return "Returned when an overlay member is called for a non-overlay surface.";
    case -0x7789fdbb: return "An attempt was made to set the cooperative level when it was already set to exclusive.";
    case -0x7789fdba: return "An attempt has been made to flip a surface that is not flippable.";
    case -0x7789fdb9: return "Can't duplicate primary & 3D surfaces, or surfaces that are implicitly created.";
    case -0x7789fdb8: return "Surface was not locked.  An attempt to unlock a surface that was not locked at all, or by this process, has been attempted.";
    case -0x7789fdb7: return "Windows can not create any more DCs.";
    case -0x7789fdb6: return "No DC was ever created for this surface.";
    case -0x7789fdb5: return "This surface can not be restored because it was created in a different mode.";
    case -0x7789fdb4: return "This surface can not be restored because it is an implicitly created surface.";
    case -0x7789fdb3: return "The surface being used is not a palette-based surface.";
    case -0x7789fd44: return "D3DERR_BADMAJORVERSION";
    case -0x7789fd43: return "D3DERR_BADMINORVERSION";
    case -0x7789fd3a: return "D3DERR_EXECUTE_CREATE_FAILED";
    case -0x7789fd39: return "D3DERR_EXECUTE_DESTROY_FAILED";
    case -0x7789fd38: return "D3DERR_EXECUTE_LOCK_FAILED";
    case -0x7789fd37: return "D3DERR_EXECUTE_UNLOCK_FAILED";
    case -0x7789fd36: return "D3DERR_EXECUTE_LOCKED";
    case -0x7789fd35: return "D3DERR_EXECUTE_NOT_LOCKED";
    case -0x7789fd34: return "D3DERR_EXECUTE_FAILED";
    case -0x7789fd33: return "D3DERR_EXECUTE_CLIPPED_FAILED";
    case -0x7789fd30: return "D3DERR_TEXTURE_NO_SUPPORT";
    case -0x7789fd2f: return "D3DERR_TEXTURE_CREATE_FAILED";
    case -0x7789fd2e: return "D3DERR_TEXTURE_DESTROY_FAILED";
    case -0x7789fd2d: return "D3DERR_TEXTURE_LOCK_FAILED";
    case -0x7789fd2c: return "D3DERR_TEXTURE_UNLOCK_FAILED";
    case -0x7789fd2b: return "D3DERR_TEXTURE_LOAD_FAILED";
    case -0x7789fd29: return "D3DERR_TEXTURELOCKED";
    case -0x7789fd28: return "D3DERR_TEXTURE_NOT_LOCKED";
    case -0x7789fd26: return "D3DERR_MATRIX_CREATE_FAILED";
    case -0x7789fd25: return "D3DERR_MATRIX_DESTROY_FAILED";
    case -0x7789fd24: return "D3DERR_MATRIX_SETDATA_FAILED";
    case -0x7789fd22: return "D3DERR_SETVIEWPORTDATA_FAILED";
    case -0x7789fd1c: return "D3DERR_MATERIAL_CREATE_FAILED";
    case -0x7789fd1b: return "D3DERR_MATERIAL_DESTROY_FAILED";
    case -0x7789fd1a: return "D3DERR_MATERIAL_SETDATA_FAILED";
    case -0x7789fd12: return "D3DERR_LIGHT_SET_FAILED";
    case -0x7789fcf3: return "D3DRMERR_BADOBJECT";
    case -0x7789fcf2: return "D3DRMERR_BADTYPE";
    case -0x7789fcf1: return "D3DRMERR_BADALLOC";
    case -0x7789fcf0: return "D3DRMERR_FACEUSED";
    case -0x7789fcef: return "D3DRMERR_NOTFOUND";
    case -0x7789fcee: return "D3DRMERR_NOTDONEYET";
    case -0x7789fced: return "The file was not found.";
    case -0x7789fcec: return "D3DRMERR_BADFILE";
    case -0x7789fceb: return "D3DRMERR_BADDEVICE";
    case -0x7789fcea: return "D3DRMERR_BADVALUE";
    case -0x7789fce9: return "D3DRMERR_BADMAJORVERSION";
    case -0x7789fce8: return "D3DRMERR_BADMINORVERSION";
    case -0x7789fce7: return "D3DRMERR_UNABLETOEXECUTE";
    default: return "Unrecognized error value.";
    }
}
