// CreditsWnd's Show/Hide-cycle methods -- see src/CreditsWnd.h and docs/subsystems.md's
// CreditsWnd entry for the full field derivation.

#include "CreditsWnd.h"

#include "WindowBase.h" // CenterRectInRect (canonical home)

#include <string.h>
#include <stdlib.h>
#include <fstream.h>
#include <strstrea.h>

#include "UIResources.h"
#include "DSoundChannel.h" // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free
#ifdef LOCO_PORT
#include "PortMode.h" // port-only: RGB565 surface pinning, see port/README.md
#endif

extern IDirectDraw2 *g_pDDraw2; // DAT_00485440

CreditsWnd *g_pCreditsWnd; // DAT_004fd390

// FUNCTION: LOCO 0x40f1c0 // EXACT MATCH
// Ctor: base PopupWndBase init, then the one-shot field zeroing (icon, lazy-resource
// pointers, timer/anim state) and an empty rectCanvas. Store order below matches the
// original's own emission order (hIcon first, SetRectEmpty's lea/push scheduled early by
// /Og but the call itself last).
CreditsWnd::CreditsWnd(HINSTANCE hInstance, UINT resourceId)
    : PopupWndBase(hInstance, resourceId)
{
    hIcon = NULL;
    bResourcesLoaded = 0;
    bAnimationStartedFlag = 0;
    nTimerId = 0;
    pTileDesc = NULL;
    pFrameBitmap = NULL;
    pAnimSurface = NULL;
    frameRampCounter = 0xfffffff6;
    pCanvasWidget = NULL;
    pOwnedObj2 = NULL;
    pTileBitmap = NULL;
    nTileKindId = 0;
    SetRectEmpty(&rectCanvas);
}

// FUNCTION: LOCO 0x40fe50 // EFFECTIVE MATCH -- 333/312 insns structurally matching, byte_diff
// 192/1004 (use --len 1004, the real Ghidra body size -- NOT the candidate's own compiled
// length, which this function's earlier buffer-size miss made look deceptively close to 1069;
// see the szLine sic comment below). Builds the 3-part credits resource path -- install path
// prefix plus credits\ plus Credits.dat -- into szPath, then reads it line-by-line into
// szResourcePathBuf. Newline-delimited entries -- CORRECTS the earlier asterisk-delimited
// doc guess, made before this function was read. Tries the RF archive first via
// g_RFIndex.LoadResource plus istrstream, falling back to a loose-file
// ifstream::open ios::nocreate when the archive lookup fails, either because the archive isn't
// open or the resource isn't found in it. Either path reads lines via istream::getline until a
// case-insensitive -9 sentinel line or EOF, concatenating each line plus a newline separator
// into szResourcePathBuf -- this project's own DecodeAndDrawFrame scans these entries by
// index. szPath is a plain `= ""` aggregate initializer (the compiler copies the pooled empty
// literal's own byte, then zeroes the tail), fully overwritten by the following wsprintfA. Sole residual (5 occurrences, ~18 extra insns): every
// istream::getline() call inlines its own internal lock()+get()+unlock() sequence (ios.h's own
// inline body) -- the original inlines the leading lock() the same way we do, but keeps the
// TRAILING unlock() as a real out-of-line call to a shared, /Gy-folded ios::unlock() body
// (elsewhere in the exe), while our compile inlines unlock() too at every site. An explicit
// `bool bHaveArchive = ...;` intermediate (matching the original's own setne/test bool
// materialization for the outer RF-archive guard) closed a separate small residual; no lever
// found yet for the unlock()-inlining asymmetry itself -- likely an MSVC inline-budget heuristic
// tied to cumulative inlined-code size within the function, not source-steerable. Own future
// session if revisited; see docs/PARKED.md.
void CreditsWnd::BuildResourcePath()
{
    memset(szResourcePathBuf, 0, sizeof(szResourcePathBuf));

    char szPath[0x105] = "";

    bool bLoaded = false;
    ifstream fileStream;
    char szLine[263]; // sic: getline() below is passed 0x1000 as its own max-length cap, far
                        // larger than this real buffer -- a genuine overflow-prone engine bug
                        // (confirmed via the original's own total stack-frame size, 0x27c, which
                        // only has room for a small buffer here, not a 4096-byte one)

    wsprintfA(szPath, "%s%s%s", g_pInstallPathPrefix, "credits\\", "Credits.dat");

    bool bHaveArchive = g_RFIndex.pFile != NULL;
    if (bHaveArchive) {
        int nSize;
        void *pRfBuf = g_RFIndex.LoadResource((const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nSize);
        if (pRfBuf != NULL) {
            istrstream *pRfStream = new istrstream((char *)pRfBuf, nSize);
            if (pRfStream != NULL) {
                szResourcePathBuf[0] = '\0';
                pRfStream->getline(szLine, 0x1000, '\n');
                while (_stricmp(szLine, "-9") != 0 && !pRfStream->eof()) {
                    strcat(szResourcePathBuf, szLine);
                    strcat(szResourcePathBuf, "\n");
                    pRfStream->getline(szLine, 0x1000, '\n');
                }
                bLoaded = true;
                delete pRfStream;
            }
            _free(pRfBuf);
        }
    }
    if (!bLoaded) {
        fileStream.open(szPath, ios::nocreate);
        szResourcePathBuf[0] = '\0';
        fileStream.getline(szLine, 0x1000, '\n');
        while (_stricmp(szLine, "-9") != 0 && !fileStream.eof()) {
            strcat(szResourcePathBuf, szLine);
            strcat(szResourcePathBuf, "\n");
            fileStream.getline(szLine, 0x1000, '\n');
        }
        fileStream.close();
    }
}

// FUNCTION: LOCO 0x40f6a0 // EFFECTIVE MATCH -- 50/50 insns structurally matching, byte_diff
// 12/188. Sole residual: the pTileDesc field store (`mov [esi+0x134],eax`) lands 2
// instructions later than the original (after the vtable-slot-1 deref for the following
// GetOrLoadFrameBitmap(0,0) call, instead of immediately after the
// TileKind_GetOrLoadDescriptor call that produced it) -- pure store-scheduling, same S-class
// tie-break as Yoda lesson #29/#30. 3 source-shape variants tried, all byte-identical: the plain
// 2-statement form below, splitting into an explicit `CursorDesc *pDesc` local assigned to
// the field first, and a nested `pFrameBitmap = (pTileDesc = ...)->GetOrLoad...()`
// single-statement form. See docs/PARKED.md.
void CreditsWnd::InitPreviewCanvasLazy()
{
    if (bResourcesLoaded != 1) {
        pTileDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3daf);
        pFrameBitmap = pTileDesc->GetOrLoadFrameBitmap(0, 0);
        pCanvasWidget = new LocoBitmap();
        pCanvasWidget->CreateAndFill(0xd8, 0xc4, 0, 0, 0);
        pCanvasWidget->Fill(9, 0);
        bResourcesLoaded = 1;
    }
}

// FUNCTION: LOCO 0x40f480 // EXACT MATCH (slot-4 override, see the plate comment in Ghidra)
void CreditsWnd::OnExit()
{
    SetModalCapture(1);
    PopupWndBase::OnExit();
    if (bResourcesLoaded != 0) {
        pTileDesc->ReleaseRef();
        pTileDesc = NULL;
        if (pCanvasWidget != NULL) {
            delete pCanvasWidget;
        }
        pCanvasWidget = NULL;
        if (pOwnedObj2 != NULL) {
            pOwnedObj2->ReleaseRef();
            pOwnedObj2 = NULL;
        }
        bResourcesLoaded = 0;
    }
    KillTimer(hwndSelf, nTimerId);
    bAnimationStartedFlag = 0;
    if (pAnimSurface != NULL) {
        pAnimSurface->Release();
        pAnimSurface = NULL;
    }
}

// FUNCTION: LOCO 0x40f2a0 // EFFECTIVE MATCH (slot-8 override, see the plate comment in
// Ghidra) -- 72/72 insns structurally matching, byte_diff 51/285. Residual is a pure
// register-allocation/instruction-scheduling tie-break around the CreateSurface call-arg setup
// and the RGB565-branch DecodeAndDrawFrame(nFrameIndex) call (same S/r-only-diff class as Yoda
// lesson #29/#30's documented register-swap residuals): tried 2 field-store-order permutations
// for the DDSURFACEDESC scratch (matching the raw disasm's own dwSize/ddsCaps/dwWidth/dwHeight/
// dwFlags order, then a dwFlags-before-dwWidth/dwHeight variant) and removing the intermediate
// `hr` local in favor of a direct `if (CreateSurface(...) == 0)` -- all three compiled
// byte-identical, ruling out simple source-shape levers. See docs/PARKED.md.
void CreditsWnd::Show()
{
    HWND hWnd = hwndSelf;
    PopupWndBase::Show();
    InitPreviewCanvasLazy();
    this->RefreshClientRect();
    ShowWindow(hWnd, SW_SHOWNORMAL);
    SetFocus(hWnd);
    nTimerId = SetTimer(hWnd, 0x7a, 0x32, NULL);
    bAnimationStartedFlag = 0;
    nAnimProgress = 0x10;
    frameRampCounter = 0xfffffff6;
    frameRampAccum = 0xa0;
    nFrameIndex = 1;
    BuildResourcePath();
    if (pAnimSurface == NULL) {
        DDSurfaceDescPadded0x7c u;
        memset(&u, 0, sizeof(u));
        u.ddsd.dwSize = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        u.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        u.ddsd.dwWidth = 0xd8;
        u.ddsd.dwHeight = 0xc4;
        u.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
#ifdef LOCO_PORT
        Port_ForceRgb565(&u.ddsd); // PORT: pin 565, do not inherit the desktop format
#endif
        if (g_pDDraw2->CreateSurface(&u.ddsd, &pAnimSurface, NULL) == 0) {
            if (u.ddsd.ddpfPixelFormat.dwRBitMask == 0x7c00) {
                wColorKey = 0x6b94;
                DecodeAndDrawFrame(nFrameIndex);
                return;
            }
            wColorKey = 0xd714;
            DecodeAndDrawFrame(nFrameIndex);
        }
    }
}

// FUNCTION: LOCO 0x40f3c0 // EXACT MATCH (WM_TIMER handler for the 0x7a-id timer, see the
// plate comment in Ghidra) -- frameRampAccum had to be retyped signed `int` (was `unsigned
// int`): the nAnimProgress = frameRampAccum/10 site compiles to the signed imul+sar+add
// divide-by-10 idiom in the original, not the unsigned mul+shr idiom an unsigned type would
// produce (confirmed via raw disasm before the fix).
void CreditsWnd::AdvanceAnimationFrame()
{
    if (frameRampCounter < 0xf) {
        frameRampCounter = frameRampCounter + 2;
    }
    if (0 < frameRampCounter) {
        if (frameRampAccum < 1000 - frameRampCounter) {
            frameRampAccum = frameRampAccum + frameRampCounter;
        } else {
            unsigned int nFrame = nFrameIndex + 1;
            nAnimProgress = 0;
            frameRampCounter = 0xfffffff6;
            frameRampAccum = 0;
            nFrameIndex = nFrame;
            unsigned char ok = DecodeAndDrawFrame(nFrame);
            if (ok == 0) {
                nFrameIndex = 1;
                DecodeAndDrawFrame(1);
            }
        }
        nAnimProgress = frameRampAccum / 10;
        BlitFadeCanvas();
        CommitScreenUpdate(hwndSelf, 0, 0);
    }
}

// FUNCTION: LOCO 0x40f510 // EXACT MATCH
// Realizes the window: loads the app icon (resource 0x65), centers a 0xf8x0xe8 frame rect
// over the desktop rect, then hands off to the PopupWndBase base Create with class style
// 0x86000000. Same shape as BuildToolCursorWnd::Create, including the if/return-1/return-0
// tail: the original narrows the base's unsigned-char return with test al,al; setne al,
// which the if-form emits but a `... != 0` return does not (that spells neg/sbb/neg).
// The nested GetClientRect(GetDesktopWindow(), ...) spelling is load-bearing
// (lea-before-call, same as RefreshClientRect).
bool CreditsWnd::Create(HWND hwndOwner)
{
    hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCE(0x65));

    RECT rectDesktop;
    GetClientRect(GetDesktopWindow(), &rectDesktop);

    RECT rectFrame;
    rectFrame.left = 0;
    rectFrame.right = 0xf8;
    rectFrame.top = 0;
    rectFrame.bottom = 0xe8;

    CenterRectInRect(&rectDesktop, &rectFrame);
    if (PopupWndBase::Create(0, hwndOwner, rectFrame.left, rectFrame.top,
                             rectFrame.right - rectFrame.left, rectFrame.bottom - rectFrame.top,
                             NULL, hIcon, 0, 0x86000000, 0, 0)) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x40f5c0 (Ghidra: FUN_0040f5c0 -- CreditsWnd vtable slot 0x18 override of
// PopupWndBase::RefreshClientRect, confirmed by reading the class vtable dword at 0x477698).
// Gated on bResourcesLoaded: chains the base implementation, then re-centers the popup over
// the DESKTOP rect at the tile descriptor's native size, plants rectCanvasPlacement at
// (15,20)-(241,218), and sizes rectCanvas to pCanvasWidget's own pixel dimensions offset by
// that placement. The nested GetClientRect(GetDesktopWindow(), ...) spelling is load-bearing:
// it makes cl emit the `lea` of the dest rect BEFORE the GetDesktopWindow call, matching the
// original (the two-statement hwndDesktop-local spelling does not -- see EditCardWnd::Create).
void CreditsWnd::RefreshClientRect()
{
    if (bResourcesLoaded != 0) {
        RECT rectDesktop;
        RECT rectFrame;

        PopupWndBase::RefreshClientRect();
        GetClientRect(GetDesktopWindow(), &rectDesktop);
        rectFrame.left = 0;
        rectFrame.right = pTileDesc->nativeWidth;
        rectFrame.top = 0;
        rectFrame.bottom = pTileDesc->nativeHeight;
        CenterRectInRect(&rectDesktop, &rectFrame);
        SetWindowPos(hwndSelf, NULL, rectFrame.left, rectFrame.top,
                     rectFrame.right - rectFrame.left, rectFrame.bottom - rectFrame.top, 0x90);
        SetRect(&rectCanvasPlacement, 0xf, 0x14, 0xf1, 0xda);
        SetRect(&rectCanvas, 0, 0, pCanvasWidget->width, pCanvasWidget->height);
        OffsetRect(&rectCanvas, rectCanvasPlacement.left, rectCanvasPlacement.top);
    }
}

// FUNCTION: LOCO 0x410280 // EFFECTIVE MATCH -- fade-blend blit of pCanvasWidget into
// pOffscreenSurface at rectCanvas, positioned against pAnimSurface via a
// DDBLT_KEYSRC Blt for the fade-in/fade-out edges. Branches on nAnimProgress
// (frameRampAccum/10, a 0-100ish value): under 0x10 = fade in (1-3 dithered passes), 0x10-0x54
// = steady single blit, over 0x54 = fade out. Called by AdvanceAnimationFrame after every
// frame update. The 3 early "goto done" bailouts and the 2 fade-loop bodies are 5 independent
// sites sharing ONE physical tail (`done:`) in the original -- the fade-in loop's own
// post-loop `return 0;` is a distinct, separately-compiled exit instead (matches the
// do-while's own natural post-loop fallthrough for fade-out, but not for fade-in, since
// fade-in's return sits inside the same block as its own `if (n > 0)` guard). Also needed the
// documented do-while-peel fix (an outer `if (n>0)` wrapping a `do{}while()` peels the loop's
// first iteration under /O2 -- rewritten as a plain `while (n>0){}`, per CLAUDE.md) and a real
// `SetRect()` call for the 0xd8x0xc4 scratch rects (an aggregate `{0,0,0xd8,0xc4}` literal
// silently constant-folds away the call the original genuinely makes). 217/211 insns
// structurally matching, byte_diff 105/643. Sole residual: the per-iteration fade-loop srcRect
// {0,0,w,h} has its 2 constant (0) fields hoisted OUT of the loop by our compile (a provably
// loop-invariant store into a stable stack slot) while the original recomputes all 4 fields
// fresh every iteration -- 4 independent source-shape variants (aggregate literal vs.
// field-by-field assignment, 2 field-write orders, with/without a named destRect local) all
// compiled byte-identical, confirming this is an intrinsic VC5 store-hoisting choice, not
// source-steerable. See docs/PARKED.md.
int CreditsWnd::BlitFadeCanvas()
{
    int w = rectCanvas.right - rectCanvas.left;
    int h = rectCanvas.bottom - rectCanvas.top;
    RECT srcRect = {0, 0, w, h};
    pCanvasWidget->RestoreOverlapBlt(rectCanvas, pOffscreenSurface, srcRect, 1);

    int progress = nAnimProgress;
    if (progress < 0x10) {
        if ((0x10 - progress) / 3 > 3)
            goto done;
    } else if (progress > 0x54) {
        if ((progress - 0x54) / 3 > 3)
            goto done;
    }
    if ((0x10 - progress) / 3 >= 3)
        goto done;

    {
        RECT r1;
        SetRect(&r1, 0, 0, 0xd8, 0xc4);
        OffsetRect(&r1, rectCanvasPlacement.left, rectCanvasPlacement.top);
        RECT r2;
        SetRect(&r2, 0, 0, 0xd8, 0xc4);
        pOffscreenSurface->Blt(&r1, pAnimSurface, &r2, 0x1008000, NULL);
    }

    progress = nAnimProgress;
    if (progress < 0x10) {
        int n = (0x10 - progress) / 3;
        if (n >= 3) n = 3;
        int w2 = rectCanvas.right - rectCanvas.left;
        int h2 = rectCanvas.bottom - rectCanvas.top;
        if (n > 0) {
            while (n > 0) {
                RECT s = {0, 0, w2, h2};
                pCanvasWidget->RestoreOverlapBlt(rectCanvas, pOffscreenSurface, s, 0x400);
                n--;
            }
            return 0;
        }
    } else if (progress > 0x54) {
        int n = (progress - 0x54) / 3;
        if (n >= 3) n = 3;
        int w2 = rectCanvas.right - rectCanvas.left;
        int h2 = rectCanvas.bottom - rectCanvas.top;
        while (n > 0) {
            RECT s = {0, 0, w2, h2};
            pCanvasWidget->RestoreOverlapBlt(rectCanvas, pOffscreenSurface, s, 0x400);
            n--;
        }
    }
done:
    return 0;
}

// FUNCTION: LOCO 0x40f890 // EFFECTIVE MATCH -- vtable slot 0x1c override of
// PopupWndBase::OnDrawContent. Ignores its PAINTSTRUCT* arg. Gated on bResourcesLoaded:
// one-shot sets bAnimationStartedFlag, RestoreOverlapBlt's pFrameBitmap over the whole
// rectWindow, then the standard present tail (BlitFadeCanvas + SetCursorDesc(cursorNormal)
// + CommitScreenUpdate). asmscore insns 64/60, byte_diff 88-99 depending on spelling.
// Sole residual: the original MATERIALIZES the {0,0,w,h} src rect as a real stack local
// (4 source-order field stores: right=EAX, bottom=EDX, left=EDI(0), top=EBX(0) -- two
// separate zero regs) with a fused offset-order copy into the outgoing by-value arg slot,
// while every plain-local spelling folds straight into the arg slot under /Og (203 B vs
// the original's 228 B). Probes refuted, do not re-run: aggregate `{0,0,w,h}`; bare
// 4-field assignment in both orders; `{0,0,0,0}` + right/bottom assignment; pointer-deref
// `*(&srcRect)` (folds); RECT-returning helper (not inlined without __inline = real call,
// 225 B; with __inline = folds to 203 B); whole-struct copy `srcRect = rectWindow` +
// collapse (folds, 188 B). See docs/PARKED.md.
void CreditsWnd::OnDrawContent(PAINTSTRUCT *pPs)
{
    if (bResourcesLoaded) {
        if (bAnimationStartedFlag == 0) {
            bAnimationStartedFlag = 1;
        }
        if (bAnimationStartedFlag != 0) {
            RECT srcRect;
            srcRect.right = rectWindow.right - rectWindow.left;
            srcRect.bottom = rectWindow.bottom - rectWindow.top;
            srcRect.left = 0;
            srcRect.top = 0;
            ((LocoBitmap *)pFrameBitmap)->RestoreOverlapBlt(rectWindow, pOffscreenSurface, srcRect, 0);
        }
        BlitFadeCanvas();
        SetCursorDesc(cursorNormal.nMaskSurfaceKey, cursorNormal.pDesc, 0, 1);
        CommitScreenUpdate(hwndSelf, 0, 0);
    }
}

// FUNCTION: LOCO 0x40f980 -- decodes/advances to frame nFrameIndex: scans
// szResourcePathBuf for the nFrameIndex-th ASTERISK-delimited entry (a run of one or
// more '*'/'\n' bytes is the separator -- distinct from BuildResourcePath's own '\n'
// getline-granularity; a frame's own caption can itself span several newline-terminated
// lines), parses an optional leading "<NNN>" TileKind-id tag (-> nTileKindId), looks up
// that TileKind's own realized frame bitmap (pOwnedObj2/pTileBitmap) and positions
// rectTile, blits it via pAnimSurface, then DrawTextA's the remaining caption text
// (vertically centered via leading '\n' padding) onto pAnimSurface's own DC. Returns 1 on
// success, 0 if nFrameIndex is out of range (the outer scan's strchr('*') fails to find another
// entry, which throws -- caught by an empty catch(int) whose continuation address is the same
// "line buffer is empty" check the normal empty-entry path also hits, since szLine is
// pre-zeroed before the scan). NOT a pixel-level RF-Huffman decode (an earlier docs pass
// guessed that; corrected v199/v200). extraout_ECX in Ghidra's own decompile is really `this`,
// held in ebx across the whole function body (its prologue's `push ecx`+later
// `mov [ebp-0x10],esp` immediately overwrites that slot with EH bookkeeping, not `this` --
// `mov ebx,ecx` right after is the real, sole `this` save). Likewise Ghidra's own decompile of
// the tail GDI block shows `(HDC)nFrameIndex` -- that's a decompiler artifact of the SAME
// class as the "dead parameter slot reused as EH scratch" lesson (CLAUDE.md/Wav_ParseAndLoad):
// once nFrameIndex's only real use (the outer loop bound) is behind it, the compiler is free to
// place a genuinely separate `HDC hdc;` local at that now-dead stack slot -- pAnimSurface's
// own GetDC(&hdc)/ReleaseDC(hdc) vtable calls (slots 0x44/0x68) confirm it, no different from
// any other stack-slot reuse. The newline-counting loop Ghidra rendered as
// `local_1094[0] = pcVar3[1]` is a SEPARATE decompiler-rendering artifact (conflating a
// register reload with a memory write) -- the real disasm is a plain register-only scan with
// NO store to szLine at all; do not transcribe a self-mutating loop. DDBLTFX's sizeof is a
// genuine 0x64 (100) in this project's own period toolchain/vc50/INCLUDE/DDRAW.H (confirmed by
// hand-counting its fields) -- unlike DDSURFACEDESC, no DDSurfaceDescPadded0x7c-style wrapper
// is needed here. The color-fill Blt's own DDBLTFX only ever gets dwSize set -- dwFillColor
// (and everything else) is read UNINITIALIZED off the stack by the real driver call, and the
// SetColorKey DDCOLORKEY's own dwColorSpaceHighValue is likewise never explicitly set -- both
// transcribed faithfully (`// sic:`) as genuine, harmless (immediately overpainted) engine bugs
// rather than "fixed". Own future session if a byte-match pass is wanted -- this pass is a
// faithful content transcription, not yet iterated against the compiled diff.
unsigned char CreditsWnd::DecodeAndDrawFrame(int nFrameIndex)
{
    g_UIResources.PlayUiSound(0x5597);

    DDCOLORKEY ddck;
    ddck.dwColorSpaceHighValue = wColorKey; // sic: see plate comment
    ddck.dwColorSpaceLowValue = wColorKey;
    pAnimSurface->SetColorKey(DDCKEY_SRCBLT, &ddck);

    RECT fillRect = {0, 0, 0xd8, 0xc4};
    DDBLTFX fx;
    fx.dwSize = 0x64; // sic: sizeof(DDBLTFX) in this SDK -- dwFillColor left uninitialized,
                        // see plate comment
    pAnimSurface->Blt(&fillRect, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);

    char szLine[4096];
    memset(szLine, 0, sizeof(szLine));

    char *p = szResourcePathBuf;
    try {
        for (unsigned int i = 0; i < (unsigned int)nFrameIndex; i++) {
            p = strchr(p, '*');
            if (p == NULL) {
                throw 0x47e3ac;
            }
            while ((*p == '*' || *p == '\n') &&
                   p < szResourcePathBuf + sizeof(szResourcePathBuf)) {
                p++;
            }
        }
    } catch (int) {
    }
    strcpy(szLine, p);

    char *pCloseTag;
    if (szLine[0] == '<' && (pCloseTag = strchr(szLine, '>')) != NULL) {
        char tag[4096];
        strcpy(tag, szLine);
        tag[0] = ' ';
        *strchr(tag, '>') = '\0';
        nTileKindId = atoi(tag);

        char rest[4096];
        strcpy(rest, strchr(szLine, '>') + 1);
        strcpy(szLine, rest);
    } else {
        nTileKindId = 0;
    }

    char *pStar = strchr(szLine, '*');
    if (pStar != NULL) {
        *strchr(szLine, '*') = '\0';
    }

    if (szLine[0] == '\0') {
        return 0;
    }

    char szCaption[4096] = "";

    unsigned int nNewlines = 0;
    for (char *q = szLine; *q != '\0'; q++) {
        if (*q == '\n') {
            nNewlines++;
        }
    }
    unsigned int nPad = 0;
    if (nNewlines < 0xd) {
        nPad = (0xd - nNewlines) >> 1;
        if (nPad != 0) {
            memset(szCaption, '\n', nPad); // idiom-exempt: nPad is a computed vertical-centering
                                             // pad count (range 0..6, from (0xd-nNewlines)>>1),
                                             // not a struct/buffer size -- no sizeof applies
        }
    }
    szCaption[nPad] = '\0';
    strcat(szCaption, szLine);

    if (pOwnedObj2 != NULL) {
        pOwnedObj2->ReleaseRef();
        pOwnedObj2 = NULL;
    }
    if (nTileKindId != 0) {
        pOwnedObj2 = g_UIResources.TileKind_GetOrLoadDescriptor(nTileKindId);
        if (pOwnedObj2 != NULL) {
            pTileBitmap = pOwnedObj2->GetOrLoadFrameBitmap(0, 0);
            SetRect(&rectTile, 0, 0, pOwnedObj2->nativeWidth, pOwnedObj2->nativeHeight);
            OffsetRect(&rectTile, (0xd8 - pOwnedObj2->nativeWidth) / 2,
                       (0xc4 - pOwnedObj2->nativeHeight) / 2);
        } else {
            nTileKindId = 0;
        }
    }
    if (nTileKindId != 0) {
        RECT srcRect = {0, 0, pOwnedObj2->nativeWidth, pOwnedObj2->nativeHeight};
        pTileBitmap->RestoreOverlapBlt(rectTile, pAnimSurface, srcRect, 0);
    }

    HDC hdc;
    pAnimSurface->GetDC(&hdc);
    COLORREF oldColor = SetTextColor(hdc, 0xff5c00);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, g_UIResources.m_hFont16);
    RECT textRect = {0, 0, 0xd8, 0xc4};
    DrawTextA(hdc, szCaption, -1, &textRect, DT_CENTER | DT_VCENTER);
    SelectObject(hdc, oldFont);
    SetBkMode(hdc, oldBkMode);
    SetTextColor(hdc, oldColor);
    pAnimSurface->ReleaseDC(hdc);
    return 1;
}

// FUNCTION: LOCO 0x40f270 (??_GCreditsWnd scalar deleting dtor -- compiler-generated around
// ~CreditsWnd() below; no source of its own)

// FUNCTION: LOCO 0x40f290
// vtable slot 0. Empty body: the vtable re-stamp (0x477680) and the PopupWndBase base
// chain are all compiler-generated under /GX.
CreditsWnd::~CreditsWnd()
{
}

// g_pApp / g_nScreenState reached without pulling AppWindow.h into the whole TU (the
// IsNetShuttingDownMaybe inline is the same local predicate four sibling TUs carry --
// its byte return is what the original's SETZ normalization shows).
class AppWindow;
extern AppWindow *g_pApp;           // DAT_004aa4a0
extern int g_nScreenState;          // DAT_004851f4
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x40f760
// vtable slot 0x7c (WM_CLOSE). While the app is alive and not already tearing down, the
// credits window swallows its close (returns 0); only once shutdown is underway does the
// base PopupWndBase::OnClose run. The guard's SETZ normalization is the byte-returning
// inline predicate (see GameNet.cpp's note on the same idiom).
LRESULT CreditsWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        return 0;
    }
    return PopupWndBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x3c -- WM_RBUTTONDOWN is simply WM_LBUTTONDOWN here, forwarded THROUGH the
// vtable exactly as an unqualified call to a virtual member compiles; do not "optimize" it to a
// class-qualified call. UNMARKED: this body ICF-folds onto 0x451520, whose marker lives on
// TutorialWnd::OnRButtonDown (src/TutorialWnd.cpp). The PopupWndBase hierarchy's copies fold to a
// DIFFERENT address than the other family's purely because the slot displacement differs.
LRESULT CreditsWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x40f7a0
// Vtable slot 0x34 (WM_LBUTTONDOWN) -- "click to skip the credits". Only meaningful once the
// scroll has actually started: AdvanceAnimationFrame's one-shot raises bAnimationStartedFlag, and
// from then on a click lowers it again, runs this window's own OnExit (slot 1, +0x4 -- the
// virtual that releases the credits resources and returns to the front end) and acknowledges with
// UI sound 0x5015, swallowing the click with a 0 return. Before the animation starts the click is not this window's business and
// falls through to the family's shared DefWindowProcStub default -- the same "not mine" tail
// AlbumCardWnd's and EditCardWnd's own handlers use.
LRESULT CreditsWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!bAnimationStartedFlag) {
        return PopupWndBase::OnLButtonDown(hwndMsg, msg, wParam, lParam);
    }
    bAnimationStartedFlag = 0;
    OnExit();
    g_UIResources.PlayUiSound(0x5015);
    return 0;
}

// FUNCTION: LOCO 0x40f870
// Vtable slot 0x2c (WM_TIMER passthrough) -- the credits scroll's clock. While the animation is
// running, a tick aimed at this window's own hwndSelf advances exactly one frame; anything else
// (a tick before the scroll started, or one aimed at another window) is ignored. The message is
// swallowed either way -- this handler never reaches a DefWindowProc tail, which is why it needs
// no `this`-live base call the way OnLButtonDown above does.
//
// The hwndMsg == hwndSelf guard is the ONLY reader of the timer's target window here; the timer
// id itself (nTimerId, the 0x7a SetTimer above) is never re-checked, so any timer on this window
// drives the scroll.
LRESULT CreditsWnd::OnTimerDefault(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bAnimationStartedFlag && hwndMsg == hwndSelf) {
        AdvanceAnimationFrame();
    }
    return 0;
}

// FUNCTION: LOCO 0x40f840
// Vtable slot 0x4c (WM_MOUSEMOVE). Mouse movement only means anything once the scroll is running:
// until bAnimationStartedFlag goes up the window swallows the message (returns 0) instead of
// letting PopupWndBase do its hover/cursor work. Same class-qualified base-virtual tail as
// OnLButtonDown above -- and for the same reason, `this` stays live across the call.
LRESULT CreditsWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!bAnimationStartedFlag) {
        return 0;
    }
    return PopupWndBase::OnMouseMove(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x40f7f0
// Vtable slot 0x50 (WM_KEYDOWN) -- the KEYBOARD half of the "skip the credits" gesture, and
// instruction-for-instruction the same dismiss OnLButtonDown runs (clear the flag, OnExit, UI
// sound 0x5015). The one difference is the else path: a keypress arriving before the scroll has
// started is simply SWALLOWED, where the mouse handler passes its click on to the base default.
// That asymmetry is the original's, and it is why this body is an `if` with no early return.
LRESULT CreditsWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bAnimationStartedFlag) {
        bAnimationStartedFlag = 0;
        OnExit();
        g_UIResources.PlayUiSound(0x5015);
    }
    return 0;
}

// FUNCTION: LOCO 0x40f820
// Vtable slot 0x60 (WM_KILLFOCUS). Losing focus ends the credits UNCONDITIONALLY -- no
// bAnimationStartedFlag guard the way every other handler here has, and no acknowledging sound,
// just lower the flag and run OnExit. Being taken off-screen is not a gesture to confirm.
LRESULT CreditsWnd::OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bAnimationStartedFlag = 0;
    OnExit();
    return 0;
}
