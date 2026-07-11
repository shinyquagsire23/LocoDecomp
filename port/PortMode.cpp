// PORT SCAFFOLDING -- compiled only in a `-D LOCO_PORT` build. See PortMode.h.
#include "PortMode.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/LocoBitmap.h" // DDSurfaceDescPadded0x7c -- the 124-byte DX5 descriptor
                        // every CreateSurface site in the repo already uses

// The window CheckMinimumDisplaySpec will accept, and the height range the UI was
// built for (AlbumCardWnd already treats anything over 800x600 as the large-screen
// layout, so 1024x768 is a well-trodden size rather than an experimental one).
#define PORT_MIN_WIDTH 800
#define PORT_MAX_WIDTH 1280
#define PORT_MIN_HEIGHT 600
#define PORT_MAX_HEIGHT 1024
#define PORT_DEFAULT_WIDTH 1024
#define PORT_DEFAULT_HEIGHT 768

static int Port_Clamp(int n, int nLo, int nHi)
{
    if (n < nLo) return nLo;
    if (n > nHi) return nHi;
    return n;
}

void Port_ClampScreenSize(int *pnWidth, int *pnHeight)
{
    int nWidth = PORT_DEFAULT_WIDTH;
    int nHeight = PORT_DEFAULT_HEIGHT;
    const char *pszSize = getenv("LOCO_PORT_SIZE");

    if (pszSize != NULL) {
        int w = 0, h = 0;
        const char *p = pszSize;
        while (*p >= '0' && *p <= '9') w = w * 10 + (*p++ - '0');
        if (*p == 'x' || *p == 'X') {
            p++;
            while (*p >= '0' && *p <= '9') h = h * 10 + (*p++ - '0');
        }
        if (w > 0 && h > 0) {
            nWidth = w;
            nHeight = h;
        }
    }

    // Never claim to be larger than the real desktop -- the main window is a
    // borderless WS_POPUP at the origin, so an oversized "screen" would put part
    // of the game off the edge with no way to reach it.
    if (nWidth > *pnWidth) nWidth = *pnWidth;
    if (nHeight > *pnHeight) nHeight = *pnHeight;

    *pnWidth = Port_Clamp(nWidth, PORT_MIN_WIDTH, PORT_MAX_WIDTH);
    *pnHeight = Port_Clamp(nHeight, PORT_MIN_HEIGHT, PORT_MAX_HEIGHT);
}

void Port_ClampDesktopRect(RECT *pRect)
{
    int nWidth = pRect->right - pRect->left;
    int nHeight = pRect->bottom - pRect->top;

    Port_ClampScreenSize(&nWidth, &nHeight);
    pRect->right = pRect->left + nWidth;
    pRect->bottom = pRect->top + nHeight;
}

static IDirectDrawSurface *g_pPortPrimary;
static HWND g_hwndPortTarget;
static int g_nPortWidth;
static int g_nPortHeight;

// RGB565. 555 would work equally well as far as the engine is concerned --
// Ddraw_Init reads the surface's own dwRBitMask back and picks the 0x22b/555 or
// 0x235/565 tag from it -- but 565 is what every modern host prefers and what
// GDI converts most directly.
#define PORT_R_MASK 0xF800
#define PORT_G_MASK 0x07E0
#define PORT_B_MASK 0x001F

void Port_ForceRgb565(DDSURFACEDESC *pDesc)
{
    pDesc->dwFlags |= DDSD_PIXELFORMAT;
    memset(&pDesc->ddpfPixelFormat, 0, sizeof(pDesc->ddpfPixelFormat));
    pDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    pDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
    pDesc->ddpfPixelFormat.dwRGBBitCount = 16;
    pDesc->ddpfPixelFormat.dwRBitMask = PORT_R_MASK;
    pDesc->ddpfPixelFormat.dwGBitMask = PORT_G_MASK;
    pDesc->ddpfPixelFormat.dwBBitMask = PORT_B_MASK;
}

IDirectDrawSurface *Port_CreateEmulatedPrimary(IDirectDraw2 *pDDraw, HWND hwnd,
                                               int nWidth, int nHeight)
{
    DDSurfaceDescPadded0x7c scratch;
    IDirectDrawSurface *pSurface = NULL;

    memset(&scratch, 0, sizeof(scratch));
    scratch.ddsd.dwSize = sizeof(scratch);
    scratch.ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    scratch.ddsd.dwWidth = nWidth;
    scratch.ddsd.dwHeight = nHeight;
    scratch.ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    Port_ForceRgb565(&scratch.ddsd);

    if (pDDraw->CreateSurface(&scratch.ddsd, &pSurface, NULL) != DD_OK)
        return NULL;

    g_pPortPrimary = pSurface;
    g_hwndPortTarget = hwnd;
    g_nPortWidth = nWidth;
    g_nPortHeight = nHeight;

    // No window repositioning here any more. Earlier versions tried to shove the main window
    // back to screen (0,0) so its client origin matched the primary's, which is what the
    // engine assumes; winemac fights that (it snaps the engine's WS_MAXIMIZE style into the
    // work area) and winning only bought a borderless window with no titlebar to grab.
    // port/PortWinShim.cpp redefines the screen instead -- "screen coordinates" now mean this
    // window's CLIENT coordinates, wherever the user has dragged it to -- so the origin is
    // correct by construction and there is nothing to correct here.
    return pSurface;
}

// --- Frame capture ------------------------------------------------------------
// Read a decimal environment variable once and cache it. Returns nDefault when
// unset or unparsable.
static int Port_EnvInt(const char *pszName, int nDefault, int *pnCache)
{
    if (*pnCache < 0) {
        const char *psz = getenv(pszName);
        *pnCache = nDefault;
        if (psz != NULL && *psz >= '0' && *psz <= '9') {
            int n = 0;
            while (*psz >= '0' && *psz <= '9')
                n = n * 10 + (*psz++ - '0');
            *pnCache = n;
        }
    }
    return *pnCache;
}

static void Port_PutDword(unsigned char *p, unsigned int dw)
{
    p[0] = (unsigned char)(dw);
    p[1] = (unsigned char)(dw >> 8);
    p[2] = (unsigned char)(dw >> 16);
    p[3] = (unsigned char)(dw >> 24);
}

// 565 -> 24bpp BMP. The header is written byte-by-byte rather than as a
// BITMAPFILEHEADER struct because that one is 14 bytes only under the SDK's own
// #pragma pack(2); spelling the 54 bytes out removes the question entirely.
static int Port_WriteBmp565(const char *pszPath, const unsigned char *pPixels,
                            long lPitch, int nWidth, int nHeight)
{
    unsigned char abHdr[54];
    unsigned char *pRow;
    HANDLE hFile;
    DWORD dwRowBytes = (DWORD)((nWidth * 3 + 3) & ~3); // BMP rows are 4-byte aligned
    DWORD dwImageBytes = dwRowBytes * (DWORD)nHeight;
    DWORD dwWritten;
    int x, y;

    hFile = CreateFileA(pszPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;
    pRow = (unsigned char *)malloc(dwRowBytes);
    if (pRow == NULL) {
        CloseHandle(hFile);
        return 0;
    }

    memset(abHdr, 0, sizeof(abHdr));
    abHdr[0] = 'B';
    abHdr[1] = 'M';
    Port_PutDword(abHdr + 2, 54 + dwImageBytes); // bfSize
    Port_PutDword(abHdr + 10, 54);               // bfOffBits
    Port_PutDword(abHdr + 14, 40);               // biSize
    Port_PutDword(abHdr + 18, (unsigned int)nWidth);
    Port_PutDword(abHdr + 22, (unsigned int)nHeight); // positive => bottom-up
    abHdr[26] = 1;                                    // biPlanes
    abHdr[28] = 24;                                   // biBitCount
    Port_PutDword(abHdr + 34, dwImageBytes);          // biSizeImage
    WriteFile(hFile, abHdr, sizeof(abHdr), &dwWritten, NULL);

    // The surface is top-down and a BMP is bottom-up, so walk the rows backwards.
    for (y = nHeight - 1; y >= 0; y--) {
        const unsigned short *pSrc = (const unsigned short *)(pPixels + (long)y * lPitch);
        memset(pRow, 0, dwRowBytes);
        for (x = 0; x < nWidth; x++) {
            unsigned int p = pSrc[x];
            unsigned int r = (p >> 11) & 0x1f;
            unsigned int g = (p >> 5) & 0x3f;
            unsigned int b = p & 0x1f;
            // Replicate the high bits into the low ones so full-scale stays full-scale.
            pRow[x * 3 + 0] = (unsigned char)((b << 3) | (b >> 2)); // BMP is BGR
            pRow[x * 3 + 1] = (unsigned char)((g << 2) | (g >> 4));
            pRow[x * 3 + 2] = (unsigned char)((r << 3) | (r >> 2));
        }
        WriteFile(hFile, pRow, dwRowBytes, &dwWritten, NULL);
    }

    free(pRow);
    CloseHandle(hFile);
    return 1;
}

static void Port_FrameStats(const unsigned char *pPixels, long lPitch, int nWidth, int nHeight,
                            unsigned int *pdwSum, unsigned int *pdwNonBlack)
{
    unsigned int dwSum = 0;
    unsigned int dwNonBlack = 0;
    int x, y;

    for (y = 0; y < nHeight; y++) {
        const unsigned short *pSrc = (const unsigned short *)(pPixels + (long)y * lPitch);
        for (x = 0; x < nWidth; x++) {
            unsigned int p = pSrc[x];
            dwSum = (dwSum << 1 | dwSum >> 31) + p; // rotate-and-add: order-sensitive, cheap
            if (p != 0)
                dwNonBlack++;
        }
    }
    *pdwSum = dwSum;
    *pdwNonBlack = dwNonBlack;
}

extern "C" int Port_DumpFrame(const char *pszPath)
{
    DDSurfaceDescPadded0x7c scratch;
    int nOk;

    if (g_pPortPrimary == NULL)
        return 0;
    memset(&scratch, 0, sizeof(scratch));
    scratch.ddsd.dwSize = sizeof(scratch);
    if (g_pPortPrimary->Lock(NULL, &scratch.ddsd, DDLOCK_WAIT | DDLOCK_READONLY, NULL) != DD_OK)
        return 0;
    nOk = Port_WriteBmp565(pszPath, (const unsigned char *)scratch.ddsd.lpSurface,
                           scratch.ddsd.lPitch, g_nPortWidth, g_nPortHeight);
    g_pPortPrimary->Unlock(NULL);
    return nOk;
}

// Blit the whole emulated primary into one window, aligned so that the pixel the
// engine drew at screen position P lands at the same place on screen.
//
// ⚠ The game is NOT one window. The front end is its own top-level window
// (FRONTWINDOWCLASS) sitting over the main one, and the in-game UI adds more --
// so a present that only ever targets g_pApp->hwndOwner draws a perfectly correct
// frame UNDERNEATH whatever is actually on top. That is what a white menu with a
// live text box in it was: not a missing frame, a covered one.
//
// The emulated primary stands in for the whole SCREEN, exactly as the real primary
// did, so each window just shows its own portion of it. That is the whole trick:
// offset the DESTINATION by minus the window's client origin (GDI clips the parts
// that fall outside) rather than trying to offset the source rectangle, which for
// a top-down DIB means arguing with StretchDIBits about where row 0 is.
static void Port_BlitTo(HWND hwnd, HDC hdc, void *pBits, long lPitch)
{
    struct {
        BITMAPINFOHEADER hdr;
        DWORD adwMask[3];
    } bmi;
    POINT ptOrigin;

    if (hdc == NULL || hwnd == NULL)
        return;

    // ⚠ The emulated primary is indexed in SCREEN coordinates, not in any window's
    // client coordinates, and that is a correctness requirement rather than a
    // choice. The engine assumes its primary IS the screen: PopupWndBase's software
    // cursor takes a raw GetCursorPos() -- screen coordinates -- and uses it
    // directly as a primary offset, while other sites go through
    // ScreenToClient(hwndOwner). Those two agree only while the main window's
    // client origin sits at screen (0,0), which is what a borderless popup at the
    // origin guaranteed on Windows in 1998 and what winemac does NOT give us.
    //
    // "Screen" here means the PORT's screen: this ClientToScreen call is one of the
    // entry points port/PortWinShim.cpp intercepts, so it returns the window's origin
    // relative to the MAIN window's client area -- (0,0) for the main window itself,
    // and the right offset for each popup. Every consumer of screen coordinates in the
    // engine is translated the same way, so the invariant holds no matter where the
    // user has dragged the frame.
    ptOrigin.x = 0;
    ptOrigin.y = 0;
    ClientToScreen(hwnd, &ptOrigin);

    memset(&bmi, 0, sizeof(bmi));
    bmi.hdr.biSize = sizeof(BITMAPINFOHEADER);
    bmi.hdr.biWidth = lPitch / 2;
    bmi.hdr.biHeight = -g_nPortHeight;
    bmi.hdr.biPlanes = 1;
    bmi.hdr.biBitCount = 16;
    bmi.hdr.biCompression = BI_BITFIELDS;
    bmi.adwMask[0] = PORT_R_MASK;
    bmi.adwMask[1] = PORT_G_MASK;
    bmi.adwMask[2] = PORT_B_MASK;

    StretchDIBits(hdc, -ptOrigin.x, -ptOrigin.y,
                  g_nPortWidth, g_nPortHeight,
                  0, 0, g_nPortWidth, g_nPortHeight,
                  pBits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, SRCCOPY);
}

void Port_PresentToDC(HWND hwnd, HDC hdc)
{
    DDSurfaceDescPadded0x7c scratch;

    if (g_pPortPrimary == NULL || hdc == NULL)
        return;
    memset(&scratch, 0, sizeof(scratch));
    scratch.ddsd.dwSize = sizeof(scratch);
    if (g_pPortPrimary->Lock(NULL, &scratch.ddsd, DDLOCK_WAIT | DDLOCK_READONLY, NULL) != DD_OK)
        return;
    Port_BlitTo(hwnd, hdc, scratch.ddsd.lpSurface, scratch.ddsd.lPitch);
    g_pPortPrimary->Unlock(NULL);
}

// EnumThreadWindows collector: every visible top-level window the game owns.
#define PORT_MAX_WINDOWS 32
static HWND g_ahwndPortTargets[PORT_MAX_WINDOWS];
static int g_nPortTargets;

static BOOL CALLBACK Port_CollectWindow(HWND hwnd, LPARAM lParam)
{
    (void)lParam;
    if (IsWindowVisible(hwnd) && g_nPortTargets < PORT_MAX_WINDOWS)
        g_ahwndPortTargets[g_nPortTargets++] = hwnd;
    return TRUE;
}

void Port_Present(void)
{
    DDSurfaceDescPadded0x7c scratch;
    HDC hdc;

    if (g_pPortPrimary == NULL || g_hwndPortTarget == NULL)
        return;

    // Before the blit, not after: a popup the window manager has nudged would otherwise be
    // presented one frame out of place, which is exactly the front-end paint offset this is
    // here to remove.
    Port_ShimReassertWindows();

    memset(&scratch, 0, sizeof(scratch));
    scratch.ddsd.dwSize = sizeof(scratch);
    if (g_pPortPrimary->Lock(NULL, &scratch.ddsd, DDLOCK_WAIT | DDLOCK_READONLY, NULL) != DD_OK)
        return;

    // Present to EVERY visible top-level window the game owns, not just the main
    // one -- see Port_BlitTo. Each gets the portion of the emulated screen it sits
    // over, so whichever is on top shows the right pixels.
    {
        int i;

        g_nPortTargets = 0;
        // Cast: this SDK's WNDENUMPROC prototype does not match the callback shape
        // the documentation gives, and VC5 will not convert it implicitly.
        EnumThreadWindows(GetCurrentThreadId(), (WNDENUMPROC)Port_CollectWindow, 0);
        // Fall back to the main window if the enumeration turned nothing up.
        if (g_nPortTargets == 0 && g_hwndPortTarget != NULL)
            g_ahwndPortTargets[g_nPortTargets++] = g_hwndPortTarget;

        for (i = 0; i < g_nPortTargets; i++) {
            hdc = GetDC(g_ahwndPortTargets[i]);
            if (hdc != NULL) {
                Port_BlitTo(g_ahwndPortTargets[i], hdc, scratch.ddsd.lpSurface,
                            scratch.ddsd.lPitch);
                ReleaseDC(g_ahwndPortTargets[i], hdc);
            }
        }
    }

    // Capture rides along on the lock the present already holds. The frame counter
    // itself is the first thing worth reading in the log: if it never advances,
    // nothing is presenting at all (FrameDriver_TickMaybe only runs off the
    // multimedia-timer latch), which is a completely different bug from a present
    // that runs and finds an empty buffer.
    {
        static int nStatCache = -1;
        static int nDumpCache = -1;
        static unsigned int dwFrame = 0;
        int nStatEvery = Port_EnvInt("LOCO_PORT_STAT", 60, &nStatCache);
        int nDumpEvery = Port_EnvInt("LOCO_PORT_DUMP", 0, &nDumpCache);

        dwFrame++;
        if (nStatEvery > 0 && (dwFrame % (unsigned int)nStatEvery) == 0) {
            unsigned int dwSum = 0, dwNonBlack = 0;
            Port_FrameStats((const unsigned char *)scratch.ddsd.lpSurface, scratch.ddsd.lPitch,
                            g_nPortWidth, g_nPortHeight, &dwSum, &dwNonBlack);
            Port_Tracef("present %u %dx%d pitch=%ld sum=%08x nonblack=%u/%u\n", dwFrame,
                        g_nPortWidth, g_nPortHeight, (long)scratch.ddsd.lPitch, dwSum, dwNonBlack,
                        (unsigned int)(g_nPortWidth * g_nPortHeight));

            // Who is actually ON TOP of the middle of the screen? A correct frame
            // blitted to a window that something else covers looks exactly like a
            // frame that was never drawn, and only this tells the two apart.
            {
                POINT pt;
                HWND hwndTop, hwndFg;
                RECT rcTarget;
                char szClass[64];

                pt.x = g_nPortWidth / 2;
                pt.y = g_nPortHeight / 2;
                hwndTop = WindowFromPoint(pt);
                hwndFg = GetForegroundWindow();
                szClass[0] = 0;
                if (hwndTop != NULL)
                    GetClassNameA(hwndTop, szClass, sizeof(szClass));
                memset(&rcTarget, 0, sizeof(rcTarget));
                GetWindowRect(g_hwndPortTarget, &rcTarget);
                Port_Tracef("  wnd target=%p vis=%d rect=%ld,%ld,%ld,%ld top=%p(%s) fg=%p\n",
                            (void *)g_hwndPortTarget, (int)IsWindowVisible(g_hwndPortTarget),
                            (long)rcTarget.left, (long)rcTarget.top, (long)rcTarget.right,
                            (long)rcTarget.bottom, (void *)hwndTop, szClass, (void *)hwndFg);

                // Cursor coordinates, the thing the software cursor is drawn from.
                // ptCur is what GetCursorPos gives the engine (screen space); ptCli
                // is the main window's client origin, which MUST be 0,0 for the
                // engine's mixed screen/client maths to agree with itself. If the
                // anchor in Port_CreateEmulatedPrimary was refused, it shows here.
                {
                    POINT ptCur;
                    POINT ptCli;
                    RECT rcClip;

                    ptCur.x = 0;
                    ptCur.y = 0;
                    GetCursorPos(&ptCur);
                    ptCli.x = 0;
                    ptCli.y = 0;
                    ClientToScreen(g_hwndPortTarget, &ptCli);
                    memset(&rcClip, 0, sizeof(rcClip));
                    GetClipCursor(&rcClip);
                    Port_Tracef("  t=%lu cur screen=%ld,%ld clientorigin=%ld,%ld "
                                "vscreen=%dx%d clip=%ld,%ld,%ld,%ld cap=%p\n",
                                (unsigned long)GetTickCount(), (long)ptCur.x, (long)ptCur.y,
                                (long)ptCli.x, (long)ptCli.y, (int)GetSystemMetrics(SM_CXSCREEN),
                                (int)GetSystemMetrics(SM_CYSCREEN), (long)rcClip.left,
                                (long)rcClip.top, (long)rcClip.right, (long)rcClip.bottom,
                                (void *)GetCapture());
                }
            }
        }
        if (nDumpEvery > 0 && (dwFrame % (unsigned int)nDumpEvery) == 0) {
            char szPath[64];
            sprintf(szPath, "port_frame%04u.bmp", dwFrame);
            Port_WriteBmp565(szPath, (const unsigned char *)scratch.ddsd.lpSurface,
                             scratch.ddsd.lPitch, g_nPortWidth, g_nPortHeight);
        }
    }

    g_pPortPrimary->Unlock(NULL);

    // After the Unlock, deliberately: the script fires on the same counter the stats above
    // print, so a trace reads in order, and nothing posted here can run while a lock is held.
    Port_AutoInput();
}

// --- Synthetic input ----------------------------------------------------------
#define PORT_MAX_HITRECTS 16
#define PORT_MAX_CLICKS 16

static struct {
    char szName[24];
    HWND hwnd;
    RECT rc;
} g_aPortHitRects[PORT_MAX_HITRECTS];
static int g_nPortHitRects;

void Port_RegisterHitRect(const char *pszName, HWND hwnd, const RECT *pRect)
{
    int i;

    // Re-registration is the normal case, not an error: RefreshClientClipRect re-derives the
    // whole layout on every clip-rect change, so update in place and keep the name unique.
    for (i = 0; i < g_nPortHitRects; i++) {
        if (strcmp(g_aPortHitRects[i].szName, pszName) == 0)
            break;
    }
    if (i == PORT_MAX_HITRECTS)
        return;
    if (i == g_nPortHitRects) {
        g_nPortHitRects++;
        strncpy(g_aPortHitRects[i].szName, pszName, sizeof(g_aPortHitRects[i].szName) - 1);
        g_aPortHitRects[i].szName[sizeof(g_aPortHitRects[i].szName) - 1] = 0;
    }
    g_aPortHitRects[i].hwnd = hwnd;
    g_aPortHitRects[i].rc = *pRect;
    Port_Tracef("hitrect %-8s hwnd=%p %ld,%ld,%ld,%ld centre=%ld,%ld\n", pszName, (void *)hwnd,
                (long)pRect->left, (long)pRect->top, (long)pRect->right, (long)pRect->bottom,
                (long)((pRect->left + pRect->right) / 2), (long)((pRect->top + pRect->bottom) / 2));
}

// Resolve a script target to a window and a client-space point. A name wins over coordinates,
// so a rect called "42,7" would shadow the literal -- no caller names one that.
static int Port_ResolveTarget(const char *pszTarget, HWND *phwnd, int *pnX, int *pnY)
{
    int i, nX, nY;

    for (i = 0; i < g_nPortHitRects; i++) {
        if (strcmp(g_aPortHitRects[i].szName, pszTarget) == 0) {
            if (g_aPortHitRects[i].hwnd == NULL)
                return 0;
            *phwnd = g_aPortHitRects[i].hwnd;
            *pnX = (g_aPortHitRects[i].rc.left + g_aPortHitRects[i].rc.right) / 2;
            *pnY = (g_aPortHitRects[i].rc.top + g_aPortHitRects[i].rc.bottom) / 2;
            return 1;
        }
    }
    if (sscanf(pszTarget, "%d,%d", &nX, &nY) == 2 && g_hwndPortTarget != NULL) {
        *phwnd = g_hwndPortTarget;
        *pnX = nX;
        *pnY = nY;
        return 1;
    }
    return 0;
}

// PostMessage rather than SendMessage: OnLButtonDown's two button arms Sleep(150) and the Esc
// arm SPINS on DSoundChannel::IsReclaimable, so dispatching them from inside the present would
// stall the frame that is still holding a DirectDraw lock a moment ago. Posting hands the work
// to the window's own message pump, which is where a real click would arrive anyway.
static void Port_PostClick(HWND hwnd, int nX, int nY, const char *pszWhat)
{
    LPARAM lParam = (LPARAM)(((nY & 0xffff) << 16) | (nX & 0xffff));

    Port_Tracef("autoclick %s hwnd=%p at %d,%d\n", pszWhat, (void *)hwnd, nX, nY);

    // Warp the REAL pointer onto the target first, and not as a nicety: the engine cross-checks
    // the mouse against the host every frame. PlacementCursorMaybe::OnMouseMoveMaybe bounds-tests
    // the last WM_MOUSEMOVE position against g_rectAppClientBounds and asks WindowFromPoint who
    // owns it, and anything it does not like ends in SetCursorCapture(0,1,0) -- which clears
    // bReady, and AdvanceAnimFrameMaybe returns immediately when bReady is false, so the pending
    // click is never committed. With the physical pointer parked outside the window that disarm
    // fires on every tick, and a scripted click lands on a cursor that is switched off. Posting
    // WM_MOUSEMOVE alone does NOT fix it: the guard reads the position, but WindowFromPoint reads
    // the host.
    //
    // SetCursorPos here is the shim's, so it takes VIRTUAL coordinates -- the same space the
    // script and the posted lParam are already in.
    SetCursorPos(nX, nY);
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, lParam);
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, lParam);
}

void Port_AutoInput(void)
{
    static int bParsed = 0;
    static struct {
        unsigned int dwFrame;
        char szTarget[32];
        int bFired;
    } aClicks[PORT_MAX_CLICKS];
    static int nClicks;
    static unsigned int dwTick;
    int i;

    if (!bParsed) {
        const char *psz = getenv("LOCO_PORT_CLICK");

        bParsed = 1;
        while (psz != NULL && *psz != 0 && nClicks < PORT_MAX_CLICKS) {
            unsigned int dwFrame = 0;
            int nLen = 0;

            while (*psz == ';' || *psz == ' ')
                psz++;
            if (*psz < '0' || *psz > '9')
                break;
            while (*psz >= '0' && *psz <= '9')
                dwFrame = dwFrame * 10 + (unsigned int)(*psz++ - '0');
            if (*psz != ':')
                break;
            psz++;
            while (*psz != 0 && *psz != ';' && nLen < (int)sizeof(aClicks[0].szTarget) - 1)
                aClicks[nClicks].szTarget[nLen++] = *psz++;
            aClicks[nClicks].szTarget[nLen] = 0;
            aClicks[nClicks].dwFrame = dwFrame;
            Port_Tracef("autoclick script #%d frame=%u target=%s\n", nClicks, dwFrame,
                        aClicks[nClicks].szTarget);
            nClicks++;
            while (*psz != 0 && *psz != ';')
                psz++;
        }
    }
    if (nClicks == 0)
        return;

    dwTick++;
    for (i = 0; i < nClicks; i++) {
        HWND hwnd;
        int nX, nY;

        if (aClicks[i].bFired || dwTick < aClicks[i].dwFrame)
            continue;
        if (!Port_ResolveTarget(aClicks[i].szTarget, &hwnd, &nX, &nY)) {
            // Not a miss to fire-and-forget: a rect registered later must still be clickable,
            // so leave the entry armed and retry on the next present.
            Port_Tracef("autoclick #%d target=%s UNRESOLVED at frame %u\n", i, aClicks[i].szTarget,
                        dwTick);
            continue;
        }
        aClicks[i].bFired = 1;
        Port_PostClick(hwnd, nX, nY, aClicks[i].szTarget);
    }
}

// --- Diagnostics --------------------------------------------------------------
extern "C" void Port_Tracef(const char *pszFmt, ...)
{
    static HANDLE hLog = INVALID_HANDLE_VALUE;
    char szLine[1024];
    va_list ap;
    int n;
    DWORD written = 0;

    if (hLog == INVALID_HANDLE_VALUE) {
        hLog = CreateFileA("port_trace.log", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hLog == INVALID_HANDLE_VALUE)
            return;
    }
    va_start(ap, pszFmt);
    n = vsprintf(szLine, pszFmt, ap);
    va_end(ap);
    if (n <= 0)
        return;
    WriteFile(hLog, szLine, (DWORD)n, &written, NULL);
    FlushFileBuffers(hLog);
}

void Port_Shutdown(void)
{
    if (g_pPortPrimary != NULL) {
        g_pPortPrimary->Release();
        g_pPortPrimary = NULL;
    }
    g_hwndPortTarget = NULL;
}
