# `port/` — the LOCO_PORT configuration

Scaffolding for running the decompiled game on a modern host (wine/CrossOver on
macOS, in practice). **Nothing here is part of the byte-match product**, and no
`#ifdef LOCO_PORT` block may change the default build's token stream — with the
macro off the preprocessor deletes every one of them, so `tools/progress.py`'s
numbers are unmovable by construction. Prove it anyway after touching one.

Build: `tools/build_port.sh` → `build/Loco-port.exe`.
Default byte-match build is unchanged: `tools/progress.py`, `tools/link_check.sh`.

## The problem

LEGO LOCO wants a **16bpp desktop** and refuses to start without one.
`AppWindow::CheckMinimumDisplaySpec` (`0x406680`) reads `GetDeviceCaps(BITSPIXEL)`,
and on anything above 16bpp puts up string `0x7a` — *"LEGO LOCO only runs in High
Colour (16-bit) 800x600, 1024x768 and 1280x1024"* — and aborts.

That check is not arbitrary. The renderer is 16bpp all the way down:

* every `LocoBitmap` blit is a hand-written 16-bit `Lock` loop;
* transparency is a magenta colour key — `0x7c1f` in 555, `0xf81f` in 565;
* `Ddraw_Init` reads the work surface's `dwRBitMask` back and publishes
  `g_nSurfaceFormatTag` / `g_nRedShiftPos` / `g_nGreenWidth` / the channel-bleed
  guard mask, which the whole engine then reads. There are exactly two arms,
  555 and 565. **There is no 32bpp arm anywhere.**

So "make it 32bpp" is not a switch anyone can flip; it would be a rewrite of the
blitter. And the usual host-side escapes do not apply either: the game never
calls `SetDisplayMode` (it is *already* windowed — `DDSCL_NORMAL` plus a clipper
on the primary is the non-screensaver default), so a DirectDraw wrapper has
nothing to intercept, and its primary surface is whatever the desktop is.

## The approach

Keep the engine entirely 16bpp; stop it from ever touching the real desktop format.

1. **Pin every offscreen surface to RGB565.** No `CreateSurface` site in the repo
   passes `DDSD_PIXELFORMAT`, so they all inherit the primary's format. Under
   `LOCO_PORT` each one gets an explicit 565 format instead (`Port_ForceRgb565`).
   The 555-vs-565 republish in `Ddraw_Init` then reads 565 back off the surface
   and picks tag `0x235` exactly as it would on a real 565 desktop — nothing
   downstream can tell the difference.

2. **Emulate the primary.** `g_pDDrawPrimarySurface` becomes a screen-sized
   system-memory 565 offscreen surface. This is what makes the approach cheap:
   `Ddraw_BltUpdateRect` is *not* the only writer of the primary —
   `src/PopupWndBase.cpp` blits to it directly from ~10 sites — and every one of
   those keeps working unchanged, now 565→565, so no converting blit is ever
   asked of DirectDraw (which is exactly the operation wrappers handle worst).

3. **Present with GDI.** `FrameDriver_TickMaybe` (the per-frame pump) ends with
   `Port_Present()`, which locks the emulated primary and `StretchDIBits`es it
   into the window with a `BI_BITFIELDS` 565 header. GDI does the conversion to
   whatever the desktop actually is. Presenting from the frame pump rather than
   from any single blit site is deliberate, for the reason in (2).

4. **Relax the gate.** `CheckMinimumDisplaySpec` accepts any non-palette desktop
   of ≥16bpp. 8bpp is still rejected: the engine has no 8bpp path at all.

## The virtual screen (`PortWinShim.cpp`)

The original needed no window management at all: the main window is created
screen-sized at the origin as a borderless popup, so screen coordinates — which is
what the primary blits use, via `ClientToScreen` — equal client coordinates. On a
modern host that is false, and it is false in a way that reaches everything. The
engine's fullscreen style carries `WS_MAXIMIZE`, and winemac snaps a maximized
window into the WORK AREA below the macOS menu bar (measured y=78), so the client
origin moves and every frame is drawn displaced and clipped.

Forcing the window back to screen (0,0) works and is the wrong answer: it leaves a
borderless window with no titlebar to grab, half of it under the menu bar, and the
correction has to be re-applied forever because `AppWindow_ApplyDisplayModeMaybe`
re-styles the window on every display-mode switch.

So the port redefines the screen instead. **"Screen coordinates" mean the MAIN
WINDOW'S CLIENT COORDINATES** — a virtual screen that travels with the frame,
wherever the user drags it. The engine keeps every assumption it already has, and
the window becomes an ordinary titled, movable one.

The translation lives entirely at the OS boundary. `PortWinShim.cpp` DEFINES ten
user32 entry points (`GetCursorPos`, `SetCursorPos`, `ClientToScreen`,
`ScreenToClient`, `WindowFromPoint`, `GetWindowRect`, `SetWindowPos`, `MoveWindow`,
`CreateWindowExA`, `SetWindowLongA`), converts, and calls through to the real export
it looked up by name. `tools/build_port.sh` compiles the port with `/D _USER32_`,
which makes `WINUSER.H` drop `__declspec(dllimport)` so calls compile to
`call _GetCursorPos@4` rather than `call [__imp__GetCursorPos@4]`; the linker
searches objects before import libraries, so every call site in every TU routes here
**without a single `#ifdef` in `src/`**, and the byte-match build is untouched by
construction. Confirm with `link/Loco-port.map`: each intercepted name must resolve
to `PortWinShim.obj`, and `GetClientRect` — deliberately NOT intercepted, since a
client rect needs no translation — to `user32`.

Two things the shim owns beyond the arithmetic:

* **The main window** is created with a caption instead of the engine's borderless
  popup, and the engine's own attempts to move, resize or re-style it are ignored, so
  the user stays in charge of where it sits. Its CLIENT size is still exactly what the
  engine asked for, which is what keeps `GetClientRect` agreeing with the primary.
* **Popup placement** is re-asserted once per present. The front-end screens are
  created at the right place and never moved by `SetWindowPos`, yet still come back
  displaced — winemac nudges a screen-sized window when it is first shown, invisibly
  to every intercept. Since `Ddraw_BltUpdateRect` paints into the primary at each
  window's own client origin, one nudged popup drags the whole front end with it.

## Touched byte-match files

| file | what the `#ifdef LOCO_PORT` does |
|---|---|
| `src/AppWindow.cpp` | `CheckMinimumDisplaySpec` accepts ≥16bpp instead of exactly ≤16bpp |
| `src/Ddraw.cpp` | emulated primary; 565 work surface; skip the clipper (offscreen surfaces take none) |
| `src/FrameDriver.cpp` | `Port_Present()` at the end of the frame tick |
| `src/DDrawSurface.cpp`, `src/CreditsWnd.cpp`, `src/EditCardWnd.cpp` | `Port_ForceRgb565` on their `CreateSurface` descriptors |

## Status

**Compiles and links; not yet runtime-verified.** `build/Loco-port.exe` is built
from the same stub set as `build/Loco-linked.exe`, which still has 405 generated
stubs (245 of them code), so neither exe reaches a playable state yet — the port
path cannot be exercised end to end until that stub count comes down. The design
above is what the code does; whether wine's DirectDraw accepts a 565 offscreen
surface on a 32bpp desktop is the first thing to check when it can be run.

If `CreateSurface` with an explicit 565 format is refused by some host, the
fallback is to drop DirectDraw for offscreen storage entirely and back every
surface with a plain system-memory buffer — a much bigger change, but the only
other way to keep the 16bpp blitter intact.
