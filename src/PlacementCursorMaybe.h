// PlacementCursorMaybe (DAT_004854c8) -- the mouse-driven drag/placement controller singleton
// (docs/subsystems.md's "Front-end screens" entry, 0x11c bytes). ONE canonical definition:
// every consumer includes THIS header, rather than each TU declaring its own divergent local
// struct (see CLAUDE.md's "never duplicate a struct across TUs" rule, 2026-07-17).
//
// PROMOTED 2026-07-25 from the old flat `PlacementCursorPartial` (pads + a handful of
// offsets) to the real class: it derives AnimDescRefObj0x477488 (src/WidgetBase.h, 0x88
// bytes), so what used to be `bReadyMaybe`/`pKindDesc` here are literally that base's own
// `bReady`/`pKindDesc` members now. The old view deferred this because folding pulls
// WidgetBase.h into consumers that didn't have it (the v333/v334 include-set rotation
// lesson); the fold happened when this TU (src/PlacementCursorMaybe.cpp) was written, since
// the class's own methods cannot be spelled against a padded view.
//
// ⚠ DECLARATION-COUNT DIAL (docs/CODEGEN.md, v380/v399): this class's member-function
// declaration COUNT is a whole-TU tie-break knob for every TU that includes this header.
// Do NOT trim the method set to chase a byte -- model the class correctly and measure.
#pragma once

#include "WidgetBase.h"             // AnimDescRefObj0x477488 -- the direct base
#include "Obj0x477798Family.h"      // Obj0x477758 -- the embedded +0x10c collection

class DecorActorBase; // src/DecorActor.h -- pHoverObjMaybe's pointee, see its note below

class PlacementCursorMaybe : public AnimDescRefObj0x477488 {
public:
    int nTypeIdMaybe;            // +0x88 -- last type id pushed through SetTypeMaybe
    char Unk0x8c;                // +0x8c -- no reader/writer anywhere in .text
    bool bCustomCursorShownMaybe; // +0x8d -- the busy (hourglass) cursor is the active one
    bool bMouseMovePendingMaybe; // +0x8e -- a WM_MOUSEMOVE arrived and has not been consumed
    // +0x90 -- that message's own packed lParam (x in the low word). UNSIGNED, like all four
    // packedPendingPos* below: every reader unpacks the high half with `>> 16` and the original
    // emits `shr`, not `sar` (corrected 2026-07-26, from AdvanceAnimFrameMaybe's two unpacks).
    unsigned int packedMousePosMaybe;
    int screenPosXCacheMaybe;    // +0x94
    int screenPosYCacheMaybe;    // +0x98
    int lastResolvedPosX;   // +0x9c
    int lastResolvedPosY;   // +0xa0
    // Four (pending flag, packed position, resolved x, resolved y) quartets, one per mouse
    // action the window procedure defers to the placement cursor's own tick: A = left down /
    // left double-click, B = right down / right double-click, C = left up, D = right up.
    bool bPendingActionAMaybe;   // +0xa4
    unsigned int packedPendingPosAMaybe;  // +0xa8
    int resolvedPosAX;      // +0xac
    int resolvedPosAY;      // +0xb0
    bool bPendingActionBMaybe;   // +0xb4
    unsigned int packedPendingPosBMaybe;  // +0xb8
    int resolvedPosBXMaybe; // +0xbc
    int resolvedPosBYMaybe; // +0xc0
    bool bPendingActionCMaybe;   // +0xc4
    unsigned int packedPendingPosCMaybe;  // +0xc8
    int resolvedPosCXMaybe; // +0xcc
    int resolvedPosCYMaybe; // +0xd0
    bool bPendingActionDMaybe;   // +0xd4
    unsigned int packedPendingPosDMaybe;  // +0xd8
    int resolvedPosDXMaybe; // +0xdc
    int resolvedPosDYMaybe; // +0xe0
    bool bSnapLockMaybe;         // +0xe4
    bool bFlagE5Maybe;           // +0xe5
    bool bFlagE6Maybe;           // +0xe6 -- left button is down (set on down, cleared on up)
    bool bFlagE7Maybe;           // +0xe7 -- a real field, not padding: the ctor zeroes it
                                 // explicitly. No other reader/writer found yet.
    // +0xe8/+0xec -- the object the cursor is currently hovering/carrying. Every DecorActorBase
    // leaf's dtor clears itself out of pHoverObjMaybe (src/DecorActor.cpp), and the actors'
    // schedule tick skips them entirely while they are the active hover target -- i.e. picking
    // an actor up freezes it.
    //
    // NARROWED 2026-07-26 from AnimDescRefObj0x477488* to DecorActorBase*, when this TU's own
    // methods were transcribed and pinned it: the tick dispatches slot 21 (CanStandAtMaybe) on
    // it, SetHoverObjMaybe/CommitPendingCoupleMaybe dispatch slot 17 (HeadForObjectMaybe), and
    // three sites read/write +0x88 clamped to 0..7 (nMoodMaybe) and +0xa0
    // (dwNextDecisionTickMaybe) -- all four are DecorActorBase's own, none exist on the widget
    // base. The category-7/8 registry bookkeeping in ReleaseHoverObjMaybe says the same thing
    // from the other side: only the two actor leaves are ever hovered. Forward-declared rather
    // than #included, so the eight other consumers of this header keep their include sets.
    DecorActorBase *pHoverObjMaybe; // +0xe8
    bool bHoverActiveMaybe;      // +0xec
    // +0xf0 / +0xfc -- a deliberate save/disable/restore triple, NOT a reproduced bug: the ctor
    // caches the user's real OS mouse-accel setting into mouseSpeedParamsMaybe via
    // SystemParametersInfoA(SPI_GETMOUSE), then pushes a zeroed mouseAccelDisableMaybe via
    // SPI_SETMOUSE to disable acceleration during play; ShutdownMaybe pushes the CACHED value
    // back to restore it on exit.
    int mouseSpeedParamsMaybe[3];  // +0xf0
    int mouseAccelDisableMaybe[3]; // +0xfc
    HCURSOR hBusyCursorMaybe;      // +0x108 -- lazy LoadCursorFromFileA cache, post\CURSORS\busy.ani
    // +0x10c -- a 10-slot instance of the shared Obj0x477798-family collection, reserved by
    // this class's own ctor and freed by its dtor. Contents unread so far.
    Obj0x477758 embeddedCollectionMaybe;

    PlacementCursorMaybe();
    virtual ~PlacementCursorMaybe();          // slot 0 -- 0x410680 (thunk 0x410660)
    // slot 10 (+0x28) -- 0x410840, this class's ONLY non-dtor vtable override (verified by
    // dumping 0x477718 against the base's 0x477488: every other slot is inherited verbatim,
    // and the class adds no new slots). The base's slot 10 advances an anim frame; here it is
    // the per-frame placement/drag tick, servicing the mouse-move flag and the four
    // pending-action quartets above. An override must share the base's NAME, which is why
    // this is not called TickMaybe.
    virtual void AdvanceAnimFrameMaybe();

    // 0x410700 -- called once, from SaveWindowAndCleanExit: restores the cached OS mouse
    // setting, drops the embedded collection, releases capture, and clears the descriptor.
    void ShutdownMaybe();
    // 0x410750 -- warms the sound bank: loads and immediately releases the four placement
    // sound effects (0x5015/0x5014/0x501a/0x501b) so the first real play doesn't stall.
    bool PreloadPlacementSoundsMaybe();
    // 0x411dc0 -- show/hide + SetCapture/ReleaseCapture + custom-vs-system-cursor toggle,
    // called from essentially every app-state transition to release the drag tool whenever
    // focus leaves the build screen.
    void SetCursorCapture(bool bCapture, bool bShowCursorAfter, bool bBusyCursor);
    // 0x410a20 -- consumes packedMousePosMaybe: re-resolves the cursor's board position from
    // the packed point just stored there. BuildToolButton::RepositionWithHotspot calls it right
    // after warping the OS cursor, so the placement cursor's own idea of where the mouse is
    // does not lag a frame behind the warp.
    void FUN_00410a20();
    // 0x411760 -- app-state-keyed cursor dispatcher, also inlined into the tick's tail.
    void UpdateCursorForAppStateMaybe();
    // 0x411fb0 -- switches the dragged type (no-op if unchanged), reloads the descriptor,
    // shows the matching cursor bitmap via slot 6, repositions via RepositionWithHotspot.
    void SetTypeMaybe(int nTypeId);
    // 0x411230 -- services pending-action slot B (right button): rotates/cycles the dragged
    // piece when the click is on it, else offers the click to actors, trains and the board.
    void CommitPendingRotateMaybe();
    // 0x411c50 / 0x411d10 -- the two paint entry points: blit the ghost sprite (and the hover
    // object's highlight) clipped to an explicit dirty rect, or to the cursor's own `rect`.
    void FUN_00411c50(RECT rcClip, char flag);
    void FUN_00411d10();

    // ---- declared-only (bodies not yet transcribed; addresses for the next session) ----
    unsigned char OnMouseMoveMaybe();          // 0x410a40
    void CommitPendingCoupleMaybe();           // 0x411000
    // 0x410d20 -- rebuilds the auto-curve tool's highlighted-tile set each tick: retracts
    // last tick's highlights (for tiles still live in g_gameWindowWidgetList), then re-stamps
    // either the single top slot under the cursor (connect mode 1) or the dragged kind's whole
    // footprint mask (mode 2) with the 0x400 highlight overlay id.
    void RefreshFootprintHighlightMaybe();
    // 0x4113a0 -- sets pHoverObjMaybe to pObj (NULL to clear), releasing whatever was hovered
    // before and deregistering the adopted actor from whichever of DecorObjMgrMaybe's two
    // per-category registries (7 = people, 8 = vehicles) it is in. Returns 1 when it adopted
    // a real object, 0 when it only cleared.
    char SetHoverObjMaybe(DecorActorBase *pObj);
    void ReleaseHoverObjMaybe();               // 0x411580
    void SelectCursorTypeAutoCurveMaybe();     // 0x4117b0
    void SelectCursorTypeTilePlacementMaybe(); // 0x411ae0
    // 0x412060 -- world-scroll-adjusts then clips (x,y) to the board grid extent, returning
    // the clamped point by value.
    POINT ClampToGridBoundsMaybe(int x, int y);
};
extern PlacementCursorMaybe PlacementCursorMaybe_004854c8; // DAT_004854c8
