// Phase 4's 4th TU: LocoBitmap, the general 8bpp-bitmap-to-DDraw-surface class
// (vtable 0x477d28; also used standalone as a small canvas widget, e.g.
// CreditsWnd's animation canvas). See docs/subsystems.md's "LocoBitmap"
// section for the full method list and field layout.
//
// All 28 methods in this TU are transcribed (content-complete); 4/28 are byte-EXACT, the rest
// PARKED (first-draft residuals, not yet byte-matched -- see each function's own header
// comment). The 9-function PixelCopyBlit/PixelCopyColorKeyBlit/MirrorColorKeyBlit/
// Upscale2x2(ColorKey)/Upscale3x3ColorKey/TranslucentBlendBlit/Shadow(Mirror/Interlaced/
// Checkerboard)Blit "blit family" was re-triaged 2026-07-21 (was previously untouched
// first-draft residuals): MirrorColorKeyBlit got a real fix (unmasking copyW/copyH at
// declaration instead of `& 0xffff`-truncating them immediately -- DIFF 169->144 bytes).
// PixelCopyColorKeyBlit's own full-EBP-frame + dead-parameter-slot-reuse shape was
// root-caused FURTHER the same day (pt.2): two genuine source-level fixes (hoisting the
// per-row pointer increments as named locals instead of recomputing them, and caching
// `pPalette` in an explicit local pointer) closed ~19% of its structural score gap --
// but this did NOT generalize to a single shared fix across the family: the `pPalette`-
// caching lever specifically REGRESSED when tried on MirrorColorKeyBlit,
// UpscaleBlit2x2ColorKey, and ShadowBlit (each reverted; see PixelCopyColorKeyBlit's own
// header comment for the full negative-result writeup). The underlying EBP-frame-vs-ESP-frame
// choice itself remains unclosed on every sibling and is now believed to be a genuinely
// per-function, non-source-steerable codegen policy decision (Yoda #29/#30 family) rather than
// one shared, fixable root cause -- don't assume a fix on one leaf transfers to another without
// re-verifying by compile. InterlacedShadowBlit/CheckerboardShadowBlit are a separate, OPPOSITE
// shape (ours has MORE insns than the original) and are a wholly different open question.
//
// Load reuses src/Wav.cpp's proven idioms wholesale: RF-archive-first
// with loose-file ifstream fallback, throw-int error codes caught by a real
// catch(int), g_RFIndex/g_pInstallPathPrefix globals. New this TU:
// BuildPaletteLUT's real signature (recovered from raw disasm at the
// 0x42ad38 call site -- Ghidra's decompile of both caller and callee was
// wrong, matching the CLAUDE.md "caller forwards own params into a
// wrongly-analyzed callee" pattern) passes the just-read BITMAPINFOHEADER BY
// VALUE (a `sub esp,0x28`+`rep movs` stack copy, not a pointer), not fixed
// in Ghidra itself yet (this REST server's set_function_prototype endpoint
// rejects by-value struct params).

#include "LocoBitmap.h"
// DDraw_CreateSurfaceFromFile's canonical C++ declaration. This TU used to spell it
// `extern "C" void *DDraw_CreateSurfaceFromFile(...)` inside the block below, which emitted a
// call to `_DDraw_CreateSurfaceFromFile` -- a symbol src/DDrawSurface.cpp does not define under
// that name and nothing else defines at all. v554 measured the fix at 124 B (it flips
// LocoBitmap::Fill, 0x42aa90, from EXACT to DIFF(2) -- an imul operand-order coin-flip the
// source cannot reach: `height * width`, `width * height`, a cached local, a file-scope
// declaration and restoring the intermediate `pSurf` local were all tried) and PARKED it as
// deliberate wrong-symbol debt. Re-measured v566: the price has NOT expired, still exactly 124 B
// / 1 func. Paid anyway, because the consequence turned out not to be cosmetic.
//
// In the port build an undefined symbol does not fail the link -- link/gen_stubs.py generates
// one. `_DDraw_CreateSurfaceFromFile` has no `@N` suffix, so gen_stubs' is_code() reads it as
// DATA and gives it a slot in the all-zero .bss mirror instead of a reporting code stub. The
// call below therefore jumps into zeroed BSS, on the ordinary path every non-8bpp bitmap asset
// takes, and it cannot even appear in stub_calls.log because a data stub runs no code.
// tools/datastubs.py is what surfaced it. See docs/CODEGEN.md #189.
#include "DDrawSurface.h"
#include "WorldBoardMaybe.h"
#include "DSoundChannel.h" // RFIndex, g_RFIndex, g_pInstallPathPrefix, _free

#include <fstream.h>
#include <strstrea.h>
#include <string.h>
#include <wingdi.h>
#include <winuser.h>

// Load's 0x428-byte scratch heap block: the just-read BITMAPINFOHEADER
// (0x28 bytes) immediately followed by the BMP's 256-entry RGBQUAD color table
// (0x400 bytes), read straight off the stream by BuildPaletteLUT.
struct PaletteScratchBuf {
    BITMAPINFOHEADER bih;
    RGBQUAD colors[256];
};
// DDSurfaceDescPadded0x7c now lives in LocoBitmap.h (shared with CreditsWnd.cpp,
// see that header's own comment for the full derivation).

extern "C" {  // TODO: idiom
    extern int g_nLiveLocoBitmaps;      // DAT_00485254 -- live-instance counter
    // Loads an image file via GDI LoadImageA, creates a matching DDraw surface
    // (IDirectDraw2::CreateSurface, vtbl+0x18), StretchBlt's the GDI bitmap onto it, then
    // deletes the GDI bitmap and returns the surface. `unused` (param_2) is read but never
    // used to build the DDSURFACEDESC; `flag` controls the fallback path when CreateSurface's
    // first attempt fails (retries in system memory and logs via Ddraw_HResultToString).
    // Written once by Ddraw::Ddraw_QuerySurfacePixelFormat (0x45b9c0) from the
    // primary surface's real DDPIXELFORMAT, read here to pack an 8bpp BMP
    // palette into the live 15/16bpp surface format. g_nRedShiftPos/
    // g_nGreenWidth are 10/5 for 555 (dwRBitMask==0x7c00) or 11/6 for 565.
    extern int g_nRedShiftPos;    // DAT_00485278
    extern int g_nGreenWidth;     // DAT_0048527c
    // Surface pixel-format tag (0x22b=555bpp, 0x235=565bpp), written by Ddraw_Init.
    extern int g_nSurfaceFormatTag; // DAT_00485274
    // Post->>1 channel-bleed guard mask for the fixed-shadow blend blitter family
    // (0x3def@555bpp, 0x7bef@565bpp), written by Ddraw_Init. TranslucentBlendBlit
    // computes its OWN dynamic version of this same mask instead of reading it.
    extern unsigned short g_wChannelBleedGuardMask; // DAT_00485280
    // ShadowBlit's own private scratch: g_wChannelBleedGuardMask<<1, recomputed fresh at the
    // top of every call and reloaded once per row inside the loop -- confirmed via xref sweep to
    // have exactly 2 references, both inside ShadowBlit itself (no other function reads or
    // writes it), so it's not shared cache like g_pSharedPalette -- just a real fixed-address
    // static rather than a stack local, per this toolchain's leaf-function register/stack
    // pressure habits.
    extern unsigned int g_nShadowMaskScratch; // DAT_00485248
    extern unsigned int g_nRBitMask; // DAT_00485288
    extern unsigned int g_nGBitMask; // DAT_0048528c
    extern unsigned int g_nBBitMask; // DAT_00485290
    // Process-wide shared (non-owned) 256-entry palette LUT cache -- used when
    // a caller passes flags!=0 and no owning LocoBitmap has claimed it yet
    // (bOwnsPalette=0 case below); ref-counted via g_nSharedPaletteRefCount.
    extern unsigned short *g_pSharedPalette; // DAT_0048524c
    extern int g_nSharedPaletteRefCount;     // DAT_00485250
    extern IDirectDraw2 *g_pDDraw2;          // DAT_00485440
    // Back/work DirectDraw surface (Ddraw_Init, 99 xrefs). RestoreOverlapBlt compares an incoming
    // target-surface pointer against this to decide whether to reuse WorldBoardMaybe's cached
    // lock state or do a local one-shot Lock/Unlock.
    // Sets the surface's DDCKEY_SRCBLT transparency colorkey (magenta 0x7c1f@555bpp /
    // 0xf81f@565bpp, picked from DAT_00485274's format tag) via IDirectDrawSurface::
    // SetColorKey. The 2nd param is caller-owned scratch the callee fully overwrites
    // (a DDCOLORKEY-sized local) -- every caller just passes the address of some
    // already-allocated nearby buffer (e.g. its own DDSURFACEDESC), content unused.
    extern void LocoBitmap_SetColorKey(void *pSurface, void *pScratch); // 0x45ba50
    // HRESULT-to-string lookup table (Ddraw namespace, plain __cdecl). Called
    // here with the fixed literal 1 (not the real HRESULT) purely for its
    // return value... which is then never used -- reproduced dead code, not a
    // bug in our transcription (confirmed via raw disasm: push 1; call; the
    // returned EAX is never read afterward, `xor al,al` always follows).
    // Centers `rect` within `outer`, preserving rect's own width/height (rewrites rect's
    // left/top/right/bottom in place). FUN_00425a50 -- a plain, non-LocoBitmap-cluster rect
    // utility (far outside this TU's 0x429a10-0x42c885 address range), so kept opaque/extern
    // like the DDraw_/Rf_ cross-TU helpers above rather than transcribed here.
}

// Reads the surface's real extent back out of its own GetSurfaceDesc. C++ linkage and
// `unsigned short *` outputs to match the canonical declaration in src/DDrawSurface.h (which
// src/Ddraw.cpp and src/WindowBase.cpp also spell this way) -- 0x4014e0 writes only 16 bits per
// output (`mov WORD PTR [eax],cx`), so an `unsigned int *` spelling both named a symbol nothing
// defines and overstated the write width.
void DDraw_QuerySurfaceDims(IDirectDrawSurface *pSurface, unsigned short *pOutWidth,
                            unsigned short *pOutHeight); // 0x4014e0, src/DDrawSurface.h
extern IDirectDrawSurface *g_pDDrawWorkSurface; // DAT_004fd3c4 -- C++ linkage (25 other TUs)
char *Ddraw_HResultToString(int hr); // 0x45bbc0, DEFINED in src/Ddraw.cpp
void CenterRectInRect(RECT *outer, RECT *rect); // FUN_00425a50, DEFINED in src/WindowBase.cpp

// FUNCTION: LOCO 0x42a110
LocoBitmap::LocoBitmap() {
    width = 0;
    height = 0;
    bOwnsPalette = 0;
    bUnk11 = 0;
    bConverted = 0;
    pPalette = 0;
    pPixels = 0;
    pSurface = 0;
    ++g_nLiveLocoBitmaps;
}

// Deep-copies src: fields, an owned-palette clone (memcpy) or a shared-palette
// pointer alias, an owned-pixel-buffer clone, and (if src has a live DDraw
// surface) a fresh system-memory offscreen-plain surface of the same size,
// Blt-filled from src's surface, with the transparency colorkey reapplied.
// FUNCTION: LOCO 0x42a1c0 (??0LocoBitmap@@QAE@PAU0@@Z)
LocoBitmap::LocoBitmap(LocoBitmap *src) {
    width = 0;
    height = 0;
    bOwnsPalette = 0;
    bUnk11 = 0;
    bConverted = 0;
    pPalette = 0;
    pPixels = 0;
    pSurface = 0;
    ++g_nLiveLocoBitmaps;

    bConverted = src->bConverted;
    width = src->width;
    height = src->height;
    bOwnsPalette = src->bOwnsPalette;
    bUnk11 = src->bUnk11;

    if (src->bOwnsPalette == 1 && src->pPalette != NULL) {
        pPalette = (unsigned short *)::operator new(256 * sizeof(unsigned short));
        memcpy(pPalette, src->pPalette, 256 * sizeof(unsigned short));
    } else {
        pPalette = src->pPalette;
    }

    if (src->pPixels != NULL) {
        pPixels = (unsigned char *)::operator new(width * height);
        memcpy(pPixels, src->pPixels, width * height);  // idiom-exempt: runtime length
    }

    if (src->pSurface != NULL) {
        // EFFECTIVE MATCH: a 0xc-byte stack-slot-offset gap between this struct
        // and `rect` vs the original persists regardless of declaration/init
        // order (tried u-then-rect, rect-then-u, and hoisting rect's field
        // inits before the CreateSurface calls -- all either identical or
        // worse); register-allocation/slot-assignment tie-break, not
        // source-steerable. See docs/PARKED.md.
        DDSurfaceDescPadded0x7c u;
        RECT rect;
        memset(&u, 0, sizeof(u));
        u.ddsd.dwWidth = width;
        u.ddsd.dwHeight = height;
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        u.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        u.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        HRESULT hr = g_pDDraw2->CreateSurface(&u.ddsd, &pSurface, NULL);
        if (hr != 0) {
            hr = g_pDDraw2->CreateSurface(&u.ddsd, &pSurface, NULL);
            if (hr != 0) {
                OutputDebugStringA("LOCOBITMAP COPY CONSTRUCTOR - failed to create surface");
                return;
            }
        }
        rect.left = 0;
        rect.top = 0;
        rect.right = width;
        rect.bottom = height;
        pSurface->Blt(&rect, src->pSurface, &rect, DDBLT_WAIT, NULL);
        LocoBitmap_SetColorKey(pSurface, &u.ddsd);
    }
}

// (Re)allocates this LocoBitmap's backing store at the given size: for the
// raw-8bpp case (bConverted==0), frees any existing pixel buffer and
// allocates a fresh width*height one; for the live-surface case
// (bConverted==1), releases any existing surface and creates a fresh
// system-memory offscreen-plain one of the new size, reapplying the
// transparency colorkey. Any other bConverted value is a silent no-op
// (returns success unchanged). Real return type is a byte bool, not the
// `void *` Ghidra originally inferred.
// ~LocoBitmap() itself is defined INLINE in LocoBitmap.h (see the header for why -- an
// out-of-line .cpp definition compiled to a real CALL from both the sites below instead of
// inlining, which doesn't match either original site).
// FUNCTION: LOCO 0x42a140 (??_GLocoBitmap scalar dtor)
// FUNCTION: LOCO 0x42a370 (??1LocoBitmap out-of-line COMDAT)
// 0x42a370 is the SAME dtor again, and it needed no new source: an inline-defined virtual dtor
// makes cl emit the body inlined at every use site AND one standalone out-of-line COMDAT, and
// the original has all three (0x42a140's inlined copy, Resize's, and this one). Our .obj has
// been emitting ??1LocoBitmap@@UAE@XZ all along -- it was simply unmarked, so nothing was
// comparing it. Worth remembering as a shape: when a class's dtor is defined in its header,
// grep the .obj's symbol table for ??1 before assuming the out-of-line address is unwritten.

// FUNCTION: LOCO 0x42a980 (?AllocSurface@LocoBitmap@@QAEEHH@Z)
unsigned char LocoBitmap::AllocSurface(int width, int height) {
    // EFFECTIVE MATCH: the original stores bSuccess=1 to its stack slot up
    // front, but its 3 early-exit sites disagree on whether they reload from
    // that slot or just return the register value directly (e.g. the
    // CreateSurface-failure site returns via a bare `xor al,al` with no
    // stack-slot touch at all, while the alloc-failure site stores AND
    // returns the register without reloading) -- tried a 3-independent-
    // `return` rewrite (matching AllocSurface's naive shape) and it
    // scored WORSE (63 vs 27 byte_diff) than this single-shared-variable
    // form, so kept this version. Residual is the differing per-site
    // reload-vs-direct-return tie-break, not source-steerable further
    // without a new idea. See docs/PARKED.md.
    unsigned char bSuccess = 1;
    int bC = bConverted;
    this->width = width;
    this->height = height;

    if (bC == 0) {
        if (pPixels != NULL) {
            ::operator delete(pPixels);
            pPixels = NULL;
        }
        pPixels = (unsigned char *)::operator new(height * width);
        if (pPixels == NULL) {
            bSuccess = 0;
        }
    } else if (bC == 1) {
        if (pSurface != NULL) {
            pSurface->Release();
            pSurface = NULL;
        }
        DDSurfaceDescPadded0x7c u;
        memset(&u, 0, sizeof(u));
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        u.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        u.ddsd.dwWidth = width;
        u.ddsd.dwHeight = height;
        u.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        HRESULT hr = g_pDDraw2->CreateSurface(&u.ddsd, &pSurface, NULL);
        if (hr != 0) {
            Ddraw_HResultToString(1); // sic: return value unused, see extern comment
            bSuccess = 0;
        } else {
            LocoBitmap_SetColorKey(pSurface, &u.ddsd);
        }
    }
    return bSuccess;
}

// If already converted (pSurface != NULL), a no-op. Otherwise creates a fresh
// system-memory offscreen-plain surface at the current width/height, reapplies
// the colorkey, Locks it, blits the raw 8bpp pPixels through the pPalette LUT
// via PixelCopyBlit, Unlocks, and on success frees the now-redundant
// palette/pixel buffers and flips bConverted to 1.
// FUNCTION: LOCO 0x42a3d0
void LocoBitmap::Convert() {
    if (pSurface != NULL) {
        return;
    }

    DDSurfaceDescPadded0x7c u;
    memset(&u, 0, sizeof(u));
    u.ddsd.dwWidth = width;
    u.ddsd.dwHeight = height;
    u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
    u.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    u.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    HRESULT hr = g_pDDraw2->CreateSurface(&u.ddsd, &pSurface, NULL);
    if (hr != 0) {
        OutputDebugStringA("LOCOBITMAP Convert - failed to create surface");
        return;
    }
    LocoBitmap_SetColorKey(pSurface, &u.ddsd);

    DDSurfaceDescPadded0x7c lockU;
    memset(&lockU, 0, sizeof(lockU));
    lockU.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
    HRESULT hrLock = pSurface->Lock(NULL, &lockU.ddsd, DDLOCK_WAIT, NULL);
    if (hrLock == 0) {
        RECT destRect;
        destRect.left = 0;
        destRect.top = 0;
        destRect.right = width;
        destRect.bottom = height;
        RECT srcRect;
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = width;
        srcRect.bottom = height;
        PixelCopyBlit(destRect, lockU.ddsd.lpSurface, lockU.ddsd.lPitch, srcRect);
        HRESULT hrUnlock = pSurface->Unlock(NULL);
        if (hrUnlock == 0) {
            bConverted = 1;
            if (bOwnsPalette == 1 && pPalette != NULL) {
                ::operator delete(pPalette);
                pPalette = NULL;
                bOwnsPalette = 0;
            }
            if (pPixels != NULL) {
                ::operator delete(pPixels);
                pPixels = NULL;
            }
        }
    }
}

// Blits this LocoBitmap's raw 8bpp pPixels (in srcRect) through pPalette's
// 16bpp LUT into a locked surface's pixel memory (pDestBase, at the given
// byte pitch destPitch), starting at destRect's origin (destRect.right/
// bottom are read by nothing below -- see the header declaration's note).
// PARKED (session-triaged 2026-07-21; re-triaged pt.3 same day): asmscore total 168920->165512
// (align 164->162, reg_pen 43->30) after hoisting the per-row pointer increments
// (`width - copyW` / `destStride - copyW`) into named `srcRowInc`/`destRowInc` locals computed
// ONCE before the loop, instead of recomputing them at every row -- same lever as
// PixelCopyColorKeyBlit's fix (1), confirmed to generalize to this sibling (unlike the
// `pPalette`-caching lever, which did NOT). Remaining residual (asmscore --len 208, total
// 165512, align=162 reg_pen=30 byte_diff=122, insns 73/79): the original reserves ONE local
// dword via `push ecx` at function entry (cheaper than `sub esp,4`; the same slot is discarded
// via a trailing `pop ecx` and never otherwise read) and defers pushing `edi` until after its
// first field load (`this` still lives in `eax` for that load; ours copies `this` into `edi`
// immediately and pushes all 4 callee-saved regs up front). Checked: the null-check's field
// READ ORDER already matches (pPixels at +0x18 tested before pPalette at +0x14, per the
// current `pPixels == NULL || pPalette == NULL` source order) -- the residual is NOT a
// field-order bug. Same push-timing/register-scheduling class as the sibling blits; no further
// source-level lever found this session within the usual triage budget. See docs/PARKED.md.
// ⚠ cc.sh's own raw byte-diff count for this function (using its OWN compiled length, not the
// true 208-byte Ghidra body span) went UP after this fix (188->194) even though the
// length-corrected asmscore total went DOWN -- exactly the "candidate's own compiled length
// silently truncates the comparison window" trap CLAUDE.md warns about; trust the
// --len-corrected asmscore dump over cc.sh's summary line for this function.
// FUNCTION: LOCO 0x42b9c0
unsigned char LocoBitmap::PixelCopyBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                              RECT srcRect) {
    if (pPixels == NULL || pPalette == NULL) {
        return 0;
    }
    int srcX = srcRect.left, srcY = srcRect.top, srcX2 = srcRect.right, srcY2 = srcRect.bottom;
    int destX = destRect.left, destY = destRect.top;
    unsigned char *pSrcRow = (unsigned char *)(srcY * width + srcX + (int)pPixels);
    unsigned int destStride = (destPitch >> 1) & 0xffff;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
    unsigned int copyW = (srcX2 - srcX) & 0xffff;
    unsigned char *pEnd = pSrcRow + copyW + (((srcY2 - srcY) & 0xffff) - 1) * width - 1;
    unsigned int srcRowInc = width - copyW;
    unsigned int destRowInc = destStride - copyW;
    for (; pSrcRow < pEnd; pSrcRow += srcRowInc) {
        unsigned char *pRowEnd = pSrcRow + copyW;
        for (; pSrcRow < pRowEnd; pSrcRow++) {
            *pDst = pPalette[*pSrcRow];
            pDst++;
        }
        pDst += destRowInc;
    }
    return 1;
}

// Same idiom as PixelCopyBlit, plus a branchless color-key trick: each pixel first
// self-stores the CURRENT dest pixel into pPalette[0] (clobbering the live palette entry --
// sic, reproduced not fixed), then writes dest = pPalette[srcIndex]. Net effect: a source index
// of 0 round-trips the dest pixel unchanged (transparent), any other index does a normal opaque
// color lookup -- without a per-pixel branch. Dispatcher default/fallback blit (RestoreOverlapBlt
// case 0 and its switch default).
// PARKED (v101; re-triaged 2026-07-21; root-caused further 2026-07-21 pt.2): the original uses a
// full EBP frame (`push ebp; mov ebp,esp; sub esp,0x10`) and spills nearly every local --
// including 4 dead incoming-PARAMETER stack slots reused as scratch (same class as
// Wav_ParseAndLoad's dead-slot reuse) -- where ours stays ESP-relative (FPO) throughout: MSVC
// picks the FPO/ESP-frame convention for our transcription, freeing ebp as a 7th GP register,
// while the original's higher simultaneous-live-value count (this session found and closed part
// of the gap -- see below) pushes IT into needing a real frame. TWO genuine, verified,
// score-improving source-level fixes landed this session (score 292001->235305, ~19%, insns
// 83->85 of a true 97): (1) hoist `srcRowInc`/`destRowInc` (= width-copyW / destStride-copyW) as
// named locals computed ONCE before the loop instead of recomputing the subtraction inline each
// row -- matches the original's actual one-time precompute + memory-operand `add` reload shape;
// reuse `copyH` directly as the row down-counter instead of a redundant `row` copy (matches the
// original's own read-modify-write-in-place counter). (2) cache `pPalette` in an explicit
// `unsigned short *pPal` local declared just before the loop -- the original loop-invariant-
// hoists `this->pPalette` into a register ONCE and keeps it resident all loop, but our compile
// (with a bare `pPalette` member access) reloaded `this->pPalette` from memory TWICE PER PIXEL;
// an explicit local forces the same one-time hoist. ⚠ Lever (2) does NOT generalize to this
// TU's other `pPalette[0]`-self-store siblings -- tried on MirrorColorKeyBlit (branch-based, no
// self-store: regressed 167291->168311), UpscaleBlit2x2ColorKey (multiply-indexed, no
// self-store: regressed 237048->269108), and ShadowBlit (SAME self-store shape as this
// function: still regressed 360200->370130) -- each reverted; the win here is specific to THIS
// function's exact register-pressure profile, not the self-store idiom in general. The
// remaining EBP-frame-vs-ESP-frame gap itself was NOT closed -- no source restructuring tried
// this session (more named locals, statement reordering to match the original's *pDst-before-
// *pSrc read order) flips the frame-pointer-omission choice; treat it as the true intrinsic
// residual (Yoda #29/#30 family, a whole-function codegen policy decision, not a per-statement
// one) and don't re-grind it without a genuinely new lever. Two duplicate `return 1;` epilogues
// ARE faithful here (confirmed via raw disasm: the "nothing to copy" and "loop completed" paths
// are two distinct physical epilogues at 0x42bb64/0x42bb6f, not merged) -- do not goto-merge them.
// FUNCTION: LOCO 0x42ba90
unsigned char LocoBitmap::PixelCopyColorKeyBlit(RECT destRect, void *pDestBase,
                                                       unsigned int destPitch, RECT srcRect) {
    if (pPixels == NULL || pPalette == NULL) {
        return 0;
    }
    int destX = destRect.left, destY = destRect.top;
    int srcX = srcRect.left, srcY = srcRect.top, srcX2 = srcRect.right, srcY2 = srcRect.bottom;
    unsigned int copyW = (srcX2 - srcX) & 0xffff;
    unsigned int destStride = (destPitch >> 1) & 0xffff;
    unsigned int copyH = (srcY2 - srcY) & 0xffff;
    if (copyW != 0 && copyH != 0) {
        unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
        unsigned char *pSrc = pPixels + srcY * width + srcX;
        unsigned short *pPal = pPalette;
        unsigned int srcRowInc = width - copyW;
        unsigned int destRowInc = destStride - copyW;
        do {
            unsigned int col = copyW;
            do {
                pPal[0] = *pDst;
                unsigned char index = *pSrc;
                pSrc++;
                *pDst = pPal[index];
                pDst++;
                col--;
            } while (col != 0);
            pSrc += srcRowInc;
            pDst += destRowInc;
            copyH--;
        } while (copyH != 0);
        return 1;
    }
    return 1;
}

// Same idiom as PixelCopyBlit, but scans the SOURCE row right-to-left while the
// DESTINATION is still written left-to-right (a horizontal-flip blit), and uses an explicit
// branch for color-key transparency (source index 0 = skip, dest pixel left untouched). Source
// is walked via a plain `int` offset re-added to pPixels[offset] each iteration (confirmed via
// raw disasm: `this->pPixels` is reloaded at every pixel, not cached as a pointer) -- same
// aliasing-driven idiom as CopyOverlapRaw's source side.
// PARKED (v101; re-triaged 2026-07-21): closed DIFF 169->144 bytes (score 197245->173294) by
// declaring copyW/copyH as plain `int` and deferring the `& 0xffff` truncation to point of use
// instead of masking at declaration -- the original computes/subtracts these BEFORE truncating
// (visible in the disasm as `sub;imul` ahead of any `and reg,0xffff`), so an early mask at
// declaration was real, source-steerable noise, not intrinsic. Residual DIFF 144/180 remains
// (insns 63/62, mostly matching `r`/`S` rows): the original loads all 4 incoming stack params up
// front in a fixed order unrelated to source declaration order, then computes destStride's
// shr/and LAST among those loads -- a load-scheduling choice that a 2nd probe (reordering
// copyH/srcOffset declarations) made WORSE (205067), reverted. Same register-pressure/
// TU-position class as PixelCopyBlit above; not spending further budget this session. See
// docs/PARKED.md.
// FUNCTION: LOCO 0x42c890
unsigned char LocoBitmap::MirrorColorKeyBlit(RECT destRect, void *pDestBase,
                                                    unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = (destPitch >> 1) & 0xffff;
    int copyW = srcRect.right - srcRect.left;
    int copyH = srcRect.bottom - srcRect.top;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
    int srcOffset = srcRect.top * width - 1 + srcRect.right;
    if (copyH != 0) {
        do {
            if (copyW != 0) {
                unsigned int col = copyW;
                do {
                    unsigned char index = pPixels[srcOffset];
                    if (index != 0) {
                        *pDst = pPalette[index];
                    }
                    pDst++;
                    srcOffset--;
                    col--;
                } while (col != 0);
            }
            pDst += (destStride - copyW);
            srcOffset += width + copyW;
            copyH--;
        } while (copyH != 0);
    }
    return 1;
}

// Nearest-neighbor 2x2 upscale blit, no color-key: each source pixel becomes a 2x2 block in the
// dest (dst[0]/dst[1] on the current dest row, dst[destStride]/dst[destStride+1] on the row
// below). Dispatcher case 5/0x85 (RestoreOverlapBlt). Two SIC quirks reproduced faithfully (confirmed
// via raw disasm, not decompiler noise): the source row stride used for indexing is copyW
// (srcRect.right-srcRect.left), not this->width -- only correct when the copied region spans a
// full pixel row; and srcRect.top is never read at all -- the scan always starts at row 0 of
// pPixels. Both differ from PixelCopyBlit's general (origin-aware, width-strided) addressing.
// PARKED (session-triaged 2026-07-21): DIFF 205/225 bytes, insns 68/73 -- a real missing-content
// gap (original has 5 more insns), likely the same EBP-frame/dead-slot-reuse structural class as
// PixelCopyColorKeyBlit above; not yet root-caused for this leaf specifically. See docs/PARKED.md.
// FUNCTION: LOCO 0x42bc80
unsigned char LocoBitmap::UpscaleBlit2x2(RECT destRect, void *pDestBase,
                                                unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = (destPitch >> 1) & 0xffff;
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
    unsigned int row = 0;
    if (srcRect.bottom != 0) {
        do {
            if (srcRect.right != 0) {
                unsigned int col = 0;
                unsigned short *pPixel = pDst;
                do {
                    unsigned short color = pPalette[pPixels[copyW * row + col]];
                    pPixel[0] = color;
                    pPixel[1] = color;
                    pDst = pPixel + 2;
                    pPixel[destStride] = pPixel[0];
                    col++;
                    pPixel[destStride + 1] = pPixel[0];
                    pPixel = pDst;
                } while (col < (unsigned int)srcRect.right);
            }
            row++;
            pDst += (destStride - copyW) * 2;
        } while (row < (unsigned int)srcRect.bottom);
    }
    return 1;
}

// Same 2x2 upscale idiom as UpscaleBlit2x2, but with an explicit color-key branch instead of
// a branchless trick: source index 0 skips the write entirely (dest pixels left untouched, not
// even the [destStride] row-below copy), any other index writes the 2x2 block. Dispatcher case
// 4/0x84 (RestoreOverlapBlt). Same two SIC quirks as the sibling above (source row stride is copyW,
// not this->width; srcRect.top is never read -- confirmed absent from the raw disasm).
// PARKED (session-triaged 2026-07-21): DIFF 158/230 bytes, insns 67/74 -- same missing-content
// shape as UpscaleBlit2x2's own sibling note above; not yet root-caused for this leaf. See
// docs/PARKED.md.
// FUNCTION: LOCO 0x42bb90
unsigned char LocoBitmap::UpscaleBlit2x2ColorKey(RECT destRect, void *pDestBase,
                                                         unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = (destPitch >> 1) & 0xffff;
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
    unsigned int row = 0;
    if (srcRect.bottom != 0) {
        do {
            if (srcRect.right != 0) {
                unsigned int col = 0;
                do {
                    unsigned char index = pPixels[copyW * row + col];
                    if (index != 0) {
                        unsigned short color = pPalette[index];
                        pDst[0] = color;
                        pDst[1] = color;
                        pDst[destStride] = pDst[0];
                        pDst[destStride + 1] = pDst[0];
                    }
                    pDst += 2;
                    col++;
                } while (col < (unsigned int)srcRect.right);
            }
            row++;
            pDst += (destStride - copyW) * 2;
        } while (row < (unsigned int)srcRect.bottom);
    }
    return 1;
}

// Nearest-neighbor 3x3 upscale blit with color-key: each source pixel becomes a 3x3 block in the
// dest (rows pDst[0..2], pDst[destStride..destStride+2], pDst[2*destStride..2*destStride+2]).
// Dispatcher dispatch 0x10/0x11 (RestoreOverlapBlt), the largest sibling. Source index 0 skips the
// write entirely (all 9 dest pixels left untouched), same explicit-branch idiom as
// UpscaleBlit2x2ColorKey. Same two SIC quirks as the 2x2 siblings (confirmed via raw disasm):
// source row stride is copyW, not this->width; srcRect.top is never read.
// PARKED (session-triaged 2026-07-21): DIFF 266/332 bytes, insns 77/103 -- the largest gap in
// the family (original has 26 more insns), same missing-content shape as the Upscale2x2
// siblings above; not yet root-caused. See docs/PARKED.md.
// FUNCTION: LOCO 0x42bd70
unsigned char LocoBitmap::UpscaleBlit3x3ColorKey(RECT destRect, void *pDestBase,
                                                         unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = (destPitch >> 1) & 0xffff;
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
    unsigned int row = 0;
    if (srcRect.bottom != 0) {
        do {
            if (srcRect.right != 0) {
                unsigned int col = 0;
                do {
                    unsigned char index = pPixels[copyW * row + col];
                    if (index != 0) {
                        unsigned short color = pPalette[index];
                        pDst[0] = color;
                        pDst[1] = color;
                        pDst[2] = pDst[0];
                        pDst[destStride] = pDst[0];
                        pDst[destStride + 1] = pDst[0];
                        pDst[destStride + 2] = pDst[0];
                        pDst[destStride * 2] = pDst[0];
                        pDst[destStride * 2 + 1] = pDst[0];
                        pDst[destStride * 2 + 2] = pDst[0];
                    }
                    pDst += 3;
                    col++;
                } while (col < (unsigned int)srcRect.right);
            }
            row++;
            pDst += (destStride - copyW) * 3;
        } while (row < (unsigned int)srcRect.bottom);
    }
    return 1;
}

// 1:1 translucent/shadow blend blitter with color-key (RestoreOverlapBlt dispatch 0x400/0x402): blends
// each opaque source pixel's palette color 50/50 with the CURRENT dest pixel via a dynamically
// computed channel-bleed guard mask (avoids carry bleeding between the 5/6-bit color channels
// during the >>1-and-add average). Source index 0 skips the blend entirely (dest untouched). Sic:
// unconditionally writes pPalette[0]/[1] as scratch every pixel (dest round-trip / its
// half-brightness copy), never read back -- dead writes reproduced faithfully, same trick as
// PixelCopyColorKeyBlit's pPalette[0] self-store. Addresses the source via this->width (not
// copyW), unlike the Upscale2x2/3x3 family.
// PARKED (session-triaged 2026-07-21): DIFF 275/395 bytes, insns 105/117 -- same missing-content
// shape as the rest of this family; not yet root-caused. See docs/PARKED.md.
// FUNCTION: LOCO 0x42bec0
unsigned char LocoBitmap::TranslucentBlendBlit(RECT destRect, void *pDestBase,
                                                       unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned int destStride = destPitch >> 1;
    int srcOffset = srcRect.top * width + srcRect.left;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destY * destStride + destX) * 2);
    unsigned short blendMask;
    if (g_nSurfaceFormatTag == 0x235) {
        blendMask = (unsigned short)(1 << (g_nGreenWidth - 1));
    } else {
        blendMask = (unsigned short)(1 << g_nGreenWidth);
    }
    blendMask = ~(blendMask | (unsigned short)(1 << g_nRedShiftPos) | 1);
    unsigned short *pRowEnd = pDst + (((srcRect.bottom - srcRect.top) & 0xffff) - 1) * destStride + copyW;
    short srcStride = (short)width;
    for (; pDst < pRowEnd; pDst += (destStride - copyW) & 0xffff) {
        unsigned short *pColEnd = pDst + copyW;
        for (; pDst < pColEnd; pDst++) {
            pPalette[0] = *pDst;
            unsigned char index = pPixels[srcOffset];
            if (index != 0) {
                pPalette[1] = (*pDst >> 1) & g_wChannelBleedGuardMask;
                *pDst = ((pPalette[index] & blendMask) >> 1) + ((*pDst & blendMask) >> 1);
            }
            srcOffset++;
        }
        srcOffset += (unsigned short)(srcStride - (short)copyW);
    }
    return 1;
}

// Fixed-mask shadow/translucency blit with color-key (RestoreOverlapBlt dispatch case 2), first of the
// 4 remaining "shadow" siblings. Branchless like PixelCopyColorKeyBlit: each pixel first
// round-trips the CURRENT dest pixel through pPalette[0]/[1] scratch (pPalette[0]=destPixel,
// pPalette[1]=(destPixel & mask)>>1, a fixed 50%-darkened copy of the dest pixel ITSELF, not the
// source), then writes dest=pPalette[srcIndex] -- source index 0 = transparent (round-trips
// pPalette[0], the unchanged dest pixel), index 1 = the half-darkened shadow of whatever was
// already there, 2-255 = a normal opaque palette lookup. Uses the FIXED
// g_wChannelBleedGuardMask (not TranslucentBlendBlit's dynamically recomputed one) --
// recomputed into g_nShadowMaskScratch once per call and reloaded once per row (confirmed via
// raw disasm: the global is re-read from memory at the top of every outer/row iteration, not kept
// resident in a register across the whole function -- Yoda lesson #19, a real global reload
// pattern, not a cached local). Addresses the source via this->width (not copyW), like
// TranslucentBlendBlit.
// PARKED (session-triaged 2026-07-21): DIFF 213/216 bytes, insns 72/85 -- same missing-content
// shape as the rest of this family; not yet root-caused. See docs/PARKED.md.
// FUNCTION: LOCO 0x42c050
unsigned char LocoBitmap::ShadowBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                            RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = destPitch >> 1;
    int copyW = srcRect.right - srcRect.left;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destStride * destY + destX) * 2);
    int copyH = srcRect.bottom - srcRect.top;
    unsigned char *pSrc = pPixels + srcRect.top * width + srcRect.left;
    g_nShadowMaskScratch = (unsigned int)g_wChannelBleedGuardMask << 1;
    if (copyH != 0 && copyW != 0) {
        do {
            int col = copyW;
            do {
                unsigned short color = *pDst;
                unsigned char index = *pSrc;
                pPalette[0] = color;
                pPalette[1] = (unsigned short)((color & g_nShadowMaskScratch) >> 1);
                pSrc++;
                col--;
                *pDst = pPalette[index];
                pDst++;
            } while (col != 0);
            pDst += destStride - copyW;
            pSrc += width - copyW;
            copyH--;
        } while (copyH != 0);
    }
    return 1;
}

// Mirrored/reversed-scan sibling of ShadowBlit (RestoreOverlapBlt dispatch case 0x22): same
// slot-0/1 branchless shadow trick (pPalette[0]=destPixel, pPalette[1]=(destPixel>>1) &
// g_wChannelBleedGuardMask, then dest=pPalette[srcIndex]), but the source is scanned
// right-to-left like MirrorColorKeyBlit, and shares ITS per-row control-flow shape (a
// nested "if (copyW != 0) { do {...} while(...); }" retested every row) rather than
// ShadowBlit's single combined guard -- confirmed via raw disasm, the row loop's back-edge
// target retests copyW!=0 each iteration, unlike ShadowBlit where copyW is only checked
// once before the outer loop. Sic: the source origin is srcRect.right, NOT srcRect.right - 1
// like MirrorColorKeyBlit -- confirmed via raw disasm, no "-1" instruction anywhere in the
// pointer setup (unlike MirrorColorKeyBlit's explicit "+ -1 + param_9") -- so it reads one
// column past the intended rightmost column on the first iteration of every row, reproduced
// faithfully rather than fixed. g_nShadowMaskScratch is still computed/stored here (matching
// the rest of the "shadow" family's shared preamble) but never read back in this function's own
// loop -- a dead write, same class as PixelCopyColorKeyBlit's pPalette[0] self-store.
// PARKED (session-triaged 2026-07-21): DIFF 189/226 bytes, insns 72/79 -- same missing-content
// shape as the rest of this family; not yet root-caused. See docs/PARKED.md.
// FUNCTION: LOCO 0x42c130
unsigned char LocoBitmap::MirrorShadowBlit(RECT destRect, void *pDestBase,
                                                  unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = destPitch >> 1;
    int copyW = srcRect.right - srcRect.left;
    int copyH = srcRect.bottom - srcRect.top;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destStride * destY + destX) * 2);
    unsigned char *pSrc = pPixels + srcRect.top * width + srcRect.right;
    g_nShadowMaskScratch = (unsigned int)g_wChannelBleedGuardMask << 1;
    if (copyH != 0 && copyW != 0) {
        do {
            if (copyW != 0) {
                int col = copyW;
                do {
                    unsigned short color = *pDst;
                    unsigned char index = *pSrc;
                    pPalette[0] = color;
                    pPalette[1] = (unsigned short)((color >> 1) & g_wChannelBleedGuardMask);
                    pSrc--;
                    col--;
                    *pDst = pPalette[index];
                    pDst++;
                } while (col != 0);
            }
            pDst += destStride - copyW;
            pSrc += width + copyW;
            copyH--;
        } while (copyH != 0);
    }
    return 1;
}

// Interlaced sibling of ShadowBlit (RestoreOverlapBlt dispatch 0x102): same slot-0/1 branchless
// dest-shadow trick (pPalette[0]=destPixel, pPalette[1]=(destPixel>>1) & g_wChannelBleedGuardMask,
// then dest=pPalette[srcIndex]), scanned forward like ShadowBlit, but touches only every
// OTHER dest/source row: pDst/pSrc both advance by 2*destStride/2*width per outer iteration
// instead of 1. A row-parity preamble ensures the interlaced scan always starts on an even source
// row: if srcRect.top is odd, both srcRect.top and destRect.top are advanced by one first
// (confirmed via raw disasm as a signed "% 2" idiom -- the cdq/xor/sub/and/xor/sub instruction
// sequence is MSVC's standard signed-modulo-by-2 expansion, not a plain "& 1" bit test). Reads
// g_wChannelBleedGuardMask directly per pixel (no g_nShadowMaskScratch global write),
// matching MirrorShadowBlit's own mask-read shape rather than ShadowBlit's.
// PARKED (session-triaged 2026-07-21): DIFF 152/271 bytes, insns 91/88 -- the OPPOSITE shape
// from the rest of this family: ours has 3 more insns than the original, not fewer (an extra-
// content residual, not a missing-content one); not yet root-caused. See docs/PARKED.md.
// FUNCTION: LOCO 0x42c220
unsigned char LocoBitmap::InterlacedShadowBlit(RECT destRect, void *pDestBase,
                                                      unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = destPitch >> 1;
    if (srcRect.top % 2 != 0) {
        srcRect.top++;
        destY++;
    }
    unsigned int copyH = (srcRect.bottom - srcRect.top) & 0xffff;
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destStride * destY + destX) * 2);
    unsigned char *pSrc = pPixels + srcRect.top * width + srcRect.left;
    for (unsigned short row = 0; row < copyH; row += 2) {
        if (copyW != 0) {
            for (unsigned short col = 0; col < copyW; col++) {
                unsigned short color = *pDst;
                unsigned char index = *pSrc;
                pPalette[0] = color;
                pPalette[1] = (unsigned short)((color >> 1) & g_wChannelBleedGuardMask);
                pSrc++;
                *pDst = pPalette[index];
                pDst++;
            }
        }
        pDst += destStride * 2 - copyW;
        pSrc += width * 2 - copyW;
    }
    return 1;
}

// Checkerboard-dithered sibling of ShadowBlit (RestoreOverlapBlt dispatch 0x202): same slot-0/1
// branchless shadow trick (pPalette[0]=destPixel, pPalette[1]=(destPixel>>1) &
// g_wChannelBleedGuardMask, then dest=pPalette[srcIndex]), but a running on/off toggle skips every
// other pixel in raster-scan order. The toggle is seeded from srcRect.top's parity (same signed
// "% 2" idiom as InterlacedShadowBlit) but is NEVER reset at row boundaries -- it just keeps
// alternating across the whole scan, including through the end-of-row stride skip -- so whether a
// given row starts "on" or "off" depends on whether copyW is even (same phase every row, vertical
// stripes) or odd (phase flips each row, a true checkerboard). Confirmed via raw disasm: the
// toggle (a bool materialized via setne/sete) lives entirely outside both loop bodies' index/row
// counters, updated unconditionally every pixel regardless of whether the "on" branch fired.
// PARKED (session-triaged 2026-07-21): DIFF 159/272 bytes, insns 95/89 -- same extra-content
// shape as InterlacedShadowBlit above (ours has 6 more insns, not fewer); not yet root-caused.
// See docs/PARKED.md.
// FUNCTION: LOCO 0x42c470
unsigned char LocoBitmap::CheckerboardShadowBlit(RECT destRect, void *pDestBase,
                                                        unsigned int destPitch, RECT srcRect) {
    int destX = destRect.left, destY = destRect.top;
    unsigned int destStride = destPitch >> 1;
    bool bOn = (srcRect.top % 2) != 0;
    unsigned int copyH = (srcRect.bottom - srcRect.top) & 0xffff;
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned short *pDst = (unsigned short *)((int)pDestBase + (destStride * destY + destX) * 2);
    unsigned char *pSrc = pPixels + srcRect.top * width + srcRect.left;
    for (unsigned short row = 0; row < copyH; row++) {
        if (copyW != 0) {
            for (unsigned short col = 0; col < copyW; col++) {
                if (bOn) {
                    unsigned char index = *pSrc;
                    pPalette[0] = *pDst;
                    pPalette[1] = (unsigned short)((*pDst >> 1) & g_wChannelBleedGuardMask);
                    *pDst = pPalette[index];
                }
                bOn = !bOn;
                pDst++;
                pSrc++;
            }
        }
        pDst += destStride - copyW;
        pSrc += width - copyW;
    }
    return 1;
}

// Loads a WAV -- no, a BMP resource by path, preferring the RF archive
// (stripping the fixed install-path prefix) and falling back to a loose
// file (same pattern as Wav_ParseAndLoad). Parses BITMAPFILEHEADER +
// BITMAPINFOHEADER; for the common 8bpp/native-size case, builds the 15/16
// bpp palette LUT via BuildPaletteLUT and reads pixel rows bottom-up
// (BMP's native storage order) with per-row 4-byte-alignment padding
// skipped via seekg. For 24/32bpp or an explicit requested size, falls back
// to a DirectDraw-native load (DDraw_CreateSurfaceFromFile/DDraw_QuerySurfaceDims
// -- cross-TU, opaque, not yet transcribed).
// FUNCTION: LOCO 0x42ab10
unsigned char LocoBitmap::Load(const char *path, unsigned int flags, int width, int height) {
    PaletteScratchBuf *pPaletteScratch = NULL;
    istrstream *pRfStream = NULL;
    ifstream fileStream;
    istream *pStream = NULL;
    void *pRfBuf = NULL;
    unsigned char bSuccess = 1;
    // Declared here (not at point of use in the DDraw branch below) -- this is
    // what closes the -0xc8/-0xc0 stack-offset gap that ran through the whole
    // function; a declaration down in the DDraw branch pushes every later
    // local's frame offset off by 8 bytes vs the original. See CLAUDE.md pickup.
    unsigned int outWidth, outHeight;

    if (pPixels != NULL) {
        ::operator delete(pPixels);
        pPixels = NULL;
    }

    unsigned char bHaveRfIndex = (g_RFIndex.pFile != NULL);
    if (bHaveRfIndex) {
        int nRfSize;
        pRfBuf = g_RFIndex.LoadResource((const unsigned char *)(path + strlen(g_pInstallPathPrefix)), &nRfSize);
        if (pRfBuf != NULL) {
            pRfStream = new istrstream((char *)pRfBuf, nRfSize);
            if (pRfStream != NULL) {
                pStream = pRfStream;
            }
        }
    }
    if (pStream == NULL) {
        fileStream.open(path, ios::nocreate | ios::binary);
        if (fileStream.fd() != -1) { // EFFECTIVE: filebuf::fd()'s own ternary
            // ((x_fd==-1)?EOF:x_fd, FSTREAM.H:85) doesn't get algebraically
            // folded with this outer "!= -1" the way the original's compile
            // did -- 3-instruction residual (jne/or/cmp) at every call site,
            // same root cause as Wav.cpp's still-unresolved fd() residual.
            // See docs/PARKED.md.
            pStream = &fileStream;
        }
        if (pStream == NULL) {
            goto cleanup;
        }
    }

    try {
        BITMAPFILEHEADER bfh;
        unsigned int nRead;
        pStream->read((char *)&bfh, sizeof(bfh));
        nRead = pStream->gcount();
        if (nRead != sizeof(bfh)) {
            throw 2;
        }
        if (bfh.bfType != 0x4d42) { // "BM"
            throw 3;
        }

        BITMAPINFOHEADER bih;
        pStream->read((char *)&bih, sizeof(bih));
        nRead = pStream->gcount();
        if (nRead != sizeof(bih)) {
            throw 4;
        }

        if (bih.biBitCount == 8 && bConverted != 1 && width == 0 && height == 0) {
            bConverted = 0;
            pPaletteScratch = (PaletteScratchBuf *)::operator new(sizeof(PaletteScratchBuf));
            if (pPaletteScratch == NULL) {
                throw 5;
            }
            memset(pPaletteScratch, 0, sizeof(PaletteScratchBuf));
            memcpy(&pPaletteScratch->bih, &bih, sizeof(bih));

            if (!BuildPaletteLUT(pStream, bih, pPaletteScratch, flags)) {
                throw 6;
            }

            this->width = bih.biWidth;
            this->height = bih.biHeight;
            unsigned int nPixelBytes = bih.biWidth * bih.biHeight;
            pPixels = (unsigned char *)::operator new(nPixelBytes);
            if (pPixels == NULL) {
                throw 7;
            }
            memset(pPixels, 0, nPixelBytes);  // idiom-exempt: runtime length

            unsigned int rowStride = this->width;
            if ((rowStride & 3) != 0) {
                rowStride = (rowStride - (rowStride & 3)) + 4;
            }
            unsigned char *pRow = pPixels + (this->height - 1) * this->width;
            pStream->seekg(bfh.bfOffBits, ios::beg);
            for (unsigned int y = 0; y < this->height; y++) {
                pStream->read((char *)pRow, this->width);
                pRow -= this->width;
                int nPad = rowStride - this->width;
                if (nPad != 0) {
                    pStream->seekg(nPad, ios::cur);
                }
            }
        } else {
            if (fileStream.fd() != -1) { // EFFECTIVE, see the open()-site comment above
                fileStream.close();
            }
            void *pSurf = DDraw_CreateSurfaceFromFile(path, 0x10, width, height, 0);
            pSurface = (IDirectDrawSurface *)pSurf;
            bConverted = 1;
            DDraw_QuerySurfaceDims(pSurface, (unsigned short *)&outWidth,
                                   (unsigned short *)&outHeight);
            this->width = outWidth & 0xffff;
            this->height = outHeight & 0xffff;
        }
    }
    catch (int) {
        if (pPixels != NULL) {
            ::operator delete(pPixels);
            pPixels = NULL;
        }
        bSuccess = 0;
    }

cleanup:
    if (pRfStream != NULL) {
        delete pRfStream;
    }
    if (fileStream.fd() != -1) { // EFFECTIVE, see the open()-site comment above
        fileStream.close();
    }
    if (pRfBuf != NULL) {
        _free(pRfBuf);
    }
    if (pPaletteScratch != NULL) {
        ::operator delete(pPaletteScratch);
    }
    return bSuccess;
}

// Builds this LocoBitmap's 256-entry 15/16bpp palette LUT from the just-parsed
// BITMAPINFOHEADER + the RGBQUAD color table already sitting in pPaletteScratch
// (copied there by Load: bih at +0x0, room for the color table at +0x28).
// Reads the remaining 0x400 bytes (256 * sizeof(RGBQUAD)) off the stream, then
// packs each entry's 8-bit B/G/R bytes into the live surface's real bit layout
// (g_nRBitMask/etc, set once by Ddraw_QuerySurfacePixelFormat). Shares a
// process-wide palette cache (g_pSharedPalette) unless the caller passes
// flags!=0 while that cache is still free, in which case this instance gets
// its own dedicated (non-shared) palette buffer instead.
// FUNCTION: LOCO 0x42af30
unsigned char LocoBitmap::BuildPaletteLUT(istream *pStream, BITMAPINFOHEADER bih, void *pPaletteScratch, unsigned int flags) {
    PaletteScratchBuf *pScratch = (PaletteScratchBuf *)pPaletteScratch;
    if (bih.biBitCount != 8) {
        goto fail;
    }
    pStream->read((char *)pScratch->colors, 0x400);
    if (pStream->gcount() != 0x400) {
        goto fail;
    }

    {
    int nGreenShift = g_nGreenWidth - 3;
    int nRedShift = g_nRedShiftPos - 3;
    unsigned short *pPalette;

    if (flags != 0 && g_nSharedPaletteRefCount == 0) {
        bOwnsPalette = 0;
        pPalette = (unsigned short *)::operator new(0x200);
        g_pSharedPalette = pPalette;
        if (pPalette == NULL) {
            goto fail;
        }
        this->pPalette = pPalette;
        g_nSharedPaletteRefCount++;
    } else {
        bOwnsPalette = 1;
        pPalette = (unsigned short *)::operator new(0x200);
        this->pPalette = pPalette;
        if (pPalette == NULL) {
            ::operator delete(g_pSharedPalette);
            g_pSharedPalette = NULL;
            return 0;
        }
    }

    RGBQUAD *pEntry = pScratch->colors;
    int i = 0x100;
    do {
        unsigned int color = ((unsigned int)pEntry->rgbGreen << nGreenShift & g_nGBitMask)
                            | ((unsigned int)pEntry->rgbRed << nRedShift & g_nRBitMask)
                            | ((unsigned int)(pEntry->rgbBlue >> 3) & g_nBBitMask);
        *pPalette = (unsigned short)color;
        pEntry++;
        pPalette++;
        i--;
    } while (i != 0);
    return 1;
    }
fail:
    return 0;
}

// Tears down the process-wide shared 16bpp palette LUT cache: frees the buffer
// and nulls the global. Note it does NOT touch g_nSharedPaletteRefCount -- an
// unconditional free, i.e. a shutdown/reset path rather than a refcounted
// release (the refcounted release lives inline in the dtor family).
// FUNCTION: LOCO 0x42a5f0
void LocoBitmap_FreeSharedPalette() {
    if (g_pSharedPalette != NULL) {
        ::operator delete(g_pSharedPalette);
        g_pSharedPalette = NULL;
    }
}

// Pixel-perfect collision test between this bitmap's raw 8bpp pixels (over r1)
// and another bitmap's (over r2's origin): walks the r1-sized region in both
// buffers in lockstep and returns 1 the moment a pixel is set (bitwise-AND
// non-zero) in BOTH. Only valid for two unconverted (raw-8bpp) bitmaps; returns
// 0 if either is already converted to a live surface, or other is NULL. Only
// r1's extent (right/bottom) is read -- r2 contributes just its origin.
// EFFECTIVE MATCH: DIFF 125/177. Structure is byte-for-byte identical in
// operations, control flow, and memory operands -- the entire residual is one
// symmetric register-allocation tie-break (the `other` pointer lands in edx
// here vs edi in the original) cascading into edx/ecx renames throughout, plus
// the resulting one-instruction schedule shift of the this->pPixels add. No
// missing/extra operations. The function has no calls, so caller- vs
// callee-saved gives no source lever to force `other` into edi -- the intrinsic
// register-swap class (Yoda #29/#30). See docs/PARKED.md.
// FUNCTION: LOCO 0x42a540
unsigned char LocoBitmap::TestPixelCollision(RECT r1, LocoBitmap *other, RECT r2) {
    if (other == NULL) {
        return 0;
    }
    if (bConverted != 0) {
        return 0;
    }
    if (other->bConverted != 0) {
        return 0;
    }

    int regionH = r1.bottom - r1.top;
    unsigned char *pThis = pPixels + r1.top * width + r1.left;
    int regionW = r1.right - r1.left;
    unsigned char *pOther = other->pPixels + r2.top * other->width + r2.left;

    for (int row = 0; row < regionH; row++) {
        for (int col = 0; col < regionW; col++) {
            if (*pThis & *pOther) {
                return 1;
            }
            pThis++;
            pOther++;
        }
        pThis += width - regionW;
        pOther += other->width - regionW;
    }
    return 0;
}

// 0x42c330 (LocoBitmap::CopyRectRawColorKey, the color-keyed twin of the function below) is
// transcribed and compiles EXACT, but is deliberately NOT landed here: it would need a new
// declaration in LocoBitmap.h, and that header's declaration count is a live codegen dial whose
// N=3 point costs -1531 B across three other TUs against this one's +421 B of gain. The body and
// the full measurement live in docs/PARKED.md's v524 section; see LocoBitmap.h's own note beside
// CopyOverlapRaw for the landing condition.

// Raw-8bpp counterpart to PixelCopyBlit: blits a rect of `this` object's OWN raw pixels
// (the pre-resize snapshot in Resize's usage) into a caller-supplied destination buffer at
// a different stride/origin. destRect.right/bottom are dead (only the origin is used), same
// convention as PixelCopyBlit. The source (this->pPixels) is read via a plain int OFFSET
// re-added to the member each iteration, NOT a cached local pointer -- matches Yoda lesson #19
// (per-statement member reloads = no caching local in the source); the dest pointer (a plain
// PARAMETER, no aliasing concern) IS cached as a real pointer, incremented in place.
// EXACT since v524. Parked for many sessions at DIFF 18/148 with an IDENTICAL instruction count
// (54/54) -- the sole residual was two instructions (a srcRect operand load and the outer-counter
// zero-init) landing one slot later in ours. The fix was one line: declare copyW BEFORE copyH.
// VC5 schedules two adjacent, independent, same-shape local initializers in the REVERSE of source
// order here, so the source order that reproduces the original's emission is the opposite of the
// order the original's own instructions appear in (it computes copyH first). See docs/CODEGEN.md.
// FUNCTION: LOCO 0x42c3d0
void LocoBitmap::CopyOverlapRaw(RECT destRect, unsigned char *pDestPixels, int destWidth, RECT srcRect) {
    unsigned int copyW = (srcRect.right - srcRect.left) & 0xffff;
    unsigned int copyH = (srcRect.bottom - srcRect.top) & 0xffff;
    unsigned char *pDst = (unsigned char *)(destWidth * destRect.top + (int)pDestPixels + destRect.left);
    int srcOffset = srcRect.top * width + srcRect.left;
    for (unsigned short wRow = 0; wRow < copyH; wRow++) {
        for (unsigned short wCol = 0; wCol < copyW; wCol++) {
            *pDst++ = pPixels[srcOffset++];
        }
        pDst += destWidth - copyW;
        srcOffset += width - copyW;
    }
}

// Resizes to (width,height) via AllocSurface, fills the fresh buffer with placeholder content
// (raw-8bpp: an 0x8 noise stamp; live-surface: a magenta-ish 0xa0c0d1 DDBLT_COLORFILL), then
// restores whatever content overlaps the new viewport-clamped bounds from a copy-ctor'd
// snapshot of the pre-resize state (temp) -- raw path via CopyOverlapRaw, converted path
// via RestoreOverlapBlt (NOT YET TRANSCRIBED, see CLAUDE.md pickup). Named 2026-07-13; the
// constant-0x8 fill suggests a placeholder/static-noise render path when no real image is
// loaded, not confirmed against an asset. `temp` is a genuine RAII stack local (copy-ctor'd
// from *this) -- its automatic teardown at scope exit reproduces the original's inlined
// ~LocoBitmap() body exactly (see LocoBitmap.h; had to move the dtor's definition INLINE
// there, an out-of-line .cpp definition compiled to a real CALL here instead of inlining).
// EFFECTIVE MATCH: DIFF 32/564 bytes, insn count identical (175/175) -- the sole residual is
// the DDBLTFX field-init pair (fx.dwSize/fx.dwFillColor) landing after the Blt vtable-pointer
// load instead of before; tried reversing the two field-init lines and caching pSurface in an
// explicit local first (matching the decompile's separate `piVar1` shape) -- neither changed
// or improved it. Pure instruction-scheduling tie-break. See docs/PARKED.md.
// FUNCTION: LOCO 0x42a610
void LocoBitmap::Resize(int width, int height) {
    LocoBitmap temp(this);
    AllocSurface(width, height);

    if (bConverted == 0 && pPixels != NULL) {
        memset(pPixels, 8, this->height * this->width);  // idiom-exempt: runtime length
    } else {
        IDirectDrawSurface *pSurf = pSurface;
        if (pSurf != NULL) {
            DDBLTFX fx;
            fx.dwSize = sizeof(DDBLTFX);
            fx.dwFillColor = 0xa0c0d1;
            pSurf->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);
        }
    }

    RECT tempFullRect;
    SetRect(&tempFullRect, 0, 0, temp.width, temp.height);
    RECT overlapRect;
    IntersectRect(&overlapRect, &g_worldBoard.rcViewport, &tempFullRect);
    CenterRectInRect(&tempFullRect, &overlapRect);
    RECT destRect = overlapRect;
    CenterRectInRect(&g_worldBoard.rcViewport, &destRect);

    if (bConverted == 0) {
        temp.CopyOverlapRaw(destRect, this->pPixels, this->width, overlapRect);
    } else {
        temp.RestoreOverlapBlt(destRect, pSurface, overlapRect, 1);
    }
}

// Converted-surface resize/redraw dispatcher, called by Resize with `this` = the pre-resize
// snapshot (temp) and pTargetSurface = the POST-resize surface being restored into. Two
// structurally different halves gated on THIS object's OWN bConverted:
//  - bConverted==1: a straight this->pSurface -> pTargetSurface DirectDraw Blt, with the shared
//    back-buffer's cached lock state (WorldBoardMaybe's tail fields) unlocked before (if a
//    snapshot taken at function entry AND the live flag both say it's locked) and unconditionally
//    re-locked after (DDraw disallows Blt-ing while the target surface is locked).
//  - bConverted==0: Locks pTargetSurface (reusing the cached lock state if pTargetSurface IS the
//    shared back-buffer, else a local one-shot Lock/Unlock) and dispatches through `flags` to one
//    of the 11 raw-pixel blit siblings above, writing into the locked surface memory; Unlocks
//    afterward (shared-surface case: only if it wasn't already guard-locked at entry).
// flags bit 0x40 (never set at the one known call site, Resize's mode=1) first runs a
// dest/src-rect clamp against this bitmap's bounds (ClampBlitRects/0042c700, NOT YET TRANSCRIBED)
// before falling through to the rest unchanged.
// FUNCTION: LOCO 0x42b050
unsigned char LocoBitmap::RestoreOverlapBlt(RECT destRect, IDirectDrawSurface *pTargetSurface,
                                         RECT srcRect, unsigned int flags) {
    if ((flags & 0xfffffffb) != 0 && (flags & 0xffffffef) != 0 &&
        srcRect.right - srcRect.left != destRect.right - destRect.left &&
        srcRect.bottom - srcRect.top != destRect.bottom - destRect.top) {
        flags |= 0x80;
    }

    if ((flags & 0x40) != 0) {
        if (bConverted == 1) {
            ClampBlitRects(&destRect, pTargetSurface, &srcRect, flags);
        } else if (bConverted == 0) {
            ClampBlitRectsRaw(&destRect, pTargetSurface, &srcRect, flags);
        }
        flags &= 0xffffffbf;
    }

    unsigned char bWasLockGuarded = g_worldBoard.bSurfaceLockGuard;

    if (bConverted != 0) {
        if (bConverted != 1) {
            return 0;
        }

        if (bWasLockGuarded != 0 && g_worldBoard.bSurfaceLockGuard != 0 &&
            g_pDDrawWorkSurface->Unlock(NULL) == 0) {
            g_worldBoard.bSurfaceLockGuard = 0;
        }

        RECT rectSrc = srcRect;
        RECT rectDest;
        unsigned int blitFlags;
        unsigned char bResult;
        unsigned char bReLock;
        if (flags == 0) {
            rectDest = destRect;
            blitFlags = 0x1008000; // DDBLT_WAIT | DDBLT_KEYSRC
            bResult = pTargetSurface->Blt(&rectDest, pSurface, &rectSrc, blitFlags, NULL) == 0;
            bReLock = (unsigned char)(blitFlags >> 24);
        } else if (flags == 1) {
            rectDest = destRect;
            blitFlags = 0x1000000; // DDBLT_WAIT
            bResult = pTargetSurface->Blt(&rectDest, pSurface, &rectSrc, blitFlags, NULL) == 0;
            bReLock = (unsigned char)(blitFlags >> 24);
        } else if (flags == 0x80) {
            rectDest = destRect;
            bResult = pTargetSurface->Blt(&rectDest, pSurface, &rectSrc, 0x1000000, NULL) == 0;
            bReLock = 1;
        } else {
            rectDest = destRect;
            blitFlags = (flags & 1) == 0 ? 0x1008000 : 0x1000000;
            bResult = pTargetSurface->Blt(&rectDest, pSurface, &rectSrc, blitFlags, NULL) == 0;
            bReLock = (unsigned char)(blitFlags >> 24);
        }

        if (bReLock != 0 && g_worldBoard.bSurfaceLockGuard == 0) {
            memset(g_worldBoard.aSurfaceDescScratch, 0, sizeof(g_worldBoard.aSurfaceDescScratch));
            g_worldBoard.aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
            if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)g_worldBoard.aSurfaceDescScratch,
                                                0, NULL) == 0) {
                g_worldBoard.bSurfaceLockGuard = 1;
            }
        }
        return bResult;
    }

    unsigned int destPitch;
    void *pDestBase;
    if (pTargetSurface == g_pDDrawWorkSurface) {
        if (g_worldBoard.bSurfaceLockGuard == 0) {
            memset(g_worldBoard.aSurfaceDescScratch, 0, sizeof(g_worldBoard.aSurfaceDescScratch));
            g_worldBoard.aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
            if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)g_worldBoard.aSurfaceDescScratch,
                                                0, NULL) != 0) {
                return 0;
            }
            g_worldBoard.bSurfaceLockGuard = 1;
        }
        destPitch = g_worldBoard.aSurfaceDescScratch[4];
        pDestBase = (void *)g_worldBoard.aSurfaceDescScratch[9];
    } else {
        DDSurfaceDescPadded0x7c u;
        memset(&u, 0, sizeof(u));
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        if (pTargetSurface->Lock(NULL, &u.ddsd, DDLOCK_WAIT, NULL) != 0) {
            return 0;
        }
        destPitch = u.ddsd.lPitch;
        pDestBase = u.ddsd.lpSurface;
    }

    unsigned char bResult;
    switch (flags) {
    case 0:
        bResult = PixelCopyColorKeyBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 1:
    case 3:
        bResult = PixelCopyBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 2:
        bResult = ShadowBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 4:
    case 0x84:
        bResult = UpscaleBlit2x2ColorKey(destRect, pDestBase, destPitch, srcRect);
        break;
    case 5:
    case 0x85:
        bResult = UpscaleBlit2x2(destRect, pDestBase, destPitch, srcRect);
        break;
    case 0x10:
    case 0x11:
        bResult = UpscaleBlit3x3ColorKey(destRect, pDestBase, destPitch, srcRect);
        break;
    case 0x20:
        bResult = MirrorColorKeyBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 0x22:
        bResult = MirrorShadowBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 0x102:
        bResult = InterlacedShadowBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 0x202:
        bResult = CheckerboardShadowBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    case 0x400:
    case 0x402:
        bResult = TranslucentBlendBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    default:
        bResult = PixelCopyColorKeyBlit(destRect, pDestBase, destPitch, srcRect);
        break;
    }

    if (pTargetSurface != g_pDDrawWorkSurface) {
        return pTargetSurface->Unlock(NULL) == 0 ? bResult : 0;
    }
    if (bWasLockGuarded != 0) {
        return bResult;
    }
    if (g_worldBoard.bSurfaceLockGuard == 0) {
        return bResult;
    }
    if (g_pDDrawWorkSurface->Unlock(NULL) != 0) {
        return bResult;
    }
    g_worldBoard.bSurfaceLockGuard = 0;
    return bResult;
}

// FUNCTION: LOCO 0x42b960
void LocoBitmap::BlitOntoBitmap(RECT destRect, LocoBitmap *pDestBitmap, RECT srcRect,
                                      unsigned int flags) {
    RestoreOverlapBlt(destRect, pDestBitmap->pSurface, srcRect, flags);
}

// bConverted==1 rect-clamp helper for RestoreOverlapBlt's flags&0x40 path (never exercised by the one
// known call site, Resize's mode=1). Validates pSrcRect via a literal self-intersect (an
// IsRectEmpty(pSrcRect) surrogate), builds an adjusted dest-origin rect compensating for any
// negative-origin overhang in pSrcRect, clamps that against the surface's own real bounds
// (GetSurfaceDesc), then writes the clamped result back through both pDestRect and pSrcRect.
// FUNCTION: LOCO 0x42c590
unsigned char LocoBitmap::ClampBlitRects(RECT *pDestRect, IDirectDrawSurface *pSurface,
                                          RECT *pSrcRect, unsigned int flags) {
    DDSurfaceDescPadded0x7c u;
    memset(&u, 0, sizeof(u));
    u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
    pSurface->GetSurfaceDesc(&u.ddsd);

    RECT surfaceRect;
    SetRect(&surfaceRect, 0, 0, u.ddsd.dwWidth, u.ddsd.dwHeight);
    RECT bitmapRect;
    SetRect(&bitmapRect, 0, 0, this->width, this->height); // sic: computed, never read

    int adjLeft = pDestRect->left - (pSrcRect->left < 0 ? pSrcRect->left : 0);
    int adjTop = pDestRect->top - (pSrcRect->top < 0 ? pSrcRect->top : 0);

    RECT srcCopy;
    IntersectRect(&srcCopy, pSrcRect, pSrcRect);
    if (IsRectEmpty(&srcCopy)) {
        goto done;
    }

    RECT adjRect;
    adjRect.left = adjLeft;
    adjRect.top = adjTop;
    adjRect.right = adjLeft + (srcCopy.right - srcCopy.left);
    adjRect.bottom = adjTop + (srcCopy.bottom - srcCopy.top);

    RECT clampRect;
    IntersectRect(&clampRect, &adjRect, &surfaceRect);
    if (IsRectEmpty(&clampRect)) {
        goto done;
    }

    if (adjLeft > 0) adjLeft = 0;
    if (adjTop > 0) adjTop = 0;

    {
        int clampW = clampRect.right - clampRect.left;
        int clampH = clampRect.bottom - clampRect.top;
        int srcAdjLeft = srcCopy.left - adjLeft;
        int srcAdjTop = srcCopy.top - adjTop;

        SetRect(pDestRect, clampRect.left, clampRect.top, clampRect.right, clampRect.bottom);
        SetRect(pSrcRect, srcAdjLeft, srcAdjTop, srcAdjLeft + clampW, srcAdjTop + clampH);
    }

done:
    return 0; // sic: every exit path shares this return, even full success
}

// bConverted==0 counterpart to ClampBlitRects: clips pSrcRect against this bitmap's own bounds
// (rather than self-intersecting it) before the shared clamp-against-surface-bounds logic, and
// reports success/failure via a real return value instead of always returning 0.
// FUNCTION: LOCO 0x42c700
unsigned char LocoBitmap::ClampBlitRectsRaw(RECT *pDestRect, IDirectDrawSurface *pSurface,
                                          RECT *pSrcRect, unsigned int flags) {
    DDSurfaceDescPadded0x7c u;
    memset(&u, 0, sizeof(u));
    u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
    pSurface->GetSurfaceDesc(&u.ddsd);

    RECT surfaceRect;
    SetRect(&surfaceRect, 0, 0, u.ddsd.dwWidth, u.ddsd.dwHeight);
    RECT bitmapRect;
    SetRect(&bitmapRect, 0, 0, this->width, this->height);

    int adjLeft = pDestRect->left - (pSrcRect->left < 0 ? pSrcRect->left : 0);
    int adjTop = pDestRect->top - (pSrcRect->top < 0 ? pSrcRect->top : 0);

    RECT clippedSrc;
    IntersectRect(&clippedSrc, &bitmapRect, pSrcRect);
    if (IsRectEmpty(&clippedSrc)) {
        return 0;
    }

    RECT adjRect;
    adjRect.left = adjLeft;
    adjRect.top = adjTop;
    adjRect.right = adjLeft + (clippedSrc.right - clippedSrc.left);
    adjRect.bottom = adjTop + (clippedSrc.bottom - clippedSrc.top);

    RECT clampRect;
    IntersectRect(&clampRect, &adjRect, &surfaceRect);
    if (IsRectEmpty(&clampRect)) {
        return 0;
    }

    if (adjLeft > 0) adjLeft = 0;
    if (adjTop > 0) adjTop = 0;

    int clampW = clampRect.right - clampRect.left;
    int clampH = clampRect.bottom - clampRect.top;
    int srcAdjLeft = clippedSrc.left - adjLeft;
    int srcAdjTop = clippedSrc.top - adjTop;

    SetRect(pDestRect, clampRect.left, clampRect.top, clampRect.right, clampRect.bottom);
    SetRect(pSrcRect, srcAdjLeft, srcAdjTop, srcAdjLeft + clampW, srcAdjTop + clampH);
    return 1;
}

// Fresh-allocates this bitmap at (width,height): sets bConverted/bOwnsPalette, frees any
// existing pPixels, calls AllocSurface(width,height), then fills the new buffer -- raw-8bpp
// path (bConverted==0): reuses g_pSharedPalette instead of an owned one if paletteRefThreshold
// (masked 0xffff) <= g_nSharedPaletteRefCount, then memsets pPixels to fillByte; converted path
// (bConverted==1): DDBLTFX colorfill (dwFillColor=0) on pSurface. Distinct from Resize, which
// reuses AllocSurface directly and preserves old content -- this is a from-scratch create+fill,
// no preserve step. Called by CreditsWnd::InitPreviewCanvasLazy(0xd8,0xc4,0,0,0) to build a
// fresh 216x196 raw-8bpp canvas.
// EFFECTIVE MATCH: byte_diff 54/291, insns 102/100. Two already-documented intrinsic residual
// classes: the DDBLTFX-field-init-vs-vtable-load ordering tie-break (same as Resize's own
// EFFECTIVE MATCH note above) and the bSuccess-reload-vs-register-cache tie-break (same as
// AllocSurface's). See docs/PARKED.md.
// FUNCTION: LOCO 0x42a850
unsigned char LocoBitmap::CreateAndFill(int width, int height, int bConverted,
                                       unsigned int paletteRefThreshold, unsigned char fillByte) {
    unsigned char bSuccess = 1;
    this->bConverted = bConverted;
    bOwnsPalette = 0;
    if (pPixels != NULL) {
        ::operator delete(pPixels);
        pPixels = NULL;
    }

    if (bConverted == 0) {
        bSuccess = AllocSurface(width, height);
        if ((paletteRefThreshold & 0xffff) <= (unsigned int)g_nSharedPaletteRefCount) {
            pPalette = g_pSharedPalette;
        }
        memset(pPixels, fillByte, height * width);  // idiom-exempt: runtime length
    }

    if (bConverted == 1) {
        bSuccess = AllocSurface(width, height);
        // sic: this->bConverted was just set to the param value (1) above, and AllocSurface
        // doesn't touch it -- so this check is always false, unreachable dead code. Confirmed
        // via raw disasm (own separate epilogue at this path, not shared with the fn's other
        // exits); reproduced faithfully, not fixed.
        if (this->bConverted == 0 && pPixels != NULL) {
            memset(pPixels, 0, this->height * this->width);  // idiom-exempt: runtime length
            return bSuccess;
        }
        if (pSurface != NULL) {
            DDBLTFX fx;
            fx.dwSize = sizeof(DDBLTFX);
            fx.dwFillColor = 0;
            pSurface->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);
        }
    }
    return bSuccess;
}

// Fills this bitmap in place: raw-8bpp path (bConverted==0 && pPixels != NULL) memsets pPixels
// to fillByte; converted path: DDBLTFX colorfill on pSurface using fillColor. Called by
// CreditsWnd::InitPreviewCanvasLazy(9,0) right after CreateAndFill to stamp the fresh canvas
// with fill byte 9.
// EFFECTIVE MATCH (DIFF 2/124): structure byte-identical; the whole residual is WHICH of the
// two memset-size operands lands in the register. The original emits
// `mov ecx,[eax+0xc]; imul ecx,[eax+0x8]` (height into the reg, width folded into the imul);
// mine picks the reverse. MSVC CANONICALIZES the commutative multiply, so this is NOT the
// source operand order -- `width * height` and `height * width` compile byte-identical (v360),
// as does hoisting either operand into its own named local first (the copy is folded away,
// same as v359's 0x402690 finding). No source lever known. See docs/PARKED.md.
// FUNCTION: LOCO 0x42aa90
void LocoBitmap::Fill(unsigned char fillByte, unsigned int fillColor) {
    if (bConverted == 0 && pPixels != NULL) {
        memset(pPixels, fillByte, height * width);  // idiom-exempt: runtime length
        return;
    }
    if (pSurface != NULL) {
        DDBLTFX fx;
        fx.dwSize = sizeof(DDBLTFX);
        fx.dwFillColor = fillColor;
        pSurface->Blt(NULL, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);
    }
}

// The CONVERTED-surface twin of HasOpaquePixelInRect below, defined at the end of this same TU
// (0x42c9f0) -- forward-declared here because HasOpaquePixelInRect calls it. ⚠ It is a free
// __stdcall function, not a member: it never reads `this`, because it does not scan the bitmap at
// all. Ghidra address-boxed it into the LocoBitmap namespace by .text adjacency; the name there
// matches this one. See its definition for the full description.
unsigned char __stdcall HasOpaquePixelInWorkSurfaceRect(RECT r); // FUN_0042c9f0

// "Is ANY pixel inside r non-transparent": scans the raw-8bpp span row by row and returns 1 at
// the first non-zero byte, 0 if the whole rect is transparent. Takes the RECT BY VALUE, like
// TestPixelCollision. On a converted (DirectDraw-surface) bitmap it forwards to its work-surface
// twin instead -- note that twin ignores `this`, so the converted path does not actually consult
// this bitmap's own surface.
// EFFECTIVE MATCH (142 B vs 147, insns 65/67, total 146584). The entire scan -- both loops, the
// zero-trip guards, the row-stride advance and all three exit blocks -- is BYTE-IDENTICAL to the
// original. The whole residual is one register coin flip in the PROLOGUE: `this` and `p` have
// overlapping live ranges (the address sum starts before the last `this` field read), and the
// original resolves it by keeping `this` in its incoming ECX and paying a `mov ecx,edx` once the
// address is final, where cl here pays a `mov edx,ecx` at entry instead. That entry instruction
// is also what stops cl from sinking the four callee-saved pushes past the bConverted branch the
// way the original does (the original's converted path saves and restores NOTHING), which is
// where the 5-byte length shortfall and the epilogue tail-merge difference both come from.
// Levers measured and REFUTED (do not re-run): writing the loop bounds inline instead of as the
// nRows/nCols locals is much WORSE (190289) -- the named locals are the original's own shape.
// One lever DID pay and is baked in: the raw-8bpp scan must be the FALL-THROUGH with the
// converted forward as the trailing tail (`if (bConverted == 0) { ... } return twin(r);`); the
// natural-reading `if (bConverted != 0) return twin(r);` early-exit form emits the converted
// block first and scores 254870.
// FUNCTION: LOCO 0x42c950
unsigned char LocoBitmap::HasOpaquePixelInRect(RECT r) {
    if (bConverted == 0) {
        int nRows = r.bottom - r.top;
        int nCols = r.right - r.left;
        unsigned char *p = pPixels + r.top * width + r.left;
        for (int y = 0; y < nRows; y++) {
            for (int x = 0; x < nCols; x++) {
                if (*p != 0) {
                    return 1;
                }
                p++;
            }
            p += width - nCols;
        }
        return 0;
    }
    return HasOpaquePixelInWorkSurfaceRect(r);
}

// The CONVERTED-surface twin of HasOpaquePixelInRect: instead of scanning the caller's own
// bitmap it locks the SHARED DirectDraw work surface (reusing g_worldBoard's cached
// DDSURFACEDESC scratch and its bSurfaceLockGuard "stay locked between calls" flag) and tests
// that surface's 16bpp pixels against the transparency colour key. A pixel counts as OPAQUE
// only when its red channel AND its blue channel both differ from 0x1f -- i.e. the key is
// magenta (red==max && blue==max), and the blue side needs no shift because blue is the low 5
// bits in both the 555 and the 565 layouts.
// ⚠ It never reads `this`, so LocoBitmap::HasOpaquePixelInRect's converted path does not
// actually consult the bitmap it was called on -- see docs/engine-bugs.md.
// EFFECTIVE MATCH (282 B vs 284, insns 89/91, total 99073). The prologue+lock block (0x00-0x4e)
// and the whole unlock block + epilogue (0xe9-end) are BYTE-IDENTICAL, and every instruction of
// both loops corresponds 1:1; the entire residual is ONE callee-saved-register coin flip and the
// two instructions it saves. The original spends EBP on nCols and therefore has NOTHING free, so
// it hoists only `g_nRedShiftPos` (which must live in CL anyway) into the loop preheader and
// re-loads BOTH colour masks from memory every iteration -- `mov edx,[g_nBBitMask]; and edx,esi`,
// keeping the pixel alive in ESI. cl here puts nCols in EDI instead, leaving EBP free for a
// hoisted g_nRBitMask, which lets the blue test fold its load into the operand
// (`and eax,[g_nBBitMask]`, destroying the now-dead pixel) -- that fold is one of the 2 missing
// instructions. The other is the original's `test eax,eax` before the zero-trip `jbe`: the
// original schedules two flag-clobbering ops between `and eax,0xffff` and the branch, where cl
// here reuses the mask's own flags.
// One lever DID pay and is baked in: the red channel's `(short)` cast (NOT `unsigned short`) is
// what produces the original's 16-bit `cmp ax,0x1f` -- with `unsigned short` cl proves the upper
// bits zero (they were just masked with 0xffff) and widens it to a 3-byte `cmp ebx,0x1f`. The
// blue side needs no such cast: its operand is the raw 16-bit pixel load, whose upper bits are
// unknown, so cl must compare 16-bit anyway. See docs/CODEGEN.md.
// Levers measured and REFUTED (do not re-run): operand order across BOTH masks
// (`g_nRBitMask & pixel` vs `pixel & g_nRBitMask`, and the same for blue, and the cast moved onto
// the mask instead of the result) is BYTE-IDENTICAL in all four spellings -- cl canonicalizes the
// commutative AND; dropping the `pixel` local and writing `*p` twice (91/91 insns but total
// 102868); hoisting the `pixel` DECLARATION out of both loops, and declaring the loop counters
// outside them VC5-old-for-scope style (both byte-identical to the current form); computing the
// two channels into named locals before the `if` (127762); declaring `p` before nRows/nCols
// (127333). `int nRedShift = g_nRedShiftPos;` scores marginally better (97353) but is REJECTED on
// evidence, not score: a source local pins the load BEFORE the zero-trip guard, where the
// original's load sits AFTER it, in the preheader -- i.e. compiler LICM, so no such local existed.
// See docs/PARKED.md.
// FUNCTION: LOCO 0x42c9f0
unsigned char __stdcall HasOpaquePixelInWorkSurfaceRect(RECT r) {
    unsigned char bFound = 0;

    if (g_worldBoard.bSurfaceLockGuard == 0) {
        memset(g_worldBoard.aSurfaceDescScratch, 0, sizeof(g_worldBoard.aSurfaceDescScratch));
        g_worldBoard.aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)g_worldBoard.aSurfaceDescScratch,
                                            0, NULL) == 0) {
            g_worldBoard.bSurfaceLockGuard = 1;
        }
    }

    unsigned short nStride = (unsigned short)((int)g_worldBoard.aSurfaceDescScratch[4] >> 1);
    unsigned short nRows = (unsigned short)(r.bottom - r.top);
    unsigned short nCols = (unsigned short)(r.right - r.left);
    unsigned short *p = (unsigned short *)g_worldBoard.aSurfaceDescScratch[9]
                        + (nStride * r.top + r.left);

    for (unsigned int y = 0; y < nRows; y++) {
        for (unsigned int x = 0; x < nCols; x++) {
            unsigned short pixel = *p;
            if ((short)((g_nRBitMask & pixel) >> g_nRedShiftPos) != 0x1f &&
                (unsigned short)(g_nBBitMask & pixel) != 0x1f) {
                bFound = 1;
                break;
            }
            p++;
        }
        // sic: on the early `break` above p has NOT walked the rest of the row, so this advance
        // leaves it mis-aligned for every remaining row -- the original scans on regardless
        // instead of returning once the answer is known. See docs/engine-bugs.md.
        p += nStride - nCols;
    }

    if (g_worldBoard.bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == 0) {
        g_worldBoard.bSurfaceLockGuard = 0;
    }
    return bFound;
}

// The single-pixel counterpart of HasOpaquePixelInRect: "is the pixel at (x, y) transparent,
// treating everything outside the clip rect r as transparent too". Takes the RECT BY VALUE like
// its two siblings. Raw-8bpp path: index 0 is the transparent color. Converted path: locks this
// bitmap's OWN surface (unlike HasOpaquePixelInWorkSurfaceRect, which asks the shared work
// surface) and tests the 16bpp pixel against the same magenta key (red==0x1f && blue==0x1f).
// EFFECTIVE MATCH (319 B vs 321, insns 100/100, total 61711). The bounds checks, the converted
// fall-through layout, the whole Lock block, both load-from-local epilogues and the 8bpp tail
// pair instruction-for-instruction; the whole residual is the 16bpp pixel-test cluster, same
// intrinsic scheduling class as 0x42c9f0's: the original keeps the pixel in dx (with a dead
// `lea eax,[ecx+2*eax]` remat artifact), the red result in eax, and loads g_nBBitMask INTO a
// register (`mov ecx,[g_nBBitMask]; and ecx,edx`) where cl here folds it (`and eax,[g_nBBitMask]`
// -- the 2-byte shortfall); and cl promotes bTransparent to bl across the Unlock call (a
// xor/bl-select) where the original keeps it memory-resident throughout.
// Two levers DID pay and are baked in, do not undo: (a) the bounds checks must be spelled
// `x > r.right` / `y > r.bottom` (the original's `cmp reg,mem; jg` form -- `r.right < x` flips
// the operands); (b) the converted path is the FALL-THROUGH with the 8bpp scan as the trailing
// tail (`if (bConverted == 1) { ... return ...; }` then the raw check), OPPOSITE of
// HasOpaquePixelInRect's baked-in lever. Levers measured and REFUTED (do not re-run): a
// pPixel pointer local, an nStride local, the `!(red==0x1f && blue==0x1f)` parenthesization, and
// declaring pixel before the desc are ALL byte-identical. See docs/PARKED.md.
// FUNCTION: LOCO 0x42cb10
unsigned char LocoBitmap::IsPixelTransparentAtMaybe(RECT r, int x, int y) {
    unsigned char bTransparent = 1;
    if (x < r.left || x > r.right || y < r.top || y > r.bottom) {
        return 1;
    }
    if (bConverted == 1) {
        DDSurfaceDescPadded0x7c u;
        memset(&u, 0, sizeof(u));
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        HRESULT hr = pSurface->Lock(NULL, &u.ddsd, DDLOCK_WAIT, NULL);
        if (hr != 0) {
            Ddraw_HResultToString(1); // sic: return value unused, see extern comment
            return 0;
        }
        unsigned short pixel =
            ((unsigned short *)u.ddsd.lpSurface)[((unsigned int)u.ddsd.lPitch >> 1) * y + x];
        if ((short)((g_nRBitMask & pixel) >> g_nRedShiftPos) != 0x1f ||
            (unsigned short)(g_nBBitMask & pixel) != 0x1f) {
            bTransparent = 0;
        }
        pSurface->Unlock(NULL);
        return bTransparent;
    }
    if (pPixels[y * width + x] != 0) {
        bTransparent = 0;
    }
    return bTransparent;
}
