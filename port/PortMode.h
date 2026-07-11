// PORT SCAFFOLDING -- compiled only in a `-D LOCO_PORT` build, never part of the
// byte-match product. See port/README.md for the whole design.
//
// LEGO LOCO refuses to start unless the DESKTOP is 16bpp (AppWindow::
// CheckMinimumDisplaySpec, 0x406680, puts up string 0x7a and aborts), and its
// renderer is 16bpp all the way down: every LocoBitmap blit is a hand-written
// 16-bit Lock loop, transparency is a magenta colour key (0x7c1f in 555, 0xf81f
// in 565), and Ddraw_Init publishes 555-vs-565 shifts/masks into globals the
// whole engine reads. So "make it 32bpp" is not a switch anyone can flip.
//
// What this port does instead: keep the engine entirely 16bpp and stop it from
// ever touching the real desktop format.
//   * every OFFSCREEN surface is created with an explicit RGB565 pixel format
//     rather than inheriting the primary's, so all that 16bpp maths stays valid;
//   * g_pDDrawPrimarySurface becomes an EMULATED primary -- a plain system-memory
//     RGB565 offscreen surface the size of the screen. Every existing
//     `g_pDDrawPrimarySurface->Blt(...)` (Ddraw_BltUpdateRect plus ~10 sites in
//     PopupWndBase) then keeps working unchanged, and is 565->565, which needs no
//     format conversion from anyone;
//   * once a frame, Port_Present() copies that emulated primary into the window
//     with StretchDIBits, and GDI does the 565 -> desktop-depth conversion.
//
// The game window is already created screen-sized at the origin as a borderless
// popup (AppWindow::CreateMainWindow), and DDSCL_NORMAL windowed is already the
// non-screensaver default -- so this needs no window-management changes at all,
// only the pixel format and the present.
#pragma once

#include <windows.h> // must precede ddraw.h -- it supplies the base Win32 types
#include <ddraw.h>

// Clamp the desktop size the game will treat AS the screen, in place.
//
// CheckMinimumDisplaySpec rejects any desktop wider than 1280 or narrower than
// 800 (src/AppWindow.cpp) -- a 1998 sanity check that every modern display fails,
// and the second gate (after the 16bpp one) standing between this build and a
// frame. The port is windowed and presents through GDI, so the game's "screen"
// no longer has to BE the desktop: clamping here gives it a supported size, and
// the main window -- created at 0,0,g_dwScreenWidth,g_dwScreenHeight -- simply
// comes out that size. Everything downstream (the emulated primary, the viewport
// maths, Port_Present) already derives from these two globals.
//
// Override with LOCO_PORT_SIZE=WxH in the environment; out-of-range values are
// clamped rather than rejected.
void Port_ClampScreenSize(int *pnWidth, int *pnHeight);

// Clamp a rect that a window's Create just filled from
// `GetClientRect(GetDesktopWindow(), &rect)`, in place, to the same size
// Port_ClampScreenSize handed the two screen globals.
//
// Found v569. SEVEN sites in src/ build a full-screen window that way -- SplashWnd,
// NetSetupWnd, ApplSetupWnd, MailWnd, EditCardWnd (twice) and the shared
// FullscreenPopupWndPartial::CreateFullscreenPopupWnd that MapWnd and AlbumCardWnd
// go through. In 1998 the desktop client rect WAS the screen and every one of them
// was right; here the desktop is whatever the host says (3600x2338 under winemac)
// while every surface the game paints into is g_dwScreenWidth x g_dwScreenHeight.
//
// It is not cosmetic. WindowBase::RefreshClientClipRect copies the client rect
// straight into rectClipBounds, and WindowBase::CommitScreenUpdate presents THAT
// rect out of the work surface whenever pUpdateRect is NULL -- which is where the
// front end's `blt ok src=0,0,3600,2338` out of a 1024x768 surface came from, and
// with it a wholly black interactive boot. The same oversized rect reaching
// LocoBitmap::RestoreOverlapBlt is the documented v562 heap-corruption mechanism
// (that path does NO clipping unless the caller passes flag 0x40).
//
// Deliberately takes the rect rather than returning the size, so a call site stays
// three lines and keeps the original's own `rect.right - rect.left` arithmetic
// intact. Preserves left/top; only the extent is capped.
void Port_ClampDesktopRect(RECT *pRect);

// Stamp an explicit RGB565 pixel format onto a CreateSurface descriptor. Call on
// every OFFSCREEN descriptor; never on the primary's.
void Port_ForceRgb565(DDSURFACEDESC *pDesc);

// Create the emulated primary and remember the window to present it to. Returns
// the surface to store in g_pDDrawPrimarySurface, or NULL on failure.
IDirectDrawSurface *Port_CreateEmulatedPrimary(IDirectDraw2 *pDDraw, HWND hwnd,
                                               int nWidth, int nHeight);

// Blit the emulated primary into the window. Cheap no-op until
// Port_CreateEmulatedPrimary has succeeded.
void Port_Present(void);

// The same blit onto a caller-supplied DC, for use inside a BeginPaint/EndPaint
// cycle. Port_Present's own GetDC blit is correct but transient: anything that
// repaints the window afterwards (a WM_PAINT satisfied by DefWindowProc, or the
// winemac driver flushing its backing store) overwrites it, which showed up as a
// white window with the presented frame flickering past on the way out. Painting
// from inside WM_PAINT makes the frame part of the window's own paint cycle, so
// it survives.
void Port_PresentToDC(HWND hwnd, HDC hdc);

// Release the emulated primary (mirrors Ddraw_Teardown).
void Port_Shutdown(void);

// --- Diagnostics --------------------------------------------------------------
// printf-style trace to port_trace.log, written unbuffered through kernel32 for
// the same reason link/stubs.cpp's Stub_Report is: the runs worth reading are the
// ones that end in a fault, where a buffered tail is lost. Scaffolding only --
// every call site in src/ is inside an #ifdef LOCO_PORT block.
extern "C" void Port_Tracef(const char *pszFmt, ...);

// --- Synthetic input ----------------------------------------------------------
// An unattended run has no way to click, and until v571 nothing in this port had ever
// exercised an input path -- so every conclusion about the front end came from what it
// PAINTED. That is a real blind spot: SplashWnd::OnLButtonDown is the only thing that
// leaves the boot-video state (state 0 -> SetState(7)) and the only thing that reaches
// OnEnterCommitAndDispatch and the hand-off into the world.
//
// Rather than teach this layer about SplashWnd, the window registers its own already-
// computed hit rects by name and the script below aims at them. That keeps the port layer
// generic, and -- because the rects are recorded where the engine derives them -- makes the
// logged coordinates ground truth rather than a hand-recomputed guess. Registration alone
// is worth having: the six rects have never been printed before.
//
// Driven by one environment variable, read once:
//   LOCO_PORT_CLICK=N:target[;N:target...]
//     N       present-frame number to fire on (the same counter LOCO_PORT_STAT prints)
//     target  a registered rect NAME (clicked at its centre), or a literal `x,y` in the
//             main window's client space
// e.g. LOCO_PORT_CLICK="60:splash;180:alone;300:enter" -- skip the boot video, toggle
// play-alone, then press Enter. Posts WM_MOUSEMOVE + WM_LBUTTONDOWN + WM_LBUTTONUP, so it
// drives the real window proc; nothing about the state machine is patched or short-circuited.
void Port_RegisterHitRect(const char *pszName, HWND hwnd, const RECT *pRect);

// Fire any script entries due this frame. Called by Port_Present, once per present, after
// the surface lock is dropped.
void Port_AutoInput(void);

// The port's VIRTUAL SCREEN (port/PortWinShim.cpp). Everything the engine calls "screen
// coordinates" is really the MAIN WINDOW'S CLIENT space, so the frame can carry a titlebar
// and be dragged anywhere without the primary, the popups and the software cursor drifting
// apart. These two convert between that space and the host's real screen; the shim applies
// them inside its own user32 intercepts, so no other caller normally needs them.
void Port_ScreenToVirtual(POINT *pt);
void Port_VirtualToScreen(POINT *pt);

// Put every front-end popup back where the engine placed it. Called once per present: the
// window manager displaces them when they are first shown, and nothing the shim intercepts
// sees that happen. See PortWinShim.cpp's popup-placement note.
void Port_ShimReassertWindows(void);

// Heap-overrun toolkit (implemented in link/stubs.cpp, beside the allocator it hooks).
//
// Register an object whose bytes must not be written by anything else -- a window object
// behind a live HWND is the canonical case -- and the allocator will report a delete of, or
// a new landing on, that address. Port_WatchedInRange answers "does this about-to-be-written
// span cover a watched object?", which is what turns a wide 16bpp blit into a named suspect.
//
// This is what identified v562's corruption: with both allocator hooks silent and the vptr
// still turning into pixel data, the only remaining explanation was a blit running past its
// own destination -- and dropping Port_WatchedInRange into the LocoBitmap blit family named
// it in one run. Kept for the next time, since the crash site is never the bad write.
extern "C" void Port_WatchObject(void *p, unsigned int nSize);
extern "C" void *Port_WatchedInRange(void *pBase, unsigned int nBytes);

// Write the emulated primary out as a 24bpp BMP. Returns 0 on failure.
//
// This is the ONLY ground truth about what the engine actually rendered. Whether
// a frame reaches the screen depends on wine's window compositing, which has its
// own failure modes (a borderless screen-sized WS_POPUP under winemac being the
// one that motivated this) -- and when nothing appears, "the engine drew nothing"
// and "the engine drew a frame nobody presented" look identical from the outside.
// A file on disk separates them, and needs no display at all, so it works under a
// headless run.
//
// Driven automatically by Port_Present via two environment variables:
//   LOCO_PORT_DUMP=N   write port_frameNNNN.bmp every Nth present (0/unset = off)
//   LOCO_PORT_STAT=N   log a checksum + non-black pixel count every Nth present
//                      (default 60, 0 = off) -- cheap enough to leave on, and
//                      enough on its own to answer "is anything being drawn?"
extern "C" int Port_DumpFrame(const char *pszPath);
