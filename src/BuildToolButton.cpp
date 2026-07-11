// BuildToolButton method bodies. See the header / docs/subsystems.md for the class.
#include "BuildToolButton.h"
#include "TutorialWnd.h"     // g_pTutorialWnd (NotifyOrLaunch)
#include "DSound.h"          // g_pDSoundManager (DSound_SetPersistentMute)
#include "WorldBoardMaybe.h" // g_worldBoard (dwViewportWidth)
#include "BuildToolCursorWnd.h"     // g_pBuildToolCursorWnd (bToolActive)
#include "PlacementCursorMaybe.h"   // PlacementCursorMaybe_004854c8
#include "AppWindow.h"              // g_pApp (hwndOwner, for the drag cursor warp)
#include "EffectSpawner.h"          // DAT_004fd220 (the sparkle effect the closed button owns)
#include "WorldActionCursor.h"      // the two sibling widget singletons the tick deselects
#include "DPlaySessionMgr.h"        // g_pDPlaySessionMgr (connectionMode)
#include "UIResources.h"            // g_UIResources (the TileKind descriptor registry)
#include "ScreenSaver.h"            // g_screenSaver (bScreenSaverMode)
#include "NetSessionEventQueue.h"    // g_NetSessionEventQueue (SaveBoardLayout)

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_Tracef world-load diagnostics
#endif

// CursorDesc::IsItemAvailableMaybe (0x4255f0) -- "is this palette/menu item offerable right
// now" (full writeup at its twin declaration in src/WorldActionCursor.cpp). Declared free
// rather than on CursorDesc.h, whose 4 other consumers this header-rotation-sensitive family
// makes expensive to touch; a single-register-arg __fastcall callee is byte-identical to
// Ghidra's own this-in-ecx rendering.
extern unsigned char __fastcall CursorDesc_IsItemAvailableMaybe(CursorDesc *pDesc);

// 0x4089d0 -- enters/leaves auto-curve-connect draw mode (sets the WorldBoardMaybe
// bBoardDirtyNeedsRebuildFlag byte among other state). Not yet transcribed.
extern void AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(int bMode);
extern int g_nScreenState; // DAT_004851f4 -- app screen-state selector, see src/GameNetMsgQueue.h
// Byte-returning predicate, the v356 lever (docs/CODEGEN.md): OnKeyDownMaybe's DEL/BACKSPACE arm
// materializes the `== 4` as `cmp/sete cl/test cl,cl`, which is what an `inline unsigned char`
// predicate compiles to and a plain `==` does not. Same file-local shape as the sibling TUs'
// own copies (Main/DecorActor/FrameDriver).
inline unsigned char IsInGameAltModeMaybe() { return g_nScreenState == 4; }
// 0x407d00, __stdcall/void -- called by the 0x240c menu-item case between the tutorial
// notify and the slot-20 dispatch. Not yet transcribed.
extern void __stdcall AppWindow_ToggleWindowedModeMaybe(void); // 0x407d00, src/AppWindow.cpp
// Shared toggle byte flipped by the 0x240b menu-item case (also save/restored around
// NetSessionEventQueue.cpp's layout load/save paths).
extern unsigned char DAT_004fd3dc;
// 0x408130 -- the app's screen/UI-mode transition switch. See src/AlbumCardWnd.cpp.
extern void AppWindow_SetScreenState(int newState);
// 0x485234 -- the auto-curve-connect sub-mode AppWindow_BuildTool_SetAutoCurveConnectModeMaybe above
// writes (0 = off, 1 = single tile, 2 = footprint/area). Same DAT_-spelled extern
// src/PlacementCursorMaybe.cpp uses for it, which is its other reader.
extern int DAT_00485234;
// Same spellings src/WorldBoardMaybe.cpp uses for them.
extern unsigned char g_bBoardScrollFlag; // DAT_00485210
extern unsigned int g_dwScreenWidth;     // DAT_004851d8
// ⚠ A free `__fastcall BuildToolButton_ResetAndCloseToolMenu(BuildToolButton *)` alias for
// 0x44ab80 stood here until v576 -- in the very file that DEFINES that address, as the member
// BuildToolButton::ResetAndCloseToolMenuMaybe. The two spellings are physically identical
// (__fastcall's first pointer arg and __thiscall's `this` both ride ECX and neither pops), so
// nothing in the byte-match, lint_alias.py or lint_ghidra_sync.py could see it -- but the call
// below compiled against a symbol nothing defines, so in the PORT closing the tool menu called
// a generated stub and the toolbar never reset. Call the member.

// TU-local byte-returning predicate over the shared session-mode global -- the original's
// `cmp [eax+0x7c4],2; setne al; test al,al; je` MATERIALIZES the comparison into a byte
// register, which is exactly what an `unsigned char`-returning inline predicate does and a
// plain `if (g_pDPlaySessionMgr->connectionMode != 2)` does not (docs/CODEGEN.md's
// sete-materialized-predicate lesson). Deliberately kept TU-local rather than hoisted next
// to the extern it wraps: adding declarations to DPlaySessionMgr.h rotates other TUs.
static inline unsigned char IsNotConnectedMaybe() {
    return g_pDPlaySessionMgr->connectionMode != 2;
}

// FUNCTION: LOCO 0x449430
// The singleton's ctor. Member construction order (base, iconBMaybe, regionAMaybe,
// regionBMaybe) is pinned by the four ctor calls at 0x44944d/0x449468/0x449478/0x449488;
// iconBMaybe starts with no descriptor (-1) at the origin.
BuildToolButton::BuildToolButton() : iconBMaybe(-1, -1, 0, 0) {
    nTypeTag = 10;
    pAutoCurveConnectMenuItemMaybe = NULL;
    pMenuItem0x240cCachedMaybe = NULL;
}

// FUNCTION: LOCO 0x4494c0 (??_GBuildToolButton scalar deleting dtor -- compiler-generated
// around ~BuildToolButton() below; no source of its own)

// FUNCTION: LOCO 0x4494e0
// Releases both icon descriptors and both sub-regions, then chains the base's ClearOwned --
// the same five statements ClearOwned() itself runs, written out again rather than delegated
// (the original inlines them here too: there is no call to 0x4495b0 anywhere in this body,
// and writing the dtor as a plain `ClearOwned();` compiles to a 127-byte CALL, not this).
// The `pSelf` hop is load-bearing, not decoration: inside a dtor MSVC 5 knows the dynamic
// type and DEVIRTUALIZES `SetDescriptor(0,-1,0)` into a direct call to the WidgetBaseObj
// override, but the original dispatches it through this's vtable (`mov edx,[esi]; call
// [edx+0x18]`). Routing the one call through a base-class pointer local suppresses the
// devirtualization and is the only shape found that reproduces it -- note the identical
// statement in ClearOwned() below needs no such hop, because outside a dtor MSVC has no
// dynamic type to fold. See docs/CODEGEN.md.
BuildToolButton::~BuildToolButton() {
    WidgetBaseObj0x4784c8 *pSelf = this;
    iconBMaybe.SetDescriptor(0, -1, 0);
    pSelf->SetDescriptor(0, -1, 0);
    regionAMaybe.ClearOwned();
    regionBMaybe.ClearOwned();
    WidgetBaseObj0x4784c8::ClearOwned();
}

// FUNCTION: LOCO 0x4495b0
// Real vtable slot 15 override. Same body as the dtor, minus the vtable store and the
// compiler-generated member destruction that follows it there.
void BuildToolButton::ClearOwned() {
    iconBMaybe.SetDescriptor(0, -1, 0);
    SetDescriptor(0, -1, 0);
    regionAMaybe.ClearOwned();
    regionBMaybe.ClearOwned();
    WidgetBaseObj0x4784c8::ClearOwned();
}

// FUNCTION: LOCO 0x449600
// One-shot toolbar construction, run once at startup. Loads the closed-button sprite (0x2400)
// and the open-toolbar sprite (0x2402, on iconBMaybe), hands regionBMaybe its own identical
// icon-building pass, then walks the 20 TileKind ids 0x2400..0x2413 creating a menu icon node
// for every one whose descriptor is both present and currently offerable -- caching the two
// nodes this class needs to reach again by hand (0x2406 auto-curve-connect, 0x240c). Finally
// it re-loads the closed sprite (0x2401), respawns the idle sparkle effect, and arms the
// closed-state visuals: descriptor ready + shadows, blit flags, state 0, inactive, no active
// tab claim, sound channel released, and the assembly parked at (0x32, 10).
//
// The `g_screenSaver.bScreenSaverMode` branch really does name the global instance rather than
// `this` (`mov ecx,0x4aa5b8` in the original, not `mov ecx,esi`) even though the two are the
// same object here -- so the qualified-through-the-global spelling is the faithful one, and it
// devirtualizes to AnimDescRefObj0x477488::SetReadyStateMaybe exactly as the original's direct
// `call 0x4061b0` does.
//
// The trailing result is a `bool`, not the raw `char` the three early guards use: the original
// materializes the 0x2401 SetDescriptor result with `setne bl` and then returns it with
// `mov al,bl`, which is the bool-conversion + `return bResult` shape -- a plain `return true`
// would have compiled to `mov al,1`, and a plain `char` copy to `mov bl,al`.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 413: total 112031, align=112 reg_pen=0
// identity_miss=0 byte_diff=31, insns 152/140; cc.sh DIFF(295), ours 411 B vs the original's
// 413). Content-complete and structurally verified instruction-by-instruction against the raw
// disasm -- every descriptor id, the 20-iteration counted loop, both cached-node cases, the
// spawner teardown/respawn pair and all eight closed-state stores line up. TWO residuals, both
// documented intrinsic classes:
//   1. TEN of the twelve extra instructions are the VC5 cross-jump/tail-merge class: the
//      original funnels the first two guards into ONE shared `xor al,al; pop*4; ret` block at
//      +0x13c (`je 0x13c` twice) while keeping guard 3's own `return 0` as a separate
//      al-already-zero epilogue; ours duplicates the first two inline instead. Both plausible
//      source shapes were measured. Writing guards 1-2 as NESTED ifs over a single trailing
//      `return 0` -- which is what `je <far shared xor block>` literally means, and what Ghidra
//      renders -- does reproduce those two jumps exactly, but then cl merges guard 3 AND the
//      final guard into that same far block too (total 164371, insns 137/140), which is worse
//      than the early-return form kept here (112031). cl's merge threshold, not a source-shape
//      bug; do not re-grind.
//   2. The remaining two are the `setne`-widening sub-case (docs/CODEGEN.md): the original's
//      `test al,al; setne bl` vs our `mov bl,al; neg bl; sbb ebx,ebx; neg ebx`. `bool x = f();`
//      (the implicit C4800 conversion), `bool x = f() != 0;` and `unsigned char x = f() != 0;`
//      all compile BYTE-IDENTICALLY here, exactly as v394's own sub-case (3) predicts.
char BuildToolButton::InitMenuIconsMaybe() {
    char bClosedIconOk = SetDescriptor(0x2400, -1, 0);
    if (pKindDesc != NULL) {
        pKindDesc->bReadyFlagMaybe = 1;
    }
    if (!bClosedIconOk) {
#ifdef LOCO_PORT
        Port_Tracef("TOOLBAR: SetDescriptor(0x2400) failed\n"); // port-only, byte-neutral
#endif
        return 0;
    }
    if (!iconBMaybe.SetDescriptor(0x2402, -1, 0)) {
#ifdef LOCO_PORT
        Port_Tracef("TOOLBAR: iconBMaybe.SetDescriptor(0x2402) failed\n");
#endif
        return 0;
    }
    if (!regionBMaybe.InitMenuIconsMaybe()) {
#ifdef LOCO_PORT
        Port_Tracef("TOOLBAR: regionBMaybe.InitMenuIconsMaybe() failed\n");
#endif
        return 0;
    }

#ifdef LOCO_PORT
    Port_Tracef("TOOLBAR: guards ok, entering icon loop\n");
#endif
    int nKindId = 0x2400;
    int nRemaining = 0x14;
    do {
        CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(nKindId);
#ifdef LOCO_PORT
        Port_Tracef("TOOLBAR: kind %04x desc=%p avail=%d btnvis=%d shadow=%p frames=%d "
                    "must=%d cant=%d\n",
                    nKindId, (void *)pDesc,
                    pDesc == NULL ? -1 : (int)CursorDesc_IsItemAvailableMaybe(pDesc),
                    pDesc == NULL ? -1 : (int)pDesc->bButtonVisible,
                    pDesc == NULL ? (void *)0 : (void *)pDesc->pShadowBitmap,
                    pDesc == NULL ? -1 : (int)pDesc->nButtonFrameCount,
                    pDesc == NULL ? 0 : (int)pDesc->nMustHaveKindId,
                    pDesc == NULL ? 0 : (int)pDesc->nCantHaveKindId);
#endif
        if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
            switch (pDesc->resourceId) {
            case 0x2406:
                pAutoCurveConnectMenuItemMaybe = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            case 0x240c:
                pMenuItem0x240cCachedMaybe = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            default:
                GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            }
        }
        nKindId++;
        nRemaining--;
    } while (nRemaining != 0);

#ifdef LOCO_PORT
    Port_Tracef("TOOLBAR: icon loop done, pMenuListHead=%p\n", (void *)pMenuListHead);
#endif
    bool bReadyOk = SetDescriptor(0x2401, -1, 0) != 0;
    if (g_screenSaver.bScreenSaverMode == 1) {
        g_BuildToolButton.SetReadyStateMaybe(0);
    }
    if (pEffectSpawner != NULL) {
        DAT_004fd220.EffectSpawner_RemoveHandle(pEffectSpawner);
    }
#ifdef LOCO_PORT
    Port_Tracef("TOOLBAR: about to spawn idle sparkle 0x3887\n");
#endif
    if (bReady) {
        pEffectSpawner = (AnimDescRefObj0x477488 *)DAT_004fd220.EffectSpawner_SpawnSimpleMaybe(
            0x3887, 1, rect.left + 0x32, rect.top + 0x32);
    }
#ifdef LOCO_PORT
    Port_Tracef("TOOLBAR: spawn returned\n");
#endif
    if (!bReadyOk) {
#ifdef LOCO_PORT
        Port_Tracef("TOOLBAR: SetDescriptor(0x2401) failed\n");
#endif
        return 0;
    }
    pKindDesc->bReadyFlagMaybe = 1;
    pKindDesc->dwRenderFlags |= 2;
    nButtonStateMaybe = 0;
    nBlitFlags |= 2;
    bActive = false;
    g_pActiveTabWidgetMaybe = NULL;
#ifdef LOCO_PORT
    Port_Tracef("TOOLBAR: closed-state stores done\n");
#endif
    ReleaseChannelAndDispatch(0);
    RepositionWithHotspot(0x32, 10);
    return bReadyOk;
}

// FUNCTION: LOCO 0x4497a0
// Real vtable slot 10 override -- the per-frame tick, and the whole of the button's
// open/close choreography.
//
// While the toolbar is SLIDING OPEN (state 1) it first cancels any world-object selection,
// then keeps the growing assembly on screen: for each of the four viewport edges it is
// currently past, it nudges the whole thing back by the overshoot scaled by the animation's
// own progress (nAnimValueCache / the descriptor's frame-set 1 end frame, a 0..1 ramp), so
// the correction eases out as the slide finishes rather than snapping.
//
// While sliding either way (states 1 and 2) it advances the base animation, drags the
// companion effect along, and watches nAnimValueCache for the two terminal frames: frame
// set 2's end = fully CLOSED (swap back to the 0x2401 closed sprite, re-anchor, respawn the
// idle sparkle effect, drop the active-tab claim, and rebuild the small closed-state hit
// area), frame set 1's end = fully OPEN (go active, claim the active tab, adopt iconBMaybe's
// rect as the hit/drag area, redraw every menu node, and -- offline only -- poke the
// tutorial).
//
// Finally, every frame: if a drag is in flight the whole assembly simply follows the cursor
// and nothing else runs; otherwise the closed button (state 0) and the open toolbar (state 3)
// each run their own hover highlight, and the open toolbar also ticks its menu nodes and its
// two sub-regions.
//
// PARTIAL -- PARKED (v394). CONTENT-COMPLETE: 1116 bytes against the original's 1113, insns
// 353/352, asmscore total 6228, and the ONLY disagreement anywhere in the body is the three
// bytes at the IsNotConnectedMaybe() gate -- see this file's own note on that predicate and
// docs/PARKED.md. Two levers landed while transcribing and are load-bearing:
// AnimDescRefObj0x477488::nAnimValueCache is UNSIGNED (the slide ramp's `fild qword` off a
// zeroed high dword; DIFF 883 -> 345), and bDraggingMaybe is spelled `== true` at its first
// two test sites but PLAIN at the third -- the original really does mix `cmp byte,1` and
// `test al,al` for the same field inside one function.
void BuildToolButton::AdvanceAnimFrameMaybe() {
    if (nButtonStateMaybe == 1) {
        if (SelectedObjWidgetMaybe_004852a0.bActive) {
            SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(0);
        }
        if (g_worldActionCursor.bActive) {
            g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(0);
        }
        double fSlideProgress = (double)nAnimValueCache / pKindDesc->paFrameEntries[1].nEndFrame;
        if (rect.left < 0) {
            RepositionWithHotspot(rect.left - (int)(rect.left * fSlideProgress) - 1, rect.top);
        }
        if (rect.right > g_worldBoard.dwViewportWidth) {
            RepositionWithHotspot(
                (int)((g_worldBoard.dwViewportWidth - rect.right) * fSlideProgress) + rect.left - 1,
                rect.top);
        }
        if (rect.top < 0) {
            RepositionWithHotspot(rect.left, rect.top - (int)(rect.top * fSlideProgress) - 1);
        }
        if (rect.top > g_worldBoard.dwViewportHeightMaybe - rectViewport.bottom) {
            RepositionWithHotspot(
                rect.left,
                (int)((g_worldBoard.dwViewportHeightMaybe - rect.bottom) * fSlideProgress) +
                    rect.top - 1);
        }
    }
    if (nButtonStateMaybe == 1 || nButtonStateMaybe == 2) {
        AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();
        if (pEffectSpawner != NULL) {
            pEffectSpawner->AdvanceAnimFrameMaybe();
        }
        MarkDirty();
        if (nAnimValueCache == pKindDesc->paFrameEntries[2].nEndFrame) {
            SetDescriptor(0x2401, -1, 0);
            RepositionWithHotspot(rect.left + 0x31, rect.top + 0x2f);
            nButtonStateMaybe = 0;
            if (pEffectSpawner != NULL) {
                DAT_004fd220.EffectSpawner_RemoveHandle(pEffectSpawner);
            }
            if (bReady) {
                pEffectSpawner = (AnimDescRefObj0x477488 *)DAT_004fd220.EffectSpawner_SpawnSimpleMaybe(
                    0x3887, 0, rect.left + 0x32, rect.top + 0x32);
            }
            g_pActiveTabWidgetMaybe = NULL;
            AppWindow_SetScreenState(3);
            SetRect(&rectHitAreaMaybe, rect.left + 0x18, rect.top + 3, rect.left + 0x2c,
                    rect.top + 0xd);
            g_worldBoard.MarkRectDirty(rectHitAreaMaybe);
        } else if (nAnimValueCache == pKindDesc->paFrameEntries[1].nEndFrame) {
            bActive = true;
            MarkDirty();
            nButtonStateMaybe = 3;
            g_pActiveTabWidgetMaybe = this;
            rectHitAreaMaybe = iconBMaybe.rect;
            g_worldBoard.MarkRectDirty(iconBMaybe.rect);
            for (MenuNodeObj0x477568 *pNode = pMenuListHead; pNode != NULL; pNode = pNode->pNext) {
                pNode->Draw();
            }
            if (IsNotConnectedMaybe()) {
                g_pTutorialWnd->NotifyOrLaunch(6, 0);
            }
        }
    }
    if (bDraggingMaybe == true) {
        RepositionWithHotspot(
            PlacementCursorMaybe_004854c8.lastResolvedPosX - nDragGrabOffsetXMaybe,
            PlacementCursorMaybe_004854c8.lastResolvedPosY - nDragGrabOffsetYMaybe);
        return;
    }
    if (nButtonStateMaybe == 0) {
        if (!ContainsHitAreaMaybe(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                                  PlacementCursorMaybe_004854c8.lastResolvedPosY) &&
            bDraggingMaybe != true) {
            if (nSubFrame == 1) {
                ReleaseChannelAndDispatch(0);
            } else {
                AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();
            }
        } else if (nSubFrame != 1) {
            ReleaseChannelAndDispatch(1);
        }
    }
    if (nButtonStateMaybe == 3) {
        if (!ContainsHitAreaMaybe(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                                  PlacementCursorMaybe_004854c8.lastResolvedPosY) &&
            !bDraggingMaybe) {
            if (iconBMaybe.nSubFrame != 0) {
                iconBMaybe.ReleaseChannelAndDispatch(0);
                g_worldBoard.MarkRectDirty(rectHitAreaMaybe);
            }
        } else if (iconBMaybe.nSubFrame != 1) {
            iconBMaybe.ReleaseChannelAndDispatch(1);
            g_worldBoard.MarkRectDirty(rectHitAreaMaybe);
        }
        for (MenuNodeObj0x477568 *pNode = pMenuListHead; pNode != NULL; pNode = pNode->pNext) {
            HandleMenuCommandMaybe(pNode);
        }
        if (regionAMaybe.bActive) {
            regionAMaybe.AdvanceAnimFrameMaybe();
        }
        if (regionBMaybe.bActive) {
            regionBMaybe.AdvanceAnimFrameMaybe();
        }
    }
}

// FUNCTION: LOCO 0x449d80
// Ex-RectObj0x449d80::Contains (phase2_probe5.cpp) -- owner resolved 2026-07-22 (v322):
// called on &DAT_004aa5b8 (the documented BuildToolButton singleton) from
// SelectCursorTypeAutoCurveMaybe / SelectCursorTypeTilePlacementMaybe, and referenced as
// slot 21 of BuildToolButton's own vtable (0x4782fc = 0x4782a8 + 21*4). 2D bounding-box
// containment check over the button's own hit-area rect at +0x168. AL-only return, no
// EAX-wide clear. Renamed from `Contains` 2026-07-25: slot 2 is separately overridden at
// 0x449ce0, so this is a NEW slot rather than the base hit-test override it was modeled as.
char BuildToolButton::ContainsHitAreaMaybe(int x, int y) {
    if (x < rectHitAreaMaybe.left || x >= rectHitAreaMaybe.right) return 0;
    if (y < rectHitAreaMaybe.top || y >= rectHitAreaMaybe.bottom) return 0;
    return 1;
}

// FUNCTION: LOCO 0x449ce0
// Real vtable slot 2 override -- 18 bytes, an unconditional tail-call to the ROOT base's own
// rect test, deliberately skipping the AnimDescRefObj/WidgetBaseObj overrides in between.
char BuildToolButton::Contains(int x, int y) {
    return RectFlagObj0x477820::Contains(x, y);
}

// FUNCTION: LOCO 0x449d00
// Real vtable slot 22 override -- "is (x,y) over any part of the button assembly". The button
// body (slot 2) or its hit area (slot 21) short-circuit to a hit; otherwise regionAMaybe gets
// asked, and regionBMaybe is consulted whenever nothing has hit yet.
char BuildToolButton::ContainsAnyRegionMaybe(int x, int y) {
    bool bHit;
    if (Contains(x, y) || ContainsHitAreaMaybe(x, y)) {
        bHit = true;
    } else if (regionAMaybe.bActive) {
        bHit = regionAMaybe.Contains(x, y);
    } else {
        bHit = false;
    }
    if (!bHit && regionBMaybe.bActive) {
        bHit = regionBMaybe.Contains(x, y);
    }
    return bHit;
}

// FUNCTION: LOCO 0x449c00
// Blits the button and everything currently attached to it into the passed dirty rect. Not a
// vtable slot of its own -- WorldBoardMaybe's FUN_00456700 calls it directly on the singleton,
// pushing the RECT by value. The sub-objects go through slot 11 (+0x2c), which for the two
// widget regions lands on WidgetBaseObj0x4784c8's composite override rather than this plain
// frame blit.
void BuildToolButton::BlitAllRegionsMaybe(RECT rect, char bFlag) {
#ifdef LOCO_PORT
    // PORT ONLY -- the toolbar is called once per dirty TILE, so this is heavily rate-limited.
    // What it answers: whether the blit is reached at all, and with what sub-object state. A
    // toolbar that never appears looks identical to one that is called constantly and finds
    // every region inactive; only bActive/bVisible/state tell the two apart.
    {
        static unsigned int nBlit = 0;
        if ((nBlit++ % 2000u) == 0)
            Port_Tracef("toolbar blit #%u rect=%ld,%ld,%ld,%ld state=%d ready=%d act=%d "
                        "self=%ld,%ld,%ld,%ld A.act=%d B.act=%d\n",
                        nBlit, rect.left, rect.top, rect.right, rect.bottom,
                        (int)nButtonStateMaybe, (int)bReady, (int)bActive,
                        rectViewport.left, rectViewport.top, rectViewport.right,
                        rectViewport.bottom,
                        (int)regionAMaybe.bActive, (int)regionBMaybe.bActive);
    }
#endif
    AnimDescRefObj0x477488::BlitAnimFrameMaybe(rect, bFlag, 0);
    if (nButtonStateMaybe == 3) {
        iconBMaybe.BlitAnimFrameMaybe(rect, bFlag, 0);
    }
    if (regionAMaybe.bActive == 1) {
        regionAMaybe.BlitAnimFrameMaybe(rect, bFlag, 0);
    }
    if (regionBMaybe.bActive == true) {
        regionBMaybe.BlitAnimFrameMaybe(rect, bFlag, 0);
    }
}

// FUNCTION: LOCO 0x449dc0
// Real vtable slot 3 override -- moves the whole button assembly to (x,y). While the toolbar
// is sliding open (state 1) the request is taken verbatim; otherwise it is clamped into the
// viewport, and which edge does the clamping depends on bSuppressRectBMaybe -- that flag
// selects whether the tool menu hangs off the button's RIGHT (limits are viewport width minus
// the menu's own width) or its LEFT (the button may not travel left of the menu it carries).
// The clamped values live in locals: the incoming x/y are left untouched precisely so the drag
// block at the end can tell whether the clamp moved anything and, if so, warp the OS cursor
// back onto the grab point rather than letting the pointer drift off the button (the final
// `cmp edi,[esp+0x1c]` reads the untouched PARAMETER slot, which is what pins the two-copy
// shape -- clamping the parameters in place would leave nothing to compare against).
// PARTIAL -- PARKED (v393/v394, register-allocation class). CONTENT-COMPLETE, and as of v394
// the compiled length is EXACT: 753 bytes against the original's 753, insns 238/237, every
// branch instruction-aligned. asmscore total 77267 (align 74, reg_pen 29, identity_miss 29,
// byte_diff 77); DIFF went 634 -> 342 (v393) -> 119 (v394). THREE source-shape levers landed
// and all are load-bearing: (1) every clamp comparison is written in the original's own
// operand order, `nX > nLimit`, NOT `nLimit < nX` -- MSVC emits `cmp` operands in source
// order, so the reversed form flips jg/jl throughout; (2) the FIRST bSuppressRectBMaybe test
// is an explicit `== true` (the original shares the constant 1 in eax with the
// `nButtonStateMaybe != 1` compare just above: `mov eax,1` / `cmp [esi+0xad],al`); (3) the
// SECOND bSuppressRectBMaybe test is a PLAIN `if (bSuppressRectBMaybe)` whose FALL-THROUGH
// arm is the `pKindDesc->nativeWidth + nX` one -- pinned by `mov al,[esi+0xad]; test al,al;
// je <other arm>` at 0x449ef5. v393 had the same predicate written `== false` with the arms
// the other way round; swapping them was worth 252 -> 119 on its own and closed the last byte
// of the length gap. Residual is the clamp chain's working-copy register assignment: the
// original loads x straight into edi at 0x0012 and compares against edi throughout, while
// ours loads it into edx in the prologue (`mov edx,[esp+0xc]` at 0x0003), copies to edi, then
// RE-READS the parameter slot at each compare (copy propagation proves nX == x on those
// paths); the three clamp arms also disagree on whether pKindDesc->nativeWidth or the
// viewport width is CSE'd into a register first. Tried and REJECTED: flipping the hit-area
// rebuild to test `!= 0` first (342 -> 448); declaring nY before nX (no change).
void BuildToolButton::RepositionWithHotspot(int x, int y) {
    int nX = x;
    int nY = y;
    if (nButtonStateMaybe != 1) {
        int nLimit;
        if (bSuppressRectBMaybe == true) {
            if (nX < 0) {
                nX = 0;
            } else if ((regionAMaybe.bActive &&
                        (nLimit = g_worldBoard.dwViewportWidth - regionAMaybe.rectViewport.right -
                                  pKindDesc->nativeWidth,
                         nX > nLimit)) ||
                       (regionBMaybe.bActive &&
                        (nLimit = g_worldBoard.dwViewportWidth - pKindDesc->nativeWidth -
                                  regionBMaybe.rectViewport.right,
                         nX > nLimit)) ||
                       (nLimit = g_worldBoard.dwViewportWidth - pKindDesc->nativeWidth,
                        nX > nLimit)) {
                nX = nLimit;
            }
        } else {
            if (nX < 0) {
                nX = 0;
            }
            if ((regionAMaybe.bActive &&
                 (nLimit = regionAMaybe.rectViewport.right, nX < nLimit)) ||
                (regionBMaybe.bActive &&
                 (nLimit = regionBMaybe.rectViewport.right, nX < nLimit)) ||
                (nLimit = g_worldBoard.dwViewportWidth - pKindDesc->nativeWidth, nX > nLimit)) {
                nX = nLimit;
            }
        }
        if (nY < 0) {
            nY = 0;
        } else {
            nLimit = g_worldBoard.dwViewportHeightMaybe - rectViewport.bottom;
            if (nY > nLimit) {
                nY = nLimit;
            }
        }
    }
    WidgetBaseObj0x4784c8::RepositionWithHotspot(nX, nY);
    iconBMaybe.RepositionWithHotspot(iconBMaybe.pKindDesc->field_0x2eMaybe + nX,
                                     iconBMaybe.pKindDesc->field_0x30Maybe + nY);
    if (bSuppressRectBMaybe) {
        if (regionAMaybe.bActive) {
            regionAMaybe.RepositionWithHotspot(pKindDesc->nativeWidth + nX, nY + 0xe);
        }
        if (regionBMaybe.bActive) {
            regionBMaybe.RepositionWithHotspot(pKindDesc->nativeWidth + nX, nY + 0xe);
        }
    } else {
        if (regionAMaybe.bActive) {
            regionAMaybe.RepositionWithHotspot(nX - regionAMaybe.rectViewport.right, nY + 0xe);
        }
        if (regionBMaybe.bActive) {
            regionBMaybe.RepositionWithHotspot(nX - regionBMaybe.rectViewport.right, nY + 0xe);
        }
    }
    if (nButtonStateMaybe == 0) {
        SetRect(&rectHitAreaMaybe, rect.left + 0x18, rect.top + 3, rect.left + 0x2c,
                rect.top + 0xd);
    } else {
        rectHitAreaMaybe = iconBMaybe.rect;
    }
    if (bDraggingMaybe && (nX != x || nY != y)) {
        POINT ptCursor;
        ptCursor.x = nX - g_worldBoard.dwScrollX + nDragGrabOffsetXMaybe;
        ptCursor.y = nY - g_worldBoard.dwScrollY + nDragGrabOffsetYMaybe;
        ClientToScreen(g_pApp->hwndOwner, &ptCursor);
        SetCursorPos(ptCursor.x, ptCursor.y);
        PlacementCursorMaybe_004854c8.packedMousePosMaybe =
            MAKELONG((short)(nX - g_worldBoard.dwScrollX) + (short)nDragGrabOffsetXMaybe,
                     (short)(nY - g_worldBoard.dwScrollY) + (short)nDragGrabOffsetYMaybe);
        PlacementCursorMaybe_004854c8.FUN_00410a20();
    }
    if (pEffectSpawner != NULL) {
        pEffectSpawner->RepositionWithHotspot(rect.left + 0x32, rect.top + 0x32);
    }
}

// FUNCTION: LOCO 0x44a0c0
// The button's mouse-down handler. Dead while the toolbar is sliding open/closed (states 1
// and 2) and while state 3 has the build-tool cursor armed. A drag already in flight is
// simply dropped; otherwise a press inside the hit area STARTS one, recording where inside
// the button's rect the cursor grabbed it so AdvanceAnimFrameMaybe can keep that point
// pinned under the mouse. With the toolbar open, unhandled presses fall through to the two
// sub-regions (each of which may deactivate itself, which is what latches
// bSuppressRectBMaybe) and finally to the base's own per-node walk.
char BuildToolButton::TryInvokeCallbackA(int x, int y) {
#ifdef LOCO_PORT
    Port_Tracef("click: TryInvokeCallbackA %d,%d state=%d drag=%d tool=%d snap=%d body=%d "
                "hitarea=%d\n",
                x, y, (int)nButtonStateMaybe, (int)bDraggingMaybe,
                (int)g_pBuildToolCursorWnd->bToolActive,
                (int)PlacementCursorMaybe_004854c8.bSnapLockMaybe, (int)Contains(x, y),
                (int)ContainsHitAreaMaybe(x, y));
#endif
    short nState = nButtonStateMaybe;
    if (nState == 1 || nState == 2 ||
        (nState == 3 && g_pBuildToolCursorWnd->bToolActive != 0)) {
        return 0;
    }
    if (bDraggingMaybe) {
        bDraggingMaybe = false;
        return 1;
    }
    if (ContainsHitAreaMaybe(x, y) && (nButtonStateMaybe == 0 || nButtonStateMaybe == 3)) {
        nDragGrabOffsetXMaybe = PlacementCursorMaybe_004854c8.resolvedPosAX - rect.left;
        nDragGrabOffsetYMaybe = PlacementCursorMaybe_004854c8.resolvedPosAY - rect.top;
        bDraggingMaybe = true;
        return 1;
    }
    if (nButtonStateMaybe == 0) {
        if (!PlacementCursorMaybe_004854c8.bSnapLockMaybe && Contains(x, y)) {
            OnPressReleaseMaybe(1);
            return 1;
        }
    } else {
#ifdef LOCO_PORT
        Port_Tracef("icon: open-arm A.act=%d A.hit=%d B.act=%d B.hit=%d nodes=%p\n",
                    (int)regionAMaybe.bActive, (int)regionAMaybe.Contains(x, y),
                    (int)regionBMaybe.bActive, (int)regionBMaybe.Contains(x, y),
                    (void *)pMenuListHead);
#endif
        if (regionAMaybe.bActive && regionAMaybe.Contains(x, y)) {
            char bHandled = regionAMaybe.TryInvokeCallbackA(x, y);
            if (!regionAMaybe.bActive) {
                bSuppressRectBMaybe = true;
            }
            return bHandled;
        }
        if (regionBMaybe.bActive && regionBMaybe.Contains(x, y)) {
            char bHandled = regionBMaybe.TryInvokeCallbackA(x, y);
            if (!regionBMaybe.bActive) {
                bSuppressRectBMaybe = true;
            }
            return bHandled;
        }
        return WidgetBaseObj0x4784c8::TryInvokeCallbackA(x, y);
    }
    return 0;
}

// FUNCTION: LOCO 0x44a250
// vtable slot 17 override -- the toolbar menu-item click handler (Ghidra:
// BuildToolButton::HitTestNodeSecondary). Early-outs unless the node exists, is
// visible, and hit-tests (own slot 2) at the click point, then dispatches on the node
// icon's resourceId (0x2403..0x240e). Cases 0x2403/4/5 (and the 0x2409/0xa mains) each
// carry a fully-duplicated "reset every sibling toggle in pMenuListHead, select the
// clicked node, adjust the viewport-fit flag, reposition, notify the tutorial" block --
// the original's shared blocks (the deselect tail at 0x44a56e, the 0x44a50d shared
// viewport copy, the 0x44a53c/0x44a543 final tail) are the compiler's own /Og
// suffix-merges of these per-case copies, not source-level gotos.
// PARTIAL -- PARKED (v326, VC5 /Og trace-driven block-layout class, same family as
// docs/PARKED.md's 0x40bbd0/0x45a880 entries). Structure verified block-by-block
// against the raw disasm: prologue + early-outs + dispatch + case 0x2406 are
// instruction-aligned for the first ~0xbd bytes, and every case body matches
// instruction-for-instruction modulo the residuals below. asmscore total 912615
// (align 902, reg_pen 92, identity_miss 97, byte_diff 445). Residuals:
// (1) the 0x2405 case's viewport tail: the original keeps TWO branchy copies of the
//     computation (its own at 0x44a398 + the 0x2404/0x2403-shared one at 0x44a50d) with
//     a shared true-store/final tail; every all-branchy source form got /Og-merged
//     3-ways into one copy here, so 0x2405 keeps a plain if/else (compiles to
//     setge/setcc) while the 0x2404/0x2403 pair merges branchy as in the original;
// (2) the deselect tail (SetNodeState(1)+bSuppressRectBMaybe=0+ActivateTab(,0)):
//     the original shares ONE copy at 0x44a56e past the final tail; ours merges the
//     three inline copies into one too but places it inside case 0x2403's body --
//     source-level goto labels for it placed the block correctly but made the total
//     score strictly worse (tail-merge magnet effect on the surrounding cases);
// (3) push-scheduling interleaves in the 0x2409/0x240a ActivateTab tails and pervasive
//     vtable-pointer register-cache swaps ([esi+0x178] into ebp vs per-use edx reload).
// 12 source-structure variants tried: shared goto labels (1.56M), full goto structure
// (2.04M), plain per-case duplication (1.01M), arm-swapped conditions (1.01M),
// arms-duplicated tails (0.968M), goto/inline hybrids (1.08M/1.15M), temp-local fit
// computation (neutral), and the landed mixed form (plain if/else for 0x2405 +
// arms-duplicated for the 0x2404/0x2403 pair, 0.913M). Retry idea: revisit if the
// TU-context /Og layout class ever cracks (see the v323 0x45a880/0x45aa50 park note).
char BuildToolButton::HitTestNodeSecondary(MenuNodeObj0x477568 *param_1, int param_2, int param_3) {
    MenuNodeObj0x477568 *pNode;
    int nLeft;
#ifdef LOCO_PORT
    {
        static unsigned int n = 0;
        if (++n <= 40)
            Port_Tracef("icon: node=%p at %d,%d vis=%d id=%04x rect=%ld,%ld,%ld,%ld hit=%d\n",
                        (void *)param_1, param_2, param_3,
                        param_1 == 0 ? -1 : (int)param_1->bVisible,
                        param_1 == 0 || param_1->pIconDesc == 0
                            ? 0
                            : (unsigned int)param_1->pIconDesc->resourceId,
                        param_1 == 0 ? 0L : (long)param_1->rect.left,
                        param_1 == 0 ? 0L : (long)param_1->rect.top,
                        param_1 == 0 ? 0L : (long)param_1->rect.right,
                        param_1 == 0 ? 0L : (long)param_1->rect.bottom,
                        param_1 == 0 ? -1 : (int)param_1->Contains(param_2, param_3));
    }
#endif
    if (param_1 == 0 || param_1->bVisible == 0 || param_1->Contains(param_2, param_3) == 0) {
        return 0;
    }
    switch (param_1->pIconDesc->resourceId) {
    case 0x2406:
        if (param_1->wState == 1) {
            g_pTutorialWnd->NotifyOrLaunch(7, 0x2406);
            param_1->SetNodeState(2);
            AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(1);
            return 1;
        }
        param_1->SetNodeState(1);
        AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
        return 1;
    case 0x2405:
        if (param_1->wState != 1) {
            param_1->SetNodeState(1);
            bSuppressRectBMaybe = false;
            regionAMaybe.ActivateTab(param_1, 0);
            return 1;
        }
        pNode = pMenuListHead;
        if (regionAMaybe.bActive) regionAMaybe.ActivateTab(pNode, 0);
        if (regionBMaybe.bActive) regionBMaybe.ActivateTab(pNode, 0);
        bSuppressRectBMaybe = false;
        AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
        for (; pNode != 0; pNode = pNode->pNext) {
            switch (pNode->pIconDesc->resourceId) {
            case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                pNode->SetNodeState(1);
            }
        }
        param_1->SetNodeState(2);
        regionAMaybe.ActivateTab(param_1, 4);
        nLeft = rect.left;
        if ((int)(g_worldBoard.dwViewportWidth - regionAMaybe.rectViewport.right -
                  pKindDesc->nativeWidth) < nLeft) {
            bSuppressRectBMaybe = false;
        } else {
            bSuppressRectBMaybe = true;
        }
        RepositionWithHotspot(nLeft, rect.top);
        regionAMaybe.MarkDirty();
        g_pTutorialWnd->NotifyOrLaunch(8, 0);
        return 1;
    case 0x2404:
        if (param_1->wState != 1) {
            param_1->SetNodeState(1);
            bSuppressRectBMaybe = false;
            regionAMaybe.ActivateTab(param_1, 0);
            return 1;
        }
        pNode = pMenuListHead;
        if (regionAMaybe.bActive) regionAMaybe.ActivateTab(pNode, 0);
        if (regionBMaybe.bActive) regionBMaybe.ActivateTab(pNode, 0);
        bSuppressRectBMaybe = false;
        AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
        for (; pNode != 0; pNode = pNode->pNext) {
            switch (pNode->pIconDesc->resourceId) {
            case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                pNode->SetNodeState(1);
            }
        }
        param_1->SetNodeState(2);
        regionAMaybe.ActivateTab(param_1, 2);
        nLeft = rect.left;
        if ((int)(g_worldBoard.dwViewportWidth - regionAMaybe.rectViewport.right -
                  pKindDesc->nativeWidth) < nLeft) {
            bSuppressRectBMaybe = false;
            RepositionWithHotspot(nLeft, rect.top);
            regionAMaybe.MarkDirty();
            g_pTutorialWnd->NotifyOrLaunch(8, 0);
            return 1;
        }
        bSuppressRectBMaybe = true;
        RepositionWithHotspot(nLeft, rect.top);
        regionAMaybe.MarkDirty();
        g_pTutorialWnd->NotifyOrLaunch(8, 0);
        return 1;
    case 0x2403:
        if (param_1->wState != 1) {
            param_1->SetNodeState(1);
            bSuppressRectBMaybe = false;
            regionAMaybe.ActivateTab(param_1, 0);
            return 1;
        }
        pNode = pMenuListHead;
        if (regionAMaybe.bActive) regionAMaybe.ActivateTab(pNode, 0);
        if (regionBMaybe.bActive) regionBMaybe.ActivateTab(pNode, 0);
        bSuppressRectBMaybe = false;
        AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
        for (; pNode != 0; pNode = pNode->pNext) {
            switch (pNode->pIconDesc->resourceId) {
            case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                pNode->SetNodeState(1);
            }
        }
        param_1->SetNodeState(2);
        regionAMaybe.ActivateTab(param_1, 3);
        nLeft = rect.left;
        if ((int)(g_worldBoard.dwViewportWidth - regionAMaybe.rectViewport.right -
                  pKindDesc->nativeWidth) < nLeft) {
            bSuppressRectBMaybe = false;
            RepositionWithHotspot(nLeft, rect.top);
            regionAMaybe.MarkDirty();
            g_pTutorialWnd->NotifyOrLaunch(8, 0);
            return 1;
        }
        bSuppressRectBMaybe = true;
        RepositionWithHotspot(nLeft, rect.top);
        regionAMaybe.MarkDirty();
        g_pTutorialWnd->NotifyOrLaunch(8, 0);
        return 1;
    case 0x2409:
        if (param_1->wState == 1) {
            pNode = pMenuListHead;
            if (regionAMaybe.bActive) regionAMaybe.ActivateTab(pNode, 0);
            if (regionBMaybe.bActive) regionBMaybe.ActivateTab(pNode, 0);
            bSuppressRectBMaybe = false;
            AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
            for (; pNode != 0; pNode = pNode->pNext) {
                switch (pNode->pIconDesc->resourceId) {
                case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                    pNode->SetNodeState(1);
                }
            }
            param_1->SetNodeState(2);
            bSuppressRectBMaybe =
                rect.left <= (int)(g_worldBoard.dwViewportWidth - pKindDesc->nativeWidth -
                                   regionBMaybe.rectViewport.right);
            regionBMaybe.ActivateTab(param_1, 1);
            RepositionWithHotspot(rect.left, rect.top);
            regionBMaybe.MarkDirty();
            g_pTutorialWnd->NotifyOrLaunch(7, 0x2409);
            return 1;
        }
        param_1->SetNodeState(1);
        bSuppressRectBMaybe = false;
        regionBMaybe.ActivateTab(param_1, 0);
        return 1;
    case 0x240a:
        if (param_1->wState == 1) {
            pNode = pMenuListHead;
            if (regionAMaybe.bActive) regionAMaybe.ActivateTab(pNode, 0);
            if (regionBMaybe.bActive) regionBMaybe.ActivateTab(pNode, 0);
            bSuppressRectBMaybe = false;
            AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
            for (; pNode != 0; pNode = pNode->pNext) {
                switch (pNode->pIconDesc->resourceId) {
                case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                    pNode->SetNodeState(1);
                }
            }
            param_1->SetNodeState(2);
            bSuppressRectBMaybe =
                rect.left <= (int)(g_worldBoard.dwViewportWidth - pKindDesc->nativeWidth -
                                   regionBMaybe.rectViewport.right);
            regionBMaybe.ActivateTab(param_1, 5);
            RepositionWithHotspot(rect.left, rect.top);
            regionBMaybe.MarkDirty();
            g_pTutorialWnd->NotifyOrLaunch(7, 0x240a);
            return 1;
        }
        param_1->SetNodeState(1);
        regionBMaybe.ActivateTab(param_1, 0);
        pNode = pMenuListHead;
        if (regionAMaybe.bActive) regionAMaybe.ActivateTab(pNode, 0);
        if (regionBMaybe.bActive) regionBMaybe.ActivateTab(pNode, 0);
        bSuppressRectBMaybe = false;
        AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
        if (pNode != 0) {
            do {
                switch (pNode->pIconDesc->resourceId) {
                case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                    pNode->SetNodeState(1);
                }
                pNode = pNode->pNext;
            } while (pNode != 0);
            return 1;
        }
        break;
    case 0x240b:
        if (param_1->wState == 1) {
            param_1->SetNodeState(2);
            DAT_004fd3dc = 1;
        } else {
            param_1->SetNodeState(1);
            DAT_004fd3dc = 0;
        }
        g_pTutorialWnd->NotifyOrLaunch(7, 0x240b);
        return 1;
    case 0x240e:
        if (g_pDSoundManager != 0) {
            if (g_pDSoundManager->bPersistentMute) {
                g_pDSoundManager->DSound_SetPersistentMute(0);
            } else {
                g_pDSoundManager->DSound_SetPersistentMute(1);
            }
            g_pTutorialWnd->NotifyOrLaunch(7, 0x240e);
            return 1;
        }
        break;
    case 0x240c:
        g_pTutorialWnd->NotifyOrLaunch(7, 0x240c);
        AppWindow_ToggleWindowedModeMaybe();
        HandleMenuCommandMaybe(param_1);
        return 1;
    default:
        if (param_1->wState == 1) {
            param_1->SetNodeState(2);
            param_1->wSelIndexMaybe = 6;
        }
        break;
    }
    return 1;
}

// FUNCTION: LOCO 0x44a9d0
// The toolbar button's press/release handler, driven from AppWndProc's mouse dispatch
// (src/Main.cpp). Both halves are guarded on bActive, so a repeated press (or a release with
// nothing open) is a no-op.
//
// PRESS (bPressed != 0, and not already open): swaps in the pressed sprite (0x2400 subframe 1),
// shifts the whole assembly up-left by the open-toolbar hotspot delta (-0x31, -0x2f), enters
// app-state 4 (the auto-curve track-connect tool) and restarts the companion effect as the
// louder press sparkle (0x3879 instead of the idle 0x3887 InitMenuIconsMaybe spawns), then
// ducks the music.
//
// RELEASE (bPressed == 0, and currently open): dirty-marks, deactivates, hands the menu list
// head to whichever sub-region is live, leaves auto-curve-connect mode, resets every
// track-family menu node (ids 0x2403-0x2406 and 0x2409/0x240a -- 0x2407/0x2408 are deliberately
// skipped, confirmed against the original's own 8-entry jump table at 0x44ab78: index bytes
// {0,0,0,0,2,2,1,1} over targets {SetNodeState, SetNodeState, fallthrough}) back to state 1,
// checkpoints the board if it changed, swaps in the released sprite (subframe 2), tells the
// effect to wind down, and un-ducks.
void BuildToolButton::OnPressReleaseMaybe(char bPressed) {
#ifdef LOCO_PORT
    Port_Tracef("click: OnPressRelease pressed=%d active=%d ready=%d\n", (int)bPressed,
                (int)bActive, (int)bReady);
#endif
    if (bPressed) {
        if (!bActive) {
            SetDescriptor(0x2400, 1, 0);
            nButtonStateMaybe = 1;
            RepositionWithHotspot(rect.left - 0x31, rect.top - 0x2f);
            AppWindow_SetScreenState(4);
            if (pEffectSpawner != NULL) {
                DAT_004fd220.EffectSpawner_RemoveHandle(pEffectSpawner);
            }
            if (bReady) {
                pEffectSpawner = (AnimDescRefObj0x477488 *)DAT_004fd220.EffectSpawner_SpawnSimpleMaybe(
                    0x3879, 1, rect.left + 0x32, rect.top + 0x32);
            }
            if (g_pDSoundManager != NULL) {
                g_pDSoundManager->DSound_SetTemporaryDuck(true);
            }
        }
    } else {
        if (bActive) {
            MarkDirty();
            MenuNodeObj0x477568 *pNode = pMenuListHead;
            bActive = false;
            if (regionAMaybe.bActive) {
                regionAMaybe.ActivateTab(pNode, 0);
            }
            if (regionBMaybe.bActive) {
                regionBMaybe.ActivateTab(pNode, 0);
            }
            bSuppressRectBMaybe = false;
            AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
            for (; pNode != 0; pNode = pNode->pNext) {
                switch (pNode->pIconDesc->resourceId) {
                case 0x2403: case 0x2404: case 0x2405: case 0x2406: case 0x2409: case 0x240a:
                    pNode->SetNodeState(1);
                }
            }
            if (g_worldBoard.bBoardDirtyNeedsRebuildFlag) {
                g_NetSessionEventQueue.SaveBoardLayout((unsigned char *)"~curr");
            }
            SetDescriptor(0x2400, 2, 0);
            nButtonStateMaybe = 2;
            if (pEffectSpawner != NULL) {
                pEffectSpawner->ReleaseChannelAndDispatch(2);
            }
            if (g_pDSoundManager != NULL) {
                g_pDSoundManager->DSound_SetTemporaryDuck(false);
            }
        }
    }
}

// FUNCTION: LOCO 0x44ac20
// vtable slot 20 override -- the toolbar menu-node tick/repeat handler, dispatched virtually
// by DispatchMenuItemClickMaybe's 0x240c case (and the natural home of the scroll-arrow
// repeat behavior WidgetTagObj0x478378::OnKeyDownMaybe arms with wSelIndexMaybe = 3). The
// pre-switch block is the repeat countdown: a non-negative wSelIndexMaybe ticks down each
// call, and the moment it reaches 0 on a node still in the pressed state (wState == 2) the
// node is released back to state 1. The switch then re-evaluates each command node's
// availability and re-stamps its state -- so an arrow/menu node that is no longer offerable
// (board unscrollable, sound muted away, peer disconnected, the 0x240b batch-placement
// toggle flipped) visibly disables itself, and the 0x2407/0x2408/0x240f one-shot commands
// REFIRE while the countdown sits at 0 (scroll-arrow auto-repeat). Returns 0 on every path
// (AL-only, xor al,al everywhere -- the caller ignores it). The case ORDER is not numeric
// (0x2407/0x2408/0x240b/0x240e/0x240d/0x240c/0x240f): VC5 lays switch bodies out in source
// order, and the original's own layout (0x44ac70..0x44ad9b) pins this sequence.
//
// PARTIAL -- EFFECTIVE, content-complete. The whole body is instruction-aligned except ONE
// residual: the candidate's 0x240d gate emits `xor eax,eax` before `setne al`
// (insns 152/149, asmscore total 132726, byte_diff 46) where the original reuses the
// switch-index EAX with no pre-clear. This is byte-for-byte the SAME xor-pre-clear-before-
// setcc residual AdvanceAnimFrameMaybe carries at its own IsNotConnectedMaybe() gate (see
// its autopsy + docs/PARKED.md) -- same predicate, same 2-3 byte class, so it parks with it
// rather than earning its own probe budget. Probes run: direct `if (IsNotConnectedMaybe())`
// (worse -- pre-clear lands in ECX, 133732); the kept named-local form (pre-clear in EAX,
// registers otherwise aligned). Case-0x240c's loads are a register-name swap only
// (edx/eax vs eax/ecx). docs/PARKED.md row added v499.
char BuildToolButton::HandleMenuCommandMaybe(MenuNodeObj0x477568 *param_1) {
    if (param_1 == NULL) {
        return 0;
    }
    if (param_1->wSelIndexMaybe >= 0) {
        param_1->wSelIndexMaybe--;
    }
    if (param_1->wSelIndexMaybe == 0 && param_1->wState == 2) {
        param_1->SetNodeState(1);
    }
    switch (param_1->pIconDesc->resourceId) {
    case 0x2407:
        if (param_1->wSelIndexMaybe == 0 && g_pTutorialWnd->NotifyOrLaunch(7, 0x2407) == 0) {
            g_NetSessionEventQueue.RebuildBoardFromPlacedObjectsMaybe();
        }
        break;
    case 0x2408:
        if (param_1->wSelIndexMaybe == 0) {
            g_pTutorialWnd->NotifyOrLaunch(7, 0x2408);
            g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
        }
        break;
    case 0x240b:
        if (DAT_004fd3dc == 1) {
            param_1->SetNodeState(2);
        } else {
            param_1->SetNodeState(1);
        }
        break;
    case 0x240e:
        if (g_pDSoundManager != NULL && !g_pDSoundManager->bPersistentMute) {
            param_1->SetNodeState(1);
        } else {
            param_1->SetNodeState(2);
        }
        break;
    case 0x240d: {
        unsigned char bNotConnected = IsNotConnectedMaybe();
        if (bNotConnected) {
            if (param_1->wSelIndexMaybe == 0) {
                param_1->SetNodeState(2);
                g_pTutorialWnd->NotifyOrLaunch(0, 0);
            } else {
                param_1->SetNodeState(1);
            }
        } else {
            param_1->SetNodeState(3);
        }
        break;
    }
    case 0x240c:
        if (g_bBoardScrollFlag != 0 && (int)g_worldBoard.dwViewportWidth <= (int)g_dwScreenWidth) {
            param_1->SetNodeState(2);
        } else {
            param_1->SetNodeState(1);
        }
        break;
    case 0x240f:
        if (param_1->wSelIndexMaybe == 0) {
            g_pTutorialWnd->NotifyOrLaunch(7, 0x240f);
            OnPressReleaseMaybe(0);
        }
        break;
    }
    return 0;
}

// FUNCTION: LOCO 0x44e8d0
// Runs the WidgetBaseObj0x4784c8 base ctor, clears the flag/state bytes this leaf owns, and
// stamps its own family type tag (0xb) over the one the base chain wrote -- the same
// "each class writes its own literal into nTypeTag" convention the rest of the widget family
// uses (see src/WidgetBase.h).
WidgetTagObj0x478378::WidgetTagObj0x478378()
{
    Unk0xac = 0;
    bActive = false;
    pMenuItem0x2802CachedMaybe = NULL;
    pMenuItem0x2803CachedMaybe = NULL;
    nTypeTag = 0xb;
}

// FUNCTION: LOCO 0x44ab80
// Drops the toolbar back to its resting state. The two sub-regions get the SAME menu-list head
// handed to their ActivateTab, and only the active one is told -- note the head is loaded once,
// above both guards, and then reused as the walk cursor below (the original's single
// `mov esi,[edi+0xd0]` in the prologue).
// ⚠ regionAMaybe's call goes THROUGH the vtable while regionBMaybe's is a direct call to
// 0x4277d0. Both are concrete members of known type, so that asymmetry is the compiler's, not
// the source's -- reproduced as found.
// The walk returns every TRACK-family icon to state 1 while leaving 0x2407/0x2408 alone. The
// original's switch is a two-level compressed table (byte index at 0x44ac14, dword targets at
// 0x44ac08) with THREE groups whose ids run 0/2/1 in resource-id order -- which is only
// possible if the source lists the two SetNodeState groups FIRST and the do-nothing group
// LAST, so that is how it is written here.
void BuildToolButton::ResetAndCloseToolMenuMaybe() {
    MenuNodeObj0x477568 *pNode = pMenuListHead;

    if (regionAMaybe.bActive) {
        regionAMaybe.ActivateTab(pNode, 0);
    }
    if (regionBMaybe.bActive) {
        regionBMaybe.ActivateTab(pNode, 0);
    }
    bSuppressRectBMaybe = 0;
    AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
    while (pNode != NULL) {
        switch (pNode->pIconDesc->resourceId) {
        case 0x2403:
        case 0x2404:
        case 0x2405:
        case 0x2406:
            pNode->SetNodeState(1);
            break;
        case 0x2409:
        case 0x240a:
            pNode->SetNodeState(1);
            break;
        case 0x2407:
        case 0x2408:
            break;
        }
        pNode = pNode->pNext;
    }
}

// FUNCTION: LOCO 0x44adf0
// Vtable slot 16, the toolbar button's own key handler: only reacts while the button assembly
// is in state 3 (fully open), and only to three keys. ESC releases the button through the same
// handler a mouse release drives; BACKSPACE and DELETE both toggle auto-curve-connect draw
// mode, but only in app state 4 -- and they drive the menu item's own highlight state with the
// toggle so the icon tracks the mode. Any other key falls through unhandled, which is what lets
// WidgetTagObj0x478378::OnKeyDownMaybe's fallback chain keep looking.
// ⚠ The menu-item pointer is RE-READ from +0x744 after the AppWindow_BuildTool_SetAutoCurveConnectModeMaybe
// call rather than cached across it (Yoda lesson #19) -- the original reloads it at 0x44ae5e.
bool BuildToolButton::OnKeyDownMaybe(unsigned int nKey) {
    bool bHandled = false;

    if (nButtonStateMaybe == 3) {
        switch (nKey) {
        case VK_ESCAPE:
            OnPressReleaseMaybe(0);
            bHandled = true;
            break;
        case VK_BACK:
        case VK_DELETE:
            if (IsInGameAltModeMaybe()) {
                if (pAutoCurveConnectMenuItemMaybe->wState == 1) {
                    AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(1);
                    pAutoCurveConnectMenuItemMaybe->SetNodeState(2);
                } else {
                    AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
                    pAutoCurveConnectMenuItemMaybe->SetNodeState(1);
                }
                PlacementCursorMaybe_004854c8.UpdateCursorForAppStateMaybe();
                bHandled = true;
            }
            break;
        }
    }
    return bHandled;
}

// The compiler-generated scalar-deleting-dtor thunk the vtable actually points at; it calls the
// out-of-line ~WidgetTagObj0x478378 below rather than inlining it.
//
// FUNCTION: LOCO 0x44e910 (??_GWidgetTagObj0x478378 scalar deleting dtor -- compiler-generated)

// FUNCTION: LOCO 0x44e930
// Empty: the whole 11-byte body is the compiler's own vptr re-stamp plus a TAIL JUMP into
// ~WidgetBaseObj0x4784c8 (0x4545a0). The separate `??_G` scalar-deleting thunk at 0x44e910
// is what the vtable actually points at.
WidgetTagObj0x478378::~WidgetTagObj0x478378()
{
}

// FUNCTION: LOCO 0x44ec50
// EFFECTIVE MATCH -- DIFF(75) at 143/140 bytes, 56/56 instructions; ONE residual class, the original reads the localized point back
// through ComputeLocalPos's returned hidden-buffer pointer (`mov ebx,[eax]` /
// `mov ebp,[eax+4]`, with the `test edi,edi` loop guard scheduled BETWEEN the two loads)
// where this build addresses the same buffer frame-relative (`mov ebx,[esp+0x10]` /
// `mov ebp,[esp+0x14]`, both ahead of the test). The by-value POINT return makes the two
// addressings alias exactly; which one cl picks is /Og scheduling state, not source shape
// (x/y declaration order probed, byte-identical score; hoisting the pMenuListHead read into
// a pre-call local WAS source-steerable and is kept). Parked in docs/PARKED.md.
//
// Vtable slot 10 -- the region's per-frame tick. Re-localizes the placement cursor's last
// resolved position into region space (x scroll-adjusted by nCarouselScrollIndex cells),
// then walks pMenuListHead dispatching each node through slot 19 (the hover test/toggle)
// and slot 20 (the execute half, which services the armed repeat countdowns). Finally drops
// the Unk0xac latch once the cursor has left the region's own rect.
void WidgetTagObj0x478378::AdvanceAnimFrameMaybe()
{
    MenuNodeObj0x477568 *pNode = pMenuListHead;
    POINT ptLocal = ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosX +
                                        nCarouselScrollIndex * 0x39,
                                    PlacementCursorMaybe_004854c8.lastResolvedPosY);
    int x = ptLocal.x;
    int y = ptLocal.y;
    for (; pNode != NULL; pNode = pNode->pNext) {
        TestAndToggleMenuNodeHoverMaybe(pNode, x, y);
        HandleMenuCommandMaybe(pNode);
    }
    if (Unk0xac != 0 && Contains(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                                 PlacementCursorMaybe_004854c8.lastResolvedPosY) == 0) {
        Unk0xac = 0;
    }
}

// FUNCTION: LOCO 0x44ece0
// Vtable slot 22 -- the menu's own backdrop pass, dispatched from the tail of ActivateTab and
// again from both scroll arrows in HandleMenuCommandMaybe. pLastHitNode (the 0x2804 tool kind
// cached in the base's own last-hit slot) doubles as a reusable STAMP: its rect is moved to
// each of the nine 0x39-pitch grid cells anchored at (7, 0x11) in turn and redrawn in place,
// tiling the backdrop out of one node rather than nine. Only after that are the real menu
// nodes drawn over it.
//
// Note (not marked sic -- no evidence it is unintended): both the right AND the bottom edge
// of every cell come from the descriptor's wShadowBitmapHeight, so the cells are forced
// SQUARE off the height alone. The sibling that lays out the real icons,
// MenuNodeObj0x477568::PlaceIconInGridMaybe, uses wShadowFrameWidth for the width instead --
// so either the backdrop tile is known-square by construction, or this stretches a
// non-square one. Nothing observed so far distinguishes the two readings.
//
// Returns 1 whenever there was a stamp node to draw with, 0 otherwise.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 152: total 12229, align=12 reg_pen=2
// identity_miss=2 byte_diff=9, insns 61/61; cc.sh DIFF(98), 155 B vs 152). Content-complete
// and structurally identical -- the ENTIRE diff is three rows, and all three are one register
// choice. The original has a spare register at the SetRect: it keeps pIconDesc in EDX and
// zero-extends wShadowBitmapHeight into a pre-cleared EAX (`xor eax,eax; mov ax,[edx+0x2a]`,
// the documented unsigned-short widening form -- which independently confirms the field's
// declared type). Ours reuses EAX for pIconDesc and so has to mask afterwards
// (`mov eax,[ecx+0x44]; mov ax,[eax+0x2a]; and eax,0xffff`) -- same instruction count, one
// different register. REFUTED as source-steerable: caching pLastHitNode in a local, caching
// pIconDesc in its own local, and declaring nCell `int` instead of `unsigned short` were all
// tried; the first two are BYTE-IDENTICAL to this form and the third is strictly worse
// (DIFF 121 / 150 B -- it drops the widening entirely). The form kept is the one whose loads
// match the original's: pLastHitNode is re-read for the Draw rather than held.
unsigned char WidgetTagObj0x478378::LayoutMenuIconGridMaybe()
{
    if (pLastHitNode == NULL) {
        return 0;
    }
    pLastHitNode->SetNodeState(1);
    int x = 0;
    int nCols = 3;
    do {
        int y = 0;
        int nRows = 3;
        do {
            unsigned short nCell = pLastHitNode->pIconDesc->wShadowBitmapHeight;
            SetRect(&pLastHitNode->rect, x + 7, y + 0x11, x + 7 + nCell, y + 0x11 + nCell);
            pLastHitNode->Draw();
            y += 0x39;
        } while (--nRows != 0);
        x += 0x39;
    } while (--nCols != 0);

    for (MenuNodeObj0x477568 *pNode = pMenuListHead; pNode != NULL; pNode = pNode->pNext) {
        pNode->Draw();
    }
    return 1;
}

// FUNCTION: LOCO 0x44ed80
// Vtable slot 17 -- the region's per-node click handler. The x coordinate must land inside
// the node's own column band: wider than the descriptor's shadow-frame width from both the
// left edge and rectViewport's right. Carousel nodes (wModeFlagsMaybe bit 2) first get x
// scroll-adjusted by nCarouselScrollIndex cells, so the hit test runs in unscrolled node
// space. Only then is the node's own Contains consulted.
//
// A hit on 0x2801 (close) or 0x2802/0x2803 (the carousel arrows) just presses the node
// (state 2, repeat countdown armed to 6) out of state 1 -- the arrows' actual scrolling
// lives in HandleMenuCommandMaybe, which reads that countdown. A hit on any OTHER id is a
// tool selection: if the auto-curve-connect mode flag (DAT_00485234) is on, it is first
// cancelled via the button's delete-key path; an already-selected node (state 3) drops
// back to normal and clears the connect mode, otherwise every node on the list is reset to
// state 1, this one takes state 3, and the placement cursor's type, the tutorial notify,
// the connect mode and the click sound all fire with the node's resourceId.
char WidgetTagObj0x478378::HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y)
{
    if (pNode != NULL) {
        int nWidth = 0x39 - pNode->pIconDesc->wShadowFrameWidth;
        if (x >= nWidth && x <= this->rectViewport.right - nWidth) {
            int nLocalX = x;
            if ((pNode->wModeFlagsMaybe & 2) != 0) {
                nLocalX = x + this->nCarouselScrollIndex * 0x39;
            }
            if (pNode->Contains(nLocalX, y)) {
                int nResourceId = pNode->pIconDesc->resourceId;
                if (nResourceId != 0x2801) {
                    if (nResourceId <= 0x2801 || nResourceId > 0x2803) {
                        if (DAT_00485234 == 1) {
                            g_BuildToolButton.OnKeyDownMaybe(0x2e);
                            AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(2);
                        }
                        if (pNode->wState == 3) {
                            pNode->SetNodeState(1);
                            AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(0);
                            return 1;
                        }
                        for (MenuNodeObj0x477568 *pOther = pMenuListHead; pOther != NULL;
                             pOther = pOther->pNext) {
                            pOther->SetNodeState(1);
                        }
                        pNode->SetNodeState(3);
                        PlacementCursorMaybe_004854c8.nTypeIdMaybe =
                            pNode->pIconDesc->resourceId;
                        g_pTutorialWnd->NotifyOrLaunch(4, pNode->pIconDesc->resourceId);
                        AppWindow_BuildTool_SetAutoCurveConnectModeMaybe(2);
                        g_UIResources.PlaySoundAtScreenPos(
                            0x5014, PlacementCursorMaybe_004854c8.lastResolvedPosX,
                            PlacementCursorMaybe_004854c8.lastResolvedPosY, 4);
                        return 1;
                    }
                    if (pNode->wState == 1) {
                        pNode->SetNodeState(2);
                        pNode->wSelIndexMaybe = 6;
                    }
                    return 0;
                }
                if (pNode->wState == 1) {
                    pNode->SetNodeState(2);
                    pNode->wSelIndexMaybe = 6;
                }
                return 1;
            }
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x44f190
// Vtable slot 16 -- this region's key handler. It always runs the base's own handler first
// (which is what routes a key into an editable node's label), then adds the two accelerators
// the tool menu owns: VK_LEFT and VK_RIGHT press the carousel's back/forward arrow nodes as
// if they had been clicked, by putting the cached node into the "pressed" state (2) and
// arming its repeat countdown to 3 -- exactly the state HandleMenuCommandMaybe's arrow arms
// then tick down. Both accelerators count as handled even when the arrow node is absent or
// not in its resting state, so the fallback below never sees an arrow key.
//
// Anything neither the base nor an accelerator claimed is handed on to the owning toolbar
// button's own handler (a direct, non-virtual call on the singleton).
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 122: total 50801, align=50 reg_pen=7
// identity_miss=7 byte_diff=31, insns 44/39; cc.sh DIFF(111), 134 B vs 122). Content-complete;
// every guard, both accelerators, the repeat-arm value and the fallback are verified against
// the raw disasm. The five extra instructions are ONE allocator decision with a knock-on: the
// original keeps bHandled in AL for the whole body (it is never live across a call there --
// each arm assigns it AFTER its SetNodeState calls, and the default path has no call at all),
// so it needs only two callee-saved registers and compares the node state against an
// immediate. cl instead gives bHandled its own callee-saved EBX (`push ebx` + `mov bl,al` +
// `mov al,bl`) and then reuses that register to hold the constant 1, turning both
// `cmp word ptr [ecx+0x48], 1` into `cmp word ptr [ecx+0x48], bx` and adding a `mov ebx,1` per
// arm. REFUTED: hoisting `bHandled = true` to the top of each case is BYTE-IDENTICAL. REFUTED
// on evidence, despite a better score: rewriting the switch as `if (nKey == VK_LEFT) ... else
// if (nKey == VK_RIGHT)` scores 132 B / DIFF(94), but it emits `cmp edi,0x25` where the
// original emits the `mov ecx,edi; sub ecx,0x25; je; sub ecx,2; jne` subtract-chain that only
// a switch produces -- structural evidence beats the byte count (v462's lesson).
bool WidgetTagObj0x478378::OnKeyDownMaybe(unsigned int nKey)
{
    bool bHandled = WidgetBaseObj0x4784c8::OnKeyDownMaybe(nKey);
    switch (nKey) {
    case VK_LEFT:
        if (pMenuItem0x2802CachedMaybe != NULL && pMenuItem0x2802CachedMaybe->wState == 1) {
            pMenuItem0x2802CachedMaybe->SetNodeState(2);
            pMenuItem0x2802CachedMaybe->wSelIndexMaybe = 3;
        }
        bHandled = true;
        break;
    case VK_RIGHT:
        if (pMenuItem0x2803CachedMaybe != NULL && pMenuItem0x2803CachedMaybe->wState == 1) {
            pMenuItem0x2803CachedMaybe->SetNodeState(2);
            pMenuItem0x2803CachedMaybe->wSelIndexMaybe = 3;
        }
        bHandled = true;
        break;
    }
    if (!bHandled) {
        g_BuildToolButton.OnKeyDownMaybe(nKey);
    }
    return bHandled;
}

// FUNCTION: LOCO 0x44ef10
// Vtable slot 19 -- the per-node hover callback AdvanceAnimFrameMaybe (0x44ec50) dispatches
// every menu node through with the localized cursor point. Only carousel-enabled nodes
// (wModeFlagsMaybe bit 2) answer at all: inside the node's rect a resting node (wState 1) is
// raised to the hover state (2), outside it a hovering node is dropped back to rest. Returns
// whether the point is inside (the caller's hover latch), 0 for a missing/ineligible node.
char WidgetTagObj0x478378::TestAndToggleMenuNodeHoverMaybe(MenuNodeObj0x477568 *pNode, int x, int y)
{
    if (pNode == NULL) {
        return 0;
    }
    if (!(pNode->wModeFlagsMaybe & 2)) {
        return 0;
    }
    if (!pNode->Contains(x, y)) {
        if (pNode->wState == 2) {
            pNode->SetNodeState(1);
        }
        return 0;
    }
    if (pNode->wState == 1) {
        pNode->SetNodeState(2);
    }
    return 1;
}

// FUNCTION: LOCO 0x44ef70
// Vtable slot 20 -- the per-node "execute this menu command" callback the widget family's
// slot-17/18 hit-test pair feeds. This leaf only owns three commands, all of them chrome for
// the tool menu ActivateTab builds: 0x2801 is the CLOSE button, 0x2802 and 0x2803 are the
// carousel's two scroll arrows (back and forward respectively -- the two arms are exact
// mirrors of each other, one bounded by 0 and the other by nCarouselMaxIndex).
//
// Before dispatching, three pieces of shared per-node bookkeeping run for EVERY node,
// command or not: a node still showing the auto-curve-connect "armed" state (3) is dropped
// back to normal while that mode is on; the node's own repeat/hold countdown ticks down one
// step (never past -1); and a node whose countdown has just run out drops out of the
// "pressed" state (2). That countdown is what makes a held-down arrow auto-repeat: each
// accepted scroll re-arms it to 6 -- but only if the pointer is still over the arrow AND the
// left button is still down, so releasing or sliding off stops the repeat on the next tick.
//
// Once the countdown reaches 0 the arrow re-states itself from the bound it just moved
// against: still scrollable -> normal (1), at the end of travel -> disabled (3).
//
// Always returns 0 ("not consumed"): every exit is a bare `xor al,al`.
char WidgetTagObj0x478378::HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode)
{
    if (pNode == NULL) {
        return 0;
    }
    if (DAT_00485234 == 1 && pNode->wState == 3) {
        pNode->SetNodeState(1);
    }
    if (pNode->wSelIndexMaybe >= 0) {
        pNode->wSelIndexMaybe--;
    }
    if (pNode->wSelIndexMaybe == 0 && pNode->wState == 2) {
        pNode->SetNodeState(1);
    }

    switch (pNode->pIconDesc->resourceId) {
    case 0x2801:
        if (pNode->wSelIndexMaybe == 0) {
            g_BuildToolButton.ResetAndCloseToolMenuMaybe();
        }
        break;
    case 0x2802:
    {
        POINT pt;
        if (pNode->wSelIndexMaybe == 0 && nCarouselScrollIndex > 0) {
            g_UIResources.PlaySoundAtScreenPos(0x5015,
                                               PlacementCursorMaybe_004854c8.resolvedPosAX,
                                               PlacementCursorMaybe_004854c8.resolvedPosAY, 4);
            nCarouselScrollIndex--;
            LayoutMenuIconGridMaybe();
            pt = ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                                 PlacementCursorMaybe_004854c8.lastResolvedPosY);
            if (pNode->Contains(pt.x, pt.y) && PlacementCursorMaybe_004854c8.bFlagE6Maybe) {
                pNode->SetNodeState(2);
                pNode->wSelIndexMaybe = 6;
            }
        }
        if (pNode->wSelIndexMaybe <= 0) {
            if (nCarouselScrollIndex <= 0) {
                pNode->SetNodeState(3);
            } else {
                pNode->SetNodeState(1);
            }
        }
        break;
    }
    case 0x2803:
    {
        POINT pt;
        if (pNode->wSelIndexMaybe == 0 && nCarouselScrollIndex < nCarouselMaxIndex) {
            g_UIResources.PlaySoundAtScreenPos(0x5015,
                                               PlacementCursorMaybe_004854c8.resolvedPosAX,
                                               PlacementCursorMaybe_004854c8.resolvedPosAY, 4);
            nCarouselScrollIndex++;
            LayoutMenuIconGridMaybe();
            pt = ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                                 PlacementCursorMaybe_004854c8.lastResolvedPosY);
            if (pNode->Contains(pt.x, pt.y) && PlacementCursorMaybe_004854c8.bFlagE6Maybe) {
                pNode->SetNodeState(2);
                pNode->wSelIndexMaybe = 6;
            }
        }
        if (pNode->wSelIndexMaybe <= 0) {
            if (nCarouselScrollIndex >= nCarouselMaxIndex) {
                pNode->SetNodeState(3);
            } else {
                pNode->SetNodeState(1);
            }
        }
        break;
    }
    }
    return 0;
}

// FUNCTION: LOCO 0x44e940
// Vtable slot 21 -- the tool-menu tag region's whole open/close body, and this leaf's exact
// counterpart to WidgetPickerObj0x477cc8::ActivateTab (0x428400): same signature, same
// category vocabulary, same pLastActivatedNode/g_pActiveTabWidgetMaybe bookkeeping.
//
// Category 0 CLOSES: dirty-mark, deactivate, hand the keyboard focus back to the button that
// owns this region, and reset the placement cursor's type -- then report "not open".
//
// Categories 2/3/4 (RE)OPEN, and the body is category-independent right up to the last
// switch. It destroys the whole existing node list, re-arms the region's own 0x2800
// descriptor, and populates in two passes:
//   1. the ten FIXED tool kinds 0x2800..0x2809, four of which are additionally cached in
//      their own fields for later lookup (see the class's field notes);
//   2. the category's own TileKind id RANGE -- 0x800 x 500 for category 2, 0xc00 x 400 plus
//      0x3000 x 100 plus 0x3400 x 100 for category 3, 0x1000 x 400 for category 4 -- with
//      every accepted node laid out through MenuNodeObj0x477568::PlaceIconInGridMaybe.
// Pass 1 goes through TileKind_GetOrLoadDescriptor and pass 2 through its no-alias twin,
// which is the only behavioural difference between the two loops' lookups.
//
// Finally it activates, decides whether the assembly still fits to the right of the viewport
// (bSuppressRectBMaybe), takes the keyboard focus, and lays the icons out through slot 22.
unsigned char WidgetTagObj0x478378::ActivateTab(MenuNodeObj0x477568 *pNode, unsigned short nCategory)
{
    pLastActivatedNode = pNode;
    if (nCategory == 0) {
        MarkDirty();
        bActive = false;
        g_pActiveTabWidgetMaybe = &g_BuildToolButton;
        PlacementCursorMaybe_004854c8.nTypeIdMaybe = -1;
        g_worldBoard.MarkRectDirty(rect);
        return 0;
    }

    if (pMenuListHead != NULL) {
        delete pMenuListHead;
        pMenuListHead = NULL;
    }
    nCarouselMaxIndex = 0;
    SetDescriptor(0x2800, -1, 0);

    int nKindId = 0x2800;
    int nRemaining = 10;
    do {
        CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(nKindId);
#ifdef LOCO_PORT
        Port_Tracef("TOOLBAR: kind %04x desc=%p avail=%d btnvis=%d shadow=%p frames=%d "
                    "must=%d cant=%d\n",
                    nKindId, (void *)pDesc,
                    pDesc == NULL ? -1 : (int)CursorDesc_IsItemAvailableMaybe(pDesc),
                    pDesc == NULL ? -1 : (int)pDesc->bButtonVisible,
                    pDesc == NULL ? (void *)0 : (void *)pDesc->pShadowBitmap,
                    pDesc == NULL ? -1 : (int)pDesc->nButtonFrameCount,
                    pDesc == NULL ? 0 : (int)pDesc->nMustHaveKindId,
                    pDesc == NULL ? 0 : (int)pDesc->nCantHaveKindId);
#endif
        if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
            switch (nKindId) {
            case 0x2804:
                pLastHitNode = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            case 0x2802:
                pMenuItem0x2802CachedMaybe = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            case 0x2801:
                pBaseCandidateDown = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            case 0x2803:
                pMenuItem0x2803CachedMaybe = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            default:
                GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                break;
            }
        }
        nKindId++;
        nRemaining--;
    } while (nRemaining != 0);

    nCarouselScrollIndex = 0;
    g_worldBoard.MarkRectDirty(rect);

    switch (nCategory) {
    case 2:
    {
        int nTileId = 0x800;
        int nLeft = 500;
        do {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptorNoAlias(nTileId);
            if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
                GetOrCreateMenuIconItemMaybe(pDesc, 2, 0)->PlaceIconInGridMaybe();
            }
            nTileId++;
            nLeft--;
        } while (nLeft != 0);
        break;
    }
    case 3:
    {
        int nTileId = 0xc00;
        int nLeft = 400;
        do {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptorNoAlias(nTileId);
            if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
                GetOrCreateMenuIconItemMaybe(pDesc, 2, 0)->PlaceIconInGridMaybe();
            }
            nTileId++;
            nLeft--;
        } while (nLeft != 0);
        nTileId = 0x3000;
        nLeft = 100;
        do {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptorNoAlias(nTileId);
            if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
                GetOrCreateMenuIconItemMaybe(pDesc, 2, 0)->PlaceIconInGridMaybe();
            }
            nTileId++;
            nLeft--;
        } while (nLeft != 0);
        nTileId = 0x3400;
        nLeft = 100;
        do {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptorNoAlias(nTileId);
            if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
                GetOrCreateMenuIconItemMaybe(pDesc, 2, 0)->PlaceIconInGridMaybe();
            }
            nTileId++;
            nLeft--;
        } while (nLeft != 0);
        break;
    }
    case 4:
    {
        int nTileId = 0x1000;
        int nLeft = 400;
        do {
            CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptorNoAlias(nTileId);
            if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
                GetOrCreateMenuIconItemMaybe(pDesc, 2, 0)->PlaceIconInGridMaybe();
            }
            nTileId++;
            nLeft--;
        } while (nLeft != 0);
        break;
    }
    }

    bActive = true;
    if (g_BuildToolButton.rect.right > g_worldBoard.dwViewportWidth - rectViewport.right) {
        bSuppressRectBMaybe = true;
    } else {
        bSuppressRectBMaybe = false;
    }
    g_pActiveTabWidgetMaybe = this;
    LayoutMenuIconGridMaybe();
    return 1;
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 9 of 13: g_BuildToolButton (DAT_004aa5b8), BuildToolButton::BuildToolButton (0x449430).
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_g_BuildToolButton(void) {
    new (&g_BuildToolButton) BuildToolButton();
}
#endif // LOCO_PORT
