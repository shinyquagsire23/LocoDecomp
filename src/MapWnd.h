// MapWnd -- the multiplayer LAYOUT/TOWN selection screen (class MAPWINDOWCLASS, RT_STRING id
// 503; ctor 0x430a90, sizeof 0x2c4 = 708 bytes; singleton g_pMapWnd/DAT_004fd388, app state 9).
// A WindowBase-derived full-desktop popup that, like AlbumCardWnd, Create()s through the shared
// FullscreenPopupWndPartial::CreateFullscreenPopupWnd helper (0x402520) rather than a Create of
// its own. See docs/subsystems.md's "MapWnd" entry.
//
// What the screen actually shows: a 3x3 (nProviderSlotsPerRow x nProviderSlotRows) grid of
// LAYOUT tiles, one per DPlaySessionMgr provider slot. Each cell draws that peer's stored layout
// bitmap (its own pLayoutData blob, blitted through a scratch LocoBitmap), and every train that
// peer has filed a placement result for. Below the grid sit nine name plates -- [0] the local
// player's own, [1..8] the other peers -- plus a "refresh the layout list" button
// (pRefreshBtnRef) and an exit button. The whole screen is gated on
// DPlaySessionMgr::connectionMode == 2 ("Layouts" world type): BeginModalCapture bounces
// straight back to app state 3 when it isn't.
//
// A 120 ms WM_TIMER (id 0x4d) drives bBlinkPhaseMaybe, which makes the CURRENTLY-HOVERED slot's
// owner dot pulse between its small and large radius -- that is the only animation on the screen.
//
// Field layout built from InitResourceRefs's (0x430b10) own zero-init writes plus every access in
// this TU's transcribed methods. RefreshClientClipRect (0x430fe0) is the layout oracle: it writes
// every RECT below in one pass, so their extents are pinned, not guessed.
#pragma once

#include <windows.h>

#include "WindowBase.h"

class ResourceRef;
class CursorDesc;
struct LocoBitmap;

class MapWnd : public WindowBase {
public:
    // 0x430a90 -- chains the WindowBase base ctor, installs the vtable, then InitResourceRefs.
    MapWnd(void *hInstanceParam, unsigned int resourceIdParam);

    HICON hIcon; // +0xe8 -- set by the shared CreateFullscreenPopupWnd helper
                       // (src/WindowBase.cpp), not by this class's own ctor
    // +0xec -- the SetTimer id (always 0x4d) for the 120 ms hover-dot blink tick, or 0 while no
    // timer is armed. BeginModalCapture arms it; EndActiveSession kills it and clears this.
    UINT nTimerId;
    // +0xf0 -- the exit/back button's screen rect (drawn from pExitIconBitmap). Clicking it, or
    // pressing RETURN/ESCAPE, flashes the pressed frame and returns to app state 3.
    RECT rectExitButton;
    // +0x100 -- the background tile's SOURCE rect: the client clip bounds recentred inside the
    // loaded tile's own native extent, exactly AlbumCardWnd::rectBackBufSrc's idiom. Doubles as
    // the offset RedrawGridMaybe adds to each grid gap-erase rect to reach the matching pixels
    // of pBackgroundBitmap.
    RECT rectBackgroundSrc;
    // +0x110 -- the DESIGN-resolution anchor rect this screen's whole layout hangs off:
    // RefreshClientClipRect sets it to {0,0,800,600} and centres it inside the client clip
    // bounds, then derives every rect below from its left/top. Same role (and same 800x600
    // literal) as AlbumCardWnd::rectLayoutBase.
    RECT rectLayoutBase;
    // +0x120 -- the 3x3 layout-grid area as a whole ({+0x16,+0x23} .. {+600,+0x1d5} off the
    // anchor). OnMouseMove divides a hit inside this rect by three in each axis to recover the
    // hovered provider-slot index; OnTimerDefaultMaybe commits it as one update rect.
    RECT rectGrid;
    // +0x130 -- the title/banner strip drawn from pTitleBitmap, horizontally frame-indexed by
    // the local player's own provider slot (selectedProviderIndex) so each player sees their own
    // colour. Clicking it just plays one of eight random chirps.
    RECT rectTitleBanner;
    // +0x140 -- the nine per-provider name plates UNDER the grid. [0] is the LOCAL player's own
    // plate (drawn separately, and skipped by the loop over the other eight); [1..8] stack
    // contiguously below it, each one pSlotPlateRef-frame tall. RefreshClientClipRect builds [0]
    // from the anchor, then [1] one 0x1e gap below it, then chains [2..8] top-to-bottom.
    RECT arectSlots[9];
    // +0x1d0 -- exactly nine more RECTs' worth of space that nothing in the TU reads or writes.
    // Kept as padding rather than modeled: its size is pinned by rectSignHotspotMaybe below, but
    // there is no evidence for what (if anything) declared it.
    char pad0x1d0[0x260 - 0x1d0];
    // +0x260 -- a second decorative hotspot hanging off the grid's bottom-right corner
    // ({+100,-0x30} .. {+200,+0}). Like rectTitleBanner it only plays a random chirp
    // (0x50f3 + rand()/0x1fff) and shows the anipoint cursor.
    RECT rectSignHotspotMaybe;
    // +0x270 -- the provider slot the mouse is currently over, or -1 for none. Drives the
    // highlighted frame in DrawSlotPlate and the enlarged/blinking dot in DrawOwnerDot.
    int nHoverSlotMaybe;
    // +0x274 -- flips on every 120 ms tick; the hovered slot's dot draws large on the true
    // phase and small on the false one.
    unsigned char bBlinkPhaseMaybe;
    unsigned char pad0x275[3];
    unsigned int Unk0x278; // +0x278 -- zeroed by InitResourceRefs, never read anywhere
    unsigned char pad0x27c;
    // +0x27d -- set once by the first OnActivate and cleared by BeginModalCapture /
    // EndActiveSession: "the grid has been painted at least once", which is what arms
    // OnMouseMove's hover hit-testing (there is nothing to hover over before the first paint).
    unsigned char bGridDrawnMaybe;
    // +0x27e -- gates the whole acquire/release pair (AcquireResources vs. the identical release
    // block in EndActiveSession and the destructor), same one-flag idiom as
    // AlbumCardWnd::bWantEraseBlit.
    unsigned char bResourcesLoaded;
    unsigned char pad0x27f;
    // +0x280/+0x284 -- the screen background tile (kind id 0x3d8a), realized lazily by
    // EnsureBackgroundTileLoaded and NOT released by EndActiveSession (only by the destructor):
    // it is loaded from outside this screen, by the app-state switch at 0x408350.
    LocoBitmap *pBackgroundBitmap; // +0x280
    CursorDesc *pBackgroundTileDesc; // +0x284
    // +0x288/+0x28c -- the exit-button icon strip (kind id 0x3d87), two frames side by side:
    // frame 0 = normal, frame 1 = pressed (drawn by OffsetRect'ing the source rect one full
    // frame width right).
    LocoBitmap *pExitIconBitmap; // +0x288
    CursorDesc *pExitIconDesc;   // +0x28c
    // +0x290/+0x294 -- the title/banner strip (kind id 0x3d88), one frame per provider slot.
    LocoBitmap *pTitleBitmap; // +0x290
    CursorDesc *pTitleDesc;   // +0x294
    // +0x298 -- the per-slot name-plate frame (resource id 0x3d89), a THREE-frame strip
    // (0 = normal, 1 = hovered, 2 = the slot is beyond field_0x8 and therefore unavailable).
    // Its own `rect` is rewritten before every draw, so one ResourceRef serves all nine plates.
    ResourceRef *pSlotPlateRef;
    // +0x29c -- the nine grid cells (resource ids 0x3da4..0x3dac), one ResourceRef each, whose
    // rects RedrawGridMaybe walks row-major across the 3x3 grid.
    ResourceRef *paGridCells[9];
    // +0x2c0 -- the "ask the host for a fresh layout list" button (resource id 0x3d8b).
    ResourceRef *pRefreshBtnRef;

    // 0x430b10 -- zero-initializes every scalar field above and news the eleven ResourceRefs.
    void InitResourceRefs();

    // 0x430c20 -- lazily realizes pBackgroundTileDesc/pBackgroundBitmap (kind id 0x3d8a).
    // Called from OUTSIDE this class (the app-state switch at 0x408350), not from this TU.
    void EnsureBackgroundTileLoaded();

    // 0x431270 -- realizes the two icon strips and Load()s all eleven ResourceRefs, once.
    void AcquireResources();

    // 0x431560 -- draws one provider slot's name plate: positions pSlotPlateRef at `rect`, picks
    // its frame (2 = slot index past field_0x8, else 1 when hovered / 0 when not), draws the
    // peer's own sAddressOrName centred in the inset rect, then the owner dot at the plate's
    // right edge. Takes the plate rect BY VALUE (four consecutive dwords at the ABI); `reserved`
    // is unread in the body and 0 at both call sites, but `ret 0x18` (six dwords popped, one more
    // than rect+nSlotIndex accounts for) proves it is really there -- the same dead trailing
    // parameter AlbumCardWnd::DrawButtonIcon and PlayButtonPressFeedback carry.
    void DrawSlotPlate(RECT rect, int nSlotIndex, int reserved);

    // 0x4316f0 -- repaints the whole 3x3 grid: erases the horizontal and vertical gaps between
    // cells straight from pBackgroundBitmap, then for each occupied cell positions that cell's
    // ResourceRef, draws its frame (2 = a provider is present, 1 = empty), blits the peer's
    // stored layout bitmap into it, and stamps every train that peer has placed.
    void RedrawGridMaybe(int nUnusedMaybe);

    // 0x431a10 -- blits one provider slot's stored layout blob (pLayoutData, wLayoutCols x
    // wLayoutRows raw 8bpp) into pDestRect via a scratch LocoBitmap that is created, filled,
    // blitted and deleted per call. `this` is unread -- kept as a member for call-site parity
    // (the caller does load ECX).
    void DrawPeerScreenshotMaybe(RECT *pDestRect, int *pnSlotIndex);

    // 0x431b30 -- stamps every train the given provider slot has filed a placement result for
    // (its pResultsChainHead list) as an owner-coloured dot inside pCellRect, mapping each
    // train's board position through WindowBase's own board-to-screen helper (0x425ac0) and
    // clamping it to the cell.
    void DrawPeerTrainDotsMaybe(RECT *pCellRect, int *pnSlotIndex);

    // 0x431ed0 -- draws ONE owner dot: a 1px black-pen ellipse filled with the slot's own colour
    // out of a fixed nine-entry palette, plus a hand-placed specular highlight. Radius is 4
    // normally and 7 while this slot is both hovered and on the blink phase's true half.
    void DrawOwnerDot(HDC hdc, int x, int y, int nSlotIndex);

    // vtable slot 0x04 override -- tears the session down: clears bGridDrawnMaybe, chains the
    // base, tells the other peers our slot went away, hands focus back to the app window,
    // dirties the whole board, kills the blink timer and releases every acquired resource.
    virtual void EndActiveSession(); // 0x430e00

    // vtable slot 0x08 override -- bounces straight back to app state 3 unless
    // DPlaySessionMgr::connectionMode == 2; otherwise acquires resources, relays out, chains the
    // base, announces our slot, shows the window, hides the system cursor and arms the 120 ms
    // blink timer.
    virtual void BeginModalCapture(); // 0x430d70

    // vtable slot 0x1c override -- chains the base implementation, then (only once resources are
    // loaded, since every extent below comes off a realized descriptor) re-lays-out every rect
    // this class owns from the 800x600 anchor.
    virtual void RefreshClientClipRect(); // 0x430fe0

    // vtable slot 0x20 override -- the full repaint: background, grid, title banner, exit icon,
    // the local player's own plate and then the other eight.
    virtual void OnActivate(int reservedMaybe); // 0x431310

    // vtable slot 0x2c override -- swallows the WM_SYSCOMMAND screensaver/monitor-power codes
    // (0xf140 family) by leaving the screen first, then falls through to DefWindowProcA.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x4324f0

    // vtable slot 0x30 override -- the 120 ms blink tick: flips bBlinkPhaseMaybe, repaints the
    // grid and all nine plates, and commits rectGrid.
    virtual LRESULT OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x4323e0

    // vtable slot 0x38 override -- hit-tests lParam against the exit button, the title banner,
    // the sign hotspot and the refresh button, in that order. Only lParam is read.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x432170

    // vtable slot 0x40 override -- right-click is a plain synonym for left-click here: it just
    // dispatches back through the vtable to OnLButtonDown.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x4323c0

    // vtable slot 0x50 override -- hover tracking: the three decorative hotspots get the anipoint
    // cursor; inside rectGrid the hovered provider slot is recovered by dividing the offset by a
    // third of the grid in each axis, and a change plays a click and re-arms the point cursor.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x432540

    // vtable slot 0x54 override -- RETURN/ESCAPE flash the exit button's pressed frame and leave
    // the screen; every other key falls through to DefWindowProcA.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x430ef0

    // vtable slot 0x80 override -- WM_CLOSE leaves the screen rather than destroying the window,
    // unless the app is already tearing down (g_nScreenState == 10) or has no main window.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x432120

    // 0x430c60 (+ the ??_G scalar deleting dtor at 0x430af0, which the virtual declaration
    // yields for free). Releases the background tile, runs the same release block
    // EndActiveSession does, then deletes all eleven ResourceRefs.
    virtual ~MapWnd(); // 0x430c60
};

extern MapWnd *g_pMapWnd; // DAT_004fd388
