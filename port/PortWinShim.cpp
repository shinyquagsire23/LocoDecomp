// PortWinShim -- the port's VIRTUAL SCREEN, and the titlebar window it buys.
//
// ── The problem ───────────────────────────────────────────────────────────────
// The engine assumes its DirectDraw primary IS the screen, and that the main window's
// client area covers it exactly: PopupWndBase's software cursor feeds a raw GetCursorPos()
// straight in as a primary offset, every popup is a separate top-level window placed in
// screen coordinates, and Port_BlitTo hands each of those windows the slice of the primary
// it happens to sit over. In 1998 a borderless WS_POPUP at (0,0) made all of that true by
// construction.
//
// Under winemac it is not. The engine's fullscreen style carries WS_MAXIMIZE, and winemac
// snaps a maximized window into the WORK AREA -- below the macOS menu bar -- so the client
// origin lands ~78 px down and every frame is drawn displaced and clipped. Forcing the
// window back to screen (0,0) fixes the picture but leaves it borderless: no titlebar to
// grab, and the top of it under the menu bar.
//
// ── The fix ───────────────────────────────────────────────────────────────────
// Stop trying to make the window sit at the screen origin, and redefine the origin instead.
// In the port, "screen coordinates" mean THE MAIN WINDOW'S CLIENT COORDINATES -- a virtual
// screen that travels with the window. The engine then keeps every assumption it already
// has, and the window can be an ordinary titled, movable frame.
//
// The translation lives entirely at the OS boundary, in this file: each Win32 entry point
// that crosses between screen space and anything else is DEFINED here, converts, and calls
// through to the real user32 export it looked up by name. Two consequences worth knowing:
//
//   * tools/build_port.sh compiles the port with /D _USER32_, which makes WINUSER.H drop the
//     __declspec(dllimport) decoration. Calls then compile to `call _GetCursorPos@4` rather
//     than `call [__imp__GetCursorPos@4]`, and the linker resolves that from THIS OBJECT
//     (objects are searched before import libraries), so every call site in every TU is
//     routed here without a single #ifdef in src/.
//   * Nothing in src/ changes, so the byte-match build is untouched by construction -- it
//     never compiles this file and never defines _USER32_.
//
// The main window itself is special-cased: it is created with a caption instead of the
// engine's borderless popup, and the engine's own attempts to move, resize or re-style it
// (AppWindow_ApplyDisplayModeMaybe does all three) are ignored so the user stays in charge
// of where it sits. Its client size is still exactly what the engine asked for, so
// GetClientRect -- which needs no translation and is deliberately NOT intercepted -- keeps
// reporting the primary's dimensions.
#include <windows.h>
#include <string.h>

#include "PortMode.h"

// The engine's main window. Everything here is relative to its client area; until it exists
// the virtual screen and the real screen are the same thing.
static HWND g_hwndPortShimMain;

// The style the port gives the main window, replacing WS_POPUP (and never WS_MAXIMIZE --
// that is the bit winemac reacts to). Kept in one place because SetWindowLongA has to be
// able to re-assert it whenever the engine re-styles the window.
#define PORT_MAIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | \
                         WS_CLIPCHILDREN | WS_VISIBLE)
#define PORT_MAIN_STYLE_STRIP (WS_POPUP | WS_MAXIMIZE | WS_THICKFRAME | WS_MAXIMIZEBOX)

// ── Real user32 entry points ──────────────────────────────────────────────────
// Resolved by name rather than called directly: a direct call would bind to the definition
// in this very file and recurse. GetModuleHandleA is used, not LoadLibrary -- user32 is
// already in the process (the import table pulls it in) and this can run before WinMain.
typedef BOOL (WINAPI *PFN_GETCURSORPOS)(LPPOINT);
typedef BOOL (WINAPI *PFN_SETCURSORPOS)(int, int);
typedef BOOL (WINAPI *PFN_CLIENTTOSCREEN)(HWND, LPPOINT);
typedef BOOL (WINAPI *PFN_SCREENTOCLIENT)(HWND, LPPOINT);
typedef HWND (WINAPI *PFN_WINDOWFROMPOINT)(POINT);
typedef BOOL (WINAPI *PFN_GETWINDOWRECT)(HWND, LPRECT);
typedef BOOL (WINAPI *PFN_SETWINDOWPOS)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL (WINAPI *PFN_MOVEWINDOW)(HWND, int, int, int, int, BOOL);
typedef HWND (WINAPI *PFN_CREATEWINDOWEXA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int,
                                           HWND, HMENU, HINSTANCE, LPVOID);
typedef LONG (WINAPI *PFN_SETWINDOWLONGA)(HWND, int, LONG);

static FARPROC Port_User32(const char *pszName)
{
    static HMODULE hUser32;

    if (hUser32 == NULL) {
        hUser32 = GetModuleHandleA("user32.dll");
    }
    if (hUser32 == NULL) {
        return NULL;
    }
    return GetProcAddress(hUser32, pszName);
}

// ── The virtual origin ────────────────────────────────────────────────────────
// Where the main window's client (0,0) currently is, in real screen coordinates. Recomputed
// on every call rather than cached: the user can drag the window at any moment, and a stale
// origin would show up as exactly the cursor/paint drift this file exists to remove.
static void Port_VirtualOrigin(POINT *pOrigin)
{
    static PFN_CLIENTTOSCREEN pfn;

    pOrigin->x = 0;
    pOrigin->y = 0;
    if (g_hwndPortShimMain == NULL) {
        return;
    }
    if (pfn == NULL) {
        pfn = (PFN_CLIENTTOSCREEN)Port_User32("ClientToScreen");
        if (pfn == NULL) {
            return;
        }
    }
    pfn(g_hwndPortShimMain, pOrigin);
}

void Port_ScreenToVirtual(POINT *pt)
{
    POINT ptOrigin;

    Port_VirtualOrigin(&ptOrigin);
    pt->x -= ptOrigin.x;
    pt->y -= ptOrigin.y;
}

void Port_VirtualToScreen(POINT *pt)
{
    POINT ptOrigin;

    Port_VirtualOrigin(&ptOrigin);
    pt->x += ptOrigin.x;
    pt->y += ptOrigin.y;
}

// ── Popup placement ───────────────────────────────────────────────────────────
// Every front-end screen is a borderless WS_POPUP the size of the whole virtual screen, and
// Ddraw_BltUpdateRect paints into the primary at the popup's OWN client origin -- so a popup
// that is not exactly where the engine put it drags the whole front end with it.
//
// They do not stay put. Created at the right place and never moved by SetWindowPos, they still
// come back displaced (measured: asked for real 3,120, found at 4,106) -- winemac nudges a
// screen-sized window when it is first shown, and that is invisible to every intercept here.
// So remember where each one BELONGS in virtual coordinates and put it back; the main window
// is excluded, since it is the one thing the user is meant to be able to drag.
#define PORT_SHIM_MAX_WINDOWS 32

static struct {
    HWND hwnd;
    int x;
    int y;
} g_aShimWindows[PORT_SHIM_MAX_WINDOWS];
static int g_nShimWindows;

static void Port_ShimRemember(HWND hwnd, int x, int y)
{
    int i;

    for (i = 0; i < g_nShimWindows; i++) {
        if (g_aShimWindows[i].hwnd == hwnd) {
            g_aShimWindows[i].x = x;
            g_aShimWindows[i].y = y;
            return;
        }
    }
    if (g_nShimWindows < PORT_SHIM_MAX_WINDOWS) {
        g_aShimWindows[g_nShimWindows].hwnd = hwnd;
        g_aShimWindows[g_nShimWindows].x = x;
        g_aShimWindows[g_nShimWindows].y = y;
        g_nShimWindows++;
    }
}

void Port_ShimReassertWindows(void)
{
    static PFN_GETWINDOWRECT pfnGetRect;
    static PFN_SETWINDOWPOS pfnSetPos;
    static unsigned int nTraced = 0;
    POINT ptOrigin;
    int i;

    if (g_nShimWindows == 0) {
        return;
    }
    if (pfnGetRect == NULL) {
        pfnGetRect = (PFN_GETWINDOWRECT)Port_User32("GetWindowRect");
        pfnSetPos = (PFN_SETWINDOWPOS)Port_User32("SetWindowPos");
        if (pfnGetRect == NULL || pfnSetPos == NULL) {
            return;
        }
    }
    Port_VirtualOrigin(&ptOrigin);

    for (i = 0; i < g_nShimWindows; i++) {
        HWND hwnd = g_aShimWindows[i].hwnd;
        int nWantX = g_aShimWindows[i].x + ptOrigin.x;
        int nWantY = g_aShimWindows[i].y + ptOrigin.y;
        RECT rcActual;

        if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
            continue;
        }
        if (!pfnGetRect(hwnd, &rcActual)) {
            continue;
        }
        if (rcActual.left == nWantX && rcActual.top == nWantY) {
            continue;
        }
        pfnSetPos(hwnd, NULL, nWantX, nWantY, 0, 0,
                  SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (++nTraced <= 8) {
            Port_Tracef("shim: replaced %p from %ld,%ld to %d,%d\n", (void *)hwnd,
                        (long)rcActual.left, (long)rcActual.top, nWantX, nWantY);
        }
    }
}

// ── The intercepts ────────────────────────────────────────────────────────────
extern "C" {

BOOL WINAPI GetCursorPos(LPPOINT lpPoint)
{
    static PFN_GETCURSORPOS pfn;

    if (pfn == NULL) {
        pfn = (PFN_GETCURSORPOS)Port_User32("GetCursorPos");
    }
    if (pfn == NULL || !pfn(lpPoint)) {
        return FALSE;
    }
    Port_ScreenToVirtual(lpPoint);
    return TRUE;
}

BOOL WINAPI SetCursorPos(int X, int Y)
{
    static PFN_SETCURSORPOS pfn;
    POINT pt;

    if (pfn == NULL) {
        pfn = (PFN_SETCURSORPOS)Port_User32("SetCursorPos");
        if (pfn == NULL) {
            return FALSE;
        }
    }
    pt.x = X;
    pt.y = Y;
    Port_VirtualToScreen(&pt);
    return pfn(pt.x, pt.y);
}

BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    static PFN_CLIENTTOSCREEN pfn;

    if (pfn == NULL) {
        pfn = (PFN_CLIENTTOSCREEN)Port_User32("ClientToScreen");
    }
    if (pfn == NULL || !pfn(hWnd, lpPoint)) {
        return FALSE;
    }
    Port_ScreenToVirtual(lpPoint);
    return TRUE;
}

BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    static PFN_SCREENTOCLIENT pfn;

    if (pfn == NULL) {
        pfn = (PFN_SCREENTOCLIENT)Port_User32("ScreenToClient");
        if (pfn == NULL) {
            return FALSE;
        }
    }
    Port_VirtualToScreen(lpPoint);
    return pfn(hWnd, lpPoint);
}

HWND WINAPI WindowFromPoint(POINT Point)
{
    static PFN_WINDOWFROMPOINT pfn;

    if (pfn == NULL) {
        pfn = (PFN_WINDOWFROMPOINT)Port_User32("WindowFromPoint");
        if (pfn == NULL) {
            return NULL;
        }
    }
    Port_VirtualToScreen(&Point);
    return pfn(Point);
}

BOOL WINAPI GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    static PFN_GETWINDOWRECT pfn;
    POINT ptOrigin;

    if (pfn == NULL) {
        pfn = (PFN_GETWINDOWRECT)Port_User32("GetWindowRect");
    }
    if (pfn == NULL || !pfn(hWnd, lpRect)) {
        return FALSE;
    }
    Port_VirtualOrigin(&ptOrigin);
    lpRect->left -= ptOrigin.x;
    lpRect->right -= ptOrigin.x;
    lpRect->top -= ptOrigin.y;
    lpRect->bottom -= ptOrigin.y;
    return TRUE;
}

// The main window is exempt from engine-driven moves and resizes: it is the thing that
// DEFINES the virtual screen, so honouring `SetWindowPos(hwnd, .., 0, 0, w, h, ..)` would
// mean "move the window to wherever it already is" at best, and undo the user's own drag at
// worst. AppWindow_ApplyDisplayModeMaybe issues exactly that on every display-mode switch.
BOOL WINAPI SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy,
                         UINT uFlags)
{
    static PFN_SETWINDOWPOS pfn;
    POINT pt;

    if (pfn == NULL) {
        pfn = (PFN_SETWINDOWPOS)Port_User32("SetWindowPos");
        if (pfn == NULL) {
            return FALSE;
        }
    }
    if (hWnd != NULL && hWnd == g_hwndPortShimMain) {
        return pfn(hWnd, hWndInsertAfter, 0, 0, 0, 0,
                   uFlags | SWP_NOMOVE | SWP_NOSIZE);
    }
    pt.x = X;
    pt.y = Y;
    Port_VirtualToScreen(&pt);
    if ((uFlags & SWP_NOMOVE) == 0) {
        Port_ShimRemember(hWnd, X, Y);
    }
    return pfn(hWnd, hWndInsertAfter, pt.x, pt.y, cx, cy, uFlags);
}

BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    static PFN_MOVEWINDOW pfn;
    POINT pt;

    if (pfn == NULL) {
        pfn = (PFN_MOVEWINDOW)Port_User32("MoveWindow");
        if (pfn == NULL) {
            return FALSE;
        }
    }
    if (hWnd != NULL && hWnd == g_hwndPortShimMain) {
        return TRUE;
    }
    pt.x = X;
    pt.y = Y;
    Port_VirtualToScreen(&pt);
    Port_ShimRemember(hWnd, X, Y);
    return pfn(hWnd, pt.x, pt.y, nWidth, nHeight, bRepaint);
}

// The main window is recognised by its class name and given a real frame. Its CLIENT area
// keeps the size the engine asked for -- that size is the primary's, and GetClientRect
// (untranslated, and deliberately not intercepted) has to keep agreeing with it.
HWND WINAPI CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                            DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                            HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    static PFN_CREATEWINDOWEXA pfn;
    int bIsMain;
    int nVirtualX = X;
    int nVirtualY = Y;
    HWND hwnd;

    if (pfn == NULL) {
        pfn = (PFN_CREATEWINDOWEXA)Port_User32("CreateWindowExA");
        if (pfn == NULL) {
            return NULL;
        }
    }

    // HIWORD == 0 means the "name" is a registered atom, not a string.
    bIsMain = (g_hwndPortShimMain == NULL && HIWORD((DWORD)lpClassName) != 0 &&
               strcmp(lpClassName, "LEGO LOCO") == 0);

    if (bIsMain) {
        RECT rcFrame;
        RECT rcWork;

        dwStyle = (dwStyle & ~PORT_MAIN_STYLE_STRIP) | PORT_MAIN_STYLE;
        rcFrame.left = 0;
        rcFrame.top = 0;
        rcFrame.right = nWidth;
        rcFrame.bottom = nHeight;
        AdjustWindowRect(&rcFrame, dwStyle, FALSE);
        nWidth = rcFrame.right - rcFrame.left;
        nHeight = rcFrame.bottom - rcFrame.top;

        // Anchor the FRAME at the work area's top-left so the caption is reachable. On a
        // display only as tall as the game the bottom will hang off; that is the price of a
        // titlebar at 1:1, and the window is movable now, which is the point.
        if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &rcWork, 0)) {
            rcWork.left = 0;
            rcWork.top = 0;
        }
        X = rcWork.left;
        Y = rcWork.top;
    } else if (X != CW_USEDEFAULT && (dwStyle & WS_CHILD) == 0) {
        POINT pt;

        nVirtualX = X;
        nVirtualY = Y;
        pt.x = X;
        pt.y = Y;
        Port_VirtualToScreen(&pt);
        X = pt.x;
        Y = pt.y;
    }

    hwnd = pfn(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight,
               hWndParent, hMenu, hInstance, lpParam);

    // Record where this popup belongs so Port_ShimReassertWindows can put it back after the
    // window manager has had its say. Doing it once here is not enough -- the displacement
    // happens when the window is first SHOWN, which is later and goes through nothing we see.
    if (!bIsMain && hwnd != NULL && (dwStyle & WS_CHILD) == 0 && X != CW_USEDEFAULT) {
        Port_ShimRemember(hwnd, nVirtualX, nVirtualY);
    }

    if (bIsMain && hwnd != NULL) {
        RECT rcClient;
        POINT ptOrigin;

        g_hwndPortShimMain = hwnd;
        GetClientRect(hwnd, &rcClient);
        Port_VirtualOrigin(&ptOrigin);
        Port_Tracef("shim: main window %p style=%08lx client=%ldx%ld origin=%ld,%ld\n",
                    (void *)hwnd, (unsigned long)dwStyle, rcClient.right - rcClient.left,
                    rcClient.bottom - rcClient.top, (long)ptOrigin.x, (long)ptOrigin.y);
    }
    return hwnd;
}

// AppWindow_ApplyDisplayModeMaybe re-styles the main window on every display-mode switch --
// including back to WS_POPUP | WS_MAXIMIZE, which is what removes the frame and hands the
// window to winemac's work-area snapping. Re-assert the port's own frame bits every time.
// (This is also what made the correction "not stick" when the user left the world and went
// back to the menu: the menu transition re-applies the style.)
LONG WINAPI SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    static PFN_SETWINDOWLONGA pfn;

    if (pfn == NULL) {
        pfn = (PFN_SETWINDOWLONGA)Port_User32("SetWindowLongA");
        if (pfn == NULL) {
            return 0;
        }
    }
    if (hWnd != NULL && hWnd == g_hwndPortShimMain && nIndex == GWL_STYLE) {
        dwNewLong = (dwNewLong & ~PORT_MAIN_STYLE_STRIP) | PORT_MAIN_STYLE;
    }
    return pfn(hWnd, nIndex, dwNewLong);
}

} // extern "C"
