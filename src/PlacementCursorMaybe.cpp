// PlacementCursorMaybe (DAT_004854c8) -- the mouse-driven drag/placement controller
// singleton. One compile unit, 0x410510-0x412410 in .text, opening with this class's own
// ctor and running to the tail helpers of the Obj0x477758 collection it embeds.
//
// The class model (and the whole widget base hierarchy it derives) lives in
// src/PlacementCursorMaybe.h / src/WidgetBase.h -- see the former's banner for the
// 2026-07-25 promotion from the old padded `PlacementCursorPartial` view.
#include "PlacementCursorMaybe.h"
#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_Tracef click-path diagnostics
#endif
// The five front-end popup singletons OnMouseMoveMaybe checks the cursor against (popup 1 of 5
// here; the other four are in alphabetical order below). Each is included only for its own
// `g_p*Wnd` extern and the shared WindowBase `hwndSelf` those all reach through.
#include "AlbumCardWnd.h"       // g_pAlbumCardWnd
#include "AppWindow.h"          // g_pApp->hwndOwner -- SetCapture/ScreenToClient target
#include "BuildToolButton.h"    // g_BuildToolButton -- the toolbar rects every hit test consults
#include "DecorActor.h"         // DecorActorBase -- pHoverObjMaybe's pointee
#include "CursorDesc.h"         // BigObj / CursorDesc -- the dragged kind's descriptor
#include "DecorObjMgrMaybe.h"   // the ambient-actor manager's own click handler
#include "DSound.h"             // g_pDSoundManager->PlaySoundById -- the pick-up sound
#include "DSoundChannel.h"      // SoundBankEntry, g_pInstallPathPrefix
#include "EditCardWnd.h"        // g_pEditCardWnd   -- (popup 2 of 5)
#include "EffectSpawner.h"      // the connect/placement effect spawner
#include "GameWindowWidgetList.h" // g_gameWindowWidgetList -- the "is this tile still live?" scan
#include "MailWnd.h"            // g_pMailWnd       -- (popup 3 of 5)
#include "MapWnd.h"             // g_pMapWnd        -- (popup 4 of 5)
#include "Obj0x478118.h"        // the minifig/person kind descriptor tier -- PickUpSoundId
#include "PeerTrainSlotQueueMaybe.h" // g_PeerTrainSlotQueue -- 3rd in the click decline chain
#include "ScreenSaver.h"        // g_screenSaver.bScreenSaverMode
#include "SplashWnd.h"          // g_pSplashWnd     -- (popup 5 of 5)
#include "UIResources.h"        // g_UIResources -- sound bank + TileKind descriptor cache
#include "WorldActionCursor.h"  // the world-action cursor widget + the selected-object widget
#include "WorldBoardMaybe.h"    // g_worldBoard.MarkRectDirty
#include <string.h>            // memset -- the ctor's mouse-param array clears

// App-state dword, see src/GameNetMsgQueue.h. 1 = front end (the placement cursor is not
// this screen's business), 3 = tile placement, 4 = auto-curve track drag.
extern int g_nScreenState;
// BuildTool_SetAutoCurveConnectModeMaybe's mode, see src/NetSessionEventQueue.cpp.
// 1 = single-tile connect, 2 = footprint/area connect.
extern int DAT_00485234;
// The bulk-placement suspend flag, raised around board loads (src/NetSessionEventQueue.cpp,
// which declares the same symbol). While it is up the highlighter still COLLECTS tiles but
// skips their visual 0x400 overlay.
extern unsigned char DAT_004fd3dc;
// The app window's client rect (tagRECT_00485220), the bound OnMouseMoveMaybe rejects an
// out-of-window mouse position against. src/PopupWndBase.cpp declares the same symbol, with the
// same name and type, as a function-local extern; both should move to a shared header (AppWindow.h
// is the natural home) in a consolidation pass.
extern RECT g_rectAppClientBounds; // DAT_00485220

// ⚠ A TU-local `WorldBoardClickView0x411000` used to stand here, declaring this TU's three
// click-routing entry points (0x4556f0 / 0x455d60 / 0x455670) and reaching them by casting
// &g_worldBoard. It was a LIVE DEFECT, not just debt: src/WorldBoardMaybe.cpp DEFINES all three
// on its own differently-named view, so every call from this file compiled against a symbol
// nothing defines -- clicking to place an object did nothing at all. The three now live on the
// real WorldBoardPartial in src/WorldBoardMaybe.h (byte-free, v576); see that header's note for
// why the -124 B price the view was built to dodge no longer applies.

// FUNCTION: LOCO 0x410510
// The vtable handoff the decompiler shows (the base ctor stamps 0x477488, this body then
// stamps 0x477718) is the compiler's own, not a source statement. The embedded collection's
// own two stamps are likewise its base-then-derived ctor pair, spelled here as the ordinary
// member initializer it is.
//
// EFFECTIVE MATCH (v402). asmscore --len 335: insns 94/95, align=6, reg_pen=0,
// identity_miss=0, byte_diff=2, total=6002 -- ONE instruction apart, everything else
// byte-identical and in order. The original carries a redundant `cmp eax, ebx` at +0x7f
// (ebx being its live zero register) between the `and eax,0xa` that computes
// `m_ptr ? 10 : 0` and the store into m_count; cl here reuses the `and`'s own flags for the
// following `jne` instead. Both sides agree on every other byte, including the whole
// neg/sbb/and materialization itself. That compare lives inside Obj0x477758Base's ctor,
// which is INLINED from the shared src/Obj0x477798Family.h and is spelled to byte-match its
// other consumer (NetSessionEventQueue.cpp, unchanged at 249 B either way) -- so it is not
// this TU's spelling to choose. Probes refuted, one compile each, both leaving the score at
// exactly 6002: (1) hoisting the ternary into a local (`int nReserved = m_ptr ? nCapacity :
// 0; m_count = nReserved; if (nReserved == 0)`), and (2) spelling the condition explicitly
// (`m_ptr != 0 ? ... : ...`). Reverted both; this is a zero-register-liveness tie-break
// driven by the OUTER ctor's register state, not by the inlined header source.
PlacementCursorMaybe::PlacementCursorMaybe()
    : AnimDescRefObj0x477488(-1, -1, 0, 0), embeddedCollectionMaybe(10)
{
    bReady = true;
    SetCursorCapture(0, 1, 0);
    nTypeIdMaybe = -1;
    pHoverObjMaybe = NULL;
    bHoverActiveMaybe = false;
    memset(mouseSpeedParamsMaybe, 0, sizeof(mouseSpeedParamsMaybe));
    memset(mouseAccelDisableMaybe, 0, sizeof(mouseAccelDisableMaybe));
    // Cache the user's real OS mouse-accel setting, then disable acceleration for play.
    // ShutdownMaybe pushes the cached value back. See the header's field comment.
    SystemParametersInfoA(SPI_GETMOUSE, 0, mouseSpeedParamsMaybe, 0);
    SystemParametersInfoA(SPI_SETMOUSE, 0, mouseAccelDisableMaybe, 0);
    bPendingActionAMaybe = false;
    bPendingActionBMaybe = false;
    bPendingActionCMaybe = false;
    bPendingActionDMaybe = false;
    bSnapLockMaybe = false;
    bFlagE5Maybe = false;
    bFlagE6Maybe = false;
    bFlagE7Maybe = false;
    hBusyCursorMaybe = NULL;
    bCustomCursorShownMaybe = false;
    AdvanceAnimFrameMaybe();
}

// FUNCTION: LOCO 0x410660 (??_GPlacementCursorMaybe scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x410680
// The scalar-deleting-dtor thunk the vtable's slot 0 actually points at (0x410660) is
// compiler-generated from this declaration, not hand-written.
PlacementCursorMaybe::~PlacementCursorMaybe()
{
}

// FUNCTION: LOCO 0x410700
void PlacementCursorMaybe::ShutdownMaybe()
{
    SystemParametersInfoA(SPI_SETMOUSE, 0, mouseSpeedParamsMaybe, 0);
    embeddedCollectionMaybe.RemoveAll();
    SetCursorCapture(0, 1, 0);
    SetDescriptor(0, -1, 0);
}

// FUNCTION: LOCO 0x410750
// Warms the sound bank for the four placement effects: look each one up, force it resident
// (bPersistent = 1) and drop the lookup reference again. pSoundEntry is used purely as the
// scratch slot for the in-flight lookup and is left cleared.
bool PlacementCursorMaybe::PreloadPlacementSoundsMaybe()
{
    bool bAllLoaded = true;

    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x5015);
    pSoundEntry = pEntry;
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
        pSoundEntry->bPersistent = 1;
        pSoundEntry->Release();
        pSoundEntry = NULL;
    } else {
        bAllLoaded = false;
    }
    pEntry = g_UIResources.SoundBank_LookupEntryById(0x5014);
    pSoundEntry = pEntry;
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
        pSoundEntry->bPersistent = 1;
        pSoundEntry->Release();
        pSoundEntry = NULL;
    } else {
        bAllLoaded = false;
    }
    pEntry = g_UIResources.SoundBank_LookupEntryById(0x501a);
    pSoundEntry = pEntry;
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
        pSoundEntry->bPersistent = 1;
        pSoundEntry->Release();
        pSoundEntry = NULL;
    } else {
        bAllLoaded = false;
    }
    pEntry = g_UIResources.SoundBank_LookupEntryById(0x501b);
    pSoundEntry = pEntry;
    if (pEntry != NULL) {
        pEntry->EnsureLoaded();
        pSoundEntry->bPersistent = 1;
        pSoundEntry->Release();
        pSoundEntry = NULL;
        return bAllLoaded;
    }
    return false;
}

// FUNCTION: LOCO 0x410a20
void PlacementCursorMaybe::FUN_00410a20()
{
    if (OnMouseMoveMaybe()) {
        RefreshFootprintHighlightMaybe();
        bMouseMovePendingMaybe = false;
    }
}

// FUNCTION: LOCO 0x411760
// App-state-keyed cursor dispatcher, also inlined into the tick's tail. State 3 (tile
// placement) -> SelectCursorTypeTilePlacementMaybe; state 4 (auto-curve) ->
// SelectCursorTypeAutoCurveMaybe; anything else -> the default cursor. Forces the default
// again whenever the descriptor didn't come up valid.
void PlacementCursorMaybe::UpdateCursorForAppStateMaybe()
{
    switch (g_nScreenState) {
    case 1:
        break;
    case 3:
        SelectCursorTypeTilePlacementMaybe();
        break;
    case 4:
        SelectCursorTypeAutoCurveMaybe();
        RefreshFootprintHighlightMaybe();
        break;
    default:
        SetTypeMaybe(0x1400);
        break;
    }
    if (bValid != true) {
        SetTypeMaybe(0x1400);
    }
}

// FUNCTION: LOCO 0x411fb0
// Switches the currently-dragged type: no-op if nTypeId already matches the loaded
// descriptor. Otherwise loads the new descriptor, shows the matching cursor bitmap through
// slot 6 (SetDescriptor), and repositions using either the category-5 point hotspot or the
// generic footprint-Y hotspot.
void PlacementCursorMaybe::SetTypeMaybe(int nTypeId)
{
    if ((pKindDesc == NULL ? -1 : pKindDesc->resourceId) == nTypeId) {
        return;
    }
    CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(nTypeId);
    if (pDesc == NULL) {
        return;
    }
    SetDescriptor(nTypeId, (short)pDesc->wDefaultFrameSetIndex, 0);
    if (bValid) {
        if (pKindDesc->categoryByte == 5) {
            AnimDescRefObj0x477488::RepositionWithHotspot(lastResolvedPosX - pKindDesc->hotspotX,
                                                          lastResolvedPosY - pKindDesc->hotspotY);
        } else {
            nTypeIdMaybe = pKindDesc == NULL ? -1 : pKindDesc->resourceId;
            AnimDescRefObj0x477488::RepositionWithHotspot(
                lastResolvedPosX, lastResolvedPosY - pKindDesc->bFootprintHotspotEncodedMaybe);
        }
    }
}

// FUNCTION: LOCO 0x411dc0
// Show/hide + SetCapture/ReleaseCapture + custom-vs-system-cursor toggle. Called from
// essentially every app-state transition to release the drag tool whenever focus leaves the
// build screen. No-op while the screen saver owns the display.
void PlacementCursorMaybe::SetCursorCapture(bool bCapture, bool bShowCursorAfter, bool bBusyCursor)
{
    if ((bReady == bCapture && bCustomCursorShownMaybe == bBusyCursor) ||
        g_screenSaver.bScreenSaverMode == 1) {
        return;
    }
    bReady = bCapture;
    g_worldBoard.MarkRectDirty(rect);
    if (bCapture) {
        if (!bBusyCursor) {
            while (ShowCursor(FALSE) >= 0) {
            }
            bCustomCursorShownMaybe = false;
        } else {
            if (!bCustomCursorShownMaybe) {
                if (hBusyCursorMaybe == NULL) {
                    char szBusyCursorPath[260];
                    wsprintfA(szBusyCursorPath, "%s\\CURSORS\\%s", g_pInstallPathPrefix, "busy.ani");
                    hBusyCursorMaybe = LoadCursorFromFileA(szBusyCursorPath);
                }
                bCustomCursorShownMaybe = true;
            }
            while (ShowCursor(TRUE) < 0) {
            }
            SetCursor(hBusyCursorMaybe);
        }
        SetCapture(g_pApp->hwndOwner);
        POINT ptCursor;
        if (GetCursorPos(&ptCursor)) {
            bMouseMovePendingMaybe = true;
            ScreenToClient(g_pApp->hwndOwner, &ptCursor);
            packedMousePosMaybe = MAKELPARAM(ptCursor.x, ptCursor.y);
            if (OnMouseMoveMaybe()) {
                RefreshFootprintHighlightMaybe();
                bMouseMovePendingMaybe = false;
            }
        }
    } else {
        PlacementCursorMaybe_004854c8.bFlagE6Maybe = false;
        ReleaseCapture();
        if (bCustomCursorShownMaybe) {
            SetCursor(LoadCursorA(NULL, IDC_ARROW));
            bCustomCursorShownMaybe = false;
        }
        if (bShowCursorAfter) {
            while (ShowCursor(TRUE) < 0) {
            }
        }
    }
}

// FUNCTION: LOCO 0x412060
// Turns a raw client-space mouse point into the board position the cursor may actually occupy:
// add the world scroll, clip to the grid extent, and -- for everything except a category-5
// (single-point-hotspot) kind -- keep the whole footprint on screen, then snap to the 16px tile
// grid unless the point is over one of the toolbar's own regions (dragging across the toolbar
// stays pixel-exact so the drop test can hit a button).
POINT PlacementCursorMaybe::ClampToGridBoundsMaybe(int x, int y)
{
    POINT pt;

    int posX = x + g_worldBoard.dwScrollX;
    int posY = y + g_worldBoard.dwScrollY;
    if (posX < 0) {
        posX = 0;
    }
    if (posX >= (g_worldBoard.wCols + 1) * 16) {
        posX = g_worldBoard.wCols * 16 + 15;
    }
    if (posY < 0) {
        posY = 0;
    }
    if (posY >= (g_worldBoard.wRows + 1) * 16) {
        posY = g_worldBoard.wRows * 16 + 15;
    }
    BigObj *pDesc = pKindDesc;
    if (pDesc != NULL && pDesc->categoryByte != 5) {
        if (posX > g_worldBoard.dwViewportWidth - pDesc->nativeWidth) {
            posX = g_worldBoard.dwViewportWidth - pDesc->nativeWidth;
        }
        if (posY < pDesc->bFootprintHotspotEncodedMaybe) {
            posY = pDesc->bFootprintHotspotEncodedMaybe;
        }
        if (posY > g_worldBoard.dwViewportHeightMaybe - pDesc->bFootprintYSteps * 16) {
            posY = g_worldBoard.dwViewportHeightMaybe - pDesc->bFootprintYSteps * 16;
        }
        if (!g_BuildToolButton.ContainsAnyRegionMaybe(posX, posY)) {
            posX -= posX % 16;
            posY -= posY % 16;
        }
    }
    pt.x = posX;
    pt.y = posY;
    return pt;
}

// FUNCTION: LOCO 0x411c50
// Paint entry point A: repaint the ghost sprite (and, while an object is being carried, that
// object too) clipped to an explicit dirty rect. The carried object is blitted with NO extra
// blit flags of its own; the cursor's two layers both take nBlitFlags.
void PlacementCursorMaybe::FUN_00411c50(RECT rcClip, char flag)
{
    if (bReady && !bCustomCursorShownMaybe) {
        if (pHoverObjMaybe != NULL && bHoverActiveMaybe) {
            pHoverObjMaybe->BlitAnimFrameMaybe(rcClip, flag, 0);
        }
        AnimDescRefObj0x477488::BlitAnimFrameMaybe(rcClip, flag, nBlitFlags);
        AnimDescRefObj0x477488::BlitOverlayFrameMaybe(rcClip, flag, nBlitFlags);
    }
}

// FUNCTION: LOCO 0x411d10
// Paint entry point B: the same repaint clipped to each object's OWN rect instead of a passed
// dirty rect. Note the guard is bReady, not bValid as in FUN_00411c50 -- WorldBoardMaybe's
// dirty-tile pass calls this one directly after intersecting the rects itself.
void PlacementCursorMaybe::FUN_00411d10()
{
    if (bReady) {
        if (pHoverObjMaybe != NULL && bHoverActiveMaybe) {
            pHoverObjMaybe->BlitAnimFrameMaybe(pHoverObjMaybe->rect, 1, 0);
        }
        AnimDescRefObj0x477488::BlitAnimFrameMaybe(rect, 1, nBlitFlags);
        AnimDescRefObj0x477488::BlitOverlayFrameMaybe(rect, 1, nBlitFlags);
    }
}

// FUNCTION: LOCO 0x410840
// This class's only non-dtor vtable override (slot 10), and the whole placement cursor's
// heartbeat: chain the base's anim advance, then drain the deferred input the window procedure
// parked in the four (pending, packed position, resolved x/y) quartets, and finally re-pick the
// cursor type if any of that input actually arrived.
//
// The tail is UpdateCursorForAppStateMaybe's body written out again rather than a call -- the
// original inlines it here (0x4109c7-0x410a0d is that function byte for byte), which is what
// the shared bRefreshCursor guard wraps.
//
// EFFECTIVE MATCH (v403). asmscore --len 468: insns 134/134, align=70, reg_pen=16,
// identity_miss=16, byte_diff=52, total=71812 -- the SAME 134 instructions in the same order
// everywhere except the two ClampToGridBoundsMaybe call sequences, which differ only in which
// half of the packed position cl computes first. The original evaluates the arguments
// right-to-left (`shr` for y into the loaded register, then `and` for x into the copy, then
// the hidden return-buffer `lea` last, at [esp+0x14] i.e. after the two pushes); cl here
// evaluates left-to-right (retbuf `lea` first, `and` into the loaded register, `shr` into the
// copy) while pushing in the identical order. Everything downstream of that is the same two
// instructions displaced, which is what inflates `align` to 70 -- there is no second
// disagreement anywhere in the body. FIVE probes refuted, one compile each, all landing on
// exactly 71812 or worse: (1) `LOWORD`/`HIWORD` on the member (81386 -- the macro's `(WORD)`
// cast makes cl re-LOAD the low half from memory as `xor edx,edx; mov dx,[esi+0xc8]` instead
// of masking the register copy, which is a real extra disagreement), (2) `HIWORD` for y with
// the plain mask for x (71812 -- so the ordering is driven by the LOW half's spelling, not the
// high one), (3) hoisting the packed member into an `unsigned int` local (71812), (4) that
// local plus `LOWORD`/`HIWORD` (71812), and (5) the pre-unsigned baseline, which was strictly
// worse because it emitted `sar`. The field-type fix that came out of probe 5 is real and kept
// -- all five packed* members are `unsigned int` now (see the header). What is left is a
// register/scheduling coin-flip at a struct-return call site.
void PlacementCursorMaybe::AdvanceAnimFrameMaybe()
{
    if (!bReady) {
        return;
    }
    AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();
    bool bRefreshCursor = bPendingActionAMaybe || bPendingActionCMaybe || bPendingActionBMaybe ||
                          bMouseMovePendingMaybe;
#ifdef LOCO_PORT
    if (bPendingActionAMaybe) {
        Port_Tracef("click: tick sees pendingA, pKindDesc=%p\n", (void *)pKindDesc);
    }
#endif
    if (pKindDesc != NULL) {
        if (bMouseMovePendingMaybe) {
            if (OnMouseMoveMaybe()) {
                RefreshFootprintHighlightMaybe();
                bMouseMovePendingMaybe = false;
            }
        }
        if (bPendingActionAMaybe) {
#ifdef LOCO_PORT
            Port_Tracef("click: commit pending, packed=%08x\n", packedPendingPosAMaybe);
#endif
            CommitPendingCoupleMaybe();
        }
        if (bPendingActionBMaybe) {
            CommitPendingRotateMaybe();
        }
        // A carried actor that has wandered somewhere it may legally stand is put back down.
        // bFlagE6Maybe (left button held) suppresses that: you keep hold of it while dragging.
        if (pHoverObjMaybe != NULL && bHoverActiveMaybe && !bFlagE6Maybe) {
            if (pHoverObjMaybe->CanStandAtMaybe(pHoverObjMaybe->hotspotPosX,
                                                pHoverObjMaybe->hotspotPosY)) {
                ReleaseHoverObjMaybe();
                if (bSnapLockMaybe) {
                    pHoverObjMaybe->dwNextDecisionTickMaybe = 0;
                    pHoverObjMaybe = NULL;
                }
            }
        }
        if (bPendingActionCMaybe) {
            POINT pt = ClampToGridBoundsMaybe(packedPendingPosCMaybe & 0xffff,
                                              packedPendingPosCMaybe >> 16);
            resolvedPosCXMaybe = pt.x;
            resolvedPosCYMaybe = pt.y;
            bPendingActionCMaybe = false;
            g_worldBoard.ResetFlag0x3c(0, 0);
            bSnapLockMaybe = false;
        }
        if (bPendingActionDMaybe) {
            POINT pt = ClampToGridBoundsMaybe(packedPendingPosDMaybe & 0xffff,
                                              packedPendingPosDMaybe >> 16);
            resolvedPosDXMaybe = pt.x;
            resolvedPosDYMaybe = pt.y;
            bPendingActionBMaybe = false;
            bPendingActionDMaybe = false;
            bFlagE5Maybe = false;
            g_worldBoard.ResetFlag0x3c(0, 0);
        }
    }
    if (bRefreshCursor) {
        switch (g_nScreenState) {
        case 1:
            break;
        case 3:
            SelectCursorTypeTilePlacementMaybe();
            break;
        case 4:
            SelectCursorTypeAutoCurveMaybe();
            RefreshFootprintHighlightMaybe();
            break;
        default:
            SetTypeMaybe(0x1400);
            break;
        }
        if (bValid != true) {
            SetTypeMaybe(0x1400);
        }
    }
}

// FUNCTION: LOCO 0x4117b0
// App-state-4 (auto-curve track drag) cursor-type selector. First the toolbar gates: dragging
// the toolbar, or hovering its slot-21 hit area, forces the "busy" type 0x1404; hovering the
// button proper with Unk0xac clear falls back to the default type; and either of the button's
// two sub-regions swallows the hover. Past all that, the eight curved-track piece ids are
// re-picked from which viewport EDGE the cursor is near -- the two quartets are the two
// handednesses of the same curve, so a curve dragged to the left edge becomes the left-edge
// curve of its own family and so on.
void PlacementCursorMaybe::SelectCursorTypeAutoCurveMaybe()
{
    if (g_BuildToolButton.bDraggingMaybe == 1) {
        SetTypeMaybe(0x1404);
        ReleaseChannelAndDispatch(2);
        return;
    }
    if (g_BuildToolButton.ContainsHitAreaMaybe(lastResolvedPosX, lastResolvedPosY)) {
        SetTypeMaybe(0x1404);
        ReleaseChannelAndDispatch(0);
        g_BuildToolButton.Unk0xac = 0;
        return;
    }
    if (g_BuildToolButton.Contains(lastResolvedPosX, lastResolvedPosY)) {
        if (g_BuildToolButton.Unk0xac == 0) {
            SetTypeMaybe(0x1400);
            return;
        }
    } else {
        g_BuildToolButton.Unk0xac = 0;
    }
    if (g_BuildToolButton.regionAMaybe.bActive) {
        if (g_BuildToolButton.regionAMaybe.RectFlagObj0x477820::Contains(lastResolvedPosX,
                                                                        lastResolvedPosY)) {
            if (lastResolvedPosY <= g_BuildToolButton.regionAMaybe.rect.bottom - 0x34) {
                if (!g_BuildToolButton.bDraggingMaybe && nTypeIdMaybe > 0) {
                    SetTypeMaybe(nTypeIdMaybe);
                    return;
                }
                SetTypeMaybe(0x1404);
                return;
            }
            SetTypeMaybe(0x1400);
            return;
        }
    }
    if (g_BuildToolButton.regionBMaybe.bActive) {
        if (g_BuildToolButton.regionBMaybe.RectFlagObj0x477820::Contains(lastResolvedPosX,
                                                                        lastResolvedPosY)) {
            SetTypeMaybe(0x1400);
            return;
        }
    }
    if (DAT_00485234 == 1) {
        nTypeIdMaybe = -1;
        SetTypeMaybe(0x1402);
        return;
    }
    if (nTypeIdMaybe >= 0) {
        unsigned char nCategory = pKindDesc == NULL ? 0 : pKindDesc->categoryByte;
        if (nCategory != 0) {
            if (nTypeIdMaybe == 0xc26 || nTypeIdMaybe == 0xc28 || nTypeIdMaybe == 0xc2a ||
                nTypeIdMaybe == 0xc2c) {
                if (lastResolvedPosX < 0x10 && lastResolvedPosY > 0x10 &&
                    lastResolvedPosY < g_worldBoard.dwViewportHeightMaybe - 0x50) {
                    nTypeIdMaybe = 0xc2c;
                }
                if (lastResolvedPosX > g_worldBoard.dwViewportWidth - 0x50 &&
                    lastResolvedPosY > 0x10 &&
                    lastResolvedPosY < g_worldBoard.dwViewportHeightMaybe - 0x50) {
                    nTypeIdMaybe = 0xc26;
                }
                if (lastResolvedPosY <= 0x10 && lastResolvedPosX > 0x10 &&
                    lastResolvedPosX < g_worldBoard.dwViewportWidth - 0x50) {
                    nTypeIdMaybe = 0xc28;
                }
                if (lastResolvedPosY > g_worldBoard.dwViewportHeightMaybe - 0x40 &&
                    lastResolvedPosX > 0x10 &&
                    lastResolvedPosX < g_worldBoard.dwViewportWidth - 0x50) {
                    nTypeIdMaybe = 0xc2a;
                }
            }
            if (nTypeIdMaybe == 0xc46 || nTypeIdMaybe == 0xc48 || nTypeIdMaybe == 0xc42 ||
                nTypeIdMaybe == 0xc44) {
                if (lastResolvedPosX < 0x10 && lastResolvedPosY > 0x10 &&
                    lastResolvedPosY < g_worldBoard.dwViewportHeightMaybe - 0x50) {
                    nTypeIdMaybe = 0xc44;
                }
                if (lastResolvedPosX > g_worldBoard.dwViewportWidth - 0x50 &&
                    lastResolvedPosY > 0x10 &&
                    lastResolvedPosY < g_worldBoard.dwViewportHeightMaybe - 0x50) {
                    nTypeIdMaybe = 0xc42;
                }
                if (lastResolvedPosY <= 0x10 && lastResolvedPosX > 0x10 &&
                    lastResolvedPosX < g_worldBoard.dwViewportWidth - 0x50) {
                    nTypeIdMaybe = 0xc46;
                }
                if (lastResolvedPosY > g_worldBoard.dwViewportHeightMaybe - 0x40 &&
                    lastResolvedPosX > 0x10 &&
                    lastResolvedPosX < g_worldBoard.dwViewportWidth - 0x50) {
                    nTypeIdMaybe = 0xc48;
                }
            }
            SetTypeMaybe(nTypeIdMaybe);
            return;
        }
    }
    SetTypeMaybe(0x1400);
}

// FUNCTION: LOCO 0x411ae0
// App-state-3 (tile placement) cursor-type selector, the sibling of the auto-curve one above.
// Carrying an object, or dragging either of the two widgets that own a toolbar, forces the
// "busy" type 0x1404 (with slot 7's arg 2 vs 0 distinguishing the two reasons). Otherwise the
// hover walks the UI front-to-back -- the toolbar's hit area and the world-action widget's
// mode icon count as "busy"; the toolbar proper, the world-action widget (rect or icon) and
// the selected-object widget all just swallow the hover into the default type -- and only a
// cursor over open board with an inactive hover object gets the drop-target type 0x1405.
//
// EFFECTIVE MATCH (v404). asmscore --len 364: insns 112/112, align=0, identity_miss=20,
// byte_diff=20, total=2020 -- every instruction present, in order, with identical opcodes and
// displacements. The ENTIRE residual is the scratch register the five hit-test call sites load
// their (x, y) argument pair into: the original cycles ecx/edx, eax/ecx, edx/eax, ecx/edx,
// eax/ecx and cl here runs the same eax->ecx->edx cycle exactly ONE STEP AHEAD (eax/ecx,
// edx/eax, ...). So the original had one more temp allocated before the first call than this
// spelling produces, and every later site inherits the offset. Three probes, one compile each,
// all landing at exactly 2020 -- (1) splitting the second condition's `||` into two separate
// `if`s with a duplicated body (much WORSE, align 118: the `||` short-circuit shape is
// confirmed correct), (2) rewriting the whole chain as if/else-if/else instead of early
// returns (codegen-IDENTICAL -- the two shapes are not distinguishable here), and (3) routing
// the first hit test's result through a named `char` local before the `||`. Known intrinsic
// class (docs/CODEGEN.md's one-step eax->ecx->edx rotation, v386).
void PlacementCursorMaybe::SelectCursorTypeTilePlacementMaybe()
{
    if ((pHoverObjMaybe != NULL && bHoverActiveMaybe) || g_BuildToolButton.bDraggingMaybe ||
        g_worldActionCursor.bDraggingMaybe) {
        SetTypeMaybe(0x1404);
        ReleaseChannelAndDispatch(2);
        return;
    }
    if (g_BuildToolButton.ContainsHitAreaMaybe(lastResolvedPosX, lastResolvedPosY) ||
        (g_worldActionCursor.bActive &&
         g_worldActionCursor.animMaybe0.RectFlagObj0x477820::Contains(lastResolvedPosX,
                                                                            lastResolvedPosY))) {
        SetTypeMaybe(0x1404);
        ReleaseChannelAndDispatch(0);
        return;
    }
    if (g_BuildToolButton.Contains(lastResolvedPosX, lastResolvedPosY)) {
        SetTypeMaybe(0x1400);
        return;
    }
    if (g_worldActionCursor.bActive &&
        g_worldActionCursor.Contains(lastResolvedPosX,
                                                              lastResolvedPosY)) {
        SetTypeMaybe(0x1400);
        return;
    }
    if (SelectedObjWidgetMaybe_004852a0.bActive &&
        SelectedObjWidgetMaybe_004852a0.RectFlagObj0x477820::Contains(lastResolvedPosX,
                                                                     lastResolvedPosY)) {
        SetTypeMaybe(0x1400);
        return;
    }
    if (pHoverObjMaybe != NULL && !bHoverActiveMaybe) {
        SetTypeMaybe(0x1405);
        return;
    }
    SetTypeMaybe(0x1400);
}

// FUNCTION: LOCO 0x411000
// Services pending-action slot A (left button down / double-click). Resolves the queued packed
// position through the grid clamp, re-syncs the anim subframe for a 0x1402 descriptor, then
// offers the click down the UI stack in strict front-to-back order: the selected-object widget,
// the world-action widget, the toolbar. Whichever one both CONTAINS the point and CONSUMES it
// gets the placement click sound. Past all of them the click belongs to the world: while
// carrying an inactive hover object it is a COUPLE -- the carried actor's mood is bumped (capped
// at 7), it is pointed at the plane-B object under the cursor, a connect effect is spawned and
// the carry is dropped -- otherwise it goes to the ambient-actor manager and, failing that, to
// the board's own click handler. Every path clears the pending flag.
//
// EFFECTIVE MATCH (v404). asmscore --len 560: insns 157/157, align=32, reg_pen=32,
// identity_miss=32, byte_diff=62, total=35582 -- the same 157 instructions in the same order,
// with exactly TWO disagreements, both already-diagnosed register/scheduling classes:
//  (1) the ClampToGridBoundsMaybe call sequence, byte-for-byte the SAME residual 0x410840
//      carries (see its own autopsy above): the original evaluates the two arguments
//      right-to-left, so the loaded register keeps the `shr` for y and the copy takes the
//      `and` for x, and the hidden return-buffer `lea` lands BETWEEN the two pushes because
//      eax frees up there; cl here goes left-to-right and has to pre-compute the return
//      buffer into a third register. v403 refuted five probes on the twin site; two more here
//      landed on exactly 35582 as well -- hoisting ONLY the y argument into its own named
//      `int` local (cl folds it), and spelling the unpack as `% 0x10000` / `/ 0x10000`, which
//      is a different AST that emits the identical `and`/`shr` pair. Seven refuted probes
//      across the two sites now; treat this as closed unless the class itself cracks.
//  (2) the one-step eax->ecx->edx rotation across the SEVEN (x, y) argument pairs the hit-test
//      chain reloads -- the same class as SelectCursorTypeTilePlacementMaybe's whole residual
//      below, and here it is downstream of (1): the two spellings leave a different number of
//      scratch registers allocated by the time the first hit test is reached.
void PlacementCursorMaybe::CommitPendingCoupleMaybe()
{
    POINT pt = ClampToGridBoundsMaybe(packedPendingPosAMaybe & 0xffff,
                                      packedPendingPosAMaybe >> 16);
    resolvedPosAX = pt.x;
    resolvedPosAY = pt.y;
    int nResourceId = pKindDesc == NULL ? -1 : pKindDesc->resourceId;
    if (nResourceId == 0x1402 && nSubFrame != (short)pKindDesc->wActiveFrameSetIndex) {
        ReleaseChannelAndDispatch((short)pKindDesc->wActiveFrameSetIndex);
    }
    char bConsumed;
    if (SelectedObjWidgetMaybe_004852a0.bActive &&
        SelectedObjWidgetMaybe_004852a0.RectFlagObj0x477820::Contains(resolvedPosAX,
                                                                     resolvedPosAY)) {
        bConsumed = SelectedObjWidgetMaybe_004852a0.TryInvokeCallbackA(resolvedPosAX,
                                                                       resolvedPosAY);
    } else if (g_worldActionCursor.bActive &&
               g_worldActionCursor.Contains(resolvedPosAX,
                                                                     resolvedPosAY)) {
        bConsumed = g_worldActionCursor.TryInvokeCallbackA(resolvedPosAX, resolvedPosAY);
    } else if (
#ifdef LOCO_PORT
        (Port_Tracef("click: at %d,%d btn rect=%ld,%ld,%ld,%ld hit=%ld,%ld,%ld,%ld any=%d\n",
                     resolvedPosAX, resolvedPosAY, g_BuildToolButton.rect.left,
                     g_BuildToolButton.rect.top, g_BuildToolButton.rect.right,
                     g_BuildToolButton.rect.bottom, g_BuildToolButton.rectHitAreaMaybe.left,
                     g_BuildToolButton.rectHitAreaMaybe.top,
                     g_BuildToolButton.rectHitAreaMaybe.right,
                     g_BuildToolButton.rectHitAreaMaybe.bottom,
                     (int)g_BuildToolButton.ContainsAnyRegionMaybe(resolvedPosAX,
                                                                  resolvedPosAY)),
         0) ||
#endif
        g_BuildToolButton.ContainsAnyRegionMaybe(resolvedPosAX, resolvedPosAY)) {
        bConsumed = g_BuildToolButton.TryInvokeCallbackA(resolvedPosAX, resolvedPosAY);
#ifdef LOCO_PORT
        Port_Tracef("click: toolbar consumed=%d state=%d\n", (int)bConsumed,
                    (int)g_BuildToolButton.nButtonStateMaybe);
#endif
    } else {
        if (pHoverObjMaybe != NULL && !bHoverActiveMaybe) {
            TilePlacedObj *pTarget =
                g_worldBoard.GetPlaneBTopSlotAtPixelMaybe(resolvedPosAX, resolvedPosAY);
            if (pTarget != NULL) {
                unsigned char nMood = pHoverObjMaybe->nMoodMaybe;
                if (nMood <= 6) {
                    pHoverObjMaybe->nMoodMaybe = nMood + 1;
                }
                pHoverObjMaybe->HeadForObjectMaybe(pTarget);
                DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(0x386d, 0, 'W', resolvedPosAX,
                                                               resolvedPosAY, 1);
                pHoverObjMaybe->dwNextDecisionTickMaybe = 0;
                SetHoverObjMaybe(NULL);
            }
        } else if (!DecorObjMgrMaybe_00485448.ResolveClickMaybe(resolvedPosAX, resolvedPosAY)) {
            g_worldBoard.ResolveWorldClickMaybe(lastResolvedPosX, lastResolvedPosY);
        }
        bPendingActionAMaybe = false;
        return;
    }
    if (bConsumed) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, resolvedPosAX, resolvedPosAY, 4);
    }
    bPendingActionAMaybe = false;
}

// FUNCTION: LOCO 0x410a40
// Per-WM_MOUSEMOVE handler, driven from the tick (and directly from FUN_00410a20 after a
// programmatic cursor warp). Returns true only when the ghost sprite was actually moved, which
// is what makes the caller re-pick the cursor type.
//
// The packed client position is bounds-checked against the app's client rect, converted to
// screen space and cached; if it has wandered onto one of the five front-end popup windows the
// drag is released outright, and anywhere else outside the game window it falls through to the
// same "release, show the system cursor" tail as an out-of-bounds position. Inside the window
// the position is clamped to the board grid; an unchanged position is a no-op. Snap-lock (armed
// while the left button is down) pins one axis for the two straight-track pairs -- 0xc1c/0x3408
// lock Y, 0xc1a/0x3409 lock X -- by warping the OS cursor back onto the locked axis. Finally the
// carried hover object and the ghost sprite itself are repositioned, the latter through the
// descriptor's own point hotspot for category 5 and its footprint-Y hotspot otherwise.
//
// sic: the `x < 0` half of the bounds guard can never fire -- x is read as a WORD (see the
// `LOWORD` spelling, which the original's `xor ecx,ecx; mov cx,[esi+0x90]` pins), so it is
// zero-extended and always non-negative. Only the y half is a live test.
// sic: the auto-curve area-connect branch calls BuildToolButton::ContainsAnyRegionMaybe purely
// for its side effects and throws the hit-test result away.
//
// PARTIAL (v404), NOT parked -- this one has a live lead. asmscore --len 736: insns 231/235,
// align=126, reg_pen=14, identity_miss=14, byte_diff=120, total=127660 (down from 230168 for
// the first transcription). The bounds guard, the popup chain, the snap-lock pairs and both
// reposition tails are all byte-identical modulo register naming; the whole residual grows out
// of ONE disagreement near the top and cascades through register pressure:
//   the original RECOMPUTES `packedMousePosMaybe >> 16` for `pt.y` (a second `shr eax,0x10`)
//   instead of reusing the guard's own copy of it, and recomputes `pt.x` as `and ecx,0xffff`
//   rather than reusing the guard's word load. cl here reuses BOTH guard temps, so it is two
//   instructions short and one register freer -- which is why it also hoists SetCursorPos's IAT
//   slot into ebp (`call ebp` twice, vs. two direct `call [0x477250]`) and skips the original's
//   `mov edi,ecx` / `mov ebx,ebp` callee-saved copies of x and y.
// The guard's two values MUST be named locals: inlining either expression into the condition
// lets cl prove `LOWORD(...) >= 0` and delete that whole test, which the original does emit
// (see the sic above) -- measured 176021 with the guard inlined. So the shape wanted is
// "named guard locals whose values are then NOT reused"; nothing tried so far produces it.
// Five probes refuted, one compile each: (1) inlining the guard's y expression (176021),
// (2) `int` rather than `unsigned short` for the hover object's `nWidth` (135655 -- the
// original's `xor eax,eax; mov ax,[edx+0x14]` really is the narrow-local form), (3) both of
// those together (184016), (4) `HIWORD(packedMousePosMaybe)` for `pt.y`, hoping the redundant
// mask would fold and leave a distinct `shr` (132759 -- cl reuses the guard temp and masks it
// instead), and (5) `int` rather than `char` for `bUnchanged` (139670 -- the original's
// `mov eax,1` / `xor eax,eax` / `test al,al` trio still wants the narrow local).
unsigned char PlacementCursorMaybe::OnMouseMoveMaybe()
{
    int nClientX = LOWORD(packedMousePosMaybe);
    int nClientY = packedMousePosMaybe >> 16;
    if (nClientX >= 0 && nClientY >= 0 && nClientX <= g_rectAppClientBounds.right &&
        nClientY <= g_rectAppClientBounds.bottom) {
        POINT pt;
        pt.x = packedMousePosMaybe & 0xffff;
        pt.y = packedMousePosMaybe >> 16;
        ClientToScreen(g_pApp->hwndOwner, &pt);
        screenPosXCacheMaybe = pt.x;
        screenPosYCacheMaybe = pt.y;
        HWND hwndUnder = WindowFromPoint(pt);
        if (hwndUnder == g_pApp->hwndOwner || !bReady) {
            POINT ptGrid = ClampToGridBoundsMaybe(packedMousePosMaybe & 0xffff,
                                                  packedMousePosMaybe >> 16);
            int x = ptGrid.x;
            int y = ptGrid.y;
            char bUnchanged = x == lastResolvedPosX && y == lastResolvedPosY;
            if (!bUnchanged) {
                bool bAreaConnect = g_nScreenState == 4;
                if (bAreaConnect && DAT_00485234 == 2) {
                    g_BuildToolButton.ContainsAnyRegionMaybe(x, y);
                }
                if (bFlagE6Maybe) {
                    bSnapLockMaybe = true;
                }
                int nDescId = pKindDesc == NULL ? -1 : pKindDesc->resourceId;
                if (nDescId == nTypeIdMaybe && bSnapLockMaybe == true) {
                    if (nTypeIdMaybe == 0xc1c || nTypeIdMaybe == 0x3408) {
                        screenPosYCacheMaybe += lastResolvedPosY - y;
                        SetCursorPos(screenPosXCacheMaybe, screenPosYCacheMaybe);
                        y = lastResolvedPosY;
                    }
                    if (nTypeIdMaybe == 0xc1a || nTypeIdMaybe == 0x3409) {
                        screenPosXCacheMaybe += lastResolvedPosX - x;
                        SetCursorPos(screenPosXCacheMaybe, screenPosYCacheMaybe);
                        x = lastResolvedPosX;
                    }
                }
                DecorActorBase *pHover = pHoverObjMaybe;
                lastResolvedPosX = x;
                lastResolvedPosY = y;
                if (pHover != NULL && bHoverActiveMaybe) {
                    unsigned short nWidth = pHover->pKindDesc->nativeWidth;
                    pHover->RepositionWithHotspot(x - (nWidth >> 1), y - (nWidth >> 2));
                }
                if (pKindDesc != NULL) {
                    if (pKindDesc->categoryByte == 5) {
                        RepositionWithHotspot(lastResolvedPosX - pKindDesc->hotspotX,
                                              lastResolvedPosY - pKindDesc->hotspotY);
                        return 1;
                    }
                    RepositionWithHotspot(lastResolvedPosX,
                                          lastResolvedPosY -
                                              pKindDesc->bFootprintHotspotEncodedMaybe);
                    return 1;
                }
            }
            return 0;
        }
        if ((g_pMailWnd != NULL && hwndUnder == g_pMailWnd->hwndSelf) ||
            (g_pEditCardWnd != NULL && hwndUnder == g_pEditCardWnd->hwndSelf) ||
            (g_pAlbumCardWnd != NULL && hwndUnder == g_pAlbumCardWnd->hwndSelf) ||
            (g_pMapWnd != NULL && hwndUnder == g_pMapWnd->hwndSelf) ||
            (g_pSplashWnd != NULL && hwndUnder == g_pSplashWnd->hwndSelf)) {
            SetCursorCapture(0, 0, 0);
            return 0;
        }
    }
    SetCursorCapture(0, 1, 0);
    return 0;
}

// FUNCTION: LOCO 0x411580
// Hands the carried actor back to the world at the end of a drag. An actor is registered in
// exactly one of DecorObjMgrMaybe's two per-category registries -- 7 (people) or 8 (vehicles) --
// and the two halves below are the same "re-register if it fell out" routine written twice, once
// per registry: ask the registry for the actor's index, and if it has none, walk forward to the
// first slot that is either free or sorts after it and insert there. Category 7 additionally
// nudges the actor's mood: dropping it onto an occupied board tile cheers it up (capped at 7),
// dropping it onto bare board takes one off (floored at 0). Finally the actor is dirty-marked so
// the board repaints it in its new home, and the hover is cleared -- which is also the only thing
// that happens when there is no valid carried actor at all.
//
// Three spellings are load-bearing here and each was worth a measured step (154837 -> 1109 -> 0):
// (1) the three unsigned counters test `> 0`, not `!= 0` -- the original's `jbe`/`ja` are the
// tell, `je` is what `!= 0` gives; (2) the sorted-insert scan caches pHoverObjMaybe in its own
// local, which is why the original keeps it in a callee-saved register across the loop's calls
// instead of reloading; and (3) the mood adjust re-reads `pHoverObjMaybe->nMoodMaybe` at every
// use rather than caching it in a local -- caching it costs an eax/ecx role swap over the whole
// block (9 bytes), because the named local keeps the tile-lookup result pinned in eax past the
// point where the original has already recycled that register.
void PlacementCursorMaybe::ReleaseHoverObjMaybe()
{
    if (pHoverObjMaybe != NULL && pHoverObjMaybe->bValid == true) {
        unsigned char nCategory =
            pHoverObjMaybe->pKindDesc == NULL ? 0 : pHoverObjMaybe->pKindDesc->categoryByte;
        if (nCategory == 7) {
            PlacedObjRegistryMaybe &reg = DecorObjMgrMaybe_00485448.regCategory7Maybe;
            int nIndex = -1;
            if (reg.nCountMaybe > 0) {
                nIndex = reg.FindIndexMaybe(pHoverObjMaybe, 0, reg.nCountMaybe - 1);
            }
            if (nIndex == -1) {
                DecorActorBase *pHover = pHoverObjMaybe;
                unsigned int i;
                if (reg.nSortKeyTypeMaybe > 0) {
                    for (i = 0; i < reg.nCountMaybe; i++) {
                        if (!reg.IsSlotOccupiedMaybe(i)) {
                            break;
                        }
                        if (reg.CompareEntriesMaybe(pHover, reg.GetAtMaybe(i)) <= 0) {
                            break;
                        }
                    }
                } else {
                    i = reg.nCountMaybe;
                }
                reg.InsertAtMaybe(i, pHover);
            }
            short nTileY = pHoverObjMaybe->hotspotPosY < 0
                               ? -1
                               : (short)(pHoverObjMaybe->hotspotPosY >> 4);
            short nTileX = pHoverObjMaybe->hotspotPosX < 0
                               ? -1
                               : (short)(pHoverObjMaybe->hotspotPosX >> 4);
            TilePlacedObj *pTile = g_worldBoard.GetPlaneASlotMaybe(nTileX, nTileY, 0);
            if (pTile == NULL) {
                if (pHoverObjMaybe->nMoodMaybe > 0) {
                    pHoverObjMaybe->nMoodMaybe = pHoverObjMaybe->nMoodMaybe - 1;
                }
            } else if (pHoverObjMaybe->nMoodMaybe <= 6) {
                pHoverObjMaybe->nMoodMaybe = pHoverObjMaybe->nMoodMaybe + 1;
            }
        }
        nCategory = pHoverObjMaybe->pKindDesc == NULL ? 0 : pHoverObjMaybe->pKindDesc->categoryByte;
        if (nCategory == 8) {
            PlacedObjRegistryMaybe &reg = DecorObjMgrMaybe_00485448.regCategory8Maybe;
            int nIndex = -1;
            if (reg.nCountMaybe > 0) {
                nIndex = reg.FindIndexMaybe(pHoverObjMaybe, 0, reg.nCountMaybe - 1);
            }
            if (nIndex == -1) {
                DecorActorBase *pHover = pHoverObjMaybe;
                unsigned int i;
                if (reg.nSortKeyTypeMaybe > 0) {
                    for (i = 0; i < reg.nCountMaybe; i++) {
                        if (!reg.IsSlotOccupiedMaybe(i)) {
                            break;
                        }
                        if (reg.CompareEntriesMaybe(pHover, reg.GetAtMaybe(i)) <= 0) {
                            break;
                        }
                    }
                } else {
                    i = reg.nCountMaybe;
                }
                reg.InsertAtMaybe(i, pHover);
            }
        }
        pHoverObjMaybe->MarkDirty();
    }
    bHoverActiveMaybe = false;
}

// FUNCTION: LOCO 0x4113a0
// Adopts pObj as the cursor's hovered/carried actor -- the exact mirror of
// ReleaseHoverObjMaybe above. Releases whatever was hovered before (unless it already IS
// pObj), then for a real pObj: latches it, cancels its current errand (slot 17,
// HeadForObjectMaybe(NULL)), and DEREGISTERS it from whichever of DecorObjMgrMaybe's two
// per-category registries it lives in -- 7 (people) or 8 (vehicles), the same half written
// twice, once per registry, exactly as ReleaseHoverObjMaybe writes its re-register twice.
// Where the release path calls the registry's own InsertAtMaybe slot, the remove-at here is
// INLINED: locate the entry, memmove the tail down over it, NULL the vacated last slot and
// drop the count. Finally it raises the hover flag (unless a pending action C already owns the
// cursor), puffs the pick-up effect at the actor's own hotspot position, and plays the kind's
// `PickUpSoundId`. Returns 1 when it adopted an object, 0 when it only cleared.
//
// That sound id is the field that pins a hovered actor's descriptor to Obj0x478118 -- the
// MINIFIG/PERSON tier, whose whole 0x10-byte tail this session named off its own ini keywords
// -- and NOT to the BigObj that the shared `pKindDesc` member is declared as. Categories 7 and
// 8 are exactly the ids the descriptor factory builds that tier for, so the downcast below is
// the same one src/CarKindDesc.h's note describes for train kinds; it is a sideways move
// between two CursorDesc siblings, hence via the common base.
char PlacementCursorMaybe::SetHoverObjMaybe(DecorActorBase *pObj)
{
    char bAdopted = 0;
    if (pHoverObjMaybe != pObj) {
        ReleaseHoverObjMaybe();
        pHoverObjMaybe = NULL;
    }
    if (pObj != NULL) {
        pHoverObjMaybe = pObj;
        pObj->HeadForObjectMaybe(NULL);
        unsigned char nCategory =
            pHoverObjMaybe->pKindDesc == NULL ? 0 : pHoverObjMaybe->pKindDesc->categoryByte;
        if (nCategory == 7) {
            PlacedObjRegistryMaybe &reg = DecorObjMgrMaybe_00485448.regCategory7Maybe;
            unsigned int nIndex = 0xffffffff;
            if (reg.nCountMaybe > 0) {
                nIndex = reg.FindIndexMaybe(pHoverObjMaybe, 0, reg.nCountMaybe - 1);
            }
            if (reg.GetAtMaybe(nIndex) != NULL) {
                if (nIndex < reg.nCountMaybe - 1) {
                    memmove(&reg.pArrayMaybe[nIndex], &reg.pArrayMaybe[nIndex + 1],
                            (reg.nCountMaybe - (nIndex + 1)) * sizeof(DecorActorBase *));
                }
                reg.pArrayMaybe[reg.nCountMaybe - 1] = NULL;
                reg.nCountMaybe = reg.nCountMaybe - 1;
            }
        }
        nCategory = pHoverObjMaybe->pKindDesc == NULL ? 0 : pHoverObjMaybe->pKindDesc->categoryByte;
        if (nCategory == 8) {
            PlacedObjRegistryMaybe &reg = DecorObjMgrMaybe_00485448.regCategory8Maybe;
            unsigned int nIndex = 0xffffffff;
            if (reg.nCountMaybe > 0) {
                nIndex = reg.FindIndexMaybe(pHoverObjMaybe, 0, reg.nCountMaybe - 1);
            }
            if (reg.GetAtMaybe(nIndex) != NULL) {
                if (nIndex < reg.nCountMaybe - 1) {
                    memmove(&reg.pArrayMaybe[nIndex], &reg.pArrayMaybe[nIndex + 1],
                            (reg.nCountMaybe - (nIndex + 1)) * sizeof(DecorActorBase *));
                }
                reg.pArrayMaybe[reg.nCountMaybe - 1] = NULL;
                reg.nCountMaybe = reg.nCountMaybe - 1;
            }
        }
        bAdopted = 1;
        if (!bPendingActionCMaybe) {
            bHoverActiveMaybe = true;
        }
        DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(0x386d, 0, 'W', pObj->hotspotPosX,
                                                       pObj->hotspotPosY, 1);
        long nPickUpSoundId =
            ((Obj0x478118 *)(CursorDesc *)pHoverObjMaybe->pKindDesc)->nPickUpSoundId;
        if (nPickUpSoundId != 0 && g_pDSoundManager != NULL) {
            g_pDSoundManager->PlaySoundById(nPickUpSoundId);
        }
    }
    return bAdopted;
}

// FUNCTION: LOCO 0x411230
// Services pending-action slot B -- the RIGHT mouse button. Resolves slot B's packed position
// to the grid first, then splits on whether the button landed on the piece currently being
// dragged: if the held descriptor is still the dragged type, the click ROTATES/CYCLES it in
// place -- either stepping nSubFrame to the next frame set (wrapping to 0 at the end) or, for a
// kind that names a linked alternate in wRMBSeq, switching the drag to that type -- and
// acknowledges with sound 0x502c at slot A's position. Otherwise the click is offered to the
// world in a decline chain -- ambient actors, then the peer train-slot queue, then the board
// itself -- and whichever one consumes it acknowledges with sound 0x5015 at slot B's position.
// Either way the hover is dropped (unless something is actively hovered) and slot B is cleared.
//
// The original re-tests `nTypeIdMaybe == pKindDesc->resourceId` in the second arm even though
// the first arm already proved it: that redundant `cmp edx,ecx` at +0x85 is the tell that this
// is a chain of independent `else if`s over the same pair of conditions, not a nested if/else
// on wRMBSeq alone. The four PlaySoundAtScreenPos calls are cross-jumped by the compiler
// into one tail, which is why three of the four decline arms have no code of their own.
//
// EFFECTIVE MATCH (v406). asmscore --len 368 (0x411230..0x4113a0; the `len=357` cc.sh prints is
// the CANDIDATE's own length): total 84604, align=80 reg_pen=41 identity_miss=41 byte_diff=94,
// insns 114/111. Every branch, every call and every argument agrees; the residual is two
// register classes, both already-documented and neither this function's own spelling to fix:
//  (1) The ClampToGridBoundsMaybe ARGUMENT-EVALUATION ORDER -- the original emits
//      `shr eax,0x10` before `and ecx,0xffff`, this emits them the other way round. That is
//      exactly the class v402/v403 closed on 0x410840 and 0x411000 after SEVEN refuted probes
//      across the two sites; it is not re-probed here.
//  (2) A CSE coin-flip cascading off it: the +3 instruction delta is ENTIRELY prologue/epilogue
//      (this saves ebx/edi as well as esi, the original only esi). cl here keeps pt.x/pt.y live
//      in those two callee-saved registers and CSEs them against the later
//      resolvedPosBXMaybe/resolvedPosBYMaybe reads, where the original spills the POINT temp and
//      re-reads both members at each of the four call sites. The same coin-flip lands again on
//      wRMBSeq: the original re-reads `word ptr [eax+0x52e]` at all THREE of its uses
//      (`cmp`, `cmp`, `movsx`), this loads it once into ax. Nothing in the source caches either
//      value -- both are written as plain member reads -- so there is no local to delete.
// One probe DID land and is kept: dropping a `BigObj *pKind = pKindDesc;` local in favour of
// plain `pKindDesc->` member reads, 85279 -> 84604. Sibling of docs/CODEGEN.md's
// "local caching a struct field costs a register role swap" lever.
void PlacementCursorMaybe::CommitPendingRotateMaybe()
{
    POINT pt = ClampToGridBoundsMaybe(packedPendingPosBMaybe & 0xffff,
                                      packedPendingPosBMaybe >> 16);
    resolvedPosBXMaybe = pt.x;
    resolvedPosBYMaybe = pt.y;
    if (pKindDesc == NULL) {
        return;
    }
    if (nTypeIdMaybe == pKindDesc->resourceId && pKindDesc->wRMBSeq == 0) {
        if (nSubFrame + 1 < (int)pKindDesc->nFrameSetCount) {
            ReleaseChannelAndDispatch((short)(nSubFrame + 1));
        } else {
            ReleaseChannelAndDispatch(0);
        }
        g_UIResources.PlaySoundAtScreenPos(0x502c, resolvedPosAX, resolvedPosAY, 4);
    } else if (nTypeIdMaybe == pKindDesc->resourceId && pKindDesc->wRMBSeq > 0) {
        nTypeIdMaybe = pKindDesc->wRMBSeq;
        g_UIResources.PlaySoundAtScreenPos(0x502c, resolvedPosAX, resolvedPosAY, 4);
    } else if (DecorObjMgrMaybe_00485448.ResolveClickMaybe(resolvedPosBXMaybe,
                                                          resolvedPosBYMaybe)) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, resolvedPosBXMaybe, resolvedPosBYMaybe, 4);
    } else if (g_PeerTrainSlotQueue.SelectCarAtPositionMaybe(resolvedPosBXMaybe,
                                                            resolvedPosBYMaybe)) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, resolvedPosBXMaybe, resolvedPosBYMaybe, 4);
    } else if (g_worldBoard.ResolveWorldClickMaybe(resolvedPosBXMaybe, resolvedPosBYMaybe)) {
        g_UIResources.PlaySoundAtScreenPos(0x5015, resolvedPosBXMaybe, resolvedPosBYMaybe, 4);
    }
    if (!bHoverActiveMaybe) {
        SetHoverObjMaybe(NULL);
    }
    bPendingActionBMaybe = false;
}

// Rebuilds the auto-curve tool's highlighted-tile set, once per tick (AdvanceAnimFrameMaybe
// calls it twice, and every cursor show/hide/position-reresolve path calls it again). Two
// halves: first RETRACT -- walk the tiles this cursor highlighted last tick, and for each one
// still present in the game window's own widget list (a linear scan of g_gameWindowWidgetList,
// the "is this tile still alive?" test) and still valid, clear its blit flags and dirty-mark
// it; then empty the collection. Second EXTEND -- only in app-state 4 (auto-curve track drag)
// with a connect mode set, and only while the cursor is NOT over the toolbar: mode 1 highlights
// the single topmost plane-A slot under the cursor, mode 2 stamps the dragged kind's whole
// footprint mask over the board, one board tile per set mask cell. Highlighted tiles get
// nBlitFlags = 0x400 (the highlight overlay id) and are re-added to the collection so the next
// tick's retract half can find them again.
//
// The two DAT_004fd3dc == 1 tests in the mode-2 loop are the batch-placement suspend flag
// (src/NetSessionEventQueue.cpp raises it around bulk board loads): while it is up, tiles are
// still collected but NOT visually highlighted, and the cursor itself skips its own 0x400.
// FUNCTION: LOCO 0x410d20
//
// EFFECTIVE MATCH (v407). 736/736 bytes, insns 230/230, total 10787 (align=10 reg_pen=7
// identity_miss=7 byte_diff=17). Every block, branch, call, argument and constant agrees;
// the entire residual is 7 instructions that pick a different register for the same value.
// Two sites: (a) the ContainsAnyRegionMaybe argument pair -- the original loads
// lastResolvedPosY/X into ecx/edx and pushes them, this loads them into eax/ecx, after which
// ecx is reloaded with the `this` literal either way; (b) the packedMousePosMaybe copy in the
// snap-lock arm, eax here vs edx there, same hoist-the-load-before-the-bool-store order.
// This is exactly the closed argument-evaluation-register class the sibling call sites in
// this TU (0x410840, 0x411000, 0x411230) are already parked under -- SEVEN refuted probes,
// deliberately not re-probed. The one probe run here, swapping the `nTileY + (short)y`
// operand order, scored identical (10787).
//
// Getting here from the first compile was four diagnosed source-shape corrections, each
// measured (414430 -> 162281 -> 116274 -> 20788 -> 10787):
//   1. if/else ARM ORDER. The over-the-toolbar arm is emitted FIRST and falls through; the
//      body was written `if (!Contains(...))` and had the two arms inverted (and was 8 bytes
//      short as a result). 414430 -> 162281.
//   2. Both retract-half loops are real `for` loops, not `if (n != 0) { do ... while }`:
//      cl's zero-trip guard for an unsigned `0 < n` is `test/jbe`, while an explicit
//      `!= 0` guard is `test/je`. 162281 -> 116274.
//   3. aFootprintOccupancyMask is a 3-D `[13][9][7]` array (13*9*7 == 819 exactly), not
//      a flat byte block walked with hand-rolled `+= 7` / `+= 0x3f` row pointers. cl's own
//      strength reduction produces those two induction pointers -- and it keeps the row base
//      in a register across the outer loop, which the hand-rolled version could not.
//      116274 -> 20788. (The field was reshaped in src/CursorDesc.h; byte-neutral for its
//      only other reader, src/Obj0x4779e0.cpp's parser, which now indexes it by name too.)
//   4. Obj0x477758::Count() returns UNSIGNED. The tail's `if (Count() > 0)` compiles to
//      `jle` when it is signed and to the original's `jbe` when it is not. 20788 -> 10787.
void PlacementCursorMaybe::RefreshFootprintHighlightMaybe()
{
    BigObj *pKind = pKindDesc;
    for (unsigned int i = 0; i < embeddedCollectionMaybe.Count(); i++) {
        TilePlacedObj *pTile = (TilePlacedObj *)embeddedCollectionMaybe.GetAt(i);
        if (pTile != NULL) {
            int nIndex = -1;
            for (unsigned int j = 0; j < g_gameWindowWidgetList.nItemCount; j++) {
                if (g_gameWindowWidgetList.paItems[j] == pTile) {
                    nIndex = j;
                    break;
                }
            }
            if (nIndex >= 0 && pTile->bValid == true) {
                pTile->nBlitFlags = 0;
                pTile->MarkDirty();
            }
        }
    }
    embeddedCollectionMaybe.RemoveAll();
    nBlitFlags = 0;

    bool bAreaConnect = g_nScreenState == 4;
    if (!bAreaConnect) {
        return;
    }
    if (DAT_00485234 == 0) {
        return;
    }
    if (g_BuildToolButton.ContainsAnyRegionMaybe(lastResolvedPosX, lastResolvedPosY)) {
        if (DAT_00485234 != 2) {
            return;
        }
        if ((pKindDesc == NULL ? -1 : pKindDesc->resourceId) != nTypeIdMaybe) {
            return;
        }
        if (bSnapLockMaybe == true) {
            bSnapLockMaybe = false;
            bFlagE6Maybe = false;
            nBlitFlags = 0x400;
            return;
        }
        nBlitFlags = 0x400;
        return;
    }
    if (bSnapLockMaybe == true) {
        bPendingActionAMaybe = true;
        packedPendingPosAMaybe = packedMousePosMaybe;
    }
    if (DAT_00485234 == 1) {
        short nTileY = lastResolvedPosY < 0 ? -1 : (short)(lastResolvedPosY >> 4);
        short nTileX = lastResolvedPosX < 0 ? -1 : (short)(lastResolvedPosX >> 4);
        short nSlotIndex;
        TilePlacedObj *pTile = g_worldBoard.GetTopPlaneBSlotMaybe(nTileX, nTileY, &nSlotIndex);
        if (pTile != NULL && pTile->bValid == true && pTile->bSaveableFlag != 0) {
            pTile->nBlitFlags = 0x400;
            pTile->MarkDirty();
            embeddedCollectionMaybe.Add(pTile);
        }
    }
    if (DAT_00485234 != 2) {
        return;
    }
    if ((pKindDesc == NULL ? -1 : pKindDesc->resourceId) != nTypeIdMaybe) {
        return;
    }
    for (unsigned int x = 0; x < pKind->bFootprintXSteps; x++) {
        for (unsigned int y = 0; y < pKind->bFootprintYSteps; y++) {
            if (pKind->aFootprintOccupancyMask[x][y][0] != 0) {
                short nTileY = lastResolvedPosY < 0 ? -1 : (short)(lastResolvedPosY >> 4);
                short nTileX = lastResolvedPosX < 0 ? -1 : (short)(lastResolvedPosX >> 4);
                TilePlacedObj *pTile =
                    g_worldBoard.GetPlaneASlotMaybe(nTileX + (short)x, nTileY + (short)y, 0);
                if (pTile != NULL && pTile->bValid == true) {
                    if (DAT_004fd3dc == 1) {
                        pTile->nBlitFlags = 0x400;
                        pTile->MarkDirty();
                    }
                    embeddedCollectionMaybe.Add(pTile);
                }
            }
        }
    }
    if (DAT_004fd3dc == 1) {
        return;
    }
    if (embeddedCollectionMaybe.Count() > 0) {
        nBlitFlags = 0x400;
    }
}

// FUNCTION: LOCO 0x412440 (?Add@Obj0x477758)
// Obj0x477758::Add, slot 13 of the embedded collection's vtable. Its BODY is in-class in
// src/Obj0x477798Family.h (v431) -- the original inlines it at every call site, including the
// one 20 lines above and TrackGraph::BuildAdjacencyAMaybe's at 0x45cecf -- so this marker has
// no source declaration to pair against and carries an explicit mangled hint instead (see
// tools/match.py's `_want_key`). It is claimed HERE because VC5 emits the out-of-line copy
// only into a TU that CONSTRUCTS an Obj0x477758, and because the ORIGINAL's .obj put it here
// too: 0x412440 sits inside this compile unit's own 0x412140..0x412577 tail run.

// FUNCTION: LOCO 0x412410 (??1Obj0x477758@@)
// Obj0x477758::~Obj0x477758, slot 1 of the embedded collection's vtable, and the exact
// counterpart of the `?Add@Obj0x477758` marker above: its BODY is in-class in
// src/Obj0x477798Family.h -- a single `m_0c = 0` -- so this marker has no source declaration to
// pair against either. Everything else in the original's 48 bytes is the compiler's own epilogue,
// the base-table re-stamp plus ~Obj0x477758Base inlined. Claimed HERE for the same two reasons:
// VC5 emits the out-of-line copy only into a TU that CONSTRUCTS an Obj0x477758, and the original's
// .obj put it in this compile unit's 0x412140..0x412577 tail run.
// Until v486 this address was transcribed as a standalone `Obj0x477798` struct's destructor, with
// the base re-stamp written out by hand as a `void **vtbl` field assignment. It byte-matched the
// whole time; it was still the wrong model. See the retirement note in src/Obj0x477798Family.h.

// ⚠ The `@@` on the hint below is LOAD-BEARING, not decoration. tools/match.py's `_want_key`
// pairs a marker by SUBSTRING, and a bare `??_GObj0x477758` is a proper prefix of
// `??_GObj0x477758Base@@UAEPAXI@Z` -- which this .obj also contains, and which sorts first, so the
// marker silently claimed the BASE thunk and reported a plausible DIFF(30) against the wrong
// function. The mangled name's own terminator is what disambiguates a class from its own base.
// FUNCTION: LOCO 0x4125c0 (??_GObj0x477758@@ scalar deleting dtor)
// The DERIVED half's scalar deleting destructor -- compiler-generated, so no source line of its
// own, and claimed here for exactly the two reasons the marker above is: this TU constructs an
// Obj0x477758 (so VC5 emits the COMDAT here, and does not emit it in the class's own home file),
// and the original's .obj put it in this same tail run.
// Its three instructions of real content are `call 0x412410` -- Obj0x477758::~Obj0x477758 -- then
// the usual `if (flag & 1) operator delete(this)` and `return this`.

// FUNCTION: LOCO 0x412580 (??_GObj0x477758Base@@ scalar deleting dtor)
// The BASE half's scalar deleting destructor, and the reason the pair above is proof rather than
// suggestion. This one contains NO call: ~Obj0x477758Base is in-class and small, so VC5 expands it
// straight into the thunk, which is why 0x412580 is 62 bytes against 0x4125c0's 30. Two thunks, at
// two addresses, sitting at slot 1 of two different tables (0x477798 and 0x477758), over what a
// single flat class would have made ONE destructor body. That is what refuted the flat model in
// v485 and what v486 acted on -- see the retirement note in src/Obj0x477798Family.h.
// Claimed here rather than in the class's own home file for the same reason as its three
// neighbours: VC5 emits `??_G` only into a TU that CONSTRUCTS the class.

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 4 of 13: PlacementCursorMaybe_004854c8 (DAT_004854c8), PlacementCursorMaybe::PlacementCursorMaybe
// (0x410510).
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_PlacementCursor(void) {
    new (&PlacementCursorMaybe_004854c8) PlacementCursorMaybe();
}
#endif // LOCO_PORT
