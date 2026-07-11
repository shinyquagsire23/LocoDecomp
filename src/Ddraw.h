#pragma once

// ⚠ Self-contained on purpose (v477): the four singleton declarations below name IDirectDraw /
// IDirectDraw2 / IDirectDrawSurface, so this header pulls in <ddraw.h> itself instead of
// relying on consumer include order. Byte-neutral by construction -- every TU reaching this
// header already includes both ahead of it, so both guards make these expand to nothing.
#include <windows.h>
#include <ddraw.h>

// Ddraw -- the game's DirectDraw/Direct3D wrapper subsystem (surface management,
// clipper rebinding, update blits; see docs/subsystems.md).

// The subsystem's four COM singletons. This is their CANONICAL home; eight consumer TUs
// (AlbumCardWnd, ApplSetupWnd, AppWindow, CreditsWnd, EditCardWnd, LocoBitmap, MapWnd,
// NetSetupWnd, PopupWndBase) still carry their own file-local `extern` copies -- pre-existing
// debt, folded in a later cleanup pass rather than here, since a decl move has to be
// byte-audited across all of them at once.
//
// ⚠ g_pDDraw2 is really an **IDirectDraw4**, not an IDirectDraw2: Ddraw_Init's QueryInterface
// passes IID_IDirectDraw4 (9c59509a-39bd-11d1-8c4a-00c04fd930c5, at 0x4785e8) and its
// CreateSurface descriptor is the 124-byte DDSURFACEDESC2, not the 108-byte DDSURFACEDESC. The
// VC5-bundled toolchain/vc50/INCLUDE/DDRAW.H is DX2/3-era and declares neither type, so the
// name kept here is the one the rest of the repo already uses. Every method any transcribed TU
// calls sits at the SAME vtable offset in both interfaces, so this costs no bytes -- but see
// Ddraw_Init's note in src/Ddraw.cpp before adding anything version-sensitive.
//
// The one thing the DX2/3-era header genuinely cannot supply is the interface's own IID, so it
// is spelled out here from the 16 bytes at 0x4785e8 (read out of the image, not off a Ghidra
// label). Everything else DX5 about Ddraw_Init -- the 124-byte descriptor, the vtable slots --
// the existing types already cover exactly; see Ddraw_Init's own note in src/Ddraw.cpp.
DEFINE_GUID(IID_IDirectDraw4, 0x9c59509a, 0x39bd, 0x11d1, 0x8c, 0x4a, 0x00, 0xc0, 0x4f, 0xd9,
            0x30, 0xc5);

extern IDirectDraw *g_pDDraw;                       // DAT_004a9908, from DirectDrawCreate
extern IDirectDraw2 *g_pDDraw2;                     // DAT_00485440, really IDirectDraw4
extern IDirectDrawSurface *g_pDDrawPrimarySurface;  // DAT_004fd3c0
extern IDirectDrawSurface *g_pDDrawWorkSurface;     // DAT_004fd3c4

// The live surface pixel-format description, written once at startup by Ddraw_Init (0x45b500)
// and again by every Ddraw_QuerySurfacePixelFormat(.., 1) call. This is their CANONICAL home;
// src/LocoBitmap.cpp, src/EditCardWnd.cpp and src/DDrawSurface.h still carry their own
// file-local `extern` copies of the ones they read -- pre-existing debt, same fold as the four
// singletons above, and for the same reason (a decl move has to be byte-audited across every
// consumer at once).
extern "C" {
    // 0x22b = 555bpp (5-5-5), 0x235 = 565bpp. Every consumer keys off this tag rather than
    // re-reading the DDPIXELFORMAT.
    extern int g_nSurfaceFormatTag;                  // DAT_00485274
    extern int g_nRedShiftPos;                       // DAT_00485278 -- 10 (555) or 11 (565)
    extern int g_nGreenWidth;                        // DAT_0048527c -- 5 (555) or 6 (565)
    // The post->>1 channel-bleed guard mask, 0x3def@555 / 0x7bef@565: the bits that survive a
    // per-channel halving without bleeding across a channel boundary.
    extern unsigned short g_wChannelBleedGuardMask;  // DAT_00485280
    // ⚠ A second copy of the guard mask, stored by both writers immediately after the real one
    // and read by NOTHING in the whole image (confirmed by xref sweep: 2 refs, both WRITEs).
    // Dead in the shipped binary, so its purpose is unrecoverable -- it keeps its Ghidra name.
    extern unsigned short DAT_00485284;              // DAT_00485284
    // The raw DDPIXELFORMAT channel masks, copied verbatim off the surface.
    extern unsigned int g_nRBitMask;                 // DAT_00485288
    extern unsigned int g_nGBitMask;                 // DAT_0048528c
    extern unsigned int g_nBBitMask;                 // DAT_00485290
}

char *Ddraw_HResultToString(int hr);  // 0x45bbc0

// 0x45baa0 -- releases the surfaces and drops the DirectDraw objects. Defined in src/Ddraw.cpp.
void Ddraw_Teardown();

// 0x45b500 -- brings DirectDraw up (mode, surfaces, clipper); returns whether it succeeded.
// The first thing UIResources::Init (0x446050) does, and its failure is that function's own
// early-out. Declared-only -- see src/Ddraw.cpp for why.
unsigned char Ddraw_Init();

// 0x45c970 -- releases the cached palette/surface singletons the thumbnail loader owns
// (0x4ff0f8..0x4ff110). Ddraw_Teardown is its only caller, which is why it is declared here and
// not next to LocoBitmap_LoadThumbPalSingletonMaybe (0x45c8a0, its only sibling, itself still
// declared file-locally in src/AppWindow.cpp). Declared-only.
void LocoBitmap_ReleaseThumbPalSingletonMaybe();
