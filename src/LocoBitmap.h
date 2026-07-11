// Shared declaration for LocoBitmap -- the general 8bpp-bitmap-to-DDraw-surface
// class (vtable 0x477d28; also used standalone as a small canvas widget, e.g.
// CreditsWnd's animation canvas). Phase 4's 4th TU, src/LocoBitmap.cpp.
#pragma once

#include <ddraw.h>

class istream;

// The original binary's DDSURFACEDESC-shaped stack scratch used for
// CreateSurface is 0x7c bytes (a 0x1f-dword zeroing loop; the dwSize field's
// own stored literal is 0x7c) -- 0x10 bytes BIGGER than this toolchain's real
// sizeof(DDSURFACEDESC)==0x6c (confirmed: field offsets for dwFlags/dwHeight/
// dwWidth/ddsCaps.dwCaps all match this header's real layout exactly, so it's
// not a different field layout, just a bigger reserved tail). Reproduced
// as-is, not "fixed" to sizeof(DDSURFACEDESC) -- see CLAUDE.md's "reproduce
// original engine/toolchain quirks literally" rule. Shared across every TU
// that builds a CreateSurface scratch this way (LocoBitmap.cpp, CreditsWnd.cpp)
// -- moved here 2026-07-18 (v200) per CLAUDE.md's "never duplicate a struct"
// rule once a 2nd consumer (CreditsWnd::Show) needed the identical shape.
struct DDSurfaceDescPadded0x7c {
    DDSURFACEDESC ddsd;
    unsigned int reserved[4];
};

extern "C" extern int g_nLiveLocoBitmaps; // DAT_00485254 -- live-instance counter

struct LocoBitmap {
    // NOTE: no explicit vtbl field -- the compiler synthesizes the vtable
    // pointer at +0x0 automatically because the class has a virtual dtor;
    // declaring one by hand here double-counts it and pushes every real
    // field 4 bytes late (confirmed empirically: a first attempt with an
    // explicit `void *vtbl;` member matched the ctor's STRUCTURE exactly
    // but every single field-init store landed at orig_offset+4).
    int bConverted;        // 0 = raw 8bpp, 1 = live DDraw surface
    // ⚠ UNSIGNED, not `int` (corrected v454). The only site in the whole codebase that can tell
    // the two models apart is WidgetPickerObj0x477cc8::ReloadActiveSaveState's
    // `pBitmap->width / pDesc->nTotalFrameCount` (0x42a027): the original emits an UNSIGNED
    // `xor edx,edx; div edi`, where an `int` width promotes the whole expression to a signed
    // `cdq; idiv`. Every other consumer only compares these for equality or passes them as
    // `int` arguments, which is why ~20 already-matched functions are indifferent -- the retype
    // was measured repo-wide and is byte-neutral everywhere except that one division, which it
    // takes from DIFF(172) to EXACT.
    unsigned width;
    unsigned height;
    unsigned char bOwnsPalette;
    unsigned char bUnk11;
    unsigned short *pPalette;   // 256 entries, pre-converted 16bpp LUT
    unsigned char *pPixels;     // raw 8bpp, width*height; freed after Convert
    IDirectDrawSurface *pSurface; // lazy

    LocoBitmap();
    LocoBitmap(LocoBitmap *src);
    // Defined inline (not in LocoBitmap.cpp) so the compiler can actually INLINE it at every
    // use site -- confirmed empirically: an out-of-line .cpp definition compiled to a real CALL
    // from both the auto-generated scalar-deleting-destructor AND Resize's stack-local
    // teardown, which doesn't match either original site (both show the body inlined directly,
    // see FUNCTION 0x42a140's marker in LocoBitmap.cpp and Resize's own comment). Releases
    // the owned palette/pixel buffers and the DDraw surface, then decrements the live-instance
    // counter.
    virtual ~LocoBitmap() {
        if (bOwnsPalette == 1 && pPalette != NULL) {
            ::operator delete(pPalette);
            pPalette = NULL;
            bOwnsPalette = 0;
        }
        if (pPixels != NULL) {
            ::operator delete(pPixels);
            pPixels = NULL;
        }
        if (pSurface != NULL) {
            pSurface->Release();
            pSurface = NULL;
        }
        --g_nLiveLocoBitmaps;
    }

    // Resizes to (width,height), reallocating pPixels or pSurface per AllocSurface, filling the
    // fresh buffer (raw-8bpp noise stamp 0x8, or a magenta-ish 0xa0c0d1 DDBLT_COLORFILL for the
    // live-surface case), then restores whatever old content overlaps the new viewport-clamped
    // bounds from a copy-ctor'd snapshot of the pre-resize state (raw path: CopyOverlapRaw;
    // converted path: RestoreOverlapBlt).
    void Resize(int width, int height);
    // Raw-8bpp counterpart to PixelCopyBlit used by Resize's resize-preserve step: `this`
    // is the PRE-resize snapshot (the source), pDestPixels/destWidth describe the POST-resize
    // buffer being written into. destRect.right/bottom are dead, same convention as
    // PixelCopyBlit.
    void CopyOverlapRaw(RECT destRect, unsigned char *pDestPixels, int destWidth, RECT srcRect);
    // NOT declared: LocoBitmap::CopyRectRawColorKey, 0x42c330 (149 B) -- the color-keyed twin of
    // CopyOverlapRaw above and its immediate .text neighbour, identical apart from the per-pixel
    // skip (source index 0 leaves the destination byte alone). NOT one of RestoreOverlapBlt's 11
    // dispatch siblings: its only caller is the station-clock chime tick (0x447400, 4 call sites),
    // which passes another LocoBitmap's own pPixels/width as pDestPixels/destWidth, i.e. it
    // composites one raw-8bpp bitmap onto another. It is fully transcribed and compiles EXACT --
    // the body is verbatim in docs/PARKED.md's v524 section, ready to paste.
    // ⚠ It stays undeclared PURELY on the declaration-count dial documented at
    // IsPixelTransparentAtMaybe below. Re-measured 2026-07-31 from a clean baseline:
    //   N=3 (adding this one alone): -1531 B. src/WorldBoardMaybe.cpp 4208 -> 3257 (the 0x457ce0
    //       canary), src/ThumbnailBmp.cpp 923 -> 403, src/TilePlacedObj.cpp 1091 -> 1031.
    //       Against its own +421 B of gain that is net -1110 B, so it is not viable alone.
    //   N=4 (this one + a dummy): only src/TilePlacedObj.cpp stays down, for a net of +361 B --
    //       but padding with a fake declaration is forbidden by this header's own rule below, so
    //       N=4 is unreachable until a SECOND REAL declaration is needed here. Land this one
    //       together with that declaration, whenever it turns up, and re-measure both.
    // Converted-surface (live DDraw target) resize/redraw blit dispatcher -- called by Resize
    // with `this` = the pre-resize snapshot (temp), pTargetSurface = the POST-resize surface being
    // restored into. Two structurally different halves depending on THIS object's OWN bConverted:
    // bConverted==1 does a straight surface-to-surface Blt (this->pSurface -> pTargetSurface);
    // bConverted==0 Locks pTargetSurface (reusing WorldBoardMaybe's cached lock state if
    // pTargetSurface is the shared back-buffer, else a local one-shot Lock/Unlock) and dispatches
    // through `flags` to one of the 11 raw-pixel blit siblings above. See docs/subsystems.md.
    unsigned char RestoreOverlapBlt(RECT destRect, IDirectDrawSurface *pTargetSurface,
                                          RECT srcRect, unsigned int flags);
    // Thin RestoreOverlapBlt wrapper taking the DEST as a LocoBitmap* (extracts its own
    // pSurface) instead of a raw IDirectDrawSurface* -- lets a caller composite `this` onto
    // another LocoBitmap-backed canvas without reaching into pDestBitmap's internals itself.
    // `this` is the SOURCE bitmap (RestoreOverlapBlt's own "this" reads FROM it), matching
    // every one of its 4 call sites: MenuNodeObj0x477568::Draw blits pIconDesc->pShadowBitmap
    // onto the owner's own realized canvas (pKindDesc->pOwnedObjA); the two
    // BuildDrawTargetComposite/BuildPreviewCanvasA-shaped functions blit a freshly-loaded
    // TileKind icon frame onto a lazily-allocated 1280x1024 composite canvas.
    void BlitOntoBitmap(RECT destRect, LocoBitmap *pDestBitmap, RECT srcRect,
                              unsigned int flags);
    // Clamps/repositions pDestRect and pSrcRect (both in place) against this bitmap's own
    // width/height and pSurface's real surface bounds (via GetSurfaceDesc) -- called only when
    // RestoreOverlapBlt's flags has bit 0x40 set (never true at the one known call site, Resize's
    // mode=1). bConverted==1 variant: validates pSrcRect via a literal self-intersect
    // (IntersectRect(&tmp,pSrcRect,pSrcRect), an IsRectEmpty(pSrcRect) surrogate) rather than
    // clipping it against anything -- confirmed from raw disasm (Ghidra's own decompile of this
    // function is unfixably broken: unaff_EBX/unaff_EBP register reads and a wrong stack-frame
    // size persist even after clear+recreate+re-prototype, see docs/PARKED.md). sic: every exit
    // path (both early IsRectEmpty bailouts and the full-success path) shares ONE physical
    // epilogue that unconditionally returns 0 -- the return value is dead at the only call site
    // regardless.
    unsigned char ClampBlitRects(RECT *pDestRect, IDirectDrawSurface *pSurface,
                                      RECT *pSrcRect, unsigned int flags);
    // Same as ClampBlitRects, bConverted==0 variant: pSrcRect is genuinely clipped against this
    // bitmap's own {0,0,width,height} rect (not self-intersected) before the shared
    // clamp-against-surface-bounds logic. Unlike its sibling, the two early-bailout sites and the
    // success path do NOT share one epilogue (each compiles its own independent inline tail) --
    // returns 0 on either early-out, 1 on full success.
    unsigned char ClampBlitRectsRaw(RECT *pDestRect, IDirectDrawSurface *pSurface,
                                         RECT *pSrcRect, unsigned int flags);

    void Convert();
    unsigned char AllocSurface(int width, int height);
    // Real signature recovered from Convert()'s call site (0x42a4e0): the
    // caller builds TWO 4-int stack blocks via the compiler's by-value-struct
    // passing idiom (`sub esp,0x10; mov [esp+n],field` x4), both shaped
    // {0,0,width,height} -- i.e. two RECTs passed BY VALUE, not 8 raw ints.
    // Only destRect.left/top (destX/destY) are read by the body -- right/
    // bottom are dead, kept only because the caller constructs a full RECT.
    // srcRect's all 4 fields are used (left/top=origin, right/bottom=end).
    unsigned char PixelCopyBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                      RECT srcRect);
    // Same idiom as PixelCopyBlit, plus a branchless color-key trick: each pixel first
    // self-stores the CURRENT dest pixel into pPalette[0] (clobbering the live palette entry,
    // sic -- reproduced, not fixed), then writes dest = pPalette[srcIndex]. Net effect: a source
    // index of 0 round-trips the dest pixel unchanged (transparent), any other index does a
    // normal opaque color lookup -- without a per-pixel branch. This is the dispatcher's
    // default/fallback blit (RestoreOverlapBlt case 0 and its switch default).
    unsigned char PixelCopyColorKeyBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                              RECT srcRect);
    // Same idiom as PixelCopyBlit, but scans the SOURCE row right-to-left while the
    // DESTINATION is still written left-to-right (a horizontal-flip blit), and uses an explicit
    // branch for color-key transparency (source index 0 = skip, dest pixel left untouched).
    unsigned char MirrorColorKeyBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                           RECT srcRect);
    // Nearest-neighbor 2x2 upscale blit, no color-key (RestoreOverlapBlt dispatch case 5/0x85). Each
    // source pixel becomes a 2x2 dest block. Two SIC quirks reproduced from the original, not
    // fixed: the per-row source stride is copyW (srcRect.right-srcRect.left), not this->width --
    // i.e. it assumes the copied region IS a full pixel row; and srcRect.top is never read (the
    // scan always starts at row 0 of pPixels) -- both only give correct results when the caller
    // passes srcRect.left==0/srcRect.top==0, unlike the general PixelCopyBlit.
    unsigned char UpscaleBlit2x2(RECT destRect, void *pDestBase, unsigned int destPitch,
                                       RECT srcRect);
    // Same 2x2 upscale idiom as UpscaleBlit2x2, plus an explicit color-key branch (source
    // index 0 = skip, dest pixels left untouched) instead of a branchless trick (RestoreOverlapBlt
    // dispatch case 4/0x84). Same two SIC quirks: source row stride is copyW not this->width, and
    // srcRect.top is never read.
    unsigned char UpscaleBlit2x2ColorKey(RECT destRect, void *pDestBase, unsigned int destPitch,
                                                RECT srcRect);
    // Nearest-neighbor 3x3 upscale blit with color-key (RestoreOverlapBlt dispatch 0x10/0x11). Each
    // source pixel becomes a 3x3 dest block; source index 0 skips the write entirely (all 9 dest
    // pixels left untouched). Same two SIC quirks as the 2x2 siblings: source row stride is
    // copyW not this->width, and srcRect.top is never read.
    unsigned char UpscaleBlit3x3ColorKey(RECT destRect, void *pDestBase, unsigned int destPitch,
                                                RECT srcRect);
    // 1:1 translucent/shadow blend blitter with color-key (RestoreOverlapBlt dispatch 0x400/0x402):
    // averages each opaque source pixel's palette color with the CURRENT dest pixel (50% alpha),
    // via a dynamically-computed channel-bleed guard mask derived from the live 555/565 surface
    // format (g_nSurfaceFormatTag/g_nGreenWidth/g_nRedShiftPos), not the precomputed
    // g_wChannelBleedGuardMask global (that's the fixed-shadow family's own copy). Source index 0
    // skips the blend (dest pixel left untouched). Sic: unconditionally clobbers pPalette[0]/[1]
    // as scratch every pixel (round-tripped dest pixel / its half-brightness copy), same trick as
    // PixelCopyColorKeyBlit's pPalette[0] self-store -- never read back, dead writes
    // reproduced faithfully. Uses this->width for source addressing (not copyW), unlike the
    // Upscale family.
    unsigned char TranslucentBlendBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                              RECT srcRect);
    // Fixed-mask shadow/translucency blit with color-key (RestoreOverlapBlt dispatch case 2), first of
    // the 4 "shadow" siblings. Each pixel first round-trips the CURRENT dest pixel through
    // pPalette[0]/[1] scratch (pPalette[0]=destPixel, pPalette[1]=(destPixel & mask)>>1, a fixed
    // 50%-darkened copy of the dest pixel itself, not the source), then writes
    // dest=pPalette[srcIndex] -- branchless like PixelCopyColorKeyBlit: source index 0 =
    // transparent (round-trips pPalette[0], i.e. the unchanged dest pixel), index 1 = the
    // half-darkened shadow of whatever was already there, 2-255 = a normal opaque palette lookup.
    // Uses the FIXED g_wChannelBleedGuardMask (not TranslucentBlendBlit's dynamically
    // recomputed one). Addresses the source via this->width (not copyW), like
    // TranslucentBlendBlit.
    unsigned char ShadowBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                    RECT srcRect);
    // Mirrored/reversed-scan sibling of ShadowBlit (RestoreOverlapBlt dispatch case 0x22):
    // same slot-0/1 branchless shadow trick, but the source is scanned right-to-left like
    // MirrorColorKeyBlit (and shares its per-row "if (copyW != 0) { do {...} } " control-flow
    // shape, not ShadowBlit's simpler combined guard). Sic: the source origin is
    // srcRect.right, NOT srcRect.right - 1 like MirrorColorKeyBlit -- confirmed via raw
    // disasm (no "-1" anywhere in the pointer setup) -- so it reads one column past the intended
    // rightmost column on the first iteration of every row, reproduced faithfully.
    unsigned char MirrorShadowBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                          RECT srcRect);
    // Interlaced sibling of ShadowBlit (RestoreOverlapBlt dispatch 0x102): same slot-0/1
    // branchless shadow trick, scanned forward like ShadowBlit, but touches only every
    // OTHER dest/source row -- pDst/pSrc both advance by 2*destStride/2*width per outer
    // iteration. A row-parity preamble: if srcRect.top is odd, srcRect.top and destRect.top are
    // both advanced by one first, so the interlaced scan always starts on an even source row.
    // Reads g_wChannelBleedGuardMask directly per pixel (no g_nShadowMaskScratch global
    // write), matching MirrorShadowBlit's own mask-read shape rather than ShadowBlit's.
    unsigned char InterlacedShadowBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                              RECT srcRect);
    // Checkerboard-dithered sibling of ShadowBlit (RestoreOverlapBlt dispatch 0x202): same
    // slot-0/1 branchless shadow trick, but a running on/off toggle skips every other pixel in
    // raster-scan order -- the toggle is NEVER reset at row boundaries, so whether a given row
    // starts "on" or "off" depends on whether copyW is even (same phase every row, vertical
    // stripes) or odd (phase flips each row, true checkerboard). Seeded from srcRect.top's parity
    // (same signed "% 2" idiom as InterlacedShadowBlit, but used only to seed the toggle
    // here, not to adjust the scan origin).
    unsigned char CheckerboardShadowBlit(RECT destRect, void *pDestBase, unsigned int destPitch,
                                                RECT srcRect);
    unsigned char Load(const char *path, unsigned int flags, int width, int height);
    // Real signature recovered 2026-07-15 from raw disasm (call site at
    // 0x42ad38 in Load): takes the BITMAPINFOHEADER BY VALUE (a 40-byte
    // `sub esp,0x28`+`rep movs` stack copy, not a pointer) plus a scratch
    // buffer and Load's own `flags` param forwarded through -- Ghidra's
    // decompile of both this function and its caller was wrong (shows only 1
    // visible explicit param on each side) until this was worked out; not yet
    // fixed in Ghidra itself (set_function_prototype rejects by-value struct
    // params via this server's REST endpoint), so the decompile will still
    // show the old wrong shape until a script-level fix lands.
    unsigned char BuildPaletteLUT(istream *pStream, BITMAPINFOHEADER bih, void *pPaletteScratch, unsigned int flags);
    // Pixel-perfect collision test vs another raw-8bpp bitmap (0x42a540). The
    // by-value RECT/pointer/RECT arg order (r1, other, r2) is what the call site
    // pushes -- other sits BETWEEN the two rects, not first.
    unsigned char TestPixelCollision(RECT r1, LocoBitmap *other, RECT r2);
    // 0x42c950 -- "is ANY pixel inside r non-transparent": scans the raw-8bpp span row by row and
    // returns 1 at the first non-zero byte, 0 if the whole rect is transparent. On a converted
    // (DirectDraw-surface) bitmap it forwards to its 0x42c9f0 sibling instead. Takes the RECT BY
    // VALUE, like TestPixelCollision. Transcribed v461 (EFFECTIVE -- see the definition).
    unsigned char HasOpaquePixelInRect(RECT r);
    // 0x42cb10 -- the single-pixel counterpart of HasOpaquePixelInRect: "is the pixel at
    // (x, y) transparent, treating everything outside the clip rect r as transparent too".
    // Returns 1 when (x, y) falls outside r, otherwise `pPixels[width*y + x] == 0` on the
    // raw-8bpp path; the converted path locks this bitmap's own surface and tests the 16bpp
    // pixel against the magenta key. Takes the RECT BY VALUE like its two siblings above.
    // Transcribed v510 (EFFECTIVE -- see the definition). Its sole caller is
    // PeerTrainSlotQueueMaybe::CheckDerailCollisionMaybe, which uses a 0 return (an OPAQUE
    // pixel) as the derail hit. ⚠ Ghidra's decompile of the converted path is broken: it
    // silently DROPS the entire pixel-load/mask-test block and shows the return as a byte of
    // the local desc pointer -- that was previously recorded here as an original bug, but the
    // raw disasm returns the real pixel test result. Trust the definition's comment, not the
    // decompile.
    unsigned char IsPixelTransparentAtMaybe(RECT r, int x, int y);
    // ⚠ MEASURED PRICE of the two declarations here (2026-07-29, when
    // PeerTrainSlotQueueMaybe::CheckDerailCollisionMaybe landed and needed the first of them).
    // This header is on the same declaration-count dial documented in src/TilePlacedObj.h, and
    // its victims are its own TU plus two consumers -- all three measured from a clean baseline:
    //   N=1 (IsPixelTransparentAtMaybe alone): -893 B. src/LocoBitmap.cpp 399 -> 275 B,
    //       src/ThumbnailBmp.cpp 923 -> 403 B, src/TutorialWnd.cpp 9445 -> 9196 B.
    //   N=2 (+ HasOpaquePixelInWorkSurfaceRect): -373 B. src/ThumbnailBmp.cpp recovers its full
    //       923 B; the other two do NOT move, so the three victims sit on different phases of
    //       the curve rather than flipping together.
    // N=2 is what is kept -- both declarations are true and one of them was required. Accepted
    // deliberately: the TU it unlocks is +561 B of newly transcribed body, so it nets +188 B of
    // coverage. ⚠ Do NOT pad this header with a third dummy declaration chasing the remaining
    // 373 B (src/TilePlacedObj.h's rule (b)); a real declaration is currency, a fake one is not.
    // 0x42c9f0 -- the DirectDraw-surface counterpart HasOpaquePixelInRect forwards to on a
    // converted bitmap: same "is any pixel inside r non-transparent" question, but asked of the
    // shared 16bpp WORK SURFACE rather than of any one bitmap's own pixels, which is why it
    // reads no `this` (Ghidra sees a plain __stdcall taking the four RECT dwords). It locks the
    // work surface through g_worldBoard's own lock guard/desc scratch on entry and unlocks on
    // the way out. Transcribed v461 (EFFECTIVE -- see the definition).
    static unsigned char HasOpaquePixelInWorkSurfaceRect(RECT r);
    // Fresh-allocate this bitmap at (width,height) (0x42a850): sets bConverted/bOwnsPalette,
    // frees any existing pPixels, calls AllocSurface(width,height), then fills the new buffer --
    // raw-8bpp path (bConverted==0): reuses g_pSharedPalette instead of an owned one if
    // paletteRefThreshold <= g_nSharedPaletteRefCount, then memsets pPixels to fillByte;
    // converted path (bConverted==1): DDBLTFX colorfill (dwFillColor=0) on pSurface. Distinct
    // from Resize (0x42a610), which reuses AllocSurface directly and preserves old content --
    // this is a from-scratch create+fill, no preserve step. EFFECTIVE MATCH -- see
    // src/LocoBitmap.cpp's own comment and docs/PARKED.md.
    unsigned char CreateAndFill(int width, int height, int bConverted,
                                    unsigned int paletteRefThreshold, unsigned char fillByte);
    // Fills this bitmap in place (0x42aa90): raw-8bpp path (bConverted==0 && pPixels != NULL)
    // memsets pPixels to fillByte; converted path: DDBLTFX colorfill on pSurface using
    // fillColor. EFFECTIVE MATCH -- see src/LocoBitmap.cpp's own comment and docs/PARKED.md.
    void Fill(unsigned char fillByte, unsigned int fillColor);
};
