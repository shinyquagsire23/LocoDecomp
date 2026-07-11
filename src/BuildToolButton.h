// BuildToolButton -- the toolbar button, single global-static instance at 0x4aa5b8
// (ctor 0x449430, dtor 0x4494e0, own vtable 0x4782a8, 0x74c bytes; full slot map in
// docs/subsystems.md's widget-family table). Derives WidgetBaseObj0x4784c8 and embeds a 2nd
// AnimDescRefObj0x477488 icon at +0xe0 (ends +0x168), so the rect below at +0x168 is the
// button's own hit area, sitting just before regionAMaybe (+0x178). Layout cross-check:
// regionBMaybe (+0x260, a full WidgetPickerObj0x477cc8) ends at 0x260+0x4e0=0x740, landing
// exactly on nButtonStateMaybe (docs/subsystems.md's widget-family table).
#pragma once

#include "WidgetBase.h"   // WidgetBaseObj0x4784c8 base
#include "WidgetPicker.h" // WidgetPickerObj0x477cc8 (regionBMaybe)
#include "MenuNode.h"     // MenuNodeObj0x477568

// WidgetTagObj0x478378 (vtable 0x478378, 0xe8 bytes) -- a small WidgetBaseObj0x4784c8 leaf,
// so far only seen embedded as BuildToolButton's regionAMaybe (+0x178, the boundary against
// regionBMaybe at +0x260 pins its 0xe8 size). No methods of its own transcribed; only the
// vtable slots a consumer dispatches through are modeled. Slots 19/20/21 are declared here
// (the base model stops at slot 18) purely to land slot 21 at its real +0x54 offset. Lives
// in THIS header, not the shared WidgetBase.h family home: WidgetBase.h is included by the
// hyper-position-sensitive DPlaySessionMgr.cpp TU (its SelectGridCellFromPointMaybe EXACT
// match rotates on ANY content change there -- see src/NameAnchorMaybe.h's note), and
// BuildToolButton.h's only consumer is BuildToolButton.cpp. Hoist it when a second consumer
// appears.
class WidgetTagObj0x478378 : public WidgetBaseObj0x4784c8 {
public:
    // +0xe0/+0xe4 -- promoted from `pad0xe0[8]` when the ctor was transcribed (it stores a
    // dword zero to each, so they are two real fields, not a gap), then TYPED and named
    // 2026-07-27 from ActivateTab below: they are the last two of FOUR per-tool-kind menu-node
    // caches this class fills in its 0x2800..0x2809 populate pass. The other two reuse base
    // fields (pLastHitNode for 0x2804, pBaseCandidateDown for 0x2801), which is why only these
    // two needed new storage. Same convention as BuildToolButton's own
    // pMenuItem0x240cCachedMaybe below. No READER has been found for either yet.
    MenuNodeObj0x477568 *pMenuItem0x2802CachedMaybe; // +0xe0
    MenuNodeObj0x477568 *pMenuItem0x2803CachedMaybe; // +0xe4

    WidgetTagObj0x478378();           // 0x44e8d0, src/BuildToolButton.cpp
    virtual ~WidgetTagObj0x478378();  // 0x44e930, slot 0 override, src/BuildToolButton.cpp

    // slot 19 (+0x4c) -- 0x44ef10, the per-node hover test/toggle. Overrides the base's own
    // slot-19 declaration in src/WidgetBase.h.
    virtual char TestAndToggleMenuNodeHoverMaybe(MenuNodeObj0x477568 *pNode, int x, int y);
    // slot 10 (+0x28) override, 0x44ec50, src/BuildToolButton.cpp -- the region's per-frame
    // tick: re-localizes the placement cursor's last resolved position (scroll-adjusted),
    // dispatches every menu node through slots 19 (hover test) and 20 (execute), and drops
    // the Unk0xac latch when the cursor leaves the region's rect. Both dispatches are plain
    // named virtual calls since v576 retyped the base's slot-19 declaration (see WidgetBase.h);
    // they went through a TU-local vtable probe until then.
    virtual void AdvanceAnimFrameMaybe();
    // slot 20 (+0x50) -- 0x44ef70, unread. Overrides the base's own slot-20 declaration
    // (WidgetBase.h) rather than opening a new slot, which is what keeps ActivateNodeMaybe
    // below at its real +0x54.
    virtual char HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode);
    // slot 21 (+0x54), 0x44e940, src/BuildToolButton.cpp -- this leaf's exact counterpart to
    // WidgetPickerObj0x477cc8::ActivateTab (0x428400): same signature, same category
    // vocabulary (0 = close, 2/3/4 = open a build-palette category), same
    // pLastActivatedNode/g_pActiveTabWidgetMaybe bookkeeping. Named for that twin as of
    // 2026-07-27 (was the placeholder `ActivateNodeMaybe`); the second parameter's real type
    // is the same 16-bit category id, not the int the placeholder guessed.
    virtual unsigned char ActivateTab(MenuNodeObj0x477568 *pNode, unsigned short nCategory);
    // slot 16 (+0x40), 0x44f190, src/BuildToolButton.cpp -- chains the base handler, then
    // adds this region's own two arrow-key accelerators before falling back to the button.
    virtual bool OnKeyDownMaybe(unsigned int nKey);
    // slot 17 (+0x44), 0x44ed80, src/BuildToolButton.cpp -- the region's per-node CLICK
    // handler (overrides the base's abstract test-half). Range-checks x against the node's
    // shadow-frame width within rectViewport, scroll-adjusts x for carousel nodes
    // (wModeFlagsMaybe bit 2), then Contains-dispatches the node: 0x2801 (close) and
    // 0x2802/0x2803 (carousel arrows) just get pressed/armed; any OTHER id toggles the tool
    // selection (deselect-all + select + cursor type + sound, or plain deselect when the
    // node was already in state 3). Returns 1 only when the node consumed the click.
    virtual char HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y);
    // slot 22 (+0x58), 0x44ece0, src/BuildToolButton.cpp -- stamps pLastHitNode's sprite over
    // a 3x3 grid of 0x39-pitch cells anchored at (7,0x11) (the menu's backdrop), then redraws
    // every node on pMenuListHead. The vtable's last real entry (+0x5c is the null terminator
    // before the next class's table).
    virtual unsigned char LayoutMenuIconGridMaybe();
};

struct BuildToolButton : public WidgetBaseObj0x4784c8 {
    AnimDescRefObj0x477488 iconBMaybe; // +0xe0 -- 2nd embedded icon (ends +0x168)
    // +0x168 -- the button's own hit area, and (once the toolbar is open) the drag rect.
    // Modeled as a RECT because RepositionWithHotspot/AdvanceAnimFrameMaybe both build it with
    // SetRect in the closed state, and copy iconBMaybe's rect into it wholesale when open.
    RECT rectHitAreaMaybe;
    WidgetTagObj0x478378 regionAMaybe;    // +0x178 -- the tool-menu tag region
    WidgetPickerObj0x477cc8 regionBMaybe; // +0x260 -- the file-picker region (ends +0x740)
    // +0x740 -- WORD-sized, not the int the widget-family gap arithmetic originally implied.
    // Written by AppWndProc's `mov WORD PTR ds:0x4aacf8,1` in the in-game WM_KEYDOWN/VK_RETURN
    // case, right after SetDescriptorMaybe(0x2400,...) -- and READ by OnKeyDownMaybe below,
    // which gates its whole body on `== 3` (corrected 2026-07-31 when that body landed; this
    // note previously said "never read", from before 0x44adf0 was transcribed). So the button
    // assembly's open/close animation states 1/2/3 and this field are the same enum, which is
    // also what AdvanceAnimFrameMaybe's slide animation and BlitAllRegionsMaybe's "only in
    // state 3" icon blit are keyed on.
    short nButtonStateMaybe;
    short Unk0x742;                       // +0x742 -- the other half of the old int; unread
    // +0x744/+0x748 -- retyped 2026-07-14 (v66, see docs/subsystems.md): written once by
    // InitMenuIconsMaybe; the first is consumed by OnKeyDown (DEL/BACKSPACE toggles
    // auto-curve-connect mode), the second is write-only (no reader anywhere in .text).
    MenuNodeObj0x477568 *pAutoCurveConnectMenuItemMaybe; // +0x744
    MenuNodeObj0x477568 *pMenuItem0x240cCachedMaybe;     // +0x748

    BuildToolButton();
    virtual ~BuildToolButton();

    // Real vtable slot 2 override (0x449ce0) -- an 18-byte forwarder that does nothing but
    // tail-call the ROOT base's rect test, skipping AnimDescRefObj/WidgetBaseObj in between.
    // The hit-area test that reads like "Contains" lives at slot 21 (ContainsHitAreaMaybe).
    virtual char Contains(int x, int y);
    // Real vtable slot 3 override (0x449dc0). Repositions the whole button assembly: clamps
    // the requested (x,y) into the viewport (unless the toolbar is opening, state 1), chains
    // WidgetBaseObj0x4784c8::RepositionWithHotspot, drags iconBMaybe/regionAMaybe/regionBMaybe
    // along, rebuilds rectHitAreaMaybe, and -- if a drag is in flight -- warps the OS cursor
    // to keep the grab point under it.
    virtual void RepositionWithHotspot(int x, int y);
    // Real vtable slot 10 override (0x4497a0), 1113 bytes -- the per-frame tick. Chains the
    // AnimDescRefObj0x477488 base explicitly, runs the open/close slide animation (states
    // 1 and 2), services an in-flight drag, and ticks every menu node and sub-region.
    virtual void AdvanceAnimFrameMaybe();
    // Real vtable slot 15 override (0x4495b0) -- releases the two icons' descriptors and both
    // sub-regions before chaining WidgetBaseObj0x4784c8::ClearOwned. Shared by the dtor.
    virtual void ClearOwned();

    // Ordinary (non-virtual) member, 0x449c00 -- called only by WorldBoardMaybe's
    // FUN_00456700 at 0x456a40, which pushes the RECT by value (`ret 0x14` = 5 dwords).
    // Blits this widget's own anim frame, then forwards the same rect through slot 11
    // (+0x2c) to iconBMaybe (only in state 3) and to each active sub-region.
    void BlitAllRegionsMaybe(RECT rect, char bFlag);

    // Vtable slot 4 (0x44a0c0) -- this class's override of
    // WidgetBaseObj0x4784c8::TryInvokeCallbackA, and DECLARED as one since v549; it was an
    // ordinary member under the descriptive name `HitTestMaybe` until then, which was the
    // "one function modeled as two" hazard. The signature is not "unrelated" to the slot at
    // all -- char(int, int) is exactly what the slot carries the whole way up the family.
    //
    // The rename is byte-free because the sole call site
    // (PlacementCursorMaybe.cpp: `g_BuildToolButton.TryInvokeCallbackA(...)`) invokes it on a
    // GLOBAL OBJECT rather than through a pointer, which C++ resolves statically -- the same
    // reasoning WorldActionCursor.h's slot-2 Contains note already relies on.
    //
    // The mouse-down/hit-test handler: drops an in-flight drag, else GRABS one (storing the
    // cursor's offset within rect into the base's nDragGrabOffsetX/YMaybe) when slot 21 hits,
    // else routes to the sub-regions.
    virtual char TryInvokeCallbackA(int x, int y);

    // Ordinary (non-virtual) member, 0x44a9d0 -- the toolbar button press/release handler,
    // driven from AppWndProc's mouse dispatch. EXACT MATCH (432 B). src/Main.cpp called it
    // through a local BuildToolButtonWndProcView0x4618c0 view struct until 2026-07-26; that
    // view is retired in favour of this declaration.
    void OnPressReleaseMaybe(char bPressed);

    // Ordinary (non-virtual) member, 0x44ab80 -- drops the toolbar back to its resting state:
    // deactivates whichever of the two sub-regions is live, clears bSuppressRectBMaybe, leaves
    // auto-curve-connect mode, and walks the menu list returning each TRACK-family icon
    // (resource ids 0x2403..0x2406 and 0x2409..0x240a) to state 1. Ghidra renders it __fastcall
    // because it takes no argument beyond `this`.
    void ResetAndCloseToolMenuMaybe();

    // Real vtable slot 16 override (0x44adf0, declared-only) -- the toolbar button's own key
    // handler, and the last-resort fallback WidgetTagObj0x478378::OnKeyDownMaybe above hands
    // an unclaimed key to. Declared so that call resolves to THIS body rather than silently
    // devirtualizing to the WidgetBaseObj0x4784c8 base's.
    virtual bool OnKeyDownMaybe(unsigned int nKey);

    // Ordinary (non-virtual) member, 0x449600 -- one-shot toolbar construction: loads the
    // 0x2400/0x2402 descriptors, then walks TileKind ids 0x2400..0x2413 creating a menu icon
    // node for each available one (caching the 0x2406 and 0x240c nodes), and finally arms the
    // closed-state visuals. Transcribed 2026-07-26, content-complete; EFFECTIVE (see the
    // body's own autopsy plate and docs/PARKED.md).
    char InitMenuIconsMaybe();

    // Vtable slot 17 (0x44a250) -- this class's override of
    // WidgetBaseObj0x4784c8::HitTestNodeSecondary, and declared as one since v549. It was an
    // ordinary member named `DispatchMenuItemClickMaybe` until then, on the reasoning that
    // keeping the behavioral Ghidra name "avoids an override-name clash with the base's
    // slot-17 HitTestNodeSecondary" -- but that clash IS the model: one slot, one virtual, one
    // name. The signature was already char(MenuNodeObj0x477568 *, int, int), identical to the
    // base's, so this is a rename plus `virtual`, not a new declaration, and the function has
    // no call site anywhere in src/ to re-dispatch. Measured byte-free repo-wide.
    //
    // The toolbar menu-item click handler: gates on the node's visibility/hit-test, then
    // switches over the node's icon resource id (0x2403..0x240e).
    virtual char HitTestNodeSecondary(MenuNodeObj0x477568 *param_1, int param_2, int param_3);

    // slot 19 (+0x4c) -- the shared ConstFalsePredicateStubMaybe (0x44ef00, `xor al,al;
    // ret 0xc`) on this class; declared only to position slot 20.
    virtual char TestAndToggleMenuNodeHoverMaybe(MenuNodeObj0x477568 *pNode, int x, int y);
    // slot 20 (+0x50), 0x44ac20 -- the menu-node tick/repeat handler (countdown +
    // availability re-stamp switch over ids 0x2407..0x240f), dispatched virtually by
    // DispatchMenuItemClickMaybe's 0x240c case with the clicked node. Named for the base's
    // own slot-20 declaration (WidgetBase.h) so that it OVERRIDES that slot instead of
    // opening a new one -- which is what keeps ContainsHitAreaMaybe at its real +0x54.
    // Transcribed v499; PARTIAL/EFFECTIVE (see the body's autopsy plate).
    virtual char HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode);
    // slot 21 (+0x54), 0x449d80 -- 2D bounding-box test against rectHitAreaMaybe. A NEW slot,
    // not an override of the base's slot-2 Contains (slot 2 is separately overridden above), so
    // it needs its own name; it was modeled as `Contains` until 2026-07-25, when reading slot 2
    // showed the two are distinct functions. AL-only return, no EAX-wide clear.
    virtual char ContainsHitAreaMaybe(int x, int y);
    // slot 22 (+0x58), 0x449d00 -- "is (x,y) over ANY part of the button": slot 2, then slot 21,
    // then each active sub-region's own slot 2. The vtable's last real entry (+0x5c is a null
    // terminator before the next class's table at 0x478308).
    virtual char ContainsAnyRegionMaybe(int x, int y);
};
extern BuildToolButton g_BuildToolButton; // DAT_004aa5b8 -- the one static instance
// DAT_004fd3e0 -- "the widget currently owning the active tab / the keyboard". Typed as the
// shared family base: five different leaves write it (SelectedObjWidgetMaybe::SelectObjMaybe,
// WorldActionCursor::SelectDecorObjAndDispatchModeMaybe, WidgetPickerObj0x477cc8::ActivateTab,
// WidgetTagObj0x478378's 0x44e940, and this class's own tick/InitMenuIconsMaybe), and its two
// readers (AppWndProc's key dispatch through slot 16, MenuNodeObj0x477568::Draw's owner test)
// only ever use base-level members. Declared HERE rather than in the family home WidgetBase.h
// for the same reason WidgetTagObj0x478378 above is -- see that class's note. src/MenuNode.cpp
// and src/WidgetPicker.cpp still carry their own `void *` views (tagged debt); fold them onto
// this declaration when a session is already touching those TUs.
extern WidgetBaseObj0x4784c8 *g_pActiveTabWidgetMaybe;
