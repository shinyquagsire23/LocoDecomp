// WorldActionCursor (vtable 0x478548, singleton DAT_004a9ef0) -- the world-action
// cursor/overlay widget leaf of the WidgetBaseObj0x4784c8 family: a contextual on-screen
// cursor showing different feedback icons per interaction mode (nModeMaybe/+0x398; confirmed
// modes 3 = tile placement, 6 = train coupling), owning the decor-placement candidate menu
// icons and the train attach/detach/spawn command set. Full behavior writeup in
// docs/subsystems.md (RESOLVED 2026-07-13 v54/v55). RENAMED 2026-07-26 (v430) off the
// Obj0x<vtable-addr> placeholder convention -- the behavior has been resolved since v55 (all 22
// vtable slots read), so the class earned a real name; the singleton went with it
// (WidgetObj0x478548_004a9ef0 -> g_worldActionCursor). docs/PICKUPS_PRIOR.md is a chronological
// archive and deliberately still says WidgetObj0x478548. The members needed by the transcribed
// methods (HitTestNodeSecondary/HandleMenuCommandMaybe, slots 17/20,
// SelectDecorObjAndDispatchModeMaybe, and RefreshTrainCouplingMenuMaybe) are modeled; the
// rest of the class (ctor, most of the 21 other vtable slots) is out of scope.
#pragma once

#include "MenuNode.h"   // MenuNodeObj0x477568 + the WidgetBaseObj0x4784c8 base chain
#include "WindowBase.h" // CenterRectInRect

// SelectedObjWidgetMaybe (DAT_004852a0) -- the sibling widget-family leaf owning the
// "currently selected world object" (docs/subsystems.md's own entries for it). Its
// method run is 0x42cd60..0x42d670: LoadCategoryIconsMaybe (0x42ce10), SelectObjMaybe
// (0x42d040), the slot-10 follow-tick AdvanceAnimFrameMaybe (0x42d1a0, EFFECTIVE-parked),
// the blit pair (0x42d280 EFFECTIVE-parked / 0x42d3a0), RepositionWithHotspot (0x42d440),
// TryInvokeCallbackA (slot 4, 0x42d670) and the dtor (0x42cd80 + ??_G 0x42cd60) are
// transcribed; CanSelectDecorObjMaybe (0x42cf90) is still a TU-local free function (see the
// .cpp for why). ClearOwned (slot 15, 0x42cdd0) and the whole vtable tail (slot 17 0x42d6b0,
// slot 20 0x42d770, slot 21 0x42d400) were folded onto this class in v550 -- the TU-local
// view struct that used to carry them is gone.
//
// ⚠ THIS HEADER'S DIAL is a STAIRCASE, and every reading of it so far has been the local shape
// of one step. Priced v552 and CURRENT: the blit pair's two declarations below cost
// TilePlacedObj::SpawnSeqRecordEffectMaybe (0x4588b0) its 143 B -- and ONE declaration costs
// exactly the same 143 B as TWO (measured both ways, repo total identical at 172151 B), so the
// step was paid entirely by the FIRST. Only src/TilePlacedObj.cpp moves; the header's seven
// other consumers are byte-identical. That is the same shape v550 saw one step lower down
// (1, 2 and 3 declarations all costing a flat 60 B on 0x458310, which is still spent) -- so
// v550's "THRESHOLD, saturated, nothing left to lose" reading was the step, not the staircase.
// The v506 "exactly one declaration / 143 B" and v533 "no longer binding" readings were each
// true when measured too. ⇒ RE-PRICE every time, and since a step is flat once entered, take
// EVERY declaration you want the moment you decide to enter one (v552 took the pair, v550 took
// slots 20/21 together). Slots 18/19 are the shared ICF-folded const stubs 0x44ef00/0x42d760.
//
// PROMOTED 2026-07-26 from the old `pad0x0[0x88]; bool bActive; pad0x89[..]` flat view to a
// real WidgetBaseObj0x4784c8 leaf (that base is exactly 0xe0 bytes, so pSelectedObjMaybe
// keeps its offset). Forced by PlacementCursorMaybe::SelectCursorTypeTilePlacementMaybe,
// which calls the root's own `Contains` on this singleton: through the pad that was a raw
// cast (lint class D/F), through the real base chain it is an ordinary inherited call.
class SelectedObjWidgetMaybe : public WidgetBaseObj0x4784c8 {
public:
    // +0xe0 (DAT_00485380) -- the world object SelectObjMaybe last selected. AppWndProc's
    // WM_USER+1 command 0xb re-drives WorldActionCursor::SelectDecorObjAndDispatchModeMaybe
    // with it after detaching a train from the board -- which is what types it: that method's
    // own parameter is an AnimDescRefObj0x477488*, dereferenced for ->pKindDesc.
    AnimDescRefObj0x477488 *pSelectedObjMaybe;
    // +0xe4 -- an embedded second anim/descriptor component of the SAME family, whose rect is
    // the widget's secondary on-screen footprint: WorldActionCursor::ClampRectIntoViewMaybe
    // tests the action cursor against BOTH this rect (0x48538c) and the widget's own
    // (0x4852a8) before shoving itself into the free corner. Modeled 2026-07-27 (v450) from
    // that one call site -- Ghidra's own struct already carried the field.
    //
    // ⚠ PRICE, measured v450: this ONE member declaration costs
    // TilePlacedObj::GetFrontRowTilePosMaybe (0x458310) its 60 B -- the same
    // sub-edx-eax/sub-eax-edx coin flip v442-v448 kept paying to this header family, and the
    // exact 60 B v448 had just won back. src/TilePlacedObj.cpp is the ONLY one of this
    // header's eight consumers that moves. Kept because 0x45a480 alone is +126 B. Declaration
    // POSITION was probed (here vs. after the class's methods) and makes no difference. So
    // this header's dial is benign for METHOD declarations (v448) but knife-edged on 0x458310
    // for MEMBER additions -- price any further one. See docs/PARKED.md's v450 section.
    AnimDescRefObj0x477488 animDescMaybe;
    // +0x16c -- the selected object's mode word: SelectObjMaybe sets it to the selected
    // descriptor's categoryByte (0 when pKindDesc is null, and 0 again on deselect); the
    // category-icon node state follows (nModeMaybe == 6). Same role as WorldActionCursor's
    // own nModeMaybe (+0x398 there).
    unsigned short nModeMaybe;
    unsigned short pad0x16e; // +0x16e -- never observed in .text
    // +0x170/+0x174/+0x178 -- the widget's three category-icon menu nodes (TileKinds
    // 0x3807/0x3808/0x3806), built by LoadCategoryIconsMaybe; the first two are ALSO cached
    // straight into the base's pBaseCandidateUp (+0xd8) / pBaseCandidateDown (+0xdc) as they
    // are created.
    MenuNodeObj0x477568 *pCategoryIconNode0x3807Maybe;
    MenuNodeObj0x477568 *pCategoryIconNode0x3808Maybe;
    MenuNodeObj0x477568 *pCategoryIconNode0x3806Maybe;
    // +0x17c -- the icon-toolbox canvas LoadCategoryIconsMaybe allocates once BOTH of its
    // SetDescriptor loads (0x3805 on the widget, 0x3804 on animDescMaybe) succeed, sized to
    // animDescMaybe's realized descriptor bitmap.
    LocoBitmap *pIconToolboxBitmapMaybe;
    // +0x180 -- that canvas's own rect (SetRect to 0,0,width,height right after the fill).
    RECT rectIconToolboxMaybe;
    // +0x190/+0x194 -- the world point the follow-tick last repositioned the widget to (the
    // selected object's rect center); recomputed every tick and slot 3 only re-fires when
    // it moved.
    int nFollowCenterXMaybe;
    int nFollowCenterYMaybe;

    // 0x42ce10, src/WorldActionCursor.cpp -- the cold-start bring-up of the widget's
    // category-icon toolbox: loads/creates the three category menu nodes, then loads the
    // two toolbox descriptors (0x3805 own, 0x3804 companion) and, only when both take,
    // allocates + fills the icon canvas bitmap. Ordinary (non-virtual) member -- confirmed
    // absent from the class's vtable run at 0x477d30; reached by direct call from the
    // world-load thread only. Returns whether both descriptors loaded.
    char LoadCategoryIconsMaybe();

    // 0x42d040, src/WorldActionCursor.cpp -- selects the given world object (NULL/ungated =
    // clear the selection). Select path: gates on in-game + CanSelectDecorObjMaybe +
    // !g_bCmdlineSFlagSet, sets bActive, adopts the descriptor's categoryByte as nModeMaybe,
    // repositions (slot 3) over the object's rect center, drives the 0x3806 category node's
    // state, dirty-marks, then forwards to WorldActionCursor::
    // SelectDecorObjAndDispatchModeMaybe. Deselect path mirrors that method's own deselect
    // tail (clears bActive/mode, hands g_pActiveTabWidgetMaybe back to the action cursor
    // when IT is active). Returns bActive. The `int` param spelling is load-bearing for the
    // callers (they pass `(int)pObj` / `0`); the body casts it back.
    char SelectObjMaybe(int nObj);
    // Real vtable slot 10 (0x42d1a0, per the vtable dword at 0x477d58) -- this class's own
    // override of the base's AdvanceAnimFrameMaybe, repurposed as the per-frame
    // follow-the-selection tick. Body in src/WorldActionCursor.cpp.
    virtual void AdvanceAnimFrameMaybe();
    // 0x42d280 / 0x42d3a0, src/WorldActionCursor.cpp -- the paint-pass blit pair, both real
    // members since v552. Both take the flushed dirty RECT by value and both ignore it (the
    // signature is the paint pass's, not the bodies'); see the definitions' own comments for
    // what they do.
    void BlitIconToolboxClippedMaybe(RECT rect);
    void BlitIconToolboxMaybe(RECT rect);
    // Real vtable slot 3 (0x42d440, per the vtable dword at 0x477d3c) -- this class's own
    // reposition override, reached only through the slot (no code xrefs). Keeps the widget
    // and its companion animDescMaybe icon straddling a world point, MIRRORING the whole
    // assembly to the other side of that point when it would otherwise run off the scrolled
    // board edge. bSuppressRectBMaybe is the mirrored/not-mirrored latch here (this class's
    // own use of that inherited flag), and flipping it also re-lays every menu node's rect
    // and re-dispatches slot 7 on both objects.
    virtual void RepositionWithHotspot(int x, int y);
    // Vtable slot 4 (0x42d670), this class's override of
    // WidgetBaseObj0x4784c8::TryInvokeCallbackA -- declared as one since v549 (it was the
    // descriptive non-virtual `HitTestConsumeMaybe` until then). Byte-free for the same reason
    // as the slot-2 Contains note below: the one call site invokes it on the global object
    // SelectedObjWidgetMaybe_004852a0, which C++ resolves statically.
    //
    // The click half of the (Contains, TryInvokeCallbackA) pair: dispatches the click at (x, y)
    // into whichever of this widget's menu nodes owns the point, and returns non-zero when the
    // widget consumed it. An in-progress DRAG short-circuits it -- the release IS the click, so
    // it ends the drag and consumes without routing anywhere.
    virtual char TryInvokeCallbackA(int x, int y);
    // Vtable slot 15 (0x42cdd0), this class's override of
    // WidgetBaseObj0x4784c8::ClearOwned -- declared as one since v550 (it was the TU-local
    // free function `SelectedObjWidgetMaybe_CloseMaybe`, declared file-locally in
    // src/AppWindow.cpp, until then). Byte-free for the same reason as the slot-4 note above:
    // the one call site invokes it on the global object SelectedObjWidgetMaybe_004852a0,
    // which C++ resolves statically.
    //
    // Close/tear-down for the selection widget: deletes the icon-toolbox canvas if one was
    // allocated, unloads BOTH descriptors (the companion animDescMaybe's first, then the
    // widget's own) with the (0,-1,0) "clear" triple, and finally chains the base's
    // ClearOwned. Body in src/WorldActionCursor.cpp.
    virtual void ClearOwned();
    // Vtable slot 17 (0x42d6b0), this class's override of
    // WidgetBaseObj0x4784c8::HitTestNodeSecondary -- the TEST half of the per-node callback
    // family. Was the ordinary member `TestMenuCommandMaybe` on the TU-local
    // SelectedObjWidgetMaybeView0x42d400 until v550; the signature already matched the base's
    // exactly, so the fold is a pure rename. No call site anywhere in src/.
    virtual char HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y);
    // Vtable slot 20 (0x42d770), this class's override of
    // WidgetBaseObj0x4784c8::HandleMenuCommandMaybe -- the EXECUTE half to slot 17's test.
    // Also folded off the TU-local view in v550; it already carried the base's name there.
    virtual char HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode);
    // Vtable slot 21 (0x42d400) -- this class's OWN new virtual, past the base's slot-20 tail:
    // hides the widget's child icon when the base flag is set, over the given tile rect. The
    // one call site is src/WorldBoardMaybe.cpp's tile paint pass, which still reaches it
    // through that TU's own SelectedObjWidgetPaintView0x456700 spelling (pre-existing debt --
    // that TU carries a full duplicate pad-model of this class and cannot include this header
    // without risking its knife-edged 0x457ce0; unchanged by this fold).
    virtual void HideChildIfBaseFlagMaybe(RECT rect, int bFlag);
    // 0x42cce0, src/WorldActionCursor.cpp -- the singleton's ctor, run from the
    // atexit-registered ctor/dtor thunk pair at 0x45c710/0x45c730. Chains the base ctor and
    // default-constructs animDescMaybe (its declared -1,-1,0,0 defaults ARE what the original
    // pushes), then stamps the class's own type tag and clears the selection/canvas pair.
    SelectedObjWidgetMaybe();
    // vtable slot 0 -- 0x42cd80 (??_G scalar deleting dtor 0x42cd60 is a free byproduct).
    // Empty body: the vtable re-stamp, animDescMaybe's destruction and the base chain are all
    // compiler-generated. Body in src/WorldActionCursor.cpp.
    virtual ~SelectedObjWidgetMaybe();
};
extern SelectedObjWidgetMaybe SelectedObjWidgetMaybe_004852a0; // DAT_004852a0

// Methods-only vtable probe for the decor-category manager sub-object at DAT_00485494
// (DecorObjMgrMaybe_00485448 + 0x4c, per Ghidra's own struct read of the 0x2c09/mode-7
// call site) -- own vtable, slot 8 (+0x20) maps a visible candidate slot to a selectable
// decor object id. No data layout known/needed (CarNetObjVtblProbe precedent).
struct DecorCategoryMgrVtblProbe {
    virtual void *_v00(); virtual void *_v04(); virtual void *_v08(); virtual void *_v0c();
    virtual void *_v10(); virtual void *_v14(); virtual void *_v18(); virtual void *_v1c();
    virtual int GetCategoryObjByIndexMaybe(int nIndex); // vtbl+0x20
};
extern DecorCategoryMgrVtblProbe DecorCategoryMgrMaybe_00485494; // DAT_00485494 // TODO: sync
                                                    // (Ghidra models the object as the field path
                                                    //  DecorObjMgrMaybe_00485448.regCategory7Maybe;
                                                    //  needs a standalone rename pushed)

// CenterRectInRect (0x425a50) -- __cdecl, recenters the inner rect inside the outer one in
// place; canonical declaration and body in src/WindowBase.h / src/WindowBase.cpp, which this
// header now includes for it. Until v446 this line instead declared a LOCAL alias,
// `void WindowBase_CenterRectInRect(int *, int *)`, and the inline helper below called THAT --
// a different mangled symbol from the real function, i.e. a call to a target that does not
// exist anywhere in the project. It byte-matched anyway because verify.py masks relocations,
// which is exactly the byte-invisible wrong-call-target class v445 found at 0x411fb0. A
// file-local extern with a renamed spelling is a class-I idiom finding for this reason, not
// only for the type-drift one.
extern const int g_anDecorCenterBoundsMaybe[4]; // DAT_00478538 -- the constant outer rect
                                                //   every mode case centers within // TODO: sync

class WorldActionCursor : public WidgetBaseObj0x4784c8 {
public:
    // +0xe0: another AnimDescRefObj0x477488 sub-icon (Ghidra's own struct already has this
    // typed; the pad it replaces was stale) -- InitTrainCouplingMenuIconsMaybe reads only its
    // own SetDescriptor(0x2402,-1,0) result, folded into that method's combined "all ready"
    // return value.
    AnimDescRefObj0x477488 animMaybe0; // +0xe0 .. +0x167
    // +0x168: four AnimDescRefObj0x477488 sub-icons (0x88 stride) -- the variant-selector
    // row driven by the 0x3868/0x3869 prev/next commands and read by the 0x3866/0x3867
    // train scratch-build.
    AnimDescRefObj0x477488 animArrayMaybe[4]; // +0x168 .. +0x387
    int anTrainSelScratchMaybe[4]; // +0x388 -- rebuilt from animArrayMaybe's anim-value
                                   //   caches on every 0x3866/0x3867 (train rebuild/spawn)
    unsigned short nModeMaybe;     // +0x398 -- interaction mode (3/6/7 observed); really the
                                   //   selected decor descriptor's categoryByte (modes 2..0xc,
                                   //   SelectDecorObjAndDispatchModeMaybe's switch)
    unsigned short nModePrevMaybe; // +0x39a -- previous nModeMaybe, stashed on select; a mode
                                   //   change clears bAttachMenuToggleMaybe
    bool bAttachMenuToggleMaybe;   // +0x39c -- set by command 0x3803 (wState 1 -> false,
                                   //   2 -> true) before RefreshTrainCouplingMenuMaybe
    bool bAttachPendingMaybe;      // +0x39d -- set true when a 0x3866/0x3867 command
                                   //   arrives while animMaybe6 is not ready
    unsigned char pad0x39e[2];     // +0x39e .. +0x39f, unmodeled
    // +0x3a0: the mode-feedback AnimDescRefObj0x477488 sub-icon -- SelectDecorObjAndDispatch
    // ModeMaybe SetDescriptor's it to the selected decor's resourceId+1 and centers it over
    // the widget rect; its pDSoundChannel is released on every reselect.
    AnimDescRefObj0x477488 animMaybe5;       // +0x3a0 .. +0x427
    // +0x428: another AnimDescRefObj0x477488 sub-icon; only its bReady (+0x44c) is
    //   observed here, gating the 0x3802/0x3866/0x3867 paths.
    AnimDescRefObj0x477488 animMaybe6;       // +0x428 .. +0x4af
    // +0x4b0: another AnimDescRefObj0x477488 sub-icon (Ghidra's animMaybe7); only its
    //   bReady (+0x4d4) is observed, set to (nModeMaybe == 6) by RefreshTrainCouplingMenuMaybe.
    AnimDescRefObj0x477488 animMaybe7;       // +0x4b0 .. +0x537
    // +0x538: the currently-targeted decor/world object (docs/subsystems.md v61: retyped
    // AnimDescRefObj0x477488*; aliased with SelectedObjWidgetMaybe's own selection). Read
    // here as an ARRAY of embedded AnimDescRefObj0x477488-laid-out slots: [2].rect.right
    // (+0x120) is the attached PeerTrainNode, [8].rect.top (+0x44c) the selected-car train.
    AnimDescRefObj0x477488 *pSelectedDecorObjMaybe; // +0x538
    // +0x53c -- base index of the currently-shown decor candidate page. UNSIGNED, and
    // this is the discriminating site: RefreshDecorCategoryCandidatesMaybe's arrow-greying
    // test compiles to `test eax,eax; jbe`, and only an unsigned `> 0` produces jbe here
    // (a signed `> 0` gives jle, and `== 0` gives je).
    unsigned int nCandidateBaseMaybe; // +0x53c -- base index of the currently-shown decor
                                   //   candidate page (cycled +-1 by 0x2c07/0x2c08)
    // +0x540: the menu node tracking the "current candidate" itself -- HitTestNodeSecondary
    // compares the tested node against it and flips its bTextRedrawEnabled (a UiIconListItem
    // leaf field at +0x58, hence the leaf pointer type).
    UiIconListItem *pActiveCandidateNodeMaybe; // +0x540
    MenuNodeObj0x477568 *pDecorMenuIconsMaybe[8];   // +0x544 -- the 8 decor candidate icons
    // +0x564/+0x568: the train-coupling menu's detach item and attach/spawn item
    // (Ghidra's pDetachMenuItemMaybe/pAttachOrSpawnMenuItemMaybe); refreshed by
    // RefreshTrainCouplingMenuMaybe (the attach/spawn one's state encodes the mode).
    MenuNodeObj0x477568 *pDetachMenuItemMaybe;        // +0x564
    MenuNodeObj0x477568 *pAttachOrSpawnMenuItemMaybe; // +0x568
    // +0x56c/+0x570: the decor candidate page up/down buttons (cycled by 0x2c07/0x2c08);
    // SelectDecorObjAndDispatchModeMaybe's mode-8 reset parks both in state 3.
    MenuNodeObj0x477568 *pCandidateUpMaybe;         // +0x56c
    MenuNodeObj0x477568 *pCandidateDownMaybe;       // +0x570
    // +0x574..+0x580: the four mutually-exclusive couple-choice buttons (Ghidra's
    // pCoupleChoiceAMaybe/DetachMaybe/NewTrainMaybe/AttachExistingMaybe); NewTrain and
    // AttachExisting share one on-screen slot (repositioned to the same coords).
    MenuNodeObj0x477568 *pCoupleChoiceAMaybe;               // +0x574
    MenuNodeObj0x477568 *pCoupleChoiceDetachMaybe;          // +0x578
    MenuNodeObj0x477568 *pCoupleChoiceNewTrainMaybe;        // +0x57c
    MenuNodeObj0x477568 *pCoupleChoiceAttachExistingMaybe;  // +0x580
    MenuNodeObj0x477568 *pCandidateVariantPrevBtnMaybe[4]; // +0x584 -- command 0x3868
    MenuNodeObj0x477568 *pCandidateVariantNextBtnMaybe[4]; // +0x594 -- command 0x3869
    // +0x5a4..+0x5ac: three icon-state targets (Ghidra's pIconStateTargetA/B/CMaybe) whose
    // bVisible tracks (nModeMaybe == 6) in RefreshTrainCouplingMenuMaybe.
    MenuNodeObj0x477568 *pIconStateTargetAMaybe; // +0x5a4
    MenuNodeObj0x477568 *pIconStateTargetBMaybe; // +0x5a8
    MenuNodeObj0x477568 *pIconStateTargetCMaybe; // +0x5ac

    // Constructor 0x4589b0. Landed v512 BUNDLED with the slot-16 retype below and the
    // WidgetBase.h ctor default args -- the two header edits cost 0x4588b0 (143 B) and
    // 0x457ce0 (951 B) their EXACTs on the shared-header parity dial (CODEGEN #78), paid
    // here once and re-won afterwards. The body zero-stores 14 fields in the original's
    // exact order (no member-init list; see src/WorldActionCursor.cpp).
    WorldActionCursor();

    // Destructor 0x458b00 (vtable slot 0; the ??_G scalar-deleting thunk is 0x458ad0).
    // The only explicit statement is the ClearOwned() call -- inside a dtor MSVC 5
    // devirtualizes it to the direct call the original shows (cf. BuildToolButton's dtor).
    virtual ~WorldActionCursor();

    // Real vtable slot 15 override (0x458bb0, the class vtable dword at 0x478584). Releases
    // the widget's own descriptor and all eight embedded sub-icons' with the (0,-1,0) clear
    // triple, chains the base's ClearOwned, then NULLs the thirteen owned menu-node/selection
    // pointers in the ctor's store order. Called directly (devirtualized) from the dtor and
    // class-qualified from AppWindow's SaveWindowAndCleanExit -- the call site that used to
    // carry the file-local `WorldActionCursor_ShutdownMaybe` free-function alias.
    virtual void ClearOwned();
    // Slot 1 (0x459d40) -- this leaf's own dirty-mark: chain the WidgetBase half, then dirty
    // the +0xe0 sub-icon too, because animMaybe0 sits OUTSIDE the widget's own rect and would
    // otherwise never be repainted when the cursor moves or closes.
    virtual void MarkDirty();

    // Real vtable slot 3 (0x45a500, per the class vtable dword at 0x478554) -- this class's
    // own override of the family's reposition slot, and the only thing that ever reaches it
    // (it has NO code xrefs; every caller dispatches through the slot). Clamps the requested
    // origin into the world board's viewport, chains the direct base, then drags all eleven
    // owned sub-icons to their fixed offsets from the new rect; if the widget is mid-DRAG and
    // the clamp actually moved it, it also warps the OS cursor so the grab point stays pinned.
    virtual void RepositionWithHotspot(int x, int y);

    // Real vtable slot 16 (0x45b3a0, Ghidra: WorldActionCursor::OnKeyDownMaybe) -- this
    // class's override of the family's OnKeyDown. Declaring it here costs
    // TilePlacedObj::SpawnSeqRecordEffectMaybe (0x4588b0) its 143-byte EXACT on the
    // shared-header parity dial (measured v508) -- paid as part of the v512 ctor bundle.
    virtual bool OnKeyDownMaybe(unsigned int nKey);

    // Real vtable slot 17 (0x45a880) -- the test half of the pair, and this class's override of
    // WidgetBaseObj0x4784c8::HitTestNodeSecondary. Declared as the override since v549 (it was
    // the ordinary member `TestMenuCommandMaybe`); the signature already matched the base's
    // exactly and the function has no call site in src/, so the change is a rename plus
    // `virtual` and was measured byte-free.
    virtual char HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y);

    // Real vtable slot 20 (0x45aa50) -- the execute half of the slot 17/20 test/execute
    // menu-command pair (test half: HitTestNodeSecondary/0x45a880, transcribed above).
    // Dispatches the node icon's command id (pIconDesc->resourceId): 0x2c07/0x2c08 cycle
    // the decor candidate page, 0x2c09 forwards a per-icon selection to
    // SelectedObjWidgetMaybe::SelectObjMaybe, 0x3802/0x3803 toggle/refresh the coupling
    // menu, 0x380e/0x380f/0x3810 do train-car selection, 0x3864/0x3865 attach/detach the
    // selected train, 0x3866/0x3867 rebuild/spawn it, 0x3868/0x3869 cycle icon variants.
    // Returns 0 only for a null node or while the node's wSelIndexMaybe countdown is still
    // running; every dispatched/undispatched path returns 1.
    virtual char HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode);

    // Real method 0x459180 (not a vtable slot -- called directly by the 0x3802/0x3866/0x3867
    // menu commands and externally). Selects a new decor/world object (or deselects on 0):
    // stashes the old mode, adopts the descriptor's categoryByte as nModeMaybe, then
    // dispatches per-mode menu/icon setup (the 2..0xc switch); the deselect path clears
    // bActive/mode, releases animMaybe5's channel, and dirty-marks the widget rect.
    // Returns bActive (1 after a select, 0 after a deselect).
    char SelectDecorObjAndDispatchModeMaybe(AnimDescRefObj0x477488 *pDecor); // 0x459180, extern
    // 0x459720, extern -- keeps the widget rect on-screen: repositions (slot 3) when it
    // intersects SelectedObjWidgetMaybe or scrolls off the world board's viewport.
    void ClampRectIntoViewMaybe();                          // EFFECTIVE, DIFF(10)/180 B
    void RefreshVariantMenuIconsMaybe();   // 0x45a400, src/WorldActionCursor.cpp
                                           //   (EFFECTIVE, DIFF(3)/125 B)
    void RefreshCategoryMenuIconsMaybe();  // 0x45a480, src/WorldActionCursor.cpp (EXACT)
    void RefreshTrainCouplingMenuMaybe();  // 0x4597e0, extern
    // 0x45a1a0, src/WorldActionCursor.cpp (EXACT) -- real vtable slot 21 (+0x54), this
    // class's only NEW slot past the base's 21 (the dword at 0x47859c pins it; docs/
    // subsystems.md's widget-family table). The widget's per-paint fan-out: WorldBoardMaybe's
    // paint pass (0x456700) calls it DIRECTLY with the dirty rect BY VALUE (through a
    // TU-local view), never through the slot. When active: its own composite blit
    // (class-qualified to the WidgetBaseObj0x4784c8 slot-11 override, which is the DIRECT
    // call the original emits), then slot-11 blits of animMaybe0, animMaybe5 (gated on
    // pKindDesc+bValid, followed by its slot-12 overlay blit), animMaybe6 and the four
    // animArrayMaybe icons (gated on animMaybe6.bReady), and animMaybe7.
    virtual void RepositionSubIconsMaybe(RECT rect, int bFlag);
    // 0x45a330, src/WorldActionCursor.cpp -- returns VOID, not int: the body has no
    // `mov eax` anywhere before either `ret 4`. nBase is UNSIGNED for the same reason
    // nCandidateBaseMaybe below is -- see that field's own note.
    void RefreshDecorCategoryCandidatesMaybe(unsigned int nBase);
    // ⚠ REAL VTABLE SLOT 2 (the dword at 0x478550) -- this class's override of
    // RectFlagObj0x477820::Contains, 0x459d60, src/WorldActionCursor.cpp. The widget's full hit
    // area: the root's own rect (reached CLASS-QUALIFIED, so this very override is bypassed) OR,
    // failing that, the mode-feedback icon's rect reached through animMaybe0's OWN slot 2.
    //
    // Returns `char`, restored 2026-07-31 (v546), reverting v446's `int`. v446 read the
    // full-width `xor eax,eax` / `mov eax,0x1` as proof of an `int` return -- but those two
    // constants are the `||` OPERATOR's own int result, which is `char`-truncated for free in
    // `al` and never re-narrowed. Written as `return A || B;` the function is EXACT at 64 bytes
    // with a `char` return; v446's DIFF(15)/59 B came from testing `char` against the NESTED-IF
    // source shape, which is the one construct that does narrow the constants. That mattered
    // because a `char` return is what makes this declarable as the override it actually is:
    // C++ forbids an override differing only in return type, so the `int` model was the sole
    // reason the vtable audit could not seat slot 2 (its LAST hard MISMATCH).
    //
    // Both call sites (PlacementCursorMaybe, 0x4110bc and 0x411ba4) dispatch on the CONCRETE
    // global g_worldActionCursor, which the standard resolves statically -- so `virtual` here
    // costs nothing and they stay direct calls, exactly as at slot 10 (0x459da0).
    virtual char Contains(int x, int y);
    // Vtable slot 4 (0x45a740, the dword at 0x478558) -- this class's override of
    // WidgetBaseObj0x4784c8::TryInvokeCallbackA. The click half of the
    // (Contains, TryInvokeCallbackA) pair: routes a click at (x, y) into whichever menu node
    // owns the point (ultimately HandleMenuCommandMaybe), returning non-zero when the widget
    // consumed it.
    //
    // ⚠ This line used to say the slot had to stay a non-virtual `TryHandleClickMaybe` because
    // making it virtual "would turn its call site into a vtable dispatch". That was WRONG, and
    // the contradiction was sitting three declarations below in this same class: the site is
    // `g_worldActionCursor.TryInvokeCallbackA(...)` -- a call on a GLOBAL OBJECT, not through a
    // pointer -- which the standard resolves statically exactly as the slot-2 Contains note
    // says. Declared as the real override in v549 and measured byte-free repo-wide. cl 5.0
    // indeed does not devirtualize, but no devirtualization is needed here; the rule only bites
    // when the object is reached through a pointer or reference.
    virtual char TryInvokeCallbackA(int x, int y);
    // 0x459da0 -- the widget's per-frame tick (hover icon, variant re-roll, drag, menu-node
    // dispatch, and the mode 3/6 menu refresh + tutorial notify). Body in the .cpp.
    //
    // This IS vtable slot 10 (+0x28), i.e. this class's override of
    // AnimDescRefObj0x477488::AdvanceAnimFrameMaybe, so it must carry that name -- it was
    // `void TickAndTutorialCheckMaybe()` (descriptive, non-virtual) until v545. The rename is
    // what C++ costs to model the slot; the descriptive reading survives in this comment. Its
    // one call site (src/FrameDriver.cpp's FrameDriver_TickMaybe) dispatches on the CONCRETE
    // global `g_worldActionCursor`, not through a pointer, so the standard requires static
    // dispatch there and the emitted direct call is unchanged -- verified, 0x45c3c0 still EXACT.
    virtual void AdvanceAnimFrameMaybe();

    // 0x458c90 -- builds the train-coupling menu icon list; called once from
    // src/LoadingScreen.cpp's bring-up sequence. Promoted onto the real class in v448 (see
    // the price note in src/WorldActionCursor.cpp).
    char InitTrainCouplingMenuIconsMaybe();
    MenuNodeObj0x477568 *GetOrCreateIconItemMaybe(CursorDesc *pDesc, int nTextLen);

    // Shared tail of every SelectDecorObjAndDispatchModeMaybe mode case: copy animMaybe5's
    // rect, center it within the constant decor bounds, and reposition the anim to the
    // result offset by the widget's own rect origin. Inlined at all 5 call sites in the
    // original (0x45930e, 0x4593d6, 0x45945a, 0x459536, 0x4595ba).
    inline void CenterModeAnimOverWidgetMaybe() {
        RECT rc = this->animMaybe5.rect;
        CenterRectInRect((RECT *)&g_anDecorCenterBoundsMaybe, &rc);
        this->animMaybe5.RepositionWithHotspot(this->rect.left + rc.left,
                                               this->rect.top + rc.top);
    }
};
extern WorldActionCursor g_worldActionCursor; // DAT_004a9ef0
