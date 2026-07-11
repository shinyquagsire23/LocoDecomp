// The load/save-game + backdrop file-picker widget (WidgetPickerObj0x477cc8). Every method is
// now transcribed: ctor/dtor + AdvanceAnimFrameMaybe/TestMenuCommand/ActivateTab/
// HandleTabSwitchMenuNode/HandleSavegameMenuNode (vtable slots 10/17/-/21/22) +
// RelocateSavegameSelection/PromoteOrSelectSaveEntry/EnumerateFiles/LoadActiveSlot/
// SaveActiveSlot -- see docs/subsystems.md's widget-family table.

#include "WidgetPicker.h"
#include "WorldBoardMaybe.h"
#include "PlacementCursorMaybe.h"
#include "UIResources.h"
#include "BuildToolCursorWnd.h"
#include "BuildToolButton.h"   // g_BuildToolButton + ResetAndCloseToolMenuMaybe
#include "NetSessionEventQueue.h" // g_NetSessionEventQueue + SaveBoardLayout/PlaceEdgeLinksAndFlush
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_Tracef world-load diagnostics
#endif


// NetSessionEventQueue (DAT_004a9990) -- the real class, src/NetSessionEventQueue.h. This TU
// used to reach it through a two-method VIEW STRUCT of its own; both of those methods are now
// transcribed there, and a view spelling mangles under the VIEW's class name, so every call
// here resolved to a symbol defined nowhere (a `xor eax,eax; ret N` stub in the port).
// Retired in favour of the canonical header -- see CODEGEN #184.

// UIResources/g_UIResources (DAT_004855e8) -- see src/UIResources.h.

// BuildToolCursorWnd/g_pBuildToolCursorWnd (DAT_00485258) -- see src/BuildToolCursorWnd.h.

// The shared text-formatting scratch buffer (see src/MenuNode.h's pText comment) -- also
// used directly here both as a 1-byte template prefix and as a "blank" default label.

extern int g_nNearRightEdgeThresholdMaybe; // DAT_004aa5c8, compared against a viewport-edge gap

// CursorDesc::IsItemAvailableMaybe (0x4255f0) -- "is this palette/menu item offerable right
// now" gate, shared by every widget's one-shot icon-build pass. Declared file-locally here as
// src/WidgetBase.cpp / src/WorldActionCursor.cpp / src/BuildToolButton.cpp already do; its real
// home is src/CursorDesc.h. // TODO: idiom
extern unsigned char __fastcall CursorDesc_IsItemAvailableMaybe(CursorDesc *pDesc); // 0x4255f0

// The world backdrop descriptor -- the real CursorDesc, canonically declared in
// src/WorldBoardMaybe.cpp / src/AppWindow.cpp / src/NetSessionEventQueue.cpp the same way.
extern CursorDesc *g_pBackdropDesc; // DAT_004fd3c8  // TODO: idiom

// FUNCTION: LOCO 0x427370
WidgetPickerObj0x477cc8::WidgetPickerObj0x477cc8()
    : embeddedIcon(-1, -1, 0, 0)
{
    nCategory = 0;
    for (int i = 0; i < 6; i++) {
        arrSlotNodes[i] = 0;
    }
    nTypeTag = 0xc;
    pTabButtonNode2 = 0;
    pScrollDownArrowNode = 0;
    pScrollUpArrowNode = 0;
    pOverviewTabButton = 0;
    pTabButtonNode1 = 0;
    pTabButtonNode0 = 0;
    pCurrentSlotNode = 0;
    pCurrentEntry = 0;
    pLinkedListHead = 0;
    szActiveSavePath[0] = 0;
    szPendingSavePath[0] = 0;
    pShownThumbnailBmp = 0;
}

// FUNCTION: LOCO 0x427460
WidgetPickerObj0x477cc8::~WidgetPickerObj0x477cc8()
{
    SavedFileEntry *pEntry = pLinkedListHead;
    while (pEntry != 0) {
        pLinkedListHead = pEntry->pNext;
        delete pEntry;
        pEntry = pLinkedListHead;
    }
    SetDescriptor(0, -1, 0);
    embeddedIcon.SetDescriptor(0, -1, 0);
    ClearOwned();
}

// FUNCTION: LOCO 0x427440 (??_GWidgetPickerObj0x477cc8 scalar dtor)

// SavedFileEntry's own destructor pair. The class body in src/WidgetPicker.h defines the dtor
// in-class and EMPTY, so both COMDATs are entirely compiler-generated: `??1` is the vptr re-stamp
// to 0x477d24 plus a TAIL JUMP into ~ThumbnailBmp for the embedded thumbnail member (14 bytes,
// no prologue at all), and `??_G` is the ordinary 30-byte call-through thunk the vtable points at.
// An in-class empty dtor does NOT stop cl emitting them out of line here -- the class is
// polymorphic and this is the TU that emits its vtable, so the definitions land alongside it.
// Both markers are hint-only: there is no source line of its own to sit above, the same shape
// `??_GDSound` uses.
//
// FUNCTION: LOCO 0x429820 (??1SavedFileEntry -- compiler-generated from the in-class empty dtor)
// FUNCTION: LOCO 0x429830 (??_GSavedFileEntry scalar deleting dtor -- compiler-generated)

// FUNCTION: LOCO 0x427520
// Vtable slot 15 (+0x3c) -- this class's override of WidgetBaseObj0x4784c8::ClearOwned. Frees
// the entire SavedFileEntry linked list (each `delete` goes through the entry's own slot 0, so
// the records really are polymorphic), unpublishing the head as it goes rather than after the
// loop -- the list is left consistent at every step, so a re-entrant walk during teardown can
// never see a freed record. Then releases this widget's own frame descriptor and the embedded
// icon's, both via the SetDescriptor(0,-1,0) "clear" convention, and chains the base body.
void WidgetPickerObj0x477cc8::ClearOwned()
{
    SavedFileEntry *pEntry = pLinkedListHead;
    while (pEntry != NULL) {
        pLinkedListHead = pEntry->pNext;
        delete pEntry;
        pEntry = pLinkedListHead;
    }
    SetDescriptor(0, -1, 0);
    embeddedIcon.SetDescriptor(0, -1, 0);
    WidgetBaseObj0x4784c8::ClearOwned();
}

// FUNCTION: LOCO 0x427580
// The one-shot icon-build pass for this widget's own tab bar -- the exact structural twin of
// BuildToolButton::InitMenuIconsMaybe. Loads the two frame descriptors (0x2c00 for the widget
// itself, 0x2c01 for the embedded icon), then, only on the very first call (pMenuListHead still
// empty), walks TileKind ids 0x2c00..0x2c13 and builds a menu icon node per available one,
// caching the individually-meaningful ones into its own named fields. The 0x2c09 "current slot"
// case additionally builds the 6 visible list slots, spaced 0x19 apart vertically.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 580: total 275216, align=272 reg_pen=28
// identity_miss=28 byte_diff=136, insns 190/185; cc.sh DIFF(309), ours 568 B vs the original's
// 580 -- note 580 is the COMDAT extent, 0x427580..0x427798 of code plus the 11-entry/44-byte
// jump table, NOT Ghidra's `Body:` span). Content-complete and verified instruction-by-
// instruction against the raw disasm: both descriptor ids, the pMenuListHead first-call guard,
// all three entry NULL stores, the 20-iteration counted loop, every one of the 9 switch arms
// (in the original's own PHYSICAL body order, which is 0x2c02/03/05/0c/04/09/07/08/default --
// not the ascending order the jump table lists) and the 6-slot sub-loop all line up.
//
// ONE residual, and it is a single allocator decision with a systematic knock-on: the original
// assigns EBX to the sub-loop's `nDy` and lets its zero constant die at `mov edi,eax` (EDI is
// zero only for the two SetDescriptor calls, then becomes pDesc), so all 17 in-loop zero
// arguments encode as 2-byte `push 0` immediates. This compile instead promotes the constant 0
// to EBX for the WHOLE function, spilling `nDy` to the stack -- so every one of those 17 sites
// encodes as a 1-byte `push ebx` (that is the entire 12-byte length shortfall, plus one extra
// `xor ebx,ebx` to re-establish the register after the sub-loop clobbers it, plus the paired
// `test edi,edi` -> `cmp edi,ebx` at the pDesc null check that v360's zero-reg lesson predicts).
//
// **Measured and INERT -- do NOT re-run:** swapping the sub-loop's two same-type sibling locals
// (`nDy` / `nSlots`) in declaration order, the documented swap-two-sibling-locals lever, scores
// an IDENTICAL 275216 -- it has nothing to grip, because the choice is between the constant and
// `nDy`, not between the two locals. **One lever IS baked in, do not undo:** the two descriptor
// guards must be a single combined `&&` over an `unsigned char bOk` local with a trailing
// `else { bOk = 0; }`, not two `return 0;` early exits -- the original funnels both guards into
// ONE shared failure epilogue (`je 0x206` twice) and materializes the result through a stack
// byte, where the early-return form makes cl duplicate the whole epilogue at each guard
// (DIFF 402 -> 309, total 409041 -> 275216). Same VC5 cross-jump/tail-merge family as the twin
// BuildToolButton::InitMenuIconsMaybe's own residual #1, but resolved in the opposite direction:
// there the original duplicates and we shared; here the original shares and we duplicated.
unsigned char WidgetPickerObj0x477cc8::InitMenuIconsMaybe()
{
    unsigned char bOk;
    if (SetDescriptor(0x2c00, -1, 0) && embeddedIcon.SetDescriptor(0x2c01, -1, 0)) {
        bOk = 1;
        if (pMenuListHead == NULL) {
            pOverviewTabButton = NULL;
            pTabButtonNode1 = NULL;
            pTabButtonNode0 = NULL;

            int nKindId = 0x2c00;
            int nRemaining = 0x14;
            do {
                CursorDesc *pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(nKindId);
                if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
                    switch (pDesc->resourceId) {
                    case 0x2c02:
                        pTabButtonNode0 = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    case 0x2c03:
                        pTabButtonNode1 = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    case 0x2c05:
                        pTabButtonNode2 = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    case 0x2c0c:
                        pTabButtonNode3 = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    case 0x2c04:
                        pOverviewTabButton = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        pBaseCandidateDown = pOverviewTabButton;
                        break;
                    case 0x2c09:
                        pCurrentSlotNode =
                            (UiIconListItem *)GetOrCreateMenuIconItemMaybe(pDesc, 0, 10);
                        pCurrentSlotNode->RepositionWithHotspot(0xb, 0x82);
                        pCurrentSlotNode->bTextRedrawEnabled = true;
                        pCurrentSlotNode->SetLabelText("");
                        {
                            int nDy = 0;
                            MenuNodeObj0x477568 **ppSlot = arrSlotNodes;
                            int nSlots = 6;
                            do {
                                MenuNodeObj0x477568 *pSlot =
                                    GetOrCreateMenuIconItemMaybe(pDesc, 0, 10);
                                *ppSlot = pSlot;
                                if (pSlot != NULL) {
                                    pSlot->RepositionWithHotspot(pSlot->rect.left,
                                                                 pSlot->rect.top + nDy);
                                }
                                ppSlot++;
                                nDy += 0x19;
                                nSlots--;
                            } while (nSlots != 0);
                        }
                        break;
                    case 0x2c07:
                        pScrollUpArrowNode = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    case 0x2c08:
                        pScrollDownArrowNode = GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    default:
                        GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
                        break;
                    }
                }
                nKindId++;
                nRemaining--;
            } while (nRemaining != 0);
        }
    } else {
#ifdef LOCO_PORT
        // PORT ONLY -- byte-neutral. Names which of the two descriptor loads refused; the
        // combined `&&` guard shape above is load-bearing for the match, so the trace goes in
        // the shared failure arm rather than being split across the two operands.
        Port_Tracef("PICKER: InitMenuIconsMaybe guard failed (0x2c00 / 0x2c01)\n");
#endif
        bOk = 0;
    }
    return bOk;
}

// FUNCTION: LOCO 0x4282b0
// EXACT (2026-07-30, 193 bytes). Two source facts are load-bearing -- do not "clean up":
//  (1) `POINT ptLocal;` declared UNINITIALIZED and assigned on the next line, NOT
//      `POINT ptLocal = ComputeLocalPos(...)`: with the initializer form VC5 makes ptLocal
//      itself the hidden return buffer and every later read comes off esp, where the
//      original reads the point back through the pointer the call left in EAX
//      (`mov ebp,[eax]` / `mov eax,[eax+4]`) -- docs/CODEGEN.md's v392 struct-return lesson
//      (the one-shape flip took the residual from byte_diff 22 to 0). The named
//      `int x`/`int y` copies off ptLocal (rather than reading ptLocal.x/.y at the call
//      site) are also load-bearing: without them the whole function's register allocation
//      collapses to 3 callee-saved regs with frame-relative x/y reloads (byte_diff 92).
//  (2) The hit-test short-circuit is spelled `if (bNodeHit || probe(...)) bNodeHit = true;`
//      -- the original re-materializes `mov bl,1` on the already-hit path, the bool
//      normalization this shape (and not `if (!bNodeHit) bNodeHit = ...`) produces.
// Vtable slot 10 -- the family-wide per-frame "Tick" slot (Ghidra: TickMenuNodesAndIconMaybe;
// overrides the base's AdvanceAnimFrameMaybe, same repurposing as WidgetTagObj0x478378's own
// slot-10 override). Re-localizes the placement cursor's last resolved position, then walks
// pMenuListHead: each node is dispatched through this widget's own slot 19 (hit-test-shaped)
// only until the first node reports a hit, and unconditionally through slot 20 (the execute
// half). After the list is exhausted, when the cursor is over the embedded icon OR outside
// the widget's own rect, the icon's hover animation is driven (slot 7 with 0 while its
// nSubFrame is in [1,5)); the icon's own slot-10 tick runs unconditionally.
void WidgetPickerObj0x477cc8::AdvanceAnimFrameMaybe()
{
    bool bNodeHit = false;
    POINT ptLocal;
    ptLocal = ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                              PlacementCursorMaybe_004854c8.lastResolvedPosY);
    int x = ptLocal.x;
    MenuNodeObj0x477568 *pNode = pMenuListHead;
    int y = ptLocal.y;
    for (; pNode != NULL; pNode = pNode->pNext) {
        if (bNodeHit || TestAndToggleMenuNodeHoverMaybe(pNode, x, y)) {
            bNodeHit = true;
        }
        HandleMenuCommandMaybe(pNode);
    }
    if (embeddedIcon.Contains(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                              PlacementCursorMaybe_004854c8.lastResolvedPosY) ||
        !Contains(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                  PlacementCursorMaybe_004854c8.lastResolvedPosY)) {
        if (embeddedIcon.nSubFrame != 0 && embeddedIcon.nSubFrame < 5) {
            embeddedIcon.ReleaseChannelAndDispatch(0);
        }
    }
    embeddedIcon.AdvanceAnimFrameMaybe();
}

// FUNCTION: LOCO 0x428380
// Vtable slot 11. Chains the base's own blit, then repaints the embedded icon over the same
// clip rect -- but only on the category-1 overview tab, where the icon is what fills the area
// the other categories give to the slot list. The icon's flags word is passed as 0 rather than
// the caller's: this second layer never inherits the outer blit's mode bits.
void WidgetPickerObj0x477cc8::BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags)
{
    WidgetBaseObj0x4784c8::BlitAnimFrameMaybe(rect, flag, flags);
    if (nCategory == 1) {
        // Through a pointer so the dispatch stays virtual (`call [vtbl+0x2c]`) rather than the
        // direct call a value-typed `embeddedIcon.BlitAnimFrameMaybe(...)` devirtualizes to --
        // the EffectSpawnerCtorViewMaybe ctor's slot-19 precedent.
        AnimDescRefObj0x477488 *pIcon = &embeddedIcon;
        pIcon->BlitAnimFrameMaybe(rect, flag, 0);
    }
}

// FUNCTION: LOCO 0x428770
// Vtable slot 3, this class's override. Moves the widget itself through the base, then places
// the embedded icon at the descriptor's own "button" offset relative to the new position --
// so the icon rides along with the picker rather than needing its own reposition pass.
// ⚠ The descriptor must be re-read INSIDE each argument rather than cached in a
// `BigObj *pKindDesc` local. Both forms are 23 instructions and 57 bytes, but the cached-local
// form makes cl compute BOTH arguments into separate registers before pushing either
// (DIFF(22)); re-reading lets it keep one scratch register (edx) and push each argument as
// soon as it is computed, which is what the original does -- the load itself is CSE'd, so the
// only thing that changes is the liveness the scheduler sees.
void WidgetPickerObj0x477cc8::RepositionWithHotspot(int x, int y)
{
    WidgetBaseObj0x4784c8::RepositionWithHotspot(x, y);
    // Through a pointer so the dispatch stays virtual (`call [vtbl+0xc]`), same as
    // BlitAnimFrameMaybe just above.
    AnimDescRefObj0x477488 *pIcon = &embeddedIcon;
    pIcon->RepositionWithHotspot(embeddedIcon.pKindDesc->field_0x2eMaybe + x,
                                 embeddedIcon.pKindDesc->field_0x30Maybe + y);
}

// FUNCTION: LOCO 0x4287b0
// ⭐ v552: this is vtable SLOT 17, and it is now DECLARED as the real override of
// WidgetBaseObj0x4784c8::HitTestNodeSecondary rather than as the ordinary non-virtual member
// `TestMenuCommand(UiIconListItem *, int, int)` it had been. The image's dword at
// 0x477cc8 + 17*4 is 0x4287b0, so our emitted vtable had been inheriting the base's slot --
// vtable_audit's "MISSING OVERRIDE DECLARATION". The name and the parameter type are the
// BASE's because C++ has no covariant parameters: the original's slot 17 necessarily took a
// MenuNodeObj0x477568* and downcast where it wanted UiIconListItem::GetLabelText, which is
// exactly what the sibling overrides WorldActionCursor::HitTestNodeSecondary (0x45a880) and
// SelectedObjWidgetMaybe::HitTestNodeSecondary (0x42d6b0) already do. VERIFIED BYTE-NEUTRAL:
// UiIconListItem derives from MenuNodeObj0x477568 at offset 0 so the downcast emits nothing,
// the slot is the function's only entry point (no call sites to turn into slot dispatches),
// and the residual below is unchanged at DIFF(286)/460 B with the TU flat at 4266 B.
// The v519 autopsy is unaffected and still current:
// EFFECTIVE MATCH (v519, DIFF 286/460 bytes -- was DIFF 368/448 with the plain return-0/1
// transcription): structure/control-flow fully verified against the raw disasm (all 8 case
// bodies, the shared post-switch wState tail, and case 0x2c04's fallthrough into that same
// tail all confirmed instruction-for-instruction). v519 retried with CODEGEN #18m (the shape
// that cracked the sibling 0x4289a0): entry-initialized `unsigned char bResult = 0`, every
// in-switch exit as `return bResult`, `bResult = 1` assignments on the pass paths. The old
// EBX-persistence residual is GONE (asmscore reg_pen 368-class -> 0; insns 149/142); case
// 0x2c09 now pairs exactly incl. the original's `mov bl,1` hoisted before GetLabelText and
// its `mov al,bl` tail. The REMAINING residual is structural: the original funnels every
// switch zero-exit through ONE shared `mov al,bl` epilogue (plus one `xor bl,bl` re-zero on
// the default/out-of-range path) where this compile proves bl==0 on each path and
// materializes per-case `xor al,al` epilogues, and the shared wState tail closes with
// `mov al,1` where the original has `mov bl,1; mov al,bl`. **Measured and REJECTED -- do
// NOT re-run:** (a) full #18m funnel (every case `bResult = X; break;` to ONE
// `return bResult`, the wState fixup block duplicated per passed case for cross-jumping) --
// 484 B, DIFF(386): the duplicated blocks did NOT cross-jump; (b) `goto pass;`/`goto done;`
// funnel with a single shared wState block -- 484 B, DIFF(392). The earlier park note's
// refuted list stands too (bResult assigned-only-at-use, nested-if vs && chains, goto
// without the accumulator). Retry only if the shared-epilogue-funnel class becomes
// source-steerable.
char WidgetPickerObj0x477cc8::HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y)
{
    char bResult = 0;
    if (pNode == 0 || !pNode->bVisible || !pNode->Contains(x, y)) {
        return 0;
    }

    switch (pNode->pIconDesc->resourceId) {
    case 0x2c02:
        if (nCategory != 1 && nCategory != 2) {
            return bResult;
        }
        break;
    case 0x2c03:
        if (nCategory != 1 && nCategory != 3) {
            return bResult;
        }
        break;
    case 0x2c04:
        break;
    case 0x2c05:
        if (nCategory != 1 && nCategory != 4) {
            return bResult;
        }
        break;
    case 0x2c07:
    case 0x2c08:
        if ((nCategory == 0 || nCategory == 2 || nCategory == 3 ||
             nCategory == 4 || nCategory == 5) &&
            pNode->wState == 1) {
            pNode->SetNodeState(2);
            pNode->wSelIndexMaybe = 6;
        }
        return bResult;
    case 0x2c09:
        if ((nCategory == 2 || nCategory == 3 || nCategory == 4 || nCategory == 5) &&
            pNode != pCurrentSlotNode) {
            bResult = 1;
            char *pLabel = ((UiIconListItem *)pNode)->GetLabelText();
            pCurrentSlotNode->SetLabelText(pLabel);
            if (pCurrentSlotNode->GetLabelTextLength() == 0 && embeddedBitmap.pPixels != 0) {
                ReloadBackdropPreview(0);
            }
            pCurrentSlotNode->Draw();
        }
        return bResult;
    case 0x2c0c:
        if (nCategory != 5) {
            return bResult;
        }
        if (pNode->wState == 1) {
            pNode->SetNodeState(2);
            bResult = 1;
            pNode->wSelIndexMaybe = 6;
        }
        bResult = 1;
        return bResult;
    default:
        return bResult;
    }

    if (pNode->wState == 1) {
        pNode->SetNodeState(2);
        pNode->wSelIndexMaybe = 6;
    }
    bResult = 1;
    return bResult;
}

// FUNCTION: LOCO 0x4277d0
// EFFECTIVE MATCH (2026-07-16, DIFF 2234/2772 bytes -- raw byte-diff% looks large but is almost
// entirely cascaded displacement/offset drift from a handful of small residuals; asmscore.py's
// structural scorer shows the real content-diff confined to ~15 instructions out of ~850, and
// everything past offset ~0xac7 is switch-jump-table data that decodes as noise on both sides
// per verify.py's own documented caveat). Structure fully verified against the raw disasm for
// all 6 switch cases (the 4 sorted-position search blocks in cases 2/3/4/5 are confirmed
// byte-for-byte IDENTICAL compiled code in the original -- see docs/subsystems.md -- so writing
// them out literally, once per case, is the faithful transcription, not a missed opportunity to
// factor into a helper). Remaining residuals, all matching already-documented intrinsic classes
// in this codebase:
//  - case 0: the redundant `ecx=&g_BuildToolButton` load (needed both for the global store
//    and the following call's implicit this) is ordered/folded differently by the allocator --
//    source order already matches the original's statement order exactly.
//  - case 1: the "SetDescriptor(...) || embeddedIcon.SetDescriptor(...)" result is
//    cached in BL across the two calls here but tested via AL directly in the original -- the
//    same EBX/BL-persistence register-economy tie-break already documented on this file's own
//    TestMenuCommand (see its own EFFECTIVE MATCH comment above).
//  - the final `if (bActive)` tail check: direct memory `cmp byte ptr[x],1` in the
//    original vs. load-then-`test al,al` here -- tried an explicit `== true` comparison, no
//    effect (see docs/PARKED.md for the full tried-and-failed list, mirroring TestMenuCommand's
//    own already-parked residual class).
unsigned char WidgetPickerObj0x477cc8::ActivateTab(MenuNodeObj0x477568 *param_1, unsigned short param_2)
{
    // Declared unconditionally at the top (not scoped to case 2/3 below) -- the original zeroes
    // this buffer on EVERY call regardless of which case runs, confirming it's a real top-level
    // local, not a per-case one.
    char szBuf[0x105] = "";
    pLastActivatedNode = param_1;
    switch (param_2) {
    case 0:
        MarkDirty();
        bActive = false;
        g_pActiveTabWidgetMaybe = &g_BuildToolButton;
        g_BuildToolButton.ResetAndCloseToolMenuMaybe();
        nCategory = param_2;
        break;
    case 1:
    {
        bActive = true;
        g_pActiveTabWidgetMaybe = this;
        nCategory = param_2;
        unsigned char bOk = SetDescriptor(0x2c00, 0, 1);
        if (!bOk) {
            bOk = embeddedIcon.SetDescriptor(0x2c01, -1, 0);
        }
        g_UIResources.PlayUiSound(0x502d);
        if (!bOk) {
            return 0;
        }
        pTabButtonNode0->RepositionWithHotspot(0x14, 0x21);
        pTabButtonNode1->RepositionWithHotspot(0xe9, 0x21);
        pTabButtonNode2->RepositionWithHotspot(0x14, 0x92);
        pOverviewTabButton->RepositionWithHotspot(0xe9, 0x92);
        pBaseCandidateUp = pOverviewTabButton;
        pTabButtonNode1->bVisible = 1;
        pTabButtonNode0->bVisible = 1;
        pTabButtonNode2->bVisible = 1;
        pTabButtonNode3->bVisible = 0;
        pCurrentSlotNode->bVisible = 0;
        EnumerateFiles();
        g_NetSessionEventQueue.SaveBoardLayout((unsigned char *)"~curr");
        break;
    }
    case 2:
    {
        bActive = true;
        g_pActiveTabWidgetMaybe = this;
        nCategory = param_2;
        unsigned char bOk = SetDescriptor(0x2c06, 0, 1);
        if (!bOk) {
            return 0;
        }
        pBaseCandidateUp = pTabButtonNode0;
        pTabButtonNode1->bVisible = 0;
        pTabButtonNode0->bVisible = 1;
        pTabButtonNode2->bVisible = 0;
        pTabButtonNode3->bVisible = 0;
        pTabButtonNode0->RepositionWithHotspot(0xa, 0x9d);
        pTabButtonNode0->Draw();
        pOverviewTabButton->RepositionWithHotspot(0x51, 0x9d);
        RelocateSavegameSelection(pLinkedListHead);
        _splitpath(szActiveSavePath, 0, 0, szBuf, 0);
        pCurrentSlotNode->SetLabelText(szBuf);
        pCurrentSlotNode->bVisible = 1;
        pCurrentSlotNode->Draw();
        int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp > 0) {
            if (pCurrentEntry->pPrev == 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pPrev);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp < 0) {
            if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pNext);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        break;
    }
    case 3:
    {
        bActive = true;
        nCategory = param_2;
        unsigned char bOk = SetDescriptor(0x2c06, 0, 1);
        if (!bOk) {
            return 0;
        }
        pBaseCandidateUp = pTabButtonNode1;
        pTabButtonNode0->bVisible = 0;
        pTabButtonNode1->bVisible = 1;
        pTabButtonNode2->bVisible = 0;
        pTabButtonNode3->bVisible = 0;
        pTabButtonNode1->RepositionWithHotspot(0xa, 0x9d);
        pTabButtonNode1->Draw();
        pOverviewTabButton->RepositionWithHotspot(0x51, 0x9d);
        RelocateSavegameSelection(pLinkedListHead);
        _splitpath(szActiveSavePath, 0, 0, szBuf, 0);
        pCurrentSlotNode->SetLabelText(szBuf);
        pCurrentSlotNode->bVisible = 1;
        pCurrentSlotNode->Draw();
        int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp > 0) {
            if (pCurrentEntry->pPrev == 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pPrev);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp < 0) {
            if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pNext);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        break;
    }
    case 4:
    {
        bActive = true;
        nCategory = param_2;
        unsigned char bOk = SetDescriptor(0x2c06, 0, 1);
        if (!bOk) {
            return 0;
        }
        pBaseCandidateUp = pTabButtonNode2;
        pTabButtonNode1->bVisible = 0;
        pTabButtonNode0->bVisible = 0;
        pTabButtonNode2->bVisible = 1;
        pTabButtonNode3->bVisible = 0;
        pTabButtonNode2->RepositionWithHotspot(0xa, 0x9d);
        pTabButtonNode2->Draw();
        pOverviewTabButton->RepositionWithHotspot(0x51, 0x9d);
        RelocateSavegameSelection(pLinkedListHead);
        pCurrentSlotNode->SetLabelText("");
        pCurrentSlotNode->bVisible = 1;
        pCurrentSlotNode->Draw();
        int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp > 0) {
            if (pCurrentEntry->pPrev == 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pPrev);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp < 0) {
            if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pNext);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        break;
    }
    case 5:
    {
        bActive = true;
        nCategory = param_2;
        g_pActiveTabWidgetMaybe = this;
        unsigned char bOk = SetDescriptor(0x2c06, 0, 1);
        if (!bOk) {
            return 0;
        }
        pBaseCandidateUp = pTabButtonNode3;
        pTabButtonNode1->bVisible = 0;
        pTabButtonNode0->bVisible = 0;
        pTabButtonNode2->bVisible = 0;
        pTabButtonNode3->bVisible = 1;
        pTabButtonNode3->RepositionWithHotspot(0xa, 0x9d);
        pTabButtonNode3->Draw();
        pOverviewTabButton->RepositionWithHotspot(0x51, 0x9d);
        EnumerateFiles();
        RelocateSavegameSelection(pLinkedListHead);
        pCurrentSlotNode->SetLabelText("");
        pCurrentSlotNode->bVisible = 1;
        pCurrentSlotNode->Draw();
        int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp > 0) {
            if (pCurrentEntry->pPrev == 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pPrev);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        while (nCmp < 0) {
            if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
                break;
            }
            RelocateSavegameSelection(pCurrentEntry->pNext);
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
        }
        break;
    }
    }

    if (bActive) {
        if ((int)(g_worldBoard.dwViewportWidth - rectViewport.right) < g_nNearRightEdgeThresholdMaybe) {
            bSuppressRectBMaybe = true;
            return 1;
        }
        bSuppressRectBMaybe = false;
    }
    return 1;
}

// BuildToolButton::ClampDragPositionMaybe (0x449dc0, a large not-yet-transcribed drag/placement-clamp
// + cursor-repositioning method) -- only its call signature is needed here, modeled as a
// same-address partial view like this file's other not-yet-transcribed singletons/methods above.
struct BuildToolButtonPartial {
    void ClampDragPositionMaybe(int param_1, int param_2);
};

// Read once, at this function's one call site, and never written anywhere in .text -- always
// zero-initialized BSS at binary load. Left at their literal DAT_ names (lowest naming-ladder
// rung) pending a caller/writer that would pin down what they actually track.
extern int DAT_004aa5c0;
extern int DAT_004aa5c4;

// FUNCTION: LOCO 0x4289a0
// The category-1-specific menu-command test: only while the "current save" overview tab is
// active does a hit on a tab-switch command node get consumed, dispatching the embedded icon's
// ReleaseChannelAndDispatch (slot 7) with the tab's own code. The switch's PHYSICAL body order
// in the original is 0x2c02/0x2c04/0x2c03/0x2c05 (not ascending), so the cases are written in
// that order -- the same physical-order rule as InitMenuIconsMaybe's switch (0x427580).
unsigned char WidgetPickerObj0x477cc8::TestTabSwitchMenuCommandMaybe(UiIconListItem *param_1,
                                                                     int param_2, int param_3)
{
    unsigned char bResult = 0;
    if (param_1 == 0) {
        return 0;
    }
    if (param_1->Contains(param_2, param_3) == 0) {
        return 0;
    }
    if (nCategory == 1) {
        switch (param_1->pIconDesc->resourceId) {
        case 0x2c02:
            embeddedIcon.ReleaseChannelAndDispatch(1);
            bResult = 1;
            break;
        case 0x2c04:
            embeddedIcon.ReleaseChannelAndDispatch(4);
            bResult = 1;
            break;
        case 0x2c03:
            embeddedIcon.ReleaseChannelAndDispatch(2);
            bResult = 1;
            break;
        case 0x2c05:
            embeddedIcon.ReleaseChannelAndDispatch(3);
            bResult = 1;
            break;
        default:
            bResult = 0;
            break;
        }
    }
    return bResult;
}

// FUNCTION: LOCO 0x428a80
// Vtable slot 20 -- this class's "execute the command on this node" half, used purely as a
// router. Retires one unit of the node's own wSelIndexMaybe (the 0xffff sentinel is negative,
// so it is left alone), then dispatches by which tab is showing: the category-1 overview tab
// goes to the tab-switch handler, categories 2-5 (savegame/backdrop lists) to the slot handler,
// and anything else to neither. Either way the node's own animation is ticked afterwards.
// Both callee dispatches are VIRTUAL (slots 21/22) in the original, not direct calls -- see
// the ⚠ note on their declarations in src/WidgetPicker.h.
// Returns 0 on every path, including the null-node one: this class never reports a command
// consumed through slot 20, even though both of its callees return a real flag.
// EFFECTIVE MATCH (v533): insns 38/38, DIFF(6), align=0 -- every instruction, block, branch
// and constant is the original's, in the original's order, INCLUDING both separate `xor al,al`
// epilogues (the early return in the category-1 arm is load-bearing; an else-if chain would
// give one shared TickAdvanceFrame call, and the original has two). The whole residual is a
// three-site eax<->edx coin flip on the vtable-pointer loads: the original reuses EAX across
// `mov eax,[ecx]` / `call [eax+0x54]` / `mov eax,[esi]`, this build alternates the other way
// starting from EDX. Probe REFUTED: hoisting nCategory into a local `short` to force the single
// load compiles BIT-IDENTICAL (cl canonicalizes it -- the original already loads it once).
// Parked in docs/PARKED.md.
char WidgetPickerObj0x477cc8::HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode)
{
    if (pNode != NULL) {
        if (pNode->wSelIndexMaybe >= 0) {
            pNode->wSelIndexMaybe--;
        }
        if (nCategory == 1) {
            HandleTabSwitchMenuNode(pNode);
            pNode->TickAdvanceFrame();
            return 0;
        }
        if (nCategory == 2 || nCategory == 3 || nCategory == 4 || nCategory == 5) {
            HandleSavegameMenuNode((UiIconListItem *)pNode);
        }
        pNode->TickAdvanceFrame();
    }
    return 0;
}

// FUNCTION: LOCO 0x428af0
// EFFECTIVE MATCH (2026-07-16, DIFF 46/168 bytes): structure fully verified against the raw
// disasm (early-out guard, the wState==2 hover-toggle, the resourceId switch mapping
// 0x2c02/0x2c03/0x2c05 onto ActivateTab categories 2/3/4 while 0x2c04 resets straight to
// category 0 without the trailing BuildToolButton reposition call). Remaining residuals,
// all matching already-documented intrinsic classes in this codebase:
//  - the pLastActivatedNode (+0xd4) read is scheduled by the allocator interleaved with
//    the resourceId dereference (independent values, no data dependency to force an order) and
//    is kept live in ONE register across the whole switch dispatch here, vs. the original
//    re-reading it a 2nd time separately (once for case 0x2c04's own check, once for the shared
//    tail) -- tried duplicating the read/call per case instead (to match the 2-read count
//    exactly), which scored WORSE under asmscore.py's alignment metric (total 146724 vs this
//    version's 80710, more total instructions) since the 3 duplicated call sites each picked a
//    DIFFERENT register for the reload, blocking the cross-jump merge that would be needed to
//    match -- an instance of the same register-choice nondeterminism as the documented
//    symmetric-register-swap class (Yoda #29/#30), just for a reload instead of a role-swap.
//  - the 2 DAT_004aa5c0/DAT_004aa5c4 loads feeding the ClampDragPositionMaybe tail call pick swapped
//    registers (ecx/edx here vs edx/eax originally) -- the same tie-break class.
//  - bytes past the real function body are switch-jump-table + inter-function padding noise
//    (this function is smaller than the original's, so the diff window runs into the next
//    function's own prologue -- see ActivateTab's own EFFECTIVE MATCH comment above for
//    the same documented caveat).
unsigned char WidgetPickerObj0x477cc8::HandleTabSwitchMenuNode(MenuNodeObj0x477568 *param_1)
{
    if (param_1->wSelIndexMaybe != 0) {
        return 0;
    }
    if (param_1->wState == 2) {
        param_1->SetNodeState(1);
    }

    MenuNodeObj0x477568 *pLast = pLastActivatedNode;
    switch (param_1->pIconDesc->resourceId) {
    case 0x2c02:
        ActivateTab(pLast, 2);
        break;
    case 0x2c03:
        ActivateTab(pLast, 3);
        break;
    case 0x2c04:
        if (pLast != 0 && pLast->wState == 2) {
            pLast->SetNodeState(1);
        }
        ActivateTab(param_1, 0);
        return 1;
    case 0x2c05:
        ActivateTab(pLast, 4);
        break;
    default:
        return 1;
    }

    ((BuildToolButtonPartial *)&g_BuildToolButton)->ClampDragPositionMaybe(DAT_004aa5c0, DAT_004aa5c4);
    return 1;
}

extern char g_pInstallPathPrefix[]; // DAT_004a99c8

// FUNCTION: LOCO 0x429850
// EFFECTIVE MATCH (2026-07-16, DIFF 319/450 bytes): structure fully verified against the raw
// disasm -- 141/141 instructions correspond 1:1 (asmscore.py's own count). The ENTIRE residual
// is the documented symmetric-register-swap class (Yoda #29/#30, confirmed repeatedly
// elsewhere in this codebase): the original assigns EBX to the walked param_1/entry pointer and
// EBP to the arrSlotNodes slot-node induction pointer; this compile picks the opposite assignment
// throughout the whole function, every single differing line being the exact same operation
// with the two registers' roles swapped. Tried and confirmed NO EFFECT: reordering the
// ppSlotNode local's declaration earlier/later relative to the null-check block (scored WORSE
// each time tried). One residual jcc-sense flip also remains on the down-arrow branch (`jne`
// vs `je` reaching the same push-1-or-3 outcome via the opposite fall-through arm) --  tried
// swapping the if/else branch order to flip it back, no effect (Yoda #6: CMP/jcc sense often
// not source-steerable). No untried lever on either; see docs/PARKED.md.
void WidgetPickerObj0x477cc8::RelocateSavegameSelection(SavedFileEntry *param_1)
{
    char szPath[0x104] = "";

    if (param_1 == 0) {
        param_1 = pLinkedListHead;
    }
    pCurrentEntry = param_1;

    MenuNodeObj0x477568 **ppSlotNode = arrSlotNodes;
    for (int i = 6; i != 0; i--) {
        (*ppSlotNode)->pEntry = param_1;
        if (param_1 != 0) {
            ((UiIconListItem *)*ppSlotNode)->SetLabelText(param_1->szShortName);
            ThumbnailBmp *pThumb = &param_1->embeddedThumbnailBmp;
            if (pThumb->wFormatTag == 0) {
                strcpy(szPath, g_pInstallPathPrefix);
                strcat(szPath, param_1->szFileName);
                pThumb->ThumbnailBmp_Load(szPath);
            }
            if (nCategory != 3 && nCategory != 4 && nCategory != 5) {
                if (!pThumb->ThumbnailBmp_IsLoaded()) {
                    (*ppSlotNode)->SetNodeState(3);
                    param_1 = param_1->pNext;
                    goto drawSlot;
                }
            }
            (*ppSlotNode)->SetNodeState(1);
            param_1 = param_1->pNext;
        } else {
            ((UiIconListItem *)*ppSlotNode)->SetLabelText("");
            (*ppSlotNode)->SetNodeState(3);
        }
    drawSlot:
        (*ppSlotNode)->Draw();
        ppSlotNode++;
    }

    if (pCurrentEntry != 0 && pCurrentEntry->pPrev != 0) {
        pScrollUpArrowNode->SetNodeState(1);
    } else {
        pScrollUpArrowNode->SetNodeState(3);
    }

    if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() != 0) {
        pScrollDownArrowNode->SetNodeState(1);
    } else {
        pScrollDownArrowNode->SetNodeState(3);
    }

    pScrollUpArrowNode->Draw();
    pScrollDownArrowNode->Draw();
    pCurrentSlotNode->Draw();
}

// FUNCTION: LOCO 0x428f90
// EFFECTIVE MATCH (2026-07-16, DIFF 8/271 bytes): structure fully verified against the raw
// disasm -- 98/98 instructions correspond 1:1 (asmscore.py's own count), both strcmp-shaped
// manual compare loops and both if-guards confirmed matching. The entire residual is the
// documented symmetric-register-swap class (Yoda #29/#30): the original loads
// embeddedBitmap.pPixels into EAX, tests it, then (only past that point) reloads
// param_1->pEntry into the now-free EAX for the second `&&` operand's address
// computation -- this compile instead assigns pPixels to ECX and hoists the pEntry load
// into EAX ahead of the test, using two live registers instead of reusing one serially. Tried
// and confirmed NO EFFECT: splitting the `&&` into a nested `if`, swapping the equality
// operand order, and caching the RHS address in an explicit local (the last one actively
// scored WORSE -- it forced a real `add` instruction instead of letting the compiler fold the
// address computation into the `cmp`, per Yoda #19/aliasing: no local means no cached pointer).
// No untried lever; see docs/PARKED.md.
void WidgetPickerObj0x477cc8::PromoteOrSelectSaveEntry(UiIconListItem *param_1)
{
    if (param_1 != pCurrentSlotNode && param_1->wState != 3) {
        if (pCurrentSlotNode->GetLabelTextLength() != 0) {
            if (strcmp(param_1->GetLabelText(), pCurrentSlotNode->GetLabelText()) != 0 &&
                param_1->wState == 2) {
                if (embeddedBitmap.pPixels != 0 &&
                    pShownThumbnailBmp == &param_1->pEntry->embeddedThumbnailBmp) {
                    ReloadBackdropPreview(0);
                }
                param_1->SetNodeState(1);
            }
            if (strcmp(param_1->GetLabelText(), pCurrentSlotNode->GetLabelText()) == 0 &&
                param_1->wState == 1) {
                param_1->SetNodeState(2);
                ReloadBackdropPreview(param_1->pEntry);
            }
        }
    }
}

// FUNCTION: LOCO 0x428ba0
// EFFECTIVE MATCH (2026-07-16, DIFF 185/996 bytes): structure fully verified against the raw
// disasm -- all 8 case bodies, the shared scroll-highlight tail (goto), and the file-list
// duplicate-name search loop confirmed instruction-for-instruction. Two levers were load-
// bearing here (both real findings, not tie-breaks): (1) the switch's case-label ORDER must
// mirror the ORIGINAL's physical .text layout (read off the raw jump-table dwords), not
// source/numeric order -- MSVC/O2 lays out jump-table case bodies in source declaration order,
// so a plain 0x2c02..0x2c0c ordering put whole case bodies in the wrong place (DIFF 724+).
// (2) the duplicate-name search over the sorted SavedFileEntry list must be written as a
// `for (pEntry = head; pEntry != 0; pEntry = pEntry->pNext)` loop, NOT the `do { ... }
// while (pEntry != 0)` shape that reads as the more direct transliteration of the disasm's
// test-at-bottom loop -- the do-while shape triggered a first-iteration PEELING optimization
// (a duplicated, null-check-free copy of the _stricmp call for iteration 1) that the original
// does not have; the for-loop's shape didn't give the optimizer the same opening (DIFF
// 426->185, closed the single largest structural residual). The remaining residual is the
// documented symmetric-register-swap/scheduling class (Yoda #29/#30 family): in both
// 0x2c07/0x2c08's Contains call, the original schedules `cmp word ptr [esi+0x48],1`
// BEFORE dereferencing the ComputeLocalPos result pointer (movs that don't touch flags),
// while this compile schedules the same cmp AFTER the first deref -- caching the x/y values
// into named locals right after the call (matches the original's own "compute unconditionally,
// test after" data flow) gave a small assist (DIFF 429->426) but didn't fully close this
// specific cmp placement; no further lever found within budget. See docs/PARKED.md.
unsigned char WidgetPickerObj0x477cc8::HandleSavegameMenuNode(UiIconListItem *param_1)
{
    int nLocalX;
    int nLocalY;

    // Case order below mirrors the ORIGINAL's physical .text layout (confirmed via the raw
    // jump table dwords at 0x428f5c, not source/numeric order): 0x2c04, 0x2c02, 0x2c03,
    // 0x2c05, 0x2c0c, 0x2c09, 0x2c07, 0x2c08, default -- MSVC/O2 lays out jump-table case
    // bodies in SOURCE declaration order, so this ordering is load-bearing for the match, not
    // stylistic (v121 finding).
    switch (param_1->pIconDesc->resourceId) {
    case 0x2c04: // reset to the "current save" overview
        if (param_1->wSelIndexMaybe == 0) {
            param_1->SetNodeState(1);
            if (param_1 != 0 && param_1->wState == 2) {
                param_1->SetNodeState(1);
            }
            ActivateTab(param_1, 0);
            return 1;
        }
        break;
    case 0x2c02: // Load
        if (param_1->wSelIndexMaybe == 0) {
            if (param_1->wState == 2) {
                param_1->SetNodeState(1);
            }
            if (pCurrentSlotNode->GetLabelTextLength() != 0) {
                LoadActiveSlot();
                return 1;
            }
        }
        break;
    case 0x2c03: // Save
        if (param_1->wSelIndexMaybe == 0) {
            if (param_1->wState == 2) {
                param_1->SetNodeState(1);
            }
            if (pCurrentSlotNode->GetLabelTextLength() != 0) {
                strcpy(szPendingSavePath, "savegame\\");
                strcat(szPendingSavePath, pCurrentSlotNode->GetLabelText());
                strcat(szPendingSavePath, ".sav");
                if (_stricmp(szPendingSavePath, szActiveSavePath) != 0) {
                    if (pLinkedListHead != 0) {
                        SavedFileEntry *pEntry;
                        for (pEntry = pLinkedListHead; pEntry != 0; pEntry = pEntry->pNext) {
                            if (_stricmp(szPendingSavePath, pEntry->szFileName) == 0) {
                                break;
                            }
                        }
                        if (pEntry != 0) {
                            g_pBuildToolCursorWnd->ShowTool(6, 0);
                            return 1;
                        }
                    }
                }
                SaveActiveSlot();
                return 1;
            }
        }
        break;
    case 0x2c05: // Delete-confirm
        if (param_1->wSelIndexMaybe == 0) {
            if (param_1->wState == 2) {
                param_1->SetNodeState(1);
            }
            if (pCurrentSlotNode->GetLabelTextLength() != 0) {
                g_pBuildToolCursorWnd->ShowTool(7, 0);
                return 1;
            }
        }
        break;
    case 0x2c0c: // commit the clicked slot's label as the "current save"
        if (param_1->wSelIndexMaybe == 0) {
            if (param_1->wState == 2) {
                param_1->SetNodeState(1);
            }
            if (pCurrentSlotNode->GetLabelTextLength() != 0) {
                g_NetSessionEventQueue.SaveBoardLayout((unsigned char *)"~curr");
                ReloadActiveSaveState(pCurrentSlotNode->GetLabelText());
                g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)"~curr");
                return 1;
            }
        }
        break;
    case 0x2c09: // promote/select the clicked slot
        PromoteOrSelectSaveEntry(param_1);
        return 1;
    case 0x2c07: // scroll up
        if (param_1->wSelIndexMaybe != 0) {
            break;
        }
        if (param_1->wState == 2) {
            g_UIResources.PlaySoundAtScreenPos(0x5015, PlacementCursorMaybe_004854c8.resolvedPosAX,
                                             PlacementCursorMaybe_004854c8.resolvedPosAY, 4);
            RelocateSavegameSelection(pCurrentEntry->pPrev);
        }
        POINT ptUp = ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosY,
                                           PlacementCursorMaybe_004854c8.lastResolvedPosX);
        nLocalX = ptUp.x;
        nLocalY = ptUp.y;
        if (param_1->wState != 1) {
            break;
        }
        if (!param_1->Contains(nLocalX, nLocalY)) {
            break;
        }
        if (!PlacementCursorMaybe_004854c8.bFlagE6Maybe) {
            break;
        }
        goto highlightScrolledNode;
    case 0x2c08: // scroll down
        if (param_1->wSelIndexMaybe != 0) {
            break;
        }
        if (param_1->wState == 2) {
            g_UIResources.PlaySoundAtScreenPos(0x5015, PlacementCursorMaybe_004854c8.resolvedPosAX,
                                             PlacementCursorMaybe_004854c8.resolvedPosAY, 4);
            RelocateSavegameSelection(pCurrentEntry->pNext);
        }
        POINT ptDown = ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosY,
                                             PlacementCursorMaybe_004854c8.lastResolvedPosX);
        nLocalX = ptDown.x;
        nLocalY = ptDown.y;
        if (param_1->wState != 1) {
            break;
        }
        if (!param_1->Contains(nLocalX, nLocalY)) {
            break;
        }
        if (!PlacementCursorMaybe_004854c8.bFlagE6Maybe) {
            break;
        }
    highlightScrolledNode:
        param_1->SetNodeState(2);
        param_1->wSelIndexMaybe = 6;
        break;
    }
    return 1;
}

// FUNCTION: LOCO 0x4290a0
// EXACT (2026-07-26, 999 bytes). Three source facts are load-bearing here -- do not "clean up"
// any of them:
//  (1) ONE `bResult` local and ONE trailing `return bResult;`. Writing the arms as direct
//      `return 0;`/`return 1;`/`return bResult;` statements is semantically identical but costs
//      the whole shared epilogue: the original funnels EVERY exit (base-consumed, wrong
//      category, both arrow keys, both edit arms) into a single `mov al,bl; pop; pop; pop;
//      ret 4`, and per-arm returns re-materialize the constant at each site instead
//      (asmscore total 198743 -> 118361 from this restructure alone).
//  (2) The VK_BACK/VK_DELETE arm must spell the assignment as a TERNARY. `bResult = f();` and
//      `bResult = f() != 0;` and even `bResult = !!f();` all make VC5 emit its 32-bit
//      bool-normalization idiom (`mov bl,al; neg bl; sbb ebx,ebx; neg ebx`); only the ternary
//      gets the original's 2-instruction `test al,al; setne bl`. An explicit if/else assigning
//      false/true compiles identically to the ternary -- the ternary is the tidier spelling of
//      the same thing (118361 -> EXACT).
//  (3) Slot 16's return type is genuinely `bool`, NOT the `char` this family's other
//      byte-returning virtuals use -- with `char` the match breaks outright (DIFF 620), because
//      `bool bResult = <char>` would need a normalization the original's plain `mov bl,al`
//      doesn't have. Since C++ forces an override's return type to match its base exactly, this
//      also PINS WidgetBaseObj0x4784c8::OnKeyDownMaybe (0x454ae0, transcribed EXACT in v446) to
//      `bool` -- the one hard fact recovered about that function so far.
// The scroll-into-view block is deliberately written out twice, once per edit arm, rather than
// factored into a helper: same call as ActivateTab's four literal copies of the sorted-position
// search (see its own note above), and the two copies here compile to DIFFERENT register
// assignments in the original (the VK_BACK/VK_DELETE copy has `bResult` pinned in BL and so
// compares through ESI, the default copy is free to use BL as a scratch byte), which is exactly
// what two independent inlinings of the same source text look like.
bool WidgetPickerObj0x477cc8::OnKeyDownMaybe(unsigned int nKey)
{
    bool bResult = WidgetBaseObj0x4784c8::OnKeyDownMaybe(nKey);
    if (bResult == 0 &&
        (nCategory == 2 || nCategory == 3 || nCategory == 4 || nCategory == 5)) {
        switch (nKey) {
        case VK_UP:
            if (pScrollUpArrowNode != 0 && pScrollUpArrowNode->wState == 1) {
                pScrollUpArrowNode->SetNodeState(2);
                pScrollUpArrowNode->wSelIndexMaybe = 6;
            }
            bResult = 1;
            break;
        case VK_DOWN:
            if (pScrollDownArrowNode != 0 && pScrollDownArrowNode->wState == 1) {
                pScrollDownArrowNode->SetNodeState(2);
                pScrollDownArrowNode->wSelIndexMaybe = 6;
            }
            bResult = 1;
            break;
        case VK_BACK:
        case VK_DELETE:
        {
            bResult = pCurrentSlotNode->HandleTextEditKey(nKey) ? true : false;
            int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            while (nCmp > 0) {
                if (pCurrentEntry->pPrev == 0) {
                    break;
                }
                RelocateSavegameSelection(pCurrentEntry->pPrev);
                nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            }
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            while (nCmp < 0) {
                if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
                    break;
                }
                RelocateSavegameSelection(pCurrentEntry->pNext);
                nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            }
            break;
        }
        default:
        {
            pCurrentSlotNode->HandleTextEditKey(nKey);
            int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            while (nCmp > 0) {
                if (pCurrentEntry->pPrev == 0) {
                    break;
                }
                RelocateSavegameSelection(pCurrentEntry->pPrev);
                nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            }
            nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            while (nCmp < 0) {
                if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
                    break;
                }
                RelocateSavegameSelection(pCurrentEntry->pNext);
                nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
            }
            bResult = 1;
            break;
        }
        }
    }
    return bResult;
}

// FUNCTION: LOCO 0x429490
// EFFECTIVE MATCH (2026-07-16, score 243520 via asmscore.py, down from a naive 787110):
// structure fully verified against the raw disasm (the sentinel-head insertion sort, the
// _findfirst/_findnext/_findclose enumeration loop, the CreateDirectoryA fallback, and all
// _splitpath/_stricmp calls confirmed instruction-for-instruction). Two real (non-tie-break)
// fixes closed most of the gap:
//  (1) a category-selected string literal must be computed into a `const char *` local FIRST,
//      never written as a ternary directly inside the strcat/strcpy call -- a ternary-in-place
//      makes the compiler duplicate the ENTIRE strcat intrinsic expansion once per branch
//      (repne scasb + rep movsd, ~50 instructions), while a shared runtime-selected pointer
//      compiles to ONE expansion regardless of the branch taken (matches the original exactly).
//  (2) the category-selected pointer's own computation must be interleaved in the SAME order
//      as the original's instruction stream: `strcpy(szPath, prefix)` first, THEN the category
//      if/assignment, THEN `strcat(szPath, pattern)` -- computing the category pick BEFORE the
//      first strcpy shifted the whole function's block layout (confirmed via asmscore.py's
//      score dropping 787110->303010 from this one reordering alone).
// The remaining residual is the documented symmetric-register-swap class (Yoda #29/#30): the
// original keeps `this` resident in EBX for the whole function; this compile picks EBP instead,
// propagating a register-only ("r") diff through dozens of otherwise-identical instructions,
// plus a couple of places where the resulting register pressure spills `this` to a stack slot
// (reloaded before the 2nd category check inside the insertion loop) where the original doesn't
// need to. No untried lever found within budget; see docs/PARKED.md.
unsigned char WidgetPickerObj0x477cc8::EnumerateFiles()
{
    // Both buffers zeroed unconditionally at function entry (same "declared unconditionally at
    // the top" idiom as ActivateTab's own szBuf, even though szShortName is only read
    // inside the loop below) -- confirmed via the raw disasm, both rep-stos zero-fills happen
    // back to back right after the SEH prologue, before the old list is even cleared.
    char szPath[0x105] = "";

    char szShortName[0x105] = "";

    // Sentinel-head insertion-sort dummy, default-constructed (not heap-allocated) -- confirmed
    // via the raw disasm: its ctor/dtor fully INLINE right here (vtable store + field zeroing
    // inline, only the embedded ThumbnailBmp sub-object's own ctor/dtor stay real out-of-line
    // calls), exactly matching SavedFileEntry's in-class-body ctor/dtor definitions.
    SavedFileEntry dummyMaybe;

    SavedFileEntry *pEntry = pLinkedListHead;
    while (pEntry != 0) {
        pLinkedListHead = pEntry->pNext;
        delete pEntry;
        pEntry = pLinkedListHead;
    }
    dummyMaybe.pNext = pLinkedListHead;

    strcpy(szPath, g_pInstallPathPrefix);
    const char *pszGlobPattern = "savegame\\*.sav";
    if (nCategory == 5) {
        pszGlobPattern = "backdrop\\*.bmp";
    }
    strcat(szPath, pszGlobPattern);

    struct _finddata_t findData;
    long hFind = _findfirst(szPath, &findData);
    if (hFind != -1) {
        do {
            if (findData.name[0] != '.') {
                _splitpath(findData.name, 0, 0, szShortName, 0);
                if (strlen(szShortName) <= 10) {
                    SavedFileEntry *pNew = new SavedFileEntry();
                    if (pNew != 0) {
                        strncpy(pNew->szShortName, szShortName, 10);
                        pNew->szShortName[10] = 0;
                        const char *pszPrefix = "savegame\\";
                        if (nCategory == 5) {
                            pszPrefix = "backdrop\\";
                        }
                        strcpy(pNew->szFileName, pszPrefix);
                        strncat(pNew->szFileName, findData.name, 0x40);

                        SavedFileEntry *pPrev = &dummyMaybe;
                        SavedFileEntry *pCur = dummyMaybe.pNext;
                        while (pCur != 0 && _stricmp(pCur->szShortName, pNew->szShortName) < 0) {
                            pPrev = pCur;
                            pCur = pCur->pNext;
                        }
                        pNew->pNext = pCur;
                        pNew->pPrev = pPrev;
                        pPrev->pNext = pNew;
                        if (pCur != 0) {
                            pCur->pPrev = pNew;
                        }
                    }
                }
            }
        } while (_findnext(hFind, &findData) == 0);
        _findclose(hFind);
    } else {
        // Fresh install / directory doesn't exist yet -- create it. Always "savegame" regardless
        // of category (sic: even the backdrop-glob-miss path recreates the savegame directory,
        // not "backdrop" -- confirmed against the raw disasm, not a transcription slip).
        strcpy(szPath, g_pInstallPathPrefix);
        strcat(szPath, "savegame");
        CreateDirectoryA(szPath, 0);
    }

    pLinkedListHead = dummyMaybe.pNext;
    if (dummyMaybe.pNext != 0) {
        dummyMaybe.pNext->pPrev = 0;
    }

    return 1;
}

// FUNCTION: LOCO 0x429a10
void WidgetPickerObj0x477cc8::LoadActiveSlot()
{
    CHAR szDirPath[MAX_PATH];
    wsprintfA(szDirPath, "%s%s", g_pInstallPathPrefix, "savegame");
    if (GetFileAttributesA(szDirPath) == 0xffffffff) {
        CreateDirectoryA(szDirPath, 0);
    }

    strcpy(szPendingSavePath, "savegame\\");
    strcat(szPendingSavePath, pCurrentSlotNode->GetLabelText());
    strcat(szPendingSavePath, ".sav");

    PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 1);
    g_worldBoard.UpdateDirtyTiles(0);
    g_NetSessionEventQueue.PlaceEdgeLinksAndFlush((unsigned char *)szPendingSavePath);
    PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 0);
}

// FUNCTION: LOCO 0x429b20
// Confirmed genuine save-commit (see docs/subsystems.md's v119 recon): tears down/frees the
// whole pLinkedListHead list, writes the game state + thumbnail via
// NetSessionEventQueue::SaveBoardLayout, re-enumerates the directory, then scrolls the
// visible 6-slot window (the same strcmp-sorted-search idiom as ActivateTab's 4 blocks
// above -- reused verbatim here, a 5th occurrence of that confirmed-strcmp shape) up then down
// until the just-saved slot is back in view.
void WidgetPickerObj0x477cc8::SaveActiveSlot()
{
    if (pCurrentSlotNode->GetLabelTextLength() == 0) {
        return;
    }

    CHAR szDirPath[MAX_PATH];
    wsprintfA(szDirPath, "%s%s", g_pInstallPathPrefix, "savegame");
    if (GetFileAttributesA(szDirPath) == 0xffffffff) {
        CreateDirectoryA(szDirPath, 0);
    }

    strcpy(szPendingSavePath, "savegame\\");
    strcat(szPendingSavePath, pCurrentSlotNode->GetLabelText());
    strcat(szPendingSavePath, ".sav");

    PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 1);

    SavedFileEntry *pEntry = pLinkedListHead;
    while (pEntry != 0) {
        pLinkedListHead = pEntry->pNext;
        delete pEntry;
        pEntry = pLinkedListHead;
    }

    g_NetSessionEventQueue.SaveBoardLayout((unsigned char *)szPendingSavePath);
    EnumerateFiles();
    RelocateSavegameSelection(pLinkedListHead);

    int nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
    while (nCmp > 0) {
        if (pCurrentEntry->pPrev == 0) {
            break;
        }
        RelocateSavegameSelection(pCurrentEntry->pPrev);
        nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
    }
    nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
    while (nCmp < 0) {
        if (((UiIconListItem *)arrSlotNodes[5])->GetLabelTextLength() <= 0) {
            break;
        }
        RelocateSavegameSelection(pCurrentEntry->pNext);
        nCmp = strcmp(((UiIconListItem *)arrSlotNodes[0])->GetLabelText(), pCurrentSlotNode->GetLabelText());
    }

    PlacementCursorMaybe_004854c8.SetCursorCapture(1, 1, 0);
}

// FUNCTION: LOCO 0x429dd0
void WidgetPickerObj0x477cc8::DeleteActiveSlot()
{
    strcpy(szPendingSavePath, g_pInstallPathPrefix);
    strcat(szPendingSavePath, "savegame\\");
    strcat(szPendingSavePath, pCurrentSlotNode->GetLabelText());
    strcat(szPendingSavePath, ".sav");

    SavedFileEntry *pEntry = pLinkedListHead;
    while (pEntry != 0) {
        pLinkedListHead = pEntry->pNext;
        delete pEntry;
        pEntry = pLinkedListHead;
    }

    BOOL bDeleted = DeleteFileA(szPendingSavePath);
    EnumerateFiles();
    if (bDeleted == 1) {
        pCurrentSlotNode->SetLabelText("");
        RelocateSavegameSelection(pLinkedListHead);
        return;
    }
    GetLastError();
}

extern void CenterRectInRect(RECT *outer, RECT *rect); // 0x425a50

// FUNCTION: LOCO 0x428400
// EFFECTIVE MATCH (DIFF 240/342 bytes; asmscore.py --dump shows every structural (S) row
// aligned, 96/97 insns, the entire residual is register-role noise -- see docs/PARKED.md).
// pEntry's own address (raw disasm: `param_1+0xf` ==
// &pEntry->szFileName, `param_1+0x50` == &pEntry->embeddedThumbnailBmp -- both fall out of
// SavedFileEntry's real layout: vtbl(4) + szShortName[11](0xf) + szFileName[0x41](0x50) +
// embeddedThumbnailBmp) is used as a plain SavedFileEntry* throughout, confirmed against every
// known caller (TestMenuCommand passes 0, PromoteOrSelectSaveEntry passes 0 or a real pEntry).
// Two real structural levers were needed (not tie-breaks): (1) the `nCategory != 5` (thumbnail)
// branch must be the FALL-THROUGH body with `nCategory == 5` (backdrop) as the jumped-to tail
// block -- the naturally-ordered `if (nCategory == 5) {...} else {...}` transcription put them
// backwards, a ~213000-point structural miss (Yoda #15/branch-order-is-sometimes-a-lever family).
// (2) ThumbnailBmp::wWidth/wHeight are declared `short` (signed, to match
// DPlaySessionMgr::LayoutSet_LoadSlotBitmap's already-EXACT read of the same fields), but THIS
// call site's raw disasm zero-extends them (`xor eax,eax; mov ax,[...]`) -- an explicit
// `(unsigned short)` cast at each use forces the same zero-extension; g_worldBoard.wCols/wRows
// need the opposite explicit `(short)` cast (that call site sign-extends, per Ghidra's own
// `(int)(short)...Maybe` decompile). Residual: a persistent "0" register (edi in the original,
// materialized once in the prologue and reused for both null-checks and every literal-0 call
// arg) that this compile doesn't allocate (uses `test reg,reg` + literal `push 0` instead),
// plus the by-now-familiar symmetric register-swap on the derived ThumbnailBmp pointer
// (esi/edi) through the thumbnail branch -- both the documented intrinsic
// allocator-tie-break class (Yoda #29/#30 and the prologue-materialization family). Tried and
// confirmed NO EFFECT: computing the IsLoaded() call target inline vs. via a named `pThumb`
// local. No untried lever.
void WidgetPickerObj0x477cc8::ReloadBackdropPreview(SavedFileEntry *pEntry)
{
    if (pEntry == 0 || embeddedBitmap.pPixels != 0) {
        pShownThumbnailBmp = 0;
        embeddedBitmap.Fill(8, 0);
        RedrawPreviewIntoOwnerIconMaybe();
    }
    if (pEntry != 0) {
        if (nCategory != 5) {
            ThumbnailBmp *pThumb = &pEntry->embeddedThumbnailBmp;
            if (!pThumb->ThumbnailBmp_IsLoaded()) {
                return;
            }
            pShownThumbnailBmp = pThumb;
            embeddedBitmap.CreateAndFill((unsigned short)pThumb->wWidth, (unsigned short)pThumb->wHeight, 0, 0, 8);
            if (pThumb->pPixels != 0) {
                memcpy(embeddedBitmap.pPixels, pThumb->pPixels,
                       (unsigned short)pThumb->wHeight * (unsigned short)pThumb->wWidth);  // idiom-exempt: runtime length
                RedrawPreviewIntoOwnerIconMaybe();
                return;
            }
        } else {
            pShownThumbnailBmp = 0;
            char szPath[264];
            sprintf(szPath, "%s%s", g_pInstallPathPrefix, pEntry->szFileName);
            embeddedBitmap.CreateAndFill((short)g_worldBoard.wCols, (short)g_worldBoard.wRows, 1, 0, 0);
            embeddedBitmap.Load(szPath, 0, (short)g_worldBoard.wCols, (short)g_worldBoard.wRows);
        }
        RedrawPreviewIntoOwnerIconMaybe();
    }
}

// FUNCTION: LOCO 0x428550
// Sparse 4-value width switch (50/64/72/80) compiled as a byte-table + jump-table dispatch
// (confirmed via the raw byte/jump tables at 0x428750/0x42873c, not a compare chain) --
// case 0x32(50) and case 0x40(64) are SEPARATE physical bodies (not a shared `case 0x32: case
// 0x40:` label) even though their content is identical, matching the jump table's own 2
// distinct target addresses; case 0x48(72)/0x50(80) DO share one physical body (same jump
// table target). All 3 taken cases end their own body with the BlitOntoBitmap call --
// duplicated per-case in source, cross-jumped to one shared physical call site by the compiler
// (same idiom as this file's own ActivateTab sorted-position search blocks) -- rather than
// lifted out after the switch, so an unmatched width skips the call entirely (falls straight
// to the dirty-mark) instead of needing a guard.
void WidgetPickerObj0x477cc8::RedrawPreviewIntoOwnerIconMaybe()
{
    RECT previewRect;
    SetRect(&previewRect, 0xf, 0x1a, 0x8e, 0x79);

    RECT srcRect;
    SetRect(&srcRect, 0, 0, embeddedBitmap.width, embeddedBitmap.height);

    RECT destRect;
    LocoBitmap *pDestBitmap;
    unsigned int flags;

    switch (embeddedBitmap.width) {
    case 0x32:
        SetRect(&destRect, 0, 0, embeddedBitmap.width * 2, embeddedBitmap.height * 2);
        CenterRectInRect(&previewRect, &destRect);
        flags = 4;
        pDestBitmap = pKindDesc->pOwnedObjA;
        embeddedBitmap.BlitOntoBitmap(destRect, pDestBitmap, srcRect, flags);
        break;
    case 0x40:
        SetRect(&destRect, 0, 0, embeddedBitmap.width * 2, embeddedBitmap.height * 2);
        CenterRectInRect(&previewRect, &destRect);
        flags = 4;
        pDestBitmap = pKindDesc->pOwnedObjA;
        embeddedBitmap.BlitOntoBitmap(destRect, pDestBitmap, srcRect, flags);
        break;
    case 0x48:
    case 0x50:
        SetRect(&destRect, 0, 0, embeddedBitmap.width, embeddedBitmap.height);
        CenterRectInRect(&previewRect, &destRect);
        flags = 0;
        pDestBitmap = pKindDesc->pOwnedObjA;
        embeddedBitmap.BlitOntoBitmap(destRect, pDestBitmap, srcRect, flags);
        break;
    }

    g_worldBoard.MarkRectDirty(rect);
}

// FUNCTION: LOCO 0x429ef0
// Applies a backdrop by slot label: drops whatever backdrop descriptor is currently installed,
// builds a fresh one from "backdrop\<label>", stretches its bitmap to the world board's viewport
// if it doesn't already fill it, and records the label. If that descriptor never realizes a
// bitmap (missing/undecodable file), falls back to the built-in TileKind 0x400 backdrop instead.
// Either way the whole viewport is dirty-marked. Called from HandleSavegameMenuNode's 0x2c0c
// command with pCurrentSlotNode->GetLabelText(), and from three non-widget sites (app bring-up,
// NetSessionEventQueue's layout load, and the main window proc).
void WidgetPickerObj0x477cc8::ReloadActiveSaveState(char *pszSlotLabel)
{
    char szPath[0x105];

    if (g_pBackdropDesc != NULL) {
        g_pBackdropDesc->ReleaseRef();
        if (g_pBackdropDesc->resourceId == -1) {
            delete g_pBackdropDesc;
        }
        g_pBackdropDesc = NULL;
    }

    strcpy(szPath, "backdrop\\");
    strcat(szPath, pszSlotLabel);
    g_pBackdropDesc = new CursorDesc(-1, szPath, 1);

    if (g_pBackdropDesc != NULL) {
        LocoBitmap *pBitmap = g_pBackdropDesc->GetOrLoadFrameBitmap(0, 0);
        if (pBitmap != NULL && (pBitmap->width != g_worldBoard.dwViewportWidth ||
                                pBitmap->height != g_worldBoard.dwViewportHeightMaybe)) {
            pBitmap->Resize(g_worldBoard.dwViewportWidth, g_worldBoard.dwViewportHeightMaybe);
            // This division is what proved LocoBitmap's width/height are `unsigned` -- see
            // src/LocoBitmap.h. It is the only site in the codebase that can tell the two
            // models apart, and it byte-matches only under the unsigned one.
            g_pBackdropDesc->nativeWidth =
                (unsigned short)(pBitmap->width / g_pBackdropDesc->nTotalFrameCount);
            g_pBackdropDesc->nativeHeight = (unsigned short)pBitmap->height;
        }
    }

    if (g_pBackdropDesc != NULL && g_pBackdropDesc->pOwnedObjA != NULL) {
        strcpy(szActiveBackdropName, pszSlotLabel);
    } else {
        if (g_pBackdropDesc != NULL && g_pBackdropDesc->resourceId == -1) {
            delete g_pBackdropDesc;
            g_pBackdropDesc = NULL;
        }
        g_pBackdropDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x400);
        if (g_pBackdropDesc != NULL) {
            g_pBackdropDesc->GetOrLoadFrameBitmap(0, 0);
        }
    }

    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
}
